"""
glass_optics.py — Physical and reflective characteristics of glass
==================================================================

Python authority for the glass optics module.
Mirrors include/optics/glass_optics.h exactly.

Implements all ten optical physics models:

    §1  Beer-Lambert absorption + complex refractive index
    §2  Pair distribution function g(r)
    §3  Optical indicatrix — isotropy / anisotropy
    §4  Rayleigh + Mie scatter regimes
    §5  Photoelastic stress-optic effect
    §6  Urbach tail — temperature-dependent absorption edge
    §7  Varshni band-gap temperature shift
    §8  Ligand-field / crystal-field Δ_oct vs bond length
    §9  Thermo-optic coefficient via Lorentz-Lorenz
    §10 Planck thermal emission with emissivity

Utility:
    clarity_score()     — composite 0–1 clarity metric
    print_summary()     — formatted summary to stdout or file

All functions are pure — no global state, no I/O side effects.
Physical constants are hardcoded and permanent.

VSEPR-SIM 3.0.0 — glass optics module.  DO NOT modify constants.
"""

from __future__ import annotations

import math
from enum import IntEnum
from typing import Tuple

# ============================================================================
# Physical constants — hardcoded, permanent
# ============================================================================

H_PLANCK   = 6.62607015e-34    # Planck constant          J·s
C_LIGHT    = 2.99792458e8      # speed of light in vacuum m/s
K_BOLTZMANN = 1.380649e-23     # Boltzmann constant       J/K
PI         = math.pi
WIEN_B     = 2.897771955e-3    # Wien displacement const  m·K


# ============================================================================
# Scatter regime classification
# ============================================================================

class ScatterRegime(IntEnum):
    RAYLEIGH  = 0   # a << λ  (sub-wavelength)
    MIE       = 1   # a ~  λ
    GEOMETRIC = 2   # a >> λ


# ============================================================================
# §1  Beer-Lambert absorption
# ============================================================================

def beer_lambert_I(I0: float, alpha: float, x: float) -> float:
    """
    Transmitted intensity at depth x.

        I(x) = I0 · exp(−α · x)

    Args:
        I0:    Incident intensity (any consistent unit)
        alpha: Absorption coefficient (m⁻¹)
        x:     Path length (m)

    Returns:
        Transmitted intensity (same unit as I0)
    """
    return I0 * math.exp(-alpha * x)


def alpha_from_kappa(kappa: float, lambda_m: float) -> float:
    """
    Absorption coefficient from extinction coefficient κ.

        α(λ) = 4π κ / λ

    Args:
        kappa:    Extinction coefficient (dimensionless)
        lambda_m: Wavelength (m)

    Returns:
        Absorption coefficient α (m⁻¹)
    """
    return (4.0 * PI * kappa) / lambda_m


# ============================================================================
# §2  Pair distribution function g(r)
# ============================================================================

def pdf_g(dN_dr: float, r: float, rho_number: float) -> float:
    """
    Pair distribution function value at separation r.

        g(r) = (1 / (4π r² ρ)) · (dN/dr)

    Args:
        dN_dr:      Shell atom count gradient (atoms/m)
        r:          Radial distance (m)
        rho_number: Number density (atoms/m³)

    Returns:
        g(r) (dimensionless)
    """
    denom = 4.0 * PI * r * r * rho_number
    if denom <= 0.0:
        return 0.0
    return dN_dr / denom


# ============================================================================
# §3  Optical indicatrix
# ============================================================================

def indicatrix_delta_n(n1: float, n2: float) -> float:
    """Birefringence between two principal refractive indices: |n1 − n2|."""
    return abs(n1 - n2)


def indicatrix_is_spherical(nx: float, ny: float, nz: float,
                             tol: float = 1e-6) -> bool:
    """
    Returns True if all three principal indices match within tol.
    An isotropic medium has a spherical indicatrix.
    """
    return abs(nx - ny) < tol and abs(ny - nz) < tol


# ============================================================================
# §4  Rayleigh scattering + scatter regime
# ============================================================================

def rayleigh_I_rel(lambda_m: float, lambda0_m: float) -> float:
    """
    Rayleigh scattered intensity relative to reference wavelength λ0.

        I_scat ∝ λ⁻⁴  →  I(λ)/I(λ0) = (λ0/λ)⁴

    Args:
        lambda_m:  Wavelength of interest (m)
        lambda0_m: Reference wavelength (m)

    Returns:
        Relative scattered intensity (dimensionless)
    """
    ratio = lambda0_m / lambda_m
    return ratio ** 4


def scatter_regime(a: float, lambda_m: float) -> ScatterRegime:
    """
    Classify scatterer radius a vs wavelength λ.

    Size parameter x = 2πa/λ:
        x < 0.3  → Rayleigh
        x < 50   → Mie
        else     → Geometric

    Args:
        a:        Scatterer radius (m)
        lambda_m: Wavelength (m)

    Returns:
        ScatterRegime enum value
    """
    x = 2.0 * PI * a / lambda_m
    if x < 0.3:
        return ScatterRegime.RAYLEIGH
    if x < 50.0:
        return ScatterRegime.MIE
    return ScatterRegime.GEOMETRIC


# ============================================================================
# §5  Photoelastic (stress-optic) effect
# ============================================================================

def stress_birefringence(C: float, sigma1: float, sigma2: float) -> float:
    """
    Induced birefringence from principal stress difference.

        Δn = C · (σ1 − σ2)

    Args:
        C:      Stress-optic coefficient (Pa⁻¹)
        sigma1: First principal stress (Pa)
        sigma2: Second principal stress (Pa)

    Returns:
        Induced birefringence Δn (dimensionless)
    """
    return C * (sigma1 - sigma2)


# ============================================================================
# §6  Urbach tail
# ============================================================================

def urbach_alpha(alpha0: float, E: float, E0: float, EU_T: float) -> float:
    """
    Urbach rule absorption coefficient (sub-gap exponential tail).

        α(E, T) = α0 · exp[(E − E0) / E_U(T)]

    Args:
        alpha0: Pre-exponential (m⁻¹)
        E:      Photon energy (eV)
        E0:     Band-edge reference energy (eV)
        EU_T:   Urbach energy at temperature T (eV)

    Returns:
        Absorption coefficient (m⁻¹)
    """
    return alpha0 * math.exp((E - E0) / EU_T)


# ============================================================================
# §7  Varshni band-gap temperature dependence
# ============================================================================

def varshni_Eg(Eg0: float, alpha: float, beta: float, T: float) -> float:
    """
    Band gap at temperature T (Varshni relation).

        Eg(T) = Eg(0) − α T² / (T + β)

    Args:
        Eg0:   Band gap at 0 K (eV)
        alpha: Varshni α (eV/K)
        beta:  Varshni β (K)
        T:     Temperature (K)

    Returns:
        Band gap Eg(T) (eV)
    """
    return Eg0 - (alpha * T * T) / (T + beta)


# ============================================================================
# §8  Ligand-field / crystal-field splitting
# ============================================================================

def delta_oct(R: float, R0: float) -> float:
    """
    Crystal-field splitting ratio relative to reference distance R0.

        Δ_oct ∝ 1/R⁵  →  ratio = (R0/R)⁵

    Multiply by the known Δ at R0 to get an absolute value.

    Args:
        R:  Metal-ligand distance (any consistent unit)
        R0: Reference distance (same unit)

    Returns:
        Dimensionless scaling factor Δ(R)/Δ(R0)
    """
    if R <= 0.0:
        return 0.0
    return (R0 / R) ** 5


# ============================================================================
# §9  Thermo-optic / Lorentz-Lorenz
# ============================================================================

def lorentz_lorenz_n(N_density: float, alpha_e: float) -> float:
    """
    Refractive index from Lorentz-Lorenz relation.

        (n²−1)/(n²+2) = (4π/3) · N · α_e

    Solved for n:
        f  = (4π/3) · N · α_e
        n² = (1 + 2f) / (1 − f)

    Args:
        N_density: Number density (m⁻³)
        alpha_e:   Electronic polarizability (C·m²/V, SI)

    Returns:
        Refractive index n  (0.0 on invalid input)
    """
    f = (4.0 * PI / 3.0) * N_density * alpha_e
    if f >= 1.0 or f < 0.0:
        return 0.0
    return math.sqrt((1.0 + 2.0 * f) / (1.0 - f))


def thermo_optic_dn_dT(N: float, dN_dT: float,
                        alpha_e: float, dalpha_dT: float,
                        dT: float = 1.0) -> float:
    """
    Approximate dn/dT via finite-difference Lorentz-Lorenz.

        dn/dT ≈ [n(N+dN, α+dα) − n(N, α)] / dT

    Args:
        N:          Number density at T (m⁻³)
        dN_dT:      d(number density)/dT  (m⁻³ K⁻¹)
        alpha_e:    Electronic polarizability at T
        dalpha_dT:  d(α_e)/dT
        dT:         Temperature step for finite difference (K)

    Returns:
        dn/dT (K⁻¹)
    """
    n0 = lorentz_lorenz_n(N, alpha_e)
    n1 = lorentz_lorenz_n(N + dN_dT * dT, alpha_e + dalpha_dT * dT)
    return (n1 - n0) / dT


# ============================================================================
# §10  Planck thermal emission
# ============================================================================

def planck_L_lambda(lambda_m: float, T: float,
                    emissivity: float = 1.0) -> float:
    """
    Spectral radiance with emissivity (modified Planck law).

        L_λ(T) = ε(λ,T) · [2hc²/λ⁵] · 1/(exp(hc/λkT) − 1)

    Args:
        lambda_m:   Wavelength (m)
        T:          Temperature (K)
        emissivity: Spectral emissivity ε ∈ [0, 1]

    Returns:
        Spectral radiance (W m⁻³ sr⁻¹)
    """
    if T <= 0.0 or lambda_m <= 0.0:
        return 0.0
    hc_lkT = (H_PLANCK * C_LIGHT) / (lambda_m * K_BOLTZMANN * T)
    l5 = lambda_m ** 5
    B = (2.0 * H_PLANCK * C_LIGHT ** 2) / (l5 * (math.exp(hc_lkT) - 1.0))
    return emissivity * B


def planck_peak_lambda(T: float) -> float:
    """
    Wien displacement: wavelength of peak emission.

        λ_max = b / T,  b = 2.897771955e-3 m·K

    Args:
        T: Temperature (K)

    Returns:
        Peak wavelength (m);  0.0 if T ≤ 0
    """
    if T <= 0.0:
        return 0.0
    return WIEN_B / T


# ============================================================================
# Composite clarity score
# ============================================================================

def clarity_score(alpha: float, x: float,
                  lambda_vis: float, lambda_ref: float,
                  C_stress_optic: float, delta_sigma: float) -> float:
    """
    Composite clarity index in [0, 1].

    Combines:
        60 % — Beer-Lambert transmission over path x
        20 % — Rayleigh scatter suppression (λ_vis vs λ_ref)
        20 % — Stress birefringence penalty

    Args:
        alpha:           Absorption coefficient (m⁻¹)
        x:               Path length (m)
        lambda_vis:      Wavelength of interest (m)
        lambda_ref:      Reference (shortest visible, e.g. 380e-9 m)
        C_stress_optic:  Stress-optic coefficient (Pa⁻¹)
        delta_sigma:     |σ1 − σ2| (Pa)

    Returns:
        Clarity index in [0, 1]; 1.0 = perfect clarity
    """
    T_abs   = math.exp(-alpha * x)
    scatter = 1.0 - min(1.0, rayleigh_I_rel(lambda_vis, lambda_ref)
                              / rayleigh_I_rel(lambda_ref, lambda_ref))
    bire    = abs(stress_birefringence(C_stress_optic, delta_sigma, 0.0))
    bire_pen = max(0.0, 1.0 - bire * 1.0e6)

    score = T_abs * 0.6 + scatter * 0.2 + bire_pen * 0.2
    return min(1.0, max(0.0, score))


# ============================================================================
# Summary printer
# ============================================================================

def print_summary(lambda_m: float, T_K: float,
                  alpha: float, kappa: float,
                  n: float, emissivity: float = 1.0,
                  file=None) -> None:
    """
    Print a formatted glass optics summary.

    Args:
        lambda_m:   Wavelength (m)
        T_K:        Temperature (K)
        alpha:      Absorption coefficient (m⁻¹)
        kappa:      Extinction coefficient
        n:          Refractive index (real part)
        emissivity: Spectral emissivity
        file:       Output file object (default: stdout)
    """
    import sys
    fp = file or sys.stdout

    I_ratio  = beer_lambert_I(1.0, alpha, 1e-3)
    peak_nm  = planck_peak_lambda(T_K) * 1e9
    L        = planck_L_lambda(lambda_m, T_K, emissivity)

    lines = [
        "═══════════════════════════════════════════════════",
        "  VSEPR-SIM Glass Optics Summary",
        "═══════════════════════════════════════════════════",
        f"  λ              = {lambda_m * 1e9:.1f} nm",
        f"  T              = {T_K:.1f} K",
        f"  n (real)       = {n:.4f}",
        f"  κ (extinction) = {kappa:.4e}",
        f"  α              = {alpha:.4e} m⁻¹",
        f"  T(1 mm)        = {I_ratio:.6f}",
        f"  Planck peak    = {peak_nm:.1f} nm",
        f"  L_λ (ε={emissivity:.2f})  = {L:.4e} W/m³/sr",
        "═══════════════════════════════════════════════════",
    ]
    for line in lines:
        print(line, file=fp)
