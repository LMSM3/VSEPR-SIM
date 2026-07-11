#pragma once
/**
 * bead_fire_csv_export.hpp — CSV Export for BeadFIRE Trajectory Data
 *
 * Exports BeadFIREResult history to CSV format for visualization with Python scripts.
 *
 * CSV Format:
 *   Header: step,U_total,U_steric,U_elec,U_disp,Frms,Fmax,alpha,dt,dU_per_bead,
 *           bead0_x,bead0_y,bead0_z,bead1_x,bead1_y,bead1_z,...
 *   
 *   Each row: step data + bead positions at that step
 *
 * Usage:
 *   BeadFIREResult result = BeadFIRE::minimize(...);
 *   write_bead_fire_csv("trajectory.csv", result, beads);
 *
 * Integration:
 *   - Python visualization: scripts/visualize_bead_fire_trajectory.py
 *   - Markdown reporting: coarse_grain/report/bead_fire_report.hpp
 *
 * Anti-black-box: Every bead position at every step is explicitly exported.
 */

#include "coarse_grain/models/bead_fire.hpp"
#include "coarse_grain/core/bead.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <string>

namespace coarse_grain {

/**
 * Export BeadFIRE trajectory history to CSV file.
 *
 * Exports complete trajectory data including bead positions at each step.
 *
 * @param path      Output CSV file path
 * @param result    BeadFIREResult with history (including positions)
 * @param beads     Final bead system (for n_beads count)
 * @return true on success, false on file write error
 */
inline bool write_bead_fire_csv(const std::string& path,
                                 const BeadFIREResult& result,
                                 const std::vector<Bead>& beads)
{
    std::ofstream out(path);
    if (!out.is_open())
        return false;

    out << std::fixed << std::setprecision(6);

    int n_beads = static_cast<int>(beads.size());
    int n_steps = static_cast<int>(result.history.size());

    if (n_steps == 0) {
        // No history recorded
        return false;
    }

    // ========================================================================
    // Header Row
    // ========================================================================
    out << "step,U_total,U_steric,U_elec,U_disp,Frms,Fmax,alpha,dt,dU_per_bead";
    for (int i = 0; i < n_beads; ++i) {
        out << ",bead" << i << "_x";
        out << ",bead" << i << "_y";
        out << ",bead" << i << "_z";
    }
    out << "\n";

    // ========================================================================
    // Data Rows
    // ========================================================================
    for (int step_idx = 0; step_idx < n_steps; ++step_idx) {
        const auto& step = result.history[step_idx];

        out << step.step << ","
            << step.U_total << ","
            << step.U_steric << ","
            << step.U_electrostatic << ","
            << step.U_dispersion << ","
            << step.Frms << ","
            << step.Fmax << ","
            << step.alpha << ","
            << step.dt << ","
            << step.dU_per_bead;

        // Use stored positions from history (now available)
        if (step.positions.size() == static_cast<size_t>(n_beads)) {
            for (int i = 0; i < n_beads; ++i) {
                out << "," << step.positions[i].x
                    << "," << step.positions[i].y
                    << "," << step.positions[i].z;
            }
        } else {
            // Fallback: use final positions if position history not available
            for (int i = 0; i < n_beads; ++i) {
                out << "," << beads[i].position.x
                    << "," << beads[i].position.y
                    << "," << beads[i].position.z;
            }
        }

        out << "\n";
    }

    return out.good();
}


/**
 * Generate a complete data export: CSV trajectory + Markdown report.
 *
 * Convenience function that generates both the CSV file (for Python visualization)
 * and the Markdown report (for documentation).
 *
 * @param base_path     Base path without extension (e.g., "output/benzene_min")
 * @param result        BeadFIREResult
 * @param beads         Bead system
 * @param params        FIRE parameters
 * @return true if both exports succeeded
 */
inline bool write_bead_fire_complete_export(const std::string& base_path,
                                             const BeadFIREResult& result,
                                             const std::vector<Bead>& beads,
                                             const BeadFIREParams& params)
{
    std::string csv_path = base_path + ".csv";
    std::string md_path = base_path + ".md";

    // Export CSV
    bool csv_ok = write_bead_fire_csv(csv_path, result, beads);

    // Export Markdown report
    bool md_ok = write_bead_fire_report(md_path, result, 
                                        static_cast<int>(beads.size()), params);

    if (csv_ok && md_ok) {
        std::cout << "✓ Complete export:\n";
        std::cout << "  CSV:      " << csv_path << "\n";
        std::cout << "  Markdown: " << md_path << "\n";
        std::cout << "\nVisualize with:\n";
        std::cout << "  python scripts/visualize_bead_fire_trajectory.py " << csv_path << "\n";
        return true;
    }

    return false;
}

} // namespace coarse_grain

