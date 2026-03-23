#pragma once
/**
 * descriptor_residual.hpp — Residual-Driven Resolution Promotion
 *
 * Provides the mechanism for continuous, residual-driven promotion
 * of descriptor resolution. Instead of hard if-statement thresholds
 * (atom count, metal center), promotion is triggered by measuring
 * the reconstruction error of the current truncation level against
 * a reference surface.
 *
 * The reconstruction residual:
 *
 *   ε(ℓ_max) = Σ_i |S_ref(n̂_i) - S_{ℓ_max}(n̂_i)|² / Σ_i |S_ref(n̂_i)|²
 *
 * provides a quantitative basis for deciding when to add:
 *   - higher ℓ_max (more angular resolution)
 *   - additional channels (more physical modes)
 *
 * Complexity is added continuously in response to representational
 * failure, not through fixed structural cutoffs.
 *
 * Anti-black-box: residual values, promotion decisions, and
 * truncation history are all explicitly inspectable.
 *
 * Reference: "Unified Descriptor Strategy" section of
 *            section_anisotropic_beads.tex
 */

#include "coarse_grain/core/unified_descriptor.hpp"
#include "coarse_grain/core/spherical_harmonics.hpp"
#include <cmath>
#include <vector>

namespace coarse_grain {

// ============================================================================
// Residual Configuration
// ============================================================================

/**
 * ResidualConfig — thresholds for residual-driven promotion.
 *
 * All parameters are explicit and inspectable.
 */
struct ResidualConfig {
    /// Residual tolerance: promote if ε > this value (default: 5%)
    double residual_tolerance = 0.05;

    /// Maximum ℓ_max to promote to (prevent unbounded growth)
    int max_l_max = 8;

    /// Step size for ℓ_max promotion
    int l_max_step = 2;

    /// Enable multi-channel activation when residual is high
    bool allow_channel_activation = true;

    /// Residual threshold above which additional channels are activated
    double channel_activation_threshold = 0.15;
};

// ============================================================================
// Residual Result
// ============================================================================

/**
 * ResidualResult — outcome of a residual evaluation.
 *
 * Records the residual value, whether promotion is recommended,
 * and what the recommended new resolution would be.
 */
struct ResidualResult {
    /// Current ℓ_max of the channel being evaluated
    int current_l_max{};

    /// Measured residual ε(ℓ_max) ∈ [0, 1]
    double residual{};

    /// Whether the residual exceeds the tolerance
    bool needs_promotion{false};

    /// Recommended new ℓ_max if promotion is needed
    int recommended_l_max{};

    /// Whether additional channels should be activated
    bool recommend_channel_activation{false};
};

// ============================================================================
// Residual Computation
// ============================================================================

/**
 * Compute the reconstruction residual for a single channel.
 *
 * Given a set of reference surface values S_ref(n̂_i) sampled at
 * specific directions, and the current SH expansion coefficients,
 * compute:
 *
 *   ε = Σ |S_ref(n̂_i) - S_trunc(n̂_i)|² / Σ |S_ref(n̂_i)|²
 *
 * where S_trunc is the truncated SH reconstruction.
 *
 * @param reference_values   Probe-sampled surface values at sample directions
 * @param sample_theta       θ coordinates of sample points
 * @param sample_phi         φ coordinates of sample points
 * @param channel            The channel to evaluate (uses its current l_max)
 * @return Residual in [0, 1] (0 = perfect reconstruction)
 */
inline double compute_channel_residual(
    const std::vector<double>& reference_values,
    const std::vector<double>& sample_theta,
    const std::vector<double>& sample_phi,
    const UnifiedChannel& channel)
{
    if (!channel.active || reference_values.empty()) return 1.0;

    int n = static_cast<int>(reference_values.size());
    double sum_error2 = 0.0;
    double sum_ref2 = 0.0;

    for (int i = 0; i < n; ++i) {
        double ref = reference_values[i];
        double trunc = channel.evaluate(sample_theta[i], sample_phi[i]);
        double diff = ref - trunc;
        sum_error2 += diff * diff;
        sum_ref2 += ref * ref;
    }

    if (sum_ref2 < 1e-20) return 0.0;  // Zero reference → trivially exact
    return sum_error2 / sum_ref2;
}

/**
 * Evaluate whether a channel needs promotion and recommend action.
 *
 * @param reference_values   Probe-sampled surface values
 * @param sample_theta       θ coordinates
 * @param sample_phi         φ coordinates
 * @param channel            The channel to evaluate
 * @param config             Promotion configuration
 * @return ResidualResult with recommendation
 */
inline ResidualResult evaluate_channel_residual(
    const std::vector<double>& reference_values,
    const std::vector<double>& sample_theta,
    const std::vector<double>& sample_phi,
    const UnifiedChannel& channel,
    const ResidualConfig& config = {})
{
    ResidualResult result;
    result.current_l_max = channel.l_max;
    result.residual = compute_channel_residual(
        reference_values, sample_theta, sample_phi, channel);

    result.needs_promotion = (result.residual > config.residual_tolerance)
                          && (channel.l_max < config.max_l_max);

    if (result.needs_promotion) {
        result.recommended_l_max = std::min(
            channel.l_max + config.l_max_step, config.max_l_max);
    } else {
        result.recommended_l_max = channel.l_max;
    }

    result.recommend_channel_activation =
        config.allow_channel_activation &&
        result.residual > config.channel_activation_threshold;

    return result;
}

// ============================================================================
// Descriptor-Level Residual
// ============================================================================

/**
 * DescriptorResidualResult — residual analysis for the entire unified descriptor.
 */
struct DescriptorResidualResult {
    ResidualResult steric_result;
    ResidualResult electrostatic_result;
    ResidualResult dispersion_result;

    /// Whether any channel needs promotion
    bool any_needs_promotion() const {
        return steric_result.needs_promotion
            || electrostatic_result.needs_promotion
            || dispersion_result.needs_promotion;
    }

    /// Maximum residual across all evaluated channels
    double max_residual() const {
        return std::max({steric_result.residual,
                         electrostatic_result.residual,
                         dispersion_result.residual});
    }

    /// Maximum recommended ℓ_max across all channels
    int max_recommended_l_max() const {
        return std::max({steric_result.recommended_l_max,
                         electrostatic_result.recommended_l_max,
                         dispersion_result.recommended_l_max});
    }
};

/**
 * Apply promotion recommendations to a unified descriptor.
 *
 * Updates channel ℓ_max values and residuals based on the
 * residual analysis. Does NOT refit coefficients (that requires
 * re-probing the atomistic structure).
 *
 * @param descriptor  The descriptor to modify
 * @param result      Residual analysis from evaluate_channel_residual
 */
inline void apply_promotion(UnifiedDescriptor& descriptor,
                             const DescriptorResidualResult& result)
{
    if (result.steric_result.needs_promotion) {
        descriptor.promote_channel(
            DescriptorChannel::STERIC,
            result.steric_result.recommended_l_max);
        descriptor.steric.residual = result.steric_result.residual;
    }

    if (result.electrostatic_result.needs_promotion) {
        descriptor.promote_channel(
            DescriptorChannel::ELECTROSTATIC,
            result.electrostatic_result.recommended_l_max);
        descriptor.electrostatic.residual = result.electrostatic_result.residual;
    }

    if (result.dispersion_result.needs_promotion) {
        descriptor.promote_channel(
            DescriptorChannel::DISPERSION,
            result.dispersion_result.recommended_l_max);
        descriptor.dispersion.residual = result.dispersion_result.residual;
    }

    // Activate channels if recommended
    if (result.steric_result.recommend_channel_activation && !descriptor.steric.active) {
        descriptor.activate_channel(DescriptorChannel::STERIC,
                                     result.steric_result.recommended_l_max);
    }
    if (result.electrostatic_result.recommend_channel_activation && !descriptor.electrostatic.active) {
        descriptor.activate_channel(DescriptorChannel::ELECTROSTATIC,
                                     result.electrostatic_result.recommended_l_max);
    }
    if (result.dispersion_result.recommend_channel_activation && !descriptor.dispersion.active) {
        descriptor.activate_channel(DescriptorChannel::DISPERSION,
                                     result.dispersion_result.recommended_l_max);
    }
}

} // namespace coarse_grain
