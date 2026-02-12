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
#include <iostream>
#include <exception>

using namespace vsepr::cli;

void show_help() {
    std::cout << R"(
VSEPR CLI - Domain-Aware Molecular Simulation

USAGE:
    vsepr <SPEC> <ACTION> [DOMAIN_PARAMS] [GLOBAL_FLAGS]

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
             --cloud <N>       Generate N atoms randomly
             --density <ρ>     Set packing density
             --preset <ID>     Use known template
    
    relax    Energy minimization (FIRE algorithm)
             --steps <INT>     Max steps (default: 1000)
             --dt <FLOAT>      Timestep (default: 0.001)
             --in <PATH>       Input structure
             --config <PATH>   Full config file
    
    test     Validate against known expectations
             --preset <ID>     Known structure to test

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
    # Generate NaCl crystal
    vsepr NaCl@crystal emit --cell 5.64,5.64,5.64 --preset rocksalt
    
    # Relax water molecule
    vsepr H2O@molecule relax --steps 2000 --out water.xyz
    
    # Periodic gas box
    vsepr H2O@gas emit --cloud 200 --box 50,50,50 --pbc --seed 42
    
    # Test crystal structure
    vsepr Al@crystal test --cell 4.05,4.05,4.05 --preset fcc

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
