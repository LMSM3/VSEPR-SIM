#pragma once
/**
 * adaptive_refinement.hpp — Bead-Level Adaptive Resolution
 *
 * Implements the Anisotropic Bead Model specification §5:
 *
 *   For each channel k:
 *     ε_B^(k) = ‖S_ref - S̃‖² / ‖S_ref‖²
 *     if ε_B^(k) > ε_tol:
 *       ℓ_max^(k) += Δℓ
 *       reproject channel k
 *
 * This wraps the existing descriptor_residual.hpp infrastructure
 * into the bead-level adaptation pattern specified in the formal
 * model document.
 *
 * Anti-black-box: all residuals, promotion decisions, and ℓ_max
 * changes are recorded in the AdaptationResult struct.
 *
 * Reference: "Anisotropic Bead Model — Implementation Specification"
 *            section of section_anisotropic_beads.tex
 */

#include "coarse_grain/core/unified_descriptor.hpp"
#include "coarse_grain/models/descriptor_residual.hpp"
#include <cmath>
#include <vector>

namespace coarse_grain {

// ============================================================================
// Adaptation Configuration
// ============================================================================

/**
 * AdaptConfig — parameters controlling the adapt_bead loop.
 */
struct AdaptConfig {
    double error_threshold{0.05};   // ε_tol: promote if residual exceeds this
    int max_l_max{8};               // Maximum ℓ_max (prevent unbounded growth)
    int l_max_step{2};              // Δℓ per promotion step
};

// ============================================================================
// Per-Channel Adaptation Record
// ============================================================================

/**
 * ChannelAdaptRecord — records what happened to a single channel.
 */
struct ChannelAdaptRecord {
    int l_max_before{};
    int l_max_after{};
    double residual{};
    bool promoted{false};
};

// ============================================================================
// Full Adaptation Result
// ============================================================================

/**
 * AdaptationResult — records the outcome of adapt_bead().
 */
struct AdaptationResult {
    ChannelAdaptRecord steric;
    ChannelAdaptRecord electrostatic;
    ChannelAdaptRecord dispersion;

    bool any_promoted() const {
        return steric.promoted || electrostatic.promoted || dispersion.promoted;
    }

    double max_residual() const {
        return std::max({steric.residual, electrostatic.residual, dispersion.residual});
    }
};

// ============================================================================
// Adapt Bead
// ============================================================================

/**
 * Evaluate reconstruction error for each active channel and promote
 * ℓ_max if the error exceeds the threshold.
 *
 * This function modifies the descriptor in-place. After promotion,
 * the new coefficients at higher ℓ are initialised to zero. The
 * caller must reproject (re-sample and re-fit) to populate them.
 *
 * @param descriptor         Unified descriptor to adapt (modified in-place)
 * @param reference_steric   Reference surface values for steric channel
 * @param reference_elec     Reference surface values for electrostatic channel
 * @param reference_disp     Reference surface values for dispersion channel
 * @param sample_theta       θ coordinates of sample points
 * @param sample_phi         φ coordinates of sample points
 * @param config             Adaptation configuration
 * @return AdaptationResult with per-channel records
 */
inline AdaptationResult adapt_bead(
    UnifiedDescriptor& descriptor,
    const std::vector<double>& reference_steric,
    const std::vector<double>& reference_elec,
    const std::vector<double>& reference_disp,
    const std::vector<double>& sample_theta,
    const std::vector<double>& sample_phi,
    const AdaptConfig& config = {})
{
    AdaptationResult result;

    // Helper lambda for per-channel adaptation
    auto adapt_channel = [&](UnifiedChannel& ch,
                             const std::vector<double>& reference,
                             ChannelAdaptRecord& record)
    {
        record.l_max_before = ch.l_max;
        record.l_max_after = ch.l_max;
        record.promoted = false;

        if (!ch.active || reference.empty()) {
            record.residual = ch.active ? 1.0 : 0.0;
            return;
        }

        // Compute reconstruction error
        record.residual = compute_channel_residual(
            reference, sample_theta, sample_phi, ch);

        ch.residual = record.residual;

        // Promote if error exceeds threshold and we haven't hit the cap
        if (record.residual > config.error_threshold && ch.l_max < config.max_l_max) {
            int new_l_max = std::min(ch.l_max + config.l_max_step, config.max_l_max);
            ch.promote(new_l_max);
            record.l_max_after = new_l_max;
            record.promoted = true;
        }
    };

    adapt_channel(descriptor.steric, reference_steric, result.steric);
    adapt_channel(descriptor.electrostatic, reference_elec, result.electrostatic);
    adapt_channel(descriptor.dispersion, reference_disp, result.dispersion);

    return result;
}

/**
 * Simplified adapt_bead for single-channel (steric-only) scenarios.
 */
inline AdaptationResult adapt_bead_steric(
    UnifiedDescriptor& descriptor,
    const std::vector<double>& reference_steric,
    const std::vector<double>& sample_theta,
    const std::vector<double>& sample_phi,
    const AdaptConfig& config = {})
{
    std::vector<double> empty;
    return adapt_bead(descriptor, reference_steric, empty, empty,
                      sample_theta, sample_phi, config);
}

} // namespace coarse_grain
