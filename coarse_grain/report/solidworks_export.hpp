#pragma once
/**
 * solidworks_export.hpp — SolidWorks-Compatible Data Export
 *
 * Exports bead positions and trajectories in formats that SolidWorks
 * can import directly:
 *
 *   1. .sldcrv — Curve Through XYZ Points
 *      SolidWorks path: Insert > Curve > Curve Through XYZ Points
 *      Format: tab-delimited X Y Z, one point per line
 *
 *   2. .xyz — Standard XYZ point cloud
 *      SolidWorks path: File > Open > set type to "All Files"
 *      Format: N atoms, comment, atom X Y Z lines
 *
 *   3. .csv — Coordinate CSV with metadata columns
 *      SolidWorks path: Insert > Curve > Curve Through XYZ Points
 *      (after converting columns, or via macro)
 *
 * Anti-black-box: coordinates are direct copies of simulation positions.
 * Deterministic: same snapshot → identical output files.
 *
 * Reference: copilot-instructions.md §9.1 (report-ready outputs)
 */

#include <cstdint>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace coarse_grain {

// ============================================================================
// SolidWorks Curve-Through-XYZ-Points (.sldcrv)
// NOTE: This header is included from snapshot_graph.hpp AFTER
// SnapshotGraphCollector is fully defined. Do not include standalone.
// ============================================================================

/**
 * Export the final-frame bead positions as a SolidWorks .sldcrv file.
 *
 * Format: tab-delimited X\tY\tZ per line (Angstroms).
 *
 * SolidWorks import:
 *   Insert > Curve > Curve Through XYZ Points > Browse > select file
 */
inline bool export_solidworks_sldcrv(
    const std::string& path,
    const SnapshotGraphCollector& collector)
{
    if (collector.snapshots.empty()) return false;

    const auto& snap = collector.snapshots.back();
    std::ofstream out(path);
    if (!out.is_open()) return false;

    out << std::fixed << std::setprecision(6);
    for (const auto& pos : snap.positions) {
        out << pos.x << "\t" << pos.y << "\t" << pos.z << "\n";
    }
    return true;
}

/**
 * Export per-bead trajectory curves as individual .sldcrv files.
 *
 * Each bead gets its own file: bead_000.sldcrv, bead_001.sldcrv, etc.
 * Each file contains the XYZ path of that bead across all snapshots.
 */
inline bool export_solidworks_trajectories(
    const std::string& directory,
    const SnapshotGraphCollector& collector)
{
    if (collector.snapshots.empty()) return false;

    size_t n_beads = collector.snapshots.front().positions.size();

    for (size_t bid = 0; bid < n_beads; ++bid) {
        std::ostringstream fname;
        fname << directory << "/bead_" << std::setw(3) << std::setfill('0')
              << bid << ".sldcrv";

        std::ofstream out(fname.str());
        if (!out.is_open()) continue;

        out << std::fixed << std::setprecision(6);
        for (const auto& snap : collector.snapshots) {
            if (bid < snap.positions.size()) {
                out << snap.positions[bid].x << "\t"
                    << snap.positions[bid].y << "\t"
                    << snap.positions[bid].z << "\n";
            }
        }
    }
    return true;
}

// ============================================================================
// XYZ Point Cloud (.xyz)
// ============================================================================

/**
 * Export the final-frame positions as a standard .xyz file.
 *
 * Format:
 *   Line 1: N (number of atoms/beads)
 *   Line 2: comment (simulation metadata)
 *   Lines 3..N+2: Element X Y Z
 */
inline bool export_solidworks_xyz(
    const std::string& path,
    const SnapshotGraphCollector& collector,
    const std::string& title = "VSEPR-SIM Seed & Bead")
{
    if (collector.snapshots.empty()) return false;

    const auto& snap = collector.snapshots.back();
    std::ofstream out(path);
    if (!out.is_open()) return false;

    out << snap.positions.size() << "\n";
    out << title << " — final structure at step " << snap.step_index
        << " | E=" << std::fixed << std::setprecision(4) << snap.total_energy
        << " kcal/mol | F_rms=" << snap.rms_force << "\n";

    out << std::setprecision(6);
    for (size_t i = 0; i < snap.positions.size(); ++i) {
        out << "Bead  " << std::setw(12) << snap.positions[i].x
            << "  " << std::setw(12) << snap.positions[i].y
            << "  " << std::setw(12) << snap.positions[i].z
            << "\n";
    }
    return true;
}

// ============================================================================
// SolidWorks-Compatible CSV (with metadata)
// ============================================================================

/**
 * Export final-frame positions with per-bead metadata as CSV.
 *
 * Columns: bead_id, X, Y, Z, eta, rho
 *
 * This file can be imported into SolidWorks via macro or used as input
 * for SolidWorks-compatible point cloud plugins.
 */
inline bool export_solidworks_csv(
    const std::string& path,
    const SnapshotGraphCollector& collector)
{
    if (collector.snapshots.empty()) return false;

    const auto& snap = collector.snapshots.back();
    std::ofstream out(path);
    if (!out.is_open()) return false;

    out << "bead_id,X,Y,Z,eta,rho\n";
    out << std::fixed << std::setprecision(6);

    for (size_t i = 0; i < snap.positions.size(); ++i) {
        out << i << ","
            << snap.positions[i].x << ","
            << snap.positions[i].y << ","
            << snap.positions[i].z << ","
            << (i < snap.eta_values.size() ? snap.eta_values[i] : 0.0) << ","
            << (i < snap.rho_values.size() ? snap.rho_values[i] : 0.0)
            << "\n";
    }
    return true;
}

// ============================================================================
// All-In-One Export
// ============================================================================

/**
 * Export all SolidWorks-compatible formats at once.
 *
 * Creates:
 *   <prefix>_structure.sldcrv   — final-frame curve points
 *   <prefix>_structure.xyz      — final-frame XYZ point cloud
 *   <prefix>_points.csv         — final-frame CSV with metadata
 *
 * Returns the number of files successfully written (0–3).
 */
inline int export_all_solidworks(
    const std::string& prefix,
    const SnapshotGraphCollector& collector,
    const std::string& title = "VSEPR-SIM Seed & Bead")
{
    int count = 0;
    if (export_solidworks_sldcrv(prefix + "_structure.sldcrv", collector))
        ++count;
    if (export_solidworks_xyz(prefix + "_structure.xyz", collector, title))
        ++count;
    if (export_solidworks_csv(prefix + "_points.csv", collector))
        ++count;
    return count;
}

} // namespace coarse_grain
