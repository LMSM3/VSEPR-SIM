#pragma once
/**
 * kinetic_event_kind.hpp
 * ======================
 * Universal kinetic event classification.
 *
 * Every transition the engine schedules — whether nuclear decay,
 * particle transport, molecular collision, or surface adsorption —
 * is tagged with one of these kinds.
 *
 * Integration:
 *   This enum is the Layer C classifier.  It sits above the
 *   physics-specific event payloads (Layer B) and below the
 *   kinetic scheduler (Layer C proper).
 *
 *   Compatible with atomistic::kinetic::EventType
 *   (formation_kinetics.hpp) which uses a finer 28-type taxonomy
 *   for formation-specific events.  EventKind here is coarser
 *   and universal — a decay event that formation_kinetics labels
 *   as EventType::DECAY maps to EventKind::decay here.
 */

#include <cstdint>

namespace vsepr::kinetics {

enum class EventKind : std::int32_t {
    unknown        = 0,
    decay,
    transport,
    collision,
    adsorption,
    desorption,
    coalescence,
    nucleation,
    deposition,
    restructuring,
    phase_change
};

} // namespace vsepr::kinetics
