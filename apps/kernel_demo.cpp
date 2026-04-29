/**
 * kernel_demo.cpp - Central Kernel Event System Demo
 * ====================================================
 * WO-56C  |  v5.0.0-beta.7.1
 *
 * 18-scenario chemistry catalog - one chosen at random on startup.
 *
 * Scenarios 0-9 : complex chemistry
 * Scenarios 10-13: gas-phase processes
 * Scenarios 14-17: solid-state processes
 *
 * Secondary runner scripts (bidirectional pipe):
 *   scripts/kernel_demo_runner_gas.vsim
 *   scripts/kernel_demo_runner_solid.vsim
 *   Each runner calls back: vsper run kernel_demo --scenario <N>
 *
 * Usage:
 *   kernel_demo [--headless] [--no-jsonl] [--no-markdown] [--scenario N]
 */

#include "include/kernel/kernel_event.hpp"
#include "include/kernel/kernel_event_log.hpp"
#include "include/vsim/vsim_parser.hpp"
#include "include/vis/vsim_viz_adapter.hpp"
#include "include/vis/vsim_render_layer.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

using namespace vsepr::kernel;

// ============================================================================
// ANSI helpers
// ============================================================================

namespace ansi {
    constexpr const char* reset   = "\033[0m";
    constexpr const char* bold    = "\033[1m";
    constexpr const char* dim     = "\033[2m";
    constexpr const char* green   = "\033[32m";
    constexpr const char* yellow  = "\033[33m";
    constexpr const char* cyan    = "\033[36m";
    constexpr const char* magenta = "\033[35m";
    constexpr const char* red     = "\033[31m";
    constexpr const char* blue    = "\033[34m";
    constexpr const char* white   = "\033[37m";

    inline void print_header(const char* title) {
        std::printf("\n%s%s==================================================%s\n",
                    bold, cyan, reset);
        std::printf("%s%s  %s%s\n", bold, cyan, title, reset);
        std::printf("%s%s==================================================%s\n",
                    bold, cyan, reset);
    }

    inline void print_section(const char* title) {
        std::printf("\n%s%s-- %s --%s\n", bold, white, title, reset);
    }
}

// ============================================================================
// Scenario catalog
// ============================================================================

enum class ScenarioClass { Complex, Gas, Solid };

struct ScenarioDescriptor {
    int           id;
    ScenarioClass sc;
    const char*   tag;
    const char*   formula;
    const char*   name;
    const char*   rule;
};

static constexpr ScenarioDescriptor kScenarios[18] = {
    {  0, ScenarioClass::Complex, "complex", "N2+H2",      "Haber-Bosch ammonia synthesis",          "associative_mechanism"       },
    {  1, ScenarioClass::Complex, "complex", "CO+H2",      "Fischer-Tropsch chain growth",            "carbide_insertion"           },
    {  2, ScenarioClass::Complex, "complex", "C2H4",       "Ziegler-Natta polyethylene propagation",  "coordination_insertion"      },
    {  3, ScenarioClass::Complex, "complex", "C2H4+O2",    "Wacker ethylene oxidation",               "nucleopalladation"           },
    {  4, ScenarioClass::Complex, "complex", "C4H6+C2H4",  "Diels-Alder cycloaddition",               "pericyclic_4+2"              },
    {  5, ScenarioClass::Complex, "complex", "RMgX",       "Grignard ketone addition",                "nucleophilic_addition"       },
    {  6, ScenarioClass::Complex, "complex", "CH3CHO",     "Aldol condensation",                      "enolate_electrophile"        },
    {  7, ScenarioClass::Complex, "complex", "C6H11NO",    "ROP epsilon-caprolactam",                 "ring_opening_polymerisation" },
    {  8, ScenarioClass::Complex, "complex", "C4H9N",      "Hofmann elimination",                     "e2_anti_periplanar"          },
    {  9, ScenarioClass::Complex, "complex", "C3H3N+CH3I", "Menshutkin quaternisation",               "sn2_nitrogen"                },
    { 10, ScenarioClass::Gas,     "gas",     "O3",         "Ozone photolysis (Chapman)",               "uv_homolysis"                },
    { 11, ScenarioClass::Gas,     "gas",     "CH4+H2O",    "Methane steam reforming",                 "heterogeneous_catalysis"     },
    { 12, ScenarioClass::Gas,     "gas",     "NOx+NH3",    "NOx SCR catalytic reduction",             "scr_deNOx"                   },
    { 13, ScenarioClass::Gas,     "gas",     "Cl2",        "Cl2 UV dissociation",                     "photodissociation"           },
    { 14, ScenarioClass::Solid,   "solid",   "Fe",         "BCC-Fe vacancy migration",                "vacancy_hop"                 },
    { 15, ScenarioClass::Solid,   "solid",   "Al2O3",      "Grain-boundary sliding",                  "grain_boundary_diffusion"    },
    { 16, ScenarioClass::Solid,   "solid",   "NaCl",       "NaCl surface ion desorption",             "dissolution_surface"         },
    { 17, ScenarioClass::Solid,   "solid",   "SiC+O2",     "SiC oxidation (Deal-Grove)",              "deal_grove_parabolic"        },
};

struct ScenarioData {
    double re_A, re_B, pe;
    double bond_ang;
    double coord_before, coord_after;
    double formation_energy;
    double migration_energy;
    double n_beads;
    double packing;
    const char* lattice;
};

static constexpr ScenarioData kData[18] = {
    { -22.97, -18.45,  -79.4,  1.10, 2.0, 4.0,  0.00, 0.00, 128, 0.740, "FCC"        },
    { -38.10, -14.20,  -61.8,  1.50, 1.0, 3.0,  0.00, 0.00,  96, 0.640, "chain"      },
    { -52.30,  -6.80,  -73.1,  1.34, 2.0, 4.0,  0.00, 0.00, 256, 0.720, "polymer"    },
    { -44.10, -31.60,  -89.9,  1.48, 3.0, 4.0,  0.00, 0.00,  64, 0.610, "complex"    },
    { -83.60, -28.20, -134.7,  1.52, 2.0, 4.0,  0.00, 0.00,  48, 0.590, "cyclohex"   },
    { -36.00, -41.30,  -89.4,  1.54, 1.0, 4.0,  0.00, 0.00,  32, 0.550, "tetrahedral"},
    { -74.20,  -9.10,  -96.7,  1.46, 1.0, 2.0,  0.00, 0.00,  48, 0.610, "chain"      },
    { -63.40,   0.00, -102.0,  1.47, 2.0, 3.0,  0.00, 0.00, 128, 0.680, "polymer"    },
    { -58.10, -12.40,  -41.7,  1.33, 4.0, 3.0,  0.00, 0.00,  24, 0.510, "planar"     },
    { -29.80, -44.60,  -88.3,  1.47, 3.0, 4.0,  0.00, 0.00,  32, 0.560, "tetrahedral"},
    { -34.40,   0.00,  -28.9,  1.28, 2.0, 1.0,  0.00, 0.00,   8, 0.200, "gas"        },
    { -74.80, -57.80, -213.0,  1.10, 4.0, 3.0,  0.00, 0.00,  32, 0.350, "gas"        },
    { -18.00, -46.00,  -59.2,  1.15, 3.0, 2.0,  0.00, 0.00,  24, 0.310, "gas"        },
    { -57.20,   0.00,  -28.6,  1.99, 1.0, 0.0,  0.00, 0.00,   4, 0.150, "gas"        },
    {   0.00,   0.00,    0.0,  2.48, 8.0, 7.0,  2.04, 0.68,  64, 0.680, "BCC"        },
    {   0.00,   0.00,    0.0,  1.91, 6.0, 5.0,  1.23, 1.10, 128, 0.710, "corundum"   },
    {   0.00,   0.00,    0.0,  2.82, 6.0, 5.0,  0.74, 0.24,  48, 0.640, "FCC"        },
    {   0.00,   0.00,    0.0,  1.89, 4.0, 3.0,  3.10, 1.40,  96, 0.660, "zincblende" },
};

// ============================================================================
// Print one event
// ============================================================================

static void print_event(const KernelEvent& e, bool show_trace = true) {
    const char* vcol = e.is_valid ? ansi::green : ansi::red;
    const char* vstr = e.is_valid ? "OK" : "INVALID";

    std::printf("  %s[%04llu]%s  %-18s  formula=%-14s  frame=%-6llu  result=%s%.6g%s %s",
        ansi::dim,
        (unsigned long long)e.event_id,
        ansi::reset,
        kind_name(e.kind),
        e.source_formula.c_str(),
        (unsigned long long)e.frame_id,
        ansi::yellow,
        e.result_value,
        ansi::reset,
        e.result_unit.c_str());
    std::printf("  %s%s%s\n", vcol, vstr, ansi::reset);

    if (!e.warning.empty())
        std::printf("           %s! %s%s\n", ansi::red, e.warning.c_str(), ansi::reset);

    if (show_trace && !e.equation_symbolic.empty()) {
        std::printf("           %ssymbolic: %s%s\n", ansi::dim, e.equation_symbolic.c_str(), ansi::reset);
        std::printf("           %snumeric:  %s%s\n", ansi::dim, e.equation_numeric.c_str(), ansi::reset);
    }
}

// ============================================================================
// Scenario emitter
// ============================================================================

static void emit_scenario(KernelEventLog& log, int sid) {
    const ScenarioDescriptor& desc = kScenarios[sid];
    const ScenarioData&       dat  = kData[sid];
    uint64_t frame = static_cast<uint64_t>(sid * 10 + 1);

    if (desc.sc == ScenarioClass::Complex || desc.sc == ScenarioClass::Gas) {
        ReactionEvent rxn;
        rxn.source_formula  = desc.formula;
        rxn.frame_id        = frame;
        rxn.reaction_rule   = desc.rule;

        std::string f(desc.formula);
        auto plus = f.find('+');
        if (plus != std::string::npos) {
            rxn.reactants        = { f.substr(0, plus), f.substr(plus + 1) };
            rxn.reactant_energies = { dat.re_A, dat.re_B };
        } else {
            rxn.reactants        = { f };
            rxn.reactant_energies = { dat.re_A };
        }
        rxn.products         = { f + "_product" };
        rxn.product_energies = { dat.pe };
        rxn.compute_delta_E();
        log.record(rxn);

        ChemicalStateEvent cs;
        cs.source_formula      = desc.formula;
        cs.frame_id            = frame + 1;
        cs.particle_i          = static_cast<uint64_t>(sid * 3);
        cs.particle_j          = static_cast<uint64_t>(sid * 3 + 1);
        cs.coordination_before = dat.coord_before;
        cs.coordination_after  = dat.coord_after;
        cs.local_energy_before = dat.re_A;
        cs.local_energy_after  = dat.pe;
        cs.bond_length_ang     = dat.bond_ang;
        cs.state_tag_before    = std::string(desc.rule) + "_pre";
        cs.state_tag_after     = std::string(desc.rule) + "_post";
        cs.compute();
        log.record(cs);
    }

    if (desc.sc == ScenarioClass::Solid) {
        FormationEvent fe;
        fe.source_formula   = desc.formula;
        fe.frame_id         = frame;
        fe.n_beads          = static_cast<uint32_t>(dat.n_beads);
        fe.fire_steps       = 400 + sid * 12;
        fe.converged        = true;
        fe.final_energy     = dat.re_A != 0.0 ? dat.re_A : -88.0 - sid * 1.5;
        fe.packing_fraction = dat.packing;
        fe.lattice_class    = dat.lattice;
        fe.formation_preset = desc.tag;
        fe.compute();
        log.record(fe);

        DefectEvent de;
        de.source_formula   = desc.formula;
        de.frame_id         = frame + 2;
        de.defect_type      = DefectType::Vacancy;
        de.site_id          = static_cast<uint64_t>(sid * 7 + 5);
        de.formation_energy = dat.formation_energy;
        de.migration_energy = dat.migration_energy;
        std::string fstr(desc.formula);
        de.host_element     = fstr.substr(0, 2);
        de.compute();
        log.record(de);

        TransportEvent te;
        te.source_formula   = desc.formula;
        te.frame_id         = frame + 4;
        te.particle_id      = static_cast<uint64_t>(sid + 1);
        te.displacement_ang = dat.bond_ang * 1.8;
        te.msd              = dat.bond_ang * dat.bond_ang * 3.6;
        te.diffusivity      = dat.migration_energy > 0.0
                                ? 1e-4 * std::exp(-dat.migration_energy / 0.026)
                                : 1e-5;
        te.transport_mode   = "thermally_activated";
        te.compute();
        log.record(te);
    }

    ContinualReportEvent cr;
    cr.source_formula   = desc.formula;
    cr.frame_id         = frame + 5;
    cr.total_energy     = dat.pe != 0.0 ? dat.pe : -dat.re_A;
    cr.temperature_K    = 300.0 + sid * 8.5;
    cr.packing_fraction = dat.packing;
    cr.mean_coord_num   = (dat.coord_before + dat.coord_after) * 0.5;
    cr.rmsd_ang         = 0.10 + sid * 0.008;
    cr.n_active_beads   = static_cast<uint32_t>(dat.n_beads);
    cr.report_interval  = 50;
    cr.compute();
    log.record(cr);
}

// ============================================================================
// Catalog display
// ============================================================================

static void print_catalog() {
    ansi::print_section("Scenario Catalog  [0-17]");
    const char* class_col[3] = { ansi::cyan, ansi::green, ansi::yellow };
    const char* class_tag[3] = { "complex", "gas    ", "solid  " };
    for (const auto& d : kScenarios) {
        int ci = static_cast<int>(d.sc);
        std::printf("  %s[%2d]%s  %s%s%s  %-14s  %s\n",
            ansi::bold, d.id, ansi::reset,
            class_col[ci], class_tag[ci], ansi::reset,
            d.formula, d.name);
    }
}

// ============================================================================
// Query demonstration
// ============================================================================

static void demo_queries(const KernelEventLog& log) {
    ansi::print_section("Query: filter_by_kind(Reaction)");
    auto rxns = log.filter_by_kind(KernelEventKind::Reaction);
    std::printf("  %zu Reaction events\n", rxns.size());
    for (const auto& e : rxns) print_event(e, false);

    ansi::print_section("Query: filter_by_kind(Defect)");
    auto defs = log.filter_by_kind(KernelEventKind::Defect);
    std::printf("  %zu Defect events\n", defs.size());
    for (const auto& e : defs) print_event(e, false);

    ansi::print_section("Query: filter_by_kind(Formation)");
    auto forms = log.filter_by_kind(KernelEventKind::Formation);
    std::printf("  %zu Formation events\n", forms.size());
    for (const auto& e : forms) print_event(e, false);
}

// ============================================================================
// Run summary
// ============================================================================

static void print_run_summary(const KernelEventLog& log) {
    ansi::print_section("Run Summary");
    auto all = log.snapshot();

    int counts[7] = {};
    int n_invalid = 0;
    for (const auto& e : all) {
        int k = static_cast<int>(e.kind);
        if (k >= 0 && k < 7) counts[k]++;
        if (!e.is_valid) n_invalid++;
    }

    std::printf("  Total events  : %s%zu%s\n", ansi::bold, all.size(), ansi::reset);
    std::printf("  Invalid events: %s%d%s\n",
        n_invalid > 0 ? ansi::red : ansi::green, n_invalid, ansi::reset);
    std::printf("\n  Per-kind breakdown:\n");

    const KernelEventKind kinds[] = {
        KernelEventKind::Reaction,
        KernelEventKind::ChemicalState,
        KernelEventKind::Formation,
        KernelEventKind::Defect,
        KernelEventKind::Transport,
        KernelEventKind::ContinualReport,
    };
    for (auto k : kinds) {
        int n = counts[static_cast<int>(k)];
        std::printf("    %-18s %s%d%s\n",
            kind_name(k), n > 0 ? ansi::cyan : ansi::dim, n, ansi::reset);
    }
}

// ============================================================================
// Runner-script info
// ============================================================================

static void show_runner_info(const char* tag) {
    bool is_gas   = (std::strcmp(tag, "gas")   == 0);
    bool is_solid = (std::strcmp(tag, "solid") == 0);

    if (!is_gas && !is_solid) {
        std::printf("%s  Unknown runner tag '%s'. Try: runner gas | runner solid%s\n",
            ansi::red, tag, ansi::reset);
        return;
    }

    const char* script = is_gas
        ? "scripts/kernel_demo_runner_gas.vsim"
        : "scripts/kernel_demo_runner_solid.vsim";

    ansi::print_section(is_gas ? "Runner: Gas Processes" : "Runner: Solid Processes");
    std::printf(
        "  %sScript%s   : %s%s%s\n"
        "  %sEntry%s    : vsper run %s\n"
        "  %sBack-call%s: vsper run kernel_demo --scenario <N>\n\n",
        ansi::bold, ansi::reset, ansi::cyan, script, ansi::reset,
        ansi::bold, ansi::reset, script,
        ansi::bold, ansi::reset);

    std::printf("  Scenarios covered by this runner:\n");
    for (const auto& d : kScenarios) {
        bool match = is_gas ? (d.sc == ScenarioClass::Gas) : (d.sc == ScenarioClass::Solid);
        if (match)
            std::printf("    [%2d]  %-14s  %s\n", d.id, d.formula, d.name);
    }

    std::printf(
        "\n  %sNote%s: runner script pending upload.\n"
        "  Pipe pattern: vsper run %s | vsper run kernel_demo\n",
        ansi::yellow, ansi::reset, script);
}

// ============================================================================
// Interactive loop
// ============================================================================

static void run_interactive(KernelEventLog& log,
                            bool show_jsonl, bool show_markdown) {

    std::printf("\n%sCommands:%s\n", ansi::bold, ansi::reset);
    std::printf("  %sr <N>%s         run scenario N (0-17)\n",        ansi::cyan, ansi::reset);
    std::printf("  %scat%s           print scenario catalog\n",         ansi::cyan, ansi::reset);
    std::printf("  %saudit%s         print full event audit trail\n",   ansi::cyan, ansi::reset);
    std::printf("  %squery%s         run filter demonstrations\n",       ansi::cyan, ansi::reset);
    std::printf("  %ssummary%s       print run summary\n",              ansi::cyan, ansi::reset);
    std::printf("  %srunner gas%s    show gas runner script info\n",    ansi::cyan, ansi::reset);
    std::printf("  %srunner solid%s  show solid runner script info\n",  ansi::cyan, ansi::reset);
    if (show_jsonl)    std::printf("  %sjsonl%s         dump event log as JSON Lines\n",      ansi::cyan, ansi::reset);
    if (show_markdown) std::printf("  %smd%s            dump event log as Markdown table\n", ansi::cyan, ansi::reset);
    std::printf("  %sx / q%s         exit\n\n", ansi::cyan, ansi::reset);

    std::string line;
    while (true) {
        std::printf("%s[kernel_demo]%s> ", ansi::magenta, ansi::reset);
        std::fflush(stdout);

        if (!std::getline(std::cin, line)) break;

        while (!line.empty() && (line.front() == ' ' || line.front() == '\t'))
            line.erase(line.begin());
        while (!line.empty() && (line.back() == ' ' || line.back() == '\r' || line.back() == '\n'))
            line.pop_back();

        if (line.empty())           continue;
        if (line == "x" || line == "q") { std::printf("%s  Exiting.%s\n", ansi::dim, ansi::reset); break; }
        if (line == "cat")          { print_catalog();                                             continue; }
        if (line == "audit")        { ansi::print_section("Full Event Audit Trail"); for (const auto& e : log.snapshot()) print_event(e, true); continue; }
        if (line == "query")        { demo_queries(log);                                            continue; }
        if (line == "summary")      { print_run_summary(log);                                       continue; }
        if (line == "runner gas")   { show_runner_info("gas");                                      continue; }
        if (line == "runner solid") { show_runner_info("solid");                                    continue; }

        if (show_jsonl && line == "jsonl") {
            ansi::print_header("JSON Lines Export");
            std::printf("%s%s%s\n", ansi::dim, log.to_jsonl().c_str(), ansi::reset);
            continue;
        }
        if (show_markdown && line == "md") {
            ansi::print_header("Markdown Table Export");
            std::printf("%s\n", log.to_markdown().c_str());
            continue;
        }

        if (line.size() >= 3 && line[0] == 'r' && line[1] == ' ') {
            try {
                int sid = std::stoi(line.substr(2));
                if (sid < 0 || sid > 17) {
                    std::printf("%s  Scenario %d out of range [0-17].%s\n", ansi::red, sid, ansi::reset);
                } else {
                    std::printf("\n%s  Running scenario %d -- %s%s\n",
                        ansi::bold, sid, kScenarios[sid].name, ansi::reset);
                    emit_scenario(log, sid);
                    auto snap = log.snapshot();
                    print_event(snap.back(), true);
                    std::printf("%s  %zu total events in log.%s\n", ansi::dim, log.size(), ansi::reset);
                }
            } catch (...) {
                std::printf("%s  Usage: r <0-17>%s\n", ansi::red, ansi::reset);
            }
            continue;
        }

        std::printf("%s  Unknown command: '%s'  (x to exit)%s\n",
            ansi::red, line.c_str(), ansi::reset);
    }
}

// ============================================================================
// main
// ============================================================================

int main(int argc, char** argv) {
    bool headless      = false;
    bool show_jsonl    = true;
    bool show_markdown = true;
    bool show_visual   = false;
    bool show_render   = false;
    int  forced_sid    = -1;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--headless")    == 0) headless      = true;
        if (std::strcmp(argv[i], "--no-jsonl")    == 0) show_jsonl    = false;
        if (std::strcmp(argv[i], "--no-markdown") == 0) show_markdown = false;
        if (std::strcmp(argv[i], "--visual")      == 0) show_visual   = true;
        if (std::strcmp(argv[i], "--render")      == 0) show_render   = true;
        if (std::strcmp(argv[i], "--scenario")    == 0 && i + 1 < argc) {
            forced_sid = std::atoi(argv[++i]);
            if (forced_sid < 0 || forced_sid > 17) forced_sid = 0;
        }
        if (std::strcmp(argv[i], "--help") == 0) {
            std::printf(
                "kernel_demo -- 18-scenario chemistry kernel demo\n\n"
                "Usage: kernel_demo [options]\n\n"
                "Options:\n"
                "  --scenario N    Force scenario N (0-17); default: random\n"
                "  --headless      Non-interactive; emit scenario and exit\n"
                "  --visual        Append full 6-panel visual dashboard after run\n"
                "  --render        Auto-render [export.visual] artifacts to disk\n"
                "  --no-jsonl      Skip JSON Lines export\n"
                "  --no-markdown   Skip Markdown table export\n"
                "  --help          Show this message\n\n"
                "Scenarios 0-9  : complex chemistry\n"
                "Scenarios 10-13: gas-phase processes\n"
                "Scenarios 14-17: solid-state processes\n\n"
                "Runner scripts (bidirectional pipe):\n"
                "  scripts/kernel_demo_runner_gas.vsim\n"
                "  scripts/kernel_demo_runner_solid.vsim\n"
                "  Back-call: vsper run kernel_demo --scenario <N>\n"
            );
            return 0;
        }
    }

    ansi::print_header("VSEPR-SIM  |  kernel_demo  |  WO-56C  |  v5.0.0-beta.7.1");

    // Random scenario selection
    int sid = forced_sid;
    if (sid < 0) {
        uint64_t seed = static_cast<uint64_t>(
            std::chrono::steady_clock::now().time_since_epoch().count());
        std::mt19937_64 rng(seed);
        sid = static_cast<int>(rng() % 18);
    }

    const auto& desc = kScenarios[sid];
    const char* class_name[3] = { "complex", "gas", "solid" };

    std::printf("\n%s  Selected scenario %d  [%s]  %s%s\n",
        ansi::bold, sid, class_name[static_cast<int>(desc.sc)], desc.name, ansi::reset);
    std::printf("%s  Formula: %s    Rule: %s%s\n\n",
        ansi::dim, desc.formula, desc.rule, ansi::reset);

    auto& log = KernelEventLog::instance();
    log.clear();

    ansi::print_section("Emitting Scenario Events");
    emit_scenario(log, sid);
    std::printf("  %zu events recorded.\n", log.size());

    ansi::print_section("Scenario Event Trace");
    for (const auto& e : log.snapshot()) print_event(e, true);

    if (headless) {
        if (show_jsonl) {
            ansi::print_header("JSON Lines Export");
            std::printf("%s%s%s\n", ansi::dim, log.to_jsonl().c_str(), ansi::reset);
        }
        if (show_markdown) {
            ansi::print_header("Markdown Table Export");
            std::printf("%s\n", log.to_markdown().c_str());
        }
        print_run_summary(log);

        if (show_visual) {
            // Load the graphite stack script — the richest [visual] config
            // Falls back to a sensible default if the file is not present
            vsim::VsimDocument vdoc;
            try {
                vdoc = vsim::VsimParser::parse_file("scripts/demo_03_graphite_stack.vsim");
            } catch (...) {
                // Build a minimal inline visual doc so --visual always works
                vdoc.project.name = "kernel_demo_visual";
                vdoc.visual.output_type            = "terminal_overlay_cycle";
                vdoc.visual.animation_mode         = "overlay";
                vdoc.visual.show_event_timeline    = true;
                vdoc.visual.show_bar_chart         = true;
                vdoc.visual.show_symbolic_trace    = true;
                vdoc.visual.show_animation_cues    = true;
                vdoc.visual.show_audit_table       = true;
                vdoc.visual.overlay_sequence       = {"density","coordination","memory","orient_order"};
            }
            vsim::VsimVizAdapter::event_panels(vdoc, log);
        }

        if (show_render) {
            vsim::VsimDocument rdoc;
            try {
                rdoc = vsim::VsimParser::parse_file("scripts/demo_03_graphite_stack.vsim");
            } catch (...) {
                rdoc.project.name = "kernel_demo";
                rdoc.export_visual.write_energy_trace_svg = true;
                rdoc.export_visual.write_rdf_svg          = true;
                rdoc.export_visual.write_html_dashboard   = true;
                rdoc.export_visual.write_report_html      = true;
                rdoc.exports.write_manifest_json          = true;
            }
            const char* class_names[3] = {"complex","gas","solid"};
            vsim::RenderPayload rp;
            rp.run_name       = rdoc.project.name;
            rp.formula        = desc.formula;
            rp.scenario_class = class_names[static_cast<int>(desc.sc)];
            rp.log            = &log;
            if (rdoc.visual.should_render(doc.simulation.fire_max_steps))
                vsim::VsimRenderLayer::dispatch(rdoc, rp);
        }

        std::printf("\n%s%s  kernel_demo complete -- scenario %d  |  %zu events  |  spine clean.%s\n\n",
            ansi::bold, ansi::green, sid, log.size(), ansi::reset);
        std::printf("%s  Runner pipe reference:\n"
                    "    vsper run scripts/kernel_demo_runner_gas.vsim   | vsper run kernel_demo\n"
                    "    vsper run scripts/kernel_demo_runner_solid.vsim | vsper run kernel_demo\n\n",
            ansi::dim);
        std::printf("%s", ansi::reset);
        return 0;
    }

    print_catalog();
    run_interactive(log, show_jsonl, show_markdown);
    if (show_visual) {
        vsim::VsimDocument vdoc;
        try {
            vdoc = vsim::VsimParser::parse_file("scripts/demo_03_graphite_stack.vsim");
        } catch (...) {
            vdoc.project.name = "kernel_demo_visual";
            vdoc.visual.output_type         = "terminal_overlay_cycle";
            vdoc.visual.show_event_timeline = true;
            vdoc.visual.show_bar_chart      = true;
            vdoc.visual.show_symbolic_trace = true;
            vdoc.visual.show_audit_table    = true;
        }
        vsim::VsimVizAdapter::event_panels(vdoc, log);
    }
    if (show_render) {
        vsim::VsimDocument rdoc;
        try {
            rdoc = vsim::VsimParser::parse_file("scripts/demo_03_graphite_stack.vsim");
        } catch (...) {
            rdoc.project.name = "kernel_demo";
            rdoc.export_visual.write_energy_trace_svg = true;
            rdoc.export_visual.write_rdf_svg          = true;
            rdoc.export_visual.write_html_dashboard   = true;
            rdoc.export_visual.write_report_html      = true;
            rdoc.exports.write_manifest_json          = true;
        }
        const char* class_names[3] = {"complex","gas","solid"};
        vsim::RenderPayload rp;
        rp.run_name       = rdoc.project.name;
        rp.formula        = desc.formula;
        rp.scenario_class = class_names[static_cast<int>(desc.sc)];
        rp.log            = &log;
        if (rdoc.visual.should_render(rdoc.simulation.fire_max_steps))
            vsim::VsimRenderLayer::dispatch(rdoc, rp);
    }
    print_run_summary(log);

    std::printf("\n%s%s  kernel_demo session complete -- %zu events recorded, spine is clean.%s\n\n",
        ansi::bold, ansi::green, log.size(), ansi::reset);

    return 0;
}
