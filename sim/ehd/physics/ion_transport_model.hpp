#pragma once
/**
 * ion_transport_model.hpp
 *
 * Electrohydrodynamic Simulation — Stage 3: Physics
 *
 * Nernst-Planck ion transport model:
 *
 *   ∂c_i/∂t + ∇·N_i = 0                         (species conservation)
 *
 *   N_i = -D_i ∇c_i  -  z_i μ_i F c_i ∇φ  +  c_i u
 *           diffusion     electromigration      convection
 *
 * Each ionic species is tracked independently.  The local space-charge
 * density feeds back to the Poisson equation:
 *
 *   ρ_e = F Σ z_i c_i
 *
 * This module provides:
 *   - Species field storage (concentration per cell)
 *   - Flux computation (diffusion + migration + convection)
 *   - Charge-density assembly
 *   - Outlet flux integration
 */

#include "sim/ehd/ehd_types.hpp"
#include "sim/ehd/physics/flow_model.hpp"
#include "sim/ehd/physics/electrostatic_model.hpp"
#include <vector>
#include <cmath>
#include <numeric>

namespace vsepr {
namespace ehd {
namespace physics {

// ============================================================================
// Species Concentration Field
// ============================================================================

struct SpeciesCell {
    double concentration = 0.0;  // c_i (mol/m³)
    Vec3   flux;                 // N_i (mol/(m²·s))
};

struct SpeciesField {
    int    nr = 0, nz = 0;
    double dr = 0.0, dz = 0.0;
    int    species_index = 0;
    std::vector<SpeciesCell> cells;

    void resize(int nr_, int nz_, double dr_, double dz_) {
        nr = nr_; nz = nz_; dr = dr_; dz = dz_;
        cells.resize(static_cast<size_t>(nr * nz));
    }

    SpeciesCell& at(int ir, int iz) { return cells[static_cast<size_t>(ir * nz + iz)]; }
    const SpeciesCell& at(int ir, int iz) const { return cells[static_cast<size_t>(ir * nz + iz)]; }
};

// ============================================================================
// IonTransportModel
// ============================================================================

class IonTransportModel {
public:
    explicit IonTransportModel(const EHDParameters& params)
        : params_(params) {}

    /**
     * Initialize species fields with uniform concentration.
     */
    std::vector<SpeciesField> initialize_uniform(int nr, int nz) const {
        double R_tube  = params_.tube_radius_m;
        double L_total = params_.tube_length_m + params_.inlet_length_m
                       + params_.outlet_length_m;

        std::vector<SpeciesField> fields;
        for (size_t i = 0; i < params_.species.size(); ++i) {
            SpeciesField sf;
            sf.species_index = static_cast<int>(i);
            sf.resize(nr, nz, R_tube / nr, L_total / nz);

            double c0 = params_.species[i].init_conc;
            for (auto& cell : sf.cells) {
                cell.concentration = c0;
            }
            fields.push_back(std::move(sf));
        }
        return fields;
    }

    /**
     * Compute Nernst-Planck flux for a single cell:
     *   N_i = -D_i ∇c_i  -  z_i μ_i F c_i ∇φ  +  c_i u
     */
    static Vec3 compute_flux(
        const IonicSpecies& sp,
        double c_i,           // local concentration
        const Vec3& grad_c,   // ∇c_i
        const Vec3& grad_phi, // ∇φ
        const Vec3& velocity, // u
        double temperature_K)
    {
        double D_i  = sp.diffusivity;                          // D_i
        double mu_i = sp.effective_mobility(temperature_K);    // μ_i (ionic mobility)
        double z_i  = static_cast<double>(sp.valence);         // z_i

        // Diffusion: -D_i ∇c_i
        Vec3 N_diff = grad_c * (-D_i);

        // Electromigration: -z_i μ_i F c_i ∇φ
        Vec3 N_migr = grad_phi * (-z_i * mu_i * constants::FARADAY * c_i);

        // Convection: c_i u
        Vec3 N_conv = velocity * c_i;

        return N_diff + N_migr + N_conv;
    }

    /**
     * Assemble the local space-charge density from all species:
     *   ρ_e = F Σ z_i c_i
     */
    static double assemble_charge_density(
        const std::vector<IonicSpecies>& species,
        const std::vector<double>& concentrations)
    {
        double rho_e = 0.0;
        for (size_t i = 0; i < species.size() && i < concentrations.size(); ++i) {
            rho_e += species[i].valence * concentrations[i];
        }
        return constants::FARADAY * rho_e;
    }

    /**
     * Compute outlet molar flux for a species:
     *   Ṅ_i = ∫_{A_out} N_i · n dA
     *
     * Approximated as sum over outlet cells.
     */
    static double outlet_molar_flux(const SpeciesField& sf, double /*tube_radius*/) {
        if (sf.nz == 0 || sf.nr == 0) return 0.0;

        double flux = 0.0;
        int iz_out = sf.nz - 1;
        for (int ir = 0; ir < sf.nr; ++ir) {
            double r = (ir + 0.5) * sf.dr;
            double dA = 2.0 * constants::PI * r * sf.dr;
            flux += sf.at(ir, iz_out).flux.z * dA;
        }
        return flux;
    }

    /**
     * Compute accumulation index (max/min concentration ratio).
     * A useful metric for how much the field concentrates or depletes species.
     */
    static double accumulation_index(const SpeciesField& sf) {
        double c_min = 1e30, c_max = -1e30;
        for (const auto& cell : sf.cells) {
            if (cell.concentration < c_min) c_min = cell.concentration;
            if (cell.concentration > c_max) c_max = cell.concentration;
        }
        if (c_min <= 0.0) return (c_max > 0.0) ? 1e30 : 1.0;
        return c_max / c_min;
    }

    const EHDParameters& params() const { return params_; }

private:
    EHDParameters params_;
};

} // namespace physics
} // namespace ehd
} // namespace vsepr
