#pragma once
/**
 * environment.hpp
 * ---------------
 * EnvironmentContext: physical context in which a structure is evaluated.
 *
 * This is a first-class input to the formation engine, not an optional flag.
 * The environment changes which physics terms are relevant, how electrostatics
 * are screened, what bonding states are stable, and what structural outcomes
 * are plausible.
 *
 * Three primary classes (Step 10.2):
 *
 *   NearVacuum       — intrinsic pair interactions dominate, low screening
 *   DryCondensed     — packing, sterics, local lattice effects matter
 *   Solution         — screened electrostatics, solvation, ion concentration
 *
 * Usage:
 *   ModelParams mp;
 *   mp.env = EnvironmentContext::solution(78.4, 300.0, 0.15);
 *   model->eval(state, mp);  // will apply dielectric screening to Coulomb
 *
 * Physics changes per context (Step 10.3):
 *
 *   NearVacuum:
 *     - dielectric ≈ 1.0 (vacuum)
 *     - Coulomb unscreened (when re-enabled)
 *     - isolated cluster energetics
 *
 *   DryCondensed:
 *     - effective dielectric 2–5 (bulk organic/inorganic)
 *     - packing and sterics drive structure
 *     - no solvation correction
 *
 *   Solution:
 *     - dielectric >> 1 (water: 78.4)
 *     - Coulomb screened by 1/epsilon
 *     - ionic strength modifies effective Debye length
 *     - concentration and redox conditions affect stability
 */

#include <cmath>

namespace atomistic {

// ============================================================================
// Medium Classification
// ============================================================================

enum class MediumType {
    NearVacuum,       // gas phase / isolated cluster
    DryCondensed,     // bulk solid, thin film, dry powder
    Solution,         // aqueous or polar solvent
    Unknown,          // not specified — falls back to NearVacuum behaviour
};

// ============================================================================
// EnvironmentContext
// ============================================================================

struct EnvironmentContext {
    MediumType medium        = MediumType::NearVacuum;
    double     temperature   = 0.0;     // K  (0 = not specified)
    double     pressure      = 1.0;     // atm
    double     dielectric    = 1.0;     // relative permittivity (eps_r)
    double     ionic_strength= 0.0;     // mol/L
    double     concentration = 0.0;     // mol/L (primary species)
    bool       periodic      = false;   // PBC active

    // ----------------------------------------------------------------
    // Derived quantities
    // ----------------------------------------------------------------

    // Effective Coulomb screening factor:
    //   k_eff = k_e / dielectric
    // Applied to Coulomb energy and forces when Coulomb is re-enabled.
    double coulomb_scale() const { return 1.0 / dielectric; }

    // Debye screening length (Angstroms) under Debye-Hückel:
    //   lambda_D = sqrt(eps_0 * eps_r * k_B * T / (2 * N_A * e^2 * I))
    // Expressed in Angstroms using the pre-computed constant 3.04/sqrt(I*eps_r/78.4).
    // Returns infinity when I = 0 (no screening).
    double debye_length() const {
        if (ionic_strength < 1e-12 || temperature < 1.0) return 1e30;
        // Simplified Debye length (Å): 3.04 / sqrt(I) at 25°C water
        // Temperature scaling: lambda ∝ sqrt(T * eps_r)
        double base = 3.04 / std::sqrt(ionic_strength);
        double t_scale = std::sqrt((temperature / 298.15) * (dielectric / 78.4));
        return base * t_scale;
    }

    // ----------------------------------------------------------------
    // Factory constructors (Step 10.2)
    // ----------------------------------------------------------------

    static EnvironmentContext near_vacuum(double T = 0.0)
    {
        EnvironmentContext e;
        e.medium      = MediumType::NearVacuum;
        e.temperature = T;
        e.dielectric  = 1.0;
        return e;
    }

    static EnvironmentContext dry_condensed(double dielectric_r = 3.0, double T = 298.15)
    {
        EnvironmentContext e;
        e.medium      = MediumType::DryCondensed;
        e.temperature = T;
        e.dielectric  = dielectric_r;
        return e;
    }

    static EnvironmentContext solution(double dielectric_r = 78.4,
                                       double T = 298.15,
                                       double ionic_str = 0.0,
                                       double conc = 0.0)
    {
        EnvironmentContext e;
        e.medium         = MediumType::Solution;
        e.temperature    = T;
        e.dielectric     = dielectric_r;
        e.ionic_strength = ionic_str;
        e.concentration  = conc;
        return e;
    }

    // ----------------------------------------------------------------
    // Utility
    // ----------------------------------------------------------------

    const char* medium_name() const {
        switch (medium) {
            case MediumType::NearVacuum:   return "near_vacuum";
            case MediumType::DryCondensed: return "dry_condensed";
            case MediumType::Solution:     return "solution";
            default:                       return "unknown";
        }
    }
};

} // namespace atomistic
