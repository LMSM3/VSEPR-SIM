/*
 * mohr.c - Mohr's Circle Calculator for TI-83+/84+
 *
 * Given a 2D stress state (sigma_x, sigma_y, tau_xy),
 * computes principal stresses and max shear.
 *
 * Formulas:
 *   sigma_avg = (sigma_x + sigma_y) / 2
 *   R = sqrt(((sigma_x - sigma_y)/2)^2 + tau_xy^2)
 *   sigma_1 = sigma_avg + R          (max principal)
 *   sigma_2 = sigma_avg - R          (min principal)
 *   tau_max = R
 *   theta_p = (1/2) * atan2(2*tau_xy, sigma_x - sigma_y)
 *
 * Output feeds into FATIGUE (sigma_1 as sigma_max)
 * and into SHAFT (verify combined stress state).
 *
 * Build: tibuild.ps1 src\mohr.c MOHR
 */

#include <stdio.h>
#include <math.h>
#include "ti84.h"

#define PI 3.14159f

int main(void)
{
    float sx, sy, txy;
    float savg, R, s1, s2, tmax;
    float theta_rad, theta_deg;

    clrhome();
    printf("=MOHR CIRCLE=\n\n");

    /* --- Input stress state --- */
    sx  = (float)iinput("Sx[MPa]", 100);
    sy  = (float)iinput("Sy[MPa]", 0);
    txy = (float)iinput("Txy[MPa]", 50);

    /* --- Compute --- */
    savg = (sx + sy) / 2.0f;

    {
        float dx = (sx - sy) / 2.0f;
        R = sqrt(dx * dx + txy * txy);
    }

    s1 = savg + R;
    s2 = savg - R;
    tmax = R;

    /* Principal angle */
    {
        float denom = sx - sy;
        if ((int)denom == 0 && (int)txy == 0)
            theta_rad = 0.0f;
        else
            theta_rad = 0.5f * atan2(2.0f * txy, sx - sy);
    }
    theta_deg = theta_rad * 180.0f / PI;

    /* --- Results --- */
    clrhome();
    printf("=MOHR RESULT=\n\n");
    printf("Savg=%d MPa\n", (int)savg);
    printf("R=%d MPa\n", (int)R);
    printf("\nS1=%d MPa\n", (int)s1);
    printf("S2=%d MPa\n", (int)s2);
    printf("Tmax=%d MPa\n", (int)tmax);
    printf("Tp=%d deg\n", (int)theta_deg);

    /* Von Mises equivalent (plane stress) */
    /* sigma_vm = sqrt(s1^2 - s1*s2 + s2^2) */
    {
        float svm = sqrt(s1*s1 - s1*s2 + s2*s2);
        printf("\nSvm=%d MPa\n", (int)svm);
    }

    waitkey();
    return 0;
}
