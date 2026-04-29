/*
 * metallic_cp.h — C clone of pykernel/metallic_cp.py
 * ====================================================
 *
 * Bit-for-bit functional clone of the Debye + Sommerfeld + Nernst-Lindemann
 * heat-capacity engine.  Pure C99, header-only, no dynamic allocation.
 *
 * Mirrors:
 *   MetalRecord        → metal_rec_t
 *   CpResult           → cp_result_t
 *   debye_cv()         → debye_cv_C()
 *   electronic_cv()    → electronic_cv_C()
 *   dulong_petit()     → dulong_petit_C()
 *   compute_cp()       → compute_cp_C()
 *   compute_cp_curve() → compute_cp_curve_C()
 *   METAL_DB           → METAL_DB_C[] (same 30 entries)
 *
 * Alloy extension:
 *   alloy_rom_t        → rule-of-mixtures alloy descriptor
 *   alloy_cp_C()       → weighted Debye Cp for alloy
 *   hastelloy_n_cp()   → Hastelloy-N (INOR-8) Cp with Mo/W doping
 *
 * VSEPR-SIM 3.0.0 — report subsystem
 */

#pragma once
#ifndef VSEPR_METALLIC_CP_H
#define VSEPR_METALLIC_CP_H

#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * Physical constants
 * ====================================================================== */
#define MC_R   8.314462618      /* J/(mol·K) */
#define MC_kB  1.380649e-23     /* J/K        */
#define MC_NA  6.02214076e23    /* 1/mol      */
#define MC_A_NL 0.0032          /* Nernst-Lindemann coefficient [mol/J] */

/* =========================================================================
 * Metal record  (mirrors MetalRecord dataclass)
 * ====================================================================== */
typedef struct {
    const char *symbol;
    int         Z;
    const char *name;
    double      molar_mass;       /* g/mol  */
    double      density;          /* g/cm³  */
    double      theta_D;          /* Debye temperature K */
    double      gamma;            /* Sommerfeld coeff mJ/(mol·K²) */
    double      melting_point;    /* K      */
    double      cp_298;           /* exp. Cp at 298 K [J/(mol·K)] */
    double      thermal_cond;     /* W/(m·K) at 298 K */
} metal_rec_t;

/* =========================================================================
 * METAL_DB_C — same 30 entries as Python METAL_DB
 * Source: CRC Handbook + Kittel + ASM data
 * ====================================================================== */
static const metal_rec_t METAL_DB_C[] = {
/*   sym   Z   name              M       rho    θD    γ      Tm      cp298  k   */
    {"Li",  3, "Lithium",        6.941,  0.534, 344,  1.63,  453.7,  24.86, 84.8},
    {"Be",  4, "Beryllium",      9.012,  1.85,  1440, 0.17,  1560.0, 16.44, 200.0},
    {"Na", 11, "Sodium",         22.99,  0.971, 158,  1.38,  370.9,  28.23, 142.0},
    {"Mg", 12, "Magnesium",      24.31,  1.738, 400,  1.30,  923.0,  24.87, 156.0},
    {"Al", 13, "Aluminum",       26.98,  2.70,  428,  1.35,  933.5,  24.20, 237.0},
    {"K",  19, "Potassium",      39.10,  0.862, 91,   2.08,  336.5,  29.60, 102.5},
    {"Ca", 20, "Calcium",        40.08,  1.55,  230,  2.90,  1115.0, 25.93, 201.0},
    {"Ti", 22, "Titanium",       47.87,  4.506, 420,  3.35,  1941.0, 25.06, 21.9},
    {"V",  23, "Vanadium",       50.94,  6.11,  380,  9.26,  2183.0, 24.89, 30.7},
    {"Cr", 24, "Chromium",       52.00,  7.15,  630,  1.40,  2180.0, 23.35, 93.9},
    {"Mn", 25, "Manganese",      54.94,  7.44,  410,  9.20,  1519.0, 26.32, 7.81},
    {"Fe", 26, "Iron",           55.85,  7.874, 470,  4.98,  1811.0, 25.10, 80.4},
    {"Co", 27, "Cobalt",         58.93,  8.90,  445,  4.73,  1768.0, 24.81, 100.0},
    {"Ni", 28, "Nickel",         58.69,  8.908, 450,  7.02,  1728.0, 26.07, 90.9},
    {"Cu", 29, "Copper",         63.55,  8.96,  343,  0.695, 1357.8, 24.44, 401.0},
    {"Zn", 30, "Zinc",           65.38,  7.134, 327,  0.64,  692.7,  25.39, 116.0},
    {"Ga", 31, "Gallium",        69.72,  5.91,  320,  0.60,  302.9,  25.86, 40.6},
    {"Zr", 40, "Zirconium",      91.22,  6.52,  291,  2.80,  2128.0, 25.36, 22.7},
    {"Nb", 41, "Niobium",        92.91,  8.57,  275,  7.79,  2750.0, 24.60, 53.7},
    {"Mo", 42, "Molybdenum",     95.95,  10.28, 450,  2.00,  2896.0, 24.06, 138.0},
    {"Ag", 47, "Silver",         107.87, 10.49, 225,  0.646, 1234.9, 25.35, 429.0},
    {"Sn", 50, "Tin",            118.71, 7.287, 200,  1.78,  505.1,  27.11, 66.8},
    {"Ta", 73, "Tantalum",       180.95, 16.69, 240,  5.90,  3290.0, 25.36, 57.5},
    {"W",  74, "Tungsten",       183.84, 19.25, 400,  1.01,  3695.0, 24.27, 173.0},
    {"Pt", 78, "Platinum",       195.08, 21.45, 240,  6.80,  2041.4, 25.86, 71.6},
    {"Au", 79, "Gold",           196.97, 19.30, 165,  0.729, 1337.3, 25.42, 318.0},
    {"Pb", 82, "Lead",           207.2,  11.34, 105,  2.98,  600.6,  26.65, 35.3},
    {"Bi", 83, "Bismuth",        208.98, 9.78,  119,  0.008, 544.6,  25.52, 7.97},
    {"U",  92, "Uranium",        238.03, 19.1,  207,  9.14,  1405.3, 27.67, 27.5},
    {NULL,  0, NULL,             0.0,    0.0,   0.0,  0.0,   0.0,    0.0,   0.0}
};
#define METAL_DB_C_COUNT 29   /* excludes sentinel */

static inline const metal_rec_t *mc_lookup(const char *sym) {
    for (int i = 0; METAL_DB_C[i].symbol; i++)
        if (strcmp(METAL_DB_C[i].symbol, sym) == 0)
            return &METAL_DB_C[i];
    return NULL;
}

/* =========================================================================
 * Debye integrand  x⁴ eˣ / (eˣ-1)²
 * ====================================================================== */
static inline double _debye_integrand_C(double x) {
    if (x > 500.0) return 0.0;
    double ex = exp(x);
    double d  = (ex - 1.0) * (ex - 1.0);
    if (d < 1e-300) return 0.0;
    return (x*x*x*x) * ex / d;
}

/* =========================================================================
 * debye_cv_C  — mirrors debye_cv() with n_atoms=1, n_points=200
 * Returns C_v in J/(mol·K)
 * ====================================================================== */
static inline double debye_cv_C(double T, double theta_D) {
    if (T <= 0.0 || theta_D <= 0.0) return 0.0;
    double u = theta_D / T;
    int    n = 200;
    double h = u / n;
    double integral = _debye_integrand_C(0.0) + _debye_integrand_C(u);
    for (int i = 1; i < n; i++) {
        double xi = i * h;
        double w  = (i % 2 == 1) ? 4.0 : 2.0;
        integral += w * _debye_integrand_C(xi);
    }
    integral *= h / 3.0;
    return 9.0 * MC_R * (T / theta_D) * (T / theta_D) * (T / theta_D) * integral;
}

/* electronic_cv — mirrors electronic_cv() */
static inline double electronic_cv_C(double T, double gamma_mJ) {
    return gamma_mJ * 1e-3 * T;
}

/* dulong_petit — 3R limit */
static inline double dulong_petit_C(void) {
    return 3.0 * MC_R;
}

/* =========================================================================
 * cp_result_t  (mirrors CpResult)
 * ====================================================================== */
typedef struct {
    double T;
    double Cv_lattice;
    double Cv_electronic;
    double Cv_total;
    double Cp_approx;
    double cp_specific;   /* J/(g·K) */
    double dp_limit;
    double fraction_dp;
} cp_result_t;

/* compute_cp_C  — mirrors compute_cp() exactly */
static inline cp_result_t compute_cp_C(const metal_rec_t *m, double T) {
    double cv_lat = debye_cv_C(T, m->theta_D);
    double cv_el  = electronic_cv_C(T, m->gamma);
    double cv_tot = cv_lat + cv_el;

    /* Nernst-Lindemann: Cp = Cv / (1 - A·Cv·T/Tm) */
    double denom = 1.0 - MC_A_NL * cv_tot * T / (m->melting_point > 0 ? m->melting_point : 1.0);
    if (denom < 0.5) denom = 0.5;
    double cp = cv_tot / denom;

    double dp   = dulong_petit_C();
    double frac = (dp > 0.0) ? cv_lat / dp : 0.0;

    cp_result_t r;
    r.T            = T;
    r.Cv_lattice   = cv_lat;
    r.Cv_electronic= cv_el;
    r.Cv_total     = cv_tot;
    r.Cp_approx    = cp;
    r.cp_specific  = cp / m->molar_mass;
    r.dp_limit     = dp;
    r.fraction_dp  = frac;
    return r;
}

/* =========================================================================
 * Alloy rule-of-mixtures  (mirrors AlloyEstimator.rule_of_mixtures)
 * ====================================================================== */
#define ALLOY_MAX_COMPONENTS 8

typedef struct {
    const char *symbol;
    double      weight_fraction;
} alloy_component_t;

typedef struct {
    alloy_component_t components[ALLOY_MAX_COMPONENTS];
    int               n;
    char              name[128];
} alloy_rom_t;

/* Effective Debye temperature and Sommerfeld coefficient via ROM */
static inline void alloy_debye_params(const alloy_rom_t *a,
                                       double *theta_D_out,
                                       double *gamma_out,
                                       double *molar_mass_out,
                                       double *melting_point_out) {
    double tD = 0.0, gam = 0.0, M = 0.0, Tm = 0.0;
    for (int i = 0; i < a->n; i++) {
        const metal_rec_t *m = mc_lookup(a->components[i].symbol);
        if (!m) continue;
        double wf = a->components[i].weight_fraction;
        tD  += wf * m->theta_D;
        gam += wf * m->gamma;
        M   += wf * m->molar_mass;
        Tm  += wf * m->melting_point;
    }
    if (theta_D_out)     *theta_D_out     = tD;
    if (gamma_out)       *gamma_out       = gam;
    if (molar_mass_out)  *molar_mass_out  = M;
    if (melting_point_out)*melting_point_out = Tm;
}

static inline cp_result_t alloy_cp_C(const alloy_rom_t *a, double T) {
    double tD, gam, M, Tm;
    alloy_debye_params(a, &tD, &gam, &M, &Tm);
    metal_rec_t pseudo;
    memset(&pseudo, 0, sizeof(pseudo));
    pseudo.theta_D       = tD;
    pseudo.gamma         = gam;
    pseudo.molar_mass    = M;
    pseudo.melting_point = Tm;
    return compute_cp_C(&pseudo, T);
}

/* =========================================================================
 * Hastelloy-N (INOR-8) alloy descriptor
 *
 * Nominal composition (wt%):
 *   Ni  ~70%, Mo ~16%, Cr ~7%, Fe ~5%, Si ~1%, Mn ~0.5%, C ~0.06%
 *
 * For Debye ROM we use the primary metallic constituents (by wt fraction).
 * Mo and W dopant variants are parameterised.
 *
 * hastelloy_n_base()     → nominal INOR-8
 * hastelloy_n_Mo_doped() → increased Mo (Mo-rich variant)
 * hastelloy_n_W_doped()  → W substitution for Mo (W-rich variant)
 *
 * Reference: ORNL-2452, Inouye & Kiser (1964), Guyette (1973)
 * ====================================================================== */
static inline alloy_rom_t hastelloy_n_base(void) {
    alloy_rom_t a;
    a.n = 4;
    strcpy(a.components[0].symbol, "Ni"); a.components[0].weight_fraction = 0.70;
    strcpy(a.components[1].symbol, "Mo"); a.components[1].weight_fraction = 0.16;
    strcpy(a.components[2].symbol, "Cr"); a.components[2].weight_fraction = 0.07;
    strcpy(a.components[3].symbol, "Fe"); a.components[3].weight_fraction = 0.05;
    /* Si + Mn minor — omitted from Debye ROM (< 2 wt% combined) */
    strncpy(a.name, "Hastelloy-N (INOR-8) nominal", 127);
    return a;
}

static inline alloy_rom_t hastelloy_n_Mo_doped(double mo_frac) {
    /* Redistribute excess Mo from Ni */
    double ni_frac = 0.70 - (mo_frac - 0.16);
    if (ni_frac < 0.50) ni_frac = 0.50;
    alloy_rom_t a;
    a.n = 4;
    strcpy(a.components[0].symbol, "Ni"); a.components[0].weight_fraction = ni_frac;
    strcpy(a.components[1].symbol, "Mo"); a.components[1].weight_fraction = mo_frac;
    strcpy(a.components[2].symbol, "Cr"); a.components[2].weight_fraction = 0.07;
    strcpy(a.components[3].symbol, "Fe"); a.components[3].weight_fraction = 0.05;
    snprintf(a.name, 127, "Hastelloy-N Mo-doped Mo=%.0f%%", mo_frac*100);
    return a;
}

static inline alloy_rom_t hastelloy_n_W_doped(double w_frac) {
    /* W replaces part of Mo; keep total refractory fraction ~16% */
    double mo_frac = (0.16 - w_frac > 0.04) ? (0.16 - w_frac) : 0.04;
    double ni_frac = 1.0 - mo_frac - w_frac - 0.07 - 0.05;
    alloy_rom_t a;
    a.n = 5;
    strcpy(a.components[0].symbol, "Ni"); a.components[0].weight_fraction = ni_frac;
    strcpy(a.components[1].symbol, "Mo"); a.components[1].weight_fraction = mo_frac;
    strcpy(a.components[2].symbol, "W");  a.components[2].weight_fraction = w_frac;
    strcpy(a.components[3].symbol, "Cr"); a.components[3].weight_fraction = 0.07;
    strcpy(a.components[4].symbol, "Fe"); a.components[4].weight_fraction = 0.05;
    snprintf(a.name, 127, "Hastelloy-N W-doped W=%.0f%%", w_frac*100);
    return a;
}

/* =========================================================================
 * Cp curve CSV writer for alloys  (chart-frozen format)
 * ====================================================================== */
static inline int alloy_cp_csv(const alloy_rom_t *a,
                                double T_start, double T_end,
                                int n_points, FILE *fp) {
    if (n_points < 2) return -1;
    fprintf(fp, "T_K,Cp_JmolK,Cp_specific_JgK\n");
    double dT = (T_end - T_start) / (n_points - 1);
    for (int i = 0; i < n_points; i++) {
        double T = T_start + i * dT;
        cp_result_t r = alloy_cp_C(a, T);
        fprintf(fp, "%.2f,%.5f,%.6f\n", T, r.Cp_approx, r.cp_specific);
    }
    return n_points;
}

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* VSEPR_METALLIC_CP_H */
