#!/usr/bin/env python3
"""
colour_pillar_plant_gen.py  --  VSEPR-SIM Colour Pillar Plant Generator
========================================================================
Defines five Colour Pillars (broad classification system), each with:
  - an assigned colour
  - a specialised sub-element (within the classical element category)

Then randomises a complete power plant configuration:
  - pipe metals and radii
  - fuel salt
  - secondary cycle working fluid
  - generates PVT diagrams (steam-table style) for each fluid

Reuses five_prong_data_gen compute_state / compute_saturation_line APIs
and pykernel.pipe3_plant_helper for pipe network analysis.

VSEPR-SIM 4.0.4
"""
from __future__ import annotations
import json, csv, math, os, random, time, sys, pathlib
from dataclasses import dataclass, field
from typing import Dict, List, Tuple, Optional

# path setup
ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))

from five_prong_data_gen import get_all_materials, compute_state, compute_saturation_line
from pykernel.pipe3_plant_helper import (
    PipeSegment, PlantPipeNetwork, NetworkAnalyzer,
    MaterialContext, InsulationType, NuclearContext,
    PlantEngineeringState, ATM,
)

OUT = ROOT / "out" / "colour_pillar_plants"
OUT.mkdir(parents=True, exist_ok=True)

# =====================================================================
# 1. COLOUR PILLARS -- 10-element taxonomy
# =====================================================================
# Classical base layer (5):
#   WOOD       Olive       growth, branching, flexibility, organic expansion
#   FIRE       Crimson     heat, excitation, combustion, radiance
#   EARTH      Amber       structure, bulk, mineral stability, support
#   METAL      Steel       rigidity, conductivity, refinement, edge
#   WATER      Cerulean    flow, dissolution, cooling, adaptation
#
# Modern-material extensions (5):
#   NUCLEAR    Magenta     decay, fission/fusion, transmutation, deep core energy
#   NOBLE      Gold        inertness, shielding, low reactivity, rarefied purity
#   PLASTIC    Lime        polymers, moldability, deformation memory, versatility
#   CERAMIC    Terracotta  brittleness, heat resistance, insulation, hard-shell
#   PLASMA     Violet      ionized matter, arcs, fields, radiative flow

@dataclass
class ColourPillar:
    name: str
    colour_hex: str
    traits: str
    description: str = ""

PILLARS: Dict[str, ColourPillar] = {
    # -- classical base layer --
    "WOOD":     ColourPillar("WOOD",     "#808000",
                             "growth, branching, flexibility, organic expansion",
                             "Organic materials, cellulosics, biological fuels"),
    "FIRE":     ColourPillar("FIRE",     "#DC143C",
                             "heat, excitation, combustion, radiance",
                             "Combustion enthalpy, thermal excitation, radiative transfer"),
    "EARTH":    ColourPillar("EARTH",    "#FFBF00",
                             "structure, bulk, mineral stability, support",
                             "Crystal structure, density, vacancy, mineral phases"),
    "METAL":    ColourPillar("METAL",    "#71797E",
                             "rigidity, conductivity, refinement, edge",
                             "Alloy selection, thermal conductivity, tensile strength"),
    "WATER":    ColourPillar("WATER",    "#007BA7",
                             "flow, dissolution, cooling, adaptation",
                             "Phase equilibria, salinity, coolant performance, dissolution"),
    # -- modern-material extensions --
    "NUCLEAR":  ColourPillar("NUCLEAR",  "#FF00FF",
                             "decay, fission/fusion, transmutation, deep core energy",
                             "Fuel salt composition, neutronics, decay heat, transmutation"),
    "NOBLE":    ColourPillar("NOBLE",    "#FFD700",
                             "inertness, shielding, low reactivity, rarefied purity",
                             "Noble gas coolants, inert cover gas, chemical shielding"),
    "PLASTIC":  ColourPillar("PLASTIC",  "#32CD32",
                             "polymers, moldability, deformation memory, versatility",
                             "Polymer seals, flexible couplings, deformation tolerance"),
    "CERAMIC":  ColourPillar("CERAMIC",  "#E2725B",
                             "brittleness, heat resistance, insulation, hard-shell",
                             "Thermal insulation, refractory linings, oxide barriers"),
    "PLASMA":   ColourPillar("PLASMA",   "#8B00FF",
                             "ionized matter, arcs, fields, radiative flow",
                             "Ionisation, plasma-facing, arc interactions, field coupling"),
}

# =====================================================================
# 2. Material pools for randomisation (per-pillar)
# =====================================================================

PIPE_ALLOYS = [
    "Hastelloy-N", "Inconel-617", "Inconel-718", "SS-316L",
    "SS-304", "Monel-400", "Incoloy-800H", "C276",
    "Ti-6Al-4V", "Zircaloy-4", "A106-B", "P91",
]

# NUCLEAR pillar: molten-salt and liquid-metal reactor coolants
FUEL_SALTS = ["Li2BeF4", "NaCl_m", "NaNO3KNO3", "Na_l", "PbBi"]

# FIRE / WATER pillar: power-cycle working fluids
SECONDARY_FLUIDS = ["H2O", "CO2", "N2", "He", "NH3", "Ar"]

# PLASTIC pillar: refrigerants / condenser-side fluids
TERTIARY_FLUIDS = ["H2O", "R134a", "R32", "R410A", "NH3", "CO2", "R22", "iC4H10"]

# WOOD pillar: organic hydrocarbons and alcohols (fuel pre-heat side-stream)
WOOD_ORGANICS = ["CH4", "C3H8", "C8H18", "JP8", "C2H5OH", "nC12H26",
                 "C6H6", "C7H8", "C2H6", "C5H12", "C6H14", "CH3OH"]

# NOBLE pillar: noble / inert gas cover-gas / shielding options
NOBLE_GASES = ["He", "Ar", "Ne", "Xe"]

# PLASMA pillar: high-T working fluids that brush plasma-adjacent behaviour
PLASMA_FLUIDS = ["H2", "He", "Ar", "N2"]

INSULATION_CHOICES = list(InsulationType)

# Alloy thermal conductivity proxy table  W/(m K)  (Metal pillar channel)
ALLOY_K = {
    "Hastelloy-N":   11.5, "Inconel-617":   13.8, "Inconel-718":   11.4,
    "SS-316L":       16.2, "SS-304":        16.3, "Monel-400":     21.8,
    "Incoloy-800H":  14.0, "C276":          10.2, "Ti-6Al-4V":      6.7,
    "Zircaloy-4":    13.8, "A106-B":        50.7, "P91":           29.0,
}

# Alloy ultimate tensile strength proxy table  MPa  (Metal pillar channel)
ALLOY_UTS = {
    "Hastelloy-N":  800, "Inconel-617":  740, "Inconel-718": 1380,
    "SS-316L":      485, "SS-304":       515, "Monel-400":   540,
    "Incoloy-800H": 520, "C276":         785, "Ti-6Al-4V":   950,
    "Zircaloy-4":   570, "A106-B":       415, "P91":         620,
}

# =====================================================================
# 3. Randomised plant configuration
# =====================================================================

@dataclass
class PlantConfig:
    plant_id: str
    seed: int
    # primary loop (NUCLEAR pillar)
    fuel_salt: str = ""
    primary_alloy: str = ""
    primary_diameter_m: float = 0.0
    primary_T_K: float = 0.0
    primary_P_atm: float = 0.0
    # secondary loop / power cycle (FIRE + WATER pillar)
    secondary_fluid: str = ""
    secondary_alloy: str = ""
    secondary_diameter_m: float = 0.0
    secondary_T_K: float = 0.0
    secondary_P_atm: float = 0.0
    # tertiary / condenser loop (PLASTIC pillar)
    tertiary_fluid: str = ""
    tertiary_alloy: str = ""
    tertiary_diameter_m: float = 0.0
    # quaternary organic pre-heat side-stream (WOOD pillar)
    organic_fluid: str = ""
    organic_alloy: str = ""
    organic_diameter_m: float = 0.0
    organic_T_K: float = 0.0
    organic_P_atm: float = 0.0
    # noble cover-gas (NOBLE pillar)
    noble_gas: str = ""
    noble_cover_P_atm: float = 0.0
    pillar_tags: List[str] = field(default_factory=list)

    def summary(self) -> str:
        lines = [
            f"Plant {self.plant_id}  (seed={self.seed})",
            f"  PRIMARY   : {self.fuel_salt} in {self.primary_alloy} "
            f"D={self.primary_diameter_m:.4f}m  T={self.primary_T_K:.0f}K  "
            f"P={self.primary_P_atm:.1f}atm",
            f"  SECONDARY : {self.secondary_fluid} in {self.secondary_alloy} "
            f"D={self.secondary_diameter_m:.4f}m  T={self.secondary_T_K:.0f}K  "
            f"P={self.secondary_P_atm:.1f}atm",
            f"  TERTIARY  : {self.tertiary_fluid} in {self.tertiary_alloy} "
            f"D={self.tertiary_diameter_m:.4f}m",
            f"  ORGANIC   : {self.organic_fluid} in {self.organic_alloy} "
            f"D={self.organic_diameter_m:.4f}m  T={self.organic_T_K:.0f}K  "
            f"P={self.organic_P_atm:.1f}atm",
            f"  NOBLE GAS : {self.noble_gas}  cover P={self.noble_cover_P_atm:.2f}atm",
            f"  PILLARS   : {', '.join(self.pillar_tags)}",
        ]
        return '\n'.join(lines)


def randomise_plant(seed: int) -> PlantConfig:
    rng = random.Random(seed)
    organic = rng.choice(WOOD_ORGANICS)
    noble   = rng.choice(NOBLE_GASES)
    tertiary = rng.choice(TERTIARY_FLUIDS)
    cfg = PlantConfig(
        plant_id=f"PLT-{seed:06d}",
        seed=seed,
        # primary (NUCLEAR)
        fuel_salt=rng.choice(FUEL_SALTS),
        primary_alloy=rng.choice(PIPE_ALLOYS),
        primary_diameter_m=round(rng.uniform(0.05, 0.40), 4),
        primary_T_K=round(rng.uniform(850, 1050), 1),
        primary_P_atm=round(rng.uniform(1.0, 5.0), 2),
        # secondary (FIRE + WATER)
        secondary_fluid=rng.choice(SECONDARY_FLUIDS),
        secondary_alloy=rng.choice(PIPE_ALLOYS),
        secondary_diameter_m=round(rng.uniform(0.08, 0.50), 4),
        secondary_T_K=round(rng.uniform(550, 850), 1),
        secondary_P_atm=round(rng.uniform(5.0, 250.0), 2),
        # tertiary / condenser (PLASTIC)
        tertiary_fluid=tertiary,
        tertiary_alloy=rng.choice(PIPE_ALLOYS),
        tertiary_diameter_m=round(rng.uniform(0.10, 0.60), 4),
        # organic pre-heat side-stream (WOOD)
        organic_fluid=organic,
        organic_alloy=rng.choice(PIPE_ALLOYS),
        organic_diameter_m=round(rng.uniform(0.03, 0.15), 4),
        organic_T_K=round(rng.uniform(300, 550), 1),
        organic_P_atm=round(rng.uniform(1.0, 30.0), 2),
        # noble cover-gas (NOBLE)
        noble_gas=noble,
        noble_cover_P_atm=round(rng.uniform(0.5, 3.0), 2),
    )
    tags = []
    # -- classical base layer (always present) --
    tags.append("WOOD")    # organic pre-heat loop always defined
    tags.append("FIRE")    # thermal source always present
    tags.append("EARTH")   # mineral/structural basis always present
    tags.append("METAL")   # alloy piping always present
    tags.append("WATER")   # cooling loop always present
    # -- modern extensions --
    tags.append("NUCLEAR")  # MSR = nuclear domain
    tags.append("NOBLE")    # noble cover-gas always defined
    # PLASTIC: refrigerant-class tertiary fluid
    if tertiary in ("R134a", "R32", "R410A", "R22", "iC4H10"):
        tags.append("PLASTIC")
    # CERAMIC: refractory/mineral insulation on any segment
    if any(ins.name in ("MINERAL_WOOL", "CERAMIC_FIBER", "CALCIUM_SILICATE")
           for ins in INSULATION_CHOICES):
        tags.append("CERAMIC")
    # PLASMA: high-temperature primary loop (T > 950 K)
    if cfg.primary_T_K > 950:
        tags.append("PLASMA")
    cfg.pillar_tags = sorted(set(tags))
    return cfg

# =====================================================================
# 4. PVT diagram generation (steam-table style)
# =====================================================================

def generate_pvt_grid(formula: str, n_T: int = 60, n_P: int = 60) -> List[dict]:
    mats = get_all_materials()
    mat = mats.get(formula)
    if mat is None:
        return []
    Tc, Pc = mat.critical.T_c, mat.critical.P_c
    T_lo = max(Tc * 0.30, 100.0)
    T_hi = Tc * 1.60
    P_lo = 1e4
    P_hi = Pc * 2.5
    rows = []
    for i_T in range(n_T):
        T = T_lo + i_T * (T_hi - T_lo) / max(n_T - 1, 1)
        for i_P in range(n_P):
            P = P_lo + i_P * (P_hi - P_lo) / max(n_P - 1, 1)
            try:
                sp = compute_state(formula, T, P)
                rows.append({
                    "material": formula, "T_K": round(T, 3),
                    "P_Pa": round(P, 1),
                    "phase": sp.phase.name if hasattr(sp.phase, 'name') else str(sp.phase),
                    "h_Jkg": round(sp.h, 4) if sp.h else None,
                    "s_JkgK": round(sp.s, 6) if sp.s else None,
                    "v_m3kg": f"{sp.v:.6e}" if sp.v else None,
                    "cp_JkgK": round(sp.c_p, 4) if sp.c_p else None,
                    "Z_comp": round(sp.Z, 6) if sp.Z else None,
                })
            except Exception:
                pass
    return rows


def generate_saturation(formula: str, n_pts: int = 120) -> List[dict]:
    try:
        sats = compute_saturation_line(formula, n_points=n_pts)
        rows = []
        for sp in sats:
            rows.append({
                "material": formula,
                "T_sat_K": round(sp.T_sat, 3),
                "P_sat_Pa": round(sp.P_sat, 1),
                "h_f": round(sp.h_f, 4) if sp.h_f else None,
                "h_g": round(sp.h_g, 4) if sp.h_g else None,
                "h_fg": round(sp.h_fg, 4) if sp.h_fg else None,
                "v_f": f"{sp.v_f:.6e}" if sp.v_f else None,
                "v_g": f"{sp.v_g:.6e}" if sp.v_g else None,
                "s_f": round(sp.s_f, 6) if sp.s_f else None,
                "s_g": round(sp.s_g, 6) if sp.s_g else None,
            })
        return rows
    except Exception:
        return []

# =====================================================================
# 5. Per-pillar specialized data channel generators
# =====================================================================
# Each returns List[dict] — written to pillar_{NAME}_channel.csv
# All are lightweight derivations from atlas data already in memory.

def _ch_wood(cfg: PlantConfig) -> List[dict]:
    """WOOD: organic fluid thermophysical profile across T sweep."""
    rows = []
    mats = get_all_materials()
    mat = mats.get(cfg.organic_fluid)
    if mat is None:
        return rows
    Tc = mat.critical.T_c
    T_lo = max(Tc * 0.35, 200.0)
    T_hi = min(Tc * 0.95, cfg.organic_T_K + 200)
    P = cfg.organic_P_atm * ATM
    for i in range(40):
        T = T_lo + i * (T_hi - T_lo) / 39
        try:
            sp = compute_state(cfg.organic_fluid, T, P)
            rows.append({
                "pillar": "WOOD", "fluid": cfg.organic_fluid,
                "T_K": round(T, 2), "P_Pa": round(P, 1),
                "phase": sp.phase.name if hasattr(sp.phase, 'name') else str(sp.phase),
                "h_Jkg": round(sp.h, 3) if sp.h else None,
                "cp_JkgK": round(sp.c_p, 3) if sp.c_p else None,
                "mu_Pa_s": f"{sp.mu:.4e}" if sp.mu else None,
                "k_W_mK": round(sp.k_thermal, 5) if sp.k_thermal else None,
                "Pr": round(sp.Pr, 4) if sp.Pr else None,
                "note": "organic pre-heat side-stream",
            })
        except Exception:
            pass
    return rows


def _ch_fire(cfg: PlantConfig) -> List[dict]:
    """FIRE: combustion enthalpy / heat release proxy across primary T sweep."""
    rows = []
    mats = get_all_materials()
    mat = mats.get(cfg.fuel_salt)
    if mat is None:
        mat = mats.get("H2O")
        formula = "H2O"
    else:
        formula = cfg.fuel_salt
    Tc = mat.critical.T_c
    T_lo = max(Tc * 0.40, 500.0)
    T_hi = cfg.primary_T_K
    P = cfg.primary_P_atm * ATM
    T_prev = None
    h_prev = None
    for i in range(30):
        T = T_lo + i * (T_hi - T_lo) / 29
        try:
            sp = compute_state(formula, T, P)
            dh_dT = ((sp.h - h_prev) / (T - T_prev)) if (h_prev is not None and T != T_prev) else None
            rows.append({
                "pillar": "FIRE", "fluid": formula,
                "T_K": round(T, 2), "P_Pa": round(P, 1),
                "h_Jkg": round(sp.h, 3) if sp.h else None,
                "cp_JkgK": round(sp.c_p, 3) if sp.c_p else None,
                "dh_dT": round(dh_dT, 4) if dh_dT else None,
                "note": "thermal excitation / heat release proxy",
            })
            T_prev, h_prev = T, sp.h
        except Exception:
            pass
    return rows


def _ch_earth(cfg: PlantConfig) -> List[dict]:
    """EARTH: mineral/critical-property compendium for all plant fluids."""
    rows = []
    mats = get_all_materials()
    fluids = sorted(set([cfg.fuel_salt, cfg.secondary_fluid,
                         cfg.tertiary_fluid, cfg.organic_fluid, cfg.noble_gas]))
    for formula in fluids:
        mat = mats.get(formula)
        if mat is None:
            continue
        rows.append({
            "pillar": "EARTH", "material": formula,
            "name": mat.name, "formula": mat.formula,
            "molar_mass_g_mol": mat.molar_mass,
            "T_c_K": mat.critical.T_c,
            "P_c_Pa": mat.critical.P_c,
            "source": mat.source,
            "tags": "|".join(mat.tags) if mat.tags else "",
            "note": "crystal/mineral stability reference",
        })
    return rows


def _ch_metal(cfg: PlantConfig) -> List[dict]:
    """METAL: alloy thermal conductivity and tensile strength table."""
    rows = []
    alloys_in_plant = sorted(set([
        cfg.primary_alloy, cfg.secondary_alloy,
        cfg.tertiary_alloy, cfg.organic_alloy,
    ]))
    for alloy in alloys_in_plant:
        k = ALLOY_K.get(alloy, 0.0)
        uts = ALLOY_UTS.get(alloy, 0)
        rows.append({
            "pillar": "METAL", "alloy": alloy,
            "k_W_mK": k,
            "UTS_MPa": uts,
            "k_rank": round(k / max(ALLOY_K.values()), 4),
            "uts_rank": round(uts / max(ALLOY_UTS.values()), 4),
            "note": "thermal conductivity and tensile proxy",
        })
    return rows


def _ch_water(cfg: PlantConfig) -> List[dict]:
    """WATER: saturation spread (v_g/v_f, h_fg) — dissolution/brine proxy."""
    rows = []
    # Focus on primary salt, secondary fluid, tertiary
    for formula in [cfg.fuel_salt, cfg.secondary_fluid, cfg.tertiary_fluid]:
        sat = generate_saturation(formula, n_pts=30)
        for sp in sat:
            vf = float(sp.get("v_f", 0) or 0)
            vg = float(sp.get("v_g", 0) or 0)
            hfg = float(sp.get("h_fg", 0) or 0)
            rows.append({
                "pillar": "WATER", "fluid": formula,
                "T_sat_K": sp.get("T_sat_K"),
                "P_sat_Pa": sp.get("P_sat_Pa"),
                "h_fg_Jkg": round(hfg, 2) if hfg else None,
                "vg_vf_ratio": round(vg / vf, 3) if (vf and vf > 0) else None,
                "v_f": sp.get("v_f"),
                "v_g": sp.get("v_g"),
                "note": "phase-spread / dissolution proxy",
            })
    return rows


def _ch_nuclear(cfg: PlantConfig) -> List[dict]:
    """NUCLEAR: fuel salt critical state survey."""
    rows = []
    mats = get_all_materials()
    mat = mats.get(cfg.fuel_salt)
    if mat is None:
        return rows
    Tc = mat.critical.T_c
    Pc = mat.critical.P_c
    for i in range(20):
        T = cfg.primary_T_K * (0.85 + i * 0.015)
        P = cfg.primary_P_atm * ATM
        try:
            sp = compute_state(cfg.fuel_salt, T, P)
            rows.append({
                "pillar": "NUCLEAR", "fuel_salt": cfg.fuel_salt,
                "T_K": round(T, 2), "P_Pa": round(P, 1),
                "T_Tc_ratio": round(T / Tc, 4),
                "P_Pc_ratio": round(P / Pc, 6),
                "phase": sp.phase.name if hasattr(sp.phase, 'name') else str(sp.phase),
                "h_Jkg": round(sp.h, 3) if sp.h else None,
                "Z": round(sp.Z, 5) if sp.Z else None,
                "note": "deep-core thermal survey",
            })
        except Exception:
            pass
    return rows


def _ch_noble(cfg: PlantConfig) -> List[dict]:
    """NOBLE: cover-gas inertness and shielding across pressure range."""
    rows = []
    mats = get_all_materials()
    mat = mats.get(cfg.noble_gas)
    if mat is None:
        return rows
    T = 300.0  # room-temperature cover-gas reference
    for i in range(20):
        P = cfg.noble_cover_P_atm * ATM * (0.5 + i * 0.1)
        try:
            sp = compute_state(cfg.noble_gas, T, P)
            rows.append({
                "pillar": "NOBLE", "gas": cfg.noble_gas,
                "T_K": round(T, 2), "P_Pa": round(P, 1),
                "P_atm": round(P / ATM, 3),
                "Z_comp": round(sp.Z, 6) if sp.Z else None,
                "rho_kg_m3": round(1.0 / sp.v, 4) if sp.v else None,
                "mu_Pa_s": f"{sp.mu:.4e}" if sp.mu else None,
                "note": "inert shielding / cover-gas purity",
            })
        except Exception:
            pass
    return rows


def _ch_plastic(cfg: PlantConfig) -> List[dict]:
    """PLASTIC: refrigerant/condenser fluid thermophysical sweep."""
    rows = []
    mats = get_all_materials()
    mat = mats.get(cfg.tertiary_fluid)
    if mat is None:
        return rows
    Tc = mat.critical.T_c
    T_lo = max(Tc * 0.40, 200.0)
    T_hi = Tc * 0.90
    P = 5.0 * ATM  # typical condenser-side pressure
    for i in range(25):
        T = T_lo + i * (T_hi - T_lo) / 24
        try:
            sp = compute_state(cfg.tertiary_fluid, T, P)
            rows.append({
                "pillar": "PLASTIC", "fluid": cfg.tertiary_fluid,
                "T_K": round(T, 2), "P_Pa": round(P, 1),
                "phase": sp.phase.name if hasattr(sp.phase, 'name') else str(sp.phase),
                "h_Jkg": round(sp.h, 3) if sp.h else None,
                "cp_JkgK": round(sp.c_p, 3) if sp.c_p else None,
                "x_quality": round(sp.x, 4) if sp.x and sp.x >= 0 else None,
                "note": "polymer/refrigerant condenser loop",
            })
        except Exception:
            pass
    return rows


def _ch_ceramic(cfg: PlantConfig, seg_dicts: List[dict]) -> List[dict]:
    """CERAMIC: insulation thermal resistance per pipe segment."""
    rows = []
    INS_K = {
        "NONE": 0.0, "MINERAL_WOOL": 0.04, "CERAMIC_FIBER": 0.08,
        "CALCIUM_SILICATE": 0.06, "AEROGEL": 0.015,
    }
    for seg in seg_dicts:
        ins_type = str(seg.get("insulation", "NONE")).upper().replace(" ", "_")
        k_ins = INS_K.get(ins_type, 0.04)
        D = float(seg.get("D_m", 0.1) or 0.1)
        wall = float(seg.get("wall_thickness_m", 0.01) if "wall_thickness_m" in seg else 0.01)
        # R_wall  = ln(r_o/r_i) / (2πkL), we use a per-unit-length proxy
        r_i = D / 2
        r_o = r_i + wall
        R_wall = math.log(r_o / r_i) / (2 * math.pi * 15.0) if r_i > 0 else 0  # k_steel ~15
        T_K = float(seg.get("T_K", 900) or 900)
        rows.append({
            "pillar": "CERAMIC",
            "segment": seg.get("segment_id", ""),
            "label": seg.get("label", ""),
            "insulation_type": ins_type,
            "k_ins_W_mK": k_ins,
            "R_wall_mK_W": round(R_wall, 6),
            "T_K": round(T_K, 1),
            "note": "refractory / insulation thermal resistance",
        })
    return rows


def _ch_plasma(cfg: PlantConfig) -> List[dict]:
    """PLASMA: surface heat flux proxy at high-T operating conditions."""
    rows = []
    mats = get_all_materials()
    # Use the hottest loop fluid as the plasma-adjacent medium
    mat = mats.get(cfg.fuel_salt)
    if mat is None:
        return rows
    sigma = 5.670374419e-8  # Stefan-Boltzmann W/(m² K⁴)
    emissivities = [0.15, 0.30, 0.50, 0.70, 0.85]  # surface finish range
    T_amb = 900.0  # hot-side ambient
    for i in range(20):
        T = cfg.primary_T_K * (0.90 + i * 0.01)
        for eps in emissivities:
            q_rad = eps * sigma * (T**4 - T_amb**4)
            q_conv_proxy = 500.0 * (T - T_amb)  # h~500 W/(m²K) rough proxy
            rows.append({
                "pillar": "PLASMA",
                "T_K": round(T, 1), "T_amb_K": round(T_amb, 1),
                "emissivity": eps,
                "q_rad_W_m2": round(q_rad, 2),
                "q_conv_proxy_W_m2": round(q_conv_proxy, 2),
                "q_total_W_m2": round(q_rad + q_conv_proxy, 2),
                "note": "ionised/radiant surface flux proxy",
            })
    return rows


def generate_pillar_channels(
    cfg: PlantConfig,
    seg_dicts: List[dict],
    plant_dir: pathlib.Path,
) -> Dict[str, str]:
    """
    Dispatch all active pillar channel generators and write CSVs.
    Returns dict mapping pillar_name -> filename (relative to plant_dir).
    """
    generators = {
        "WOOD":    lambda: _ch_wood(cfg),
        "FIRE":    lambda: _ch_fire(cfg),
        "EARTH":   lambda: _ch_earth(cfg),
        "METAL":   lambda: _ch_metal(cfg),
        "WATER":   lambda: _ch_water(cfg),
        "NUCLEAR": lambda: _ch_nuclear(cfg),
        "NOBLE":   lambda: _ch_noble(cfg),
        "PLASTIC": lambda: _ch_plastic(cfg),
        "CERAMIC": lambda: _ch_ceramic(cfg, seg_dicts),
        "PLASMA":  lambda: _ch_plasma(cfg),
    }
    channel_files: Dict[str, str] = {}
    for pillar in cfg.pillar_tags:
        gen = generators.get(pillar)
        if gen is None:
            continue
        rows = gen()
        if not rows:
            continue
        fname = f"pillar_{pillar}_channel.csv"
        fpath = plant_dir / fname
        with open(fpath, "w", newline="", encoding="utf-8") as f:
            w = csv.DictWriter(f, fieldnames=rows[0].keys())
            w.writeheader()
            w.writerows(rows)
        channel_files[pillar] = fname
        print(f"  [{pillar}] channel: {fname} ({len(rows)} rows)")
    return channel_files



def build_network(cfg: PlantConfig) -> PlantPipeNetwork:
    rng = random.Random(cfg.seed + 7)
    net = PlantPipeNetwork(f"{cfg.plant_id}")
    net.engineering.material.structural_alloy = cfg.primary_alloy
    net.engineering.nuclear.fuel_salt_composition = {cfg.fuel_salt: 1.0}

    # primary loop segments (fuel salt side, He as proxy gas for flow calc)
    for i, (label, length) in enumerate([
        ("Core outlet",    rng.uniform(3, 8)),
        ("Hot leg",        rng.uniform(10, 30)),
        ("IHX primary",    rng.uniform(4, 12)),
        ("Cold leg",       rng.uniform(10, 30)),
        ("Pump suction",   rng.uniform(2, 6)),
    ]):
        net.add(PipeSegment(
            label=f"P-{label}",
            length_m=round(length, 2),
            inner_diameter_m=cfg.primary_diameter_m,
            wall_thickness_m=round(rng.uniform(0.005, 0.02), 4),
            roughness_m=4.5e-5,
            gas_symbol="He",
            T_K=cfg.primary_T_K - i * rng.uniform(5, 20),
            P_Pa=cfg.primary_P_atm * ATM,
            velocity_m_s=round(rng.uniform(1.5, 6.0), 2),
            n_elbows=rng.randint(1, 4),
            n_valves=rng.randint(0, 2),
            material=MaterialContext(
                structural_alloy=cfg.primary_alloy,
                salt_chemistry=cfg.fuel_salt,
            ),
            insulation=rng.choice(INSULATION_CHOICES),
            insulation_thickness_m=round(rng.uniform(0.03, 0.12), 3),
        ))

    # secondary loop (power cycle fluid -- FIRE + WATER pillar)
    _SEC_GAS_MAP = {"He": "He", "CO2": "CO2", "N2": "N2", "Ar": "Ar",
                    "H2O": "H2O", "NH3": "NH3"}
    sec_gas = _SEC_GAS_MAP.get(cfg.secondary_fluid, "N2")
    for i, (label, length) in enumerate([
        ("IHX secondary",  rng.uniform(4, 12)),
        ("Turbine inlet",  rng.uniform(5, 15)),
        ("Turbine outlet", rng.uniform(5, 15)),
        ("Recuperator",    rng.uniform(6, 20)),
        ("Precooler",      rng.uniform(4, 10)),
        ("Compressor in",  rng.uniform(3, 8)),
    ]):
        net.add(PipeSegment(
            label=f"S-{label}",
            length_m=round(length, 2),
            inner_diameter_m=cfg.secondary_diameter_m,
            wall_thickness_m=round(rng.uniform(0.004, 0.015), 4),
            roughness_m=4.5e-5,
            gas_symbol=sec_gas,
            T_K=cfg.secondary_T_K - i * rng.uniform(10, 40),
            P_Pa=cfg.secondary_P_atm * ATM,
            velocity_m_s=round(rng.uniform(15, 60), 2),
            n_elbows=rng.randint(2, 6),
            n_valves=rng.randint(1, 3),
            material=MaterialContext(structural_alloy=cfg.secondary_alloy),
            insulation=rng.choice(INSULATION_CHOICES),
            insulation_thickness_m=round(rng.uniform(0.02, 0.08), 3),
        ))

    # quaternary organic pre-heat side-stream (WOOD pillar)
    # uses CH4 as GAS_DB proxy if organic is not directly in it
    _ORGANIC_GAS_MAP = {"CH4": "CH4", "H2": "H2"}
    org_gas = _ORGANIC_GAS_MAP.get(cfg.organic_fluid, "CH4")
    for i, (label, length) in enumerate([
        ("Organic feed",   rng.uniform(3, 10)),
        ("Pre-heater",     rng.uniform(5, 15)),
        ("Vaporizer out",  rng.uniform(4, 10)),
    ]):
        net.add(PipeSegment(
            label=f"W-{label}",
            length_m=round(length, 2),
            inner_diameter_m=cfg.organic_diameter_m,
            wall_thickness_m=round(rng.uniform(0.003, 0.010), 4),
            roughness_m=4.5e-5,
            gas_symbol=org_gas,
            T_K=cfg.organic_T_K + i * rng.uniform(10, 40),
            P_Pa=cfg.organic_P_atm * ATM,
            velocity_m_s=round(rng.uniform(2.0, 12.0), 2),
            n_elbows=rng.randint(1, 3),
            n_valves=rng.randint(1, 2),
            material=MaterialContext(structural_alloy=cfg.organic_alloy),
            insulation=rng.choice(INSULATION_CHOICES),
            insulation_thickness_m=round(rng.uniform(0.02, 0.06), 3),
        ))

    return net

# =====================================================================
# 6. Run a single plant
# =====================================================================

def run_plant(cfg: PlantConfig):
    plant_dir = OUT / cfg.plant_id
    plant_dir.mkdir(parents=True, exist_ok=True)
    print(cfg.summary())

    # pipe network
    net = build_network(cfg)
    analyzer = NetworkAnalyzer(net)
    analyzed = analyzer.analyze()
    analyzed.print_table(f"{cfg.plant_id} Pipe Network")

    seg_dicts = analyzed.as_dicts()
    if seg_dicts:
        with open(plant_dir / "pipe_network.csv", "w", newline="", encoding="utf-8") as f:
            w = csv.DictWriter(f, fieldnames=seg_dicts[0].keys())
            w.writeheader()
            w.writerows(seg_dicts)

    # PVT grids for every fluid in the plant (incl. organic and noble)
    fluids = sorted(set([
        cfg.fuel_salt, cfg.secondary_fluid, cfg.tertiary_fluid,
        cfg.organic_fluid, cfg.noble_gas,
    ]))
    for formula in fluids:
        print(f"  PVT grid: {formula} ...", end=" ")
        grid = generate_pvt_grid(formula, n_T=50, n_P=50)
        if grid:
            with open(plant_dir / f"pvt_grid_{formula}.csv", "w", newline="", encoding="utf-8") as f:
                w = csv.DictWriter(f, fieldnames=grid[0].keys())
                w.writeheader()
                w.writerows(grid)
            print(f"{len(grid)} points")
        else:
            print("(no material data)")

        sat = generate_saturation(formula, n_pts=100)
        if sat:
            with open(plant_dir / f"saturation_{formula}.csv", "w", newline="", encoding="utf-8") as f:
                w = csv.DictWriter(f, fieldnames=sat[0].keys())
                w.writeheader()
                w.writerows(sat)
            print(f"  Saturation: {formula} -- {len(sat)} points")

    # per-pillar specialized channel CSVs
    print(f"\n  -- Pillar channels --")
    channel_files = generate_pillar_channels(cfg, seg_dicts, plant_dir)

    # pillar metadata
    pillar_meta = []
    for tag in cfg.pillar_tags:
        p = PILLARS[tag]
        pillar_meta.append({
            "pillar": p.name, "colour": p.colour_hex,
            "traits": p.traits,
            "description": p.description,
            "channel_file": channel_files.get(tag, None),
        })

    # summary JSON
    summary = {
        "plant_id": cfg.plant_id, "seed": cfg.seed,
        "primary": {"fuel_salt": cfg.fuel_salt, "alloy": cfg.primary_alloy,
                     "D_m": cfg.primary_diameter_m, "T_K": cfg.primary_T_K,
                     "P_atm": cfg.primary_P_atm},
        "secondary": {"fluid": cfg.secondary_fluid, "alloy": cfg.secondary_alloy,
                       "D_m": cfg.secondary_diameter_m, "T_K": cfg.secondary_T_K,
                       "P_atm": cfg.secondary_P_atm},
        "tertiary": {"fluid": cfg.tertiary_fluid, "alloy": cfg.tertiary_alloy,
                      "D_m": cfg.tertiary_diameter_m},
        "organic": {"fluid": cfg.organic_fluid, "alloy": cfg.organic_alloy,
                     "D_m": cfg.organic_diameter_m, "T_K": cfg.organic_T_K,
                     "P_atm": cfg.organic_P_atm},
        "noble": {"gas": cfg.noble_gas, "cover_P_atm": cfg.noble_cover_P_atm},
        "network": {"n_segments": len(seg_dicts),
                     "total_length_m": analyzed.total_length_m,
                     "total_dP_Pa": analyzed.total_dP_Pa,
                     "total_heat_loss_W": analyzed.total_heat_loss_W},
        "pillar_tags": cfg.pillar_tags,
        "pillars": pillar_meta,
        "fluids_computed": fluids,
        "channel_files": channel_files,
    }
    with open(plant_dir / "plant_summary.json", "w", encoding="utf-8") as f:
        json.dump(summary, f, indent=2, default=str)

    return summary

# =====================================================================
# 7. Main
# =====================================================================

def main(n_plants: int = 5, base_seed: int = None):
    if base_seed is None:
        base_seed = int(time.time()) % 1_000_000
    print("=" * 80)
    print("VSEPR-SIM  Colour Pillar Plant Generator")
    print("=" * 80)
    print()
    print("COLOUR PILLARS (10-element system):")
    for tag, p in PILLARS.items():
        print(f"  {p.colour_hex}  {tag:10s}  {p.traits}")
    print()

    all_summaries = []
    for i in range(n_plants):
        seed = base_seed + i * 137
        print(f"\n{'~' * 80}")
        cfg = randomise_plant(seed)
        s = run_plant(cfg)
        all_summaries.append(s)

    # master index
    index_path = OUT / "colour_pillar_index.json"
    with open(index_path, "w", encoding="utf-8") as f:
        json.dump({
            "generator": "colour_pillar_plant_gen.py",
            "base_seed": base_seed,
            "n_plants": n_plants,
            "pillars": {k: {"colour": v.colour_hex, "traits": v.traits,
                            "description": v.description}
                         for k, v in PILLARS.items()},
            "plants": all_summaries,
        }, f, indent=2, default=str)

    print(f"\n{'=' * 80}")
    print(f"Done. {n_plants} plants generated.")
    print(f"Outputs: {OUT}")
    print(f"Index:   {index_path}")
    print(f"{'=' * 80}")


if __name__ == "__main__":
    main()
