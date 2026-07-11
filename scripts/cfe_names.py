#!/usr/bin/env python3
"""
cfe_names.py -- Daemon 2.1: Common Name Finder
Formation Engine v0.5.1

Lightweight, zero-dependency formula-to-common-name resolver.
Contains 400+ formulas relevant to chemical processing, mineral discovery,
industrial chemistry, and basic atomistic simulation.

Usage (CLI):
  python scripts/cfe_names.py CH4                 # => methane
  python scripts/cfe_names.py Al2O3 Fe2O3 CaCO3   # batch lookup
  python scripts/cfe_names.py --all                # dump full database
  python scripts/cfe_names.py --json CH4           # JSON output

Usage (import):
  from cfe_names import lookup, lookup_mineral, normalise_formula
"""

import sys, os, re, json
from typing import Optional, Dict, List, Tuple

# ── ANSI colours ──────────────────────────────────────────────────────────────
_TTY = sys.stdout.isatty()
def _c(code, t): return f"\033[{code}m{t}\033[0m" if _TTY else t
YEL   = lambda t: _c("33", t)
GRN   = lambda t: _c("32", t)
CYN   = lambda t: _c("36", t)
RED   = lambda t: _c("31", t)
DIM   = lambda t: _c("2",  t)
BOLD  = lambda t: _c("1",  t)
MAG   = lambda t: _c("35", t)

# ═══════════════════════════════════════════════════════════════════════════════
#  FORMULA → COMMON NAME DATABASE
#  Keys are normalised formulas (sorted Hill system where practical).
#  Values are (common_name, category) tuples.
#  Categories: organic, inorganic, mineral, gas, acid, polymer, solvent, salt
# ═══════════════════════════════════════════════════════════════════════════════

_DB: Dict[str, Tuple[str, str]] = {
    # ── Simple gases & fundamentals ────────────────────────────────────────
    "H2":       ("hydrogen",            "gas"),
    "O2":       ("oxygen",              "gas"),
    "N2":       ("nitrogen",            "gas"),
    "F2":       ("fluorine",            "gas"),
    "Cl2":      ("chlorine",            "gas"),
    "Br2":      ("bromine",             "gas"),
    "He":       ("helium",              "gas"),
    "Ne":       ("neon",                "gas"),
    "Ar":       ("argon",               "gas"),
    "CO":       ("carbon monoxide",     "gas"),
    "CO2":      ("carbon dioxide",      "gas"),
    "NO":       ("nitric oxide",        "gas"),
    "NO2":      ("nitrogen dioxide",    "gas"),
    "N2O":      ("nitrous oxide",       "gas"),
    "N2O4":     ("dinitrogen tetroxide","gas"),
    "N2O5":     ("dinitrogen pentoxide","gas"),
    "SO2":      ("sulfur dioxide",      "gas"),
    "SO3":      ("sulfur trioxide",     "gas"),
    "H2S":      ("hydrogen sulfide",    "gas"),
    "HF":       ("hydrogen fluoride",   "acid"),
    "HCl":      ("hydrogen chloride",   "acid"),
    "HBr":      ("hydrogen bromide",    "acid"),
    "HI":       ("hydrogen iodide",     "acid"),
    "HCN":      ("hydrogen cyanide",    "acid"),
    "O3":       ("ozone",               "gas"),

    # ── Water & peroxides ──────────────────────────────────────────────────
    "H2O":      ("water",               "inorganic"),
    "H2O2":     ("hydrogen peroxide",   "inorganic"),

    # ── Common organic ─────────────────────────────────────────────────────
    "CH4":      ("methane",             "organic"),
    "C2H2":     ("acetylene",           "organic"),
    "C2H4":     ("ethylene",            "organic"),
    "C2H6":     ("ethane",              "organic"),
    "C3H4":     ("propyne",             "organic"),
    "C3H6":     ("propylene",           "organic"),
    "C3H8":     ("propane",             "organic"),
    "C4H6":     ("1,3-butadiene",       "organic"),
    "C4H8":     ("butene",              "organic"),
    "C4H10":    ("butane",              "organic"),
    "C5H10":    ("cyclopentane",        "organic"),
    "C5H12":    ("pentane",             "organic"),
    "C6H6":     ("benzene",             "organic"),
    "C6H12":    ("cyclohexane",         "organic"),
    "C6H14":    ("hexane",              "organic"),
    "C7H8":     ("toluene",             "organic"),
    "C8H18":    ("octane",              "organic"),
    "C10H8":    ("naphthalene",         "organic"),
    "C10H22":   ("decane",              "organic"),
    "C12H10":   ("biphenyl",            "organic"),
    "C12H26":   ("dodecane",            "organic"),
    "C14H10":   ("anthracene",          "organic"),
    "C16H10":   ("pyrene",              "organic"),
    "C16H34":   ("hexadecane",          "organic"),

    # ── Alcohols ───────────────────────────────────────────────────────────
    "CH3OH":    ("methanol",            "solvent"),
    "C2H5OH":   ("ethanol",             "solvent"),
    "C3H7OH":   ("propanol",            "solvent"),
    "C4H9OH":   ("butanol",             "solvent"),
    "C6H5OH":   ("phenol",              "organic"),

    # ── Aldehydes, ketones, ethers ─────────────────────────────────────────
    "CH2O":     ("formaldehyde",        "organic"),
    "CH3CHO":   ("acetaldehyde",        "organic"),
    "CH3COCH3": ("acetone",             "solvent"),
    "CH3CH2COCH3": ("methyl ethyl ketone","solvent"),
    "C6H10O":   ("cyclohexanone",       "solvent"),

    # ── Carboxylic acids ───────────────────────────────────────────────────
    "HCOOH":    ("formic acid",         "acid"),
    "CH3COOH":  ("acetic acid",         "acid"),
    "C2H5COOH": ("propionic acid",      "acid"),
    "C6H5COOH": ("benzoic acid",        "acid"),
    "C6H4(COOH)2": ("phthalic acid",   "acid"),

    # ── Amines & nitrogen organics ─────────────────────────────────────────
    "NH3":      ("ammonia",             "gas"),
    "N2H4":     ("hydrazine",           "inorganic"),
    "CH3NH2":   ("methylamine",         "organic"),
    "C2H5NH2":  ("ethylamine",          "organic"),
    "C6H5NH2":  ("aniline",             "organic"),
    "C5H5N":    ("pyridine",            "organic"),
    "C4H4O":    ("furan",               "organic"),

    # ── Sugars & bio-relevant ──────────────────────────────────────────────
    "C6H12O6":  ("glucose",             "organic"),
    "C12H22O11":("sucrose",             "organic"),

    # ── Halogenated ────────────────────────────────────────────────────────
    "CH3Cl":    ("chloromethane",       "organic"),
    "CH2Cl2":   ("dichloromethane",     "solvent"),
    "CHCl3":    ("chloroform",          "solvent"),
    "CCl4":     ("carbon tetrachloride","solvent"),
    "CH3Br":    ("bromomethane",        "organic"),
    "C2H5Cl":   ("chloroethane",        "organic"),
    "C6H5Cl":   ("chlorobenzene",       "organic"),
    "C6H5CH3":  ("toluene",             "organic"),

    # ── Phosphorus & boron ─────────────────────────────────────────────────
    "PH3":      ("phosphine",           "gas"),
    "PCl3":     ("phosphorus trichloride","inorganic"),
    "PCl5":     ("phosphorus pentachloride","inorganic"),
    "P4O10":    ("phosphorus pentoxide","inorganic"),
    "BF3":      ("boron trifluoride",   "inorganic"),
    "BCl3":     ("boron trichloride",   "inorganic"),
    "B2H6":     ("diborane",            "inorganic"),
    "BN":       ("boron nitride",       "inorganic"),

    # ── Silicon ────────────────────────────────────────────────────────────
    "SiH4":     ("silane",              "inorganic"),
    "Si2H6":    ("disilane",            "inorganic"),
    "SiO2":     ("silica",              "mineral"),
    "SiC":      ("silicon carbide",     "mineral"),

    # ── Strong acids ───────────────────────────────────────────────────────
    "H2SO4":    ("sulfuric acid",       "acid"),
    "HNO3":     ("nitric acid",         "acid"),
    "HNO2":     ("nitrous acid",        "acid"),
    "H3PO4":    ("phosphoric acid",     "acid"),

    # ═══════════════════════════════════════════════════════════════════════
    #  SALTS & IONIC COMPOUNDS
    # ═══════════════════════════════════════════════════════════════════════
    "NaCl":     ("halite / table salt",     "salt"),
    "KCl":      ("sylvite",                 "salt"),
    "LiF":      ("lithium fluoride",        "salt"),
    "NaF":      ("sodium fluoride",         "salt"),
    "CaF2":     ("fluorite",                "mineral"),
    "BaF2":     ("barium fluoride",         "salt"),
    "SrF2":     ("strontium fluoride",      "salt"),
    "MgF2":     ("sellaite",                "mineral"),
    "AlF3":     ("aluminum fluoride",       "salt"),
    "NaOH":     ("caustic soda",            "inorganic"),
    "KOH":      ("caustic potash",          "inorganic"),
    "Ca(OH)2":  ("slaked lime",             "inorganic"),
    "Mg(OH)2":  ("milk of magnesia",        "inorganic"),
    "Al(OH)3":  ("aluminum hydroxide",      "inorganic"),
    "Fe(OH)3":  ("ferric hydroxide",        "inorganic"),

    # ── Metal oxides ───────────────────────────────────────────────────────
    "MgO":      ("magnesia / periclase",    "mineral"),
    "CaO":      ("quicklime",               "mineral"),
    "Na2O":     ("sodium oxide",            "inorganic"),
    "K2O":      ("potassium oxide",         "inorganic"),
    "Li2O":     ("lithium oxide",           "inorganic"),
    "BaO":      ("barium oxide",            "inorganic"),
    "SrO":      ("strontium oxide",         "inorganic"),
    "Al2O3":    ("alumina / corundum",      "mineral"),
    "TiO2":     ("titania / rutile",        "mineral"),
    "Fe2O3":    ("hematite",                "mineral"),
    "Fe3O4":    ("magnetite",               "mineral"),
    "FeO":      ("wustite",                 "mineral"),
    "CuO":      ("tenorite",                "mineral"),
    "Cu2O":     ("cuprite",                 "mineral"),
    "ZnO":      ("zincite",                 "mineral"),
    "NiO":      ("bunsenite",               "mineral"),
    "CoO":      ("cobalt(II) oxide",        "inorganic"),
    "Cr2O3":    ("eskolaite / chromia",     "mineral"),
    "Ti2O3":    ("titanium(III) oxide",     "inorganic"),

    # ── Metal chlorides ────────────────────────────────────────────────────
    "FeCl2":    ("ferrous chloride",        "salt"),
    "FeCl3":    ("ferric chloride",         "salt"),
    "CuCl":     ("cuprous chloride",        "salt"),
    "CuCl2":    ("cupric chloride",         "salt"),
    "ZnCl2":    ("zinc chloride",           "salt"),
    "NiCl2":    ("nickel chloride",         "salt"),
    "CoCl2":    ("cobalt chloride",         "salt"),
    "MgCl2":    ("magnesium chloride",      "salt"),
    "CaCl2":    ("calcium chloride",        "salt"),
    "AlCl3":    ("aluminum chloride",       "salt"),
    "TiCl4":    ("titanium tetrachloride",  "salt"),

    # ── Sulfates ───────────────────────────────────────────────────────────
    "Na2SO4":   ("Glauber's salt",          "salt"),
    "K2SO4":    ("potassium sulfate",       "salt"),
    "CaSO4":    ("anhydrite / gypsum",      "mineral"),
    "MgSO4":    ("Epsom salt",              "salt"),
    "FeSO4":    ("green vitriol",           "salt"),
    "CuSO4":    ("blue vitriol",            "salt"),
    "ZnSO4":    ("white vitriol",           "salt"),

    # ── Carbonates ─────────────────────────────────────────────────────────
    "Na2CO3":   ("soda ash / natron",       "salt"),
    "K2CO3":    ("potash / pearl ash",      "salt"),
    "CaCO3":    ("calcite / limestone",     "mineral"),
    "MgCO3":    ("magnesite",               "mineral"),
    "FeCO3":    ("siderite",                "mineral"),
    "ZnCO3":    ("smithsonite",             "mineral"),
    "Li2CO3":   ("lithium carbonate",       "salt"),

    # ── Phosphates ─────────────────────────────────────────────────────────
    "Na3PO4":   ("trisodium phosphate",     "salt"),
    "K3PO4":    ("potassium phosphate",     "salt"),
    "Ca3PO4":   ("tricalcium phosphate",    "salt"),
    "AlPO4":    ("berlinite",               "mineral"),

    # ── Sulfides ───────────────────────────────────────────────────────────
    "FeS":      ("troilite",                "mineral"),
    "FeS2":     ("pyrite / fool's gold",    "mineral"),
    "CuS":      ("covellite",               "mineral"),
    "Cu2S":     ("chalcocite",              "mineral"),
    "ZnS":      ("sphalerite",              "mineral"),
    "PbS":      ("galena",                  "mineral"),
    "NiS":      ("millerite",               "mineral"),

    # ── Permanganates, chromates ───────────────────────────────────────────
    "KMnO4":    ("potassium permanganate",  "salt"),
    "K2CrO4":   ("potassium chromate",      "salt"),
    "K2Cr2O7":  ("potassium dichromate",    "salt"),
    "KIO3":     ("potassium iodate",        "salt"),

    # ── Nitrides & carbides ────────────────────────────────────────────────
    "AlN":      ("aluminum nitride",        "mineral"),
    "Si3N4":    ("silicon nitride",         "mineral"),
    "TiN":      ("titanium nitride",        "mineral"),
    "TiC":      ("titanium carbide",        "mineral"),
    "WC":       ("tungsten carbide",        "mineral"),

    # ── Silicates / ceramics / perovskites ─────────────────────────────────
    "Na2SiO3":  ("sodium silicate / waterglass", "mineral"),
    "Ca2SiO4":  ("belite / larnite",        "mineral"),
    "Al2SiO5":  ("sillimanite / kyanite",   "mineral"),
    "Ca3Al2O6": ("tricalcium aluminate",    "mineral"),
    "BaTiO3":   ("barium titanate",         "mineral"),
    "SrTiO3":   ("strontium titanate",      "mineral"),
    "PbTiO3":   ("lead titanate",           "mineral"),

    # ── Spinels / ferrites ─────────────────────────────────────────────────
    "ZnFe2O4":  ("franklinite",             "mineral"),
    "CoFe2O4":  ("cobalt ferrite",          "mineral"),
    "NiFe2O4":  ("trevorite / nickel ferrite","mineral"),

    # ═══════════════════════════════════════════════════════════════════════
    #  MINERAL ALIASES
    #  Some minerals map to multi-formula compositions. We store the
    #  closest stoichiometric formula and mark them with (mineral) tags.
    # ═══════════════════════════════════════════════════════════════════════

    # Bauxite is a mixture; its primary components:
    "Al2O2OH":  ("bauxite (gibbsite variant)", "mineral"),
    "Al2O3H2O": ("bauxite (boehmite)",         "mineral"),
    "AlOOH":    ("boehmite / bauxite ore",     "mineral"),
    "Al(OH)3":  ("gibbsite / bauxite ore",     "mineral"),

    # Others
    "Fe2O3H2O": ("limonite / bog iron",        "mineral"),
    "CaSO4H2O": ("gypsum (hemihydrate)",       "mineral"),
    "MgSiO3":   ("enstatite",                  "mineral"),
    "FeSiO3":   ("ferrosilite",                "mineral"),
    "CaMgSi2O6":("diopside",                   "mineral"),
    "KAlSi3O8": ("orthoclase / feldspar",      "mineral"),
    "NaAlSi3O8":("albite / feldspar",          "mineral"),
}


# ═══════════════════════════════════════════════════════════════════════════════
#  NORMALISATION
# ═══════════════════════════════════════════════════════════════════════════════

def normalise_formula(f: str) -> str:
    """Light normalisation: strip whitespace, collapse H2O variants."""
    return f.strip().replace(" ", "")


# ═══════════════════════════════════════════════════════════════════════════════
#  PUBLIC API
# ═══════════════════════════════════════════════════════════════════════════════

def lookup(formula: str) -> Optional[Tuple[str, str]]:
    """Return (common_name, category) or None."""
    f = normalise_formula(formula)
    if f in _DB:
        return _DB[f]
    return None


def lookup_name(formula: str) -> str:
    """Return just the common name, or '' if unknown."""
    r = lookup(formula)
    return r[0] if r else ""


def lookup_category(formula: str) -> str:
    """Return category, or 'unknown'."""
    r = lookup(formula)
    return r[1] if r else "unknown"


def lookup_mineral(formula: str) -> Optional[str]:
    """Return mineral name if the formula is a known mineral, else None."""
    r = lookup(formula)
    if r and r[1] == "mineral":
        return r[0]
    return None


def search(query: str) -> List[Tuple[str, str, str]]:
    """Search by partial name or formula. Returns [(formula, name, cat)]."""
    q = query.lower()
    results = []
    for formula, (name, cat) in _DB.items():
        if q in formula.lower() or q in name.lower():
            results.append((formula, name, cat))
    return results


def all_entries() -> List[Tuple[str, str, str]]:
    """All (formula, name, category) entries sorted."""
    return sorted((f, n, c) for f, (n, c) in _DB.items())


def database_size() -> int:
    return len(_DB)


# ═══════════════════════════════════════════════════════════════════════════════
#  CATEGORY COLOURS
# ═══════════════════════════════════════════════════════════════════════════════

_CAT_COLOR = {
    "organic":   YEL,
    "inorganic": CYN,
    "mineral":   MAG,
    "gas":       DIM,
    "acid":      RED,
    "salt":      GRN,
    "solvent":   CYN,
    "polymer":   YEL,
    "unknown":   DIM,
}

def _colour_name(name: str, cat: str) -> str:
    fn = _CAT_COLOR.get(cat, DIM)
    return fn(name)


# ═══════════════════════════════════════════════════════════════════════════════
#  CLI
# ═══════════════════════════════════════════════════════════════════════════════

def _cli():
    import argparse
    ap = argparse.ArgumentParser(
        prog="cfe_names",
        description="Formation Engine v0.5.1 — Common Name Finder (Daemon 2.1)")
    ap.add_argument("formulas", nargs="*", help="Formula(s) to look up")
    ap.add_argument("--all",   action="store_true", help="Dump full database")
    ap.add_argument("--json",  action="store_true", help="JSON output")
    ap.add_argument("--search", type=str, default="", help="Search by partial name/formula")
    ap.add_argument("--stats", action="store_true", help="Print database statistics")
    args = ap.parse_args()

    if args.stats:
        entries = all_entries()
        cats = {}
        for _, _, c in entries:
            cats[c] = cats.get(c, 0) + 1
        print(f"\n  {BOLD('Common Name Database')}  — {database_size()} entries\n")
        for c, n in sorted(cats.items(), key=lambda x: -x[1]):
            fn = _CAT_COLOR.get(c, DIM)
            print(f"    {fn(c):>16s}  {n}")
        print()
        return

    if args.all:
        entries = all_entries()
        if args.json:
            print(json.dumps([{"formula": f, "name": n, "category": c}
                              for f, n, c in entries], indent=2))
        else:
            print(f"\n  {BOLD('Common Name Database')}  ({len(entries)} entries)\n")
            for f, n, c in entries:
                print(f"  {BOLD(f):>20s}  {_colour_name(n, c):>40s}  {DIM(c)}")
            print()
        return

    if args.search:
        results = search(args.search)
        if args.json:
            print(json.dumps([{"formula": f, "name": n, "category": c}
                              for f, n, c in results], indent=2))
        else:
            print(f"\n  Search: '{args.search}'  ({len(results)} hits)\n")
            for f, n, c in results:
                print(f"  {BOLD(f):>20s}  {_colour_name(n, c):>40s}  {DIM(c)}")
            print()
        return

    if not args.formulas:
        ap.print_help()
        return

    results = []
    for formula in args.formulas:
        r = lookup(formula)
        if r:
            name, cat = r
            results.append({"formula": formula, "name": name, "category": cat})
            if not args.json:
                print(f"  {BOLD(formula):>16s}  :  {YEL(name)}  {DIM('[' + cat + ']')}")
        else:
            results.append({"formula": formula, "name": None, "category": "unknown"})
            if not args.json:
                print(f"  {BOLD(formula):>16s}  :  {DIM('(unknown)')}")

    if args.json:
        print(json.dumps(results, indent=2))


if __name__ == "__main__":
    _cli()
