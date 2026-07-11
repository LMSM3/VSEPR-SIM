#pragma once
// =============================================================================
// src/core/stats/energy_drift.hpp
// =============================================================================
// Tracks total-energy drift over the course of a simulation run.
//
// Metrics:
//   E_0               — baseline energy at first push (or explicit set_baseline)
//   E_total(t)        — current total energy
//   delta_E(t)        — E_total(t) - E_0
//   relative_drift(t) — delta_E(t) / |E_0|   (dimensionless)
//
// Why this matters:
//   If relative_drift grows without bound, the integrator, timestep, force
//   signs, or unit scaling is broken.  None of the "emergent property" claims
//   that follow are credible until this is flat.
//
// Anti-black-box: all fields public, all math explicit.
// =============================================================================

#include "online_stats.hpp"
#include <cmath>
#include <limits>
#include <cstddef>

namespace vsepr {

struct EnergyDriftTracker {

	// -------------------------------------------------------------------------
	// State (all public — inspectable at any time)
	// -------------------------------------------------------------------------

	double   E_0          = std::numeric_limits<double>::quiet_NaN();
	double   E_current    = 0.0;
	double   delta_E      = 0.0;           // E_current - E_0
	double   rel_drift    = 0.0;           // delta_E / |E_0|
	uint64_t frame        = 0;

	OnlineStats drift_stats;               // running stats on delta_E

	// -------------------------------------------------------------------------
	// Configuration
	// -------------------------------------------------------------------------

	// Tolerance below which |E_0| is treated as zero (avoids /0).
	double zero_energy_eps = 1e-12;

	// -------------------------------------------------------------------------
	// Interface
	// -------------------------------------------------------------------------

	/// Feed the current total energy.
	/// The first call sets E_0 as the baseline.
	void push(double E_total) noexcept {
		E_current = E_total;

		if (frame == 0) {
			E_0 = E_total;
		}

		delta_E = E_current - E_0;
		drift_stats.push(delta_E);

		const double denom = std::abs(E_0);
		rel_drift = (denom > zero_energy_eps)
			? (delta_E / denom)
			: 0.0;

		++frame;
	}

	/// Explicitly set a new baseline (e.g. after equilibration).
	void set_baseline(double E_ref) noexcept {
		E_0       = E_ref;
		delta_E   = E_current - E_0;
		const double denom = std::abs(E_0);
		rel_drift = (denom > zero_energy_eps)
			? (delta_E / denom)
			: 0.0;
		drift_stats.reset();
	}

	/// True when |relative_drift| exceeds the given threshold.
	bool is_drifting(double threshold = 0.01) const noexcept {
		return std::abs(rel_drift) > threshold;
	}

	/// Reset tracker to empty state.
	void reset() noexcept {
		E_0       = std::numeric_limits<double>::quiet_NaN();
		E_current = 0.0;
		delta_E   = 0.0;
		rel_drift = 0.0;
		frame     = 0;
		drift_stats.reset();
	}
};

} // namespace vsepr
