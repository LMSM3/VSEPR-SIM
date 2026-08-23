#include "cli/actions.hpp"
#include "cli/emit_crystal.hpp"
#include "cli/emit_gas.hpp"
#include <iostream>

namespace vsepr {
namespace cli {

int action_emit(const ParsedCommand& cmd, RunContext& ctx) {
    std::cout << "=== VSEPR EMIT ===\n\n";

    // Route based on mode and parameters

    // Crystal path: requires preset and PBC-enabled mode
    if (!cmd.action_params.preset.empty()) {
        if (!ctx.rules.pbc_enabled) {
            std::cerr << "ERROR: Presets require PBC-enabled mode (@crystal or @bulk)\n";
            std::cerr << "Current mode: @" << cmd.spec.mode_string() << "\n";
            std::cerr << "Try: " << cmd.spec.formula() << "@crystal emit --preset "
                      << cmd.action_params.preset << " --cell ...\n";
            return 1;
        }

        return emit_crystal(cmd, ctx);
    }

    // Gas/random cloud path: requires --cloud
    else if (cmd.action_params.cloud_size > 0) {
        if (ctx.cell_or_box.empty()) {
            std::cerr << "ERROR: Random cloud emission requires --box x,y,z\n";
            std::cerr << "Example: --box 50,50,50\n";
            return 1;
        }

        return emit_gas(cmd, ctx);
    }

    // No valid parameters
    else {
        std::cerr << "ERROR: emit requires either:\n";
        std::cerr << "  --preset <ID>  (for crystal structures with PBC)\n";
        std::cerr << "  --cloud <N>    (for random gas/molecular clouds)\n";
        std::cerr << "\n";
        std::cerr << "Examples:\n";
        std::cerr << "  vsepr NaCl@crystal emit --preset rocksalt --cell 5.64,5.64,5.64\n";
        std::cerr << "  vsepr H2O@gas emit --cloud 300 --box 50,50,50 --pbc\n";
        return 1;
    }
}

} // namespace cli
} // namespace vsepr
