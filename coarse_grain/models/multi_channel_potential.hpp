#pragma once
/**
 * multi_channel_potential.hpp — Channel-Coupled Bead-Bead Interaction
 *
 * Extends the single-channel anisotropic potential to a multi-channel
 * formulation where each physical channel (steric, electrostatic,
 * dispersion) contributes independently via its own radial kernel:
 *
 *   U ≈ ∫ Σ_k S_A^(k)(θ,φ) · S_B^(k)(θ',φ') · K_k(r) dΩ
 *
 * In practice, the surface coupling integral is evaluated as a dot
 * product of SH coefficient vectors per channel, modulated by
 * channel-specific radial kernels K_k(r) and coupling weights.
 *
 * Anti-black-box: per-channel energy contributions are individually
 * reported, enabling decomposition and diagnosis.
 *
 * Reference: "Descriptor Enrichment for Complex Anisotropic Structures"
 *            subsection of section_anisotropic_beads.tex
 */

#include "coarse_grain/core/multi_channel_descriptor.hpp"
#include "coarse_grain/core/spherical_harmonics.hpp"
#include "atomistic/core/state.hpp"
#include <algorithm>
#include <cmath>

namespace coarse_grain {

/**
 * ChannelKernel — per-channel radial kernel and coupling weight.
 *
 * Each channel has its own:
 *   - lambda:  coupling strength (dimensionless)
 *   - exponent: radial decay exponent for K_k(r) = (σ/r)^exponent
 */
struct ChannelKernel {
    double lambda{};      // Coupling strength
    double exponent{6.0}; // Radial decay exponent (default: r^-6)
};

/**
 * MultiChannelPotentialParams — parameters for multi-channel interaction.
 */
struct MultiChannelPotentialParams {
    double sigma{};       // LJ sigma (Å)
    double epsilon{};     // LJ epsilon (kcal/mol)

    // Per-channel kernels
    ChannelKernel steric_kernel      {0.10, 6.0};   // Shape-driven, short-range
    ChannelKernel electrostatic_kernel{0.05, 3.0};   // Charge-driven, longer-range
    ChannelKernel dispersion_kernel  {0.08, 6.0};   // Dispersion, same range as LJ
};

/**
 * MultiChannelPotentialResult — decomposed energy output.
 */
struct MultiChannelPotentialResult {
    double E_isotropic{};         // Standard LJ 12-6 contribution
    double E_steric{};            // Steric channel anisotropic correction
    double E_electrostatic{};     // Electrostatic channel correction
    double E_dispersion{};        // Dispersion channel correction
    double E_anisotropic_total{}; // Sum of all channel corrections
    double E_total{};             // E_isotropic + E_anisotropic_total

    // Per-channel coupling coefficients
    double coupling_steric{};
    double coupling_electrostatic{};
    double coupling_dispersion{};
};

/**
 * Compute the SH coupling coefficient for a single channel.
 *
 * Uses SH orthogonality: ∫ Y_ℓm · Y_ℓ'm' dΩ = δ_{ℓℓ'}δ_{mm'}
 * So coupling reduces to the dot product of coefficient vectors,
 * limited to the minimum length of the two coefficient arrays.
 */
inline double channel_coupling(const ChannelDescriptor& A,
                                const ChannelDescriptor& B)
{
    double dot = 0.0;
    int n = static_cast<int>(std::min(A.coeffs.size(), B.coeffs.size()));
    for (int i = 0; i < n; ++i)
        dot += A.coeffs[i] * B.coeffs[i];
    return dot;
}

/**
 * Evaluate radial kernel K_k(r) = (σ/r)^exponent.
 */
inline double radial_kernel(double r, double sigma, double exponent) {
    if (r < 1e-10) r = 1e-10;
    double sr = sigma / r;
    return std::pow(sr, exponent);
}

/**
 * Compute multi-channel bead-bead interaction energy.
 *
 * The model:
 *   U(r, Ω_A, Ω_B) = U_LJ(r; σ, ε) + Σ_k λ_k · C_k(Ω_A, Ω_B) · K_k(r) · ε
 *
 * Each channel contributes independently with its own coupling coefficient,
 * radial kernel, and coupling weight.
 *
 * @param r_vec   Separation vector from bead A to bead B (Å)
 * @param desc_A  Multi-channel descriptor of bead A
 * @param desc_B  Multi-channel descriptor of bead B
 * @param params  Interaction parameters
 */
inline MultiChannelPotentialResult multi_channel_potential(
    const atomistic::Vec3& r_vec,
    const MultiChannelDescriptor& desc_A,
    const MultiChannelDescriptor& desc_B,
    const MultiChannelPotentialParams& params)
{
    MultiChannelPotentialResult result;

    double r2 = r_vec.x * r_vec.x + r_vec.y * r_vec.y + r_vec.z * r_vec.z;
    if (r2 < 1e-10) r2 = 1e-10;
    double r = std::sqrt(r2);

    // Isotropic LJ 12-6
    double sr = params.sigma / r;
    double sr6 = sr * sr * sr * sr * sr * sr;
    double sr12 = sr6 * sr6;
    result.E_isotropic = 4.0 * params.epsilon * (sr12 - sr6);

    // Steric channel
    result.coupling_steric = channel_coupling(desc_A.steric, desc_B.steric);
    double K_steric = radial_kernel(r, params.sigma, params.steric_kernel.exponent);
    result.E_steric = params.steric_kernel.lambda * result.coupling_steric * K_steric * params.epsilon;

    // Electrostatic channel
    result.coupling_electrostatic = channel_coupling(desc_A.electrostatic, desc_B.electrostatic);
    double K_elec = radial_kernel(r, params.sigma, params.electrostatic_kernel.exponent);
    result.E_electrostatic = params.electrostatic_kernel.lambda * result.coupling_electrostatic * K_elec * params.epsilon;

    // Dispersion channel
    result.coupling_dispersion = channel_coupling(desc_A.dispersion, desc_B.dispersion);
    double K_disp = radial_kernel(r, params.sigma, params.dispersion_kernel.exponent);
    result.E_dispersion = params.dispersion_kernel.lambda * result.coupling_dispersion * K_disp * params.epsilon;

    // Totals
    result.E_anisotropic_total = result.E_steric + result.E_electrostatic + result.E_dispersion;
    result.E_total = result.E_isotropic + result.E_anisotropic_total;

    return result;
}

/**
 * Pure isotropic LJ 12-6 energy (fallback for beads without multi-channel data).
 */
inline double multi_channel_isotropic_lj(double r, double sigma, double epsilon) {
    if (r < 1e-10) r = 1e-10;
    double sr = sigma / r;
    double sr6 = sr * sr * sr * sr * sr * sr;
    return 4.0 * epsilon * (sr6 * sr6 - sr6);
}

} // namespace coarse_grain
