#pragma once
/**
 * element_descriptor.hpp
 * ======================
 * Immutable chemical element reference data.
 *
 * This is the *identity layer* — pure factual data about an element.
 * No simulation state, no mutation, no index coupling.
 *
 * Integration:
 *   Uses PeriodicTable (pot/periodic_db.hpp) as upstream data source
 *   when available. Can also be populated from ComprehensiveElementData.
 *
 * Design rule (anti-black-box):
 *   Every field is inspectable, every value deterministic.
 *   No hidden defaults. If a value is unknown, it is explicitly zero.
 */

#include <cstdint>
#include <string>

namespace vsepr {

struct ElementDescriptor {
    int         atomic_number       = 0;        // Z
    std::string symbol;                         // "H", "C", "Pu"
    std::string name;                           // "Hydrogen", "Carbon", "Plutonium"
    double      atomic_mass         = 0.0;      // u (unified atomic mass)
    int         valence_hint        = 0;        // Typical valence (e.g. 4 for C)
    double      covalent_radius     = 0.0;      // Å
    double      vdw_radius          = 0.0;      // Å (van der Waals)
    double      electronegativity   = 0.0;      // Pauling scale (0 = unknown)
    bool        radioactive         = false;     // True for Tc, Pm, Po, At, Rn, Fr-Og

    // Optional isotope fields (behind feature flag or filled when relevant)
    int         mass_number         = 0;        // A (0 = natural abundance mixture)
    int         neutron_number      = 0;        // N (0 = not specified)
    double      decay_clock         = 0.0;      // Half-life countdown (seconds, 0 = stable)
    bool        metastable          = false;     // Nuclear isomer flag
    std::string isotope_label;                  // "Pu-239", "U-235", "" = natural

    bool is_valid() const { return atomic_number > 0 && atomic_number <= 118; }

    bool operator==(const ElementDescriptor& o) const {
        return atomic_number == o.atomic_number && mass_number == o.mass_number;
    }
    bool operator!=(const ElementDescriptor& o) const { return !(*this == o); }
};

} // namespace vsepr
