#pragma once
/**
 * snapshot_graph.hpp — Snapshot & Graph Capture for Automatic Report Generation
 *
 * Provides frame-level snapshot capture and time-series graph data collection
 * during live simulation. Generates CSV data files and Markdown report fragments
 * suitable for inclusion in automated research reports.
 *
 * Components:
 *   SnapshotCapture  — captures bead positions/states at defined intervals
 *   GraphSeries      — accumulates scalar time-series (energy, force, η, etc.)
 *   ReportAssembler  — generates final Markdown report from captured data
 *
 * Anti-black-box: every captured value is traceable to a step index.
 * Deterministic: same simulation → identical report.
 *
 * Reference: copilot-instructions.md §9.1 (report-ready outputs)
 */

#include "coarse_grain/models/seed_bead_stepper.hpp"
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace coarse_grain {

// ============================================================================
// Graph Series — time-series accumulator
// ============================================================================

struct GraphPoint {
    uint64_t step{};
    double value{};
};

struct GraphSeries {
    std::string name;
    std::string unit;
    std::vector<GraphPoint> points;

    void add(uint64_t step, double val) {
        points.push_back({step, val});
    }

    double min_val() const {
        double m = 1e30;
        for (auto& p : points) if (p.value < m) m = p.value;
        return m;
    }

    double max_val() const {
        double m = -1e30;
        for (auto& p : points) if (p.value > m) m = p.value;
        return m;
    }

    double final_val() const {
        return points.empty() ? 0.0 : points.back().value;
    }
};

// ============================================================================
// Frame Snapshot (lightweight, for viewer consumption)
// ============================================================================

struct BeadFrameSnapshot {
    uint64_t step_index{};
    double time_fs{};
    std::vector<atomistic::Vec3> positions;
    std::vector<double> eta_values;
    std::vector<double> rho_values;
    std::vector<double> P2_values;
    double total_energy{};
    double rms_force{};
    bool is_steady_state{};
};

// ============================================================================
// Snapshot & Graph Collector
// ============================================================================

/**
 * SnapshotGraphCollector — accumulates diagnostics from step records
 * for automatic report generation.
 */
class SnapshotGraphCollector {
public:
    // Graph channels (always populated)
    GraphSeries energy_series{"Energy", "kcal/mol"};
    GraphSeries rms_force_series{"RMS Force", "kcal/(mol·Å)"};
    GraphSeries max_force_series{"Max Force", "kcal/(mol·Å)"};
    GraphSeries avg_eta_series{"Mean η", ""};
    GraphSeries avg_rho_series{"Mean ρ", ""};
    GraphSeries avg_C_series{"Mean C", ""};
    GraphSeries avg_P2_series{"Mean P₂", ""};
    GraphSeries avg_target_f_series{"Mean f", ""};
    GraphSeries max_deta_series{"Max |Δη|", ""};
    GraphSeries dt_series{"Timestep", "fs"};
    GraphSeries g_steric_series{"g_steric", ""};
    GraphSeries g_elec_series{"g_elec", ""};
    GraphSeries g_disp_series{"g_disp", ""};
    GraphSeries ke_series{"Kinetic Energy", "kcal/mol"};

    // Frame snapshots (at configurable intervals)
    std::vector<BeadFrameSnapshot> snapshots;
    uint64_t snapshot_interval{10};

    /**
     * Ingest a step record from the stepper.
     */
    void record(const SeedBeadStepRecord& rec) {
        uint64_t s = rec.step_index;

        energy_series.add(s, rec.total_energy);
        rms_force_series.add(s, rec.rms_force);
        max_force_series.add(s, rec.max_force);
        avg_eta_series.add(s, rec.avg_eta);
        avg_rho_series.add(s, rec.avg_rho);
        avg_C_series.add(s, rec.avg_C);
        avg_P2_series.add(s, rec.avg_P2);
        avg_target_f_series.add(s, rec.avg_target_f);
        max_deta_series.add(s, rec.max_delta_eta);
        dt_series.add(s, rec.dt_current);
        g_steric_series.add(s, rec.avg_g_steric);
        g_elec_series.add(s, rec.avg_g_elec);
        g_disp_series.add(s, rec.avg_g_disp);
        ke_series.add(s, rec.kinetic_energy);

        // Capture snapshot if interval matches or steady state reached
        if (s % snapshot_interval == 0 || rec.steady_state) {
            if (!rec.bead_positions.empty()) {
                BeadFrameSnapshot snap;
                snap.step_index = s;
                snap.positions = rec.bead_positions;
                snap.eta_values = rec.bead_eta;
                snap.rho_values = rec.bead_rho;
                snap.total_energy = rec.total_energy;
                snap.rms_force = rec.rms_force;
                snap.is_steady_state = rec.steady_state;
                snapshots.push_back(std::move(snap));
            }
        }
    }

    // ========================================================================
    // CSV Export
    // ========================================================================

    /**
     * Export all graph series to a single CSV file.
     */
    bool export_timeseries_csv(const std::string& path) const {
        std::ofstream out(path);
        if (!out.is_open()) return false;

        out << "step,energy,rms_force,max_force,avg_eta,avg_rho,avg_C,"
            << "avg_P2,avg_target_f,max_delta_eta,dt,g_steric,g_elec,g_disp,ke\n";

        size_t n = energy_series.points.size();
        for (size_t i = 0; i < n; ++i) {
            out << std::fixed << std::setprecision(8);
            out << energy_series.points[i].step << ","
                << energy_series.points[i].value << ","
                << rms_force_series.points[i].value << ","
                << max_force_series.points[i].value << ","
                << avg_eta_series.points[i].value << ","
                << avg_rho_series.points[i].value << ","
                << avg_C_series.points[i].value << ","
                << avg_P2_series.points[i].value << ","
                << avg_target_f_series.points[i].value << ","
                << max_deta_series.points[i].value << ","
                << dt_series.points[i].value << ","
                << g_steric_series.points[i].value << ","
                << g_elec_series.points[i].value << ","
                << g_disp_series.points[i].value << ","
                << ke_series.points[i].value << "\n";
        }
        return true;
    }

    /**
     * Export snapshot positions to a trajectory CSV.
     */
    bool export_snapshots_csv(const std::string& path) const {
        std::ofstream out(path);
        if (!out.is_open()) return false;

        out << "step,bead_id,x,y,z,eta,rho\n";
        for (const auto& snap : snapshots) {
            for (size_t i = 0; i < snap.positions.size(); ++i) {
                out << std::fixed << std::setprecision(6);
                out << snap.step_index << ","
                    << i << ","
                    << snap.positions[i].x << ","
                    << snap.positions[i].y << ","
                    << snap.positions[i].z << ","
                    << (i < snap.eta_values.size() ? snap.eta_values[i] : 0.0) << ","
                    << (i < snap.rho_values.size() ? snap.rho_values[i] : 0.0) << "\n";
            }
        }
        return true;
    }

    // ========================================================================
    // Excel XML Export
    // ========================================================================

    /**
     * Export to Excel-compatible XML spreadsheet (SpreadsheetML 2003).
     * Opens directly in Microsoft Excel and LibreOffice Calc.
     */
    bool export_excel(const std::string& path,
                      const std::string& title = "Seed & Bead Report") const;

    // ========================================================================
    // SolidWorks Export
    // ========================================================================

    /**
     * Export final-frame positions as SolidWorks .sldcrv file.
     * Import path: Insert > Curve > Curve Through XYZ Points
     */
    bool export_solidworks_curve(const std::string& path) const;

    /**
     * Export final-frame positions as .xyz point cloud.
     */
    bool export_solidworks_pointcloud(const std::string& path,
                                       const std::string& title = "VSEPR-SIM") const;

    /**
     * Export final-frame positions as CSV with metadata.
     */
    bool export_solidworks_metadata_csv(const std::string& path) const;
};

} // namespace coarse_grain

// ============================================================================
// Include export modules now that SnapshotGraphCollector is fully defined
// (Included OUTSIDE namespace to avoid double-nesting)
// ============================================================================

#include "coarse_grain/report/excel_export.hpp"
#include "coarse_grain/report/solidworks_export.hpp"

namespace coarse_grain {

// ============================================================================
// Inline implementations — Excel & SolidWorks convenience methods
// ============================================================================

inline bool SnapshotGraphCollector::export_excel(
    const std::string& path, const std::string& title) const
{
    return export_excel_xml(path, *this, title);
}

inline bool SnapshotGraphCollector::export_solidworks_curve(
    const std::string& path) const
{
    return export_solidworks_sldcrv(path, *this);
}

inline bool SnapshotGraphCollector::export_solidworks_pointcloud(
    const std::string& path, const std::string& title) const
{
    return export_solidworks_xyz(path, *this, title);
}

inline bool SnapshotGraphCollector::export_solidworks_metadata_csv(
    const std::string& path) const
{
    return coarse_grain::export_solidworks_csv(path, *this);
}

// ============================================================================
// Report Assembler — Markdown report from collected data
// ============================================================================

/**
 * Generate a complete Markdown study report from collected data.
 */
inline std::string assemble_seed_bead_report(
    const std::string& title,
    const SeedBeadParams& params,
    const SeedBeadStepper::SeedBeadResult& result,
    const SnapshotGraphCollector& collector)
{
    std::ostringstream md;
    md << std::fixed;

    md << "# " << title << "\n\n";
    md << "**Generated by VSEPR-SIM Seed-and-Bead Stepper**\n\n";
    md << "---\n\n";

    // Outcome
    md << "## 1. Outcome\n\n";
    md << "| Metric | Value |\n";
    md << "|--------|-------|\n";
    md << "| Converged | " << (result.converged ? "**YES**" : "NO") << " |\n";
    md << std::setprecision(0);
    md << "| Steps taken | " << result.steps_taken << " |\n";
    md << std::setprecision(6);
    md << "| Final energy | " << result.final_energy << " kcal/mol |\n";
    md << "| Final RMS force | " << result.final_rms_force << " kcal/(mol·Å) |\n";
    md << "| Final mean η | " << result.final_avg_eta << " |\n";
    md << "\n";

    // Parameters
    md << "## 2. Parameters\n\n";
    md << "### 2.1 SEED Parameters\n\n";
    md << "| Parameter | Value |\n";
    md << "|-----------|-------|\n";
    md << std::setprecision(4);
    md << "| dt_initial | " << params.dt_initial << " fs |\n";
    md << "| dt_max | " << params.dt_max << " fs |\n";
    md << "| F tolerance | " << params.f_tol << " kcal/(mol·Å) |\n";
    md << "| E tolerance | " << params.e_tol << " |\n";
    md << "| FIRE α₀ | " << params.fire_alpha_start << " |\n";
    md << std::setprecision(0);
    md << "| Max steps | " << params.max_steps << " |\n";
    md << "\n";

    md << "### 2.2 BEAD Parameters (6 Environment Coupling)\n\n";
    md << "| Parameter | Symbol | Value |\n";
    md << "|-----------|--------|-------|\n";
    md << std::setprecision(4);
    md << "| Density weight | α | " << params.env_params.alpha << " |\n";
    md << "| Orientation weight | β | " << params.env_params.beta << " |\n";
    md << "| Relaxation time | τ | " << params.env_params.tau << " fs |\n";
    md << "| Steric modulation | γ_steric | " << params.env_params.gamma_steric << " |\n";
    md << "| Electrostatic modulation | γ_elec | " << params.env_params.gamma_elec << " |\n";
    md << "| Dispersion modulation | γ_disp | " << params.env_params.gamma_disp << " |\n";
    md << "\n";

    // Energy summary
    md << "## 3. Energy Convergence\n\n";
    md << std::setprecision(6);
    md << "| | Value |\n";
    md << "|---|---|\n";
    md << "| Initial energy | " << collector.energy_series.points.front().value << " kcal/mol |\n";
    md << "| Final energy | " << collector.energy_series.final_val() << " kcal/mol |\n";
    md << "| Min energy | " << collector.energy_series.min_val() << " kcal/mol |\n";
    md << "\n";

    // Environment state
    md << "## 4. Environment-Responsive State\n\n";
    md << "| Observable | Final Mean |\n";
    md << "|------------|------------|\n";
    md << std::setprecision(4);
    md << "| ρ (density) | " << collector.avg_rho_series.final_val() << " |\n";
    md << "| C (coordination) | " << collector.avg_C_series.final_val() << " |\n";
    md << "| P₂ (orientation) | " << collector.avg_P2_series.final_val() << " |\n";
    md << "| η (slow state) | " << collector.avg_eta_series.final_val() << " |\n";
    md << "| f (target) | " << collector.avg_target_f_series.final_val() << " |\n";
    md << "\n";

    // Kernel modulation
    md << "## 5. Kernel Modulation\n\n";
    md << "| Channel | Final ⟨g⟩ | Physical Effect |\n";
    md << "|---------|-----------|------------------|\n";
    md << "| Steric | " << collector.g_steric_series.final_val()
       << " | Hardening under compression |\n";
    md << "| Electrostatic | " << collector.g_elec_series.final_val()
       << " | Screening under crowding |\n";
    md << "| Dispersion | " << collector.g_disp_series.final_val()
       << " | Enhancement under ordering |\n";
    md << "\n";

    // Mathematical model
    md << "## 6. Mathematical Model\n\n";
    md << "### 6+9 Steady-State Step Function\n\n";
    md << "**SEED Layer (6 units):**\n\n";
    md << "$$\\mathbf{F}_i = -\\nabla_i U_{\\text{LJ}}^{\\text{mod}}$$\n\n";
    md << "$$\\mathbf{v}_i \\leftarrow (1 - \\alpha)\\mathbf{v}_i + \\alpha |\\mathbf{v}| \\hat{F}_i$$\n\n";
    md << "$$\\mathbf{r}_i(t + \\Delta t) = \\mathbf{r}_i(t) + \\Delta t \\, \\mathbf{v}_i$$\n\n";
    md << "**BEAD Layer (9 units):**\n\n";
    md << "$$\\rho_B = \\sum_{C \\neq B} w(r_{BC})$$\n\n";
    md << "$$f = \\alpha \\hat{\\rho} + \\beta \\hat{P}_2$$\n\n";
    md << "$$\\eta(t + \\Delta t) = f + (\\eta(t) - f) e^{-\\Delta t / \\tau}$$\n\n";
    md << "$$K_k(\\ell, r; \\eta_A, \\eta_B) = K_k(\\ell, r) \\cdot (1 + \\gamma_k \\bar{\\eta})$$\n\n";

    // Snapshot count & export formats
    md << "## 7. Data Artifacts\n\n";
    md << "| Artifact | Count / Format |\n";
    md << "|----------|----------------|\n";
    md << std::setprecision(0);
    md << "| Graph data points | " << collector.energy_series.points.size() << " |\n";
    md << "| Frame snapshots | " << collector.snapshots.size() << " |\n";
    md << "\n";

    md << "### 7.1 Export Formats\n\n";
    md << "| Format | Extension | Description |\n";
    md << "|--------|-----------|-------------|\n";
    md << "| CSV Timeseries | `.csv` | 15-column step-level diagnostics |\n";
    md << "| CSV Snapshots | `.csv` | Per-bead XYZ + η, ρ at snapshot intervals |\n";
    md << "| Excel XML | `.xml` | SpreadsheetML 2003 (opens in Excel/Calc) |\n";
    md << "| SolidWorks Curve | `.sldcrv` | Tab-delimited XYZ for Insert > Curve |\n";
    md << "| XYZ Point Cloud | `.xyz` | Standard atomistic point cloud |\n";
    md << "| SolidWorks CSV | `.csv` | Coordinates + η, ρ metadata |\n";
    md << "\n";

    md << "---\n";
    md << "*Report generated by VSEPR-SIM v2.8.0 — Deterministic atomistic platform*\n";

    return md.str();
}

/**
 * Write a complete report to file.
 */
inline bool write_seed_bead_report(
    const std::string& path,
    const std::string& title,
    const SeedBeadParams& params,
    const SeedBeadStepper::SeedBeadResult& result,
    const SnapshotGraphCollector& collector)
{
    std::ofstream out(path);
    if (!out.is_open()) return false;
    out << assemble_seed_bead_report(title, params, result, collector);
    return true;
}

} // namespace coarse_grain
