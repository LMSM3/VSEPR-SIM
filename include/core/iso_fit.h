/**
 * iso_fit.h - ISO 286 Tolerance & Fit Computation (Pure C)
 * VSEPR-Sim Engineering Module
 *
 * Implements:
 *   - Standard tolerance unit  i = 0.45 * D^(1/3) + 0.001 * D   [um]
 *   - IT grades 01 .. 18
 *   - Fundamental deviations for holes (A-ZC) and shafts (a-zc)
 *   - Fit classification: clearance / transition / interference
 *   - Full fit computation from designation strings (e.g. "H7/k6")
 *
 * Coordinate convention (ISO 286-1):
 *   Zero line  = basic (nominal) size  D
 *   Hole:  D_min = D + EI,   D_max = D + ES        (EI,ES in um)
 *   Shaft: d_min = d + ei,   d_max = d + es        (ei,es in um)
 *   Tolerance width:  IT = ES - EI  =  es - ei
 *   Clearance:  delta = D_hole - d_shaft  (positive = gap, negative = overlap)
 *
 * Rules (exhaustive and mutually exclusive):
 *   Clearance fit:     delta_min > 0   (always gap)
 *   Transition fit:    delta_min <= 0  AND  delta_max >= 0   (uncertain)
 *   Interference fit:  delta_max < 0   (always overlap)
 *
 * Build (standalone):
 *   gcc -std=c11 -O2 -Wall -Wextra -lm -o iso_fit iso_fit.c
 */

#ifndef ISO_FIT_H
#define ISO_FIT_H

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================= */
/*  Types                                                                     */
/* ========================================================================= */

/** Three mutually exclusive fit types. No other state exists. */
typedef enum {
    ISO_FIT_CLEARANCE    = 0,   /* delta > 0 always: freedom, motion        */
    ISO_FIT_TRANSITION   = 1,   /* delta in (-,+):   alignment, control     */
    ISO_FIT_INTERFERENCE = 2    /* delta < 0 always: force, torque transfer  */
} IsoFitType;

/** Tolerance zone: one side of a fit (hole or shaft). */
typedef struct {
    double d_nominal_mm;    /* basic size [mm]                       */
    char   letter;          /* position letter  (H, k, g, ...)      */
    int    grade;           /* IT grade number  (5, 6, 7, ...)      */
    double ei_um;           /* lower deviation  [um] (EI or ei)     */
    double es_um;           /* upper deviation  [um] (ES or es)     */
    double d_min_mm;        /* d_nominal + ei   [mm]                 */
    double d_max_mm;        /* d_nominal + es   [mm]                 */
    double it_um;           /* tolerance width  es - ei [um]         */
} IsoToleranceZone;

/** Complete fit result.  This is the final answer. */
typedef struct {
    IsoFitType       type;       /* clearance / transition / interference */
    double           delta_min;  /* D_min(hole) - d_max(shaft) [um]      */
    double           delta_max;  /* D_max(hole) - d_min(shaft) [um]      */
    IsoToleranceZone hole;
    IsoToleranceZone shaft;
} IsoFitResult;

/* ========================================================================= */
/*  Core Formulas                                                             */
/* ========================================================================= */

/**
 * Standard tolerance unit (ISO 286-1 s4.1).
 *   i = 0.45 * cbrt(D) + 0.001 * D    [um, D in mm]
 *
 * D must be the geometric mean of the nominal size range.
 * Returns 0.0 on invalid input (D <= 0).
 */
double iso_tolerance_unit(double d_mm);

/**
 * Geometric-mean diameter for the ISO size range containing d_mm.
 * Returns 0.0 if d_mm is outside 1-500 mm.
 */
double iso_geometric_mean(double d_mm);

/**
 * IT grade value in um.
 *   grade in [1 .. 18]   (IT01 and IT0 not implemented).
 * Uses the standard multiplier table: IT5=7i, IT6=10i, ... IT18=2500i.
 * IT1-IT4 are geometrically interpolated between IT1 base and IT5.
 * Returns -1.0 on invalid input.
 */
double iso_it_value(double d_mm, int grade);

/* ========================================================================= */
/*  Deviation Lookup                                                          */
/* ========================================================================= */

/**
 * Compute hole deviations (EI, ES) for a given letter + grade.
 * Supported hole letters: H  (the dominant system - EI = 0).
 *   Additional: A B C D E F G  (mirrors of shaft positions).
 *
 * Returns 0 on success, -1 if the letter/grade is unsupported.
 */
int iso_hole_deviations(double d_mm, char letter, int grade,
                        double *ei_um, double *es_um);

/**
 * Compute shaft deviations (ei, es) for a given letter + grade.
 * Supported shaft letters:
 *   Below zero line:  a b c d e f g h   (es <= 0, clearance with H hole)
 *   Near  zero line:  js k m n          (transition territory)
 *   Above zero line:  p r s t u         (interference territory)
 *
 * Returns 0 on success, -1 if the letter/grade is unsupported.
 */
int iso_shaft_deviations(double d_mm, char letter, int grade,
                         double *ei_um, double *es_um);

/* ========================================================================= */
/*  Fit Computation                                                           */
/* ========================================================================= */

/**
 * Compute a complete fit.
 *
 *   d_mm          - nominal (basic) size [mm], range 1-500
 *   hole_letter   - hole position letter (usually 'H')
 *   hole_grade    - hole IT grade number
 *   shaft_letter  - shaft position letter
 *   shaft_grade   - shaft IT grade number
 *
 * Returns a fully populated IsoFitResult.
 * On error (unsupported letter/grade), type is set to ISO_FIT_CLEARANCE
 * and delta_min = delta_max = 0.0.
 */
IsoFitResult iso_compute_fit(double d_mm,
                             char hole_letter,  int hole_grade,
                             char shaft_letter, int shaft_grade);

/* ========================================================================= */
/*  Classification & Printing                                                 */
/* ========================================================================= */

/** Classify from raw delta values. Pure logic, no table lookup. */
IsoFitType iso_classify_fit(double delta_min, double delta_max);

/** Human-readable name: "Clearance" / "Transition" / "Interference". */
const char *iso_fit_type_str(IsoFitType t);

/**
 * Print a complete fit result to the given FILE stream.
 * Format matches the engineering notation used in ISO standards.
 */
void iso_fit_print(const IsoFitResult *r, void *stream);

#ifdef __cplusplus
}
#endif

#endif /* ISO_FIT_H */
