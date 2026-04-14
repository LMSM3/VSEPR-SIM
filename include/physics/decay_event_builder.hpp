#pragma once
/**
 * decay_event_builder.hpp
 * =======================
 * Factory functions for common decay modes.
 *
 * Each builder populates a DecayEvent with:
 *   - emitted particle list
 *   - default energy partitioning fractions
 *   - damage score estimate
 *
 * Also provides constexpr daughter-state arithmetic for Z,A bookkeeping.
 *
 * Integration:
 *   NuclearState here is a lightweight transition-oriented struct.
 *   It does NOT replace ElementDescriptor (identity layer) or
 *   vsepr::nuclear::DecayNuclide (chain data).  It is deliberately
 *   minimal so that decay builders remain fast and composable.
 */

#include "decay_event.hpp"

namespace vsepr::physics {

// ============================================================================
// Lightweight nuclear state for transition arithmetic
// ============================================================================

struct NuclearState {
    std::int32_t species_id {0};
    int Z {0};
    int A {0};
};

struct DaughterState {
    int Z {0};
    int A {0};
};

// ============================================================================
// Daughter-state arithmetic (constexpr)
// ============================================================================

[[nodiscard]] constexpr DaughterState alpha_daughter(const NuclearState& s) noexcept {
    return DaughterState{ s.Z - 2, s.A - 4 };
}

[[nodiscard]] constexpr DaughterState beta_minus_daughter(const NuclearState& s) noexcept {
    return DaughterState{ s.Z + 1, s.A };
}

[[nodiscard]] constexpr DaughterState beta_plus_daughter(const NuclearState& s) noexcept {
    return DaughterState{ s.Z - 1, s.A };
}

// ============================================================================
// Decay event factories
// ============================================================================

/**
 * Alpha decay: S(Z,A) -> S(Z-2,A-4) + alpha
 * Heavy, short-range, high local damage.
 * Default fractions: deposited 0.85, transported 0.15, damage 0.90
 */
[[nodiscard]] inline DecayEvent make_alpha_decay(
    std::int32_t parent_species_id,
    std::int32_t daughter_species_id,
    double released_energy_eV,
    double event_time_s,
    double confidence
) {
    DecayEvent ev {};
    ev.parent_species_id   = parent_species_id;
    ev.daughter_species_id = daughter_species_id;
    ev.released_energy_eV  = released_energy_eV;
    ev.event_time_s        = event_time_s;
    ev.confidence          = confidence;
    ev.deposited_energy_fraction   = 0.85;
    ev.transported_energy_fraction = 0.15;
    ev.local_damage_score          = 0.90;
    (void)ev.add_emitted(ParticleID::alpha);
    return ev;
}

/**
 * Beta-minus decay
 * Light, medium range, moderate local damage.
 * Default fractions: deposited 0.35, transported 0.65, damage 0.30
 */
[[nodiscard]] inline DecayEvent make_beta_minus_decay(
    std::int32_t parent_species_id,
    std::int32_t daughter_species_id,
    double released_energy_eV,
    double event_time_s,
    double confidence
) {
    DecayEvent ev {};
    ev.parent_species_id   = parent_species_id;
    ev.daughter_species_id = daughter_species_id;
    ev.released_energy_eV  = released_energy_eV;
    ev.event_time_s        = event_time_s;
    ev.confidence          = confidence;
    ev.deposited_energy_fraction   = 0.35;
    ev.transported_energy_fraction = 0.65;
    ev.local_damage_score          = 0.30;
    (void)ev.add_emitted(ParticleID::beta_minus);
    (void)ev.add_emitted(ParticleID::antineutrino);
    return ev;
}

/**
 * Gamma decay (isomeric transition): S*(Z,A) -> S(Z,A) + gamma
 * Penetrating, low local damage, high transport fraction.
 * Default fractions: deposited 0.10, transported 0.90, damage 0.05
 */
[[nodiscard]] inline DecayEvent make_gamma_decay(
    std::int32_t parent_species_id,
    std::int32_t daughter_species_id,
    double released_energy_eV,
    double event_time_s,
    double confidence
) {
    DecayEvent ev {};
    ev.parent_species_id   = parent_species_id;
    ev.daughter_species_id = daughter_species_id;
    ev.released_energy_eV  = released_energy_eV;
    ev.event_time_s        = event_time_s;
    ev.confidence          = confidence;
    ev.deposited_energy_fraction   = 0.10;
    ev.transported_energy_fraction = 0.90;
    ev.local_damage_score          = 0.05;
    (void)ev.add_emitted(ParticleID::gamma);
    return ev;
}

} // namespace vsepr::physics
