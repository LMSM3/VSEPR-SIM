#pragma once
/**
 * pipeline_trace.hpp — Symbolic Trace and Animation Cue Types
 * ============================================================
 *
 * Provides the data structures that let the beta-7 pipeline stages record
 * WHY a value exists (SymbolicTrace) and HOW the dashboard should later
 * present it (AnimationCue), without coupling stage code to rendering.
 *
 * Design rules:
 *   - SymbolicTrace and AnimationCue are plain aggregates. No methods.
 *   - StageTraceBundle is the container carried inside pipeline records.
 *   - Stages DESCRIBE animation instructions. Nothing here renders anything.
 *   - This is the WO-56C symbolic trace layer.
 *
 * VSEPR-SIM  |  beta-7  |  WO-56C
 */

#include <string>
#include <vector>

namespace vsepr::pipeline {

// ============================================================================
// SymbolicTrace — one derived metric, fully documented
// ============================================================================

/**
 * SymbolicTrace
 *
 * Records the complete calculation lineage for a single derived metric.
 * Consumed by stage_report (Markdown emission) and KernelEventLog export.
 *
 * Example for energy_per_bead:
 *
 *   symbolic_expression  = "E_bead = E_final / N_beads"
 *   substituted_expression = "E_bead = -123.45 / 64"
 *   result_expression    = "E_bead = -1.929"
 *   units                = "energy/bead"
 *   interpretation       = "Mean energy per structural bead."
 *   source_stage         = "stage_analysis"
 */
struct SymbolicTrace {
	std::string metric_name;            // e.g. "energy_per_bead"
	std::string symbolic_expression;    // e.g. "E_bead = E_final / N_beads"
	std::string substituted_expression; // e.g. "E_bead = -123.45 / 64"
	std::string result_expression;      // e.g. "E_bead = -1.929"
	std::string units;                  // e.g. "energy/bead", "dimensionless"
	std::string interpretation;         // plain-language sentence
	std::string source_stage;           // "stage_fingerprint" / "stage_analysis" / etc.
};

// ============================================================================
// AnimationCue — one declarative dashboard instruction
// ============================================================================

/**
 * AnimationCue
 *
 * A declarative description of a single animation event that the dashboard,
 * SVG renderer, or terminal display may consume in beta-8+.
 *
 * Stages emit cues. Stages do NOT animate. The cue is consumed downstream.
 *
 * Fields:
 *   id         Unique cue identifier (e.g. "anim_stability_gauge_Al")
 *   stage      Which stage emitted this cue
 *   cue_type   Kind of animation (gauge_fill, pulse, flash, bar_grow,
 *              network_grow, step, …)
 *   target     What is being animated (symbol name, registry name, …)
 *   label      Human-readable label for the animated quantity
 *   value0     Start value (commonly 0.0)
 *   value1     End / peak value
 *   t0, t1     Relative time range in the animation timeline (seconds)
 *   easing     "linear", "ease_out", "ease_in", "step"
 *   notes      Optional annotation for the renderer
 */
struct AnimationCue {
	std::string id;
	std::string stage;
	std::string cue_type;
	std::string target;
	std::string label;
	double      value0{0.0};
	double      value1{0.0};
	double      t0{0.0};
	double      t1{1.0};
	std::string easing{"linear"};
	std::string notes;
};

// ============================================================================
// StageTraceBundle — container carried inside each pipeline record
// ============================================================================

/**
 * StageTraceBundle
 *
 * Aggregated trace data for one pipeline case.
 * Records accumulate as they pass through each stage.
 */
struct StageTraceBundle {
	std::vector<SymbolicTrace> expressions;  // ordered by stage emission
	std::vector<AnimationCue>  animations;   // ordered by stage emission
};

} // namespace vsepr::pipeline
