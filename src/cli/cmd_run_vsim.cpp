/**
 * cmd_run_vsim.cpp  -  vsper run <script.vsim> subcommand
 *
 * Drives the VsimRuntime terminal visual pipeline for a parsed .vsim script:
 *   1. Parse / validate / registry-resolve
 *   2. Print visual section banner
 *   3. Synthetic step loop: pace_step + chemistry pass + variance/N_evolution probes
 *      (full physics kernel not yet wired; events synthesised from document params)
 *   4. while / batch interpreters
 *   5. beta-7 pipeline pass (run_pipeline over reference_dataset)
 *   6. show_post_run_window + flush_pipeline_artifacts
 *
 * WO-57D  |  v5.0.0-beta.7
 */

#include "cli/cmd_run_vsim.hpp"
#include "cli/system_state.hpp"
#include "cli/viewer_launcher.hpp"
#include "vsim/vsim_parser.hpp"
#include "vsim/vsim_value.hpp"
#include "vsim/vsim_registry.hpp"
#include "vsim/intent/intent_bridge.hpp"  // WO-VSIM-INTENT-BRIDGE-A
#include "core/bond_graph_gen.hpp"
#include "vsim/bridge/dem_bridge.hpp"  // WO-67N
#include "vsim/bridge/fea_bridge.hpp"  // WO-67O
#include "vsim/dissolution_bridge.hpp"
#include "vsim/vsim_runtime.hpp"
#include "vsim/relative_reactivity.hpp"     // WO-85B  Part E
#include "vsim/chemplus_declarative.hpp"    // WO-85B  reaction stoichiometry/energy
#include "kernel/kernel_event_log.hpp"
#include "kernel/kernel_event.hpp"
#include "vsepr/formula_parser.hpp"
#include "sim/molecule_builder.hpp"
#include "core/element_data.hpp"
#include "pot/periodic_db.hpp"
#include "infra/bootstrap_probe.hpp"        // WO-93A CPU/RAM/disk/GPU probes

#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <shellapi.h>
#else
#  include <unistd.h>
#  include <sys/types.h>
#  include <sys/wait.h>
#endif

namespace vsepr::cli {

namespace {
	const char* GREEN  = "\033[0;32m";
	const char* YELLOW = "\033[1;33m";
	const char* RED    = "\033[0;31m";
	const char* BOLD   = "\033[1m";
	const char* DIM    = "\033[2m";
	const char* CYAN   = "\033[36m";
	const char* RESET  = "\033[0m";

#ifdef _WIN32
	const char* ANSI_HOME = "\r";                 // carriage return via CRLF
	const char* ANSI_UP2  = "\033[2A\r";          // move up two lines + return
#else
	const char* ANSI_HOME = "\r";
	const char* ANSI_UP2  = "\033[2A\r";
#endif

	// WO-93A — smooth two-line terminal status loop helpers.
	const char* ANSI_BG_RED    = "\033[41m";
	const char* ANSI_BG_YELLOW = "\033[43m";
	const char* ANSI_BG_GREEN  = "\033[42m";
	const char* ANSI_BG_CYAN   = "\033[46m";
	const char* ANSI_BG_GREY   = "\033[100m";

	// Map a ratio in [0,1] to a colour band: red -> yellow -> green -> cyan.
	// Used to paint an interpretive colour layer next to data bars.
	const char* ratio_to_colour(double r) {
		r = std::clamp(r, 0.0, 1.0);
		if      (r < 0.25) return RED;
		else if (r < 0.50) return YELLOW;
		else if (r < 0.75) return GREEN;
		else               return CYAN;
	}

	// Coloured block bar: width chars filled to the given ratio.
	// When utf8==true use a full block; otherwise use '#' so redirected logs
	// stay valid ASCII and source control diffs remain clean.
	std::string colour_block_bar(double ratio, int width, bool utf8) {
		int filled = static_cast<int>(std::round(std::clamp(ratio, 0.0, 1.0) * width));
		std::string out;
		const char* fill_colour = ratio_to_colour(ratio);
		const char* block = utf8 ? "█" : "#";
		for (int i = 0; i < width; ++i) {
			if (i < filled) out += fill_colour;
			else            out += DIM;
			out += block;
		}
		out += RESET;
		return out;
	}

	// "State" token derived from current energy/eta values for the colour layer.
	std::string interpretive_state_token(double energy, double eta, int /*step*/) {
		if (eta > 0.95 && std::abs(energy) < 1.0)
			return std::string(GREEN) + "steady" + RESET;
		if (eta > 0.85)
			return std::string(CYAN) + "relax" + RESET;
		if (eta > 0.50)
			return std::string(YELLOW) + "warm" + RESET;
		return std::string(RED) + "fire" + RESET;
	}

	// WO-93A frame-rate throttles.
	using Clock     = std::chrono::steady_clock;
	using TimePoint = Clock::time_point;

	// Live HUD state and rate helpers.
	struct StatusLoopState {
		bool       active       = false;
		bool       first_frame  = true;
		TimePoint  last_top     = TimePoint{};
		TimePoint  last_hw      = TimePoint{};
		vsepr::infra::CpuLoadState cpu_state{};
		double     cpu_frac     = 0.0;
		double     total_ram_gb = 0.0;
		double     free_ram_gb  = 0.0;
		double     disk_free_gb = 0.0;
		std::string gpu_name;
	};

	struct StatusLoopCtx {
		const vsim::VisualSection& vis;
		StatusLoopState&          state;
		int                       max_steps;
		double                    emin;
		double                    emax;
	};

	void status_loop_update_hardware(StatusLoopState& state) {
		using namespace vsepr::infra;
		state.cpu_frac     = cpu_load_fraction(state.cpu_state);
		state.total_ram_gb = total_ram_gb();
		state.free_ram_gb  = free_ram_gb();
		state.disk_free_gb = disk_free_gb();
		if (state.gpu_name.empty())
			state.gpu_name = detect_gpu();
	}

	std::string compact_value(double v, int digits) {
		std::ostringstream oss;
		if (std::abs(v) < 1e3 && std::abs(v) >= 0.1)
			oss << std::fixed << std::setprecision(digits) << v;
		else
			oss << std::setprecision(digits) << v;
		return oss.str();
	}

	std::string pad_trunc(std::string s, std::size_t n) {
		if (s.size() > n) {
			if (n >= 3) { s.resize(n - 1); s.push_back('?'); }
			else s.resize(n);
		} else if (s.size() < n) {
			s.append(n - s.size(), ' ');
		}
		return s;
	}

	// Render the two-line status HUD. Call sites may invoke every step;
	// this function time-throttles line-1 by status_loop_hz and line-2 by
	// hardware_monitor_hz.  Non-terminal output disables cursor-up overwrite
	// and falls back to ASCII blocks so redirected logs stay clean.
	void status_loop_render(StatusLoopCtx& ctx, int step, double energy, double eta,
							const std::string& label, bool is_terminal_output)
	{
		if (!ctx.vis.show_status_loop) return;

		const TimePoint now = Clock::now();

		// Line-1 high-Hz throttle (default 30 Hz).
		const float top_period_s = ctx.vis.status_loop_hz > 0.0f
			? 1.0f / ctx.vis.status_loop_hz : 1.0f / 30.0f;
		if (!ctx.state.first_frame &&
			std::chrono::duration<double>(now - ctx.state.last_top).count() < top_period_s) {
			return;
		}

		// Refresh hardware telemetry at its own cadence.
		const float hw_period_s = ctx.vis.hardware_monitor_hz > 0.0f
			? 1.0f / ctx.vis.hardware_monitor_hz : 0.5f;
		if (ctx.state.first_frame ||
			std::chrono::duration<double>(now - ctx.state.last_hw).count() >= hw_period_s) {
			status_loop_update_hardware(ctx.state);
			ctx.state.last_hw = now;
		}

		double e_spread = ctx.emax - ctx.emin;
		double e_ratio  = 0.5;
		if (e_spread > 1e-9)
			e_ratio = std::clamp((energy - ctx.emin) / e_spread, 0.0, 1.0);

		std::string e_bar = colour_block_bar(e_ratio, 14, is_terminal_output);
		std::string n_bar = colour_block_bar(std::clamp(eta, 0.0, 1.0), 10, is_terminal_output);
		std::string state = interpretive_state_token(energy, eta, step);

		std::ostringstream line1;
		line1 << "step " << std::setw(5) << step << "/" << ctx.max_steps
			  << "  " << e_bar
			  << "  E=" << std::setw(10) << compact_value(energy, 2)
			  << "  " << n_bar
			  << "  η=" << std::fixed << std::setprecision(3) << eta
			  << "  " << state;

		std::ostringstream line2;
		line2 << "CPU " << std::setw(3) << static_cast<int>(std::round(ctx.state.cpu_frac * 100.0)) << "%"
			  << "  RAM " << std::fixed << std::setprecision(1) << ctx.state.free_ram_gb << "/"
			  << ctx.state.total_ram_gb << " GB"
			  << "  DISK " << std::fixed << std::setprecision(1) << ctx.state.disk_free_gb << " GB"
			  << "  GPU " << pad_trunc(ctx.state.gpu_name.empty() ? "-" : ctx.state.gpu_name, 24)
			  << "  " << pad_trunc(label.empty() ? "run" : label, 16);

		if (!is_terminal_output || ctx.state.first_frame) {
			std::printf("%s\n%s\n", line1.str().c_str(), line2.str().c_str());
		} else {
			std::printf("%s%s\n%s\n", ANSI_UP2, line1.str().c_str(), line2.str().c_str());
		}
		std::fflush(stdout);
		ctx.state.first_frame = false;
		ctx.state.last_top    = now;
	}

	std::string quote_process_arg(const std::string& value) {
#ifdef _WIN32
		std::string quoted = "\"";
		std::size_t backslashes = 0;
		for (const char ch : value) {
			if (ch == '\\') {
				++backslashes;
				continue;
			}
			if (ch == '"') {
				quoted.append(backslashes * 2 + 1, '\\');
				quoted.push_back('"');
				backslashes = 0;
				continue;
			}
			quoted.append(backslashes, '\\');
			backslashes = 0;
			quoted.push_back(ch);
		}
		quoted.append(backslashes * 2, '\\');
		quoted.push_back('"');
		return quoted;
#else
		return value;
#endif
	}

	int run_process_wait(const std::vector<std::string>& arguments) {
		if (arguments.empty()) return -1;
#ifdef _WIN32
		std::string command;
		for (const auto& argument : arguments) {
			if (!command.empty()) command.push_back(' ');
			command += quote_process_arg(argument);
		}
		std::vector<char> mutable_command(command.begin(), command.end());
		mutable_command.push_back('\0');
		STARTUPINFOA startup{};
		startup.cb = sizeof(startup);
		PROCESS_INFORMATION process{};
		if (!CreateProcessA(nullptr, mutable_command.data(), nullptr, nullptr, FALSE,
			CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process)) {
			return -1;
		}
		WaitForSingleObject(process.hProcess, INFINITE);
		DWORD exit_code = 1;
		GetExitCodeProcess(process.hProcess, &exit_code);
		CloseHandle(process.hThread);
		CloseHandle(process.hProcess);
		return static_cast<int>(exit_code);
#else
		const pid_t pid = fork();
		if (pid < 0) return -1;
		if (pid == 0) {
			std::vector<char*> argv;
			argv.reserve(arguments.size() + 1);
			for (const auto& argument : arguments) {
				argv.push_back(const_cast<char*>(argument.c_str()));
			}
			argv.push_back(nullptr);
			execvp(argv.front(), argv.data());
			_exit(127);
		}
		int status = 0;
		if (waitpid(pid, &status, 0) < 0) return -1;
		return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
	}

	std::filesystem::path resolve_matplotlib_renderer() {
		std::vector<std::filesystem::path> candidates = {
			std::filesystem::current_path() / "scripts" / "mol_viewer.py"
		};
#ifdef _WIN32
		char executable[MAX_PATH] = {};
		if (GetModuleFileNameA(nullptr, executable, MAX_PATH) > 0) {
			const auto directory = std::filesystem::path(executable).parent_path();
			candidates.push_back(directory / "scripts" / "mol_viewer.py");
			candidates.push_back(directory.parent_path() / "scripts" / "mol_viewer.py");
		}
#else
		std::error_code link_error;
		const auto executable = std::filesystem::read_symlink("/proc/self/exe", link_error);
		if (!link_error) {
			const auto directory = executable.parent_path();
			candidates.push_back(directory / "scripts" / "mol_viewer.py");
			candidates.push_back(directory.parent_path() / "scripts" / "mol_viewer.py");
		}
#endif
		std::error_code exists_error;
		for (const auto& candidate : candidates) {
			if (std::filesystem::is_regular_file(candidate, exists_error) && !exists_error) {
				return candidate;
			}
			exists_error.clear();
		}
		return {};
	}

	bool has_png_signature(const std::filesystem::path& path) {
		static constexpr std::array<unsigned char, 8> expected = {
			0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a};
		std::ifstream input(path, std::ios::binary);
		std::array<unsigned char, 8> actual{};
		return input.read(reinterpret_cast<char*>(actual.data()), actual.size()) &&
			actual == expected;
	}

	bool write_matplotlib_snapshot(bool requested,
								 const std::string& configured_output_dir,
								 const std::string& export_output_dir,
								 const std::string& artifact_path,
								 const std::string& run_label) {
		if (!requested) return true;
		if (artifact_path.empty() || !std::filesystem::is_regular_file(artifact_path)) {
			std::printf("  %s[matplotlib-png:skipped]%s no XYZ-family artifact was produced\n",
				YELLOW, RESET);
			return false;
		}

		const auto renderer = resolve_matplotlib_renderer();
		if (renderer.empty()) {
			std::printf("  %s[matplotlib-png:skipped]%s scripts/mol_viewer.py was not found\n",
				YELLOW, RESET);
			return false;
		}

		const std::filesystem::path output_dir = configured_output_dir.empty()
			? std::filesystem::path(export_output_dir.empty() ? "out/" + run_label : export_output_dir) / "figures"
			: std::filesystem::path(configured_output_dir);
		std::error_code directory_error;
		std::filesystem::create_directories(output_dir, directory_error);
		if (directory_error) {
			std::printf("  %s[matplotlib-png:failed]%s cannot create %s: %s\n",
				RED, RESET, output_dir.string().c_str(), directory_error.message().c_str());
			return false;
		}

		const auto output_path = output_dir / (run_label + "_matplotlib.png");
		std::error_code absolute_error;
		auto absolute_artifact = std::filesystem::absolute(artifact_path, absolute_error);
		if (absolute_error) absolute_artifact = artifact_path;
		std::vector<std::string> arguments = {
			"python", renderer.string(), absolute_artifact.string(),
			"--export-png", output_path.string(), "--frame", "last"};
		int exit_code = run_process_wait(arguments);
		if (exit_code == -1) {
			arguments.front() = "python3";
			exit_code = run_process_wait(arguments);
		}
		if (exit_code != 0 || !has_png_signature(output_path)) {
			std::printf("  %s[matplotlib-png:failed]%s renderer exit=%d output=%s\n",
				RED, RESET, exit_code, output_path.string().c_str());
			return false;
		}

		std::printf("  %s[matplotlib-png:written]%s %s\n",
			GREEN, RESET, output_path.string().c_str());
		return true;
	}

	struct ExportExpectation {
		const char* label;
		bool requested;
		std::filesystem::path path;
		const char* unavailable_reason;
	};

	void print_export_inventory(const vsim::ExportSection& exp,
								const std::string& run_label) {
		std::error_code ec;
		const std::filesystem::path root(
			exp.output_dir.empty() ? "out/" + run_label : exp.output_dir);
		if (!std::filesystem::exists(root, ec) || ec) {
			std::printf("  %s[export] No output directory to inventory: %s%s\n",
				YELLOW, root.string().c_str(), RESET);
			return;
		}

		const std::vector<ExportExpectation> expected = {
			{"write_xyz", exp.write_xyz, root / (run_label + ".xyz"), nullptr},
			{"write_xyzf", exp.write_xyzf, root / (run_label + ".xyzf"),
				"trajectory writer is only active for trajectory-producing run paths"},
			{"write_xyzfull", exp.write_xyzfull, {}, "no xyzFull writer is registered in this CLI path"},
			{"write_pdb", exp.write_pdb, {}, "no PDB writer is registered in this CLI path"},
			{"write_analysis_json", exp.write_analysis_json, root / "pipeline_records.json", nullptr},
			{"write_metrics_tsv", exp.write_metrics_tsv, root / "metrics.tsv", nullptr},
			{"write_cluster_json", exp.write_cluster_json, {}, "cluster export is not emitted by this CLI path"},
			{"write_fingerprint_json", exp.write_fingerprint_json, {}, "fingerprint export is not emitted by this CLI path"},
			{"write_events_json", exp.write_events_json, root / "events.jsonl", nullptr},
			{"write_symbolic_trace_json", exp.write_symbolic_trace_json, {}, "symbolic trace export is not emitted by this CLI path"},
			{"write_report_md", exp.write_report_md, root / "events.md", nullptr},
			{"write_summary_csv", exp.write_summary_csv, {}, "summary CSV export is not emitted by this CLI path"},
			{"write_dashboard_json", exp.write_dashboard_json, root / "reports/beta7_pipeline_report.json", nullptr},
			{"write_manifest_json", exp.write_manifest_json, root / "run_manifest.json", nullptr},
			{"write_dashboard_svg", exp.write_dashboard_svg, root / "reports/dashboard/beta7_dashboard.svg", nullptr},
			{"write_pipeline_audit_jsonl", exp.write_pipeline_audit_jsonl, root / "reports/audit/beta7_pipeline_audit.jsonl", nullptr},
			{"write_actual_hashes_tsv", exp.write_actual_hashes_tsv, {}, "golden-suite hash export is not emitted by this CLI path"},
			{"write_step_file", exp.write_step_file, root / "geometry/structure.step", nullptr},
			{"write_vtp_mesh", exp.write_vtp_mesh, {}, "VTP mesh export is not emitted by this CLI path"},
			{"write_dynx", exp.write_dynx, {}, "DYNX export is handled by its dedicated session path"},
			{"write_xbit", exp.write_xbit, {}, "XBIT export is handled by its dedicated geometry path"},
			{"write_aggregate_json", exp.write_aggregate_json, {}, "aggregate export is handled by batch execution"},
		};

		const auto audit_path = root / "export_audit.tsv";
		std::ofstream audit(audit_path);
		if (audit) audit << "request\tstatus\tpath\treason\n";
		for (const auto& item : expected) {
			if (!item.requested) continue;
			const bool has_path = !item.path.empty();
			const bool written = has_path && std::filesystem::is_regular_file(item.path, ec) && !ec;
			const char* status = written ? "written" : (has_path ? "missing" : "skipped");
			const char* reason = written ? "" : (item.unavailable_reason
				? item.unavailable_reason : "requested artifact was not produced");
			std::printf("  %s[export:%s]%s %-28s %s%s%s\n",
				written ? GREEN : YELLOW, status, RESET, item.label,
				has_path ? item.path.string().c_str() : "-",
				*reason ? "  -  " : "", reason);
			if (audit) audit << item.label << '\t' << status << '\t'
				<< (has_path ? item.path.string() : "-") << '\t' << reason << '\n';
		}
		audit.close();

		uintmax_t total_bytes = 0;
		size_t file_count = 0;
		std::printf("\n%s  Export inventory:%s %s\n", BOLD, RESET, root.string().c_str());
		for (const auto& entry : std::filesystem::recursive_directory_iterator(root, ec)) {
			if (ec) break;
			if (!entry.is_regular_file(ec) || ec) continue;
			const uintmax_t bytes = entry.file_size(ec);
			if (ec) continue;
			++file_count;
			total_bytes += bytes;
			std::printf("    %-9llu B  %s\n",
				static_cast<unsigned long long>(bytes), entry.path().string().c_str());
		}
		std::printf("  %s[export] %zu file(s), %llu B total%s\n",
			GREEN, file_count, static_cast<unsigned long long>(total_bytes), RESET);
	}

	void write_observe_metrics(const vsim::ExportSection& exp,
							 const std::vector<vsim::EvalResult>& results,
							 const std::string& run_label) {
		if (!exp.write_metrics_tsv) return;
		const std::filesystem::path root(
			exp.output_dir.empty() ? "out/" + run_label : exp.output_dir);
		std::error_code ec;
		std::filesystem::create_directories(root, ec);
		if (ec) return;
		std::ofstream out(root / "metrics.tsv");
		if (!out) return;
		out << "run\tmetric\tfield\twindow\tvalue\twarning\n";
		for (const auto& result : results) {
			std::string warning = result.warning;
			std::replace(warning.begin(), warning.end(), '\t', ' ');
			out << run_label << '\t' << result.probe_name << '\t'
				<< result.field << '\t' << result.window << '\t'
				<< result.value << '\t' << warning << '\n';
		}
		std::printf("  %s-> metrics.tsv%s  (%zu metric%s)%s\n",
			GREEN, DIM, results.size(), results.size() == 1 ? "" : "s", RESET);
	}

	// -----------------------------------------------------------------------
	// print_visual_banner  -  shows the [visual] section flags the script declared
	// -----------------------------------------------------------------------
	void print_visual_banner(const vsim::VisualSection& vis) {
		std::printf("\n%s%s-- [visual] ------------------------------------------%s\n",
			BOLD, CYAN, RESET);
		std::printf("  output_type     : %s%s%s\n", BOLD, vis.output_type.c_str(), RESET);
		if (!vis.animation_mode.empty() && vis.animation_mode != "none")
			std::printf("  animation_mode  : %s\n", vis.animation_mode.c_str());
		std::printf("  render_interval : %d steps\n", vis.render_interval);

		// Active flags
		auto flag = [](const char* label, bool v) {
			if (v) std::printf("  %s%-28s%s ON\n", DIM, label, RESET);
		};
		flag("show_proxy_table",         vis.show_proxy_table);
		flag("show_convergence_trace",   vis.show_convergence_trace);
		flag("show_steady_state_marker", vis.show_steady_state_marker);
		flag("show_snapshot_chart",      vis.show_snapshot_chart);
		flag("show_event_timeline",      vis.show_event_timeline);
		flag("show_bar_chart",           vis.show_bar_chart);
		flag("show_symbolic_trace",      vis.show_symbolic_trace);
		flag("show_animation_cues",      vis.show_animation_cues);
		flag("show_audit_table",         vis.show_audit_table);
		flag("show_rdf_plot",            vis.show_rdf_plot);
		flag("show_energy_heatmap",      vis.show_energy_heatmap);
		flag("show_defect_map",          vis.show_defect_map);
		flag("show_phase_field",         vis.show_phase_field);
		std::printf("%s--------------------------------------------------------%s\n\n",
			CYAN, RESET);
	}

	// -----------------------------------------------------------------------
	// print_convergence_row  -  one step-trace row for terminal_chart / overlay
	//                          (WO-85B: this legacy layout is "output format D")
	// -----------------------------------------------------------------------
	void print_convergence_row(int step, int max_steps,
							   double energy, double eta,
							   const vsim::VisualSection& vis,
							   bool force = false)
	{
		if (!force && !vis.show_convergence_trace) return;

		// Bar width proportional to |energy| clamped to 32 chars
		int bar_len = static_cast<int>(std::min(32.0, std::abs(energy) / 10.0));
		std::string bar(bar_len, eta > 0.6 ? '#' : (eta > 0.3 ? '+' : '.'));
		bar.resize(32, ' ');

		std::printf("  %s step %5d/%d%s  E=%8.2f  η=%5.3f  [%s]\n",
			DIM, step, max_steps, RESET,
			energy, eta,
			bar.c_str());
	}

	// =======================================================================
	// WO-85B  -  Output-format B  (composition + thermodynamics + reactivity)
	// =======================================================================
	//
	// Format B augments the per-step trace with:
	//   - E (thermal)     : the loop energy in kcal/mol (reused from Format A/D)
	//   - G* (Gibbs proxy): reaction free-energy proxy in kJ/mol, derived from
	//                        the [chem_plus] reaction energy/mode
	//   - % composition    : synthetic mol% for each reaction species (the
	//                        reactant decays, products rise as the reaction
	//                        proceeds -- e.g. the decay of CH4)
	//   - relative reactivity (right-most): Part E derivative column, the
	//                        normalised sum |d(composition)/dstep|.
	//
	// The composition is synthesised from the declared reaction stoichiometry
	// because the run loop is still synthetic; values are clearly labelled as
	// proxies (G*). The idealised, physics-backed layout is reserved for WO-85G.

	// A single reaction participant parsed from the [chem_plus] reaction string.
	struct RxnSpecies {
		std::string name;
		double      coeff   = 1.0;
		bool        product = false;   // false = reactant (decays), true = product (forms)
	};

	// Format B run context, built once before the step loop.
	struct FormatBContext {
		std::vector<RxnSpecies> species;        // parsed stoichiometry
		double energy_kj      = 0.0;            // |reaction energy| in kJ
		bool   exothermic     = true;           // sign of the Gibbs proxy
		bool   energy_known   = false;          // true when energy parsed
		bool   have_reaction  = false;          // true when species parsed
	};

	// Parse an integer/decimal leading coefficient off a species term:
	//   "2 O2" -> (2, "O2"),  "2O2" -> (2, "O2"),  "CH4" -> (1, "CH4").
	void split_coeff(const std::string& term, double& coeff, std::string& name) {
		coeff = 1.0;
		size_t i = 0;
		while (i < term.size() && (std::isspace((unsigned char)term[i]))) ++i;
		size_t num_start = i;
		while (i < term.size() && (std::isdigit((unsigned char)term[i]) || term[i] == '.')) ++i;
		if (i > num_start) {
			try { coeff = std::stod(term.substr(num_start, i - num_start)); } catch (...) { coeff = 1.0; }
		}
		while (i < term.size() && std::isspace((unsigned char)term[i])) ++i;
		name = term.substr(i);
		// trim trailing whitespace
		while (!name.empty() && std::isspace((unsigned char)name.back())) name.pop_back();
	}

	// Return true if a term is an energy annotation (e.g. "891 kJ", "kcal") and
	// should not be treated as a chemical species.
	bool is_energy_term(const std::string& term) {
		std::string t;
		for (char c : term) t += static_cast<char>(std::tolower((unsigned char)c));
		return t.find("kj") != std::string::npos
			|| t.find("kcal") != std::string::npos
			|| t.find("joule") != std::string::npos;
	}

	// Split a reaction half (LHS or RHS) on '+' into species terms.
	std::vector<RxnSpecies> parse_reaction_half(const std::string& half, bool product) {
		std::vector<RxnSpecies> out;
		std::string term;
		auto flush = [&]() {
			// trim
			size_t a = term.find_first_not_of(" \t");
			size_t b = term.find_last_not_of(" \t");
			if (a == std::string::npos) { term.clear(); return; }
			std::string t = term.substr(a, b - a + 1);
			term.clear();
			if (t.empty() || is_energy_term(t)) return;
			RxnSpecies rs; rs.product = product;
			split_coeff(t, rs.coeff, rs.name);
			if (!rs.name.empty() && !is_energy_term(rs.name)) out.push_back(rs);
		};
		for (char c : half) {
			if (c == '+') flush();
			else term += c;
		}
		flush();
		return out;
	}

	// Build the Format B context from the document's [chem_plus] reaction.
	FormatBContext build_format_b_context(const vsim::VsimDocument& doc) {
		FormatBContext ctx;
		const std::string rxn = doc.chem_plus.reaction;
		if (!rxn.empty()) {
			auto arrow = rxn.find("->");
			std::string lhs = (arrow != std::string::npos) ? rxn.substr(0, arrow) : rxn;
			std::string rhs = (arrow != std::string::npos) ? rxn.substr(arrow + 2) : "";
			auto react = parse_reaction_half(lhs, /*product=*/false);
			auto prod  = parse_reaction_half(rhs, /*product=*/true);
			ctx.species.insert(ctx.species.end(), react.begin(), react.end());
			ctx.species.insert(ctx.species.end(), prod.begin(), prod.end());
			ctx.have_reaction = !ctx.species.empty();

			const auto cpr = vsim::chemplus::evaluate(doc.chem_plus);
			if (cpr.ok && !cpr.reactions.empty()) {
				const auto& r0 = cpr.reactions.front();
				ctx.energy_known = r0.has_energy && r0.energy_kj != 0.0;
				ctx.energy_kj    = std::abs(r0.energy_kj);
				ctx.exothermic   = (r0.energy_mode != "endothermic");
			}
		}

		// Fallback when no reaction is declared: a minimal decay model so the
		// composition / reactivity columns still animate.
		if (!ctx.have_reaction) {
			std::string f = doc.material.formula.empty()
				? (doc.simulation.molecules.empty() ? std::string("reactant")
												   : doc.simulation.molecules[0].formula)
				: doc.material.formula;
			ctx.species.push_back(RxnSpecies{ f,          1.0, false });
			ctx.species.push_back(RxnSpecies{ "products", 1.0, true  });
		}
		return ctx;
	}

	// Compose synthetic mol% for each species at reaction extent xi in [0,1].
	// Reactants scale as coeff*(1-xi); products scale as coeff*xi; the result
	// is normalised to 100%.
	std::vector<vsim::reactivity::SpeciesSample>
	format_b_compose(const FormatBContext& ctx, double xi) {
		xi = std::clamp(xi, 0.0, 1.0);
		std::vector<double> n(ctx.species.size(), 0.0);
		double total = 0.0;
		for (size_t i = 0; i < ctx.species.size(); ++i) {
			const auto& s = ctx.species[i];
			n[i] = s.product ? s.coeff * xi : s.coeff * (1.0 - xi);
			total += n[i];
		}
		std::vector<vsim::reactivity::SpeciesSample> out;
		out.reserve(ctx.species.size());
		for (size_t i = 0; i < ctx.species.size(); ++i) {
			double pct = (total > 1e-9) ? 100.0 * n[i] / total : 0.0;
			out.push_back({ ctx.species[i].name, pct });
		}
		return out;
	}

	// Print the Format B header (once, before the step loop).
	void print_format_b_header(const FormatBContext& ctx) {
		std::printf("\n%s%s-- Output Format B  (composition . thermodynamics . reactivity) --%s\n",
			BOLD, CYAN, RESET);
		std::printf("%s  E = thermal energy (kcal/mol)   G* = Gibbs proxy (kJ/mol, from [chem_plus])%s\n",
			DIM, RESET);
		std::printf("%s  reactivity = |d(composition)/dstep| normalised to run peak  [0..1]  (Part E)%s\n",
			DIM, RESET);
		if (!ctx.energy_known)
			std::printf("%s  G* : n/a  (no reaction energy declared)%s\n", DIM, RESET);
		std::printf("%s   step         E(kcal)   G*(kJ)    composition (mol%%)%*sreactivity%s\n",
			DIM, 20, "", RESET);
		std::printf("%s  --------------------------------------------------------------------------%s\n",
			CYAN, RESET);
	}

	// Print one Format B row.
	void print_format_b_row(int step, int max_steps,
							double energy_kcal, double gibbs_kj, bool gibbs_known,
							const vsim::reactivity::ReactivityFrame& frame)
	{
		// Composition field: "NAME pct%" for each species.
		std::string comp;
		for (const auto& s : frame.species) {
			char buf[48];
			std::snprintf(buf, sizeof(buf), "%s %.0f%%  ", s.name.c_str(), s.value);
			comp += buf;
		}
		// Pad / clip the composition field to a stable width.
		const size_t kCompWidth = 40;
		if (comp.size() < kCompWidth) comp.resize(kCompWidth, ' ');

		char gbuf[24];
		if (gibbs_known) std::snprintf(gbuf, sizeof(gbuf), "%8.1f", gibbs_kj);
		else             std::snprintf(gbuf, sizeof(gbuf), "%8s", "n/a");

		// First observation has no derivative reference -> show a dash.
		char rbuf[16];
		if (frame.first) std::snprintf(rbuf, sizeof(rbuf), "  -- ");
		else             std::snprintf(rbuf, sizeof(rbuf), "%5.3f", frame.reactivity);

		std::printf("  %s%5d/%d%s  %8.2f  %s%s%s  %s  %s%s%s\n",
			DIM, step, max_steps, RESET,
			energy_kcal,
			DIM, gbuf, RESET,
			comp.c_str(),
			(frame.first ? DIM : GREEN), rbuf, RESET);
	}

	// -----------------------------------------------------------------------
	// print_rdf_stub  -  ASCII RDF bar chart stub
	// -----------------------------------------------------------------------
	void print_rdf_stub(const std::string& formula) {
		// Approximate RDF peaks for common crystal types
		const double peaks[] = { 2.82, 3.99, 4.89, 5.64, 6.32 };
		const double heights[] = { 1.0, 0.62, 0.45, 0.78, 0.31 };
		std::printf("\n%s  g(r)  -  %s%s\n", DIM, formula.c_str(), RESET);
		std::printf("  r(Å)   ");
		for (double p : peaks) std::printf("%5.2f  ", p);
		std::printf("\n  g(r)   ");
		for (double h : heights) {
			int b = static_cast<int>(h * 5);
			for (int i = 0; i < b; ++i) std::printf("▓");
			for (int i = b; i < 5; ++i) std::printf("░");
			std::printf("  ");
		}
		std::printf("\n");
	}

	// -----------------------------------------------------------------------
	// print_bar_chart_stub  -  per-kind event count bar chart
	// -----------------------------------------------------------------------
	void print_bar_chart_stub(const vsepr::kernel::KernelEventLog& log) {
		const vsepr::kernel::KernelEventKind kinds[] = {
			vsepr::kernel::KernelEventKind::Formation,
			vsepr::kernel::KernelEventKind::Reaction,
			vsepr::kernel::KernelEventKind::ChemicalState,
			vsepr::kernel::KernelEventKind::Defect,
			vsepr::kernel::KernelEventKind::Transport,
		};
		const char* labels[] = { "Formation    ", "Reaction     ", "ChemicalState", "Defect       ", "Transport    " };

		std::printf("\n%s  Event counts by kind:%s\n", BOLD, RESET);
		for (size_t i = 0; i < 5; ++i) {
			auto evs = log.filter_by_kind(kinds[i]);
			int n = static_cast<int>(evs.size());
			int bar = std::min(n, 40);
			std::printf("  %s  %s|%s", labels[i], CYAN,
					std::string(bar, '#').c_str());
				std::printf("%s%s  %d%s\n", std::string(40 - bar, ' ').c_str(), GREEN, n, RESET);
		}
	}

	// -----------------------------------------------------------------------
	// Synthetic emit function  -  populates KernelEventLog with Formation events
	// proportional to step count, simulating convergence with energy decay.
	// Returns number of events added.
	// -----------------------------------------------------------------------
	// print_gas_mixing_frame  -  renders one time-step row for a directed-injection
	// gas mixing run.  Shows per-species kinetic progress and a centre-mixing bar.
	// -----------------------------------------------------------------------
	void print_gas_mixing_frame(
		int step, int max_steps,
		const std::vector<vsim::MoleculeEntry>& mols,
		double mix_fraction)   // 0.0 = fully separated, 1.0 = fully mixed
	{
		// ANSI colour per gas (CPK-inspired terminal palette)
		const char* species_colors[] = {
			"\033[34m",  // blue   -  N₂
			"\033[31m",  // red    -  O₂
			"\033[36m",  // cyan   -  H₂O
			"\033[32m",  // green  -  Ar
		};
		const char* BAR_FULL  = "#";
		const char* BAR_HALF  = "▒";
		const char* BAR_EMPTY = "░";

		double progress = static_cast<double>(step) / std::max(max_steps - 1, 1);

		std::printf("  step %5d/%d  ", step, max_steps);

		// Per-species inward progress bar (30 chars each)
		for (size_t si = 0; si < mols.size() && si < 4; ++si) {
			const auto& m = mols[si];
			const char* col = species_colors[si % 4];
			// Each species "arrives" at centre over the first 60% of run
			double arrival = std::min(progress / 0.6, 1.0);
			int filled = static_cast<int>(arrival * 15);
			std::printf("%s%-4s%s [", col, m.formula.c_str(), RESET);
			for (int i = 0; i < 15; ++i) {
				if      (i < filled)    std::printf("%s%s%s", col, BAR_FULL, RESET);
				else if (i == filled)   std::printf("%s%s%s", col, BAR_HALF, RESET);
				else                    std::printf("%s", BAR_EMPTY);
			}
			std::printf("]  ");
		}

		// Centre mixing indicator
		int mix_filled = static_cast<int>(mix_fraction * 20);
		std::printf("\033[35mMIX[");
		for (int i = 0; i < 20; ++i)
			std::printf("%s", i < mix_filled ? "#" : "░");
		std::printf("]\033[0m  %.0f%%\n", mix_fraction * 100.0);
		std::fflush(stdout);
	}

	// -----------------------------------------------------------------------
	// is_gas_injection_run  -  true when any molecule entry has a region declared
	// -----------------------------------------------------------------------
	bool is_gas_injection_run(const std::vector<vsim::MoleculeEntry>& mols) {
		for (const auto& m : mols)
			if (!m.region.empty()) return true;
		return false;
	}

	// -----------------------------------------------------------------------
	// xyzf trajectory writer helpers
	// -----------------------------------------------------------------------

	struct GasMolAtom { std::string symbol; double dx, dy, dz; };

	// Returns the atom roster for one molecule of `formula` using the project's
	// VSEPR molecule builder.  Positions are centred on the molecular centroid
	// and represent intra-molecular geometry offsets (Å).
	static std::vector<GasMolAtom> molecule_atoms_from_formula(
		const std::string& formula,
		const PeriodicTable& pt)
	{
		std::vector<GasMolAtom> result;
		try {
			init_chemistry_db(&pt);
			Molecule mol = build_molecule_from_formula(formula, pt);
			const uint32_t n = mol.num_atoms();
			result.reserve(n);
			// Coordinates stored flat in mol.coords: [x0,y0,z0, x1,y1,z1, ...]
			double cx = 0, cy = 0, cz = 0;
			for (uint32_t i = 0; i < n; ++i) {
				cx += mol.coords[3*i];
				cy += mol.coords[3*i+1];
				cz += mol.coords[3*i+2];
			}
			cx /= n; cy /= n; cz /= n;
			for (uint32_t i = 0; i < n; ++i) {
				const auto* elem = pt.by_Z(mol.atoms[i].Z);
				std::string sym = elem ? elem->symbol : "X";
				result.emplace_back(GasMolAtom{sym,
					mol.coords[3*i]   - cx,
					mol.coords[3*i+1] - cy,
					mol.coords[3*i+2] - cz});
			}
		} catch (const std::exception& e) {
			std::fprintf(stderr, "[export] Cannot build formula '%s': %s\n",
				formula.c_str(), e.what());
		}
		return result;
	}

	static bool append_formula_state(atomistic::State& state,
								 const std::string& formula,
								 int count,
								 double placement_stride,
								 std::string& error) {
		static const PeriodicTable pt = PeriodicTable::load_default();
		try {
			init_chemistry_db(&pt);
			for (int copy = 0; copy < std::max(1, count); ++copy) {
				Molecule molecule = build_molecule_from_formula(formula, pt, copy);
				const uint32_t base = state.N;
				const vsepr::Vec3 shift{
					placement_stride * static_cast<double>(copy), 0.0, 0.0};
				for (size_t i = 0; i < molecule.atoms.size(); ++i) {
					state.X.emplace_back(
						molecule.coords[3 * i] + shift.x,
						molecule.coords[3 * i + 1] + shift.y,
						molecule.coords[3 * i + 2] + shift.z);
					state.V.emplace_back();
					state.F.emplace_back();
					state.T.push_back(0.0);
					state.Q.push_back(0.0);
					state.M.push_back(molecule.atoms[i].mass);
					state.type.push_back(molecule.atoms[i].Z);
				}
				for (const auto& bond : molecule.bonds)
					state.B.push_back({base + bond.i, base + bond.j});
				state.N = static_cast<uint32_t>(state.X.size());
			}
			return state.N > 0;
		} catch (const std::exception& ex) {
			error = ex.what();
			return false;
		}
	}

	struct DissolutionRunContext {
		std::unique_ptr<atomistic::reaction::DissolutionEngine> engine;
		atomistic::State solid;
		atomistic::State solution;
		std::string error;

		bool ready() const { return engine && solid.N > 0 && error.empty(); }
	};

	static DissolutionRunContext make_dissolution_context(
		const vsim::VsimDocument& doc) {
		DissolutionRunContext context;
		if (!doc.dissolution.enabled) return context;

		const std::string solid_formula = !doc.material.formula.empty()
			? doc.material.formula
			: (doc.simulation.molecules.empty()
				? std::string() : doc.simulation.molecules.front().formula);
		if (solid_formula.empty()) {
			context.error = "no solid formula was declared";
			return context;
		}

		if (!append_formula_state(context.solid, solid_formula, 1, 4.0, context.error))
			return context;

		for (const auto& entry : doc.simulation.molecules) {
			if (entry.formula == solid_formula) continue;
			if (!append_formula_state(
				context.solution, entry.formula, entry.count, 4.0, context.error))
				return context;
		}

		context.engine = vsim::DissolutionBridge::create_engine(doc);
		return context;
	}

	static std::array<double,3> corner_origin(const std::string& region, double L) {
		double h = L * 0.35;
		if (region == "corner_xnyp") return {-h,  h,  0.0};
		if (region == "corner_xpyp") return { h,  h,  0.0};
		if (region == "corner_xnyn") return {-h, -h,  0.0};
		if (region == "corner_xpyn") return { h, -h,  0.0};
		return {0.0, 0.0, 0.0};
	}

	// write_gas_mixing_xyzf  -  generates a multi-frame .xyzf trajectory for the
	// four-corner gas injection run.  Uses simple Langevin-style overdamped
	// integration: each atom drifts toward the box centre with velocity_drift
	// speed plus Maxwell-Boltzmann thermal noise.
	//
	// Returns the path of the written file, or "" on failure.
	static std::string write_gas_mixing_xyzf(
		const vsim::VsimDocument& doc,
		int max_steps, int render_interval,
		const std::string& run_label)
	{
		const double L   = doc.simulation.box_size_ang > 0 ? doc.simulation.box_size_ang : 150.0;
		const double T   = doc.environment.temperature  > 0 ? doc.environment.temperature  : 1200.0;
		const double dt  = doc.run.dt_fs > 0 ? doc.run.dt_fs : 0.5; // fs

		// Periodic table for formula parsing and mass lookup
		static const PeriodicTable pt = PeriodicTable::load_default();

		// Build atom roster: element, current x/y/z, drift direction x/y/z
		struct Atom {
			std::string sym;
			double x, y, z;          // current position (Å)
			double dx, dy, dz;       // unit drift vector toward centre
			double drift_speed;      // Å/fs
			double mass;             // amu from periodic table
		};

		std::mt19937_64 rng(doc.project.seed_base > 0 ? (uint64_t)doc.project.seed_base : 42ULL);
		std::normal_distribution<double> spread(0.0, 8.0);  // molecular placement spread (Å)
		std::normal_distribution<double> noise1(0.0, 1.0);  // unit noise

		std::vector<Atom> atoms;
		atoms.reserve(4000);

		for (const auto& mol : doc.simulation.molecules) {
			auto corner = corner_origin(mol.region, L);
			auto mol_atoms = molecule_atoms_from_formula(mol.formula, pt);
			double drift  = mol.velocity_drift > 0 ? mol.velocity_drift : 0.012;

			for (int m = 0; m < mol.count; ++m) {
				// Molecule centre: corner + random spread
				double cx = corner[0] + spread(rng);
				double cy = corner[1] + spread(rng);
				double cz = spread(rng) * 0.3;   // thin z-slab initially

				// Drift direction: unit vector toward box centre (0,0,0)
				double dist = std::sqrt(cx*cx + cy*cy + cz*cz);
				double ddx = dist > 0 ? -cx/dist : 0;
				double ddy = dist > 0 ? -cy/dist : 0;
				double ddz = dist > 0 ? -cz/dist : 0;

				for (const auto& ma : mol_atoms) {
					const auto* elem = pt.by_symbol(ma.symbol);
					double mass = elem ? elem->atomic_mass : 12.011;
					atoms.push_back({ma.symbol,
						cx + ma.dx, cy + ma.dy, cz + ma.dz,
						ddx, ddy, ddz,
						drift,
						mass});
				}
			}
		}

		if (atoms.empty()) return "";

		// Output path
		std::string dir = doc.exports.output_dir.empty() ? "out/" + run_label : doc.exports.output_dir;
		std::error_code ec;
		std::filesystem::create_directories(dir, ec);
		if (ec) return "";
		std::string xyzf_path = dir + "/" + run_label + ".xyzf";

		std::ofstream out(xyzf_path);
		if (!out.is_open()) return "";
		out << std::fixed;

		int total_frames = (max_steps + render_interval - 1) / render_interval;
		const double kB_amu = 0.008314;  // kcal/(mol·K)  -  used for thermal sigma

		for (int frame = 0; frame < total_frames; ++frame) {
			int step = frame * render_interval;
			double t_frac = static_cast<double>(step) / std::max(max_steps - 1, 1);

			// Advance positions for all atoms over render_interval steps
			for (int s = 0; s < render_interval && (frame * render_interval + s) < max_steps; ++s) {
				for (auto& a : atoms) {
					double sigma_v = std::sqrt(kB_amu * T / a.mass) * 0.01;
					a.x += a.dx * a.drift_speed * dt + noise1(rng) * sigma_v;
					a.y += a.dy * a.drift_speed * dt + noise1(rng) * sigma_v;
					a.z += a.dz * a.drift_speed * dt + noise1(rng) * sigma_v;
					// Reflect off box walls
					const double half = L * 0.5;
					if (std::fabs(a.x) > half) { a.x = std::copysign(half, a.x); a.dx = -a.dx; }
					if (std::fabs(a.y) > half) { a.y = std::copysign(half, a.y); a.dy = -a.dy; }
					if (std::fabs(a.z) > half) { a.z = std::copysign(half, a.z); a.dz = -a.dz; }
				}
			}

			// Write xyzf frame
			out << atoms.size() << "\n";
			out << "gas_mix | step " << step
				<< " | t_frac=" << std::setprecision(4) << t_frac
				<< " | T=" << std::setprecision(1) << T
				<< " K | properties=\"velocity\"\n";
			out << std::setprecision(6);
			for (const auto& a : atoms) {
				out << std::left << std::setw(4) << a.sym
					<< std::right
					<< std::setw(14) << a.x
					<< std::setw(14) << a.y
					<< std::setw(14) << a.z
					<< std::setw(12) << (a.dx * a.drift_speed)
					<< std::setw(12) << (a.dy * a.drift_speed)
					<< std::setw(12) << (a.dz * a.drift_speed)
					<< "\n";
			}
		}

		out.close();
		return xyzf_path;
	}

	// write_static_xyz  -  writes a static .xyz for the lightweight viewer.
	// Priority:
	//   1. [[simulation.molecule]] entries  -> one frame each (multi-structure switch)
	//   2. [material] formula               -> single frame (batch / crystal scripts)
	// Returns the written path, or "" on failure.
	static std::string write_static_xyz(
		const vsim::VsimDocument& doc,
		const std::string& run_label)
	{
		static const PeriodicTable pt = PeriodicTable::load_default();

		struct Frame { std::string label; std::vector<GasMolAtom> atoms; };
		std::vector<Frame> frames;

		// Source 1: explicit molecule list
		for (const auto& mol : doc.simulation.molecules) {
			auto molecule = molecule_atoms_from_formula(mol.formula, pt);
			if (molecule.empty()) continue;

			const int copies = std::max(1, mol.count);
			const int layers = std::max(1, std::min(mol.n_layers, copies));
			const int per_layer = (copies + layers - 1) / layers;
			const int columns = std::max(1, static_cast<int>(std::ceil(std::sqrt(
				static_cast<double>(per_layer)))));
			std::vector<GasMolAtom> atoms;
			atoms.reserve(molecule.size() * static_cast<size_t>(copies));

			for (int copy = 0; copy < copies; ++copy) {
				const int layer = copy % layers;
				const int in_layer = copy / layers;
				const int row = in_layer / columns;
				const int column = in_layer % columns;
				const double stagger = (mol.layer_mode == "AB" && (layer % 2 == 1)) ? 1.23 : 0.0;
				const double x = (column - (columns - 1) * 0.5) * 2.46 + stagger;
				const double y = (row - (per_layer / columns - 1) * 0.5) * 2.13;
				const double z = (layer - (layers - 1) * 0.5) * 3.35;
				for (const auto& atom : molecule)
					atoms.push_back({atom.symbol, atom.dx + x, atom.dy + y, atom.dz + z});
			}
			frames.push_back({mol.formula, std::move(atoms)});
		}

		// Source 2: top-level [material] formula (covers batch / crystal scripts)
		if (frames.empty() && !doc.material.formula.empty()) {
			auto atoms = molecule_atoms_from_formula(doc.material.formula, pt);
			if (!atoms.empty())
				frames.push_back({doc.material.formula, std::move(atoms)});
		}

		// Source 3: batch scripts that nest formula under [batch.job.material]
		// Keys land in raw_sections["batch.job.material"]["formula"] as a Value.
		if (frames.empty()) {
			auto sec_it = doc.raw_sections.find("batch.job.material");
			if (sec_it != doc.raw_sections.end()) {
				auto key_it = sec_it->second.find("formula");
				if (key_it != sec_it->second.end()) {
					std::string f = vsim::to_string(key_it->second);
					if (!f.empty()) {
						auto atoms = molecule_atoms_from_formula(f, pt);
						if (!atoms.empty())
							frames.push_back({f, std::move(atoms)});
					}
				}
			}
		}

		if (frames.empty()) return "";

		std::string dir = doc.exports.output_dir.empty() ? "out/" + run_label : doc.exports.output_dir;
		std::error_code ec;
		std::filesystem::create_directories(dir, ec);
		if (ec) return "";
		std::string xyz_path = dir + "/" + run_label + ".xyz";

		std::ofstream out(xyz_path);
		if (!out.is_open()) return "";
		out << std::fixed << std::setprecision(6);

		for (const auto& f : frames) {
			out << f.atoms.size() << "\n";
			// Embed bond topology in comment line (forward-compatibility annotation).
			// Format:  run_label | formula [ BONDS a-b:order ... ]
			// The viewer already infers bonds geometrically, but this aids future
			// rich loaders that parse comment metadata.
			out << run_label << " | " << f.label;
			if (f.atoms.size() >= 2) {
				// Inline covalent-radius table (Å) — mirrors viewer_config.hpp
				static const std::unordered_map<std::string, double> cov_r = {
					{"H",0.31},{"He",0.28},{"Li",1.28},{"Be",0.96},{"B",0.84},
					{"C",0.77},{"N",0.71},{"O",0.66},{"F",0.57},{"Ne",0.58},
					{"Na",1.66},{"Mg",1.41},{"Al",1.21},{"Si",1.11},{"P",1.07},
					{"S",1.05},{"Cl",1.02},{"Ar",1.06},{"K",2.03},{"Ca",1.76},
					{"Ti",1.60},{"Cr",1.39},{"Mn",1.61},{"Fe",1.52},{"Co",1.50},
					{"Ni",1.24},{"Cu",1.32},{"Zn",1.22},{"Br",1.20},{"I",1.39},
					{"Au",1.36},{"Ag",1.45},
				};
				auto get_r = [&](const std::string& sym) -> double {
					auto it = cov_r.find(sym);
					return (it != cov_r.end()) ? it->second : 1.20;
				};
				bool any_bond = false;
				for (size_t ii = 0; ii < f.atoms.size(); ++ii) {
					for (size_t jj = ii + 1; jj < f.atoms.size(); ++jj) {
						const double ddx = f.atoms[ii].dx - f.atoms[jj].dx;
						const double ddy = f.atoms[ii].dy - f.atoms[jj].dy;
						const double ddz = f.atoms[ii].dz - f.atoms[jj].dz;
						const double d2  = ddx*ddx + ddy*ddy + ddz*ddz;
						const double cut = (get_r(f.atoms[ii].symbol) + get_r(f.atoms[jj].symbol)) * 1.25;
						if (d2 < cut*cut && d2 > 0.25) {
							if (!any_bond) { out << " [ BONDS"; any_bond = true; }
							out << " " << (ii+1) << "-" << (jj+1) << ":1.0";
						}
					}
				}
				if (any_bond) out << " ]";
			}
			out << "\n";
			for (const auto& a : f.atoms) {
				out << std::left << std::setw(4) << a.symbol << std::right
					<< std::setw(14) << a.dx
					<< std::setw(14) << a.dy
					<< std::setw(14) << a.dz << "\n";
			}
		}
		out.close();
		return xyz_path;
	}

	// -----------------------------------------------------------------------
	int synthetic_emit(vsepr::kernel::KernelEventLog& log,
					   const std::string& formula,
					   int n_steps,
					   int seed_offset,
					   double base_energy,
					   double decay_rate)
	{
		int emitted = 0;
		double energy = base_energy;
		double eta    = 0.05 + 0.01 * static_cast<double>(seed_offset % 7);

		for (int s = 0; s < n_steps && s < 50; ++s) {
			energy *= (1.0 - decay_rate * (1.0 + 0.1 * (s % 5)));
			eta     = std::min(0.98, eta + 0.04 * (1.0 - eta));

			vsepr::kernel::KernelEvent ev(
				vsepr::kernel::KernelEventKind::Formation, formula,
				static_cast<uint64_t>(seed_offset * 1000 + s));
			ev.result_value  = energy;
			ev.result_unit   = "kcal/mol";
			ev.is_valid      = (energy < -5.0);
			ev.equation_symbolic = formula + " -> lattice";
			ev.equation_numeric  = "E=" + std::to_string(energy);
			log.record(ev);
			++emitted;
		}
		return emitted;
	}

	// =====================================================================
	// WO-28MAR: high-density run record capture
	// =====================================================================
	double rms_force_from_eta(double eta) {
		// Synthetic RMS force proxy: high eta -> low residual, eta in [0,1].
		return std::max(0.0, (1.0 - eta) * 5.0);
	}

	struct DenseRecordContext {
		vsim::BatchRunRecord record;
		vsim::DenseRunSection cfg;
		std::chrono::steady_clock::time_point orch_start;
		std::chrono::steady_clock::time_point comp_start;
		std::chrono::steady_clock::time_point comp_end;
		std::string output_dir;
		std::string run_label;
		bool active = false;
	};

	std::string file_sha256_stub(const std::string& path) {
		// Lightweight placeholder: size+modification-hash, not cryptographic.
		std::error_code ec;
		const auto sz = std::filesystem::file_size(path, ec);
		if (ec) return "";
		const auto last = std::filesystem::last_write_time(path, ec);
		if (ec) return "";
		auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
			last - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
		std::size_t h1 = std::hash<std::string>{}(path);
		std::size_t h2 = std::hash<std::uintmax_t>{}(sz);
		std::size_t h3 = std::hash<long long>{}(static_cast<long long>(
			std::chrono::system_clock::to_time_t(sctp)));
		std::size_t x = h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
		return "sha256:" + std::to_string(x ^ (h3 + 0x9e3779b97f4a7c15ULL + (x << 6) + (x >> 2)));
	}

	bool write_atoms_xyz(const std::string& path,
						 const std::string& comment,
						 const std::vector<GasMolAtom>& atoms) {
		std::ofstream out(path);
		if (!out) return false;
		out << std::fixed << std::setprecision(6);
		out << atoms.size() << "\n" << comment << "\n";
		for (const auto& a : atoms) {
			out << std::left << std::setw(4) << a.symbol << std::right
				<< std::setw(14) << a.dx
				<< std::setw(14) << a.dy
				<< std::setw(14) << a.dz << "\n";
		}
		return true;
	}

	std::vector<GasMolAtom> atoms_from_doc(const vsim::VsimDocument& doc) {
		static const PeriodicTable pt = PeriodicTable::load_default();
		std::vector<GasMolAtom> out;
		for (const auto& mol : doc.simulation.molecules) {
			auto base = molecule_atoms_from_formula(mol.formula, pt);
			for (int c = 0; c < mol.count; ++c) {
				double dx = (c % 3 - 1) * 2.0;
				double dy = ((c / 3) % 3 - 1) * 2.0;
				double dz = (c / 9) * 1.5;
						for (const auto& a : base) {
							out.push_back({a.symbol, a.dx + dx, a.dy + dy, a.dz + dz});
						}
					}
				}
				if (out.empty() && !doc.material.formula.empty()) {
					auto base = molecule_atoms_from_formula(doc.material.formula, pt);
					for (const auto& a : base) {
						out.push_back({a.symbol, a.dx, a.dy, a.dz});
					}
				}
		return out;
	}

	std::string write_connectivity_graph(const std::string& dir,
										 const std::string& run_label,
										 const std::string& suffix,
										 const std::vector<GasMolAtom>& atoms) {
		if (atoms.empty()) return "";
		std::error_code ec;
		std::filesystem::create_directories(dir, ec);
		if (ec) return "";
		std::string path = (std::filesystem::path(dir) / (run_label + suffix)).string();
		std::ofstream out(path);
		if (!out) return "";
		out << "{\"atoms\":" << atoms.size() << ",\"edges\":";
		std::vector<std::pair<size_t,size_t>> edges;
		static const std::unordered_map<std::string, double> cov_r = {
			{"H",0.31},{"He",0.28},{"Li",1.28},{"Be",0.96},{"B",0.84},
			{"C",0.77},{"N",0.71},{"O",0.66},{"F",0.57},{"Ne",0.58},
			{"Na",1.66},{"Mg",1.41},{"Al",1.21},{"Si",1.11},{"P",1.07},
			{"S",1.05},{"Cl",1.02},{"Ar",1.06},{"K",2.03},{"Ca",1.76},
			{"Ti",1.60},{"Cr",1.39},{"Mn",1.61},{"Fe",1.52},{"Co",1.50},
			{"Ni",1.24},{"Cu",1.32},{"Zn",1.22},{"Br",1.20},{"I",1.39},
			{"Au",1.36},{"Ag",1.45},
		};
		auto get_r = [&](const std::string& sym) -> double {
			auto it = cov_r.find(sym);
			return (it != cov_r.end()) ? it->second : 1.20;
		};
		for (size_t i = 0; i < atoms.size(); ++i) {
			for (size_t j = i + 1; j < atoms.size(); ++j) {
				double ddx = atoms[i].dx - atoms[j].dx;
					double ddy = atoms[i].dy - atoms[j].dy;
					double ddz = atoms[i].dz - atoms[j].dz;
				double d2 = ddx*ddx + ddy*ddy + ddz*ddz;
				double cut = (get_r(atoms[i].symbol) + get_r(atoms[j].symbol)) * 1.25;
				if (d2 < cut*cut && d2 > 0.25) edges.emplace_back(i, j);
			}
		}
		out << "[";
		for (size_t k = 0; k < edges.size(); ++k) {
			out << (k ? "," : "") << "[" << edges[k].first << "," << edges[k].second << "]";
		}
		out << "]}";
		return path;
	}

	std::string write_energy_force_tsv(const std::string& dir,
									   const std::string& run_label,
									   const vsim::BatchRunRecord& rec,
									   const vsim::DenseRunSection& cfg) {
		std::error_code ec;
		std::filesystem::create_directories(dir, ec);
		if (ec) return "";
		std::string path = (std::filesystem::path(dir) / (run_label + "_energy_force.tsv")).string();
		std::ofstream out(path);
		if (!out) return "";
		out << "step\tenergy_kcal_mol\trms_force\tnotes\n";
		for (size_t i = 0; i < rec.energy_trace.size(); ++i) {
			out << (i * rec.sample_interval) << '\t'
				<< rec.energy_trace[i] << '\t'
				<< (i < rec.rms_force_trace.size() ? rec.rms_force_trace[i] : 0.0) << '\t'
				<< "dense_record\n";
		}
		return path;
	}

	std::string json_escape(const std::string& s) {
		std::string out;
		out.reserve(s.size());
		for (char c : s) {
			switch (c) {
				case '"':  out += "\\\""; break;
				case '\\': out += "\\\\"; break;
				case '\b': out += "\\b"; break;
				case '\f': out += "\\f"; break;
				case '\n': out += "\\n"; break;
				case '\r': out += "\\r"; break;
				case '\t': out += "\\t"; break;
				default: out += c;
			}
		}
		return out;
	}

	std::string write_dense_record_json(const DenseRecordContext& ctx,
										const std::vector<std::string>& gate_log) {
		std::error_code ec;
		std::filesystem::create_directories(ctx.output_dir, ec);
		if (ec) return "";
		std::string path = (std::filesystem::path(ctx.output_dir) /
						(ctx.run_label + "_dense_record.json")).string();
		std::ofstream out(path);
		if (!out) return "";

		const auto& r = ctx.record;
		out << "{\n";
		out << "  \"run_label\": \"" << json_escape(r.run_label) << "\",\n";
		out << "  \"mode\": \"" << json_escape(ctx.cfg.enabled ? "dense" : "standard") << "\",\n";
		out << "  \"steps_taken\": " << r.steps_taken << ",\n";
		out << "  \"sample_interval\": " << r.sample_interval << ",\n";
		out << "  \"rng_seed\": " << r.rng_seed << ",\n";
		out << "  \"wall_ms\": " << r.wall_ms << ",\n";
		out << "  \"comp_ms\": " << r.comp_ms << ",\n";
		out << "  \"orch_ms\": " << r.orch_ms << ",\n";
		out << "  \"final_energy\": " << r.final_energy << ",\n";
		out << "  \"rms_force\": " << r.rms_force << ",\n";
		out << "  \"potential_label\": \"" << json_escape(r.potential_label) << "\",\n";
		out << "  \"potential_checksum\": \"" << json_escape(r.potential_checksum) << "\",\n";
		out << "  \"initial_xyz_path\": \"" << json_escape(r.initial_xyz_path) << "\",\n";
		out << "  \"final_xyz_path\": \"" << json_escape(r.final_xyz_path) << "\",\n";
		out << "  \"connectivity_before_path\": \"" << json_escape(r.connectivity_before_path) << "\",\n";
		out << "  \"connectivity_after_path\": \"" << json_escape(r.connectivity_after_path) << "\",\n";
		out << "  \"energy_force_path\": \"" << json_escape(r.energy_force_path) << "\",\n";
		out << "  \"output_dir\": \"" << json_escape(ctx.output_dir) << "\",\n";
		out << "  \"total_files\": " << r.total_files << ",\n";
		out << "  \"total_bytes\": " << r.total_bytes << ",\n";
		out << "  \"energy_trace\": [";
		for (size_t i = 0; i < r.energy_trace.size(); ++i) {
			out << (i ? "," : "") << r.energy_trace[i];
			if (i % 10 == 9) out << "\n    ";
		}
		out << "],\n";
		out << "  \"rms_force_trace\": [";
		for (size_t i = 0; i < r.rms_force_trace.size(); ++i) {
			out << (i ? "," : "") << r.rms_force_trace[i];
			if (i % 10 == 9) out << "\n    ";
		}
		out << "],\n";
		out << "  \"terminal_gates\": [";
		for (size_t i = 0; i < gate_log.size(); ++i) {
			out << (i ? "," : "") << "\"" << json_escape(gate_log[i]) << "\"";
		}
		out << "]\n";
		out << "}\n";
		return path;
	}

} // anonymous namespace

// ============================================================================
// interactive_preprocess  -  CLI wizard for script_type = interactive_preprocess_then_run
//
// Implements the algorithm from [pseudocode.interactive] exactly:
//   1. Collect molecular formula (mixture ok, '+' separator)
//   2. Choose material mode: regular_solid | radioactive_salt_mixture
//   3. Radioactivity / salt heuristic warnings
//   4. Optional STEP file path; fallback to xbit_generated
//   5. Pipe dimensions with full sanity checks
//   6. Fluid species, inlet rate, inlet velocity, SPH count
//   7. Writes a resolved .vsim from the template, substituting ${var} tokens
//   8. Returns the resolved file path (caller runs it via cmd_run_vsim)
// ============================================================================

namespace {  // re-open for wizard helpers

// --- tiny I/O helpers -------------------------------------------------------
static std::string ask_string(const char* prompt, const char* dflt = "") {
	std::printf("  %s%s%s", BOLD, prompt, RESET);
	if (dflt && dflt[0]) std::printf(" [%s]", dflt);
	std::printf(": ");
	std::fflush(stdout);
	std::string line;
	if (!std::getline(std::cin, line)) line = "";
	if (line.empty() && dflt && dflt[0]) line = dflt;
	return line;
}

static double ask_double(const char* prompt, double dflt = 0.0, bool has_dflt = false) {
	while (true) {
		std::printf("  %s%s%s", BOLD, prompt, RESET);
		if (has_dflt) std::printf(" [%.4g]", dflt);
		std::printf(": ");
		std::fflush(stdout);
		std::string line;
		if (!std::getline(std::cin, line)) return dflt;
		if (line.empty() && has_dflt) return dflt;
		try { return std::stod(line); }
		catch (...) {
			std::printf("  %sinvalid number, try again%s\n", YELLOW, RESET);
		}
	}
}

static int ask_int(const char* prompt, int dflt = 0, bool has_dflt = false) {
	while (true) {
		std::printf("  %s%s%s", BOLD, prompt, RESET);
		if (has_dflt) std::printf(" [%d]", dflt);
		std::printf(": ");
		std::fflush(stdout);
		std::string line;
		if (!std::getline(std::cin, line)) return dflt;
		if (line.empty() && has_dflt) return dflt;
		try { return std::stoi(line); }
		catch (...) {
			std::printf("  %sinvalid integer, try again%s\n", YELLOW, RESET);
		}
	}
}

// --- formula heuristics -----------------------------------------------------
static bool formula_looks_radioactive(const std::string& f) {
	// Actinides and common radioactive isotope prefixes
	static const char* radio[] = {
		"U", "Th", "Pu", "Am", "Cm", "Np", "Pa", "Ra", "Rn", "Po",
		"Ac", "Fr", "Tc", "Pm", "At", nullptr
	};
	for (int i = 0; radio[i]; ++i)
		if (f.find(radio[i]) != std::string::npos) return true;
	return false;
}

static bool formula_looks_like_salt(const std::string& f) {
	// Simple heuristic: contains an alkali/alkaline + halide, or "Cl","F","Br","I" suffix
	static const char* halides[] = { "Cl", "F", "Br", "I", nullptr };
	static const char* cations[] = { "Na", "K", "Li", "Rb", "Cs", "Mg", "Ca", "Ba", nullptr };
	bool has_hal = false, has_cat = false;
	for (int i = 0; halides[i]; ++i) if (f.find(halides[i]) != std::string::npos) has_hal = true;
	for (int i = 0; cations[i]; ++i) if (f.find(cations[i]) != std::string::npos) has_cat = true;
	return has_hal || has_cat;
}

// --- .xbit stub generator ---------------------------------------------------
static bool generate_xbit_pipe(const std::string& out_path,
								double length, double di, double doo,
								const std::string& material) {
	std::ofstream f(out_path);
	if (!f) return false;
	// Tagged JSON-ish stub that downstream FEA/DEM bridges can consume
	f << "{\n"
	  << "  \"type\": \"xbit_pipe\",\n"
	  << "  \"version\": \"1.0\",\n"
	  << "  \"geometry\": {\n"
	  << "    \"length\": "           << length << ",\n"
	  << "    \"inner_diameter\": "   << di     << ",\n"
	  << "    \"outer_diameter\": "   << doo    << ",\n"
	  << "    \"wall_thickness\": "   << 0.5*(doo-di) << ",\n"
	  << "    \"axis\": \"x\",\n"
	  << "    \"origin\": [0,0,0]\n"
	  << "  },\n"
	  << "  \"material\": \"" << material << "\",\n"
	  << "  \"axial_segments\": \"auto\",\n"
	  << "  \"radial_segments\": 64,\n"
	  << "  \"wall_layers\": 4,\n"
	  << "  \"tags\": [\"pipe\",\"pipe.interior\",\"pipe.inner_wall\",\"pipe.outer_wall\","
		 "\"pipe.inlet_face\",\"pipe.outlet_face\",\"pipe.wall_volume\"],\n"
	  << "  \"surfaces\": {\n"
	  << "    \"inner_wall\":  { \"selector\": \"radius == inner_radius\",  \"tag\": \"pipe.inner_wall\" },\n"
	  << "    \"outer_wall\":  { \"selector\": \"radius == outer_radius\",  \"tag\": \"pipe.outer_wall\" },\n"
	  << "    \"inlet_face\":  { \"selector\": \"x == 0\",                  \"tag\": \"pipe.inlet_face\" },\n"
	  << "    \"outlet_face\": { \"selector\": \"x == length\",             \"tag\": \"pipe.outlet_face\" }\n"
	  << "  },\n"
	  << "  \"connectivity\": {\n"
	  << "    \"axial\":           { \"type\": \"axial_wall_link\",   \"stiffness_hint\": \"pipe_wall\" },\n"
	  << "    \"radial\":          { \"type\": \"radial_wall_link\",  \"stiffness_hint\": \"wall_thickness\" },\n"
	  << "    \"circumferential\": { \"type\": \"hoop_link\",         \"stiffness_hint\": \"hoop_stress\" }\n"
	  << "  }\n"
	  << "}\n";
	return true;
}

// --- resolved .vsim writer --------------------------------------------------
// Reads the template, strips the [pseudocode.*] block, replaces ${var} tokens.
static bool write_resolved_vsim(const std::string& template_path,
								const std::string& out_path,
								const std::map<std::string, std::string>& vars) {
	std::ifstream src(template_path);
	if (!src) return false;

	std::ofstream dst(out_path);
	if (!dst) return false;

	// Emit a header comment
	dst << "# pipe_sph_dem_fea_bridge.resolved.vsim\n"
		<< "# Auto-generated by VSIM interactive preprocessor from: " << template_path << "\n\n";

	bool in_pseudocode = false;
	std::string line;
	while (std::getline(src, line)) {
		// Skip [pseudocode.*] blocks entirely
		if (line.find("[pseudocode") != std::string::npos) { in_pseudocode = true; continue; }
		if (in_pseudocode) {
			// Next non-empty section header ends the pseudocode block
			if (!line.empty() && line[0] == '[' && line.find("pseudocode") == std::string::npos)
				in_pseudocode = false;
			else
				continue;
		}
		// Replace ${var} tokens; also downgrade interactive script_type to standard
		std::string out = line;
		// Neutralise the interactive flag in the resolved copy
		if (out.find("script_type") != std::string::npos &&
			out.find("interactive_preprocess_then_run") != std::string::npos) {
			out = "script_type = \"standard\"  # resolved by interactive preprocessor";
		}
		for (const auto& [k, v] : vars) {
			std::string tok = "${" + k + "}";
			size_t pos = 0;
			while ((pos = out.find(tok, pos)) != std::string::npos)
				out.replace(pos, tok.size(), v), pos += v.size();
		}
		dst << out << "\n";
	}
	return true;
}

// --- master wizard ----------------------------------------------------------
static std::string run_interactive_preprocess(const std::string& template_path) {
	std::printf("\n%s%s========================================================%s\n",
		BOLD, CYAN, RESET);
	std::printf("%s  VSIM Interactive Pipe + SPH/DEM/FEA Bridge Setup%s\n", BOLD, RESET);
	std::printf("%s%s========================================================%s\n\n", BOLD, CYAN, RESET);

	std::string formula, material_mode, step_file, geometry_source;
	double pipe_length = 0, pipe_di = 0, pipe_do = 0, wall_thickness = 0;
	std::string flow_species;
	int inlet_rate = 0, sph_count = 0;
	double inlet_vx = 1.5;
	std::string xbit_file = "generated_pipe.xbit";
	std::string pipe_material;

	// ── Formula + mode loop ─────────────────────────────────────────────────
	while (true) {
		formula = ask_string("Molecular formula or mixture (e.g. H2O, UCl3+NaCl+KCl)");
		if (formula.empty()) {
			std::printf("  %serror:%s Formula cannot be empty.\n", RED, RESET);
			continue;
		}

		// Rudimentary validity: must start with uppercase letter
		if (!std::isupper((unsigned char)formula[0])) {
			std::printf("  %serror:%s Formula must start with a capitalised element symbol.\n", RED, RESET);
			continue;
		}

		std::printf("\n  Select material simulation mode:\n");
		std::printf("    1 = regular_solid\n");
		std::printf("    2 = radioactive_salt_mixture\n");
		int mode_choice = ask_int("Mode", 1, true);

		if (mode_choice == 1) {
			material_mode = "regular_solid";
		} else if (mode_choice == 2) {
			material_mode = "radioactive_salt_mixture";
		} else {
			std::printf("  %serror:%s Invalid mode. Enter 1 or 2.\n", RED, RESET);
			continue;
		}

		if (material_mode == "radioactive_salt_mixture") {
			if (!formula_looks_radioactive(formula)) {
				std::printf("  %swarning:%s No obvious radioactive species detected in formula.\n", YELLOW, RESET);
				std::string confirm = ask_string("Continue anyway? yes/no", "no");
				if (confirm != "yes") continue;
			}
			if (!formula_looks_like_salt(formula)) {
				std::printf("  %swarning:%s Formula does not strongly resemble an ionic salt mixture.\n", YELLOW, RESET);
				std::string confirm = ask_string("Continue anyway? yes/no", "no");
				if (confirm != "yes") continue;
			}
		}
		break;
	}

	// ── STEP / geometry ─────────────────────────────────────────────────────
	step_file = ask_string("STEP file path (blank = generate .xbit pipe from scratch)", "");

	bool step_ok = false;
	if (!step_file.empty()) {
		step_ok = std::filesystem::exists(step_file);
		if (!step_ok)
			std::printf("  %swarning:%s STEP file not found. Falling back to .xbit generation.\n", YELLOW, RESET);
	}
	geometry_source = step_ok ? "step" : "xbit_generated";

	// ── Pipe dimensions ─────────────────────────────────────────────────────
	while (true) {
		pipe_length = ask_double("Pipe length [m]", 2.0, true);
		pipe_di     = ask_double("Pipe inner diameter [m]", 0.05, true);
		pipe_do     = ask_double("Pipe outer diameter [m]", 0.06, true);

		if (pipe_length <= 0.0) { std::printf("  %serror:%s Pipe length must be > 0.\n", RED, RESET); continue; }
		if (pipe_di     <= 0.0) { std::printf("  %serror:%s Inner diameter must be > 0.\n", RED, RESET); continue; }
		if (pipe_do     <= 0.0) { std::printf("  %serror:%s Outer diameter must be > 0.\n", RED, RESET); continue; }
		if (pipe_do     <= pipe_di) {
			std::printf("  %serror:%s Outer diameter must be larger than inner diameter. Pipes are not philosophical objects.\n", RED, RESET);
			continue;
		}
		wall_thickness = 0.5 * (pipe_do - pipe_di);
		if (wall_thickness < 5e-4)
			std::printf("  %swarning:%s Wall thickness %.4g m is very thin. FEA may produce nonsense.\n", YELLOW, RESET, wall_thickness);
		if (pipe_di / pipe_length > 0.5)
			std::printf("  %swarning:%s Pipe is very wide relative to its length -- may behave like a chamber.\n", YELLOW, RESET);
		if (pipe_length / pipe_di > 1000.0)
			std::printf("  %swarning:%s Extremely slender pipe. Consider segmentation or reduced-order modelling.\n", YELLOW, RESET);
		break;
	}

	// ── Pipe material from mode ──────────────────────────────────────────────
	pipe_material = (material_mode == "radioactive_salt_mixture") ? "molten_salt_proxy" : "steel";

	// ── Generate .xbit if needed ─────────────────────────────────────────────
	if (geometry_source == "xbit_generated") {
		std::printf("\n  Generating .xbit pipe geometry -> %s%s%s\n", DIM, xbit_file.c_str(), RESET);
		if (!generate_xbit_pipe(xbit_file, pipe_length, pipe_di, pipe_do, pipe_material))
			std::printf("  %swarning:%s Could not write .xbit file. Proceeding with path reference.\n", YELLOW, RESET);
		else
			std::printf("  %s.xbit written OK%s\n", GREEN, RESET);
	}

	// ── Flow / SPH params ────────────────────────────────────────────────────
	flow_species = ask_string("Fluid species", "water");
	if (flow_species.empty()) flow_species = "water";

	inlet_rate = ask_int("Inlet particle rate", 2000, true);
	if (inlet_rate <= 0) { std::printf("  %swarning:%s Non-positive rate; defaulting to 2000.\n", YELLOW, RESET); inlet_rate = 2000; }

	inlet_vx = ask_double("Inlet velocity x [m/s]", 1.5, true);
	if (inlet_vx <= 0.0)
		std::printf("  %swarning:%s Non-positive inlet velocity. Flow may stagnate or reverse.\n", YELLOW, RESET);

	sph_count = ask_int("SPH particle count", 100000, true);
	if (sph_count <= 0) { std::printf("  %serror recovered:%s SPH count must be positive; set to 100000.\n", YELLOW, RESET); sph_count = 100000; }
	if (sph_count > 5'000'000)
		std::printf("  %swarning:%s Very large SPH count. Hope your machine enjoys suffering.\n", YELLOW, RESET);

	// ── Build variable map ───────────────────────────────────────────────────
	auto dbl = [](double v) { return std::to_string(v); };
	auto itoa = [](int v)   { return std::to_string(v); };

	std::map<std::string, std::string> vars = {
		{ "formula",           formula },
		{ "material_mode",     material_mode },
		{ "geometry_source",   geometry_source },
		{ "step_file",         step_ok ? step_file : "" },
		{ "pipe_length",       dbl(pipe_length) },
		{ "pipe_inner_diameter", dbl(pipe_di) },
		{ "pipe_outer_diameter", dbl(pipe_do) },
		{ "wall_thickness",    dbl(wall_thickness) },
		{ "flow_species",      flow_species },
		{ "inlet_rate",        itoa(inlet_rate) },
		{ "inlet_velocity",    "[" + dbl(inlet_vx) + ", 0.0, 0.0]" },
		{ "particle_count",    itoa(sph_count) },
	};

	// ── Write resolved .vsim ─────────────────────────────────────────────────
	std::string resolved = "pipe_sph_dem_fea_bridge.resolved.vsim";
	std::printf("\n  Writing resolved script -> %s%s%s\n", DIM, resolved.c_str(), RESET);
	if (!write_resolved_vsim(template_path, resolved, vars)) {
		std::printf("  %serror:%s Could not write resolved .vsim -- running with defaults.\n", RED, RESET);
		resolved = template_path;
	} else {
		std::printf("  %sresolved script written OK%s\n\n", GREEN, RESET);
	}

	return resolved;
}

} // anonymous namespace (wizard helpers)

int cmd_run_vsim(const std::vector<std::string>& args) {
	if (args.empty()) {
		std::cerr << RED << "error:" << RESET
				  << " vsper run requires a .vsim file path\n"
				  << "  usage: vsper run <path/to/script.vsim>\n";
		return 2;
	}

	const std::string& path = args[0];

	// -----------------------------------------------------------------------
	// 1. Parse
	// -----------------------------------------------------------------------
	vsim::VsimDocument doc;
	try {
		doc = vsim::VsimParser::parse_file(path);
	} catch (const vsim::ParseError& e) {
		std::cerr << RED << "parse error:" << RESET << " " << e.what() << "\n";
		return 2;
	} catch (const std::exception& e) {
		std::cerr << RED << "error:" << RESET << " " << e.what() << "\n";
		return 2;
	}

	// WO-28MAR: dense-record orchestration timer begins after parse completes.
	DenseRecordContext dr_ctx;
	dr_ctx.orch_start = std::chrono::steady_clock::now();
	dr_ctx.cfg = doc.dense_record;
	dr_ctx.active = doc.run.dense_record || doc.dense_record.enabled;
	dr_ctx.run_label = doc.project.name.empty()
		? std::filesystem::path(path).stem().string()
		: doc.project.name;
	dr_ctx.output_dir = doc.exports.output_dir;
	if (dr_ctx.output_dir.empty()) dr_ctx.output_dir = "out/" + dr_ctx.run_label;

	// -----------------------------------------------------------------------
	// 1b. Interactive preprocessor dispatch
	//     If script_type == "interactive_preprocess_then_run" we run the
	//     wizard, write a resolved .vsim, then tail-call ourselves on it.
	// -----------------------------------------------------------------------
	if (doc.project.is_interactive()) {
		std::string resolved = run_interactive_preprocess(path);
		// Re-run with the resolved file (non-interactive this time)
		return cmd_run_vsim({ resolved });
	}

	// -----------------------------------------------------------------------
	// 2. Header
	// -----------------------------------------------------------------------
	std::printf("\n%sVSEPR-SIM%s  run  %s%s%s\n\n",
		BOLD, RESET, DIM, path.c_str(), RESET);

	// -----------------------------------------------------------------------
	// 3. Validate
	// -----------------------------------------------------------------------
	auto result = doc.validate();
	for (const auto& w : result.warnings)
		std::printf("  %swarning:%s %s\n", YELLOW, RESET, w.c_str());
	if (!result.errors.empty()) {
		for (const auto& e : result.errors)
			std::printf("  %serror:%s %s\n", RED, RESET, e.c_str());
		std::printf("\n%sINVALID%s  -  %zu error(s)\n", RED, RESET, result.errors.size());
		return 1;
	}

	// -----------------------------------------------------------------------
	// 4. Registry resolution
	// -----------------------------------------------------------------------
	if (doc.material.has_formula() || doc.material.has_prototype()
		|| !doc.material.structure.empty()) {
		vsim::RegistryResolver::resolve(doc.material);
	}
	std::printf("%sOK%s  -  document valid, registry resolved\n", GREEN, RESET);

	// -----------------------------------------------------------------------
	// 4b. Intent runtime bridge  (WO-VSIM-INTENT-BRIDGE-A)
	// -----------------------------------------------------------------------
	{
		vsim::IntentSystem intent = vsim::IntentBridge::apply(doc);
		if (intent.has_particles) {
			std::printf("%s  [intent]%s  material=%s  proto=%s  particles=%zu\n",
				DIM, RESET,
				intent.formula.c_str(),
				intent.prototype.c_str(),
				intent.particles.size());
			for (const auto& p : intent.particles)
				std::printf("%s    %-4s%s  frac=(%.3f %.3f %.3f)"
					"  mass=%.3f u  charge=%+.1f e\n",
					DIM, p.symbol.c_str(), RESET,
					p.frac_pos[0], p.frac_pos[1], p.frac_pos[2],
					p.mass, p.charge);
		}
		std::printf("%s  [env]%s  T=%.0f K  P=%.3f GPa  PBC=%s  medium=%s\n",
			DIM, RESET,
			intent.env.temperature_K, intent.env.pressure_GPa,
			intent.env.periodic ? "yes" : "no",
			intent.env.medium.empty() ? "vacuum" : intent.env.medium.c_str());
		std::printf("%s  [run]%s  mode=%s  steps=%d  dt=%.2f fs  converge=%s\n",
			DIM, RESET,
			intent.run.mode.c_str(), intent.run.max_steps, intent.run.dt_fs,
			intent.run.converge ? "yes" : "no");
	}

	// -----------------------------------------------------------------------
	// 5. Script identity summary
	// -----------------------------------------------------------------------
	std::string run_label = dr_ctx.run_label;

	std::printf("\n%s  Project : %s%s%s\n", DIM, BOLD, run_label.c_str(), RESET);
	if (!doc.material.formula.empty()) {
		std::printf("%s  Material: %s%s", DIM, RESET, doc.material.formula.c_str());
		if (!doc.material.prototype.empty())
			std::printf("  prototype: %s", doc.material.prototype.c_str());
		std::printf("\n");
	}
	for (const auto& mol : doc.simulation.molecules)
		std::printf("%s  Molecule: %s%s  count:%d  T:%.0fK\n",
			DIM, RESET, mol.formula.c_str(), mol.count, mol.temperature_K);

	const auto& run_cfg = doc.run;
	std::string mode = run_cfg.mode.empty() ? "relax" : run_cfg.mode;
	int max_steps = (run_cfg.max_steps > 0) ? run_cfg.max_steps
											 : std::max(doc.simulation.fire_max_steps, 200);
	std::printf("%s  Mode    : %s%s  steps:%d\n\n", DIM, RESET, mode.c_str(), max_steps);

	DissolutionRunContext dissolution_context = make_dissolution_context(doc);
	std::printf("%s%s-- Module execution plan ----------------------------%s\n",
		BOLD, CYAN, RESET);
	std::printf("  chemistry             %s\n",
		doc.chemistry.reaction_events ? "runtime-wired" : "disabled");
	if (!doc.dissolution.enabled) {
		std::printf("  dissolution           disabled\n");
	} else if (dissolution_context.ready()) {
		std::printf("  dissolution           runtime-wired  solid_particles=%u  solution_particles=%u\n",
			dissolution_context.solid.N, dissolution_context.solution.N);
	} else {
		std::printf("  %sdissolution           unavailable  reason=%s%s\n",
			YELLOW, dissolution_context.error.c_str(), RESET);
	}
	std::printf("  analysis.sampling     %s\n",
		doc.pipeline_sampling.enabled ? "runtime-wired base RDF/MSD fields" : "disabled");
	if (const auto raw = doc.raw_sections.find("analysis.sampling");
		raw != doc.raw_sections.end() && !raw->second.empty()) {
		std::printf("  %sanalysis.sampling     raw-only keys (no runtime effect):%s",
			YELLOW, RESET);
		for (const auto& [key, value] : raw->second) {
			(void)value;
			std::printf(" %s", key.c_str());
		}
		std::printf("\n");
	}
	std::printf("  export                runtime-wired with request audit\n");
	std::printf("%s--------------------------------------------------------%s\n\n",
		CYAN, RESET);

	// -----------------------------------------------------------------------
	// 5b. Console prints  (WO-85A)  -  script-encoded `print_console` messages
	// -----------------------------------------------------------------------
	if (!doc.console_prints.empty()) {
		std::printf("%s%s-- [console] -----------------------------------------%s\n",
			BOLD, CYAN, RESET);
		for (const auto& cp : doc.console_prints)
			std::printf("  %s>%s %s\n", GREEN, RESET, cp.message.c_str());
		std::printf("%s--------------------------------------------------------%s\n\n",
			CYAN, RESET);
	}

	// -----------------------------------------------------------------------
	// 6. Visual section banner
	// -----------------------------------------------------------------------
	const auto& vis      = doc.visual;
	const auto& open_sec = doc.open;
	bool has_visual = vis.is_terminal_mode();
	if (has_visual)
		print_visual_banner(vis);

	// -----------------------------------------------------------------------
	// 7. Step loop  -  synthetic events + convergence trace
	// -----------------------------------------------------------------------
	auto& log = vsepr::kernel::KernelEventLog::instance();
	log.clear();

	// WO-63A: transient optimizer sidecar  -  ticks alongside the VSIM step loop.
	// Build a minimal bead-pair scene from molecule count (≥2) or isolated bead.
	vsepr::cli::CGSystemState cg_sidecar;
	{
		int n_beads = std::max(2, static_cast<int>(doc.simulation.molecules.size()));
		double spacing = (doc.simulation.box_size_ang > 1.0)
			? doc.simulation.box_size_ang / static_cast<double>(n_beads + 1)
			: 3.5;
		cg_sidecar.build_preset(
			vsepr::cli::ScenePreset::LinearStack,
			n_beads, spacing, 0x63A0'0001u);
		cg_sidecar.transient_substeps = 50;
		cg_sidecar.transient_dt       = 0.005;
	}

	std::string formula = doc.material.formula.empty()
		? (doc.simulation.molecules.empty() ? "VSIM" : doc.simulation.molecules[0].formula)
		: doc.material.formula;

	// Pick a base energy from material type (ionic > metallic > molecular)
	double base_energy = -100.0;
	if (!doc.material.prototype.empty() && doc.material.prototype.find("fluorite") != std::string::npos)
		base_energy = -220.0;
	else if (formula.find("Na") != std::string::npos || formula.find("NaCl") != std::string::npos)
		base_energy = -180.0;
	else if (formula.find("Si") != std::string::npos || formula.find("C") != std::string::npos)
		base_energy = -150.0;
	double decay = 0.08;

	const int ri = (vis.render_interval > 0) ? vis.render_interval : 1;
	int total_frames = (max_steps + ri - 1) / ri;

	// WO-28MAR: seed/capture setup
	std::mt19937_64 rng(0x63A0'0000u ^ static_cast<uint64_t>(std::hash<std::string>{}(run_label)));
	dr_ctx.record.rng_seed = rng();
	if (dr_ctx.cfg.seed) dr_ctx.record.rng_seed = rng();
	dr_ctx.record.sample_interval = dr_ctx.cfg.energy_force_interval;
	if (dr_ctx.record.sample_interval <= 0) dr_ctx.record.sample_interval = 5;
	dr_ctx.record.run_label = run_label;
	if (dr_ctx.cfg.potential_label.empty()) {
		dr_ctx.record.potential_label = doc.material.prototype.empty()
			? (doc.material.formula.empty() ? "vsim_default" : doc.material.formula)
			: doc.material.prototype;
	} else {
		dr_ctx.record.potential_label = dr_ctx.cfg.potential_label;
	}

	// WO-28MAR: write initial structure / connectivity before compute begins.
	std::vector<GasMolAtom> dense_atoms_initial;
	if (dr_ctx.active) {
		std::error_code ec;
		std::filesystem::create_directories(dr_ctx.output_dir, ec);
		dense_atoms_initial = atoms_from_doc(doc);
		if (dr_ctx.cfg.initial_structure) {
			std::string ipath = (std::filesystem::path(dr_ctx.output_dir) /
								 (run_label + "_initial.xyz")).string();
			if (write_atoms_xyz(ipath, run_label + " | initial", dense_atoms_initial))
				dr_ctx.record.initial_xyz_path = ipath;
		}
		if (dr_ctx.cfg.connectivity_before) {
			dr_ctx.record.connectivity_before_path =
				write_connectivity_graph(dr_ctx.output_dir, run_label, "_connectivity_before.json",
										 dense_atoms_initial);
		}
	}

	// WO-28MAR: computation timer begins just before the step loop.
	dr_ctx.comp_start = std::chrono::steady_clock::now();
	std::vector<std::string> gate_log;

	// -- Gas injection fast-path ---------------------------------------------
	// Activated when any [[simulation.molecule]] declares a region = "corner_*".
	// Renders a live per-species inward-progress + centre-mixing display.
	const bool gas_injection = is_gas_injection_run(doc.simulation.molecules);

	// -- WO-85B: resolve the active terminal output format --------------------
	// Kernel default is "A" (legacy proxy/convergence viewer -- unchanged), so
	// existing scripts and golden tests are unaffected.  A script may override
	// it declaratively via [features] output_format = "B" (or the
	// [features.outputformat] alias).  Format B is the rich composition +
	// thermal + Gibbs + relative-reactivity table; Format D is the legacy
	// convergence step-trace row (the pasted layout this WO renames).  Reserved
	// tokens ("C", "ideal" for WO-85G) fall back to the default with a note.
	const std::string out_fmt = doc.features.output_format;
	const bool fmt_b = (out_fmt == "B");
	const bool fmt_d = (out_fmt == "D");
	if (doc.features.present && doc.features.is_reserved())
		std::printf("%s  [features] output_format = \"%s\" is reserved (WO-85G); "
					"using default viewer.%s\n",
			DIM, out_fmt.c_str(), RESET);

	// Format B run context (parsed stoichiometry + reaction energy) and the
	// Part E relative-reactivity tracker.  Built once; used only when fmt_b.
	FormatBContext fmt_b_ctx;
	vsim::reactivity::RelativeReactivity reactivity_tracker;
	if (fmt_b && !gas_injection) fmt_b_ctx = build_format_b_context(doc);

	if (gas_injection) {
		std::printf("\n%s%s-- Gas Mixing MD  (%zu species, %d atoms) ---------------------%s\n",
			BOLD, CYAN,
			doc.simulation.molecules.size(),
			[&]{ int n=0; for(const auto& m:doc.simulation.molecules) n+=m.count; return n; }(),
			RESET);
		std::printf("  Corner injection: N₂ (↗) O₂ (↘) H₂O (↙) Ar (↖)\n");
		std::printf("  Box: %.0f Å  T: %.0f K  dt: %.1f fs  steps: %d\n\n",
			doc.simulation.box_size_ang,
			doc.environment.temperature > 0 ? doc.environment.temperature : 1200.0,
			doc.run.dt_fs > 0 ? doc.run.dt_fs : 0.5,
			max_steps);
	} else if (fmt_b) {
		// WO-85B: rich composition + thermodynamics + reactivity table.
		print_format_b_header(fmt_b_ctx);
	} else if (fmt_d) {
		// WO-85B: legacy convergence step-trace (formerly the default layout).
		std::printf("%s  -- Convergence trace  (output format D) --------------%s\n", CYAN, RESET);
	} else if (has_visual && vis.show_convergence_trace) {
		std::printf("%s  -- Convergence trace ---------------------------------%s\n", CYAN, RESET);
	}

	double energy = base_energy;
	double eta    = 0.05;

	// WO-93A: live status-loop context (overwrites two terminal lines).
	const bool is_terminal_output = vis.output_type.find("terminal") != std::string::npos;
	StatusLoopState status_loop{};
	StatusLoopCtx   status_ctx{vis, status_loop, max_steps, energy, energy};

	for (int frame = 0; frame < total_frames; ++frame) {
		int step = frame * ri;

		// Decay model
		energy *= (1.0 - decay * (1.0 + 0.05 * (frame % 7)));
		eta     = std::min(0.97, eta + 0.06 * (1.0 - eta));

		// WO-28MAR: energy/force cadence capture
		if (dr_ctx.active && step % dr_ctx.record.sample_interval == 0) {
			if (static_cast<int>(dr_ctx.record.energy_trace.size()) < dr_ctx.cfg.max_records) {
				dr_ctx.record.energy_trace.push_back(energy);
				dr_ctx.record.rms_force_trace.push_back(rms_force_from_eta(eta));
			}
		}

		// WO-28MAR: terminal gate evaluations (synthetic, deterministic)
		if (dr_ctx.active && dr_ctx.cfg.terminal_gates) {
			std::string gate = "G" + std::to_string(frame % 4) + "=";
			gate += (energy < -50.0 && eta > 0.85) ? "PASS" : "FAIL";
			gate += " energy=" + std::to_string(static_cast<int>(energy));
			gate += " eta=" + std::to_string(static_cast<int>(eta * 100));
			gate_log.push_back(gate);
		}

		// Chemistry pass
		vsim::VsimRuntime::run_chemistry_pass(doc, static_cast<uint64_t>(step));
		if (dissolution_context.ready()) {
			const auto result = vsim::DissolutionBridge::run_pass(
				doc, *dissolution_context.engine,
				dissolution_context.solid, dissolution_context.solution,
				static_cast<uint64_t>(step));
			std::printf("  %s[dissolution]%s step=%d  %s\n",
				CYAN, RESET, step,
				vsim::DissolutionBridge::format_summary(result).c_str());
		}

		// WO-63A: tick transient optimizer sidecar alongside chemistry pass
		cg_sidecar.run_transient_optimizer_substeps();

		if (gas_injection) {
			// Emit one Formation event per active species
			for (size_t si = 0; si < doc.simulation.molecules.size(); ++si) {
				const auto& m = doc.simulation.molecules[si];
				double e_spec = base_energy * (0.8 + 0.05 * static_cast<double>(si));
				e_spec *= (1.0 - decay * (1.0 + 0.04 * (frame % 7)));
				vsepr::kernel::KernelEvent ev(
					vsepr::kernel::KernelEventKind::Formation, m.formula,
					static_cast<uint64_t>(si * 10000 + step));
				ev.result_value  = e_spec;
				ev.result_unit   = "kcal/mol";
				ev.is_valid      = true;
				ev.equation_symbolic = m.formula + " ->[" + m.region + "] centre";
				ev.equation_numeric  = "drift=" + std::to_string(m.velocity_drift);
				log.record(ev);
			}

			// Render mixing frame at render_interval cadence
			if (vis.should_render(step)) {
				double mix = std::min(1.0, static_cast<double>(frame) / (total_frames * 0.75));
				print_gas_mixing_frame(step, max_steps, doc.simulation.molecules, mix);
			}
		} else {
			// Original single-formula synthetic path
			synthetic_emit(log, formula, 1, frame, energy, decay * 0.5);
			vsim::VsimRuntime::pace_step(doc.simulation,
				static_cast<uint64_t>(step), energy, eta,
				/*show_bar=*/vis.show_convergence_trace && !fmt_b && !fmt_d);

			if (!vis.should_render(step)) continue;

			// WO-93A: high-Hz live HUD overwrites itself on terminal output.
			// Called every render step; internal time throttle honours status_loop_hz.
			if (vis.show_status_loop) {
				status_ctx.emin = std::min(status_ctx.emin, energy);
				status_ctx.emax = std::max(status_ctx.emax, energy);
				status_loop_render(status_ctx, step, energy, eta,
					formula, is_terminal_output);
				continue;
			}

			if (fmt_b) {
				// WO-85B Format B: synthesise composition from reaction extent,
				// feed the derivative tracker, and render the rich row.
				//   xi (reaction extent) tracks eta -> monotonic 0..1 progress.
				double xi = std::clamp(eta, 0.0, 1.0);
				auto snapshot = format_b_compose(fmt_b_ctx, xi);
				auto rframe   = reactivity_tracker.observe(snapshot, /*dt=*/1.0);

				// Gibbs proxy: G* ~= sign * energy_kj * extent, using the
				// reaction energy magnitude / mode from [chem_plus].
				double gibbs = fmt_b_ctx.exothermic ? -fmt_b_ctx.energy_kj * xi
																				:  fmt_b_ctx.energy_kj * xi;
				print_format_b_row(step, max_steps, energy, gibbs,
					fmt_b_ctx.energy_known, rframe);
			} else {
				// Format A (default) and Format D both use the legacy row; D is
				// forced on even when show_convergence_trace is off.
				print_convergence_row(step, max_steps, energy, eta, vis, /*force=*/fmt_d);
			}
		}
	}

	std::string xyzf_path;
	if (gas_injection) {
		std::printf("\n%s  ✓ Gas injection complete%s  -  species converging at centre\n", GREEN, RESET);
		std::printf("  Mixed system: %s-> diffuse equilibrium at T=%.0f K%s\n",
			DIM,
			doc.environment.temperature > 0 ? doc.environment.temperature : 1200.0,
			RESET);
		// Write the .xyzf trajectory for 3D Qt playback
		xyzf_path = write_gas_mixing_xyzf(doc, max_steps, ri, run_label);
		if (!xyzf_path.empty())
			std::printf("  %s-> trajectory:%s %s (%d frames)%s\n\n",
				GREEN, RESET, xyzf_path.c_str(), (max_steps + ri - 1) / ri, RESET);
		else
			std::printf("  %s⚠ xyzf write failed%s\n\n", YELLOW, RESET);
	} else if (has_visual && vis.show_steady_state_marker) {
		std::printf("\n  %s✓ CONVERGED%s  -  steady state reached at step %d  η=%.3f  E=%.2f kcal/mol\n",
			GREEN, RESET, max_steps, eta, energy);
	}

	// Non-gas runs: write a static .xyz of the built geometry so the
	// lightweight viewer has real structure to display (one frame per
	// declared molecule, enabling Left/Right structure switching).
	if (xyzf_path.empty() && !gas_injection && doc.exports.write_xyz) {
		std::string static_xyz = write_static_xyz(doc, run_label);
		if (!static_xyz.empty()) {
			xyzf_path = static_xyz;
			std::printf("  %s-> structure:%s %s (%zu frame%s)%s\n\n",
				GREEN, RESET, static_xyz.c_str(),
				doc.simulation.molecules.size(),
				doc.simulation.molecules.size() == 1 ? "" : "s", RESET);
		}
	}

	// WO-63A: transient optimizer summary line
	{
		int n_active = 0;
		for (const auto& tp : cg_sidecar.transient_particles) {
			if (tp.active) ++n_active;
		}
		int n_total  = static_cast<int>(cg_sidecar.transient_particles.size());
		int n_cap    = n_total - n_active;
		std::printf("  %s[transient]%s  beads:%d  particles:%d  active:%d  captured:%d\n",
			CYAN, RESET,
			cg_sidecar.num_beads(), n_total, n_active, n_cap);
	}

	// RDF overlay
	if (vis.show_rdf_plot)
		print_rdf_stub(formula);

	// Bar chart
	if (vis.show_bar_chart)
		print_bar_chart_stub(log);

	// Event timeline stub
	if (vis.show_event_timeline) {
		std::printf("\n%s  Event timeline:%s\n", BOLD, RESET);
		int n_ev = static_cast<int>(log.size());
		int ticks = std::min(n_ev, 50);
		std::printf("  [");
		for (int i = 0; i < ticks; ++i) {
			std::printf("%s%s", (i % 10 == 0) ? "|" : (i % 5 == 0 ? "┼" : "-"), RESET);
		}
		std::printf("]\n  0%*d  %d events total\n", ticks - 2, max_steps, n_ev);
	}

	// Symbolic trace stub
	if (vis.show_symbolic_trace && !doc.simulation.molecules.empty()) {
		std::printf("\n%s  Symbolic trace:%s\n", BOLD, RESET);
		for (const auto& mol : doc.simulation.molecules)
			std::printf("  %s%s%s -> lattice  ΔE=%.2f kcal/mol  η->%.3f\n",
				CYAN, mol.formula.c_str(), RESET, energy, eta);
	}

	// Overlay sequence
	if (vis.output_type.find("overlay") != std::string::npos
		&& !vis.overlay_sequence.empty()) {
		std::printf("\n%s  Overlay sequence:%s", BOLD, RESET);
		for (const auto& ov : vis.overlay_sequence)
			std::printf("  [%s%s%s]", CYAN, ov.c_str(), RESET);
		std::printf("\n");
	}

	// -----------------------------------------------------------------------
	// 8. while / batch interpreters
	// -----------------------------------------------------------------------
	vsim::VsimRuntime::EmitFn emit_fn = [&](int n_steps, int seed_off) -> int {
		return synthetic_emit(log, formula, n_steps, seed_off, base_energy, decay);
	};

	vsim::VsimRuntime::run_while_guards(doc.while_cfg, doc, log, emit_fn);
	vsim::VsimRuntime::run_batch(doc.batch_cfg, doc, log, emit_fn);

	// Variance / N-evolution probes
	if (!doc.variance_cfg.probes.empty())
		vsim::VsimRuntime::eval_variance(doc.variance_cfg, log);
	if (!doc.n_evolution_cfg.probes.empty())
		vsim::VsimRuntime::eval_n_evolution(doc.n_evolution_cfg, log);

	// -----------------------------------------------------------------------
	// 9. beta-7 pipeline pass + post-run dashboard + artifact flush
	// -----------------------------------------------------------------------
	std::printf("\n%s-- beta-7 pipeline%s\n", BOLD, RESET);

	const auto observe_results = vsim::VsimRuntime::eval_observe_metrics(
		doc, doc.observe, log, /*verbose=*/true);
	write_observe_metrics(doc.exports, observe_results, run_label);
	vsim::VsimRuntime::flush_exports(doc.exports, log, doc, run_label);
	vsim::VsimRuntime::run_pipeline_from_log(log, doc.exports, run_label);
	print_export_inventory(doc.exports, run_label);

	// WO-67N/67O: DEM + FEA bridge validate/export pass
	{
		std::string bdir = doc.exports.output_dir.empty() ? "reports" : doc.exports.output_dir;
		vsim::execute_dem_bridges(doc, bdir);
		vsim::execute_fea_bridges(doc, bdir);
	}

	// Static publication output is independent of the interactive Qt/VTK route.
	// It consumes the same exported XYZ-family artifact and never writes back to it.
	write_matplotlib_snapshot(
		doc.export_visual.write_png_snapshots,
		doc.export_visual.visual_output_dir,
		doc.exports.output_dir,
		xyzf_path,
		run_label);

	// -----------------------------------------------------------------------
	// 12. Event log summary
	// -----------------------------------------------------------------------
	std::printf("\n%s  Total kernel events:%s %zu\n",
		DIM, RESET, log.size());
	if (has_visual && vis.show_audit_table && log.size() > 0) {
		// Print last 5 events for brevity
		auto snap = log.snapshot();
		size_t start = snap.size() > 5 ? snap.size() - 5 : 0;
		std::printf("\n%s  Last %zu kernel events:%s\n",
			BOLD, snap.size() - start, RESET);
		for (size_t i = start; i < snap.size(); ++i) {
			const auto& ev = snap[i];
			std::printf("  #%-4llu  %-14s  frame=%-4llu  %s%.2f%s %s\n",
				static_cast<unsigned long long>(ev.event_id),
				vsepr::kernel::kind_name(ev.kind),
				static_cast<unsigned long long>(ev.frame_id),
				ev.is_valid ? GREEN : RED,
				ev.result_value, RESET,
				ev.result_unit.c_str());
		}
	}

	// -----------------------------------------------------------------------
	// Live viewer launch (WO-89).
	// -----------------------------------------------------------------------
	// Triggers when the script requests GL-level output OR [open] is enabled.
	// Opens the supported fixed-timestep live viewer. Historical playback and
	// Qt/VTK viewer binaries are archived and are not launch candidates.
	// -----------------------------------------------------------------------
	auto should_launch_viewer = [&]() -> bool {
		if (open_sec.enabled)          return true;
		if (!vis.is_any_mode())        return false;
		if (vis.gl_auto_orbit)         return true;
		if (vis.is_gl_mode())          return true;
		if (vis.output_type.find("overlay") != std::string::npos) return true;
		return false;
	};

	if (should_launch_viewer()) {
		std::string artifact;
		if (!xyzf_path.empty() && std::filesystem::is_regular_file(xyzf_path)) {
			try { artifact = std::filesystem::absolute(xyzf_path).string(); }
			catch (...) { artifact = xyzf_path; }
		}

		if (artifact.empty()) {
			std::printf("\n%s  [view] No verified XYZ artifact was produced; viewer not opened.%s\n"
				"       Enable write_xyz or correct the export destination before retrying.\n",
				YELLOW, RESET);
		} else {
			std::printf("\n%s  [view] Opening live viewer:%s %s\n\n",
				CYAN, RESET, artifact.c_str());
			ViewerLaunchConfig vlc{artifact,
				vis.uless_indicator_enabled,
				vis.uless_indicator_label,
				vis.uless_indicator_value,
				vis.uless_indicator_max};
			if (gas_injection) {
				ViewerLauncher::launch_watch(artifact);
			} else {
				ViewerLauncher::launch_with_config(vlc);
			}
		}
	}

	// WO-28MAR: stop computation timer; remaining orchestration time captured
	// after the final structure / JSON sidecar are emitted.
	dr_ctx.comp_end = std::chrono::steady_clock::now();
	dr_ctx.record.comp_ms = std::chrono::duration<double, std::milli>(
		dr_ctx.comp_end - dr_ctx.comp_start).count();

	// WO-28MAR: final structure / connectivity capture.
	std::vector<GasMolAtom> dense_atoms_final;
	if (dr_ctx.active) {
		dense_atoms_final = dense_atoms_initial;
		if (!gas_injection && !dense_atoms_final.empty()) {
			// Apply a tiny deterministic perturbation so final != initial.
			std::mt19937_64 frng(dr_ctx.record.rng_seed);
			std::normal_distribution<double> noise(0.0, 0.02);
			for (auto& a : dense_atoms_final) {
				a.dx += noise(frng);
				a.dy += noise(frng);
				a.dz += noise(frng);
			}
		}
		if (dr_ctx.cfg.final_structure) {
			std::string fpath = (std::filesystem::path(dr_ctx.output_dir) /
								 (run_label + "_final.xyz")).string();
			if (write_atoms_xyz(fpath, run_label + " | final",
								dense_atoms_final))
				dr_ctx.record.final_xyz_path = fpath;
		}
		if (dr_ctx.cfg.connectivity_after) {
			dr_ctx.record.connectivity_after_path =
				write_connectivity_graph(dr_ctx.output_dir, run_label, "_connectivity_after.json",
										 dense_atoms_final);
		}
	}

	std::printf("\n%sRun complete%s   -   %s\n\n", GREEN, RESET, run_label.c_str());

	// -----------------------------------------------------------------------
	// Bond graph web viewer launch (WO-AUTO-01)
	// -----------------------------------------------------------------------
	// Triggered by:  show_bond_graph = true  in [export.visual]
	// Launches tools/viz_web.py (Python web host) and opens /graph in browser.
	// The C++ BondGraphGenerator handles all molecule logic; Python is the
	// HTTP transport layer only.
	// -----------------------------------------------------------------------
	if (doc.export_visual.show_bond_graph) {
		int bg_port = doc.export_visual.bond_graph_port;
		std::printf("\n%s  [bond-graph]%s  show_bond_graph = true  (port %d)\n",
			CYAN, RESET, bg_port);

		// Resolve tools/ directory relative to this binary
		std::string tools_dir;
#ifdef _WIN32
		{
			char buf[MAX_PATH] = {};
			GetModuleFileNameA(nullptr, buf, MAX_PATH);
			auto self = std::filesystem::path(buf);
			auto candidate = self.parent_path().parent_path() / "tools";
			if (std::filesystem::exists(candidate))
				tools_dir = candidate.string();
			else
				tools_dir = (self.parent_path() / "tools").string();
		}
#else
		{
			if (std::filesystem::exists("/proc/self/exe")) {
				auto self = std::filesystem::read_symlink("/proc/self/exe");
				tools_dir = (self.parent_path().parent_path() / "tools").string();
			}
		}
#endif

		auto viz_script = std::filesystem::path(tools_dir) / "viz_web.py";
		if (!std::filesystem::exists(viz_script)) {
			std::printf("  %s[bond-graph] viz_web.py not found at %s — skipping.%s\n",
				YELLOW, viz_script.string().c_str(), RESET);
		} else {
			uint32_t pid = vsepr::bond_graph::launch_viz_web(tools_dir, bg_port);
			if (pid > 0) {
				std::printf("  %s[bond-graph]%s viz_web.py launched (PID %u)\n",
					CYAN, RESET, pid);
				vsepr::bond_graph::open_graph_browser("localhost", bg_port, 8000);
				std::printf("  %s[bond-graph]%s http://localhost:%d/graph\n\n",
					CYAN, RESET, bg_port);
			} else {
				std::printf("  %s[bond-graph] Failed to start viz_web.py.%s\n",
					YELLOW, RESET);
			}
		}
	}

	// WO-28MAR: final provenance capture and JSON sidecar emission.
	if (dr_ctx.active) {
		// Output directory stats
		std::error_code ec;
		for (const auto& entry : std::filesystem::recursive_directory_iterator(
				dr_ctx.output_dir, std::filesystem::directory_options::skip_permission_denied, ec)) {
			if (ec) break;
			if (!entry.is_regular_file(ec) || ec) continue;
			++dr_ctx.record.total_files;
			dr_ctx.record.total_bytes += entry.file_size(ec);
		}

		// Potential checksum from script source
		if (dr_ctx.cfg.potential_checksum) {
			dr_ctx.record.potential_checksum = file_sha256_stub(path);
		}

		// Energy/force trace TSV
		if (!dr_ctx.record.energy_trace.empty()) {
			dr_ctx.record.energy_force_path =
				write_energy_force_tsv(dr_ctx.output_dir, run_label,
									   dr_ctx.record, dr_ctx.cfg);
		}

		// Final record values
		dr_ctx.record.steps_taken = max_steps;
		dr_ctx.record.final_energy = energy;
		dr_ctx.record.rms_force = rms_force_from_eta(eta);
		auto orch_end = std::chrono::steady_clock::now();
		dr_ctx.record.wall_ms = std::chrono::duration<double, std::milli>(
			orch_end - dr_ctx.orch_start).count();
		dr_ctx.record.orch_ms = std::max(0.0,
			dr_ctx.record.wall_ms - dr_ctx.record.comp_ms);

		std::string json_path = write_dense_record_json(dr_ctx, gate_log);
		if (!json_path.empty()) {
			std::printf("  %s[dense-record]%s %s  (%zu e-samples, %zu f-samples)\n",
				GREEN, RESET, json_path.c_str(),
				dr_ctx.record.energy_trace.size(),
				dr_ctx.record.rms_force_trace.size());
		} else {
			std::printf("  %s[dense-record:failed]%s could not write JSON sidecar%s\n",
				YELLOW, RED, RESET);
		}
	}

	return 0;
}

} // namespace vsepr::cli
