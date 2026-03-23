#pragma once
/**
 * channel_kernels.hpp — Per-ℓ Channel Radial Kernels
 *
 * Implements the physically motivated, per-angular-momentum radial
 * kernels for each interaction channel as specified in the Anisotropic
 * Bead Model Implementation Specification (§4).
 *
 * Three kernel definitions:
 *
 *   Steric:        K(ℓ, r) = exp(-α_s · r) / (1 + ℓ)
 *   Electrostatic: K(ℓ, r) = 1 / r^(ℓ + 1)
 *   Dispersion:    K(ℓ, r) = -C₆ / r^(6 + ℓ)
 *
 * These kernels are evaluated per (ℓ, r) pair during the harmonic-space
 * interaction sum. They replace the single-exponent radial_kernel()
 * used in earlier revisions.
 *
 * Anti-black-box: all parameters (α_s, C₆) are stored in a single
 * inspectable struct. No hidden constants.
 *
 * Reference: "Anisotropic Bead Model — Implementation Specification"
 *            section of section_anisotropic_beads.tex
 */

#include <cmath>
#include <cstdint>

namespace coarse_grain {

// ============================================================================
// Channel Enum (canonical, spec §1)
// ============================================================================

/**
 * Channel — physical interaction channel index.
 *
 * Fixed at three entries for this revision.  Expansion prior to
 * validating all three is explicitly out of scope.
 */
enum class Channel : uint8_t {
    Steric        = 0,
    Electrostatic = 1,
    Dispersion    = 2,
    COUNT         = 3
};

inline const char* channel_name(Channel ch) {
    switch (ch) {
        case Channel::Steric:        return "Steric";
        case Channel::Electrostatic: return "Electrostatic";
        case Channel::Dispersion:    return "Dispersion";
        default:                     return "Unknown";
    }
}

// ============================================================================
// Kernel Parameters
// ============================================================================

/**
 * ChannelKernelParams — inspectable parameters for the per-ℓ kernels.
 *
 * All values are explicit; no hidden state.
 */
struct ChannelKernelParams {
    double alpha_s{1.0};   // Steric decay constant (1/Å)
    double C6{1.0};        // Dispersion coefficient (kcal/mol · Å⁶)
};

// ============================================================================
// Per-ℓ Kernel Evaluation
// ============================================================================

/**
 * Evaluate the per-channel, per-ℓ radial kernel K_k(ℓ, r).
 *
 *   Steric:        exp(-α_s · r) / (1 + ℓ)
 *   Electrostatic: 1 / r^(ℓ + 1)
 *   Dispersion:    -C₆ / r^(6 + ℓ)
 *
 * @param ch      Channel type
 * @param l       Angular momentum order (ℓ ≥ 0)
 * @param r       Bead-bead separation (Å, must be > 0)
 * @param params  Kernel parameters
 * @return Kernel value K_k(ℓ, r)
 */
inline double channel_kernel(Channel ch, int l, double r,
                              const ChannelKernelParams& params = {})
{
    if (r < 1e-10) r = 1e-10;

    switch (ch) {
        case Channel::Steric:
            return std::exp(-params.alpha_s * r) / static_cast<double>(1 + l);

        case Channel::Electrostatic:
            return 1.0 / std::pow(r, static_cast<double>(l + 1));

        case Channel::Dispersion:
            return -params.C6 / std::pow(r, static_cast<double>(6 + l));

        default:
            return 0.0;
    }
}

/**
 * Evaluate a single channel kernel with explicit parameter values.
 * Convenience overload for quick evaluation without constructing params.
 */
inline double channel_kernel(Channel ch, int l, double r,
                              double alpha_s, double C6)
{
    ChannelKernelParams p{alpha_s, C6};
    return channel_kernel(ch, l, r, p);
}

} // namespace coarse_grain
