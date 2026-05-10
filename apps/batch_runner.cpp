/**
 * apps/batch_runner.cpp
 * ======================
 * WO-B9-001 — Beta9 Batch Manifest Runner: CLI Entry Point
 *
 * Usage:
 *   batch_runner <manifest.json> [--dry-run] [--quiet] [--abort-on-fail]
 *                               [--max-steps N] [--output-root PATH]
 *
 * Arguments:
 *   <manifest.json>        Path to batch manifest (required).
 *   --dry-run              Parse and validate; print plan; do not execute.
 *   --quiet                Suppress per-run progress lines.
 *   --abort-on-fail        Stop on first non-converged run.
 *   --max-steps N          Override FIRE step limit (default: 500).
 *   --output-root PATH     Override manifest output_root.
 *   --help                 Print usage and exit.
 *
 * Outputs (under runs/<BATCH_ID>/):
 *   run_NNNN/run_meta.json        per-run provenance
 *   run_NNNN/metrics.tsv          per-run energy / force trace
 *   batch_summary.tsv             all runs, execution order
 *   ranked_candidates.tsv         sorted by composite score
 *   batch_report.md               human-readable Markdown report
 *
 * Exit codes:
 *   0  all runs completed (some may not have converged)
 *   1  manifest load or validation failed
 *   2  no runs completed (abort_on_fail triggered on run 1)
 *
 * WO-B9-001  |  VSEPR-SIM beta-9
 */

#include "src/batch/manifest_loader.hpp"
#include "src/batch/manifest_runner.hpp"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

// ─────────────────────────────────────────────────────────────────────────────
// Usage
// ─────────────────────────────────────────────────────────────────────────────

static void print_usage(const char* argv0) {
	std::cout <<
		"Usage:\n"
		"  " << argv0 << " <manifest.json> [options]\n\n"
		"Options:\n"
		"  --dry-run          Parse + validate + print plan; no execution.\n"
		"  --quiet            Suppress per-run progress output.\n"
		"  --abort-on-fail    Stop on first non-converged run.\n"
		"  --max-steps N      Override FIRE step limit (default 500).\n"
		"  --output-root PATH Override manifest output_root field.\n"
		"  --help             Print this message and exit.\n\n"
		"Outputs  runs/<BATCH_ID>/\n"
		"  run_NNNN/run_meta.json    per-run provenance\n"
		"  run_NNNN/metrics.tsv      per-run energy/force trace\n"
		"  batch_summary.tsv         all runs, execution order\n"
		"  ranked_candidates.tsv     sorted by composite score\n"
		"  batch_report.md           Markdown report\n\n"
		"WO-B9-001  |  VSEPR-SIM beta-9\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// main
// ─────────────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
	// ── Parse arguments ──────────────────────────────────────────────────────
	std::string manifest_path;
	bool dry_run       = false;
	bool quiet         = false;
	bool abort_on_fail = false;
	int  max_steps     = 500;
	std::string output_root_override;

	for (int i = 1; i < argc; ++i) {
		const char* arg = argv[i];
		if (std::strcmp(arg, "--help") == 0 || std::strcmp(arg, "-h") == 0) {
			print_usage(argv[0]);
			return 0;
		} else if (std::strcmp(arg, "--dry-run") == 0) {
			dry_run = true;
		} else if (std::strcmp(arg, "--quiet") == 0) {
			quiet = true;
		} else if (std::strcmp(arg, "--abort-on-fail") == 0) {
			abort_on_fail = true;
		} else if (std::strcmp(arg, "--max-steps") == 0 && i + 1 < argc) {
			max_steps = std::atoi(argv[++i]);
		} else if (std::strcmp(arg, "--output-root") == 0 && i + 1 < argc) {
			output_root_override = argv[++i];
		} else if (arg[0] != '-') {
			manifest_path = arg;
		} else {
			std::cerr << "Unknown option: " << arg << "\n";
			print_usage(argv[0]);
			return 1;
		}
	}

	if (manifest_path.empty()) {
		std::cerr << "Error: manifest path required.\n\n";
		print_usage(argv[0]);
		return 1;
	}

	// ── Load manifest ────────────────────────────────────────────────────────
	vsim::BatchManifestSection manifest;
	std::string load_error;

	if (!vsim::batch::load_manifest(manifest_path, manifest, load_error)) {
		std::cerr << "Error loading manifest: " << load_error << "\n";
		return 1;
	}

	// Apply CLI overrides
	if (!output_root_override.empty())
		manifest.output_root = output_root_override;

	// Validate
	const auto vr = vsim::batch::validate_manifest(manifest);
	for (const auto& w : vr.warnings)
		std::cout << "  [warn] " << w << "\n";
	if (!vr.ok) {
		for (const auto& e : vr.errors)
			std::cerr << "  [error] " << e << "\n";
		std::cerr << "Manifest validation failed.\n";
		return 1;
	}

	// ── Print plan ───────────────────────────────────────────────────────────
	std::cout << "\n";
	std::cout << "+------------------------------------------------------------------+\n";
	std::cout << "| WO-B9-001  Batch Manifest Runner\n";
	std::cout << "+------------------------------------------------------------------+\n";
	std::cout << "  manifest   : " << manifest_path << "\n";
	std::cout << "  batch_id   : " << manifest.batch_id << "\n";
	std::cout << "  total runs : " << manifest.total_runs() << "\n";
	std::cout << "  seeds      : " << manifest.seeds << "\n";
	std::cout << "  score_by   : " << manifest.score_by << "\n";
	std::cout << "  output_root: " << manifest.output_root << "\n";
	if (!manifest.description.empty())
		std::cout << "  description: " << manifest.description << "\n";
	std::cout << "\n";

	if (!manifest.sweep.empty()) {
		std::cout << "  Sweep axes:\n";
		for (const auto& ax : manifest.sweep) {
			std::cout << "    " << ax.param << "  [";
			for (size_t i = 0; i < ax.values.size(); ++i) {
				if (i) std::cout << ", ";
				std::cout << ax.values[i];
			}
			std::cout << "]\n";
		}
		std::cout << "\n";
	}

	if (dry_run) {
		std::cout << "  [dry-run] plan printed; no runs executed.\n\n";
		return 0;
	}

	// ── Execute ──────────────────────────────────────────────────────────────
	vsim::batch::BatchRunnerConfig cfg;
	cfg.verbose        = !quiet;
	cfg.abort_on_fail  = abort_on_fail;
	cfg.max_steps      = max_steps;

	const auto result = vsim::batch::run_manifest(manifest, cfg);

	if (!result.ok()) {
		std::cerr << "Batch produced no results.\n";
		return 2;
	}

	// ── Final summary ────────────────────────────────────────────────────────
	std::cout << "\n+------------------------------------------------------------------+\n";
	std::cout << "| Batch complete: " << manifest.batch_id << "\n";
	std::cout << "+------------------------------------------------------------------+\n";
	std::cout << "  Total    : " << result.n_total << "\n";
	std::cout << "  Converged: " << result.n_converged << "\n";
	std::cout << "  Failed   : " << result.n_failed << "\n";
	std::cout << "\n";
	std::cout << "  Artefacts:\n";
	std::cout << "    " << result.summary_tsv_path << "\n";
	std::cout << "    " << result.ranked_tsv_path  << "\n";
	std::cout << "    " << result.report_md_path   << "\n";
	std::cout << "\n";

	return 0;
}
