/*
 * shaft.c - Shaft Design Calculator for TI-83+/84+
 *
 * Combined bending + torsion on a rotating shaft.
 * Von Mises equivalent stresses for fatigue.
 *
 * Input:
 *   M  = bending moment [N.mm]
 *   T  = torque [N.mm]
 *   d  = shaft diameter [mm]
 *   Kt = bending SCF, Kts = torsion SCF
 *   q  = notch sensitivity
 *   Sut, Sy, Se = material props [MPa]
 *
 * Computes:
 *   sigma_b = 32*M / (pi*d^3)        (bending stress)
 *   tau_t   = 16*T / (pi*d^3)        (torsion stress)
 *   Kf  = 1 + q*(Kt - 1)
 *   Kfs = 1 + q*(Kts - 1)
 *   sigma_a' = sqrt((Kf*sigma_b)^2 + 3*(Kfs*tau_t)^2)  (von Mises alt)
 *   sigma_m' = sqrt(sigma_bm^2 + 3*tau_tm^2)            (von Mises mean)
 *   n_Goodman = 1 / (sigma_a'/Se + sigma_m'/Sut)
 *
 * For fully-reversed bending + steady torsion (common case):
 *   sigma_a = Kf * sigma_b
 *   sigma_m = 0 (bending is reversed)
 *   tau_a   = 0 (torque is steady)
 *   tau_m   = Kfs * tau_t
 *
 * Build: tibuild.ps1 src\shaft.c SHAFT
 */

#include <stdio.h>
#include <math.h>
#include "ti84.h"

#define PI 3.14159f

int main(void)
{
    float M, T, d;
    int Kt10, Kts10, q10;
    float Kt, Kts, q_val;
    float Kf, Kfs;
    float sigma_b, tau_t;
    float sa_vm, sm_vm;
    float Sut, Sy, Se;
    float n_g, n_s, n_gerb;

    clrhome();
    printf("=SHAFT DESIGN=\n\n");

    /* --- Loading --- */
    M = (float)iinput("M[Nmm]", 50000);
    T = (float)iinput("T[Nmm]", 30000);
    d = (float)iinput("d[mm]", 25);

    if ((int)d == 0) {
        printf("ERR:d>0\n");
        waitkey();
        return 1;
    }

    /* --- Stress concentration --- */
    clrhome();
    printf("=CONC FACTORS=\n");
    printf("(x10: 20=2.0)\n\n");

    Kt10  = iinput("Kt*10", 20);   /* 20 = 2.0 */
    Kts10 = iinput("Kts*10", 15);  /* 15 = 1.5 */
    q10   = iinput("q*10", 8);     /* 8 = 0.8 */

    Kt  = (float)Kt10 / 10.0f;
    Kts = (float)Kts10 / 10.0f;
    q_val = (float)q10 / 10.0f;

    /* Kf = 1 + q*(Kt - 1) */
    Kf  = 1.0f + q_val * (Kt - 1.0f);
    Kfs = 1.0f + q_val * (Kts - 1.0f);

    /* --- Material --- */
    clrhome();
    printf("=MATERIAL=\n\n");

    Sut = (float)iinput("Sut", 400);
    Sy  = (float)iinput("Sy", 300);
    Se  = (float)iinput("Se", 107);

    /* --- Nominal stresses --- */
    /* sigma_b = 32M / (pi * d^3) */
    sigma_b = 32.0f * M / (PI * d * d * d);
    /* tau_t = 16T / (pi * d^3) */
    tau_t = 16.0f * T / (PI * d * d * d);

    /* --- Von Mises (reversed bending + steady torsion) --- */
    /* sigma_a' = sqrt((Kf*sigma_b)^2 + 3*(0)^2) = Kf*sigma_b */
    /* sigma_m' = sqrt(0^2 + 3*(Kfs*tau_t)^2) = sqrt(3)*Kfs*tau_t */
    sa_vm = Kf * sigma_b;
    sm_vm = 1.732f * Kfs * tau_t;

    /* --- Safety factors --- */
    /* Goodman: n = 1/(sa'/Se + sm'/Sut) */
    n_g = 1.0f / (sa_vm / Se + sm_vm / Sut);

    /* Soderberg: n = 1/(sa'/Se + sm'/Sy) */
    n_s = 1.0f / (sa_vm / Se + sm_vm / Sy);

    /* Gerber: quadratic solve */
    {
        float A_coeff = sa_vm / Se;
        float B_coeff = (sm_vm * sm_vm) / (Sut * Sut);
        if ((int)(B_coeff * 1000.0f) == 0)
            n_gerb = Se / sa_vm;
        else {
            float disc = A_coeff * A_coeff + 4.0f * B_coeff;
            n_gerb = (-A_coeff + sqrt(disc)) / (2.0f * B_coeff);
        }
    }

    /* --- Results page 1: stresses --- */
    clrhome();
    printf("=STRESSES=\n\n");
    printf("Sig_b=%d MPa\n", (int)sigma_b);
    printf("Tau_t=%d MPa\n", (int)tau_t);
    printf("Kf=%d%% Kfs=%d%%\n",
           (int)(Kf * 100.0f), (int)(Kfs * 100.0f));
    printf("\nVon Mises:\n");
    printf("Sa'=%d MPa\n", (int)sa_vm);
    printf("Sm'=%d MPa\n", (int)sm_vm);

    waitkey();

    /* --- Results page 2: safety --- */
    clrhome();
    printf("=SAFETY FACTOR=\n\n");
    printf("Goodman:\n");
    printf(" n=%d%%\n", (int)(n_g * 100.0f));
    printf("Soderberg:\n");
    printf(" n=%d%%\n", (int)(n_s * 100.0f));
    printf("Gerber:\n");
    printf(" n=%d%%\n", (int)(n_gerb * 100.0f));

    printf("\n");
    if (n_g >= 1.0f)
        printf(">>SAFE(GOOD)\n");
    else
        printf(">>UNSAFE\n");

    if (n_s >= 1.0f)
        printf(">>SAFE(SOD)\n");

    waitkey();
    return 0;
}
