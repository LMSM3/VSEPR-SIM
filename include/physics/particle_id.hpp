#pragma once
/**
 * particle_id.hpp
 * ===============
 * Unified species code namespace for the VSEPR-SIM engine.
 *
 * The species_code is the single integer that tags every row in the
 * geometry table (and every entity the engine tracks):
 *
 *   species_code >= 0   standard atom/species templates
 *                        0..108  element Z (H=1 .. Og=118, 0=placeholder)
 *                        > 118   engine-assigned molecule / candidate IDs
 *
 *   species_code <  0   reserved class space — nonstandard or derived
 *                        entities that have coordinates but are not
 *                        ordinary atoms
 *
 * Reserved negative ladder
 * ─────────────────────────────────────────────────────────────────────
 * Physical emission / decay particles
 *   -1   alpha               He-4 nucleus emitted in alpha decay
 *   -2   beta_minus          electron emitted in beta-minus decay
 *   -3   gamma               gamma photon (isomeric transition)
 *   -4   eta                 QMD energy packet — abstract high-definition
 *                             energy carrier for QMD simulations; extra
 *                             to the standard kernel energy unit
 *   -5   antineutrino        antineutrino loss token (beta-minus partner)
 *   -6   neutrino            neutrino loss token (beta-plus / EC partner)
 *   -7   neutron_free        free neutron (prompt emission or transport)
 *   -8   proton_free         free proton (proton emission or transport)
 *
 * Energy / field / abstract carriers
 *   -9   excitation_packet   internal electronic excitation carrier
 *  -10   ionization_event    ionization bookkeeping token
 *  -11   heat_packet         phonon / thermal energy packet
 *  -12   charge_cloud        distributed charge carrier
 *  -13   field_source        field-origin sentinel
 *  -14   field_probe         field-sampling sentinel
 *
 * Defect / structural / bookkeeping entities
 *  -15   vacancy             lattice vacancy defect token
 *  -16   interstitial        lattice interstitial defect token
 *  -17   ghost               ghost particle — virtual, massless positional
 *                             sentinel for field evaluation, constraint
 *                             anchoring, or higher-order virtual entities
 *  -18   transition_state    transition-state marker (reaction coordinate)
 * ─────────────────────────────────────────────────────────────────────
 *
 * XYZ geometry table integration:
 *   Every row carries a species_code column.  Readers and writers use
 *   species_code_to_label() to produce human-readable atom labels without
 *   a separate lookup table.  Negative codes print as their canonical
 *   name string rather than an element symbol.
 *
 *   sample_id  bead_id  local_id  species_code  label   x       y       z
 *   17         0        0          6             C       0.000   1.402   0.000
 *   17         0        1          1             H       1.050   2.100   0.000
 *   17         1        0         -1             alpha   2.500   0.300   0.200
 *   17         1        1         -3             gamma   2.700   0.300   0.200
 *
 * Integration:
 *   Works alongside vsepr::nuclear::DecayMode  (decay_chains.hpp)
 *   and atomistic::nuclear::DecayType           (nuclear_stability.hpp).
 *   Those enums classify HOW something decays; species_code identifies
 *   WHAT was emitted as a first-class geometry-table entity.
 *
 * Design rule (anti-black-box):
 *   Every code is constexpr-queryable, every label deterministic.
 */

#include <string_view>
#include <cstdint>

namespace vsepr::physics {

// ============================================================================
// Canonical species code type
// ============================================================================

// species_code is std::int32_t throughout the engine.
// The enum names the reserved negative entries; positive values
// (atom Z numbers and molecule IDs) are plain int32_t literals.

enum class ParticleID : std::int32_t {
    // Nonnegative: ordinary matter (Z or engine-assigned molecule ID)
    placeholder            =  0,   // generic / unresolved species

    // ── Physical emission / decay particles ─────────────────────────────
    alpha                  = -1,   // He-4 nucleus
    beta_minus             = -2,   // electron (beta-minus decay)
    gamma                  = -3,   // gamma photon
    eta                    = -4,   // QMD energy packet (high-definition
                                   // abstract energy carrier)
    antineutrino           = -5,   // antineutrino loss token
    neutrino               = -6,   // neutrino loss token
    neutron_free           = -7,   // free neutron
    proton_free            = -8,   // free proton

    // ── Energy / field / abstract carriers ──────────────────────────────
    excitation_packet      = -9,   // electronic excitation carrier
    ionization_event       = -10,  // ionization bookkeeping token
    heat_packet            = -11,  // phonon / thermal packet
    charge_cloud           = -12,  // distributed charge carrier
    field_source           = -13,  // field-origin sentinel
    field_probe            = -14,  // field-sampling sentinel

    // ── Defect / structural / bookkeeping entities ───────────────────────
    vacancy                = -15,  // lattice vacancy
    interstitial           = -16,  // lattice interstitial
    ghost                  = -17,  // ghost particle — virtual massless
                                   // sentinel (field eval, constraints,
                                   // higher-order virtual entities)
    transition_state       = -18   // transition-state marker
};

// ============================================================================
// Sub-range predicates  (all constexpr noexcept)
// ============================================================================

/// True for the full engine-reserved range [0, -99].
[[nodiscard]] constexpr bool is_reserved(ParticleID id) noexcept {
    const auto v = static_cast<std::int32_t>(id);
    return (v <= 0 && v >= -99);
}

/// True for physical decay / emission particles [-1, -8].
[[nodiscard]] constexpr bool is_decay_particle(ParticleID id) noexcept {
    const auto v = static_cast<std::int32_t>(id);
    return (v >= -8 && v <= -1);
}

/// True for particles that physically propagate through matter.
[[nodiscard]] constexpr bool is_transportable(ParticleID id) noexcept {
    switch (id) {
        case ParticleID::alpha:
        case ParticleID::beta_minus:
        case ParticleID::gamma:
        case ParticleID::neutron_free:
        case ParticleID::proton_free:
            return true;
        default:
            return false;
    }
}

/// True for bookkeeping-only tokens that are never transported.
[[nodiscard]] constexpr bool is_bookkeeping_only(ParticleID id) noexcept {
    switch (id) {
        case ParticleID::eta:
        case ParticleID::antineutrino:
        case ParticleID::neutrino:
        case ParticleID::excitation_packet:
        case ParticleID::ionization_event:
        case ParticleID::heat_packet:
        case ParticleID::charge_cloud:
        case ParticleID::field_source:
        case ParticleID::field_probe:
        case ParticleID::transition_state:
            return true;
        default:
            return false;
    }
}

/// True for energy / field abstract carriers [-9, -14].
[[nodiscard]] constexpr bool is_energy_carrier(ParticleID id) noexcept {
    const auto v = static_cast<std::int32_t>(id);
    return (v >= -14 && v <= -9);
}

/// True for defect / structural / bookkeeping entities [-15, -18].
[[nodiscard]] constexpr bool is_defect_or_virtual(ParticleID id) noexcept {
    const auto v = static_cast<std::int32_t>(id);
    return (v >= -18 && v <= -15);
}

/// True specifically for the ghost particle sentinel.
[[nodiscard]] constexpr bool is_ghost(ParticleID id) noexcept {
    return id == ParticleID::ghost;
}

// ============================================================================
// Label for geometry table output
// ============================================================================

/// Returns the canonical label string used in XYZ geometry table rows.
/// Positive species codes (atom Z) are NOT handled here — the caller
/// maps Z → element symbol via the periodic table.
[[nodiscard]] constexpr std::string_view to_string(ParticleID id) noexcept {
    switch (id) {
        case ParticleID::placeholder:         return "X";
        case ParticleID::alpha:               return "alpha";
        case ParticleID::beta_minus:          return "beta-";
        case ParticleID::gamma:               return "gamma";
        case ParticleID::eta:                 return "eta";
        case ParticleID::antineutrino:        return "antineutrino";
        case ParticleID::neutrino:            return "neutrino";
        case ParticleID::neutron_free:        return "neutron";
        case ParticleID::proton_free:         return "proton";
        case ParticleID::excitation_packet:   return "excitation";
        case ParticleID::ionization_event:    return "ionization";
        case ParticleID::heat_packet:         return "heat";
        case ParticleID::charge_cloud:        return "charge_cloud";
        case ParticleID::field_source:        return "field_source";
        case ParticleID::field_probe:         return "field_probe";
        case ParticleID::vacancy:             return "vacancy";
        case ParticleID::interstitial:        return "interstitial";
        case ParticleID::ghost:               return "ghost";
        case ParticleID::transition_state:    return "transition_state";
        default:                              return "unknown";
    }
}

// ============================================================================
// species_code helpers (raw int32_t interface for geometry table I/O)
// ============================================================================

/// Convert a raw species_code integer to its geometry-table label.
/// For positive codes (atom Z), returns the placeholder "?" — callers
/// must resolve element symbols separately.
[[nodiscard]] constexpr std::string_view species_code_to_label(std::int32_t code) noexcept {
    if (code > 0) return "?";   // caller resolves Z -> element symbol
    return to_string(static_cast<ParticleID>(code));
}

/// True if the species_code is a standard atom template (Z in [1, 118]).
[[nodiscard]] constexpr bool is_atom_species(std::int32_t code) noexcept {
    return (code >= 1 && code <= 118);
}

/// True if the species_code is a reserved engine entity (code <= 0).
[[nodiscard]] constexpr bool is_reserved_code(std::int32_t code) noexcept {
    return (code <= 0 && code >= -99);
}

} // namespace vsepr::physics
