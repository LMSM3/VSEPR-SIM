#pragma once
// =============================================================================
// src/core/stats/structural_residual.hpp
// =============================================================================
// Measures how much the current structure deviates from a reference lattice
// or topology at a per-site level.
//
// RMSD says "the structure changed."
// Structural residual says "where and how the structure stopped matching."
//
// Model
// -----
// For each site i in the reference:
//   displacement_i  = |r_i(t) - r_i_ref|          [Å]
//   site_deviant_i  = displacement_i > defect_radius_threshold
//
// From those:
//   structural_residual = RMS of all site displacements            [Å]
//   defect_fraction     = (number of deviant sites) / N            [0,1]
//   max_site_deviation  = largest single-site displacement         [Å]
//
// The structural_residual is the primary scalar output.
// defect_fraction tells you what fraction of the lattice is damaged.
// max_site_deviation is the worst single-site offender.
//
// This is how vacancies, interstitials, surface roughness, voids,
// grain boundaries, and radiation damage are detected from state deviation
// rather than hardcoded defect types.
//
// No Eigen required — pure Vec3 math.
// Anti-black-box: all fields public.
// =============================================================================

#include "../math_vec3.hpp"
#include "online_stats.hpp"
#include <vector>
#include <cmath>
#include <cstddef>

namespace vsepr {

struct StructuralResidual {

	// -------------------------------------------------------------------------
	// State
	// -------------------------------------------------------------------------

	double   structural_residual = 0.0;  // RMS site displacement from reference  [Å]
	double   defect_fraction     = 0.0;  // fraction of sites beyond threshold    [0,1]
	double   max_site_deviation  = 0.0;  // worst single-site displacement        [Å]
	uint64_t frame               = 0;

	// Per-site displacement vector — kept public for spatial analysis.
	std::vector<double> site_displacements;

	OnlineStats residual_stats;          // running stats on structural_residual

	// -------------------------------------------------------------------------
	// Configuration
	// -------------------------------------------------------------------------

	/// A site is flagged as "deviant" (contributing to defect_fraction)
	/// when its displacement exceeds this radius.  Default: 0.5 Å —
	/// reasonable for crystal-site defect classification.
	double defect_radius_threshold = 0.5;

	// -------------------------------------------------------------------------
private:
	std::vector<vsepr::Vec3> reference_frame_;
	bool has_reference_ = false;

public:
	// -------------------------------------------------------------------------
	// Interface
	// -------------------------------------------------------------------------

	/// Set the reference lattice/topology frame.
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
		site_displacements.resize(N);

		double sum_sq   = 0.0;
		double mx       = 0.0;
		std::size_t deviant_count = 0;

		for (std::size_t i = 0; i < N; ++i) {
			const double d = (positions[i] - reference_frame_[i]).norm();
			site_displacements[i] = d;
			sum_sq += d * d;
			if (d > mx) mx = d;
			if (d > defect_radius_threshold) ++deviant_count;
		}

		structural_residual = (N > 0) ? std::sqrt(sum_sq / static_cast<double>(N)) : 0.0;
		defect_fraction     = (N > 0) ? static_cast<double>(deviant_count) / static_cast<double>(N) : 0.0;
		max_site_deviation  = mx;

		residual_stats.push(structural_residual);
		++frame;
	}

	/// Reset history but keep reference frame and threshold.
	void reset_history() noexcept {
		structural_residual = 0.0;
		defect_fraction     = 0.0;
		max_site_deviation  = 0.0;
		frame               = 0;
		site_displacements.clear();
		residual_stats.reset();
	}

	/// Full reset including reference frame.
	void reset() noexcept {
		reset_history();
		reference_frame_.clear();
		has_reference_ = false;
	}
};

} // namespace vsepr
