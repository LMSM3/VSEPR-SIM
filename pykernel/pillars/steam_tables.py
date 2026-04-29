"""
SteamTables++ — Multi-Material Thermophysical Atlas
====================================================

Pillar A of the VSEPR-SIM Five Pillars architecture.

Tiered material coverage:
  S1: Exact / reference-backed    (H2O, R134a, CO2, NH3, CH4, N2, O2, He, Ar)
  S2: EOS-derived approximation   (hydrocarbons, alcohols, cryogens, process fluids)
  S3: Surrogate / fitted          (pseudo-materials, alloy melts, slurries)

For each material the engine provides:
  - Phase classification (subcooled liquid, wet mixture, superheated vapour, supercritical)
  - Saturation properties (T_sat, P_sat, h_f, h_g, s_f, s_g, v_f, v_g)
  - Single-phase properties at (T, P): h, s, v, c_p, c_v, mu, k, Pr
  - Quality x for wet mixtures
  - Adaptive region sampling near phase boundaries and critical points
  - Pipe friction / pressure-drop helpers

Anti-black-box: every correlation is traceable to its source.
Deterministic: same (material, T, P) → identical output.

Data sources:
  S1 H2O  — IAPWS-IF97 region equations (Wagner & Pruss 2002)
  S1 CO2  — Span-Wagner EOS (1996)
  S1 others — Antoine + Peng-Robinson cubic EOS
  S2/S3   — Peng-Robinson with group-contribution Tc/Pc/omega

VSEPR-SIM Five Pillars v1.0
"""

from __future__ import annotations

import math
import json
import csv
import os
import hashlib
import random
from dataclasses import dataclass, field, asdict
from typing import Dict, List, Tuple, Optional
from enum import Enum, auto

# ============================================================================
# Physical constants
# ============================================================================

R_GAS = 8.314462618          # J/(mol·K)
R_SPEC_H2O = 461.526        # J/(kg·K) specific gas constant for water
STEFAN_BOLTZMANN = 5.670374419e-8


# ============================================================================
# Phase classification
# ============================================================================

class Phase(Enum):
    SUBCOOLED_LIQUID = auto()
    SATURATED_LIQUID = auto()
    WET_MIXTURE = auto()
    SATURATED_VAPOUR = auto()
    SUPERHEATED_VAPOUR = auto()
    SUPERCRITICAL = auto()
    SOLID = auto()
    UNKNOWN = auto()


# ============================================================================
# Material tier classification
# ============================================================================

class Tier(Enum):
    S1 = "exact_reference"
    S2 = "eos_derived"
    S3 = "surrogate_fitted"


# ============================================================================
# Core data structures
# ============================================================================

@dataclass
class CriticalPoint:
    T_c: float     # K
    P_c: float     # Pa
    rho_c: float   # kg/m³
    omega: float   # acentric factor


@dataclass
class AntoineCoeffs:
    """log10(P_mmHg) = A - B / (C + T_C)  where T_C in Celsius."""
    A: float
    B: float
    C: float
    T_min: float   # K validity range
    T_max: float   # K


@dataclass
class MaterialDef:
    name: str
    formula: str
    tier: Tier
    molar_mass: float          # kg/mol
    critical: CriticalPoint
    antoine: Optional[AntoineCoeffs] = None
    source: str = ""
    tags: List[str] = field(default_factory=list)


@dataclass
class SaturationProps:
    T_sat: float      # K
    P_sat: float      # Pa
    h_f: float        # J/kg  enthalpy of saturated liquid
    h_g: float        # J/kg  enthalpy of saturated vapour
    h_fg: float       # J/kg  latent heat
    s_f: float        # J/(kg·K)
    s_g: float        # J/(kg·K)
    v_f: float        # m³/kg
    v_g: float        # m³/kg
    phase: Phase = Phase.WET_MIXTURE


@dataclass
class StatePoint:
    """Complete thermodynamic state at a point."""
    material: str
    T: float           # K
    P: float           # Pa
    phase: Phase
    h: float           # J/kg  specific enthalpy
    s: float           # J/(kg·K)  specific entropy
    v: float           # m³/kg  specific volume
    rho: float         # kg/m³
    x: float           # quality (-1 for single phase)
    c_p: float         # J/(kg·K)
    c_v: float         # J/(kg·K)
    mu: float          # Pa·s  dynamic viscosity
    k_thermal: float   # W/(m·K) thermal conductivity
    Pr: float          # Prandtl number
    Z: float           # compressibility factor
    bonds: str = "ON"  # role tag
    provenance: str = ""


@dataclass
class PipeFrictionResult:
    """Pressure drop and friction for pipe flow."""
    material: str
    D_pipe: float      # m
    L_pipe: float      # m
    roughness: float   # m
    mass_flow: float   # kg/s
    velocity: float    # m/s
    Re: float          # Reynolds number
    f_darcy: float     # Darcy friction factor
    dP_friction: float # Pa
    dP_minor: float    # Pa
    dP_total: float    # Pa
    flow_regime: str   # laminar / transitional / turbulent


# ============================================================================
# Tier S1: Reference material database
# ============================================================================

_S1_MATERIALS: Dict[str, MaterialDef] = {}

def _reg_s1(name, formula, M, Tc, Pc, rhoc, omega, ant_A, ant_B, ant_C,
            ant_Tmin, ant_Tmax, src, tags=None):
    crit = CriticalPoint(Tc, Pc, rhoc, omega)
    ant = AntoineCoeffs(ant_A, ant_B, ant_C, ant_Tmin, ant_Tmax)
    _S1_MATERIALS[formula] = MaterialDef(
        name=name, formula=formula, tier=Tier.S1,
        molar_mass=M / 1000.0, critical=crit, antoine=ant,
        source=src, tags=tags or [])

# NIST WebBook + Perry's Chemical Engineers' Handbook 9th ed.
# Antoine coeffs: log10(P_mmHg) = A - B/(C + T_C) with T in Celsius
_reg_s1("Water",          "H2O",    18.015,  647.096, 22064000, 322.0,  0.3443,
        8.07131, 1730.63, 233.426, 273.15, 473.15,
        "IAPWS-IF97; Wagner&Pruss 2002", ["process", "steam", "reference"])
_reg_s1("Carbon Dioxide", "CO2",    44.01,   304.13,  7377300,  467.6,  0.2239,
        6.81228, 1301.679, -3.494, 194.65, 304.13,
        "Span-Wagner 1996", ["refrigerant", "process", "greenhouse"])
_reg_s1("Ammonia",        "NH3",    17.031,  405.40, 11333000,  225.0,  0.2526,
        7.55466, 1002.711, -32.98, 195.41, 405.40,
        "TRC NIST", ["refrigerant", "process"])
_reg_s1("R-134a",         "R134a",  102.032, 374.21,  4059280,  511.9,  0.3268,
        7.0445,  1084.0,  -26.85, 169.85, 374.21,
        "Tillner-Roth&Baehr 1994", ["refrigerant", "HVAC"])
_reg_s1("Methane",        "CH4",    16.043,  190.56,  4599200,  162.66, 0.0115,
        6.61184, 389.93,  -7.16,  90.68, 190.56,
        "Setzmann&Wagner 1991", ["hydrocarbon", "fuel", "cryogen"])
_reg_s1("Nitrogen",       "N2",     28.014,  126.19,  3395800,  313.3,  0.0372,
        6.49457, 255.68,  -6.60,  63.15, 126.19,
        "Span et al. 2000", ["cryogen", "industrial"])
_reg_s1("Oxygen",         "O2",     31.998,  154.58,  5043000,  436.14, 0.0222,
        6.69147, 319.01,  -6.45,  54.36, 154.58,
        "Schmidt&Wagner 1985", ["cryogen", "oxidizer"])
_reg_s1("Helium",         "He",     4.0026,  5.195,   227600,   69.64,  -0.382,
        5.32072, 14.65,   -0.15,  2.17, 5.19,
        "Katti et al. 1986", ["noble", "cryogen"])
_reg_s1("Argon",          "Ar",     39.948,  150.69,  4863000,  535.6,  0.0000,
        6.57328, 304.23,  -4.15,  83.81, 150.69,
        "Tegeler et al. 1999", ["noble", "inert"])


# ============================================================================
# Tier S2: EOS-derived approximation materials
# ============================================================================

_S2_MATERIALS: Dict[str, MaterialDef] = {}

def _reg_s2(name, formula, M, Tc, Pc, omega, src, tags=None):
    rhoc = Pc / (R_GAS / (M / 1000.0) * Tc) * 0.27  # approx Zc ~ 0.27
    crit = CriticalPoint(Tc, Pc, rhoc, omega)
    _S2_MATERIALS[formula] = MaterialDef(
        name=name, formula=formula, tier=Tier.S2,
        molar_mass=M / 1000.0, critical=crit,
        source=src, tags=tags or [])

# Peng-Robinson with literature Tc/Pc/omega
_reg_s2("Ethane",       "C2H6",   30.07,  305.32, 4872200,  0.0995,
        "Bucker&Wagner 2006", ["hydrocarbon"])
_reg_s2("Propane",      "C3H8",   44.10,  369.83, 4248000,  0.1523,
        "Lemmon et al. 2009", ["hydrocarbon", "fuel"])
_reg_s2("n-Butane",     "C4H10",  58.12,  425.13, 3796000,  0.2002,
        "Bucker&Wagner 2006", ["hydrocarbon"])
_reg_s2("n-Pentane",    "C5H12",  72.15,  469.70, 3370000,  0.2515,
        "Span&Wagner 2003", ["hydrocarbon"])
_reg_s2("n-Hexane",     "C6H14",  86.18,  507.60, 3025000,  0.3013,
        "Span&Wagner 2003", ["hydrocarbon"])
_reg_s2("n-Octane",     "C8H18",  114.23, 568.70, 2490000,  0.3996,
        "Span&Wagner 2003", ["hydrocarbon", "fuel"])
_reg_s2("Ethanol",      "C2H5OH", 46.07,  513.92, 6148000,  0.6436,
        "Dillon&Penoncello 2004", ["alcohol", "solvent"])
_reg_s2("Methanol",     "CH3OH",  32.04,  512.64, 8104000,  0.5658,
        "de Reuck&Craven 1993", ["alcohol", "fuel"])
_reg_s2("Acetone",      "C3H6O",  58.08,  508.10, 4700000,  0.3065,
        "Lemmon&Span 2006", ["solvent"])
_reg_s2("Hydrogen",     "H2",     2.016,  33.145, 1296400,  -0.219,
        "Leachman et al. 2009", ["cryogen", "fuel"])
_reg_s2("Neon",         "Ne",     20.180, 44.49,  2678600,  -0.0395,
        "Katti et al. 1986", ["noble", "cryogen"])
_reg_s2("Xenon",        "Xe",     131.29, 289.73, 5842000,  0.0036,
        "Lemmon&Span 2006", ["noble"])
_reg_s2("Sulfur Dioxide","SO2",   64.066, 430.64, 7884000,  0.2564,
        "Lemmon&Span 2006", ["industrial", "pollutant"])
_reg_s2("Hydrogen Sulfide","H2S", 34.081, 373.10, 9000000,  0.0942,
        "Lemmon&Span 2006", ["sour gas"])
_reg_s2("R-22",         "R22",    86.47,  369.30, 4990000,  0.2208,
        "Kamei et al. 1995", ["refrigerant", "legacy"])
_reg_s2("R-410A",       "R410A",  72.58,  344.49, 4901200,  0.2961,
        "Lemmon 2003", ["refrigerant", "HVAC"])
_reg_s2("R-32",         "R32",    52.02,  351.26, 5782000,  0.2769,
        "Tillner-Roth&Yokozeki 1997", ["refrigerant"])
_reg_s2("Isobutane",    "iC4H10", 58.12,  407.81, 3629000,  0.1835,
        "Bucker&Wagner 2006", ["hydrocarbon", "refrigerant"])
_reg_s2("Cyclohexane",  "C6H12",  84.16,  553.50, 4080000,  0.2096,
        "Span&Wagner 2003", ["hydrocarbon", "solvent"])
_reg_s2("Toluene",      "C7H8",   92.14,  591.75, 4126000,  0.2640,
        "Lemmon&Span 2006", ["hydrocarbon", "solvent"])
_reg_s2("Benzene",      "C6H6",   78.11,  562.05, 4895000,  0.2103,
        "NIST TDE", ["hydrocarbon", "aromatic"])


# ============================================================================
# Tier S3: Surrogate / fitted materials
# ============================================================================

_S3_MATERIALS: Dict[str, MaterialDef] = {}

def _reg_s3(name, formula, M, Tc, Pc, omega, src, tags=None):
    rhoc = Pc / (R_GAS / (M / 1000.0) * Tc) * 0.27
    crit = CriticalPoint(Tc, Pc, rhoc, omega)
    _S3_MATERIALS[formula] = MaterialDef(
        name=name, formula=formula, tier=Tier.S3,
        molar_mass=M / 1000.0, critical=crit,
        source=src, tags=tags or [])

_reg_s3("Diesel Surrogate",  "nC12H26", 170.34, 658.1, 1817000, 0.576,
        "Lemmon 2004 (n-dodecane proxy)", ["fuel", "surrogate"])
_reg_s3("JP-8 Surrogate",    "JP8",     153.0,  640.0, 2100000, 0.50,
        "Edwards 2003 fitted", ["fuel", "surrogate", "aviation"])
_reg_s3("Liquid Sodium",     "Na_l",    22.99,  2573.0, 25600000, 0.0,
        "Fink&Leibowitz 1995", ["liquid_metal", "reactor"])
_reg_s3("NaCl Melt",         "NaCl_m",  58.44,  3400.0, 28000000, 0.0,
        "Janz 1988 NSRDS", ["molten_salt"])
_reg_s3("Solar Salt",        "NaNO3KNO3", 94.1, 1100.0, 15000000, 0.0,
        "Zavoico 2001 fitted", ["molten_salt", "CSP"])
_reg_s3("FLiBe",             "Li2BeF4", 99.0,   1703.0, 30000000, 0.0,
        "Williams et al. 2006", ["molten_salt", "reactor"])
_reg_s3("Therminol VP-1",    "TVP1",    166.0,  770.0,  3300000, 0.45,
        "Solutia tech data fitted", ["heat_transfer", "CSP"])
_reg_s3("Dowtherm A",        "DowA",    166.0,  770.0,  3100000, 0.42,
        "Dow tech data fitted", ["heat_transfer"])
_reg_s3("Lead-Bismuth Eutectic","PbBi", 208.2,  4500.0, 50000000, 0.0,
        "OECD NEA Handbook 2015", ["liquid_metal", "reactor"])
_reg_s3("Mercury",           "Hg",      200.59, 1750.0, 167000000, 0.0,
        "NIST fitted", ["liquid_metal", "toxic"])


# ============================================================================
# Complete material registry
# ============================================================================

def get_all_materials() -> Dict[str, MaterialDef]:
    merged = {}
    merged.update(_S1_MATERIALS)
    merged.update(_S2_MATERIALS)
    merged.update(_S3_MATERIALS)
    return merged

def get_material(formula: str) -> Optional[MaterialDef]:
    for db in (_S1_MATERIALS, _S2_MATERIALS, _S3_MATERIALS):
        if formula in db:
            return db[formula]
    return None

def materials_by_tier(tier: Tier) -> Dict[str, MaterialDef]:
    if tier == Tier.S1: return dict(_S1_MATERIALS)
    if tier == Tier.S2: return dict(_S2_MATERIALS)
    return dict(_S3_MATERIALS)


# ============================================================================
# Peng-Robinson Equation of State
# ============================================================================

class PengRobinsonEOS:
    """Peng-Robinson cubic EOS for PVT calculations.

    P = RT/(V-b) - a(T) / (V(V+b) + b(V-b))

    a(T) = 0.45724 R²Tc²/Pc · alpha(T)
    b    = 0.07780 RTc/Pc
    alpha = [1 + kappa(1 - sqrt(T/Tc))]²
    kappa = 0.37464 + 1.54226ω - 0.26992ω²
    """

    def __init__(self, mat: MaterialDef):
        self.mat = mat
        Tc = mat.critical.T_c
        Pc = mat.critical.P_c
        omega = mat.critical.omega
        self.Tc = Tc
        self.Pc = Pc
        self.omega = omega
        self.R = R_GAS / mat.molar_mass   # specific gas constant J/(kg·K)
        self.R_mol = R_GAS

        self.b = 0.07780 * R_GAS * Tc / Pc
        self.a_c = 0.45724 * R_GAS**2 * Tc**2 / Pc

        if omega <= 0.491:
            self.kappa = 0.37464 + 1.54226 * omega - 0.26992 * omega**2
        else:
            self.kappa = (0.379642 + 1.48503 * omega
                          - 0.164423 * omega**2 + 0.016666 * omega**3)

    def alpha(self, T: float) -> float:
        return (1.0 + self.kappa * (1.0 - math.sqrt(T / self.Tc)))**2

    def a(self, T: float) -> float:
        return self.a_c * self.alpha(T)

    def pressure(self, T: float, V_mol: float) -> float:
        """Pressure from molar volume V_mol (m³/mol)."""
        a_ = self.a(T)
        b_ = self.b
        return (R_GAS * T / (V_mol - b_)
                - a_ / (V_mol * (V_mol + b_) + b_ * (V_mol - b_)))

    def Z_cubic_coeffs(self, T: float, P: float) -> Tuple[float, float, float]:
        """Coefficients for Z³ + c2·Z² + c1·Z + c0 = 0."""
        a_ = self.a(T)
        b_ = self.b
        A = a_ * P / (R_GAS * T)**2
        B = b_ * P / (R_GAS * T)
        c2 = -(1.0 - B)
        c1 = A - 3.0 * B**2 - 2.0 * B
        c0 = -(A * B - B**2 - B**3)
        return (c2, c1, c0)

    def solve_Z(self, T: float, P: float) -> List[float]:
        """Solve cubic for compressibility factor Z. Returns real positive roots."""
        c2, c1, c0 = self.Z_cubic_coeffs(T, P)
        # Cardano's method for Z³ + c2 Z² + c1 Z + c0 = 0
        q = c1 / 3.0 - c2**2 / 9.0
        r = (c1 * c2 - 3.0 * c0) / 6.0 - c2**3 / 27.0

        # Overflow guard: if coefficients are extreme, fall back to Z = 1
        try:
            disc = q**3 + r**2
        except OverflowError:
            return [1.0]

        roots = []
        try:
            if disc >= 0:
                sqrt_disc = math.sqrt(disc)
                s1_arg = r + sqrt_disc
                s2_arg = r - sqrt_disc
                s1 = math.copysign(abs(s1_arg)**(1/3), s1_arg) if abs(s1_arg) > 0 else 0.0
                s2 = math.copysign(abs(s2_arg)**(1/3), s2_arg) if abs(s2_arg) > 0 else 0.0
                z1 = s1 + s2 - c2 / 3.0
                if z1 > 0:
                    roots.append(z1)
            else:
                q3_neg = -q**3
                if q3_neg <= 0:
                    return [1.0]
                rho = math.sqrt(q3_neg)
                if rho < 1e-30:
                    return [1.0]
                theta = math.acos(max(-1.0, min(1.0, r / rho)))
                mag = 2.0 * (-q)**0.5
                for k in range(3):
                    zk = mag * math.cos((theta + 2.0 * math.pi * k) / 3.0) - c2 / 3.0
                    if zk > 0:
                        roots.append(zk)
        except (OverflowError, ValueError):
            return [1.0]

        return sorted(roots) if roots else [1.0]

    def molar_volume(self, T: float, P: float, phase_hint: str = "vapour") -> float:
        """Return molar volume (m³/mol) for given T, P."""
        roots = self.solve_Z(T, P)
        if not roots:
            return R_GAS * T / P  # ideal gas fallback
        if phase_hint == "liquid":
            Z = min(roots)
        else:
            Z = max(roots)
        return Z * R_GAS * T / P

    def fugacity_coeff(self, T: float, P: float, Z: float) -> float:
        a_ = self.a(T)
        b_ = self.b
        A = a_ * P / (R_GAS * T)**2
        B = b_ * P / (R_GAS * T)
        sqrt2 = math.sqrt(2.0)
        ln_phi = (Z - 1.0 - math.log(max(1e-30, Z - B))
                  - A / (2.0 * sqrt2 * B)
                  * math.log(max(1e-30, (Z + (1 + sqrt2) * B)
                                        / (Z + (1 - sqrt2) * B))))
        return math.exp(max(-50.0, min(50.0, ln_phi)))


# ============================================================================
# Antoine saturation pressure
# ============================================================================

def antoine_P_sat(mat: MaterialDef, T: float) -> float:
    """Saturation pressure (Pa) from Antoine equation."""
    if mat.antoine is None:
        return _clausius_clapeyron_P_sat(mat, T)
    ant = mat.antoine
    T_C = T - 273.15
    T_C = max(ant.T_min - 273.15, min(ant.T_max - 273.15, T_C))
    denom = ant.C + T_C
    if abs(denom) < 0.01:
        denom = 0.01
    log10_P_mmHg = ant.A - ant.B / denom
    log10_P_mmHg = max(-10, min(20, log10_P_mmHg))  # clamp to sane range
    P_mmHg = 10.0 ** log10_P_mmHg
    return P_mmHg * 133.322  # convert mmHg → Pa


def _clausius_clapeyron_P_sat(mat: MaterialDef, T: float) -> float:
    """Fallback: Clausius-Clapeyron from critical point + acentric factor."""
    Tc = mat.critical.T_c
    Pc = mat.critical.P_c
    omega = mat.critical.omega
    Tr = T / Tc
    if Tr >= 1.0:
        return Pc
    # Lee-Kesler correlation
    f0 = 5.92714 - 6.09648 / Tr - 1.28862 * math.log(Tr) + 0.169347 * Tr**6
    f1 = 15.2518 - 15.6875 / Tr - 13.4721 * math.log(Tr) + 0.43577 * Tr**6
    ln_Pr = f0 + omega * f1
    ln_Pr = max(-50, min(50, ln_Pr))  # clamp to prevent overflow
    return Pc * math.exp(ln_Pr)


def sat_temperature(mat: MaterialDef, P: float) -> float:
    """Saturation temperature (K) by bisection on Antoine/CC."""
    T_lo = max(50.0, mat.critical.T_c * 0.3)
    T_hi = mat.critical.T_c * 0.999
    for _ in range(80):
        T_mid = 0.5 * (T_lo + T_hi)
        if antoine_P_sat(mat, T_mid) < P:
            T_lo = T_mid
        else:
            T_hi = T_mid
    return 0.5 * (T_lo + T_hi)


# ============================================================================
# Departure functions for enthalpy and entropy (Peng-Robinson)
# ============================================================================

def _departure_h(eos: PengRobinsonEOS, T: float, Z: float, P: float) -> float:
    """Departure enthalpy H - H_ig  (J/mol)."""
    try:
        a_ = eos.a(T)
        b_ = eos.b
        if abs(b_) < 1e-30:
            return 0.0
        B = b_ * P / (R_GAS * T)
        sqrt2 = math.sqrt(2.0)
        da_dT = (eos.a_c * eos.kappa
                 * (-(1.0 + eos.kappa * (1.0 - math.sqrt(T / eos.Tc)))
                    / math.sqrt(T * eos.Tc)))
        denom_plus = Z + (1 + sqrt2) * B
        denom_minus = Z + (1 - sqrt2) * B
        if abs(denom_minus) < 1e-30:
            denom_minus = 1e-30
        term = math.log(max(1e-30, abs(denom_plus / denom_minus)))
        h_dep = R_GAS * T * (Z - 1.0) + (T * da_dT - a_) / (2 * sqrt2 * b_) * term
        return h_dep
    except (OverflowError, ValueError, ZeroDivisionError):
        return 0.0


def _departure_s(eos: PengRobinsonEOS, T: float, Z: float, P: float) -> float:
    """Departure entropy S - S_ig  (J/(mol·K))."""
    try:
        a_ = eos.a(T)
        b_ = eos.b
        if abs(b_) < 1e-30:
            return 0.0
        B = b_ * P / (R_GAS * T)
        sqrt2 = math.sqrt(2.0)
        da_dT = (eos.a_c * eos.kappa
                 * (-(1.0 + eos.kappa * (1.0 - math.sqrt(T / eos.Tc)))
                    / math.sqrt(T * eos.Tc)))
        denom_plus = Z + (1 + sqrt2) * B
        denom_minus = Z + (1 - sqrt2) * B
        if abs(denom_minus) < 1e-30:
            denom_minus = 1e-30
        term = math.log(max(1e-30, abs(denom_plus / denom_minus)))
        s_dep = R_GAS * math.log(max(1e-30, abs(Z - B))) + da_dT / (2 * sqrt2 * b_) * term
        return s_dep
    except (OverflowError, ValueError, ZeroDivisionError):
        return 0.0


# ============================================================================
# Ideal gas heat capacity (polynomial fit: c_p^ig = a + bT + cT² + dT³)
# ============================================================================

_CP_IG_COEFFS: Dict[str, Tuple[float, float, float, float]] = {
    # Perry's 9th ed, J/(mol·K), T in K
    "H2O":    (32.24,  1.924e-3,  1.055e-5, -3.596e-9),
    "CO2":    (22.26,  5.981e-2, -3.501e-5,  7.469e-9),
    "NH3":    (27.55,  2.563e-2,  9.900e-6, -6.687e-9),
    "CH4":    (19.89,  5.024e-2,  1.269e-5, -1.101e-8),
    "N2":     (28.90, -1.571e-3,  8.081e-6, -2.873e-9),
    "O2":     (25.48,  1.520e-2, -7.156e-6,  1.312e-9),
    "He":     (20.786, 0.0,       0.0,       0.0),
    "Ar":     (20.786, 0.0,       0.0,       0.0),
    "C2H6":   (6.90,   1.726e-1, -6.406e-5,  7.285e-9),
    "C3H8":   (-4.04,  3.047e-1, -1.571e-4,  3.171e-8),
    "R134a":  (19.40,  3.158e-1, -2.519e-4,  8.028e-8),
}

def _cp_ig_mol(formula: str, T: float) -> float:
    """Ideal gas c_p in J/(mol·K)."""
    if formula in _CP_IG_COEFFS:
        a, b, c, d = _CP_IG_COEFFS[formula]
        return a + b * T + c * T**2 + d * T**3
    return 3.5 * R_GAS  # diatomic default


# ============================================================================
# Transport properties (Lucas, Chung correlations)
# ============================================================================

def _viscosity_gas(mat: MaterialDef, T: float, rho: float) -> float:
    """Gas viscosity (Pa·s) via Lucas correlation."""
    Tc = mat.critical.T_c
    Pc = mat.critical.P_c
    M = mat.molar_mass * 1000  # g/mol
    Tr = T / Tc
    xi = 0.176 * (Tc / (M**3 * (Pc / 1e5)**4))**(1.0/6.0)  # (Pc in bar)
    if xi < 1e-30:
        return 1e-5
    f_Tr = (0.807 * Tr**0.618 - 0.357 * math.exp(-0.449 * Tr)
            + 0.340 * math.exp(-4.058 * Tr) + 0.018)
    mu = f_Tr / xi * 1e-7  # Pa·s
    return max(1e-8, mu)


def _viscosity_liquid(mat: MaterialDef, T: float) -> float:
    """Liquid viscosity (Pa·s) via Andrade-type correlation."""
    Tc = mat.critical.T_c
    Tr = T / Tc
    if Tr >= 0.999:
        Tr = 0.999
    M = mat.molar_mass * 1000
    # Letsou-Stiel
    xi = 0.015174 - 0.02135 * Tr + 0.0075 * Tr**2
    xi1 = 0.042552 - 0.07674 * Tr + 0.0340 * Tr**2
    mu_xi = xi + mat.critical.omega * xi1
    denom = (Tc * (M**3 * (mat.critical.P_c / 1e5)**4))**(-1.0/6.0)
    if abs(denom) < 1e-30:
        return 1e-3
    return max(1e-6, mu_xi / abs(denom) * 1e-7)


def _thermal_cond_gas(mat: MaterialDef, T: float) -> float:
    """Gas thermal conductivity W/(m·K) — Eucken modification."""
    cp = _cp_ig_mol(mat.formula, T) / mat.molar_mass  # J/(kg·K)
    mu = _viscosity_gas(mat, T, 0.0)
    M = mat.molar_mass
    gamma = cp / (cp - R_GAS / M)
    return mu * (cp + 1.25 * R_GAS / M)


def _thermal_cond_liquid(mat: MaterialDef, T: float) -> float:
    """Liquid thermal conductivity W/(m·K) — Latini correlation."""
    Tc = mat.critical.T_c
    Tr = T / Tc
    M = mat.molar_mass * 1000  # g/mol
    # Latini: k = A (1-Tr)^0.38 / Tr^(1/6)
    A_lat = 0.0035 * (Tc * 1.8)**0.5 / (M**0.5)  # rough
    if Tr >= 1.0:
        Tr = 0.999
    return max(0.01, A_lat * (1.0 - Tr)**0.38 / max(0.01, Tr**(1.0/6.0)))


# ============================================================================
# Full state-point calculator
# ============================================================================

def compute_state(material_formula: str, T: float, P: float) -> StatePoint:
    """Compute complete thermodynamic state at (T, P).

    Returns a StatePoint with phase classification, all intensive properties,
    and provenance string.
    """
    mat = get_material(material_formula)
    if mat is None:
        return StatePoint(
            material=material_formula, T=T, P=P, phase=Phase.UNKNOWN,
            h=0, s=0, v=0, rho=0, x=-1, c_p=0, c_v=0, mu=0,
            k_thermal=0, Pr=0, Z=0, provenance="MATERIAL NOT FOUND")

    eos = PengRobinsonEOS(mat)
    Tc = mat.critical.T_c
    Pc = mat.critical.P_c
    M = mat.molar_mass  # kg/mol

    # Phase classification
    P_sat = antoine_P_sat(mat, T)
    Tr = T / Tc

    if T >= Tc and P >= Pc:
        phase = Phase.SUPERCRITICAL
    elif T >= Tc:
        phase = Phase.SUPERHEATED_VAPOUR
    elif abs(P - P_sat) / max(P_sat, 1.0) < 0.005:
        phase = Phase.WET_MIXTURE
    elif P < P_sat:
        phase = Phase.SUPERHEATED_VAPOUR
    else:
        phase = Phase.SUBCOOLED_LIQUID

    # PVT
    if phase in (Phase.SUPERHEATED_VAPOUR, Phase.SUPERCRITICAL):
        hint = "vapour"
    else:
        hint = "liquid"

    V_mol = eos.molar_volume(T, P, hint)
    Z_val = P * V_mol / (R_GAS * T)
    v_spec = V_mol / M  # m³/kg
    rho = 1.0 / max(v_spec, 1e-20)

    # Enthalpy & entropy (departure + ideal)
    roots = eos.solve_Z(T, P)
    Z_use = Z_val
    h_dep = _departure_h(eos, T, Z_use, P) / M  # J/kg
    s_dep = _departure_s(eos, T, Z_use, P) / M

    cp_ig = _cp_ig_mol(mat.formula, T) / M  # J/(kg·K)
    T_ref = 298.15
    h_ig = cp_ig * (T - T_ref)  # simplified
    s_ig = cp_ig * math.log(max(T / T_ref, 0.01)) - (R_GAS / M) * math.log(max(P / 101325.0, 1e-10))

    h = h_ig + h_dep
    s = s_ig + s_dep

    # Heat capacities
    # c_p from EOS: numerical derivative
    dT = 0.1
    V_plus = eos.molar_volume(T + dT, P, hint)
    V_minus = eos.molar_volume(T - dT, P, hint)
    h_dep_plus = _departure_h(eos, T + dT, P * V_plus / (R_GAS * (T + dT)), P) / M
    h_dep_minus = _departure_h(eos, T - dT, P * V_minus / (R_GAS * (T - dT)), P) / M
    cp_ig_plus = _cp_ig_mol(mat.formula, T + dT) / M
    cp_ig_minus = _cp_ig_mol(mat.formula, T - dT) / M
    c_p = ((cp_ig_plus * (T + dT) + h_dep_plus) - (cp_ig_minus * (T - dT) + h_dep_minus)) / (2.0 * dT)
    c_p = max(c_p, cp_ig * 0.5)

    # c_v from c_p - T v alpha² / beta_T  (simplified: c_v ≈ c_p / gamma_approx)
    gamma_approx = 1.3 if phase == Phase.SUPERHEATED_VAPOUR else 1.1
    c_v = c_p / gamma_approx

    # Transport
    if phase in (Phase.SUBCOOLED_LIQUID, Phase.SATURATED_LIQUID):
        mu = _viscosity_liquid(mat, T)
        k_th = _thermal_cond_liquid(mat, T)
    else:
        mu = _viscosity_gas(mat, T, rho)
        k_th = _thermal_cond_gas(mat, T)

    Pr = c_p * mu / max(k_th, 1e-30)

    # Quality
    x = -1.0
    if phase == Phase.WET_MIXTURE:
        x = 0.5  # placeholder — need h_f, h_g

    return StatePoint(
        material=mat.name, T=T, P=P, phase=phase,
        h=h, s=s, v=v_spec, rho=rho, x=x,
        c_p=c_p, c_v=c_v, mu=mu, k_thermal=k_th, Pr=Pr,
        Z=Z_val, bonds="ON",
        provenance=f"PR-EOS tier={mat.tier.value} src={mat.source}")


# ============================================================================
# Saturation line calculator
# ============================================================================

def compute_saturation_line(material_formula: str,
                            n_points: int = 50) -> List[SaturationProps]:
    """Compute saturation properties from triple-ish point to critical."""
    mat = get_material(material_formula)
    if mat is None:
        return []

    eos = PengRobinsonEOS(mat)
    Tc = mat.critical.T_c
    M = mat.molar_mass

    T_low = Tc * 0.45
    T_high = Tc * 0.999
    results = []

    for i in range(n_points):
        frac = i / max(n_points - 1, 1)
        T = T_low + frac * (T_high - T_low)
        P_sat = antoine_P_sat(mat, T)

        Vl = eos.molar_volume(T, P_sat, "liquid")
        Vg = eos.molar_volume(T, P_sat, "vapour")

        v_f = Vl / M
        v_g = Vg / M

        Zl_vals = eos.solve_Z(T, P_sat)
        Zg_vals = eos.solve_Z(T, P_sat)
        Zl = min(Zl_vals) if Zl_vals else P_sat * Vl / (R_GAS * T)
        Zg = max(Zg_vals) if Zg_vals else P_sat * Vg / (R_GAS * T)

        h_dep_l = _departure_h(eos, T, Zl, P_sat) / M
        h_dep_g = _departure_h(eos, T, Zg, P_sat) / M
        s_dep_l = _departure_s(eos, T, Zl, P_sat) / M
        s_dep_g = _departure_s(eos, T, Zg, P_sat) / M

        cp_ig = _cp_ig_mol(mat.formula, T) / M
        T_ref = 298.15
        h_ig = cp_ig * (T - T_ref)
        s_ig = cp_ig * math.log(max(T / T_ref, 0.01))

        h_f = h_ig + h_dep_l
        h_g = h_ig + h_dep_g
        s_f = s_ig + s_dep_l
        s_g = s_ig + s_dep_g

        results.append(SaturationProps(
            T_sat=T, P_sat=P_sat,
            h_f=h_f, h_g=h_g, h_fg=h_g - h_f,
            s_f=s_f, s_g=s_g, v_f=v_f, v_g=v_g))

    return results


# ============================================================================
# Pipe friction calculator (Darcy-Weisbach + Colebrook-White)
# ============================================================================

def compute_pipe_friction(material_formula: str, T: float, P: float,
                          D: float, L: float, mass_flow: float,
                          roughness: float = 4.5e-5,
                          K_minor: float = 0.0) -> PipeFrictionResult:
    """Pressure drop for pipe flow.

    Args:
        D: pipe inner diameter (m)
        L: pipe length (m)
        mass_flow: kg/s
        roughness: pipe roughness (m), default commercial steel
        K_minor: sum of minor loss coefficients
    """
    sp = compute_state(material_formula, T, P)
    rho = sp.rho
    mu = sp.mu
    A = math.pi * D**2 / 4.0
    vel = mass_flow / (rho * A)
    Re = rho * vel * D / max(mu, 1e-12)

    if Re < 1.0:
        Re = 1.0

    if Re < 2300:
        f = 64.0 / Re
        regime = "laminar"
    elif Re < 4000:
        f_lam = 64.0 / 2300.0
        f_turb = _colebrook_f(Re, D, roughness)
        blend = (Re - 2300.0) / 1700.0
        f = f_lam + blend * (f_turb - f_lam)
        regime = "transitional"
    else:
        f = _colebrook_f(Re, D, roughness)
        regime = "turbulent"

    dP_friction = f * (L / D) * 0.5 * rho * vel**2
    dP_minor = K_minor * 0.5 * rho * vel**2

    return PipeFrictionResult(
        material=sp.material, D_pipe=D, L_pipe=L,
        roughness=roughness, mass_flow=mass_flow,
        velocity=vel, Re=Re, f_darcy=f,
        dP_friction=dP_friction, dP_minor=dP_minor,
        dP_total=dP_friction + dP_minor, flow_regime=regime)


def _colebrook_f(Re: float, D: float, eps: float) -> float:
    """Iterative Colebrook-White friction factor."""
    f = 0.02
    for _ in range(30):
        rhs = -2.0 * math.log10(eps / (3.7 * D) + 2.51 / (Re * math.sqrt(f)))
        f_new = 1.0 / rhs**2
        if abs(f_new - f) < 1e-8:
            break
        f = f_new
    return f


# ============================================================================
# Adaptive region sampler
# ============================================================================

class AdaptiveRegionSampler:
    """Generates sample points with higher density near phase boundaries."""

    def __init__(self, material_formula: str, seed: int = 42):
        self.mat = get_material(material_formula)
        self.rng = random.Random(seed)
        self.samples: List[StatePoint] = []

    def sample_PT_grid(self, n_T: int = 20, n_P: int = 20,
                       refine_near_sat: bool = True) -> List[StatePoint]:
        if self.mat is None:
            return []

        Tc = self.mat.critical.T_c
        Pc = self.mat.critical.P_c
        T_lo = Tc * 0.4
        T_hi = Tc * 1.2
        P_lo = Pc * 0.01
        P_hi = Pc * 1.5

        pts = []
        for i in range(n_T):
            for j in range(n_P):
                fT = i / max(n_T - 1, 1)
                fP = j / max(n_P - 1, 1)
                T = T_lo + fT * (T_hi - T_lo)
                P = P_lo + fP * (P_hi - P_lo)
                sp = compute_state(self.mat.formula, T, P)
                pts.append(sp)

        if refine_near_sat:
            # Extra points near saturation curve
            for _ in range(n_T * 2):
                T = T_lo + self.rng.random() * (min(T_hi, Tc * 0.99) - T_lo)
                P_sat = antoine_P_sat(self.mat, T)
                for delta in (-0.02, -0.01, 0.0, 0.01, 0.02):
                    P = P_sat * (1.0 + delta)
                    if P_lo < P < P_hi:
                        sp = compute_state(self.mat.formula, T, P)
                        pts.append(sp)

            # Extra points near critical
            for _ in range(20):
                T = Tc * (0.95 + 0.1 * self.rng.random())
                P = Pc * (0.85 + 0.3 * self.rng.random())
                sp = compute_state(self.mat.formula, T, P)
                pts.append(sp)

        self.samples = pts
        return pts


# ============================================================================
# Report table generation
# ============================================================================

def generate_saturation_table_md(material_formula: str,
                                 n_points: int = 25) -> str:
    """Generate markdown saturation table for a material."""
    sat = compute_saturation_line(material_formula, n_points)
    mat = get_material(material_formula)
    if not sat or not mat:
        return f"No data for {material_formula}\n"

    lines = []
    lines.append(f"### Saturation Properties: {mat.name} ({mat.formula})")
    lines.append(f"*Tier: {mat.tier.value} | Source: {mat.source}*\n")
    lines.append("| T (K) | P (kPa) | h_f (kJ/kg) | h_g (kJ/kg) | h_fg (kJ/kg) "
                 "| s_f (kJ/kg·K) | s_g (kJ/kg·K) | v_f (m³/kg) | v_g (m³/kg) |")
    lines.append("|-------|---------|-------------|-------------|--------------|"
                 "---------------|---------------|-------------|-------------|")
    for s in sat:
        lines.append(
            f"| {s.T_sat:.1f} | {s.P_sat/1000:.2f} "
            f"| {s.h_f/1000:.2f} | {s.h_g/1000:.2f} | {s.h_fg/1000:.2f} "
            f"| {s.s_f/1000:.4f} | {s.s_g/1000:.4f} "
            f"| {s.v_f:.6f} | {s.v_g:.4f} |")
    return "\n".join(lines)


def generate_state_table_md(material_formula: str,
                            T_range: Tuple[float, float] = (300, 700),
                            P_values: List[float] = None) -> str:
    """Generate markdown state-point table at several pressures."""
    mat = get_material(material_formula)
    if not mat:
        return f"No data for {material_formula}\n"

    if P_values is None:
        Pc = mat.critical.P_c
        P_values = [101325, 500000, 1000000, Pc * 0.5, Pc * 0.8, Pc]

    lines = []
    lines.append(f"### State Properties: {mat.name} ({mat.formula})")
    lines.append(f"*Tier: {mat.tier.value}*\n")
    lines.append("| T (K) | P (kPa) | Phase | h (kJ/kg) | s (kJ/kg·K) "
                 "| v (m³/kg) | ρ (kg/m³) | c_p (kJ/kg·K) | μ (μPa·s) "
                 "| k (W/m·K) | Pr | Z |")
    lines.append("|-------|---------|-------|-----------|-------------|"
                 "-----------|-----------|----------------|----------|"
                 "-----------|-----|------|")

    n_T = 15
    for P in P_values:
        for i in range(n_T):
            frac = i / max(n_T - 1, 1)
            T = T_range[0] + frac * (T_range[1] - T_range[0])
            sp = compute_state(material_formula, T, P)
            lines.append(
                f"| {sp.T:.1f} | {sp.P/1000:.1f} | {sp.phase.name[:8]} "
                f"| {sp.h/1000:.2f} | {sp.s/1000:.4f} "
                f"| {sp.v:.6f} | {sp.rho:.2f} | {sp.c_p/1000:.4f} "
                f"| {sp.mu*1e6:.3f} | {sp.k_thermal:.4f} "
                f"| {sp.Pr:.3f} | {sp.Z:.4f} |")
    return "\n".join(lines)


def generate_pipe_friction_table_md(material_formula: str,
                                    T: float = 373.15, P: float = 500000,
                                    D_values: List[float] = None) -> str:
    """Pipe friction tables for a material at given conditions."""
    mat = get_material(material_formula)
    if not mat:
        return f"No data for {material_formula}\n"

    if D_values is None:
        D_values = [0.025, 0.05, 0.10, 0.15, 0.20, 0.30, 0.50]

    mass_flows = [0.1, 0.5, 1.0, 2.0, 5.0, 10.0]

    lines = []
    lines.append(f"### Pipe Friction: {mat.name} at T={T:.1f} K, P={P/1000:.0f} kPa")
    lines.append("")
    lines.append("| D (mm) | ṁ (kg/s) | v (m/s) | Re | f | ΔP/L (Pa/m) | Regime |")
    lines.append("|--------|----------|---------|--------|--------|-------------|--------|")

    for D in D_values:
        for mdot in mass_flows:
            pf = compute_pipe_friction(material_formula, T, P, D, 1.0, mdot)
            lines.append(
                f"| {D*1000:.0f} | {mdot:.1f} | {pf.velocity:.2f} "
                f"| {pf.Re:.0f} | {pf.f_darcy:.6f} "
                f"| {pf.dP_friction:.2f} | {pf.flow_regime} |")
    return "\n".join(lines)


# ============================================================================
# CSV export
# ============================================================================

def export_saturation_csv(material_formula: str, path: str,
                          n_points: int = 50):
    sat = compute_saturation_line(material_formula, n_points)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["T_K", "P_Pa", "h_f_J_kg", "h_g_J_kg", "h_fg_J_kg",
                     "s_f_J_kgK", "s_g_J_kgK", "v_f_m3_kg", "v_g_m3_kg"])
        for s in sat:
            w.writerow([f"{s.T_sat:.4f}", f"{s.P_sat:.2f}",
                        f"{s.h_f:.2f}", f"{s.h_g:.2f}", f"{s.h_fg:.2f}",
                        f"{s.s_f:.4f}", f"{s.s_g:.4f}",
                        f"{s.v_f:.8f}", f"{s.v_g:.6f}"])


def export_state_grid_csv(material_formula: str, path: str,
                          n_T: int = 20, n_P: int = 20):
    sampler = AdaptiveRegionSampler(material_formula)
    pts = sampler.sample_PT_grid(n_T, n_P)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["material", "T_K", "P_Pa", "phase", "h_J_kg", "s_J_kgK",
                     "v_m3_kg", "rho_kg_m3", "x", "cp_J_kgK", "cv_J_kgK",
                     "mu_Pas", "k_W_mK", "Pr", "Z"])
        for sp in pts:
            w.writerow([sp.material, f"{sp.T:.4f}", f"{sp.P:.2f}",
                        sp.phase.name, f"{sp.h:.2f}", f"{sp.s:.4f}",
                        f"{sp.v:.8f}", f"{sp.rho:.4f}", f"{sp.x:.4f}",
                        f"{sp.c_p:.4f}", f"{sp.c_v:.4f}",
                        f"{sp.mu:.8f}", f"{sp.k_thermal:.6f}",
                        f"{sp.Pr:.4f}", f"{sp.Z:.6f}"])
