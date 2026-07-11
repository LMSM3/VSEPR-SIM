/**
 * gas2_heat.hpp
 * -------------
 * Heat Transfer and Thermal Properties for gas2 Module.
 *
 * Provides:
 *   - Isobaric/isochoric heat capacity from DOF
 *   - Adiabatic processes (compression/expansion)
 *   - Heat conduction through gas layers
 *   - Convective heat transfer coefficients
 *   - Joule-Thomson coefficient
 *   - Debye temperature estimation for comparison with solid-state
 *
 * Mirrors pattern: thermal_properties.hpp  (Cv, Cp, conductivity)
 *                  thermal_api.hpp         (pathway classification)
 *                  pipe_thermal_engine.hpp  (Nusselt, convective h)
 *
 * Anti-black-box: every Cp, Cv, gamma value is traceable to DOF count.
 */

#pragma once

#include "gas2_constants.hpp"
#include "gas2_species.hpp"
#include "gas2_kinetic.hpp"
#include <cmath>
#include <string>
#include <sstream>
#include <iomanip>

namespace vsepr {
namespace gas2 {

// ============================================================================
// Heat capacity from degrees of freedom
// ============================================================================

// Cv = (f/2) · R   (per mole, ideal)
inline double Cv_from_dof(int f) {
    return 0.5 * static_cast<double>(f) * R_gas;
}

// Cp = Cv + R  (per mole, ideal)
inline double Cp_from_dof(int f) {
    return Cv_from_dof(f) + R_gas;
}

// gamma = Cp/Cv = (f+2)/f
inline double gamma_from_dof(int f) {
    return (f > 0) ? static_cast<double>(f + 2) / static_cast<double>(f) : 1.0;
}

// ============================================================================
// Adiabatic processes (reversible)
// ============================================================================

// T2 = T1 · (V1/V2)^(gamma-1)
inline double adiabatic_T_from_V(double T1, double V1, double V2, double gamma) {
    return T1 * std::pow(V1 / V2, gamma - 1.0);
}

// T2 = T1 · (P2/P1)^((gamma-1)/gamma)
inline double adiabatic_T_from_P(double T1, double P1, double P2, double gamma) {
    return T1 * std::pow(P2 / P1, (gamma - 1.0) / gamma);
}

// Work done in adiabatic process: W = nCv(T1-T2) = P1V1/(gamma-1) · [1 - (V1/V2)^(gamma-1)]
inline double adiabatic_work(double n, double T1, double T2, int f) {
    return n * Cv_from_dof(f) * (T1 - T2);
}

// Speed of sound: c = sqrt(gamma·R·T/M)
inline double speed_of_sound(double T, double M_kg, double gamma) {
    return std::sqrt(gamma * R_gas * T / M_kg);
}

// ============================================================================
// Joule-Thomson coefficient (Van der Waals approximation)
// ============================================================================

// mu_JT = (1/Cp) · [T·(dV/dT)_P - V]
// For VdW gas: mu_JT ≈ (2a/RT - b) / Cp
inline double joule_thomson_vdw(double T, double a, double b, double Cp) {
    return (2.0 * a / (R_gas * T) - b) / Cp;
}

// Inversion temperature (VdW): T_inv = 2a/(Rb)
inline double inversion_temperature_vdw(double a, double b) {
    return 2.0 * a / (R_gas * b);
}

// ============================================================================
// Gas thermal conductivity (Eucken correction for polyatomics)
// ============================================================================

// k = mu · Cv · (9·gamma - 5) / 4   (Eucken formula)
inline double eucken_conductivity(double mu, double Cv, double gamma) {
    return mu * Cv * (9.0 * gamma - 5.0) / 4.0;
}

// ============================================================================
// Full thermal property report
// ============================================================================

struct ThermalReport {
    std::string formula;
    double T_K;
    DOFPartition dof;
    double Cv_calc;      // J/(mol·K) from DOF
    double Cp_calc;      // J/(mol·K) from DOF
    double gamma_calc;
    double Cv_tabulated; // from species database
    double Cp_tabulated;
    double gamma_tabulated;
    double c_sound;      // m/s
    double mu_JT;        // K/Pa (Joule-Thomson)
    double T_inversion;  // K

    std::string format() const;
};

} // namespace gas2
} // namespace vsepr
