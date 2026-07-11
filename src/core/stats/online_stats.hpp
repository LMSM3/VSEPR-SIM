#pragma once
// =============================================================================
// src/core/stats/online_stats.hpp — Welford Online Statistics (Day #56)
// =============================================================================
// Port of the Python OnlineStats accumulator to C++.
//
// Algorithm: Welford (1962) one-pass variance.
//   n, mean, M2 updated each push().
//   Variance = M2 / (n - 1)  (sample variance, unbiased)
//   StdDev   = sqrt(variance)
//
// Use for:
//   - RMSD drift over time
//   - Kinetic energy stability
//   - Temperature proxy stability
//   - Pressure proxy stability
//   - Structural residual drift
//
// Anti-black-box: all fields are public, all calculations explicit.
// Thread safety: none — single-threaded per-accumulator model.
// =============================================================================

#include <cmath>
#include <cstddef>

namespace vsepr {

struct OnlineStats {
	std::size_t n   = 0;
	double mean     = 0.0;
	double m2       = 0.0;

	// Ingest one new observation.
	void push(double x) noexcept {
		++n;
		const double delta  = x - mean;
		mean               += delta / static_cast<double>(n);
		const double delta2 = x - mean;
		m2                 += delta * delta2;
	}

	// Number of observations ingested.
	std::size_t count() const noexcept { return n; }

	// Sample variance (unbiased, Bessel corrected).
	// Returns 0 when fewer than two samples are available.
	double variance() const noexcept {
		return n > 1 ? m2 / static_cast<double>(n - 1) : 0.0;
	}

	// Sample standard deviation.
	double stddev() const noexcept {
		return std::sqrt(variance());
	}

	// Reset accumulator to empty state.
	void reset() noexcept {
		n    = 0;
		mean = 0.0;
		m2   = 0.0;
	}
};

} // namespace vsepr
