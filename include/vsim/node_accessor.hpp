#pragma once
/**
 * include/vsim/node_accessor.hpp
 * ================================
 * Qt-free bridge between NodePath strings (from ViewRequest / show directives)
 * and typed row structs drawn from KernelEventLog.
 *
 * Design rules:
 *   - No Qt headers; usable from headless tests and kernel code.
 *   - All methods are const and thread-safe (KernelEventLog is mutex-guarded).
 *   - nullptr-safe: callers check has_data() before iterating rows.
 *   - Synthetic-fallback: windows that receive nullptr fall back to demo
 *     geometry, so nothing regresses in a headless / BUILD_DESKTOP=OFF build.
 *
 * Supported NodePaths (v1):
 *   "run.history"   → helix_rows()  — ContinualReportEvent sequence
 *   "room.solver"   → heat_rows()   — ContinualReportEvent filtered by formula
 *
 * WO-VSIM-VIS-OVERHAUL-01
 */

#include "kernel/kernel_event.hpp"
#include "kernel/kernel_event_log.hpp"

#include <string>
#include <vector>

namespace vsim {

// ============================================================================
// HelixRow — one data point for the calibration.helix window
// ============================================================================

struct HelixRow {
	uint64_t    frame_id         = 0;
	float       temperature_K    = 0.f;    // raw temperature
	float       t_c_norm         = 0.f;    // normalised 0..1 for colour map
	float       severity         = 0.f;    // 0..1 derived from packing_fraction
	float       coord_num        = 0.f;    // mean_coord_num
	bool        regime_transition = false; // FormationEvent.converged flip
	bool        loop_boundary     = false; // frame_id % report_interval == 0
};

// ============================================================================
// HeatRow — one voxel for the room.heatfield window
// ============================================================================

struct HeatRow {
	float   x        = 0.f;  // normalised grid coord 0..1
	float   y        = 0.f;  // normalised grid coord 0..1
	float   density  = 0.f;  // normalised temperature density 0..1
	bool    obstacle = false;
};

// ============================================================================
// NodeAccessor
// ============================================================================

class NodeAccessor {
public:
	// Construct with an optional formula filter (used by room.solver to select
	// only events from the room material, e.g. "He").
	explicit NodeAccessor(std::string formula_filter = "")
		: formula_filter_(std::move(formula_filter)) {}

	// ------------------------------------------------------------------
	// "run.history" → calibration helix rows
	//
	// Returns one HelixRow per ContinualReportEvent in frame_id order.
	// Temperature is normalised over [T_min, T_max] of the full sequence.
	// Regime transitions are inferred from FormationEvent records where
	// converged changes from false→true within a 10-frame window.
	// Loop boundaries are every `boundary_every` frames (default 40).
	// ------------------------------------------------------------------
	[[nodiscard]] std::vector<HelixRow>
	helix_rows(int boundary_every = 40) const;

	// ------------------------------------------------------------------
	// "room.solver" → heat field voxel rows
	//
	// Returns one HeatRow per ContinualReportEvent (filtered by formula).
	// temperature_K is mapped to a 2D grid position using frame_id as
	// the time axis and packing_fraction as the spatial axis.
	// Density is normalised over [T_min, T_max] of the filtered set.
	// ------------------------------------------------------------------
	[[nodiscard]] std::vector<HeatRow>
	heat_rows(int subsample_cap = 80000) const;

	// ------------------------------------------------------------------
	// Scalar summary for data.scalar.panel
	// Returns key→value string pairs for properties display.
	// ------------------------------------------------------------------
	[[nodiscard]] std::vector<std::pair<std::string, std::string>>
	scalar_summary() const;

	// True if any ContinualReportEvent rows exist (matching filter).
	[[nodiscard]] bool has_data() const;

private:
	std::string formula_filter_;

	// Collect ContinualReportEvent rows from the global log, optionally
	// filtered by source_formula.
	std::vector<vsepr::kernel::ContinualReportEvent>
	collect_continual_events() const;

	// Collect FormationEvent rows (for regime detection).
	std::vector<vsepr::kernel::FormationEvent>
	collect_formation_events() const;
};

} // namespace vsim
