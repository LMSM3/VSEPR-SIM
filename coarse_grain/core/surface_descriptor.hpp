#pragma once
/**
 * surface_descriptor.hpp — Anisotropic Surface Descriptor for CG Beads
 *
 * Stores the spherical harmonic expansion coefficients that encode
 * the directional interaction profile of a coarse-grained bead.
 *
 *   S_B(θ, φ) = Σ c_ℓm · Y_ℓm(θ, φ)
 *
 * This descriptor captures shape anisotropy (planar, elongated, etc.)
 * and directional interaction strength from the parent atom group.
 *
 * Anti-black-box: all coefficients, metrics, and derived quantities
 * are explicitly stored and inspectable.
 *
 * Reference: Equations (5)-(7) of section_anisotropic_beads.tex
 */

#include "coarse_grain/core/spherical_harmonics.hpp"
#include "coarse_grain/core/inertia_frame.hpp"
#include <array>
#include <cmath>

namespace coarse_grain {

/**
 * SurfaceDescriptor — spherical harmonic encoding of bead surface.
 *
 * All 25 coefficients (ℓ=0..4, m=-ℓ..ℓ) are stored.
 * The descriptor is defined in the bead's local principal-axis frame.
 */
struct SurfaceDescriptor {
    /// SH expansion coefficients c_{ℓm} indexed by sh_index(ℓ, m)
    std::array<double, SH_NUM_COEFFS> coeffs{};

    /// Probe radius used during sampling (Å)
    double probe_radius{};

    /// Number of sample points used to compute the expansion
    int n_samples{};

    /// Local frame in which the descriptor is defined
    InertiaFrame frame{};

    // ========================================================================
    // Evaluation
    // ========================================================================

    /**
     * Evaluate the surface descriptor at direction (θ, φ) in the local frame.
     */
    double evaluate(double theta, double phi) const {
        return evaluate_sh_expansion(coeffs, theta, phi);
    }

    // ========================================================================
    // Anisotropy Metrics
    // ========================================================================

    /**
     * Total power of the descriptor (L2 norm of coefficients).
     */
    double total_power() const {
        return sh_power(coeffs);
    }

    /**
     * Power in each ℓ band.
     */
    std::array<double, SH_L_MAX + 1> band_power() const {
        return sh_band_power(coeffs);
    }

    /**
     * Fraction of power in ℓ ≥ 1 bands (0 = isotropic, ~1 = strongly anisotropic).
     */
    double anisotropy_ratio() const {
        return sh_anisotropy_ratio(coeffs);
    }

    /**
     * Asphericity from the inertia frame (convenience accessor).
     */
    double asphericity() const {
        return frame.asphericity;
    }

    /**
     * Dominant ℓ band (the one with highest power, excluding ℓ=0).
     * Returns -1 if all coefficients are zero.
     */
    int dominant_band() const {
        auto bp = band_power();
        int best_l = -1;
        double best_p = 0.0;
        for (int l = 1; l <= SH_L_MAX; ++l) {
            if (bp[l] > best_p) {
                best_p = bp[l];
                best_l = l;
            }
        }
        return best_l;
    }

    /**
     * Isotropic component: c_{0,0} / √(4π).
     * This is the average value of S(θ,φ) over the sphere.
     */
    double isotropic_component() const {
        constexpr double inv_sqrt_4pi = 1.0 / std::sqrt(4.0 * 3.14159265358979323846);
        return coeffs[0] * inv_sqrt_4pi;
    }
};

} // namespace coarse_grain
