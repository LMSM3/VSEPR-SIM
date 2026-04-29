/**
 * run_report_automation.cpp — Unified Report Automation Runner
 *
 * Exercises ALL user-side report functions in a single deterministic run:
 *
 *   1. bead_fire_report_md()           → Markdown string
 *   2. write_bead_fire_report()        → .md file
 *   3. bead_fire_summary()             → console summary
 *   4. write_bead_fire_csv()           → .csv trajectory file
 *   5. fire_report_md()                → atomistic FIRE report
 *   6. export_excel_xml()              → Excel XML spreadsheet
 *   7. export_solidworks_sldcrv()      → SolidWorks curve file
 *   8. SnapshotGraphCollector exports  → timeseries + snapshot CSVs
 *   9. assemble_seed_bead_report()     → full seed-bead Markdown
 *
 * Usage:
 *   ./run_report_automation [output_directory]
 *
 * All outputs are written to output_directory/ (default: ./report_outputs/).
 * Every file written is logged to stdout for traceability.
 *
 * Anti-black-box: every report function is called with explicit synthetic data.
 * Deterministic: same binary → identical outputs.
 *
 * Reference: .github/copilot-instructions.md §2, §5, §9
 */

#include "coarse_grain/report/bead_fire_report.hpp"
#include "coarse_grain/report/bead_fire_csv_export.hpp"
#include "coarse_grain/report/mapping_report.hpp"
#include "coarse_grain/report/snapshot_graph.hpp"
#include "atomistic/report/report_md.hpp"

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// ============================================================================
// Synthetic Data Builders
// ============================================================================

/**
 * build_synthetic_fire_result — 10,000-iteration multi-cycle FIRE trajectory.
 *
 * Generates a long-run FIRE minimization with many convergence cycles:
 *   - 10,000 total iterations
 *   - Frames recorded at ~1 Hz (every 10-12 steps)
 *   - 8 beads with evolving positions per frame
 *   - Multiple perturbation/reconvergence cycles (thermal kicks)
 *   - Deterministic: same binary → identical trajectory
 *
 * Physics model:
 *   Each cycle starts with a perturbation (lattice thermal kick) that
 *   disrupts the converged state, followed by FIRE relaxation back to
 *   a (slightly shifted) minimum.  This models annealing / sampling.
 *
 * Frame rate:  ~1 Hz → record every FRAME_STRIDE steps.
 *              With FRAME_STRIDE=10 and 10,000 iterations → 1,000 frames.
 *
 * Crystal lattice context:
 *   Bead positions are placed on an FCC-like ring in Angstrom.
 *   Perturbations couple Z-oscillation to bead angle, simulating
 *   anisotropic thermal vibration of lattice sites.
 */
static coarse_grain::BeadFIREResult build_synthetic_fire_result() {
    constexpr int   TOTAL_ITERS   = 10000;
    constexpr int   FRAME_STRIDE  = 10;    // ~1 Hz: record every 10-12 steps
    constexpr int   N_BEADS       = 8;
    constexpr int   CYCLE_LEN     = 500;   // steps per convergence cycle
    constexpr double KICK_FRAC    = 0.15;  // fraction of cycle used for perturbation onset

    coarse_grain::BeadFIREResult r;
    r.converged      = true;
    r.steps_taken    = TOTAL_ITERS;
    r.U_final        = -98.4523;
    r.Frms_final     = 9.0e-5;
    r.Fmax_final     = 3.1e-4;
    r.alpha_final    = 0.022;
    r.dt_final       = 8.7;
    r.U_steric       = -39.781;
    r.U_electrostatic= -44.123;
    r.U_dispersion   = -14.548;
    r.record_history = true;

    r.history.reserve(TOTAL_ITERS / FRAME_STRIDE + 1);

    double U_prev = -20.0;

    for (int i = 0; i < TOTAL_ITERS; ++i) {
        // ── Cycle-local coordinate ──
        int    cycle      = i / CYCLE_LEN;
        int    local      = i % CYCLE_LEN;
        double t_local    = static_cast<double>(local);
        double t_global   = static_cast<double>(i);

        // Deep-well offset drifts slightly per cycle (annealing)
        double well_shift = -2.0 * cycle;

        // Perturbation envelope at cycle start: Gaussian kick
        double kick = 0.0;
        double kick_sigma = KICK_FRAC * CYCLE_LEN;
        if (t_local < kick_sigma * 3.0) {
            kick = std::exp(-0.5 * (t_local / kick_sigma) * (t_local / kick_sigma));
        }

        // ── Energy (smooth decay per cycle + kick) ──
        double decay_tau = 40.0 + 5.0 * (cycle % 4);  // vary per cycle
        double U_base = -20.0 + well_shift
                        - 80.0 * (1.0 - std::exp(-t_local / decay_tau));
        double U_kick = 15.0 * kick;

        double U_total = U_base + U_kick;
        double U_ster  = 0.40 * U_total;
        double U_elec  = 0.45 * U_total;
        double U_disp  = 0.15 * U_total;

        // ── Forces (exponential decay + kick spike) ──
        double Frms = 5.0 * std::exp(-t_local / 30.0) + 8.0 * kick + 1e-4;
        double Fmax = 2.5 * Frms + 3e-4;

        // ── FIRE parameters ──
        double alpha = 0.1 * std::exp(-t_local / 60.0) + 0.05 * kick;
        double dt    = 1.0 + 9.0 * (1.0 - std::exp(-t_local / 50.0));
        if (kick > 0.01) dt *= (1.0 - 0.7 * kick);  // dt cuts on perturbation

        double dU_per_bead = (U_total - U_prev) / N_BEADS;
        U_prev = U_total;

        // ── Frame sampling at ~1 Hz ──
        // Record every FRAME_STRIDE steps, plus first and last
        bool record_frame = (i % FRAME_STRIDE == 0) || (i == TOTAL_ITERS - 1);
        if (!record_frame) continue;

        coarse_grain::BeadFIREStep step;
        step.step             = i;
        step.U_total          = U_total;
        step.U_steric         = U_ster;
        step.U_electrostatic  = U_elec;
        step.U_dispersion     = U_disp;
        step.Frms             = Frms;
        step.Fmax             = Fmax;
        step.alpha            = alpha;
        step.dt               = dt;
        step.dU_per_bead      = dU_per_bead;

        // ── Bead positions (FCC-ring in Angstrom) ──
        for (int b = 0; b < N_BEADS; ++b) {
            double angle  = 2.0 * M_PI * b / N_BEADS;
            double radius = 5.0 - 0.5 * (1.0 - std::exp(-t_local / 40.0))
                          + 0.3 * kick * std::sin(b * 1.1 + cycle * 0.7);

            atomistic::Vec3 pos;
            pos.x = radius * std::cos(angle)
                   + 0.01 * std::sin(t_global * 0.1 + b)
                   + 0.08 * kick * std::cos(b * 2.3 + cycle);
            pos.y = radius * std::sin(angle)
                   + 0.01 * std::cos(t_global * 0.1 + b)
                   + 0.08 * kick * std::sin(b * 2.3 + cycle);
            pos.z = 0.5 * std::cos(2.0 * angle) * std::exp(-t_local / 80.0)
                   + 0.2 * kick * std::sin(b * 0.9 + cycle * 1.3);
            step.positions.push_back(pos);
        }

        r.history.push_back(std::move(step));
    }

    return r;
}

static std::vector<coarse_grain::Bead> build_synthetic_beads(int n = 8) {
    std::vector<coarse_grain::Bead> beads;
    for (int i = 0; i < n; ++i) {
        coarse_grain::Bead b;
        double angle = 2.0 * M_PI * i / n;
        b.position.x = 4.5 * std::cos(angle);
        b.position.y = 4.5 * std::sin(angle);
        b.position.z = 0.0;
        beads.push_back(b);
    }
    return beads;
}

static atomistic::State build_synthetic_atomistic_state() {
    atomistic::State s;
    s.N = 8;
    s.E.UvdW  = -35.4;
    s.E.UCoul = -12.8;
    return s;
}

static atomistic::FIREStats build_synthetic_fire_stats() {
    atomistic::FIREStats st;
    st.U           = -48.2;
    st.Frms        = 0.0002;
    st.dU_per_atom = 1.2e-7;
    st.alpha       = 0.03;
    st.dt          = 7.5;
    return st;
}

static coarse_grain::SnapshotGraphCollector build_synthetic_collector() {
    coarse_grain::SnapshotGraphCollector collector;
    collector.snapshot_interval = 10;

    for (uint64_t s = 0; s < 300; ++s) {
        double t = static_cast<double>(s);

        // Build a synthetic step record
        coarse_grain::SeedBeadStepRecord rec;
        rec.step_index    = s;
        rec.total_energy  = -50.0 * (1.0 - std::exp(-t / 60.0));
        rec.rms_force     = 5.0 * std::exp(-t / 40.0) + 0.005;
        rec.max_force     = rec.rms_force * 2.5;
        rec.avg_eta       = 0.6 * (1.0 - std::exp(-t / 100.0));
        rec.avg_rho       = 3.0 + 2.0 * (1.0 - std::exp(-t / 80.0));
        rec.avg_C         = 6.0 + 3.0 * (1.0 - std::exp(-t / 70.0));
        rec.avg_P2        = 0.4 * (1.0 - std::exp(-t / 120.0));
        rec.avg_target_f  = 0.5 * (1.0 - std::exp(-t / 90.0));
        rec.max_delta_eta = 0.02 * std::exp(-t / 50.0);
        rec.dt_current    = 1.0 + 8.0 * (1.0 - std::exp(-t / 60.0));
        rec.avg_g_steric  = 1.0 + 0.2 * (1.0 - std::exp(-t / 80.0));
        rec.avg_g_elec    = 1.0 - 0.15 * (1.0 - std::exp(-t / 80.0));
        rec.avg_g_disp    = 1.0 + 0.3 * (1.0 - std::exp(-t / 80.0));
        rec.kinetic_energy= 5.0 * std::exp(-t / 50.0);
        rec.steady_state  = (s == 299);

        // Synthetic bead positions for snapshots
        for (int b = 0; b < 8; ++b) {
            double angle = 2.0 * M_PI * b / 8.0;
            double radius = 5.0 - 0.3 * (1.0 - std::exp(-t / 60.0));
            atomistic::Vec3 pos;
            pos.x = radius * std::cos(angle);
            pos.y = radius * std::sin(angle);
            pos.z = 0.2 * std::cos(2.0 * angle) * std::exp(-t / 100.0);
            rec.bead_positions.push_back(pos);
            rec.bead_eta.push_back(rec.avg_eta + 0.01 * b);
            rec.bead_rho.push_back(rec.avg_rho + 0.05 * b);
        }

        collector.record(rec);
    }

    return collector;
}

// ============================================================================
// Report Runner
// ============================================================================

static int run_all_reports(const std::string& out_dir) {
    int pass = 0;
    int fail = 0;

    auto ok = [&](const std::string& label, bool success, const std::string& path = "") {
        if (success) {
            ++pass;
            std::cout << "[✓] " << label;
            if (!path.empty()) std::cout << " → " << path;
            std::cout << "\n";
        } else {
            ++fail;
            std::cerr << "[✗] " << label;
            if (!path.empty()) std::cerr << " → " << path;
            std::cerr << "\n";
        }
    };

    // Create output directory
    fs::create_directories(out_dir);

    // ====================================================================
    // 1. BeadFIRE Markdown report (string)
    // ====================================================================
    {
        auto result = build_synthetic_fire_result();
        coarse_grain::BeadFIREParams params;
        std::string report = coarse_grain::bead_fire_report_md(result, 8, params);
        ok("bead_fire_report_md()", !report.empty());
    }

    // ====================================================================
    // 2. BeadFIRE report → file
    // ====================================================================
    {
        auto result = build_synthetic_fire_result();
        coarse_grain::BeadFIREParams params;
        std::string path = out_dir + "/bead_fire_report.md";
        bool s = coarse_grain::write_bead_fire_report(path, result, 8, params);
        ok("write_bead_fire_report()", s, path);
    }

    // ====================================================================
    // 3. BeadFIRE console summary
    // ====================================================================
    {
        auto result = build_synthetic_fire_result();
        std::string summary = coarse_grain::bead_fire_summary(result);
        ok("bead_fire_summary()", !summary.empty());
        std::cout << "     Summary: " << summary << "\n";
    }

    // ====================================================================
    // 4. BeadFIRE CSV trajectory export
    // ====================================================================
    {
        auto result = build_synthetic_fire_result();
        auto beads  = build_synthetic_beads(8);
        std::string path = out_dir + "/bead_fire_trajectory.csv";
        bool s = coarse_grain::write_bead_fire_csv(path, result, beads);
        ok("write_bead_fire_csv()", s, path);
    }

    // ====================================================================
    // 5. Atomistic FIRE report (Markdown string)
    // ====================================================================
    {
        auto state = build_synthetic_atomistic_state();
        auto stats = build_synthetic_fire_stats();
        std::string report = atomistic::fire_report_md(state, stats);
        ok("fire_report_md() [atomistic]", !report.empty());

        // Write to file for inspection
        std::string path = out_dir + "/atomistic_fire_report.md";
        std::ofstream f(path);
        if (f.is_open()) { f << report; f.close(); }
        ok("atomistic FIRE report → file", f.good(), path);
    }

    // ====================================================================
    // 6. SnapshotGraphCollector — timeseries CSV
    // ====================================================================
    {
        auto collector = build_synthetic_collector();
        std::string path = out_dir + "/timeseries.csv";
        bool s = collector.export_timeseries_csv(path);
        ok("export_timeseries_csv()", s, path);
    }

    // ====================================================================
    // 7. SnapshotGraphCollector — snapshot CSV
    // ====================================================================
    {
        auto collector = build_synthetic_collector();
        std::string path = out_dir + "/snapshots.csv";
        bool s = collector.export_snapshots_csv(path);
        ok("export_snapshots_csv()", s, path);
    }

    // ====================================================================
    // 8. Excel XML spreadsheet
    // ====================================================================
    {
        auto collector = build_synthetic_collector();
        std::string path = out_dir + "/report.xml";
        bool s = coarse_grain::export_excel_xml(path, collector, "Automation Report");
        ok("export_excel_xml()", s, path);
    }

    // ====================================================================
    // 9. SolidWorks .sldcrv curve export
    // ====================================================================
    {
        auto collector = build_synthetic_collector();
        std::string path = out_dir + "/final_positions.sldcrv";
        bool s = coarse_grain::export_solidworks_sldcrv(path, collector);
        ok("export_solidworks_sldcrv()", s, path);
    }

    // ====================================================================
    // 10. SolidWorks XYZ point cloud
    // ====================================================================
    {
        auto collector = build_synthetic_collector();
        std::string path = out_dir + "/final_positions.xyz";
        bool s = coarse_grain::export_solidworks_xyz(path, collector, "Report Automation");
        ok("export_solidworks_xyz()", s, path);
    }

    // ====================================================================
    // 11. SolidWorks trajectory curves (per-bead)
    // ====================================================================
    {
        auto collector = build_synthetic_collector();
        std::string traj_dir = out_dir + "/solidworks_trajectories";
        fs::create_directories(traj_dir);
        bool s = coarse_grain::export_solidworks_trajectories(traj_dir, collector);
        ok("export_solidworks_trajectories()", s, traj_dir);
    }

    // ====================================================================
    // Summary
    // ====================================================================
    std::cout << "\n";
    std::cout << "═══════════════════════════════════════════════════════\n";
    std::cout << "  Report Automation Complete\n";
    std::cout << "  Passed: " << pass << "  Failed: " << fail << "\n";
    std::cout << "  Output: " << out_dir << "/\n";
    std::cout << "═══════════════════════════════════════════════════════\n";

    return (fail == 0) ? 0 : 1;
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[]) {
    std::cout << "╔═══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  VSEPR-SIM Report Automation Runner                          ║\n";
    std::cout << "║  Running all user-side report functions                       ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════════╝\n\n";

    std::string out_dir = (argc > 1) ? argv[1] : "report_outputs";

    return run_all_reports(out_dir);
}
