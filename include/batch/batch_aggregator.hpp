#pragma once
/**
 * include/batch/batch_aggregator.hpp
 * =====================================
 * WO-VSIM-62C — Static-Axis Post-Run Aggregation
 *
 * Runs after all runs complete.  Stochastic and formation axis
 * dimensions are deferred to v5.1.0 / v5.2.0.
 *
 * Supported statistics (spec §7):
 *   mean, std, cv, min, max, median, fraction_true
 *
 * Output files:
 *   batch_summary.tsv        — all runs: params + raw metrics + status
 *   ranked_candidates.tsv    — PASS/WARN runs sorted by rank_by metric
 *   aggregate_results.tsv    — grouped statistics
 *   aggregate_results.json   — same, machine-readable
 *
 * WO-VSIM-62C | beta-12
 */

#include "include/batch/batch_verification.hpp"
#include "include/vsim/vsim_document.hpp"
#include <map>
#include <string>
#include <vector>

namespace vsim {
namespace batch {

// ─────────────────────────────────────────────────────────────────────────────
// BatchRunRecord — per-run result consumed by the aggregator
// (lives here rather than in batch_verification.hpp to keep the 62C
// layer self-contained; the verification kernel fills extra fields)
// ─────────────────────────────────────────────────────────────────────────────

struct BatchRunRecord {
	std::string run_id;
	std::string case_name;
	std::string verify_status;        // PASS | WARN | FAIL | MISSING | ERROR
	std::vector<std::string> failure_modes;
	std::map<std::string, std::string> axis_values;
	std::map<std::string, double>      metrics;   // metric name → numeric value
};

// ─────────────────────────────────────────────────────────────────────────────
// Aggregation control section (mirrors [batch.aggregate] in the study file)
// ─────────────────────────────────────────────────────────────────────────────

struct BatchAggregateSection {
    bool                     enabled  = false;
    std::vector<std::string> group_by;
    std::vector<std::string> statistics = {"mean", "std", "min", "max"};
};

// ─────────────────────────────────────────────────────────────────────────────
// Aggregation output
// ─────────────────────────────────────────────────────────────────────────────

struct AggregateGroupResult {
	std::map<std::string, std::string>              group_keys;
	int                                             run_count = 0;
	std::map<std::string, std::map<std::string, double>> results; // metric → { stat → value }
};

struct BatchAggregateResult {
	std::vector<AggregateGroupResult> groups;
};

BatchAggregateResult aggregate_runs(
	const std::vector<BatchRunRecord>&  records,
	const BatchAggregateSection&        agg_section);

// ─────────────────────────────────────────────────────────────────────────────
// Output writers
// ─────────────────────────────────────────────────────────────────────────────

void write_batch_summary(const std::vector<BatchRunRecord>& records,
						 const std::string&                  path);

void write_ranked_candidates(const std::vector<BatchRunRecord>& records,
							  const std::string&                  rank_by,
							  const std::string&                  path);

void write_aggregate_tsv (const BatchAggregateResult& result, const std::string& path);
void write_aggregate_json(const BatchAggregateResult& result, const std::string& path);

} // namespace batch
} // namespace vsim
