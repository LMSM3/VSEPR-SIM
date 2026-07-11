/**
 * core_run.cpp — V4.0.4.04 Continual Formation Engine
 * ═══════════════════════════════════════════════════
 * "Turn on the core" — runs formation continuously across all 12
 * reference systems, applying random permutations of the central
 * atom selection on every iteration.
 *
 * Each cycle:
 *   1. Pick a reference system (round-robin across 12 elements)
 *   2. Build a lattice scene for that system's structure type
 *   3. Randomly permute which bead is treated as the central atom
 *      (all N_beads candidates are probed per system per cycle)
 *   4. Run formation history from each candidate central atom
 *   5. Score gamma / data_quality / compactness for each
 *   6. Print live streaming table to stdout
 *   7. Accumulate into correlation matrix
 *   8. Re-print ranked correlations every REPORT_INTERVAL cycles
 *
 * This runs until Ctrl+C.
 *
 * Build (WSL / GCC 14):
 *   g++ -std=c++23 -O3 -ftrivial-auto-var-init=pattern \
 *       -I. v4/core_run.cpp -o v4/core_run
 *
 * C++26 features:
 *   - Erroneous-behaviour pattern init (-ftrivial-auto-var-init=pattern)
 *   - Contract emulation (V4_CONTRACT_PRE/POST in all v4/ headers)
 *   - Structured bindings (auto& [vi, vj, r])
 *   - Trailing return types throughout
 */

// ── V4 kernel ──────────────────────────────────────────────────────────────
#include "v4/formation_record.hpp"
#include "v4/gamma_score.hpp"
#include "v4/data_quality.hpp"
#include "v4/compactness.hpp"
#include "v4/correlation_matrix.hpp"
#include "v4/corr_heatmap.hpp"

// ── Existing kernel infrastructure ─────────────────────────────────────────
#include "tests/scene_builders.hpp"
#include "tests/test_runners.hpp"
#include "coarse_grain/core/environment_state.hpp"
#include "coarse_grain/models/environment_coupling.hpp"

// ── STL ────────────────────────────────────────────────────────────────────
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <string>
#include <vector>

// ============================================================================
// Engine Configuration
// ============================================================================

static constexpr int    N_BEADS        = 64;
static constexpr int    N_STEPS_MAX    = 6000;
static constexpr int    N_STEPS_FAST   = 500;   // max for trivial geometries
static constexpr double DT             = 10.0;
static constexpr double CONV_THRESH    = 1e-4;   // rms_force convergence
static constexpr int    REPORT_INTERVAL = 4;     // cycles between corr reports
static constexpr int    N_ELEMENTS     = 12;
static constexpr int    MAX_CENTRAL_PERMS = 8;   // max central-atom probes per element

// ============================================================================
// Graceful Shutdown
// ============================================================================

static volatile bool g_running = true;

static void handle_sigint(int) {
    g_running = false;
    std::cout << "\n\n[CORE] SIGINT received — finishing current cycle...\n";
}

// ============================================================================
// Formation Parameters (matched to Suite #4 defaults)
// ============================================================================

static auto make_params() -> coarse_grain::EnvironmentParams {
    coarse_grain::EnvironmentParams p;
    p.alpha        = 0.5;
    p.beta         = 0.5;
    p.tau          = 100.0;
    p.gamma_steric = 0.2;
    p.gamma_elec   = -0.1;
    p.gamma_disp   = 0.5;
    p.sigma_rho    = 3.0;
    p.r_cutoff     = 8.0;
    p.delta_sw     = 1.0;
    p.rho_max      = 10.0;
    return p;
}

// ============================================================================
// Scene Builder — maps LatticeClass → scene geometry
// ============================================================================

/**
 * Build a scene appropriate for a given lattice class.
 * FCC / BCC → cubic-like lattice with 4^3 = 64 beads.
 * HCP       → layered slab approximation.
 *
 * The central atom is the bead at index `central_idx`.
 * All other beads are neighbours. Permuting central_idx probes
 * every possible "central atom" environment in one scene.
 */
static auto build_scene_for(v4::LatticeClass lc, double spacing)
    -> std::vector<test_util::SceneBead>
{
    switch (lc) {
        case v4::LatticeClass::FCC:
            // FCC: 4x4x4 = 64 beads, slight orientational disorder
            return test_util::scene_perturbed_lattice(4, spacing, 0.05, 42u);

        case v4::LatticeClass::BCC:
            // BCC: denser packing, also 4x4x4 base + jitter
            return test_util::scene_perturbed_lattice(4, spacing * 0.87, 0.05, 137u);

        case v4::LatticeClass::HCP:
            // HCP: layered slab (4 layers of 4x4 = 64 beads)
            return test_util::scene_layered_slab(4, 16, spacing, spacing * 1.633);

        default:
            return test_util::scene_cubic_lattice(4, spacing);
    }
}

// ============================================================================
// Run One Formation — single central-atom probe
// ============================================================================

struct ProbeResult {
    int    central_idx{0};
    double mean_eta{0};
    double mean_rho{0};
    double mean_C{0};
    double rms_force{0};
    double final_energy{0};
    double sigma_eta{0};
    bool   converged{false};
    int    steps{0};
    double elapsed_ms{0};
};

static auto run_probe(
    const std::vector<test_util::SceneBead>& scene,
    int central_idx,
    const coarse_grain::EnvironmentParams& params,
    int max_steps) -> ProbeResult
{
    ProbeResult pr;
    pr.central_idx = central_idx;

    auto t0 = std::chrono::steady_clock::now();

    int n = static_cast<int>(scene.size());
    std::vector<double> etas(n, 0.0);

    // Set slightly non-zero initial eta for central atom (breaks symmetry)
    etas[central_idx] = 0.05;

    std::vector<coarse_grain::EnvironmentState> states(n);

    double prev_rms = 1e30;
    bool converged  = false;
    int conv_step   = max_steps;

    for (int step = 0; step < max_steps && g_running; ++step) {
        double sum_eta = 0, sum_rho = 0, sum_C = 0;
        double sum_df2 = 0;

        for (int i = 0; i < n; ++i) {
            auto nbs = test_util::build_neighbours(scene, i);
            states[i] = coarse_grain::update_environment_state(
                etas[i], scene[i].n_hat, scene[i].has_orientation,
                nbs, params, DT);
            etas[i]  = states[i].eta;
            sum_eta += states[i].eta;
            sum_rho += states[i].rho;
            sum_C   += states[i].C;
            double df = states[i].eta - (step > 0 ? etas[i] : 0.0);
            sum_df2 += df * df;
        }

        double rms = std::sqrt(sum_df2 / n);

        // Check convergence every 50 steps
        if (step > 0 && step % 50 == 0) {
            if (rms < CONV_THRESH && std::abs(rms - prev_rms) < CONV_THRESH * 0.1) {
                converged = true;
                conv_step = step;
                pr.mean_eta = sum_eta / n;
                pr.mean_rho = sum_rho / n;
                pr.mean_C   = sum_C / n;
                pr.rms_force = rms;
                break;
            }
            prev_rms = rms;
        }

        // Always record final snapshot
        if (step == max_steps - 1) {
            pr.mean_eta = sum_eta / n;
            pr.mean_rho = sum_rho / n;
            pr.mean_C   = sum_C / n;
            pr.rms_force = rms;
        }
    }

    // Sigma_eta across final bead states
    {
        double sum2 = 0;
        for (auto v : etas) sum2 += v * v;
        double mean2 = pr.mean_eta * pr.mean_eta;
        pr.sigma_eta = std::sqrt(std::max(0.0, sum2 / n - mean2));
    }

    // Approximate final energy as sum of eta * rho (internal cohesion proxy)
    {
        double E = 0;
        for (int i = 0; i < n; ++i)
            E += states[i].eta * states[i].rho;
        pr.final_energy = -std::abs(E) * 100.0;  // sign convention: negative = bound
    }

    auto t1 = std::chrono::steady_clock::now();
    pr.elapsed_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    pr.converged  = converged;
    pr.steps      = conv_step;

    return pr;
}

// ============================================================================
// Assemble V4 FormationRecord from Probe + Reference Data
// ============================================================================

static auto make_record(
    const v4::FormationRecord& ref,
    const ProbeResult& pr,
    int central_idx,
    int n_beads) -> v4::FormationRecord
{
    v4::FormationRecord rec;
    rec.symbol      = ref.symbol + "#" + std::to_string(central_idx);
    rec.name        = ref.name;
    rec.structure   = ref.structure;
    rec.n_beads     = n_beads;
    rec.converged   = pr.converged;
    rec.steps       = pr.steps;
    rec.rms_force   = pr.rms_force;
    rec.avg_eta     = pr.mean_eta;
    rec.avg_rho     = pr.mean_rho;
    rec.avg_C       = pr.mean_C;
    rec.final_energy = pr.final_energy;
    rec.elapsed_ms  = pr.elapsed_ms;
    rec.n_l3_domains = (pr.mean_C > 10.0) ? 1 : 0;

    // Macro precursors: forward from reference (calibrated experimental values)
    rec.macro_rigidity  = ref.macro_rigidity;
    rec.macro_ductility = ref.macro_ductility;
    rec.macro_color     = ref.macro_color;

    // Experimental references: inherit from reference dataset
    rec.Ecoh_eV  = ref.Ecoh_eV;
    rec.Tmelt_K  = ref.Tmelt_K;
    rec.a0_ang   = ref.a0_ang;
    rec.B_GPa    = ref.B_GPa;
    rec.G_GPa    = ref.G_GPa;
    rec.E_GPa    = ref.E_GPa;

    // Score all three V4 meta-scores
    v4::score_gamma(rec,
        v4::NaN,          // no reference run
        pr.sigma_eta,     // history sigma
        rec.macro_rigidity,
        0, 4);

    v4::score_data_quality(rec);
    v4::score_compactness(rec);

    return rec;
}

// ============================================================================
// Live Stream Header
// ============================================================================

static void print_header() {
    std::cout << "\n";
    std::cout << "═══════════════════════════════════════════════════════════════════════════════════\n";
    VSEPR-SIM V4.0.4.04 — CONTINUAL FORMATION ENGINE
    std::cout << "  O(xN) Central-Atom Permutation Sweep  |  Day #47 Reference Systems\n";
    std::cout << "  C++26: erroneous-behaviour | contracts | flat Pearson matrix\n";
    std::cout << "  Press Ctrl+C to stop.\n";
    std::cout << "═══════════════════════════════════════════════════════════════════════════════════\n";
    std::cout << std::setw(6)  << "Cycle"
              << std::setw(4)  << "Sym"
              << std::setw(4)  << "Ctr"
              << std::setw(5)  << "Lat"
              << std::setw(6)  << "Conv"
              << std::setw(7)  << "Steps"
              << std::setw(9)  << "avg_eta"
              << std::setw(9)  << "avg_rho"
              << std::setw(9)  << "avg_C"
              << std::setw(9)  << "rms_f"
              << std::setw(9)  << "gamma"
              << std::setw(9)  << "Q_data"
              << std::setw(10) << "C_compact"
              << std::setw(10) << "ms"
              << "\n";
    std::cout << std::string(100, '-') << "\n";
}

// ============================================================================
// Main Engine Loop
// ============================================================================

int main() {
    std::signal(SIGINT, handle_sigint);

    print_header();

    auto ref_data  = v4::reference_dataset();
    auto params    = make_params();

    // Accumulated dataset for correlation matrix
    std::vector<v4::FormationRecord> accumulated;
    accumulated.reserve(4096);

    // Lattice spacings from Day #47 reference (a0_ang)
    // FCC: ~4.0 Å,  BCC: ~3.0 Å,  HCP: ~2.7 Å
    auto spacing_for = [&](const v4::FormationRecord& ref) -> double {
        return std::isfinite(ref.a0_ang) && ref.a0_ang > 0.0
               ? ref.a0_ang * 0.9   // slight compression toward ideal
               : 4.0;
    };

    // Per-element PRNG for permutation order (each element gets its own seed)
    std::array<std::mt19937, N_ELEMENTS> rngs;
    for (int e = 0; e < N_ELEMENTS; ++e)
        rngs[e] = std::mt19937(static_cast<uint32_t>(e * 997u + 42u));

    int cycle      = 0;
    int total_runs = 0;

    while (g_running) {
        ++cycle;

        // Round-robin across all 12 elements
        for (int e = 0; e < N_ELEMENTS && g_running; ++e) {
            const auto& ref = ref_data[e];
            double sp       = spacing_for(ref);
            auto   scene    = build_scene_for(ref.structure, sp);
            int    n        = static_cast<int>(scene.size());

            // Clamp to actual bead count
            int n_probes = std::min(MAX_CENTRAL_PERMS, n);

            // Generate random permutation of central-atom indices
            std::vector<int> indices(n);
            std::iota(indices.begin(), indices.end(), 0);
            std::shuffle(indices.begin(), indices.end(), rngs[e]);
            indices.resize(n_probes);

            for (int pi = 0; pi < n_probes && g_running; ++pi) {
                int central = indices[pi];

                // Choose step budget: full systems get more time
                int budget = ref.converged ? N_STEPS_MAX : N_STEPS_FAST;

                auto pr  = run_probe(scene, central, params, budget);
                auto rec = make_record(ref, pr, central, n);

                accumulated.push_back(rec);
                ++total_runs;

                // Live stream line
                std::cout << std::setw(6)  << cycle
                          << std::setw(4)  << ref.symbol
                          << std::setw(4)  << central
                          << std::setw(5)  << v4::lattice_name(ref.structure)
                          << std::setw(6)  << (rec.converged ? "Y" : "N")
                          << std::setw(7)  << rec.steps
                          << std::setw(9)  << std::fixed << std::setprecision(4) << rec.avg_eta
                          << std::setw(9)  << std::fixed << std::setprecision(4) << rec.avg_rho
                          << std::setw(9)  << std::fixed << std::setprecision(4) << rec.avg_C
                          << std::setw(9)  << std::fixed << std::setprecision(5) << rec.rms_force
                          << std::setw(9)  << std::fixed << std::setprecision(4) << rec.gamma
                          << std::setw(9)  << std::fixed << std::setprecision(4) << rec.data_quality
                          << std::setw(10) << std::fixed << std::setprecision(4) << rec.compactness
                          << std::setw(10) << std::fixed << std::setprecision(1) << rec.elapsed_ms
                          << "\n";
                std::cout.flush();
            }

            // ── Per-batch visual card: one popup per element per cycle ──────
            if (n_probes > 0) {
                int batch_start = static_cast<int>(accumulated.size()) - n_probes;
                double sum_g = 0, sum_q = 0, sum_c = 0;
                for (int k = batch_start; k < static_cast<int>(accumulated.size()); ++k) {
                    sum_g += accumulated[k].gamma;
                    sum_q += accumulated[k].data_quality;
                    sum_c += accumulated[k].compactness;
                }
                double mg = sum_g / n_probes;
                double mq = sum_q / n_probes;
                double mc = sum_c / n_probes;
                std::string lat = v4::lattice_name(ref.structure);
                std::string cmd = "DISPLAY=:0 python3 tools/crystal_card_popup.py "
                                  + ref.symbol + " " + lat
                                  + " " + std::to_string(mg)
                                  + " " + std::to_string(mq)
                                  + " " + std::to_string(mc)
                                  + " &";
                std::system(cmd.c_str());
            }
        }

        // Periodic correlation report
        if (cycle % REPORT_INTERVAL == 0 && accumulated.size() >= 11) {
            std::cout << "\n";
            auto mat = v4::compute_correlation_matrix(accumulated);

            std::cout << "── Correlation Update (cycle " << cycle
                      << ", N=" << accumulated.size() << ") ──────────────\n";
            auto preds = v4::predictors_of(mat, v4::CorrVar::C_COMPACT);
            std::cout << "  C_compact top predictors:\n";
            for (size_t k = 0; k < std::min<size_t>(4, preds.size()); ++k) {
                const auto& [vi, vj, r] = preds[k];
                std::cout << "    " << std::setw(14) << v4::corr_var_name(vj)
                          << " r=" << std::fixed << std::setprecision(4) << r << "\n";
            }

            // Export current HTML heatmap
            {
                std::string html = v4::generate_html_heatmap(mat);
                std::ofstream hf("v4_live_heatmap.html");
                if (hf.is_open()) hf << html;
            }
            // Export CSV snapshot
            {
                std::ofstream csv("v4_live_correlation.csv");
                if (csv.is_open()) v4::export_csv(csv, mat);
            }

            std::cout << "  [FILES] v4_live_heatmap.html + v4_live_correlation.csv updated\n\n";
            print_header();  // Re-print column headers after the report
        }
    }

    // ── Final Report ────────────────────────────────────────────────────────
    std::cout << "\n═══════════════════════════════════════════════════════════════\n";
    std::cout << "  CORE STOPPED — Total runs: " << total_runs
              << "  |  Cycles: " << cycle << "\n";

    if (accumulated.size() >= 11) {
        auto mat = v4::compute_correlation_matrix(accumulated);
        v4::print_ranked_report(std::cout, mat);

        // Final heatmap + CSV
        std::ofstream hf("v4_final_heatmap.html");
        if (hf.is_open()) hf << v4::generate_html_heatmap(mat);

        std::ofstream cf("v4_final_correlation.csv");
        if (cf.is_open()) v4::export_csv(cf, mat);

        std::cout << "\n  [FILES] v4_final_heatmap.html + v4_final_correlation.csv\n";
    }

    std::cout << "═══════════════════════════════════════════════════════════════\n";
    return 0;
}
