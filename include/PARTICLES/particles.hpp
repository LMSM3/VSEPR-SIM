#pragma once
/**
 * particles.hpp  -  PARTICLES Library — umbrella header
 *
 * Organizes all particle-type, colour-mapping, trail, and overlay content
 * for transient and QCD particle visualization into one includable unit.
 *
 *   PARTICLES scope:
 *     - RGB colour triple
 *     - Transient particle type codes (-1..-6) and TransientParticle state
 *     - Deterministic colour palette + velocity-gradient heat ramp
 *     - Trail geometry (TrailPoint, taper, segment colour blend)
 *     - Transient overlay descriptors (TransientOverlay, make_overlay)
 *     - QCD particle types (ColorCharge, GluonChannel, QuarkFlavor)
 *     - QCD particle state (QCDParticle, QuarkGluonPlasma, QCDPopulationConfig)
 *     - QCD colour-charge to RGB mapping, confinement string colour
 *     - QCD overlay descriptors (QCDOverlay, make_qcd_overlay)
 *
 * All sub-headers are header-only with no Qt/GL/OS dependency.
 *
 * Quick include:
 *     #include "PARTICLES/particles.hpp"
 *
 * Fine-grained includes:
 *     #include "PARTICLES/rgb.hpp"
 *     #include "PARTICLES/transient_particle.hpp"
 *     #include "PARTICLES/trail.hpp"
 *     #include "PARTICLES/qcd_particle.hpp"
 *     #include "PARTICLES/qcd_colour.hpp"
 *
 * Relationship to legacy headers:
 *   This library aggregates and re-organizes the particle/vis content of:
 *     include/coarse_grain/vis/transient_renderer.hpp
 *     include/coarse_grain/vis/qcd_colour_charge.hpp
 *     include/coarse_grain/physics/qcd_transient.hpp  (particle types only)
 *     include/cli/system_state.hpp  (TransientTypeCode / TransientParticle)
 *
 *   Those headers remain in place for backward compatibility.
 *   Prefer including from PARTICLES/ for all new code.
 *
 * v5.1.3~2  |  WO-63A + WO-v513-QCD  |  v5.0.0-main
 */

#include "PARTICLES/rgb.hpp"
#include "PARTICLES/trail.hpp"
#include "PARTICLES/transient_particle.hpp"
#include "PARTICLES/qcd_particle.hpp"
#include "PARTICLES/qcd_colour.hpp"

// SM-channel identity layer (depends on PARTICLES types; included after them)
#include "IDENTITY/identity.hpp"
