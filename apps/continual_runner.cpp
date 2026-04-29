/**
 * continual_runner.cpp — Continual Formation Engine Worker
 *
 * A lean, deterministic simulation worker that:
 *   1. Reads formulas from a queue file (one per line)
 *   2. Runs tiered simulation: SCREEN → MEDIUM → DEEP
 *   3. Writes structured JSON results per formula
 *   4. Appends to a master CSV ledger
 *   5. Checkpoints progress so it can be killed and resumed
 *
 * Usage:
 *   continual_runner --queue work_queue.txt --out results/ [--tier deep]
 *
 * Tiers:
 *   screen  — 100 FIRE steps,   fast reject/accept (< 0.5 s per formula)
 *   medium  — 1000 FIRE steps,  decent convergence  (< 5 s per formula)
 *   deep    — 5000 FIRE steps,  publication-quality  (< 30 s per formula)
 *
 * The orchestrator (Python) feeds the queue; this binary only consumes.
 * Anti-black-box: every result is a JSON with full provenance.
 */

#include "atomistic/core/state.hpp"
#include "atomistic/models/model.hpp"
#include "atomistic/integrators/fire.hpp"
#include "atomistic/compilers/xyz_compiler.hpp"
#include "io/xyz_format.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <cstdlib>
#include <algorithm>
#include <cmath>
#include <random>
#include <thread>
#include <cctype>

namespace fs = std::filesystem;
using namespace atomistic;

// ============================================================================
// Configuration
// ============================================================================

struct RunnerConfig {
    std::string queue_file  = "work_queue.txt";
    std::string output_dir  = "continual_results";
    std::string ledger_file = ""; // auto: output_dir/ledger.csv
    std::string tier        = "medium";
    int    screen_steps     = 100;
    int    medium_steps     = 1000;
    int    deep_steps       = 5000;
    int    override_steps   = -1;   // --steps N overrides tier step count
    double eps_force        = 1e-4;
    double eps_energy       = 1e-8;
    bool   verbose          = false;
    uint32_t base_seed      = 42;
    int    seeds_per_formula = 3;   // ensemble width
};

struct FormationResult {
    std::string formula;
    uint32_t    seed;
    std::string tier;
    int         steps_taken;
    int         max_steps;
    double      energy;          // kcal/mol
    double      energy_per_atom; // kcal/mol/atom
    double      rms_force;
    double      max_force;
    double      alpha_final;
    double      dt_final;
    int         num_atoms;
    bool        converged;
    std::string classification;  // stable, metastable, unstable, fragment, collapsed, timeout
    double      wall_time_ms;
    std::string timestamp;
    std::string xyz_file;
};

// ============================================================================
// Helpers
// ============================================================================

static std::string now_iso() {
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    std::ostringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
    return ss.str();
}

static std::string classify(const FormationResult& r) {
    if (r.steps_taken >= r.max_steps) return "timeout";
    if (std::isnan(r.energy) || std::isinf(r.energy)) return "collapsed";
    if (r.energy_per_atom > 1000.0) return "collapsed";
    if (r.rms_force < 1e-3 && r.energy_per_atom < 0.0) return "stable";
    if (r.rms_force < 1e-2 && r.energy_per_atom < 10.0) return "metastable";
    if (r.rms_force < 1.0)  return "unstable";
    if (r.num_atoms <= 1)   return "fragment";
    return "unstable";
}

static void write_result_json(const FormationResult& r, const std::string& path) {
    std::ofstream f(path);
    f << "{\n";
    f << "  \"formula\": \"" << r.formula << "\",\n";
    f << "  \"seed\": " << r.seed << ",\n";
    f << "  \"tier\": \"" << r.tier << "\",\n";
    f << "  \"num_atoms\": " << r.num_atoms << ",\n";
    f << "  \"steps_taken\": " << r.steps_taken << ",\n";
    f << "  \"max_steps\": " << r.max_steps << ",\n";
    f << std::fixed << std::setprecision(6);
    f << "  \"energy_kcal_mol\": " << r.energy << ",\n";
    f << "  \"energy_per_atom\": " << r.energy_per_atom << ",\n";
    f << "  \"rms_force\": " << r.rms_force << ",\n";
    f << "  \"max_force\": " << r.max_force << ",\n";
    f << "  \"alpha_final\": " << r.alpha_final << ",\n";
    f << "  \"dt_final\": " << r.dt_final << ",\n";
    f << "  \"converged\": " << (r.converged ? "true" : "false") << ",\n";
    f << "  \"classification\": \"" << r.classification << "\",\n";
    f << "  \"wall_time_ms\": " << r.wall_time_ms << ",\n";
    f << "  \"timestamp\": \"" << r.timestamp << "\",\n";
    f << "  \"xyz_file\": \"" << r.xyz_file << "\"\n";
    f << "}\n";
}

static void append_ledger_row(const std::string& ledger, const FormationResult& r) {
    bool needs_header = !fs::exists(ledger) || fs::file_size(ledger) == 0;
    std::ofstream f(ledger, std::ios::app);
    if (needs_header) {
        f << "timestamp,formula,seed,tier,num_atoms,steps,max_steps,"
          << "energy,energy_per_atom,rms_force,classification,wall_ms,converged\n";
    }
    f << r.timestamp << ","
      << r.formula << ","
      << r.seed << ","
      << r.tier << ","
      << r.num_atoms << ","
      << r.steps_taken << ","
      << r.max_steps << ","
      << std::fixed << std::setprecision(4) << r.energy << ","
      << r.energy_per_atom << ","
      << r.rms_force << ","
      << r.classification << ","
      << r.wall_time_ms << ","
      << (r.converged ? 1 : 0) << "\n";
}

// ============================================================================
// Formula → State builder (self-contained, no external builder dependency)
// ============================================================================

// Minimal atomic mass table (Z → mass in amu)
static const std::map<int, double> ATOMIC_MASS = {
    {1,1.008},{2,4.003},{3,6.941},{4,9.012},{5,10.81},{6,12.011},{7,14.007},
    {8,15.999},{9,18.998},{10,20.18},{11,22.99},{12,24.305},{13,26.982},
    {14,28.086},{15,30.974},{16,32.065},{17,35.453},{18,39.948},{19,39.098},
    {20,40.078},{22,47.867},{24,51.996},{25,54.938},{26,55.845},{27,58.933},
    {28,58.693},{29,63.546},{30,65.38},{35,79.904},{53,126.904},{79,196.967}
};

// Minimal symbol → Z table
static const std::map<std::string, int> SYMBOL_TO_Z = {
    {"H",1},{"He",2},{"Li",3},{"Be",4},{"B",5},{"C",6},{"N",7},{"O",8},
    {"F",9},{"Ne",10},{"Na",11},{"Mg",12},{"Al",13},{"Si",14},{"P",15},
    {"S",16},{"Cl",17},{"Ar",18},{"K",19},{"Ca",20},{"Ti",22},{"Cr",24},
    {"Mn",25},{"Fe",26},{"Co",27},{"Ni",28},{"Cu",29},{"Zn",30},{"Br",35},
    {"I",53},{"Au",79}
};

// Z → symbol (reverse map)
static std::string z_to_symbol(int Z) {
    for (auto& [sym, z] : SYMBOL_TO_Z) {
        if (z == Z) return sym;
    }
    return "X";
}

// Parse chemical formula string → vector of (Z, count)
static std::vector<std::pair<int,int>> parse_formula(const std::string& formula) {
    std::vector<std::pair<int,int>> composition;
    size_t i = 0;
    while (i < formula.size()) {
        if (std::isspace(static_cast<unsigned char>(formula[i]))) { ++i; continue; }
        if (!std::isupper(static_cast<unsigned char>(formula[i]))) { ++i; continue; }

        std::string sym;
        sym += formula[i++];
        if (i < formula.size() && std::islower(static_cast<unsigned char>(formula[i]))) {
            sym += formula[i++];
        }

        int count = 0;
        while (i < formula.size() && std::isdigit(static_cast<unsigned char>(formula[i]))) {
            count = count * 10 + (formula[i++] - '0');
        }
        if (count == 0) count = 1;

        auto it = SYMBOL_TO_Z.find(sym);
        if (it != SYMBOL_TO_Z.end()) {
            composition.push_back({it->second, count});
        }
    }
    return composition;
}

// Build an atomistic State from a formula + random seed
static std::pair<State, std::vector<std::string>> state_from_formula(
    const std::string& formula, uint32_t seed
) {
    auto composition = parse_formula(formula);

    int total = 0;
    for (auto& [Z, cnt] : composition) total += cnt;

    State s;
    s.N = (uint32_t)total;
    s.X.resize(s.N);
    s.V.resize(s.N, Vec3{0,0,0});
    s.Q.resize(s.N, 0.0);
    s.M.resize(s.N);
    s.type.resize(s.N);
    s.F.resize(s.N, Vec3{0,0,0});

    std::vector<std::string> element_names;
    element_names.reserve(s.N);

    // Place atoms on a sphere with jitter for initial geometry
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> jitter(-0.2, 0.2);

    uint32_t idx = 0;
    for (auto& [Z, cnt] : composition) {
        double mass = 1.0;
        auto mit = ATOMIC_MASS.find(Z);
        if (mit != ATOMIC_MASS.end()) mass = mit->second;

        std::string sym = z_to_symbol(Z);

        for (int j = 0; j < cnt; ++j) {
            double angle = 2.0 * M_PI * idx / std::max(total, 6);
            double layer = (idx % 3 == 0) ? 0.0 : ((idx % 3 == 1) ? 1.2 : -1.2);
            double radius = 2.0 + 0.3 * (Z > 10 ? 1.5 : 1.0);

            s.X[idx] = Vec3{
                radius * std::cos(angle) + jitter(rng),
                radius * std::sin(angle) + jitter(rng),
                layer + jitter(rng)
            };
            s.M[idx] = mass;
            s.type[idx] = (uint32_t)Z;
            element_names.push_back(sym);
            ++idx;
        }
    }

    return {s, element_names};
}

// ============================================================================
// Core: run one formation
// ============================================================================

static FormationResult run_formation(
    const std::string& formula,
    uint32_t seed,
    int max_steps,
    double epsF,
    double epsU,
    const std::string& tier_name,
    const std::string& out_dir
) {
    FormationResult res;
    res.formula   = formula;
    res.seed      = seed;
    res.tier      = tier_name;
    res.max_steps = max_steps;
    res.timestamp = now_iso();

    auto t0 = std::chrono::high_resolution_clock::now();

    try {
        // Build state from formula
        auto [s, elem_names] = state_from_formula(formula, seed);
        res.num_atoms = (int)s.N;

        if (s.N == 0) {
            res.classification = "fragment";
            res.wall_time_ms = 0;
            return res;
        }

        // Configure model (LJ + Coulomb via factory)
        ModelParams mp;
        mp.rc = 10.0;
        auto model = create_lj_coulomb_model();

        // Configure FIRE
        FIREParams fp;
        fp.max_steps = max_steps;
        fp.epsF      = epsF;
        fp.epsU      = epsU;

        FIRE fire(*model, mp);
        auto stats = fire.minimize(s, fp);

        res.steps_taken     = stats.step;
        res.energy          = stats.U;
        res.energy_per_atom = stats.U / (double)s.N;
        res.rms_force       = stats.Frms;
        res.max_force       = stats.Frms; // approximate
        res.alpha_final     = stats.alpha;
        res.dt_final        = stats.dt;
        res.converged       = (stats.step < max_steps);
        res.classification  = classify(res);

        // Write XYZ of final state
        std::string xyz_name = formula + "_s" + std::to_string(seed) + ".xyz";
        std::replace(xyz_name.begin(), xyz_name.end(), '(', '_');
        std::replace(xyz_name.begin(), xyz_name.end(), ')', '_');
        res.xyz_file = (fs::path(out_dir) / "structures" / xyz_name).string();
        fs::create_directories(fs::path(res.xyz_file).parent_path());

        // Write XYZ using the compiler
        compilers::save_xyza(res.xyz_file, s, elem_names);

    } catch (const std::exception& e) {
        res.classification = "collapsed";
        res.converged = false;
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    res.wall_time_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    return res;
}

// ============================================================================
// Queue consumption: read one formula, remove it, return it
// ============================================================================

static std::string pop_formula(const std::string& queue_file) {
    std::ifstream in(queue_file);
    if (!in.good()) return "";

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(in, line)) {
        // Trim whitespace
        auto start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        auto end = line.find_last_not_of(" \t\r\n");
        std::string trimmed = line.substr(start, end - start + 1);
        if (!trimmed.empty() && trimmed[0] != '#') {
            lines.push_back(trimmed);
        }
    }
    in.close();

    if (lines.empty()) return "";

    std::string formula = lines[0];

    // Rewrite queue without the consumed line
    std::ofstream out(queue_file, std::ios::trunc);
    for (size_t i = 1; i < lines.size(); i++) {
        out << lines[i] << "\n";
    }

    return formula;
}

// ============================================================================
// Main
// ============================================================================

void print_help() {
    std::cout << R"(
continual_runner — Continual Formation Engine Worker

USAGE:
  continual_runner --queue <file> --out <dir> [options]

OPTIONS:
  --queue <file>       Work queue file (one formula per line)
  --out <dir>          Output directory for results
  --tier <level>       screen | medium | deep (default: medium)
  --steps <N>          Override step count directly (ignores tier steps)
  --seeds <N>          Seeds per formula (default: 3)
  --base-seed <N>      Starting seed (default: 42)
  --verbose            Print progress
  --help               Show this help

TIERS:
  screen       100 FIRE steps   ~0.5 s/formula   Quick viability check
  medium      1000 FIRE steps   ~5 s/formula     Standard formation
  deep        5000 FIRE steps   ~30 s/formula    Publication quality

--steps overrides the tier step count.  Examples:
  --steps 50      fast/exploratory sweep
  --steps 10000   high-N deep refinement

The worker consumes formulas from the queue file until it is empty,
then exits. The Python orchestrator refills the queue continuously.
)";
}

int main(int argc, char** argv) {
    RunnerConfig cfg;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") { print_help(); return 0; }
        else if (arg == "--queue"     && i+1 < argc) cfg.queue_file       = argv[++i];
        else if (arg == "--out"       && i+1 < argc) cfg.output_dir       = argv[++i];
        else if (arg == "--tier"      && i+1 < argc) cfg.tier             = argv[++i];
        else if (arg == "--steps"     && i+1 < argc) cfg.override_steps   = std::stoi(argv[++i]);
        else if (arg == "--seeds"     && i+1 < argc) cfg.seeds_per_formula = std::stoi(argv[++i]);
        else if (arg == "--base-seed" && i+1 < argc) cfg.base_seed        = (uint32_t)std::stoi(argv[++i]);
        else if (arg == "--verbose")                  cfg.verbose          = true;
    }

    if (cfg.ledger_file.empty()) {
        cfg.ledger_file = (fs::path(cfg.output_dir) / "ledger.csv").string();
    }

    fs::create_directories(cfg.output_dir);

    int max_steps = cfg.medium_steps;
    if (cfg.tier == "screen") max_steps = cfg.screen_steps;
    else if (cfg.tier == "deep") max_steps = cfg.deep_steps;
    if (cfg.override_steps > 0) max_steps = cfg.override_steps;  // --steps wins

    std::cout << "╔═══════════════════════════════════════════════╗\n";
    std::cout << "║  Continual Formation Engine — Worker          ║\n";
    std::cout << "╠═══════════════════════════════════════════════╣\n";
    std::cout << "║  Queue:  " << cfg.queue_file << "\n";
    std::cout << "║  Output: " << cfg.output_dir << "\n";
    std::cout << "║  Tier:   " << cfg.tier << " (" << max_steps << " steps)\n";
    std::cout << "║  Seeds:  " << cfg.seeds_per_formula << " per formula\n";
    std::cout << "╚═══════════════════════════════════════════════╝\n\n";

    int total_formations = 0;
    int total_stable = 0;
    int total_converged = 0;
    auto session_start = std::chrono::steady_clock::now();

    // Main consumption loop — runs until queue is empty
    while (true) {
        std::string formula = pop_formula(cfg.queue_file);
        if (formula.empty()) {
            // Check for stop signal
            if (fs::exists(fs::path(cfg.output_dir) / "STOP")) {
                std::cout << "\n[STOP signal received]\n";
                break;
            }
            // Queue empty — wait and retry (orchestrator may refill)
            std::this_thread::sleep_for(std::chrono::seconds(2));
            formula = pop_formula(cfg.queue_file);
            if (formula.empty()) break; // truly done
        }

        // Run ensemble for this formula
        for (int s = 0; s < cfg.seeds_per_formula; s++) {
            uint32_t seed = cfg.base_seed + (uint32_t)(total_formations * 7 + s * 13);

            auto res = run_formation(
                formula, seed, max_steps,
                cfg.eps_force, cfg.eps_energy,
                cfg.tier, cfg.output_dir
            );

            // Write individual JSON
            std::string json_name = formula + "_s" + std::to_string(seed) + ".json";
            std::replace(json_name.begin(), json_name.end(), '(', '_');
            std::replace(json_name.begin(), json_name.end(), ')', '_');
            std::string json_path = (fs::path(cfg.output_dir) / "results" / json_name).string();
            fs::create_directories(fs::path(json_path).parent_path());
            write_result_json(res, json_path);

            // Append to ledger
            append_ledger_row(cfg.ledger_file, res);

            total_formations++;
            if (res.converged) total_converged++;
            if (res.classification == "stable") total_stable++;

            if (cfg.verbose) {
                std::cout << "[" << total_formations << "] "
                          << formula << " s=" << seed
                          << " → " << res.classification
                          << " E/N=" << std::fixed << std::setprecision(2)
                          << res.energy_per_atom
                          << " t=" << std::setprecision(1) << res.wall_time_ms << "ms\n";
            } else if (total_formations % 10 == 0) {
                auto elapsed = std::chrono::duration<double>(
                    std::chrono::steady_clock::now() - session_start).count();
                double rate = total_formations / elapsed;
                std::cout << "\r  Formations: " << total_formations
                          << " | Stable: " << total_stable
                          << " | Conv: " << total_converged
                          << " | Rate: " << std::fixed << std::setprecision(1)
                          << rate << "/s" << std::flush;
            }
        }
    }

    auto elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - session_start).count();

    std::cout << "\n\n═══ SESSION COMPLETE ═══\n";
    std::cout << "  Total formations: " << total_formations << "\n";
    std::cout << "  Stable:           " << total_stable << "\n";
    std::cout << "  Converged:        " << total_converged << "\n";
    std::cout << "  Wall time:        " << std::fixed << std::setprecision(1)
              << elapsed << " s\n";
    std::cout << "  Rate:             " << std::setprecision(2)
              << total_formations / std::max(elapsed, 0.01) << " formations/s\n";
    std::cout << "  Ledger:           " << cfg.ledger_file << "\n";

    return 0;
}
