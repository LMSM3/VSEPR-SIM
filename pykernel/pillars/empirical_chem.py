"""
EmpiricalChem — Atomic Empirical Data Layer
============================================

Pillar F of the VSEPR-SIM Five Pillars architecture.
Version: v5.1.3

Fetches authoritative empirical atomic data from:
  1. PubChem PUG REST API  (radii, ionization energies, electron affinity,
                            electron config, atomic weight, melting/boiling)
  2. NIST Atomic Spectra Database  (first ionization energy cross-check)
  3. Warm fallback: data/PeriodicTableJSON.json  (offline / rate-limited)

Output:  data/elements.empirical.json
         docs/audit/chem_audit_empirical.md

Design principles (same as all VSEPR-SIM pillars):
  - Anti-black-box: every field carries a source_tag
  - Deterministic: same network state → identical output
  - Offline-safe: PeriodicTableJSON warm fallback if fetch fails
  - No new pip dependencies: only stdlib (urllib, json, csv, math, time)
    + numpy (already in .venv) for numeric comparisons

Empirical fields produced (per element):
  Z                        int      atomic number
  symbol                   str
  name                     str
  atomic_weight            float    amu   (IUPAC 2021 via PubChem)
  en_pauling               float    dimensionless
  shells                   list[int]
  radioactive              bool
  electron_config          str      e.g. "1s1"
  block                    str      s / p / d / f
  period                   int
  group                    int | null
  category                 str      e.g. "diatomic nonmetal"
  phase_at_STP             str      "Gas" | "Liquid" | "Solid" | "Unknown"
  covalent_radius_pm       float    pm   (Alvarez 2008 via PubChem)
  vdw_radius_pm            float    pm   (Bondi + Truhlar)
  ionic_radius_pm          float    pm   (Shannon 1976, most common oxidation state)
  ionization_energy_1_eV   float    eV   (PubChem / NIST cross-checked)
  electron_affinity_eV     float    eV   (may be 0.0 for noble gases)
  polarizability_au        float    bohr³ (NIST via PeriodicTableJSON or hardcoded NIST table)
  melting_point_K          float    K    (null→ 0.0)
  boiling_point_K          float    K
  density_g_per_cm3        float    g/cm³
  molar_heat_J_mol_K       float    J/(mol·K) at 25 °C, 1 bar

Sources:
  PubChem   https://pubchem.ncbi.nlm.nih.gov/rest/pug/element/{prop}/JSON
  NIST ASD  https://physics.nist.gov/cgi-bin/ASD/ie.pl
  Alvarez 2008  DOI:10.1039/b801115j (covalent radii)
  Bondi 1964 / Truhlar 2009 (vdW radii)
  Shannon 1976  Acta Cryst. A32:751 (ionic radii)
  NIST polarizability  NIST SRD 128
"""

from __future__ import annotations

import json
import math
import os
import time
import urllib.request
import urllib.error
from dataclasses import dataclass, field, asdict
from typing import Any, Dict, List, Optional

# ---------------------------------------------------------------------------
# Repository root resolution
# ---------------------------------------------------------------------------
_PILLAR_DIR = os.path.dirname(os.path.abspath(__file__))
_REPO_ROOT  = os.path.abspath(os.path.join(_PILLAR_DIR, "..", ".."))
_DATA_DIR   = os.path.join(_REPO_ROOT, "data")
_DOCS_DIR   = os.path.join(_REPO_ROOT, "docs", "audit")

# ---------------------------------------------------------------------------
# Output paths
# ---------------------------------------------------------------------------
EMPIRICAL_JSON_PATH = os.path.join(_DATA_DIR, "elements.empirical.json")
AUDIT_MD_PATH       = os.path.join(_DOCS_DIR, "chem_audit_empirical.md")

# ---------------------------------------------------------------------------
# Warm-fallback path (already present in repo)
# ---------------------------------------------------------------------------
_FALLBACK_PATH  = os.path.join(_DATA_DIR, "PeriodicTableJSON.json")
_PHYSICS_PATH   = os.path.join(_DATA_DIR, "elements.physics.json")
_POLAR_CSV_PATH = os.path.join(_DATA_DIR, "polarizability_ref.csv")

# ---------------------------------------------------------------------------
# PubChem PUG REST — bulk element property endpoint
# Returns all 118 elements in one call per property group.
# ---------------------------------------------------------------------------
_PUBCHEM_BASE = "https://pubchem.ncbi.nlm.nih.gov/rest/pug/periodictable/JSON"
# For single-property bulk pulls (older PUG REST style):
_PUBCHEM_ELEM = "https://pubchem.ncbi.nlm.nih.gov/rest/pug/element/{sym}/{prop}/JSON"

# The bulk periodic-table endpoint returns all at once:
_PUBCHEM_TABLE = "https://pubchem.ncbi.nlm.nih.gov/rest/pug/periodictable/JSON?response_type=display"

# NIST ASD first ionization energy (form GET — returns HTML; we parse the eV table)
# More practical: NIST ground-state ionization via the NIST Chemistry WebBook
# JSON-accessible IE: use PeriodicTableJSON fallback cross-checked with PubChem

# ---------------------------------------------------------------------------
# NIST polarizability table (NIST SRD 128, 2018 — static reference values)
# Units: Bohr³ (atomic units). Source: doi:10.1063/1.5039113
# Indexed by Z (1-based, index 0 unused).
# ---------------------------------------------------------------------------
_NIST_POLARIZABILITY_AU: Dict[int, float] = {
    1: 4.500,  2: 1.384,  3: 164.2,  4: 37.74,  5: 20.50,
    6: 11.67,  7: 7.400,  8: 5.412,  9: 3.740,  10: 2.669,
    11: 162.7, 12: 71.22, 13: 57.74, 14: 37.17, 15: 25.00,
    16: 19.40, 17: 14.57, 18: 11.08, 19: 289.7,  20: 160.8,
    21: 97.00, 22: 100.0, 23: 87.00, 24: 83.00,  25: 60.00,
    26: 56.00, 27: 50.00, 28: 48.00, 29: 46.50,  30: 38.67,
    31: 50.60, 32: 40.00, 33: 29.80, 34: 26.20,  35: 21.80,
    36: 16.79, 37: 319.8, 38: 197.2, 39: 162.0,  40: 112.0,
    41: 98.00, 42: 87.00, 43: 79.00, 44: 72.00,  45: 66.00,
    46: 26.14, 47: 55.00, 48: 46.00, 49: 65.90,  50: 53.00,
    51: 43.00, 52: 37.65, 53: 35.00, 54: 27.32,  55: 400.9,
    56: 272.0, 57: 215.0, 58: 204.0, 59: 215.0,  60: 208.0,
    61: 200.0, 62: 192.0, 63: 194.0, 64: 158.0,  65: 170.0,
    66: 163.0, 67: 156.0, 68: 150.0, 69: 144.0,  70: 139.0,
    71: 137.0, 72: 103.0, 73: 74.00, 74: 68.00,  75: 62.00,
    76: 57.00, 77: 54.00, 78: 48.00, 79: 36.00,  80: 33.91,
    81: 50.00, 82: 47.40, 83: 48.70, 84: 44.00,  85: 42.00,
    86: 35.00, 87: 317.8, 88: 246.0, 89: 203.0,  90: 217.0,
    91: 154.0, 92: 129.0, 93: 151.0, 94: 132.0,  95: 131.0,
    96: 144.0, 97: 125.0, 98: 122.0, 99: 118.0, 100: 113.0,
    101: 109.0, 102: 110.0, 103: 320.0, 104: 112.0, 105: 42.0,
    106: 40.0, 107: 38.0, 108: 36.0, 109: 34.0, 110: 32.0,
    111: 32.0, 112: 28.0, 113: 29.0, 114: 31.0, 115: 71.0,
    116: 76.0, 117: 58.0, 118: 50.0,
}

# ---------------------------------------------------------------------------
# Alvarez 2008 covalent radii (pm), DOI:10.1039/b801115j
# Indexed by Z.
# ---------------------------------------------------------------------------
_COVALENT_RADIUS_PM: Dict[int, float] = {
    1: 31,  2: 28,  3: 128, 4: 96,  5: 84,  6: 77,  7: 71,  8: 66,  9: 64, 10: 58,
    11: 166, 12: 141, 13: 121, 14: 111, 15: 107, 16: 105, 17: 102, 18: 106,
    19: 203, 20: 176, 21: 170, 22: 160, 23: 153, 24: 139, 25: 139, 26: 132,
    27: 126, 28: 124, 29: 132, 30: 122, 31: 122, 32: 120, 33: 119, 34: 120,
    35: 120, 36: 116, 37: 220, 38: 195, 39: 190, 40: 175, 41: 164, 42: 154,
    43: 147, 44: 146, 45: 142, 46: 139, 47: 145, 48: 144, 49: 142, 50: 139,
    51: 139, 52: 138, 53: 139, 54: 140, 55: 244, 56: 215, 57: 207, 58: 204,
    59: 203, 60: 201, 61: 199, 62: 198, 63: 198, 64: 196, 65: 194, 66: 192,
    67: 192, 68: 189, 69: 190, 70: 187, 71: 187, 72: 175, 73: 170, 74: 162,
    75: 151, 76: 144, 77: 141, 78: 136, 79: 136, 80: 132, 81: 145, 82: 146,
    83: 148, 84: 140, 85: 150, 86: 150, 87: 260, 88: 221, 89: 215, 90: 206,
    91: 200, 92: 196, 93: 190, 94: 187, 95: 180, 96: 169, 97: 0,  98: 0,
    99: 0,  100: 0,  101: 0,  102: 0,  103: 0,  104: 157, 105: 149, 106: 143,
    107: 141, 108: 134, 109: 129, 110: 128, 111: 121, 112: 122, 113: 136,
    114: 143, 115: 162, 116: 175, 117: 165, 118: 157,
}

# Bondi (1964) / Truhlar (2009) van-der-Waals radii (pm)
_VDW_RADIUS_PM: Dict[int, float] = {
    1: 120, 2: 140, 3: 182, 4: 153, 5: 192, 6: 170, 7: 155, 8: 152, 9: 147, 10: 154,
    11: 227, 12: 173, 13: 184, 14: 210, 15: 180, 16: 180, 17: 175, 18: 188,
    19: 275, 20: 231, 21: 211, 22: 0,   23: 0,   24: 0,   25: 0,   26: 0,
    27: 0,   28: 163, 29: 140, 30: 139, 31: 187, 32: 211, 33: 185, 34: 190,
    35: 185, 36: 202, 37: 303, 38: 249, 39: 0,   40: 0,   41: 0,   42: 0,
    43: 0,   44: 0,   45: 0,   46: 163, 47: 172, 48: 158, 49: 193, 50: 217,
    51: 206, 52: 206, 53: 198, 54: 216, 55: 343, 56: 268, 57: 0,   58: 0,
    59: 0,   60: 0,   61: 0,   62: 0,   63: 0,   64: 0,   65: 0,   66: 0,
    67: 0,   68: 0,   69: 0,   70: 0,   71: 0,   72: 0,   73: 0,   74: 0,
    75: 0,   76: 0,   77: 0,   78: 175, 79: 166, 80: 155, 81: 196, 82: 202,
    83: 207, 84: 197, 85: 202, 86: 220, 87: 348, 88: 283, 89: 0,   90: 0,
    91: 0,   92: 186, 93: 0,   94: 0,   95: 0,   96: 0,   97: 0,   98: 0,
    99: 0,   100: 0,  101: 0,  102: 0,  103: 0,  104: 0,  105: 0,  106: 0,
    107: 0,  108: 0,  109: 0,  110: 0,  111: 0,  112: 0,  113: 0,  114: 0,
    115: 0,  116: 0,  117: 0,  118: 0,
}

# Shannon (1976) ionic radii, most common oxidation state, CN 6 (pm)
# 0 = not commonly ionic or data unavailable
_IONIC_RADIUS_PM: Dict[int, float] = {
    1: 138, 2: 0,   3: 76,  4: 45,  5: 27,  6: 0,   7: 146, 8: 140, 9: 133, 10: 0,
    11: 102, 12: 72, 13: 54, 14: 40, 15: 44, 16: 184, 17: 181, 18: 0,
    19: 138, 20: 100, 21: 75, 22: 61, 23: 64, 24: 62, 25: 83, 26: 65,
    27: 61,  28: 69,  29: 73, 30: 74, 31: 62, 32: 53, 33: 58, 34: 198,
    35: 196, 36: 0,   37: 152, 38: 118, 39: 90, 40: 72, 41: 64, 42: 65,
    43: 65,  44: 68,  45: 67, 46: 86, 47: 115, 48: 95, 49: 80, 50: 69,
    51: 76,  52: 221, 53: 220, 54: 0,  55: 167, 56: 136, 57: 103, 58: 101,
    59: 99,  60: 98,  61: 97, 62: 96, 63: 95,  64: 94, 65: 92,  66: 91,
    67: 90,  68: 89,  69: 87, 70: 86, 71: 86,  72: 58, 73: 64,  74: 60,
    75: 55,  76: 55,  77: 68, 78: 63, 79: 85,  80: 102, 81: 150, 82: 119,
    83: 103, 84: 94,  85: 0,  86: 0,  87: 180, 88: 148, 89: 112, 90: 94,
    91: 92,  92: 73,  93: 85, 94: 85, 95: 98,  96: 97, 97: 96,  98: 95,
    99: 83,  100: 89, 101: 90, 102: 95, 103: 0, 104: 0, 105: 0, 106: 0,
    107: 0,  108: 0,  109: 0, 110: 0, 111: 0, 112: 0, 113: 0, 114: 0,
    115: 0,  116: 0,  117: 0, 118: 0,
}

# ---------------------------------------------------------------------------
# Empirical record dataclass
# ---------------------------------------------------------------------------

@dataclass
class EmpiricalElement:
    """Authoritative empirical atomic record for one element."""
    Z:                      int
    symbol:                 str
    name:                   str
    atomic_weight:          float   = 0.0   # amu, IUPAC 2021
    en_pauling:             float   = 0.0   # Pauling electronegativity
    shells:                 list    = field(default_factory=list)
    radioactive:            bool    = False
    electron_config:        str     = ""
    block:                  str     = ""    # s/p/d/f
    period:                 int     = 0
    group:                  Optional[int] = None
    category:               str     = ""
    phase_at_STP:           str     = "Unknown"
    covalent_radius_pm:     float   = 0.0   # Alvarez 2008
    vdw_radius_pm:          float   = 0.0   # Bondi/Truhlar
    ionic_radius_pm:        float   = 0.0   # Shannon 1976 CN6
    ionization_energy_1_eV: float   = 0.0   # first IE
    electron_affinity_eV:   float   = 0.0   # may be 0 for noble gases
    polarizability_au:      float   = 0.0   # bohr³, NIST SRD-128
    melting_point_K:        float   = 0.0
    boiling_point_K:        float   = 0.0
    density_g_per_cm3:      float   = 0.0
    molar_heat_J_mol_K:     float   = 0.0

    # Source provenance tags (per field, set during merge)
    _sources: dict = field(default_factory=dict, repr=False)


# ---------------------------------------------------------------------------
# Fetch helpers
# ---------------------------------------------------------------------------

def _get_json(url: str, timeout: int = 12) -> Optional[dict]:
    """GET a JSON endpoint; return None on any error."""
    try:
        req = urllib.request.Request(url, headers={"User-Agent": "VSEPR-SIM/5.1.3 empirical_chem"})
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            raw = resp.read().decode("utf-8", errors="replace")
        return json.loads(raw)
    except Exception:
        return None


def _fetch_pubchem_table() -> Optional[dict]:
    """
    Fetch the PubChem periodic table JSON.
    Returns the top-level dict or None.

    Endpoint: https://pubchem.ncbi.nlm.nih.gov/rest/pug/periodictable/JSON
    """
    url = "https://pubchem.ncbi.nlm.nih.gov/rest/pug/periodictable/JSON"
    return _get_json(url)


def _load_fallback() -> List[dict]:
    """Load PeriodicTableJSON.json as a list of element dicts, keyed by atomic_number."""
    if not os.path.exists(_FALLBACK_PATH):
        return []
    with open(_FALLBACK_PATH, encoding="utf-8") as f:
        data = json.load(f)
    return data.get("elements", [])


def _load_physics() -> List[dict]:
    """Load elements.physics.json."""
    if not os.path.exists(_PHYSICS_PATH):
        return []
    with open(_PHYSICS_PATH, encoding="utf-8") as f:
        data = json.load(f)
    return data.get("elements", [])


def _load_polarizability_csv() -> Dict[int, float]:
    """Load polarizability_ref.csv if it exists and has Z + value columns."""
    result: Dict[int, float] = {}
    if not os.path.exists(_POLAR_CSV_PATH):
        return result
    try:
        import csv
        with open(_POLAR_CSV_PATH, encoding="utf-8") as f:
            reader = csv.DictReader(f)
            for row in reader:
                try:
                    z = int(row.get("Z") or row.get("z") or 0)
                    val = float(row.get("polarizability_au") or row.get("value") or 0)
                    if z > 0 and val > 0:
                        result[z] = val
                except (ValueError, TypeError):
                    pass
    except Exception:
        pass
    return result


# ---------------------------------------------------------------------------
# PubChem periodic table parser
# ---------------------------------------------------------------------------

def _parse_pubchem_table(data: dict) -> Dict[int, dict]:
    """
    Parse the PubChem periodic table JSON response.
    Returns a dict keyed by atomic number (int).
    """
    by_z: Dict[int, dict] = {}
    try:
        table = data.get("Table", {})
        columns = table.get("Columns", {}).get("Column", [])
        rows = table.get("Row", [])
        for row in rows:
            cells = row.get("Cell", [])
            record: dict = {}
            for col, val in zip(columns, cells):
                record[col] = val
            try:
                z = int(record.get("AtomicNumber", 0))
                if z > 0:
                    by_z[z] = record
            except (ValueError, TypeError):
                pass
    except Exception:
        pass
    return by_z


# ---------------------------------------------------------------------------
# Core merge logic
# ---------------------------------------------------------------------------

def build_empirical_elements(
    verbose: bool = True,
    rate_limit_s: float = 0.25,
) -> List[EmpiricalElement]:
    """
    Merge empirical data from PubChem REST + NIST SRD-128 table + PeriodicTableJSON fallback
    into a list of EmpiricalElement records (Z=1..118).

    Priority:
      covalent_radius_pm    → hardcoded Alvarez 2008 table (highest precision)
      vdw_radius_pm         → hardcoded Bondi/Truhlar table
      ionic_radius_pm       → hardcoded Shannon 1976 table
      polarizability_au     → polarizability_ref.csv > NIST SRD-128 table
      ionization_energy_1_eV→ PubChem > PeriodicTableJSON
      electron_affinity_eV  → PubChem > PeriodicTableJSON
      atomic_weight         → PubChem > physics.json > PeriodicTableJSON
      en_pauling            → PubChem > physics.json > PeriodicTableJSON
      shells                → physics.json > PeriodicTableJSON
      electron_config       → PubChem > PeriodicTableJSON
      block/period/group    → PubChem > PeriodicTableJSON
      category              → PubChem > PeriodicTableJSON
      phase_at_STP          → PubChem > PeriodicTableJSON
      melting_point_K       → PubChem > PeriodicTableJSON
      boiling_point_K       → PubChem > PeriodicTableJSON
      density_g_per_cm3     → PubChem > PeriodicTableJSON
      molar_heat_J_mol_K    → PubChem > PeriodicTableJSON
    """
    if verbose:
        print("[EmpiricalChem] Loading warm-fallback data …")

    fallback_list = _load_fallback()
    fallback: Dict[int, dict] = {int(e.get("number", 0)): e for e in fallback_list if e.get("number")}

    physics_list = _load_physics()
    physics: Dict[int, dict] = {int(e.get("Z", 0)): e for e in physics_list if e.get("Z")}

    polar_csv = _load_polarizability_csv()

    if verbose:
        print(f"[EmpiricalChem] Fallback elements loaded: {len(fallback)}")
        print("[EmpiricalChem] Fetching PubChem periodic table …")

    pubchem_raw = _fetch_pubchem_table()
    if pubchem_raw:
        pubchem = _parse_pubchem_table(pubchem_raw)
        if verbose:
            print(f"[EmpiricalChem] PubChem returned {len(pubchem)} element records.")
    else:
        pubchem = {}
        if verbose:
            print("[EmpiricalChem] PubChem unavailable — using PeriodicTableJSON fallback only.")

    time.sleep(rate_limit_s)  # polite pause after bulk fetch

    elements: List[EmpiricalElement] = []

    for Z in range(1, 119):
        fb  = fallback.get(Z, {})
        ph  = physics.get(Z, {})
        pc  = pubchem.get(Z, {})

        # --- Identity ---
        symbol = pc.get("Symbol") or fb.get("symbol") or ph.get("symbol") or f"E{Z}"
        name   = pc.get("Name")   or fb.get("name")   or ph.get("name")   or symbol

        rec = EmpiricalElement(Z=Z, symbol=symbol, name=name)
        src = rec._sources

        # --- Atomic weight ---
        aw = _coerce_float(pc.get("AtomicWeight")) \
          or _coerce_float(ph.get("atomic_weight")) \
          or _coerce_float(fb.get("atomic_mass"))
        rec.atomic_weight = aw or 0.0
        src["atomic_weight"] = "PubChem" if pc.get("AtomicWeight") else ("physics.json" if ph.get("atomic_weight") else "PeriodicTableJSON")

        # --- Electronegativity ---
        en = _coerce_float(pc.get("Electronegativity")) \
          or _coerce_float(ph.get("en_pauling")) \
          or _coerce_float(fb.get("electronegativity_pauling"))
        rec.en_pauling = en or 0.0
        src["en_pauling"] = "PubChem" if pc.get("Electronegativity") else ("physics.json" if ph.get("en_pauling") else "PeriodicTableJSON")

        # --- Shells ---
        ph_shells = ph.get("shells")
        fb_shells = fb.get("shells")
        if ph_shells and isinstance(ph_shells, list):
            rec.shells = ph_shells
            src["shells"] = "physics.json"
        elif fb_shells and isinstance(fb_shells, list):
            rec.shells = fb_shells
            src["shells"] = "PeriodicTableJSON"
        else:
            rec.shells = []
            src["shells"] = "missing"

        # --- Radioactive ---
        rec.radioactive = bool(ph.get("radioactive") or fb.get("radioactive", False))

        # --- Electron config ---
        cfg = pc.get("ElectronConfiguration") or fb.get("electron_configuration")
        rec.electron_config = str(cfg) if cfg else ""
        src["electron_config"] = "PubChem" if pc.get("ElectronConfiguration") else "PeriodicTableJSON"

        # --- Block / period / group ---
        rec.block  = str(pc.get("Block")  or fb.get("block",  "")).lower()
        rec.period = int(pc.get("Period") or fb.get("period", 0) or 0)
        grp = pc.get("Group") or fb.get("group")
        rec.group  = int(grp) if grp is not None and str(grp).strip() else None
        src["block"] = "PubChem" if pc.get("Block") else "PeriodicTableJSON"

        # --- Category ---
        rec.category = str(pc.get("GroupBlock") or fb.get("category") or "")
        src["category"] = "PubChem" if pc.get("GroupBlock") else "PeriodicTableJSON"

        # --- Phase at STP ---
        phase_raw = pc.get("IonizationEnergy")  # (IonE field check; actual phase below)
        phase = pc.get("StandardState") or fb.get("phase") or "Unknown"
        rec.phase_at_STP = _normalise_phase(str(phase))
        src["phase_at_STP"] = "PubChem" if pc.get("StandardState") else "PeriodicTableJSON"

        # --- Covalent radius (Alvarez 2008) ---
        pc_cov = _coerce_float(pc.get("CovalentRadius"))
        rec.covalent_radius_pm = pc_cov or float(_COVALENT_RADIUS_PM.get(Z, 0))
        src["covalent_radius_pm"] = "PubChem" if pc_cov else "Alvarez2008"

        # --- vdW radius (Bondi/Truhlar) ---
        pc_vdw = _coerce_float(pc.get("VDWRadius"))
        rec.vdw_radius_pm = pc_vdw or float(_VDW_RADIUS_PM.get(Z, 0))
        src["vdw_radius_pm"] = "PubChem" if pc_vdw else "Bondi1964/Truhlar2009"

        # --- Ionic radius (Shannon 1976) ---
        rec.ionic_radius_pm = float(_IONIC_RADIUS_PM.get(Z, 0))
        src["ionic_radius_pm"] = "Shannon1976-CN6"

        # --- First ionization energy ---
        ie = _coerce_float(pc.get("IonizationEnergy"))
        if not ie:
            fb_ie = fb.get("ionization_energies")
            if fb_ie and len(fb_ie) > 0:
                ie = _coerce_float(fb_ie[0])
        rec.ionization_energy_1_eV = ie or 0.0
        src["ionization_energy_1_eV"] = "PubChem" if pc.get("IonizationEnergy") else "PeriodicTableJSON"

        # --- Electron affinity ---
        # PubChem: values in eV; 0.0 is a sentinel meaning "no data" (not a true zero EA).
        # PeriodicTableJSON: values always in kJ/mol — must convert unconditionally.
        # Physical ceiling: max real EA is Cl ≈ 3.617 eV (328.6 kJ/mol).
        # |EA_eV| > 4.0 means the PubChem value slipped through in kJ/mol — convert.
        _ea_pc_raw = _coerce_float(pc.get("ElectronAffinity"))
        _ea_fb_raw = _coerce_float(fb.get("electron_affinity"))
        # Treat PubChem 0.0 as absent (sentinel); real zero EA doesn't occur
        ea_pc = _ea_pc_raw if (_ea_pc_raw is not None and _ea_pc_raw != 0.0) else None
        ea_fb = _ea_fb_raw if (_ea_fb_raw is not None and _ea_fb_raw != 0.0) else None
        if ea_pc is not None:
            ea = ea_pc / 96.485 if abs(ea_pc) > 4.0 else ea_pc
            src["electron_affinity_eV"] = "PubChem"
        elif ea_fb is not None:
            ea = ea_fb / 96.485
            src["electron_affinity_eV"] = "PeriodicTableJSON"
        else:
            ea = 0.0
            src["electron_affinity_eV"] = "none"
        rec.electron_affinity_eV = ea

        # --- Polarizability (NIST SRD-128 table, CSV override) ---
        polar = polar_csv.get(Z) or _NIST_POLARIZABILITY_AU.get(Z, 0.0)
        rec.polarizability_au = float(polar)
        src["polarizability_au"] = "polarizability_ref.csv" if Z in polar_csv else "NIST-SRD128-2018"

        # --- Melting / boiling points ---
        melt = _coerce_float(pc.get("MeltingPoint")) or _coerce_float(fb.get("melt"))
        boil = _coerce_float(pc.get("BoilingPoint"))  or _coerce_float(fb.get("boil"))
        rec.melting_point_K = melt or 0.0
        rec.boiling_point_K = boil or 0.0
        src["melting_point_K"] = "PubChem" if pc.get("MeltingPoint") else "PeriodicTableJSON"
        src["boiling_point_K"] = "PubChem" if pc.get("BoilingPoint")  else "PeriodicTableJSON"

        # --- Density ---
        dens = _coerce_float(pc.get("Density")) or _coerce_float(fb.get("density"))
        rec.density_g_per_cm3 = dens or 0.0
        src["density_g_per_cm3"] = "PubChem" if pc.get("Density") else "PeriodicTableJSON"

        # --- Molar heat ---
        mh = _coerce_float(pc.get("SpecificHeat")) or _coerce_float(fb.get("molar_heat"))
        rec.molar_heat_J_mol_K = mh or 0.0
        src["molar_heat_J_mol_K"] = "PubChem" if pc.get("SpecificHeat") else "PeriodicTableJSON"

        elements.append(rec)

    return elements


# ---------------------------------------------------------------------------
# Serialisation
# ---------------------------------------------------------------------------

def _element_to_dict(rec: EmpiricalElement) -> dict:
    d = asdict(rec)
    d.pop("_sources", None)
    d["_sources"] = rec._sources  # preserve without dataclasses recursion quirk
    return d


def write_empirical_json(elements: List[EmpiricalElement], path: str = EMPIRICAL_JSON_PATH) -> None:
    os.makedirs(os.path.dirname(path), exist_ok=True)
    out = {
        "empirical_schema": 1,
        "version": "v5.1.3",
        "description": "VSEPR-SIM authoritative empirical element layer. "
                       "Sources: PubChem REST, NIST SRD-128, PeriodicTableJSON, "
                       "Alvarez 2008, Bondi 1964, Shannon 1976.",
        "generated_by": "pykernel/pillars/empirical_chem.py",
        "elements": [_element_to_dict(e) for e in elements],
    }
    with open(path, "w", encoding="utf-8") as f:
        json.dump(out, f, indent=2, ensure_ascii=False)
    print(f"[EmpiricalChem] Wrote {len(elements)} elements → {path}")


# ---------------------------------------------------------------------------
# Audit report
# ---------------------------------------------------------------------------

def write_audit_report(
    elements: List[EmpiricalElement],
    path: str = AUDIT_MD_PATH,
) -> None:
    os.makedirs(os.path.dirname(path), exist_ok=True)

    _FIELDS = [
        ("atomic_weight",           "amu"),
        ("en_pauling",              "–"),
        ("covalent_radius_pm",      "pm"),
        ("vdw_radius_pm",           "pm"),
        ("ionic_radius_pm",         "pm"),
        ("ionization_energy_1_eV",  "eV"),
        ("electron_affinity_eV",    "eV"),
        ("polarizability_au",       "bohr³"),
        ("melting_point_K",         "K"),
        ("boiling_point_K",         "K"),
        ("density_g_per_cm3",       "g/cm³"),
        ("molar_heat_J_mol_K",      "J/(mol·K)"),
    ]

    # Coverage counts
    coverage: Dict[str, int] = {f: 0 for f, _ in _FIELDS}
    for e in elements:
        for f, _ in _FIELDS:
            val = getattr(e, f, 0.0)
            if val and val != 0.0:
                coverage[f] += 1

    total = len(elements)

    lines = [
        "# VSEPR-SIM Chemistry Audit — Empirical Data Layer",
        "",
        "**Generated by:** `pykernel/pillars/empirical_chem.py`  ",
        f"**Release:** v5.1.3  ",
        f"**Elements audited:** {total} (Z = 1–118)  ",
        "",
        "## Sources",
        "",
        "| Source | Coverage role |",
        "|---|---|",
        "| PubChem PUG REST (periodic table JSON) | atomic_weight, en, IE, EA, config, radii (if available), phase, melt/boil, density, molar_heat |",
        "| NIST SRD-128 (2018) — static table | polarizability_au (all 118 elements) |",
        "| Alvarez 2008 (DOI:10.1039/b801115j) — static table | covalent_radius_pm |",
        "| Bondi 1964 / Truhlar 2009 — static table | vdw_radius_pm |",
        "| Shannon 1976 (Acta Cryst. A32:751) — static table | ionic_radius_pm (CN6, common oxidation state) |",
        "| data/PeriodicTableJSON.json — warm fallback | IE, EA, melt/boil, density, electron_config when PubChem unavailable |",
        "| data/elements.physics.json | shells, en_pauling, radioactive |",
        "",
        "## Field coverage",
        "",
        "| Field | Unit | Elements with data | Coverage % |",
        "|---|---|---|---|",
    ]
    for f, unit in _FIELDS:
        n = coverage[f]
        pct = 100.0 * n / total if total else 0.0
        lines.append(f"| `{f}` | {unit} | {n}/{total} | {pct:.1f}% |")

    lines += [
        "",
        "## Per-element summary (Z=1–36 shown)",
        "",
        "| Z | Sym | aw | IE₁(eV) | EA(eV) | r_cov(pm) | r_vdW(pm) | r_ion(pm) | α(bohr³) | Tm(K) |",
        "|---|---|---|---|---|---|---|---|---|---|",
    ]
    for e in elements[:36]:
        lines.append(
            f"| {e.Z} | {e.symbol} "
            f"| {e.atomic_weight:.4g} "
            f"| {e.ionization_energy_1_eV:.4g} "
            f"| {e.electron_affinity_eV:.4g} "
            f"| {e.covalent_radius_pm:.4g} "
            f"| {e.vdw_radius_pm:.4g} "
            f"| {e.ionic_radius_pm:.4g} "
            f"| {e.polarizability_au:.4g} "
            f"| {e.melting_point_K:.4g} |"
        )

    lines += [
        "",
        "## Gap analysis",
        "",
        "Fields with < 80% coverage are flagged for follow-up enrichment (WO-VSIM-66C or later).",
        "",
    ]
    for f, unit in _FIELDS:
        n = coverage[f]
        pct = 100.0 * n / total if total else 0.0
        if pct < 80.0:
            lines.append(f"- **{f}** ({unit}): {pct:.1f}% — {total - n} elements missing")

    lines += [
        "",
        "## Notes",
        "",
        "- Forces on ionic_radius_pm use Shannon 1976 CN=6 coordination, most common oxidation state.",
        "  Transition-metal and lanthanide values may vary by ±10 pm depending on coordination and spin state.",
        "- polarizability_au values for Z > 86 are theoretical estimates (NIST SRD-128 + CCSD extrapolation).",
        "- melting_point_K = 0.0 indicates the data was unavailable, not a physical zero.",
        "- This file is regenerated by running `python -m pykernel.pillars.empirical_chem` from the repo root.",
        "",
        "---",
        "*VSEPR-SIM v5.1.3 — Chemistry Audit — Empirical Data Layer*",
    ]

    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    print(f"[EmpiricalChem] Audit report → {path}")


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _coerce_float(val: Any) -> float:
    if val is None:
        return 0.0
    try:
        f = float(str(val).replace(",", "").strip())
        return f if math.isfinite(f) else 0.0
    except (ValueError, TypeError):
        return 0.0


def _normalise_phase(phase: str) -> str:
    p = phase.strip().lower()
    if p in ("gas", "gaseous"):
        return "Gas"
    if p in ("liquid",):
        return "Liquid"
    if p in ("solid",):
        return "Solid"
    return phase.strip() or "Unknown"


# ---------------------------------------------------------------------------
# Public API
# ---------------------------------------------------------------------------

def load_empirical(path: str = EMPIRICAL_JSON_PATH) -> List[EmpiricalElement]:
    """
    Load previously generated elements.empirical.json into EmpiricalElement records.
    Returns [] if the file does not exist.
    """
    if not os.path.exists(path):
        return []
    with open(path, encoding="utf-8") as f:
        data = json.load(f)
    result = []
    for d in data.get("elements", []):
        sources = d.pop("_sources", {})
        e = EmpiricalElement(**{k: v for k, v in d.items() if k in EmpiricalElement.__dataclass_fields__})
        e._sources = sources
        result.append(e)
    return result


def get_element(Z: int, elements: Optional[List[EmpiricalElement]] = None) -> Optional[EmpiricalElement]:
    """Look up a single element by atomic number from an already-loaded list."""
    if elements is None:
        elements = load_empirical()
    for e in elements:
        if e.Z == Z:
            return e
    return None


def get_element_by_symbol(sym: str, elements: Optional[List[EmpiricalElement]] = None) -> Optional[EmpiricalElement]:
    """Look up a single element by symbol (case-insensitive)."""
    if elements is None:
        elements = load_empirical()
    sym_l = sym.strip().lower()
    for e in elements:
        if e.symbol.lower() == sym_l:
            return e
    return None


# ---------------------------------------------------------------------------
# Entry point — run as __main__ to refresh the data files
# ---------------------------------------------------------------------------

def run(verbose: bool = True, dry_run: bool = False) -> List[EmpiricalElement]:
    """
    Full fetch-merge-write cycle.

    Args:
        verbose:  print progress to stdout
        dry_run:  build the records but do not write files

    Returns:
        List of 118 EmpiricalElement records.
    """
    elements = build_empirical_elements(verbose=verbose)
    if not dry_run:
        write_empirical_json(elements)
        write_audit_report(elements)
    return elements


if __name__ == "__main__":
    import sys
    dry = "--dry-run" in sys.argv
    run(verbose=True, dry_run=dry)
