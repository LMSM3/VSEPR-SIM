/**
 * test_iso_fit.c - Validation for ISO 286 Tolerance & Fit
 *
 * Tests verify:
 *   1. Tolerance unit formula
 *   2. IT grade values against published tables
 *   3. H-hole deviations (EI = 0 always)
 *   4. Shaft deviations for common letters
 *   5. Fit classification (all three types)
 *   6. Full pipeline: H7/g6, H7/k6, H7/s6
 */

#include "core/iso_fit.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

static int g_pass = 0;
static int g_fail = 0;

#define ASSERT_MSG(cond, msg) do {                                      \
    if (!(cond)) {                                                      \
        fprintf(stderr, "  FAIL [%s:%d]: %s\n",                         \
                __FILE__, __LINE__, msg);                               \
        g_fail++;                                                       \
    } else {                                                            \
        g_pass++;                                                       \
    }                                                                   \
} while (0)

static void assert_near(double val, double expected, double tol,
                         const char *name, const char *file, int line)
{
    if (fabs(val - expected) <= tol) {
        g_pass++;
    } else {
        fprintf(stderr, "  FAIL [%s:%d]: %s = %.4f, expected %.4f (+/-%.4f)\n",
                file, line, name, val, expected, tol);
        g_fail++;
    }
}

#define ASSERT_NEAR(val, expected, tol) \
    assert_near((val), (expected), (tol), #val, __FILE__, __LINE__)

#define ASSERT_EQ_INT(val, expected) \
    ASSERT_MSG((val) == (expected), #val " != " #expected)

/* ========================================================================= */
/*  Test: tolerance unit                                                     */
/* ========================================================================= */

static void test_tolerance_unit(void)
{
    printf("--- tolerance unit ---\n");

    double i25 = iso_tolerance_unit(25.0);
    ASSERT_MSG(i25 > 1.0 && i25 < 2.0, "tolerance unit @ 25mm out of range");

    double i100 = iso_tolerance_unit(100.0);
    ASSERT_MSG(i100 > 2.0 && i100 < 3.0, "tolerance unit @ 100mm out of range");

    ASSERT_NEAR(iso_tolerance_unit(0.5), 0.0, 1e-9);
    ASSERT_NEAR(iso_tolerance_unit(600.0), 0.0, 1e-9);
}

/* ========================================================================= */
/*  Test: IT grade values                                                    */
/* ========================================================================= */

static void test_it_grades(void)
{
    double prev;
    int g;

    printf("--- IT grade values ---\n");

    ASSERT_NEAR(iso_it_value(25.0, 6), 13.0, 1.5);
    ASSERT_NEAR(iso_it_value(25.0, 7), 21.0, 1.5);
    ASSERT_NEAR(iso_it_value(25.0, 8), 33.0, 2.0);

    prev = iso_it_value(50.0, 5);
    for (g = 6; g <= 18; g++) {
        double curr = iso_it_value(50.0, g);
        ASSERT_MSG(curr > prev, "IT grade hierarchy violated");
        prev = curr;
    }

    ASSERT_MSG(iso_it_value(25.0, 0)  < 0.0, "IT0 should be unsupported");
    ASSERT_MSG(iso_it_value(25.0, 19) < 0.0, "IT19 should be unsupported");
}

/* ========================================================================= */
/*  Test: H-hole deviations                                                  */
/* ========================================================================= */

static void test_h_hole(void)
{
    double ei, es;
    int rc, g;

    printf("--- H hole deviations ---\n");

    rc = iso_hole_deviations(25.0, 'H', 7, &ei, &es);
    ASSERT_EQ_INT(rc, 0);
    ASSERT_NEAR(ei, 0.0, 1e-9);
    ASSERT_NEAR(es, iso_it_value(25.0, 7), 0.1);

    for (g = 5; g <= 12; g++) {
        rc = iso_hole_deviations(50.0, 'H', g, &ei, &es);
        ASSERT_EQ_INT(rc, 0);
        ASSERT_NEAR(ei, 0.0, 1e-9);
    }
}

/* ========================================================================= */
/*  Test: shaft deviations sign rules                                        */
/* ========================================================================= */

static void test_shaft_deviations(void)
{
    double ei, es, es_g;

    printf("--- shaft deviations ---\n");

    ASSERT_EQ_INT(iso_shaft_deviations(25.0, 'h', 6, &ei, &es), 0);
    ASSERT_NEAR(es, 0.0, 1e-9);
    ASSERT_MSG(ei < 0.0, "h6 shaft: ei must be < 0");

    ASSERT_EQ_INT(iso_shaft_deviations(25.0, 'g', 6, &ei, &es), 0);
    ASSERT_MSG(es < 0.0, "g6 shaft: es must be < 0");
    ASSERT_MSG(ei < es,  "g6 shaft: ei must be < es");

    es_g = es;
    ASSERT_EQ_INT(iso_shaft_deviations(25.0, 'f', 6, &ei, &es), 0);
    ASSERT_MSG(es < es_g, "f6 must be further from zero than g6");

    ASSERT_EQ_INT(iso_shaft_deviations(25.0, 'k', 6, &ei, &es), 0);
    ASSERT_MSG(ei > 0.0 || fabs(ei) < 5.0, "k6 shaft: ei should be near/above zero");

    ASSERT_EQ_INT(iso_shaft_deviations(25.0, 's', 6, &ei, &es), 0);
    ASSERT_MSG(ei > 0.0, "s6 shaft: ei must be > 0");
    ASSERT_MSG(ei > 20.0, "s6 shaft: ei should be well above zero");
}

/* ========================================================================= */
/*  Test: fit classification (the three rules)                               */
/* ========================================================================= */

static void test_classification(void)
{
    printf("--- fit classification ---\n");

    ASSERT_EQ_INT(iso_classify_fit(10.0, 50.0), ISO_FIT_CLEARANCE);
    ASSERT_EQ_INT(iso_classify_fit(0.001, 0.002), ISO_FIT_CLEARANCE);

    ASSERT_EQ_INT(iso_classify_fit(-50.0, -10.0), ISO_FIT_INTERFERENCE);
    ASSERT_EQ_INT(iso_classify_fit(-0.002, -0.001), ISO_FIT_INTERFERENCE);

    ASSERT_EQ_INT(iso_classify_fit(-10.0, 20.0), ISO_FIT_TRANSITION);
    ASSERT_EQ_INT(iso_classify_fit(-0.001, 0.001), ISO_FIT_TRANSITION);

    ASSERT_EQ_INT(iso_classify_fit(0.0, 10.0), ISO_FIT_TRANSITION);
    ASSERT_EQ_INT(iso_classify_fit(-10.0, 0.0), ISO_FIT_TRANSITION);
}

/* ========================================================================= */
/*  Test: full fit pipeline                                                  */
/* ========================================================================= */

static void test_full_fits(void)
{
    IsoFitResult r1, r2, r3;

    printf("--- full fit pipeline ---\n");

    r1 = iso_compute_fit(25.0, 'H', 7, 'g', 6);
    ASSERT_EQ_INT(r1.type, ISO_FIT_CLEARANCE);
    ASSERT_MSG(r1.delta_min > 0.0, "H7/g6: delta_min must be > 0");
    ASSERT_MSG(r1.delta_max > r1.delta_min, "H7/g6: delta_max must be > delta_min");

    r2 = iso_compute_fit(25.0, 'H', 7, 'k', 6);
    ASSERT_EQ_INT(r2.type, ISO_FIT_TRANSITION);

    r3 = iso_compute_fit(25.0, 'H', 7, 's', 6);
    ASSERT_EQ_INT(r3.type, ISO_FIT_INTERFERENCE);
    ASSERT_MSG(r3.delta_max < 0.0, "H7/s6: delta_max must be < 0");

    printf("\n");
    iso_fit_print(&r2, stdout);
}

/* ========================================================================= */
/*  Test: zone geometry consistency                                          */
/* ========================================================================= */

static void test_zone_consistency(void)
{
    IsoFitResult r;
    double range, total_it;

    printf("--- zone consistency ---\n");

    r = iso_compute_fit(50.0, 'H', 7, 'f', 6);

    ASSERT_MSG(r.hole.d_max_mm > r.hole.d_min_mm, "Hole: d_max must be > d_min");
    ASSERT_MSG(r.shaft.d_max_mm > r.shaft.d_min_mm, "Shaft: d_max must be > d_min");

    ASSERT_MSG(r.hole.it_um > 0.0, "Hole IT must be > 0");
    ASSERT_MSG(r.shaft.it_um > 0.0, "Shaft IT must be > 0");

    ASSERT_NEAR(r.hole.it_um, r.hole.es_um - r.hole.ei_um, 0.01);
    ASSERT_NEAR(r.shaft.it_um, r.shaft.es_um - r.shaft.ei_um, 0.01);

    range = r.delta_max - r.delta_min;
    total_it = r.hole.it_um + r.shaft.it_um;
    ASSERT_NEAR(range, total_it, 0.1);
}

/* ========================================================================= */
/*  Main                                                                     */
/* ========================================================================= */

int main(void)
{
    printf("=== ISO 286 Fit Tests ===\n\n");

    test_tolerance_unit();
    test_it_grades();
    test_h_hole();
    test_shaft_deviations();
    test_classification();
    test_full_fits();
    test_zone_consistency();

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return (g_fail > 0) ? 1 : 0;
}
