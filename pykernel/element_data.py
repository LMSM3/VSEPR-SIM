"""
element_data.py — Authoritative element property access for Python tools.

Parses scientific reference data directly from the C++ kernel source files:
  - CPK colors:       src/vis/renderer_base.cpp   (Jmol reference, 119 elements)
  - Covalent radii:   src/pot/covalent_radii.hpp   (Pyykkö & Atsumi 2009)
  - VdW radii:        src/pot/vdw_radii.hpp        (Bondi 1964 / Alvarez 2013)
  - Atomic masses:    src/pot/atomic_masses.hpp     (IUPAC 2021 standard)
  - Element metadata: data/elements.vsepr.json     (names, electronegativity, shells)

NO scientific data is stored in this file.  All values are read at import time
from the single-source-of-truth headers in the kernel.  If the kernel data
changes, every Python tool automatically picks up the new values.

The ELEMENT_ARCHIVE dictionary merges all kernel data pipes and JSON metadata
for Z=1–110 (H through Ds) into a single inspectable record per element.
Elements Z=111–118 are excluded from the archive because their properties
are almost entirely estimated.

Anti-black-box: every number is traceable to a specific line in a specific
kernel file or JSON source.
"""

import json
import os
import re
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, Tuple, Optional, List

# ============================================================================
# Locate project root (walk up from this file until we find CMakeLists.txt)
# ============================================================================

def _find_project_root() -> Path:
    """Walk upward from this file to find the project root."""
    d = Path(__file__).resolve().parent
    for _ in range(10):
        if (d / "CMakeLists.txt").exists() and (d / "src").is_dir():
            return d
        d = d.parent
    raise FileNotFoundError(
        "element_data: cannot locate project root (no CMakeLists.txt found "
        "above pykernel/)"
    )

_ROOT = _find_project_root()

# ============================================================================
# Symbol ↔ Atomic Number mapping  (Z = 1–118, IUPAC standard)
# ============================================================================
#
# This is the universal periodic-table ordering.  It is NOT a scientific
# measurement — it is a naming convention maintained by IUPAC and will not
# change.  Embedding it here avoids a runtime file dependency for a static
# mapping that every chemistry tool needs.
#

SYMBOLS: List[str] = [
    "",    # Z=0 placeholder
    "H",  "He",
    "Li", "Be", "B",  "C",  "N",  "O",  "F",  "Ne",
    "Na", "Mg", "Al", "Si", "P",  "S",  "Cl", "Ar",
    "K",  "Ca",
    "Sc", "Ti", "V",  "Cr", "Mn", "Fe", "Co", "Ni", "Cu", "Zn",
    "Ga", "Ge", "As", "Se", "Br", "Kr",
    "Rb", "Sr",
    "Y",  "Zr", "Nb", "Mo", "Tc", "Ru", "Rh", "Pd", "Ag", "Cd",
    "In", "Sn", "Sb", "Te", "I",  "Xe",
    "Cs", "Ba",
    "La", "Ce", "Pr", "Nd", "Pm", "Sm", "Eu", "Gd",
    "Tb", "Dy", "Ho", "Er", "Tm", "Yb", "Lu",
    "Hf", "Ta", "W",  "Re", "Os", "Ir", "Pt", "Au", "Hg",
    "Tl", "Pb", "Bi", "Po", "At", "Rn",
    "Fr", "Ra",
    "Ac", "Th", "Pa", "U",  "Np", "Pu", "Am", "Cm",
    "Bk", "Cf", "Es", "Fm", "Md", "No", "Lr",
    "Rf", "Db", "Sg", "Bh", "Hs", "Mt", "Ds", "Rg", "Cn",
    "Nh", "Fl", "Mc", "Lv", "Ts", "Og",
]

SYMBOL_TO_Z: Dict[str, int] = {s: z for z, s in enumerate(SYMBOLS) if s}

# ============================================================================
# Kernel file parsers
# ============================================================================

def _strip_cpp_comments(text: str) -> str:
    """Remove C and C++ style comments from source text."""
    # Remove block comments /* ... */
    text = re.sub(r'/\*.*?\*/', '', text, flags=re.DOTALL)
    # Remove line comments // ...
    text = re.sub(r'//[^\n]*', '', text)
    return text


def _parse_constexpr_array(filepath: Path, expected_size: int = 119) -> List[float]:
    """
    Parse a C++ constexpr std::array<double, N> = { ... }; block.

    Returns a list of floats indexed 0..N-1.
    """
    text = filepath.read_text(encoding="utf-8")
    text = _strip_cpp_comments(text)

    # Match the array initializer: everything between the first { and };
    m = re.search(r'=\s*\{([^;]+)\};', text, re.DOTALL)
    if not m:
        raise ValueError(f"element_data: no array literal found in {filepath}")
    body = m.group(1)
    values = []
    for token in re.findall(r'[-+]?\d*\.?\d+', body):
        values.append(float(token))
    if len(values) < expected_size:
        raise ValueError(
            f"element_data: expected {expected_size} values in {filepath}, "
            f"got {len(values)}"
        )
    return values[:expected_size]


def _parse_cpk_colors(filepath: Path, expected_size: int = 119) -> List[Tuple[float, float, float]]:
    """
    Parse the CPK color table from renderer_base.cpp.

    The table is a static vector<array<float,3>> initializer list.
    Returns a list of (R, G, B) tuples indexed 0..118.
    """
    text = filepath.read_text(encoding="utf-8")

    # Find the color table initializer (nested braces)
    # Look for the static const vector initialization
    m = re.search(
        r'static\s+const\s+std::vector.*?colors\s*=\s*\{(.+?)\};\s*\n\s*return',
        text, re.DOTALL
    )
    if not m:
        raise ValueError(f"element_data: no CPK color table found in {filepath}")
    body = m.group(1)

    # Extract each {r, g, b} triplet
    colors = []
    for triplet in re.finditer(r'\{\s*([-+]?\d*\.?\d+)f?\s*,\s*([-+]?\d*\.?\d+)f?\s*,\s*([-+]?\d*\.?\d+)f?\s*\}', body):
        r = float(triplet.group(1))
        g = float(triplet.group(2))
        b = float(triplet.group(3))
        colors.append((r, g, b))

    if len(colors) < expected_size:
        raise ValueError(
            f"element_data: expected {expected_size} CPK entries in {filepath}, "
            f"got {len(colors)}"
        )
    return colors[:expected_size]


# ============================================================================
# Load data from kernel source files
# ============================================================================

_COV_PATH  = _ROOT / "src" / "pot" / "covalent_radii.hpp"
_VDW_PATH  = _ROOT / "src" / "pot" / "vdw_radii.hpp"
_MASS_PATH = _ROOT / "src" / "pot" / "atomic_masses.hpp"
_CPK_PATH  = _ROOT / "src" / "vis" / "renderer_base.cpp"

# Indexed by atomic number Z (0..118)
COVALENT_RADII: List[float] = _parse_constexpr_array(_COV_PATH)
VDW_RADII:      List[float] = _parse_constexpr_array(_VDW_PATH)
ATOMIC_MASSES:  List[float] = _parse_constexpr_array(_MASS_PATH)
CPK_COLORS: List[Tuple[float, float, float]] = _parse_cpk_colors(_CPK_PATH)

# Archive boundary: Z=1 through Z=110 (H through Ds)
# The last 8 elements (Rg–Og, Z=111–118) are excluded — property data is
# almost entirely estimated for superheavy elements beyond Darmstadtium.
ARCHIVE_Z_MAX = 110


# ============================================================================
# JSON metadata parser — names, electronegativity, electron shells
# ============================================================================

_JSON_PATH = _ROOT / "data" / "elements.vsepr.json"


def _parse_elements_json(filepath: Path) -> Dict[int, dict]:
    """
    Parse data/elements.vsepr.json into a dict keyed by atomic number Z.

    Each entry is a raw dict with keys: Z, symbol, name, atomic_weight,
    en_pauling (may be None), shells, cpk (hex string or None), radioactive.
    """
    text = filepath.read_text(encoding="utf-8")
    data = json.loads(text)
    elements = data.get("elements", [])
    by_z: Dict[int, dict] = {}
    for elem in elements:
        z = elem.get("Z", 0)
        if 1 <= z <= 118:
            by_z[z] = elem
    return by_z


_JSON_ELEMENTS: Dict[int, dict] = _parse_elements_json(_JSON_PATH)


# ============================================================================
# ElementRecord — unified record for the centralized archive
# ============================================================================

@dataclass(frozen=True)
class ElementRecord:
    """Immutable, inspectable record combining all data pipes for one element.

    Sources:
      - Z, symbol:          IUPAC periodic table ordering
      - name:               data/elements.vsepr.json
      - atomic_mass:        src/pot/atomic_masses.hpp  (IUPAC 2021)
      - covalent_radius:    src/pot/covalent_radii.hpp (Pyykkö & Atsumi 2009)
      - vdw_radius:         src/pot/vdw_radii.hpp      (Bondi 1964 / Alvarez 2013)
      - cpk_color:          src/vis/renderer_base.cpp  (Jmol reference)
      - electronegativity:  data/elements.vsepr.json   (Pauling scale, may be None)
      - electron_shells:    data/elements.vsepr.json   (IUPAC shell occupancies)
      - radioactive:        data/elements.vsepr.json
    """
    Z: int
    symbol: str
    name: str
    atomic_mass: float
    covalent_radius: float
    vdw_radius: float
    cpk_color: Tuple[float, float, float]
    electronegativity: Optional[float]
    electron_shells: Tuple[int, ...]
    radioactive: bool


def _build_archive() -> Dict[int, ElementRecord]:
    """Construct the centralized element archive for Z=1–ARCHIVE_Z_MAX.

    Merges data from all kernel source files and JSON metadata into a single
    ElementRecord per atomic number.  This function runs once at import time.
    """
    archive: Dict[int, ElementRecord] = {}
    for Z in range(1, ARCHIVE_Z_MAX + 1):
        sym = SYMBOLS[Z]
        json_entry = _JSON_ELEMENTS.get(Z, {})
        name = json_entry.get("name", sym)
        en = json_entry.get("en_pauling", None)
        shells = tuple(json_entry.get("shells", []))
        radio = json_entry.get("radioactive", False)

        archive[Z] = ElementRecord(
            Z=Z,
            symbol=sym,
            name=name,
            atomic_mass=ATOMIC_MASSES[Z],
            covalent_radius=COVALENT_RADII[Z],
            vdw_radius=VDW_RADII[Z],
            cpk_color=CPK_COLORS[Z],
            electronegativity=float(en) if en is not None else None,
            electron_shells=shells,
            radioactive=bool(radio),
        )
    return archive


ELEMENT_ARCHIVE: Dict[int, ElementRecord] = _build_archive()

# Convenience: lookup by symbol
ARCHIVE_BY_SYMBOL: Dict[str, ElementRecord] = {
    rec.symbol: rec for rec in ELEMENT_ARCHIVE.values()
}


def get_element(key) -> Optional[ElementRecord]:
    """Look up an archived element by symbol (str) or atomic number (int).

    Returns None if the element is outside the archive (Z>110) or unknown.
    """
    if isinstance(key, int):
        return ELEMENT_ARCHIVE.get(key)
    if isinstance(key, str):
        return ARCHIVE_BY_SYMBOL.get(key)


# ============================================================================
# Public lookup API — by element symbol
# ============================================================================

DEFAULT_CPK_COLOR = (1.0, 0.08, 0.58)   # Magenta (unknown), matches Z=0 in kernel
DEFAULT_COV_RADIUS = 0.80
DEFAULT_VDW_RADIUS = 2.00
DEFAULT_ATOMIC_MASS = 0.0


def cpk_color(symbol: str) -> Tuple[float, float, float]:
    """CPK color for an element symbol.  Falls back to magenta for unknowns."""
    Z = SYMBOL_TO_Z.get(symbol, 0)
    if Z < len(CPK_COLORS):
        return CPK_COLORS[Z]
    return DEFAULT_CPK_COLOR


def covalent_radius(symbol: str) -> float:
    """Covalent radius (Å) for an element symbol."""
    Z = SYMBOL_TO_Z.get(symbol, 0)
    if Z < len(COVALENT_RADII) and COVALENT_RADII[Z] > 0.0:
        return COVALENT_RADII[Z]
    return DEFAULT_COV_RADIUS


def vdw_radius(symbol: str) -> float:
    """Van der Waals radius (Å) for an element symbol."""
    Z = SYMBOL_TO_Z.get(symbol, 0)
    if Z < len(VDW_RADII) and VDW_RADII[Z] > 0.0:
        return VDW_RADII[Z]
    return DEFAULT_VDW_RADIUS


def atomic_mass(symbol: str) -> float:
    """IUPAC standard atomic weight (Da) for an element symbol."""
    Z = SYMBOL_TO_Z.get(symbol, 0)
    if Z < len(ATOMIC_MASSES) and ATOMIC_MASSES[Z] > 0.0:
        return ATOMIC_MASSES[Z]
    return DEFAULT_ATOMIC_MASS


def is_archived(symbol: str) -> bool:
    """True if the element is within the centralized archive (Z=1–110)."""
    Z = SYMBOL_TO_Z.get(symbol, 0)
    return 1 <= Z <= ARCHIVE_Z_MAX


# ============================================================================
# XYZ file loader
# ============================================================================

def load_xyz(path: str) -> Tuple[str, List[Tuple[str, float, float, float]]]:
    """
    Read an XYZ file.

    Returns (comment, [(symbol, x, y, z), ...]).
    """
    atoms = []
    with open(path) as f:
        n = int(f.readline().strip())
        comment = f.readline().strip()
        for _ in range(n):
            parts = f.readline().split()
            atoms.append((parts[0], float(parts[1]), float(parts[2]), float(parts[3])))
    return comment, atoms


def molecules_dir() -> Path:
    """Return the path to the examples/molecules/ directory."""
    return _ROOT / "examples" / "molecules"
