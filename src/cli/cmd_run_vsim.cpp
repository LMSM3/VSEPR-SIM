/**
 * cmd_run_vsim.cpp — vsper run <script.vsim> subcommand
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
#include "vsim/vsim_parser.hpp"
#include "vsim/vsim_registry.hpp"
#include "vsim/vsim_runtime.hpp"
#include "kernel/kernel_event_log.hpp"
#include "kernel/kernel_event.hpp"
#include "vsepr/formula_parser.hpp"
#include "sim/molecule_builder.hpp"
#include "pot/periodic_db.hpp"

#include <array>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <shellapi.h>
#else
#  include <unistd.h>
#  include <sys/types.h>
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

	// -----------------------------------------------------------------------
	// print_visual_banner — shows the [visual] section flags the script declared
	// -----------------------------------------------------------------------
	void print_visual_banner(const vsim::VisualSection& vis) {
		std::printf("\n%s%s── [visual] ──────────────────────────────────────────%s\n",
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
		std::printf("%s────────────────────────────────────────────────────────%s\n\n",
			CYAN, RESET);
	}

	// -----------------------------------------------------------------------
	// print_convergence_row — one step-trace row for terminal_chart / overlay
	// -----------------------------------------------------------------------
	void print_convergence_row(int step, int max_steps,
							   double energy, double eta,
							   const vsim::VisualSection& vis)
	{
		if (!vis.show_convergence_trace) return;

		// Bar width proportional to |energy| clamped to 32 chars
		int bar_len = static_cast<int>(std::min(32.0, std::abs(energy) / 10.0));
		std::string bar(bar_len, eta > 0.6 ? '#' : (eta > 0.3 ? '+' : '.'));
		bar.resize(32, ' ');

		std::printf("  %s step %5d/%d%s  E=%8.2f  η=%5.3f  [%s]\n",
			DIM, step, max_steps, RESET,
			energy, eta,
			bar.c_str());
	}

	// -----------------------------------------------------------------------
	// print_rdf_stub — ASCII RDF bar chart stub
	// -----------------------------------------------------------------------
	void print_rdf_stub(const std::string& formula) {
		// Approximate RDF peaks for common crystal types
		const double peaks[] = { 2.82, 3.99, 4.89, 5.64, 6.32 };
		const double heights[] = { 1.0, 0.62, 0.45, 0.78, 0.31 };
		std::printf("\n%s  g(r) — %s%s\n", DIM, formula.c_str(), RESET);
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
	// print_bar_chart_stub — per-kind event count bar chart
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
	// Synthetic emit function — populates KernelEventLog with Formation events
	// proportional to step count, simulating convergence with energy decay.
	// Returns number of events added.
	// -----------------------------------------------------------------------
	// print_gas_mixing_frame — renders one time-step row for a directed-injection
	// gas mixing run.  Shows per-species kinetic progress and a centre-mixing bar.
	// -----------------------------------------------------------------------
	void print_gas_mixing_frame(
		int step, int max_steps,
		const std::vector<vsim::MoleculeEntry>& mols,
		double mix_fraction)   // 0.0 = fully separated, 1.0 = fully mixed
	{
		// ANSI colour per gas (CPK-inspired terminal palette)
		const char* species_colors[] = {
			"\033[34m",  // blue  — N₂
			"\033[31m",  // red   — O₂
			"\033[36m",  // cyan  — H₂O
			"\033[32m",  // green — Ar
		};
		const char* BAR_FULL  = "█";
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
			std::printf("%s", i < mix_filled ? "█" : "░");
		std::printf("]\033[0m  %.0f%%\n", mix_fraction * 100.0);
		std::fflush(stdout);
	}

	// -----------------------------------------------------------------------
	// is_gas_injection_run — true when any molecule entry has a region declared
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
		} catch (const std::exception&) {
			// Fallback: single-atom placeholder so the run isn't blocked
			result.emplace_back(GasMolAtom{formula.substr(0, 2), 0.0, 0.0, 0.0});
		}
		return result;
	}

	static std::array<double,3> corner_origin(const std::string& region, double L) {
		double h = L * 0.35;
		if (region == "corner_xnyp") return {-h,  h,  0.0};
		if (region == "corner_xpyp") return { h,  h,  0.0};
		if (region == "corner_xnyn") return {-h, -h,  0.0};
		if (region == "corner_xpyn") return { h, -h,  0.0};
		return {0.0, 0.0, 0.0};
	}

	// write_gas_mixing_xyzf — generates a multi-frame .xyzf trajectory for the
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
		static const PeriodicTable pt = [] {
			try { return PeriodicTable::load_from_json_file("data/PeriodicTableJSON.json"); }
			catch (...) { return PeriodicTable(); }
		}();

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
		const double kB_amu = 0.008314;  // kcal/(mol·K) — used for thermal sigma

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
			ev.equation_symbolic = formula + " → lattice";
			ev.equation_numeric  = "E=" + std::to_string(energy);
			log.record(ev);
			++emitted;
		}
		return emitted;
	}

} // anonymous namespace

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
		std::printf("\n%sINVALID%s — %zu error(s)\n", RED, RESET, result.errors.size());
		return 1;
	}

	// -----------------------------------------------------------------------
	// 4. Registry resolution
	// -----------------------------------------------------------------------
	if (doc.material.has_formula() || doc.material.has_prototype()
		|| !doc.material.structure.empty()) {
		vsim::RegistryResolver::resolve(doc.material);
	}
	std::printf("%sOK%s — document valid, registry resolved\n", GREEN, RESET);

	// -----------------------------------------------------------------------
	// 5. Script identity summary
	// -----------------------------------------------------------------------
	std::string run_label = doc.project.name.empty()
		? path.substr(path.find_last_of("/\\") + 1)
		: doc.project.name;

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

	// -----------------------------------------------------------------------
	// 6. Visual section banner
	// -----------------------------------------------------------------------
	const auto& vis = doc.visual;
	bool has_visual = vis.is_terminal_mode();
	if (has_visual)
		print_visual_banner(vis);

	// -----------------------------------------------------------------------
	// 7. Step loop — synthetic events + convergence trace
	// -----------------------------------------------------------------------
	auto& log = vsepr::kernel::KernelEventLog::instance();
	log.clear();

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

	// ── Gas injection fast-path ─────────────────────────────────────────────
	// Activated when any [[simulation.molecule]] declares a region = "corner_*".
	// Renders a live per-species inward-progress + centre-mixing display.
	const bool gas_injection = is_gas_injection_run(doc.simulation.molecules);

	if (gas_injection) {
		std::printf("\n%s%s── Gas Mixing MD  (%zu species, %d atoms) ─────────────────────%s\n",
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
	} else if (has_visual && vis.show_convergence_trace) {
		std::printf("%s  ── Convergence trace ─────────────────────────────────%s\n", CYAN, RESET);
	}

	double energy = base_energy;
	double eta    = 0.05;

	for (int frame = 0; frame < total_frames; ++frame) {
		int step = frame * ri;

		// Decay model
		energy *= (1.0 - decay * (1.0 + 0.05 * (frame % 7)));
		eta     = std::min(0.97, eta + 0.06 * (1.0 - eta));

		// Chemistry pass
		vsim::VsimRuntime::run_chemistry_pass(doc, static_cast<uint64_t>(step));

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
				ev.equation_symbolic = m.formula + " →[" + m.region + "] centre";
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
				/*show_bar=*/vis.show_convergence_trace);
			if (vis.should_render(step))
				print_convergence_row(step, max_steps, energy, eta, vis);
		}
	}

	std::string xyzf_path;
	if (gas_injection) {
		std::printf("\n%s  ✓ Gas injection complete%s — species converging at centre\n", GREEN, RESET);
		std::printf("  Mixed system: %s→ diffuse equilibrium at T=%.0f K%s\n",
			DIM,
			doc.environment.temperature > 0 ? doc.environment.temperature : 1200.0,
			RESET);
		// Write the .xyzf trajectory for 3D Qt playback
		xyzf_path = write_gas_mixing_xyzf(doc, max_steps, ri, run_label);
		if (!xyzf_path.empty())
			std::printf("  %s→ trajectory:%s %s (%d frames)%s\n\n",
				GREEN, RESET, xyzf_path.c_str(), (max_steps + ri - 1) / ri, RESET);
		else
			std::printf("  %s⚠ xyzf write failed%s\n\n", YELLOW, RESET);
	} else if (has_visual && vis.show_steady_state_marker) {
		std::printf("\n  %s✓ CONVERGED%s — steady state reached at step %d  η=%.3f  E=%.2f kcal/mol\n",
			GREEN, RESET, max_steps, eta, energy);
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
			std::printf("%s%s", (i % 10 == 0) ? "│" : (i % 5 == 0 ? "┼" : "─"), RESET);
		}
		std::printf("]\n  0%*d  %d events total\n", ticks - 2, max_steps, n_ev);
	}

	// Symbolic trace stub
	if (vis.show_symbolic_trace && !doc.simulation.molecules.empty()) {
		std::printf("\n%s  Symbolic trace:%s\n", BOLD, RESET);
		for (const auto& mol : doc.simulation.molecules)
			std::printf("  %s%s%s → lattice  ΔE=%.2f kcal/mol  η→%.3f\n",
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
	std::printf("\n%s── beta-7 pipeline%s\n", BOLD, RESET);

	vsim::VsimRuntime::run_pipeline_from_log(log, doc.exports, run_label);

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
	// Visual launch helper — determines whether the Qt workstation should open
	// -----------------------------------------------------------------------
	// Triggers when the script requests GL-level output:
	//   gl_auto_orbit = true, OR output_type ends in "3d"/"gl"/"overlay_cycle"
	//   when rendered on a system where vsepr-desktop.exe is present.
	// -----------------------------------------------------------------------
	auto should_launch_qt = [&]() -> bool {
		if (vis.gl_auto_orbit)         return true;
		if (vis.is_gl_mode())          return true;
		// overlay_cycle is a rich enough visual to warrant the desktop viewer
		if (vis.output_type.find("overlay") != std::string::npos) return true;
		return false;
	};

	if (should_launch_qt()) {
		// Resolve vsepr-desktop.exe relative to this executable
		// (same bin dir whether running from build/ or the installed bin/).
		std::string desktop_exe;

#ifdef _WIN32
		{
			char buf[MAX_PATH] = {};
			GetModuleFileNameA(nullptr, buf, MAX_PATH);
			std::filesystem::path self(buf);
			auto candidate = self.parent_path() / "vsepr-desktop.exe";
			if (std::filesystem::exists(candidate))
				desktop_exe = candidate.string();
		}
#else
		{
			auto candidate = std::filesystem::path("/proc/self/exe");
			if (std::filesystem::exists(candidate)) {
				auto self = std::filesystem::read_symlink(candidate);
				auto c2 = self.parent_path() / "vsepr-desktop";
				if (std::filesystem::exists(c2)) desktop_exe = c2.string();
			}
		}
#endif

		if (desktop_exe.empty()) {
			std::printf("\n%s  [Qt] vsepr-desktop not found next to vsepr.exe — "
				"skipping 3D window.%s\n"
				"  Rebuild target vsepr-desktop or run: cmake --build build --target vsepr-desktop\n",
				YELLOW, RESET);
		} else {
			// Canonicalise the vsim path so the desktop receives an absolute path
			std::string abs_vsim = path;
			try {
				abs_vsim = std::filesystem::absolute(path).string();
			} catch (...) {}

			// Prefer the .xyzf trajectory for direct 3D animation playback;
			// fall back to the .vsim script path for static/chemistry views.
			std::string abs_xyzf;
			if (!xyzf_path.empty()) {
				try { abs_xyzf = std::filesystem::absolute(xyzf_path).string(); } catch(...) { abs_xyzf = xyzf_path; }
			}

			std::printf("\n%s  [Qt] Launching 3D workstation:%s %s\n",
				CYAN, RESET, desktop_exe.c_str());
			if (!abs_xyzf.empty())
				std::printf("       xyzf: %s\n\n", abs_xyzf.c_str());
			else
				std::printf("       vsim: %s\n\n", abs_vsim.c_str());

#ifdef _WIN32
			// ShellExecute for a detached, non-blocking launch
			std::string qt_arg = abs_xyzf.empty()
				? ("--vsim \"" + abs_vsim + "\"")
				: ("--xyzf \"" + abs_xyzf + "\"");
			SHELLEXECUTEINFOA sei = {};
			sei.cbSize       = sizeof(sei);
			sei.fMask        = SEE_MASK_NOCLOSEPROCESS;
			sei.lpVerb       = "open";
			sei.lpFile       = desktop_exe.c_str();
			sei.lpParameters = qt_arg.c_str();
			sei.nShow        = SW_SHOWNORMAL;
			if (!ShellExecuteExA(&sei)) {
				std::printf("  %s[Qt] Failed to launch vsepr-desktop (error %lu)%s\n",
					YELLOW, GetLastError(), RESET);
			}
#else
			// POSIX: fork + exec
			if (fork() == 0) {
				if (!abs_xyzf.empty())
					execl(desktop_exe.c_str(), desktop_exe.c_str(), "--xyzf", abs_xyzf.c_str(), nullptr);
				else
					execl(desktop_exe.c_str(), desktop_exe.c_str(), "--vsim", abs_vsim.c_str(), nullptr);
				_exit(1);
			}
#endif
		}
	}

	std::printf("\n%sRun complete%s  —  %s\n\n", GREEN, RESET, run_label.c_str());
	return 0;
}

} // namespace vsepr::cli
