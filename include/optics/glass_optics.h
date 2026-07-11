/*
 * glass_optics.h — Physical and reflective characteristics of glass
 * =================================================================
 *
 * Permanent, hardcoded module implementing the ten optical physics
 * models for glass clarity, absorption, disorder, and thermal effects.
 *
 * Models implemented (section numbers match the theoretical document):
 *
 *   §1  Beer-Lambert absorption + complex refractive index
 *           beer_lambert_I()       — intensity vs path length
 *           alpha_from_kappa()     — absorption coefficient from κ
 *
 *   §2  Pair distribution function (PDF / RDF)
 *           pdf_g()                — g(r) from dN/dr and number density
 *
 *   §3  Optical indicatrix — isotropy / anisotropy
 *           indicatrix_delta_n()   — birefringence from axis indices
 *           indicatrix_is_spherical() — isotropy check
 *
 *   §4  Rayleigh + Mie scattering regimes
 *           rayleigh_I_rel()       — relative Rayleigh intensity vs λ
 *           scatter_regime()       — classify scatterer size vs wavelength
 *
 *   §5  Photoelastic (stress-optic) effect
 *           stress_birefringence() — Δn = C(σ1 − σ2)
 *
 *   §6  Urbach tail — temperature-dependent absorption edge
 *           urbach_alpha()         — exponential sub-gap absorption
 *
 *   §7  Varshni band-gap shift with temperature
 *           varshni_Eg()           — Eg(T) = Eg(0) − αT²/(T+β)
 *
 *   §8  Ligand-field / crystal-field splitting vs bond length
 *           delta_oct()            — Δ_oct ∝ 1/R^5 (Tanabe-Sugano scaling)
 *
 *   §9  Thermo-optic coefficient via Lorentz-Lorenz
 *           lorentz_lorenz_n()     — n from N, α_e
 *           thermo_optic_dn_dT()   — dn/dT from density + polarizability change
 *
 *   §10 Planck thermal emission with emissivity
 *           planck_L_lambda()      — spectral radiance L_λ(T)
 *           planck_peak_lambda()   — Wien displacement: λ_max = b/T
 *
 * Utility:
 *           glass_optics_clarity_score() — composite 0-1 clarity metric
 *           glass_optics_print_summary() — formatted summary to FILE*
 *
 * Physical constants are hardcoded and permanent.
 * All functions are pure — no global state, no heap allocation.
 *
 * VSEPR-SIM 3.0.0 — glass optics module.  DO NOT modify constants.
 */

#pragma once
#ifndef VSEPR_GLASS_OPTICS_H
#define VSEPR_GLASS_OPTICS_H

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * Physical constants — hardcoded, permanent
 * ====================================================================== */
#define GO_h    6.62607015e-34   /* Planck constant          J·s          */
#define GO_c    2.99792458e8     /* speed of light in vacuum m/s          */
#define GO_kB   1.380649e-23     /* Boltzmann constant       J/K          */
#define GO_PI   3.14159265358979323846
#define GO_WIEN 2.897771955e-3   /* Wien displacement const  m·K          */

/* Scatter regime classification */
typedef enum {
    GO_REGIME_RAYLEIGH = 0,  /* a << λ  (sub-wavelength)  */
    GO_REGIME_MIE      = 1,  /* a ~  λ  (Mie)             */
    GO_REGIME_GEOMETRIC = 2  /* a >> λ  (geometric optics) */
} go_scatter_regime_t;

/* =========================================================================
 * §1  Beer-Lambert absorption
 * ====================================================================== */

/*
 * beer_lambert_I — transmitted intensity at depth x (m).
 *
 *   I(x) = I0 · exp(-α · x)
 *
 * @param I0     Incident intensity (any consistent unit)
 * @param alpha  Absorption coefficient (m⁻¹)
 * @param x      Path length (m)
 * @return       Transmitted intensity
 */
static inline double beer_lambert_I(double I0, double alpha, double x)
{
    return I0 * exp(-alpha * x);
}

/*
 * alpha_from_kappa — absorption coefficient from extinction coefficient κ.
 *
 *   α(λ) = 4π κ / λ
 *
 * @param kappa  Extinction coefficient (dimensionless)
 * @param lambda Wavelength (m)
 * @return       Absorption coefficient (m⁻¹)
 */
static inline double alpha_from_kappa(double kappa, double lambda)
{
    return (4.0 * GO_PI * kappa) / lambda;
}

/* =========================================================================
 * §2  Pair distribution function  g(r)
 * ====================================================================== */

/*
 * pdf_g — radial distribution function value at separation r.
 *
 *   g(r) = (1 / (4π r² ρ)) · (dN/dr)
 *
 * @param dN_dr       Shell atom count gradient (atoms / m)
 * @param r           Radial distance (m)
 * @param rho_number  Number density (atoms / m³)
 * @return            g(r) (dimensionless)
 */
static inline double pdf_g(double dN_dr, double r, double rho_number)
{
    double denom = 4.0 * GO_PI * r * r * rho_number;
    if (denom <= 0.0) return 0.0;
    return dN_dr / denom;
}

/* =========================================================================
 * §3  Optical indicatrix
 * ====================================================================== */

/*
 * indicatrix_delta_n — birefringence between two principal refractive indices.
 *
 * @param n1  First principal index
 * @param n2  Second principal index
 * @return    |n1 - n2|
 */
static inline double indicatrix_delta_n(double n1, double n2)
{
    return fabs(n1 - n2);
}

/*
 * indicatrix_is_spherical — returns true if all three principal indices match
 *                           within tolerance tol (isotropic medium).
 *
 * @param nx, ny, nz  Principal refractive indices
 * @param tol         Equality tolerance
 */
static inline bool indicatrix_is_spherical(double nx, double ny, double nz,
                                            double tol)
{
    return (fabs(nx - ny) < tol) && (fabs(ny - nz) < tol);
}

/* =========================================================================
 * §4  Rayleigh scattering + scatter regime
 * ====================================================================== */

/*
 * rayleigh_I_rel — Rayleigh scattered intensity relative to a reference λ0.
 *
 *   I_scat ∝ λ⁻⁴  →  I(λ) / I(λ0) = (λ0/λ)^4
 *
 * @param lambda    Wavelength of interest (m)
 * @param lambda0   Reference wavelength (m)
 * @return          Relative intensity (dimensionless)
 */
static inline double rayleigh_I_rel(double lambda, double lambda0)
{
    double ratio = lambda0 / lambda;
    return ratio * ratio * ratio * ratio;
}

/*
 * scatter_regime — classify scatterer radius a vs wavelength λ.
 *
 *   size parameter x = 2πa/λ
 *   x < 0.3  → Rayleigh
 *   x < 50   → Mie
 *   else     → Geometric
 */
static inline go_scatter_regime_t scatter_regime(double a, double lambda)
{
    double x = 2.0 * GO_PI * a / lambda;
    if (x < 0.3)  return GO_REGIME_RAYLEIGH;
    if (x < 50.0) return GO_REGIME_MIE;
    return GO_REGIME_GEOMETRIC;
}

/* =========================================================================
 * §5  Photoelastic (stress-optic) effect
 * ====================================================================== */

/*
 * stress_birefringence — induced birefringence from principal stress difference.
 *
 *   Δn = C · (σ1 − σ2)
 *
 * @param C      Stress-optic coefficient (Pa⁻¹)
 * @param sigma1 First principal stress (Pa)
 * @param sigma2 Second principal stress (Pa)
 * @return       Induced birefringence Δn (dimensionless)
 */
static inline double stress_birefringence(double C,
                                           double sigma1, double sigma2)
{
    return C * (sigma1 - sigma2);
}

/* =========================================================================
 * §6  Urbach tail — temperature-dependent sub-gap absorption
 * ====================================================================== */

/*
 * urbach_alpha — Urbach rule absorption coefficient.
 *
 *   α(E, T) = α0 · exp[(E − E0) / E_U(T)]
 *
 * @param alpha0  Pre-exponential (m⁻¹)
 * @param E       Photon energy (eV)
 * @param E0      Band-edge reference energy (eV)
 * @param EU_T    Urbach energy at temperature T (eV)
 * @return        Absorption coefficient (m⁻¹)
 */
static inline double urbach_alpha(double alpha0, double E,
                                   double E0, double EU_T)
{
    return alpha0 * exp((E - E0) / EU_T);
}

/* =========================================================================
 * §7  Varshni band-gap temperature dependence
 * ====================================================================== */

/*
 * varshni_Eg — band gap at temperature T.
 *
 *   Eg(T) = Eg(0) − α T² / (T + β)
 *
 * @param Eg0   Band gap at 0 K (eV)
 * @param alpha Varshni α parameter (eV/K)
 * @param beta  Varshni β parameter (K)
 * @param T     Temperature (K)
 * @return      Band gap Eg(T) (eV)
 */
static inline double varshni_Eg(double Eg0, double alpha,
                                 double beta, double T)
{
    return Eg0 - (alpha * T * T) / (T + beta);
}

/* =========================================================================
 * §8  Ligand-field splitting — crystal-field Δ_oct scaling
 * ====================================================================== */

/*
 * delta_oct — relative crystal-field splitting vs metal-ligand distance R.
 *
 *   Δ_oct ∝ 1 / R^5
 *
 * Returns Δ_oct(R) / Δ_oct(R0), i.e. the ratio relative to a reference
 * distance R0.  Multiply by the known Δ at R0 to get an absolute value.
 *
 * @param R   Metal-ligand distance (any consistent unit)
 * @param R0  Reference distance (same unit as R)
 * @return    Dimensionless scaling factor
 */
static inline double delta_oct(double R, double R0)
{
    double r5   = R  * R  * R  * R  * R;
    double r05  = R0 * R0 * R0 * R0 * R0;
    if (r5 <= 0.0) return 0.0;
    return r05 / r5;
}

/* =========================================================================
 * §9  Thermo-optic / Lorentz-Lorenz
 * ====================================================================== */

/*
 * lorentz_lorenz_n — refractive index from Lorentz-Lorenz relation.
 *
 *   (n²-1)/(n²+2) = (4π/3) · N · α_e
 *
 * Solved for n:
 *   Let  f = (4π/3) · N · α_e
 *   n² = (1 + 2f) / (1 - f)
 *
 * @param N_density   Number density (m⁻³)
 * @param alpha_e     Electronic polarizability (C·m²/V, SI)
 * @return            Refractive index n  (returns 0 on invalid input)
 */
static inline double lorentz_lorenz_n(double N_density, double alpha_e)
{
    double f = (4.0 * GO_PI / 3.0) * N_density * alpha_e;
    if (f >= 1.0 || f < 0.0) return 0.0;
    double n2 = (1.0 + 2.0 * f) / (1.0 - f);
    return sqrt(n2);
}

/*
 * thermo_optic_dn_dT — approximate dn/dT via Lorentz-Lorenz differencing.
 *
 * Uses finite difference over a small temperature step dT:
 *
 *   dn/dT ≈ [n(N+dN, α_e+dα_e) − n(N, α_e)] / dT
 *
 * @param N          Number density at T (m⁻³)
 * @param dN_dT      d(number density)/dT  (m⁻³ K⁻¹)
 * @param alpha_e    Electronic polarizability at T
 * @param dalpha_dT  d(α_e)/dT
 * @param dT         Temperature step for finite difference (K)
 * @return           dn/dT (K⁻¹)
 */
static inline double thermo_optic_dn_dT(double N, double dN_dT,
                                         double alpha_e, double dalpha_dT,
                                         double dT)
{
    double n0 = lorentz_lorenz_n(N, alpha_e);
    double n1 = lorentz_lorenz_n(N + dN_dT * dT,
                                  alpha_e + dalpha_dT * dT);
    return (n1 - n0) / dT;
}

/* =========================================================================
 * §10  Planck thermal emission
 * ====================================================================== */

/*
 * planck_L_lambda — spectral radiance with emissivity.
 *
 *   L_λ(T) = ε(λ,T) · [2hc² / λ⁵] · 1/(exp(hc/λkT) − 1)
 *
 * @param lambda     Wavelength (m)
 * @param T          Temperature (K)
 * @param emissivity Spectral emissivity ε ∈ [0,1]
 * @return           Spectral radiance (W m⁻³ sr⁻¹)
 */
static inline double planck_L_lambda(double lambda, double T,
                                      double emissivity)
{
    if (T <= 0.0 || lambda <= 0.0) return 0.0;
    double hc_lkT = (GO_h * GO_c) / (lambda * GO_kB * T);
    double l5 = lambda * lambda * lambda * lambda * lambda;
    double B  = (2.0 * GO_h * GO_c * GO_c) / (l5 * (exp(hc_lkT) - 1.0));
    return emissivity * B;
}

/*
 * planck_peak_lambda — Wien displacement: wavelength of maximum emission.
 *
 *   λ_max = b / T,  b = 2.897771955e-3 m·K
 *
 * @param T   Temperature (K)
 * @return    Peak wavelength (m)
 */
static inline double planck_peak_lambda(double T)
{
    if (T <= 0.0) return 0.0;
    return GO_WIEN / T;
}

/* =========================================================================
 * Composite clarity score
 *
 * Returns a dimensionless clarity index in [0, 1] combining:
 *   - transmission at path length x:  T = exp(-α x)
 *   - relative Rayleigh scatter suppression (λ_vis / λ_ref)^4 normalised
 *   - stress birefringence penalty
 *
 * All inputs in SI base units.
 *
 * @param alpha          Absorption coefficient (m⁻¹)
 * @param x              Path length (m)
 * @param lambda_vis     Visible wavelength of interest (m)
 * @param lambda_ref     Reference (shortest visible, e.g. 380e-9 m)
 * @param C_stress_optic Stress-optic coefficient (Pa⁻¹)
 * @param delta_sigma    Principal stress difference |σ1 − σ2| (Pa)
 * @return               Clarity index in [0,1]; 1.0 = perfect clarity
 * ====================================================================== */
static inline double glass_optics_clarity_score(
        double alpha, double x,
        double lambda_vis, double lambda_ref,
        double C_stress_optic, double delta_sigma)
{
    double T_abs    = exp(-alpha * x);
    double scatter  = 1.0 - fmin(1.0, rayleigh_I_rel(lambda_vis, lambda_ref)
                                       / rayleigh_I_rel(lambda_ref, lambda_ref));
    double bire     = fabs(stress_birefringence(C_stress_optic, delta_sigma, 0.0));
    double bire_pen = fmax(0.0, 1.0 - bire * 1.0e6); /* 1e-6 Δn → zero penalty */

    double score = T_abs * 0.6 + scatter * 0.2 + bire_pen * 0.2;
    return fmin(1.0, fmax(0.0, score));
}

/* =========================================================================
 * Summary printer
 * ====================================================================== */
static inline void glass_optics_print_summary(FILE *fp,
        double lambda_m, double T_K,
        double alpha, double kappa,
        double n, double emissivity)
{
    double I_ratio   = exp(-alpha * 1e-3);          /* 1 mm path */
    double peak_nm   = planck_peak_lambda(T_K) * 1e9;
    double L         = planck_L_lambda(lambda_m, T_K, emissivity);

    fprintf(fp, "═══════════════════════════════════════════════════\n");
    fprintf(fp, "  VSEPR-SIM Glass Optics Summary\n");
    fprintf(fp, "═══════════════════════════════════════════════════\n");
    fprintf(fp, "  λ              = %.1f nm\n",  lambda_m * 1e9);
    fprintf(fp, "  T              = %.1f K\n",   T_K);
    fprintf(fp, "  n (real)       = %.4f\n",     n);
    fprintf(fp, "  κ (extinction) = %.4e\n",     kappa);
    fprintf(fp, "  α              = %.4e m⁻¹\n", alpha);
    fprintf(fp, "  T(1 mm)        = %.6f\n",     I_ratio);
    fprintf(fp, "  Planck peak    = %.1f nm\n",  peak_nm);
    fprintf(fp, "  L_λ (ε=%.2f)  = %.4e W/m³/sr\n", emissivity, L);
    fprintf(fp, "═══════════════════════════════════════════════════\n");
}

#ifdef __cplusplus
}
#endif

#endif /* VSEPR_GLASS_OPTICS_H */
