/**
 * vsepr.cpp - Unified CLI Entry Point  (v5.0.0)
 *
 * v5 VSIM commands (primary):
 *   vsepr run    <script.vsim>  — parse, validate, and run a .vsim script
 *   vsepr validate <script.vsim> — validate a .vsim script without running
 *   vsepr doctor                — print installation health summary
 *   vsepr --version             — print version string
 *   vsepr --build-info          — print compiler / module build information
 *
 * Legacy v4 commands (fallback — still functional):
 *   vsepr <SPEC> <ACTION> [DOMAIN_PARAMS] [GLOBAL_FLAGS]
 *   vsepr therm / cg / gas / gas2 / gas3 / viz / serve / tui / modules
 */

#include "cli/parse.hpp"
#include "cli/run_context.hpp"
#include "cli/actions.hpp"
#include "cli/cg_commands.hpp"
#include "cli/cmd_therm.hpp"
#include "cli/cmd_tui.hpp"
#include "cli/cmd_validate.hpp"
#include "cli/cmd_run_vsim.hpp"
#include "core/gas_module.hpp"
#include "gas2/gas2_engine.hpp"
#include "gas3/gas3_engine.hpp"
#include "core/live_server.hpp"
#include "core/viz_server.hpp"
#include "core/module_registry.hpp"
#ifdef BUILD_VISUALIZATION
#include "coarse_grain/vis/cg_viz_viewer.hpp"
#include "cli/system_state.hpp"
#endif
#include <iostream>
#include <exception>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <cstdlib>

using namespace vsepr::cli;

void show_help() {
    std::cout << R"(
VSEPR-SIM v5.0.0 - Atomistic Simulation and Analysis Platform

USAGE:
    vsepr run    <script.vsim>     Run a .vsim simulation script
    vsepr validate <script.vsim>   Validate a .vsim script (no run)
    vsepr doctor                   Print installation health summary
    vsepr --version                Print version
    vsepr --build-info             Print compiler / module build info

LEGACY USAGE (v4 grammar, still supported):
    vsepr <SPEC> <ACTION> [DOMAIN_PARAMS] [GLOBAL_FLAGS]
    vsepr therm <input.xyz> [OPTIONS]
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
        fire      LL-FIRE minimization on bead system
        viz       Launch lightweight bead viewer

    Examples:
        vsepr cg scene --preset pair --spacing 4.0
        vsepr cg env --preset stack --beads 8 --steps 200
        vsepr cg interact --preset pair --spacing 4.0 --all
        vsepr cg fire --preset shell --beads 12 --spacing 5.0
        vsepr cg inspect --preset shell --beads 12 --env-steps 100
        vsepr cg viz --preset shell --beads 12 --overlay rho

    Run 'vsepr cg help' for full details.

GAS MODULE:
    vsepr gas <command> [options]

    Commands:
        props <FORMULA> [opts]   Compute gas properties (PV=nRT, VdW, kinetic)
        sample <FORMULA> [opts]  Sample Maxwell-Boltzmann velocities
        help                     Show gas module help

    Examples:
        vsepr gas props Ar -T 300 -P 1.0
        vsepr gas sample N2 -T 300 -N 5000 --histogram

GAS2 MODULE (Advanced Heat + Gas):
    vsepr gas2 <command> [options]

    Commands:
        analyze <FORMULA> [opts]   Full analysis (3-EOS + kinetic + thermal)
        thermal <FORMULA> [opts]   Thermal property report (DOF, Cp, Cv)
        compare <FORMULA> [opts]   Compare Ideal vs VdW vs Redlich-Kwong
        sample  <FORMULA> [opts]   Maxwell-Boltzmann velocity sampling
        species [FORMULA]          Show species database
        help                       Show gas2 module help

    Examples:
        vsepr gas2 analyze Ar -T 300 -P 1.0
        vsepr gas2 thermal CO2 -T 500
        vsepr gas2 compare N2 -T 200 -P 50
        vsepr gas2 species

GAS3 MODULE (Quality Pipeline + Fitting + Reporting):
    vsepr gas3 <command> [options]

    Commands:
        quick                      Quick sanity check (all species, STP)
        sweep [opts]               Linear deterministic sweep with export
        random [opts]              Random sampling stress test
        pipeline [opts]            Full quality pipeline (sweep + fit + report)
        help                       Show gas3 module help

    Options:
        --T-min, --T-max, --T-step   Temperature range (K)
        --P-grid <list>              Pressure points (atm, comma-separated)
        --random <N>                 Random sample count
        --species <list>             Species filter (comma-separated)
        --verbose                    Print every state to stdout

    Quality Tiers: Q4 (reference) > Q3 (production) > Q2 (usable) > Q1 (weak) > Q0 (failed)

    Examples:
        vsepr gas3 quick
        vsepr gas3 sweep --T-min 200 --T-max 1000 --T-step 25
        vsepr gas3 pipeline --species CO2,N2,Ar --verbose
        vsepr gas3 pipeline --random 2000

VIZ STREAM SERVER
    vsepr viz <FORMULA> [options]

    Port 9999  -> Atomic View  (live atom positions, bonds, lattice, HUD)
    Port 10001 -> Analysis View (graphs, candidates, property panels)

    Connect viewers after starting the server:
        python tools/viz_atomic.py
        python tools/viz_analysis.py

    Options:
        -T <K>              Temperature (or single T for constant)
        --T-start/--T-end   Temperature sweep range
        -N <INT>            Number of atoms in view (default: 64)
        --frames <INT>      Total frames (0 = infinite)
        --atomic-fps <N>    Atomic view frame rate (default: 15)
        --analysis-fps <N>  Analysis view frame rate (default: 2)
        --verbose           Print frame stats to stdout

    Examples:
        vsepr viz Ar -T 300
        vsepr viz N2 --T-start 77 --T-end 500 -N 125
        vsepr viz CO2 -T 400 --frames 500 --atomic-fps 20

LIVE ANALYSIS SERVER:
    vsepr serve [options]

    Zero-input HTTP server streaming random molecular analysis.
    Starts on port 99998 with no configuration required.

    Options:
        --port <PORT>       Listen port (default: 99998)
        --interval <MS>     Cycle interval in ms (default: 3000)
        --seed <S>          Base RNG seed (default: time-based)

    Endpoints:
        /              HTML dashboard (auto-updating)
        /stream        Server-Sent Events
        /snapshot      Latest analysis as JSON
        /status        Server uptime and stats

    Examples:
        vsepr serve
        vsepr serve --port 8080 --interval 1000

MODULE REGISTRY:
    vsepr modules              List all registered modules
    vsepr help <module>        Detailed help for a specific module

THERMAL ANALYSIS:
    vsepr therm <input.xyz> [options]

    Options:
        --temperature, -T <K>   Set temperature (default: 298.15 K)
        --generate-object, -g   Generate thermal object file
        --viz                   Enhanced visualization output
        --generations <N>       Run thermal evolution over N generations
        --sample-interval <M>   Sample every Mth generation

    Examples:
        vsepr therm water.xyz
        vsepr therm molecule.xyz --temperature 500
        vsepr therm diamond.xyz -T 1000 --generate-object --viz

LIGHTWEIGHT VIEWER:
    vsepr --viz [options]

    Opens a minimal bead viewer directly from the command line.
    Supports all scene presets and overlay modes.

    Examples:
        vsepr --viz --preset pair --spacing 4.0
        vsepr --viz --preset shell --beads 12 --env-steps 200 --overlay eta
        vsepr --viz --preset cloud --beads 20 --spacing 12.0

SEE ALSO:
    Full documentation:    docs/INDEX.md
    CLI walkthrough:       docs/CLI_WALKTHROUGH.txt
    File format reference: docs/FILE_FORMATS.md

)";
}

int main(int argc, char** argv) {
    try {
        // Show help if no args or --help
        if (argc < 2 || std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h") {
            show_help();
            return 0;
        }

        const std::string cmd = argv[1];

        // ── v5 VSIM commands ───────────────────────────────────────────────

        // Version flag
        if (cmd == "--version" || cmd == "-v" || cmd == "version") {
            std::cout << "VSEPR-SIM v5.0.0\n";
            return 0;
        }

        // Build-info flag
        if (cmd == "--build-info" || cmd == "build-info") {
            std::cout << "VSEPR-SIM v5.0.0  |  C++23  |  branch: v5.0.0-beta.7-step-attempt\n";
            std::cout << "  Compiler: " << __VERSION__ << "\n";
            return 0;
        }

        // run <script.vsim>
        if (cmd == "run") {
            std::vector<std::string> rest;
            for (int i = 2; i < argc; ++i) rest.emplace_back(argv[i]);
            return vsepr::cli::cmd_run_vsim(rest);
        }

        // validate <script.vsim>
        if (cmd == "validate") {
            std::vector<std::string> rest;
            for (int i = 2; i < argc; ++i) rest.emplace_back(argv[i]);
            return vsepr::cli::cmd_validate(rest);
        }

        // doctor — real installation health check
        if (cmd == "doctor") {
            namespace fs = std::filesystem;

            const std::string SEP  = std::string(54, '-');
            const std::string PASS = "  [ok]   ";
            const std::string FAIL = "  [FAIL] ";
            const std::string WARN = "  [warn] ";

            std::cout << "VSEPR-SIM v5.0.0  installation health\n" << SEP << "\n\n";

            int failures = 0;
            int warnings = 0;

            // ── 1. Locate install root ────────────────────────────────────
            // Prefer %LOCALAPPDATA%\VSEPR-SIM, fall back to working directory
            std::string localAppData;
            if (const char* p = std::getenv("LOCALAPPDATA")) localAppData = p;
            fs::path installRoot = localAppData.empty()
                ? fs::current_path()
                : fs::path(localAppData) / "VSEPR-SIM";

            // Also check dev-local layout (repo root next to exe)
            fs::path exeDir = fs::path(argv[0]).parent_path();
            fs::path repoRoot; // repo-local data (build tree)
            // Walk up from exe until we find CMakeLists.txt
            for (fs::path p = exeDir; p != p.parent_path(); p = p.parent_path()) {
                if (fs::exists(p / "CMakeLists.txt")) { repoRoot = p; break; }
            }

            bool isDevBuild = !repoRoot.empty();
            fs::path dataDir    = isDevBuild ? repoRoot / "data"
                                              : installRoot / "runtime" / "data" / "data";
            fs::path scriptsDir = isDevBuild ? repoRoot / "scripts"
                                              : installRoot / "examples";

            std::cout << "  binary      : " << fs::weakly_canonical(argv[0]).string() << "\n";
            std::cout << "  install root: " << installRoot.string() << "\n";
            std::cout << "  data dir    : " << dataDir.string() << "\n";
            std::cout << "  scripts dir : " << scriptsDir.string() << "\n";
            if (isDevBuild)
                std::cout << "  mode        : dev-build (repo at " << repoRoot.string() << ")\n";
            else
                std::cout << "  mode        : installed\n";
            std::cout << "\n";

            // ── 2. Install manifest ───────────────────────────────────────
            std::cout << "Install manifest:\n";
            fs::path manifest = installRoot / "install_manifest.json";
            if (fs::exists(manifest)) {
                std::cout << PASS << "install_manifest.json found\n";
            } else if (isDevBuild) {
                std::cout << WARN << "install_manifest.json not found (dev-build — expected)\n";
                ++warnings;
            } else {
                std::cout << FAIL << "install_manifest.json missing — run install-vsepr.ps1\n";
                ++failures;
            }

            // ── 3. Required runtime data files ───────────────────────────
            std::cout << "\nRuntime data files:\n";
            const std::vector<std::string> requiredData = {
                "PeriodicTableJSON.json",
                "elements.physics.json",
                "elements.vsepr.json",
                "element_weights.json",
                "periodic_table_102.json",
                "isotopes.vsepr.json",
                "polarizability_ref.csv",
                "states_db.csv",
            };
            for (const auto& f : requiredData) {
                fs::path p = dataDir / f;
                if (fs::exists(p)) {
                    std::cout << PASS << f << "\n";
                } else {
                    std::cout << FAIL << f << "  (expected: " << p.string() << ")\n";
                    ++failures;
                }
            }

            // ── 4. Demo / example scripts ─────────────────────────────────
            std::cout << "\nExample scripts:\n";
            const std::vector<std::string> demoScripts = {
                "demo_01_nacl_level0.vsim",
                "demo_02_silicon_diamond.vsim",
                "demo_03_pbc_nacl.vsim",
                "golden_tests.vsim",
            };
            int scriptsMissing = 0;
            for (const auto& f : demoScripts) {
                fs::path p = scriptsDir / f;
                if (fs::exists(p)) {
                    std::cout << PASS << f << "\n";
                } else {
                    std::cout << WARN << f << " not found\n";
                    ++scriptsMissing;
                }
            }
            if (scriptsMissing > 0) {
                std::cout << WARN << scriptsMissing
                          << " example script(s) missing — reinstall with examples enabled\n";
                ++warnings;
            }

            // ── 5. v5 feature checks ──────────────────────────────────────
            std::cout << "\nv5 pipeline features:\n";
            // Validate: run cmd_validate against a known demo script if available
            fs::path nacl = scriptsDir / "demo_01_nacl_level0.vsim";
            if (fs::exists(nacl)) {
                // Suppress sub-command output during doctor probe
                std::streambuf* coutBuf = std::cout.rdbuf(nullptr);
                std::vector<std::string> vargs = { nacl.string() };
                int rc = vsepr::cli::cmd_validate(vargs);
                std::cout.rdbuf(coutBuf);
                if (rc == 0) {
                    std::cout << PASS << "validate pipeline (demo_01_nacl_level0.vsim)\n";
                } else {
                    std::cout << FAIL << "validate pipeline returned error " << rc << "\n";
                    ++failures;
                }
            } else {
                std::cout << WARN << "validate pipeline — no demo script to probe\n";
                ++warnings;
            }

            // Run: same file
            if (fs::exists(nacl)) {
                std::streambuf* coutBuf = std::cout.rdbuf(nullptr);
                std::vector<std::string> rargs = { nacl.string() };
                int rc = vsepr::cli::cmd_run_vsim(rargs);
                std::cout.rdbuf(coutBuf);
                if (rc == 0) {
                    std::cout << PASS << "run pipeline (demo_01_nacl_level0.vsim)\n";
                } else {
                    std::cout << FAIL << "run pipeline returned error " << rc << "\n";
                    ++failures;
                }
            } else {
                std::cout << WARN << "run pipeline — no demo script to probe\n";
                ++warnings;
            }

            // ── 6. PATH check ─────────────────────────────────────────────
            std::cout << "\nPATH:\n";
            fs::path installedBin = installRoot / "bin";
            std::string pathEnv;
            if (const char* p = std::getenv("PATH")) pathEnv = p;
            if (!localAppData.empty() && pathEnv.find(installedBin.string()) != std::string::npos) {
                std::cout << PASS << "installed bin is in PATH\n";
            } else if (isDevBuild) {
                std::cout << WARN << "installed bin not in PATH (dev-build — use build/ directly)\n";
                ++warnings;
            } else {
                std::cout << FAIL << "installed bin not in PATH — re-run install-vsepr.ps1\n";
                ++failures;
            }

            // ── Summary ───────────────────────────────────────────────────
            std::cout << "\n" << SEP << "\n";
            if (failures == 0 && warnings == 0) {
                std::cout << "  PASS — installation is complete and healthy.\n";
                return 0;
            } else if (failures == 0) {
                std::cout << "  PASS with warnings — " << warnings
                          << " warning(s), 0 failure(s).\n";
                return 0;
            } else {
                std::cout << "  INCOMPLETE — " << failures << " failure(s), "
                          << warnings << " warning(s).\n";
                std::cout << "  Run: dist\\VSEPR-SIM-5.0.0-local\\install-vsepr.ps1\n";
                return 2;
            }
        }

        // ── Legacy v4 routing ─────────────────────────────────────────────

        // Route to module registry listing
        if (cmd == "modules") {
            vsepr::modules::register_builtin_modules();
            vsepr::ModuleRegistry::instance().print_table();
            return 0;
        }

        // Route to per-module help: vsepr help <module>
        if (cmd == "help" && argc >= 3) {
            vsepr::modules::register_builtin_modules();
            vsepr::ModuleRegistry::instance().print_help(argv[2]);
            return 0;
        }

        // Route to gas module when argv[1] == "gas"
        if (cmd == "gas") {
            return vsepr::gas::gas_dispatch(argc, argv);
        }

        // Route to gas2 module (advanced heat + gas analysis)
        if (cmd == "gas2") {
            return vsepr::gas2::gas2_dispatch(argc, argv);
        }

        // Route to gas3 module (quality pipeline, fitting, reporting)
        if (cmd == "gas3") {
            return vsepr::gas3::gas3_dispatch(argc, argv);
        }

        // Route to dual-port viz stream server when argv[1] == "viz"
        if (cmd == "viz") {
            return vsepr::viz::viz_dispatch(argc, argv);
        }

        // Route to live analysis server when argv[1] == "serve"
        if (cmd == "serve") {
            return vsepr::live::serve_dispatch(argc, argv);
        }

        // Route to coarse-grained console when argv[1] == "cg"
        if (cmd == "cg") {
            return vsepr::cli::cg_dispatch(argc, argv);
        }

        // Route to TUI terminal XYZ viewer when argv[1] == "tui"
        if (cmd == "tui") {
            vsepr::cli::TuiCommand tui;
            std::vector<std::string> tui_args;
            for (int i = 2; i < argc; ++i) {
                tui_args.push_back(argv[i]);
            }
            return tui.Execute(tui_args);
        }

        // Route to thermal analysis when argv[1] == "therm"
        if (cmd == "therm") {
            vsepr::cli::ThermCommand therm;
            std::vector<std::string> therm_args;
            for (int i = 2; i < argc; ++i) {
                therm_args.push_back(argv[i]);
            }
            return therm.Execute(therm_args);
        }

        // Route to lightweight visualization when argv[1] == "--viz"
        if (cmd == "--viz") {
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

        // Parse command (legacy v4 grammar fallback)
        CommandParser parser;
        ParsedCommand parsed_cmd = parser.parse(argc, argv);

        // Build run context (validates domain rules)
        RunContext ctx = RunContext::from_parsed(parsed_cmd);

        // Dispatch to action handler
        switch (parsed_cmd.action) {
            case Action::Emit:
                return action_emit(parsed_cmd, ctx);

            case Action::Relax:
                return action_relax(parsed_cmd, ctx);

            case Action::Form:
                return action_form(parsed_cmd, ctx);

            case Action::Test:
                return action_test(parsed_cmd, ctx);

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
