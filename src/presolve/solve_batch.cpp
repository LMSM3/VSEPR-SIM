// =============================================================================
// solve_batch.cpp  —  Live Persistent Instance + Batch Solve  (WO-XSUITE-02B)
// =============================================================================

#include "vsim/solve_batch.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <numeric>
#include <sstream>

namespace vsepr {
namespace presolve {

// ---------------------------------------------------------------------------
// batch_solve_run
// ---------------------------------------------------------------------------

BatchSolveResult batch_solve_run(
	const BatchSolveConfig& cfg,
	ShortSolveFn            solve_fn,
	std::ostream&           log)
{
	BatchSolveResult res;

	if (cfg.batch_count <= 0) {
		res.error = "batch_solve_run: batch_count must be > 0";
		return res;
	}

	log << "[batch_solve] mode=" << cfg.mode
		<< "  batches=" << cfg.batch_count
		<< "  seed_start=" << cfg.seed_start
		<< "  seed_stride=" << cfg.seed_stride
		<< "  max_steps=" << cfg.max_steps << "\n";

	double energy_sum = 0.0;

	for (int b = 0; b < cfg.batch_count; ++b) {
		int seed = cfg.seed_start + b * cfg.seed_stride;

		BatchSolveRecord rec;
		rec.seed  = seed;
		rec.batch = b;

		bool ok = solve_fn(seed, cfg.max_steps, rec);
		++res.seeds_run;

		if (ok && rec.converged) {
			++res.seeds_converged;
			energy_sum += rec.final_energy;
		}

		res.records.push_back(std::move(rec));
	}

	if (res.seeds_converged > 0)
		res.mean_energy = energy_sum / res.seeds_converged;

	// Write basis summary if requested
	if (!cfg.write_basis.empty()) {
		std::ofstream f(cfg.write_basis);
		if (f) {
			f << "{\"type\":\"presolve_basis_index\","
			  << "\"seeds_run\":" << res.seeds_run << ","
			  << "\"seeds_converged\":" << res.seeds_converged << ","
			  << "\"mean_energy\":" << res.mean_energy << "}\n";
			res.basis_path = cfg.write_basis;
			log << "[batch_solve] basis index written -> " << cfg.write_basis << "\n";
		}
	}

	// Write summary markdown
	if (!cfg.write_summary.empty()) {
		std::ofstream f(cfg.write_summary);
		if (f) {
			f << "# Presolve Batch Summary\n\n"
			  << "| Field | Value |\n|---|---|\n"
			  << "| mode | " << cfg.mode << " |\n"
			  << "| seeds_run | " << res.seeds_run << " |\n"
			  << "| seeds_converged | " << res.seeds_converged << " |\n"
			  << "| mean_energy | " << res.mean_energy << " |\n";
			res.summary_path = cfg.write_summary;
			log << "[batch_solve] summary written -> " << cfg.write_summary << "\n";
		}
	}

	res.ok = true;
	log << "[batch_solve] DONE  seeds=" << res.seeds_run
		<< "  converged=" << res.seeds_converged << "\n";
	return res;
}

// ---------------------------------------------------------------------------
// live_instance_flush_state  —  NDJSON atomic state snapshot
// ---------------------------------------------------------------------------

void live_instance_flush_state(
	const LiveXSuiteInstance& inst,
	const std::string&        path)
{
	if (path.empty()) return;
	std::ofstream f(path, std::ios::app);
	if (!f) return;

	f << "{\"type\":\"atomic_state\","
	  << "\"instance_id\":" << inst.instance_id << ","
	  << "\"frame\":" << inst.frame << ","
	  << "\"step\":" << inst.step << ","
	  << "\"time\":" << inst.time << ","
	  << "\"running\":" << (inst.running ? "true" : "false") << ","
	  << "\"paused\":" << (inst.paused  ? "true" : "false") << ","
	  << "\"n_atoms\":" << inst.truth.pos.size() / 3
	  << "}\n";
}

// ---------------------------------------------------------------------------
// live_instance_flush_health  —  NDJSON analysis/health snapshot
// ---------------------------------------------------------------------------

void live_instance_flush_health(
	const LiveXSuiteInstance& inst,
	const std::string&        path)
{
	if (path.empty()) return;
	std::ofstream f(path, std::ios::app);
	if (!f) return;

	f << "{\"type\":\"analysis_health\","
	  << "\"instance_id\":" << inst.instance_id << ","
	  << "\"step\":" << inst.step << ","
	  << "\"ke\":" << inst.dynamics.ke << ","
	  << "\"pe\":" << inst.dynamics.pe << ","
	  << "\"msd\":" << inst.analysis.msd
	  << "}\n";
}

} // namespace presolve
} // namespace vsepr
