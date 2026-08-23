#pragma once
// =============================================================================
// src/batch/batch_verification_kernel.hpp  -  WO-VSEPR-SIM-62B
// =============================================================================
//
// Internal kernel functions that operate on the types declared in
// include/batch/batch_verification.hpp.
//
// Consumers: src/batch/batch_verification.cpp, tests/test_batch_verification.cpp
//
// WO-VSEPR-SIM-62B  |  beta-12
// =============================================================================

#include "include/batch/batch_verification.hpp"
#include "include/vsim/vsim_document.hpp"
#include <string>
#include <vector>

namespace vsim {
namespace batch {

// ---------------------------------------------------------------------------
// load_verification_record
//   Reads verify_report.json from run_dir; returns MISSING record if absent.
// ---------------------------------------------------------------------------

VerificationRunRecord load_verification_record(const std::string& run_id,
                                               const std::string& run_dir);

// ---------------------------------------------------------------------------
// classify_failure_modes
//   Annotates rec.failure_modes based on per-check pass flags and status.
// ---------------------------------------------------------------------------

void classify_failure_modes(VerificationRunRecord& rec);

// ---------------------------------------------------------------------------
// aggregate_verification
//   Consumes all VerificationRunRecords, groups by policy.group_by axes,
//   computes pass-rate tables, and returns a BatchVerificationSummary.
// ---------------------------------------------------------------------------

BatchVerificationSummary aggregate_verification(
    const std::vector<VerificationRunRecord>& records,
    const BatchAggregateVerifySection&        policy);

// ---------------------------------------------------------------------------
// evaluate_gates
//   Evaluates pass-rate thresholds against `summary` and sets
//   batch_empirical_ready / gate_failure_reason accordingly.
// ---------------------------------------------------------------------------

void evaluate_gates(BatchVerificationSummary&               summary,
                    const BatchAggregateVerifyGatesSection& gates);

// ---------------------------------------------------------------------------
// Output writers
// ---------------------------------------------------------------------------

void write_batch_verify_summary(const BatchVerificationSummary& summary,
                                const std::string&              path);

void write_batch_verify_matrix(const BatchVerificationSummary& summary,
                               const std::string&              path);

void write_batch_failure_modes(const BatchVerificationSummary& summary,
                               const std::string&              path);

void write_batch_empirical_report(const BatchVerificationSummary& summary,
                                  const std::string&              study_name,
                                  const std::string&              path);

} // namespace batch
} // namespace vsim
