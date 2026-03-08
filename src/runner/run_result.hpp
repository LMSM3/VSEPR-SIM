#pragma once
/**
 * run_result.hpp
 * --------------
 * Structured output from a single molecule generation run.
 * Every result carries full provenance (seed, preset, version, timings).
 *
 * Guiding principle: Every state is reproducible. Every result is traceable.
 */

#include "sim/molecule.hpp"
#include "identity/canonical_identity.hpp"
#include "validation/validation_gates.hpp"
#include <string>
#include <chrono>
#include <map>
#include <cstdint>

namespace vsepr {
namespace runner {

// ============================================================================
// Provenance: Everything needed to reproduce this run
// ============================================================================

struct Provenance {
    // Generator
    uint32_t seed = 0;
    std::string engine_version = "2.5.0";
    std::string builder_version = "formula_builder_v1";

    // Preset
    std::string preset_name;
    std::string preset_id;         // Unique preset identifier

    // Solver
    std::string optimizer = "FIRE";
    uint32_t max_iterations = 0;
    double force_tolerance = 0.0;
    bool vsepr_only = true;

    // Timing
    double build_time_ms = 0.0;
    double optimize_time_ms = 0.0;
    double validate_time_ms = 0.0;
    double total_time_ms = 0.0;

    // Iteration counts
    uint32_t fire_iterations = 0;
    uint32_t clash_iterations = 0;
};

// ============================================================================
// Run Result: Complete output from one generation attempt
// ============================================================================

enum class RunStatus : uint8_t {
    SUCCESS,
    VALIDATION_FAILED,
    OPTIMIZATION_FAILED,
    TIMEOUT,
    BUILD_ERROR,
    DUPLICATE,
};

inline const char* status_name(RunStatus s) {
    switch (s) {
        case RunStatus::SUCCESS:             return "SUCCESS";
        case RunStatus::VALIDATION_FAILED:   return "VALIDATION_FAILED";
        case RunStatus::OPTIMIZATION_FAILED: return "OPTIMIZATION_FAILED";
        case RunStatus::TIMEOUT:             return "TIMEOUT";
        case RunStatus::BUILD_ERROR:         return "BUILD_ERROR";
        case RunStatus::DUPLICATE:           return "DUPLICATE";
        default: return "UNKNOWN";
    }
}

struct RunResult {
    // Status
    RunStatus status = RunStatus::BUILD_ERROR;
    std::string error_message;

    // Molecule (valid only if status == SUCCESS)
    Molecule molecule;

    // Identity
    identity::MolecularIdentity identity;

    // Validation
    validation::ValidationReport validation;

    // Provenance
    Provenance provenance;

    // Scoring (filled by evaluation harness)
    double convergence_score = 0.0;   // 0.0 = didn't converge, 1.0 = tight convergence
    double stability_score = 0.0;     // How stable is this minimum?
    double geometry_score = 0.0;      // How reasonable are bond lengths/angles?
    double composite_score = 0.0;     // Weighted combination

    bool succeeded() const { return status == RunStatus::SUCCESS; }
};

// ============================================================================
// Preset: Named parameter set for molecule generation
// ============================================================================

struct Preset {
    std::string name;           // "fast", "standard", "full", "production"
    std::string id;             // Unique hash for this preset config

    // Builder params
    uint32_t seed_start = 0;
    uint32_t seed_count = 1;    // Number of seeds to try
    bool use_spherical_3d = false;

    // Optimizer params
    uint32_t max_fire_iterations = 1000;
    double force_tolerance = 1e-3;
    bool vsepr_only = true;
    double timeout_seconds = 30.0;

    // Quality params
    double min_composite_score = 0.0;   // Reject below this
    bool skip_validation = false;

    // Presets
    static Preset fast() {
        Preset p;
        p.name = "fast";
        p.id = "fast_v1";
        p.seed_count = 1;
        p.max_fire_iterations = 200;
        p.force_tolerance = 1e-2;
        p.vsepr_only = true;
        p.timeout_seconds = 5.0;
        return p;
    }

    static Preset standard() {
        Preset p;
        p.name = "standard";
        p.id = "standard_v1";
        p.seed_count = 4;
        p.max_fire_iterations = 1000;
        p.force_tolerance = 1e-3;
        p.use_spherical_3d = true;
        p.timeout_seconds = 30.0;
        return p;
    }

    static Preset full() {
        Preset p;
        p.name = "full";
        p.id = "full_v1";
        p.seed_count = 12;
        p.max_fire_iterations = 5000;
        p.force_tolerance = 1e-4;
        p.use_spherical_3d = true;
        p.vsepr_only = false;
        p.timeout_seconds = 120.0;
        return p;
    }

    static Preset production() {
        Preset p;
        p.name = "production";
        p.id = "production_v1";
        p.seed_count = 16;
        p.max_fire_iterations = 10000;
        p.force_tolerance = 1e-5;
        p.use_spherical_3d = true;
        p.vsepr_only = false;
        p.timeout_seconds = 300.0;
        p.min_composite_score = 0.5;
        return p;
    }
};

// ============================================================================
// Aggregate Result: Summary across multiple seeds
// ============================================================================

struct AggregateResult {
    std::string formula;
    std::string preset_name;
    uint32_t total_attempts = 0;
    uint32_t successes = 0;
    uint32_t failures = 0;
    uint32_t duplicates = 0;
    uint32_t timeouts = 0;

    // Best result
    RunResult best;
    double best_score = -1.0;

    // Statistics across successful runs
    double mean_score = 0.0;
    double variance_score = 0.0;
    double mean_iterations = 0.0;
    double mean_time_ms = 0.0;

    // All results (for detailed analysis)
    std::vector<RunResult> all_results;

    double success_rate() const {
        return total_attempts > 0 ? double(successes) / total_attempts : 0.0;
    }

    std::string summary() const {
        std::ostringstream oss;
        oss << formula << " [" << preset_name << "]: "
            << successes << "/" << total_attempts << " succeeded ("
            << std::fixed << std::setprecision(0) << (success_rate() * 100) << "%)";
        if (successes > 0) {
            oss << " best=" << std::setprecision(3) << best_score
                << " mean=" << mean_score;
        }
        return oss.str();
    }
};

} // namespace runner
} // namespace vsepr
