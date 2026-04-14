#pragma once
/**
 * kinetic_event_record.hpp
 * ========================
 * Universal kinetic event record with variant payload.
 *
 * Every scheduled event in the engine is wrapped in a
 * KineticEventRecord that carries:
 *   - EventKind    (coarse classification)
 *   - intensity    (transition rate or probability)
 *   - barrier_score (activation energy proxy)
 *   - environment_multiplier (medium / temperature correction)
 *   - EventPayload (type-safe physics-specific data)
 *
 * The payload is a std::variant so that a single kinetic engine
 * can dispatch decay events, transport events, and future event
 * types without casting or union hacks.
 *
 * Integration:
 *   Layer A: particle_id.hpp   (identity)
 *   Layer B: decay_event.hpp   (physics payload)
 *   Layer C: this file         (kinetics wrapper)
 */

#include "../physics/decay_event.hpp"
#include "kinetic_event_kind.hpp"

#include <variant>
#include <cstdint>

namespace vsepr::kinetics {

// ============================================================================
// Generic transport event (placeholder for future expansion)
// ============================================================================

struct GenericTransportEvent {
    int source_id {0};
    int target_id {0};
    double time_s {0.0};
    double confidence {0.0};
};

// ============================================================================
// Event payload — type-safe union of all physics-specific events
// ============================================================================

using EventPayload = std::variant<
    vsepr::physics::DecayEvent,
    GenericTransportEvent
>;

// ============================================================================
// Universal kinetic event record
// ============================================================================

struct KineticEventRecord {
    EventKind kind {EventKind::unknown};
    double intensity {0.0};
    double barrier_score {0.0};
    double environment_multiplier {1.0};
    EventPayload payload {};
};

} // namespace vsepr::kinetics
