/**
 * @file fatigue.c
 * @brief S-N fatigue analysis library — implementation
 *
 * All formulas follow Shigley's Mechanical Engineering Design
 * and ISO fatigue conventions.
 *
 * SPDX-License-Identifier: MIT
 */

#include "core/fatigue.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <float.h>

/* ========================================================================= */
/*  Internal constants                                                        */
/* ========================================================================= */

#define FATIGUE_TOL 1.0e-9

/* Marin ka coefficients: { a, b } indexed by MarinSurfaceFinish.
 * Sut in MPa.  ka = a * Sut^b.
 * Source: Shigley Table 6-2 (SI units). */
static const double KA_TABLE[][2] = {
    /* GROUND       */ { 1.58,    -0.085 },
    /* MACHINED     */ { 4.51,    -0.265 },
    /* HOT_ROLLED   */ { 57.7,    -0.718 },
    /* AS_FORGED    */ { 272.0,   -0.995 }
};

/* Reliability ke table: { reliability_fraction, ke }.
 * Source: Shigley Table 6-5. */
static const double KE_TABLE[][2] = {
    { 0.50,     1.000 },
    { 0.90,     0.897 },
    { 0.95,     0.868 },
    { 0.99,     0.814 },
    { 0.999,    0.753 },
    { 0.9999,   0.702 },
    { 0.99999,  0.659 },
    { 0.999999, 0.620 }
};
#define KE_TABLE_SIZE (sizeof(KE_TABLE) / sizeof(KE_TABLE[0]))

/* ========================================================================= */
/*  1. Loading Types                                                          */
/* ========================================================================= */

int fatigue_stress_components(double sigma_max, double sigma_min,
                              double *sigma_m, double *sigma_a)
{
    if (sigma_max < sigma_min) return -1;
    *sigma_m = (sigma_max + sigma_min) / 2.0;
    *sigma_a = (sigma_max - sigma_min) / 2.0;
    return 0;
}

FatigueLoadType fatigue_classify_loading(double sigma_max, double sigma_min)
{
    if (sigma_max < sigma_min) return FATIGUE_LOAD_FLUCTUATING;

    double R = (fabs(sigma_max) > FATIGUE_TOL)
             ? sigma_min / sigma_max
             : 0.0;

    if (fabs(R - (-1.0)) < FATIGUE_TOL)
        return FATIGUE_LOAD_REVERSED;
    if (fabs(R) < FATIGUE_TOL && sigma_min >= 0.0)
        return FATIGUE_LOAD_REPEATED;
    return FATIGUE_LOAD_FLUCTUATING;
}

const char *fatigue_load_type_str(FatigueLoadType t)
{
    switch (t) {
    case FATIGUE_LOAD_REVERSED:    return "Reversed";
    case FATIGUE_LOAD_REPEATED:    return "Repeated";
    case FATIGUE_LOAD_FLUCTUATING: return "Fluctuating";
    }
    return "Unknown";
}

/* ========================================================================= */
/*  2. Stress Ratio                                                           */
/* ========================================================================= */

double fatigue_stress_ratio(double sigma_max, double sigma_min)
{
    if (fabs(sigma_max) < FATIGUE_TOL) return 0.0;
    return sigma_min / sigma_max;
}

/* ========================================================================= */
/*  3. Endurance Limit Estimation                                             */
/* ========================================================================= */

double fatigue_sep_estimate(double Sut_mpa)
{
    if (Sut_mpa <= 0.0) return -1.0;
    if (Sut_mpa <= 1400.0) return 0.5 * Sut_mpa;
    return 700.0;
}

/* ========================================================================= */
/*  4. Marin Factors                                                          */
/* ========================================================================= */

double fatigue_marin_ka(double Sut_mpa, MarinSurfaceFinish finish)
{
    if (Sut_mpa <= 0.0) return -1.0;
    if (finish < MARIN_SURFACE_GROUND || finish > MARIN_SURFACE_AS_FORGED)
        return -1.0;
    double a = KA_TABLE[finish][0];
    double b = KA_TABLE[finish][1];
    return a * pow(Sut_mpa, b);
}

double fatigue_marin_kb(double d_mm)
{
    if (d_mm <= 0.0) return -1.0;
    if (d_mm <= 2.79) return 1.0;
    if (d_mm <= 51.0) return 1.24 * pow(d_mm, -0.107);
    if (d_mm <= 254.0) return 1.51 * pow(d_mm, -0.157);
    return -1.0;   /* out of range */
}

double fatigue_marin_kc(MarinLoadType load)
{
    switch (load) {
    case MARIN_LOAD_BENDING: return 1.0;
    case MARIN_LOAD_AXIAL:   return 0.85;
    case MARIN_LOAD_TORSION: return 0.59;
    }
    return -1.0;
}

double fatigue_marin_kd(double temp_c)
{
    if (temp_c <= 20.0) return 1.0;
    double T  = temp_c;
    double T2 = T * T;
    double T3 = T2 * T;
    double T4 = T3 * T;
    double kd = 0.975 + 0.432e-3 * T - 0.115e-5 * T2
                + 0.104e-8 * T3 - 0.595e-12 * T4;
    if (kd < 0.0) kd = 0.0;
    return kd;
}

double fatigue_marin_ke(double reliability)
{
    if (reliability < 0.50 || reliability > 0.999999)
        return -1.0;

    /* Find nearest table entry */
    double best_dist = 2.0;
    double best_ke   = 1.0;
    for (size_t i = 0; i < KE_TABLE_SIZE; i++) {
        double dist = fabs(reliability - KE_TABLE[i][0]);
        if (dist < best_dist) {
            best_dist = dist;
            best_ke   = KE_TABLE[i][1];
        }
    }
    return best_ke;
}

double fatigue_marin_Se(double Sep, double ka, double kb, double kc,
                        double kd, double ke, double kf)
{
    return ka * kb * kc * kd * ke * kf * Sep;
}

/* ========================================================================= */
/*  5. Fatigue Stress Concentration                                           */
/* ========================================================================= */

double fatigue_Kf(double Kt, double q)
{
    if (Kt < 1.0 || q < 0.0 || q > 1.0) return -1.0;
    return 1.0 + q * (Kt - 1.0);
}

double fatigue_Kfs(double Kts, double qs)
{
    if (Kts < 1.0 || qs < 0.0 || qs > 1.0) return -1.0;
    return 1.0 + qs * (Kts - 1.0);
}

/* ========================================================================= */
/*  6. Mean Stress Correction                                                 */
/* ========================================================================= */

const char *fatigue_method_str(FatigueMeanStressMethod m)
{
    switch (m) {
    case FATIGUE_GOODMAN:   return "Goodman";
    case FATIGUE_SODERBERG: return "Soderberg";
    case FATIGUE_GERBER:    return "Gerber";
    }
    return "Unknown";
}

double fatigue_safety_factor(FatigueMeanStressMethod method,
                             double Sut, double Sy, double Se,
                             double sigma_a, double sigma_m)
{
    if (Se <= 0.0 || sigma_a < 0.0) return -1.0;
    if (sigma_a < FATIGUE_TOL) return 1.0e30;  /* no alternating stress */

    /* Pure alternating (sigma_m = 0): all methods reduce to Se/sigma_a */
    if (fabs(sigma_m) < FATIGUE_TOL)
        return Se / sigma_a;

    switch (method) {
    case FATIGUE_GOODMAN:
        if (Sut <= 0.0) return -1.0;
        return 1.0 / (sigma_a / Se + sigma_m / Sut);

    case FATIGUE_SODERBERG:
        if (Sy <= 0.0) return -1.0;
        return 1.0 / (sigma_a / Se + sigma_m / Sy);

    case FATIGUE_GERBER: {
        if (Sut <= 0.0) return -1.0;
        /* n*sigma_a/Se + (n*sigma_m/Sut)^2 = 1
         * Let A = sigma_a/Se, B = (sigma_m/Sut)^2
         * B*n^2 + A*n - 1 = 0
         * n = (-A + sqrt(A^2 + 4*B)) / (2*B)                           */
        double A = sigma_a / Se;
        double B = (sigma_m * sigma_m) / (Sut * Sut);
        if (B < FATIGUE_TOL) return Se / sigma_a;
        double disc = A * A + 4.0 * B;
        return (-A + sqrt(disc)) / (2.0 * B);
    }
    }
    return -1.0;
}

/* ========================================================================= */
/*  7. S-N Finite Life                                                        */
/* ========================================================================= */

int fatigue_sn_constants(double f_Sut, double Se,
                         double *a_out, double *b_out)
{
    if (f_Sut <= 0.0 || Se <= 0.0) return -1;
    if (f_Sut <= Se) return -1;  /* S1 must exceed S2 for valid curve */

    /* S1 = f*Sut at N1 = 1e3,  S2 = Se at N2 = 1e6 */
    double S1 = f_Sut;
    double S2 = Se;
    double N1 = 1.0e3;
    double N2 = 1.0e6;

    *b_out = log10(S2 / S1) / log10(N2 / N1);
    *a_out = S1 / pow(N1, *b_out);
    return 0;
}

double fatigue_Sf(double a, double b, double N)
{
    if (a <= 0.0 || N <= 0.0) return -1.0;
    return a * pow(N, b);
}

double fatigue_cycles(double a, double b, double sigma_a)
{
    if (a <= 0.0 || sigma_a <= 0.0 || fabs(b) < FATIGUE_TOL) return -1.0;
    return pow(sigma_a / a, 1.0 / b);
}

/* ========================================================================= */
/*  8. Combined Loading                                                       */
/* ========================================================================= */

double fatigue_vonmises_alt(double sigma_a, double tau_a)
{
    return sqrt(sigma_a * sigma_a + 3.0 * tau_a * tau_a);
}

double fatigue_vonmises_mean(double sigma_m, double tau_m)
{
    return sqrt(sigma_m * sigma_m + 3.0 * tau_m * tau_m);
}

/* ========================================================================= */
/*  Printing                                                                  */
/* ========================================================================= */

void fatigue_print(const FatigueAnalysis *a, void *stream)
{
    FILE *fp = (FILE *)stream;

    fprintf(fp, "\n=== FATIGUE ANALYSIS ===\n\n");

    /* Loading */
    fprintf(fp, "Loading:\n");
    fprintf(fp, "  sigma_max = %+.2f MPa\n", a->sigma_max);
    fprintf(fp, "  sigma_min = %+.2f MPa\n", a->sigma_min);
    fprintf(fp, "  sigma_m   = %+.2f MPa  (mean)\n", a->sigma_m);
    fprintf(fp, "  sigma_a   = %+.2f MPa  (alternating)\n", a->sigma_a);
    fprintf(fp, "  R         = %.4f\n", a->R);
    fprintf(fp, "  Type      = %s\n\n", fatigue_load_type_str(a->load_type));

    /* Material */
    fprintf(fp, "Material:\n");
    fprintf(fp, "  Sut = %.2f MPa\n", a->Sut);
    fprintf(fp, "  Sy  = %.2f MPa\n", a->Sy);
    fprintf(fp, "  Se' = %.2f MPa  (uncorrected)\n", a->Sep);
    fprintf(fp, "  Se  = %.4f MPa  (Marin-corrected)\n\n", a->Se);

    /* Marin factors */
    fprintf(fp, "Marin Factors:\n");
    fprintf(fp, "  ka (surface)     = %.4f\n", a->ka);
    fprintf(fp, "  kb (size)        = %.4f\n", a->kb);
    fprintf(fp, "  kc (load)        = %.4f\n", a->kc);
    fprintf(fp, "  kd (temperature) = %.4f\n", a->kd);
    fprintf(fp, "  ke (reliability) = %.4f\n", a->ke);
    fprintf(fp, "  kf (misc)        = %.4f\n\n", a->kf);

    /* Stress concentration */
    if (a->Kf > 1.0 + FATIGUE_TOL) {
        fprintf(fp, "Stress Concentration:\n");
        fprintf(fp, "  Kf = %.4f\n\n", a->Kf);
    }

    /* S-N curve */
    fprintf(fp, "S-N Curve (10^3 to 10^6):\n");
    fprintf(fp, "  f  = %.2f\n", a->f);
    fprintf(fp, "  a  = %.4f\n", a->sn_a);
    fprintf(fp, "  b  = %.6f\n", a->sn_b);
    fprintf(fp, "  Sf = %.4f MPa  (at N)\n", a->Sf);
    fprintf(fp, "  N  = %.1f cycles\n\n", a->N);

    if (a->N >= 1.0e6)
        fprintf(fp, "  >> INFINITE LIFE (N >= 1E6)\n\n");
    else if (a->N < 1000.0)
        fprintf(fp, "  >> LOW-CYCLE — use strain-life (E-N) method\n\n");
    else
        fprintf(fp, "  >> FINITE LIFE\n\n");

    /* Safety factors */
    fprintf(fp, "Mean Stress Safety Factors (sigma_m = %.2f MPa):\n",
            a->sigma_m);
    fprintf(fp, "  Goodman:   n = %.4f\n", a->n_goodman);
    fprintf(fp, "  Soderberg: n = %.4f\n", a->n_soderberg);
    fprintf(fp, "  Gerber:    n = %.4f\n", a->n_gerber);

    fprintf(fp, "\n");
}
