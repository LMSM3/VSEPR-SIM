/*
 * beam.c - Beam Stress Calculator for TI-83+/84+
 *
 * Computes bending stress, shear stress, and deflection
 * for common beam loading cases.
 *
 * Cases:
 *   1. Simply supported, center load
 *   2. Simply supported, uniform load
 *   3. Cantilever, end load
 *   4. Cantilever, uniform load
 *
 * Formulas:
 *   sigma_b = M * c / I          (bending stress)
 *   tau     = V * Q / (I * b)    (shear stress, rectangular)
 *   For round:  I = pi*d^4/64,  c = d/2
 *   For rect:   I = b*h^3/12,   c = h/2
 *
 * The bending stress output is the sigma_max that feeds
 * directly into FATIGUE as the loading input.
 *
 * Build: tibuild.ps1 src\beam.c BEAM
 */

#include <stdio.h>
#include <math.h>
#include "ti84.h"

/* pi constant */
#define PI 3.14159f

int main(void)
{
    int loadcase, xsec;
    float L, P, w;
    float d, bw, h;
    float I, c, M, V, sigma, tau, defl;

    clrhome();
    printf("=BEAM STRESS=\n\n");

    /* --- Load case --- */
    printf("LOAD CASE:\n");
    printf("1:SS CENTER\n");
    printf("2:SS UNIFORM\n");
    printf("3:CANT END\n");
    printf("4:CANT UNIF\n");
    loadcase = iinput("Case", 1);

    if (loadcase < 1 || loadcase > 4) {
        printf("ERR:1-4\n");
        waitkey();
        return 1;
    }

    /* --- Geometry --- */
    clrhome();
    printf("=GEOMETRY=\n\n");

    L = (float)iinput("L[mm]", 500);

    printf("\nCROSS SEC:\n");
    printf("1:ROUND\n");
    printf("2:RECT\n");
    xsec = iinput("XSec", 1);

    if (xsec == 1) {
        /* Round bar */
        d = (float)iinput("d[mm]", 25);
        I = PI * d * d * d * d / 64.0f;
        c = d / 2.0f;
        bw = d;  /* width at neutral axis for shear */
    } else {
        /* Rectangular */
        bw = (float)iinput("b[mm]", 20);
        h  = (float)iinput("h[mm]", 30);
        I = bw * h * h * h / 12.0f;
        c = h / 2.0f;
    }

    /* --- Loading --- */
    clrhome();
    printf("=LOADING=\n\n");

    switch (loadcase) {
    case 1: /* SS center point load */
    case 3: /* Cantilever end load */
        P = (float)iinput("P[N]", 1000);
        w = 0.0f;
        break;
    case 2: /* SS uniform */
    case 4: /* Cantilever uniform */
        w = (float)iinput("w[N/mm]", 10);
        P = 0.0f;
        break;
    default:
        P = 0.0f; w = 0.0f;
    }

    /* --- Compute M_max, V_max, deflection_max --- */
    /* All in N, mm -> stress in MPa (N/mm^2) */

    switch (loadcase) {
    case 1: /* SS, center: M=PL/4, V=P/2, d=PL^3/(48EI) */
        M = P * L / 4.0f;
        V = P / 2.0f;
        defl = P * L * L * L / (48.0f * 200000.0f * I);
        break;
    case 2: /* SS, uniform: M=wL^2/8, V=wL/2 */
        M = w * L * L / 8.0f;
        V = w * L / 2.0f;
        defl = 5.0f * w * L * L * L * L / (384.0f * 200000.0f * I);
        break;
    case 3: /* Cantilever, end: M=PL, V=P */
        M = P * L;
        V = P;
        defl = P * L * L * L / (3.0f * 200000.0f * I);
        break;
    case 4: /* Cantilever, uniform: M=wL^2/2, V=wL */
        M = w * L * L / 2.0f;
        V = w * L;
        defl = w * L * L * L * L / (8.0f * 200000.0f * I);
        break;
    default:
        M = 0; V = 0; defl = 0;
    }

    /* Bending stress: sigma = M*c/I  [N*mm * mm / mm^4 = N/mm^2 = MPa] */
    sigma = M * c / I;

    /* Shear stress at neutral axis (rectangular approx): tau = 3V/(2A) */
    if (xsec == 1) {
        /* Round: tau_max = 4V/(3A), A = pi*d^2/4 */
        float A = PI * d * d / 4.0f;
        tau = 4.0f * V / (3.0f * A);
    } else {
        /* Rect: tau_max = 3V/(2*b*h) */
        tau = 3.0f * V / (2.0f * bw * h);
    }

    /* --- Results --- */
    clrhome();
    printf("=BEAM RESULT=\n\n");
    printf("M=%d N.mm\n", (int)M);
    printf("V=%d N\n", (int)V);
    printf("\nSig=%d MPa\n", (int)sigma);
    printf("Tau=%d MPa\n", (int)tau);
    printf("Defl=%d um\n", (int)(defl * 1000.0f));
    printf("\n->FATIGUE:\n");
    printf("Smax=%d\n", (int)sigma);
    printf("Smin=%d\n", (int)(-sigma));

    waitkey();
    return 0;
}
