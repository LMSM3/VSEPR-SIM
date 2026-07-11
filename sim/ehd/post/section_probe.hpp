#pragma once
/**
 * section_probe.hpp
 *
 * Electrohydrodynamic Simulation — Stage 5: Postprocessing
 *
 * Extracts field quantities along axial or radial cross-sections.
 * Produces 1D profiles suitable for line plots (velocity profile, E(r), c(z), etc.).
 */

#include "sim/ehd/physics/coupled_solver.hpp"
#include "sim/ehd/post/contour_export.hpp"
#include <vector>
#include <fstream>
#include <iomanip>
#include <string>
#include <cmath>

namespace vsepr {
namespace ehd {
namespace post {

struct ProbePoint {
    double coordinate;  // r or z depending on probe direction
    double value;
};

using ProbeProfile = std::vector<ProbePoint>;

/**
 * Radial probe: extract a quantity along r at a fixed z-index.
 */
inline ProbeProfile radial_probe(
    const physics::CoupledSolver& solver,
    FieldQuantity quantity,
    int iz_index,
    int species_index = 0)
{
    ProbeProfile profile;
    const auto& ff = solver.flow_field();
    const auto& ef = solver.electric_field();

    int iz = std::min(iz_index, ff.nz - 1);

    for (int ir = 0; ir < ff.nr; ++ir) {
        double r = (ir + 0.5) * ff.dr;
        double val = 0.0;

        switch (quantity) {
            case FieldQuantity::VELOCITY_MAGNITUDE:
                val = ff.at(ir, iz).velocity.norm(); break;
            case FieldQuantity::PRESSURE:
                val = ff.at(ir, iz).pressure; break;
            case FieldQuantity::POTENTIAL:
                val = ef.at(ir, iz).potential; break;
            case FieldQuantity::FIELD_MAGNITUDE:
                val = ef.at(ir, iz).field.norm(); break;
            case FieldQuantity::CHARGE_DENSITY:
                val = ef.at(ir, iz).charge_density; break;
            case FieldQuantity::SPECIES_CONCENTRATION: {
                const auto& sfs = solver.species_fields();
                if (species_index >= 0
                    && static_cast<size_t>(species_index) < sfs.size())
                    val = sfs[static_cast<size_t>(species_index)]
                              .at(ir, iz).concentration;
                break;
            }
        }
        profile.push_back({r, val});
    }
    return profile;
}

/**
 * Axial probe: extract a quantity along z at a fixed r-index.
 */
inline ProbeProfile axial_probe(
    const physics::CoupledSolver& solver,
    FieldQuantity quantity,
    int ir_index,
    int species_index = 0)
{
    ProbeProfile profile;
    const auto& ff = solver.flow_field();
    const auto& ef = solver.electric_field();

    int ir = std::min(ir_index, ff.nr - 1);

    for (int iz = 0; iz < ff.nz; ++iz) {
        double z = (iz + 0.5) * ff.dz;
        double val = 0.0;

        switch (quantity) {
            case FieldQuantity::VELOCITY_MAGNITUDE:
                val = ff.at(ir, iz).velocity.norm(); break;
            case FieldQuantity::PRESSURE:
                val = ff.at(ir, iz).pressure; break;
            case FieldQuantity::POTENTIAL:
                val = ef.at(ir, iz).potential; break;
            case FieldQuantity::FIELD_MAGNITUDE:
                val = ef.at(ir, iz).field.norm(); break;
            case FieldQuantity::CHARGE_DENSITY:
                val = ef.at(ir, iz).charge_density; break;
            case FieldQuantity::SPECIES_CONCENTRATION: {
                const auto& sfs = solver.species_fields();
                if (species_index >= 0
                    && static_cast<size_t>(species_index) < sfs.size())
                    val = sfs[static_cast<size_t>(species_index)]
                              .at(ir, iz).concentration;
                break;
            }
        }
        profile.push_back({z, val});
    }
    return profile;
}

/**
 * Export a probe profile to CSV.
 */
inline bool export_probe_csv(
    const ProbeProfile& profile,
    const std::string& filepath,
    const std::string& coord_label = "coordinate",
    const std::string& value_label = "value")
{
    std::ofstream out(filepath);
    if (!out.is_open()) return false;

    out << std::scientific << std::setprecision(8);
    out << coord_label << "," << value_label << "\n";

    for (const auto& pt : profile) {
        out << pt.coordinate << "," << pt.value << "\n";
    }
    return true;
}

} // namespace post
} // namespace ehd
} // namespace vsepr
