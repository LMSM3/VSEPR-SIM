#pragma once
/**
 * preset_runner.hpp
 * -----------------
 * Executes molecule generation with a given preset.
 * Takes formula + preset, returns RunResult with full provenance.
 *
 * This is the "one function to rule them all" for batch generation.
 *
 * Pipeline per seed:
 *   parse -> build -> optimize -> validate -> score -> identity
 *
 * Aggregate mode:
 *   run N seeds -> collect results -> pick best -> compute stats
 */

#include "run_result.hpp"
#include "build/builder_core.hpp"
#include "build/formula_builder.hpp"
#include "identity/canonical_identity.hpp"
#include "validation/validation_gates.hpp"
#include "pot/periodic_db.hpp"
#include <chrono>
#include <vector>
#include <string>
#include <algorithm>
#include <functional>
#include <iostream>

namespace vsepr {
namespace runner {

// ============================================================================
// Scoring Functions
// ============================================================================

namespace scoring {

/**
 * Score convergence quality.
 * 1.0 = converged well within tolerance
 * 0.0 = didn't converge / exploded
 */
inline double convergence_score(uint32_t iterations, uint32_t max_iterations,
                                double final_force, double tolerance) {
    if (iterations == 0) return 0.0;
    if (final_force > tolerance * 100) return 0.0;  // Exploded

    // Fraction of budget used (lower is better)
    double budget_frac = double(iterations) / double(max_iterations);
    double budget_score = std::max(0.0, 1.0 - budget_frac);

    // Force convergence quality
    double force_score = (final_force < tolerance) ? 1.0 :
                         std::max(0.0, 1.0 - std::log10(final_force / tolerance) / 3.0);

    return 0.4 * budget_score + 0.6 * force_score;
}

/**
 * Score geometry quality from validation gates.
 * 1.0 = all gates passed
 * 0.0 = critical failure
 */
inline double geometry_score(const validation::ValidationReport& report) {
    if (report.results.empty()) return 0.0;
    int passed = 0;
    for (const auto& r : report.results) {
        if (r.passed) passed++;
    }
    return double(passed) / double(report.results.size());
}

/**
 * Compute composite score from components.
 */
inline double composite(double convergence, double geometry, double stability = 1.0) {
    return 0.4 * convergence + 0.4 * geometry + 0.2 * stability;
}

} // namespace scoring

// ============================================================================
// Single Run Executor
// ============================================================================

/**
 * Execute one molecule generation attempt.
 *
 * @param formula   Chemical formula (e.g., "H2O", "CH4")
 * @param preset    Generation preset
 * @param seed      Specific seed for this attempt
 * @param pt        Loaded periodic table
 * @param Z_to_sym  Element symbol lookup function
 * @return          RunResult with full provenance
 */
inline RunResult execute_single(
    const std::string& formula,
    const Preset& preset,
    uint32_t seed,
    const PeriodicTable& pt,
    const std::function<std::string(uint8_t)>& Z_to_sym
) {
    RunResult result;
    result.provenance.seed = seed;
    result.provenance.preset_name = preset.name;
    result.provenance.preset_id = preset.id;
    result.provenance.max_iterations = preset.max_fire_iterations;
    result.provenance.force_tolerance = preset.force_tolerance;
    result.provenance.vsepr_only = preset.vsepr_only;

    auto t_start = std::chrono::high_resolution_clock::now();

    // Stage 1: Build
    auto t_build_start = std::chrono::high_resolution_clock::now();
    try {
        MoleculeBuilderOptions opts;
        opts.seed = static_cast<int>(seed);
        opts.geometry_style = preset.use_spherical_3d
            ? GeometryGuessStyle::SPHERICAL_3D
            : GeometryGuessStyle::CIRCULAR_2D;
        opts.auto_generate_angles = true;
        opts.auto_generate_torsions = true;
        opts.assign_lone_pairs = true;

        result.molecule = build_from_formula(formula, pt, opts);
    } catch (const std::exception& e) {
        result.status = RunStatus::BUILD_ERROR;
        result.error_message = e.what();
        return result;
    }
    auto t_build_end = std::chrono::high_resolution_clock::now();
    result.provenance.build_time_ms = std::chrono::duration<double, std::milli>(
        t_build_end - t_build_start).count();

    // Stage 2: Optimize
    auto t_opt_start = std::chrono::high_resolution_clock::now();
    try {
        FIREOptimizer optimizer;
        OptimizerSettings opt_settings;
        opt_settings.max_iterations = preset.max_fire_iterations;
        opt_settings.force_tolerance = preset.force_tolerance;

        bool use_vsepr = preset.vsepr_only;
        EnergyModel energy(result.molecule,
                          100.0,         // k_bond
                          use_vsepr,     // use_vsepr
                          !use_vsepr,    // use_mm
                          NonbondedParams{},
                          !use_vsepr,    // use_pairwise_nb
                          false,         // use_cell_list
                          10.0);         // cutoff

        optimizer.minimize(result.molecule.coords, energy, opt_settings);
        result.provenance.fire_iterations = optimizer.iterations_used();
        result.provenance.optimizer = "FIRE";

    } catch (const std::exception& e) {
        result.status = RunStatus::OPTIMIZATION_FAILED;
        result.error_message = e.what();
        return result;
    }
    auto t_opt_end = std::chrono::high_resolution_clock::now();
    result.provenance.optimize_time_ms = std::chrono::duration<double, std::milli>(
        t_opt_end - t_opt_start).count();

    // Stage 3: Validate
    auto t_val_start = std::chrono::high_resolution_clock::now();
    result.validation = validation::validate_molecule(result.molecule);
    auto t_val_end = std::chrono::high_resolution_clock::now();
    result.provenance.validate_time_ms = std::chrono::duration<double, std::milli>(
        t_val_end - t_val_start).count();

    if (!result.validation.all_passed && !preset.skip_validation) {
        result.status = RunStatus::VALIDATION_FAILED;
        result.error_message = result.validation.summary();
        // Still compute identity and score for diagnostics
    }

    // Stage 4: Identity
    result.identity = identity::compute_identity(result.molecule, Z_to_sym);

    // Stage 5: Score
    result.convergence_score = scoring::convergence_score(
        result.provenance.fire_iterations,
        preset.max_fire_iterations,
        result.provenance.force_tolerance,  // Approximate: use tolerance as proxy
        preset.force_tolerance);
    result.geometry_score = scoring::geometry_score(result.validation);
    result.composite_score = scoring::composite(
        result.convergence_score, result.geometry_score);

    // Timing
    auto t_end = std::chrono::high_resolution_clock::now();
    result.provenance.total_time_ms = std::chrono::duration<double, std::milli>(
        t_end - t_start).count();

    if (result.validation.all_passed) {
        result.status = RunStatus::SUCCESS;
    }

    return result;
}

// ============================================================================
// Aggregate Runner (multiple seeds)
// ============================================================================

/**
 * Run a formula through N seeds and aggregate results.
 *
 * @param formula   Chemical formula
 * @param preset    Generation preset (determines N seeds, tolerances, etc.)
 * @param pt        Loaded periodic table
 * @param Z_to_sym  Element symbol lookup
 * @param verbose   Print progress
 * @return          Aggregated results with best-of-N selection
 */
inline AggregateResult run_aggregate(
    const std::string& formula,
    const Preset& preset,
    const PeriodicTable& pt,
    const std::function<std::string(uint8_t)>& Z_to_sym,
    bool verbose = false
) {
    AggregateResult agg;
    agg.formula = formula;
    agg.preset_name = preset.name;
    agg.total_attempts = preset.seed_count;

    double score_sum = 0.0;
    double score_sq_sum = 0.0;
    double iter_sum = 0.0;
    double time_sum = 0.0;

    for (uint32_t s = 0; s < preset.seed_count; ++s) {
        uint32_t seed = preset.seed_start + s;

        RunResult result = execute_single(formula, preset, seed, pt, Z_to_sym);
        
        if (verbose) {
            std::cout << "  Seed " << seed << ": " << status_name(result.status);
            if (result.succeeded()) {
                std::cout << " score=" << std::fixed << std::setprecision(3)
                          << result.composite_score
                          << " iters=" << result.provenance.fire_iterations
                          << " time=" << std::setprecision(1) << result.provenance.total_time_ms << "ms";
            }
            std::cout << "\n";
        }

        switch (result.status) {
            case RunStatus::SUCCESS:
                agg.successes++;
                score_sum += result.composite_score;
                score_sq_sum += result.composite_score * result.composite_score;
                iter_sum += result.provenance.fire_iterations;
                time_sum += result.provenance.total_time_ms;
                if (result.composite_score > agg.best_score) {
                    agg.best_score = result.composite_score;
                    agg.best = result;
                }
                break;
            case RunStatus::DUPLICATE:
                agg.duplicates++;
                break;
            case RunStatus::TIMEOUT:
                agg.timeouts++;
                break;
            default:
                agg.failures++;
                break;
        }

        agg.all_results.push_back(std::move(result));
    }

    // Compute statistics
    if (agg.successes > 0) {
        agg.mean_score = score_sum / agg.successes;
        agg.variance_score = (agg.successes > 1)
            ? (score_sq_sum - score_sum * score_sum / agg.successes) / (agg.successes - 1)
            : 0.0;
        agg.mean_iterations = iter_sum / agg.successes;
        agg.mean_time_ms = time_sum / agg.successes;
    }

    return agg;
}

} // namespace runner
} // namespace vsepr
