// =============================================================================
// release_gate.cpp  —  Release Improvement Presolve Gate  (WO-XSUITE-02B)
// =============================================================================

#include "vsim/release_gate.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

namespace vsepr {
namespace presolve {

// ---------------------------------------------------------------------------
// release_gate_evaluate
// ---------------------------------------------------------------------------

ReleaseGateResult release_gate_evaluate(
	const ReleaseGateConfig& cfg,
	const VersionMetrics&    baseline,
	const VersionMetrics&    candidate,
	std::ostream&            log)
{
	ReleaseGateResult res;

	res.hash_score     = candidate.hash_success_rate;
	res.quality_score  = candidate.eigen_quality;
	res.curvefit_score = candidate.curvefit_score;
	res.failure_rate   = candidate.failure_rate;

	// V = (M_new - M_old) / (|M_old| + ε)
	// Use composite metric: speed improvement + energy stability
	double eps = 1e-12;
	double m_old = baseline.runtime_speed_s  + baseline.energy_stability;
	double m_new = candidate.runtime_speed_s + candidate.energy_stability;
	res.version_improvement = (m_new - m_old) / (std::abs(m_old) + eps);

	// Release score: R = a1·H + a2·Q + a3·C + a4·V - a5·F
	res.release_score =
		  cfg.w_hash    * res.hash_score
		+ cfg.w_quality * res.quality_score
		+ cfg.w_curvefit* res.curvefit_score
		+ cfg.w_version * std::max(-1.0, std::min(1.0, res.version_improvement))
		- cfg.w_failure * res.failure_rate;

	// Gate checks
	std::string reason;
	if (res.hash_score < cfg.require_hash_success)
		reason += "hash_success_rate=" + std::to_string(res.hash_score)
			   + " < " + std::to_string(cfg.require_hash_success) + "; ";
	if (res.quality_score < cfg.require_eigen_quality)
		reason += "eigen_quality=" + std::to_string(res.quality_score)
			   + " < " + std::to_string(cfg.require_eigen_quality) + "; ";
	if (res.curvefit_score < cfg.require_curvefit_score)
		reason += "curvefit_score=" + std::to_string(res.curvefit_score)
			   + " < " + std::to_string(cfg.require_curvefit_score) + "; ";
	if (res.failure_rate > cfg.max_failure_rate)
		reason += "failure_rate=" + std::to_string(res.failure_rate)
			   + " > " + std::to_string(cfg.max_failure_rate) + "; ";

	res.passed       = reason.empty();
	res.verdict      = res.passed ? "PASS" : "BLOCKED";
	res.block_reason = reason;

	log << "[release_gate] " << cfg.baseline_version
		<< " -> " << cfg.candidate_version
		<< "  verdict=" << res.verdict
		<< "  score=" << res.release_score << "\n";
	if (!res.passed)
		log << "[release_gate] BLOCKED: " << reason << "\n";

	// Write outputs
	if (!cfg.write_gate_report.empty()) {
		release_gate_write_report(res, cfg, baseline, candidate, cfg.write_gate_report);
		res.report_path = cfg.write_gate_report;
	}
	if (!cfg.write_comparison.empty()) {
		release_gate_write_comparison(res, cfg, baseline, candidate, cfg.write_comparison);
		res.comparison_path = cfg.write_comparison;
	}

	return res;
}

// ---------------------------------------------------------------------------
// release_gate_write_report
// ---------------------------------------------------------------------------

void release_gate_write_report(
	const ReleaseGateResult& result,
	const ReleaseGateConfig& cfg,
	const VersionMetrics&    baseline,
	const VersionMetrics&    candidate,
	const std::string&       path)
{
	std::ofstream f(path);
	if (!f) return;

	f << "# Release Gate Report\n\n"
	  << "## Verdict: **" << result.verdict << "**\n\n";
	if (!result.passed)
		f << "> Blocked reason: " << result.block_reason << "\n\n";

	f << "## Scores\n\n"
	  << "| Metric | Baseline (" << cfg.baseline_version << ") "
	  << "| Candidate (" << cfg.candidate_version << ") "
	  << "| Threshold | Status |\n"
	  << "|---|---|---|---|---|\n"
	  << "| hash_success_rate | " << baseline.hash_success_rate
	  << " | " << result.hash_score
	  << " | ≥ " << cfg.require_hash_success
	  << " | " << (result.hash_score >= cfg.require_hash_success ? "✓" : "✗") << " |\n"
	  << "| eigen_quality | " << baseline.eigen_quality
	  << " | " << result.quality_score
	  << " | ≥ " << cfg.require_eigen_quality
	  << " | " << (result.quality_score >= cfg.require_eigen_quality ? "✓" : "✗") << " |\n"
	  << "| curvefit_score | " << baseline.curvefit_score
	  << " | " << result.curvefit_score
	  << " | ≥ " << cfg.require_curvefit_score
	  << " | " << (result.curvefit_score >= cfg.require_curvefit_score ? "✓" : "✗") << " |\n"
	  << "| failure_rate | " << baseline.failure_rate
	  << " | " << result.failure_rate
	  << " | ≤ " << cfg.max_failure_rate
	  << " | " << (result.failure_rate <= cfg.max_failure_rate ? "✓" : "✗") << " |\n"
	  << "| release_score | — | " << result.release_score
	  << " | — | — |\n"
	  << "| version_improvement | — | " << result.version_improvement
	  << " | — | — |\n";
}

// ---------------------------------------------------------------------------
// release_gate_write_comparison
// ---------------------------------------------------------------------------

void release_gate_write_comparison(
	const ReleaseGateResult& result,
	const ReleaseGateConfig& cfg,
	const VersionMetrics&    baseline,
	const VersionMetrics&    candidate,
	const std::string&       path)
{
	std::ofstream f(path, std::ios::app);
	if (!f) return;

	f << "{\"type\":\"release_comparison\","
	  << "\"baseline\":\"" << cfg.baseline_version << "\","
	  << "\"candidate\":\"" << cfg.candidate_version << "\","
	  << "\"verdict\":\"" << result.verdict << "\","
	  << "\"release_score\":" << result.release_score << ","
	  << "\"hash_score\":" << result.hash_score << ","
	  << "\"quality_score\":" << result.quality_score << ","
	  << "\"curvefit_score\":" << result.curvefit_score << ","
	  << "\"failure_rate\":" << result.failure_rate << ","
	  << "\"version_improvement\":" << result.version_improvement
	  << "}\n";
}

} // namespace presolve
} // namespace vsepr
