/*
 * bolts.c - Bolted Joint Fatigue Analysis for TI-83+/84+
 *
 * Computes bolt preload, stiffness ratio, and fatigue safety
 * for a bolted joint under fluctuating external load.
 *
 * Formulas:
 *   C = kb / (kb + km)              (stiffness ratio)
 *   Fb = Fi + C*P                   (bolt load)
 *   Fm = Fi - (1-C)*P               (member clamping)
 *   sigma_a = C*Pa / (2*At)         (alternating bolt stress)
 *   sigma_m = Fi/At + C*Pm/(At)     (mean bolt stress)
 *   n = 1 / (sigma_a/Se + sigma_m/Sut)  (Goodman)
 *
 * Where:
 *   Fi = preload [N]
 *   P  = external load (fluctuating) [N]
 *   At = tensile stress area [mm^2]
 *   kb = bolt stiffness [N/mm]
 *   km = member stiffness [N/mm]
 *
 * Build: tibuild.ps1 src\bolts.c BOLTS
 */

#include <stdio.h>
#include <math.h>
#include "ti84.h"

#define PI 3.14159f

int main(void)
{
    float At, Fi, Pmax, Pmin, Pm, Pa;
    float kb, km, C;
    float sigma_i, sigma_a, sigma_m;
    float Sut, Sp, Se;
    float n_g, n_sep;

    clrhome();
    printf("=BOLT FATIGUE=\n\n");

    /* --- Bolt geometry --- */
    printf("BOLT PROPS:\n");
    At  = (float)iinput("At[mm2]", 84);    /* M10x1.5: 58mm2, M12: 84mm2 */
    Sut = (float)iinput("Sut", 830);       /* Grade 8.8: 830 MPa */
    Sp  = (float)iinput("Sp", 600);        /* Grade 8.8: 600 MPa proof */
    Se  = (float)iinput("Se[MPa]", 129);   /* Rolled threads ~ 129 MPa */

    /* --- Preload --- */
    clrhome();
    printf("=PRELOAD=\n\n");
    /* Common rule: Fi = 0.75*Sp*At (non-permanent) or 0.90*Sp*At (permanent) */
    {
        int pct = iinput("Fi%%Sp", 75);
        Fi = (float)pct / 100.0f * Sp * At;
        printf("Fi=%d N\n", (int)Fi);
    }

    /* --- Stiffness --- */
    clrhome();
    printf("=STIFFNESS=\n\n");
    printf("(N/mm)\n");
    kb = (float)iinput("kb", 500000);
    km = (float)iinput("km", 1500000);

    C = kb / (kb + km);
    printf("\nC=%d%%\n", (int)(C * 100.0f));

    /* --- External load --- */
    clrhome();
    printf("=EXT LOAD=\n\n");
    Pmax = (float)iinput("Pmax[N]", 20000);
    Pmin = (float)iinput("Pmin[N]", 5000);

    Pm = (Pmax + Pmin) / 2.0f;
    Pa = (Pmax - Pmin) / 2.0f;

    /* --- Bolt stresses --- */
    /* Initial preload stress */
    sigma_i = Fi / At;

    /* Alternating stress in bolt */
    sigma_a = C * Pa / At;

    /* Mean stress in bolt */
    sigma_m = sigma_i + C * Pm / At;

    /* --- Safety factors --- */
    /* Goodman */
    n_g = 1.0f / (sigma_a / Se + sigma_m / Sut);

    /* Separation check: P_sep = Fi / (1 - C) */
    n_sep = Fi / ((1.0f - C) * Pmax);

    /* --- Results page 1 --- */
    clrhome();
    printf("=BOLT STRESS=\n\n");
    printf("Si=%d MPa\n", (int)sigma_i);
    printf("Sa=%d MPa\n", (int)sigma_a);
    printf("Sm=%d MPa\n", (int)sigma_m);
    printf("\nC=%d%%\n", (int)(C * 100.0f));
    printf("Fi=%dN\n", (int)Fi);

    waitkey();

    /* --- Results page 2 --- */
    clrhome();
    printf("=BOLT SAFETY=\n\n");
    printf("Goodman:\n");
    printf(" n=%d%%\n", (int)(n_g * 100.0f));

    if (n_g >= 1.0f)
        printf(" >>SAFE\n");
    else
        printf(" >>UNSAFE\n");

    printf("\nSeparation:\n");
    printf(" n=%d%%\n", (int)(n_sep * 100.0f));

    if (n_sep >= 1.0f)
        printf(" >>NO SEP\n");
    else
        printf(" >>SEPARATES!\n");

    /* Yielding check: sigma_i + C*Pmax/At vs Sp */
    {
        float sigma_bolt_max = sigma_i + C * Pmax / At;
        float n_yield = Sp / sigma_bolt_max;
        printf("\nYield:\n");
        printf(" n=%d%%\n", (int)(n_yield * 100.0f));
        if (n_yield >= 1.0f)
            printf(" >>NO YIELD\n");
        else
            printf(" >>YIELDS!\n");
    }

    waitkey();
    return 0;
}
