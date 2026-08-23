#pragma once
// =============================================================================
// cmd_x_suite.hpp  —  `vsepr x` subcommand declaration
// =============================================================================

#include <vector>
#include <string>

namespace vsepr {
namespace cli {

// Entry point for `vsepr x <sub-command> [args...]`
//
// Sub-commands:
//   inspect  <file.X>    — print suite contents and file status
//   validate <file.X>    — check file presence and contracts
//   run      <file.X>    — compile (if enabled) then run entry script
//   replay   <file.X>    — open trajectory/rich replay in desktop viewer
//   export   <file.X>    — regenerate reports and artifacts
//   open     <file.X>    — double-click launcher: 2 CMD + Qt desktop
//
// Returns 0 on success, non-zero on failure.

int cmd_x_suite(const std::vector<std::string>& args);

}} // namespace vsepr::cli
