#pragma once
/**
 * atomic_descriptors_ext.hpp
 * ==========================
 * Experimental extension of atomic_descriptors.hpp to Z=119-120.
 *
 * Z=119 (Uue, ununennium) and Z=120 (Ubn, unbinilium) are not yet
 * synthesised in macroscopic quantities.  All data here are from
 * fully-relativistic 4-component DFT/CCSD(T) predictions.
 *
 * Physical notes:
 *   Z=119: expected [Og] 8s^1  — super-heavy alkali analogue, period 8
 *   Z=120: expected [Og] 8s^2  — super-heavy alkaline-earth analogue
 *
 *   Relativistic effects are extreme here:
 *   - 8s orbital strongly contracted (s penetration of nuclear charge)
 *   - 7p_{1/2} spin-orbit lowered into 8s energy range
 *   - alpha may be SMALLER than Cs/Fr despite heavier mass (relativistic s-contraction)
 *
 * References:
 *   Schwerdtfeger & Nagle, Mol. Phys. 117, 1200 (2019) — Tables III-IV
 *   Pershina & Borschevsky, J. Chem. Phys. 149, 204306 (2018)
 *   Fricke et al., Struct. Chem. 32, 111 (2021)
 *
 * Usage:
 *   #include "atomic_descriptors_ext.hpp"
 *   double r = atomistic::polarization::ext::cov_radius_ext(119);
 *
 * This header is intentionally separate from atomic_descriptors.hpp.
 * Including it opts into theoretical territory.
 */

#include "atomic_descriptors.hpp"
#include <cstdint>

namespace atomistic {
namespace polarization {
namespace ext {

// ============================================================================
// Extended covalent radii: Z=119-120
//
// Values from Pyykkö 2015 extrapolation and Fricke 2021 estimates.
// Uncertainty: ±0.15 Angstrom (larger than Z<=118 table).
// ============================================================================

inline double cov_radius_ext(uint32_t Z) noexcept {
    if (Z <= 118) return desc::cov_radius(Z);
    if (Z == 119) return 2.60;  // Uue: super-heavy alkali, Pyykkö 2015 extrapolation
    if (Z == 120) return 2.21;  // Ubn: super-heavy alkaline-earth
    // Z>120: return a period-8 fallback (same logic as period-7 but larger)
    return 1.80;
}

// ============================================================================
// Extended period: returns 8 for Z=119-120
// ============================================================================

inline uint32_t period_ext(uint32_t Z) noexcept {
    if (Z <= 118) return desc::period(Z);
    if (Z <= 120) return 8;
    return 8;  // hypothetical Z>120
}

// ============================================================================
// Extended group: Z=119=group 1, Z=120=group 2
// ============================================================================

inline uint32_t group_ext(uint32_t Z) noexcept {
    if (Z <= 118) return desc::group(Z);
    if (Z == 119) return 1;   // Uue: expected s^1, alkali analogue
    if (Z == 120) return 2;   // Ubn: expected s^2, alkaline-earth analogue
    return 0;
}

// ============================================================================
// Extended block
// ============================================================================

inline desc::Block block_ext(uint32_t Z) noexcept {
    if (Z <= 118) return desc::block(Z);
    // Z=119-120 are expected s-block
    return desc::Block::s;
}

inline uint32_t block_index_ext(uint32_t Z) noexcept {
    return static_cast<uint32_t>(block_ext(Z));
}

// ============================================================================
// Extended active_valence
//
// Z=119: 8s^1 → 1 valence electron
// Z=120: 8s^2 → 2 valence electrons
// Heavy relativistic effects may reduce chemical activity but the descriptor
// uses the nominal valence count for the fitted model.
// ============================================================================

inline uint32_t active_valence_ext(uint32_t Z) noexcept {
    if (Z <= 118) return desc::active_valence(Z);
    if (Z == 119) return 1;
    if (Z == 120) return 2;
    return 2;
}

// ============================================================================
// Extended max_valence_in_period: period 8 → 50 (8s 8p 7d 6f 5g)
// Only s subshell fills for Z=119-120, but period ceiling sets the descriptor.
// ============================================================================

inline uint32_t max_valence_in_period_ext(uint32_t p) noexcept {
    if (p <= 7) return desc::max_valence_in_period(p);
    return 50;  // period 8: 8s + 8p + 7d + 6f + 5g (theoretical)
}

// ============================================================================
// Extended electronegativity proxy
// ============================================================================

inline double electronegativity_proxy_ext(uint32_t Z) noexcept {
    if (Z <= 118) return desc::electronegativity_proxy(Z);
    double r = cov_radius_ext(Z);
    if (r < 0.1) return 2.0;
    uint32_t p = period_ext(Z);
    double v = static_cast<double>(active_valence_ext(Z));
    double chi = v / (r * r) * 0.359;
    return std::max(chi, 0.1);
}

// ============================================================================
// Extended softness
// ============================================================================

inline double softness_ext(uint32_t Z) noexcept {
    if (Z <= 118) return desc::softness(Z);
    uint32_t p    = period_ext(Z);
    uint32_t nv   = active_valence_ext(Z);
    uint32_t maxv = max_valence_in_period_ext(p);
    double   chi  = electronegativity_proxy_ext(Z);
    return (static_cast<double>(nv) / static_cast<double>(maxv)) / chi;
}

} // namespace ext
} // namespace polarization
} // namespace atomistic
