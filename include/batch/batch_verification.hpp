#pragma once
/**
 * include/batch/batch_verification.hpp
 * ======================================
 * WO-VSEPR-SIM-62B — Batch Verification Aggregation Types
 *
 * Defines the data structures produced and consumed by the batch
 * verification aggregation kernel.  The kernel reads verify_report.json
 * from each completed run, classifies failure modes, aggregates into
 * grouped and batch-level summaries, evaluates pass-rate gates, and
 * drives the output writers (batch_verify_summary.tsv,
 * batch_verify_matrix.tsv, batch_failure_modes.tsv,
 * batch_empirical_report.md).
 *
 * Doctrine: analysis computes, verification judges, aggregation trusts.
 * The aggregation kernel NEVER recomputes verification checks.
 *
 * WO-VSEPR-SIM-62B  |  beta-12
 */

#include "include/batch/failure_modes.hpp"
#include <map>
#include <string>
#include <vector>

namespace vsim {
namespace batch {

// ─────────────────────────────────────────────────────────────────────────────
// VerificationRunRecord
//
// Consumed from one run's verify_report.json.  Populated by the batch
// aggregation kernel; never written back into the per-run folder.
// ─────────────────────────────────────────────────────────────────────────────

struct VerificationRunRecord {
	std::string run_id;
	std::string case_name;

	// Top-level verdict
	bool        overall_pass = false;
	std::string status;  // "PASS" | "WARN" | "FAIL" | "SKIP" | "MISSING" | "ERROR"

	// Per-check verdicts
	bool structure_pass = false;
	bool rdf_pass       = false;
	bool mass_pass      = false;
	bool msd_pass       = false;

	// Per-check numeric detail (for ranking and matrix output)
	double rdf_first_peak_error_A  = 0.0;
	double rdf_second_peak_error_A = 0.0;
	double mass_relative_error     = 0.0;
	double structure_nn_error_A    = 0.0;
	double msd_bound_error         = 0.0;

	// Failure classification (BatchFailureMode code strings for serialisation)
	std::vector<std::string> failure_modes;

	// Axis values for this run (used for group_by grouping)
	std::map<std::string, std::string> axis_values;
};

// ─────────────────────────────────────────────────────────────────────────────
// GroupedVerificationSummary
//
// One entry per unique combination of group_by axis values.
// ─────────────────────────────────────────────────────────────────────────────

struct GroupedVerificationSummary {
	// Keys that identify this group
	std::map<std::string, std::string> group_keys;

	int runs   = 0;
	int passed = 0;
	int failed = 0;
	int warned = 0;

	double pass_rate           = 0.0;
	double structure_pass_rate = 0.0;
	double rdf_pass_rate       = 0.0;
	double mass_pass_rate      = 0.0;
	double msd_pass_rate       = 0.0;

	// Failure mode frequency within this group
	std::map<std::string, int>    failure_modes;

	// Mean numeric errors across runs in the group
	// keys: "rdf_first_peak_error_A", "mass_relative_error", etc.
	std::map<std::string, double> mean_errors;
};

// ─────────────────────────────────────────────────────────────────────────────
// BatchVerificationSummary
//
// Batch-level aggregate produced after consuming all VerificationRunRecords.
// Written to batch_verify_summary.tsv and embedded in
// batch_empirical_report.md.
// ─────────────────────────────────────────────────────────────────────────────

struct BatchVerificationSummary {
	int total_runs   = 0;
	int passed_runs  = 0;
	int failed_runs  = 0;
	int warned_runs  = 0;
	int skipped_runs = 0;
	int missing_runs = 0;
	int error_runs   = 0;

	double overall_pass_rate   = 0.0;
	double structure_pass_rate = 0.0;
	double rdf_pass_rate       = 0.0;
	double mass_pass_rate      = 0.0;
	double msd_pass_rate       = 0.0;

	// Failure mode frequency table across the entire batch
	std::map<std::string, int> failure_mode_counts;

	// Per-group breakdowns
	std::vector<GroupedVerificationSummary> grouped;

	// Batch-level gate result
	bool        batch_empirical_ready = false;
	std::string gate_failure_reason;  // empty when batch_empirical_ready = true

	// Raw per-run records retained for ranked_candidates filtering
	std::vector<VerificationRunRecord> run_records;
};

} // namespace batch
} // namespace vsim
