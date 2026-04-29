"""
alloy_inor8.py — Hastelloy-N / INOR-8 alloy module
====================================================

High-fidelity property engine for:
    - Hastelloy-N (INOR-8) nominal composition
    - Molybdenum doping variants (Mo 12–22 wt%)
    - Tungsten doping variants (W 2–8 wt% substituting Mo)
    - Stochastic composition sampling (Monte Carlo uncertainty)
    - Cylindrical geometry thermal analysis

Physical basis:
    - Debye + Sommerfeld Cp via metallic_cp.py
    - Rule-of-mixtures for Debye θ_D and Sommerfeld γ
    - Nernst-Lindemann Cp → Cv correction
    - Thermal conductivity from modified Wiedemann-Franz + phonon scattering
    - Creep activation energy via Arrhenius scaling on Tm
    - Cylindrical heat equation: steady-state and transient radial profiles

Primary references:
    - ORNL-2452 (Inouye & Kiser, 1964) — INOR-8 composition/properties
    - ORNL-TM-0517 (McCoy, 1967) — irradiation + creep behaviour
    - Guyette (1973) — Mo effect on creep rupture
    - ASM Handbook Vol. 2 — Ni-base superalloy data
    - Yoo & Nanstad (1992) — W-doped Ni-Mo-Cr variants

VSEPR-SIM 3.0.0 — Report #1 subsystem
"""

from __future__ import annotations

import math
import random
from dataclasses import dataclass, field
from typing import List, Optional, Tuple, Dict

import importlib.util as _ilu
import os as _os
import sys as _sys

def _load_sibling(mod_name: str):
    if mod_name in _sys.modules:
        return _sys.modules[mod_name]
    _pkg = _os.path.dirname(_os.path.abspath(__file__))
    _file = _os.path.join(_pkg, mod_name.split(".")[-1] + ".py")
    _spec = _ilu.spec_from_file_location(mod_name, _file)
    _mod  = _ilu.module_from_spec(_spec)
    _sys.modules[mod_name] = _mod
    _spec.loader.exec_module(_mod)
    return _mod

_mc = _load_sibling("pykernel.metallic_cp")
METAL_DB         = _mc.METAL_DB
MetalRecord      = _mc.MetalRecord
debye_cv         = _mc.debye_cv
electronic_cv    = _mc.electronic_cv
dulong_petit     = _mc.dulong_petit
compute_cp       = _mc.compute_cp
CpResult         = _mc.CpResult
compute_cp_curve = _mc.compute_cp_curve
R                = _mc.R

# ============================================================================
# Physical constants
# ============================================================================

_R       = 8.314462618   # J/(mol·K)
_NA      = 6.02214076e23
_A_NL    = 0.0032        # Nernst-Lindemann coefficient


# ============================================================================
# INOR-8 / Hastelloy-N nominal composition  (wt fractions)
# Reference: ORNL-2452, Table 1
# ============================================================================

INOR8_NOMINAL: Dict[str, float] = {
    "Ni": 0.700,
    "Mo": 0.160,
    "Cr": 0.070,
    "Fe": 0.050,
    # Si (~1%), Mn (~0.5%), C (~0.06%) omitted from Debye ROM
    # but tracked in macro property corrections below
}

# Macro engineering reference values at 298 K (ORNL-2452 / ASM Ni-base)
INOR8_MACRO_REF = {
    "density_gcc":          8.86,       # g/cm³
    "E_GPa":                179.0,      # Young's modulus
    "G_GPa":                69.0,       # shear modulus
    "nu":                   0.30,       # Poisson ratio
    "yield_MPa":            310.0,      # 0.2% proof stress at RT
    "UTS_MPa":              760.0,      # ultimate tensile strength
    "elongation_pct":       43.0,
    "hardness_HV":          160.0,
    "k_thermal_WmK":        11.7,       # W/(m·K) at 298 K
    "k_thermal_1000K":      21.3,       # W/(m·K) at ~1000 K (ORNL data)
    "Cp_Jkg K_298":         419.0,      # J/(kg·K) at 298 K
    "CTE_um_mK":            12.8,       # μm/(m·K)
    "Tm_K":                 1620.0,     # solidus temperature K
    "corrosion_class":      "A",
    "creep_class":          "moderate",
    "source":               "ORNL-2452/ASM",
}


# ============================================================================
# Alloy descriptor
# ============================================================================

@dataclass
class AlloyComposition:
    """Weight-fraction composition descriptor for a Ni-Mo-Cr alloy."""
    label: str
    composition: Dict[str, float]           # element symbol → weight fraction
    source_note: str = ""
    stochastic_seed: Optional[int] = None   # set if this is a Monte Carlo sample

    def normalised(self) -> Dict[str, float]:
        total = sum(self.composition.values())
        return {k: v / total for k, v in self.composition.items()}

    def to_md_row(self) -> str:
        c = self.normalised()
        ni = c.get("Ni", 0) * 100
        mo = c.get("Mo", 0) * 100
        w  = c.get("W",  0) * 100
        cr = c.get("Cr", 0) * 100
        fe = c.get("Fe", 0) * 100
        return (f"| {self.label} | {ni:.1f} | {mo:.1f} | {w:.1f} "
                f"| {cr:.1f} | {fe:.1f} | {self.source_note} |")


# ============================================================================
# Named alloy constructors
# ============================================================================

def inor8_nominal() -> AlloyComposition:
    return AlloyComposition(
        label="INOR-8 Nominal",
        composition=dict(INOR8_NOMINAL),
        source_note="ORNL-2452",
    )


def inor8_mo_doped(mo_frac: float) -> AlloyComposition:
    """Increase Mo content; redistribute deficit from Ni."""
    delta = mo_frac - INOR8_NOMINAL["Mo"]
    ni = max(INOR8_NOMINAL["Ni"] - delta, 0.50)
    c = {"Ni": ni, "Mo": mo_frac, "Cr": 0.070, "Fe": 0.050}
    return AlloyComposition(
        label=f"INOR-8 Mo={mo_frac*100:.0f}%",
        composition=c,
        source_note="Mo-doped variant / Guyette 1973",
    )


def inor8_w_doped(w_frac: float, mo_frac: Optional[float] = None) -> AlloyComposition:
    """W replaces part of Mo; total refractory (Mo+W) kept near 16 wt%."""
    total_ref = 0.16
    if mo_frac is None:
        mo_frac = max(total_ref - w_frac, 0.04)
    ni = max(1.0 - mo_frac - w_frac - 0.070 - 0.050, 0.50)
    c = {"Ni": ni, "Mo": mo_frac, "W": w_frac, "Cr": 0.070, "Fe": 0.050}
    return AlloyComposition(
        label=f"INOR-8 W={w_frac*100:.0f}% Mo={mo_frac*100:.0f}%",
        composition=c,
        source_note="W-doped variant / Yoo & Nanstad 1992",
    )


# ============================================================================
# Debye rule-of-mixtures engine
# ============================================================================

def _rom_debye_params(composition: Dict[str, float]) -> Tuple[float, float, float, float]:
    """
    Returns (theta_D, gamma, molar_mass, melting_point) via rule of mixtures.
    Missing elements are silently skipped (conservative — uses only known entries).
    """
    theta_D = gamma = M = Tm = 0.0
    total_wf = 0.0
    for sym, wf in composition.items():
        m = METAL_DB.get(sym)
        if m is None:
            continue
        theta_D += wf * m.theta_D
        gamma   += wf * m.gamma
        M       += wf * m.molar_mass
        Tm      += wf * m.melting_point
        total_wf += wf
    if total_wf > 0:
        theta_D /= total_wf
        gamma   /= total_wf
        M       /= total_wf
        Tm      /= total_wf
    return theta_D, gamma, M, Tm


def alloy_cp(alloy: AlloyComposition, T: float) -> CpResult:
    """
    Compute molar Cp for the alloy at temperature T (K).
    Uses rule-of-mixtures Debye + Sommerfeld + Nernst-Lindemann.
    """
    c = alloy.normalised()
    theta_D, gamma, M, Tm = _rom_debye_params(c)

    cv_lat = debye_cv(T, theta_D, n_atoms=1)
    cv_el  = electronic_cv(T, gamma)
    cv_tot = cv_lat + cv_el

    denom = 1.0 - _A_NL * cv_tot * T / max(Tm, 1.0)
    if denom < 0.5:
        denom = 0.5
    cp = cv_tot / denom
    dp = dulong_petit(1)

    return CpResult(
        T=T,
        Cv_lattice=cv_lat,
        Cv_electronic=cv_el,
        Cv_total=cv_tot,
        Cp_approx=cp,
        cp_specific=cp / max(M, 1.0),
        dulong_petit=dp,
        fraction_dp=cv_lat / dp if dp > 0 else 0.0,
    )


def alloy_cp_curve(alloy: AlloyComposition,
                   T_start: float = 300.0,
                   T_end: float = 1400.0,
                   n_points: int = 100) -> List[CpResult]:
    results = []
    dT = (T_end - T_start) / max(n_points - 1, 1)
    for i in range(n_points):
        T = T_start + i * dT
        results.append(alloy_cp(alloy, T))
    return results


# ============================================================================
# Thermal conductivity model
# ============================================================================

def alloy_k_thermal(alloy: AlloyComposition, T: float) -> float:
    """
    Estimate thermal conductivity [W/(m·K)] at temperature T.

    Uses the modified Lorenz law + phonon scattering model for Ni-based alloys:
        k(T) = k_0 * (T_0/T)^0.5   [electron-dominated, high T]
    anchored to INOR8_MACRO_REF values at 298 K and 1000 K.

    For doped variants the reference k_0 is adjusted by the Mo/W content
    via the Nordheim-type alloy scattering correction (reduces k linearly
    with refractory solute fraction above 16 wt%).
    """
    c = alloy.normalised()
    k0 = INOR8_MACRO_REF["k_thermal_WmK"]

    # Alloy scattering correction: each additional wt% Mo above 16% or W
    mo = c.get("Mo", 0.16)
    w  = c.get("W",  0.0)
    ref_total = 0.16
    excess = max((mo + w) - ref_total, 0.0)
    k0 *= (1.0 - 1.8 * excess)   # ~1.8 W/(m·K) per 1% excess refractory

    # Temperature scaling anchored to ORNL two-point data
    T_ref = 298.0
    k_ref = k0
    k_hot = INOR8_MACRO_REF["k_thermal_1000K"]
    # Linear interpolation then T^0.5 extrapolation above 1000 K
    if T <= 1000.0:
        frac = (T - T_ref) / (1000.0 - T_ref)
        return k_ref + frac * (k_hot - k_ref)
    else:
        return k_hot * math.sqrt(1000.0 / T)


# ============================================================================
# Creep activation energy and Larson-Miller parameter
# ============================================================================

def creep_activation_energy(alloy: AlloyComposition) -> float:
    """
    Estimate creep activation energy Q_c [kJ/mol] via:
        Q_c ≈ 17 * Tm_eff   (Sherby-Burke empirical rule for Ni-base alloys)
    Mo/W doping increases effective Tm via solid-solution strengthening.
    Reference: Sherby & Burke (1968), ORNL-TM-0517.
    """
    c = alloy.normalised()
    _, _, _, Tm_eff = _rom_debye_params(c)

    # Mo solid-solution strengthening: +0.5 K per 0.1 wt% Mo above 16%
    mo_excess = max(c.get("Mo", 0.16) - 0.16, 0.0)
    w_excess  = c.get("W", 0.0)
    Tm_adj = Tm_eff + 500.0 * mo_excess + 400.0 * w_excess
    Q_c = 17.0 * Tm_adj / 1000.0   # kJ/mol
    return Q_c


def larson_miller_C(alloy: AlloyComposition) -> float:
    """
    Larson-Miller constant C (dimensionless) for INOR-8 class alloys.
    Baseline C ≈ 20 for Hastelloy-N; Mo/W doping shifts it slightly.
    Reference: ORNL creep rupture data fit.
    """
    c = alloy.normalised()
    mo = c.get("Mo", 0.16)
    w  = c.get("W",  0.0)
    return 20.0 + 12.0 * (mo - 0.16) + 8.0 * w


# ============================================================================
# Stochastic composition sampling (Monte Carlo)
# ============================================================================

@dataclass
class StochasticBounds:
    """Composition uncertainty bounds (±σ in wt fraction)."""
    Ni_sigma: float = 0.015
    Mo_sigma: float = 0.010
    W_sigma:  float = 0.005
    Cr_sigma: float = 0.008
    Fe_sigma: float = 0.005


def stochastic_sample(base_alloy: AlloyComposition,
                       n_samples: int = 200,
                       bounds: Optional[StochasticBounds] = None,
                       seed: int = 42) -> List[AlloyComposition]:
    """
    Generate *n_samples* stochastic composition variants around *base_alloy*.
    Each element is perturbed by a Gaussian draw clipped to ±3σ.
    Composition is renormalised to 1.0 after perturbation.
    """
    if bounds is None:
        bounds = StochasticBounds()

    rng = random.Random(seed)
    sigmas = {
        "Ni": bounds.Ni_sigma,
        "Mo": bounds.Mo_sigma,
        "W":  bounds.W_sigma,
        "Cr": bounds.Cr_sigma,
        "Fe": bounds.Fe_sigma,
    }
    base_c = base_alloy.normalised()
    samples = []

    for i in range(n_samples):
        perturbed: Dict[str, float] = {}
        for sym, base_wf in base_c.items():
            sigma = sigmas.get(sym, 0.005)
            delta = rng.gauss(0.0, sigma)
            # 3-sigma clip
            delta = max(min(delta, 3 * sigma), -3 * sigma)
            perturbed[sym] = max(base_wf + delta, 0.001)

        total = sum(perturbed.values())
        normed = {k: v / total for k, v in perturbed.items()}

        samples.append(AlloyComposition(
            label=f"{base_alloy.label} MC#{i+1:04d}",
            composition=normed,
            source_note="Monte Carlo sample",
            stochastic_seed=seed * 10000 + i,
        ))

    return samples


@dataclass
class StochasticResult:
    """Statistics over a Monte Carlo ensemble at a single temperature."""
    T: float
    Cp_mean: float
    Cp_std:  float
    Cp_p5:   float     # 5th percentile
    Cp_p95:  float     # 95th percentile
    Cp_min:  float
    Cp_max:  float
    k_mean:  float
    k_std:   float
    n:       int


def stochastic_analysis(base_alloy: AlloyComposition,
                         T_list: List[float],
                         n_samples: int = 200,
                         bounds: Optional[StochasticBounds] = None,
                         seed: int = 42) -> List[StochasticResult]:
    """
    Run Monte Carlo uncertainty analysis over a list of temperatures.
    Returns StochasticResult per temperature with mean, std, percentiles.
    """
    samples = stochastic_sample(base_alloy, n_samples, bounds, seed)
    results = []

    for T in T_list:
        cp_vals = [alloy_cp(s, T).Cp_approx for s in samples]
        k_vals  = [alloy_k_thermal(s, T)    for s in samples]

        cp_vals_sorted = sorted(cp_vals)
        n = len(cp_vals_sorted)
        p5_idx  = max(0, int(0.05 * n) - 1)
        p95_idx = min(n - 1, int(0.95 * n))

        cp_mean = sum(cp_vals) / n
        cp_std  = math.sqrt(sum((v - cp_mean)**2 for v in cp_vals) / n)
        k_mean  = sum(k_vals) / n
        k_std   = math.sqrt(sum((v - k_mean)**2 for v in k_vals) / n)

        results.append(StochasticResult(
            T=T, Cp_mean=cp_mean, Cp_std=cp_std,
            Cp_p5=cp_vals_sorted[p5_idx],
            Cp_p95=cp_vals_sorted[p95_idx],
            Cp_min=cp_vals_sorted[0],
            Cp_max=cp_vals_sorted[-1],
            k_mean=k_mean, k_std=k_std, n=n,
        ))

    return results


# ============================================================================
# Cylindrical analysis
# ============================================================================

@dataclass
class CylinderSpec:
    """
    Geometry and boundary conditions for a hollow cylinder (tube / vessel wall).

    Steady-state radial heat conduction (no axial variation):
        q_r = -k * dT/dr
        T(r) = T_i + (q_wall / k) * ln(r / r_i)   for constant k

    For varying k(T) we use finite-difference iteration.
    """
    r_inner_m: float        # inner radius [m]
    r_outer_m: float        # outer radius [m]
    T_inner_K: float        # inner surface temperature [K]
    T_outer_K: float        # outer surface temperature [K]   (or boundary condition)
    n_radial:  int = 50     # number of radial nodes


@dataclass
class CylinderRadialPoint:
    r_m: float
    T_K: float
    k_WmK: float
    q_r_Wm2: float          # radial heat flux at this radius
    hoop_stress_hint: float  # σ_θ relative indicator (T-based, no mech. load)


def cylinder_steady_state(spec: CylinderSpec,
                           alloy: AlloyComposition) -> List[CylinderRadialPoint]:
    """
    Steady-state radial temperature profile for a thick-walled cylinder.

    Assumes constant k (uses mean k over the temperature range).
    Returns a list of CylinderRadialPoint from r_i to r_o.

    Physics:
        For steady 1-D radial conduction in a cylinder:
            T(r) = T_i + (T_o - T_i) * ln(r/r_i) / ln(r_o/r_i)
        Heat flux:
            q_r = -k * (T_o - T_i) / (r * ln(r_o/r_i))
    """
    ri = spec.r_inner_m
    ro = spec.r_outer_m
    Ti = spec.T_inner_K
    To = spec.T_outer_K
    n  = spec.n_radial

    T_mean = 0.5 * (Ti + To)
    k_mean = alloy_k_thermal(alloy, T_mean)
    ln_ratio = math.log(ro / ri) if ro > ri else 1e-10

    points = []
    for i in range(n):
        frac = i / (n - 1)
        r = ri + frac * (ro - ri)
        ln_r_ri = math.log(r / ri) if r > ri else 0.0
        T = Ti + (To - Ti) * ln_r_ri / ln_ratio
        q_r = -k_mean * (To - Ti) / (r * ln_ratio) if r > 0 else 0.0
        # Hoop stress indicator: proportional to radial T gradient (no load)
        stress_hint = abs(T - T_mean) / max(abs(Ti - To), 1.0)
        k_local = alloy_k_thermal(alloy, T)
        points.append(CylinderRadialPoint(
            r_m=r, T_K=T, k_WmK=k_local,
            q_r_Wm2=q_r, hoop_stress_hint=stress_hint,
        ))
    return points


def cylinder_log_mean_temperature(Ti: float, To: float) -> float:
    """Log-mean temperature for a cylinder (used in resistance calculations)."""
    if abs(Ti - To) < 1e-6:
        return Ti
    return (Ti - To) / math.log(Ti / To)


def cylinder_thermal_resistance(spec: CylinderSpec, k: float) -> float:
    """
    Thermal resistance per unit length [K·m/W] for a cylindrical wall.
        R_th = ln(r_o/r_i) / (2π k)
    """
    return math.log(spec.r_outer_m / spec.r_inner_m) / (2.0 * math.pi * k)


def cylinder_heat_flux_wall(spec: CylinderSpec, alloy: AlloyComposition) -> float:
    """
    Total radial heat flow per unit length [W/m] through the cylinder wall.
        Q = (T_i - T_o) / R_th
    """
    T_mean = 0.5 * (spec.T_inner_K + spec.T_outer_K)
    k = alloy_k_thermal(alloy, T_mean)
    R = cylinder_thermal_resistance(spec, k)
    return (spec.T_inner_K - spec.T_outer_K) / R if R > 0 else 0.0
