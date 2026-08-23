#pragma once
/**
 * transient_optimizer.hpp  -  Transient Particle Capture Optimizer
 *
 * The transient optimizer sidecar (WO-63A) runs alongside the bead kernel.
 * It integrates ballistic transient particles (electrons, ions, neutrons, etc.)
 * and captures them to beads when they pass within a threshold radius,
 * applying charge and mass deltas to the bead field.
 *
 * This is the COLLISION library extraction of the logic previously inline
 * inside CGSystemState::run_transient_optimizer_substeps().
 *
 * Parameters:
 *   capture_radius   — Å; any charged transient within this distance
 *                      of a bead is absorbed and deactivated
 *   substeps         — K inner substeps per outer T_B call
 *   dt               — timestep for ballistic integration (fs)
 *
 * Source: extracted from include/cli/system_state.hpp
 * v5.1.3~2  |  WO-63A  |  v5.0.0-main
 */

#include "PARTICLES/transient_particle.hpp"
#include "atomistic/core/state.hpp"

#include <cmath>
#include <limits>
#include <vector>

namespace vsepr {
namespace collision {

// ============================================================================
// Transient optimizer parameters
// Previously hard-coded literals in system_state.hpp — promoted to a struct
// so callers can tune per-scene.
// ============================================================================

struct TransientOptimizerParams {
	double capture_radius{0.8};  // Å — absorption threshold (charged only)
	int    substeps{100};         // K inner substeps per T_B call
	double dt{0.01};              // fs per substep
};

// ============================================================================
// Bead proxy: minimal bead interface required by the optimizer.
// The caller provides references to these arrays; no bead type dependency.
// ============================================================================

struct BeadProxy {
	double x{}, y{}, z{};  // position (Å)
	double charge{};
	double mass{};
};

// ============================================================================
// run_transient_optimizer
//
// Integrates all active transient particles forward by K substeps.
// Charged particles are absorbed by the nearest bead within capture_radius.
// Neutral particles (neutron-like, gamma-like) remain ballistic; no capture.
//
// charge_delta and mass_delta accumulate deltas and are applied to beads
// at the end of each call (zeroed afterward).
//
// Returns the number of capture events that occurred.
// ============================================================================

inline int run_transient_optimizer(
	std::vector<particles::TransientParticle>& transients,
	std::vector<BeadProxy>&                    beads,
	std::vector<double>&                       charge_delta,
	std::vector<double>&                       mass_delta,
	const TransientOptimizerParams&            params)
{
	if (transients.empty() || beads.empty()) return 0;

	const int    K        = (params.substeps > 0) ? params.substeps : 1;
	const double dt_sub   = (params.dt > 1e-9) ? params.dt : 1e-4;
	const double cap_r2   = params.capture_radius * params.capture_radius;

	int capture_count = 0;

	for (int sub = 0; sub < K; ++sub) {
		for (auto& tp : transients) {
			if (!tp.active) continue;

			// Ballistic step
			tp.position.x += tp.velocity.x * dt_sub;
			tp.position.y += tp.velocity.y * dt_sub;
			tp.position.z += tp.velocity.z * dt_sub;

			// Neutral transients: ballistic only, no capture
			if (particles::transient_is_neutral(tp.type_code)) continue;

			// Charged transient: find nearest bead
			double best_d2 = std::numeric_limits<double>::max();
			int    best_i  = -1;
			for (int i = 0; i < static_cast<int>(beads.size()); ++i) {
				double dx = tp.position.x - beads[i].x;
				double dy = tp.position.y - beads[i].y;
				double dz = tp.position.z - beads[i].z;
				double d2 = dx*dx + dy*dy + dz*dz;
				if (d2 < best_d2) { best_d2 = d2; best_i = i; }
			}

			if (best_i >= 0 && best_d2 < cap_r2) {
				charge_delta[static_cast<size_t>(best_i)] += tp.charge;
				mass_delta  [static_cast<size_t>(best_i)] += tp.mass;
				tp.active = false;
				++capture_count;
			}
		}
	}

	// Apply accumulated deltas
	for (size_t i = 0; i < beads.size(); ++i) {
		beads[i].charge += charge_delta[i];
		beads[i].mass    = (beads[i].mass + mass_delta[i] > 0.0)
						   ? beads[i].mass + mass_delta[i] : 0.0;
		charge_delta[i] = 0.0;
		mass_delta  [i] = 0.0;
	}

	return capture_count;
}

} // namespace collision
} // namespace vsepr
