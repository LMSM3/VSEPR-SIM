#pragma once
/**
 * reactive_multiphase.hpp
 *
 * Electrohydrodynamic Simulation — Stage 3: Physics
 *
 * Reactive Multiphase Flow Model
 *
 * Couples the Eulerian carrier-gas field (from flow_model / coupled_solver)
 * with the Lagrangian particle phase (from combustion_model) using two-way
 * source terms:
 *
 *   Gas-phase equations:
 *     ∂ρ/∂t + ∇·(ρu) = S_mass        (mass source from evaporation/burnout)
 *     ∂(ρu)/∂t + ...  = ... + S_mom    (drag coupling)
 *     ∂(ρe)/∂t + ...  = ... + S_energy  (heat release + radiative loss)
 *     ∂Y_O2/∂t + ...  = ... + S_O2      (oxygen consumption)
 *
 *   Particle phase:
 *     dx_p/dt = v_p
 *     dv_p/dt = (u - v_p)/τ_p + g + F_E/m_p
 *     dd^n/dt = -K(T,Y_O2,E)
 *     dT_p/dt = (Q_gen - Q_rad) / (m_p · c_p)
 *
 * The module provides:
 *   - Source-term accumulation onto the Eulerian grid
 *   - Damköhler number estimation (reaction vs. flow timescale)
 *   - Particle loading ratio
 *   - Flame zone detection (based on local heat release)
 *   - Coupling with EHD body forces on charged burning particles
 *
 * All units SI.
 */

#include "sim/ehd/ehd_types.hpp"
#include "sim/ehd/physics/combustion_model.hpp"
#include "sim/ehd/physics/flow_model.hpp"
#include "sim/ehd/physics/electrostatic_model.hpp"
#include <vector>
#include <cmath>
#include <algorithm>

namespace vsepr {
namespace ehd {
namespace physics {

// ============================================================================
// Eulerian Source-Term Cell
// ============================================================================

struct SourceCell {
    double S_mass   = 0.0;  // kg/(m³·s) — mass source from particle burnout
    double S_energy = 0.0;  // W/m³      — heat release to gas
    double S_O2     = 0.0;  // kg/(m³·s) — oxygen consumption (negative)
    Vec3   S_mom;           // N/m³      — momentum coupling (drag)
    double S_rad    = 0.0;  // W/m³      — radiative loss (outgoing)
    int    n_particles = 0; // count of particles mapped to this cell
};

struct SourceField {
    int    nr = 0, nz = 0;
    double dr = 0.0, dz = 0.0;
    std::vector<SourceCell> cells;

    void resize(int nr_, int nz_, double dr_, double dz_) {
        nr = nr_; nz = nz_; dr = dr_; dz = dz_;
        cells.assign(static_cast<size_t>(nr * nz), SourceCell{});
    }

    SourceCell& at(int ir, int iz) { return cells[static_cast<size_t>(ir * nz + iz)]; }
    const SourceCell& at(int ir, int iz) const { return cells[static_cast<size_t>(ir * nz + iz)]; }

    /** Peak volumetric heat release (W/m³) */
    double max_heat_release() const {
        double qmax = 0.0;
        for (const auto& c : cells) {
            if (c.S_energy > qmax) qmax = c.S_energy;
        }
        return qmax;
    }

    /** Total heat release rate (W) — integrate over volume */
    double total_heat_release() const {
        double dV = 2.0 * constants::PI * dr * dz;  // annular cell volume factor
        double total = 0.0;
        for (int ir = 0; ir < nr; ++ir) {
            double r = (ir + 0.5) * dr;
            for (int iz = 0; iz < nz; ++iz) {
                total += at(ir, iz).S_energy * r * dV;
            }
        }
        return total;
    }
};

// ============================================================================
// Particle-to-Grid Mapping
// ============================================================================

/**
 * Map a particle position to grid indices (ir, iz) on a cylindrical grid.
 * Assumes axis at r=0, z starting from 0.
 * Returns false if the particle is outside the grid.
 */
inline bool particle_to_cell(const Vec3& pos, double dr, double dz,
                              int nr, int nz, int& ir, int& iz) {
    double r = std::sqrt(pos.x * pos.x + pos.y * pos.y);
    ir = static_cast<int>(r / dr);
    iz = static_cast<int>(pos.z / dz);
    return (ir >= 0 && ir < nr && iz >= 0 && iz < nz);
}

// ============================================================================
// Source-Term Accumulation
// ============================================================================

/**
 * Accumulate source terms from a particle cloud onto the Eulerian grid.
 *
 * For each active particle:
 *   1. Locate its cell (ir, iz)
 *   2. Compute local source contributions per unit volume
 *   3. Add to the SourceField
 *
 * V_cell = 2π·r·Δr·Δz (cylindrical) or Δx·Δy·Δz (Cartesian).
 */
inline void accumulate_sources(SourceField& sf,
                                const ParticleCloud& cloud,
                                const FuelData& fd,
                                const FlowField& flow,
                                double mu_gas) {
    for (const auto& p : cloud.particles) {
        if (!p.active) continue;

        int ir, iz;
        if (!particle_to_cell(p.position, sf.dr, sf.dz, sf.nr, sf.nz, ir, iz))
            continue;

        // Cell volume (cylindrical annular)
        double r = (ir + 0.5) * sf.dr;
        double V_cell = 2.0 * constants::PI * r * sf.dr * sf.dz;
        if (V_cell <= 0.0) continue;
        double N_pV = 1.0 / V_cell;  // one particle contribution per volume

        // Mass loss
        double dm_dt = mass_loss_rate(p.diameter, fd.density,
                                       fd.burning_rate_K, fd.burning_rate_n);

        // Heat release
        double Q_dot = dm_dt * fd.heat_of_combustion;

        // Radiative loss
        double P_rad = radiative_power(p.diameter, fd.emissivity,
                                        p.temperature, 300.0);

        // Drag coupling
        const Vec3& u_gas = flow.at(ir, iz).velocity;
        Vec3 F_drag = particle_drag_source(
            p.diameter, mu_gas, fd.density, N_pV, u_gas, p.velocity);

        // O₂ consumption
        double S_O2 = oxygen_consumption_rate(dm_dt, N_pV, fd.stoich_air_fuel);

        // Accumulate
        SourceCell& sc = sf.at(ir, iz);
        sc.S_mass   += dm_dt * N_pV;
        sc.S_energy += Q_dot * N_pV;
        sc.S_rad    += P_rad * N_pV;
        sc.S_O2     -= S_O2;
        sc.S_mom     = sc.S_mom + F_drag;
        sc.n_particles += 1;
    }
}

// ============================================================================
// Dimensionless Numbers for Reactive Multiphase Flow
// ============================================================================

/**
 * Damköhler number (first kind):
 *   Da_I = τ_flow / τ_reaction
 *
 * τ_flow     = L / U  (residence time)
 * τ_reaction = t_burn = d₀^n / K
 *
 * Da >> 1: fast reaction (chemistry-limited by mixing)
 * Da << 1: slow reaction (kinetics-limited)
 */
inline double damkohler_I(double d0, double K, double n,
                            double L_char, double U_char) {
    if (U_char <= 0.0 || K <= 0.0) return 0.0;
    double tau_flow = L_char / U_char;
    double tau_rxn  = burnout_time(d0, K, n);
    return tau_flow / tau_rxn;
}

/**
 * Particle loading ratio (mass of particles per unit mass of gas):
 *   φ_m = ṁ_p / ṁ_g = (N_p · m_p) / (ρ_g · Q)
 *
 * Determines importance of two-way coupling:
 *   φ_m < 0.1:  one-way coupling acceptable
 *   φ_m > 0.1:  two-way coupling required
 */
inline double mass_loading_ratio(int N_p, double m_p,
                                  double rho_gas, double Q_flow) {
    if (rho_gas <= 0.0 || Q_flow <= 0.0) return 0.0;
    return (N_p * m_p) / (rho_gas * Q_flow);
}

/**
 * Biot number for a burning particle:
 *   Bi = h · d / (2 · k_p)
 *
 * For metal particles Bi << 1 typically (lumped-capacitance valid).
 * k_p ≈ thermal conductivity of particle (W/(m·K)).
 */
inline double biot_number(double h_conv, double d, double k_particle) {
    if (k_particle <= 0.0) return 0.0;
    return h_conv * d / (2.0 * k_particle);
}

/**
 * Nusselt number for a sphere in Stokes flow (Ranz-Marshall correlation):
 *   Nu = 2 + 0.6 · Re_p^0.5 · Pr^(1/3)
 */
inline double nusselt_ranz_marshall(double Re_p, double Pr) {
    return 2.0 + 0.6 * std::sqrt(std::abs(Re_p)) * std::cbrt(Pr);
}

/**
 * Particle Reynolds number:
 *   Re_p = ρ_g · |u_g - u_p| · d / μ_g
 */
inline double particle_reynolds(double rho_gas, const Vec3& u_gas,
                                 const Vec3& u_particle, double d,
                                 double mu_gas) {
    if (mu_gas <= 0.0) return 0.0;
    double u_rel = (u_gas - u_particle).norm();
    return rho_gas * u_rel * d / mu_gas;
}

// ============================================================================
// Flame Zone Detection
// ============================================================================

/**
 * Identify cells where combustion is actively occurring.
 * A cell is "in the flame zone" if S_energy > threshold.
 *
 * Returns list of (ir, iz) pairs.
 */
inline std::vector<std::pair<int,int>> detect_flame_zone(
    const SourceField& sf,
    double threshold_Wm3 = 1e3)
{
    std::vector<std::pair<int,int>> zone;
    for (int ir = 0; ir < sf.nr; ++ir) {
        for (int iz = 0; iz < sf.nz; ++iz) {
            if (sf.at(ir, iz).S_energy > threshold_Wm3) {
                zone.emplace_back(ir, iz);
            }
        }
    }
    return zone;
}

/**
 * Flame length: axial extent of the flame zone.
 * Returns (z_min, z_max) in metres.
 */
inline std::pair<double,double> flame_extent(
    const SourceField& sf,
    double threshold_Wm3 = 1e3)
{
    int iz_min = sf.nz, iz_max = -1;
    for (int ir = 0; ir < sf.nr; ++ir) {
        for (int iz = 0; iz < sf.nz; ++iz) {
            if (sf.at(ir, iz).S_energy > threshold_Wm3) {
                iz_min = std::min(iz_min, iz);
                iz_max = std::max(iz_max, iz);
            }
        }
    }
    if (iz_max < 0) return {0.0, 0.0};
    return {iz_min * sf.dz, (iz_max + 1) * sf.dz};
}

// ============================================================================
// Reactive Multiphase Summary
// ============================================================================

struct ReactiveMultiphaseSummary {
    FuelType fuel;
    int    total_particles;
    int    active_particles;
    double total_heat_release_W;
    double total_radiative_W;
    double peak_source_Wm3;
    double flame_length_m;
    double damkohler;
    double mass_loading;
    double stokes;
};

/**
 * Compute a comprehensive summary of the reactive multiphase state.
 */
inline ReactiveMultiphaseSummary summarise_reactive(
    const ParticleCloud& cloud,
    const SourceField& sf,
    FuelType fuel,
    double d0, double U_char, double L_char,
    double rho_gas, double Q_flow, double mu_gas)
{
    FuelData fd = get_fuel_data(fuel);
    ReactiveMultiphaseSummary s{};
    s.fuel = fuel;
    s.total_particles  = static_cast<int>(cloud.particles.size());
    s.active_particles = static_cast<int>(cloud.active_count());
    s.total_heat_release_W = sf.total_heat_release();
    s.total_radiative_W = cloud.total_radiative_power(fd.emissivity, 300.0);
    s.peak_source_Wm3 = sf.max_heat_release();

    auto [z0, z1] = flame_extent(sf);
    s.flame_length_m = z1 - z0;

    s.damkohler = damkohler_I(d0, fd.burning_rate_K, fd.burning_rate_n,
                               L_char, U_char);

    double m_p = fd.density * (constants::PI / 6.0) * d0 * d0 * d0;
    s.mass_loading = mass_loading_ratio(s.total_particles, m_p, rho_gas, Q_flow);
    s.stokes = stokes_number(d0, fd.density, mu_gas, L_char, U_char);

    return s;
}

} // namespace physics
} // namespace ehd
} // namespace vsepr
