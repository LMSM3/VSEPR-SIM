#pragma once
/**
 * cmd_run_vsim.hpp — vsper run <script.vsim> subcommand
 * ======================================================
 *
 * Parses a .vsim script, resolves the registry, runs the formation
 * pipeline, and writes requested export artifacts.
 *
 * Usage:
 *   vsper run scripts/demo_01_nacl_level0.vsim
 *   vsper run scripts/demo_03_graphite_stack.vsim
 *
 * Exit codes:
 *   0  — run completed (warnings may be present)
 *   1  — run failed (simulation / export error)
 *   2  — file not found / parse exception
 *
 * WO-57D  |  v5.0.0-beta.7
 */

#include <string>
#include <vector>

namespace vsepr::cli {

/**
 * Run the run subcommand.
 *
 * @param args  Remaining argv after "run" token.
 *              args[0] should be the .vsim file path.
 * @return      Exit code (0 = ok, 1 = run error, 2 = parse/IO error)
 */
int cmd_run_vsim(const std::vector<std::string>& args);

} // namespace vsepr::cli
