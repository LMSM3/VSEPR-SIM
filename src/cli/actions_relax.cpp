#include "cli/actions.hpp"
#include <iostream>

namespace vsepr {
namespace cli {

int action_relax(const ParsedCommand& cmd, RunContext& ctx) {
    std::cout << "=== ACTION: relax ===\n";
    std::cout << "Formula: " << cmd.spec.formula() << "\n";
    std::cout << "Mode: " << cmd.spec.mode_string() << "\n";
    std::cout << "PBC: " << (ctx.rules.pbc_enabled ? "ON" : "OFF");
    if (ctx.rules.pbc_mandatory) std::cout << " (mandatory)";
    std::cout << "\n";
    
    if (!ctx.cell_or_box.empty()) {
        std::cout << (ctx.has_cell ? "Cell" : "Box") << ": "
                  << ctx.cell_or_box[0] << ", "
                  << ctx.cell_or_box[1] << ", "
                  << ctx.cell_or_box[2] << "\n";
    }
    
    std::cout << "\nRelaxation parameters:\n";
    std::cout << "  Steps: " << cmd.action_params.steps << "\n";
    std::cout << "  Timestep: " << cmd.action_params.dt << "\n";
    if (!cmd.action_params.input_file.empty()) {
        std::cout << "  Input: " << cmd.action_params.input_file << "\n";
    }
    if (!cmd.action_params.config_file.empty()) {
        std::cout << "  Config: " << cmd.action_params.config_file << "\n";
    }

    std::cout << "\nSeed: " << ctx.seed << "\n";
    std::cout << "Output: " << ctx.output_path << "\n";
    // Removed: Viz (never implemented, use separate viewers)

    std::cout << "\n[NOT IMPLEMENTED YET]\n";
    return 1;
}

} // namespace cli
} // namespace vsepr
