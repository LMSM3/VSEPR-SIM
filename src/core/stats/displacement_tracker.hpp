#pragma once
// =============================================================================
// src/core/stats/displacement_tracker.hpp
// =============================================================================
// Tracks mean displacement of all particles from their reference positions.
//
//   mean_displacement = (1/N) Σ_i |r_i(t) - r_i(0)|
//
// This is a blunt but fast metric — no Kabsch alignment, no rotation removal.
// It tells you the average particle travel distance from the starting layout.
//
// Use as a support metric alongside RMSD and structural residual.
// Do not use as the sole truth judge:
//   - All atoms shifting rigidly → high displacement, structure unchanged.
//   - Single-site defect forming  → low average displacement, but real damage.
//
// Anti-black-box: all fields public.
// =============================================================================

#include "../math_vec3.hpp"
#include "online_stats.hpp"
#include <vector>
#include <cmath>
#include <cstddef>
#include <numeric>

namespace vsepr {

struct DisplacementTracker {

	// -------------------------------------------------------------------------
	// State
	// -------------------------------------------------------------------------

	double   mean_displacement = 0.0;
	double   max_displacement  = 0.0;   // worst-offender particle
	uint64_t frame             = 0;

	OnlineStats mean_disp_stats;        // running stats on mean_displacement

	// -------------------------------------------------------------------------
private:
	std::vector<vsepr::Vec3> reference_frame_;
	bool has_reference_ = false;

public:
	// -------------------------------------------------------------------------
	// Interface
	// -------------------------------------------------------------------------

	/// Set explicit reference positions.
	/// If never called, the first push() establishes the reference.
	void set_reference(const std::vector<vsepr::Vec3>& ref) {
		reference_frame_ = ref;
		has_reference_   = true;
	}

	/// Feed the current frame of positions.
	void push(const std::vector<vsepr::Vec3>& positions) {
		if (!has_reference_) {
			reference_frame_ = positions;
			has_reference_   = true;
		}

		const std::size_t N = std::min(positions.size(), reference_frame_.size());
		if (N == 0) {
			++frame;
			return;
		}

		double sum = 0.0;
		double mx  = 0.0;
		for (std::size_t i = 0; i < N; ++i) {
			const double d = (positions[i] - reference_frame_[i]).norm();
			sum += d;
			if (d > mx) mx = d;
		}

		mean_displacement = sum / static_cast<double>(N);
		max_displacement  = mx;
		mean_disp_stats.push(mean_displacement);
		++frame;
	}

	/// Reset history but keep reference frame.
	void reset_history() noexcept {
		mean_displacement = 0.0;
		max_displacement  = 0.0;
		frame             = 0;
		mean_disp_stats.reset();
	}

	/// Full reset including reference frame.
	void reset() noexcept {
		reset_history();
		reference_frame_.clear();
		has_reference_ = false;
	}
};

} // namespace vsepr
