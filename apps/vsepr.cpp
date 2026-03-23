/**
 * vsepr.cpp - Unified CLI Entry Point
 * 
 * Grammar: vsepr <SPEC> <ACTION> [DOMAIN_PARAMS] [GLOBAL_FLAGS]
 * 
 * SPEC:
 *   - Formula: H2O, NaCl, C6H6
 *   - Mode hint: @gas, @crystal, @bulk, @molecule
 * 
 * ACTIONS:
 *   - emit: Generate structure without optimization
 *   - relax: Energy minimization
 *   - test: Validate against known expectations
 * 
 * DOMAIN RULES:
 *   - @crystal/@bulk → PBC mandatory, requires --cell
 *   - @gas/@molecule → PBC optional, requires --box if --pbc enabled
 */

#include "cli/parse.hpp"
#include "cli/run_context.hpp"
#include "cli/actions.hpp"
#include "cli/cg_commands.hpp"
#ifdef BUILD_VISUALIZATION
#include "coarse_grain/vis/cg_viz_viewer.hpp"
#include "cli/system_state.hpp"
#endif
#include <iostream>
#include <exception>

using namespace vsepr::cli;

void show_help() {
    std::cout << R"(
VSEPR CLI - Domain-Aware Molecular Simulation

USAGE:
    vsepr <SPEC> <ACTION> [DOMAIN_PARAMS] [GLOBAL_FLAGS]
    vsepr cg <COMMAND> [OPTIONS]
    vsepr --viz [OPTIONS]

SPEC:
    <FORMULA>[@MODE]
    
    Formula:
        H2O, NaCl, C6H6, Al, etc.
        Universal semantic anchor (composition only)
    
    Mode hints:
        @gas      - Isolated gas-phase system
        @crystal  - Crystalline solid (PBC mandatory)
        @bulk     - Bulk material (PBC mandatory)
        @molecule - Isolated molecule (default)

ACTIONS:
    emit     Generate structure without optimization
             --cloud <N>         Generate N atoms randomly
             --density <ρ>       Set packing density
             --preset <ID>       Use known crystal template
             --supercell n,n,n   Replicate unit cell (for crystals)

    relax    Energy minimization (FIRE algorithm)
             --steps <INT>       Max steps (default: 1000)
             --dt <FLOAT>        Timestep (default: 0.001)
             --in <PATH>         Input structure
             --config <PATH>     Full config file

    test     Validate against known expectations
             --preset <ID>       Known structure to test

DOMAIN PARAMETERS:
    --cell a,b,c    Unit cell dimensions (Å)
                    REQUIRED for @crystal and @bulk
    
    --box x,y,z     Bounding box (Å)
                    For confinement or non-crystal PBC
    
    --pbc           Enable PBC (only for @gas/@molecule)
                    FORBIDDEN for @crystal/@bulk (redundant)
                    If used, requires --box or --cell

GLOBAL FLAGS:
    --out <PATH>    Output file (default: out.xyz)
    --seed <INT>    RNG seed for reproducibility
    --viz           Launch viewer after completion (static snapshot)
    --watch         Launch live viewer (updates during simulation)

DOMAIN RULES (Enforced):
    @crystal → PBC ON (mandatory), requires --cell
    @bulk    → PBC ON (mandatory), requires --cell
    @gas     → PBC OFF by default, --pbc enables it
    @molecule → PBC OFF by default, --pbc enables it

EXAMPLES:
    # Generate NaCl crystal (atomistic preset with fixed params)
    vsepr NaCl@crystal emit --preset nacl_atomistic --out nacl_unit.xyz

    # Build 3×3×3 supercell of aluminum FCC
    vsepr Al@crystal emit --preset al --supercell 3,3,3 --out al_supercell.xyz

    # Relax water molecule
    vsepr H2O@molecule relax --steps 2000 --out water.xyz

    # Periodic gas box
    vsepr H2O@gas emit --cloud 200 --box 50,50,50 --pbc --seed 42

    # Custom NaCl with user-defined cell (motif-based preset)
    vsepr NaCl@crystal emit --preset rocksalt --cell 5.7,5.7,5.7 --out nacl_custom.xyz

    # Test crystal structure
    vsepr Al@crystal test --preset fcc

COARSE-GRAINED CONSOLE:
    vsepr cg <command> [options]

    Commands:
        scene     Build or load a bead scene from presets
        inspect   Inspect bead positions, environment, descriptors
        env       Run environment update pipeline (eta relaxation)
        interact  Evaluate pairwise interactions, energy decomposition
        viz       Launch lightweight bead viewer

    Examples:
        vsepr cg scene --preset pair --spacing 4.0
        vsepr cg env --preset stack --beads 8 --steps 200
        vsepr cg interact --preset pair --spacing 4.0 --all
        vsepr cg inspect --preset shell --beads 12 --env-steps 100
        vsepr cg viz --preset shell --beads 12 --overlay rho

    Run 'vsepr cg help' for full details.

LIGHTWEIGHT VIEWER:
    vsepr --viz [options]

    Opens a minimal bead viewer directly from the command line.
    Supports all scene presets and overlay modes.

    Examples:
        vsepr --viz --preset pair --spacing 4.0
        vsepr --viz --preset shell --beads 12 --env-steps 200 --overlay eta
        vsepr --viz --preset cloud --beads 20 --spacing 12.0

SEE ALSO:
    Full documentation: docs/VSEPR_CLI_GUIDE.md
    Grammar reference: docs/VSEPR_CLI_GRAMMAR.md

)";
}

int main(int argc, char** argv) {
    try {
        // Show help if no args or --help
        if (argc < 2 || std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h") {
            show_help();
            return 0;
        }

        // Route to coarse-grained console when argv[1] == "cg"
        if (std::string(argv[1]) == "cg") {
            return vsepr::cli::cg_dispatch(argc, argv);
        }

        // Route to lightweight visualization when argv[1] == "--viz"
        if (std::string(argv[1]) == "--viz") {
#ifdef BUILD_VISUALIZATION
            // Parse optional arguments: --viz [--preset X] [--overlay Y] ...
            // Reuse CG system state for scene construction
            vsepr::cli::CGSystemState state;
            coarse_grain::vis::VizConfig config;
            std::string preset_str = "pair";
            int n_beads = 5;
            double spacing = 4.0;
            uint32_t seed = 42;
            int env_steps = 0;

            for (int i = 2; i < argc; ++i) {
                std::string arg = argv[i];
                if (arg == "--preset" && i + 1 < argc) { preset_str = argv[++i]; }
                else if (arg == "--beads" && i + 1 < argc) { n_beads = std::atoi(argv[++i]); }
                else if (arg == "--spacing" && i + 1 < argc) { spacing = std::atof(argv[++i]); }
                else if (arg == "--seed" && i + 1 < argc) { seed = static_cast<uint32_t>(std::atoi(argv[++i])); }
                else if (arg == "--env-steps" && i + 1 < argc) { env_steps = std::atoi(argv[++i]); }
                else if (arg == "--overlay" && i + 1 < argc) {
                    config.overlay = coarse_grain::vis::parse_overlay_mode(argv[++i]);
                }
                else if (arg == "--no-axes") { config.show_axes = false; }
            }

            state.build_preset(vsepr::cli::parse_scene_preset(preset_str),
                               n_beads, spacing, seed);
            if (env_steps > 0) state.update_environment(env_steps);

            return coarse_grain::vis::CGVizViewer::run(state, config);
#else
            std::cerr << "Visualization not available. Rebuild with -DBUILD_VIS=ON\n";
            return 1;
#endif
        }

        // Parse command
        CommandParser parser;
        ParsedCommand cmd = parser.parse(argc, argv);
        
        // Build run context (validates domain rules)
        RunContext ctx = RunContext::from_parsed(cmd);
        
        // Dispatch to action handler
        switch (cmd.action) {
            case Action::Emit:
                return action_emit(cmd, ctx);

            case Action::Relax:
                return action_relax(cmd, ctx);

            case Action::Form:
                return action_form(cmd, ctx);

            case Action::Test:
                return action_test(cmd, ctx);

            default:
                std::cerr << "ERROR: Unknown action\n";
                return 1;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << "\n\n";
        std::cerr << "Run 'vsepr --help' for usage information.\n";
        return 1;
    }
}
