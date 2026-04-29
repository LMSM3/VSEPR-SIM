#pragma once
/**
 * combustion_model.hpp
 *
 * Electrohydrodynamic Simulation — Stage 3: Physics
 *
 * Metallic Particle Combustion and Reactive Multiphase Flow
 *
 * Models the combustion of five fuel types observed in EHD-coupled systems
 * (cf. Figure Z.3 — Bergthorson 2015):
 *
 *   1. Methane (CH₄)       — gaseous premixed flame, reference baseline
 *   2. Iron (Fe)            — surface oxidation, Fe₂O₃ product, orange emission
 *   3. Aluminum (Al)        — vapour-phase, Al₂O₃ product, white/bright flame
 *   4. Boron/Aluminum (B/Al)— hybrid oxidation, B₂O₃+Al₂O₃, green emission (BO₂)
 *   5. Zirconium (Zr)       — surface + vapour-phase, ZrO₂ product, spark shower
 *
 * Physics implemented:
 *
 *   ◦ d²-law burning model (single-particle regression):
 *       d²(t) = d₀² − K·t
 *     where K is the burning-rate constant (m²/s), function of T_ambient, O₂ fraction.
 *
 *   ◦ Adiabatic flame temperature:
 *       T_ad = T₀ + ΔH_c / (c_p · (1 + AF_stoich))
 *     computed from tabulated heats of combustion and stoichiometric air-fuel ratios.
 *
 *   ◦ Particle burnout time:
 *       t_burn = d₀² / K
 *
 *   ◦ Heat release rate (single particle):
 *       Q_dot = (π/6) · ρ_p · K · d · ΔH_c
 *
 *   ◦ Radiative emission power (Stefan-Boltzmann):
 *       P_rad = ε_p · σ · A_p · (T_p⁴ − T_∞⁴)
 *     with fuel-specific emissivity ε_p and characteristic emission wavelength.
 *
 *   ◦ Volumetric source terms for two-way coupling with the carrier gas:
 *       - mass source (O₂ consumption, product generation)
 *       - momentum coupling (particle drag heating)
 *       - energy source (heat release to gas)
 *
 * All units SI.  Thermodynamic data from NIST-JANAF, Glassman & Yetter.
 */

#include "sim/ehd/ehd_types.hpp"
#include <cmath>
#include <string>
#include <vector>
#include <array>
#include <algorithm>

namespace vsepr {
namespace ehd {
namespace physics {

// ============================================================================
// Physical Constants for Combustion
// ============================================================================

namespace combustion_constants {
    inline constexpr double STEFAN_BOLTZMANN = 5.670374419e-8;  // W/(m²·K⁴)
    inline constexpr double SPEED_OF_LIGHT   = 2.99792458e8;    // m/s
    inline constexpr double PLANCK           = 6.62607015e-34;  // J·s
    inline constexpr double WIEN_B           = 2.897771955e-3;  // m·K
} // namespace combustion_constants

// ============================================================================
// Fuel Types (Figure Z.3)
// ============================================================================

enum class FuelType {
    METHANE,           // CH₄ — gaseous reference
    IRON,              // Fe  — heterogeneous surface oxidation
    ALUMINUM,          // Al  — vapour-phase combustion
    BORON_ALUMINUM,    // B/Al alloy — mixed-mode
    ZIRCONIUM          // Zr  — hybrid combustion, spark emission
};

// ============================================================================
// Combustion Regime Classification
// ============================================================================

enum class CombustionRegime {
    PREMIXED_GAS,      // Gaseous premixed flame (methane)
    SURFACE,           // Heterogeneous surface reaction (shrinking core)
    VAPOUR_PHASE,      // Particle vaporises then burns in gas phase
    HYBRID             // Both surface and vapour-phase contributions
};

// ============================================================================
// Oxide Product Descriptor
// ============================================================================

struct OxideProduct {
    std::string formula;
    double molar_mass;          // kg/mol
    double melting_point_K;
    double boiling_point_K;
    double density;             // kg/m³ (solid)
};

// ============================================================================
// Fuel Thermodynamic Data (per-fuel tabulated properties)
// ============================================================================

struct FuelData {
    FuelType        fuel;
    CombustionRegime regime;
    std::string     name;
    std::string     formula;

    // --- Particle / fuel properties ---
    double molar_mass;          // kg/mol
    double density;             // kg/m³ (particle or liquid fuel)
    double melting_point_K;
    double boiling_point_K;
    double heat_capacity_Jkg;   // c_p (J/(kg·K))

    // --- Combustion thermodynamics ---
    double heat_of_combustion;  // ΔH_c (J/kg), positive for exothermic
    double stoich_air_fuel;     // stoichiometric air-to-fuel mass ratio
    double activation_energy;   // E_a (J/mol) — Arrhenius
    double pre_exponential;     // A (1/s) — Arrhenius pre-exponential

    // --- d²-law parameters ---
    double burning_rate_K;      // K (m²/s) — burning-rate constant at 300 K, 21% O₂
    double burning_rate_n;      // d^n law exponent (2 for classical d² law)

    // --- Radiative properties ---
    double emissivity;          // ε_p — effective particle emissivity
    double peak_wavelength_nm;  // λ_peak (nm) — characteristic emission
    std::string emission_species; // Emitting species (e.g., "BO₂", "AlO", "FeO")

    // --- Adiabatic flame temperature ---
    double adiabatic_flame_T;   // T_ad (K) at stoichiometric in air

    // --- Oxide product ---
    OxideProduct oxide;
};

// ============================================================================
// Thermodynamic Database — Five fuels from Figure Z.3
// ============================================================================

/**
 * Return tabulated thermodynamic data for a given fuel type.
 *
 * Sources:
 *   - Glassman & Yetter, "Combustion" (4th ed.)
 *   - Bergthorson, "Recyclable metal fuels..." (2015)
 *   - NIST-JANAF Thermochemical Tables
 */
inline FuelData get_fuel_data(FuelType fuel) {
    switch (fuel) {

    case FuelType::METHANE:
        return {
            FuelType::METHANE,
            CombustionRegime::PREMIXED_GAS,
            "Methane", "CH4",
            /* molar_mass  */ 0.01604,
            /* density     */ 0.657,      // kg/m³ (gas at STP)
            /* T_melt      */ 90.7,
            /* T_boil      */ 111.7,
            /* c_p         */ 2220.0,     // J/(kg·K)
            /* ΔH_c        */ 50.0e6,     // 50 MJ/kg
            /* AF_stoich   */ 17.2,
            /* E_a         */ 125.5e3,    // J/mol (CH₄ + air global mechanism)
            /* A           */ 1.3e8,
            /* K           */ 0.0,        // Not applicable to gas
            /* n           */ 2.0,
            /* emissivity  */ 0.10,       // low luminosity (blue flame)
            /* λ_peak      */ 431.0,      // nm — CH band
            /* emission    */ "CH*",
            /* T_ad        */ 2223.0,     // K
            /* oxide       */ {"CO2", 0.04401, 194.7, 216.6, 1562.0}
        };

    case FuelType::IRON:
        return {
            FuelType::IRON,
            CombustionRegime::SURFACE,
            "Iron", "Fe",
            /* molar_mass  */ 0.05585,
            /* density     */ 7874.0,
            /* T_melt      */ 1811.0,
            /* T_boil      */ 3134.0,
            /* c_p         */ 449.0,
            /* ΔH_c        */ 7.4e6,      // 7.4 MJ/kg (→ Fe₂O₃)
            /* AF_stoich   */ 1.29,
            /* E_a         */ 168.0e3,    // J/mol (iron oxidation)
            /* A           */ 2.1e6,
            /* K           */ 3.0e-7,     // m²/s (surface-reaction limited)
            /* n           */ 1.0,        // d^1 law for surface regime
            /* emissivity  */ 0.85,       // high emissivity, orange glow
            /* λ_peak      */ 590.0,      // nm — FeO orange-red emission
            /* emission    */ "FeO",
            /* T_ad        */ 2340.0,
            /* oxide       */ {"Fe2O3", 0.15969, 1839.0, 3414.0, 5250.0}
        };

    case FuelType::ALUMINUM:
        return {
            FuelType::ALUMINUM,
            CombustionRegime::VAPOUR_PHASE,
            "Aluminum", "Al",
            /* molar_mass  */ 0.02698,
            /* density     */ 2700.0,
            /* T_melt      */ 933.5,
            /* T_boil      */ 2743.0,
            /* c_p         */ 897.0,
            /* ΔH_c        */ 31.1e6,     // 31.1 MJ/kg (→ Al₂O₃)
            /* AF_stoich   */ 3.97,
            /* E_a         */ 73.6e3,     // J/mol
            /* A           */ 1.5e7,
            /* K           */ 2.0e-6,     // m²/s (classical d² law)
            /* n           */ 2.0,
            /* emissivity  */ 0.95,       // very bright white flame
            /* λ_peak      */ 484.0,      // nm — AlO B-X band, broad continuum
            /* emission    */ "AlO",
            /* T_ad        */ 3673.0,     // K (highest of common metal fuels)
            /* oxide       */ {"Al2O3", 0.10196, 2345.0, 3250.0, 3950.0}
        };

    case FuelType::BORON_ALUMINUM:
        return {
            FuelType::BORON_ALUMINUM,
            CombustionRegime::HYBRID,
            "Boron/Aluminum", "B/Al",
            /* molar_mass  */ 0.0185,     // effective (mixture)
            /* density     */ 2500.0,     // blended alloy
            /* T_melt      */ 933.0,      // ~Al melting
            /* T_boil      */ 2743.0,
            /* c_p         */ 1100.0,
            /* ΔH_c        */ 40.0e6,     // combined: B is 58.5 MJ/kg
            /* AF_stoich   */ 5.65,
            /* E_a         */ 95.0e3,
            /* A           */ 5.0e6,
            /* K           */ 1.5e-6,
            /* n           */ 2.0,
            /* emissivity  */ 0.88,       // green-tinted emission
            /* λ_peak      */ 546.0,      // nm — BO₂ green system
            /* emission    */ "BO2",
            /* T_ad        */ 3450.0,
            /* oxide       */ {"B2O3+Al2O3", 0.0696, 723.0, 2133.0, 2550.0}
        };

    case FuelType::ZIRCONIUM:
        return {
            FuelType::ZIRCONIUM,
            CombustionRegime::HYBRID,
            "Zirconium", "Zr",
            /* molar_mass  */ 0.09122,
            /* density     */ 6506.0,
            /* T_melt      */ 2128.0,
            /* T_boil      */ 4682.0,
            /* c_p         */ 278.0,
            /* ΔH_c        */ 12.0e6,     // 12 MJ/kg (→ ZrO₂)
            /* AF_stoich   */ 1.75,
            /* E_a         */ 142.0e3,
            /* A           */ 3.0e6,
            /* K           */ 4.0e-7,     // m²/s (slower than Al)
            /* n           */ 1.8,        // slightly sub-d² behaviour
            /* emissivity  */ 0.80,       // sparking, bright yellow-white
            /* λ_peak      */ 560.0,      // nm — ZrO bands, broadband
            /* emission    */ "ZrO",
            /* T_ad        */ 2950.0,
            /* oxide       */ {"ZrO2", 0.12322, 2988.0, 4573.0, 5680.0}
        };
    }

    // Unreachable, but satisfy compiler
    return get_fuel_data(FuelType::METHANE);
}

// ============================================================================
// d²-Law Burning Model
// ============================================================================

/**
 * d^n-law particle diameter at time t.
 *
 *   d(t)^n = d₀^n - K·t
 *
 * Returns diameter in metres.  If the particle is consumed (d ≤ 0), returns 0.
 */
inline double particle_diameter(double d0, double K, double n, double t) {
    double dn = std::pow(d0, n) - K * t;
    return (dn > 0.0) ? std::pow(dn, 1.0 / n) : 0.0;
}

/**
 * Particle burnout time:
 *   t_burn = d₀^n / K
 */
inline double burnout_time(double d0, double K, double n) {
    if (K <= 0.0) return 1e30;
    return std::pow(d0, n) / K;
}

/**
 * Instantaneous mass-loss rate (single spherical particle):
 *   dm/dt = -(π/2) · ρ_p · K · d^(2-n)    (derived from d^n law)
 *
 * For classical d² (n=2):  dm/dt = -π·ρ_p·K/2  (constant, independent of d)
 * For d^1 (n=1, surface):  dm/dt = -π·ρ_p·K·d/2
 */
inline double mass_loss_rate(double d, double rho_p, double K, double n) {
    if (d <= 0.0) return 0.0;
    return (constants::PI / 2.0) * rho_p * K * std::pow(d, 2.0 - n);
}

/**
 * Instantaneous heat release rate (single particle):
 *   Q_dot = |dm/dt| · ΔH_c
 */
inline double heat_release_rate(double d, double rho_p,
                                 double K, double n,
                                 double delta_H_c) {
    return mass_loss_rate(d, rho_p, K, n) * delta_H_c;
}

// ============================================================================
// Burning-Rate Constant Corrections
// ============================================================================

/**
 * Temperature-corrected burning-rate constant (Arrhenius form):
 *   K(T) = K₀ · exp(-E_a / (R·T))  /  exp(-E_a / (R·T₀))
 *        = K₀ · exp[ (E_a/R) · (1/T₀ - 1/T) ]
 *
 * T₀ = 300 K reference.  Higher gas temperature accelerates combustion.
 */
inline double burning_rate_corrected(double K0, double E_a,
                                      double T_gas, double T_ref = 300.0) {
    double inv_dT = (1.0 / T_ref) - (1.0 / T_gas);
    return K0 * std::exp(E_a * inv_dT / constants::GAS_CONSTANT);
}

/**
 * Oxygen-fraction correction (linear first approximation):
 *   K(Y_O2) = K₀ · (Y_O2 / 0.233)
 *
 * Y_O2 = 0.233 is the ambient air mass fraction.
 */
inline double burning_rate_O2_corrected(double K0, double Y_O2) {
    return K0 * (Y_O2 / 0.233);
}

// ============================================================================
// Adiabatic Flame Temperature
// ============================================================================

/**
 * Adiabatic flame temperature for stoichiometric combustion in air:
 *   T_ad = T_0 + ΔH_c / [ c_p_mix · (1 + AF_stoich) ]
 *
 * This is the first-law estimate assuming no dissociation.
 * For metal fuels at high T, dissociation lowers the actual temperature
 * by ~5-15 %, but T_ad provides the upper bound.
 */
inline double adiabatic_flame_temperature(double T_ambient,
                                           double delta_H_c,
                                           double cp_mix,
                                           double AF_stoich) {
    if (cp_mix <= 0.0) return T_ambient;
    return T_ambient + delta_H_c / (cp_mix * (1.0 + AF_stoich));
}

/**
 * Convenience: compute T_ad directly from fuel data.
 * Uses a simple product-averaged c_p = 1100 J/(kg·K) for combustion products.
 */
inline double adiabatic_flame_temperature(const FuelData& fd,
                                           double T_ambient = 300.0) {
    double cp_products = 1100.0;  // J/(kg·K), approximate average
    return adiabatic_flame_temperature(
        T_ambient, fd.heat_of_combustion, cp_products, fd.stoich_air_fuel);
}

// ============================================================================
// Radiative Emission
// ============================================================================

/**
 * Stefan-Boltzmann radiative power from a spherical particle:
 *   P_rad = ε · σ · A_p · (T_p⁴ - T_∞⁴)
 *
 * A_p = π·d² (surface area of sphere of diameter d).
 */
inline double radiative_power(double d, double emissivity,
                               double T_particle, double T_ambient) {
    double A_p = constants::PI * d * d;
    double T4_diff = std::pow(T_particle, 4) - std::pow(T_ambient, 4);
    return emissivity * combustion_constants::STEFAN_BOLTZMANN * A_p * T4_diff;
}

/**
 * Wien's law peak emission wavelength:
 *   λ_peak = b / T
 *
 * Returns wavelength in metres.
 */
inline double wien_peak_wavelength(double T_K) {
    if (T_K <= 0.0) return 0.0;
    return combustion_constants::WIEN_B / T_K;
}

/**
 * Spectral radiance at a specific wavelength (Planck's law):
 *   B(λ, T) = (2hc² / λ⁵) · 1 / [exp(hc / (λ·k_B·T)) - 1]
 *
 * Returns W/(m²·sr·m).
 */
inline double planck_spectral_radiance(double lambda_m, double T_K) {
    if (lambda_m <= 0.0 || T_K <= 0.0) return 0.0;

    double c  = combustion_constants::SPEED_OF_LIGHT;
    double h  = combustion_constants::PLANCK;
    double kB = constants::BOLTZMANN;

    double lambda5 = std::pow(lambda_m, 5);
    double exponent = h * c / (lambda_m * kB * T_K);

    // Guard against overflow
    if (exponent > 500.0) return 0.0;

    return (2.0 * h * c * c / lambda5) / (std::exp(exponent) - 1.0);
}

// ============================================================================
// Particle State (Lagrangian descriptor for a single burning particle)
// ============================================================================

struct BurningParticle {
    Vec3   position;
    Vec3   velocity;
    double diameter;         // m (current)
    double diameter_0;       // m (initial)
    double temperature;      // K (particle surface)
    double mass;             // kg
    FuelType fuel;
    bool   active = true;    // false if fully consumed

    double surface_area() const { return constants::PI * diameter * diameter; }
    double volume()       const { return (constants::PI / 6.0) * diameter * diameter * diameter; }
};

// ============================================================================
// Particle Cloud (collection of burning particles)
// ============================================================================

struct ParticleCloud {
    std::vector<BurningParticle> particles;

    /** Total active particle count */
    size_t active_count() const {
        size_t n = 0;
        for (const auto& p : particles) { if (p.active) ++n; }
        return n;
    }

    /** Total heat release rate from all active particles (W) */
    double total_heat_release(const FuelData& fd) const {
        double Q_total = 0.0;
        for (const auto& p : particles) {
            if (!p.active) continue;
            Q_total += heat_release_rate(
                p.diameter, fd.density, fd.burning_rate_K, fd.burning_rate_n,
                fd.heat_of_combustion);
        }
        return Q_total;
    }

    /** Total radiative power from all active particles (W) */
    double total_radiative_power(double emissivity, double T_ambient) const {
        double P_total = 0.0;
        for (const auto& p : particles) {
            if (!p.active) continue;
            P_total += radiative_power(p.diameter, emissivity,
                                        p.temperature, T_ambient);
        }
        return P_total;
    }
};

// ============================================================================
// Volumetric Source Terms (for two-way coupling with carrier gas)
// ============================================================================

/**
 * Volume-averaged heat source from N_p particles in a cell of volume V_cell:
 *   S_h = (N_p / V_cell) · Q_dot_per_particle
 *
 * Returns W/m³ — added to the energy equation RHS.
 */
inline double volumetric_heat_source(double Q_dot_per_particle,
                                      double N_p_per_volume) {
    return Q_dot_per_particle * N_p_per_volume;
}

/**
 * Oxygen consumption rate per unit volume (kg/(m³·s)):
 *   S_O2 = -(N_p / V_cell) · |dm/dt| · AF_stoich / (1 + AF_stoich)
 *
 * The stoichiometric fraction of air that is oxygen is ~0.233.
 */
inline double oxygen_consumption_rate(double mass_loss_per_particle,
                                       double N_p_per_volume,
                                       double AF_stoich) {
    return N_p_per_volume * mass_loss_per_particle
         * (0.233 * AF_stoich) / (1.0 + AF_stoich);
}

/**
 * Particle drag force per unit volume (Stokes drag, spherical particle):
 *   F_drag = 18·μ·N_p / (ρ_p·d²) · (u_gas - u_particle)
 *
 * Returns the momentum source vector (N/m³) for the carrier gas.
 */
inline Vec3 particle_drag_source(double d, double mu_gas, double rho_p,
                                  double N_p_per_volume,
                                  const Vec3& u_gas,
                                  const Vec3& u_particle) {
    if (d <= 0.0) return {0.0, 0.0, 0.0};
    double coeff = 18.0 * mu_gas * N_p_per_volume / (rho_p * d * d);
    Vec3 delta_u = u_gas - u_particle;
    return delta_u * coeff;
}

/**
 * Stokes number (characterises particle-gas coupling):
 *   St = τ_p / τ_f = (ρ_p · d² / 18μ) / (L / U)
 *
 * St << 1: particles follow the flow (good mixing)
 * St >> 1: particles decouple (ballistic trajectories)
 */
inline double stokes_number(double d, double rho_p, double mu_gas,
                             double L_char, double U_char) {
    if (L_char <= 0.0 || U_char <= 0.0 || mu_gas <= 0.0) return 0.0;
    double tau_p = rho_p * d * d / (18.0 * mu_gas);
    double tau_f = L_char / U_char;
    return tau_p / tau_f;
}

// ============================================================================
// Particle Time Integration (single step, explicit Euler)
// ============================================================================

/**
 * Advance a single burning particle by Δt.
 *
 * Updates: position, velocity (drag), diameter (d^n-law), temperature
 * (balance of heat release vs. radiative loss), mass, active flag.
 */
inline void advance_particle(BurningParticle& p,
                              const FuelData& fd,
                              const Vec3& u_gas,
                              double T_gas,
                              double mu_gas,
                              double dt) {
    if (!p.active) return;

    // --- Burning-rate with temperature correction ---
    double K_eff = burning_rate_corrected(fd.burning_rate_K, fd.activation_energy,
                                           T_gas);

    // --- d^n-law diameter regression ---
    double dn = std::pow(p.diameter, fd.burning_rate_n) - K_eff * dt;
    if (dn <= 0.0) {
        p.active = false;
        p.diameter = 0.0;
        p.mass = 0.0;
        return;
    }
    p.diameter = std::pow(dn, 1.0 / fd.burning_rate_n);

    // --- Mass update ---
    p.mass = fd.density * (constants::PI / 6.0)
           * p.diameter * p.diameter * p.diameter;

    // --- Velocity: Stokes drag ---
    if (p.diameter > 0.0 && fd.density > 0.0) {
        double tau_p = fd.density * p.diameter * p.diameter / (18.0 * mu_gas);
        if (tau_p > 0.0) {
            Vec3 drag_accel = (u_gas - p.velocity) * (dt / tau_p);
            p.velocity = p.velocity + drag_accel;
        }
    }

    // --- Position update ---
    p.position = p.position + p.velocity * dt;

    // --- Temperature: heat generation vs radiative loss ---
    double Q_gen  = heat_release_rate(p.diameter, fd.density,
                                       K_eff, fd.burning_rate_n,
                                       fd.heat_of_combustion);
    double Q_rad  = radiative_power(p.diameter, fd.emissivity,
                                     p.temperature, T_gas);
    double m_cp = p.mass * fd.heat_capacity_Jkg;
    if (m_cp > 1e-30) {
        p.temperature += (Q_gen - Q_rad) * dt / m_cp;
    }

    // Clamp temperature to physically reasonable bounds
    p.temperature = std::max(T_gas, std::min(p.temperature, fd.boiling_point_K));
}

// ============================================================================
// Cloud Initialisation
// ============================================================================

/**
 * Create a uniform cloud of N_p identical particles.
 */
inline ParticleCloud create_uniform_cloud(
    FuelType fuel,
    int N_p,
    double d0,              // initial diameter (m)
    double T_init,          // initial temperature (K)
    const Vec3& origin,     // injection point
    const Vec3& u_inject)   // injection velocity
{
    FuelData fd = get_fuel_data(fuel);
    ParticleCloud cloud;
    cloud.particles.resize(static_cast<size_t>(N_p));

    double m0 = fd.density * (constants::PI / 6.0) * d0 * d0 * d0;

    for (auto& p : cloud.particles) {
        p.position    = origin;
        p.velocity    = u_inject;
        p.diameter    = d0;
        p.diameter_0  = d0;
        p.temperature = T_init;
        p.mass        = m0;
        p.fuel        = fuel;
        p.active      = true;
    }
    return cloud;
}

// ============================================================================
// Advance Entire Cloud
// ============================================================================

/**
 * Advance all particles in the cloud by Δt.
 * Returns the total heat release during this step (J).
 */
inline double advance_cloud(ParticleCloud& cloud,
                             FuelType fuel,
                             const Vec3& u_gas,
                             double T_gas,
                             double mu_gas,
                             double dt) {
    FuelData fd = get_fuel_data(fuel);
    double Q_total = 0.0;

    for (auto& p : cloud.particles) {
        if (!p.active) continue;
        double Q_pre = heat_release_rate(p.diameter, fd.density,
                                          fd.burning_rate_K, fd.burning_rate_n,
                                          fd.heat_of_combustion);
        advance_particle(p, fd, u_gas, T_gas, mu_gas, dt);
        Q_total += Q_pre * dt;
    }
    return Q_total;
}

// ============================================================================
// Comparison / Ranking Utilities
// ============================================================================

/**
 * Compute energy density (J/m³) for a fuel:
 *   e_v = ρ · ΔH_c
 */
inline double volumetric_energy_density(const FuelData& fd) {
    return fd.density * fd.heat_of_combustion;
}

/**
 * Compute gravimetric energy density (J/kg):
 *   e_g = ΔH_c
 */
inline double gravimetric_energy_density(const FuelData& fd) {
    return fd.heat_of_combustion;
}

/**
 * Summary structure for comparing fuels.
 */
struct FuelComparison {
    FuelType fuel;
    std::string name;
    double T_ad;                // K
    double energy_density_vol;  // MJ/m³
    double energy_density_grav; // MJ/kg
    double K_burn;              // m²/s
    double t_burn_10um;         // burnout time for d₀=10 μm (s)
    double emissivity;
    double peak_wavelength_nm;
};

/**
 * Build a comparison table for all five fuels.
 */
inline std::vector<FuelComparison> build_fuel_comparison() {
    std::vector<FuelComparison> table;
    static const std::array<FuelType, 5> fuels = {
        FuelType::METHANE, FuelType::IRON, FuelType::ALUMINUM,
        FuelType::BORON_ALUMINUM, FuelType::ZIRCONIUM
    };

    double d0 = 10.0e-6;  // 10 μm reference diameter

    for (auto ft : fuels) {
        FuelData fd = get_fuel_data(ft);
        double e_v = volumetric_energy_density(fd) / 1.0e6;   // MJ/m³
        double e_g = gravimetric_energy_density(fd) / 1.0e6;  // MJ/kg
        double t_b = (fd.burning_rate_K > 0.0)
                   ? burnout_time(d0, fd.burning_rate_K, fd.burning_rate_n)
                   : 0.0;

        table.push_back({
            ft, fd.name, fd.adiabatic_flame_T,
            e_v, e_g, fd.burning_rate_K, t_b,
            fd.emissivity, fd.peak_wavelength_nm
        });
    }
    return table;
}

// ============================================================================
// EHD–Combustion Coupling
// ============================================================================

/**
 * Electric-field-enhanced burning rate.
 *
 * An applied electric field modifies the combustion via:
 *   - Ionic wind increasing local convective transport
 *   - Charged soot / oxide deflection altering flame structure
 *
 * Empirical correlation (Lawton & Weinberg):
 *   K_E = K₀ · [1 + α_E · (E / E_ref)²]
 *
 * where α_E ≈ 0.05-0.3 (fuel-dependent), E_ref = 10 kV/m.
 */
inline double burning_rate_ehd_enhanced(double K0, double E_field,
                                         double alpha_E = 0.1,
                                         double E_ref = 1.0e4) {
    double ratio = E_field / E_ref;
    return K0 * (1.0 + alpha_E * ratio * ratio);
}

/**
 * Lorentz force on a charged burning particle in the EHD field:
 *   F_E = q_p · E
 *
 * Particles acquire charge via thermionic emission:
 *   q_p ≈ 4πε₀ · r_p · k_B · T_p / e   (Rayleigh limit estimate)
 *
 * Returns force vector (N) on a single particle.
 */
inline Vec3 charged_particle_force(double d, double T_particle,
                                    const Vec3& E_field) {
    double r = d * 0.5;
    double q = 4.0 * constants::PI * constants::EPSILON_0 * r
             * constants::BOLTZMANN * T_particle / constants::ELEMENTARY_E;
    return E_field * q;
}

} // namespace physics
} // namespace ehd
} // namespace vsepr
