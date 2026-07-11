#!/usr/bin/env python3
"""
cfe_audit.py -- Daemon 2.4: High-N Name Audit & Unknown Symbol Detector
Formation Engine v0.5.1

Scans the formation ledger and flags formulas that:
  1. Have no common name in the local database      → YELLOW "unnamed"
  2. Have high atom count (N) and no name            → RED "high-N unknown"
  3. Contain unusual element combinations             → ORANGE "exotic pair"
  4. Cross-references against importable chemistry DBs if available

The goal is to surface formulas that deserve manual investigation —
either because they are truly novel or because the name database
needs expansion.

Usage:
  python scripts/cfe_audit.py continual_results           # full audit
  python scripts/cfe_audit.py continual_results --red     # only RED items
  python scripts/cfe_audit.py continual_results --enrich  # attempt PubChem
  python scripts/cfe_audit.py continual_results --watch 5 # daemon mode
  python scripts/cfe_audit.py continual_results --json    # machine output
"""

import sys, os, io, csv, re, time, argparse, json
from pathlib import Path
from collections import Counter, defaultdict
from typing import List, Dict, Tuple, Optional

# Fix Windows encoding
if sys.stdout.encoding and sys.stdout.encoding.lower().replace('-', '') not in ('utf8', 'utf16'):
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8', errors='replace')

_SCRIPT_DIR = Path(__file__).parent.resolve()
sys.path.insert(0, str(_SCRIPT_DIR))
from cfe_names import (lookup, lookup_name, database_size,
                        YEL, GRN, CYN, RED, DIM, BOLD, MAG)

# ── ANSI extras ───────────────────────────────────────────────────────────────
_TTY = sys.stdout.isatty()
def _c(code, t): return f"\033[{code}m{t}\033[0m" if _TTY else t
ORG  = lambda t: _c("38;5;208", t)
WHT  = lambda t: _c("37;1", t)

# ═══════════════════════════════════════════════════════════════════════════════
#  FORMULA PARSER
# ═══════════════════════════════════════════════════════════════════════════════

def parse_formula(formula: str) -> Dict[str, int]:
    """Parse a chemical formula into {element: count} dict."""
    tokens = re.findall(r'([A-Z][a-z]?)(\d*)', formula)
    comp = {}
    for sym, cnt in tokens:
        if sym:
            comp[sym] = comp.get(sym, 0) + (int(cnt) if cnt else 1)
    return comp


def atom_count(formula: str) -> int:
    """Total number of atoms in a formula."""
    comp = parse_formula(formula)
    return sum(comp.values())


def element_count(formula: str) -> int:
    """Number of distinct elements."""
    return len(parse_formula(formula))


# ═══════════════════════════════════════════════════════════════════════════════
#  EXOTIC ELEMENT DETECTION
#  Common elements expected in most formation runs. Anything outside this
#  set is flagged as "exotic" — not wrong, just unusual and worth noting.
# ═══════════════════════════════════════════════════════════════════════════════

_COMMON_ELEMENTS = {
    'H', 'He', 'Li', 'Be', 'B', 'C', 'N', 'O', 'F', 'Ne',
    'Na', 'Mg', 'Al', 'Si', 'P', 'S', 'Cl', 'Ar',
    'K', 'Ca', 'Ti', 'Cr', 'Mn', 'Fe', 'Co', 'Ni', 'Cu', 'Zn',
    'Br', 'Ag', 'I', 'Sn', 'Au',
}

# High-N threshold: formulas with more atoms than this AND no name → RED
_HIGH_N_THRESHOLD = 10


# ═══════════════════════════════════════════════════════════════════════════════
#  AUDIT LEVELS
# ═══════════════════════════════════════════════════════════════════════════════

class AuditLevel:
    GREEN  = "green"    # named, all common elements
    YELLOW = "yellow"   # unnamed but small / common
    ORANGE = "orange"   # exotic elements or unusual combo
    RED    = "red"      # high-N unnamed — needs investigation


class AuditResult:
    __slots__ = ('formula', 'level', 'name', 'category',
                 'n_atoms', 'n_elements', 'exotic', 'reasons')

    def __init__(self, formula: str):
        self.formula    = formula
        self.level      = AuditLevel.GREEN
        self.name       = ""
        self.category   = "unknown"
        self.n_atoms    = 0
        self.n_elements = 0
        self.exotic     = []
        self.reasons    = []

    def to_dict(self):
        return {
            "formula":    self.formula,
            "level":      self.level,
            "name":       self.name or None,
            "category":   self.category,
            "n_atoms":    self.n_atoms,
            "n_elements": self.n_elements,
            "exotic":     self.exotic,
            "reasons":    self.reasons,
        }


def audit_formula(formula: str) -> AuditResult:
    """Audit a single formula for name coverage and element novelty."""
    r = AuditResult(formula)
    comp = parse_formula(formula)
    r.n_atoms    = sum(comp.values())
    r.n_elements = len(comp)

    # Name check
    hit = lookup(formula)
    if hit:
        r.name, r.category = hit
        r.level = AuditLevel.GREEN
    else:
        r.level = AuditLevel.YELLOW
        r.reasons.append("no common name in DB")

    # Exotic element check
    for elem in comp:
        if elem not in _COMMON_ELEMENTS:
            r.exotic.append(elem)
    if r.exotic:
        if r.level != AuditLevel.RED:
            r.level = AuditLevel.ORANGE
        r.reasons.append(f"exotic element(s): {', '.join(r.exotic)}")

    # High-N unnamed → RED
    if not hit and r.n_atoms > _HIGH_N_THRESHOLD:
        r.level = AuditLevel.RED
        r.reasons.append(f"high-N ({r.n_atoms} atoms) with no common name")

    return r


# ═══════════════════════════════════════════════════════════════════════════════
#  OPTIONAL PUBCHEM ENRICHMENT
# ═══════════════════════════════════════════════════════════════════════════════

_PUBCHEM_AVAILABLE = False
try:
    import urllib.request
    _PUBCHEM_AVAILABLE = True
except ImportError:
    pass


def _pubchem_lookup(formula: str) -> Optional[str]:
    """Attempt to resolve a formula via PubChem REST API. Returns name or None."""
    if not _PUBCHEM_AVAILABLE:
        return None
    try:
        url = (f"https://pubchem.ncbi.nlm.nih.gov/rest/pug/compound/"
               f"formula/{formula}/property/IUPACName/JSON")
        req = urllib.request.Request(url, headers={"Accept": "application/json"})
        with urllib.request.urlopen(req, timeout=5) as resp:
            data = json.loads(resp.read().decode())
            props = data.get("PropertyTable", {}).get("Properties", [])
            if props:
                return props[0].get("IUPACName", None)
    except Exception:
        pass
    return None


# ═══════════════════════════════════════════════════════════════════════════════
#  LEDGER READER
# ═══════════════════════════════════════════════════════════════════════════════

def _read_ledger(path: Path) -> List[dict]:
    if not path.exists():
        return []
    rows = []
    with open(path, newline='', encoding='utf-8', errors='replace') as f:
        reader = csv.DictReader(f)
        for row in reader:
            rows.append(row)
    return rows


# ═══════════════════════════════════════════════════════════════════════════════
#  FULL AUDIT
# ═══════════════════════════════════════════════════════════════════════════════

def run_audit(rows: List[dict], enrich: bool = False) -> Dict:
    """Run full audit on all unique formulas in the ledger."""
    formulas = sorted(set(r.get('formula', '') for r in rows if r.get('formula')))
    results = []
    counts  = Counter()

    for formula in formulas:
        ar = audit_formula(formula)

        # Optional PubChem enrichment for unnamed formulas
        if enrich and not ar.name:
            pcname = _pubchem_lookup(formula)
            if pcname:
                ar.name     = pcname
                ar.category = "pubchem"
                if ar.level in (AuditLevel.YELLOW, AuditLevel.RED):
                    ar.level = AuditLevel.ORANGE
                    ar.reasons.append(f"PubChem: {pcname}")

        counts[ar.level] += 1
        results.append(ar)

    return {
        "total_formulas": len(formulas),
        "total_rows":     len(rows),
        "db_size":        database_size(),
        "coverage_pct":   counts[AuditLevel.GREEN] / max(len(formulas), 1) * 100,
        "counts":         dict(counts),
        "results":        results,
    }


# ═══════════════════════════════════════════════════════════════════════════════
#  DISPLAY
# ═══════════════════════════════════════════════════════════════════════════════

_LEVEL_COLOR = {
    AuditLevel.GREEN:  GRN,
    AuditLevel.YELLOW: YEL,
    AuditLevel.ORANGE: ORG,
    AuditLevel.RED:    RED,
}

_LEVEL_SYMBOL = {
    AuditLevel.GREEN:  "●",
    AuditLevel.YELLOW: "◐",
    AuditLevel.ORANGE: "◑",
    AuditLevel.RED:    "○",
}


def _print_header():
    print()
    print(BOLD(CYN("  ┌─────────────────────────────────────────────────────────────┐")))
    print(BOLD(CYN("  │  CFE Audit — Name Coverage & Unknown Detector   FE v0.5.1  │")))
    print(BOLD(CYN("  │                                              [Layer 2.4]    │")))
    print(BOLD(CYN("  └─────────────────────────────────────────────────────────────┘")))
    print()


def _print_report(info: Dict, red_only: bool = False):
    if info.get("total_formulas", 0) == 0:
        print(YEL("  No formulas found."))
        return

    _print_header()

    total = info["total_formulas"]
    cov   = info["coverage_pct"]
    db    = info["db_size"]
    cts   = info["counts"]

    # Coverage bar
    bar_w = 40
    green_b  = int(round(cts.get("green", 0) / max(total, 1) * bar_w))
    yellow_b = int(round(cts.get("yellow", 0) / max(total, 1) * bar_w))
    orange_b = int(round(cts.get("orange", 0) / max(total, 1) * bar_w))
    red_b    = bar_w - green_b - yellow_b - orange_b

    bar = (GRN("█" * green_b) + YEL("█" * yellow_b) +
           ORG("█" * orange_b) + RED("█" * max(red_b, 0)))

    print(f"  {BOLD('Name Coverage')}:  {bar}  {cov:.0f}%  "
          f"({total} formulas, DB has {db} entries)")
    print()
    print(f"    {GRN('● green')}  {cts.get('green',0):>4d}  — named, common elements")
    print(f"    {YEL('◐ yellow')} {cts.get('yellow',0):>4d}  — unnamed, small/common")
    print(f"    {ORG('◑ orange')} {cts.get('orange',0):>4d}  — exotic elements or PubChem-only")
    print(f"    {RED('○ red')}    {cts.get('red',0):>4d}  — high-N unknown {BOLD('← investigate')}")
    print()

    # Results table
    results = info["results"]
    if red_only:
        results = [r for r in results if r.level == AuditLevel.RED]

    if results:
        print(f"  {'':>3s} {'Formula':>16s}  {'Name':<28s}  "
              f"{'N':>3s}  {'#El':>3s}  Reasons")
        print(f"  {'':>3s} {'-'*16}  {'-'*28}  {'-'*3}  {'-'*3}  {'-'*36}")

        for ar in results:
            lv_fn  = _LEVEL_COLOR.get(ar.level, DIM)
            sym    = _LEVEL_SYMBOL.get(ar.level, "?")
            sym_c  = lv_fn(sym)
            f_str  = lv_fn(f"{ar.formula:>16s}")
            n_str  = ar.name[:28] if ar.name else DIM("--")

            reason_parts = []
            for reason in ar.reasons:
                if "high-N" in reason:
                    reason_parts.append(RED(reason))
                elif "exotic" in reason:
                    reason_parts.append(ORG(reason))
                elif "PubChem" in reason:
                    reason_parts.append(CYN(reason))
                else:
                    reason_parts.append(YEL(reason))
            reasons = "  ".join(reason_parts)

            print(f"  {sym_c:>3s} {f_str}  {n_str:<28s}  "
                  f"{ar.n_atoms:>3d}  {ar.n_elements:>3d}  {reasons}")
    print()


# ═══════════════════════════════════════════════════════════════════════════════
#  CLI
# ═══════════════════════════════════════════════════════════════════════════════

def _find_ledger(d: Path) -> Optional[Path]:
    if (d / 'ledger.csv').exists():
        return d / 'ledger.csv'
    if d.is_file():
        return d
    for sub in ('fast', 'medium', 'highn'):
        p = d / sub / 'ledger.csv'
        if p.exists():
            return p
    return None


def main():
    ap = argparse.ArgumentParser(
        prog="cfe_audit",
        description="Formation Engine v0.5.1 — Name Audit Daemon (Layer 2.4)")
    ap.add_argument("dir", nargs="?", default="continual_results",
                    help="Results directory containing ledger.csv")
    ap.add_argument("--red", action="store_true",
                    help="Show only RED (high-N unknown) entries")
    ap.add_argument("--enrich", action="store_true",
                    help="Attempt PubChem enrichment for unnamed formulas")
    ap.add_argument("--watch", type=float, default=0,
                    help="Daemon mode: refresh every N seconds")
    ap.add_argument("--json", action="store_true",
                    help="JSON output")
    args = ap.parse_args()

    d = Path(args.dir)
    ledger = _find_ledger(d)
    if not ledger:
        print(RED(f"  No ledger.csv found in {d}"))
        sys.exit(1)
    print(DIM(f"  Ledger: {ledger}"))

    if args.watch > 0:
        while True:
            os.system('cls' if os.name == 'nt' else 'clear')
            rows = _read_ledger(ledger)
            info = run_audit(rows, enrich=args.enrich)
            if args.json:
                out = dict(info)
                out["results"] = [r.to_dict() for r in info["results"]]
                print(json.dumps(out, indent=2))
            else:
                _print_report(info, red_only=args.red)
            ts = time.strftime("%H:%M:%S")
            print(DIM(f"  Refreshing every {args.watch}s  |  {ts}"))
            time.sleep(args.watch)
    else:
        rows = _read_ledger(ledger)
        info = run_audit(rows, enrich=args.enrich)
        if args.json:
            out = dict(info)
            out["results"] = [r.to_dict() for r in info["results"]]
            print(json.dumps(out, indent=2))
        else:
            _print_report(info, red_only=args.red)


if __name__ == "__main__":
    main()
