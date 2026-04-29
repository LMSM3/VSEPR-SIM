/**
 * gas3_engine.hpp
 * ---------------
 * Gas3 Module — Top-Level API and CLI Dispatch.
 *
 * Gas3 is the higher-order analysis and reporting layer:
 *   - Random and adaptive sampling
 *   - Curve fitting and residual analysis
 *   - HTML dashboards and CSV exports
 *   - Quality-scored dataset generation
 *   - Publication-grade outputs
 *
 * Gas2 remains the disciplined property engine.
 * Gas3 is the analyst drawing surfaces and making them publishable.
 *
 * Composes:
 *   - gas3_state_record.hpp   (GasStateRecord, quality tiers)
 *   - gas3_quality.hpp        (scoring, penalty catalogue)
 *   - gas3_sweep.hpp          (linear, random, adaptive sweeps)
 *   - gas3_fitting.hpp        (polynomial regression)
 *   - gas3_export.hpp         (CSV, HTML, manifest)
 *
 * CLI dispatch: vsepr gas3 <subcommand>
 */

#pragma once

#include "gas3_state_record.hpp"
#include "gas3_quality.hpp"
#include "gas3_sweep.hpp"
#include "gas3_fitting.hpp"
#include "gas3_export.hpp"

#include <string>
#include <cstdint>

namespace vsepr {
namespace gas3 {

// ============================================================================
// Run type enumeration
// ============================================================================

enum class RunType {
    QuickCheck,
    LinearSweep,
    RandomSweep,
    AdaptiveSweep,
    FitBuild,
    ReportBuild,
    FullPipeline
};

// ============================================================================
// Pipeline configuration
// ============================================================================

struct PipelineConfig {
    RunType run_type = RunType::FullPipeline;
    SweepConfig sweep;
    std::string output_dir = "output/gas_runs";
    std::string run_name;         // auto-generated if empty
    bool csv_output    = true;
    bool html_output   = true;
    bool json_manifest = true;
    bool verbose       = false;
    int  fit_degree_1d = 3;
    int  fit_degree_2d = 2;
};

// ============================================================================
// CLI dispatch
// ============================================================================

int gas3_dispatch(int argc, char** argv);

} // namespace gas3
} // namespace vsepr
