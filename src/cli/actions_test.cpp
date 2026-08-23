#include "cli/actions.hpp"
#include <iostream>

namespace vsepr {
namespace cli {

int action_test(const ParsedCommand& cmd, RunContext& ctx) {
    std::cout << "=== ACTION: test ===\n";
    std::cout << "Formula: " << cmd.spec.formula() << "\n";
    std::cout << "Mode: " << cmd.spec.mode_string() << "\n";
    std::cout << "PBC: " << (ctx.rules.pbc_enabled ? "ON" : "OFF");
    if (ctx.rules.pbc_mandatory) std::cout << " (mandatory)";
    std::cout << "\n";
    
    if (!cmd.action_params.preset.empty()) {
        std::cout << "  Preset: " << cmd.action_params.preset << "\n";
    }
    
    std::cout << "\n[NOT IMPLEMENTED YET]\n";
    return 1;
}

} // namespace cli
} // namespace vsepr
