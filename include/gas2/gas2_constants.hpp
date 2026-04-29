/**
 * gas2_constants.hpp
 * ------------------
 * Physical Constants and Unit Definitions for gas2 Module.
 *
 * CODATA 2018 recommended values. SI base units throughout.
 * All constants are constexpr for compile-time evaluation.
 *
 * Reference: include/core/energy_units.hpp (project canonical constants)
 *            NIST CODATA 2018 (https://physics.nist.gov/cuu/Constants/)
 */

#pragma once

#include <cmath>

namespace vsepr {
namespace gas2 {

// ============================================================================
// Fundamental Constants (CODATA 2018, exact where indicated)
// ============================================================================

constexpr double kB       = 1.380649e-23;       // J/K       — Boltzmann constant (exact)
constexpr double R_gas    = 8.314462618;         // J/(mol·K) — molar gas constant
constexpr double N_A      = 6.02214076e23;       // mol^-1    — Avogadro constant (exact)
constexpr double h_planck = 6.62607015e-34;      // J·s       — Planck constant (exact)
constexpr double c_light  = 299792458.0;         // m/s       — speed of light (exact)
constexpr double sigma_SB = 5.670374419e-8;      // W/(m^2·K^4) — Stefan-Boltzmann

// ============================================================================
// Conversion Factors
// ============================================================================

constexpr double atm_to_Pa   = 101325.0;         // Pa per atm
constexpr double bar_to_Pa   = 1.0e5;            // Pa per bar
constexpr double amu_to_kg   = 1.66053906660e-27; // kg per amu
constexpr double eV_to_J     = 1.602176634e-19;  // J per eV
constexpr double kcal_to_J   = 4184.0;           // J per kcal
constexpr double kJ_to_J     = 1000.0;           // J per kJ
constexpr double angstrom_to_m = 1.0e-10;        // m per Å
constexpr double nm_to_m     = 1.0e-9;           // m per nm
constexpr double Hartree_J   = 4.3597447222071e-18; // J per Eh (CODATA 2018)

// ============================================================================
// Mathematical Constants
// ============================================================================

constexpr double PI = 3.14159265358979323846;
constexpr double TWO_PI = 2.0 * PI;
constexpr double SQRT2 = 1.41421356237309504880;

} // namespace gas2
} // namespace vsepr
