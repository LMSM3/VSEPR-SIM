/**
 * sim_101x50.cpp — 101 Simulations × 50 Chemical Arrangements
 *
 * Deterministic batch runner that generates 50 distinct chemical
 * arrangements and runs 101 atomistic simulations across them.
 *
 * Arrangement generation strategy:
 *   - 3 library reactions (benzene nitration, thorium oxalate, Cu decomposition)
 *   - Each base reaction spawns parametric variants:
 *       × 3 temperature regimes  (low, standard, high)
 *       × 3 coupling strengths   (weak, moderate, strong)
 *       × 2 system sizes         (small, large)
 *   - 3 × 3 × 3 × 2 = 54 arrangements → capped at 50
 *
 * Simulation schedule:
 *   - Arrangements 1–49:  2 simulations each (short + long) = 98
 *   - Arrangement 50:     3 simulations (short + long + extended) = 3
 *   - Total = 101 simulations
 *
 * Output:
 *   - Per-simulation: convergence, energy, RMS force, mean η, steps
 *   - Summary CSV:    sim_101x50_results.csv
 *   - Summary report: sim_101x50_report.md
 *
 * Anti-black-box: every parameter variation is explicitly tabulated.
 * Deterministic: identical build → identical results.
 *
 * Reference: copilot-instructions.md §2, §5, §7
 */

#include "coarse_grain/chemistry/reaction_engine.hpp"
#include "coarse_grain/chemistry/reaction_library.hpp"
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace coarse_grain;
using namespace coarse_grain::chemistry;

// ============================================================================
// Arrangement Descriptor
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
    uint32_t    system_scale{1};    // Multiplier on bead count
};

// ============================================================================
// Simulation Result Record
// ============================================================================

struct SimRecord {
    uint32_t    sim_id{};
    uint32_t    arrangement_id{};
    std::string arrangement_label;
    std::string run_tag;            // "short", "long", "extended"
    uint64_t    max_steps{};
    double      dt_initial{};
    bool        converged{};
    uint64_t    steps_taken{};
    double      final_energy{};
    double      final_rms_force{};
    double      final_avg_eta{};
    double      elapsed_ms{};
};

// ============================================================================
// Build 50 Arrangements
// ============================================================================

static std::vector<ArrangementDescriptor> build_arrangements() {
    std::vector<ArrangementDescriptor> arr;
    arr.reserve(50);

    // Base reactions from the library
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

    // Temperature regimes
    double temps[] = {250.0, 298.15, 500.0};
    const char* temp_tags[] = {"cold", "std", "hot"};

    // Coupling strengths
    double coupling_scale[] = {0.5, 1.0, 2.0};
    const char* coupling_tags[] = {"weak", "mod", "strong"};

    // System sizes
    uint32_t scales[] = {1, 2};
    const char* scale_tags[] = {"small", "large"};

    uint32_t id = 0;
    for (auto& base : bases) {
        for (int ti = 0; ti < 3; ++ti) {
            for (int ci = 0; ci < 3; ++ci) {
                for (int si = 0; si < 2; ++si) {
                    if (id >= 50) break;

                    ArrangementDescriptor ad;
                    ad.id = id;
                    ad.base_reaction = base.label;
                    ad.temperature_K = temps[ti];
                    ad.tau = base.default_tau * coupling_scale[ci];
                    ad.gamma_steric = base.default_g_steric * coupling_scale[ci];
                    ad.gamma_elec = base.default_g_elec * coupling_scale[ci];
                    ad.gamma_disp = base.default_g_disp * coupling_scale[ci];
                    ad.system_scale = scales[si];

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

    // Pad remaining slots with additional temperature sweeps if needed
    while (arr.size() < 50) {
        auto& last = arr.back();
        ArrangementDescriptor ad = last;
        ad.id = static_cast<uint32_t>(arr.size());
        ad.temperature_K += 25.0;
        ad.label = last.label + "_T" + std::to_string(static_cast<int>(ad.temperature_K));
        arr.push_back(ad);
    }

    return arr;
}

// ============================================================================
// Map arrangement → ChemicalReaction + SeedBeadParams
// ============================================================================

static ChemicalReaction get_base_reaction(const std::string& name) {
    if (name == "benzene_nitration")
        return library::build_benzene_nitration();
    if (name == "thorium_oxalate")
        return library::build_thorium_oxalate_precipitation();
    if (name == "copper_decomposition")
        return library::build_copper_nitrate_decomposition();
    // Fallback
    return library::build_benzene_nitration();
}

static SeedBeadParams build_params_for(const ArrangementDescriptor& ad,
                                        uint64_t max_steps, double dt_init)
{
    auto rxn = get_base_reaction(ad.base_reaction);
    SeedBeadParams params = ReactionEngine::build_reaction_params(rxn);
    params.dt_initial = dt_init;
    params.max_steps = max_steps;
    params.env_params.tau = ad.tau;
    params.env_params.gamma_steric = ad.gamma_steric;
    params.env_params.gamma_elec = ad.gamma_elec;
    params.env_params.gamma_disp = ad.gamma_disp;
    params.snapshot_interval = 50;
    params.record_positions = false;  // Save memory in batch mode
    return params;
}

// ============================================================================
// Run a single simulation
// ============================================================================

static SimRecord run_single_sim(
    uint32_t sim_id,
    const ArrangementDescriptor& ad,
    const std::string& run_tag,
    uint64_t max_steps,
    double dt_init)
{
    SimRecord rec;
    rec.sim_id = sim_id;
    rec.arrangement_id = ad.id;
    rec.arrangement_label = ad.label;
    rec.run_tag = run_tag;
    rec.max_steps = max_steps;
    rec.dt_initial = dt_init;

    auto rxn = get_base_reaction(ad.base_reaction);
    rxn.thermal.temperature_K = ad.temperature_K;

    BeadSystem system = ReactionEngine::map_reaction_to_beads(rxn);

    // Scale system if requested
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

    SeedBeadParams params = build_params_for(ad, max_steps, dt_init);

    size_t N = system.beads.size();
    std::vector<EnvironmentState> env_states(N);
    std::vector<atomistic::Vec3> velocities(N);
    std::vector<atomistic::Vec3> forces(N);
    FIREState fire;
    fire.dt = params.dt_initial;
    fire.alpha = params.fire_alpha_start;

    SeedBeadStepper::init(system, env_states, params);

    auto t0 = std::chrono::high_resolution_clock::now();

    for (uint64_t s = 0; s < params.max_steps; ++s) {
        auto step_rec = SeedBeadStepper::step(
            system, env_states, velocities, forces, fire, params, s);

        if (step_rec.steady_state) {
            rec.converged = true;
            rec.steps_taken = s + 1;
            rec.final_energy = step_rec.total_energy;
            rec.final_rms_force = step_rec.rms_force;
            rec.final_avg_eta = step_rec.avg_eta;
            break;
        }

        // Store last-step values for non-converged runs
        rec.steps_taken = s + 1;
        rec.final_energy = step_rec.total_energy;
        rec.final_rms_force = step_rec.rms_force;
        rec.final_avg_eta = step_rec.avg_eta;
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    rec.elapsed_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    return rec;
}

// ============================================================================
// Write CSV results
// ============================================================================

static void write_csv(const std::string& path, const std::vector<SimRecord>& records) {
    std::ofstream out(path);
    if (!out.is_open()) {
        std::cerr << "ERROR: Cannot write " << path << "\n";
        return;
    }

    out << "sim_id,arrangement_id,arrangement_label,run_tag,max_steps,dt_initial,"
        << "converged,steps_taken,final_energy,final_rms_force,final_avg_eta,elapsed_ms\n";

    out << std::fixed;
    for (auto& r : records) {
        out << r.sim_id << ","
            << r.arrangement_id << ","
            << r.arrangement_label << ","
            << r.run_tag << ","
            << r.max_steps << ","
            << std::setprecision(2) << r.dt_initial << ","
            << (r.converged ? "true" : "false") << ","
            << r.steps_taken << ","
            << std::setprecision(6) << r.final_energy << ","
            << std::setprecision(6) << r.final_rms_force << ","
            << std::setprecision(6) << r.final_avg_eta << ","
            << std::setprecision(1) << r.elapsed_ms << "\n";
    }

    std::cout << "  Written: " << path << " (" << records.size() << " rows)\n";
}

// ============================================================================
// Write Markdown Report
// ============================================================================

static void write_report(const std::string& path,
                         const std::vector<ArrangementDescriptor>& arrangements,
                         const std::vector<SimRecord>& records)
{
    std::ofstream out(path);
    if (!out.is_open()) {
        std::cerr << "ERROR: Cannot write " << path << "\n";
        return;
    }

    out << "# VSEPR-SIM: 101 Simulations x 50 Chemical Arrangements\n\n";
    out << "**Generated by VSEPR-SIM v2.9.2 Deterministic Atomistic Platform**\n\n";
    out << "---\n\n";

    // Summary stats
    uint32_t converged_count = 0;
    double total_time_ms = 0.0;
    double best_energy = 1e30;
    uint32_t best_sim = 0;
    for (auto& r : records) {
        if (r.converged) ++converged_count;
        total_time_ms += r.elapsed_ms;
        if (r.final_energy < best_energy) {
            best_energy = r.final_energy;
            best_sim = r.sim_id;
        }
    }

    out << "## 1. Summary\n\n";
    out << "| Metric | Value |\n";
    out << "|--------|-------|\n";
    out << "| Total simulations | 101 |\n";
    out << "| Chemical arrangements | 50 |\n";
    out << "| Converged | " << converged_count << " / 101 |\n";
    out << std::fixed << std::setprecision(1);
    out << "| Total wall time | " << total_time_ms / 1000.0 << " s |\n";
    out << "| Avg time / sim | " << total_time_ms / 101.0 << " ms |\n";
    out << std::setprecision(6);
    out << "| Best energy | " << best_energy << " kcal/mol (sim #" << best_sim << ") |\n\n";

    // Arrangement table
    out << "## 2. Chemical Arrangements (50)\n\n";
    out << "| ID | Label | Base Reaction | T (K) | tau | g_steric | g_elec | g_disp | Scale |\n";
    out << "|----|-------|---------------|-------|-----|----------|--------|--------|-------|\n";
    out << std::setprecision(2);
    for (auto& a : arrangements) {
        out << "| " << a.id
            << " | " << a.label
            << " | " << a.base_reaction
            << " | " << a.temperature_K
            << " | " << a.tau
            << " | " << a.gamma_steric
            << " | " << a.gamma_elec
            << " | " << a.gamma_disp
            << " | " << a.system_scale << "x |\n";
    }
    out << "\n";

    // Results table
    out << "## 3. Simulation Results (101)\n\n";
    out << "| Sim | Arr | Label | Run | Steps | Conv | Energy | RMS F | eta | Time (ms) |\n";
    out << "|-----|-----|-------|-----|-------|------|--------|-------|-----|----------|\n";
    for (auto& r : records) {
        out << std::setprecision(4);
        out << "| " << r.sim_id
            << " | " << r.arrangement_id
            << " | " << r.arrangement_label
            << " | " << r.run_tag
            << " | " << r.steps_taken
            << " | " << (r.converged ? "YES" : "no")
            << " | " << r.final_energy
            << " | " << r.final_rms_force
            << " | " << r.final_avg_eta
            << " | " << std::setprecision(1) << r.elapsed_ms << " |\n";
    }
    out << "\n";

    // Convergence by base reaction
    out << "## 4. Convergence by Base Reaction\n\n";
    out << "| Base Reaction | Total Sims | Converged | Rate |\n";
    out << "|---------------|-----------|-----------|------|\n";
    std::map<std::string, std::pair<int, int>> base_stats;
    for (auto& r : records) {
        auto& a = arrangements[r.arrangement_id];
        base_stats[a.base_reaction].first++;
        if (r.converged) base_stats[a.base_reaction].second++;
    }
    for (auto& [name, stats] : base_stats) {
        double rate = stats.first > 0 ? 100.0 * stats.second / stats.first : 0.0;
        out << "| " << name << " | " << stats.first
            << " | " << stats.second
            << " | " << std::setprecision(1) << rate << "% |\n";
    }
    out << "\n";

    // Convergence by temperature
    out << "## 5. Convergence by Temperature Regime\n\n";
    out << "| T Range | Total | Converged | Rate |\n";
    out << "|---------|-------|-----------|------|\n";
    std::map<std::string, std::pair<int, int>> temp_stats;
    for (auto& r : records) {
        auto& a = arrangements[r.arrangement_id];
        std::string tbin;
        if (a.temperature_K < 280.0) tbin = "Cold (<280 K)";
        else if (a.temperature_K < 400.0) tbin = "Standard (280-400 K)";
        else tbin = "Hot (>400 K)";
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

    out << "---\n";
    out << "*Report generated by VSEPR-SIM v2.9.2 — deterministic atomistic platform*\n";
    out << "*Anti-black-box: every parameter, metric, and outcome is explicitly tabulated.*\n";

    std::cout << "  Written: " << path << "\n";
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "================================================================\n";
    std::cout << " VSEPR-SIM v2.9.2: 101 Simulations x 50 Chemical Arrangements\n";
    std::cout << " Deterministic Atomistic Batch Runner\n";
    std::cout << "================================================================\n\n";

    // Build 50 arrangements
    auto arrangements = build_arrangements();
    std::cout << "Generated " << arrangements.size() << " chemical arrangements.\n\n";

    std::vector<SimRecord> all_results;
    all_results.reserve(101);

    uint32_t sim_id = 0;
    auto total_t0 = std::chrono::high_resolution_clock::now();

    // Arrangements 0–48: 2 simulations each = 98
    for (uint32_t a = 0; a < 49 && a < arrangements.size(); ++a) {
        auto& ad = arrangements[a];

        // Short run
        std::cout << "[Sim " << std::setw(3) << sim_id << "] "
                  << ad.label << " (short)... " << std::flush;
        auto r1 = run_single_sim(sim_id++, ad, "short", 500, 0.5);
        std::cout << (r1.converged ? "CONVERGED" : "timeout")
                  << " E=" << std::setprecision(4) << r1.final_energy
                  << " (" << std::setprecision(1) << r1.elapsed_ms << " ms)\n";
        all_results.push_back(r1);

        // Long run
        std::cout << "[Sim " << std::setw(3) << sim_id << "] "
                  << ad.label << " (long)...  " << std::flush;
        auto r2 = run_single_sim(sim_id++, ad, "long", 2000, 0.3);
        std::cout << (r2.converged ? "CONVERGED" : "timeout")
                  << " E=" << std::setprecision(4) << r2.final_energy
                  << " (" << std::setprecision(1) << r2.elapsed_ms << " ms)\n";
        all_results.push_back(r2);
    }

    // Arrangement 49: 3 simulations = 3 (total = 101)
    if (arrangements.size() >= 50) {
        auto& ad = arrangements[49];

        std::cout << "[Sim " << std::setw(3) << sim_id << "] "
                  << ad.label << " (short)...    " << std::flush;
        auto r1 = run_single_sim(sim_id++, ad, "short", 500, 0.5);
        std::cout << (r1.converged ? "CONVERGED" : "timeout")
                  << " E=" << std::setprecision(4) << r1.final_energy
                  << " (" << std::setprecision(1) << r1.elapsed_ms << " ms)\n";
        all_results.push_back(r1);

        std::cout << "[Sim " << std::setw(3) << sim_id << "] "
                  << ad.label << " (long)...     " << std::flush;
        auto r2 = run_single_sim(sim_id++, ad, "long", 2000, 0.3);
        std::cout << (r2.converged ? "CONVERGED" : "timeout")
                  << " E=" << std::setprecision(4) << r2.final_energy
                  << " (" << std::setprecision(1) << r2.elapsed_ms << " ms)\n";
        all_results.push_back(r2);

        std::cout << "[Sim " << std::setw(3) << sim_id << "] "
                  << ad.label << " (extended)... " << std::flush;
        auto r3 = run_single_sim(sim_id++, ad, "extended", 5000, 0.2);
        std::cout << (r3.converged ? "CONVERGED" : "timeout")
                  << " E=" << std::setprecision(4) << r3.final_energy
                  << " (" << std::setprecision(1) << r3.elapsed_ms << " ms)\n";
        all_results.push_back(r3);
    }

    auto total_t1 = std::chrono::high_resolution_clock::now();
    double total_ms = std::chrono::duration<double, std::milli>(total_t1 - total_t0).count();

    // Summary
    std::cout << "\n================================================================\n";
    std::cout << " COMPLETE: " << all_results.size() << " simulations across "
              << arrangements.size() << " arrangements\n";
    std::cout << " Total wall time: " << std::fixed << std::setprecision(1)
              << total_ms / 1000.0 << " s\n";

    uint32_t conv = 0;
    for (auto& r : all_results) if (r.converged) ++conv;
    std::cout << " Converged: " << conv << " / " << all_results.size() << "\n";
    std::cout << "================================================================\n\n";

    // Write outputs
    write_csv("sim_101x50_results.csv", all_results);
    write_report("sim_101x50_report.md", arrangements, all_results);

    std::cout << "\nDone.\n";
    return 0;
}
