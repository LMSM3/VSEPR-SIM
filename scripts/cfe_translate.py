#!/usr/bin/env python3
"""
cfe_translate.py -- Daemon 2.2: Live Translation & Character-Cast Layer
Formation Engine v0.5.1

Continuously reads the ledger and overlays human-readable names, colour-coded
classifications, and real-time translation of every formula result.

Modes:
  --tail     Live tail: watches ledger, prints each new row with name overlay
  --cast     Full cast: re-reads entire ledger, prints colour-coded table
  --watch N  Refresh every N seconds (daemon mode)

Usage:
  python scripts/cfe_translate.py --tail continual_results
  python scripts/cfe_translate.py --cast benchmark_v2/fast
  python scripts/cfe_translate.py --watch 5 continual_results
"""

import sys, os, io, csv, time, argparse, json
from pathlib import Path
from typing import List, Dict, Optional

# Fix Windows encoding
if sys.stdout.encoding and sys.stdout.encoding.lower().replace('-','') not in ('utf8','utf16'):
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8', errors='replace')

# Import sibling name database
_SCRIPT_DIR = Path(__file__).parent.resolve()
sys.path.insert(0, str(_SCRIPT_DIR))
from cfe_names import lookup, lookup_name, YEL, GRN, CYN, RED, DIM, BOLD, MAG

# ── ANSI helpers ──────────────────────────────────────────────────────────────
_TTY = sys.stdout.isatty()
def _c(code, t): return f"\033[{code}m{t}\033[0m" if _TTY else t
WHT  = lambda t: _c("37;1", t)

_CLS_COLOR = {
    "stable":     GRN,
    "metastable": YEL,
    "unstable":   RED,
    "timeout":    DIM,
    "collapsed":  RED,
    "fragment":   DIM,
}

_CAT_ICON = {
    "organic":   "C",
    "inorganic": "I",
    "mineral":   "M",
    "gas":       "G",
    "acid":      "A",
    "salt":      "S",
    "solvent":   "V",
    "unknown":   "?",
}


# ── Ledger reader ─────────────────────────────────────────────────────────────
def _read_ledger(path: Path) -> List[dict]:
    if not path.exists():
        return []
    rows = []
    with open(path, newline='', encoding='utf-8', errors='replace') as f:
        reader = csv.DictReader(f)
        for row in reader:
            for k in ('energy_per_atom', 'rms_force', 'wall_ms'):
                try:    row[k] = float(row[k])
                except: pass
            for k in ('num_atoms', 'steps', 'max_steps', 'converged'):
                try:    row[k] = int(row[k])
                except: pass
            rows.append(row)
    return rows


# ── Translate one row ─────────────────────────────────────────────────────────
def _translate_row(row: dict, idx: int) -> str:
    formula = row.get('formula', '?')
    cls     = row.get('classification', '?')
    epa     = row.get('energy_per_atom', 0.0)
    tier    = row.get('tier', '?')
    ms      = row.get('wall_ms', 0.0)

    # Name lookup
    result  = lookup(formula)
    name    = result[0] if result else ""
    cat     = result[1] if result else "unknown"
    icon    = _CAT_ICON.get(cat, "?")

    # Colour the classification
    cls_fn = _CLS_COLOR.get(cls, DIM)
    cls_str = cls_fn(f"{cls:>12s}")

    # Colour the formula + name
    if name:
        name_str = YEL(name)
    else:
        name_str = DIM("--")

    epa_str = f"{epa:+.4f}" if isinstance(epa, float) else str(epa)

    return (f"  {idx:>5d}  {BOLD(formula):>16s}  {name_str:<32s} [{icon}]  "
            f"{cls_str}  {epa_str:>10s}  {tier:>7s}  {ms:>7.1f}ms")


# ── Header ────────────────────────────────────────────────────────────────────
def _print_header():
    print()
    print(BOLD(CYN("  +---------------------------------------------------------"
                    "-------------------------+")))
    print(BOLD(CYN("  |  CFE Translate — Live Name Overlay                      "
                    "   Formation Engine 0.5.1 |")))
    print(BOLD(CYN("  +---------------------------------------------------------"
                    "-------------------------+")))
    print()
    print(f"  {'#':>5s}  {'Formula':>16s}  {'Common Name':<32s} Cat  "
          f"{'Class':>12s}  {'E/atom':>10s}  {'Tier':>7s}  {'Time':>9s}")
    print(f"  {'-'*5}  {'-'*16}  {'-'*32} ---  "
          f"{'-'*12}  {'-'*10}  {'-'*7}  {'-'*9}")


# ── Cast mode: full table ─────────────────────────────────────────────────────

def _splash_summary(rows: list):
    if not rows:
        return
    # Use the most recent row (or last 2-3 if available)
    last = rows[-1]
    formula = last.get('formula', '?')
    name, cat = (lookup(formula) or ("", "unknown"))
    epa = last.get('energy_per_atom', None)
    cls = last.get('classification', '')
    tier = last.get('tier', '')
    # Try to extract torsion/angle/conformer info if present
    # (Assume extra columns or parse from a 'conformer' field if present)
    conformer = last.get('conformer', '')
    # Theme by category
    theme = {
        'organic': GRN,
        'inorganic': CYN,
        'mineral': MAG,
        'gas': DIM,
        'acid': RED,
        'salt': GRN,
        'solvent': CYN,
        'polymer': YEL,
        'unknown': DIM,
        'metal': WHT,
        'organometallic': WHT,
        'noble': MAG,
    }.get(cat, DIM)
    # Special: metals/organometallics (silver/white), noble (pink/magenta)
    if any(x in formula for x in ("Fe", "Ni", "Co", "Cu", "Zn", "Ag", "Au", "Pt", "Pd", "Ir", "Os", "Ru", "Rh", "Mn", "Cr", "V", "Ti")):
        theme = WHT
    if any(x in formula for x in ("He", "Ne", "Ar", "Kr", "Xe", "Rn")):
        theme = MAG
    # Compose splash lines
    splash = []
    splash.append(theme(f"  77 {formula}  {name if name else ''}  [{cat}]"))
    if epa is not None:
        splash.append(theme(f"      Energy: {epa:+.4f} kcal/mol/atom   Class: {cls}   Tier: {tier}"))
    if conformer:
        splash.append(theme(f"      Conformer: {conformer}"))
    # Show angles/torsions if present in last 1-2 rows
    for r in rows[-3:]:
        if any(k in r for k in ("angle", "torsion", "phi", "conformer")):
            phi = r.get('phi', None)
            ang = r.get('angle', None)
            conf = r.get('conformer', None)
            if phi is not None:
                splash.append(theme(f"      Torsion: {phi} deg   {conf or ''}"))
            if ang is not None:
                splash.append(theme(f"      Angle: {ang} deg   {conf or ''}"))
    # Limit to 3-6 lines
    for line in splash[:6]:
        print(line)

def _mode_cast(ledger_path: Path, max_rows: int = 0):
    rows = _read_ledger(ledger_path)
    if not rows:
        print(YEL("  No results found in ledger."))
        return

    _splash_summary(rows)
    _print_header()
    shown = rows if max_rows <= 0 else rows[-max_rows:]
    base  = len(rows) - len(shown)
    named = 0
    for i, row in enumerate(shown):
        line = _translate_row(row, base + i + 1)
        print(line)
        if lookup_name(row.get('formula', '')):
            named += 1

    # Summary line
    total = len(rows)
    pct = named / len(shown) * 100 if shown else 0
    print()
    print(f"  {BOLD('Total')}: {total} formations  |  "
          f"{GRN(str(named))} named ({pct:.0f}%)  |  "
          f"{DIM(str(len(shown) - named))} unknown")
    print()


# ── Tail mode: watch for new rows ────────────────────────────────────────────
def _mode_tail(ledger_path: Path, poll: float = 1.0):
    _print_header()
    seen = 0
    while True:
        rows = _read_ledger(ledger_path)
        if len(rows) > seen:
            for i in range(seen, len(rows)):
                print(_translate_row(rows[i], i + 1))
            seen = len(rows)
        time.sleep(poll)


# ── Watch mode: periodic full refresh ─────────────────────────────────────────
def _mode_watch(ledger_path: Path, interval: float):
    while True:
        os.system('cls' if os.name == 'nt' else 'clear')
        _mode_cast(ledger_path, max_rows=40)
        ts = time.strftime("%H:%M:%S")
        print(f"  {DIM(f'Refreshing every {interval}s  |  {ts}')}")
        time.sleep(interval)


# ── CLI ───────────────────────────────────────────────────────────────────────
def main():
    ap = argparse.ArgumentParser(
        prog="cfe_translate",
        description="Formation Engine v0.5.1 — Live Translation Daemon (Layer 2.2)")
    ap.add_argument("dir", nargs="?", default="continual_results",
                    help="Results directory containing ledger.csv")
    ap.add_argument("--tail",  action="store_true",
                    help="Live tail mode: show new results as they appear")
    ap.add_argument("--cast",  action="store_true",
                    help="Cast mode: print full colour table")
    ap.add_argument("--watch", type=float, default=0,
                    help="Daemon watch mode: refresh every N seconds")
    ap.add_argument("--last",  type=int, default=0,
                    help="Show only the last N rows (0=all)")
    ap.add_argument("--json",  action="store_true",
                    help="JSON output instead of coloured table")
    args = ap.parse_args()

    # Resolve ledger path
    d = Path(args.dir)
    if (d / 'ledger.csv').exists():
        ledger = d / 'ledger.csv'
    elif d.is_file():
        ledger = d
    else:
        # Try mode sub-dirs
        for sub in ('fast', 'medium', 'highn'):
            p = d / sub / 'ledger.csv'
            if p.exists():
                ledger = p
                break
        else:
            print(RED(f"  No ledger.csv found in {d}"))
            sys.exit(1)

    print(DIM(f"  Ledger: {ledger}"))

    if args.json:
        rows = _read_ledger(ledger)
        out = []
        for r in rows:
            f = r.get('formula', '')
            result = lookup(f)
            out.append({
                "formula": f,
                "name": result[0] if result else None,
                "category": result[1] if result else "unknown",
                "classification": r.get('classification', ''),
                "energy_per_atom": r.get('energy_per_atom', 0),
            })
        print(json.dumps(out, indent=2))
        return

    if args.watch > 0:
        _mode_watch(ledger, args.watch)
    elif args.tail:
        _mode_tail(ledger)
    else:
        _mode_cast(ledger, max_rows=args.last)


if __name__ == "__main__":
    main()
