#pragma once
/**
 * contour_export.hpp
 *
 * Electrohydrodynamic Simulation — Stage 5: Postprocessing
 *
 * Exports scalar field data as structured CSV for contour / heatmap plotting.
 * Supported fields: velocity magnitude, pressure, potential, |E|, ρ_ion, c_i.
 */

#include "sim/ehd/physics/coupled_solver.hpp"
#include <fstream>
#include <iomanip>
#include <string>

namespace vsepr {
namespace ehd {
namespace post {

enum class FieldQuantity {
    VELOCITY_MAGNITUDE,
    PRESSURE,
    POTENTIAL,
    FIELD_MAGNITUDE,
    CHARGE_DENSITY,
    SPECIES_CONCENTRATION   // requires species_index
};

/**
 * Export a 2D (r, z) scalar field to CSV.
 * Columns: r, z, value
 */
inline bool export_contour_csv(
    const physics::CoupledSolver& solver,
    FieldQuantity quantity,
    const std::string& filepath,
    int species_index = 0)
{
    std::ofstream out(filepath);
    if (!out.is_open()) return false;

    out << std::scientific << std::setprecision(8);
    out << "r,z,value\n";

    const auto& ff = solver.flow_field();
    const auto& ef = solver.electric_field();

    int nr = ff.nr, nz = ff.nz;
    double dr = ff.dr, dz = ff.dz;

    for (int ir = 0; ir < nr; ++ir) {
        double r = (ir + 0.5) * dr;
        for (int iz = 0; iz < nz; ++iz) {
            double z = (iz + 0.5) * dz;
            double val = 0.0;

            switch (quantity) {
                case FieldQuantity::VELOCITY_MAGNITUDE:
                    val = ff.at(ir, iz).velocity.norm();
                    break;
                case FieldQuantity::PRESSURE:
                    val = ff.at(ir, iz).pressure;
                    break;
                case FieldQuantity::POTENTIAL:
                    val = ef.at(ir, iz).potential;
                    break;
                case FieldQuantity::FIELD_MAGNITUDE:
                    val = ef.at(ir, iz).field.norm();
                    break;
                case FieldQuantity::CHARGE_DENSITY:
                    val = ef.at(ir, iz).charge_density;
                    break;
                case FieldQuantity::SPECIES_CONCENTRATION: {
                    const auto& sfs = solver.species_fields();
                    if (species_index >= 0
                        && static_cast<size_t>(species_index) < sfs.size()) {
                        val = sfs[static_cast<size_t>(species_index)]
                                  .at(ir, iz).concentration;
                    }
                    break;
                }
            }

            out << r << "," << z << "," << val << "\n";
        }
    }

    return true;
}

} // namespace post
} // namespace ehd
} // namespace vsepr
