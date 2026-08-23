#pragma once
// =============================================================================
// release_gate.hpp  —  Release Improvement Presolve Gate  (WO-XSUITE-02B)
// =============================================================================
//
// Final gate before a new runtime version is tagged.  Aggregates:
//   H  = hash replay success rate       (≥ 0.999 required)
//   Q  = eigenmode quality score        (≥ Q_min required)
//   C  = curvefit validation score      (≥ C_min required)
//   F  = failure rate                   (≤ F_max required)
//   V  = version-to-version improvement (informational)
//
// Release score: R = a1·H + a2·Q + a3·C + a4·V - a5·F
//
// =============================================================================

#include <string>
#include <vector>
#include <ostream>
#include <iostream>

namespace vsepr {
namespace presolve {

// ---------------------------------------------------------------------------
// ReleaseGateConfig  —  mirrors [release_gate] .X block
// ---------------------------------------------------------------------------

struct ReleaseGateConfig {
	std::string baseline_version;
	std::string candidate_version;
	double      require_hash_success   = 0.999;
	double      require_curvefit_score = 0.85;
	double      require_eigen_quality  = 0.80;
	double      max_failure_rate       = 0.01;
	std::string write_gate_report;      // reports/release_gate.md
	std::string write_comparison;       // archive/release_comparison.ndjson
	// Score weights
	double      w_hash     = 0.30;
	double      w_quality  = 0.25;
	double      w_curvefit = 0.25;
	double      w_version  = 0.10;
	double      w_failure  = 0.10;
};

// ---------------------------------------------------------------------------
// BaselineMetrics / CandidateMetrics  —  per-version inputs
// ---------------------------------------------------------------------------

struct VersionMetrics {
	std::string version;
	double      hash_success_rate = 0.0;  // H ∈ [0,1]
	double      eigen_quality     = 0.0;  // Q ∈ [0,1]
	double      curvefit_score    = 0.0;  // C ∈ [0,1]
	double      failure_rate      = 0.0;  // F ∈ [0,1]
	double      runtime_speed_s   = 0.0;  // wall-clock for reference run
	double      energy_stability  = 0.0;  // σ(E) / |E_mean|
};

// ---------------------------------------------------------------------------
// ReleaseGateResult
// ---------------------------------------------------------------------------

struct ReleaseGateResult {
	bool        passed         = false;
	std::string verdict;          // "PASS" | "BLOCKED"
	std::string block_reason;     // non-empty when blocked

	double      hash_score     = 0.0;
	double      quality_score  = 0.0;
	double      curvefit_score = 0.0;
	double      version_improvement = 0.0;  // V = (M_new - M_old) / (|M_old| + ε)
	double      failure_rate   = 0.0;
	double      release_score  = 0.0;       // R = weighted sum

	std::string report_path;
	std::string comparison_path;
};

// ---------------------------------------------------------------------------
// release_gate_evaluate  —  compute gate result from two VersionMetrics
// ---------------------------------------------------------------------------

ReleaseGateResult release_gate_evaluate(
	const ReleaseGateConfig& cfg,
	const VersionMetrics&    baseline,
	const VersionMetrics&    candidate,
	std::ostream&            log = std::cout);

// ---------------------------------------------------------------------------
// release_gate_write_report  —  emit markdown report to path
// ---------------------------------------------------------------------------

void release_gate_write_report(
	const ReleaseGateResult& result,
	const ReleaseGateConfig& cfg,
	const VersionMetrics&    baseline,
	const VersionMetrics&    candidate,
	const std::string&       path);

// ---------------------------------------------------------------------------
// release_gate_write_comparison  —  emit NDJSON comparison record
// ---------------------------------------------------------------------------

void release_gate_write_comparison(
	const ReleaseGateResult& result,
	const ReleaseGateConfig& cfg,
	const VersionMetrics&    baseline,
	const VersionMetrics&    candidate,
	const std::string&       path);

} // namespace presolve
} // namespace vsepr
