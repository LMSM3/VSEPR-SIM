#pragma once
/**
 * decay_event.hpp
 * ===============
 * Deterministic decay event record — captures a single nuclear decay
 * transition with parent/daughter IDs, emitted particles, energy
 * partitioning, and damage scoring.
 *
 * Integration:
 *   Emitted particles are identified by vsepr::physics::ParticleID.
 *   Parent/daughter species use engine-assigned species IDs (positive
 *   integers that map to ElementDescriptor or isotope tables).
 *
 * Design rule (anti-black-box):
 *   Every fraction, every emitted particle, and every energy value is
 *   explicit and inspectable.  No hidden defaults once populated.
 */

#include "particle_id.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace vsepr::physics {

struct DecayEvent {
    static constexpr std::size_t max_emitted_particles = 8;

    std::int32_t parent_species_id  {0};
    std::int32_t daughter_species_id {0};

    std::array<ParticleID, max_emitted_particles> emitted_particle_ids {};
    std::size_t emitted_count {0};

    double released_energy_eV          {0.0};
    double deposited_energy_fraction   {0.0};
    double transported_energy_fraction {0.0};
    double local_damage_score          {0.0};

    double event_time_s {0.0};
    double confidence   {0.0};

    [[nodiscard]] bool add_emitted(ParticleID id) noexcept {
        if (emitted_count >= emitted_particle_ids.size()) {
            return false;
        }
        emitted_particle_ids[emitted_count++] = id;
        return true;
    }
};

} // namespace vsepr::physics
