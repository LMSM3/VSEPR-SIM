#pragma once
// =============================================================================
// src/batch/manifest_runner.hpp  -  WO-B9-001  Batch Manifest Runner
// =============================================================================
//
// Sweep-expansion engine and artifact writers.
// BatchRunnerConfig controls verbosity and abort behaviour.
// BatchRunnerResult accumulates per-run records and aggregate stats.
//
// SimulateRunFn allows injection of a custom run kernel (default: deterministic
// stub in default_stub_simulator).
//
// WO-B9-001  |  VSEPR-SIM beta-9
// =============================================================================

#include "include/vsim/vsim_document.hpp"
#include <functional>
#include <string>
#include <vector>

namespace vsim {
namespace batch {

// ---------------------------------------------------------------------------
// BatchRunnerConfig  -  runtime options for run_manifest()
// ---------------------------------------------------------------------------

struct BatchRunnerConfig {
    bool verbose       = true;
    bool abort_on_fail = false;
    int  max_steps     = 500;
};

// ---------------------------------------------------------------------------
// BatchRunnerResult  -  aggregate output of run_manifest()
// ---------------------------------------------------------------------------

struct BatchRunnerResult {
    std::string batch_id;
    std::string batch_dir;

    std::string summary_tsv_path;
    std::string ranked_tsv_path;
    std::string report_md_path;

    int n_total     = 0;
    int n_converged = 0;
    int n_failed    = 0;

    double best_energy   = 0.0;
    double mean_energy   = 0.0;
    double wall_ms_total = 0.0;

    std::vector<BatchRunRecord> records;

    bool ok() const { return n_total > 0; }
};

// ---------------------------------------------------------------------------
// SimulateRunFn  -  injectable per-run kernel
// ---------------------------------------------------------------------------

using SimulateRunFn = std::function<BatchRunRecord(const BatchRunRecord& proto,
                                                   const std::string&    run_dir)>;

// ---------------------------------------------------------------------------
// default_stub_simulator  -  deterministic seed-driven stub
// ---------------------------------------------------------------------------

BatchRunRecord default_stub_simulator(const BatchRunRecord& proto,
                                      const std::string&    run_dir);

// ---------------------------------------------------------------------------
// write_batch_outputs  -  public artefact writer (TSV / MD)
// ---------------------------------------------------------------------------

bool write_batch_outputs(BatchRunnerResult& result);

// ---------------------------------------------------------------------------
// run_manifest  -  entry point
// ---------------------------------------------------------------------------

BatchRunnerResult run_manifest(const BatchManifestSection& manifest,
                               const BatchRunnerConfig&    cfg      = {},
                               SimulateRunFn               simulate = nullptr);

} // namespace batch
} // namespace vsim
