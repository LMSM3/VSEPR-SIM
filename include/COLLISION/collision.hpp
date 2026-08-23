#pragma once
/**
 * collision.hpp  -  COLLISION Library — umbrella header
 *
 * Organizes all physics-layer content for particle interactions,
 * force accumulation, and timing into one includable unit.
 *
 *   COLLISION scope:
 *     - Cornell potential (Coulombic + linear string confinement)
 *     - SU(3) Casimir color factor
 *     - T_A|B dual-timescale scheduler
 *     - QCD force accumulation, velocity-Verlet integrator,
 *       hadronization gate, plasma seeding
 *     - Transient particle optimizer (ballistic integrate + bead capture)
 *
 * All sub-headers are header-only with no Qt/GL/OS dependency.
 *
 * Quick include:
 *     #include "COLLISION/collision.hpp"
 *
 * Fine-grained includes (for build-time isolation):
 *     #include "COLLISION/cornell.hpp"
 *     #include "COLLISION/casimir.hpp"
 *     #include "COLLISION/tab_scheduler.hpp"
 *     #include "COLLISION/qcd_force.hpp"
 *     #include "COLLISION/transient_optimizer.hpp"
 *
 * Relationship to legacy headers:
 *   This library aggregates and re-organizes the physics content of:
 *     include/coarse_grain/physics/qcd_transient.hpp
 *     include/cli/system_state.hpp (transient optimizer section)
 *
 *   Those headers remain in place for backward compatibility.
 *   Prefer including from COLLISION/ for all new code.
 *
 * v5.1.3~2  |  WO-v513-QCD  |  v5.0.0-main
 */

#include "COLLISION/cornell.hpp"
#include "COLLISION/casimir.hpp"
#include "COLLISION/tab_scheduler.hpp"
#include "COLLISION/qcd_force.hpp"
#include "COLLISION/transient_optimizer.hpp"
