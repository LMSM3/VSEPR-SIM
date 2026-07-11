"""
species_record.py — VSEPR-SIM Species Record Schema & Engine
==============================================================

Defines the canonical VSEPR-format species record, which splits every
species into five inspectable sections:

    1. Identity           — id, name, formula, phase, source provenance
    2. Composition/Structure — atoms, VSEPR geometry, symmetry hints
    3. Thermochemical Source Data — Hf298, S298, reference state
    4. Temperature-Region Fits — Shomate coefficient regions
    5. Engine-Ready Derived Metadata — flags for downstream solvers

Supports four source formats:
    NIST  — NIST WebBook Shomate polynomials
    JANAF — NIST-JANAF Thermochemical Tables (Chase 1998)
    VSEPR — Native VSEPR-SIM species record format
    VSPES — VSEPR Pillar Engine Species (derived/compiled records)

Cp model (Shomate):
    Cp(t) = A + B*t + C*t^2 + D*t^3 + E/t^2   [J/(mol·K)]
    where t = T(K) / 1000

    H(t) - H298 = A*t + B*t^2/2 + C*t^3/3 + D*t^4/4 - E/t + F - H   [kJ/mol]
    S(t) = A*ln(t) + B*t + C*t^2/2 + D*t^3/3 - E/(2*t^2) + G        [J/(mol·K)]

Anti-black-box: every coefficient traces to a cited source.
Deterministic: same species + T → identical Cp, H, S.

VSEPR-SIM 3.0.0
"""

from __future__ import annotations

import json
import math
import re
from dataclasses import dataclass, field, asdict
from enum import Enum
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple


# ═══════════════════════════════════════════════════════════════════════
# Enumerations
# ═══════════════════════════════════════════════════════════════════════

class SourceFamily(Enum):
    NIST_JANAF = "NIST-JANAF"
    NIST_WEBBOOK = "NIST-WebBook"
    VSEPR_NATIVE = "VSEPR-native"
    VSPES_COMPILED = "VSPES-compiled"


class CpModel(Enum):
    SHOMATE = "SHOMATE"
    NASA7 = "NASA7"
    NASA9 = "NASA9"
    CONSTANT = "CONSTANT"


class PhaseTag(Enum):
    GAS = "gas"
    LIQUID = "liquid"
    SOLID = "solid"
    SOLID_II = "solid_II"
    PLASMA = "plasma"
    SUPERCRITICAL = "supercritical"


class GeometryCategory(Enum):
    MONATOMIC = "monatomic"
    DIATOMIC = "diatomic"
    LINEAR = "linear"
    TRIGONAL_PLANAR = "trigonal_planar"
    BENT = "bent"
    TETRAHEDRAL = "tetrahedral"
    TRIGONAL_PYRAMIDAL = "trigonal_pyramidal"
    TRIGONAL_BIPYRAMIDAL = "trigonal_bipyramidal"
    SEESAW = "seesaw"
    T_SHAPED = "t_shaped"
    SQUARE_PLANAR = "square_planar"
    OCTAHEDRAL = "octahedral"
    SQUARE_PYRAMIDAL = "square_pyramidal"
    PENTAGONAL_BIPYRAMIDAL = "pentagonal_bipyramidal"
    OTHER = "other"


class ReactiveFamily(Enum):
    INERT_MONATOMIC = "inert_monatomic"
    INERT_DIATOMIC = "inert_diatomic"
    REACTIVE_DIATOMIC = "reactive_diatomic"
    OXIDIZER = "oxidizer"
    FUEL = "fuel"
    REFRIGERANT = "refrigerant"
    PROCESS_FLUID = "process_fluid"
    CORROSIVE = "corrosive"
    NOBLE = "noble"
    GENERAL = "general"


# ═══════════════════════════════════════════════════════════════════════
# Data Structures — Section 1: Identity
# ═══════════════════════════════════════════════════════════════════════

@dataclass
class AtomEntry:
    element: str
    count: int


@dataclass
class StructureModel:
    category: str = ""
    vsepr_domain_count: int = 0
    geometry: str = ""
    bond_order_hint: int = 0
    formal_charge: int = 0
    radical: bool = False
    symmetry_hint: str = ""


@dataclass
class ReferenceState:
    T_ref_K: float = 298.15
    P_std_bar: float = 1.0
    standard_state_note: str = "ideal_gas_standard_state"


@dataclass
class ThermoReference:
    Hf298_kJmol: float = 0.0
    S298_JmolK: float = 0.0


# ═══════════════════════════════════════════════════════════════════════
# Data Structures — Section 4: Shomate Region
# ═══════════════════════════════════════════════════════════════════════

@dataclass
class ShomateRegion:
    """One temperature region of Shomate coefficients."""
    Tmin_K: float
    Tmax_K: float
    A: float = 0.0
    B: float = 0.0
    C: float = 0.0
    D: float = 0.0
    E: float = 0.0
    F: float = 0.0
    G: float = 0.0
    H: float = 0.0

    def cp(self, T: float) -> float:
        """Cp in J/(mol·K) at temperature T (K)."""
        t = T / 1000.0
        return self.A + self.B * t + self.C * t**2 + self.D * t**3 + self.E / t**2

    def enthalpy(self, T: float) -> float:
        """H(T) - H(298.15) in kJ/mol."""
        t = T / 1000.0
        return (self.A * t + self.B * t**2 / 2.0 + self.C * t**3 / 3.0 +
                self.D * t**4 / 4.0 - self.E / t + self.F - self.H)

    def entropy(self, T: float) -> float:
        """S(T) in J/(mol·K)."""
        t = T / 1000.0
        return (self.A * math.log(t) + self.B * t + self.C * t**2 / 2.0 +
                self.D * t**3 / 3.0 - self.E / (2.0 * t**2) + self.G)

    def contains(self, T: float) -> bool:
        return self.Tmin_K <= T <= self.Tmax_K


# ═══════════════════════════════════════════════════════════════════════
# Data Structures — Section 5: Engine Flags
# ═══════════════════════════════════════════════════════════════════════

@dataclass
class EngineFlags:
    allow_ideal_gas: bool = True
    allow_real_gas_upgrade: bool = True
    allow_dissociation: bool = False
    allow_ionization: bool = False
    reactive_family: str = "general"


# ═══════════════════════════════════════════════════════════════════════
# Master Record
# ═══════════════════════════════════════════════════════════════════════

@dataclass
class SpeciesRecord:
    """
    Complete VSEPR-format species record.

    Sections:
        1. Identity
        2. Composition / Structure
        3. Thermochemical Source Data
        4. Temperature-Region Fits (Shomate)
        5. Engine-Ready Derived Metadata
    """
    # Section 1: Identity
    id: str = ""
    name: str = ""
    formula: str = ""
    phase: str = "gas"
    source_family: str = "NIST-JANAF"
    source_ref: str = ""
    source_url: str = ""

    # Section 2: Composition / Structure
    molar_mass_gmol: float = 0.0
    cas_number: str = ""
    inchi: str = ""
    inchikey: str = ""
    atoms: List[AtomEntry] = field(default_factory=list)
    structure_model: StructureModel = field(default_factory=StructureModel)

    # Section 3: Thermochemical Source Data
    reference_state: ReferenceState = field(default_factory=ReferenceState)
    thermo_reference: ThermoReference = field(default_factory=ThermoReference)

    # Section 4: Temperature-Region Fits
    cp_model: str = "SHOMATE"
    regions: List[ShomateRegion] = field(default_factory=list)

    # Section 5: Engine Flags
    engine_flags: EngineFlags = field(default_factory=EngineFlags)

    # ── Thermodynamic evaluation ──

    def _find_region(self, T: float) -> Optional[ShomateRegion]:
        for reg in self.regions:
            if reg.contains(T):
                return reg
        return None

    def cp(self, T: float) -> float:
        """Cp at T (K) in J/(mol·K). Returns NaN if T is outside all regions."""
        reg = self._find_region(T)
        if reg is None:
            return float("nan")
        return reg.cp(T)

    def enthalpy(self, T: float) -> float:
        """H(T) - H(298.15) in kJ/mol."""
        reg = self._find_region(T)
        if reg is None:
            return float("nan")
        return reg.enthalpy(T)

    def entropy(self, T: float) -> float:
        """S(T) in J/(mol·K)."""
        reg = self._find_region(T)
        if reg is None:
            return float("nan")
        return reg.entropy(T)

    def gibbs(self, T: float) -> float:
        """G(T) = H(T) - T*S(T) in kJ/mol (relative to elements)."""
        h = self.enthalpy(T)
        s = self.entropy(T)
        if math.isnan(h) or math.isnan(s):
            return float("nan")
        return (self.thermo_reference.Hf298_kJmol + h) - T * s / 1000.0

    @property
    def T_range(self) -> Tuple[float, float]:
        """Overall temperature validity range."""
        if not self.regions:
            return (0.0, 0.0)
        return (self.regions[0].Tmin_K, self.regions[-1].Tmax_K)

    @property
    def n_regions(self) -> int:
        return len(self.regions)

    def cp_table(self, T_start: float = 298.15, T_end: float = 3000.0,
                 step: float = 100.0) -> List[dict]:
        """Generate a Cp/H/S/G table over a temperature range."""
        rows = []
        T = T_start
        while T <= T_end + 0.01:
            cp_val = self.cp(T)
            if not math.isnan(cp_val):
                rows.append({
                    "T_K": round(T, 2),
                    "Cp_JmolK": round(cp_val, 4),
                    "H_kJmol": round(self.enthalpy(T), 4),
                    "S_JmolK": round(self.entropy(T), 4),
                    "G_kJmol": round(self.gibbs(T), 4),
                })
            T += step
        return rows

    # ── Serialization: VSEPR format ──

    def to_vsepr_text(self) -> str:
        """Serialize to VSEPR-format species text block."""
        lines = ["SPECIES_BEGIN"]
        lines.append(f"  id                  = {self.id}")
        lines.append(f"  name                = {self.name}")
        lines.append(f"  formula             = {self.formula}")
        lines.append(f"  phase               = {self.phase}")
        lines.append(f"  source_family       = {self.source_family}")
        lines.append(f"  source_ref          = {self.source_ref}")
        lines.append(f"  source_url          = {self.source_url}")
        lines.append("")
        lines.append(f"  molar_mass_gmol     = {self.molar_mass_gmol}")
        lines.append(f"  cas_number          = {self.cas_number}")
        lines.append(f"  inchi               = {self.inchi}")
        lines.append(f"  inchikey            = {self.inchikey}")
        lines.append("")

        # Atoms
        lines.append("  atoms = [")
        for a in self.atoms:
            lines.append(f"    {{element = {a.element}, count = {a.count}}}")
        lines.append("  ]")
        lines.append("")

        # Structure model
        sm = self.structure_model
        lines.append("  structure_model = {")
        lines.append(f"    category            = {sm.category}")
        lines.append(f"    vsepr_domain_count  = {sm.vsepr_domain_count}")
        lines.append(f"    geometry            = {sm.geometry}")
        lines.append(f"    bond_order_hint     = {sm.bond_order_hint}")
        lines.append(f"    formal_charge       = {sm.formal_charge}")
        lines.append(f"    radical             = {'true' if sm.radical else 'false'}")
        lines.append(f"    symmetry_hint       = {sm.symmetry_hint}")
        lines.append("  }")
        lines.append("")

        # Reference state
        rs = self.reference_state
        lines.append("  reference_state = {")
        lines.append(f"    T_ref_K             = {rs.T_ref_K}")
        lines.append(f"    P_std_bar           = {rs.P_std_bar}")
        lines.append(f"    standard_state_note = {rs.standard_state_note}")
        lines.append("  }")
        lines.append("")

        # Thermo reference
        tr = self.thermo_reference
        lines.append("  thermo_reference = {")
        lines.append(f"    Hf298_kJmol         = {tr.Hf298_kJmol}")
        lines.append(f"    S298_JmolK          = {tr.S298_JmolK}")
        lines.append("  }")
        lines.append("")

        lines.append(f"  cp_model = {self.cp_model}")
        lines.append("")

        # Regions
        lines.append("  regions = [")
        for reg in self.regions:
            lines.append("    {")
            lines.append(f"      Tmin_K = {reg.Tmin_K}")
            lines.append(f"      Tmax_K = {reg.Tmax_K}")
            for coeff in ("A", "B", "C", "D", "E", "F", "G", "H"):
                lines.append(f"      {coeff} = {getattr(reg, coeff)}")
            lines.append("    },")
        lines.append("  ]")
        lines.append("")

        # Engine flags
        ef = self.engine_flags
        lines.append("  engine_flags = {")
        lines.append(f"    allow_ideal_gas         = {'true' if ef.allow_ideal_gas else 'false'}")
        lines.append(f"    allow_real_gas_upgrade  = {'true' if ef.allow_real_gas_upgrade else 'false'}")
        lines.append(f"    allow_dissociation      = {'true' if ef.allow_dissociation else 'false'}")
        lines.append(f"    allow_ionization        = {'true' if ef.allow_ionization else 'false'}")
        lines.append(f"    reactive_family         = {ef.reactive_family}")
        lines.append("  }")
        lines.append("SPECIES_END")
        return "\n".join(lines)

    def to_json(self) -> dict:
        """Serialize to JSON-compatible dict."""
        return {
            "id": self.id,
            "name": self.name,
            "formula": self.formula,
            "phase": self.phase,
            "source_family": self.source_family,
            "source_ref": self.source_ref,
            "source_url": self.source_url,
            "molar_mass_gmol": self.molar_mass_gmol,
            "cas_number": self.cas_number,
            "inchi": self.inchi,
            "inchikey": self.inchikey,
            "atoms": [{"element": a.element, "count": a.count} for a in self.atoms],
            "structure_model": asdict(self.structure_model),
            "reference_state": asdict(self.reference_state),
            "thermo_reference": asdict(self.thermo_reference),
            "cp_model": self.cp_model,
            "regions": [asdict(r) for r in self.regions],
            "engine_flags": asdict(self.engine_flags),
        }

    @classmethod
    def from_json(cls, data: dict) -> SpeciesRecord:
        """Deserialize from JSON dict."""
        rec = cls()
        rec.id = data.get("id", "")
        rec.name = data.get("name", "")
        rec.formula = data.get("formula", "")
        rec.phase = data.get("phase", "gas")
        rec.source_family = data.get("source_family", "")
        rec.source_ref = data.get("source_ref", "")
        rec.source_url = data.get("source_url", "")
        rec.molar_mass_gmol = data.get("molar_mass_gmol", 0.0)
        rec.cas_number = data.get("cas_number", "")
        rec.inchi = data.get("inchi", "")
        rec.inchikey = data.get("inchikey", "")
        rec.atoms = [AtomEntry(**a) for a in data.get("atoms", [])]
        if "structure_model" in data:
            rec.structure_model = StructureModel(**data["structure_model"])
        if "reference_state" in data:
            rec.reference_state = ReferenceState(**data["reference_state"])
        if "thermo_reference" in data:
            rec.thermo_reference = ThermoReference(**data["thermo_reference"])
        rec.cp_model = data.get("cp_model", "SHOMATE")
        rec.regions = [ShomateRegion(**r) for r in data.get("regions", [])]
        if "engine_flags" in data:
            rec.engine_flags = EngineFlags(**data["engine_flags"])
        return rec


# ═══════════════════════════════════════════════════════════════════════
# VSEPR text parser
# ═══════════════════════════════════════════════════════════════════════

def _parse_value(raw: str) -> Any:
    """Parse a raw value string into Python type."""
    raw = raw.strip()
    if raw.lower() == "true":
        return True
    if raw.lower() == "false":
        return False
    try:
        if "." in raw:
            return float(raw)
        return int(raw)
    except ValueError:
        return raw


def parse_vsepr_text(text: str) -> List[SpeciesRecord]:
    """Parse one or more SPECIES_BEGIN…SPECIES_END blocks."""
    records = []
    blocks = re.findall(
        r"SPECIES_BEGIN\s*(.*?)\s*SPECIES_END",
        text, re.DOTALL)

    for block in blocks:
        rec = SpeciesRecord()
        lines = block.split("\n")
        i = 0
        while i < len(lines):
            line = lines[i].strip()
            i += 1
            if not line or line.startswith("#"):
                continue

            # atoms = [ ... ]
            if line.startswith("atoms"):
                atoms = []
                while i < len(lines):
                    al = lines[i].strip()
                    i += 1
                    if al == "]":
                        break
                    m = re.search(r"element\s*=\s*(\w+).*count\s*=\s*(\d+)", al)
                    if m:
                        atoms.append(AtomEntry(m.group(1), int(m.group(2))))
                rec.atoms = atoms
                continue

            # regions = [ ... ]
            if line.startswith("regions"):
                regions = []
                while i < len(lines):
                    rl = lines[i].strip()
                    i += 1
                    if rl == "]":
                        break
                    if rl == "{":
                        region_data = {}
                        while i < len(lines):
                            rrl = lines[i].strip()
                            i += 1
                            if rrl.startswith("}"):
                                break
                            if "=" in rrl:
                                k, v = rrl.split("=", 1)
                                region_data[k.strip()] = _parse_value(v)
                        regions.append(ShomateRegion(
                            Tmin_K=region_data.get("Tmin_K", 0.0),
                            Tmax_K=region_data.get("Tmax_K", 0.0),
                            A=region_data.get("A", 0.0),
                            B=region_data.get("B", 0.0),
                            C=region_data.get("C", 0.0),
                            D=region_data.get("D", 0.0),
                            E=region_data.get("E", 0.0),
                            F=region_data.get("F", 0.0),
                            G=region_data.get("G", 0.0),
                            H=region_data.get("H", 0.0),
                        ))
                rec.regions = regions
                continue

            # structure_model = { ... }
            if line.startswith("structure_model"):
                sm_data = {}
                while i < len(lines):
                    sl = lines[i].strip()
                    i += 1
                    if sl.startswith("}"):
                        break
                    if "=" in sl:
                        k, v = sl.split("=", 1)
                        sm_data[k.strip()] = _parse_value(v)
                rec.structure_model = StructureModel(
                    category=str(sm_data.get("category", "")),
                    vsepr_domain_count=int(sm_data.get("vsepr_domain_count", 0)),
                    geometry=str(sm_data.get("geometry", "")),
                    bond_order_hint=int(sm_data.get("bond_order_hint", 0)),
                    formal_charge=int(sm_data.get("formal_charge", 0)),
                    radical=bool(sm_data.get("radical", False)),
                    symmetry_hint=str(sm_data.get("symmetry_hint", "")),
                )
                continue

            # reference_state = { ... }
            if line.startswith("reference_state"):
                rs_data = {}
                while i < len(lines):
                    sl = lines[i].strip()
                    i += 1
                    if sl.startswith("}"):
                        break
                    if "=" in sl:
                        k, v = sl.split("=", 1)
                        rs_data[k.strip()] = _parse_value(v)
                rec.reference_state = ReferenceState(
                    T_ref_K=float(rs_data.get("T_ref_K", 298.15)),
                    P_std_bar=float(rs_data.get("P_std_bar", 1.0)),
                    standard_state_note=str(rs_data.get("standard_state_note", "")),
                )
                continue

            # thermo_reference = { ... }
            if line.startswith("thermo_reference"):
                tr_data = {}
                while i < len(lines):
                    sl = lines[i].strip()
                    i += 1
                    if sl.startswith("}"):
                        break
                    if "=" in sl:
                        k, v = sl.split("=", 1)
                        tr_data[k.strip()] = _parse_value(v)
                rec.thermo_reference = ThermoReference(
                    Hf298_kJmol=float(tr_data.get("Hf298_kJmol", 0.0)),
                    S298_JmolK=float(tr_data.get("S298_JmolK", 0.0)),
                )
                continue

            # engine_flags = { ... }
            if line.startswith("engine_flags"):
                ef_data = {}
                while i < len(lines):
                    sl = lines[i].strip()
                    i += 1
                    if sl.startswith("}"):
                        break
                    if "=" in sl:
                        k, v = sl.split("=", 1)
                        ef_data[k.strip()] = _parse_value(v)
                rec.engine_flags = EngineFlags(
                    allow_ideal_gas=bool(ef_data.get("allow_ideal_gas", True)),
                    allow_real_gas_upgrade=bool(ef_data.get("allow_real_gas_upgrade", True)),
                    allow_dissociation=bool(ef_data.get("allow_dissociation", False)),
                    allow_ionization=bool(ef_data.get("allow_ionization", False)),
                    reactive_family=str(ef_data.get("reactive_family", "general")),
                )
                continue

            # Simple key = value
            if "=" in line:
                k, v = line.split("=", 1)
                k = k.strip()
                v = _parse_value(v)
                if hasattr(rec, k):
                    setattr(rec, k, v)

        records.append(rec)
    return records


# ═══════════════════════════════════════════════════════════════════════
# File I/O helpers
# ═══════════════════════════════════════════════════════════════════════

def save_species_json(records: List[SpeciesRecord], path: Path):
    """Save species records to JSON."""
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        json.dump([r.to_json() for r in records], f, indent=2)


def load_species_json(path: Path) -> List[SpeciesRecord]:
    """Load species records from JSON."""
    with open(path, "r", encoding="utf-8") as f:
        data = json.load(f)
    return [SpeciesRecord.from_json(d) for d in data]


def save_species_vsepr(records: List[SpeciesRecord], path: Path):
    """Save species records in native VSEPR text format."""
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        for rec in records:
            f.write(rec.to_vsepr_text())
            f.write("\n\n")


def load_species_vsepr(path: Path) -> List[SpeciesRecord]:
    """Load species records from VSEPR text format."""
    text = path.read_text(encoding="utf-8")
    return parse_vsepr_text(text)
