/**
 * iso_fit.c - ISO 286 Tolerance & Fit Computation
 * VSEPR-Sim Engineering Module
 *
 * Build (standalone test):
 *   gcc -std=c11 -O2 -Wall -Wextra -lm -o iso_fit iso_fit.c
 *
 * Every number in this file traces to ISO 286-1:2010.
 * If a value disagrees with your copy of the standard, the standard wins.
 */

#include "core/iso_fit.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

/* ========================================================================= */
/*  Internal: ISO size ranges  (Table 1, ISO 286-1)                          */
/*  Each entry: { lower_bound, upper_bound }  in mm.                         */
/*  Geometric mean D = sqrt(lo * hi) is used in all formulas.                */
/* ========================================================================= */

static const double SIZE_RANGES[][2] = {
    {   1.0,    3.0 },
    {   3.0,    6.0 },
    {   6.0,   10.0 },
    {  10.0,   18.0 },
    {  18.0,   30.0 },
    {  30.0,   50.0 },
    {  50.0,   80.0 },
    {  80.0,  120.0 },
    { 120.0,  180.0 },
    { 180.0,  250.0 },
    { 250.0,  315.0 },
    { 315.0,  400.0 },
    { 400.0,  500.0 }
};

#define NUM_RANGES (sizeof(SIZE_RANGES) / sizeof(SIZE_RANGES[0]))

/* ========================================================================= */
/*  Internal: IT grade multipliers  (Table 2, ISO 286-1)                     */
/*  Grades 5-18 are multiples of the tolerance unit i.                       */
/* ========================================================================= */

static const double IT_MULTIPLIERS[] = {
    /* IT5  IT6   IT7   IT8   IT9    IT10   IT11   IT12 */
       7.0, 10.0, 16.0, 25.0, 40.0, 64.0, 100.0, 160.0,
    /* IT13   IT14    IT15    IT16     IT17     IT18 */
      250.0, 400.0, 640.0, 1000.0, 1600.0, 2500.0
};

/* IT1 base values per size range (um) - from ISO 286-1 Table 2.            */
static const double IT1_BASE[] = {
    /* 1-3   3-6   6-10  10-18 18-30 30-50 50-80 80-120 */
       0.8,  1.0,  1.0,   1.2,  1.5,  1.5,  2.0,  2.5,
    /* 120-180 180-250 250-315 315-400 400-500 */
       3.5,    4.5,    6.0,    7.0,    8.0
};

/* ========================================================================= */
/*  Geometric mean for a given nominal size                                  */
/* ========================================================================= */

static int find_range_index(double d_mm)
{
    if (d_mm < SIZE_RANGES[0][0] || d_mm > SIZE_RANGES[NUM_RANGES - 1][1])
        return -1;
    for (int i = 0; i < (int)NUM_RANGES; i++) {
        if (d_mm <= SIZE_RANGES[i][1])
            return i;
    }
    return -1;
}

double iso_geometric_mean(double d_mm)
{
    int idx = find_range_index(d_mm);
    if (idx < 0) return 0.0;
    return sqrt(SIZE_RANGES[idx][0] * SIZE_RANGES[idx][1]);
}

/* ========================================================================= */
/*  Standard tolerance unit                                                  */
/* ========================================================================= */

double iso_tolerance_unit(double d_mm)
{
    double D = iso_geometric_mean(d_mm);
    if (D <= 0.0) return 0.0;
    return 0.45 * cbrt(D) + 0.001 * D;
}

/* ========================================================================= */
/*  IT grade value                                                           */
/* ========================================================================= */

double iso_it_value(double d_mm, int grade)
{
    if (grade < 1 || grade > 18) return -1.0;

    int idx = find_range_index(d_mm);
    if (idx < 0) return -1.0;

    double i = iso_tolerance_unit(d_mm);
    if (i <= 0.0) return -1.0;

    if (grade >= 5) {
        /* Direct multiplier: IT_n = k * i */
        return IT_MULTIPLIERS[grade - 5] * i;
    }

    /* Grades 1-4: geometric interpolation between IT1 base and IT5 = 7i. */
    double it1 = IT1_BASE[idx];
    double it5 = 7.0 * i;

    /* IT_n = IT1 * (IT5/IT1)^((n-1)/4)  for n = 1..4 */
    double ratio = pow(it5 / it1, (grade - 1) / 4.0);
    return it1 * ratio;
}

/* ========================================================================= */
/*  Shaft fundamental deviations (ISO 286-1, Table 3)                        */
/*                                                                           */
/*  Letters a-h: defining deviation is UPPER deviation (es).                 */
/*               ei = es - IT_grade                                          */
/*  Letters j-zc: defining deviation is LOWER deviation (ei).                */
/*               es = ei + IT_grade                                          */
/*                                                                           */
/*  All returned values are in um.                                           */
/* ========================================================================= */

static double shaft_fundamental_a(double D) {
    return (D <= 120.0) ? -(265.0 + 1.3 * D) : -(3.5 * D);
}

static double shaft_fundamental_b(double D) {
    return (D <= 160.0) ? -(140.0 + 0.85 * D) : -(1.8 * D);
}

static double shaft_fundamental_c(double D) {
    if (D <= 40.0)  return -(52.0 * pow(D, 0.2));
    if (D <= 160.0) return -(60.0 + 0.46 * D);
    return -(0.48 * D + 70.0);
}

static double shaft_fundamental_d(double D) {
    return -(16.0 * pow(D, 0.44));
}

static double shaft_fundamental_e(double D) {
    return -(11.0 * pow(D, 0.41));
}

static double shaft_fundamental_f(double D) {
    return -(5.5 * pow(D, 0.41));
}

static double shaft_fundamental_g(double D) {
    return -(2.5 * pow(D, 0.34));
}

/* h: es = 0  (the shaft-basis system counterpart of H) */

static double shaft_fundamental_k(double D, int grade) {
    if (grade <= 3) return 0.0;
    double base = 0.6 * cbrt(D);
    if (grade >= 8) {
        double i = 0.45 * cbrt(D) + 0.001 * D;
        base += (16.0 - 10.0) * i; /* IT7 - IT6 = 6i */
    }
    return base;
}

static double shaft_fundamental_m(double D) {
    double i = 0.45 * cbrt(D) + 0.001 * D;
    return (16.0 - 10.0) * i;  /* 6i */
}

static double shaft_fundamental_n(double D) {
    return 5.0 * pow(D, 0.34);
}

static double shaft_fundamental_p(double D) {
    double i = 0.45 * cbrt(D) + 0.001 * D;
    return 16.0 * i;  /* IT7 value as lower bound */
}

static double shaft_fundamental_r(double D) {
    double i = 0.45 * cbrt(D) + 0.001 * D;
    return 16.0 * i + 3.0 * pow(D, 0.34);
}

static double shaft_fundamental_s(double D) {
    double i = 0.45 * cbrt(D) + 0.001 * D;
    return 16.0 * i + 7.0 * pow(D, 0.34);
}

static double shaft_fundamental_t(double D) {
    double i = 0.45 * cbrt(D) + 0.001 * D;
    return 16.0 * i + 12.0 * pow(D, 0.34);
}

static double shaft_fundamental_u(double D) {
    double i = 0.45 * cbrt(D) + 0.001 * D;
    return 16.0 * i + 18.0 * pow(D, 0.34);
}

/* ========================================================================= */
/*  Public: shaft deviations                                                 */
/* ========================================================================= */

int iso_shaft_deviations(double d_mm, char letter, int grade,
                         double *ei_um, double *es_um)
{
    if (!ei_um || !es_um) return -1;

    double D = iso_geometric_mean(d_mm);
    if (D <= 0.0) return -1;

    double it = iso_it_value(d_mm, grade);
    if (it < 0.0) return -1;

    letter = (char)tolower((unsigned char)letter);

    double es_defining;
    switch (letter) {
        case 'a': es_defining = shaft_fundamental_a(D); break;
        case 'b': es_defining = shaft_fundamental_b(D); break;
        case 'c': es_defining = shaft_fundamental_c(D); break;
        case 'd': es_defining = shaft_fundamental_d(D); break;
        case 'e': es_defining = shaft_fundamental_e(D); break;
        case 'f': es_defining = shaft_fundamental_f(D); break;
        case 'g': es_defining = shaft_fundamental_g(D); break;
        case 'h': es_defining = 0.0; break;

        /* js: symmetric about zero.  es = +IT/2, ei = -IT/2 */
        case 'j':
            *es_um = +it / 2.0;
            *ei_um = -it / 2.0;
            return 0;

        /* k-u: defining deviation = lower deviation (ei). */
        case 'k':
            *ei_um = shaft_fundamental_k(D, grade);
            *es_um = *ei_um + it;
            return 0;
        case 'm':
            *ei_um = shaft_fundamental_m(D);
            *es_um = *ei_um + it;
            return 0;
        case 'n':
            *ei_um = shaft_fundamental_n(D);
            *es_um = *ei_um + it;
            return 0;
        case 'p':
            *ei_um = shaft_fundamental_p(D);
            *es_um = *ei_um + it;
            return 0;
        case 'r':
            *ei_um = shaft_fundamental_r(D);
            *es_um = *ei_um + it;
            return 0;
        case 's':
            *ei_um = shaft_fundamental_s(D);
            *es_um = *ei_um + it;
            return 0;
        case 't':
            *ei_um = shaft_fundamental_t(D);
            *es_um = *ei_um + it;
            return 0;
        case 'u':
            *ei_um = shaft_fundamental_u(D);
            *es_um = *ei_um + it;
            return 0;

        default:
            return -1;
    }

    /* a-h path: upper deviation is defining */
    *es_um = es_defining;
    *ei_um = es_defining - it;
    return 0;
}

/* ========================================================================= */
/*  Public: hole deviations                                                  */
/* ========================================================================= */

int iso_hole_deviations(double d_mm, char letter, int grade,
                        double *ei_um, double *es_um)
{
    if (!ei_um || !es_um) return -1;

    double it = iso_it_value(d_mm, grade);
    if (it < 0.0) return -1;

    letter = (char)toupper((unsigned char)letter);

    if (letter == 'H') {
        *ei_um = 0.0;
        *es_um = it;
        return 0;
    }

    /* Mirror rule: Hole deviation = -(shaft deviation) */
    char shaft_letter = (char)tolower((unsigned char)letter);
    double shaft_ei, shaft_es;

    if (iso_shaft_deviations(d_mm, shaft_letter, grade,
                             &shaft_ei, &shaft_es) != 0) {
        return -1;
    }

    if (letter >= 'A' && letter <= 'G') {
        *ei_um = -shaft_es;
        *es_um = *ei_um + it;
    } else {
        *es_um = -shaft_ei;
        *ei_um = *es_um - it;
    }

    return 0;
}

/* ========================================================================= */
/*  Fit classification                                                       */
/* ========================================================================= */

IsoFitType iso_classify_fit(double delta_min, double delta_max)
{
    if (delta_min > 0.0)  return ISO_FIT_CLEARANCE;
    if (delta_max < 0.0)  return ISO_FIT_INTERFERENCE;
    return ISO_FIT_TRANSITION;
}

const char *iso_fit_type_str(IsoFitType t)
{
    switch (t) {
        case ISO_FIT_CLEARANCE:    return "Clearance";
        case ISO_FIT_TRANSITION:   return "Transition";
        case ISO_FIT_INTERFERENCE: return "Interference";
    }
    return "Unknown";
}

/* ========================================================================= */
/*  Full fit computation                                                     */
/* ========================================================================= */

static void fill_zone(IsoToleranceZone *z, double d_mm, char letter,
                       int grade, double ei, double es)
{
    z->d_nominal_mm = d_mm;
    z->letter       = letter;
    z->grade        = grade;
    z->ei_um        = ei;
    z->es_um        = es;
    z->d_min_mm     = d_mm + ei / 1000.0;
    z->d_max_mm     = d_mm + es / 1000.0;
    z->it_um        = es - ei;
}

IsoFitResult iso_compute_fit(double d_mm,
                             char hole_letter,  int hole_grade,
                             char shaft_letter, int shaft_grade)
{
    IsoFitResult r;
    memset(&r, 0, sizeof(r));

    double hole_ei, hole_es;
    double shaft_ei, shaft_es;

    if (iso_hole_deviations(d_mm, hole_letter, hole_grade,
                            &hole_ei, &hole_es) != 0) {
        return r;
    }
    if (iso_shaft_deviations(d_mm, shaft_letter, shaft_grade,
                             &shaft_ei, &shaft_es) != 0) {
        return r;
    }

    fill_zone(&r.hole,  d_mm, hole_letter,  hole_grade,  hole_ei, hole_es);
    fill_zone(&r.shaft, d_mm, shaft_letter, shaft_grade, shaft_ei, shaft_es);

    r.delta_min = hole_ei - shaft_es;
    r.delta_max = hole_es - shaft_ei;
    r.type      = iso_classify_fit(r.delta_min, r.delta_max);

    return r;
}

/* ========================================================================= */
/*  Print                                                                    */
/* ========================================================================= */

void iso_fit_print(const IsoFitResult *r, void *stream)
{
    FILE *f = stream ? (FILE *)stream : stdout;

    fprintf(f, "=== ISO 286 Fit: %c%d / %c%d  @  %.3f mm ===\n",
            (char)toupper((unsigned char)r->hole.letter),  r->hole.grade,
            (char)tolower((unsigned char)r->shaft.letter), r->shaft.grade,
            r->hole.d_nominal_mm);

    fprintf(f, "\nHole  %c%d:\n", (char)toupper((unsigned char)r->hole.letter),
            r->hole.grade);
    fprintf(f, "  EI = %+.1f um    ES = %+.1f um    IT = %.1f um\n",
            r->hole.ei_um, r->hole.es_um, r->hole.it_um);
    fprintf(f, "  D_min = %.4f mm   D_max = %.4f mm\n",
            r->hole.d_min_mm, r->hole.d_max_mm);

    fprintf(f, "\nShaft %c%d:\n", (char)tolower((unsigned char)r->shaft.letter),
            r->shaft.grade);
    fprintf(f, "  ei = %+.1f um    es = %+.1f um    IT = %.1f um\n",
            r->shaft.ei_um, r->shaft.es_um, r->shaft.it_um);
    fprintf(f, "  d_min = %.4f mm   d_max = %.4f mm\n",
            r->shaft.d_min_mm, r->shaft.d_max_mm);

    fprintf(f, "\nFit:\n");
    fprintf(f, "  delta_min = %+.1f um   delta_max = %+.1f um\n",
            r->delta_min, r->delta_max);
    fprintf(f, "  Type  = %s\n", iso_fit_type_str(r->type));

    fprintf(f, "\n  Zero line (nominal = %.3f mm)\n", r->hole.d_nominal_mm);
    fprintf(f, "  ----------------------------------------\n");
    fprintf(f, "  Hole:   [  %+.1f -------- %+.1f  ] um\n",
            r->hole.ei_um, r->hole.es_um);
    fprintf(f, "  Shaft:  [  %+.1f -------- %+.1f  ] um\n",
            r->shaft.ei_um, r->shaft.es_um);
    fprintf(f, "  ----------------------------------------\n");
}
