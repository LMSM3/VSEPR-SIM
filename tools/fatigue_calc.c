/**
 * @file fatigue_calc.c
 * @brief S-N fatigue life calculator — command-line tool
 *
 * Cross-check for FATIGUE.ti and full-pipeline demonstration.
 * Links against vsepr_fatigue library.
 *
 * Usage:
 *   fatigue_calc <Sut> <Sep> <ka> <kb> <kc> <sigma_a>
 *   fatigue_calc default
 *   fatigue_calc full <Sut> <Sy> <sigma_max> <sigma_min> <tau_a> <tau_m> \
 *                     <surface> <d_mm> <load> <reliability> <Kt> <q>
 *
 * SPDX-License-Identifier: MIT
 */

#include "core/fatigue.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Simple mode (matches FATIGUE.ti pipeline exactly)                 */
/* ------------------------------------------------------------------ */

static int run_simple(double Sut, double Sep, double ka, double kb,
                      double kc, double sigma_a)
{
    /* Marin correction (3 factors only, matching TI-84 program) */
    double Se = ka * kb * kc * Sep;
    double f  = 0.9;

    double sn_a, sn_b;
    int rc = fatigue_sn_constants(f * Sut, Se, &sn_a, &sn_b);
    if (rc != 0) {
        fprintf(stderr, "Error: invalid S-N parameters\n");
        return 2;
    }

    double N = fatigue_cycles(sn_a, sn_b, sigma_a);

    printf("=== FATIGUE S-N RESULTS ===\n\n");
    printf("Input:\n");
    printf("  Sut      = %.2f MPa\n",  Sut);
    printf("  Se'      = %.2f MPa\n",  Sep);
    printf("  ka       = %.4f\n",       ka);
    printf("  kb       = %.4f\n",       kb);
    printf("  kc       = %.4f\n",       kc);
    printf("  sigma_a  = %.2f MPa\n\n", sigma_a);
    printf("Computed:\n");
    printf("  Se (corrected) = %.4f MPa\n", Se);
    printf("  f              = %.2f\n",      f);
    printf("  a (coeff)      = %.4f\n",      sn_a);
    printf("  b (exponent)   = %.6f\n",      sn_b);
    printf("  N (cycles)     = %.1f\n\n",    N);

    if (N >= 1.0e6)      printf(">> INFINITE LIFE (N >= 1E6)\n");
    else if (N < 1000.0) printf(">> LOW-CYCLE — use strain-life (E-N) method\n");
    else                 printf(">> FINITE LIFE\n");

    return 0;
}

/* ------------------------------------------------------------------ */
/*  Full mode (all 8 sections of fatigue analysis)                    */
/* ------------------------------------------------------------------ */

static int run_full(int argc, char *argv[])
{
    if (argc != 14) {
        fprintf(stderr, "full mode requires 12 arguments after 'full'\n");
        return 1;
    }

    double Sut       = atof(argv[2]);
    double Sy        = atof(argv[3]);
    double sigma_max = atof(argv[4]);
    double sigma_min = atof(argv[5]);
    double tau_a_in  = atof(argv[6]);
    double tau_m_in  = atof(argv[7]);
    int    surface   = atoi(argv[8]);   /* 0-3 */
    double d_mm      = atof(argv[9]);
    int    load      = atoi(argv[10]);  /* 0-2 */
    double reliab    = atof(argv[11]);
    double Kt        = atof(argv[12]);
    double q         = atof(argv[13]);

    FatigueAnalysis a;
    memset(&a, 0, sizeof(a));

    /* Material */
    a.Sut = Sut;
    a.Sy  = Sy;
    a.Sep = fatigue_sep_estimate(Sut);

    /* Loading */
    a.sigma_max = sigma_max;
    a.sigma_min = sigma_min;
    fatigue_stress_components(sigma_max, sigma_min, &a.sigma_m, &a.sigma_a);
    a.R = fatigue_stress_ratio(sigma_max, sigma_min);
    a.load_type = fatigue_classify_loading(sigma_max, sigma_min);

    /* Marin factors */
    a.ka = fatigue_marin_ka(Sut, (MarinSurfaceFinish)surface);
    a.kb = fatigue_marin_kb(d_mm);
    a.kc = fatigue_marin_kc((MarinLoadType)load);
    a.kd = fatigue_marin_kd(20.0);  /* room temperature */
    a.ke = fatigue_marin_ke(reliab);
    a.kf = 1.0;
    a.Se = fatigue_marin_Se(a.Sep, a.ka, a.kb, a.kc, a.kd, a.ke, a.kf);

    /* Stress concentration */
    a.Kf = fatigue_Kf(Kt, q);

    /* Apply Kf, then von Mises combination */
    double sa_eff = a.Kf * a.sigma_a;
    double sa_vm  = fatigue_vonmises_alt(sa_eff, tau_a_in);
    double sm_vm  = fatigue_vonmises_mean(a.sigma_m, tau_m_in);

    /* S-N */
    a.f = 0.9;
    fatigue_sn_constants(a.f * Sut, a.Se, &a.sn_a, &a.sn_b);
    a.N  = fatigue_cycles(a.sn_a, a.sn_b, sa_vm);
    a.Sf = fatigue_Sf(a.sn_a, a.sn_b, a.N);

    /* Safety factors */
    a.n_goodman   = fatigue_safety_factor(FATIGUE_GOODMAN, Sut, Sy, a.Se,
                                          sa_vm, sm_vm);
    a.n_soderberg = fatigue_safety_factor(FATIGUE_SODERBERG, Sut, Sy, a.Se,
                                          sa_vm, sm_vm);
    a.n_gerber    = fatigue_safety_factor(FATIGUE_GERBER, Sut, Sy, a.Se,
                                          sa_vm, sm_vm);

    fatigue_print(&a, stdout);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  main                                                              */
/* ------------------------------------------------------------------ */

int main(int argc, char *argv[])
{
    if (argc == 2 && strcmp(argv[1], "default") == 0) {
        printf("Using mild-steel defaults (sigma_a = 150 MPa)\n\n");
        return run_simple(400.0, 200.0, 0.70, 0.85, 0.897, 150.0);
    }
    if (argc == 7 && strcmp(argv[1], "full") != 0) {
        return run_simple(atof(argv[1]), atof(argv[2]), atof(argv[3]),
                          atof(argv[4]), atof(argv[5]), atof(argv[6]));
    }
    if (argc >= 3 && strcmp(argv[1], "full") == 0) {
        return run_full(argc, argv);
    }

    fprintf(stderr,
        "Usage:\n"
        "  fatigue_calc <Sut> <Sep> <ka> <kb> <kc> <sigma_a>\n"
        "  fatigue_calc default\n"
        "  fatigue_calc full <Sut> <Sy> <sig_max> <sig_min> <tau_a> <tau_m>\n"
        "                    <surface:0-3> <d_mm> <load:0-2> <reliability>\n"
        "                    <Kt> <q>\n\n"
        "Surface: 0=ground, 1=machined, 2=hot-rolled, 3=as-forged\n"
        "Load:    0=bending, 1=axial, 2=torsion\n\n"
        "Example:\n"
        "  fatigue_calc 400 200 0.70 0.85 0.897 150\n"
        "  fatigue_calc full 400 300 300 100 40 20 1 25 0 0.90 2.0 0.85\n");
    return 1;
}
