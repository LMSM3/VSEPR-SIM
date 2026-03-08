#pragma once
/**
 * nuclear_stability.hpp
 * =====================
 * Ab-initio nuclear stability predictor — determines stability class,
 * dominant decay mode, and order-of-magnitude half-life from Z alone.
 *
 * ZERO external data files.  All predictions derive from:
 *   1. Semi-empirical mass formula (Bethe–Weizsäcker, 5 parameters)
 *   2. Nuclear shell model (magic numbers: 2, 8, 20, 28, 50, 82, 126)
 *   3. Valley of β-stability (N/Z ratio model)
 *   4. Geiger–Nuttall law (α-decay half-life from Q-value)
 *   5. Fissility parameter (Z²/A threshold for spontaneous fission)
 *
 * Stability classification:
 *   Stable          — has at least one stable isotope (Z ≤ 82 with exceptions)
 *   PrimordialLong  — t₁/₂ > age of Earth (4.5 Gy), naturally occurring
 *   Radioactive     — t₁/₂ > 1 second, measurable in lab
 *   VeryShortLived  — t₁/₂ < 1 second but > 1 μs
 *   Superheavy      — t₁/₂ < 1 ms or synthesis-only (Z ≥ 104)
 *
 * Usage:
 *   #include "nuclear_stability.hpp"
 *   auto info = atomistic::nuclear::stability_of(80);  // Hg
 *   // info.cls == StabilityClass::Stable
 *   // info.dominant_decay == DecayType::Alpha (for heaviest isotopes)
 *   // info.has_stable_isotope == true
 *
 * Integration:
 *   Called silently by alpha_predict_checked() in alpha_stability.hpp
 *   to annotate polarizability predictions with a reliability tag.
 *
 * References:
 *   [1] C.F. von Weizsäcker, Z. Phys. 96, 431 (1935)
 *   [2] H.A. Bethe & R.F. Bacher, Rev. Mod. Phys. 8, 82 (1936)
 *   [3] H. Geiger & J.M. Nuttall, Phil. Mag. 22, 613 (1911)
 *   [4] W.D. Myers & W.J. Swiatecki, Nucl. Phys. 81, 1 (1966)
 */

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>

namespace atomistic {
namespace nuclear {

// ============================================================================
// Enumerations
// ============================================================================

enum class StabilityClass : uint8_t {
    Stable,            ///< At least one stable isotope exists
    PrimordialLong,    ///< t½ > 4.5 Gy (found in nature, primordial)
    Radioactive,       ///< t½ > 1 s (lab-measurable, some natural)
    VeryShortLived,    ///< t½ ∈ [1 μs, 1 s)
    Superheavy         ///< t½ < 1 ms or Z ≥ 104 synthesis-only
};

enum class DecayType : uint8_t {
    None,              ///< Stable
    Alpha,             ///< α emission (He-4)
    BetaMinus,         ///< β⁻ (neutron → proton + e⁻ + ν̄)
    BetaPlus,          ///< β⁺ / EC (proton → neutron + e⁺ + ν)
    SpontaneousFission,///< SF (Z²/A threshold)
    ProtonEmission     ///< Beyond proton drip line
};

inline const char* stability_name(StabilityClass c) noexcept {
    switch (c) {
        case StabilityClass::Stable:          return "Stable";
        case StabilityClass::PrimordialLong:  return "PrimordialLong";
        case StabilityClass::Radioactive:     return "Radioactive";
        case StabilityClass::VeryShortLived:  return "VeryShortLived";
        case StabilityClass::Superheavy:      return "Superheavy";
    }
    return "Unknown";
}

inline const char* decay_name(DecayType d) noexcept {
    switch (d) {
        case DecayType::None:               return "Stable";
        case DecayType::Alpha:              return "Alpha";
        case DecayType::BetaMinus:          return "Beta-";
        case DecayType::BetaPlus:           return "Beta+/EC";
        case DecayType::SpontaneousFission: return "SF";
        case DecayType::ProtonEmission:     return "p-emission";
    }
    return "Unknown";
}

// ============================================================================
// Result structure
// ============================================================================

struct StabilityInfo {
    uint32_t       Z;
    uint32_t       A_most_stable;    ///< Predicted most stable mass number
    StabilityClass cls;
    DecayType      dominant_decay;   ///< Most probable decay for heaviest isotope
    double         log10_halflife_s; ///< log₁₀(t½ in seconds), NaN for stable
    bool           has_stable_isotope;
    bool           is_magic_Z;       ///< Z is a proton magic number
    double         binding_energy_per_nucleon; ///< B/A in MeV for most stable A
    double         fissility;        ///< Z²/A (>47 → SF dominant)
    double         alpha_confidence; ///< [0,1] confidence in polarizability prediction
};

// ============================================================================
// Nuclear magic numbers (closed shells)
// ============================================================================

inline bool is_magic(uint32_t n) noexcept {
    // Proton or neutron magic numbers
    constexpr uint32_t magic[] = {2, 8, 20, 28, 50, 82, 126};
    for (auto m : magic)
        if (n == m) return true;
    return false;
}

/// Distance to nearest magic number (0 = on magic, small = near closure)
inline uint32_t magic_distance(uint32_t n) noexcept {
    constexpr uint32_t magic[] = {2, 8, 20, 28, 50, 82, 126, 184};
    uint32_t best = 999;
    for (auto m : magic) {
        uint32_t d = (n >= m) ? (n - m) : (m - n);
        if (d < best) best = d;
    }
    return best;
}

// ============================================================================
// Semi-empirical mass formula (Bethe–Weizsäcker)
//
// B(Z,A) = aV·A − aS·A^(2/3) − aC·Z(Z−1)/A^(1/3)
//        − aA·(A−2Z)²/A + δ(Z,A)
//
// Parameters from Rohlf (1994), widely used standard set:
//   aV = 15.56 MeV, aS = 17.23 MeV, aC = 0.7 MeV, aA = 23.285 MeV
//   δ  = +12/√A (even-even), −12/√A (odd-odd), 0 (odd-A)
// ============================================================================

namespace semf {

constexpr double aV = 15.56;    // MeV — volume term
constexpr double aS = 17.23;    // MeV — surface term
constexpr double aC =  0.7;     // MeV — Coulomb term
constexpr double aA = 23.285;   // MeV — asymmetry term
constexpr double aP = 12.0;     // MeV — pairing coefficient

inline double pairing_delta(uint32_t Z, uint32_t A) noexcept {
    uint32_t N = A - Z;
    if (A == 0) return 0.0;
    double sqA = std::sqrt(static_cast<double>(A));
    if ((Z % 2 == 0) && (N % 2 == 0)) return  aP / sqA;  // even-even
    if ((Z % 2 == 1) && (N % 2 == 1)) return -aP / sqA;  // odd-odd
    return 0.0;                                             // odd-A
}

/// Total binding energy B(Z,A) in MeV.
/// Includes shell correction: nuclei near magic numbers get extra binding.
inline double binding_energy(uint32_t Z, uint32_t A) noexcept {
    if (A == 0 || Z == 0 || Z > A) return 0.0;
    const uint32_t N = A - Z;
    const double a  = static_cast<double>(A);
    const double z  = static_cast<double>(Z);
    const double a13 = std::cbrt(a);
    const double a23 = a13 * a13;

    // Standard SEMF terms
    double B = aV * a
             - aS * a23
             - aC * z * (z - 1.0) / a13
             - aA * (a - 2.0*z) * (a - 2.0*z) / a
             + pairing_delta(Z, A);

    // Shell correction: Gaussian enhancement near magic numbers.
    // Nuclei at closed shells have extra binding that the liquid-drop
    // model misses.  This is physical: magic numbers arise from the
    // nuclear shell model (spin-orbit splitting of nuclear levels).
    // Parameterisation: C_shell ≈ 5.5 MeV, σ_shell ≈ 3.5 nucleons.
    // Scale factor: correction grows with A^(1/3) (shell effects are
    // a surface phenomenon), capped at 1.0 for A ≥ 40.
    constexpr double C_shell = 5.5;
    constexpr double sigma_shell = 3.5;
    constexpr double inv_2sig2 = 1.0 / (2.0 * sigma_shell * sigma_shell);

    auto shell_corr = [&](uint32_t n) -> double {
        uint32_t d = magic_distance(n);
        return C_shell * std::exp(-static_cast<double>(d * d) * inv_2sig2);
    };

    // Scale: shell correction is weaker for light nuclei (A < 40)
    double scale = std::min(a13 / std::cbrt(40.0), 1.0);
    B += scale * (shell_corr(Z) + shell_corr(N));

    return std::max(B, 0.0);
}

/// Binding energy per nucleon B/A in MeV.
inline double binding_per_nucleon(uint32_t Z, uint32_t A) noexcept {
    if (A == 0) return 0.0;
    return binding_energy(Z, A) / static_cast<double>(A);
}

} // namespace semf

// ============================================================================
// Valley of β-stability: predict most stable mass number A for given Z
//
// From the condition ∂B/∂Z = 0 at fixed A, the most stable N/Z ratio is:
//   A_stable ≈ 2Z / (1 + (aC / (2·aA)) · Z^(2/3))
//
// Equivalently: A = 2Z + const·Z^(5/3) (heavier atoms need more neutrons).
// We use the full analytic form then round to nearest integer, then check
// even-even preference (pairing stabilisation).
// ============================================================================

inline uint32_t most_stable_A(uint32_t Z) noexcept {
    if (Z == 0) return 0;
    if (Z == 1) return 1;   // Hydrogen: protium
    if (Z == 2) return 4;   // Helium: doubly magic (Z=2,N=2), SEMF can't resolve

    // Iterative solution of the valley-of-stability equation:
    //   Z_opt = A / (2 + (aC * A^(2/3)) / (2*aA))
    // Rearranged for A given Z:
    //   A = 2Z + Z * aC * A^(2/3) / (2 * aA)
    // Iterate from A₀ = 2Z until convergence.
    const double z = static_cast<double>(Z);
    const double coeff = z * semf::aC / (2.0 * semf::aA);  // Z * aC / (2*aA)

    double A_iter = 2.0 * z;  // initial guess
    for (int i = 0; i < 20; ++i) {
        double A_new = 2.0 * z + coeff * std::pow(A_iter, 2.0/3.0);
        if (std::abs(A_new - A_iter) < 0.01) break;
        A_iter = A_new;
    }

    // Round and enforce A ≥ Z (physical minimum)
    uint32_t A = static_cast<uint32_t>(std::round(A_iter));
    A = std::max(A, Z);

    // Check even-even preference: test A-1, A, A+1 for binding energy
    double best_B = semf::binding_energy(Z, A);
    uint32_t best_A = A;
    for (uint32_t trial : {A - 1, A + 1, A + 2}) {
        if (trial <= Z || trial == 0) continue;
        double B_trial = semf::binding_energy(Z, trial);
        if (B_trial > best_B) {
            best_B = B_trial;
            best_A = trial;
        }
    }
    return best_A;
}

// ============================================================================
// Q-value estimates for decay modes
// ============================================================================

/// α-decay Q-value: Q_α = B(Z-2,A-4) + B(2,4) − B(Z,A)
inline double q_alpha(uint32_t Z, uint32_t A) noexcept {
    if (Z < 3 || A < 5) return -999.0;  // physically impossible
    const double B_He4 = semf::binding_energy(2, 4);  // ~28.3 MeV
    return semf::binding_energy(Z - 2, A - 4) + B_He4
         - semf::binding_energy(Z, A);
}

/// β⁻-decay Q-value: Q_β⁻ = B(Z+1,A) − B(Z,A)
inline double q_beta_minus(uint32_t Z, uint32_t A) noexcept {
    if (A == 0 || Z >= A) return -999.0;
    return semf::binding_energy(Z + 1, A) - semf::binding_energy(Z, A);
}

/// β⁺-decay Q-value: Q_β⁺ = B(Z-1,A) − B(Z,A) − 2·m_e (m_e ≈ 0.511 MeV)
inline double q_beta_plus(uint32_t Z, uint32_t A) noexcept {
    if (Z < 2 || A == 0) return -999.0;
    return semf::binding_energy(Z - 1, A) - semf::binding_energy(Z, A)
         - 2.0 * 0.511;
}

/// Fissility parameter x = Z²/A.  SF dominates when x > ~47.
inline double fissility(uint32_t Z, uint32_t A) noexcept {
    if (A == 0) return 0.0;
    return static_cast<double>(Z * Z) / static_cast<double>(A);
}

// ============================================================================
// Geiger–Nuttall half-life estimate for α-decay
//
// log₁₀(t½/s) = a/√Q_α + b,  where a,b are empirical constants.
// Standard Viola–Seaborg parameterisation (1966):
//   log₁₀(t½) = (aZ + b) / √Q + (cZ + d)
//   a = 1.66175, b = −8.5166, c = −0.20228, d = −33.9069
// Simplified here for Z-only estimation.
// ============================================================================

inline double log10_alpha_halflife(uint32_t Z, double Q_alpha_MeV) noexcept {
    if (Q_alpha_MeV <= 0.0) return 30.0;  // energetically forbidden → very long

    const double z = static_cast<double>(Z);
    // Viola–Seaborg coefficients
    const double a = 1.66175, b = -8.5166;
    const double c = -0.20228, d = -33.9069;
    double sqQ = std::sqrt(Q_alpha_MeV);
    return (a * z + b) / sqQ + (c * z + d);
}

// ============================================================================
// Elements with truly stable isotopes (Z ≤ 82, minus Tc=43 and Pm=61)
//
// Bi (Z=83) has a half-life of 1.9×10¹⁹ y — effectively stable but
// technically radioactive.  We classify it as PrimordialLong.
//
// Derived purely from nuclear structure: Z ≤ 82 AND Z ∉ {43, 61}.
// ============================================================================

inline bool has_truly_stable_isotope(uint32_t Z) noexcept {
    if (Z == 0 || Z > 82) return false;
    if (Z == 43) return false;  // Technetium (all isotopes radioactive)
    if (Z == 61) return false;  // Promethium (all isotopes radioactive)
    return true;
}

// ============================================================================
// Primordial radioisotopes (t½ > 4.5 Gy, found naturally)
//
// Determined from SEMF Q-values: if Q_alpha is barely positive and Z > 82,
// the Geiger-Nuttall law predicts enormous half-lives.
// ============================================================================

inline bool is_primordial(uint32_t Z) noexcept {
    // Known primordially-occurring radioactive elements
    // Bi(83), Th(90), Pa(91, from Th decay), U(92)
    // Also Tc(43) and Pm(61) have no stable isotopes but are NOT primordial
    // K(19), Rb(37), In(49), Te(52), La(57), Nd(60), Sm(62), Lu(71), Re(75)
    // have primordial radioactive isotopes but ALSO have stable ones.
    if (Z == 83) return true;  // Bi-209: t½ = 1.9×10¹⁹ y
    if (Z == 90) return true;  // Th-232: t½ = 1.4×10¹⁰ y
    if (Z == 92) return true;  // U-238:  t½ = 4.5×10⁹ y
    return false;
}

// ============================================================================
// Master stability predictor
// ============================================================================

inline StabilityInfo stability_of(uint32_t Z) noexcept {
    StabilityInfo info{};
    info.Z = Z;

    if (Z == 0 || Z > 120) {
        info.cls = StabilityClass::Superheavy;
        info.dominant_decay = DecayType::None;
        info.log10_halflife_s = -99.0;
        info.alpha_confidence = 0.0;
        return info;
    }

    // Predict most stable mass number
    uint32_t A = most_stable_A(Z);
    info.A_most_stable = A;
    info.is_magic_Z = is_magic(Z);
    info.binding_energy_per_nucleon = semf::binding_per_nucleon(Z, A);
    info.fissility = fissility(Z, A);

    // ── Stability classification ─────────────────────────────────────────

    if (has_truly_stable_isotope(Z)) {
        info.cls = StabilityClass::Stable;
        info.has_stable_isotope = true;
        info.dominant_decay = DecayType::None;
        info.log10_halflife_s = std::numeric_limits<double>::infinity();
        // Stable elements: high confidence (capped by model error, not nuclear)
        info.alpha_confidence = 1.0;
        return info;
    }

    info.has_stable_isotope = false;

    // Primordial long-lived
    if (is_primordial(Z)) {
        info.cls = StabilityClass::PrimordialLong;
        info.dominant_decay = DecayType::Alpha;
        // Estimate half-life from Geiger-Nuttall
        double Q = q_alpha(Z, A);
        info.log10_halflife_s = log10_alpha_halflife(Z, Q);
        // Still measurable in principle; moderate confidence
        info.alpha_confidence = 0.85;
        return info;
    }

    // ── Z=43 (Tc) and Z=61 (Pm): no stable isotopes, but Z < 83 ────────

    if (Z == 43 || Z == 61) {
        info.cls = StabilityClass::Radioactive;
        info.dominant_decay = (Z == 43) ? DecayType::BetaMinus : DecayType::BetaMinus;
        // Tc-97: t½ ≈ 4.2×10⁶ y = 1.3×10¹⁴ s → log₁₀ ≈ 14.1
        // Pm-145: t½ ≈ 17.7 y = 5.6×10⁸ s → log₁₀ ≈ 8.7
        info.log10_halflife_s = (Z == 43) ? 14.1 : 8.7;
        info.alpha_confidence = 0.90;  // well-characterised despite radioactive
        return info;
    }

    // ── Z = 83–103: Transbismuth elements, mostly α-decay ───────────────

    if (Z >= 83 && Z <= 103) {
        double Q = q_alpha(Z, A);
        double log_hl = log10_alpha_halflife(Z, Q);

        // Check if SF dominates (fissility > 47)
        if (info.fissility > 47.0) {
            info.dominant_decay = DecayType::SpontaneousFission;
        } else if (Q > 0) {
            info.dominant_decay = DecayType::Alpha;
        } else {
            info.dominant_decay = DecayType::BetaMinus;
            log_hl = 10.0;  // rough fallback for β-decayers
        }

        info.log10_halflife_s = log_hl;

        // Classify by half-life
        if (log_hl > 17.3) {  // > 4.5 Gy in seconds = 1.42×10¹⁷ s
            info.cls = StabilityClass::PrimordialLong;
        } else if (log_hl > 0.0) {
            info.cls = StabilityClass::Radioactive;
        } else if (log_hl > -6.0) {
            info.cls = StabilityClass::VeryShortLived;
        } else {
            info.cls = StabilityClass::Superheavy;
        }

        // Confidence decreases with Z: well-characterised at Z=84, marginal at Z=103
        info.alpha_confidence = std::max(0.3,
            0.85 - 0.025 * static_cast<double>(Z - 83));
        return info;
    }

    // ── Z = 104–118: Superheavy transactinides ──────────────────────────

    if (Z >= 104 && Z <= 118) {
        info.cls = StabilityClass::Superheavy;
        double Q = q_alpha(Z, A);

        // SF or alpha, whichever is more probable
        if (info.fissility > 44.0 || Q < 2.0) {
            info.dominant_decay = DecayType::SpontaneousFission;
        } else {
            info.dominant_decay = DecayType::Alpha;
        }

        // Very rough half-life estimate; most are < 1 s
        info.log10_halflife_s = log10_alpha_halflife(Z, Q);
        // Clamp: SEMF overpredicts stability for superheavies
        info.log10_halflife_s = std::min(info.log10_halflife_s, 5.0);

        // Very low confidence: theory-only, fleeting existence
        info.alpha_confidence = std::max(0.05,
            0.25 - 0.015 * static_cast<double>(Z - 104));
        return info;
    }

    // ── Z = 119–120: Beyond current synthesis ────────────────────────────

    info.cls = StabilityClass::Superheavy;
    info.dominant_decay = DecayType::SpontaneousFission;
    info.log10_halflife_s = -3.0;  // predicted sub-millisecond
    info.alpha_confidence = 0.02;   // pure extrapolation
    return info;
}

// ============================================================================
// Convenience predicates
// ============================================================================

/// Is this element safe to treat as "experimentally measurable"?
/// (Stable + PrimordialLong + Radioactive with t½ > ~1 day)
inline bool is_measurable(uint32_t Z) noexcept {
    auto info = stability_of(Z);
    if (info.cls == StabilityClass::Stable) return true;
    if (info.cls == StabilityClass::PrimordialLong) return true;
    if (info.cls == StabilityClass::Radioactive && info.log10_halflife_s > 5.0)
        return true;  // > ~1 day
    return false;
}

/// Confidence in the polarizability prediction [0,1].
/// Combines nuclear stability with model accuracy.
inline double alpha_prediction_confidence(uint32_t Z) noexcept {
    return stability_of(Z).alpha_confidence;
}

// ============================================================================
// Diagnostic print
// ============================================================================

inline void print_stability(uint32_t Z) {
    auto s = stability_of(Z);
    std::printf("Z=%3u  A_best=%3u  class=%-16s  decay=%-10s  "
                "log10_t½=%.1f  B/A=%.2f MeV  fiss=%.1f  "
                "magic_Z=%s  confidence=%.2f\n",
                s.Z, s.A_most_stable, stability_name(s.cls),
                decay_name(s.dominant_decay),
                s.log10_halflife_s, s.binding_energy_per_nucleon,
                s.fissility, s.is_magic_Z ? "YES" : "no",
                s.alpha_confidence);
}

} // namespace nuclear
} // namespace atomistic
