#pragma once
// =============================================================================
// src/core/stats/rmsd_tracker.hpp
// =============================================================================
// Tracks RMSD in two modes, both using the Kabsch-aligned RMSD from
// src/analysis/kabsch.hpp so rotation and translation are factored out.
//
//   Mode A — RMSD to reference frame (RMSD_ref):
//     How far has the current structure moved from the original ideal state?
//     Detects defect accumulation, disorder, diffusion, and lattice drift.
//     Flat → structure is preserved.
//     Rising → deforming, diffusing, or unstable.
//
//   Mode B — RMSD between consecutive frames (RMSD_step):
//     Is the structure still changing, or has it settled?
//     Flat → system is stationary (feeds StationarityGate).
//     Rising → still evolving.
//     Spike → impact, decay event, or numerical failure.
//
// Anti-black-box: both RMSD values always inspectable.
// Depends on: src/analysis/kabsch.hpp (which depends on Eigen via eigen_bridge)
// =============================================================================

#include "../math_vec3.hpp"
#include "online_stats.hpp"
#include <vector>
#include <cstddef>
#include <cmath>

// Forward-declared to avoid forcing Eigen into every translation unit
// that only needs the scalar outputs.  Callers that want to use
// RMSDTracker must include analysis/kabsch.hpp before this header,
// OR include this header via sim_metrics.hpp which does it for them.
#ifndef VSEPR_KABSCH_HPP_INCLUDED
#include "../../analysis/kabsch.hpp"
#define VSEPR_KABSCH_HPP_INCLUDED
#endif

namespace vsepr {

struct RMSDTracker {

	// -------------------------------------------------------------------------
	// State
	// -------------------------------------------------------------------------

	double   rmsd_ref   = 0.0;   // Kabsch RMSD vs reference frame
	double   rmsd_step  = 0.0;   // Kabsch RMSD vs previous frame
	uint64_t frame      = 0;

	OnlineStats ref_stats;       // running stats on rmsd_ref (feeds StationarityGate)
	OnlineStats step_stats;      // running stats on rmsd_step

	// -------------------------------------------------------------------------
	// Internal storage
	// -------------------------------------------------------------------------
private:
	std::vector<vsepr::Vec3> reference_frame_;
	std::vector<vsepr::Vec3> previous_frame_;
	bool has_reference_   = false;
	bool has_previous_    = false;

public:
	// -------------------------------------------------------------------------
	// Interface
	// -------------------------------------------------------------------------

	/// Set an explicit reference frame.
	/// If never called, the first push() establishes it automatically.
	void set_reference(const std::vector<vsepr::Vec3>& ref) {
		reference_frame_ = ref;
		has_reference_   = true;
	}

	/// Feed the current frame of positions.
	/// Computes rmsd_ref and rmsd_step.
	void push(const std::vector<vsepr::Vec3>& positions) {
		if (!has_reference_) {
			reference_frame_ = positions;
			has_reference_   = true;
		}

		if (positions.size() == reference_frame_.size() && !positions.empty()) {
			rmsd_ref = vsepr::analysis::kabsch_rmsd(reference_frame_, positions);
			ref_stats.push(rmsd_ref);
		}

		if (has_previous_ &&
			positions.size() == previous_frame_.size() &&
			!positions.empty()) {
			rmsd_step = vsepr::analysis::kabsch_rmsd(previous_frame_, positions);
			step_stats.push(rmsd_step);
		}

		previous_frame_ = positions;
		has_previous_   = true;
		++frame;
	}

	/// True when rmsd_step has flattened below threshold — system likely settled.
	bool is_settled(double threshold_angstrom = 0.01) const noexcept {
		return has_previous_ && (rmsd_step < threshold_angstrom);
	}

	/// Reset all state but keep the reference frame.
	void reset_history() noexcept {
		rmsd_ref   = 0.0;
		rmsd_step  = 0.0;
		frame      = 0;
		previous_frame_.clear();
		has_previous_ = false;
		ref_stats.reset();
		step_stats.reset();
	}

	/// Full reset including reference frame.
	void reset() noexcept {
		reset_history();
		reference_frame_.clear();
		has_reference_ = false;
	}
};

} // namespace vsepr
