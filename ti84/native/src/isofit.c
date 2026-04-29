/*
 * isofit.c - ISO 286 Fit Calculator for TI-83+/84+
 *
 * Computes hole/shaft tolerance zones and fit classification
 * for the H-system (hole-basis fits).
 *
 * Input:
 *   D   = nominal diameter [mm]
 *   Shaft letter (a-u as integer code: a=1 ... h=8, k=11, etc.)
 *   Shaft grade (IT number)
 *   Hole grade (IT number, hole always H)
 *
 * Computes:
 *   Tolerance unit: i = 0.45*cbrt(D) + 0.001*D
 *   IT value from grade multiplier table
 *   Fundamental deviation from shaft letter
 *   Fit classification: clearance / transition / interference
 *
 * Build: tibuild.ps1 src\isofit.c ISOFIT
 */

#include <stdio.h>
#include <math.h>
#include "ti84.h"

/* ------------------------------------------------------------------ */
/*  ISO 286 size ranges: {min, max} in mm                             */
/* ------------------------------------------------------------------ */

/* Simplified table for common sizes */
static float size_lo[] = { 1, 3, 6, 10, 18, 30, 50, 80, 120, 180, 250, 315 };
static float size_hi[] = { 3, 6, 10, 18, 30, 50, 80, 120, 180, 250, 315, 400 };
#define NRANGES 12

/* IT grade multipliers (IT5 through IT13) */
static int it_mult[] = { 7, 10, 16, 25, 40, 64, 100, 160, 250 };
/* Index: 0=IT5, 1=IT6, 2=IT7, 3=IT8, ... 8=IT13 */

/* ------------------------------------------------------------------ */
/*  Fundamental deviations for common shaft letters (in tolerance     */
/*  units i, for the H-system mirror)                                 */
/*  Negative = below zero line (clearance with H hole)                */
/*  Positive = above zero line (interference)                         */
/* ------------------------------------------------------------------ */

/* Shaft fundamental deviation as integer multiplier of tolerance unit.
 * For letters a-h: upper deviation es (negative).
 * For letters k-u: lower deviation ei (positive).
 * We store the multiplier; actual deviation = mult * i(D).
 *
 * Simplified table (most common letters): */

static int get_shaft_dev_mult(int letter_code)
{
    /* letter_code: f=6, g=7, h=8, k=11, m=13, n=14, p=16, s=19 */
    switch (letter_code) {
    case 6:  return -5;    /* f: es ~ -5.5i */
    case 7:  return -2;    /* g: es ~ -2.5i */
    case 8:  return 0;     /* h: es = 0 */
    case 11: return 0;     /* k: ei ~ 0 (slightly positive) */
    case 13: return 2;     /* m: ei ~ +2i */
    case 14: return 5;     /* n: ei ~ +5i */
    case 16: return 10;    /* p: ei ~ +10i */
    case 19: return 17;    /* s: ei ~ +17i */
    default: return 0;
    }
}

int main(void)
{
    float D;
    int shaft_letter, shaft_grade, hole_grade;
    int range_idx;
    float geo_mean, tol_unit;
    float it_hole, it_shaft;
    float hole_ei, hole_es;
    float shaft_ei, shaft_es;
    float delta_min, delta_max;
    int dev_mult;

    clrhome();
    printf("=ISO 286 FIT=\n\n");

    /* --- Input --- */
    D = (float)iinput("D[mm]", 25);

    printf("\nSHAFT LETTER:\n");
    printf("f=6 g=7 h=8\n");
    printf("k=11 m=13 n=14\n");
    printf("p=16 s=19\n");
    shaft_letter = iinput("Ltr#", 7);  /* default g */
    shaft_grade  = iinput("ShGrd", 6); /* default IT6 */
    hole_grade   = iinput("HlGrd", 7); /* default IT7 -> H7 */

    /* --- Find size range --- */
    range_idx = -1;
    {
        int i;
        for (i = 0; i < NRANGES; i++) {
            if (D >= size_lo[i] && D <= size_hi[i]) {
                range_idx = i;
                break;
            }
        }
    }
    if (range_idx < 0) {
        printf("ERR:D RANGE\n");
        waitkey();
        return 1;
    }

    /* Geometric mean of range */
    geo_mean = sqrt(size_lo[range_idx] * size_hi[range_idx]);

    /* Tolerance unit: i = 0.45 * cbrt(D) + 0.001 * D  [um] */
    tol_unit = 0.45f * pow(geo_mean, 1.0f / 3.0f) + 0.001f * geo_mean;

    /* --- IT values --- */
    /* Grade 5-13 supported */
    if (hole_grade < 5 || hole_grade > 13 ||
        shaft_grade < 5 || shaft_grade > 13) {
        printf("ERR:GRADE 5-13\n");
        waitkey();
        return 1;
    }

    it_hole  = (float)it_mult[hole_grade - 5] * tol_unit;
    it_shaft = (float)it_mult[shaft_grade - 5] * tol_unit;

    /* --- Hole H: EI = 0, ES = IT --- */
    hole_ei = 0.0f;
    hole_es = it_hole;

    /* --- Shaft deviation --- */
    dev_mult = get_shaft_dev_mult(shaft_letter);

    if (shaft_letter <= 8) {
        /* Letters a-h: es = dev * i (negative or zero) */
        shaft_es = (float)dev_mult * tol_unit;
        shaft_ei = shaft_es - it_shaft;
    } else {
        /* Letters k-u: ei = dev * i (positive) */
        shaft_ei = (float)dev_mult * tol_unit;
        shaft_es = shaft_ei + it_shaft;
    }

    /* --- Fit deltas --- */
    /* delta_min = hole_EI - shaft_es */
    /* delta_max = hole_ES - shaft_ei */
    delta_min = hole_ei - shaft_es;
    delta_max = hole_es - shaft_ei;

    /* --- Classification --- */
    clrhome();
    printf("=FIT RESULT=\n");
    printf("D=%dmm H%d/", (int)D, hole_grade);
    /* Print shaft letter */
    switch (shaft_letter) {
    case 6:  printf("f"); break;
    case 7:  printf("g"); break;
    case 8:  printf("h"); break;
    case 11: printf("k"); break;
    case 13: printf("m"); break;
    case 14: printf("n"); break;
    case 16: printf("p"); break;
    case 19: printf("s"); break;
    default: printf("?"); break;
    }
    printf("%d\n\n", shaft_grade);

    printf("HOLE H%d:\n", hole_grade);
    printf(" EI=%d ES=%d um\n", (int)hole_ei, (int)hole_es);

    printf("SHAFT:\n");
    printf(" ei=%d es=%d um\n", (int)shaft_ei, (int)shaft_es);

    printf("\nDmin=%d um\n", (int)delta_min);
    printf("Dmax=%d um\n", (int)delta_max);

    printf("\nTYPE: ");
    if ((int)delta_min > 0 && (int)delta_max > 0)
        printf("CLEARANCE\n");
    else if ((int)delta_min < 0 && (int)delta_max < 0)
        printf("INTERFER\n");
    else
        printf("TRANSITION\n");

    waitkey();
    return 0;
}
