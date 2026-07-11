#pragma once
/**
 * run_card.hpp
 *
 * Electrohydrodynamic Simulation — Run Card I/O
 *
 * Serialises / deserialises the full EHD case specification to a structured
 * text format.  Each run card fully specifies a reproducible simulation case:
 *   - Case ID
 *   - Geometry parameters
 *   - Fluid properties
 *   - Species list
 *   - Boundary conditions
 *   - Mesh controls
 *   - Solver settings
 *   - Requested outputs
 */

#include "sim/ehd/ehd_types.hpp"
#include "sim/ehd/physics/coupled_solver.hpp"
#include <fstream>
#include <iomanip>
#include <string>

namespace vsepr {
namespace ehd {

/**
 * Write a complete run card to text file.
 */
inline bool write_run_card(const EHDParameters& p,
                            const physics::RunCard& rc,
                            const std::string& filepath)
{
    std::ofstream out(filepath);
    if (!out.is_open()) return false;

    out << std::fixed << std::setprecision(6);

    out << "# ============================================================================\n";
    out << "# EHD Run Card — Case: " << rc.case_id << "\n";
    out << "# ============================================================================\n\n";

    out << "[case]\n";
    out << "  id = " << rc.case_id << "\n\n";

    out << "[geometry]\n";
    out << "  tube_radius      = " << p.tube_radius_m    << "  # m\n";
    out << "  tube_length      = " << p.tube_length_m    << "  # m\n";
    out << "  helix_pitch      = " << p.helix_pitch_m    << "  # m\n";
    out << "  wire_diameter    = " << p.wire_diameter_m   << "  # m\n";
    out << "  num_turns        = " << p.num_turns         << "\n";
    out << "  wall_clearance   = " << p.wall_clearance_m  << "  # m\n";
    out << "  inlet_length     = " << p.inlet_length_m    << "  # m\n";
    out << "  outlet_length    = " << p.outlet_length_m   << "  # m\n\n";

    out << "[electrical]\n";
    out << "  V_pos = " << p.voltage_pos << "  # V\n";
    out << "  V_neg = " << p.voltage_neg << "  # V\n\n";

    out << "[flow]\n";
    out << "  inlet_velocity       = " << p.inlet_velocity       << "  # m/s\n";
    out << "  outlet_pressure      = " << p.outlet_pressure      << "  # Pa\n";
    out << "  volumetric_flow_rate = " << p.volumetric_flow_rate << "  # m³/s\n\n";

    out << "[fluid]\n";
    out << "  name                = " << p.fluid.name                << "\n";
    out << "  density             = " << p.fluid.density             << "  # kg/m³\n";
    out << "  viscosity           = " << std::scientific << p.fluid.viscosity << "  # Pa·s\n";
    out << std::fixed;
    out << "  relative_permittivity = " << p.fluid.relative_permittivity << "\n";
    out << "  temperature         = " << p.fluid.temperature_K       << "  # K\n\n";

    for (size_t i = 0; i < p.species.size(); ++i) {
        const auto& sp = p.species[i];
        out << "[species." << i << "]\n";
        out << "  name        = " << sp.name        << "\n";
        out << "  valence     = " << sp.valence     << "\n";
        out << "  diffusivity = " << std::scientific << sp.diffusivity << "  # m²/s\n";
        out << std::fixed;
        out << "  mobility    = " << std::scientific << sp.mobility    << "  # m²/(V·s)\n";
        out << std::fixed;
        out << "  init_conc   = " << sp.init_conc   << "  # mol/m³\n\n";
    }

    out << "[solver]\n";
    out << "  nr                   = " << rc.nr                   << "\n";
    out << "  nz                   = " << rc.nz                   << "\n";
    out << "  max_outer_iterations = " << rc.max_outer_iterations << "\n";
    out << "  convergence_tol      = " << std::scientific << rc.convergence_tol << "\n";
    out << std::fixed;
    out << "  relax_flow           = " << rc.relax_flow     << "\n";
    out << "  relax_electric       = " << rc.relax_electric << "\n";
    out << "  relax_species        = " << rc.relax_species  << "\n\n";

    out << "[outputs]\n";
    out << "  deltaP\n";
    out << "  Emax\n";
    out << "  Eavg\n";
    out << "  ion_flux_out\n";
    out << "  accumulation_index\n";

    return true;
}

} // namespace ehd
} // namespace vsepr
