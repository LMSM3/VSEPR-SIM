// =============================================================================
// cmd_x_suite.cpp  —  `vsepr x` subcommand implementation
// =============================================================================
//
// Usage:
//   vsepr x inspect  <file.X>
//   vsepr x validate <file.X>
//   vsepr x run      <file.X>
//   vsepr x replay   <file.X>
//   vsepr x export   <file.X>
//   vsepr x open     <file.X>    (double-click launcher)
//
// =============================================================================

#include "cmd_x_suite.hpp"
#include "vsim/xsuite.hpp"
#include <iostream>
#include <cstdlib>
#include <string>
#include <vector>
#include <filesystem>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace vsepr {
namespace cli {

namespace {

// ---------------------------------------------------------------------------
// sub-command: inspect
// ---------------------------------------------------------------------------
int x_inspect(const std::string& path) {
	auto res = vsepr::xsuite::xsuite_parse(path);
	if (!res.ok) {
		std::cerr << "x inspect: parse error — " << res.error << "\n";
		return 1;
	}
	for (const auto& w : res.warnings) {
		std::cout << "  [warn] " << w << "\n";
	}
	vsepr::xsuite::xsuite_inspect(res.suite);
	return 0;
}

// ---------------------------------------------------------------------------
// sub-command: validate
// ---------------------------------------------------------------------------
int x_validate(const std::string& path) {
	auto res = vsepr::xsuite::xsuite_parse(path);
	if (!res.ok) {
		std::cerr << "x validate: parse error — " << res.error << "\n";
		return 1;
	}
	for (const auto& w : res.warnings) {
		std::cout << "  [warn] " << w << "\n";
	}
	return vsepr::xsuite::xsuite_validate(res.suite) == 0 ? 0 : 1;
}

// ---------------------------------------------------------------------------
// sub-command: run
// ---------------------------------------------------------------------------
int x_run(const std::string& path) {
	auto res = vsepr::xsuite::xsuite_parse(path);
	if (!res.ok) {
		std::cerr << "x run: parse error — " << res.error << "\n";
		return 1;
	}
	const auto& s = res.suite;

	std::cout << "vsepr x run: " << s.name << "\n";

	// Validate before running
	std::cout << "--- validate ---\n";
	if (vsepr::xsuite::xsuite_validate(s) != 0) {
		std::cerr << "x run: validation failed — aborting\n";
		return 2;
	}

	// Optional build step
	if (s.compile) {
		std::string build_cmd = "cmake --preset " + s.build_config;
		std::cout << "--- build: " << build_cmd << " ---\n";
		if (std::system(build_cmd.c_str()) != 0) {
			std::cerr << "x run: build failed\n";
			return 3;
		}
	}

	// Delegate to `vsepr run <entry_script>` (same process routing)
	if (s.run && !s.entry_script.empty()) {
		std::cout << "--- run: vsepr run " << s.entry_script << " ---\n";
		// Invoke `vsepr run` via CLI shim
		std::string run_cmd = "vsepr run \"" + s.entry_script + "\"";
		int ret = std::system(run_cmd.c_str());
		if (ret != 0) return 4;
	}

	std::cout << "x run: done\n";
	return 0;
}

// ---------------------------------------------------------------------------
// sub-command: replay
// ---------------------------------------------------------------------------
int x_replay(const std::string& path) {
	auto res = vsepr::xsuite::xsuite_parse(path);
	if (!res.ok) {
		std::cerr << "x replay: parse error — " << res.error << "\n";
		return 1;
	}
	const auto& s = res.suite;

	// Prefer rich replay (.xyzFull), fall back to trajectory (.xyzf)
	std::string replay_file;
	if (!s.xyzfull_path.empty() && std::filesystem::exists(s.xyzfull_path)) {
		replay_file = s.xyzfull_path;
	} else if (!s.xyzf_path.empty() && std::filesystem::exists(s.xyzf_path)) {
		replay_file = s.xyzf_path;
	} else {
		std::cerr << "x replay: no trajectory or rich file found\n";
		return 1;
	}

	std::string cmd = "vsepr-view --artifact \"" + replay_file + "\"";
	std::cout << "x replay: " << cmd << "\n";
	return std::system(cmd.c_str());
}

// ---------------------------------------------------------------------------
// sub-command: export
// ---------------------------------------------------------------------------
int x_export(const std::string& path) {
	auto res = vsepr::xsuite::xsuite_parse(path);
	if (!res.ok) {
		std::cerr << "x export: parse error — " << res.error << "\n";
		return 1;
	}
	const auto& s = res.suite;

	if (!s.export_outputs) {
		std::cout << "x export: actions.export = false — skipping\n";
		return 0;
	}

	std::cout << "x export: " << s.name << "\n";
	if (!s.report_path.empty())
		std::cout << "  report -> " << s.report_path << "\n";
	if (!s.json_path.empty())
		std::cout << "  json   -> " << s.json_path << "\n";
	if (!s.log_path.empty())
		std::cout << "  log    -> " << s.log_path << "\n";

	// Delegate to `vsepr run --export-only <entry>` (future hook)
	// For now: report that export was requested and files are targeted
	std::cout << "x export: output paths registered — use `vsepr run` with export flags.\n";
	return 0;
}

// ---------------------------------------------------------------------------
// sub-command: open  (double-click launcher)
// Spawns:
//   1. CMD window running `vsepr run <entry_script>` (simulation console)
//   2. CMD window running `vsepr x validate <file.X>` (status console)
//   3. vsepr-view for live molecular visualization
// ---------------------------------------------------------------------------
int x_open(const std::string& path) {
	auto res = vsepr::xsuite::xsuite_parse(path);
	if (!res.ok) {
		std::cerr << "x open: parse error — " << res.error << "\n";
		return 1;
	}
	const auto& s = res.suite;

	std::string abs_path = std::filesystem::absolute(path).string();

	std::cout << "x open: launching suite: " << s.name << "\n";

#ifdef _WIN32
	// CMD 1: simulation run console
	{
		std::string cmd1 = "cmd.exe /k \"vsepr run " + s.entry_script + "\"";
		ShellExecuteA(nullptr, "open", "cmd.exe",
					  ("/k vsepr run \"" + s.entry_script + "\"").c_str(),
					  nullptr, SW_SHOW);
	}

	// CMD 2: validate / status console
	{
		ShellExecuteA(nullptr, "open", "cmd.exe",
					  ("/k vsepr x validate \"" + abs_path + "\"").c_str(),
					  nullptr, SW_SHOW);
	}

	// Supported live viewer (trajectory if available, else standalone)
	{
		std::string artifact_arg;
		if (!s.xyzfull_path.empty() && std::filesystem::exists(s.xyzfull_path))
			artifact_arg = "--artifact \"" + s.xyzfull_path + "\"";
		else if (!s.xyzf_path.empty() && std::filesystem::exists(s.xyzf_path))
			artifact_arg = "--artifact \"" + s.xyzf_path + "\"";

		ShellExecuteA(nullptr, "open", "vsepr-view.exe",
					  artifact_arg.empty() ? nullptr : artifact_arg.c_str(),
					  nullptr, SW_SHOW);
	}
	std::cout << "x open: launched 2 consoles + live viewer\n";
	return 0;
#else
	// Non-Windows fallback: open three terminals sequentially
	std::system(("xterm -e 'vsepr run \"" + s.entry_script + "\"' &").c_str());
	std::system(("xterm -e 'vsepr x validate \"" + abs_path + "\"' &").c_str());
	std::string artifact_arg;
	if (!s.xyzfull_path.empty()) artifact_arg = " --artifact \"" + s.xyzfull_path + "\"";
	else if (!s.xyzf_path.empty()) artifact_arg = " --artifact \"" + s.xyzf_path + "\"";
	std::system(("vsepr-view" + artifact_arg + " &").c_str());
	return 0;
#endif
}

void x_usage() {
	std::cout << R"(
vsepr x  —  .X saved-run suite commands

  vsepr x inspect  <file.X>   show suite contents and file status
  vsepr x validate <file.X>   check file presence and contracts
  vsepr x run      <file.X>   compile (if enabled), then run entry script
  vsepr x replay   <file.X>   open trajectory/rich replay in desktop viewer
  vsepr x export   <file.X>   regenerate reports and artifacts
  vsepr x open     <file.X>   double-click launcher: 2 CMD + Qt desktop

.X file format:  INI sections [xsuite] [run] [files] required;
				 [build] [hash] [actions] [outputs] optional.

See docs/wo/WO-XYZSUITE-X.md for full format reference.
)";
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// cmd_x_suite — public entry point
// ---------------------------------------------------------------------------

int cmd_x_suite(const std::vector<std::string>& args) {
	if (args.empty()) {
		x_usage();
		return 0;
	}

	const std::string& sub = args[0];

	if (sub == "help" || sub == "--help" || sub == "-h") {
		x_usage();
		return 0;
	}

	if (args.size() < 2) {
		std::cerr << "vsepr x " << sub << ": missing <file.X> argument\n";
		return 1;
	}

	const std::string& file = args[1];

	if (sub == "inspect")  return x_inspect(file);
	if (sub == "validate") return x_validate(file);
	if (sub == "run")      return x_run(file);
	if (sub == "replay")   return x_replay(file);
	if (sub == "export")   return x_export(file);
	if (sub == "open")     return x_open(file);

	std::cerr << "vsepr x: unknown sub-command '" << sub << "'\n";
	x_usage();
	return 1;
}

}} // namespace vsepr::cli
