#pragma once
/**
 * reported_quantity.hpp  —  Physical Quantity Reporting Layer
 * ============================================================
 * VSEPR-SIM 3.0.1
 *
 * Reporting revision:
 *   Any subsystem using internal unitless or normalized representations
 *   must convert final material quantities back into physical reporting
 *   units.  At minimum, outputs include:
 *
 *     m_g       — total mass in grams
 *     E_Ha      — energy in Hartrees (with multi-unit views)
 *     Z_count   — atomic-number-resolved composition summary
 *     n_mol     — amount of substance in moles
 *
 *   Internal reduced forms are permitted only inside the kernel and
 *   must NOT be the final reported state.
 *
 * Design:
 *   ReportedQuantity is the co-report struct attached to any tracked
 *   object, species, cluster, or assembled structure before export.
 *
 *   The internal solver may operate on nondimensionalized state
 *   variables for stability and performance.  The reporting layer
 *   remaps these values into physically interpretable outputs.
 *
 * Anti-black-box:
 *   Every field explicit, every conversion traceable, no hidden
 *   normalization in final outputs.
 */

#include "core/energy_units.hpp"

#include <string>
#include <map>
#include <vector>
#include <cstdint>
#include <cmath>

namespace vsepr {

// ============================================================================
// Z-count composition table
// ============================================================================

using ZCountMap = std::map<int, std::size_t>;   // Z -> count

// ============================================================================
// ReportedQuantity  —  physical quantities for any tracked object
// ============================================================================

struct ReportedQuantity {
    // ── Required report fields (per specification) ──

    double       mass_g          = 0.0;     // Total mass in grams
    Energy       energy;                     // Canonical energy (Hartree internal)
    double       amount_mol      = 0.0;     // Amount of substance in moles
    ZCountMap    z_count;                    // Atomic number -> count

    // ── Optional metadata ──

    std::string  label;                      // Human-readable name ("water", "Ar cluster")
    std::string  formula;                    // Chemical formula ("H2O", "Ar4")
    double       molar_mass_g_per_mol = 0.0; // Computed or provided molar mass

    // ── Derived accessors ──

    // Total atom count
    std::size_t total_atoms() const {
        std::size_t n = 0;
        for (const auto& [z, count] : z_count) n += count;
        return n;
    }

    // Number of distinct elements
    std::size_t element_count() const {
        return z_count.size();
    }

    // Energy in any unit
    double energy_as(EnergyUnit u) const {
        return energy.as(u);
    }

    // Is this a valid/populated report?
    bool is_valid() const {
        return !z_count.empty() && mass_g > 0.0;
    }

    // ── Construction helpers ──

    // Build from a flat list of atomic numbers and a per-atom mass table
    static ReportedQuantity from_composition(
        const std::vector<int>& atomic_numbers,
        const std::vector<double>& atomic_masses_amu,
        const Energy& total_energy = Energy());

    // Build from Z-count map and per-element mass table
    static ReportedQuantity from_z_count(
        const ZCountMap& composition,
        const std::vector<double>& mass_table_amu,
        const Energy& total_energy = Energy());

    // ── Formatting ──

    // Render a full report block (multi-line, human-readable)
    std::string format_report(int precision = 6) const;

    // Render a compact one-line summary
    std::string format_summary() const;

    // Render the Z-count table as "6×C, 12×H, 6×O" etc.
    std::string format_z_count() const;

    // Render energy in all four units
    std::string format_energy_all(int precision = 6) const;
};

// ============================================================================
// Moles / mass helper functions
// ============================================================================

// Compute moles from mass and molar mass
inline double compute_moles(double mass_g, double molar_mass_g_per_mol) {
    return (molar_mass_g_per_mol > 0.0)
        ? mass_g / molar_mass_g_per_mol
        : 0.0;
}

// Compute mass from moles and molar mass
inline double compute_mass_g(double moles, double molar_mass_g_per_mol) {
    return moles * molar_mass_g_per_mol;
}

// Compute mass from atom count and atomic mass in amu
inline double atoms_to_grams(std::size_t count, double atomic_mass_amu) {
    return static_cast<double>(count) * atomic_mass_amu * energy_const::AMU_TO_GRAMS;
}

// Compute moles from atom count
inline double atoms_to_moles(std::size_t count) {
    return static_cast<double>(count) / energy_const::AVOGADRO;
}

// Compute molar mass from Z-count and per-element mass table
inline double compute_molar_mass(const ZCountMap& z_count,
                                  const std::vector<double>& mass_table_amu) {
    double mm = 0.0;
    for (const auto& [z, count] : z_count) {
        if (z > 0 && z <= static_cast<int>(mass_table_amu.size())) {
            mm += static_cast<double>(count) * mass_table_amu[z - 1];
        }
    }
    return mm;
}

// ============================================================================
// Standard atomic masses (Z=1-118, natural abundance)
// ============================================================================
//
// Returns a reference to a static table indexed [0..117] where index = Z-1.
// Source: IUPAC 2021 standard atomic weights.
//
const std::vector<double>& standard_atomic_masses_amu();

// ============================================================================
// Element symbol lookup (Z -> symbol string)
// ============================================================================

const char* element_symbol(int Z);

} // namespace vsepr
