#pragma once
/**
 * streamline_export.hpp
 *
 * Electrohydrodynamic Simulation — Stage 5: Postprocessing
 *
 * Exports velocity-field streamline seeds and traced paths to CSV.
 * Uses simple Euler integration on the discrete velocity grid.
 */

#include "sim/ehd/physics/flow_model.hpp"
#include "sim/ehd/ehd_types.hpp"
#include <vector>
#include <fstream>
#include <iomanip>
#include <cmath>

namespace vsepr {
namespace ehd {
namespace post {

struct StreamlinePoint {
    double r, z;
    double velocity_mag;
};

using Streamline = std::vector<StreamlinePoint>;

/**
 * Trace a single streamline starting at (r0, z0) through the velocity field.
 * Uses forward-Euler integration with adaptive step.
 */
inline Streamline trace_streamline(
    const physics::FlowField& field,
    double r0, double z0,
    int max_steps = 2000,
    double step_fraction = 0.3)
{
    Streamline sl;
    double r = r0, z = z0;

    double R_max = field.nr * field.dr;
    double Z_max = field.nz * field.dz;

    for (int s = 0; s < max_steps; ++s) {
        if (r < 0 || r >= R_max || z < 0 || z >= Z_max) break;

        int ir = static_cast<int>(r / field.dr);
        int iz = static_cast<int>(z / field.dz);
        ir = std::min(ir, field.nr - 1);
        iz = std::min(iz, field.nz - 1);

        const auto& cell = field.at(ir, iz);
        double vmag = cell.velocity.norm();
        if (vmag < 1e-15) break;

        sl.push_back({r, z, vmag});

        // Step size proportional to cell size
        double ds = std::min(field.dr, field.dz) * step_fraction;

        // Euler step (axisymmetric: velocity.x = u_r, velocity.z = u_z)
        r += cell.velocity.x * (ds / vmag);
        z += cell.velocity.z * (ds / vmag);
    }

    return sl;
}

/**
 * Generate a set of streamlines seeded at the inlet across radial positions.
 */
inline std::vector<Streamline> generate_inlet_streamlines(
    const physics::FlowField& field,
    int num_seeds = 10)
{
    std::vector<Streamline> lines;
    double R_max = field.nr * field.dr;

    for (int i = 0; i < num_seeds; ++i) {
        double r0 = R_max * (i + 0.5) / num_seeds;
        double z0 = field.dz * 0.5;
        lines.push_back(trace_streamline(field, r0, z0));
    }
    return lines;
}

/**
 * Export streamlines to CSV.
 * Columns: streamline_id, r, z, velocity_mag
 */
inline bool export_streamlines_csv(
    const std::vector<Streamline>& lines,
    const std::string& filepath)
{
    std::ofstream out(filepath);
    if (!out.is_open()) return false;

    out << std::scientific << std::setprecision(8);
    out << "streamline_id,r,z,velocity_mag\n";

    for (size_t i = 0; i < lines.size(); ++i) {
        for (const auto& pt : lines[i]) {
            out << i << "," << pt.r << "," << pt.z << ","
                << pt.velocity_mag << "\n";
        }
    }
    return true;
}

} // namespace post
} // namespace ehd
} // namespace vsepr
