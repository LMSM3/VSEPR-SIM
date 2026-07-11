/*
 * fatigue.c - S-N Fatigue Life Calculator for TI-83+/84+
 *
 * Native Z80 C program.  Compiles to .8xp via tibuild.
 *
 * Pipeline:
 *   1. Input sigma_max, sigma_min
 *   2. Decompose: sigma_m, sigma_a, R
 *   3. Input material: Sut, Se', ka, kb, kc
 *   4. Marin correction: Se = ka * kb * kc * Se'
 *   5. S-N curve: a = (f*Sut)^2/Se, b = -(1/3)*log10(f*Sut/Se)
 *   6. Cycles: N = (sigma_a / a)^(1/b)
 *   7. Goodman: n = 1/(sigma_a/Se + sigma_m/Sut)
 *
 * Build: tibuild.ps1 src\fatigue.c FATIGUE
 *
 * Z80 constraints applied:
 *   - float only (no double)
 *   - no == on floats (use int sentinel comparison)
 *   - minimal stack, short strings
 *   - getk() for keyboard polling
 */

#include <stdio.h>
#include <math.h>

/* z88dk keyboard polling */
extern int __LIB__ getk(void);

/* ------------------------------------------------------------------ */
/*  Input helper: prompt and read a float via integer input trick     */
/* ------------------------------------------------------------------ */

static float finput(const char *label, float def)
{
    int iv;
    float v;

    printf("%s(%d)?", label, (int)def);

    if (scanf("%d", &iv) != 1) {
        return def;
    }

    /* Sentinel: 999 = use default (integer compare, safe on Z80) */
    if (iv == 999) {
        printf("=%d\n", (int)def);
        return def;
    }

    v = (float)iv;
    return v;
}

/* Float input for Marin factors (values like 0.70) */
/* We input as integer percentage: 70 = 0.70 */
static float finput_pct(const char *label, int def_pct)
{
    int iv;

    printf("%s(%d%%)?", label, def_pct);

    if (scanf("%d", &iv) != 1) {
        return (float)def_pct / 100.0f;
    }

    if (iv == 999) {
        printf("=%d%%\n", def_pct);
        return (float)def_pct / 100.0f;
    }

    return (float)iv / 100.0f;
}

/* ------------------------------------------------------------------ */
/*  Wait for keypress                                                 */
/* ------------------------------------------------------------------ */

static void waitkey(void)
{
    printf("[KEY]");
    /* Wait for release of any held key */
    while (getk()) ;
    /* Wait for new press */
    while (!getk()) ;
    /* Wait for release (debounce) */
    while (getk()) ;
}

/* ------------------------------------------------------------------ */
/*  Main                                                              */
/* ------------------------------------------------------------------ */

int main(void)
{
    float smax, smin, sm, sa;
    float Sut, Sep, ka, kb, kc;
    float Se, f, a, b, N, n;
    int R_pct;

    /* Clear screen */
    printf("\x1b[2J");
    printf("=FATIGUE S-N=\n\n");

    /* --- Loading --- */
    smax = finput("Smax", 300.0f);
    smin = finput("Smin", 100.0f);

    if (smax < smin) {
        printf("ERR:MAX>=MIN\n");
        waitkey();
        return 1;
    }

    sm = (smax + smin) / 2.0f;
    sa = (smax - smin) / 2.0f;

    /* R as percentage to avoid float display issues */
    if ((int)smax != 0)
        R_pct = (int)((smin * 100.0f) / smax);
    else
        R_pct = 0;

    printf("\nSm=%d Sa=%d\n", (int)sm, (int)sa);
    printf("R=%d%%\n", R_pct);
    waitkey();

    /* --- Material --- */
    printf("\x1b[2J");
    printf("=MATERIAL=\n\n");

    Sut = finput("Sut", 400.0f);
    Sep = finput("Se'", 200.0f);
    ka  = finput_pct("ka", 70);
    kb  = finput_pct("kb", 85);
    kc  = finput_pct("kc", 90);

    /* --- Marin correction --- */
    Se = ka * kb * kc * Sep;

    /* --- S-N curve --- */
    f = 0.9f;
    a = (f * Sut) * (f * Sut) / Se;
    b = -(1.0f / 3.0f) * log10(f * Sut / Se);

    /* --- Cycles to failure --- */
    N = pow(sa / a, 1.0f / b);

    /* --- Goodman --- */
    if ((int)sm == 0)
        n = Se / sa;
    else
        n = 1.0f / (sa / Se + sm / Sut);

    /* --- Results --- */
    printf("\x1b[2J");
    printf("=RESULTS=\n\n");
    printf("Se=%d MPa\n", (int)Se);
    printf("a=%d\n", (int)a);

    printf("\nN=");
    /* Print N in readable form */
    if (N >= 1000000.0f) {
        printf(">1M cyc\n");
        printf(">>INF LIFE\n");
    } else if (N >= 1000.0f) {
        printf("%dK cyc\n", (int)(N / 1000.0f));
        printf(">>FINITE\n");
    } else {
        printf("%d cyc\n", (int)N);
        printf(">>LOW-CYC\n");
    }

    printf("\nn(Good)=");
    /* Print n as percentage for readability */
    printf("%d%%\n", (int)(n * 100.0f));
    if (n >= 1.0f)
        printf(">>SAFE\n");
    else
        printf(">>UNSAFE\n");

    waitkey();
    return 0;
}
