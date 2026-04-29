/**
 * sim_1000.cpp — 1000-Iteration Seeded Random Atomistic Simulation Runner
 *
 * Runs 1000 atomistic simulations. Each iteration randomly selects one of
 * the 50 pre-built chemical arrangements using a seeded RNG. Every simulation
 * is assigned a deterministic seed hash that uniquely identifies it and ties
 * together all its outputs.
 *
 * Per-simulation outputs (all keyed to seed hash):
 *   - Reaction equation string
 *   - Full numerical intermediates: every step's energy, forces, η, ρ, C, P₂,
 *     target f, kernel modulations (g_steric, g_elec, g_disp), dt, role counts
 *   - Sample size: N_beads (bead count = sample size)
 *   - Sample mass:  total_mass_amu = Σ bead.mass
 *
 * Outputs:
 *   - sim_1000_results.csv          — one row per simulation, all metadata
 *   - intermediates/<HASH>.csv      — one row per step, full numerical trace
 *   - sim_1000_report.md            — summary statistics and analysis
 *
 * Seed hash:
 *   seed_hash = master_seed XOR (arr_id * 2654435761) XOR (iter * 40503)
 *   Formatted as a 16-digit zero-padded hexadecimal string.
 *
 * Usage:
 *   sim-1000 [master_seed]          default master seed = 42
 *
 * Anti-black-box: every parameter, equation, intermediate, and metric is
 * explicitly recorded and traceable to its seed hash.
 * Deterministic: identical master_seed → bit-identical results.
 *
 * Reference: copilot-instructions.md §2, §5, §7
 */

#include "coarse_grain/chemistry/reaction_engine.hpp"
#include "coarse_grain/chemistry/reaction_library.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

using namespace coarse_grain;
using namespace coarse_grain::chemistry;

// ============================================================================
// Constants
// ============================================================================

static constexpr uint32_t N_ITERATIONS  = 1000;
static constexpr uint64_t DEFAULT_SEED  = 42ULL;
static constexpr uint64_t HASH_PRIME_A  = 2654435761ULL;  // Knuth multiplicative
static constexpr uint64_t HASH_PRIME_B  = 40503ULL;

// ============================================================================
// Seed Hash Utilities
// ============================================================================

static uint64_t make_seed_hash(uint64_t master, uint32_t arr_id, uint32_t iter)
{
    return master
         ^ (static_cast<uint64_t>(arr_id) * HASH_PRIME_A)
         ^ (static_cast<uint64_t>(iter)   * HASH_PRIME_B);
}

static std::string fmt_hash(uint64_t h)
{
    std::ostringstream s;
    s << std::hex << std::uppercase
      << std::setw(16) << std::setfill('0') << h;
    return s.str();
}

// ============================================================================
// Arrangement Descriptor (identical to sim_101x50 for consistency)
// ============================================================================

struct ArrangementDescriptor {
    uint32_t    id{};
    std::string label;
    std::string base_reaction;
    double      temperature_K{298.15};
    double      tau{100.0};
    double      gamma_steric{0.2};
    double      gamma_elec{-0.15};
    double      gamma_disp{0.45};
    uint32_t    system_scale{1};
};

// ============================================================================
// Per-Step Intermediate Record
// ============================================================================

struct IntermediateRow {
    uint64_t step_index{};
    // SEED layer
    double   total_energy{};        // kcal/mol
    double   kinetic_energy{};      // kcal/mol
    double   potential_energy{};    // kcal/mol
    double   rms_force{};           // kcal/(mol·Å)
    double   max_force{};           // kcal/(mol·Å)
    double   dt_current{};          // fs (FIRE-adjusted)
    // BEAD layer averages
    double   avg_rho{};             // Mean local density
    double   avg_C{};               // Mean coordination number
    double   avg_P2{};              // Mean orientational order P₂
    double   avg_eta{};             // Mean slow state η
    double   avg_target_f{};        // Mean target function f(ρ̂, P̂₂)
    double   max_delta_eta{};       // Max |Δη| this step
    // Kernel modulation averages
    double   avg_g_steric{};
    double   avg_g_elec{};
    double   avg_g_disp{};
    // Role distribution Σ_i
    uint32_t n_inert{};
    uint32_t n_ionic{};
    uint32_t n_covalent{};
    uint32_t n_metallic{};
    uint32_t n_mixed{};
    // Convergence flags
    bool     seed_converged{};
    bool     bead_converged{};
    bool     steady_state{};
};

// ============================================================================
// Main Simulation Record
// ============================================================================

struct SimRecord1000 {
    uint32_t    sim_id{};
    std::string seed_hash;
    uint64_t    master_seed{};
    uint32_t    arrangement_id{};
    std::string arrangement_label;
    std::string base_reaction;
    std::string equation;           // Reaction equation string
    double      temperature_K{};
    double      tau{};
    double      gamma_steric{};
    double      gamma_elec{};
    double      gamma_disp{};
    uint32_t    system_scale{};
    // Sample size and mass
    uint32_t    N_beads{};          // Sample size (bead count)
    double      total_mass_amu{};   // Sample mass Σ bead.mass (amu)
    // Simulation outcome
    uint64_t    max_steps{};
    double      dt_initial{};
    bool        converged{};
    uint64_t    steps_taken{};
    double      final_energy{};     // kcal/mol
    double      final_rms_force{};  // kcal/(mol·Å)
    double      final_avg_eta{};
    double      elapsed_ms{};
    // Intermediates (one entry per step)
    std::vector<IntermediateRow> intermediates;
};

// ============================================================================
// Build 50 Arrangements
// ============================================================================

static std::vector<ArrangementDescriptor> build_arrangements()
{
    std::vector<ArrangementDescriptor> arr;
    arr.reserve(50);

    struct BaseRxn {
        std::string label;
        double default_tau;
        double default_g_steric;
        double default_g_elec;
        double default_g_disp;
    };

    std::vector<BaseRxn> bases = {
        {"benzene_nitration",     60.0,  0.20, -0.15,  0.45},
        {"thorium_oxalate",       40.0,  0.25, -0.20,  0.30},
        {"copper_decomposition",  30.0,  0.30, -0.10,  0.55},
    };

    double temps[]           = {250.0, 298.15, 500.0};
    const char* temp_tags[]  = {"cold", "std", "hot"};
    double coupling_scale[]  = {0.5, 1.0, 2.0};
    const char* coupling_tags[] = {"weak", "mod", "strong"};
    uint32_t scales[]        = {1, 2};
    const char* scale_tags[] = {"small", "large"};

    uint32_t id = 0;
    for (auto& base : bases) {
        for (int ti = 0; ti < 3; ++ti) {
            for (int ci = 0; ci < 3; ++ci) {
                for (int si = 0; si < 2; ++si) {
                    if (id >= 50) break;

                    ArrangementDescriptor ad;
                    ad.id           = id;
                    ad.base_reaction   = base.label;
                    ad.temperature_K   = temps[ti];
                    ad.tau             = base.default_tau    * coupling_scale[ci];
                    ad.gamma_steric    = base.default_g_steric * coupling_scale[ci];
                    ad.gamma_elec      = base.default_g_elec   * coupling_scale[ci];
                    ad.gamma_disp      = base.default_g_disp   * coupling_scale[ci];
                    ad.system_scale    = scales[si];

                    std::ostringstream lbl;
                    lbl << base.label << "_" << temp_tags[ti]
                        << "_" << coupling_tags[ci]
                        << "_" << scale_tags[si];
                    ad.label = lbl.str();
                    arr.push_back(ad);
                    ++id;
                }
                if (id >= 50) break;
            }
            if (id >= 50) break;
        }
    }

    while (arr.size() < 50) {
        auto& last = arr.back();
        ArrangementDescriptor ad = last;
        ad.id           = static_cast<uint32_t>(arr.size());
        ad.temperature_K += 25.0;
        ad.label = last.label + "_T" + std::to_string(static_cast<int>(ad.temperature_K));
        arr.push_back(ad);
    }

    return arr;
}

// ============================================================================
// Resolve Base Reaction
// ============================================================================

static ChemicalReaction get_base_reaction(const std::string& name)
{
    if (name == "benzene_nitration")    return library::build_benzene_nitration();
    if (name == "thorium_oxalate")      return library::build_thorium_oxalate_precipitation();
    if (name == "copper_decomposition") return library::build_copper_nitrate_decomposition();
    return library::build_benzene_nitration();
}

// ============================================================================
// Build SeedBeadParams for an arrangement
// ============================================================================

static SeedBeadParams build_params_for(const ArrangementDescriptor& ad,
                                        uint64_t max_steps, double dt_init)
{
    auto rxn = get_base_reaction(ad.base_reaction);
    SeedBeadParams params = ReactionEngine::build_reaction_params(rxn);
    params.dt_initial                = dt_init;
    params.max_steps                 = max_steps;
    params.env_params.tau            = ad.tau;
    params.env_params.gamma_steric   = ad.gamma_steric;
    params.env_params.gamma_elec     = ad.gamma_elec;
    params.env_params.gamma_disp     = ad.gamma_disp;
    params.snapshot_interval         = 0;   // No snapshot data; capture via record
    params.record_positions          = false;
    return params;
}

// ============================================================================
// Run One Simulation — returns full record including all intermediates
// ============================================================================

static SimRecord1000 run_single_sim(
    uint32_t    sim_id,
    uint64_t    master_seed,
    uint32_t    iter,
    const ArrangementDescriptor& ad,
    uint64_t    max_steps,
    double      dt_init)
{
    SimRecord1000 rec;
    rec.sim_id          = sim_id;
    rec.master_seed     = master_seed;
    rec.arrangement_id  = ad.id;
    rec.arrangement_label = ad.label;
    rec.base_reaction   = ad.base_reaction;
    rec.temperature_K   = ad.temperature_K;
    rec.tau             = ad.tau;
    rec.gamma_steric    = ad.gamma_steric;
    rec.gamma_elec      = ad.gamma_elec;
    rec.gamma_disp      = ad.gamma_disp;
    rec.system_scale    = ad.system_scale;
    rec.max_steps       = max_steps;
    rec.dt_initial      = dt_init;
    rec.seed_hash       = fmt_hash(make_seed_hash(master_seed, ad.id, iter));

    // Resolve reaction and extract equation
    auto rxn = get_base_reaction(ad.base_reaction);
    rxn.thermal.temperature_K = ad.temperature_K;
    rec.equation = rxn.equation;

    // Map reaction to bead system
    BeadSystem system = ReactionEngine::map_reaction_to_beads(rxn);

    // Scale system
    if (ad.system_scale > 1) {
        auto base_beads = system.beads;
        for (uint32_t s = 1; s < ad.system_scale; ++s) {
            double offset = s * 12.0;
            for (auto b : base_beads) {
                b.position.x += offset;
                system.beads.push_back(b);
            }
        }
        system.source_atom_count = static_cast<uint32_t>(system.beads.size());
    }

    // Sample size and mass
    rec.N_beads = static_cast<uint32_t>(system.beads.size());
    rec.total_mass_amu = 0.0;
    for (const auto& b : system.beads)
        rec.total_mass_amu += b.mass;

    // Build parameters
    SeedBeadParams params = build_params_for(ad, max_steps, dt_init);

    // Initialise stepper state
    size_t N = system.beads.size();
    std::vector<EnvironmentState>   env_states(N);
    std::vector<atomistic::Vec3>    velocities(N);
    std::vector<atomistic::Vec3>    forces(N);
    FIREState fire;
    fire.dt    = params.dt_initial;
    fire.alpha = params.fire_alpha_start;

    SeedBeadStepper::init(system, env_states, params);

    rec.intermediates.reserve(std::min(max_steps, uint64_t(2000)));

    auto t0 = std::chrono::high_resolution_clock::now();

    for (uint64_t s = 0; s < params.max_steps; ++s) {
        auto sr = SeedBeadStepper::step(
            system, env_states, velocities, forces, fire, params, s);

        // Capture full numerical intermediates for every step
        IntermediateRow row;
        row.step_index       = sr.step_index;
        row.total_energy     = sr.total_energy;
        row.kinetic_energy   = sr.kinetic_energy;
        row.potential_energy = sr.potential_energy;
        row.rms_force        = sr.rms_force;
        row.max_force        = sr.max_force;
        row.dt_current       = sr.dt_current;
        row.avg_rho          = sr.avg_rho;
        row.avg_C            = sr.avg_C;
        row.avg_P2           = sr.avg_P2;
        row.avg_eta          = sr.avg_eta;
        row.avg_target_f     = sr.avg_target_f;
        row.max_delta_eta    = sr.max_delta_eta;
        row.avg_g_steric     = sr.avg_g_steric;
        row.avg_g_elec       = sr.avg_g_elec;
        row.avg_g_disp       = sr.avg_g_disp;
        row.n_inert          = sr.n_inert;
        row.n_ionic          = sr.n_ionic;
        row.n_covalent       = sr.n_covalent;
        row.n_metallic       = sr.n_metallic;
        row.n_mixed          = sr.n_mixed;
        row.seed_converged   = sr.seed_converged;
        row.bead_converged   = sr.bead_converged;
        row.steady_state     = sr.steady_state;
        rec.intermediates.push_back(row);

        if (sr.steady_state) {
            rec.converged      = true;
            rec.steps_taken    = s + 1;
            rec.final_energy   = sr.total_energy;
            rec.final_rms_force = sr.rms_force;
            rec.final_avg_eta  = sr.avg_eta;
            break;
        }

        rec.steps_taken     = s + 1;
        rec.final_energy    = sr.total_energy;
        rec.final_rms_force = sr.rms_force;
        rec.final_avg_eta   = sr.avg_eta;
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    rec.elapsed_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    return rec;
}

// ============================================================================
// Write Intermediates CSV — one file per simulation, keyed to seed hash
// ============================================================================

static void write_intermediates(const std::string& dir,
                                 const SimRecord1000& rec)
{
    std::string path = dir + "/" + rec.seed_hash + ".csv";
    std::ofstream out(path);
    if (!out.is_open()) {
        std::cerr << "  WARNING: Cannot write intermediates " << path << "\n";
        return;
    }

    // Header — reaction equation and sample metadata as comment lines
    out << "# sim_id: "         << rec.sim_id          << "\n";
    out << "# seed_hash: "      << rec.seed_hash        << "\n";
    out << "# master_seed: "    << rec.master_seed      << "\n";
    out << "# arrangement: "    << rec.arrangement_label << "\n";
    out << "# base_reaction: "  << rec.base_reaction    << "\n";
    out << "# equation: "       << rec.equation         << "\n";
    out << "# temperature_K: "  << rec.temperature_K    << "\n";
    out << "# tau: "            << rec.tau              << "\n";
    out << "# gamma_steric: "   << rec.gamma_steric     << "\n";
    out << "# gamma_elec: "     << rec.gamma_elec       << "\n";
    out << "# gamma_disp: "     << rec.gamma_disp       << "\n";
    out << "# N_beads: "        << rec.N_beads          << "\n";
    out << "# total_mass_amu: " << rec.total_mass_amu   << "\n";
    out << "#\n";

    // Column header
    out << "step_index"
        << ",total_energy,kinetic_energy,potential_energy"
        << ",rms_force,max_force,dt_current"
        << ",avg_rho,avg_C,avg_P2,avg_eta,avg_target_f,max_delta_eta"
        << ",avg_g_steric,avg_g_elec,avg_g_disp"
        << ",n_inert,n_ionic,n_covalent,n_metallic,n_mixed"
        << ",seed_converged,bead_converged,steady_state"
        << "\n";

    out << std::fixed;
    for (const auto& r : rec.intermediates) {
        out << r.step_index
            << "," << std::setprecision(8) << r.total_energy
            << "," << std::setprecision(8) << r.kinetic_energy
            << "," << std::setprecision(8) << r.potential_energy
            << "," << std::setprecision(8) << r.rms_force
            << "," << std::setprecision(8) << r.max_force
            << "," << std::setprecision(6) << r.dt_current
            << "," << std::setprecision(8) << r.avg_rho
            << "," << std::setprecision(8) << r.avg_C
            << "," << std::setprecision(8) << r.avg_P2
            << "," << std::setprecision(8) << r.avg_eta
            << "," << std::setprecision(8) << r.avg_target_f
            << "," << std::setprecision(8) << r.max_delta_eta
            << "," << std::setprecision(8) << r.avg_g_steric
            << "," << std::setprecision(8) << r.avg_g_elec
            << "," << std::setprecision(8) << r.avg_g_disp
            << "," << r.n_inert
            << "," << r.n_ionic
            << "," << r.n_covalent
            << "," << r.n_metallic
            << "," << r.n_mixed
            << "," << (r.seed_converged ? 1 : 0)
            << "," << (r.bead_converged ? 1 : 0)
            << "," << (r.steady_state   ? 1 : 0)
            << "\n";
    }
}

// ============================================================================
// Write Main Results CSV
// ============================================================================

static void write_results_csv(const std::string& path,
                               const std::vector<SimRecord1000>& records)
{
    std::ofstream out(path);
    if (!out.is_open()) {
        std::cerr << "ERROR: Cannot write " << path << "\n";
        return;
    }

    out << "sim_id,seed_hash,master_seed,arrangement_id,arrangement_label,"
        << "base_reaction,equation,temperature_K,tau,gamma_steric,gamma_elec,gamma_disp,"
        << "system_scale,N_beads,total_mass_amu,"
        << "max_steps,dt_initial,converged,steps_taken,"
        << "final_energy,final_rms_force,final_avg_eta,elapsed_ms,"
        << "intermediates_steps\n";

    out << std::fixed;
    for (const auto& r : records) {
        out << r.sim_id                     << ","
            << r.seed_hash                  << ","
            << r.master_seed                << ","
            << r.arrangement_id             << ","
            << r.arrangement_label          << ","
            << r.base_reaction              << ","
            << "\"" << r.equation << "\""   << ","
            << std::setprecision(2)  << r.temperature_K   << ","
            << std::setprecision(4)  << r.tau              << ","
            << std::setprecision(4)  << r.gamma_steric     << ","
            << std::setprecision(4)  << r.gamma_elec       << ","
            << std::setprecision(4)  << r.gamma_disp       << ","
            << r.system_scale               << ","
            << r.N_beads                    << ","
            << std::setprecision(4)  << r.total_mass_amu   << ","
            << r.max_steps                  << ","
            << std::setprecision(2)  << r.dt_initial       << ","
            << (r.converged ? "true" : "false") << ","
            << r.steps_taken                << ","
            << std::setprecision(8)  << r.final_energy     << ","
            << std::setprecision(8)  << r.final_rms_force  << ","
            << std::setprecision(8)  << r.final_avg_eta    << ","
            << std::setprecision(1)  << r.elapsed_ms       << ","
            << r.intermediates.size()       << "\n";
    }

    std::cout << "  Written: " << path
              << " (" << records.size() << " rows)\n";
}

// ============================================================================
// Write Markdown Summary Report
// ============================================================================

static void write_report(const std::string& path,
                          uint64_t master_seed,
                          const std::vector<ArrangementDescriptor>& arrangements,
                          const std::vector<SimRecord1000>& records)
{
    std::ofstream out(path);
    if (!out.is_open()) {
        std::cerr << "ERROR: Cannot write " << path << "\n";
        return;
    }

    // Aggregate statistics
    uint32_t n_converged    = 0;
    double   total_time_ms  = 0.0;
    double   best_energy    = 1e30;
    uint32_t best_sim       = 0;
    double   total_mass_sum = 0.0;
    uint64_t total_steps    = 0;
    uint64_t total_inter    = 0;
    double   min_mass = 1e30, max_mass = 0.0;
    uint32_t min_N = UINT32_MAX, max_N = 0;

    for (const auto& r : records) {
        if (r.converged) ++n_converged;
        total_time_ms  += r.elapsed_ms;
        total_steps    += r.steps_taken;
        total_inter    += static_cast<uint64_t>(r.intermediates.size());
        total_mass_sum += r.total_mass_amu;
        if (r.final_energy < best_energy) {
            best_energy = r.final_energy;
            best_sim    = r.sim_id;
        }
        if (r.total_mass_amu < min_mass) min_mass = r.total_mass_amu;
        if (r.total_mass_amu > max_mass) max_mass = r.total_mass_amu;
        if (r.N_beads < min_N) min_N = r.N_beads;
        if (r.N_beads > max_N) max_N = r.N_beads;
    }
    double avg_mass = records.empty() ? 0.0 : total_mass_sum / records.size();

    out << "# VSEPR-SIM: 1000-Iteration Seeded Random Atomistic Simulation\n\n";
    out << "**Generated by VSEPR-SIM v2.9.2 Deterministic Atomistic Platform**\n\n";
    out << "---\n\n";

    // --- Section 1: Run Configuration ---
    out << "## 1. Run Configuration\n\n";
    out << "| Parameter | Value |\n";
    out << "|-----------|-------|\n";
    out << "| Master seed | `" << master_seed << "` |\n";
    out << "| Total iterations | " << N_ITERATIONS << " |\n";
    out << "| Chemical arrangements | 50 |\n";
    out << "| Selection method | Seeded uniform random (mt19937\\_64) |\n";
    out << "| Seed hash scheme | `master_seed XOR (arr_id × 2654435761) XOR (iter × 40503)` |\n";
    out << "| Intermediates captured | All steps (energy, forces, η, ρ, C, P₂, g channels, dt) |\n\n";

    // --- Section 2: Summary Statistics ---
    out << "## 2. Summary Statistics\n\n";
    out << "| Metric | Value |\n";
    out << "|--------|-------|\n";
    out << "| Simulations run | " << records.size() << " |\n";
    out << "| Converged | " << n_converged << " / " << records.size() << " |\n";
    out << std::fixed << std::setprecision(1);
    out << "| Convergence rate | "
        << (records.empty() ? 0.0 : 100.0 * n_converged / records.size())
        << "% |\n";
    out << "| Total wall time | " << total_time_ms / 1000.0 << " s |\n";
    out << "| Avg time / sim | " << total_time_ms / records.size() << " ms |\n";
    out << "| Total integration steps | " << total_steps << " |\n";
    out << "| Total intermediate rows | " << total_inter << " |\n";
    out << std::setprecision(6);
    out << "| Best final energy | " << best_energy
        << " kcal/mol (sim #" << best_sim << ") |\n\n";

    // --- Section 3: Sample Size and Mass Statistics ---
    out << "## 3. Sample Size and Mass Statistics\n\n";
    out << "| Metric | Value |\n";
    out << "|--------|-------|\n";
    out << "| Min beads (sample size) | " << min_N << " |\n";
    out << "| Max beads (sample size) | " << max_N << " |\n";
    out << std::fixed << std::setprecision(4);
    out << "| Min total mass | " << min_mass << " amu |\n";
    out << "| Max total mass | " << max_mass << " amu |\n";
    out << "| Mean total mass | " << avg_mass << " amu |\n\n";

    // --- Section 4: Arrangement Selection Frequency ---
    out << "## 4. Arrangement Selection Frequency\n\n";
    out << "| Arr ID | Label | Times Selected | Converged | Conv Rate |\n";
    out << "|--------|-------|---------------|-----------|----------|\n";
    std::vector<uint32_t> sel_count(50, 0);
    std::vector<uint32_t> conv_count(50, 0);
    for (const auto& r : records) {
        if (r.arrangement_id < 50) {
            sel_count[r.arrangement_id]++;
            if (r.converged) conv_count[r.arrangement_id]++;
        }
    }
    for (uint32_t i = 0; i < 50 && i < arrangements.size(); ++i) {
        double rate = sel_count[i] > 0
                      ? 100.0 * conv_count[i] / sel_count[i] : 0.0;
        out << "| " << i
            << " | " << arrangements[i].label
            << " | " << sel_count[i]
            << " | " << conv_count[i]
            << " | " << std::setprecision(1) << rate << "% |\n";
    }
    out << "\n";

    // --- Section 5: Convergence by Base Reaction ---
    out << "## 5. Convergence by Base Reaction\n\n";
    out << "| Base Reaction | Equation | Sims | Converged | Rate |\n";
    out << "|---------------|----------|------|-----------|------|\n";
    std::map<std::string, std::pair<int,int>> rxn_stats;
    std::map<std::string, std::string>        rxn_eq;
    for (const auto& r : records) {
        rxn_stats[r.base_reaction].first++;
        if (r.converged) rxn_stats[r.base_reaction].second++;
        rxn_eq[r.base_reaction] = r.equation;
    }
    for (auto& [name, stats] : rxn_stats) {
        double rate = stats.first > 0 ? 100.0 * stats.second / stats.first : 0.0;
        out << "| " << name
            << " | " << rxn_eq[name]
            << " | " << stats.first
            << " | " << stats.second
            << " | " << std::setprecision(1) << rate << "% |\n";
    }
    out << "\n";

    // --- Section 6: Convergence by Temperature Regime ---
    out << "## 6. Convergence by Temperature Regime\n\n";
    out << "| T Regime | Sims | Converged | Rate |\n";
    out << "|----------|------|-----------|------|\n";
    std::map<std::string, std::pair<int,int>> temp_stats;
    for (const auto& r : records) {
        std::string tbin;
        if      (r.temperature_K < 280.0) tbin = "Cold (<280 K)";
        else if (r.temperature_K < 400.0) tbin = "Standard (280-400 K)";
        else                               tbin = "Hot (>400 K)";
        temp_stats[tbin].first++;
        if (r.converged) temp_stats[tbin].second++;
    }
    for (auto& [name, stats] : temp_stats) {
        double rate = stats.first > 0 ? 100.0 * stats.second / stats.first : 0.0;
        out << "| " << name << " | " << stats.first
            << " | " << stats.second
            << " | " << std::setprecision(1) << rate << "% |\n";
    }
    out << "\n";

    // --- Section 7: Seed Hash Traceability Table (first 20) ---
    out << "## 7. Seed Hash Traceability (first 20 simulations)\n\n";
    out << "| Sim | Seed Hash | Arr | Equation | N_beads | Mass (amu) | Conv | Steps |\n";
    out << "|-----|-----------|-----|----------|---------|------------|------|-------|\n";
    uint32_t preview = std::min(static_cast<uint32_t>(records.size()), 20u);
    for (uint32_t i = 0; i < preview; ++i) {
        const auto& r = records[i];
        out << "| " << r.sim_id
            << " | `" << r.seed_hash << "`"
            << " | " << r.arrangement_id
            << " | " << r.equation
            << " | " << r.N_beads
            << " | " << std::setprecision(2) << r.total_mass_amu
            << " | " << (r.converged ? "YES" : "no")
            << " | " << r.steps_taken << " |\n";
    }
    out << "\n";

    out << "---\n";
    out << "*Report generated by VSEPR-SIM v2.9.2 — deterministic atomistic platform*\n";
    out << "*Anti-black-box: every seed hash, equation, intermediate, and metric is explicitly tabulated.*\n";
    out << "*Intermediates directory: `intermediates/<SEED_HASH>.csv` — one file per simulation, one row per integration step.*\n";

    std::cout << "  Written: " << path << "\n";
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[])
{
    // Parse optional master seed from command line
    uint64_t master_seed = DEFAULT_SEED;
    if (argc >= 2) {
        try {
            master_seed = std::stoull(argv[1]);
        } catch (...) {
            std::cerr << "WARNING: Invalid seed argument '" << argv[1]
                      << "', using default seed " << DEFAULT_SEED << "\n";
        }
    }

    std::cout << "================================================================\n";
    std::cout << " VSEPR-SIM v2.9.2: 1000-Iteration Seeded Random Atomistic Sim\n";
    std::cout << " Master seed: " << master_seed
              << "  (0x" << std::hex << master_seed << std::dec << ")\n";
    std::cout << "================================================================\n\n";

    // Build the 50 arrangements
    auto arrangements = build_arrangements();
    std::cout << "Built " << arrangements.size() << " chemical arrangements.\n";

    // Seeded RNG for random arrangement selection
    std::mt19937_64 rng(master_seed);
    std::uniform_int_distribution<uint32_t> arr_dist(
        0, static_cast<uint32_t>(arrangements.size() - 1));

    // Ensure intermediates output directory exists
    const std::string inter_dir = "intermediates";
    std::filesystem::create_directories(inter_dir);

    std::vector<SimRecord1000> all_results;
    all_results.reserve(N_ITERATIONS);

    auto wall_t0 = std::chrono::high_resolution_clock::now();

    std::cout << "\nRunning " << N_ITERATIONS << " simulations...\n";
    std::cout << std::string(64, '-') << "\n";

    for (uint32_t iter = 0; iter < N_ITERATIONS; ++iter) {
        // Randomly select an arrangement
        uint32_t arr_idx = arr_dist(rng);
        const ArrangementDescriptor& ad = arrangements[arr_idx];

        // Derive seed hash for this simulation
        std::string hash = fmt_hash(make_seed_hash(master_seed, ad.id, iter));

        // Progress print every 50 iterations
        if (iter % 50 == 0) {
            std::cout << "[" << std::setw(4) << iter << "/" << N_ITERATIONS << "] "
                      << "arr=" << std::setw(2) << arr_idx
                      << " hash=" << hash
                      << " label=" << ad.label << "\n";
        }

        // Run simulation (500 steps, dt=0.5 fs)
        auto rec = run_single_sim(iter, master_seed, iter, ad, 500, 0.5);

        // Write per-sim intermediates CSV immediately (avoid holding all in RAM)
        write_intermediates(inter_dir, rec);

        // Strip intermediates from the record before pushing to summary
        // (they are now on disk — keep only final scalars in memory)
        rec.intermediates.clear();
        rec.intermediates.shrink_to_fit();

        all_results.push_back(rec);
    }

    auto wall_t1 = std::chrono::high_resolution_clock::now();
    double wall_ms = std::chrono::duration<double, std::milli>(wall_t1 - wall_t0).count();

    std::cout << std::string(64, '-') << "\n";
    std::cout << "Completed " << N_ITERATIONS << " simulations in "
              << std::fixed << std::setprecision(2) << wall_ms / 1000.0 << " s\n\n";

    // Convergence summary
    uint32_t n_conv = 0;
    for (const auto& r : all_results) if (r.converged) ++n_conv;
    std::cout << "Converged: " << n_conv << " / " << N_ITERATIONS
              << " (" << std::setprecision(1) << 100.0 * n_conv / N_ITERATIONS << "%)\n\n";

    // Write outputs
    std::cout << "Writing outputs...\n";
    write_results_csv("sim_1000_results.csv", all_results);
    write_report("sim_1000_report.md", master_seed, arrangements, all_results);

    std::cout << "\nIntermediates: " << inter_dir << "/ ("
              << N_ITERATIONS << " files, one per simulation)\n";
    std::cout << "Each file keyed to its seed hash — traceable to master seed "
              << master_seed << "\n\n";

    std::cout << "================================================================\n";
    std::cout << " Done.\n";
    std::cout << "================================================================\n";

    return 0;
}
