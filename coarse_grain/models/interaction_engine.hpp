#pragma once
/**
 * interaction_engine.hpp — Harmonic-Space Pairwise Interaction Engine
 *
 * Implements the Anisotropic Bead Model specification §4:
 *
 *   U_AB = Σ_k I_AB^(k)(r, Q_A, Q_B)
 *
 * where each per-channel interaction is computed as:
 *
 *   I_AB^(k) = Σ_{ℓ,m} c_{ℓm}^(k,A) · c̃_{ℓm}^(k,B) · K_k(ℓ, r)
 *
 * with c̃ denoting coefficients of B rotated into A's local frame,
 * and K_k(ℓ, r) the per-channel, per-ℓ radial kernel.
 *
 * This engine replaces the earlier unified_potential.hpp for cases
 * where per-ℓ kernel physics and orientation-dependent rotation are
 * required.  The earlier dot-product model remains valid as a fast
 * approximation when both beads share a common frame.
 *
 * Anti-black-box: per-channel, per-ℓ energy contributions are all
 * individually reported.
 *
 * Reference: "Anisotropic Bead Model — Implementation Specification"
 *            section of section_anisotropic_beads.tex
 */

#include "coarse_grain/core/channel_kernels.hpp"
#include "coarse_grain/core/sh_rotation.hpp"
#include "coarse_grain/core/unified_descriptor.hpp"
#include "coarse_grain/core/spherical_harmonics.hpp"
#include "atomistic/core/state.hpp"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace coarse_grain {

// ============================================================================
// Interaction Parameters
// ============================================================================

/**
 * InteractionParams — full parameter set for the interaction engine.
 */
struct InteractionParams {
    ChannelKernelParams kernel_params{};

    /// Per-channel coupling weights (dimensionless)
    double lambda_steric{1.0};
    double lambda_electrostatic{1.0};
    double lambda_dispersion{1.0};
};

// ============================================================================
// Per-Channel Interaction Result
// ============================================================================

/**
 * ChannelInteractionResult — decomposed per-channel energy output.
 */
struct ChannelInteractionResult {
    Channel channel{Channel::Steric};
    double energy{};
    int l_max_used{};

    /// Per-ℓ energy decomposition (indexed by ℓ)
    std::vector<double> per_l_energy;
};

// ============================================================================
// Full Interaction Result
// ============================================================================

/**
 * InteractionResult — complete decomposed energy output.
 */
struct InteractionResult {
    double E_total{};

    ChannelInteractionResult steric;
    ChannelInteractionResult electrostatic;
    ChannelInteractionResult dispersion;

    double separation{};   // Bead-bead distance (Å)
    bool frames_valid{};   // Whether both frames were valid for rotation
};

// ============================================================================
// Per-Channel Interaction
// ============================================================================

/**
 * Compute the interaction energy for a single channel between two beads.
 *
 * If either channel is inactive, returns zero.
 * Rotated coefficients of B are provided by the caller.
 *
 * @param A         Channel A coefficients
 * @param B_rot     Channel B coefficients, already rotated into A's frame
 * @param ch        Channel type
 * @param r         Bead-bead separation (Å)
 * @param params    Kernel parameters
 * @return ChannelInteractionResult with per-ℓ decomposition
 */
inline ChannelInteractionResult channel_interaction(
    const UnifiedChannel& A,
    const std::vector<double>& B_rot,
    Channel ch,
    double r,
    const ChannelKernelParams& params = {})
{
    ChannelInteractionResult result;
    result.channel = ch;

    if (!A.active || B_rot.empty()) return result;

    int l_max = A.l_max;
    result.l_max_used = l_max;
    result.per_l_energy.resize(l_max + 1, 0.0);

    int n_a = static_cast<int>(A.coeffs.size());
    int n_b = static_cast<int>(B_rot.size());

    for (int l = 0; l <= l_max; ++l) {
        double K = channel_kernel(ch, l, r, params);
        double lm_sum = 0.0;

        for (int m = -l; m <= l; ++m) {
            int idx = sh_index(l, m);
            if (idx < n_a && idx < n_b) {
                lm_sum += A.coeffs[idx] * B_rot[idx];
            }
        }

        double E_l = lm_sum * K;
        result.per_l_energy[l] = E_l;
        result.energy += E_l;
    }

    return result;
}

// ============================================================================
// Full Interaction Energy
// ============================================================================

/**
 * Compute the full pairwise interaction energy between two beads.
 *
 * Steps:
 *   1. Compute separation vector and distance
 *   2. Compute relative rotation R = Q_A^T · Q_B
 *   3. Rotate B's coefficients into A's frame (per channel)
 *   4. Sum per-(ℓ,m) contributions with per-channel kernels
 *
 * @param desc_A   Unified descriptor of bead A
 * @param desc_B   Unified descriptor of bead B
 * @param r_vec    Separation vector from A to B (Å)
 * @param params   Interaction parameters
 * @return InteractionResult with full decomposition
 */
inline InteractionResult interaction_energy(
    const UnifiedDescriptor& desc_A,
    const UnifiedDescriptor& desc_B,
    const atomistic::Vec3& r_vec,
    const InteractionParams& params = {})
{
    InteractionResult result;

    // Distance
    double r2 = r_vec.x * r_vec.x + r_vec.y * r_vec.y + r_vec.z * r_vec.z;
    if (r2 < 1e-20) r2 = 1e-20;
    result.separation = std::sqrt(r2);

    // Relative rotation
    result.frames_valid = desc_A.frame.valid && desc_B.frame.valid;

    Mat3 R;
    if (result.frames_valid) {
        R = compute_relative_rotation(desc_A.frame, desc_B.frame);
    } else {
        // Identity rotation (no frame → no rotation)
        R(0,0) = 1.0; R(1,1) = 1.0; R(2,2) = 1.0;
    }

    // Rotate B's coefficients into A's frame, then compute per-channel interaction

    // Steric
    if (desc_A.steric.active && desc_B.steric.active) {
        int l_max_b = desc_B.steric.l_max;
        auto B_rot = rotate_sh_coefficients(desc_B.steric.coeffs, l_max_b, R);
        result.steric = channel_interaction(
            desc_A.steric, B_rot, Channel::Steric,
            result.separation, params.kernel_params);
        result.steric.energy *= params.lambda_steric;
    }

    // Electrostatic
    if (desc_A.electrostatic.active && desc_B.electrostatic.active) {
        int l_max_b = desc_B.electrostatic.l_max;
        auto B_rot = rotate_sh_coefficients(desc_B.electrostatic.coeffs, l_max_b, R);
        result.electrostatic = channel_interaction(
            desc_A.electrostatic, B_rot, Channel::Electrostatic,
            result.separation, params.kernel_params);
        result.electrostatic.energy *= params.lambda_electrostatic;
    }

    // Dispersion
    if (desc_A.dispersion.active && desc_B.dispersion.active) {
        int l_max_b = desc_B.dispersion.l_max;
        auto B_rot = rotate_sh_coefficients(desc_B.dispersion.coeffs, l_max_b, R);
        result.dispersion = channel_interaction(
            desc_A.dispersion, B_rot, Channel::Dispersion,
            result.separation, params.kernel_params);
        result.dispersion.energy *= params.lambda_dispersion;
    }

    result.E_total = result.steric.energy
                   + result.electrostatic.energy
                   + result.dispersion.energy;

    return result;
}

} // namespace coarse_grain
