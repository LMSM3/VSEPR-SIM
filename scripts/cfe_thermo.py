#!/usr/bin/env python3
"""
cfe_thermo.py -- Daemon 2.3: Correctness Estimation — Entropy & Enthalpy
Formation Engine v0.5.1

Analyses the formation ledger for thermodynamic plausibility.
Flags suspect results, estimates formation enthalpy from atomistic energy,
provides Boltzmann entropy of the conformer population, and colour-codes
confidence in each result.

This is an *estimation* layer — it does not replace full DFT thermodynamics,
but it catches obvious failures and highlights suspect formations for
manual review.

Usage:
  python scripts/cfe_thermo.py continual_results            # full report
  python scripts/cfe_thermo.py continual_results --watch 5   # daemon mode
  python scripts/cfe_thermo.py continual_results --json      # machine output
  python scripts/cfe_thermo.py continual_results --flags     # only flagged
"""

import sys, os, io, csv, time, argparse, json, math
from pathlib import Path
from collections import Counter, defaultdict
from typing import List, Dict, Tuple, Optional

# Fix Windows encoding
if sys.stdout.encoding and sys.stdout.encoding.lower().replace('-', '') not in ('utf8', 'utf16'):
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8', errors='replace')

_SCRIPT_DIR = Path(__file__).parent.resolve()
sys.path.insert(0, str(_SCRIPT_DIR))
from cfe_names import lookup, lookup_name, YEL, GRN, CYN, RED, DIM, BOLD, MAG

# ── ANSI extras ───────────────────────────────────────────────────────────────
_TTY = sys.stdout.isatty()
def _c(code, t): return f"\033[{code}m{t}\033[0m" if _TTY else t
ORG  = lambda t: _c("38;5;208", t)  # orange-ish
WHT  = lambda t: _c("37;1", t)

# ═══════════════════════════════════════════════════════════════════════════════
#  REFERENCE ENTHALPIES OF FORMATION  (ΔHf° in kcal/mol, gas phase, 298 K)
#  Source: NIST WebBook / CRC Handbook reference values.
#  These are used to judge plausibility of the simulation's energy_per_atom.
# ═══════════════════════════════════════════════════════════════════════════════

_REF_ENTHALPY: Dict[str, Tuple[float, float]] = {
    # formula: (ΔHf° kcal/mol, num_atoms)  → ΔHf°/atom reference
    "H2":     (0.0,    2),
    "O2":     (0.0,    2),
    "N2":     (0.0,    2),
    "H2O":    (-57.8,  3),
    "CO2":    (-94.1,  3),
    "CO":     (-26.4,  2),
    "CH4":    (-17.9,  5),
    "C2H6":   (-20.0,  8),
    "C2H4":   (12.5,   6),
    "C2H2":   (54.2,   4),
    "C3H8":   (-25.0, 11),
    "C6H6":   (19.8,  12),
    "NH3":    (-11.0,  4),
    "HF":     (-65.3,  2),
    "HCl":    (-22.1,  2),
    "NO":     (21.6,   2),
    "NO2":    (7.9,    3),
    "N2O":    (19.6,   3),
    "SO2":    (-70.9,  3),
    "SO3":    (-94.6,  4),
    "H2S":    (-4.9,   3),
    "PH3":    (1.3,    4),
    "SiH4":   (8.2,    5),
    "BF3":    (-271.4, 4),
    "CH3OH":  (-48.0,  6),
    "HCOOH":  (-90.5,  5),
    "NaCl":   (-98.2,  2),
    "MgO":    (-143.8, 2),
    "Al2O3":  (-400.5, 5),
    "TiO2":   (-225.6, 3),
    "CaO":    (-151.8, 2),
    "Fe2O3":  (-196.5, 5),
    "SiO2":   (-217.7, 3),
}


# ═══════════════════════════════════════════════════════════════════════════════
#  PLAUSIBILITY THRESHOLDS
# ═══════════════════════════════════════════════════════════════════════════════

# Energy per atom bounds (kcal/mol) for crude sanity checking
_EPA_SANE_MIN   = -500.0   # anything lower is suspect (too bound)
_EPA_SANE_MAX   =  200.0   # anything higher is likely exploded
_EPA_WARN_MIN   = -200.0   # mild concern below this
_EPA_WARN_MAX   =   50.0   # mild concern above this

# Force-convergence expectation
_FORCE_OK       = 1e-3   # rms_force below = well converged
_FORCE_WARN     = 1e-1   # above this = not converged, enthalpy unreliable


# ═══════════════════════════════════════════════════════════════════════════════
#  FLAG SYSTEM
# ═══════════════════════════════════════════════════════════════════════════════

class ThermoFlag:
    """A single correctness flag attached to a formation result."""
    __slots__ = ('level', 'code', 'msg')
    def __init__(self, level: str, code: str, msg: str):
        self.level = level   # "ok", "warn", "error"
        self.code  = code    # short machine code
        self.msg   = msg

    def to_dict(self):
        return {"level": self.level, "code": self.code, "msg": self.msg}

    def coloured(self) -> str:
        if self.level == "error":
            return RED(f"✖ {self.code}: {self.msg}")
        elif self.level == "warn":
            return ORG(f"⚠ {self.code}: {self.msg}")
        else:
            return GRN(f"✓ {self.code}")


def _check_row(row: dict) -> List[ThermoFlag]:
    """Run all plausibility checks on a single ledger row."""
    flags = []
    epa   = row.get('energy_per_atom', None)
    rms   = row.get('rms_force', None)
    cls   = row.get('classification', '')
    form  = row.get('formula', '')
    natom = row.get('num_atoms', 0)
    steps = row.get('steps', 0)
    mstep = row.get('max_steps', 0)

    # -- F1: Energy bounds --
    if isinstance(epa, (int, float)):
        if epa < _EPA_SANE_MIN:
            flags.append(ThermoFlag("error", "E-LOW",
                f"E/atom={epa:.2f} kcal/mol — unreasonably low (< {_EPA_SANE_MIN})"))
        elif epa > _EPA_SANE_MAX:
            flags.append(ThermoFlag("error", "E-HIGH",
                f"E/atom={epa:.2f} kcal/mol — likely exploded (> {_EPA_SANE_MAX})"))
        elif epa < _EPA_WARN_MIN:
            flags.append(ThermoFlag("warn", "E-DEEP",
                f"E/atom={epa:.2f} — unusually deep well"))
        elif epa > _EPA_WARN_MAX:
            flags.append(ThermoFlag("warn", "E-WEAK",
                f"E/atom={epa:.2f} — weakly or unbound"))
        else:
            flags.append(ThermoFlag("ok", "E-OK", ""))
    else:
        flags.append(ThermoFlag("error", "E-NAN", "energy_per_atom is NaN or missing"))

    # -- F2: Force convergence --
    if isinstance(rms, (int, float)):
        if rms > _FORCE_WARN:
            flags.append(ThermoFlag("warn", "F-HIGH",
                f"rms_force={rms:.4f} — not converged, enthalpy unreliable"))
        elif rms > _FORCE_OK:
            flags.append(ThermoFlag("warn", "F-FAIR",
                f"rms_force={rms:.4f} — partially converged"))
        else:
            flags.append(ThermoFlag("ok", "F-OK", ""))

    # -- F3: Classification sanity --
    if cls == "collapsed":
        flags.append(ThermoFlag("error", "CLS-COLL", "Collapsed structure — discard"))
    elif cls == "timeout":
        flags.append(ThermoFlag("warn", "CLS-TMO", "Timeout — did not converge"))
    elif cls == "fragment":
        flags.append(ThermoFlag("warn", "CLS-FRAG", "Fragment — fewer atoms than expected"))

    # -- F4: Step saturation --
    if isinstance(steps, int) and isinstance(mstep, int) and mstep > 0:
        ratio = steps / mstep
        if ratio > 0.95 and cls != "stable":
            flags.append(ThermoFlag("warn", "STEP-SAT",
                f"Used {ratio*100:.0f}% of max steps — near timeout"))

    # -- F5: Reference enthalpy comparison --
    ref = _REF_ENTHALPY.get(form)
    if ref and isinstance(epa, (int, float)) and isinstance(natom, int) and natom > 0:
        ref_dhf, ref_n = ref
        ref_epa = ref_dhf / ref_n
        sim_dhf = epa * natom
        dev = abs(epa - ref_epa)
        if dev > 100:
            flags.append(ThermoFlag("error", "REF-DEV",
                f"ΔHf°/atom deviates {dev:.1f} kcal/mol from reference ({ref_epa:.1f})"))
        elif dev > 30:
            flags.append(ThermoFlag("warn", "REF-GAP",
                f"ΔHf°/atom off by {dev:.1f} from NIST ref ({ref_epa:.1f})"))
        else:
            flags.append(ThermoFlag("ok", "REF-OK",
                f"within {dev:.1f} of NIST ref"))

    return flags


# ═══════════════════════════════════════════════════════════════════════════════
#  BOLTZMANN ENTROPY OF CONFORMER POPULATION
#  S = -kB Σ pi ln(pi)  — measured in kcal/(mol·K) or dimensionless units.
#  This estimates the configurational entropy of the set of formation results
#  for a given formula (multiple seeds → conformer distribution).
# ═══════════════════════════════════════════════════════════════════════════════

_KB_KCAL = 1.987204e-3  # Boltzmann constant in kcal/(mol·K)

def _boltzmann_entropy(energies: List[float], T: float = 298.15) -> Dict:
    """
    Given a list of total energies (kcal/mol) from different seeds for
    the same formula, compute the Boltzmann population and configurational
    entropy.

    Returns dict with:
      populations:  list of (energy, probability) tuples
      S:            configurational entropy in kcal/(mol·K)
      S_units:      entropy in cal/(mol·K)  (more common unit)
      dominant_pct: fraction of population in the lowest-energy state
    """
    if not energies or len(energies) < 2:
        return {"populations": [], "S": 0.0, "S_units": 0.0,
                "dominant_pct": 100.0, "n_conformers": len(energies)}

    e_min = min(energies)
    beta  = 1.0 / (_KB_KCAL * T)

    # Boltzmann factors — shift by e_min for numerical stability
    bfs = []
    for e in energies:
        de = e - e_min
        # cap exponent to avoid underflow
        bf = math.exp(-beta * de) if (beta * de) < 500 else 0.0
        bfs.append(bf)

    Z = sum(bfs)
    if Z < 1e-300:
        return {"populations": [], "S": 0.0, "S_units": 0.0,
                "dominant_pct": 100.0, "n_conformers": len(energies)}

    probs = [bf / Z for bf in bfs]

    # S = -kB Σ pi ln(pi)
    S = 0.0
    for p in probs:
        if p > 1e-30:
            S -= p * math.log(p)
    S *= _KB_KCAL  # kcal/(mol·K)

    pops = sorted(zip(energies, probs), key=lambda x: -x[1])
    dom  = pops[0][1] * 100.0

    return {
        "populations":   pops,
        "S":             S,
        "S_units":       S * 1000.0,  # cal/(mol·K)
        "dominant_pct":  dom,
        "n_conformers":  len(energies),
    }


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
            for k in ('energy_per_atom', 'energy', 'rms_force', 'wall_ms'):
                try:    row[k] = float(row[k])
                except: pass
            for k in ('num_atoms', 'steps', 'max_steps', 'converged'):
                try:    row[k] = int(row[k])
                except: pass
            rows.append(row)
    return rows


# ═══════════════════════════════════════════════════════════════════════════════
#  AGGREGATE ANALYSIS
# ═══════════════════════════════════════════════════════════════════════════════

def analyse_thermo(rows: List[dict]) -> Dict:
    """Full thermodynamic plausibility report."""
    if not rows:
        return {"total": 0}

    # Per-row flags
    row_reports = []
    n_ok = n_warn = n_err = 0
    for row in rows:
        flags = _check_row(row)
        worst = "ok"
        for f in flags:
            if f.level == "error": worst = "error"
            elif f.level == "warn" and worst != "error": worst = "warn"

        if worst == "ok":    n_ok   += 1
        elif worst == "warn": n_warn += 1
        else:                 n_err  += 1

        row_reports.append({
            "formula": row.get('formula', ''),
            "classification": row.get('classification', ''),
            "energy_per_atom": row.get('energy_per_atom', 0),
            "flags": flags,
            "worst": worst,
        })

    # Per-formula Boltzmann entropy
    by_formula = defaultdict(list)
    for row in rows:
        f = row.get('formula', '')
        e = row.get('energy', None)
        if f and isinstance(e, (int, float)) and not (math.isnan(e) or math.isinf(e)):
            by_formula[f].append(e)

    entropy_reports = {}
    for formula, energies in by_formula.items():
        if len(energies) >= 2:
            entropy_reports[formula] = _boltzmann_entropy(energies)

    # Global energy distribution stats
    epas = [r.get('energy_per_atom', 0) for r in rows
            if isinstance(r.get('energy_per_atom'), (int, float))
            and abs(r.get('energy_per_atom', 0)) < 1e6]
    if epas:
        epas_sorted = sorted(epas)
        n = len(epas_sorted)
        median = epas_sorted[n // 2]
        mean = sum(epas) / n
        stddev = (sum((x - mean)**2 for x in epas) / n) ** 0.5
    else:
        median = mean = stddev = 0.0

    return {
        "total":           len(rows),
        "n_ok":            n_ok,
        "n_warn":          n_warn,
        "n_err":           n_err,
        "row_reports":     row_reports,
        "entropy_reports": entropy_reports,
        "epa_median":      median,
        "epa_mean":        mean,
        "epa_stddev":      stddev,
    }


# ═══════════════════════════════════════════════════════════════════════════════
#  DISPLAY
# ═══════════════════════════════════════════════════════════════════════════════

def _print_header():
    print()
    print(BOLD(CYN("  ┌─────────────────────────────────────────────────────────────┐")))
    print(BOLD(CYN("  │  CFE Thermo — Correctness Estimation    FE v0.5.1  [2.3]   │")))
    print(BOLD(CYN("  └─────────────────────────────────────────────────────────────┘")))
    print()


def _print_report(info: Dict, flags_only: bool = False, max_rows: int = 0):
    if info.get("total", 0) == 0:
        print(YEL("  No results in ledger."))
        return

    _print_header()

    # ── Summary bar ───────────────────────────────────────────────────────
    total = info["total"]
    ok    = info["n_ok"]
    warn  = info["n_warn"]
    err   = info["n_err"]

    ok_pct   = ok / total * 100
    warn_pct = warn / total * 100
    err_pct  = err / total * 100

    bar_w = 40
    ok_b   = int(round(ok_pct / 100 * bar_w))
    warn_b = int(round(warn_pct / 100 * bar_w))
    err_b  = bar_w - ok_b - warn_b
    bar = GRN("█" * ok_b) + ORG("█" * warn_b) + RED("█" * max(err_b, 0))

    print(f"  {BOLD('Plausibility')}:  {bar}  "
          f"{GRN(f'{ok_pct:.0f}% ok')}  {ORG(f'{warn_pct:.0f}% warn')}  "
          f"{RED(f'{err_pct:.0f}% err')}  ({total} total)")
    print()

    # ── Energy distribution ───────────────────────────────────────────────
    print(f"  {BOLD('E/atom stats')}:  "
          f"μ={info['epa_mean']:+.2f}  med={info['epa_median']:+.2f}  "
          f"σ={info['epa_stddev']:.2f} kcal/mol/atom")
    print()

    # ── Flagged rows ──────────────────────────────────────────────────────
    reports = info["row_reports"]
    if flags_only:
        reports = [r for r in reports if r["worst"] != "ok"]

    if max_rows > 0:
        reports = reports[-max_rows:]

    if reports:
        print(f"  {'Formula':>16s}  {'Class':>12s}  {'E/atom':>10s}  Flags")
        print(f"  {'-'*16}  {'-'*12}  {'-'*10}  {'-'*40}")
        for rr in reports:
            form = rr["formula"]
            cls  = rr["classification"]
            epa  = rr["energy_per_atom"]

            # Colour the formula by worst flag
            if rr["worst"] == "error":
                f_str = RED(f"{form:>16s}")
            elif rr["worst"] == "warn":
                f_str = ORG(f"{form:>16s}")
            else:
                f_str = GRN(f"{form:>16s}")

            epa_str = f"{epa:+.4f}" if isinstance(epa, (int, float)) else "N/A"
            flag_strs = "  ".join(f.coloured() for f in rr["flags"])
            print(f"  {f_str}  {cls:>12s}  {epa_str:>10s}  {flag_strs}")
    print()

    # ── Entropy section ───────────────────────────────────────────────────
    ent = info.get("entropy_reports", {})
    if ent:
        # Sort by entropy descending (most interesting first)
        sorted_ent = sorted(ent.items(), key=lambda x: -x[1]["S"])
        n_show = min(20, len(sorted_ent))
        print(f"  {BOLD('Conformer Entropy')}  (top {n_show} of {len(sorted_ent)} multi-seed formulas)")
        print(f"  {'Formula':>16s}  {'N':>3s}  {'S cal/(mol·K)':>14s}  "
              f"{'Dominant%':>10s}  Assessment")
        print(f"  {'-'*16}  {'-'*3}  {'-'*14}  {'-'*10}  {'-'*24}")
        for formula, es in sorted_ent[:n_show]:
            n = es["n_conformers"]
            s = es["S_units"]
            d = es["dominant_pct"]
            name = lookup_name(formula)
            tag  = f" ({YEL(name)})" if name else ""

            # Assessment
            if d > 99.0:
                assess = GRN("single conformer")
            elif d > 80.0:
                assess = GRN("well-defined minimum")
            elif d > 50.0:
                assess = YEL("moderately degenerate")
            else:
                assess = ORG("highly degenerate")

            print(f"  {BOLD(formula):>16s}  {n:>3d}  {s:>14.4f}  "
                  f"{d:>9.1f}%  {assess}{tag}")
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
        prog="cfe_thermo",
        description="Formation Engine v0.5.1 — Thermo Correctness Daemon (Layer 2.3)")
    ap.add_argument("dir", nargs="?", default="continual_results",
                    help="Results directory containing ledger.csv")
    ap.add_argument("--flags", action="store_true",
                    help="Show only flagged (warn/error) rows")
    ap.add_argument("--watch", type=float, default=0,
                    help="Daemon mode: refresh every N seconds")
    ap.add_argument("--last", type=int, default=0,
                    help="Show only last N rows (0=all)")
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
            info = analyse_thermo(rows)
            if args.json:
                # Serialise flags
                for rr in info.get("row_reports", []):
                    rr["flags"] = [f.to_dict() for f in rr["flags"]]
                print(json.dumps(info, indent=2, default=str))
            else:
                _print_report(info, flags_only=args.flags, max_rows=args.last)
            ts = time.strftime("%H:%M:%S")
            print(DIM(f"  Refreshing every {args.watch}s  |  {ts}"))
            time.sleep(args.watch)
    else:
        rows = _read_ledger(ledger)
        info = analyse_thermo(rows)
        if args.json:
            for rr in info.get("row_reports", []):
                rr["flags"] = [f.to_dict() for f in rr["flags"]]
            print(json.dumps(info, indent=2, default=str))
        else:
            _print_report(info, flags_only=args.flags, max_rows=args.last)


if __name__ == "__main__":
    main()
