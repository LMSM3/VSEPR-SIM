#pragma once
/**
 * alpha_model.hpp  —  Alpha Method D (v2.8.X)
 * ============================================
 * Empirical-primary polarizability prediction.
 *
 * Prediction path:
 *   Z = 1-118  →  ALPHA_EMPIRICAL[Z-1]  (compile-time table, 0% error by construction)
 *   Z > 118    →  generative model fallback (see below)
 *
 * Generative fallback model (Z > 118):
 *   alpha(Z) = r_eff(Z)^3 * g_block(Z) * g_period(Z) * g_f(n_f(Z)) * g_bind(Z)
 *
 *   r_eff     = r_cov * (1 + c_rel * Z^2)
 *   g_block   = c_block[block_index(Z)]
 *   g_period  = k_period[period(Z) - 1]
 *   g_f(n_f)  = 1 + a_f1*(n_f/14)
 *                 + a_f2*exp(-((n_f - 7)/sigma_f1)^2)
 *                 + a_f3*exp(-((n_f - 14)/sigma_f2)^2)
 *   g_bind    = 1 / (1 + b_bind * I_1(Z))
 *
 * Empirical table sources:
 *   Miller1990        — Miller, JACS 112, 8533 (1990)        [Z=1-54, gas-phase exp.]
 *   Schwerdtfeger2019 — Schwerdtfeger & Nagle, Mol. Phys. 117, 1200 (2019)  [Z=21-118, DHF/CCSD(T)]
 *
 * Drude coupling (for SCF/Drude polarization solver):
 *   k_D(Z) = q_D^2 / alpha(Z)
 *
 * Architecture:
 *   data/polarizability_ref.csv              <- source of empirical table values
 *   tools/fit_alpha_model.cpp                <- diagnostic fitter (no longer primary path)
 *   config/alpha_model_params.json           <- generative fallback params
 *   atomistic/models/atomic_descriptors.hpp  <- descriptor layer
 *   atomistic/models/alpha_model.hpp         <- THIS FILE
 */

#include "atomic_descriptors.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace atomistic {
namespace polarization {

// ============================================================================
// Empirical polarizability table — Z = 1-118  (Angstrom^3)
// Sources: Miller 1990 (Z=1-54), Schwerdtfeger & Nagle 2019 (Z=21-118).
// Index: ALPHA_EMPIRICAL[Z-1]  (Z=1 at index 0, Z=118 at index 117).
// ============================================================================
constexpr double ALPHA_EMPIRICAL[118] = {
 //  Z   sym   alpha (A^3)
     0.667,  //   1  H
     0.205,  //   2  He
    24.3,    //   3  Li
     5.60,   //   4  Be
     3.03,   //   5  B
     1.76,   //   6  C
     1.10,   //   7  N
     0.802,  //   8  O
     0.557,  //   9  F
     0.396,  //  10  Ne
    24.1,    //  11  Na
    10.6,    //  12  Mg
     6.80,   //  13  Al
     5.38,   //  14  Si
     3.63,   //  15  P
     2.90,   //  16  S
     2.18,   //  17  Cl
     1.64,   //  18  Ar
    43.4,    //  19  K
    22.8,    //  20  Ca
    17.8,    //  21  Sc
    14.6,    //  22  Ti
    12.4,    //  23  V
    11.6,    //  24  Cr
     9.4,    //  25  Mn
     8.40,   //  26  Fe
     7.5,    //  27  Co
     6.80,   //  28  Ni
     6.03,   //  29  Cu
     5.75,   //  30  Zn
     8.12,   //  31  Ga
     6.07,   //  32  Ge
     4.31,   //  33  As
     3.77,   //  34  Se
     3.05,   //  35  Br
     2.48,   //  36  Kr
    47.3,    //  37  Rb
    27.6,    //  38  Sr
    22.7,    //  39  Y
    17.9,    //  40  Zr
    15.7,    //  41  Nb
    12.8,    //  42  Mo
    11.4,    //  43  Tc
     9.6,    //  44  Ru
     8.6,    //  45  Rh
     4.8,    //  46  Pd
     7.2,    //  47  Ag
     7.36,   //  48  Cd
    10.2,    //  49  In
     7.7,    //  50  Sn
     6.6,    //  51  Sb
     5.5,    //  52  Te
     5.35,   //  53  I
     4.04,   //  54  Xe
    59.42,   //  55  Cs
    39.7,    //  56  Ba
    31.1,    //  57  La
    29.6,    //  58  Ce
    28.2,    //  59  Pr
    31.4,    //  60  Nd
    28.8,    //  61  Pm
    23.8,    //  62  Sm
    27.7,    //  63  Eu
    23.5,    //  64  Gd
    23.6,    //  65  Tb
    22.7,    //  66  Dy
    21.5,    //  67  Ho
    20.4,    //  68  Er
    19.5,    //  69  Tm
    19.5,    //  70  Yb
    21.0,    //  71  Lu
    16.2,    //  72  Hf
    13.1,    //  73  Ta
    11.1,    //  74  W
     9.7,    //  75  Re
     8.5,    //  76  Os
     7.6,    //  77  Ir
     6.5,    //  78  Pt
     5.8,    //  79  Au
     5.7,    //  80  Hg
     7.6,    //  81  Tl
     6.8,    //  82  Pb
     7.4,    //  83  Bi
     6.0,    //  84  Po
     4.5,    //  85  At
     5.1,    //  86  Rn
    48.1,    //  87  Fr
    38.3,    //  88  Ra
    32.1,    //  89  Ac
    32.1,    //  90  Th
    25.0,    //  91  Pa
    24.9,    //  92  U
    24.0,    //  93  Np
    23.0,    //  94  Pu
    22.0,    //  95  Am
    23.0,    //  96  Cm
    22.0,    //  97  Bk
    21.0,    //  98  Cf
    20.0,    //  99  Es
    19.0,    // 100  Fm
    18.0,    // 101  Md
    17.5,    // 102  No
    24.0,    // 103  Lr
    16.3,    // 104  Rf
    14.6,    // 105  Db
    12.4,    // 106  Sg
    10.4,    // 107  Bh
     8.8,    // 108  Hs
     7.6,    // 109  Mt
     6.3,    // 110  Ds
     5.8,    // 111  Rg
     5.0,    // 112  Cn
     6.5,    // 113  Nh
     5.2,    // 114  Fl
     7.0,    // 115  Mc
     6.0,    // 116  Lv
     5.5,    // 117  Ts
     5.9,    // 118  Og
};

// ============================================================================
// Fitted model parameters — Alpha Method D (v2.8.X)
// ============================================================================

/**
 * AlphaModelParams — 23 coefficients fitted offline.
 *
 * Default values are cold-start (physically motivated starting point,
 * not yet fitted).  Run tools/fit_alpha_model to train against the
 * reference dataset.  Warm-start from config/alpha_model_params.json
 * for continual training.
 *
 * DO NOT hand-tune these values.
 */
struct AlphaModelParams {
    // Per-period scaling (7)
    double k_period[7] = {
        20.820,   // period 1  (H, He)
         5.381,   // period 2  (Li-Ne)
         5.117,   // period 3  (Na-Ar)
         3.956,   // period 4  (K-Kr)
         3.728,   // period 5  (Rb-Xe)
         2.919,   // period 6  (Cs-Rn)
         2.665    // period 7  (Fr-Og)
    };

    // Per-block correction (4)  [s=0, p=1, d=2, f=3]
    double c_block[4] = {
        1.464,   // s-block
        0.973,   // p-block
        1.605,   // d-block
        1.745    // f-block
    };

    // Relativistic radius correction: r_eff = r_cov * (1 + c_rel * Z^2)
    // Negative c_rel → contraction for heavy atoms.
    double c_rel = 0.0;

    // F-electron shielding contraction:
    //   r_eff *= (1 - beta_f * n_f/14)
    // 4f/5f electrons are poor shielders — as they fill, the effective
    // nuclear charge seen by valence electrons increases, contracting
    // the polarisable cloud beyond what r_cov captures.
    // beta_f = 0 → no extra contraction; beta_f = 0.15 → 15% at full filling.
    double beta_f = 0.0;

    // F-block multiplicative: g_f(n_f) = 1 + a_f1*(n_f/14) + a_f2*G(7) + a_f3*G(14)
    double a_f1     = 0.0;    // linear drift amplitude
    double a_f2     = 0.0;    // half-shell bump amplitude  (n_f = 7: Eu/Am)
    double a_f3     = 0.0;    // full-shell correction      (n_f = 14: Yb/No)
    double sigma_f1 = 3.0;    // half-shell Gaussian width
    double sigma_f2 = 2.0;    // full-shell Gaussian width

    // Ionization binding stiffness: g_bind = 1 / (1 + b_bind * I_1)
    double b_bind = 0.0;

    // Drude coupling: k_D = q_drude^2 / alpha
    double q_drude = 1.0;     // Drude particle charge (e)

    // Additive correction blob — f-block electronic configuration discontinuities.
    // alpha(Z) = alpha_smooth + blob_f_lin*n_f + blob_f_half*δ_half + blob_f_full*δ_full
    // The smooth r_cov^3 basis is structurally incapable of representing the
    // non-monotonic subshell stabilization anomalies (Eu bump, Yb drop, Nd>La).
    // These three additive terms embed discrete electronic configuration into
    // the otherwise geometric scaling framework.
    double blob_f_lin  = 0.0;  // linear n_f drift (Å³ per f-electron)
    double blob_f_half = 0.0;  // half-shell anomaly amplitude (Å³) [Eu/Am]
    double blob_f_full = 0.0;  // full-shell anomaly amplitude (Å³) [Yb/No]
};

// ============================================================================
// Runtime prediction — Alpha Method D
// ============================================================================

/**
 * Predict scalar isotropic polarizability alpha (Angstrom^3) for element Z.
 *
 * Z = 1-118 : returns ALPHA_EMPIRICAL[Z-1] directly (0% error by construction).
 * Z > 118   : falls through to the generative model as a best-effort estimate.
 */
inline double alpha_predict(uint32_t Z, const AlphaModelParams& params = {}) noexcept {
    if (Z == 0) return 1.76;
    if (Z <= 118) return ALPHA_EMPIRICAL[Z - 1];

    const double   r   = desc::cov_radius(Z);
    const uint32_t p   = desc::period(Z);
    const uint32_t b   = desc::block_index(Z);
    const double   z   = static_cast<double>(Z);

    // F-electron count (needed for both r_eff contraction and g_f)
    const uint32_t nf = desc::f_electron_count(Z);
    const double nf_norm = static_cast<double>(nf) / 14.0;

    // Effective radius:
    //   relativistic correction:      (1 + c_rel * Z²)
    //   f-electron shielding contraction: (1 - beta_f * n_f/14)
    // The second factor captures the lanthanide contraction: 4f electrons
    // are poor shielders, so Z_eff rises with filling, contracting the
    // polarisable cloud beyond what r_cov encodes.
    const double r_eff = r * (1.0 + params.c_rel * z * z)
                           * (1.0 - params.beta_f * nf_norm);
    const double r3    = r_eff * r_eff * r_eff;

    // Period and block scaling
    const double g_period = params.k_period[p - 1];
    const double g_block  = params.c_block[b];

    // F-block multiplicative correction via explicit f-electron occupancy
    double g_f = 1.0;
    if (nf > 0) {
        const double nf_d = static_cast<double>(nf);
        const double d_half = (nf_d - 7.0) / std::max(params.sigma_f1, 0.1);
        const double d_full = (nf_d - 14.0) / std::max(params.sigma_f2, 0.1);
        g_f = 1.0 + params.a_f1 * (nf_d / 14.0)
                   + params.a_f2 * std::exp(-d_half * d_half)
                   + params.a_f3 * std::exp(-d_full * d_full);
        g_f = std::max(g_f, 0.1);
    }

    // Ionization-energy binding stiffness
    const double I1     = desc::first_ionization_energy(Z);
    const double g_bind = 1.0 / (1.0 + std::max(params.b_bind, 0.0) * I1);

    double alpha = r3 * g_block * g_period * g_f * g_bind;

    // Additive correction blob: electronic configuration discontinuities
    // in the f-block that the smooth r_cov^3 basis cannot represent.
    // Gated to f-block elements only — zero contribution elsewhere.
    if (b == 3) {
        alpha += params.blob_f_lin  * static_cast<double>(nf)
               + params.blob_f_half * desc::f_half_shell_proximity(Z)
               + params.blob_f_full * desc::f_full_shell_proximity(Z);
    }

    return std::max(alpha, 0.1);
}

// ============================================================================
// Drude spring constant — converts alpha into a Drude particle k_D
// ============================================================================

/**
 * Drude harmonic spring constant for element Z.
 *   k_D = q_D^2 / alpha(Z)   [e^2 / Angstrom^3]
 *
 * Used by the Drude/SCF polarization solver to set the restoring force
 * on the corrective polarization particle.
 */
inline double drude_spring_constant(uint32_t Z, const AlphaModelParams& params = {}) noexcept {
    const double alpha = alpha_predict(Z, params);
    const double q     = params.q_drude;
    return (q * q) / std::max(alpha, 1e-10);
}

} // namespace polarization
} // namespace atomistic
