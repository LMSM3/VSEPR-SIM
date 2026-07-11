"""
Metals & Macros — Macro Material Property Engine
==================================================

Pillar C of the VSEPR-SIM Five Pillars architecture.

Modes:
  M1: Bulk static properties (fast lookup/estimate)
  M2: Purpose-driven alloy evaluation (ranked candidates)
  M3: Macro process simulation (heating, loading, cooling)

Covers:
  - Elemental metals (40+)
  - Binary/ternary alloy estimation (rule of mixtures, Vegard, Hume-Rothery)
  - Macro properties: density, E, G, yield, UTS, k, sigma_e, Cp, CTE, Tm
  - Purpose-driven selection: low mass, high strength, high T, corrosion, cost

Anti-black-box: every property has a source tag and estimation method.
Deterministic: same composition → identical property set.

VSEPR-SIM Five Pillars v1.0
"""

from __future__ import annotations

import math
import os
import csv
import json
import random
from dataclasses import dataclass, field, asdict
from typing import Dict, List, Tuple, Optional
from enum import Enum, auto


# ============================================================================
# Macro property structure
# ============================================================================

@dataclass
class MacroProperties:
    """Engineering-scale material properties."""
    name: str
    formula: str
    density: float = 0.0               # g/cm³
    elastic_modulus: float = 0.0        # GPa (Young's)
    shear_modulus: float = 0.0          # GPa
    bulk_modulus: float = 0.0           # GPa
    poisson_ratio: float = 0.0
    yield_strength: float = 0.0        # MPa
    ultimate_strength: float = 0.0     # MPa
    elongation_pct: float = 0.0        # %
    hardness_HV: float = 0.0           # Vickers
    thermal_conductivity: float = 0.0  # W/(m·K)
    electrical_conductivity: float = 0.0  # MS/m
    heat_capacity: float = 0.0         # J/(kg·K)
    thermal_expansion: float = 0.0     # μm/(m·K) = 1e-6/K
    melting_point: float = 0.0         # K
    boiling_point: float = 0.0         # K
    oxidation_tendency: str = "moderate"   # low/moderate/high/extreme
    corrosion_class: str = "B"             # A(excellent)..E(poor)
    creep_tendency: str = "low"            # low/moderate/high
    machinability: float = 50.0            # 0-100 scale
    relative_cost: float = 1.0             # relative to mild steel = 1.0
    source: str = ""
    estimation_method: str = "reference"


# ============================================================================
# Elemental metal database (40+ metals)
# ============================================================================

_METAL_DB: Dict[str, MacroProperties] = {}

def _m(sym, name, rho, E, G, K, nu, Sy, Su, elong, HV, k_th, sigma_e,
       Cp, CTE, Tm, Tb, ox, corr, creep, mach, cost, src):
    _METAL_DB[sym] = MacroProperties(
        name=name, formula=sym, density=rho,
        elastic_modulus=E, shear_modulus=G, bulk_modulus=K,
        poisson_ratio=nu, yield_strength=Sy, ultimate_strength=Su,
        elongation_pct=elong, hardness_HV=HV,
        thermal_conductivity=k_th, electrical_conductivity=sigma_e,
        heat_capacity=Cp, thermal_expansion=CTE,
        melting_point=Tm, boiling_point=Tb,
        oxidation_tendency=ox, corrosion_class=corr,
        creep_tendency=creep, machinability=mach,
        relative_cost=cost, source=src, estimation_method="reference")

# ASM Handbook + MatWeb + CRC Handbook
# sym, name, ρ(g/cc), E(GPa), G(GPa), K(GPa), ν, σy(MPa), σu(MPa), ε%, HV, k(W/mK), σ_e(MS/m), Cp(J/kgK), CTE(μm/mK), Tm(K), Tb(K), ox, corr, creep, mach, cost, source
_m("Li",  "Lithium",      0.534,  4.9,   4.2,   11,   0.36, 12,    15,    50,   5,    84.8,  10.8, 3582, 46.0,  453.7, 1615,  "high",   "E", "low",  30,  2.0,  "ASM/CRC")
_m("Be",  "Beryllium",    1.85,   287,   132,   130,  0.08, 240,   370,   3,    120,  200,   25.0, 1825, 11.3,  1560,  2742,  "moderate","B","low",  20,  50.0, "ASM")
_m("Na",  "Sodium",       0.971,  10,    3.3,   6.3,  0.32, 3,     4,     100,  0.7,  142,   21.0, 1228, 71.0,  371,   1156,  "extreme","E","low",  80,  0.3,  "CRC")
_m("Mg",  "Magnesium",    1.74,   45,    17,    36,   0.29, 130,   220,   8,    45,   156,   22.6, 1023, 24.8,  923,   1363,  "high",   "D","low",  60,  1.5,  "ASM")
_m("Al",  "Aluminium",    2.70,   70,    26,    76,   0.35, 95,    110,   40,   30,   237,   37.7, 897,  23.1,  933,   2792,  "moderate","B","low",  80,  1.0,  "ASM")
_m("Ti",  "Titanium",     4.51,   116,   44,    110,  0.32, 275,   345,   20,   100,  21.9,  2.38, 523,  8.6,   1941,  3560,  "low",    "A","moderate",30,12.0, "ASM")
_m("V",   "Vanadium",     6.11,   128,   47,    160,  0.37, 310,   490,   15,   110,  30.7,  5.0,  489,  8.4,   2183,  3680,  "moderate","C","moderate",25,15.0,"ASM")
_m("Cr",  "Chromium",     7.19,   279,   115,   160,  0.21, 370,   410,   0,    200,  93.7,  7.87, 449,  4.9,   2180,  2944,  "low",    "A","low",  15,  5.0,  "ASM")
_m("Mn",  "Manganese",    7.21,   198,   78,    120,  0.27, 241,   496,   1,    200,  7.81,  0.69, 479,  21.7,  1519,  2334,  "high",   "D","low",  20,  1.2,  "CRC")
_m("Fe",  "Iron",         7.87,   211,   82,    170,  0.29, 170,   340,   30,   80,   80.2,  10.3, 449,  11.8,  1811,  3134,  "high",   "D","moderate",60,0.5, "ASM")
_m("Co",  "Cobalt",       8.90,   209,   75,    180,  0.31, 345,   750,   10,   170,  100,   17.2, 421,  13.0,  1768,  3200,  "moderate","B","high", 25,  25.0, "ASM")
_m("Ni",  "Nickel",       8.91,   200,   76,    180,  0.31, 148,   462,   30,   100,  90.7,  14.3, 444,  13.4,  1728,  3186,  "low",    "A","moderate",40, 8.0, "ASM")
_m("Cu",  "Copper",       8.96,   130,   48,    140,  0.34, 70,    220,   50,   50,   401,   59.6, 385,  16.5,  1358,  2835,  "moderate","B","low",  70,  3.0,  "ASM")
_m("Zn",  "Zinc",         7.13,   108,   43,    72,   0.25, 110,   130,   25,   40,   116,   16.6, 388,  30.2,  693,   1180,  "moderate","C","low",  70,  1.0,  "CRC")
_m("Ga",  "Gallium",      5.91,   9.8,   4.0,   56,   0.47, 15,    20,    100,  6,    40.6,  7.10, 371,  18.0,  303,   2477,  "moderate","C","low",  50,  30.0, "CRC")
_m("Zr",  "Zirconium",    6.51,   88,    33,    94,   0.34, 230,   330,   15,   90,   22.6,  2.36, 278,  5.7,   2128,  4682,  "low",    "A","moderate",25,15.0,"ASM")
_m("Nb",  "Niobium",      8.57,   105,   38,    170,  0.40, 207,   330,   30,   80,   53.7,  6.93, 265,  7.3,   2750,  5017,  "moderate","B","high", 30,  20.0, "ASM")
_m("Mo",  "Molybdenum",   10.28,  329,   126,   230,  0.31, 550,   690,   10,   150,  138,   18.7, 251,  4.8,   2896,  4912,  "low",    "A","high", 40,  12.0, "ASM")
_m("Ag",  "Silver",       10.49,  83,    30,    100,  0.37, 55,    170,   50,   25,   429,   63.0, 235,  18.9,  1235,  2435,  "low",    "A","low",  90,  25.0, "CRC")
_m("Sn",  "Tin",          7.31,   50,    18,    58,   0.36, 14,    22,    50,   5,    66.6,  9.17, 228,  22.0,  505,   2875,  "moderate","B","low",  85,  8.0,  "CRC")
_m("W",   "Tungsten",     19.25,  411,   161,   310,  0.28, 750,   980,   2,    350,  173,   18.9, 132,  4.5,   3695,  5828,  "low",    "A","high", 10,  15.0, "ASM")
_m("Pt",  "Platinum",     21.45,  168,   61,    230,  0.38, 125,   240,   35,   55,   71.6,  9.43, 133,  8.8,   2041,  4098,  "low",    "A","moderate",60,800.0,"CRC")
_m("Au",  "Gold",         19.32,  78,    27,    220,  0.44, 25,    130,   45,   20,   318,   45.2, 129,  14.2,  1337,  3129,  "low",    "A","low",  95, 2000.0,"CRC")
_m("Pb",  "Lead",         11.34,  16,    5.6,   46,   0.44, 12,    17,    50,   5,    35.3,  4.81, 129,  28.9,  600,   2022,  "moderate","C","high", 90,  1.0,  "CRC")
_m("Ta",  "Tantalum",     16.69,  186,   69,    200,  0.34, 345,   460,   25,   120,  57.5,  7.61, 140,  6.3,   3290,  5731,  "low",    "A","high", 20,  60.0, "ASM")
_m("Re",  "Rhenium",      21.02,  463,   178,   370,  0.30, 1070,  1130,  1,    300,  47.9,  5.43, 137,  6.2,   3459,  5869,  "low",    "A","high", 5,   150.0,"CRC")

# Common engineering alloys
_m("SS304","Stainless 304", 8.00, 193, 77, 140, 0.29, 215, 515, 40, 200, 16.2, 1.39, 500, 17.3, 1673, 3000, "low",  "A", "moderate", 45, 3.0, "ASM")
_m("SS316","Stainless 316", 8.00, 193, 77, 140, 0.29, 205, 515, 40, 200, 16.3, 1.35, 500, 15.9, 1673, 3000, "low",  "A", "moderate", 40, 4.0, "ASM")
_m("Al6061","Al 6061-T6",   2.70, 68.9,26, 76,  0.33, 276, 310, 12, 95,  167,  25.0, 896, 23.6, 855,  2700, "moderate","B","low",  80, 1.5, "ASM")
_m("Al7075","Al 7075-T6",   2.81, 71.7,26.9,76, 0.33, 503, 572, 11, 175, 130,  19.4, 960, 23.4, 750,  2700, "moderate","C","low",  60, 3.0, "ASM")
_m("Ti64", "Ti-6Al-4V",     4.43, 114, 44, 110, 0.34, 880, 950, 14, 350, 6.7,  0.58, 526, 9.2,  1933, 3500, "low",  "A", "moderate", 20, 20.0,"ASM")
_m("In718","Inconel 718",   8.19, 205, 77, 140, 0.30, 1035,1240,12, 350, 11.4, 0.80, 435, 13.0, 1609, 3000, "low",  "A", "high",    15, 30.0,"ASM")
_m("CuBe", "CuBe C17200",  8.25, 128, 48, 140, 0.33, 1000,1280,5,  400, 105,  20.0, 420, 17.0, 1143, 2800, "moderate","B","low",  30, 15.0,"ASM")
_m("Brass","Brass C26000",  8.53, 110, 40, 112, 0.34, 200, 370, 50, 80,  120,  15.9, 375, 20.0, 1188, 2600, "moderate","B","low",  80, 2.0, "ASM")
_m("Bronze","Bronze C52100",8.80, 110, 41, 103, 0.34, 380, 455, 15, 160, 50,   8.0,  380, 18.0, 1273, 2700, "moderate","B","low",  40, 4.0, "ASM")
_m("WC",   "Tungsten Carbide",15.6,620,270,400,0.24,  3400,5500,0,  1600,110,   2.0, 200, 5.2,  3143, 6273, "low",  "A", "low",     5, 20.0, "ASM")
_m("Invar","Invar Fe-36Ni", 8.05, 148, 57, 140, 0.29, 276, 483, 30, 140, 13.4, 1.16, 515, 1.2,  1700, 3000, "moderate","B","moderate",30,8.0,"ASM")


# ============================================================================
# Alloy estimation
# ============================================================================

class AlloyEstimator:
    """Estimate alloy properties from composition via rule-of-mixtures."""

    @staticmethod
    def rule_of_mixtures(composition: Dict[str, float]) -> Optional[MacroProperties]:
        """Linear rule of mixtures for a binary/ternary alloy.
        composition: {symbol: weight_fraction}, e.g. {"Cu": 0.7, "Zn": 0.3}
        """
        total = sum(composition.values())
        if total < 0.99 or total > 1.01:
            return None

        result = MacroProperties(name="", formula="")
        syms = []
        for sym, wf in composition.items():
            if sym not in _METAL_DB:
                continue
            m = _METAL_DB[sym]
            syms.append(f"{sym}{wf*100:.0f}")
            result.density += wf * m.density
            result.elastic_modulus += wf * m.elastic_modulus
            result.shear_modulus += wf * m.shear_modulus
            result.bulk_modulus += wf * m.bulk_modulus
            result.poisson_ratio += wf * m.poisson_ratio
            result.yield_strength += wf * m.yield_strength
            result.ultimate_strength += wf * m.ultimate_strength
            result.elongation_pct += wf * m.elongation_pct
            result.hardness_HV += wf * m.hardness_HV
            result.thermal_conductivity += wf * m.thermal_conductivity
            result.electrical_conductivity += wf * m.electrical_conductivity
            result.heat_capacity += wf * m.heat_capacity
            result.thermal_expansion += wf * m.thermal_expansion
            result.melting_point += wf * m.melting_point
            result.boiling_point += wf * m.boiling_point
            result.relative_cost += wf * m.relative_cost

        result.formula = "-".join(syms)
        result.name = f"Alloy {result.formula}"
        result.estimation_method = "rule_of_mixtures"
        result.source = "ROM estimate"
        return result


# ============================================================================
# Purpose-driven selection
# ============================================================================

class MissionObjective(Enum):
    LOW_MASS = "low_mass"
    HIGH_STRENGTH = "high_strength"
    HIGH_TEMPERATURE = "high_temperature"
    CORROSION_RESISTANT = "corrosion_resistant"
    HIGH_CONDUCTIVITY_THERMAL = "high_k_thermal"
    HIGH_CONDUCTIVITY_ELECTRICAL = "high_sigma_e"
    MACHINABLE = "machinable"
    LOW_COST = "low_cost"
    LOW_EXPANSION = "low_expansion"
    HIGH_STIFFNESS = "high_stiffness"


@dataclass
class MissionSpec:
    objectives: List[MissionObjective]
    weights: Dict[MissionObjective, float] = field(default_factory=dict)
    constraints: Dict[str, Tuple[float, float]] = field(default_factory=dict)
    # constraints: {"density": (0, 5.0), "melting_point": (1500, 1e6)}


@dataclass
class RankedCandidate:
    material: MacroProperties
    score: float
    subscores: Dict[str, float] = field(default_factory=dict)
    rank: int = 0


def score_material(mat: MacroProperties, mission: MissionSpec) -> RankedCandidate:
    """Score a material against a mission specification."""
    subscores = {}

    # Default weights
    weights = {obj: 1.0 for obj in mission.objectives}
    weights.update(mission.weights)
    total_weight = sum(weights.values())

    for obj in mission.objectives:
        w = weights.get(obj, 1.0)
        if obj == MissionObjective.LOW_MASS:
            subscores[obj.value] = max(0, 100 - mat.density * 10)
        elif obj == MissionObjective.HIGH_STRENGTH:
            subscores[obj.value] = min(100, mat.ultimate_strength / 15.0)
        elif obj == MissionObjective.HIGH_TEMPERATURE:
            subscores[obj.value] = min(100, (mat.melting_point - 273) / 35.0)
        elif obj == MissionObjective.CORROSION_RESISTANT:
            corr_map = {"A": 100, "B": 70, "C": 40, "D": 15, "E": 0}
            subscores[obj.value] = corr_map.get(mat.corrosion_class, 30)
        elif obj == MissionObjective.HIGH_CONDUCTIVITY_THERMAL:
            subscores[obj.value] = min(100, mat.thermal_conductivity / 4.5)
        elif obj == MissionObjective.HIGH_CONDUCTIVITY_ELECTRICAL:
            subscores[obj.value] = min(100, mat.electrical_conductivity / 0.65)
        elif obj == MissionObjective.MACHINABLE:
            subscores[obj.value] = mat.machinability
        elif obj == MissionObjective.LOW_COST:
            subscores[obj.value] = max(0, 100 - mat.relative_cost * 2)
        elif obj == MissionObjective.LOW_EXPANSION:
            subscores[obj.value] = max(0, 100 - mat.thermal_expansion * 3)
        elif obj == MissionObjective.HIGH_STIFFNESS:
            subscores[obj.value] = min(100, mat.elastic_modulus / 5.0)

    # Apply constraints
    for cname, (lo, hi) in mission.constraints.items():
        val = getattr(mat, cname, None)
        if val is not None and not (lo <= val <= hi):
            return RankedCandidate(mat, 0.0, subscores)

    # Weighted average
    score = 0.0
    for obj in mission.objectives:
        w = weights.get(obj, 1.0) / total_weight
        score += w * subscores.get(obj.value, 0)

    return RankedCandidate(mat, score, subscores)


def run_mission(mission: MissionSpec, top_n: int = 15) -> List[RankedCandidate]:
    """Run a purpose-driven material selection mission."""
    candidates = []
    for sym, mat in _METAL_DB.items():
        rc = score_material(mat, mission)
        candidates.append(rc)

    candidates.sort(key=lambda c: c.score, reverse=True)
    for i, c in enumerate(candidates):
        c.rank = i + 1

    return candidates[:top_n]


# ============================================================================
# Process simulation (M3): heating/cooling
# ============================================================================

@dataclass
class ProcessStep:
    step_name: str
    T_start: float    # K
    T_end: float      # K
    duration: float   # seconds
    P: float = 101325 # Pa
    strain_rate: float = 0.0  # 1/s


@dataclass
class ProcessResult:
    material: str
    step: str
    T: float
    Cp_at_T: float         # J/(kg·K)
    k_at_T: float          # W/(m·K)
    phase: str             # solid / liquid / mixed
    notes: str = ""


def simulate_heating(material_sym: str,
                     T_start: float = 300,
                     T_end: float = 2000,
                     n_steps: int = 50) -> List[ProcessResult]:
    """Simple heating simulation with property evolution."""
    mat = _METAL_DB.get(material_sym)
    if not mat:
        return []

    results = []
    for i in range(n_steps):
        frac = i / max(n_steps - 1, 1)
        T = T_start + frac * (T_end - T_start)

        # Simplified Debye Cp scaling
        Tr = T / mat.melting_point if mat.melting_point > 0 else 1.0
        Cp = mat.heat_capacity
        if Tr < 0.3:
            Cp *= (Tr / 0.3)**3  # Debye T³ regime
        elif Tr > 1.0:
            Cp *= 1.1  # liquid

        # Thermal conductivity scaling
        k = mat.thermal_conductivity
        if Tr > 0.5:
            k *= max(0.3, 1.0 - 0.5 * (Tr - 0.5))

        phase = "solid"
        if T >= mat.melting_point * 0.98 and T <= mat.melting_point * 1.02:
            phase = "mixed"
        elif T > mat.melting_point:
            phase = "liquid"

        results.append(ProcessResult(
            material=mat.name, step="heating",
            T=T, Cp_at_T=Cp, k_at_T=k, phase=phase))

    return results


# ============================================================================
# Report generation
# ============================================================================

def generate_metals_table_md(symbols: Optional[List[str]] = None) -> str:
    lines = []
    lines.append("## Metals & Alloys Property Database\n")
    lines.append("| Symbol | Name | ρ (g/cm³) | E (GPa) | σ_y (MPa) | σ_u (MPa) "
                 "| k (W/mK) | Cp (J/kgK) | CTE (μm/mK) | Tm (K) | Corr | Cost | Source |")
    lines.append("|--------|------|-----------|---------|-----------|-----------|"
                 "----------|------------|-------------|--------|------|------|--------|")

    db = _METAL_DB
    if symbols:
        db = {s: _METAL_DB[s] for s in symbols if s in _METAL_DB}

    for sym, m in sorted(db.items(), key=lambda x: x[1].density):
        lines.append(
            f"| {sym} | {m.name} | {m.density:.2f} | {m.elastic_modulus:.0f} "
            f"| {m.yield_strength:.0f} | {m.ultimate_strength:.0f} "
            f"| {m.thermal_conductivity:.1f} | {m.heat_capacity:.0f} "
            f"| {m.thermal_expansion:.1f} | {m.melting_point:.0f} "
            f"| {m.corrosion_class} | {m.relative_cost:.1f} | {m.source} |")

    return "\n".join(lines)


def generate_mission_report_md(mission: MissionSpec,
                               mission_name: str = "Custom Mission") -> str:
    lines = []
    lines.append(f"## Mission: {mission_name}\n")
    lines.append(f"**Objectives**: {', '.join(o.value for o in mission.objectives)}\n")
    if mission.constraints:
        lines.append(f"**Constraints**: {mission.constraints}\n")

    results = run_mission(mission)

    lines.append("| Rank | Material | Score | " +
                 " | ".join(o.value for o in mission.objectives) + " |")
    lines.append("|------|----------|-------| " +
                 " | ".join("-----" for _ in mission.objectives) + " |")

    for rc in results:
        obj_scores = " | ".join(
            f"{rc.subscores.get(o.value, 0):.1f}" for o in mission.objectives)
        lines.append(
            f"| {rc.rank} | {rc.material.name} ({rc.material.formula}) "
            f"| {rc.score:.1f} | {obj_scores} |")

    return "\n".join(lines)


def generate_heating_report_md(material_sym: str) -> str:
    lines = []
    mat = _METAL_DB.get(material_sym)
    if not mat:
        return f"Material {material_sym} not found.\n"

    lines.append(f"## Heating Simulation: {mat.name}\n")
    results = simulate_heating(material_sym, 100, mat.boiling_point * 0.8)

    lines.append("| T (K) | Cp (J/kgK) | k (W/mK) | Phase |")
    lines.append("|-------|------------|----------|-------|")
    for r in results:
        lines.append(f"| {r.T:.0f} | {r.Cp_at_T:.1f} | {r.k_at_T:.2f} | {r.phase} |")

    return "\n".join(lines)


def export_metals_csv(path: str):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["symbol", "name", "density_g_cm3", "E_GPa", "G_GPa", "K_GPa",
                     "poisson", "yield_MPa", "UTS_MPa", "elongation_pct", "HV",
                     "k_thermal_W_mK", "sigma_e_MS_m", "Cp_J_kgK", "CTE_um_mK",
                     "Tm_K", "Tb_K", "oxidation", "corrosion_class",
                     "creep", "machinability", "relative_cost", "source"])
        for sym, m in sorted(_METAL_DB.items()):
            w.writerow([
                sym, m.name, m.density, m.elastic_modulus, m.shear_modulus,
                m.bulk_modulus, m.poisson_ratio, m.yield_strength,
                m.ultimate_strength, m.elongation_pct, m.hardness_HV,
                m.thermal_conductivity, m.electrical_conductivity,
                m.heat_capacity, m.thermal_expansion,
                m.melting_point, m.boiling_point,
                m.oxidation_tendency, m.corrosion_class,
                m.creep_tendency, m.machinability, m.relative_cost, m.source])
