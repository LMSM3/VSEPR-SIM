#pragma once
// =============================================================================
// src/core/stats/sim_metrics.hpp
// =============================================================================
// Aggregator for the five core simulation health metrics.
//
//   1. Energy drift        (EnergyDriftTracker)
//   2. RMSD to reference   (RMSDTracker — mode A)
//   3. RMSD frame-to-frame (RMSDTracker — mode B)
//   4. Mean displacement   (DisplacementTracker)
//   5. Structural residual (StructuralResidual)
//
// Output row (SimMetricsRow) matches the minimum output table specified:
//
//   frame | time | E_total | E_drift | RMSD_ref | RMSD_step
//         | mean_displacement | structural_residual | stationary_flag
//
// Usage:
//
//   SimMetrics metrics;
//   metrics.set_reference(initial_positions, E_total_0);
//
//   // inside time loop:
//   metrics.push(frame_index, sim_time, E_total_now, positions_now);
//
//   SimMetricsRow row = metrics.current_row();
//   metrics.log.push_back(row);    // optional: accumulate the full log
//
// The stationary_flag is true when ALL four position-based stationarity
// gates agree the system has settled.  Energy drift is checked separately
// via EnergyDriftTracker::is_drifting().
//
// Anti-black-box: all sub-trackers public and individually inspectable.
// =============================================================================

#include "energy_drift.hpp"
#include "rmsd_tracker.hpp"
#include "displacement_tracker.hpp"
#include "structural_residual.hpp"
#include "stationarity_gate.hpp"
#include "../math_vec3.hpp"
#include <vector>
#include <cstdint>
#include <string>
#include <sstream>
#include <iomanip>

namespace vsepr {

// ---------------------------------------------------------------------------
// SimMetricsRow  — one row of the output table
// ---------------------------------------------------------------------------

struct SimMetricsRow {
	uint64_t frame              = 0;
	double   time               = 0.0;    // simulation time (ps or fs — caller's units)
	double   E_total            = 0.0;
	double   E_drift            = 0.0;    // absolute: E_total - E_0
	double   E_rel_drift        = 0.0;    // relative: delta_E / |E_0|
	double   RMSD_ref           = 0.0;    // Å — Kabsch RMSD vs reference frame
	double   RMSD_step          = 0.0;    // Å — Kabsch RMSD vs previous frame
	double   mean_displacement  = 0.0;    // Å — average site travel from reference
	double   structural_residual= 0.0;    // Å — RMS lattice-site deviation
	double   defect_fraction    = 0.0;    // [0,1] fraction of sites beyond threshold
	bool     stationary_flag    = false;  // true when all gates agree system settled
	std::string motion_class;             // metric-behavior classification label

	/// Format as a tab-separated line for file output.
	std::string to_tsv() const {
		std::ostringstream ss;
		ss << std::fixed << std::setprecision(6)
		   << frame            << '\t'
		   << time             << '\t'
		   << E_total          << '\t'
		   << E_drift          << '\t'
		   << E_rel_drift      << '\t'
		   << RMSD_ref         << '\t'
		   << RMSD_step        << '\t'
		   << mean_displacement<< '\t'
		   << structural_residual << '\t'
		   << defect_fraction  << '\t'
		   << (stationary_flag ? 1 : 0) << '\t'
		   << motion_class;
		return ss.str();
	}

	/// TSV header line matching to_tsv() column order.
	static std::string tsv_header() {
		return "frame\ttime\tE_total\tE_drift\tE_rel_drift"
			   "\tRMSD_ref\tRMSD_step"
			   "\tmean_displacement\tstructural_residual"
			   "\tdefect_fraction\tstationary_flag\tmotion_class";
	}
};

// ---------------------------------------------------------------------------
// classify_motion()  — metric-behavior classifier
// ---------------------------------------------------------------------------
// Maps a single row's metric values to a human-readable motion label.
// This is a metric behavior classifier, not a materials classifier.
//
// Labels:
//   none                    — no movement, no drift
//   rigid_motion            — shape unchanged (RMSD≈0), but position changed
//   nonrigid_deformation    — shape changed (RMSD>0), still evolving
//   settled                 — all stationarity gates agree system stopped changing
//   unstable_or_bad_timestep— energy is drifting significantly
// ---------------------------------------------------------------------------

inline std::string classify_motion(const SimMetricsRow& r) {
	constexpr double rmsd_eps = 1e-6;
	constexpr double disp_eps = 1e-6;
	constexpr double drift_threshold = 0.05;  // 5% energy drift → suspect

	if (r.stationary_flag)
		return "settled";
	if (r.E_rel_drift > drift_threshold)
		return "unstable_or_bad_timestep";
	if (r.RMSD_ref < rmsd_eps && r.mean_displacement < disp_eps)
		return "none";
	if (r.RMSD_ref < rmsd_eps && r.mean_displacement > disp_eps)
		return "rigid_motion";
	if (r.RMSD_ref > rmsd_eps)
		return "nonrigid_deformation";
	return "none";
}

// ---------------------------------------------------------------------------
// SimMetricsLog  — ordered collection of rows for a full run
// ---------------------------------------------------------------------------

using SimMetricsLog = std::vector<SimMetricsRow>;

// ---------------------------------------------------------------------------
// SimMetrics  — aggregator
// ---------------------------------------------------------------------------

struct SimMetrics {

	// -------------------------------------------------------------------------
	// Sub-trackers (all public — individually inspectable)
	// -------------------------------------------------------------------------

	EnergyDriftTracker   energy;
	RMSDTracker          rmsd;
	DisplacementTracker  displacement;
	StructuralResidual   structure;

	// Stationarity gates — one per position-based metric.
	// Defaults: 1e-3 relative tolerance, 100-sample minimum.
	// Tune per-metric if needed (e.g. lower for RMSD_step in fast relaxers).
	StationarityGate gate_rmsd_ref;
	StationarityGate gate_rmsd_step;
	StationarityGate gate_displacement;
	StationarityGate gate_residual;

	// -------------------------------------------------------------------------
	// Interface
	// -------------------------------------------------------------------------

	/// Establish reference state.
	/// Call once with the initial/ideal positions and baseline energy.
	void set_reference(const std::vector<vsepr::Vec3>& positions,
					   double E_total_0) {
		energy.push(E_total_0);         // sets E_0
		rmsd.set_reference(positions);
		displacement.set_reference(positions);
		structure.set_reference(positions);
	}

	/// Feed a new simulation frame.
	void push(uint64_t frame_index,
			  double   sim_time,
			  double   E_total_now,
			  const std::vector<vsepr::Vec3>& positions) {
		energy.push(E_total_now);
		rmsd.push(positions);
		displacement.push(positions);
		structure.push(positions);

		// Feed stationarity gates
		gate_rmsd_ref.push(rmsd.rmsd_ref);
		gate_rmsd_step.push(rmsd.rmsd_step);
		gate_displacement.push(displacement.mean_displacement);
		gate_residual.push(structure.structural_residual);

		// Cache frame/time for current_row()
		current_frame_ = frame_index;
		current_time_  = sim_time;
	}

	/// Snapshot of all metrics at the current push.
	SimMetricsRow current_row() const {
		SimMetricsRow r;
		r.frame               = current_frame_;
		r.time                = current_time_;
		r.E_total             = energy.E_current;
		r.E_drift             = energy.delta_E;
		r.E_rel_drift         = energy.rel_drift;
		r.RMSD_ref            = rmsd.rmsd_ref;
		r.RMSD_step           = rmsd.rmsd_step;
		r.mean_displacement   = displacement.mean_displacement;
		r.structural_residual = structure.structural_residual;
		r.defect_fraction     = structure.defect_fraction;
		r.stationary_flag     = gate_rmsd_ref.ready()
							 && gate_rmsd_step.ready()
							 && gate_displacement.ready()
							 && gate_residual.ready();
		r.motion_class        = classify_motion(r);
		return r;
	}

	/// Reset all trackers and gates (keeps reference frames).
	void reset_history() {
		energy.reset();
		rmsd.reset_history();
		displacement.reset_history();
		structure.reset_history();
		gate_rmsd_ref.reset();
		gate_rmsd_step.reset();
		gate_displacement.reset();
		gate_residual.reset();
		current_frame_ = 0;
		current_time_  = 0.0;
	}

	/// Full reset including all reference frames.
	void reset() {
		reset_history();
		rmsd.reset();
		displacement.reset();
		structure.reset();
	}

private:
	uint64_t current_frame_ = 0;
	double   current_time_  = 0.0;
};

} // namespace vsepr
