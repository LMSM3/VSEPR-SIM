#pragma once
/**
 * cmd_validate.hpp — vsper validate subcommand
 * =============================================
 *
 * Parses a .vsim script file, validates its structure, and
 * prints a human-readable report to stdout.
 *
 * Usage:
 *   vsper validate scripts/demo_01_minimal_hexene.vsim
 *   vsper validate scripts/demo_02_graphene_sheet.vsim
 *   vsper validate scripts/demo_03_graphite_stack.vsim
 *
 * Exit codes:
 *   0  — document is valid (warnings may be present)
 *   1  — document has errors
 *   2  — file not found / parse exception
 *
 * WO-56C  |  v5.0.0-beta.7
 */

#include <string>
#include <vector>

namespace vsepr::cli {

/**
 * Run the validate subcommand.
 *
 * @param args  Remaining argv after "validate" token.
 *              args[0] should be the .vsim file path.
 * @return      Exit code (0 = ok, 1 = validation errors, 2 = parse/IO error)
 */
int cmd_validate(const std::vector<std::string>& args);

} // namespace vsepr::cli
