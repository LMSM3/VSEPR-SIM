"""
Crystal Discovery — Structure Generation and Catalog
======================================================

Pillar B of the VSEPR-SIM Five Pillars architecture.

Subsystems:
  - Crystal Catalog: stores formula, space-group, density, coordination, stability
  - Discovery Engine: lattice prototypes, supercell variants, defect insertion
  - Organic mode: molecular packing, H-bond motifs, aromatic stacking
  - Inorganic mode: coordination shells, ionic packing, lattice strain

Anti-black-box: every generated structure has a provenance hash and
deterministic seed.  Same seed → identical crystal.

VSEPR-SIM Five Pillars v1.0
"""

from __future__ import annotations

import math
import os
import json
import csv
import hashlib
import random
from dataclasses import dataclass, field, asdict
from typing import Dict, List, Tuple, Optional
from enum import Enum, auto


# ============================================================================
# Crystal system classification
# ============================================================================

class CrystalSystem(Enum):
    CUBIC = "cubic"
    TETRAGONAL = "tetragonal"
    ORTHORHOMBIC = "orthorhombic"
    HEXAGONAL = "hexagonal"
    TRIGONAL = "trigonal"
    MONOCLINIC = "monoclinic"
    TRICLINIC = "triclinic"


class BravaisLattice(Enum):
    SC = "simple_cubic"
    BCC = "body_centered_cubic"
    FCC = "face_centered_cubic"
    HCP = "hexagonal_close_packed"
    DIAMOND = "diamond_cubic"
    ROCKSALT = "NaCl_rocksalt"
    ZINCBLENDE = "ZnS_zincblende"
    FLUORITE = "CaF2_fluorite"
    PEROVSKITE = "ABX3_perovskite"
    WURTZITE = "wurtzite"
    RUTILE = "TiO2_rutile"
    CORUNDUM = "Al2O3_corundum"
    SPINEL = "AB2O4_spinel"
    GARNET = "A3B2C3O12_garnet"
    LAYERED = "layered_vdW"
    MOLECULAR = "molecular_crystal"


class CrystalClass(Enum):
    IONIC = "ionic"
    METALLIC = "metallic"
    COVALENT = "covalent_network"
    MOLECULAR_ORGANIC = "molecular_organic"
    MOLECULAR_INORGANIC = "molecular_inorganic"
    FRAMEWORK = "framework"
    MIXED = "mixed_bonding"


class StabilityClass(Enum):
    GROUND_STATE = "ground_state"
    METASTABLE = "metastable"
    UNSTABLE = "unstable"
    HYPOTHETICAL = "hypothetical"


# ============================================================================
# Core data structures
# ============================================================================

@dataclass
class LatticeParams:
    a: float           # Angstrom
    b: float
    c: float
    alpha: float = 90.0  # degrees
    beta: float = 90.0
    gamma: float = 90.0

    @property
    def volume(self) -> float:
        """Unit cell volume in Å³."""
        ar = math.radians(self.alpha)
        br = math.radians(self.beta)
        gr = math.radians(self.gamma)
        ca, cb, cg = math.cos(ar), math.cos(br), math.cos(gr)
        v2 = 1 - ca**2 - cb**2 - cg**2 + 2*ca*cb*cg
        return self.a * self.b * self.c * math.sqrt(max(v2, 0.0))


@dataclass
class AtomSite:
    symbol: str
    Z: int
    x: float  # fractional
    y: float
    z: float
    occupancy: float = 1.0
    wyckoff: str = ""
    oxidation_state: int = 0


@dataclass
class CrystalEntry:
    """Complete crystal catalog entry."""
    entry_id: str
    formula: str
    name: str
    crystal_system: CrystalSystem
    bravais: BravaisLattice
    crystal_class: CrystalClass
    lattice: LatticeParams
    atoms: List[AtomSite]
    space_group_number: int = 1
    space_group_symbol: str = "P1"
    density: float = 0.0          # g/cm³
    Z_formula_units: int = 1
    coordination: Dict[str, int] = field(default_factory=dict)
    bond_network: str = ""
    polymorph_tag: str = "alpha"
    stability: StabilityClass = StabilityClass.GROUND_STATE
    stability_score: float = 0.0  # 0-100
    formation_energy: float = 0.0 # eV/atom
    bandgap: float = -1.0         # eV (-1 = metallic or unknown)
    tags: List[str] = field(default_factory=list)
    provenance: str = ""
    seed: int = 0

    @property
    def n_atoms(self) -> int:
        return len(self.atoms)

    @property
    def provenance_hash(self) -> str:
        raw = f"{self.formula}|{self.seed}|{self.bravais.value}|{self.lattice.a}"
        return hashlib.sha256(raw.encode()).hexdigest()[:12]


@dataclass
class DefectSpec:
    defect_type: str   # vacancy, interstitial, substitution, antisite, Frenkel
    site_index: int
    replacement_symbol: str = ""
    displacement: Tuple[float, float, float] = (0.0, 0.0, 0.0)


# ============================================================================
# Element data for crystal generation
# ============================================================================

_ELEM_DATA: Dict[str, dict] = {
    "Li": {"Z": 3,  "r_ionic": 0.76, "r_cov": 1.28, "r_met": 1.52, "mass": 6.941,  "eneg": 0.98, "ox": [1]},
    "Be": {"Z": 4,  "r_ionic": 0.45, "r_cov": 0.96, "r_met": 1.12, "mass": 9.012,  "eneg": 1.57, "ox": [2]},
    "B":  {"Z": 5,  "r_ionic": 0.27, "r_cov": 0.84, "r_met": 0.00, "mass": 10.81,  "eneg": 2.04, "ox": [3]},
    "C":  {"Z": 6,  "r_ionic": 0.16, "r_cov": 0.76, "r_met": 0.00, "mass": 12.011, "eneg": 2.55, "ox": [4, -4]},
    "N":  {"Z": 7,  "r_ionic": 1.46, "r_cov": 0.71, "r_met": 0.00, "mass": 14.007, "eneg": 3.04, "ox": [-3, 5]},
    "O":  {"Z": 8,  "r_ionic": 1.40, "r_cov": 0.66, "r_met": 0.00, "mass": 15.999, "eneg": 3.44, "ox": [-2]},
    "F":  {"Z": 9,  "r_ionic": 1.33, "r_cov": 0.57, "r_met": 0.00, "mass": 18.998, "eneg": 3.98, "ox": [-1]},
    "Na": {"Z": 11, "r_ionic": 1.02, "r_cov": 1.66, "r_met": 1.86, "mass": 22.990, "eneg": 0.93, "ox": [1]},
    "Mg": {"Z": 12, "r_ionic": 0.72, "r_cov": 1.41, "r_met": 1.60, "mass": 24.305, "eneg": 1.31, "ox": [2]},
    "Al": {"Z": 13, "r_ionic": 0.54, "r_cov": 1.21, "r_met": 1.43, "mass": 26.982, "eneg": 1.61, "ox": [3]},
    "Si": {"Z": 14, "r_ionic": 0.40, "r_cov": 1.11, "r_met": 0.00, "mass": 28.086, "eneg": 1.90, "ox": [4, -4]},
    "P":  {"Z": 15, "r_ionic": 0.44, "r_cov": 1.07, "r_met": 0.00, "mass": 30.974, "eneg": 2.19, "ox": [-3, 5]},
    "S":  {"Z": 16, "r_ionic": 1.84, "r_cov": 1.05, "r_met": 0.00, "mass": 32.065, "eneg": 2.58, "ox": [-2, 6]},
    "Cl": {"Z": 17, "r_ionic": 1.81, "r_cov": 1.02, "r_met": 0.00, "mass": 35.453, "eneg": 3.16, "ox": [-1]},
    "K":  {"Z": 19, "r_ionic": 1.38, "r_cov": 2.03, "r_met": 2.27, "mass": 39.098, "eneg": 0.82, "ox": [1]},
    "Ca": {"Z": 20, "r_ionic": 1.00, "r_cov": 1.76, "r_met": 1.97, "mass": 40.078, "eneg": 1.00, "ox": [2]},
    "Ti": {"Z": 22, "r_ionic": 0.61, "r_cov": 1.60, "r_met": 1.47, "mass": 47.867, "eneg": 1.54, "ox": [4, 3, 2]},
    "V":  {"Z": 23, "r_ionic": 0.54, "r_cov": 1.53, "r_met": 1.34, "mass": 50.942, "eneg": 1.63, "ox": [5, 4, 3, 2]},
    "Cr": {"Z": 24, "r_ionic": 0.52, "r_cov": 1.39, "r_met": 1.28, "mass": 51.996, "eneg": 1.66, "ox": [3, 6, 2]},
    "Mn": {"Z": 25, "r_ionic": 0.53, "r_cov": 1.39, "r_met": 1.27, "mass": 54.938, "eneg": 1.55, "ox": [2, 4, 7]},
    "Fe": {"Z": 26, "r_ionic": 0.55, "r_cov": 1.32, "r_met": 1.26, "mass": 55.845, "eneg": 1.83, "ox": [2, 3]},
    "Co": {"Z": 27, "r_ionic": 0.55, "r_cov": 1.26, "r_met": 1.25, "mass": 58.933, "eneg": 1.88, "ox": [2, 3]},
    "Ni": {"Z": 28, "r_ionic": 0.55, "r_cov": 1.24, "r_met": 1.25, "mass": 58.693, "eneg": 1.91, "ox": [2]},
    "Cu": {"Z": 29, "r_ionic": 0.57, "r_cov": 1.32, "r_met": 1.28, "mass": 63.546, "eneg": 1.90, "ox": [2, 1]},
    "Zn": {"Z": 30, "r_ionic": 0.60, "r_cov": 1.22, "r_met": 1.34, "mass": 65.38,  "eneg": 1.65, "ox": [2]},
    "Ga": {"Z": 31, "r_ionic": 0.47, "r_cov": 1.22, "r_met": 1.35, "mass": 69.723, "eneg": 1.81, "ox": [3]},
    "Ge": {"Z": 32, "r_ionic": 0.39, "r_cov": 1.20, "r_met": 0.00, "mass": 72.64,  "eneg": 2.01, "ox": [4]},
    "As": {"Z": 33, "r_ionic": 0.46, "r_cov": 1.19, "r_met": 0.00, "mass": 74.922, "eneg": 2.18, "ox": [-3, 5]},
    "Se": {"Z": 34, "r_ionic": 1.98, "r_cov": 1.20, "r_met": 0.00, "mass": 78.96,  "eneg": 2.55, "ox": [-2, 6]},
    "Br": {"Z": 35, "r_ionic": 1.96, "r_cov": 1.20, "r_met": 0.00, "mass": 79.904, "eneg": 2.96, "ox": [-1]},
    "Sr": {"Z": 38, "r_ionic": 1.18, "r_cov": 1.95, "r_met": 2.15, "mass": 87.62,  "eneg": 0.95, "ox": [2]},
    "Zr": {"Z": 40, "r_ionic": 0.72, "r_cov": 1.75, "r_met": 1.60, "mass": 91.224, "eneg": 1.33, "ox": [4]},
    "Nb": {"Z": 41, "r_ionic": 0.64, "r_cov": 1.64, "r_met": 1.46, "mass": 92.906, "eneg": 1.60, "ox": [5, 3]},
    "Mo": {"Z": 42, "r_ionic": 0.59, "r_cov": 1.54, "r_met": 1.39, "mass": 95.96,  "eneg": 2.16, "ox": [6, 4]},
    "Ag": {"Z": 47, "r_ionic": 1.15, "r_cov": 1.45, "r_met": 1.44, "mass": 107.87, "eneg": 1.93, "ox": [1]},
    "Sn": {"Z": 50, "r_ionic": 0.69, "r_cov": 1.39, "r_met": 1.51, "mass": 118.71, "eneg": 1.96, "ox": [4, 2]},
    "Ba": {"Z": 56, "r_ionic": 1.35, "r_cov": 2.15, "r_met": 2.22, "mass": 137.33, "eneg": 0.89, "ox": [2]},
    "W":  {"Z": 74, "r_ionic": 0.60, "r_cov": 1.62, "r_met": 1.39, "mass": 183.84, "eneg": 2.36, "ox": [6, 4]},
    "Pt": {"Z": 78, "r_ionic": 0.63, "r_cov": 1.36, "r_met": 1.39, "mass": 195.08, "eneg": 2.28, "ox": [4, 2]},
    "Au": {"Z": 79, "r_ionic": 1.37, "r_cov": 1.36, "r_met": 1.44, "mass": 196.97, "eneg": 2.54, "ox": [3, 1]},
    "Pb": {"Z": 82, "r_ionic": 1.19, "r_cov": 1.46, "r_met": 1.75, "mass": 207.2,  "eneg": 2.33, "ox": [2, 4]},
}


# ============================================================================
# Lattice prototype generators
# ============================================================================

def _generate_sc(sym: str, a: float) -> List[AtomSite]:
    Z = _ELEM_DATA.get(sym, {}).get("Z", 0)
    return [AtomSite(sym, Z, 0.0, 0.0, 0.0)]

def _generate_bcc(sym: str, a: float) -> List[AtomSite]:
    Z = _ELEM_DATA.get(sym, {}).get("Z", 0)
    return [AtomSite(sym, Z, 0.0, 0.0, 0.0),
            AtomSite(sym, Z, 0.5, 0.5, 0.5)]

def _generate_fcc(sym: str, a: float) -> List[AtomSite]:
    Z = _ELEM_DATA.get(sym, {}).get("Z", 0)
    return [AtomSite(sym, Z, 0.0, 0.0, 0.0),
            AtomSite(sym, Z, 0.5, 0.5, 0.0),
            AtomSite(sym, Z, 0.5, 0.0, 0.5),
            AtomSite(sym, Z, 0.0, 0.5, 0.5)]

def _generate_hcp(sym: str, a: float, c: float) -> List[AtomSite]:
    Z = _ELEM_DATA.get(sym, {}).get("Z", 0)
    return [AtomSite(sym, Z, 0.0, 0.0, 0.0),
            AtomSite(sym, Z, 1/3, 2/3, 0.5)]

def _generate_diamond(sym: str, a: float) -> List[AtomSite]:
    Z = _ELEM_DATA.get(sym, {}).get("Z", 0)
    sites = []
    for fx, fy, fz in [(0,0,0),(0.5,0.5,0),(0.5,0,0.5),(0,0.5,0.5),
                         (0.25,0.25,0.25),(0.75,0.75,0.25),
                         (0.75,0.25,0.75),(0.25,0.75,0.75)]:
        sites.append(AtomSite(sym, Z, fx, fy, fz))
    return sites

def _generate_rocksalt(sym_A: str, sym_B: str, a: float) -> List[AtomSite]:
    Z_A = _ELEM_DATA.get(sym_A, {}).get("Z", 0)
    Z_B = _ELEM_DATA.get(sym_B, {}).get("Z", 0)
    sites = []
    for fx, fy, fz in [(0,0,0),(0.5,0.5,0),(0.5,0,0.5),(0,0.5,0.5)]:
        sites.append(AtomSite(sym_A, Z_A, fx, fy, fz, oxidation_state=1))
    for fx, fy, fz in [(0.5,0.5,0.5),(0,0,0.5),(0,0.5,0),(0.5,0,0)]:
        sites.append(AtomSite(sym_B, Z_B, fx, fy, fz, oxidation_state=-1))
    return sites

def _generate_perovskite(sym_A: str, sym_B: str, sym_X: str, a: float) -> List[AtomSite]:
    Z_A = _ELEM_DATA.get(sym_A, {}).get("Z", 0)
    Z_B = _ELEM_DATA.get(sym_B, {}).get("Z", 0)
    Z_X = _ELEM_DATA.get(sym_X, {}).get("Z", 0)
    sites = [
        AtomSite(sym_A, Z_A, 0.0, 0.0, 0.0),
        AtomSite(sym_B, Z_B, 0.5, 0.5, 0.5),
        AtomSite(sym_X, Z_X, 0.5, 0.5, 0.0),
        AtomSite(sym_X, Z_X, 0.5, 0.0, 0.5),
        AtomSite(sym_X, Z_X, 0.0, 0.5, 0.5),
    ]
    return sites

def _generate_fluorite(sym_A: str, sym_B: str, a: float) -> List[AtomSite]:
    Z_A = _ELEM_DATA.get(sym_A, {}).get("Z", 0)
    Z_B = _ELEM_DATA.get(sym_B, {}).get("Z", 0)
    sites = []
    for fx, fy, fz in [(0,0,0),(0.5,0.5,0),(0.5,0,0.5),(0,0.5,0.5)]:
        sites.append(AtomSite(sym_A, Z_A, fx, fy, fz))
    for fx, fy, fz in [(0.25,0.25,0.25),(0.75,0.75,0.25),
                         (0.75,0.25,0.75),(0.25,0.75,0.75),
                         (0.25,0.25,0.75),(0.75,0.75,0.75),
                         (0.75,0.25,0.25),(0.25,0.75,0.25)]:
        sites.append(AtomSite(sym_B, Z_B, fx, fy, fz))
    return sites


# ============================================================================
# Prototype crystal database
# ============================================================================

_CRYSTAL_PROTOTYPES: List[dict] = [
    # Metallic FCC
    {"formula": "Al",   "name": "Aluminium",          "bravais": BravaisLattice.FCC,       "system": CrystalSystem.CUBIC,       "cls": CrystalClass.METALLIC,     "a": 4.050, "sg_num": 225, "sg": "Fm-3m",    "density": 2.70,  "Ef": 0.0,    "Eg": -1, "coord": {"Al": 12}},
    {"formula": "Cu",   "name": "Copper",             "bravais": BravaisLattice.FCC,       "system": CrystalSystem.CUBIC,       "cls": CrystalClass.METALLIC,     "a": 3.615, "sg_num": 225, "sg": "Fm-3m",    "density": 8.96,  "Ef": 0.0,    "Eg": -1, "coord": {"Cu": 12}},
    {"formula": "Au",   "name": "Gold",               "bravais": BravaisLattice.FCC,       "system": CrystalSystem.CUBIC,       "cls": CrystalClass.METALLIC,     "a": 4.078, "sg_num": 225, "sg": "Fm-3m",    "density": 19.32, "Ef": 0.0,    "Eg": -1, "coord": {"Au": 12}},
    {"formula": "Ag",   "name": "Silver",             "bravais": BravaisLattice.FCC,       "system": CrystalSystem.CUBIC,       "cls": CrystalClass.METALLIC,     "a": 4.086, "sg_num": 225, "sg": "Fm-3m",    "density": 10.49, "Ef": 0.0,    "Eg": -1, "coord": {"Ag": 12}},
    {"formula": "Ni",   "name": "Nickel",             "bravais": BravaisLattice.FCC,       "system": CrystalSystem.CUBIC,       "cls": CrystalClass.METALLIC,     "a": 3.524, "sg_num": 225, "sg": "Fm-3m",    "density": 8.91,  "Ef": 0.0,    "Eg": -1, "coord": {"Ni": 12}},
    {"formula": "Pt",   "name": "Platinum",           "bravais": BravaisLattice.FCC,       "system": CrystalSystem.CUBIC,       "cls": CrystalClass.METALLIC,     "a": 3.924, "sg_num": 225, "sg": "Fm-3m",    "density": 21.45, "Ef": 0.0,    "Eg": -1, "coord": {"Pt": 12}},
    {"formula": "Pb",   "name": "Lead",               "bravais": BravaisLattice.FCC,       "system": CrystalSystem.CUBIC,       "cls": CrystalClass.METALLIC,     "a": 4.950, "sg_num": 225, "sg": "Fm-3m",    "density": 11.34, "Ef": 0.0,    "Eg": -1, "coord": {"Pb": 12}},
    # Metallic BCC
    {"formula": "Fe",   "name": "Iron (alpha)",       "bravais": BravaisLattice.BCC,       "system": CrystalSystem.CUBIC,       "cls": CrystalClass.METALLIC,     "a": 2.867, "sg_num": 229, "sg": "Im-3m",    "density": 7.87,  "Ef": 0.0,    "Eg": -1, "coord": {"Fe": 8}},
    {"formula": "W",    "name": "Tungsten",           "bravais": BravaisLattice.BCC,       "system": CrystalSystem.CUBIC,       "cls": CrystalClass.METALLIC,     "a": 3.165, "sg_num": 229, "sg": "Im-3m",    "density": 19.25, "Ef": 0.0,    "Eg": -1, "coord": {"W": 8}},
    {"formula": "Cr",   "name": "Chromium",           "bravais": BravaisLattice.BCC,       "system": CrystalSystem.CUBIC,       "cls": CrystalClass.METALLIC,     "a": 2.884, "sg_num": 229, "sg": "Im-3m",    "density": 7.19,  "Ef": 0.0,    "Eg": -1, "coord": {"Cr": 8}},
    {"formula": "Mo",   "name": "Molybdenum",         "bravais": BravaisLattice.BCC,       "system": CrystalSystem.CUBIC,       "cls": CrystalClass.METALLIC,     "a": 3.147, "sg_num": 229, "sg": "Im-3m",    "density": 10.28, "Ef": 0.0,    "Eg": -1, "coord": {"Mo": 8}},
    {"formula": "V",    "name": "Vanadium",           "bravais": BravaisLattice.BCC,       "system": CrystalSystem.CUBIC,       "cls": CrystalClass.METALLIC,     "a": 3.024, "sg_num": 229, "sg": "Im-3m",    "density": 6.11,  "Ef": 0.0,    "Eg": -1, "coord": {"V": 8}},
    {"formula": "Nb",   "name": "Niobium",            "bravais": BravaisLattice.BCC,       "system": CrystalSystem.CUBIC,       "cls": CrystalClass.METALLIC,     "a": 3.300, "sg_num": 229, "sg": "Im-3m",    "density": 8.57,  "Ef": 0.0,    "Eg": -1, "coord": {"Nb": 8}},
    # Metallic HCP
    {"formula": "Ti",   "name": "Titanium (alpha)",   "bravais": BravaisLattice.HCP,       "system": CrystalSystem.HEXAGONAL,   "cls": CrystalClass.METALLIC,     "a": 2.951, "c": 4.684, "sg_num": 194, "sg": "P6_3/mmc", "density": 4.51, "Ef": 0.0, "Eg": -1, "coord": {"Ti": 12}},
    {"formula": "Zn",   "name": "Zinc",               "bravais": BravaisLattice.HCP,       "system": CrystalSystem.HEXAGONAL,   "cls": CrystalClass.METALLIC,     "a": 2.665, "c": 4.947, "sg_num": 194, "sg": "P6_3/mmc", "density": 7.13, "Ef": 0.0, "Eg": -1, "coord": {"Zn": 12}},
    {"formula": "Mg",   "name": "Magnesium",          "bravais": BravaisLattice.HCP,       "system": CrystalSystem.HEXAGONAL,   "cls": CrystalClass.METALLIC,     "a": 3.209, "c": 5.211, "sg_num": 194, "sg": "P6_3/mmc", "density": 1.74, "Ef": 0.0, "Eg": -1, "coord": {"Mg": 12}},
    {"formula": "Co",   "name": "Cobalt (alpha)",     "bravais": BravaisLattice.HCP,       "system": CrystalSystem.HEXAGONAL,   "cls": CrystalClass.METALLIC,     "a": 2.507, "c": 4.069, "sg_num": 194, "sg": "P6_3/mmc", "density": 8.90, "Ef": 0.0, "Eg": -1, "coord": {"Co": 12}},
    {"formula": "Zr",   "name": "Zirconium (alpha)",  "bravais": BravaisLattice.HCP,       "system": CrystalSystem.HEXAGONAL,   "cls": CrystalClass.METALLIC,     "a": 3.232, "c": 5.147, "sg_num": 194, "sg": "P6_3/mmc", "density": 6.51, "Ef": 0.0, "Eg": -1, "coord": {"Zr": 12}},
    # Covalent
    {"formula": "C",    "name": "Diamond",            "bravais": BravaisLattice.DIAMOND,   "system": CrystalSystem.CUBIC,       "cls": CrystalClass.COVALENT,     "a": 3.567, "sg_num": 227, "sg": "Fd-3m",    "density": 3.51,  "Ef": 0.0,    "Eg": 5.47, "coord": {"C": 4}},
    {"formula": "Si",   "name": "Silicon",            "bravais": BravaisLattice.DIAMOND,   "system": CrystalSystem.CUBIC,       "cls": CrystalClass.COVALENT,     "a": 5.431, "sg_num": 227, "sg": "Fd-3m",    "density": 2.33,  "Ef": 0.0,    "Eg": 1.12, "coord": {"Si": 4}},
    {"formula": "Ge",   "name": "Germanium",          "bravais": BravaisLattice.DIAMOND,   "system": CrystalSystem.CUBIC,       "cls": CrystalClass.COVALENT,     "a": 5.658, "sg_num": 227, "sg": "Fd-3m",    "density": 5.32,  "Ef": 0.0,    "Eg": 0.66, "coord": {"Ge": 4}},
    {"formula": "SiC",  "name": "Silicon Carbide 3C", "bravais": BravaisLattice.ZINCBLENDE,"system": CrystalSystem.CUBIC,       "cls": CrystalClass.COVALENT,     "a": 4.360, "sg_num": 216, "sg": "F-43m",    "density": 3.21,  "Ef": -0.72,  "Eg": 2.36, "coord": {"Si": 4, "C": 4}},
    {"formula": "GaAs", "name": "Gallium Arsenide",   "bravais": BravaisLattice.ZINCBLENDE,"system": CrystalSystem.CUBIC,       "cls": CrystalClass.COVALENT,     "a": 5.653, "sg_num": 216, "sg": "F-43m",    "density": 5.32,  "Ef": -0.74,  "Eg": 1.42, "coord": {"Ga": 4, "As": 4}},
    # Ionic
    {"formula": "NaCl", "name": "Sodium Chloride",    "bravais": BravaisLattice.ROCKSALT,  "system": CrystalSystem.CUBIC,       "cls": CrystalClass.IONIC,        "a": 5.640, "sg_num": 225, "sg": "Fm-3m",    "density": 2.16,  "Ef": -4.26,  "Eg": 8.5,  "coord": {"Na": 6, "Cl": 6}},
    {"formula": "KCl",  "name": "Potassium Chloride", "bravais": BravaisLattice.ROCKSALT,  "system": CrystalSystem.CUBIC,       "cls": CrystalClass.IONIC,        "a": 6.293, "sg_num": 225, "sg": "Fm-3m",    "density": 1.98,  "Ef": -4.43,  "Eg": 8.4,  "coord": {"K": 6, "Cl": 6}},
    {"formula": "MgO",  "name": "Magnesium Oxide",    "bravais": BravaisLattice.ROCKSALT,  "system": CrystalSystem.CUBIC,       "cls": CrystalClass.IONIC,        "a": 4.212, "sg_num": 225, "sg": "Fm-3m",    "density": 3.58,  "Ef": -6.13,  "Eg": 7.8,  "coord": {"Mg": 6, "O": 6}},
    {"formula": "CaO",  "name": "Calcium Oxide",      "bravais": BravaisLattice.ROCKSALT,  "system": CrystalSystem.CUBIC,       "cls": CrystalClass.IONIC,        "a": 4.811, "sg_num": 225, "sg": "Fm-3m",    "density": 3.35,  "Ef": -6.35,  "Eg": 7.1,  "coord": {"Ca": 6, "O": 6}},
    {"formula": "CaF2", "name": "Calcium Fluoride",   "bravais": BravaisLattice.FLUORITE,  "system": CrystalSystem.CUBIC,       "cls": CrystalClass.IONIC,        "a": 5.463, "sg_num": 225, "sg": "Fm-3m",    "density": 3.18,  "Ef": -12.68, "Eg": 12.1, "coord": {"Ca": 8, "F": 4}},
    {"formula": "TiO2", "name": "Rutile",             "bravais": BravaisLattice.RUTILE,    "system": CrystalSystem.TETRAGONAL,  "cls": CrystalClass.IONIC,        "a": 4.594, "c": 2.959, "sg_num": 136, "sg": "P4_2/mnm", "density": 4.23, "Ef": -9.73, "Eg": 3.0, "coord": {"Ti": 6, "O": 3}},
    {"formula": "Al2O3","name": "Corundum",           "bravais": BravaisLattice.CORUNDUM,  "system": CrystalSystem.TRIGONAL,    "cls": CrystalClass.IONIC,        "a": 4.760, "c": 12.991,"sg_num": 167, "sg": "R-3c",     "density": 3.99, "Ef": -17.4, "Eg": 9.9, "coord": {"Al": 6, "O": 4}},
    # Perovskites
    {"formula": "SrTiO3","name": "Strontium Titanate","bravais": BravaisLattice.PEROVSKITE,"system": CrystalSystem.CUBIC,       "cls": CrystalClass.IONIC,        "a": 3.905, "sg_num": 221, "sg": "Pm-3m",    "density": 5.12,  "Ef": -16.5,  "Eg": 3.2,  "coord": {"Sr": 12, "Ti": 6, "O": 2}},
    {"formula": "BaTiO3","name": "Barium Titanate",   "bravais": BravaisLattice.PEROVSKITE,"system": CrystalSystem.TETRAGONAL,  "cls": CrystalClass.IONIC,        "a": 3.994, "c": 4.034, "sg_num": 99,  "sg": "P4mm",     "density": 6.02, "Ef": -16.8, "Eg": 3.2, "coord": {"Ba": 12, "Ti": 6, "O": 2}},
    # Molecular organic
    {"formula": "C6H12O6","name": "alpha-D-Glucose",  "bravais": BravaisLattice.MOLECULAR, "system": CrystalSystem.ORTHORHOMBIC,"cls": CrystalClass.MOLECULAR_ORGANIC, "a": 10.37, "b": 14.85, "c": 4.97, "sg_num": 19, "sg": "P2_12_12_1", "density": 1.56, "Ef": -0.1, "Eg": 5.0, "coord": {}},
    {"formula": "C14H10","name": "Anthracene",        "bravais": BravaisLattice.MOLECULAR, "system": CrystalSystem.MONOCLINIC,  "cls": CrystalClass.MOLECULAR_ORGANIC, "a": 8.562, "b": 6.038, "c": 11.184, "beta": 124.7, "sg_num": 14, "sg": "P2_1/c", "density": 1.28, "Ef": 0.0, "Eg": 3.3, "coord": {}},
    {"formula": "C10H8", "name": "Naphthalene",       "bravais": BravaisLattice.MOLECULAR, "system": CrystalSystem.MONOCLINIC,  "cls": CrystalClass.MOLECULAR_ORGANIC, "a": 8.235, "b": 5.966, "c": 8.658, "beta": 122.9, "sg_num": 14, "sg": "P2_1/c", "density": 1.16, "Ef": 0.0, "Eg": 3.9, "coord": {}},
    {"formula": "C6H5COOH","name":"Benzoic Acid",     "bravais": BravaisLattice.MOLECULAR, "system": CrystalSystem.MONOCLINIC,  "cls": CrystalClass.MOLECULAR_ORGANIC, "a": 5.510, "b": 5.155, "c": 21.95, "beta": 97.4, "sg_num": 14, "sg": "P2_1/c", "density": 1.27, "Ef": 0.0, "Eg": 4.0, "coord": {}},
    {"formula": "CO(NH2)2","name":"Urea",             "bravais": BravaisLattice.MOLECULAR, "system": CrystalSystem.TETRAGONAL,  "cls": CrystalClass.MOLECULAR_ORGANIC, "a": 5.661, "c": 4.712, "sg_num": 113, "sg": "P-42_1m", "density": 1.32, "Ef": -0.3, "Eg": 5.0, "coord": {}},
    {"formula": "C6H12N4","name":"Hexamethylenetetramine","bravais": BravaisLattice.MOLECULAR,"system": CrystalSystem.CUBIC,    "cls": CrystalClass.MOLECULAR_ORGANIC, "a": 7.021, "sg_num": 217, "sg": "I-43m",   "density": 1.33, "Ef": 0.0, "Eg": 5.5, "coord": {}},
]


# ============================================================================
# Crystal catalog
# ============================================================================

class CrystalCatalog:
    """In-memory crystal catalog with generation, search, and export."""

    def __init__(self):
        self.entries: List[CrystalEntry] = []
        self._build_from_prototypes()

    def _build_from_prototypes(self):
        for p in _CRYSTAL_PROTOTYPES:
            a = p["a"]
            b = p.get("b", a)
            c = p.get("c", a)
            alpha = p.get("alpha", 90.0)
            beta = p.get("beta", 90.0)
            gamma = 120.0 if p["system"] == CrystalSystem.HEXAGONAL else p.get("gamma", 90.0)

            lattice = LatticeParams(a, b, c, alpha, beta, gamma)
            bravais = p["bravais"]
            formula = p["formula"]
            syms = [s for s in formula if s.isupper()]  # rough

            # Generate atom sites based on lattice type
            if bravais == BravaisLattice.FCC:
                atoms = _generate_fcc(formula, a)
            elif bravais == BravaisLattice.BCC:
                atoms = _generate_bcc(formula, a)
            elif bravais == BravaisLattice.HCP:
                atoms = _generate_hcp(formula, a, c)
            elif bravais == BravaisLattice.DIAMOND:
                atoms = _generate_diamond(formula.replace("Si", "Si").split()[0] if " " not in formula else formula, a)
            elif bravais == BravaisLattice.ROCKSALT:
                parts = _split_binary(formula)
                atoms = _generate_rocksalt(parts[0], parts[1], a) if len(parts) == 2 else []
            elif bravais == BravaisLattice.PEROVSKITE:
                parts = _split_perovskite(formula)
                atoms = _generate_perovskite(*parts, a) if len(parts) == 3 else []
            elif bravais == BravaisLattice.FLUORITE:
                parts = _split_binary(formula)
                atoms = _generate_fluorite(parts[0], parts[1], a) if len(parts) == 2 else []
            else:
                atoms = [AtomSite(formula, 0, 0.0, 0.0, 0.0)]

            entry = CrystalEntry(
                entry_id=f"VSEPR-CRY-{len(self.entries):04d}",
                formula=formula,
                name=p["name"],
                crystal_system=p["system"],
                bravais=bravais,
                crystal_class=p["cls"],
                lattice=lattice,
                atoms=atoms,
                space_group_number=p["sg_num"],
                space_group_symbol=p["sg"],
                density=p["density"],
                coordination=p.get("coord", {}),
                stability_score=85.0 if p["Ef"] <= 0 else 50.0,
                formation_energy=p["Ef"],
                bandgap=p["Eg"],
                tags=[p["cls"].value, p["system"].value],
                provenance=f"prototype_db seed=0",
                seed=0,
            )
            self.entries.append(entry)

    def search(self, crystal_class: Optional[CrystalClass] = None,
               system: Optional[CrystalSystem] = None,
               min_density: float = 0.0,
               max_density: float = 1e6) -> List[CrystalEntry]:
        results = []
        for e in self.entries:
            if crystal_class and e.crystal_class != crystal_class:
                continue
            if system and e.crystal_system != system:
                continue
            if not (min_density <= e.density <= max_density):
                continue
            results.append(e)
        return results

    def generate_supercell(self, entry: CrystalEntry,
                           nx: int = 2, ny: int = 2, nz: int = 2) -> CrystalEntry:
        """Create a supercell by replicating the unit cell."""
        new_atoms = []
        for ix in range(nx):
            for iy in range(ny):
                for iz in range(nz):
                    for a in entry.atoms:
                        new_atoms.append(AtomSite(
                            a.symbol, a.Z,
                            (a.x + ix) / nx,
                            (a.y + iy) / ny,
                            (a.z + iz) / nz,
                            a.occupancy, a.wyckoff, a.oxidation_state))

        new_lattice = LatticeParams(
            entry.lattice.a * nx,
            entry.lattice.b * ny,
            entry.lattice.c * nz,
            entry.lattice.alpha,
            entry.lattice.beta,
            entry.lattice.gamma)

        return CrystalEntry(
            entry_id=f"{entry.entry_id}-SC{nx}{ny}{nz}",
            formula=entry.formula,
            name=f"{entry.name} {nx}x{ny}x{nz} supercell",
            crystal_system=entry.crystal_system,
            bravais=entry.bravais,
            crystal_class=entry.crystal_class,
            lattice=new_lattice,
            atoms=new_atoms,
            space_group_number=entry.space_group_number,
            space_group_symbol=entry.space_group_symbol,
            density=entry.density,
            coordination=entry.coordination,
            stability_score=entry.stability_score,
            formation_energy=entry.formation_energy,
            bandgap=entry.bandgap,
            tags=entry.tags + ["supercell"],
            provenance=f"supercell of {entry.entry_id}",
            seed=entry.seed)

    def insert_defect(self, entry: CrystalEntry,
                      defect: DefectSpec) -> CrystalEntry:
        """Insert a point defect into a crystal entry."""
        new_atoms = list(entry.atoms)
        idx = defect.site_index % len(new_atoms) if new_atoms else 0

        if defect.defect_type == "vacancy":
            new_atoms.pop(idx)
        elif defect.defect_type == "substitution" and defect.replacement_symbol:
            old = new_atoms[idx]
            Z_new = _ELEM_DATA.get(defect.replacement_symbol, {}).get("Z", 0)
            new_atoms[idx] = AtomSite(
                defect.replacement_symbol, Z_new,
                old.x, old.y, old.z, old.occupancy)
        elif defect.defect_type == "interstitial":
            Z_new = _ELEM_DATA.get(defect.replacement_symbol, {}).get("Z", 0)
            new_atoms.append(AtomSite(
                defect.replacement_symbol, Z_new,
                defect.displacement[0],
                defect.displacement[1],
                defect.displacement[2]))

        return CrystalEntry(
            entry_id=f"{entry.entry_id}-DEF-{defect.defect_type[:3]}",
            formula=entry.formula,
            name=f"{entry.name} ({defect.defect_type})",
            crystal_system=entry.crystal_system,
            bravais=entry.bravais,
            crystal_class=entry.crystal_class,
            lattice=entry.lattice,
            atoms=new_atoms,
            space_group_number=1,
            space_group_symbol="P1",
            density=entry.density * len(new_atoms) / max(len(entry.atoms), 1),
            stability=StabilityClass.METASTABLE,
            stability_score=entry.stability_score * 0.7,
            formation_energy=entry.formation_energy + 0.5,
            bandgap=entry.bandgap,
            tags=entry.tags + ["defected", defect.defect_type],
            provenance=f"defect({defect.defect_type}) of {entry.entry_id}",
            seed=entry.seed)


# ============================================================================
# Discovery engine
# ============================================================================

class CrystalDiscoveryEngine:
    """Generate candidate crystals via substitution, perturbation, etc."""

    def __init__(self, catalog: CrystalCatalog, seed: int = 42):
        self.catalog = catalog
        self.rng = random.Random(seed)

    def substitution_scan(self, base_formula: str,
                          site_symbol: str,
                          candidates: List[str]) -> List[CrystalEntry]:
        """Try substituting one element for candidates."""
        results = []
        base_entries = [e for e in self.catalog.entries if e.formula == base_formula]
        if not base_entries:
            return results

        base = base_entries[0]
        for cand in candidates:
            for i, a in enumerate(base.atoms):
                if a.symbol == site_symbol:
                    defect = DefectSpec("substitution", i, cand)
                    new = self.catalog.insert_defect(base, defect)
                    new.formula = base.formula.replace(site_symbol, cand, 1)
                    new.name = f"{base.name} ({site_symbol}→{cand})"
                    new.tags.append("substitution_scan")
                    results.append(new)
                    break
        return results

    def stoichiometric_perturbation(self, base_formula: str,
                                     n_variants: int = 5) -> List[CrystalEntry]:
        """Generate lattice parameter perturbations."""
        results = []
        base_entries = [e for e in self.catalog.entries if e.formula == base_formula]
        if not base_entries:
            return results

        base = base_entries[0]
        for i in range(n_variants):
            da = self.rng.gauss(0, 0.05)
            db = self.rng.gauss(0, 0.05)
            dc = self.rng.gauss(0, 0.05)
            new_lattice = LatticeParams(
                base.lattice.a * (1 + da),
                base.lattice.b * (1 + db),
                base.lattice.c * (1 + dc),
                base.lattice.alpha, base.lattice.beta, base.lattice.gamma)

            new = CrystalEntry(
                entry_id=f"{base.entry_id}-PERT{i:02d}",
                formula=base.formula,
                name=f"{base.name} perturbation #{i}",
                crystal_system=base.crystal_system,
                bravais=base.bravais,
                crystal_class=base.crystal_class,
                lattice=new_lattice,
                atoms=list(base.atoms),
                space_group_number=1,
                space_group_symbol="P1",
                density=base.density * (base.lattice.volume / max(new_lattice.volume, 0.01)),
                stability=StabilityClass.HYPOTHETICAL,
                stability_score=max(0, base.stability_score - 20 - abs(da + db + dc) * 100),
                formation_energy=base.formation_energy + 0.1 * (da**2 + db**2 + dc**2) * 1000,
                bandgap=base.bandgap,
                tags=base.tags + ["perturbation"],
                provenance=f"perturbation of {base.entry_id} seed={42+i}",
                seed=42 + i)
            results.append(new)
        return results


# ============================================================================
# Helpers
# ============================================================================

def _split_binary(formula: str) -> List[str]:
    """Split e.g. 'NaCl' → ['Na','Cl'], 'MgO' → ['Mg','O'], 'CaF2' → ['Ca','F']."""
    parts = []
    i = 0
    while i < len(formula):
        if formula[i].isupper():
            sym = formula[i]
            i += 1
            while i < len(formula) and formula[i].islower():
                sym += formula[i]
                i += 1
            while i < len(formula) and formula[i].isdigit():
                i += 1
            parts.append(sym)
        else:
            i += 1
    return parts[:2]


def _split_perovskite(formula: str) -> List[str]:
    """Split e.g. 'SrTiO3' → ['Sr','Ti','O']."""
    parts = _split_binary(formula)
    # For perovskites we need 3 elements
    all_parts = []
    i = 0
    while i < len(formula):
        if formula[i].isupper():
            sym = formula[i]
            i += 1
            while i < len(formula) and formula[i].islower():
                sym += formula[i]
                i += 1
            while i < len(formula) and formula[i].isdigit():
                i += 1
            all_parts.append(sym)
        else:
            i += 1
    return all_parts[:3]


# ============================================================================
# Report generation
# ============================================================================

def generate_crystal_catalog_md(catalog: CrystalCatalog) -> str:
    lines = []
    lines.append("## Crystal Catalog\n")

    for cls in CrystalClass:
        entries = catalog.search(crystal_class=cls)
        if not entries:
            continue
        lines.append(f"### {cls.value.replace('_', ' ').title()} Crystals\n")
        lines.append("| ID | Formula | Name | System | Space Group | a (Å) | Density (g/cm³) | E_f (eV/at) | Eg (eV) | Score |")
        lines.append("|-----|---------|------|--------|-------------|-------|-----------------|-------------|---------|-------|")
        for e in entries:
            eg_str = f"{e.bandgap:.2f}" if e.bandgap >= 0 else "metal"
            lines.append(
                f"| {e.entry_id} | {e.formula} | {e.name} | {e.crystal_system.value} "
                f"| {e.space_group_symbol} | {e.lattice.a:.3f} | {e.density:.2f} "
                f"| {e.formation_energy:.2f} | {eg_str} | {e.stability_score:.0f} |")
        lines.append("")
    return "\n".join(lines)


def generate_crystal_discovery_md(engine: CrystalDiscoveryEngine,
                                  base_formula: str = "NaCl") -> str:
    lines = []
    lines.append(f"## Crystal Discovery: {base_formula} substitution scan\n")

    candidates = ["K", "Li", "Rb", "Cs", "Ag", "Cu"]
    results = engine.substitution_scan(base_formula, "Na", candidates)

    if results:
        lines.append("| Formula | Name | a (Å) | Score | Stability | Tags |")
        lines.append("|---------|------|-------|-------|-----------|------|")
        for r in results:
            lines.append(
                f"| {r.formula} | {r.name} | {r.lattice.a:.3f} "
                f"| {r.stability_score:.0f} | {r.stability.value} "
                f"| {', '.join(r.tags[:3])} |")
    else:
        lines.append("No substitution candidates generated.\n")

    lines.append("")
    lines.append(f"## Lattice Perturbation: {base_formula}\n")
    perts = engine.stoichiometric_perturbation(base_formula, 8)
    if perts:
        lines.append("| ID | a (Å) | b (Å) | c (Å) | V (ų) | Density | Score |")
        lines.append("|----|-------|-------|-------|--------|---------|-------|")
        for p in perts:
            lines.append(
                f"| {p.entry_id} | {p.lattice.a:.3f} | {p.lattice.b:.3f} "
                f"| {p.lattice.c:.3f} | {p.lattice.volume:.1f} "
                f"| {p.density:.2f} | {p.stability_score:.0f} |")

    return "\n".join(lines)


def export_catalog_csv(catalog: CrystalCatalog, path: str):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["entry_id", "formula", "name", "crystal_class", "crystal_system",
                     "bravais", "space_group", "a_A", "b_A", "c_A", "volume_A3",
                     "density_g_cm3", "n_atoms", "formation_energy_eV",
                     "bandgap_eV", "stability_score", "stability_class", "tags"])
        for e in catalog.entries:
            w.writerow([
                e.entry_id, e.formula, e.name, e.crystal_class.value,
                e.crystal_system.value, e.bravais.value, e.space_group_symbol,
                f"{e.lattice.a:.4f}", f"{e.lattice.b:.4f}", f"{e.lattice.c:.4f}",
                f"{e.lattice.volume:.2f}", f"{e.density:.4f}", e.n_atoms,
                f"{e.formation_energy:.4f}", f"{e.bandgap:.2f}",
                f"{e.stability_score:.1f}", e.stability.value,
                "|".join(e.tags)])
