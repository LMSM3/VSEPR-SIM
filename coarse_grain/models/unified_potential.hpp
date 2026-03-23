#pragma once
/**
 * unified_potential.hpp — Unified Energy Evaluator
 *
 * Single energy evaluation function for all resolution levels:
 *
 *   U = F(r, Q_A, Q_B, {c_{ℓm}^(k)})
 *
 * The same machinery handles all systems — from simple aromatics
 * (few nonzero coefficients, one channel) to complex organometallics
 * (many coefficients, all channels active). There is no branching
 * on model tier or discrete regime.
 *
 * The energy decomposes as:
 *
 *   U = U_0(r) + Σ_k λ_k · C_k(Ω_A, Ω_B) · K_k(r)
 *
 * where:
 *   U_0(r) = isotropic LJ baseline
 *   C_k    = SH coefficient dot product for channel k
 *   K_k(r) = radial kernel for channel k
 *   λ_k    = coupling weight for channel k
 *
 * For inactive channels, the coupling is identically zero (no branching,
 * no special cases — zero coefficients produce zero contribution).
 *
 * Anti-black-box: per-channel decomposition is always available.
 *
 * Reference: "Unified Descriptor Strategy" section of
 *            section_anisotropic_beads.tex
 */

#include "coarse_grain/core/unified_descriptor.hpp"
#include "coarse_grain/core/spherical_harmonics.hpp"
#include "coarse_grain/models/multi_channel_potential.hpp"
#include "atomistic/core/state.hpp"
#include <algorithm>
#include <cmath>

namespace coarse_grain {

// ============================================================================
// Unified Potential Parameters
// ============================================================================

/**
 * UnifiedPotentialParams — parameters for the unified interaction model.
 *
 * Identical structure to MultiChannelPotentialParams, because the
 * energy machinery is the same — only the descriptor resolution differs.
 */
struct UnifiedPotentialParams {
    double sigma{};       // LJ sigma (Å)
    double epsilon{};     // LJ epsilon (kcal/mol)

    // Per-channel kernels (same structure as multi-channel model)
    ChannelKernel steric_kernel       {0.10, 6.0};   // Shape-driven, short-range
    ChannelKernel electrostatic_kernel{0.05, 3.0};   // Charge-driven, longer-range
    ChannelKernel dispersion_kernel   {0.08, 6.0};   // Dispersion, same range as LJ
};

// ============================================================================
// Unified Potential Result
// ============================================================================

/**
 * UnifiedPotentialResult — decomposed energy output with resolution metadata.
 */
struct UnifiedPotentialResult {
    double E_isotropic{};         // Standard LJ 12-6 contribution
    double E_steric{};            // Steric channel anisotropic correction
    double E_electrostatic{};     // Electrostatic channel correction
    double E_dispersion{};        // Dispersion channel correction
    double E_anisotropic_total{}; // Sum of all channel corrections
    double E_total{};             // E_isotropic + E_anisotropic_total

    // Per-channel coupling coefficients (SH dot products)
    double coupling_steric{};
    double coupling_electrostatic{};
    double coupling_dispersion{};

    // Resolution metadata (inspectable, anti-black-box)
    ResolutionLevel resolution_A{ResolutionLevel::ISOTROPIC};
    ResolutionLevel resolution_B{ResolutionLevel::ISOTROPIC};
    int active_channels_A{};
    int active_channels_B{};
};

// ============================================================================
// Channel Coupling (Unified)
// ============================================================================

/**
 * Compute SH coupling between two unified channels.
 *
 * If either channel is inactive, coupling is zero.
 * Uses SH orthogonality: dot product of coefficient vectors.
 */
inline double unified_channel_coupling(const UnifiedChannel& A,
                                        const UnifiedChannel& B)
{
    if (!A.active || !B.active) return 0.0;

    double dot = 0.0;
    int n = static_cast<int>(std::min(A.coeffs.size(), B.coeffs.size()));
    for (int i = 0; i < n; ++i)
        dot += A.coeffs[i] * B.coeffs[i];
    return dot;
}

// ============================================================================
// Unified Potential Evaluation
// ============================================================================

/**
 * Compute bead-bead interaction energy using the unified formalism.
 *
 * No branching on model tier. The same function handles:
 *   - Isotropic beads (all channels inactive → pure LJ)
 *   - Axial/moderate beads (one channel, low ℓ_max)
 *   - Enriched beads (all channels, high ℓ_max)
 *
 * Inactive channels contribute zero energy automatically.
 *
 * @param r_vec   Separation vector from bead A to bead B (Å)
 * @param desc_A  Unified descriptor of bead A
 * @param desc_B  Unified descriptor of bead B
 * @param params  Interaction parameters
 */
inline UnifiedPotentialResult unified_potential(
    const atomistic::Vec3& r_vec,
    const UnifiedDescriptor& desc_A,
    const UnifiedDescriptor& desc_B,
    const UnifiedPotentialParams& params)
{
    UnifiedPotentialResult result;

    // Resolution metadata
    result.resolution_A = desc_A.resolution_level();
    result.resolution_B = desc_B.resolution_level();
    result.active_channels_A = desc_A.num_active_channels();
    result.active_channels_B = desc_B.num_active_channels();

    // Distance
    double r2 = r_vec.x * r_vec.x + r_vec.y * r_vec.y + r_vec.z * r_vec.z;
    if (r2 < 1e-10) r2 = 1e-10;
    double r = std::sqrt(r2);

    // Isotropic LJ 12-6 (always present)
    double sr = params.sigma / r;
    double sr6 = sr * sr * sr * sr * sr * sr;
    double sr12 = sr6 * sr6;
    result.E_isotropic = 4.0 * params.epsilon * (sr12 - sr6);

    // Steric channel coupling
    result.coupling_steric = unified_channel_coupling(desc_A.steric, desc_B.steric);
    double K_steric = radial_kernel(r, params.sigma, params.steric_kernel.exponent);
    result.E_steric = params.steric_kernel.lambda * result.coupling_steric * K_steric * params.epsilon;

    // Electrostatic channel coupling
    result.coupling_electrostatic = unified_channel_coupling(desc_A.electrostatic, desc_B.electrostatic);
    double K_elec = radial_kernel(r, params.sigma, params.electrostatic_kernel.exponent);
    result.E_electrostatic = params.electrostatic_kernel.lambda * result.coupling_electrostatic * K_elec * params.epsilon;

    // Dispersion channel coupling
    result.coupling_dispersion = unified_channel_coupling(desc_A.dispersion, desc_B.dispersion);
    double K_disp = radial_kernel(r, params.sigma, params.dispersion_kernel.exponent);
    result.E_dispersion = params.dispersion_kernel.lambda * result.coupling_dispersion * K_disp * params.epsilon;

    // Totals
    result.E_anisotropic_total = result.E_steric + result.E_electrostatic + result.E_dispersion;
    result.E_total = result.E_isotropic + result.E_anisotropic_total;

    return result;
}

/**
 * Pure isotropic fallback (no descriptor data).
 */
inline double unified_isotropic_lj(double r, double sigma, double epsilon) {
    if (r < 1e-10) r = 1e-10;
    double sr = sigma / r;
    double sr6 = sr * sr * sr * sr * sr * sr;
    return 4.0 * epsilon * (sr6 * sr6 - sr6);
}

} // namespace coarse_grain
