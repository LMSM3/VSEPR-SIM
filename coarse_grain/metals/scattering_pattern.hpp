#pragma once
/**
 * scattering_pattern.hpp — Deterministic X-ray Scattering Pattern Generator
 *
 * Computes SAXS/WAXS-like scattering intensity profiles directly from bead
 * positions using the Debye scattering equation. NO Monte Carlo sampling.
 *
 * The Debye formula:
 *
 *   I(q) = Σ_i Σ_j f_i(q) · f_j(q) · sin(q·r_ij) / (q·r_ij)
 *
 * where:
 *   q = 4π·sin(θ)/λ   — scattering vector magnitude (Å⁻¹)
 *   r_ij = |r_i - r_j| — pair distance
 *   f_i(q) = Z_i · exp(-B_i · q²/(16π²)) — atomic form factor (Gaussian approx)
 *
 * This produces the concentric ring patterns seen in the SAXS/WAXS images:
 *   - Sharp Bragg peaks → crystalline order (silicon-like)
 *   - Broad rings → polycrystalline (welded Cu, stainless steel)
 *   - Diffuse halo → amorphous (wood, carbon fibre)
 *
 * For a cluster of N beads, the full Debye sum is O(N²) — acceptable for
 * our cluster sizes (64–512 beads, ~4000 pairs max for 64).
 *
 * Application contexts:
 *   - Mesocrystal analysis (internal symmetry verification)
 *   - Defect detection (missing Bragg peaks)
 *   - Phase identification (peak positions → d-spacings)
 *   - Radiation damage assessment (peak broadening)
 *
 * References:
 *   - Debye, Ann. Phys. 351, 809 (1915)
 *   - Warren, X-ray Diffraction (1969)
 *   - International Tables for Crystallography, Vol. C
 */

#include "coarse_grain/core/bead.hpp"
#include "coarse_grain/core/bead_system.hpp"
#include "coarse_grain/metals/metal_registry.hpp"
#include "atomistic/core/state.hpp"
#include <cmath>
#include <string>
#include <vector>

namespace coarse_grain {
namespace metals {

// ============================================================================
// Scattering profile data
// ============================================================================

struct ScatteringPoint {
    double q{};                 ///< Scattering vector (Å⁻¹)
    double two_theta_deg{};     ///< 2θ (degrees) at reference λ
    double intensity{};         ///< I(q) — Debye sum result
    double d_spacing_ang{};     ///< d = 2π/q (Å)
};

struct ScatteringProfile {
    std::string material;
    uint32_t    n_beads{};
    double      lambda_ang{};       ///< X-ray wavelength (Å)
    uint32_t    n_q_bins{};
    double      q_min{};
    double      q_max{};
    std::vector<ScatteringPoint> points;

    // Derived diagnostics
    double peak_q{};                ///< q at maximum intensity
    double peak_d_spacing{};        ///< d-spacing at peak (Å)
    double peak_intensity{};        ///< I(q_peak)
    double crystallinity_index{};   ///< peak_I / mean_I — sharpness indicator
    bool   has_bragg_peaks{};       ///< Peak/mean > 5 → crystalline
};

// ============================================================================
// Gaussian form factor
// ============================================================================

/**
 * Simplified atomic form factor (Gaussian approximation).
 *
 *   f(q) = Z · exp(-B · q² / (16π²))
 *
 * B = Debye–Waller factor (Å²). Typical metallic value: 0.5–1.0 Å².
 * For a deterministic model, we use B from Debye temperature:
 *   B ≈ 3ℏ²/(m·k_B·Θ_D) ≈ 8π²·⟨u²⟩
 * Simplified: B_proxy = 150.0 / Θ_D (approximate scaling)
 */
inline double form_factor(uint32_t Z, double q, double B_factor) {
    constexpr double INV_16PI2 = 1.0 / (16.0 * M_PI * M_PI);
    return static_cast<double>(Z) * std::exp(-B_factor * q * q * INV_16PI2);
}

inline double estimate_B_factor(double debye_temp_K) {
    if (debye_temp_K <= 0) return 1.0;
    return 150.0 / debye_temp_K; // Å², approximate
}

// ============================================================================
// Debye scattering equation
// ============================================================================

/**
 * Compute the full Debye scattering profile from bead positions.
 *
 * I(q) = Σ_i f_i² + 2·Σ_{i<j} f_i·f_j·sin(q·r_ij)/(q·r_ij)
 *
 * @param system     BeadSystem with bead positions
 * @param metal      MetalRecord (Z, Θ_D for form factor)
 * @param n_q_bins   Number of q bins (default 256)
 * @param q_min      Minimum q (Å⁻¹, default 0.1)
 * @param q_max      Maximum q (Å⁻¹, default 8.0)
 * @param lambda_ang X-ray wavelength (Å, default 1.5406 = Cu Kα)
 */
inline ScatteringProfile compute_debye_pattern(
    const BeadSystem&  system,
    const MetalRecord& metal,
    uint32_t           n_q_bins  = 256,
    double             q_min     = 0.1,
    double             q_max     = 8.0,
    double             lambda_ang = 1.5406)
{
    ScatteringProfile prof;
    prof.material   = metal.symbol;
    prof.n_beads    = static_cast<uint32_t>(system.beads.size());
    prof.lambda_ang = lambda_ang;
    prof.n_q_bins   = n_q_bins;
    prof.q_min      = q_min;
    prof.q_max      = q_max;

    const uint32_t N = prof.n_beads;
    if (N == 0) return prof;

    double B = estimate_B_factor(metal.debye_temperature_K);
    double dq = (q_max - q_min) / std::max(n_q_bins - 1u, 1u);

    // Precompute pair distances (O(N²) but N is small)
    struct PairDist { double r; uint32_t i; uint32_t j; };
    std::vector<PairDist> pairs;
    pairs.reserve(N * (N - 1) / 2);

    for (uint32_t i = 0; i < N; ++i) {
        for (uint32_t j = i + 1; j < N; ++j) {
            double dx = system.beads[j].position.x - system.beads[i].position.x;
            double dy = system.beads[j].position.y - system.beads[i].position.y;
            double dz = system.beads[j].position.z - system.beads[i].position.z;
            double r = std::sqrt(dx*dx + dy*dy + dz*dz);
            if (r > 1.0e-6)
                pairs.push_back({r, i, j});
        }
    }

    // Compute I(q) for each bin
    prof.points.resize(n_q_bins);
    double sum_I = 0.0;
    double max_I = 0.0;
    uint32_t max_idx = 0;

    for (uint32_t k = 0; k < n_q_bins; ++k) {
        double q = q_min + k * dq;
        double f = form_factor(metal.Z, q, B);
        double f2 = f * f;

        // Self-scattering: N · f²
        double I = N * f2;

        // Cross-terms: 2 · Σ_{i<j} f_i·f_j · sin(qr)/(qr)
        for (const auto& pd : pairs) {
            double qr = q * pd.r;
            double sinc = (qr > 1.0e-8) ? std::sin(qr) / qr : 1.0;
            I += 2.0 * f2 * sinc;  // all same element → f_i = f_j
        }

        double two_theta = 2.0 * std::asin(q * lambda_ang / (4.0 * M_PI));
        double d_spacing = (q > 1.0e-8) ? 2.0 * M_PI / q : 0.0;

        prof.points[k] = {q, two_theta * 180.0 / M_PI, I, d_spacing};
        sum_I += I;
        if (I > max_I) { max_I = I; max_idx = k; }
    }

    // Derived diagnostics
    double mean_I = sum_I / std::max(n_q_bins, 1u);
    prof.peak_q          = prof.points[max_idx].q;
    prof.peak_d_spacing  = prof.points[max_idx].d_spacing_ang;
    prof.peak_intensity  = max_I;
    prof.crystallinity_index = (mean_I > 0) ? max_I / mean_I : 0.0;
    prof.has_bragg_peaks = (prof.crystallinity_index > 5.0);

    return prof;
}

// ============================================================================
// CSV export
// ============================================================================

inline std::string scattering_to_csv(const ScatteringProfile& prof) {
    std::string csv = "q_inv_ang,two_theta_deg,intensity,d_spacing_ang\n";
    for (const auto& p : prof.points) {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "%.6f,%.4f,%.6e,%.4f\n",
                      p.q, p.two_theta_deg, p.intensity, p.d_spacing_ang);
        csv += buf;
    }
    return csv;
}

// ============================================================================
// Terminal summary
// ============================================================================

inline std::string scattering_summary_terminal(const ScatteringProfile& prof) {
    const char* BOLD = "\033[1m"; const char* CYAN = "\033[36m";
    const char* GREEN = "\033[32m"; const char* RESET = "\033[0m";

    char buf[512];
    std::snprintf(buf, sizeof(buf),
        "%s\n  Scattering Profile: %s  (N=%u, λ=%.4f Å)%s\n"
        "%s  Peak q = %.4f Å⁻¹  →  d = %.3f Å  (2θ = %.2f°)\n"
        "  Crystallinity index = %.2f  →  %s%s%s\n"
        "  q range: [%.2f, %.2f] Å⁻¹  (%u bins)\n%s\n",
        BOLD, prof.material.c_str(), prof.n_beads, prof.lambda_ang, RESET,
        CYAN, prof.peak_q, prof.peak_d_spacing,
        prof.points.empty() ? 0.0 : prof.points[0].two_theta_deg,
        prof.crystallinity_index,
        GREEN, prof.has_bragg_peaks ? "CRYSTALLINE (Bragg)" : "POLYCRYSTALLINE/AMORPHOUS",
        RESET,
        prof.q_min, prof.q_max, prof.n_q_bins, RESET);
    return buf;
}

} // namespace metals
} // namespace coarse_grain
