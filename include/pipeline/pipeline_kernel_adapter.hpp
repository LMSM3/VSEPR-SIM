#pragma once
/**
 * pipeline_kernel_adapter.hpp — Pipeline → Central Kernel Bridge
 * ==============================================================
 *
 * Bridges the beta-7 pure-transform pipeline into the WO-56C kernel
 * event spine. The pipeline itself remains a clean transformation layer.
 * This adapter runs the pipeline and then registers each produced
 * AnalysisRecord as a KernelEvent in the central KernelEventLog.
 *
 * Usage (beta-8 path):
 *
 *   auto [records, dash] = run_pipeline_with_kernel(formations, "my-run");
 *   // All events now live in KernelEventLog::instance()
 *   auto jsonl = KernelEventLog::instance().to_jsonl();
 *   auto md    = KernelEventLog::instance().to_markdown();
 *
 * Design rules:
 *   - pipeline_stages.hpp stays pure. It never touches KernelEventLog.
 *   - This file owns the registration loop and conversion logic.
 *   - from_pipeline_record() converts one PipelineRecord into a
 *     FormationEvent so the kernel can own and export it.
 *   - Symbolic traces from AnalysisRecord::trace are forwarded into
 *     the FormationEvent's equation fields for audit.
 *
 * VSEPR-SIM  |  beta-7  |  WO-56C
 */

#include "include/pipeline/pipeline_stages.hpp"
#include "include/kernel/kernel_event.hpp"
#include "include/kernel/kernel_event_log.hpp"

#include <string>
#include <vector>

namespace vsepr::pipeline {

// ============================================================================
// Conversion: PipelineRecord → FormationEvent
// ============================================================================

/**
 * from_pipeline_record
 *
 * Converts one fully-processed PipelineRecord into a FormationEvent
 * suitable for registration in the central KernelEventLog.
 *
 * Forwarded fields:
 *   source_formula    ← symbol
 *   frame_id          ← 0 (formation events are pre-trajectory; caller may override)
 *   n_beads           ← formation.n_beads
 *   fire_steps        ← formation.steps
 *   converged         ← formation.converged
 *   final_energy      ← formation.final_energy
 *   packing_fraction  ← analysis.packing_quality (proxy)
 *   lattice_class     ← analysis.motif_class
 *   result_value      ← analysis.stability_score
 *   equation_symbolic ← first SymbolicTrace in analysis.trace (or summary)
 *   equation_numeric  ← substituted_expression of that trace
 */
inline vsepr::kernel::FormationEvent from_pipeline_record(
	const PipelineRecord& pr)
{
	vsepr::kernel::FormationEvent ev;
	ev.source_formula     = pr.analysis.symbol;
	ev.source_particle_id = -1;   // system-level event
	ev.frame_id           = 0;
	ev.n_beads            = pr.formation.n_beads;
	ev.fire_steps         = pr.formation.steps;
	ev.converged          = pr.formation.converged;
	ev.final_energy       = pr.formation.final_energy;
	ev.packing_fraction   = pr.analysis.packing_quality;
	ev.lattice_class      = pr.analysis.motif_class;
	ev.result_value       = pr.analysis.stability_score;
	ev.result_unit        = "stability [0,1]";
	ev.is_valid           = pr.report.warnings.empty();

	// Build symbolic equation summary from trace bundle
	if (!pr.analysis.trace.expressions.empty()) {
		const auto& first = pr.analysis.trace.expressions.front();
		ev.equation_symbolic = first.symbolic_expression;
		ev.equation_numeric  = first.substituted_expression;
	} else {
		ev.equation_symbolic = "stability_score = f(convergence, packing, defects)";
		ev.equation_numeric  = "S = " + std::to_string(pr.analysis.stability_score);
	}

	if (!ev.is_valid && !pr.analysis.interpretation.empty()) {
		ev.warning = pr.analysis.interpretation;
	}

	return ev;
}

// ============================================================================
// run_pipeline_with_kernel
// ============================================================================

/**
 * run_pipeline_with_kernel
 *
 * Runs the full beta-7 pipeline and registers a FormationEvent for each
 * case into the central KernelEventLog singleton.
 *
 * Returns the same pair<vector<PipelineRecord>, DashboardRecord> as
 * run_pipeline() so existing call sites need only swap the function name.
 *
 * Post-condition: KernelEventLog::instance() contains one FormationEvent
 * per formation case, with stable event_ids for downstream audit.
 *
 * @param formations  Input formation cases
 * @param run_label   Run identifier forwarded to the dashboard
 * @param epsilon     Cluster distance threshold (default 0.20)
 */
inline std::pair<std::vector<PipelineRecord>, DashboardRecord>
run_pipeline_with_kernel(
	const std::vector<v4::FormationRecord>& formations,
	const std::string& run_label = "beta-7-run",
	double epsilon = 0.20)
{
	auto [records, dash] = run_pipeline(formations, run_label, epsilon);

	auto& log = vsepr::kernel::KernelEventLog::instance();

	for (const auto& pr : records) {
		vsepr::kernel::FormationEvent ev = from_pipeline_record(pr);
		log.record(ev);
	}

	return { std::move(records), std::move(dash) };
}

} // namespace vsepr::pipeline
