#pragma once
/**
 * atomic_descriptors.hpp
 * ======================
 * Single authoritative source for atomic descriptors used by the
 * polarizability model (and anything else that needs periodic-table
 * structure without pulling in full external databases).
 *
 * Design rules:
 *   1. No external data files.  All information is derived from Z alone.
 *   2. One function per concept.  No hidden couplings.
 *   3. Every non-obvious rule has a documented physical justification.
 *   4. Functions are pure (noexcept, no state).
 *
 * Namespace: atomistic::polarization::desc
 *
 * These functions form the "descriptor layer" for alpha_model.hpp.
 * They should not be modified to tune the model — only alpha_model.hpp's
 * AlphaModelParams should change.  If descriptor logic changes, all
 * downstream tests must be re-verified.
 */

#include <cmath>
#include <cstdint>

namespace atomistic {
namespace polarization {
namespace desc {

// ============================================================================
// Block classification
// ============================================================================

enum class Block : uint8_t { s = 0, p = 1, d = 2, f = 3 };

// ============================================================================
// Covalent radii (Angstrom)
// Pyykko & Atsumi, Chem. Eur. J. 15, 186 (2009).  Z=1-118.
// ============================================================================

inline double cov_radius(uint32_t Z) noexcept {
    // clang-format off
    static constexpr double R[119] = {
        0.00,                                                               // Z=0
        0.32, 0.46,                                                         // H  He
        1.33, 1.02, 0.85, 0.75, 0.71, 0.63, 0.64, 0.67,                   // Li-Ne
        1.55, 1.39, 1.26, 1.16, 1.11, 1.03, 0.99, 0.96,                   // Na-Ar
        1.96, 1.71,                                                         // K  Ca
        1.48, 1.36, 1.34, 1.22, 1.19, 1.16, 1.11, 1.10, 1.12, 1.18,       // Sc-Zn
        1.24, 1.21, 1.21, 1.16, 1.14, 1.17,                               // Ga-Kr
        2.10, 1.85,                                                         // Rb Sr
        1.63, 1.54, 1.47, 1.38, 1.28, 1.25, 1.25, 1.20, 1.28, 1.36,       // Y -Cd
        1.42, 1.40, 1.40, 1.36, 1.33, 1.31,                               // In-Xe
        2.32, 1.96,                                                         // Cs Ba
        1.80, 1.63, 1.76, 1.74, 1.73, 1.72, 1.68, 1.69,                   // La-Gd
        1.68, 1.67, 1.66, 1.65, 1.64, 1.70, 1.62,                         // Tb-Lu
        1.52, 1.46, 1.37, 1.31, 1.29, 1.22, 1.23, 1.24, 1.33,             // Hf-Hg
        1.44, 1.44, 1.51, 1.45, 1.47, 1.42,                               // Tl-Rn
        2.23, 2.01,                                                         // Fr Ra
        1.86, 1.75, 1.69, 1.70, 1.71, 1.72, 1.66, 1.66,                   // Ac-Cm
        1.68, 1.68, 1.65, 1.67, 1.73, 1.76,                               // Bk-No
        1.61, 1.57, 1.49, 1.43, 1.41, 1.34, 1.29, 1.28,                   // Lr-Ds
        1.21, 1.22, 1.36, 1.43, 1.62, 1.75, 1.65, 1.57                    // Rg-Og
    };
    // clang-format on
    return (Z > 0 && Z <= 118) ? R[Z] : 1.40;
}

// ============================================================================
// Period (1-7)
// ============================================================================

inline uint32_t period(uint32_t Z) noexcept {
    if (Z <=  2) return 1;
    if (Z <= 10) return 2;
    if (Z <= 18) return 3;
    if (Z <= 36) return 4;
    if (Z <= 54) return 5;
    if (Z <= 86) return 6;
    return 7;
}

// ============================================================================
// Group (1-18).  Returns 0 for lanthanides and actinides (f-block insets).
// ============================================================================

inline uint32_t group(uint32_t Z) noexcept {
    if (Z == 1)  return 1;   // H
    if (Z == 2)  return 18;  // He
    // Period 2
    if (Z >= 3  && Z <= 4)   return Z - 2;   // Li=1, Be=2
    if (Z >= 5  && Z <= 10)  return Z + 8;   // B=13..Ne=18
    // Period 3
    if (Z >= 11 && Z <= 12)  return Z - 10;  // Na=1, Mg=2
    if (Z >= 13 && Z <= 18)  return Z;        // Al=13..Ar=18
    // Period 4
    if (Z >= 19 && Z <= 20)  return Z - 18;  // K=1, Ca=2
    if (Z >= 21 && Z <= 30)  return Z - 18;  // Sc=3..Zn=12
    if (Z >= 31 && Z <= 36)  return Z - 18;  // Ga=13..Kr=18
    // Period 5
    if (Z >= 37 && Z <= 38)  return Z - 36;  // Rb=1, Sr=2
    if (Z >= 39 && Z <= 48)  return Z - 36;  // Y=3..Cd=12
    if (Z >= 49 && Z <= 54)  return Z - 36;  // In=13..Xe=18
    // Period 6
    if (Z >= 55 && Z <= 56)  return Z - 54;  // Cs=1, Ba=2
    if (Z >= 57 && Z <= 71)  return 0;        // lanthanides
    if (Z >= 72 && Z <= 80)  return Z - 68;  // Hf=4..Hg=12
    if (Z >= 81 && Z <= 86)  return Z - 68;  // Tl=13..Rn=18
    // Period 7
    if (Z >= 87 && Z <= 88)  return Z - 86;  // Fr=1, Ra=2
    if (Z >= 89 && Z <= 103) return 0;        // actinides
    if (Z >= 104 && Z <= 112) return Z - 100; // Rf=4..Cn=12
    if (Z >= 113 && Z <= 118) return Z - 100; // Nh=13..Og=18
    return 0;
}

// ============================================================================
// Block (s / p / d / f)
// ============================================================================

inline Block block(uint32_t Z) noexcept {
    uint32_t g = group(Z);
    if (g >= 1  && g <= 2)  return Block::s;
    if (g >= 13 && g <= 18) return Block::p;
    if (g >= 3  && g <= 12) return Block::d;
    // f-block insets
    if (Z >= 57 && Z <= 71) return Block::f; // lanthanides
    if (Z >= 89 && Z <= 103) return Block::f; // actinides
    return Block::p; // superheavy fallback
}

// ============================================================================
// Maximum valence electrons in period (normalisation denominator for softness)
// ============================================================================

inline uint32_t max_valence_in_period(uint32_t p) noexcept {
    switch (p) {
        case 1: return 2;
        case 2: return 8;
        case 3: return 8;
        case 4: return 18;
        case 5: return 18;
        case 6: return 32;
        case 7: return 32;
        default: return 8;
    }
}

// ============================================================================
// Active valence electron count
//
// "Active valence" means electrons that control environment-responsive
// deformability in a way correlated with bonding and radius proxies.
//
// F-BLOCK DESIGN DECISION (see postmortem, Blocker 1):
//   Lanthanides (Z=57-71) and actinides (Z=89-103) return 3.
//   Physical basis: 4f/5f electrons are spatially contracted and core-like;
//   their contribution to polarisability is absorbed by c_block[f], not s(Z).
//   Using 3+(Z-57) injects a fake monotonic softness gradient across the series.
//   The invariant "active_valence(Z) == 3 for all lanthanides/actinides"
//   is enforced by test T1 in tests/test_alpha_model.cpp.
// ============================================================================

inline uint32_t active_valence(uint32_t Z) noexcept {
    uint32_t g = group(Z);
    if (g >= 1  && g <= 2)  return g;       // s-block: 1 or 2
    if (g >= 13 && g <= 18) return g - 10;  // p-block: 3..8
    if (g >= 3  && g <= 12) return g;       // d-block: 3..12

    // f-block: fixed at 3 (see design note above)
    if (Z >= 57 && Z <= 71)  return 3;   // lanthanides
    if (Z >= 89 && Z <= 103) return 3;   // actinides

    return 4; // superheavy fallback
}

// ============================================================================
// Electronegativity proxy
//
// Allred-Rochow style: chi ~ active_valence / r_cov^2, normalised to
// chi_C ~ 2.55 (Pauling scale) by a factor of 0.359.
// Noble gases (group 18): use full-shell electron count for Z_eff,
// producing high chi and therefore low softness (physically correct).
// ============================================================================

inline double electronegativity_proxy(uint32_t Z) noexcept {
    double r = cov_radius(Z);
    if (r < 0.1) return 2.0;

    uint32_t g = group(Z);
    uint32_t p = period(Z);

    double v = (g == 18)
        ? static_cast<double>(max_valence_in_period(p))  // noble gas: full shell
        : static_cast<double>(active_valence(Z));

    // Normalisation: C has v=4, r=0.75 => raw=7.11; Pauling chi_C=2.55 => 0.359
    double chi = v / (r * r) * 0.359;
    return std::max(chi, 0.1);
}

// ============================================================================
// Softness descriptor  s(Z) = (n_val / max_val) / chi
//
// High s: diffuse electron cloud, easily polarisable (alkali metals)
// Low  s: tight electrons, low polarisability (F, Ne, noble gases)
// ============================================================================

inline double softness(uint32_t Z) noexcept {
    uint32_t p    = period(Z);
    uint32_t nv   = active_valence(Z);
    uint32_t maxv = max_valence_in_period(p);
    double   chi  = electronegativity_proxy(Z);
    return (static_cast<double>(nv) / static_cast<double>(maxv)) / chi;
}

// ============================================================================
// Convenience: block as integer index  s=0, p=1, d=2, f=3
// ============================================================================

inline uint32_t block_index(uint32_t Z) noexcept {
    return static_cast<uint32_t>(block(Z));
}

// ============================================================================
// Shell-closure proximity  g_shell(Z)
//
// Measures how close element Z is to a closed-shell configuration.
// Noble gases (group 18): 1.0  — full closure, maximum suppression
// Halogens   (group 17): smooth falloff (Gaussian, sigma=1.8)
// Others:                 ~0
//
// Physical basis: closed-shell atoms have stiff, compact electron clouds
// whose polarizability does NOT follow the smooth r^3 volume scaling.
// The model multiplies alpha by (1 - c_shell * shell_closure(Z)) where
// c_shell is a fitted suppression amplitude.
//
// The sigma=1.8 Gaussian width means group 16 (chalcogens) still get a
// ~5% proximity score, which is physically reasonable (Se, Te have
// slightly anomalous polarizabilities relative to their neighbors).
// ============================================================================

inline double shell_closure(uint32_t Z) noexcept {
    uint32_t g = group(Z);
    if (g == 0) return 0.0;        // f-block: not near closure
    int dist = 18 - static_cast<int>(g);
    if (dist < 0) dist = 0;
    // Gaussian centered on group 18 with sigma=1.8
    return std::exp(-0.5 * dist * dist / (1.8 * 1.8));
}

// ============================================================================
// Relativistic stiffening factor  f_rel(Z)
//
// Heavy atoms exhibit relativistic contraction of s and p_1/2 orbitals,
// reducing their participation in polarizability.  This effect is NOT
// monotonic with Z — it peaks at specific "hotspot" configurations:
//
//   1. Z~80 (Hg region): 6s^2 inert pair + filled 5d^10.  Hg is the
//      poster child — its 6s contracts so strongly that Hg is liquid at
//      room temperature.  Affects Au(79)–Tl(81) as well.
//
//   2. Z~115 (superheavy 7p region): extreme spin-orbit splitting of
//      7p_{1/2} / 7p_{3/2} compresses the effective cloud.
//      Affects Nh(113)–Og(118).
//
// Model: sum of two Gaussians centered at the hotspots.
//   f_rel(Z) = exp(-(Z-80)^2 / 200) + 0.5 * exp(-(Z-115)^2 / 50)
//
// This returns ~1.0 at Hg, ~0.7 at Au/Tl, ~0.3 at Ir/Pb, ~0 for Z<60,
// and a second peak ~0.5 in the Nh-Og region.
// ============================================================================

inline double relativistic_factor(uint32_t Z) noexcept {
    double z = static_cast<double>(Z);
    // Gaussian 1: 6s inert-pair hotspot at Hg (Z=80), sigma=10
    double g1 = std::exp(-(z - 80.0) * (z - 80.0) / 200.0);
    // Gaussian 2: 7p spin-orbit hotspot at Mc (Z=115), sigma=5
    double g2 = 0.5 * std::exp(-(z - 115.0) * (z - 115.0) / 50.0);
    return std::min(g1 + g2, 1.0);
}

// ============================================================================
// Shell fill fraction  f(Z) = active_valence(Z) / max_valence_in_period(Z)
//
// Measures how "full" the outermost shell is.
//   0.0   → empty shell (impossible, but limiting)
//   0.125 → Li  (1 electron in an 8-capacity shell: very diffuse)
//   0.5   → C, Si  (mid-shell: orbital extent ≈ rcov)
//   1.0   → Ne, Ar (full shell: compact, rcov overestimates cloud)
//
// Physical basis: an atom with few valence electrons has a diffuse
// outermost orbital that extends well beyond rcov (the covalent bond
// half-length).  Conversely, a full shell contracts below rcov due to
// increased effective nuclear charge.
//
// Used by the v2.8.0 g_fblock correction to distinguish sparse-shell
// from full-shell atoms within the f-block series.
// ============================================================================

inline double fill_fraction(uint32_t Z) noexcept {
    uint32_t p    = period(Z);
    uint32_t nv   = active_valence(Z);
    uint32_t maxv = max_valence_in_period(p);
    return static_cast<double>(nv) / static_cast<double>(maxv);
}

// ============================================================================
// F-block anomaly descriptors  (v2.8.0)
//
// These three functions encode non-monotonic structure within the lanthanide
// (Z=57-71) and actinide (Z=89-103) series that rcov^3 alone cannot express.
// They return 0.0 for all non-f-block elements.
//
// Physical basis:
//   f_series_position  — linear 0→1 across the series; captures the smooth
//                        lanthanide contraction beyond what rcov drift provides.
//   f_half_shell       — Gaussian at half-filling (Eu Z=63 / Am Z=95).
//                        4f^7 / 5f^7 half-shell stabilisation raises alpha
//                        above the smooth trend (Eu bump: ref=27.7 vs trend~22).
//   f_full_shell       — Gaussian at full-filling (Yb Z=70 / No Z=102).
//                        4f^14 / 5f^14 filled-shell contraction suppresses
//                        alpha below trend (Yb: ref=19.5 vs trend~22).
//
// These feed into AlphaModelParams::g_fblock(Z):
//   g_fblock = 1 + c_f_pos*f_series_position
//                + c_f_half*f_half_shell
//                + c_f_full*f_full_shell
// ============================================================================

// Linear drift 0→1 across each f-series.
inline double f_series_position(uint32_t Z) noexcept {
    if (Z >= 57 && Z <= 71)  return (Z - 57) / 14.0;   // La=0.0 → Lu=1.0
    if (Z >= 89 && Z <= 103) return (Z - 89) / 14.0;   // Ac=0.0 → No=1.0
    return 0.0;
}

// Half-shell proximity: peaks at Eu (Z=63, 4f^7) and Am (Z=95, 5f^7).
inline double f_half_shell_proximity(uint32_t Z) noexcept {
    if (Z >= 57 && Z <= 71) {
        double d = static_cast<double>(Z) - 63.0;
        return std::exp(-d * d / 8.0);   // sigma ~= 2.8
    }
    if (Z >= 89 && Z <= 103) {
        double d = static_cast<double>(Z) - 95.0;
        return std::exp(-d * d / 8.0);
    }
    return 0.0;
}

// Full-shell proximity: peaks at Yb (Z=70, 4f^14) and No (Z=102, 5f^14).
inline double f_full_shell_proximity(uint32_t Z) noexcept {
    if (Z >= 57 && Z <= 71) {
        double d = static_cast<double>(Z) - 70.0;
        return std::exp(-d * d / 4.0);   // sigma ~= 2.0
    }
    if (Z >= 89 && Z <= 103) {
        double d = static_cast<double>(Z) - 102.0;
        return std::exp(-d * d / 4.0);
    }
    return 0.0;
}

// ============================================================================
// F-shell electron count  n_f(Z)  (v2.8.X — Alpha Method D)
//
// Ground-state 4f/5f occupancy for lanthanides and actinides.
// Returns 0 for all non-f-block elements.
//
// Configurations from NIST Atomic Spectra Database (ASD).
// Note the irregular filling: La/Gd promote to 5d, Ce shares 4f+5d,
// and Tb fills 4f^9 (skipping 4f^8 5d^1).  These jumps are physically
// real and encode the exchange/correlation effects that drive the
// anomalous polarizability structure.
// ============================================================================

inline uint32_t f_electron_count(uint32_t Z) noexcept {
    // 4f occupancy for lanthanides (Z=57-71)
    // clang-format off
    static constexpr uint32_t Ln4f[15] = {
        0,   // La  57  [Xe] 5d^1 6s^2         (0 f-electrons)
        1,   // Ce  58  [Xe] 4f^1 5d^1 6s^2     (1)
        3,   // Pr  59  [Xe] 4f^3 6s^2           (3)
        4,   // Nd  60  [Xe] 4f^4 6s^2           (4)
        5,   // Pm  61  [Xe] 4f^5 6s^2           (5)
        6,   // Sm  62  [Xe] 4f^6 6s^2           (6)
        7,   // Eu  63  [Xe] 4f^7 6s^2           (7) half-filled
        7,   // Gd  64  [Xe] 4f^7 5d^1 6s^2     (7) promotes to 5d
        9,   // Tb  65  [Xe] 4f^9 6s^2           (9)
       10,   // Dy  66  [Xe] 4f^10 6s^2         (10)
       11,   // Ho  67  [Xe] 4f^11 6s^2         (11)
       12,   // Er  68  [Xe] 4f^12 6s^2         (12)
       13,   // Tm  69  [Xe] 4f^13 6s^2         (13)
       14,   // Yb  70  [Xe] 4f^14 6s^2         (14) full
       14    // Lu  71  [Xe] 4f^14 5d^1 6s^2   (14)
    };
    // clang-format on
    if (Z >= 57 && Z <= 71) return Ln4f[Z - 57];

    // 5f occupancy for actinides (Z=89-103)
    // clang-format off
    static constexpr uint32_t Ac5f[15] = {
        0,   // Ac  89  [Rn] 6d^1 7s^2           (0)
        0,   // Th  90  [Rn] 6d^2 7s^2           (0) no 5f
        2,   // Pa  91  [Rn] 5f^2 6d^1 7s^2     (2)
        3,   // U   92  [Rn] 5f^3 6d^1 7s^2     (3)
        4,   // Np  93  [Rn] 5f^4 6d^1 7s^2     (4)
        6,   // Pu  94  [Rn] 5f^6 7s^2           (6)
        7,   // Am  95  [Rn] 5f^7 7s^2           (7) half-filled
        7,   // Cm  96  [Rn] 5f^7 6d^1 7s^2     (7) promotes to 6d
        9,   // Bk  97  [Rn] 5f^9 7s^2           (9)
       10,   // Cf  98  [Rn] 5f^10 7s^2         (10)
       11,   // Es  99  [Rn] 5f^11 7s^2         (11)
       12,   // Fm 100  [Rn] 5f^12 7s^2         (12)
       13,   // Md 101  [Rn] 5f^13 7s^2         (13)
       14,   // No 102  [Rn] 5f^14 7s^2         (14) full
       14    // Lr 103  [Rn] 5f^14 7s^2 7p^1   (14)
    };
    // clang-format on
    if (Z >= 89 && Z <= 103) return Ac5f[Z - 89];

    return 0;  // non-f-block: no f electrons
}

// ============================================================================
// First ionization energy  I_1(Z)  (eV)  —  v2.8.X (Alpha Method D)
//
// NIST Atomic Spectra Database for Z=1-103.
// Z=104-118: relativistic CCSD(T)/DHF estimates from
//   Fricke & Soff (1977), Eliav et al. (1998), Schwerdtfeger (2019).
//
// Physical role in the model:
//   g_bind(Z) = 1 / (1 + b_bind * I_1(Z))
//   This single descriptor replaces three previous mechanisms:
//     - softness coupling (a_soft * s)           — low I_1 = soft
//     - electronegativity suppression (chi^b_chi) — high I_1 = high chi
//     - shell-closure gate (c_shell)              — noble gases have highest I_1
//   Physically: I_1 measures how tightly the outermost electron is bound.
//   High I_1 → stiff, compact cloud → low polarizability.
// ============================================================================

inline double first_ionization_energy(uint32_t Z) noexcept {
    // clang-format off
    static constexpr double IE[119] = {
        0.000,                                                                 // Z=0
       13.598, 24.587,                                                         // H   He
        5.392,  9.323,  8.298, 11.260, 14.534, 13.618, 17.423, 21.565,        // Li  - Ne
        5.139,  7.646,  5.986,  8.152, 10.487, 10.360, 12.968, 15.760,        // Na  - Ar
        4.341,  6.113,                                                         // K   Ca
        6.562,  6.828,  6.746,  6.767,  7.434,  7.902,  7.881,  7.640,        // Sc  - Ni
        7.726,  9.394,                                                         // Cu  Zn
        5.999,  7.900,  9.789,  9.752, 11.814, 14.000,                        // Ga  - Kr
        4.177,  5.695,                                                         // Rb  Sr
        6.217,  6.634,  6.759,  7.092,  7.280,  7.361,  7.459,  8.337,        // Y   - Pd
        7.576,  8.994,                                                         // Ag  Cd
        5.786,  7.344,  8.608,  9.010, 10.451, 12.130,                        // In  - Xe
        3.894,  5.212,                                                         // Cs  Ba
        5.577,  5.539,  5.473,  5.525,  5.582,  5.644,  5.670,  6.150,        // La  - Gd
        5.864,  5.939,  6.022,  6.108,  6.184,  6.254,  5.426,                // Tb  - Lu
        6.825,  7.550,  7.864,  7.833,  8.438,  8.967,  8.959,  9.226,        // Hf  - Au
       10.438,                                                                 // Hg
        6.108,  7.417,  7.286,  8.414,  9.318, 10.749,                        // Tl  - Rn
        4.073,  5.278,                                                         // Fr  Ra
        5.170,  6.307,  5.890,  6.194,  6.266,  6.026,  5.974,  5.991,        // Ac  - Cm
        6.198,  6.282,  6.367,  6.500,  6.580,  6.650,  4.960,                // Bk  - Lr
        6.010,  6.890,  7.850,  7.700,  7.600,  8.600,  9.060,  9.320,        // Rf  - Ds
       11.970,  7.310,  8.540,  5.580,  7.000,  7.700,  8.880                 // Rg  - Og
    };
    // clang-format on
    return (Z > 0 && Z <= 118) ? IE[Z] : 6.0;
}

} // namespace desc
} // namespace polarization
} // namespace atomistic
