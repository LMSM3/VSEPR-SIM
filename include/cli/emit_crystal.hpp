#pragma once

#include "cli/parse.hpp"
#include "cli/run_context.hpp"
#include "cli/emit_output.hpp"
#include <vector>

namespace vsepr {
namespace cli {

// Emit crystal structure from preset template
int emit_crystal(const ParsedCommand& cmd, RunContext& ctx);

// Generate crystal structure atoms (for internal use by other actions)
std::vector<Atom> generate_crystal_atoms(
    const std::string& preset,
    const ParsedCommand& cmd,
    double a, double b, double c
);

} // namespace cli
} // namespace vsepr
