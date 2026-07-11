#pragma once
/**
 * energy_units.hpp  —  Canonical Energy Unit System
 * ==================================================
 * VSEPR-SIM 3.0.1
 *
 * Design principle:
 *   Store energy once in a canonical unit (Hartree), then expose
 *   views in multiple unit systems.  No subsystem invents its own
 *   religion — one clean engine, three windows into the result.
 *
 * Unit convention:
 *   Hartree  = computation layer     (quantum kernels, scoring)
 *   kcal/mol = chemistry layer       (reports, MD, force fields)
 *   kJ/mol   = chemistry layer       (SI-facing, European convention)
 *   eV       = materials/physics     (band gaps, defects, surfaces)
 *
 * Constants (CODATA 2018, NIST):
 *   1 Hartree  = 27.211386245988  eV
 *   1 Hartree  = 627.509474       kcal/mol
 *   1 Hartree  = 2625.49962       kJ/mol
 *   kB T(298K) = 0.000950044      Hartree
 *   kB T(298K) = 0.02569 eV  =  0.5922 kcal/mol  =  2.479 kJ/mol
 *
 * Anti-black-box:
 *   Every conversion factor is a named constant.
 *   Every value can be inspected in any unit at any time.
 */

#include <string>
#include <cstdint>
#include <cmath>
#include <limits>

namespace vsepr {

// ============================================================================
// Energy Unit Enumeration
// ============================================================================

enum class EnergyUnit : uint8_t {
    Hartree  = 0,
    EV       = 1,
    KcalMol  = 2,
    KJMol    = 3
};

// Number of energy units (for iteration)
constexpr int ENERGY_UNIT_COUNT = 4;

// ============================================================================
// CODATA Conversion Constants
// ============================================================================

namespace energy_const {
    // Hartree -> X
    constexpr double HARTREE_TO_EV       = 27.211386245988;
    constexpr double HARTREE_TO_KCALMOL  = 627.509474;
    constexpr double HARTREE_TO_KJMOL    = 2625.49962;

    // X -> Hartree (reciprocals)
    constexpr double EV_TO_HARTREE       = 1.0 / HARTREE_TO_EV;
    constexpr double KCALMOL_TO_HARTREE  = 1.0 / HARTREE_TO_KCALMOL;
    constexpr double KJMOL_TO_HARTREE    = 1.0 / HARTREE_TO_KJMOL;

    // Thermal energy at 298.15 K
    constexpr double KB_T_298_HARTREE    = 0.000950044;
    constexpr double KB_T_298_EV         = 0.025693;
    constexpr double KB_T_298_KCALMOL    = 0.592186;
    constexpr double KB_T_298_KJMOL      = 2.47897;

    // Boltzmann constant
    constexpr double KB_HARTREE_PER_K    = 3.1668115634556e-6;
    constexpr double KB_EV_PER_K         = 8.617333262145e-5;

    // Avogadro's number
    constexpr double AVOGADRO            = 6.02214076e23;

    // Atomic mass unit in grams
    constexpr double AMU_TO_GRAMS        = 1.66053906660e-24;
}

// ============================================================================
// Unit label strings
// ============================================================================

inline const char* energy_unit_str(EnergyUnit u) {
    switch (u) {
        case EnergyUnit::Hartree: return "Hartree";
        case EnergyUnit::EV:      return "eV";
        case EnergyUnit::KcalMol: return "kcal/mol";
        case EnergyUnit::KJMol:   return "kJ/mol";
    }
    return "unknown";
}

inline const char* energy_unit_symbol(EnergyUnit u) {
    switch (u) {
        case EnergyUnit::Hartree: return "Ha";
        case EnergyUnit::EV:      return "eV";
        case EnergyUnit::KcalMol: return "kcal/mol";
        case EnergyUnit::KJMol:   return "kJ/mol";
    }
    return "?";
}

// ============================================================================
// Conversion: from any unit to Hartree (canonical)
// ============================================================================

inline double to_hartree(double value, EnergyUnit from) {
    switch (from) {
        case EnergyUnit::Hartree: return value;
        case EnergyUnit::EV:      return value * energy_const::EV_TO_HARTREE;
        case EnergyUnit::KcalMol: return value * energy_const::KCALMOL_TO_HARTREE;
        case EnergyUnit::KJMol:   return value * energy_const::KJMOL_TO_HARTREE;
    }
    return value;
}

// Conversion: from Hartree to any unit
inline double from_hartree(double hartree, EnergyUnit to) {
    switch (to) {
        case EnergyUnit::Hartree: return hartree;
        case EnergyUnit::EV:      return hartree * energy_const::HARTREE_TO_EV;
        case EnergyUnit::KcalMol: return hartree * energy_const::HARTREE_TO_KCALMOL;
        case EnergyUnit::KJMol:   return hartree * energy_const::HARTREE_TO_KJMOL;
    }
    return hartree;
}

// General conversion: from any unit to any unit
inline double convert_energy(double value, EnergyUnit from, EnergyUnit to) {
    if (from == to) return value;
    return from_hartree(to_hartree(value, from), to);
}

// ============================================================================
// Energy  —  canonical energy value with multi-unit access
// ============================================================================

struct Energy {
    double value_hartree = 0.0;

    // ── Construction ──

    Energy() = default;
    explicit Energy(double ha) : value_hartree(ha) {}

    // Construct from any unit
    static Energy from(double value, EnergyUnit unit) {
        return Energy(to_hartree(value, unit));
    }

    static Energy from_hartree(double ha)    { return Energy(ha); }
    static Energy from_ev(double ev)         { return from(ev, EnergyUnit::EV); }
    static Energy from_kcalmol(double kcal)  { return from(kcal, EnergyUnit::KcalMol); }
    static Energy from_kjmol(double kj)      { return from(kj, EnergyUnit::KJMol); }

    // ── Multi-unit access ──

    double as(EnergyUnit u) const {
        return vsepr::from_hartree(value_hartree, u);
    }

    double as_hartree() const { return value_hartree; }
    double as_ev()      const { return as(EnergyUnit::EV); }
    double as_kcalmol() const { return as(EnergyUnit::KcalMol); }
    double as_kjmol()   const { return as(EnergyUnit::KJMol); }

    // ── Thermal accessibility ──

    // How many kB*T at a given temperature?
    double thermal_ratio(double T_kelvin = 298.15) const {
        double kbt = energy_const::KB_HARTREE_PER_K * T_kelvin;
        return (kbt > 0.0) ? std::abs(value_hartree) / kbt : 0.0;
    }

    // Is this energy thermally accessible at T? (within n * kBT)
    bool thermally_accessible(double T_kelvin = 298.15, double n_sigma = 1.0) const {
        return thermal_ratio(T_kelvin) <= n_sigma;
    }

    // ── Arithmetic ──

    Energy operator+(const Energy& o) const { return Energy(value_hartree + o.value_hartree); }
    Energy operator-(const Energy& o) const { return Energy(value_hartree - o.value_hartree); }
    Energy operator*(double s)        const { return Energy(value_hartree * s); }
    Energy operator/(double s)        const { return Energy(value_hartree / s); }
    Energy operator-()                const { return Energy(-value_hartree); }

    Energy& operator+=(const Energy& o) { value_hartree += o.value_hartree; return *this; }
    Energy& operator-=(const Energy& o) { value_hartree -= o.value_hartree; return *this; }
    Energy& operator*=(double s)        { value_hartree *= s; return *this; }

    // ── Comparison ──

    bool operator<(const Energy& o)  const { return value_hartree < o.value_hartree; }
    bool operator>(const Energy& o)  const { return value_hartree > o.value_hartree; }
    bool operator<=(const Energy& o) const { return value_hartree <= o.value_hartree; }
    bool operator>=(const Energy& o) const { return value_hartree >= o.value_hartree; }

    bool is_zero() const { return value_hartree == 0.0; }
    bool is_negative() const { return value_hartree < 0.0; }
    bool is_bound() const { return value_hartree < 0.0; }

    // ── Formatting ──

    // Format as "value unit" string
    std::string format(EnergyUnit u, int precision = 6) const;

    // Format in all four units
    std::string format_all(int precision = 6) const;
};

// Scalar * Energy
inline Energy operator*(double s, const Energy& e) { return e * s; }

// ============================================================================
// kB*T helper
// ============================================================================

inline Energy kbt(double T_kelvin = 298.15) {
    return Energy(energy_const::KB_HARTREE_PER_K * T_kelvin);
}

} // namespace vsepr
