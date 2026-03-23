#pragma once
/**
 * spherical_harmonics.hpp — Real Spherical Harmonics
 *
 * Provides evaluation of real spherical harmonics Y_ℓ^m(θ, φ) used
 * to compress anisotropic surface descriptors on coarse-grained beads.
 *
 * Implementation:
 *   - Associated Legendre polynomials via stable upward recurrence.
 *   - Real form: cosine for m>0, sine for m<0, normalized.
 *   - Fixed API: ℓ_max = 4 → 25 basis functions (backward compatible).
 *   - Dynamic API: runtime-configurable ℓ_max → (ℓ_max+1)² functions.
 *
 * Anti-black-box: every coefficient index is deterministically mapped
 * via sh_index(ℓ, m), and all formulas are explicit in code.
 *
 * Reference: Equations (6) and (7) of section_anisotropic_beads.tex
 */

#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <vector>

namespace coarse_grain {

/// Maximum spherical harmonic order supported (compile-time fixed API).
inline constexpr int SH_L_MAX = 4;

/// Total number of (ℓ, m) pairs for ℓ = 0 .. L_MAX: (L_MAX+1)^2
inline constexpr int SH_NUM_COEFFS = (SH_L_MAX + 1) * (SH_L_MAX + 1);  // 25

/**
 * Flat index for (ℓ, m) pair.
 *   ℓ ∈ [0, ℓ_max],  m ∈ [-ℓ, ℓ]
 *   index = ℓ² + ℓ + m
 */
inline constexpr int sh_index(int l, int m) {
    return l * l + l + m;
}

/**
 * Total number of SH coefficients for a given ℓ_max.
 */
inline constexpr int sh_num_coeffs(int l_max) {
    return (l_max + 1) * (l_max + 1);
}

/**
 * Evaluate all 25 real spherical harmonics at direction (θ, φ).
 *
 * θ = polar angle   [0, π]
 * φ = azimuthal angle [0, 2π]
 *
 * Uses the real convention:
 *   Y_ℓ^m  = √2 · K_ℓ^m · P_ℓ^m(cos θ) · cos(m φ)    for m > 0
 *   Y_ℓ^0  = K_ℓ^0 · P_ℓ^0(cos θ)
 *   Y_ℓ^m  = √2 · K_ℓ^|m| · P_ℓ^|m|(cos θ) · sin(|m| φ)  for m < 0
 *
 * where K_ℓ^m = √((2ℓ+1)/(4π) · (ℓ-m)!/(ℓ+m)!)
 */
inline std::array<double, SH_NUM_COEFFS> evaluate_all_harmonics(double theta, double phi) {
    std::array<double, SH_NUM_COEFFS> Y{};

    const double ct = std::cos(theta);
    const double st = std::sin(theta);

    // Associated Legendre polynomials P_ℓ^m(cos θ), stored as [ℓ][m] with m≥0.
    // We compute up to ℓ=4, m=4.
    double P[SH_L_MAX + 1][SH_L_MAX + 1]{};

    // Seed values
    P[0][0] = 1.0;

    // Sectoral: P_m^m = -(2m-1) · sin(θ) · P_{m-1}^{m-1}
    for (int m = 1; m <= SH_L_MAX; ++m) {
        P[m][m] = -(2.0 * m - 1.0) * st * P[m - 1][m - 1];
    }

    // One step above sectoral: P_{m+1}^m = (2m+1) · cos(θ) · P_m^m
    for (int m = 0; m < SH_L_MAX; ++m) {
        P[m + 1][m] = (2.0 * m + 1.0) * ct * P[m][m];
    }

    // General recurrence: (ℓ-m)·P_ℓ^m = (2ℓ-1)·cos(θ)·P_{ℓ-1}^m - (ℓ+m-1)·P_{ℓ-2}^m
    for (int m = 0; m <= SH_L_MAX; ++m) {
        for (int l = m + 2; l <= SH_L_MAX; ++l) {
            P[l][m] = ((2.0 * l - 1.0) * ct * P[l - 1][m]
                       - (l + m - 1.0) * P[l - 2][m])
                      / static_cast<double>(l - m);
        }
    }

    // Normalization constant K_ℓ^m
    // K_ℓ^m = √((2ℓ+1)/(4π) · (ℓ-m)!/(ℓ+m)!)
    auto factorial_ratio = [](int l, int m) -> double {
        // Compute (ℓ-m)! / (ℓ+m)! iteratively to avoid overflow
        double ratio = 1.0;
        for (int k = l - m + 1; k <= l + m; ++k)
            ratio /= static_cast<double>(k);
        return ratio;
    };

    constexpr double inv_4pi = 1.0 / (4.0 * 3.14159265358979323846);
    constexpr double sqrt2 = 1.41421356237309504880;

    for (int l = 0; l <= SH_L_MAX; ++l) {
        // m = 0
        double K0 = std::sqrt((2.0 * l + 1.0) * inv_4pi);
        Y[sh_index(l, 0)] = K0 * P[l][0];

        // m > 0 and m < 0
        for (int m = 1; m <= l; ++m) {
            double Km = std::sqrt((2.0 * l + 1.0) * inv_4pi * factorial_ratio(l, m));

            Y[sh_index(l,  m)] = sqrt2 * Km * P[l][m] * std::cos(m * phi);
            Y[sh_index(l, -m)] = sqrt2 * Km * P[l][m] * std::sin(m * phi);
        }
    }

    return Y;
}

/**
 * Evaluate a surface descriptor from its SH coefficients at (θ, φ).
 *
 *   S(θ, φ) = Σ c_ℓm · Y_ℓm(θ, φ)
 */
inline double evaluate_sh_expansion(const std::array<double, SH_NUM_COEFFS>& coeffs,
                                     double theta, double phi) {
    auto Y = evaluate_all_harmonics(theta, phi);
    double result = 0.0;
    for (int i = 0; i < SH_NUM_COEFFS; ++i)
        result += coeffs[i] * Y[i];
    return result;
}

/**
 * Compute the L2 norm (power) of the SH expansion.
 * By Parseval's theorem: ||S||² = Σ c_ℓm²
 */
inline double sh_power(const std::array<double, SH_NUM_COEFFS>& coeffs) {
    double sum = 0.0;
    for (int i = 0; i < SH_NUM_COEFFS; ++i)
        sum += coeffs[i] * coeffs[i];
    return sum;
}

/**
 * Compute the power per ℓ band.
 * Returns array indexed by ℓ = 0..SH_L_MAX.
 */
inline std::array<double, SH_L_MAX + 1> sh_band_power(
    const std::array<double, SH_NUM_COEFFS>& coeffs)
{
    std::array<double, SH_L_MAX + 1> band{};
    for (int l = 0; l <= SH_L_MAX; ++l) {
        for (int m = -l; m <= l; ++m) {
            double c = coeffs[sh_index(l, m)];
            band[l] += c * c;
        }
    }
    return band;
}

/**
 * Anisotropy ratio: fraction of total power in ℓ ≥ 1 bands.
 *
 * Returns 0.0 for perfectly isotropic beads, approaches 1.0 for
 * strongly anisotropic beads.
 */
inline double sh_anisotropy_ratio(const std::array<double, SH_NUM_COEFFS>& coeffs) {
    double total = sh_power(coeffs);
    if (total < 1e-30) return 0.0;
    double iso = coeffs[0] * coeffs[0];  // ℓ=0 component
    return 1.0 - iso / total;
}

// ============================================================================
// Dynamic API — Runtime-configurable ℓ_max
// ============================================================================
// These functions mirror the fixed-size API above but use std::vector
// for coefficient storage, allowing arbitrary ℓ_max at runtime.
// ============================================================================

/**
 * Evaluate all (l_max+1)² real spherical harmonics at direction (θ, φ)
 * for a runtime-specified l_max.
 *
 * θ = polar angle   [0, π]
 * φ = azimuthal angle [0, 2π]
 */
inline std::vector<double> evaluate_all_harmonics_dynamic(double theta, double phi, int l_max) {
    const int n_coeffs = sh_num_coeffs(l_max);
    std::vector<double> Y(n_coeffs, 0.0);

    const double ct = std::cos(theta);
    const double st = std::sin(theta);

    // Associated Legendre polynomials P_ℓ^m(cos θ)
    // Dynamically sized: [ℓ][m] with m ≥ 0
    std::vector<std::vector<double>> P(l_max + 1, std::vector<double>(l_max + 1, 0.0));

    // Seed
    P[0][0] = 1.0;

    // Sectoral: P_m^m = -(2m-1) · sin(θ) · P_{m-1}^{m-1}
    for (int m = 1; m <= l_max; ++m) {
        P[m][m] = -(2.0 * m - 1.0) * st * P[m - 1][m - 1];
    }

    // One step above sectoral: P_{m+1}^m = (2m+1) · cos(θ) · P_m^m
    for (int m = 0; m < l_max; ++m) {
        P[m + 1][m] = (2.0 * m + 1.0) * ct * P[m][m];
    }

    // General recurrence
    for (int m = 0; m <= l_max; ++m) {
        for (int l = m + 2; l <= l_max; ++l) {
            P[l][m] = ((2.0 * l - 1.0) * ct * P[l - 1][m]
                       - (l + m - 1.0) * P[l - 2][m])
                      / static_cast<double>(l - m);
        }
    }

    // Normalization
    auto factorial_ratio = [](int l, int m) -> double {
        double ratio = 1.0;
        for (int k = l - m + 1; k <= l + m; ++k)
            ratio /= static_cast<double>(k);
        return ratio;
    };

    constexpr double inv_4pi = 1.0 / (4.0 * 3.14159265358979323846);
    constexpr double sqrt2 = 1.41421356237309504880;

    for (int l = 0; l <= l_max; ++l) {
        // m = 0
        double K0 = std::sqrt((2.0 * l + 1.0) * inv_4pi);
        Y[sh_index(l, 0)] = K0 * P[l][0];

        // m > 0 and m < 0
        for (int m = 1; m <= l; ++m) {
            double Km = std::sqrt((2.0 * l + 1.0) * inv_4pi * factorial_ratio(l, m));
            Y[sh_index(l,  m)] = sqrt2 * Km * P[l][m] * std::cos(m * phi);
            Y[sh_index(l, -m)] = sqrt2 * Km * P[l][m] * std::sin(m * phi);
        }
    }

    return Y;
}

/**
 * Evaluate a surface descriptor from dynamic SH coefficients at (θ, φ).
 */
inline double evaluate_sh_expansion_dynamic(const std::vector<double>& coeffs,
                                             double theta, double phi, int l_max) {
    auto Y = evaluate_all_harmonics_dynamic(theta, phi, l_max);
    double result = 0.0;
    int n = static_cast<int>(std::min(coeffs.size(), Y.size()));
    for (int i = 0; i < n; ++i)
        result += coeffs[i] * Y[i];
    return result;
}

/**
 * Compute the L2 norm (power) of a dynamic SH expansion.
 */
inline double sh_power_dynamic(const std::vector<double>& coeffs) {
    double sum = 0.0;
    for (double c : coeffs)
        sum += c * c;
    return sum;
}

/**
 * Compute the power per ℓ band for a dynamic SH expansion.
 */
inline std::vector<double> sh_band_power_dynamic(const std::vector<double>& coeffs, int l_max) {
    std::vector<double> band(l_max + 1, 0.0);
    for (int l = 0; l <= l_max; ++l) {
        for (int m = -l; m <= l; ++m) {
            int idx = sh_index(l, m);
            if (idx < static_cast<int>(coeffs.size())) {
                band[l] += coeffs[idx] * coeffs[idx];
            }
        }
    }
    return band;
}

/**
 * Anisotropy ratio for a dynamic SH expansion.
 */
inline double sh_anisotropy_ratio_dynamic(const std::vector<double>& coeffs) {
    double total = sh_power_dynamic(coeffs);
    if (total < 1e-30) return 0.0;
    double iso = (coeffs.empty() ? 0.0 : coeffs[0] * coeffs[0]);
    return 1.0 - iso / total;
}

} // namespace coarse_grain
