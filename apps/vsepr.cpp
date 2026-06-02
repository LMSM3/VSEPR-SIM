/**
 * vsepr.cpp - Unified CLI Entry Point  (v5.1.4)
 *
 * v5 VSIM commands (primary):
 *   vsepr run    <script.vsim>   -  parse, validate, and run a .vsim script
 *   vsepr validate <script.vsim>  -  validate a .vsim script without running
 *   vsepr doctor                 -  print installation health summary
 *   vsepr --version              -  print version string
 *   vsepr --build-info           -  print compiler / module build information
 *
 * Legacy v4 commands (fallback  -  still functional):
 *   vsepr <SPEC> <ACTION> [DOMAIN_PARAMS] [GLOBAL_FLAGS]
 *   vsepr therm / cg / gas / gas2 / gas3 / viz / serve / tui / modules
 */

#include "cli/parse.hpp"
#include "vsim/settings/visual_settings.hpp"
#include "vsim/demo/demo_molecule.hpp"
#include "cli/run_context.hpp"
#include "cli/actions.hpp"
#include "cli/cg_commands.hpp"
#include "cli/cmd_therm.hpp"
#include "cli/cmd_tui.hpp"
#include "cli/cmd_validate.hpp"
#include "cli/cmd_run_vsim.hpp"
#include "cli/cmd_doctor_tests.hpp"
#include "cli/cmd_x_suite.hpp"
#include "cli/cmd_cache.hpp"
#include "cli/cmd_mlprop.hpp"
#include "cli/cmd_new_wizard.hpp"
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
#include <cstring>
#include <iomanip>
#include <algorithm>
#ifdef _WIN32
#  include <windows.h>
#endif

using namespace vsepr::cli;

// ============================================================================
// Post-install welcome screen  (shown on bare `vsepr` invocation)
// ============================================================================

static void welcome_line(const char* color, const char* tag,
                         const char* cmd, const char* desc)
{
    // tag   e.g. "  RUN   "
    // cmd   e.g. "vsepr run <script.vsim>"
    // desc  e.g. "Execute a .vsim simulation script"
    std::cout << color << tag << "\033[0m  "
              << "\033[0;36m" << cmd << "\033[0m"
              << std::string(std::max(0, 44 - (int)std::strlen(cmd)), ' ')
              << "\033[0;37m" << desc << "\033[0m\n";
}

static void welcome_section(const char* title)
{
    std::cout << "\n\033[1;33m  " << title << "\033[0m\n";
    std::cout << "  " << std::string(62, '-') << "\n";
}

static std::string health_dot(bool ok)
{
    return ok ? "\033[0;32m●\033[0m" : "\033[0;31m●\033[0m";
}

static void show_welcome()
{
    namespace fs = std::filesystem;

    // ── Enable VT sequences on Windows ──────────────────────────────────────
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD mode = 0;
        GetConsoleMode(hOut, &mode);
        SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
#endif

    // ── Banner ───────────────────────────────────────────────────────────────
    std::cout
        << "\n"
        << "\033[1;35m  ╔══════════════════════════════════════════════════════════════╗\033[0m\n"
        << "\033[1;35m  ║  \033[0m"
        << "\033[1;37mVSEPR-SIM\033[0m  "
        << "\033[0;36mv5.1.4\033[0m"
        << "  \033[0;37m│  atomistic simulation & analysis platform\033[0m"
        << "\033[1;35m  ║\033[0m\n"
        << "\033[1;35m  ╚══════════════════════════════════════════════════════════════╝\033[0m\n";

    // ── Build strip ─────────────────────────────────────────────────────────
    std::cout
        << "  \033[0;37mCompiler : \033[0;36m" << __VERSION__ << "\033[0m\n"
        << "  \033[0;37mBranch   : \033[0;36mv5.0.0-main\033[0m\n"
        << "  \033[0;37mStandard : \033[0;36mC++23\033[0m\n";

    // ── Install health (fast file-presence check, no pipeline run) ──────────
    welcome_section("INSTALL HEALTH");

    // Locate data dir (same strategy as doctor command)
    fs::path exeDir;
    {
        std::string localAppData;
        if (const char* p = std::getenv("LOCALAPPDATA")) localAppData = p;
        exeDir = fs::path(localAppData.empty() ? "." : localAppData) / "VSEPR-SIM";
    }
    // Also try beside the exe on PATH
    fs::path dataDir = exeDir / "data";
    if (!fs::exists(dataDir)) {
        // Fallback: look relative to cwd
        fs::path cwd_data = fs::current_path() / "data";
        if (fs::exists(cwd_data)) dataDir = cwd_data;
    }

    const std::vector<std::pair<std::string,std::string>> dataFiles = {
        { "PeriodicTableJSON.json",  "Element table"         },
        { "elements.physics.json",   "Physics properties"    },
        { "elements.vsepr.json",     "VSEPR geometry rules"  },
        { "element_weights.json",    "Atomic weights"        },
        { "periodic_table_102.json", "Full Z=1-102 table"    },
        { "states_db.csv",           "Phase state database"  },
        { "polarizability_ref.csv",  "Polarizability data"   },
    };

    int data_ok = 0, data_miss = 0;
    for (const auto& [fname, label] : dataFiles) {
        bool ok = fs::exists(dataDir / fname);
        ok ? ++data_ok : ++data_miss;
        std::cout << "    " << health_dot(ok) << "  " << std::left
                  << std::setw(38) << label
                  << "  \033[0;37m" << fname << "\033[0m\n";
    }

    // Neighbor exe check (beside vsepr.exe)
    struct NeighborCheck { const char* exe; const char* role; };
    const std::vector<NeighborCheck> neighbors = {
        { "vsepr-desktop.exe",  "Qt workstation GUI"    },
        { "vsepr-launcher.exe", "File association handler" },
        { "property_train.exe", "Property-based trainer" },
        { "continual_runner.exe","Continual formation engine"},
        { "vsepr-view-bench.exe","View demo bench suite" },
    };
    // Try to locate the exe beside this process
    fs::path binDir;
    {
        // On Windows, argv[0] gives the full exe path after exec
        // Use current_path as approximation if not determinable
        binDir = fs::current_path();
    }
    std::cout << "\n  \033[1;33m  Companion executables\033[0m\n"
              << "  " << std::string(62, '-') << "\n";
    for (const auto& n : neighbors) {
        bool ok = fs::exists(binDir / n.exe);
        std::cout << "    " << health_dot(ok) << "  " << std::left
                  << std::setw(28) << n.role
                  << "  \033[0;37m" << n.exe << "\033[0m\n";
    }

    if (data_miss > 0) {
        std::cout << "\n  \033[1;33m  [!]\033[0m  "
                  << data_miss << " data file(s) missing. "
                  << "Run \033[0;36mvsepr doctor\033[0m for details.\n";
    } else {
        std::cout << "\n  \033[0;32m  [ok]\033[0m  All " << data_ok
                  << " runtime data files present.\n";
    }

    // ── Command reference ────────────────────────────────────────────────────
    welcome_section("SIMULATION  (primary workflow)");
    welcome_line("\033[0;32m", "  RUN     ", "vsepr run <script.vsim>",       "Execute a .vsim simulation script");
    welcome_line("\033[0;32m", "  RUN     ", "vsepr <script.vsim>",           "Short form — no subcommand needed");
    welcome_line("\033[0;36m", "  CHECK   ", "vsepr validate <script.vsim>",  "Parse & validate without running");
    welcome_line("\033[0;36m", "  VIEW    ", "vsepr view <file>",             "Open .xyz / .xyzFull / .dynx viewer");

    welcome_section("TRAINING  (property-based continual engine)");
    welcome_line("\033[0;35m", "  TRAIN   ", "property_train --formations 100 --seeds 3",
                                             "Run property-based invariant training");
    welcome_line("\033[0;35m", "  TRAIN   ", "property_train --help",         "Full training CLI options");
    welcome_line("\033[0;35m", "  BENCH   ", "vsepr-view-bench --bench",      "Run all 21 view demo scenarios");
    welcome_line("\033[0;35m", "  BENCH   ", "continual_runner",              "Continual formation engine");

    welcome_section("ANALYSIS  (modules)");
    welcome_line("\033[0;36m", "  MODULE  ", "vsepr gas   <cmd>",             "Gas-phase thermodynamics");
    welcome_line("\033[0;36m", "  MODULE  ", "vsepr gas2  <cmd>",             "Advanced EOS analysis");
    welcome_line("\033[0;36m", "  MODULE  ", "vsepr gas3  <cmd>",             "Quality pipeline & fitting");
    welcome_line("\033[0;36m", "  MODULE  ", "vsepr cg    <cmd>",             "Coarse-grained bead scene");
    welcome_line("\033[0;36m", "  MODULE  ", "vsepr therm <file>",            "Thermal analysis on .xyz");
    welcome_line("\033[0;36m", "  MODULE  ", "vsepr x run <file.X>",         "Run a saved .X suite file");
    welcome_line("\033[0;36m", "  MODULE  ", "vsepr mlprop <formula>",        "ML material-property recommender");

    welcome_section("DESKTOP  (Qt workstation)");
    welcome_line("\033[1;37m", "  LAUNCH  ", "vsepr-desktop",                 "Full Qt workstation (OpenGL + docks)");
    welcome_line("\033[1;37m", "  LAUNCH  ", "vsepr-launcher <file.vsim>",    "Qt launcher (file association handler)");
    welcome_line("\033[1;37m", "  DEMO    ", "vsepr --demo",                  "Rotating molecule viewer (random)");
    welcome_line("\033[1;37m", "  DEMO    ", "vsepr --demo --list",           "List all 20 demo molecules");
    welcome_line("\033[1;37m", "  DEMO    ", "vsepr --demo0",                 "Element tour Z=1..102");

    welcome_section("INSTALLATION & HEALTH");
    welcome_line("\033[1;33m", "  HEALTH  ", "vsepr doctor",                  "Full install health report");
    welcome_line("\033[1;33m", "  HEALTH  ", "vsepr doctor integratedtest",   "Run 37-test integration suite");
    welcome_line("\033[1;33m", "  HEALTH  ", "vsepr doctor benchmark",        "Throughput benchmarks");
    welcome_line("\033[1;33m", "  INSTALL ", "vsepr install register-associations", "Register .vsim/.x/.dynx with shell");
    welcome_line("\033[1;33m", "  CONFIG  ", "vsepr settings show",           "View visual settings");
    welcome_line("\033[1;33m", "  CONFIG  ", "vsepr settings reset",          "Write default settings file");

    welcome_section("META");
    welcome_line("\033[0;37m", "  INFO    ", "vsepr --version",               "Print version string");
    welcome_line("\033[0;37m", "  INFO    ", "vsepr --build-info",            "Compiler / module build info");
    welcome_line("\033[0;37m", "  INFO    ", "vsepr --help",                  "Full command reference");
    welcome_line("\033[0;37m", "  INFO    ", "vsepr modules",                 "List registered simulation modules");

    // ── Quick-start example ──────────────────────────────────────────────────
    std::cout
        << "\n\033[1;35m  ╔══ QUICK START ══════════════════════════════════════════════════╗\033[0m\n"
        << "\033[1;35m  ║\033[0m  \033[0;37mCreate a minimal script and run it:\033[0m\033[1;35m                         ║\033[0m\n"
        << "\033[1;35m  ║\033[0m\033[1;35m                                                                ║\033[0m\n"
        << "\033[1;35m  ║\033[0m  \033[0;36m[project]\033[0m                                               \033[1;35m  ║\033[0m\n"
        << "\033[1;35m  ║\033[0m  \033[0;37mname = my_first_run\033[0m                                     \033[1;35m  ║\033[0m\n"
        << "\033[1;35m  ║\033[0m  \033[0;36m[objects]\033[0m                                               \033[1;35m  ║\033[0m\n"
        << "\033[1;35m  ║\033[0m  \033[0;37msystem.crystal = CrystalModule(lattice=fcc, a=3.52, species=[Ni])\033[0m\033[1;35m║\033[0m\n"
        << "\033[1;35m  ║\033[0m  \033[0;36m[run]\033[0m                                                   \033[1;35m  ║\033[0m\n"
        << "\033[1;35m  ║\033[0m  \033[0;37mmode = relax   max_steps = 600\033[0m                          \033[1;35m  ║\033[0m\n"
        << "\033[1;35m  ║\033[0m\033[1;35m                                                                ║\033[0m\n"
        << "\033[1;35m  ║\033[0m  Then: \033[0;32mvsepr run my_first_run.vsim\033[0m                         \033[1;35m   ║\033[0m\n"
        << "\033[1;35m  ║\033[0m  Docs: \033[0;36mdocs/VSIM_LANGUAGE.md   VSIM_REFERENCE.md\033[0m             \033[1;35m  ║\033[0m\n"
        << "\033[1;35m  ╚══════════════════════════════════════════════════════════════╝\033[0m\n\n";
}

void show_help() {
    std::cout << R"(
VSEPR-SIM v5.1.4  |  atomistic simulation and analysis platform

USAGE
    vsepr <script.vsim>            Run a .vsim simulation script (short form)
    vsepr run      <script.vsim>   Run a .vsim simulation script
    vsepr validate <script.vsim>   Validate script syntax without running
    vsepr view     <file>             Open a file in the lightweight viewer
    vsepr doctor                      Print installation health summary
    vsepr doctor integratedtest       Run dependency-ordered integration tests
    vsepr doctor benchmark            Run timed throughput benchmarks
    vsepr install register-associations [--dry-run]  Register shell file associations
    vsepr install unregister-associations            Remove shell file associations
    vsepr --demo                       Rotating molecule viewer (random each run)
    vsepr --demo<N>                    Specific molecule by number, e.g. --demo3
    vsepr --demo --list                List all 20 molecules with their numbers
    vsepr --demo --molecule <name>    Force a specific molecule (H2O CH4 NH3 SF6 ...)
    vsepr --demo0                      Element tour: cycles Z=1..102 at semi-random rate
    vsepr --demo0 --start <Z>          Start element tour from atomic number Z
    vsepr --demo0 --list               List all 102 elements in the tour
    vsepr settings show               Show current visual settings
    vsepr settings set <key> <val>    Set a visual setting
    vsepr settings reset              Write default visual settings file
    vsepr --version                   Print version string
    vsepr --build-info                Print compiler / module build information
    vsepr --help                      This help text

SIMULATION SCRIPTS (.vsim)
    Scripts are plain-text files with INI-style sections.
    Minimal script:

        [project]
        name = my_run

        [objects]
        system.crystal = CrystalModule(lattice = fcc, a = 3.52, species = [Ni])

        [run]
        mode      = relax
        max_steps = 600

    Full language reference:  docs/VSIM_LANGUAGE.md
    Section reference:        VSIM_REFERENCE.md

VIEWER
    vsepr view molecule.xyz         Single-frame static geometry
    vsepr view run.xyzFull          Multi-frame trajectory browser
    vsepr view session.dynx         Dynamic session archive
    vsepr view --small run.xyzf     Compact 700x900 window

    Requires vsepr-light-view in PATH or next to vsepr.exe.
    Build with:  cmake --preset vis && cmake --build build_vis --target vsepr-light-view

SUITE FILES (.X)
    vsepr x run      file.X         Run a saved suite
    vsepr x inspect  file.X         Show suite contents
    vsepr x validate file.X         Check file presence and contracts
    vsepr x replay   file.X         Replay trajectory from a suite

DOCTOR
    vsepr doctor                    Full health check (data files, PATH, pipeline)
    vsepr doctor integratedtest     Integration test sequence
    vsepr doctor benchmark          Throughput benchmark

OTHER MODULES
    vsepr gas   <cmd>   Gas-phase thermodynamics (props / sample)
    vsepr gas2  <cmd>   Advanced EOS analysis (analyze / thermal / compare)
    vsepr gas3  <cmd>   Quality pipeline (sweep / pipeline / quick)
    vsepr cg    <cmd>   Coarse-grained bead scene and viewer
    vsepr therm <file>  Thermal analysis on an .xyz structure
    vsepr viz   <formula>  Dual-port live viz stream server (ports 9999 + 10001)
    vsepr serve         Live analysis HTTP server (port 99998)
    vsepr modules       List all registered modules

SEE ALSO
    VSIM_REFERENCE.md             Complete section/field reference
    docs/VSIM_LANGUAGE.md         .vsim language specification
    docs/CLI_WALKTHROUGH.txt      Worked examples
    docs/FILE_FORMATS.md          Output artifact formats

)";
}

int main(int argc, char** argv) {
    try {
        // Bare invocation — post-install welcome screen + optional script wizard (WO-72K / WO-72K-EXT)
        if (argc < 2) {
            show_welcome();

            // Prompt: enter wizard or exit
            std::cout << "\033[1;33m  Press N to create a new script with the wizard,"
                      << " or Enter to exit.\n  > \033[0m";
            std::string ans;
            std::getline(std::cin, ans);

            if (ans == "n" || ans == "N" || ans == "new" || ans == "NEW") {
                int wrc = vsepr::cli::cmd_new_wizard();

                // Sentinel 2 = user asked to validate the generated file
                if (wrc == 2) {
                    // Re-read the filename the wizard wrote from cwd
                    namespace fs = std::filesystem;
                    // Find the most recently written .vsim in cwd
                    fs::path newest;
                    std::filesystem::file_time_type newest_t{};
                    for (const auto& e : fs::directory_iterator(fs::current_path())) {
                        if (e.path().extension() == ".vsim") {
                            auto t = fs::last_write_time(e);
                            if (newest.empty() || t > newest_t) {
                                newest_t = t;
                                newest   = e.path();
                            }
                        }
                    }
                    if (!newest.empty()) {
                        std::cout << "\n\033[0;36m  Running: vsepr validate "
                                  << newest.filename().string() << "\033[0m\n\n";
                        std::vector<std::string> vargs = { newest.string() };
                        return vsepr::cli::cmd_validate(vargs);
                    }
                }
                return (wrc == 2) ? 0 : wrc;
            }
            return 0;
        }

        const std::string cmd = argv[1];

        // Demo0 — element tour (Z=1..102, semi-random dwell)
        // --demo0 / demo0 : cycle through all 102 elements
        {
            bool is_demo0 = (cmd == "--demo0" || cmd == "demo0");
            if (is_demo0) {
                std::vector<std::string> rest;
                for (int i = 2; i < argc; ++i) rest.emplace_back(argv[i]);
                std::vector<const char*> dargv;
                for (auto& s : rest) dargv.push_back(s.c_str());
                return vsim::demo::run_demo_element_tour(
                    (int)dargv.size(), const_cast<char**>(dargv.data()));
            }
        }

        // Demo — rotating molecule viewer
        // --demo          : random molecule
        // --demo<N>       : molecule N (1-based, e.g. --demo3)
        // --demo --list   : list all molecules with their numbers
        // Aliases without leading '--' also accepted (demo, demo3, etc.)
        {
            auto is_demo_cmd = [](const std::string& s) -> int {
                // Returns -1 = not a demo cmd, 0 = bare demo, >0 = 1-based index
                for (auto& pfx : { std::string("--demo"), std::string("demo") }) {
                    if (s.size() >= pfx.size() && s.substr(0, pfx.size()) == pfx) {
                        std::string tail = s.substr(pfx.size());
                        if (tail.empty()) return 0;
                        bool alldig = true;
                        for (char c : tail) if (c < '0' || c > '9') { alldig = false; break; }
                        if (alldig) return std::stoi(tail);
                    }
                }
                return -1;
            };
            int demo_n = is_demo_cmd(cmd);
            if (demo_n >= 0) {
                std::vector<std::string> rest;
                for (int i = 2; i < argc; ++i) rest.emplace_back(argv[i]);
                std::vector<const char*> dargv;
                for (auto& s : rest) dargv.push_back(s.c_str());
                int forced = (demo_n > 0) ? (demo_n - 1) : -1;
                return vsim::demo::run_demo((int)dargv.size(),
                                            const_cast<char**>(dargv.data()),
                                            forced);
            }
        }

        // Full help — only on explicit request
        if (cmd == "--help" || cmd == "-h") {
            show_help();
            return 0;
        }

        // -- v5 VSIM commands -----------------------------------------------

        // Version flag
        if (cmd == "--version" || cmd == "-v" || cmd == "version") {
            std::cout << "VSEPR-SIM v5.1.4\n";
            return 0;
        }

        // Build-info flag
        if (cmd == "--build-info" || cmd == "build-info") {
            std::cout << "VSEPR-SIM v5.1.4  |  C++23  |  branch: v5.0.0-main\n";
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

        // doctor  -  real installation health check (+ integratedtest / benchmark)
        if (cmd == "doctor") {
            // Sub-commands: integratedtest, benchmark
            if (argc >= 3) {
                std::string sub = argv[2];
                if (sub == "integratedtest" || sub == "itest") {
                    std::vector<std::string> rest;
                    for (int i = 3; i < argc; ++i) rest.emplace_back(argv[i]);
                    return vsepr::cli::cmd_integrated_test(rest);
                } else if (sub == "benchmark" || sub == "bench") {
                    std::vector<std::string> rest;
                    for (int i = 3; i < argc; ++i) rest.emplace_back(argv[i]);
                    return vsepr::cli::cmd_benchmark(rest);
                } else {
                    std::fprintf(stderr,
                        "unknown doctor sub-command '%s'\n"
                        "  valid: integratedtest (itest) | benchmark (bench)\n",
                        sub.c_str());
                    return 2;
                }
            }

            namespace fs = std::filesystem;

            const std::string SEP  = std::string(54, '-');
            const std::string PASS = "  [ok]   ";
            const std::string FAIL = "  [FAIL] ";
            const std::string WARN = "  [warn] ";

            std::cout << "VSEPR-SIM v5.1.4  installation health\n" << SEP << "\n\n";

            int failures = 0;
            int warnings = 0;

            // -- 1. Locate install root ------------------------------------
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

            // -- 2. Install manifest ---------------------------------------
            std::cout << "Install manifest:\n";
            fs::path manifest = installRoot / "install_manifest.json";
            if (fs::exists(manifest)) {
                std::cout << PASS << "install_manifest.json found\n";
            } else if (isDevBuild) {
                std::cout << WARN << "install_manifest.json not found (dev-build  -  expected)\n";
                ++warnings;
            } else {
                std::cout << FAIL << "install_manifest.json missing  -  run install-vsepr.ps1\n";
                ++failures;
            }

            // -- 3. Required runtime data files ---------------------------
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

            // -- 4. Demo / example scripts ---------------------------------
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
                          << " example script(s) missing  -  reinstall with examples enabled\n";
                ++warnings;
            }

            // -- 5. v5 feature checks --------------------------------------
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
                std::cout << WARN << "validate pipeline  -  no demo script to probe\n";
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
                std::cout << WARN << "run pipeline  -  no demo script to probe\n";
                ++warnings;
            }

            // -- 6. PATH check ---------------------------------------------
            std::cout << "\nPATH:\n";
            fs::path installedBin = installRoot / "bin";
            std::string pathEnv;
            if (const char* p = std::getenv("PATH")) pathEnv = p;
            if (!localAppData.empty() && pathEnv.find(installedBin.string()) != std::string::npos) {
                std::cout << PASS << "installed bin is in PATH\n";
            } else if (isDevBuild) {
                std::cout << WARN << "installed bin not in PATH (dev-build  -  use build/ directly)\n";
                ++warnings;
            } else {
                std::cout << FAIL << "installed bin not in PATH  -  re-run install-vsepr.ps1\n";
                ++failures;
            }

            // -- Summary ---------------------------------------------------
            std::cout << "\n" << SEP << "\n";
            if (failures == 0 && warnings == 0) {
                std::cout << "  PASS  -  installation is complete and healthy.\n";
                return 0;
            } else if (failures == 0) {
                std::cout << "  PASS with warnings  -  " << warnings
                          << " warning(s), 0 failure(s).\n";
                return 0;
            } else {
                std::cout << "  INCOMPLETE  -  " << failures << " failure(s), "
                          << warnings << " warning(s).\n";
                std::cout << "  Run: dist\\VSEPR-SIM-5.1.4-local\\install-vsepr.ps1\n";
                return 2;
            }
        }

        // -- install --------------------------------------------------------
        // vsepr install register-associations   [--dry-run] [--unregister]
        // vsepr install unregister-associations
        if (cmd == "install") {
            std::string sub = (argc >= 3) ? argv[2] : "";
            bool unregister = false;
            bool dryRun = false;
            for (int i = 3; i < argc; ++i) {
                std::string a = argv[i];
                if (a == "--unregister" || a == "unregister") unregister = true;
                if (a == "--dry-run"    || a == "dry-run")    dryRun = true;
            }

            if (sub == "register-associations" || sub == "register") {
                // Locate the PowerShell script next to the binary, or in installer/
                namespace fs = std::filesystem;
                fs::path exeDir = fs::path(argv[0]).parent_path();
                fs::path script;
                for (auto&& candidate : {
                        exeDir / "register-file-associations.ps1",
                        exeDir / ".." / "installer" / "register-file-associations.ps1",
                        fs::path("installer") / "register-file-associations.ps1" }) {
                    if (fs::exists(candidate)) { script = candidate; break; }
                }
                if (script.empty()) {
                    std::cerr << "register-file-associations.ps1 not found.\n"
                              << "  searched next to binary and installer/ directory.\n";
                    return 2;
                }

                std::string psArgs = unregister ? " -Unregister" : "";
                if (dryRun) psArgs += " -DryRun";
                std::string cmd_line = "powershell -ExecutionPolicy Bypass -File \""
                                     + fs::weakly_canonical(script).string()
                                     + "\"" + psArgs;
                std::cout << "Running: " << cmd_line << "\n";
                return std::system(cmd_line.c_str());

            } else if (sub == "unregister-associations" || sub == "unregister") {
                namespace fs = std::filesystem;
                fs::path exeDir = fs::path(argv[0]).parent_path();
                fs::path script;
                for (auto&& candidate : {
                        exeDir / "register-file-associations.ps1",
                        exeDir / ".." / "installer" / "register-file-associations.ps1",
                        fs::path("installer") / "register-file-associations.ps1" }) {
                    if (fs::exists(candidate)) { script = candidate; break; }
                }
                if (script.empty()) {
                    std::cerr << "register-file-associations.ps1 not found.\n";
                    return 2;
                }
                std::string cmd_line = "powershell -ExecutionPolicy Bypass -File \""
                                     + fs::weakly_canonical(script).string()
                                     + "\" -Unregister";
                std::cout << "Running: " << cmd_line << "\n";
                return std::system(cmd_line.c_str());

            } else {
                std::cout << "VSEPR-SIM install commands:\n"
                          << "  vsepr install register-associations [--dry-run]\n"
                          << "      Register .vsim / .dynx / .X / .xyzFull etc. with Windows shell\n"
                          << "  vsepr install unregister-associations\n"
                          << "      Remove all VSEPR-SIM file associations\n";
                return 0;
            }
        }

        // -- settings -------------------------------------------------------
        // vsepr settings show
        // vsepr settings set <key> <value>
        // vsepr settings reset
        if (cmd == "settings") {
            namespace fs = std::filesystem;
            std::string localAppData;
            if (const char* p = std::getenv("LOCALAPPDATA")) localAppData = p;
            fs::path cfgPath = localAppData.empty()
                ? fs::path("visual_settings.json")
                : fs::path(localAppData) / "VSEPR-SIM" / "config" / "visual_settings.json";

            std::string sub = (argc >= 3) ? argv[2] : "show";

            if (sub == "show") {
                if (!fs::exists(cfgPath)) {
                    std::cout << "No visual settings file found at:\n  "
                              << cfgPath.string() << "\n"
                              << "Run 'vsepr settings reset' to create defaults.\n";
                    return 0;
                }
                std::ifstream f(cfgPath);
                std::cout << f.rdbuf() << "\n";
                return 0;

            } else if (sub == "set" && argc >= 5) {
                std::string key   = argv[3];
                std::string value = argv[4];
                // Load current settings (or defaults), apply the key, save
                vsim::VisualSettings vs = vsim::VisualSettings::load(cfgPath);
                vs.apply_key(key, value);
                vs.save(cfgPath);
                std::cout << "  [ok] " << key << " = " << value << "\n";
                return 0;

            } else if (sub == "reset") {
                fs::create_directories(cfgPath.parent_path());
                std::ofstream fout(cfgPath);
                fout << "{\n"
                     << "  \"theme\": \"dark\",\n"
                     << "  \"atom_radius_scale\": \"1.0\",\n"
                     << "  \"bond_radius\": \"0.15\",\n"
                     << "  \"background_color\": \"0x1a1a2e\",\n"
                     << "  \"label_font_size\": \"12\",\n"
                     << "  \"show_axes\": \"true\",\n"
                     << "  \"show_bonds\": \"true\",\n"
                     << "  \"show_unit_cell\": \"true\",\n"
                     << "  \"trajectory_fps\": \"30\"\n"
                     << "}\n";
                std::cout << "Visual settings reset to defaults at:\n  "
                          << cfgPath.string() << "\n";
                return 0;

            } else {
                std::cout << "VSEPR-SIM settings commands:\n"
                          << "  vsepr settings show              Show current visual settings\n"
                          << "  vsepr settings set <key> <val>   Set a visual setting\n"
                          << "  vsepr settings reset             Write default settings file\n"
                          << "\nSettings file: " << cfgPath.string() << "\n"
                          << "\nDefault keys:\n"
                          << "  theme                 dark | light\n"
                          << "  atom_radius_scale     float (default 1.0)\n"
                          << "  bond_radius           float (default 0.15)\n"
                          << "  background_color      hex RGB (default 0x1a1a2e)\n"
                          << "  label_font_size       int   (default 12)\n"
                          << "  show_axes             true | false\n"
                          << "  show_bonds            true | false\n"
                          << "  show_unit_cell        true | false\n"
                          << "  trajectory_fps        int   (default 30)\n";
                return 0;
            }
        }

        // -- Legacy v4 routing ---------------------------------------------

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

        // x <sub-command> <file.X>  — saved-run suite commands (WO-XYZSUITE-X)
        if (cmd == "x") {
            std::vector<std::string> rest;
            for (int i = 2; i < argc; ++i) rest.emplace_back(argv[i]);
            return vsepr::cli::cmd_x_suite(rest);
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

        // mlprop  --  WO-72W material-property ML recommender
        if (cmd == "mlprop") {
            std::vector<std::string> rest;
            for (int i = 2; i < argc; ++i) rest.emplace_back(argv[i]);
            return vsepr::cli::cmd_mlprop(rest);
        }

        // cache  --  WO-72V precomputed cache queries
        if (cmd == "cache") {
            std::vector<std::string> rest;
            for (int i = 2; i < argc; ++i) rest.emplace_back(argv[i]);
            return vsepr::cli::cmd_cache(rest);
        }

        // Bare .vsim file (WO-72K) — vsepr scriptname.vsim (no subcommand needed)
        // Canonicalize path so relative paths from drag-and-drop work correctly.
        if (cmd.size() > 5 && cmd.substr(cmd.size() - 5) == ".vsim") {
            namespace fs = std::filesystem;
            fs::path vsim_path = fs::weakly_canonical(fs::path(cmd));
            if (!fs::exists(vsim_path)) {
                std::cerr << "ERROR: file not found: " << vsim_path.string() << "\n";
                std::cerr << "  Usage: vsepr run <script.vsim>\n";
                return 1;
            }
            std::vector<std::string> rest;
            rest.push_back(vsim_path.string());
            for (int i = 2; i < argc; ++i) rest.emplace_back(argv[i]);
            return vsepr::cli::cmd_run_vsim(rest);
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
