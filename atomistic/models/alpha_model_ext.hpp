#pragma once
/**
 * alpha_model_ext.hpp
 * ===================
 * Experimental polarizability model extended to Z=119-120.
 *
 * IMPORTANT: This module covers elements that have not been synthesised
 * in quantities sufficient for experimental measurement.  All predictions
 * are based on relativistic 4-component DFT/CCSD(T) calculations from:
 *
 *   Schwerdtfeger & Nagle, Mol. Phys. 117, 1200 (2019)   [primary source]
 *   Pershina & Borschevsky, J. Chem. Phys. 149, 204306 (2018)
 *
 * Predicted values:
 *   Z=119 (Uue): alpha ~ 165-200 A^3
 *     The relativistic contraction of the 8s orbital reduces alpha below
 *     what a naive extrapolation of the Cs/Fr/119 group-1 trend would give.
 *     Schwerdtfeger 2019 gives ~165 A^3 (4c-DHF).
 *
 *   Z=120 (Ubn): alpha ~ 100-160 A^3
 *     Similarly contracted.  Schwerdtfeger 2019 gives ~109 A^3 (4c-DHF).
 *
 * The generative model (k_period * r^3 * ...) is not re-fitted for Z>118
 * because there are no training points.  Instead alpha_predict_ext() uses
 * the period-7 k_period coefficient (reasonable floor) with the ext
 * descriptors.  For research use, override with the literature values.
 *
 * Usage:
 *   #include "alpha_model_ext.hpp"
 *   double a119 = atomistic::polarization::ext::alpha_predict_ext(119);
 *
 *   // Override with literature best estimate:
 *   double a119_lit = atomistic::polarization::ext::alpha_lit(119);
 */

#include "alpha_model.hpp"
#include "atomic_descriptors_ext.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace atomistic {
namespace polarization {
namespace ext {

// ============================================================================
// Literature best-estimate values for Z=119-120
// Schwerdtfeger & Nagle 2019, 4-component DHF relativistic calculation.
// ============================================================================

inline double alpha_lit(uint32_t Z) noexcept {
    if (Z == 119) return 165.0;  // Uue: 4c-DHF, Schwerdtfeger 2019
    if (Z == 120) return 109.0;  // Ubn: 4c-DHF, Schwerdtfeger 2019
    return 0.0;  // no lit value for Z>120
}

// ============================================================================
// Generative prediction extended to Z=119-120
//
// Uses the standard AlphaModelParams fitted on Z=1-118, but applies
// ext:: descriptors so the period/block/radius are physically plausible.
// Period-8 uses k_period[6] (period-7 coefficient) as a floor.
// ============================================================================

inline double alpha_predict_ext(uint32_t Z, const AlphaModelParams& params = {}) noexcept {
    if (Z <= 118) return alpha_predict(Z, params);

    // Z=119 or Z=120 only
    if (Z > 120) return 1.76;  // fallback for undefined Z

    // Use the core model formula with ext descriptors.
    // No f-electrons, no IE data for Z>118 — use simplified baseline.
    const double r   = cov_radius_ext(Z);
    const double z   = static_cast<double>(Z);
    const double r_eff = r * (1.0 + params.c_rel * z * z);
    const double r3  = r_eff * r_eff * r_eff;
    const uint32_t b = block_index_ext(Z);  // s-block = 0
    const double k   = params.k_period[6];  // period-7 coefficient as floor
    const double cb  = params.c_block[b];

    // Estimate g_bind with a typical alkali I_1 (~4 eV for period-8 s-block)
    const double g_bind = 1.0 / (1.0 + std::max(params.b_bind, 0.0) * 4.0);

    return std::max(cb * k * r3 * g_bind, 0.1);
}

} // namespace ext
} // namespace polarization
} // namespace atomistic
