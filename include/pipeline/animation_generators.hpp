#pragma once
/**
 * animation_generators.hpp — Declarative Animation Cue Builders
 * ==============================================================
 *
 * Pure functions that return AnimationCue structs describing what the
 * dashboard, SVG renderer, or terminal display should show in beta-8+.
 *
 * IMPORTANT: nothing in this file renders anything.
 * Stages describe animation instructions. Renderers consume them.
 * If stage code starts caring about pixels, the architecture begins
 * chewing drywall.
 *
 * Naming convention: anim::<event_name>(inputs...)
 *
 * VSEPR-SIM  |  beta-7  |  WO-56C
 */

#include "include/pipeline/pipeline_trace.hpp"

#include <cstdint>
#include <string>

namespace vsepr::pipeline::anim {

/**
 * feature_vector_bars
 *
 * Animate the 8-component feature vector growing as bars from zero
 * to their normalised magnitudes.
 */
inline AnimationCue feature_vector_bars(
	const std::string& symbol,
	double             feature_norm)
{
	AnimationCue cue;
	cue.id       = "anim_feature_bars_" + symbol;
	cue.stage    = "stage_fingerprint";
	cue.cue_type = "bar_grow";
	cue.target   = symbol;
	cue.label    = "Feature vector magnitude";
	cue.value0   = 0.0;
	cue.value1   = feature_norm;
	cue.t0       = 0.0;
	cue.t1       = 1.0;
	cue.easing   = "ease_out";
	cue.notes    = "Grow 8-bar histogram from zero to feature vector components.";
	return cue;
}

/**
 * cluster_assignment_pulse
 *
 * Emit a pulse indicating that `symbol` joined cluster `cluster_id`.
 */
inline AnimationCue cluster_assignment_pulse(
	const std::string& symbol,
	uint64_t           cluster_id)
{
	AnimationCue cue;
	cue.id       = "anim_cluster_pulse_" + symbol;
	cue.stage    = "stage_cluster";
	cue.cue_type = "pulse";
	cue.target   = symbol;
	cue.label    = "Cluster assignment";
	cue.value0   = 0.0;
	cue.value1   = static_cast<double>(cluster_id & 0xFFFFFF); // truncated for display
	cue.t0       = 0.0;
	cue.t1       = 0.4;
	cue.easing   = "ease_out";
	cue.notes    = "Flash node when case joins a cluster.";
	return cue;
}

/**
 * stability_gauge
 *
 * Fill a gauge arc from 0 to stability_score over a fixed window.
 */
inline AnimationCue stability_gauge(
	const std::string& symbol,
	double             stability_score)
{
	AnimationCue cue;
	cue.id       = "anim_stability_gauge_" + symbol;
	cue.stage    = "stage_analysis";
	cue.cue_type = "gauge_fill";
	cue.target   = symbol;
	cue.label    = "Stability score";
	cue.value0   = 0.0;
	cue.value1   = stability_score;
	cue.t0       = 1.2;
	cue.t1       = 2.0;
	cue.easing   = "ease_out";
	cue.notes    = "Fill gauge arc from zero to stability score.";
	return cue;
}

/**
 * defect_warning_flash
 *
 * Flash the case node when defect_indicator crosses threshold.
 * Threshold check is performed by the caller — this cue is only
 * emitted when the caller determines it is warranted.
 */
inline AnimationCue defect_warning_flash(
	const std::string& symbol,
	double             defect_indicator)
{
	AnimationCue cue;
	cue.id       = "anim_defect_flash_" + symbol;
	cue.stage    = "stage_analysis";
	cue.cue_type = "flash";
	cue.target   = symbol;
	cue.label    = "Defect indicator";
	cue.value0   = 0.0;
	cue.value1   = defect_indicator;
	cue.t0       = 2.0;
	cue.t1       = 2.4;
	cue.easing   = "step";
	cue.notes    = "Flash if defect indicator crosses threshold.";
	return cue;
}

/**
 * dashboard_cluster_growth
 *
 * Animate the cluster registry growing from 0 to n_clusters
 * over the duration of the full run.
 */
inline AnimationCue dashboard_cluster_growth(
	int n_cases,
	int n_clusters)
{
	AnimationCue cue;
	cue.id       = "anim_dashboard_cluster_growth";
	cue.stage    = "stage_dashboard";
	cue.cue_type = "network_grow";
	cue.target   = "cluster_registry";
	cue.label    = "Cluster registry growth";
	cue.value0   = 0.0;
	cue.value1   = static_cast<double>(n_clusters);
	cue.t0       = 0.0;
	cue.t1       = static_cast<double>(n_cases) * 0.1;
	cue.easing   = "linear";
	cue.notes    = "Animate cluster registry growth over processed cases.";
	return cue;
}

} // namespace vsepr::pipeline::anim
