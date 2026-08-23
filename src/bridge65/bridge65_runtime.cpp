/**
 * bridge65_runtime.cpp
 * ====================
 * WO-BRIDGE-65 runtime integration and validation gate.
 *
 * Provides:
 *   - Bridge65Runtime  — top-level object that holds config, logger, and
 *     empirical data; orchestrates the bridge validation gate.
 *   - make_default_runtime()  — convenience factory for smoke tests.
 *   - bridge65_validate_force_agreement()  — compare matrix-force path
 *     against classical MD path and report the agreement ratio.
 *
 * Subatomic sampling: DISABLED.  The config field subatomic_enabled is
 * asserted false at runtime.  Any attempt to enable it during a bridge
 * phase run is rejected with a hard error.
 *
 * WO-BRIDGE-65  |  v5.1.13
 */

#include "vsim/bridge65/bridge65_config.hpp"
#include "vsim/bridge65/atom_event_logger.hpp"
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <vector>
#include <cmath>

namespace vsim::bridge65 {

// ============================================================================
// Bridge65Runtime
// ============================================================================

class Bridge65Runtime {
public:
	explicit Bridge65Runtime(Bridge65Config cfg = {})
		: cfg_(std::move(cfg)),
		  logger_(cfg_.logger)
	{
		if (cfg_.subatomic_enabled) {
			// Hard policy: subatomic sampling is reserved and disabled here.
			throw std::logic_error(
				"[WO-BRIDGE-65] subatomic_enabled=true is rejected. "
				"Subatomic sampling requires its own work order and gate. "
				"Set subatomic_enabled=false in your bridge65 config.");
		}

		if (!cfg_.enabled) return;

		std::fprintf(stdout,
			"[BRIDGE65] WO-BRIDGE-65 runtime initialised\n"
			"  version=%s  mode=%s  subatomic=%s\n",
			cfg_.version.c_str(),
			cfg_.mode.c_str(),
			cfg_.subatomic_enabled ? "ENABLED (rejected)" : "DISABLED");
	}

	// Called at the start of each MD step.
	void begin_step(std::uint64_t step, double time_s) {
		if (!cfg_.enabled) return;
		logger_.begin_step(step, time_s);
	}

	// Submit one atom event from the integration loop.
	void submit_event(const AtomEvent& ev) {
		if (!cfg_.enabled) return;
		logger_.log(ev);
	}

	// Called at the end of each MD step.
	void end_step(std::uint64_t step, double time_s) {
		if (!cfg_.enabled) return;
		logger_.end_step(step, time_s);
	}

	// Force flush all outputs.
	void flush() { logger_.flush(); }

	const Bridge65Config& config() const { return cfg_; }

private:
	Bridge65Config   cfg_;
	AtomEventLogger  logger_;
};

// ============================================================================
// Force agreement validation
// ============================================================================

/**
 * Compare two sets of per-atom force magnitudes (matrix path vs MD path).
 * Returns the Pearson correlation as the agreement metric.
 * Gate: must be >= config.matrix.force_agreement_min (default 0.999).
 *
 * Returns true if the gate passes.
 */
bool bridge65_validate_force_agreement(
	const std::vector<double>& matrix_forces,
	const std::vector<double>& md_forces,
	const Bridge65MatrixConfig& cfg,
	double* out_agreement = nullptr)
{
	if (matrix_forces.size() != md_forces.size() || matrix_forces.empty()) {
		if (out_agreement) *out_agreement = 0.0;
		return false;
	}

	const std::size_t N = matrix_forces.size();
	double sum_a = 0.0, sum_b = 0.0;
	for (std::size_t i = 0; i < N; ++i) {
		sum_a += matrix_forces[i];
		sum_b += md_forces[i];
	}
	double mean_a = sum_a / N, mean_b = sum_b / N;

	double cov = 0.0, var_a = 0.0, var_b = 0.0;
	for (std::size_t i = 0; i < N; ++i) {
		double da = matrix_forces[i] - mean_a;
		double db = md_forces[i]     - mean_b;
		cov   += da * db;
		var_a += da * da;
		var_b += db * db;
	}

	double denom = std::sqrt(var_a * var_b);
	double agreement = (denom < 1e-15) ? 1.0 : (cov / denom);
	if (out_agreement) *out_agreement = agreement;

	bool pass = agreement >= cfg.force_agreement_min;
	std::fprintf(stdout,
		"[BRIDGE65] force_agreement=%.6f gate=%.3f %s\n",
		agreement, cfg.force_agreement_min, pass ? "PASS" : "FAIL");
	return pass;
}

// ============================================================================
// Convenience factory
// ============================================================================

Bridge65Runtime make_default_runtime() {
	Bridge65Config cfg;
	cfg.logger.jsonl_path = "events/atom_events.jsonl";
	cfg.logger.tsv_path   = "events/event_summary.tsv";
	cfg.logger.run_id     = "bridge65_default";
	return Bridge65Runtime(cfg);
}

} // namespace vsim::bridge65
