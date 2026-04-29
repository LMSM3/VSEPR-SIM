/**
 * @file test_fatigue.c
 * @brief Comprehensive tests for the fatigue analysis library.
 *
 * Tests cover all 8 sections:
 *   1. Loading types (reversed, repeated, fluctuating)
 *   2. Stress ratio R
 *   3. Endurance limit estimation
 *   4. Marin factors (ka, kb, kc, kd, ke)
 *   5. Fatigue stress concentration (Kf)
 *   6. Mean stress correction (Goodman, Soderberg, Gerber)
 *   7. S-N finite life (a, b, Sf, N)
 *   8. Combined loading (von Mises for fatigue)
 */

#include "core/fatigue.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_pass = 0;
static int g_fail = 0;

#define TOL 1.0e-3
#define RTOL 1.0e-2  /* 1% relative tolerance for empirical formulas */

#define ASSERT_NEAR(val, expected, tol, msg) do { \
    double _v = (val), _e = (expected), _t = (tol); \
    if (fabs(_v - _e) <= _t) { g_pass++; } \
    else { g_fail++; printf("FAIL [%s]: got %.6f, expected %.6f (tol %.6f)\n", \
           (msg), _v, _e, _t); } \
} while(0)

#define ASSERT_RNEAR(val, expected, rtol, msg) do { \
    double _v = (val), _e = (expected), _r = (rtol); \
    double _t = fabs(_e) * _r + 1e-12; \
    if (fabs(_v - _e) <= _t) { g_pass++; } \
    else { g_fail++; printf("FAIL [%s]: got %.6f, expected %.6f (rtol %.4f)\n", \
           (msg), _v, _e, _r); } \
} while(0)

#define ASSERT_EQ_INT(val, expected, msg) do { \
    int _v = (val), _e = (expected); \
    if (_v == _e) { g_pass++; } \
    else { g_fail++; printf("FAIL [%s]: got %d, expected %d\n", \
           (msg), _v, _e); } \
} while(0)

#define ASSERT_GT(val, threshold, msg) do { \
    double _v = (val), _t = (threshold); \
    if (_v > _t) { g_pass++; } \
    else { g_fail++; printf("FAIL [%s]: got %.6f, expected > %.6f\n", \
           (msg), _v, _t); } \
} while(0)

#define ASSERT_LT(val, threshold, msg) do { \
    double _v = (val), _t = (threshold); \
    if (_v < _t) { g_pass++; } \
    else { g_fail++; printf("FAIL [%s]: got %.6f, expected < %.6f\n", \
           (msg), _v, _t); } \
} while(0)

#define ASSERT_STR(val, expected, msg) do { \
    const char *_v = (val), *_e = (expected); \
    if (strcmp(_v, _e) == 0) { g_pass++; } \
    else { g_fail++; printf("FAIL [%s]: got \"%s\", expected \"%s\"\n", \
           (msg), _v, _e); } \
} while(0)

/* ------------------------------------------------------------------ */
/*  1. Loading Types                                                  */
/* ------------------------------------------------------------------ */

static void test_stress_components(void)
{
    printf("--- stress components ---\n");
    double sm, sa;

    /* Fully reversed: max = +100, min = -100 */
    ASSERT_EQ_INT(fatigue_stress_components(100, -100, &sm, &sa), 0,
                  "reversed return");
    ASSERT_NEAR(sm, 0.0, TOL, "reversed sigma_m");
    ASSERT_NEAR(sa, 100.0, TOL, "reversed sigma_a");

    /* Repeated: max = 200, min = 0 */
    ASSERT_EQ_INT(fatigue_stress_components(200, 0, &sm, &sa), 0,
                  "repeated return");
    ASSERT_NEAR(sm, 100.0, TOL, "repeated sigma_m");
    ASSERT_NEAR(sa, 100.0, TOL, "repeated sigma_a");

    /* Fluctuating: max = 300, min = 100 */
    ASSERT_EQ_INT(fatigue_stress_components(300, 100, &sm, &sa), 0,
                  "fluctuating return");
    ASSERT_NEAR(sm, 200.0, TOL, "fluctuating sigma_m");
    ASSERT_NEAR(sa, 100.0, TOL, "fluctuating sigma_a");

    /* Error: max < min */
    ASSERT_EQ_INT(fatigue_stress_components(50, 200, &sm, &sa), -1,
                  "invalid max<min");
}

static void test_classify_loading(void)
{
    printf("--- classify loading ---\n");
    ASSERT_EQ_INT(fatigue_classify_loading(100, -100),
                  FATIGUE_LOAD_REVERSED, "R=-1 reversed");
    ASSERT_EQ_INT(fatigue_classify_loading(200, 0),
                  FATIGUE_LOAD_REPEATED, "R=0 repeated");
    ASSERT_EQ_INT(fatigue_classify_loading(300, 100),
                  FATIGUE_LOAD_FLUCTUATING, "general fluctuating");
    ASSERT_EQ_INT(fatigue_classify_loading(300, -50),
                  FATIGUE_LOAD_FLUCTUATING, "negative min fluctuating");

    /* String names */
    ASSERT_STR(fatigue_load_type_str(FATIGUE_LOAD_REVERSED), "Reversed",
               "str reversed");
    ASSERT_STR(fatigue_load_type_str(FATIGUE_LOAD_REPEATED), "Repeated",
               "str repeated");
    ASSERT_STR(fatigue_load_type_str(FATIGUE_LOAD_FLUCTUATING), "Fluctuating",
               "str fluctuating");
}

/* ------------------------------------------------------------------ */
/*  2. Stress Ratio                                                   */
/* ------------------------------------------------------------------ */

static void test_stress_ratio(void)
{
    printf("--- stress ratio ---\n");
    ASSERT_NEAR(fatigue_stress_ratio(100, -100), -1.0, TOL,
                "R fully reversed");
    ASSERT_NEAR(fatigue_stress_ratio(200, 0), 0.0, TOL,
                "R repeated");
    ASSERT_NEAR(fatigue_stress_ratio(300, 100), 100.0/300.0, TOL,
                "R fluctuating");
    ASSERT_NEAR(fatigue_stress_ratio(0, 0), 0.0, TOL,
                "R degenerate zero");
}

/* ------------------------------------------------------------------ */
/*  3. Endurance Limit Estimation                                     */
/* ------------------------------------------------------------------ */

static void test_sep_estimate(void)
{
    printf("--- endurance limit estimation ---\n");

    /* Below cap: Se' = 0.5 * Sut */
    ASSERT_NEAR(fatigue_sep_estimate(400.0), 200.0, TOL, "Se' 400 MPa");
    ASSERT_NEAR(fatigue_sep_estimate(1000.0), 500.0, TOL, "Se' 1000 MPa");
    ASSERT_NEAR(fatigue_sep_estimate(1400.0), 700.0, TOL, "Se' 1400 MPa cap boundary");

    /* Above cap: Se' = 700 */
    ASSERT_NEAR(fatigue_sep_estimate(1600.0), 700.0, TOL, "Se' 1600 MPa capped");
    ASSERT_NEAR(fatigue_sep_estimate(2000.0), 700.0, TOL, "Se' 2000 MPa capped");

    /* Invalid */
    ASSERT_LT(fatigue_sep_estimate(0.0), 0.0, "Se' zero Sut invalid");
    ASSERT_LT(fatigue_sep_estimate(-100.0), 0.0, "Se' negative Sut invalid");
}

/* ------------------------------------------------------------------ */
/*  4. Marin Factors                                                  */
/* ------------------------------------------------------------------ */

static void test_marin_ka(void)
{
    printf("--- Marin ka (surface) ---\n");

    /* Ground finish, 400 MPa: ka = 1.58 * 400^(-0.085) */
    double ka_ground = fatigue_marin_ka(400.0, MARIN_SURFACE_GROUND);
    ASSERT_GT(ka_ground, 0.9, "ka ground > 0.9");
    ASSERT_LT(ka_ground, 1.0, "ka ground < 1.0");

    /* Machined, 400 MPa: ka = 4.51 * 400^(-0.265) */
    double ka_mach = fatigue_marin_ka(400.0, MARIN_SURFACE_MACHINED);
    ASSERT_GT(ka_mach, 0.7, "ka machined > 0.7");
    ASSERT_LT(ka_mach, 0.95, "ka machined < 0.95");

    /* Hot-rolled: worse than machined */
    double ka_hr = fatigue_marin_ka(400.0, MARIN_SURFACE_HOT_ROLLED);
    ASSERT_LT(ka_hr, ka_mach, "ka hot-rolled < machined");

    /* As-forged: worst */
    double ka_af = fatigue_marin_ka(400.0, MARIN_SURFACE_AS_FORGED);
    ASSERT_LT(ka_af, ka_hr, "ka as-forged < hot-rolled");

    /* Ordering: ground > machined > hot-rolled > as-forged */
    ASSERT_GT(ka_ground, ka_mach, "ka ordering ground > machined");

    /* Higher Sut -> lower ka (more sensitive to surface) */
    double ka_mach_800 = fatigue_marin_ka(800.0, MARIN_SURFACE_MACHINED);
    ASSERT_LT(ka_mach_800, ka_mach, "ka higher Sut -> lower ka");

    /* Invalid */
    ASSERT_LT(fatigue_marin_ka(0.0, MARIN_SURFACE_GROUND), 0.0,
              "ka zero Sut invalid");
}

static void test_marin_kb(void)
{
    printf("--- Marin kb (size) ---\n");

    /* Tiny: kb = 1.0 */
    ASSERT_NEAR(fatigue_marin_kb(2.0), 1.0, TOL, "kb tiny d=2");

    /* Small: 2.79 < d <= 51 */
    double kb_25 = fatigue_marin_kb(25.0);
    ASSERT_GT(kb_25, 0.8, "kb d=25 > 0.8");
    ASSERT_LT(kb_25, 1.0, "kb d=25 < 1.0");

    /* Larger -> smaller kb */
    double kb_50 = fatigue_marin_kb(50.0);
    ASSERT_LT(kb_50, kb_25, "kb d=50 < kb d=25");

    /* Large: 51 < d <= 254 */
    double kb_100 = fatigue_marin_kb(100.0);
    ASSERT_GT(kb_100, 0.6, "kb d=100 > 0.6");
    ASSERT_LT(kb_100, kb_50, "kb d=100 < kb d=50");

    /* Out of range */
    ASSERT_LT(fatigue_marin_kb(300.0), 0.0, "kb out of range");
    ASSERT_LT(fatigue_marin_kb(0.0), 0.0, "kb zero invalid");
}

static void test_marin_kc(void)
{
    printf("--- Marin kc (load) ---\n");
    ASSERT_NEAR(fatigue_marin_kc(MARIN_LOAD_BENDING), 1.0, TOL, "kc bending");
    ASSERT_NEAR(fatigue_marin_kc(MARIN_LOAD_AXIAL), 0.85, TOL, "kc axial");
    ASSERT_NEAR(fatigue_marin_kc(MARIN_LOAD_TORSION), 0.59, TOL, "kc torsion");
}

static void test_marin_kd(void)
{
    printf("--- Marin kd (temperature) ---\n");

    /* Room temperature or below: kd = 1.0 */
    ASSERT_NEAR(fatigue_marin_kd(20.0), 1.0, TOL, "kd room temp");
    ASSERT_NEAR(fatigue_marin_kd(-10.0), 1.0, TOL, "kd below room");

    /* Moderate temperature: kd close to 1 */
    double kd_100 = fatigue_marin_kd(100.0);
    ASSERT_GT(kd_100, 0.95, "kd 100C > 0.95");
    ASSERT_LT(kd_100, 1.05, "kd 100C < 1.05");

    /* High temperature: kd drops */
    double kd_500 = fatigue_marin_kd(500.0);
    ASSERT_LT(kd_500, kd_100, "kd 500C < kd 100C");
    ASSERT_GT(kd_500, 0.5, "kd 500C > 0.5");
}

static void test_marin_ke(void)
{
    printf("--- Marin ke (reliability) ---\n");
    ASSERT_NEAR(fatigue_marin_ke(0.50), 1.000, TOL, "ke 50%");
    ASSERT_NEAR(fatigue_marin_ke(0.90), 0.897, TOL, "ke 90%");
    ASSERT_NEAR(fatigue_marin_ke(0.95), 0.868, TOL, "ke 95%");
    ASSERT_NEAR(fatigue_marin_ke(0.99), 0.814, TOL, "ke 99%");
    ASSERT_NEAR(fatigue_marin_ke(0.999), 0.753, TOL, "ke 99.9%");

    /* Higher reliability -> lower ke */
    ASSERT_LT(fatigue_marin_ke(0.99), fatigue_marin_ke(0.90),
              "ke 99% < ke 90%");

    /* Out of range */
    ASSERT_LT(fatigue_marin_ke(0.10), 0.0, "ke too low invalid");
}

static void test_marin_Se(void)
{
    printf("--- Marin Se (corrected endurance) ---\n");

    /* Se = ka*kb*kc*kd*ke*kf * Se' */
    double Se = fatigue_marin_Se(200.0, 0.7, 0.85, 1.0, 1.0, 0.897, 1.0);
    /* 0.7 * 0.85 * 1.0 * 1.0 * 0.897 * 1.0 * 200 = 106.743 */
    ASSERT_RNEAR(Se, 106.743, RTOL, "Se corrected mild steel");

    /* All factors = 1.0 -> Se = Se' */
    ASSERT_NEAR(fatigue_marin_Se(200.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0),
                200.0, TOL, "Se all factors unity");
}

/* ------------------------------------------------------------------ */
/*  5. Fatigue Stress Concentration                                   */
/* ------------------------------------------------------------------ */

static void test_stress_concentration(void)
{
    printf("--- stress concentration ---\n");

    /* Kf = 1 + q*(Kt - 1) */
    /* Kt=2.5, q=0.8 -> Kf = 1 + 0.8*1.5 = 2.2 */
    ASSERT_NEAR(fatigue_Kf(2.5, 0.8), 2.2, TOL, "Kf basic");

    /* q=0 -> Kf = 1 (no notch effect) */
    ASSERT_NEAR(fatigue_Kf(3.0, 0.0), 1.0, TOL, "Kf q=0");

    /* q=1 -> Kf = Kt (full notch sensitivity) */
    ASSERT_NEAR(fatigue_Kf(2.5, 1.0), 2.5, TOL, "Kf q=1");

    /* Torsion variant */
    ASSERT_NEAR(fatigue_Kfs(1.8, 0.7), 1.0 + 0.7 * 0.8, TOL, "Kfs torsion");

    /* Invalid */
    ASSERT_LT(fatigue_Kf(0.5, 0.8), 0.0, "Kf Kt<1 invalid");
    ASSERT_LT(fatigue_Kf(2.0, 1.5), 0.0, "Kf q>1 invalid");
    ASSERT_LT(fatigue_Kf(2.0, -0.1), 0.0, "Kf q<0 invalid");
}

/* ------------------------------------------------------------------ */
/*  6. Mean Stress Correction                                         */
/* ------------------------------------------------------------------ */

static void test_mean_stress(void)
{
    printf("--- mean stress correction ---\n");

    double Sut = 400.0, Sy = 300.0, Se = 106.743;
    double sa = 80.0, sm = 50.0;

    /* Goodman: n = 1 / (sa/Se + sm/Sut) */
    double n_g = fatigue_safety_factor(FATIGUE_GOODMAN, Sut, Sy, Se, sa, sm);
    double expected_g = 1.0 / (sa / Se + sm / Sut);
    ASSERT_RNEAR(n_g, expected_g, RTOL, "Goodman n");

    /* Soderberg: n = 1 / (sa/Se + sm/Sy) */
    double n_s = fatigue_safety_factor(FATIGUE_SODERBERG, Sut, Sy, Se, sa, sm);
    double expected_s = 1.0 / (sa / Se + sm / Sy);
    ASSERT_RNEAR(n_s, expected_s, RTOL, "Soderberg n");

    /* Gerber — verify it's between Goodman and pure Se/sigma_a */
    double n_gerb = fatigue_safety_factor(FATIGUE_GERBER, Sut, Sy, Se, sa, sm);
    ASSERT_GT(n_gerb, n_g, "Gerber > Goodman");
    ASSERT_LT(n_gerb, Se / sa, "Gerber < pure alternating");

    /* Ordering: Soderberg < Goodman < Gerber (for ductile, positive sm) */
    ASSERT_LT(n_s, n_g, "Soderberg < Goodman");
    ASSERT_LT(n_g, n_gerb, "Goodman < Gerber");

    /* Pure alternating (sigma_m = 0): all give Se/sigma_a */
    double n_pure = Se / sa;
    ASSERT_RNEAR(fatigue_safety_factor(FATIGUE_GOODMAN, Sut, Sy, Se, sa, 0.0),
                 n_pure, RTOL, "Goodman pure alt");
    ASSERT_RNEAR(fatigue_safety_factor(FATIGUE_SODERBERG, Sut, Sy, Se, sa, 0.0),
                 n_pure, RTOL, "Soderberg pure alt");
    ASSERT_RNEAR(fatigue_safety_factor(FATIGUE_GERBER, Sut, Sy, Se, sa, 0.0),
                 n_pure, RTOL, "Gerber pure alt");

    /* String names */
    ASSERT_STR(fatigue_method_str(FATIGUE_GOODMAN), "Goodman", "str Goodman");
    ASSERT_STR(fatigue_method_str(FATIGUE_SODERBERG), "Soderberg", "str Soderberg");
    ASSERT_STR(fatigue_method_str(FATIGUE_GERBER), "Gerber", "str Gerber");
}

/* ------------------------------------------------------------------ */
/*  7. S-N Finite Life                                                */
/* ------------------------------------------------------------------ */

static void test_sn_curve(void)
{
    printf("--- S-N finite life ---\n");

    double Sut = 400.0;
    double Se  = 106.743;
    double f   = 0.9;
    double f_Sut = f * Sut;  /* = 360 */

    double a, b;
    int rc = fatigue_sn_constants(f_Sut, Se, &a, &b);
    ASSERT_EQ_INT(rc, 0, "sn_constants return");
    ASSERT_GT(a, 0.0, "sn_constants a > 0");
    ASSERT_LT(b, 0.0, "sn_constants b < 0");

    /* Verify anchor points: Sf(1e3) ~ f*Sut, Sf(1e6) ~ Se */
    double Sf_1e3 = fatigue_Sf(a, b, 1.0e3);
    ASSERT_RNEAR(Sf_1e3, f_Sut, RTOL, "Sf at 1e3 ~ f*Sut");

    double Sf_1e6 = fatigue_Sf(a, b, 1.0e6);
    ASSERT_RNEAR(Sf_1e6, Se, RTOL, "Sf at 1e6 ~ Se");

    /* Monotone decreasing: Sf(1e4) > Sf(1e5) > Sf(1e6) */
    double Sf_1e4 = fatigue_Sf(a, b, 1.0e4);
    double Sf_1e5 = fatigue_Sf(a, b, 1.0e5);
    ASSERT_GT(Sf_1e4, Sf_1e5, "Sf 1e4 > Sf 1e5");
    ASSERT_GT(Sf_1e5, Sf_1e6, "Sf 1e5 > Sf 1e6");

    /* Round-trip: N = cycles(a, b, sigma_a) then Sf(a, b, N) ~ sigma_a */
    double sigma_test = 150.0;
    double N_test = fatigue_cycles(a, b, sigma_test);
    ASSERT_GT(N_test, 0.0, "cycles > 0");
    double Sf_rt = fatigue_Sf(a, b, N_test);
    ASSERT_RNEAR(Sf_rt, sigma_test, RTOL, "round-trip Sf ~ sigma_a");

    /* Cross-check with direct formula:
     * b = log10(Se/f_Sut) / log10(1e6/1e3) = log10(Se/f_Sut) / 3 */
    double b_check = log10(Se / f_Sut) / 3.0;
    ASSERT_RNEAR(b, b_check, RTOL, "b cross-check");

    /* a = f_Sut / (1e3)^b */
    double a_check = f_Sut / pow(1.0e3, b_check);
    ASSERT_RNEAR(a, a_check, RTOL, "a cross-check");
}

/* ------------------------------------------------------------------ */
/*  8. Combined Loading                                               */
/* ------------------------------------------------------------------ */

static void test_combined_loading(void)
{
    printf("--- combined loading (von Mises) ---\n");

    /* Pure normal: sigma_a'= sigma_a */
    ASSERT_NEAR(fatigue_vonmises_alt(100.0, 0.0), 100.0, TOL,
                "vm alt pure normal");

    /* Pure torsion: sigma_a' = sqrt(3) * tau_a */
    ASSERT_RNEAR(fatigue_vonmises_alt(0.0, 100.0), sqrt(3.0) * 100.0, RTOL,
                 "vm alt pure torsion");

    /* Combined: sigma_a' = sqrt(sigma_a^2 + 3*tau_a^2) */
    double sa_comb = fatigue_vonmises_alt(100.0, 50.0);
    double expected = sqrt(100.0*100.0 + 3.0*50.0*50.0);
    ASSERT_RNEAR(sa_comb, expected, RTOL, "vm alt combined");

    /* Mean component: same formula structure */
    double sm_comb = fatigue_vonmises_mean(80.0, 40.0);
    double expected_m = sqrt(80.0*80.0 + 3.0*40.0*40.0);
    ASSERT_RNEAR(sm_comb, expected_m, RTOL, "vm mean combined");

    /* von Mises always >= individual components */
    ASSERT_GT(sa_comb, 100.0, "vm >= normal component");
    ASSERT_GT(sa_comb, sqrt(3.0) * 50.0, "vm >= torsion component");
}

/* ------------------------------------------------------------------ */
/*  Integration: full pipeline                                        */
/* ------------------------------------------------------------------ */

static void test_full_pipeline(void)
{
    printf("--- full pipeline ---\n");

    /* Shaft problem: 25mm machined mild steel, bending + torsion,
     * fluctuating load max=300 min=100 MPa, tau_a=40 tau_m=20 MPa,
     * Kt=2.0 q=0.85, 90% reliability, room temperature */

    double Sut = 400.0;
    double Sy  = 300.0;
    double Sep = fatigue_sep_estimate(Sut);
    ASSERT_NEAR(Sep, 200.0, TOL, "pipeline Se'");

    double ka = fatigue_marin_ka(Sut, MARIN_SURFACE_MACHINED);
    double kb = fatigue_marin_kb(25.0);
    double kc = fatigue_marin_kc(MARIN_LOAD_BENDING);
    double kd = fatigue_marin_kd(20.0);
    double ke = fatigue_marin_ke(0.90);
    double kf = 1.0;

    ASSERT_GT(ka, 0.0, "pipeline ka > 0");
    ASSERT_GT(kb, 0.0, "pipeline kb > 0");
    ASSERT_NEAR(kc, 1.0, TOL, "pipeline kc bending");
    ASSERT_NEAR(kd, 1.0, TOL, "pipeline kd room");
    ASSERT_NEAR(ke, 0.897, TOL, "pipeline ke 90%");

    double Se = fatigue_marin_Se(Sep, ka, kb, kc, kd, ke, kf);
    ASSERT_GT(Se, 0.0, "pipeline Se > 0");
    ASSERT_LT(Se, Sep, "pipeline Se < Se'");

    /* Loading */
    double sigma_max = 300.0, sigma_min = 100.0;
    double sigma_m, sigma_a;
    fatigue_stress_components(sigma_max, sigma_min, &sigma_m, &sigma_a);
    ASSERT_NEAR(sigma_m, 200.0, TOL, "pipeline sigma_m");
    ASSERT_NEAR(sigma_a, 100.0, TOL, "pipeline sigma_a");

    /* Stress concentration */
    double Kf_val = fatigue_Kf(2.0, 0.85);
    ASSERT_NEAR(Kf_val, 1.85, TOL, "pipeline Kf");

    /* Apply Kf to alternating stress */
    double sigma_a_eff = Kf_val * sigma_a;
    ASSERT_NEAR(sigma_a_eff, 185.0, TOL, "pipeline sigma_a_eff");

    /* Combined loading (von Mises) */
    double tau_a = 40.0, tau_m = 20.0;
    double sa_vm = fatigue_vonmises_alt(sigma_a_eff, tau_a);
    double sm_vm = fatigue_vonmises_mean(sigma_m, tau_m);
    ASSERT_GT(sa_vm, sigma_a_eff, "pipeline sa_vm > sigma_a_eff");

    /* Safety factors */
    double n_g = fatigue_safety_factor(FATIGUE_GOODMAN, Sut, Sy, Se,
                                       sa_vm, sm_vm);
    double n_s = fatigue_safety_factor(FATIGUE_SODERBERG, Sut, Sy, Se,
                                       sa_vm, sm_vm);
    double n_gerb = fatigue_safety_factor(FATIGUE_GERBER, Sut, Sy, Se,
                                          sa_vm, sm_vm);

    ASSERT_GT(n_g, 0.0, "pipeline n_goodman > 0");
    ASSERT_LT(n_s, n_g, "pipeline Soderberg < Goodman");
    ASSERT_GT(n_gerb, n_g, "pipeline Gerber > Goodman");

    /* S-N life */
    double f = 0.9;
    double sn_a, sn_b;
    fatigue_sn_constants(f * Sut, Se, &sn_a, &sn_b);
    double N = fatigue_cycles(sn_a, sn_b, sa_vm);
    ASSERT_GT(N, 0.0, "pipeline N > 0");

    printf("\n  Pipeline results for shaft problem:\n");
    printf("    Se = %.2f MPa, Kf = %.2f\n", Se, Kf_val);
    printf("    sa'(vm) = %.2f MPa, sm'(vm) = %.2f MPa\n", sa_vm, sm_vm);
    printf("    n_Goodman = %.3f, n_Soderberg = %.3f, n_Gerber = %.3f\n",
           n_g, n_s, n_gerb);
    printf("    N = %.0f cycles\n", N);
}

/* ------------------------------------------------------------------ */
/*  main                                                              */
/* ------------------------------------------------------------------ */

int main(void)
{
    printf("=== Fatigue Analysis Tests ===\n\n");

    test_stress_components();
    test_classify_loading();
    test_stress_ratio();
    test_sep_estimate();
    test_marin_ka();
    test_marin_kb();
    test_marin_kc();
    test_marin_kd();
    test_marin_ke();
    test_marin_Se();
    test_stress_concentration();
    test_mean_stress();
    test_sn_curve();
    test_combined_loading();
    test_full_pipeline();

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
