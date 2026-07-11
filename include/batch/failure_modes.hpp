#pragma once
/**
 * include/batch/failure_modes.hpp
 * ================================
 * WO-VSEPR-SIM-62B — Batch Failure Mode Enumeration
 *
 * Enumerated failure/warning codes emitted per run by the batch
 * verification aggregation kernel.  These codes appear in:
 *   - BatchRunRecord.failure_modes
 *   - VerificationRunRecord.failure_modes
 *   - batch_failure_modes.tsv
 *   - batch_empirical_report.md
 *
 * Naming convention: FAIL_* for definitive failures; WARN_* for marginal
 * conditions that do not flip overall_pass to false.
 *
 * WO-VSEPR-SIM-62B  |  beta-12
 */

#include <string>

namespace vsim {
namespace batch {

enum class BatchFailureMode {
	// ── Analysis / inference failures ────────────────────────────────────
	FAIL_MISSING_SCALE_EVIDENCE,    // M_op did not produce a valid ScaleSampleRecord
	FAIL_INVALID_RVE,               // No qualifying RVE window found
	FAIL_RUNTIME_CRASH,             // Run exited non-zero or produced no outputs

	// ── Verification check failures ───────────────────────────────────────
	FAIL_MASS_CONSERVATION,         // Relative mass error exceeds tolerance
	FAIL_STRUCTURE_COORDINATION,    // Coordination number outside expected range
	FAIL_RDF_REFERENCE,             // First or second peak position outside tolerance
	FAIL_MSD_SOLID_BOUND,           // MSD exceeds solid-state bound

	// ── Batch requirement failures ────────────────────────────────────────
	FAIL_OUTPUT_MISSING,            // Required file absent from run output
	FAIL_CHECK_MISSING,             // Required check absent from verify_report.json

	// ── Marginal warnings (do not fail overall_pass) ──────────────────────
	WARN_RDF_PEAK_WIDTH,            // Peak width outside expected range; position valid
	WARN_CONVERGENCE_SLOW,          // Converged but used >90 % of max_steps
	WARN_SCALE_MARGINAL,            // RVE candidate found but spatial_cv near threshold
};

inline const char* to_string(BatchFailureMode m) {
	switch (m) {
	case BatchFailureMode::FAIL_MISSING_SCALE_EVIDENCE:  return "FAIL_MISSING_SCALE_EVIDENCE";
	case BatchFailureMode::FAIL_INVALID_RVE:             return "FAIL_INVALID_RVE";
	case BatchFailureMode::FAIL_RUNTIME_CRASH:           return "FAIL_RUNTIME_CRASH";
	case BatchFailureMode::FAIL_MASS_CONSERVATION:       return "FAIL_MASS_CONSERVATION";
	case BatchFailureMode::FAIL_STRUCTURE_COORDINATION:  return "FAIL_STRUCTURE_COORDINATION";
	case BatchFailureMode::FAIL_RDF_REFERENCE:           return "FAIL_RDF_REFERENCE";
	case BatchFailureMode::FAIL_MSD_SOLID_BOUND:         return "FAIL_MSD_SOLID_BOUND";
	case BatchFailureMode::FAIL_OUTPUT_MISSING:          return "FAIL_OUTPUT_MISSING";
	case BatchFailureMode::FAIL_CHECK_MISSING:           return "FAIL_CHECK_MISSING";
	case BatchFailureMode::WARN_RDF_PEAK_WIDTH:          return "WARN_RDF_PEAK_WIDTH";
	case BatchFailureMode::WARN_CONVERGENCE_SLOW:        return "WARN_CONVERGENCE_SLOW";
	case BatchFailureMode::WARN_SCALE_MARGINAL:          return "WARN_SCALE_MARGINAL";
	default:                                             return "UNKNOWN";
	}
}

// Returns true for WARN_* codes (run is still empirical_ready)
inline bool is_warning(BatchFailureMode m) {
	switch (m) {
	case BatchFailureMode::WARN_RDF_PEAK_WIDTH:
	case BatchFailureMode::WARN_CONVERGENCE_SLOW:
	case BatchFailureMode::WARN_SCALE_MARGINAL:
		return true;
	default:
		return false;
	}
}

} // namespace batch
} // namespace vsim
