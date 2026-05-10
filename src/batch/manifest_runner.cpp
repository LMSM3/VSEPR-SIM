/**
 * src/batch/manifest_runner.cpp
 * ==============================
 * WO-B9-001 — Batch Manifest Runner Implementation
 *
 * Sweep expansion:
 *   Cross-product of all sweep axes × seeds → flat list of BatchRunRecord stubs.
 *   Each run gets a unique 1-based index → run_NNNN folder.
 *
 * Run isolation:
 *   Each run_NNNN folder is created before the simulator is invoked.
 *   Artefacts written by the simulator are scoped to that folder.
 *
 * Output artefacts:
 *   batch_summary.tsv      — all runs, execution order
 *   ranked_candidates.tsv  — sorted by score_composite (desc)
 *   batch_report.md        — human-readable Markdown
 *
 * Physics:
 *   The default simulator is a deterministic stub (WO-B9-001).
 *   WO-B9-002 replaces it with the real steady-state gate pipeline.
 */

#include "src/batch/manifest_runner.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <numeric>
#include <random>
#include <sstream>

namespace fs = std::filesystem;

namespace vsim {
namespace batch {

// ─────────────────────────────────────────────────────────────────────────────
// Sweep expansion — Cartesian product of all axes × seeds
// ─────────────────────────────────────────────────────────────────────────────

static std::vector<BatchRunRecord> expand_sweep(const BatchManifestSection& m) {
	// Build axis list: if no sweep, single default combination
	std::vector<const BatchManifestSweepAxis*> axes;
	for (const auto& ax : m.sweep)
		if (!ax.values.empty()) axes.push_back(&ax);

	// Generate Cartesian product indices
	// Each element of `combos` is a map<param, value_string>
	std::vector<std::map<std::string, std::string>> combos;
	combos.push_back({}); // seed with empty combination

	for (const auto* ax : axes) {
		std::vector<std::map<std::string, std::string>> expanded;
		for (const auto& combo : combos) {
			for (const auto& val : ax->values) {
				auto c = combo;
				c[ax->param] = val;
				expanded.push_back(std::move(c));
			}
		}
		combos = std::move(expanded);
	}

	// Expand with seeds
	std::vector<BatchRunRecord> records;
	int idx = 1;
	for (const auto& combo : combos) {
		for (int s = 0; s < m.seeds; ++s) {
			BatchRunRecord r;
			r.run_index = idx++;
			r.run_id    = "run_" + [&](){
				std::ostringstream ss;
				ss << std::setw(4) << std::setfill('0') << r.run_index;
				return ss.str();
			}();
			r.params = combo;
			r.seed   = s;
			records.push_back(std::move(r));
		}
	}
	return records;
}

// ─────────────────────────────────────────────────────────────────────────────
// Default stub simulator — deterministic, seed-driven
// ─────────────────────────────────────────────────────────────────────────────

BatchRunRecord default_stub_simulator(const BatchRunRecord& proto,
									  const std::string& run_dir) {
	// Deterministic RNG from seed + run_index
	std::mt19937 rng(static_cast<uint32_t>(proto.seed * 10007 + proto.run_index));
	std::uniform_real_distribution<double> noise(-1.0, 1.0);
	std::uniform_real_distribution<double> unif(0.0, 1.0);

	BatchRunRecord r = proto;

	// Base energy influenced by sweep params (parse temperature_K if present)
	double base_energy = -120.0; // kcal/mol reference
	auto it = r.params.find("temperature_K");
	if (it != r.params.end()) {
		try {
			double T = std::stod(it->second);
			base_energy += 0.008314 * T; // rough thermal contribution
		} catch (...) {}
	}
	auto pit = r.params.find("pressure_GPa");
	if (pit != r.params.end()) {
		try {
			double P = std::stod(pit->second);
			base_energy += 15.0 * P; // pressure work term
		} catch (...) {}
	}

	r.final_energy = base_energy + noise(rng) * 5.0;
	r.rms_force    = 1e-5 + unif(rng) * 1e-3;
	r.steps_taken  = 200 + static_cast<int>(unif(rng) * 300);
	r.converged    = (r.rms_force < 1e-3);
	r.wall_ms      = 10.0 + unif(rng) * 90.0;

	// Scoring
	r.score_energy      = batch_score::energy_score(r.final_energy, -130.0);
	r.score_convergence = batch_score::convergence_score(r.steps_taken, 500, r.rms_force, 1e-4);
	r.score_composite   = batch_score::composite(r.score_energy, r.score_convergence,
												  r.converged ? 1.0 : 0.0);
	r.steady_pass       = r.converged; // stub: steady = converged

	// per-run artefacts
	// run_meta.json
	{
		std::ofstream mf(run_dir + "/run_meta.json");
		mf << "{\n";
		mf << "  \"run_id\":       \"" << r.run_id << "\",\n";
		mf << "  \"run_index\":    " << r.run_index << ",\n";
		mf << "  \"seed\":         " << r.seed << ",\n";
		mf << "  \"converged\":    " << (r.converged ? "true" : "false") << ",\n";
		mf << "  \"final_energy\": " << std::fixed << std::setprecision(6) << r.final_energy << ",\n";
		mf << "  \"rms_force\":    " << r.rms_force << ",\n";
		mf << "  \"steps_taken\":  " << r.steps_taken << ",\n";
		mf << "  \"wall_ms\":      " << std::fixed << std::setprecision(2) << r.wall_ms << ",\n";
		mf << "  \"score_composite\": " << std::fixed << std::setprecision(4) << r.score_composite << ",\n";
		mf << "  \"steady_pass\":  " << (r.steady_pass ? "true" : "false") << ",\n";
		mf << "  \"params\": {\n";
		bool first = true;
		for (const auto& [k, v] : r.params) {
			if (!first) mf << ",\n";
			mf << "    \"" << k << "\": \"" << v << "\"";
			first = false;
		}
		mf << "\n  }\n}\n";
	}

	// metrics.tsv — synthetic step trace
	{
		std::ofstream tf(run_dir + "/metrics.tsv");
		tf << "step\tenergy_kcal_mol\trms_force\n";
		double E = base_energy * 0.5; // start high
		double F = 1.0;
		std::mt19937 rng2(rng());
		std::uniform_real_distribution<double> jitter(-0.5, 0.5);
		for (int step = 1; step <= r.steps_taken; ++step) {
			double frac = static_cast<double>(step) / r.steps_taken;
			E += (r.final_energy - E) * 0.05 + jitter(rng2) * 0.1;
			F  = std::max(1e-6, F * (1.0 - frac * 0.01));
			if (step % 20 == 0 || step == r.steps_taken)
				tf << step << "\t"
				   << std::fixed << std::setprecision(4) << E << "\t"
				   << std::scientific << std::setprecision(3) << F << "\n";
		}
	}

	return r;
}

// ─────────────────────────────────────────────────────────────────────────────
// Output writers
// ─────────────────────────────────────────────────────────────────────────────

static void write_summary_tsv(const BatchRunnerResult& result) {
	std::ofstream f(result.summary_tsv_path);
	// Header
	f << "run_id\tseed\tconverged\tsteady_pass\tfinal_energy\trms_force"
		 "\tsteps\twall_ms\tscore_energy\tscore_convergence\tscore_composite"
		 "\tfailure_reason";

	// Collect all unique param names for column headers
	std::vector<std::string> param_names;
	for (const auto& r : result.records)
		for (const auto& [k, v] : r.params)
			if (std::find(param_names.begin(), param_names.end(), k) == param_names.end())
				param_names.push_back(k);
	for (const auto& p : param_names) f << "\t" << p;
	f << "\n";

	for (const auto& r : result.records) {
		f << r.run_id << "\t"
		  << r.seed << "\t"
		  << (r.converged ? "true" : "false") << "\t"
		  << (r.steady_pass ? "true" : "false") << "\t"
		  << std::fixed << std::setprecision(4) << r.final_energy << "\t"
		  << std::scientific << std::setprecision(3) << r.rms_force << "\t"
		  << r.steps_taken << "\t"
		  << std::fixed << std::setprecision(1) << r.wall_ms << "\t"
		  << std::fixed << std::setprecision(4) << r.score_energy << "\t"
		  << r.score_convergence << "\t"
		  << r.score_composite << "\t"
		  << r.failure_reason;
		for (const auto& p : param_names) {
			auto it = r.params.find(p);
			f << "\t" << (it != r.params.end() ? it->second : "");
		}
		f << "\n";
	}
}

static void write_ranked_tsv(const BatchRunnerResult& result) {
	// Rank by score_composite descending
	std::vector<const BatchRunRecord*> ordered;
	for (const auto& r : result.records) ordered.push_back(&r);
	std::stable_sort(ordered.begin(), ordered.end(),
		[](const BatchRunRecord* a, const BatchRunRecord* b) {
			return a->score_composite > b->score_composite;
		});

	std::ofstream f(result.ranked_tsv_path);
	f << "rank\trun_id\tseed\tconverged\tsteady_pass"
		 "\tscore_composite\tscore_energy\tscore_convergence"
		 "\tfinal_energy\tfailure_reason\n";

	int rank = 1;
	for (const auto* r : ordered) {
		f << rank++ << "\t"
		  << r->run_id << "\t"
		  << r->seed << "\t"
		  << (r->converged ? "true" : "false") << "\t"
		  << (r->steady_pass ? "true" : "false") << "\t"
		  << std::fixed << std::setprecision(4) << r->score_composite << "\t"
		  << r->score_energy << "\t"
		  << r->score_convergence << "\t"
		  << r->final_energy << "\t"
		  << r->failure_reason << "\n";
	}
}

static void write_report_md(const BatchRunnerResult& result,
							const BatchManifestSection& manifest) {
	std::ofstream f(result.report_md_path);

	// Timestamp
	std::time_t now = std::time(nullptr);
	char ts[32];
	std::strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", std::localtime(&now));

	f << "# Batch Report: " << result.batch_id << "\n\n";
	f << "> Generated: " << ts << "  \n";
	f << "> Work order: WO-B9-001  \n";
	if (!manifest.description.empty())
		f << "> Description: " << manifest.description << "  \n";
	f << "\n";

	f << "## Summary\n\n";
	f << "| Field | Value |\n";
	f << "|---|---|\n";
	f << "| Batch ID | `" << result.batch_id << "` |\n";
	f << "| Total runs | " << result.n_total << " |\n";
	f << "| Converged | " << result.n_converged << " |\n";
	f << "| Failed | " << result.n_failed << " |\n";
	f << "| Best energy (kcal/mol) | "
	  << std::fixed << std::setprecision(4) << result.best_energy << " |\n";
	f << "| Mean energy (kcal/mol) | "
	  << std::fixed << std::setprecision(4) << result.mean_energy << " |\n";
	f << "| Total wall time (ms) | "
	  << std::fixed << std::setprecision(1) << result.wall_ms_total << " |\n";
	f << "\n";

	// Sweep parameter summary
	if (!manifest.sweep.empty()) {
		f << "## Sweep Parameters\n\n";
		f << "| Axis | Values |\n";
		f << "|---|---|\n";
		for (const auto& ax : manifest.sweep) {
			f << "| `" << ax.param << "` | ";
			bool first = true;
			for (const auto& v : ax.values) {
				if (!first) f << ", ";
				f << "`" << v << "`";
				first = false;
			}
			f << " |\n";
		}
		f << "\n";
	}

	// Top-5 ranked candidates
	std::vector<const BatchRunRecord*> ordered;
	for (const auto& r : result.records) ordered.push_back(&r);
	std::stable_sort(ordered.begin(), ordered.end(),
		[](const BatchRunRecord* a, const BatchRunRecord* b) {
			return a->score_composite > b->score_composite;
		});

	f << "## Top Candidates\n\n";
	f << "| Rank | Run | Seed | Converged | Score | Energy (kcal/mol) |\n";
	f << "|---|---|---|---|---|---|\n";
	int n = std::min(static_cast<int>(ordered.size()), 5);
	for (int i = 0; i < n; ++i) {
		const auto* r = ordered[i];
		f << "| " << (i+1) << " | `" << r->run_id << "` | " << r->seed
		  << " | " << (r->converged ? "yes" : "no")
		  << " | " << std::fixed << std::setprecision(4) << r->score_composite
		  << " | " << r->final_energy << " |\n";
	}
	f << "\n";

	f << "## Output Artefacts\n\n";
	f << "| File | Purpose |\n";
	f << "|---|---|\n";
	f << "| `batch_summary.tsv` | All runs, execution order |\n";
	f << "| `ranked_candidates.tsv` | All runs, sorted by composite score |\n";
	f << "| `batch_report.md` | This document |\n";
	f << "| `run_NNNN/run_meta.json` | Per-run provenance |\n";
	f << "| `run_NNNN/metrics.tsv` | Per-run energy / force trace |\n";
	f << "\n";

	f << "## Gate Status (WO-B9-002 — pending)\n\n";
	f << "Steady-state gates (`energy_slope_gate`, `flux_balance_gate`, "
		 "`residence_time_stability_gate`, `energy_drift_gate`, "
		 "`wall_residence_gate`) are defined in WO-B9-002 and not yet wired.\n"
		 "All `steady_pass` values in this report reflect converged == true (stub).\n\n";

	f << "## Discovery Ranking (WO-B9-003 — pending)\n\n";
	f << "Full ranking engine with `candidate_id`, `valid_energy`, "
		 "`steady_pass`, and `failure_reason` filtering is defined in WO-B9-003.\n\n";

	f << "---\n\n";
	f << "*VSEPR-SIM beta-9 | WO-B9-001 | Batch Manifest Runner*\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// write_batch_outputs — public
// ─────────────────────────────────────────────────────────────────────────────

bool write_batch_outputs(BatchRunnerResult& result) {
	try {
		write_summary_tsv(result);
		write_ranked_tsv(result);
		return true;
	} catch (...) {
		return false;
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// run_manifest — public entry point
// ─────────────────────────────────────────────────────────────────────────────

BatchRunnerResult run_manifest(const BatchManifestSection& manifest,
							   const BatchRunnerConfig& cfg,
							   SimulateRunFn simulate) {
	BatchRunnerResult result;
	result.batch_id = manifest.batch_id;

	// Choose simulator
	if (!simulate) simulate = default_stub_simulator;

	// Create batch root directory
	const fs::path batch_root = fs::path(manifest.output_root) / manifest.batch_id;
	fs::create_directories(batch_root);
	result.batch_dir = batch_root.string();

	// Set output artefact paths
	result.summary_tsv_path = (batch_root / "batch_summary.tsv").string();
	result.ranked_tsv_path  = (batch_root / "ranked_candidates.tsv").string();
	result.report_md_path   = (batch_root / "batch_report.md").string();

	// Expand sweep
	auto records = expand_sweep(manifest);
	const int total = static_cast<int>(records.size());
	result.n_total = total;

	if (cfg.verbose) {
		std::printf("\n+------------------------------------------------------------------+\n");
		std::printf("| WO-B9-001  Batch Manifest Runner\n");
		std::printf("+------------------------------------------------------------------+\n");
		std::printf("  batch_id   : %s\n", manifest.batch_id.c_str());
		std::printf("  output_dir : %s\n", result.batch_dir.c_str());
		std::printf("  total runs : %d\n", total);
		std::printf("  seeds      : %d\n", manifest.seeds);
		std::printf("  score_by   : %s\n", manifest.score_by.c_str());
		for (const auto& ax : manifest.sweep) {
			std::printf("  sweep %-16s  [", ax.param.c_str());
			for (size_t i = 0; i < ax.values.size(); ++i) {
				if (i) std::printf(", ");
				std::printf("%s", ax.values[i].c_str());
			}
			std::printf("]\n");
		}
		std::printf("\n");
	}

	// Execute runs
	auto wall_start = std::chrono::steady_clock::now();
	double energy_sum = 0.0;
	double best_e = 1e18;

	for (auto& rec : records) {
		// Create isolated run folder
		const fs::path run_dir = batch_root / rec.run_id;
		fs::create_directories(run_dir);
		rec.run_dir = run_dir.string();

		// Execute
		const auto t0 = std::chrono::steady_clock::now();
		BatchRunRecord done = simulate(rec, rec.run_dir);
		const auto t1 = std::chrono::steady_clock::now();
		done.wall_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

		// Accumulate stats
		if (done.converged) ++result.n_converged;
		else                ++result.n_failed;

		if (std::isfinite(done.final_energy)) {
			energy_sum += done.final_energy;
			if (done.final_energy < best_e) best_e = done.final_energy;
		}

		if (cfg.verbose) {
			std::printf("  [%4d/%4d]  %-10s  seed=%d  E=%8.3f  F=%6.3e  "
						"conv=%-5s  score=%.3f\n",
				done.run_index, total,
				done.run_id.c_str(), done.seed,
				done.final_energy, done.rms_force,
				done.converged ? "true" : "false",
				done.score_composite);
		}

		result.records.push_back(std::move(done));

		if (cfg.abort_on_fail && !result.records.back().converged) {
			std::printf("  [abort_on_fail] run %d did not converge — stopping.\n",
				result.records.back().run_index);
			break;
		}
	}

	// Assign ranks
	{
		std::vector<int> idx(result.records.size());
		std::iota(idx.begin(), idx.end(), 0);
		std::stable_sort(idx.begin(), idx.end(), [&](int a, int b) {
			return result.records[a].score_composite > result.records[b].score_composite;
		});
		for (int i = 0; i < static_cast<int>(idx.size()); ++i)
			result.records[idx[i]].rank = i + 1;
	}

	result.best_energy   = (best_e < 1e17) ? best_e : 0.0;
	result.mean_energy   = result.n_total > 0 ? energy_sum / result.n_total : 0.0;
	result.wall_ms_total = std::chrono::duration<double, std::milli>(
		std::chrono::steady_clock::now() - wall_start).count();

	// Write batch-level artefacts
	write_summary_tsv(result);
	write_ranked_tsv(result);
	write_report_md(result, manifest);

	if (cfg.verbose) {
		std::printf("\n  Batch complete.\n");
		std::printf("  Converged   : %d / %d\n", result.n_converged, result.n_total);
		std::printf("  Best energy : %.4f kcal/mol\n", result.best_energy);
		std::printf("  Mean energy : %.4f kcal/mol\n", result.mean_energy);
		std::printf("  Wall time   : %.1f ms\n", result.wall_ms_total);
		std::printf("\n  Artefacts written:\n");
		std::printf("    %s\n", result.summary_tsv_path.c_str());
		std::printf("    %s\n", result.ranked_tsv_path.c_str());
		std::printf("    %s\n", result.report_md_path.c_str());
		std::printf("\n");
	}

	return result;
}

} // namespace batch
} // namespace vsim
