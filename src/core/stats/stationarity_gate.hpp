#pragma once
// =============================================================================
// src/core/stats/stationarity_gate.hpp — First Stationarity Gate (Day #56)
// =============================================================================
// Determines when a running simulation has reached a stationary regime
// based on recent metric statistics — not vibes, not arbitrary step counts.
//
// Design: "almost stupid first version" as specified.
//   Gate OPENS when:
//     - Enough samples have accumulated (>= min_samples)
//     - The standard deviation of recent observations is below tolerance
//       relative to the running mean (relative drift criterion)
//
// Intended target metrics (one gate per metric):
//   - RMSD drift                (Å)
//   - Total energy drift        (kcal/mol)
//   - Structural residual drift (Å)
//   - Mean displacement drift   (Å)
//
// Usage:
//   StationarityGate gate;
//   gate.push(rmsd);
//   if (gate.ready()) { // begin production measurement }
//
// Anti-black-box: all fields public, gate criterion explicit and readable.
// =============================================================================

#include "online_stats.hpp"

namespace vsepr {

struct StationarityGate {
	// Accumulator over recent observations.
	OnlineStats window_stats;

	// Gate opens only when stddev / |mean| < relative_tolerance
	// (relative drift: tolerates different absolute scales automatically).
	double relative_tolerance = 1e-3;   // 0.1% relative fluctuation

	// Absolute fallback tolerance — used when mean ≈ 0 to avoid division.
	double absolute_tolerance = 1e-6;

	// Minimum samples before the gate can open.
	std::size_t min_samples = 100;

	// Ingest one observation.
	void push(double x) noexcept { window_stats.push(x); }

	// Reset — call when the metric is restarted or the window slides.
	void reset() noexcept { window_stats.reset(); }

	// Number of observations accumulated.
	std::size_t count() const noexcept { return window_stats.count(); }

	// Relative drift of the accumulated window.
	// Returns infinity when fewer than 2 samples exist.
	double relative_drift() const noexcept {
		if (window_stats.count() < 2) return 1e300;
		const double sd  = window_stats.stddev();
		const double mu  = window_stats.mean;
		const double denom = (mu < 0.0 ? -mu : mu);
		return (denom > absolute_tolerance) ? (sd / denom) : sd;
	}

	// Gate criterion: sufficient samples AND drift below tolerance.
	bool ready() const noexcept {
		if (window_stats.count() < min_samples) return false;
		return relative_drift() < relative_tolerance;
	}

	// Diagnostic summary string (for test output / logs).
	// Deliberately simple — no stdlib formatting overhead in hot paths.
	const OnlineStats& stats() const noexcept { return window_stats; }
};

} // namespace vsepr
