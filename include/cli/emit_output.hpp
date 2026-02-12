#pragma once

#include "cli/parse.hpp"
#include "cli/run_context.hpp"
#include <string>
#include <vector>

namespace vsepr {
namespace cli {

// Simple atom structure for output
struct Atom {
    std::string element;
    double x, y, z;
};

// Write XYZ file with metadata-rich comment line
void write_xyz(
    const std::string& path,
    const std::vector<Atom>& atoms,
    const ParsedCommand& cmd,
    const RunContext& ctx
);

} // namespace cli
} // namespace vsepr
