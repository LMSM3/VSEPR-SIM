/**
 * src/cli/cmd_dynx.cpp
 * ======================
 * WO-VSIM-DYNX-V1-B  |  Phase 9  |  v5.1.x
 *
 * Implements `vsepr dynx inspect` and `vsepr dynx validate`.
 */

#include "cmd_dynx.hpp"
#include "vsim/io/dynx_writer.hpp"

#include <cstdio>
#include <string>
#include <vector>

// ANSI helpers (match the style used across other CLI commands)
static constexpr const char* GRN  = "\033[32m";
static constexpr const char* RED  = "\033[31m";
static constexpr const char* YEL  = "\033[33m";
static constexpr const char* CYN  = "\033[36m";
static constexpr const char* DIM  = "\033[2m";
static constexpr const char* BOLD = "\033[1m";
static constexpr const char* RST  = "\033[0m";

namespace vsepr {
namespace cli {

// ============================================================================
// Help
// ============================================================================

std::string DynxCommand::Help() const {
	return R"(
Usage:
  vsepr dynx inspect  <file.dynx>   Print session archive metadata header
  vsepr dynx validate <file.dynx>   Full structural validation

Exit codes:
  0  success / valid
  1  validation failure or bad file
  2  file not found / wrong sub-command
)";
}

// ============================================================================
// Execute
// ============================================================================

int DynxCommand::Execute(const std::vector<std::string>& args) {
	if (args.empty()) {
		std::fputs("Usage: vsepr dynx <inspect|validate> <file.dynx>\n", stdout);
		return 2;
	}
	const std::string& sub = args[0];
	std::vector<std::string> rest(args.begin() + 1, args.end());

	if (sub == "inspect")  return cmd_inspect(rest);
	if (sub == "validate") return cmd_validate(rest);

	std::printf("Unknown dynx sub-command: '%s'\n", sub.c_str());
	std::printf("%s\n", Help().c_str());
	return 2;
}

// ============================================================================
// dynx inspect
// ============================================================================

int DynxCommand::cmd_inspect(const std::vector<std::string>& args) {
	if (args.empty()) {
		std::fputs("Usage: vsepr dynx inspect <file.dynx>\n", stdout);
		return 2;
	}
	const std::string& path = args[0];

	auto r = vsim::io::dynx_inspect(path);
	if (!r.ok) {
		std::printf("%sERROR%s  %s\n", RED, RST, r.error.c_str());
		return 1;
	}

	std::printf("\n%s%s  Dynx Session Archive%s  %s%s%s\n\n",
		BOLD, CYN, RST, DIM, path.c_str(), RST);
	std::printf("  %-20s  %s\n", "source",         r.source_path.c_str());
	std::printf("  %-20s  %s\n", "source_hash",     r.source_hash.c_str());
	std::printf("  %-20s  %s\n", "kernel_version",  r.kernel_version.c_str());
	std::printf("  %-20s  %d\n", "frame_count",     r.frame_count);
	std::printf("  %-20s  %.6f fs\n", "frame_interval", r.frame_interval_fs);
	std::printf("  %-20s  %d\n", "particle_count",  r.particle_count);
	std::printf("  %-20s  %s\n", "timestamp",        r.timestamp.c_str());
	std::printf("\n");
	return 0;
}

// ============================================================================
// dynx validate
// ============================================================================

int DynxCommand::cmd_validate(const std::vector<std::string>& args) {
	if (args.empty()) {
		std::fputs("Usage: vsepr dynx validate <file.dynx>\n", stdout);
		return 2;
	}
	const std::string& path = args[0];

	auto r = vsim::io::dynx_validate(path);

	std::printf("\n%s%s  Dynx Validate%s  %s%s%s\n\n",
		BOLD, CYN, RST, DIM, path.c_str(), RST);

	const char* mono_col = r.monotonic_time ? GRN : RED;
	const char* hash_col = r.hash_present   ? GRN : YEL;

	std::printf("  %-22s  %s%s%s\n", "frame_count",
		BOLD, std::to_string(r.frame_count).c_str(), RST);
	std::printf("  %-22s  %d\n",     "particle_count", r.particle_count);
	std::printf("  %-22s  %s%s%s\n", "monotonic_time",
		mono_col, r.monotonic_time ? "yes" : "NO", RST);
	std::printf("  %-22s  %s%s%s\n", "hash_present",
		hash_col, r.hash_present ? "yes" : "no (warning)", RST);

	for (const auto& w : r.warnings)
		std::printf("  %sWARN%s  %s\n", YEL, RST, w.c_str());

	if (!r.error.empty()) {
		std::printf("  %sFAIL%s  %s\n\n", RED, RST, r.error.c_str());
		return 1;
	}

	if (r.ok) {
		std::printf("\n  %s✓ valid%s\n\n", GRN, RST);
		return 0;
	} else {
		std::printf("\n  %s✗ invalid%s\n\n", RED, RST);
		return 1;
	}
}

} // namespace cli
} // namespace vsepr
