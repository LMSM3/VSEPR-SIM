#pragma once

#include "parse.hpp"
#include "run_context.hpp"

namespace vsepr {
namespace cli {

// Action entrypoints (replacing old tool main() functions)

// Generate structure without optimization
int action_emit(const ParsedCommand& cmd, RunContext& ctx);

// Energy minimization / structure relaxation
int action_relax(const ParsedCommand& cmd, RunContext& ctx);

// PMF formation sandbox (diffusion + checkpoint + metrics)
int action_form(const ParsedCommand& cmd, RunContext& ctx);

// Validate against known structure expectations
int action_test(const ParsedCommand& cmd, RunContext& ctx);

} // namespace cli
} // namespace vsepr
