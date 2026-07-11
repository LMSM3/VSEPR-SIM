#pragma once
/**
 [VSEPR-SIM 4.0.4.04]
 * ═══════════════════════════════════════════════
 * Separates "case finished" from "case is good."
 *
 * From section_v4_transition.tex Eq. (3.1):
 *
 *   Q_data = a₁·C_conv + a₂·C_energy + a₃·C_stability
 *          + a₄·C_consistency + a₅·C_completeness − a₆·P_failure
 *
 * Default weights: {0.25, 0.20, 0.15, 0.15, 0.15, 0.10}
 *
 * All components ∈ [0,1].  Final Q_data ∈ [0,1] (clamped).
 *
 * C++26 features:
 *   - Contract emulation
 *   - Erroneous-behaviour trapping
 *   - Trailing return types
 *
 * Anti-black-box: every component exposed and inspectable.
 */

#include "formation_record.hpp"

#include <algorithm>
#include <cmath>

namespace v4 {

// ============================================================================
// Data Quality Components
// ============================================================================

struct DataQualityComponents {
    double C_conv{0.0};          // true convergence reward
    double C_energy{0.0};        // physically sensible energy
    double C_stability{0.0};     // low residual force / oscillation
    double C_consistency{0.0};   // agreement across derived fields
    double C_completeness{0.0};  // full descriptor population
    double P_failure{0.0};       // penalty for step-limit/zero/unstable
};

// ============================================================================
// Data Quality Weights
// ============================================================================

struct DataQualityWeights {
    double a1{0.25};  // convergence
    double a2{0.20};  // energy
    double a3{0.15};  // stability
    double a4{0.15};  // consistency
    double a5{0.15};  // completeness
    double a6{0.10};  // failure penalty

    auto valid() const -> bool {
        double s = a1 + a2 + a3 + a4 + a5;
        // a6 is subtracted, so positive weights should sum > a6
        return s > a6 && (s + a6) > 0.0;
    }
};

inline constexpr DataQualityWeights DEFAULT_DQ_WEIGHTS{};

// ============================================================================
// Component Computations
// ============================================================================

/**
 * C_conv: Convergence Reward
 *
 * Binary 1.0 if converged, reduced by how close to step limit.
 * A case that converges in 10% of max steps scores higher than
 * one that converges at 99%.
 *
 * @param converged    whether formation converged
 * @param steps        actual steps taken
 * @param max_steps    step ceiling (6000 in reference data)
 */
inline auto compute_C_conv(bool converged, int steps, int max_steps = 6000) -> double
{
    if (!converged) return 0.0;
    if (max_steps <= 0) return 1.0;
    // Early convergence is better: score = 1 - (steps/max)^0.5
    double ratio = static_cast<double>(steps) / static_cast<double>(max_steps);
    return std::clamp(1.0 - std::sqrt(ratio), 0.0, 1.0);
}

/**
 * C_energy: Physically Sensible Energy
 *
 * Rewards negative (bound) final energy.  Score scales with magnitude.
 * Returns 0 if energy is NaN, zero, or positive.
 *
 * @param final_energy   final energy from formation
 * @param E_ref          reference energy scale (negative, e.g. -30000)
 */
inline auto compute_C_energy(double final_energy, double E_ref = -35000.0) -> double
{
    if (!std::isfinite(final_energy) || final_energy >= 0.0) return 0.0;
    if (!std::isfinite(E_ref) || E_ref >= 0.0) E_ref = -35000.0;
    // Ratio of magnitude to reference (both negative)
    double ratio = final_energy / E_ref;  // positive when both negative
    return std::clamp(ratio, 0.0, 1.0);
}

/**
 * C_stability: Low Residual Force / Low Oscillation
 *
 * Rewards low rms_force.  Score = exp(-rms_force / σ_ref).
 *
 * @param rms_force  RMS force at final step
 * @param sigma_ref  reference force scale (default 0.02)
 */
inline auto compute_C_stability(double rms_force, double sigma_ref = 0.02) -> double
{
    if (!std::isfinite(rms_force) || rms_force < 0.0) return 0.0;
    if (rms_force == 0.0) return 1.0;  // zero force = perfectly stable
    return std::exp(-rms_force / sigma_ref);
}

/**
 * C_consistency: Agreement Across Derived Fields
 *
 * Checks whether macro fields are mutually consistent:
 * - rigidity + ductility + color should sum near 1.0
 * - experimental refs should be internally ordered sensibly
 *
 * @param macro_rigi   macro rigidity precursor
 * @param macro_duc    macro ductility precursor
 * @param macro_col    macro color precursor
 */
inline auto compute_C_consistency(double macro_rigi, double macro_duc,
                                  double macro_col) -> double
{
    // If any field is NaN, no consistency check possible
    if (!std::isfinite(macro_rigi) || !std::isfinite(macro_duc)
        || !std::isfinite(macro_col))
    {
        // All zero = trivial case (Ag, Cu, Al) → partial score
        if (macro_rigi == 0.0 && macro_duc == 0.0 && macro_col == 0.0)
            return 0.3;
        return 0.0;
    }

    // Macro precursors should form a roughly normalized triplet
    double sum = macro_rigi + macro_duc + macro_col;
    if (sum < 1e-15) return 0.3;

    // Score = 1 - |sum - 1.0| (closer to 1.0 sum = better)
    double dev = std::abs(sum - 1.0);
    return std::clamp(1.0 - dev, 0.0, 1.0);
}

/**
 * C_completeness: Descriptor Population
 *
 * Fraction of formation record fields that are populated.
 * Same as S_intensity but used in quality context.
 */
inline auto compute_C_completeness(const FormationRecord& rec) -> double
{
    return static_cast<double>(rec.populated_fields())
         / static_cast<double>(FormationRecord::FIELD_COUNT);
}

/**
 * P_failure: Failure Penalty
 *
 * Penalises: step-limit hits, zero-filled fields, unstable outputs.
 * Returns a value ∈ [0,1] where 1 = maximum penalty.
 */
inline auto compute_P_failure(const FormationRecord& rec,
                              int max_steps = 6000) -> double
{
    double penalty = 0.0;

    // Step-limit hit
    if (!rec.converged && rec.steps >= max_steps)
        penalty += 0.4;

    // Zero-filled formation fields (avg_eta, avg_rho, avg_C all zero)
    if (rec.avg_eta == 0.0 && rec.avg_rho == 0.0 && rec.avg_C == 0.0)
        penalty += 0.3;

    // NaN in critical fields
    if (!std::isfinite(rec.final_energy)) penalty += 0.15;
    if (!std::isfinite(rec.rms_force))    penalty += 0.15;

    return std::clamp(penalty, 0.0, 1.0);
}

// ============================================================================
// Full Data Quality Score
// ============================================================================

struct DataQualityResult {
    DataQualityComponents components;
    DataQualityWeights    weights;
    double                Q_data{0.0};
};

/**
 * Compute the full data quality score for a formation record.
 */
inline auto compute_data_quality(
    const FormationRecord& rec,
    const DataQualityWeights& w = DEFAULT_DQ_WEIGHTS,
    int max_steps = 6000,
    double E_ref = -35000.0) -> DataQualityResult
{
    V4_CONTRACT_PRE(w.valid());

    DataQualityResult result;
    result.weights = w;

    auto& c = result.components;
    c.C_conv         = compute_C_conv(rec.converged, rec.steps, max_steps);
    c.C_energy       = compute_C_energy(rec.final_energy, E_ref);
    c.C_stability    = compute_C_stability(rec.rms_force);
    c.C_consistency  = compute_C_consistency(rec.macro_rigidity,
                                             rec.macro_ductility,
                                             rec.macro_color);
    c.C_completeness = compute_C_completeness(rec);
    c.P_failure      = compute_P_failure(rec, max_steps);

    result.Q_data = w.a1 * c.C_conv
                  + w.a2 * c.C_energy
                  + w.a3 * c.C_stability
                  + w.a4 * c.C_consistency
                  + w.a5 * c.C_completeness
                  - w.a6 * c.P_failure;

    result.Q_data = std::clamp(result.Q_data, 0.0, 1.0);

    V4_CONTRACT_POST(result.Q_data >= 0.0 && result.Q_data <= 1.0);
    return result;
}

/**
 * Convenience: compute data quality and write into record.
 */
inline auto score_data_quality(
    FormationRecord& rec,
    const DataQualityWeights& w = DEFAULT_DQ_WEIGHTS,
    int max_steps = 6000,
    double E_ref = -35000.0) -> DataQualityResult
{
    auto result = compute_data_quality(rec, w, max_steps, E_ref);
    rec.data_quality = result.Q_data;
    return result;
}

} // namespace v4
