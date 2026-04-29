"""
metallic_cp — Metallic specific heat capacity (c_p) engine.

Computes molar and specific heat capacity for metallic elements and
alloys using:
  1. Debye model:          C_v(T) = 9R (T/Θ_D)³ ∫₀^{Θ_D/T} x⁴eˣ/(eˣ-1)² dx
  2. Electronic correction: C_el = γ T  (Sommerfeld coefficient)
  3. Dulong–Petit limit:    C_v → 3R  as T → ∞
  4. Empirical reference table for 30+ common metals

The module feeds the heating-over-time simulation pipeline and is
continuously validated by the thermo test suite.

VSEPR-SIM 4.0.4 — Metals v3: Lanthanides, Noble Gases, Organometallic Centers
"""

from __future__ import annotations

import math
from dataclasses import dataclass, field
from typing import Optional


# ═══════════════════════════════════════════════════════════════════════
# Constants
# ═══════════════════════════════════════════════════════════════════════

R = 8.314462618    # J/(mol·K) — gas constant
kB = 1.380649e-23  # J/K — Boltzmann
NA = 6.02214076e23 # Avogadro


# ═══════════════════════════════════════════════════════════════════════
# Missing-data sentinel (integer-sentinel pattern — never compare floats)
# ═══════════════════════════════════════════════════════════════════════

MISSING: float = -9999.0   # sentinel: value unknown / not measured
"""Use MISSING instead of NaN or None.  Check with `v == MISSING`
(exact int-backed comparison, safe on Z80-era and modern alike)."""


# ═══════════════════════════════════════════════════════════════════════
# Empirical metal database
# ═══════════════════════════════════════════════════════════════════════

@dataclass(frozen=True)
class MetalRecord:
    """Empirical data for one metallic element."""
    symbol: str
    Z: int
    name: str
    molar_mass: float        # g/mol
    density: float           # g/cm³
    theta_D: float           # Debye temperature (K)
    gamma: float             # Sommerfeld coeff (mJ/(mol·K²))
    melting_point: float     # K
    cp_298: float            # Experimental c_p at 298 K (J/(mol·K))
    thermal_cond: float      # W/(m·K) at 298 K


# CRC Handbook + Kittel "Intro to Solid State Physics" + ASM data
METAL_DB: dict[str, MetalRecord] = {}

def _add(sym, Z, name, M, rho, tD, gam, Tm, cp, k):
    METAL_DB[sym] = MetalRecord(sym, Z, name, M, rho, tD, gam, Tm, cp, k)

_add("Li",  3,  "Lithium",      6.941,  0.534, 344,  1.63, 453.7,  24.86, 84.8)
_add("Be",  4,  "Beryllium",    9.012,  1.85,  1440, 0.17, 1560,   16.44, 200)
_add("Na",  11, "Sodium",       22.99,  0.971, 158,  1.38, 370.9,  28.23, 142)
_add("Mg",  12, "Magnesium",    24.31,  1.738, 400,  1.3,  923,    24.87, 156)
_add("Al",  13, "Aluminum",     26.98,  2.70,  428,  1.35, 933.5,  24.20, 237)
_add("K",   19, "Potassium",    39.10,  0.862, 91,   2.08, 336.5,  29.60, 102.5)
_add("Ca",  20, "Calcium",      40.08,  1.55,  230,  2.9,  1115,   25.93, 201)
_add("Ti",  22, "Titanium",     47.87,  4.506, 420,  3.35, 1941,   25.06, 21.9)
_add("V",   23, "Vanadium",     50.94,  6.11,  380,  9.26, 2183,   24.89, 30.7)
_add("Cr",  24, "Chromium",     52.00,  7.15,  630,  1.40, 2180,   23.35, 93.9)
_add("Mn",  25, "Manganese",    54.94,  7.44,  410,  9.20, 1519,   26.32, 7.81)
_add("Fe",  26, "Iron",         55.85,  7.874, 470,  4.98, 1811,   25.10, 80.4)
_add("Co",  27, "Cobalt",       58.93,  8.90,  445,  4.73, 1768,   24.81, 100)
_add("Ni",  28, "Nickel",       58.69,  8.908, 450,  7.02, 1728,   26.07, 90.9)
_add("Cu",  29, "Copper",       63.55,  8.96,  343,  0.695,1357.8, 24.44, 401)
_add("Zn",  30, "Zinc",         65.38,  7.134, 327,  0.64, 692.7,  25.39, 116)
_add("Ga",  31, "Gallium",      69.72,  5.91,  320,  0.60, 302.9,  25.86, 40.6)
_add("Zr",  40, "Zirconium",    91.22,  6.52,  291,  2.80, 2128,   25.36, 22.7)
_add("Nb",  41, "Niobium",      92.91,  8.57,  275,  7.79, 2750,   24.60, 53.7)
_add("Mo",  42, "Molybdenum",   95.95,  10.28, 450,  2.0,  2896,   24.06, 138)
_add("Ag",  47, "Silver",       107.87, 10.49, 225,  0.646,1234.9, 25.35, 429)
_add("Sn",  50, "Tin",          118.71, 7.287, 200,  1.78, 505.1,  27.11, 66.8)
_add("Ta",  73, "Tantalum",     180.95, 16.69, 240,  5.9,  3290,   25.36, 57.5)
_add("W",   74, "Tungsten",     183.84, 19.25, 400,  1.01, 3695,   24.27, 173)
_add("Pt",  78, "Platinum",     195.08, 21.45, 240,  6.8,  2041.4, 25.86, 71.6)
_add("Au",  79, "Gold",         196.97, 19.30, 165,  0.729,1337.3, 25.42, 318)
_add("Pb",  82, "Lead",         207.2,  11.34, 105,  2.98, 600.6,  26.65, 35.3)
_add("Bi",  83, "Bismuth",      208.98, 9.78,  119,  0.008,544.6,  25.52, 7.97)
_add("U",   92, "Uranium",      238.03, 19.1,  200,  9.14, 1405.3, 27.67, 27.5)
# ^^^ Debye θ_D corrected to 200 K (IAEA/Grimvall consensus; prior value 207 K
#     was from a single-crystal measurement not representative of reactor-grade α-U).
#     Melting point 1405.3 K is the accepted IAEA value.
_add("Th",  90, "Thorium",      232.04, 11.7,  170,  4.32, 2023.0, 26.23, 54.0)
_add("Pu",  94, "Plutonium",    244.06, 19.85, 162,  12.8, 912.5,  32.77, 6.74)

# ─── Lanthanides (4f series) ───
# Debye temps from Gschneidner & Eyring "Handbook on the Physics and Chemistry
# of Rare Earths"; Sommerfeld coefficients from low-T calorimetry reviews.
# These elements are anomalous in Debye/Sommerfeld models due to crystal-field
# splitting of 4f levels — standard VSEPR patterning is insufficient.
_add("La",  57, "Lanthanum",    138.91, 6.15,  142,  9.4,  1193,   27.11, 13.4)
_add("Ce",  58, "Cerium",       140.12, 6.77,  146, 12.8,  1068,   26.94, 11.3)
_add("Pr",  59, "Praseodymium", 140.91, 6.77,  152,  7.5,  1208,   27.20, 12.5)
_add("Nd",  60, "Neodymium",    144.24, 7.01,  159, 10.0,  1297,   27.45, 16.5)
_add("Gd",  64, "Gadolinium",   157.25, 7.90,  169,  5.8,  1585,   37.03, 10.6)
_add("Dy",  66, "Dysprosium",   162.50, 8.55,  180,  6.3,  1680,   27.70, 10.7)
_add("Er",  68, "Erbium",       167.26, 9.07,  188,  7.6,  1802,   28.12, 14.5)
_add("Lu",  71, "Lutetium",     174.97, 9.84,  210,  8.2,  1925,   26.86, 16.4)

# ─── Noble gases (van der Waals solids at low T; gas at STP) ───
# Debye temps are for the fcc/hcp solid phases near 0 K;
# melting points are at 1 atm. Sommerfeld γ ≈ 0 (filled shells).
# These break VSEPR assumptions entirely — no directional bonding.
_add("He",  2,  "Helium",        4.003, 0.145,  25,   0.0,   0.95,  20.79, 0.152)
_add("Ne",  10, "Neon",         20.18,  1.207,  75,   0.0,  24.56,  20.79, 0.0491)
_add("Ar",  18, "Argon",        39.95,  1.40,   93,   0.0,  83.81,  20.79, 0.0177)
_add("Kr",  36, "Krypton",      83.80,  2.413,  72,   0.0, 115.78,  20.79, 0.00943)
_add("Xe",  54, "Xenon",       131.29,  2.942,  64,   0.0, 161.40,  20.79, 0.00565)

# ─── Additional transition metals (organometallic / catalytic relevance) ───
# Rh, Pd, Ir — key organometallic catalytic centers;
# Hf — group 4 metallocene chemistry; Re, Os — heavy 5d metals.
_add("Rh",  45, "Rhodium",     102.91, 12.41, 480,  4.9,  2237,   24.98, 150)
_add("Pd",  46, "Palladium",   106.42, 12.02, 274,  9.42, 1828.1, 25.98, 71.8)
_add("Hf",  72, "Hafnium",     178.49, 13.31, 252,  2.16, 2506,   25.73, 23.0)
_add("Re",  75, "Rhenium",     186.21, 21.02, 430,  2.3,  3459,   25.48, 47.9)
_add("Os",  76, "Osmium",      190.23, 22.59, 500,  2.35, 3306,   24.70, 87.6)
_add("Ir",  77, "Iridium",     192.22, 22.56, 420,  3.1,  2719,   25.10, 147)
_add("Ru",  44, "Ruthenium",   101.07, 12.37, 600,  3.3,  2607,   24.06, 117)

del _add


def lookup_metal(symbol: str) -> Optional[MetalRecord]:
    """Look up a metal by element symbol (case-insensitive first letter)."""
    key = symbol.strip()
    if key in METAL_DB:
        return METAL_DB[key]
    # Try capitalisation fix
    cap = key[0].upper() + key[1:].lower() if len(key) > 1 else key.upper()
    return METAL_DB.get(cap)


def all_metals() -> list[MetalRecord]:
    return list(METAL_DB.values())


# ═══════════════════════════════════════════════════════════════════════
# Debye model
# ═══════════════════════════════════════════════════════════════════════

def _debye_integrand(x: float) -> float:
    """x⁴ eˣ / (eˣ - 1)²  with overflow protection."""
    if x > 500:
        return 0.0
    ex = math.exp(x)
    denom = (ex - 1.0) ** 2
    if denom < 1e-300:
        return 0.0
    return x ** 4 * ex / denom


def debye_cv(T: float, theta_D: float, n_atoms: int = 1, n_points: int = 200) -> float:
    """Lattice heat capacity via Debye model.

    Returns C_v in J/(mol·K) for *n_atoms* atoms per formula unit.

    C_v = 9 n R (T/Θ_D)³ ∫₀^{Θ_D/T} x⁴eˣ/(eˣ-1)² dx
    """
    if T <= 0 or theta_D <= 0:
        return 0.0

    u = theta_D / T
    # Numerical integration (Simpson's rule)
    h = u / n_points
    integral = _debye_integrand(0.0) + _debye_integrand(u)
    for i in range(1, n_points):
        xi = i * h
        weight = 4.0 if i % 2 == 1 else 2.0
        integral += weight * _debye_integrand(xi)
    integral *= h / 3.0

    return 9.0 * n_atoms * R * (T / theta_D) ** 3 * integral


def electronic_cv(T: float, gamma_mJ: float) -> float:
    """Electronic heat capacity: C_el = γ T.

    *gamma_mJ* is the Sommerfeld coefficient in mJ/(mol·K²).
    Returns C_el in J/(mol·K).
    """
    return gamma_mJ * 1e-3 * T


def dulong_petit(n_atoms: int = 1) -> float:
    """Classical high-T limit: C_v = 3nR."""
    return 3.0 * n_atoms * R


# ═══════════════════════════════════════════════════════════════════════
# Composite c_p calculator
# ═══════════════════════════════════════════════════════════════════════

@dataclass
class CpResult:
    """Heat capacity result at a single temperature."""
    T: float                # Temperature (K)
    Cv_lattice: float       # Debye lattice contribution (J/(mol·K))
    Cv_electronic: float    # Sommerfeld electronic contribution
    Cv_total: float         # Cv_lattice + Cv_electronic
    Cp_approx: float        # ≈ Cv + correction (for solids Cp ≈ Cv)
    cp_specific: float      # J/(g·K)  (Cp / molar_mass)
    dulong_petit: float     # 3nR limit
    fraction_dp: float      # Cv_lattice / dulong_petit


def compute_cp(metal: MetalRecord, T: float) -> CpResult:
    """Compute c_p for a metal at temperature T (K)."""
    cv_lat = debye_cv(T, metal.theta_D, n_atoms=1)
    cv_el  = electronic_cv(T, metal.gamma)
    cv_tot = cv_lat + cv_el

    # Cp − Cv correction for metals is small (~1-5%) at moderate T.
    # Use the Nernst–Lindemann approximation:
    #   Cp − Cv = A · Cp² · T / Tm
    # where A ≈ 0.0032 mol/J for metals (Grimvall 1999).
    # Iterative: Cp = Cv / (1 − A·Cv·T/Tm)
    A = 0.0032
    denom = 1.0 - A * cv_tot * T / max(metal.melting_point, 1.0)
    if denom < 0.5:
        denom = 0.5  # safety clamp for near-melting
    cp = cv_tot / denom

    dp = dulong_petit(1)
    frac = cv_lat / dp if dp > 0 else 0.0

    return CpResult(
        T=T,
        Cv_lattice=cv_lat,
        Cv_electronic=cv_el,
        Cv_total=cv_tot,
        Cp_approx=cp,
        cp_specific=cp / metal.molar_mass,
        dulong_petit=dp,
        fraction_dp=frac,
    )


def compute_cp_curve(
    metal: MetalRecord,
    T_start: float = 10.0,
    T_end: float = 1000.0,
    n_points: int = 100,
) -> list[CpResult]:
    """Compute c_p over a temperature range."""
    results = []
    dT = (T_end - T_start) / max(n_points - 1, 1)
    for i in range(n_points):
        T = T_start + i * dT
        results.append(compute_cp(metal, T))
    return results
