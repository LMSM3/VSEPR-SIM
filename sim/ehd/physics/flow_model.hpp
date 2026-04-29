#pragma once
/**
 * flow_model.hpp
 *
 * Electrohydrodynamic Simulation — Stage 3: Physics
 *
 * Incompressible Navier-Stokes flow model for laminar channel flow.
 *
 * Governing equations:
 *   ∇·u = 0                                       (continuity)
 *   ρ(∂u/∂t + u·∇u) = -∇p + μ∇²u + f_elec       (momentum)
 *
 * The electric body force f_elec = ρ_e · E is injected by the coupled solver.
 * This module handles:
 *   - Reynolds number estimation
 *   - Analytical Hagen-Poiseuille baseline solution
 *   - Pressure-drop calculation
 *   - Flow field storage on a discrete grid
 */

#include "sim/ehd/ehd_types.hpp"
#include <vector>
#include <cmath>
#include <iostream>

namespace vsepr {
namespace ehd {
namespace physics {

// ============================================================================
// Discrete Flow Field (axisymmetric / 2D cross-section)
// ============================================================================

struct FlowCell {
    Vec3   velocity;     // (u_r, u_θ, u_z)  or  (u_x, u_y, u_z)
    double pressure = 0.0;
};

struct FlowField {
    int    nr = 0, nz = 0;          // radial × axial grid size
    double dr = 0.0, dz = 0.0;     // cell spacing
    std::vector<FlowCell> cells;    // row-major: index = ir * nz + iz

    void resize(int nr_, int nz_, double dr_, double dz_) {
        nr = nr_; nz = nz_; dr = dr_; dz = dz_;
        cells.resize(static_cast<size_t>(nr * nz));
    }

    FlowCell& at(int ir, int iz) { return cells[static_cast<size_t>(ir * nz + iz)]; }
    const FlowCell& at(int ir, int iz) const { return cells[static_cast<size_t>(ir * nz + iz)]; }

    double max_velocity_magnitude() const {
        double u_mag_max = 0.0;
        for (const auto& cell : cells) {
            double u_mag = cell.velocity.norm();
            if (u_mag > u_mag_max) u_mag_max = u_mag;
        }
        return u_mag_max;
    }
};

// ============================================================================
// FlowModel
// ============================================================================

class FlowModel {
public:
    explicit FlowModel(const EHDParameters& params)
        : params_(params) {}

    /** Reynolds number for the channel */
    double reynolds_number() const {
        return params_.reynolds_number();
    }

    /** Is the flow expected to be laminar? (Re < 2300 for pipe flow) */
    bool is_laminar() const {
        return reynolds_number() < 2300.0;
    }

    /**
     * Analytical Hagen-Poiseuille velocity profile for a straight tube.
     * u_z(r) = 2·U_avg · (1 - (r/R)²)
     */
    double poiseuille_velocity(double r) const {
        double R_tube = params_.tube_radius_m;
        double U_avg  = params_.effective_inlet_velocity();
        double ratio  = r / R_tube;
        return 2.0 * U_avg * (1.0 - ratio * ratio);
    }

    /**
     * Hagen-Poiseuille pressure drop for a straight tube of length L.
     * ΔP = 8·μ·L·Q / (π·R⁴) = 128·μ·L·Q / (π·D⁴)
     */
    double poiseuille_pressure_drop(double length) const {
        double R_tube = params_.tube_radius_m;
        double U_avg  = params_.effective_inlet_velocity();
        double mu_f   = params_.fluid.viscosity;   // μ (fluid viscosity)
        // ΔP = 8·μ·U·L / R²   (from mean velocity form)
        return 8.0 * mu_f * U_avg * length / (R_tube * R_tube);
    }

    /**
     * Initialize flow field with Poiseuille profile (baseline).
     */
    FlowField initialize_poiseuille(int nr, int nz) const {
        double R_tube  = params_.tube_radius_m;
        double L_total = params_.tube_length_m + params_.inlet_length_m
                       + params_.outlet_length_m;

        FlowField field;
        field.resize(nr, nz, R_tube / nr, L_total / nz);

        double P_inlet = poiseuille_pressure_drop(L_total);

        for (int ir = 0; ir < nr; ++ir) {
            double r = (ir + 0.5) * field.dr;
            double u_z = poiseuille_velocity(r);

            for (int iz = 0; iz < nz; ++iz) {
                auto& cell = field.at(ir, iz);
                cell.velocity = {0.0, 0.0, u_z};
                cell.pressure = P_inlet * (1.0 - static_cast<double>(iz) / nz);
            }
        }
        return field;
    }

    /**
     * Compute pressure drop from a solved flow field.
     * ΔP = P_in(avg) - P_out(avg)
     */
    static double compute_pressure_drop(const FlowField& field) {
        if (field.nr == 0 || field.nz == 0) return 0.0;

        double p_in = 0.0, p_out = 0.0;
        for (int ir = 0; ir < field.nr; ++ir) {
            p_in  += field.at(ir, 0).pressure;
            p_out += field.at(ir, field.nz - 1).pressure;
        }
        p_in  /= field.nr;
        p_out /= field.nr;
        return p_in - p_out;
    }

    const EHDParameters& params() const { return params_; }

private:
    EHDParameters params_;
};

} // namespace physics
} // namespace ehd
} // namespace vsepr
