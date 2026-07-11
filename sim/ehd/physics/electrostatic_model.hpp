#pragma once
/**
 * electrostatic_model.hpp
 *
 * Electrohydrodynamic Simulation — Stage 3: Physics
 *
 * Electrostatic field model for the Poisson equation:
 *
 *   ∇·(ε∇φ) = -ρ_e           (Poisson)
 *   E = -∇φ                   (field from potential)
 *
 * Supports:
 *   - Dirichlet voltage BCs on electrode surfaces
 *   - Neumann insulating BCs on dielectric walls
 *   - Analytical coaxial baseline solution
 *   - Discrete field storage
 */

#include "sim/ehd/ehd_types.hpp"
#include <vector>
#include <cmath>
#include <algorithm>

namespace vsepr {
namespace ehd {
namespace physics {

// ============================================================================
// Discrete Electric Field
// ============================================================================

struct ElectricCell {
    double potential = 0.0;  // φ (V)
    Vec3   field;            // E = -∇φ  (V/m)
    double charge_density = 0.0; // ρ_e (C/m³)
};

struct ElectricField {
    int    nr = 0, nz = 0;
    double dr = 0.0, dz = 0.0;
    std::vector<ElectricCell> cells;

    void resize(int nr_, int nz_, double dr_, double dz_) {
        nr = nr_; nz = nz_; dr = dr_; dz = dz_;
        cells.resize(static_cast<size_t>(nr * nz));
    }

    ElectricCell& at(int ir, int iz) { return cells[static_cast<size_t>(ir * nz + iz)]; }
    const ElectricCell& at(int ir, int iz) const { return cells[static_cast<size_t>(ir * nz + iz)]; }

    /** Maximum field magnitude in the domain */
    double E_max() const {
        double emax = 0.0;
        for (const auto& c : cells) {
            double e = c.field.norm();
            if (e > emax) emax = e;
        }
        return emax;
    }

    /** Volume-averaged field magnitude: Ē = (1/|Ω|)∫_Ω |E| dV */
    double E_avg(double total_volume) const {
        if (cells.empty() || total_volume <= 0.0) return 0.0;
        double sum = 0.0;
        for (const auto& c : cells) {
            sum += c.field.norm();
        }
        double cell_vol = total_volume / cells.size();
        return (sum * cell_vol) / total_volume;
    }
};

// ============================================================================
// ElectrostaticModel
// ============================================================================

class ElectrostaticModel {
public:
    explicit ElectrostaticModel(const EHDParameters& params)
        : params_(params) {}

    /**
     * Analytical solution for coaxial capacitor:
     *   φ(r) = ΔV · ln(R_out/r) / ln(R_out/R_in)
     *   E_r(r) = ΔV / (r · ln(R_out/R_in))
     *
     * Used as baseline for the straight-tube geometry (no helix).
     */
    double coaxial_potential(double r) const {
        double R_in    = params_.wire_diameter_m * 0.5;
        double R_out   = params_.tube_radius_m;
        double delta_V = params_.voltage_pos - params_.voltage_neg;  // ΔV
        if (r < R_in || r > R_out) return 0.0;
        if (std::abs(r - R_out) < 1e-15) return 0.0; // grounded outer
        return delta_V * std::log(R_out / r) / std::log(R_out / R_in);
    }

    double coaxial_field_r(double r) const {
        double R_in    = params_.wire_diameter_m * 0.5;
        double R_out   = params_.tube_radius_m;
        double delta_V = params_.voltage_pos - params_.voltage_neg;  // ΔV
        if (r < R_in || r > R_out) return 0.0;
        return delta_V / (r * std::log(R_out / R_in));
    }

    /**
     * Maximum field for coaxial geometry (occurs at inner electrode surface).
     */
    double coaxial_E_max() const {
        double R_in = params_.wire_diameter_m * 0.5;
        return coaxial_field_r(R_in);
    }

    /**
     * Initialize electric field with coaxial analytical solution.
     */
    ElectricField initialize_coaxial(int nr, int nz) const {
        double R_tube  = params_.tube_radius_m;
        double L_total = params_.tube_length_m + params_.inlet_length_m
                       + params_.outlet_length_m;

        ElectricField field;
        field.resize(nr, nz, R_tube / nr, L_total / nz);

        for (int ir = 0; ir < nr; ++ir) {
            double r = (ir + 0.5) * field.dr;
            double phi = coaxial_potential(r);
            double Er  = coaxial_field_r(r);

            for (int iz = 0; iz < nz; ++iz) {
                auto& cell = field.at(ir, iz);
                cell.potential = phi;
                // Radial field only in the analytical solution
                // Decompose into x/y based on angle = 0 (axisymmetric)
                cell.field = {Er, 0.0, 0.0};
                cell.charge_density = 0.0;
            }
        }
        return field;
    }

    /**
     * Compute field from potential using finite differences: E = -∇φ
     * Operates in-place on an existing ElectricField.
     */
    static void compute_field_from_potential(ElectricField& ef) {
        for (int ir = 1; ir < ef.nr - 1; ++ir) {
            for (int iz = 1; iz < ef.nz - 1; ++iz) {
                double dphidr = (ef.at(ir + 1, iz).potential
                               - ef.at(ir - 1, iz).potential) / (2.0 * ef.dr);
                double dphidz = (ef.at(ir, iz + 1).potential
                               - ef.at(ir, iz - 1).potential) / (2.0 * ef.dz);
                ef.at(ir, iz).field = {-dphidr, 0.0, -dphidz};
            }
        }
    }

    const EHDParameters& params() const { return params_; }

private:
    EHDParameters params_;
};

} // namespace physics
} // namespace ehd
} // namespace vsepr
