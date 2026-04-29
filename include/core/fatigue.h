/**
 * @file fatigue.h
 * @brief S-N fatigue analysis library — ISO/Shigley conventions
 *
 * Covers:
 *   1. Loading type classification (reversed, repeated, fluctuating)
 *   2. Stress ratio R
 *   3. Endurance limit estimation (Se' from Sut, with cap)
 *   4. Marin factors (ka, kb, kc, kd, ke, kf)
 *   5. Fatigue stress concentration (Kf from Kt and q)
 *   6. Mean stress correction (Goodman, Soderberg, Gerber)
 *   7. Finite-life S-N curve (a, b, Sf at N cycles)
 *   8. Combined loading (von Mises equivalent for fatigue)
 *
 * Pure C11.  No C++ dependencies.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef FATIGUE_H
#define FATIGUE_H

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================= */
/*  1. Loading Types                                                          */
/* ========================================================================= */

typedef enum {
    FATIGUE_LOAD_REVERSED    = 0,   /* sigma_m = 0, R = -1                  */
    FATIGUE_LOAD_REPEATED    = 1,   /* sigma_min = 0, R = 0                 */
    FATIGUE_LOAD_FLUCTUATING = 2    /* general case                         */
} FatigueLoadType;

/**
 * Decompose max/min stress into mean and alternating components.
 *
 *   sigma_m = (sigma_max + sigma_min) / 2
 *   sigma_a = (sigma_max - sigma_min) / 2
 *
 * Returns 0 on success, -1 if sigma_max < sigma_min.
 */
int fatigue_stress_components(double sigma_max, double sigma_min,
                              double *sigma_m, double *sigma_a);

/**
 * Classify loading type from max/min stress.
 *
 *   R = -1  ->  REVERSED
 *   R =  0  ->  REPEATED
 *   else    ->  FLUCTUATING
 *
 * Tolerance of 1e-9 used for floating-point comparison.
 */
FatigueLoadType fatigue_classify_loading(double sigma_max, double sigma_min);

/** Human-readable name: "Reversed" / "Repeated" / "Fluctuating". */
const char *fatigue_load_type_str(FatigueLoadType t);

/* ========================================================================= */
/*  2. Stress Ratio                                                           */
/* ========================================================================= */

/**
 * Compute stress ratio R = sigma_min / sigma_max.
 *
 * Returns R, or 0.0 if sigma_max == 0 (degenerate).
 */
double fatigue_stress_ratio(double sigma_max, double sigma_min);

/* ========================================================================= */
/*  3. Endurance Limit Estimation                                             */
/* ========================================================================= */

/**
 * Estimate uncorrected rotating-beam endurance limit Se' from Sut.
 *
 *   Se' = 0.5 * Sut       for Sut <= 1400 MPa
 *   Se' = 700 MPa         for Sut >  1400 MPa  (cap)
 *
 * Returns Se' in MPa, or -1.0 if Sut <= 0.
 */
double fatigue_sep_estimate(double Sut_mpa);

/* ========================================================================= */
/*  4. Marin Factors                                                          */
/* ========================================================================= */

/**
 * Surface finish conditions for ka calculation.
 */
typedef enum {
    MARIN_SURFACE_GROUND     = 0,
    MARIN_SURFACE_MACHINED   = 1,   /* also cold-drawn */
    MARIN_SURFACE_HOT_ROLLED = 2,
    MARIN_SURFACE_AS_FORGED  = 3
} MarinSurfaceFinish;

/**
 * Compute ka (surface factor) from Sut and surface finish.
 *
 *   ka = a * Sut^b
 *
 * where (a, b) come from the Shigley table (Sut in MPa):
 *
 *   Ground:       a = 1.58,   b = -0.085
 *   Machined:     a = 4.51,   b = -0.265
 *   Hot-rolled:   a = 57.7,   b = -0.718
 *   As-forged:    a = 272.0,  b = -0.995
 *
 * Returns ka, or -1.0 on error.
 */
double fatigue_marin_ka(double Sut_mpa, MarinSurfaceFinish finish);

/**
 * Compute kb (size factor) for round rotating bar in bending/torsion.
 *
 *   d <= 2.79 mm:   kb = 1.0
 *   2.79 < d <= 51: kb = 1.24 * d^(-0.107)
 *   51 < d <= 254:  kb = 1.51 * d^(-0.157)
 *
 * For axial loading, kb = 1.0 regardless.
 * d_mm = diameter in mm.  Returns kb, or -1.0 if d_mm <= 0.
 */
double fatigue_marin_kb(double d_mm);

/**
 * Load factor kc.
 *
 *   Bending:  1.0
 *   Axial:    0.85
 *   Torsion:  0.59
 */
typedef enum {
    MARIN_LOAD_BENDING = 0,
    MARIN_LOAD_AXIAL   = 1,
    MARIN_LOAD_TORSION = 2
} MarinLoadType;

double fatigue_marin_kc(MarinLoadType load);

/**
 * Temperature factor kd.
 *
 *   kd = 0.975 + 0.432e-3*T - 0.115e-5*T^2 + 0.104e-8*T^3 - 0.595e-12*T^4
 *
 * T in degrees Celsius.  Valid roughly for T in [20, 550].
 * For T <= 20 C, returns 1.0.
 */
double fatigue_marin_kd(double temp_c);

/**
 * Reliability factor ke.
 *
 * Table lookup (Shigley Table 6-5 style):
 *
 *   50%  -> 1.000
 *   90%  -> 0.897
 *   95%  -> 0.868
 *   99%  -> 0.814
 *   99.9% -> 0.753
 *   99.99% -> 0.702
 *   99.999% -> 0.659
 *   99.9999% -> 0.620
 *
 * Accepts reliability as a fraction (0.50 to 0.999999).
 * Returns the nearest tabulated ke, or -1.0 if out of range.
 */
double fatigue_marin_ke(double reliability);

/**
 * Compute corrected endurance limit Se from all Marin factors.
 *
 *   Se = ka * kb * kc * kd * ke * kf * Se'
 *
 * kf is the miscellaneous factor (user-supplied, typically 1.0).
 */
double fatigue_marin_Se(double Sep, double ka, double kb, double kc,
                        double kd, double ke, double kf);

/* ========================================================================= */
/*  5. Fatigue Stress Concentration                                           */
/* ========================================================================= */

/**
 * Fatigue stress concentration factor Kf.
 *
 *   Kf = 1 + q * (Kt - 1)
 *
 * Kt = theoretical SCF from geometry charts.
 * q  = notch sensitivity (0..1) from material/notch-radius curves.
 *
 * Returns Kf, or -1.0 if Kt < 1.0 or q outside [0, 1].
 */
double fatigue_Kf(double Kt, double q);

/**
 * Fatigue stress concentration factor for torsion Kfs.
 *
 *   Kfs = 1 + qs * (Kts - 1)
 */
double fatigue_Kfs(double Kts, double qs);

/* ========================================================================= */
/*  6. Mean Stress Correction (Safety Factor)                                 */
/* ========================================================================= */

/**
 * Mean stress correction methods.
 */
typedef enum {
    FATIGUE_GOODMAN   = 0,
    FATIGUE_SODERBERG = 1,
    FATIGUE_GERBER    = 2
} FatigueMeanStressMethod;

/**
 * Compute factor of safety n using the specified method.
 *
 * Goodman:   sigma_a/Se + sigma_m/Sut = 1/n
 * Soderberg: sigma_a/Se + sigma_m/Sy  = 1/n
 * Gerber:    sigma_a/Se + (sigma_m/Sut)^2 = 1/n
 *
 * For Gerber, the quadratic is solved as:
 *   n = (1/2) * (Sut/sigma_m)^2 * (sigma_a/Se)
 *       * [-1 + sqrt(1 + (2*sigma_m*Se/(Sut^2*sigma_a/Se)))]
 *   ... or more practically via the standard rearrangement.
 *
 * Sut = ultimate tensile strength [MPa]
 * Sy  = yield strength [MPa] (only needed for Soderberg; pass 0 otherwise)
 * Se  = corrected endurance limit [MPa]
 * sigma_a = alternating stress [MPa]
 * sigma_m = mean stress [MPa]
 *
 * Returns n (safety factor), or -1.0 on invalid input.
 * For pure alternating (sigma_m = 0), returns Se / sigma_a for all methods.
 */
double fatigue_safety_factor(FatigueMeanStressMethod method,
                             double Sut, double Sy, double Se,
                             double sigma_a, double sigma_m);

/** Human-readable name: "Goodman" / "Soderberg" / "Gerber". */
const char *fatigue_method_str(FatigueMeanStressMethod m);

/* ========================================================================= */
/*  7. S-N Finite Life                                                        */
/* ========================================================================= */

/**
 * S-N curve parameters from two anchor points.
 *
 * Anchor 1: S1 = f * Sut  at  N1 = 10^3 cycles
 * Anchor 2: S2 = Se       at  N2 = 10^6 cycles
 *
 *   b = log(S2/S1) / log(N2/N1)
 *   a = S1 / N1^b
 *
 * Returns 0 on success, populates *a_out and *b_out.
 * Returns -1 if any input is invalid.
 */
int fatigue_sn_constants(double f_Sut, double Se,
                         double *a_out, double *b_out);

/**
 * Fatigue strength Sf at N cycles.
 *
 *   Sf = a * N^b
 *
 * where a, b from fatigue_sn_constants().
 */
double fatigue_Sf(double a, double b, double N);

/**
 * Cycles to failure from alternating stress.
 *
 *   N = (sigma_a / a)^(1/b)
 */
double fatigue_cycles(double a, double b, double sigma_a);

/* ========================================================================= */
/*  8. Combined Loading (von Mises for Fatigue)                               */
/* ========================================================================= */

/**
 * Distortion-energy equivalent stress for alternating component.
 *
 *   sigma_a' = sqrt(sigma_a^2 + 3 * tau_a^2)
 */
double fatigue_vonmises_alt(double sigma_a, double tau_a);

/**
 * Distortion-energy equivalent stress for mean component.
 *
 *   sigma_m' = sqrt(sigma_m^2 + 3 * tau_m^2)
 */
double fatigue_vonmises_mean(double sigma_m, double tau_m);

/* ========================================================================= */
/*  Full Pipeline (convenience)                                               */
/* ========================================================================= */

/**
 * Complete fatigue analysis result.
 */
typedef struct {
    /* Loading decomposition */
    double sigma_max;
    double sigma_min;
    double sigma_m;
    double sigma_a;
    double R;
    FatigueLoadType load_type;

    /* Material / endurance */
    double Sut;
    double Sy;
    double Sep;             /* Se' (uncorrected)                    */
    double Se;              /* Se  (Marin-corrected)                */

    /* Marin factors used */
    double ka, kb, kc, kd, ke, kf;

    /* Stress concentration */
    double Kf;

    /* S-N curve */
    double f;               /* fatigue strength fraction (0.9)      */
    double sn_a;            /* coefficient a                        */
    double sn_b;            /* exponent b (negative)                */
    double Sf;              /* fatigue strength at N cycles          */
    double N;               /* cycles to failure                    */

    /* Mean stress safety factors */
    double n_goodman;
    double n_soderberg;
    double n_gerber;
} FatigueAnalysis;

/**
 * Print a complete fatigue analysis to the given FILE stream.
 */
void fatigue_print(const FatigueAnalysis *a, void *stream);

#ifdef __cplusplus
}
#endif

#endif /* FATIGUE_H */
