#pragma once

#include "cli/parse.hpp"
#include "cli/run_context.hpp"

namespace vsepr {
namespace cli {

// Emit random gas cloud
int emit_gas(const ParsedCommand& cmd, RunContext& ctx);

} // namespace cli
} // namespace vsepr
