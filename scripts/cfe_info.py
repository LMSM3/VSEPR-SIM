#!/usr/bin/env python3
"""
cfe_info.py — Continual Formation Engine: Lightweight Status Reporter

Reads the ledger + checkpoint produced by the live stack and prints a
compact, human-readable summary.  No simulation is started.  No matplotlib
required.  Runs in < 0.1 s on any sized ledger.

Usage:
    python scripts/cfe_info.py                    # default output dirs
    python scripts/cfe_info.py continual_results  # explicit dir
    python scripts/cfe_info.py --watch            # refresh every 5 s
    python scripts/cfe_info.py --json             # machine-readable JSON
    python scripts/cfe_info.py --top 10           # show top stable formulas
"""

import argparse
import csv
import json
import os
import sys
import time
from collections import Counter
from datetime import datetime, timezone

# ── ANSI colours (disabled when not a tty) ──────────────────────────────────
_USE_COLOR = sys.stdout.isatty()

def _c(code: str, text: str) -> str:
    return f"\033[{code}m{text}\033[0m" if _USE_COLOR else text

GREEN  = lambda t: _c("32", t)
YELLOW = lambda t: _c("33", t)
CYAN   = lambda t: _c("36", t)
BOLD   = lambda t: _c("1",  t)
DIM    = lambda t: _c("2",  t)
RED    = lambda t: _c("31", t)


# ── Core readers ─────────────────────────────────────────────────────────────

def read_ledger(path: str) -> list[dict]:
    if not os.path.exists(path):
        return []
    rows = []
    with open(path, newline='', encoding='utf-8', errors='replace') as f:
        reader = csv.DictReader(f)
        for row in reader:
            for k in ('num_atoms', 'steps', 'max_steps', 'converged'):
                try:
                    row[k] = int(row[k])
                except (KeyError, ValueError, TypeError):
                    pass
            for k in ('energy', 'energy_per_atom', 'rms_force', 'wall_ms'):
                try:
                    row[k] = float(row[k])
                except (KeyError, ValueError, TypeError):
                    pass
            rows.append(row)
    return rows


def read_checkpoint(path: str) -> dict:
    if not os.path.exists(path):
        return {}
    with open(path, encoding='utf-8') as f:
        return json.load(f)


# ── Analytics ────────────────────────────────────────────────────────────────

def analyse(rows: list[dict], ckpt: dict) -> dict:
    n = len(rows)
    if n == 0:
        return {"total": 0}

    cls_counts = Counter(r.get('classification', 'unknown') for r in rows)
    tier_counts = Counter(r.get('tier', '?') for r in rows)

    energies = [r['energy_per_atom'] for r in rows
                if isinstance(r.get('energy_per_atom'), float)
                and abs(r['energy_per_atom']) < 1e6]
    times    = [r['wall_ms'] for r in rows
                if isinstance(r.get('wall_ms'), float) and r['wall_ms'] > 0]

    stable_rows = [r for r in rows if r.get('classification') == 'stable']
    meta_rows   = [r for r in rows if r.get('classification') == 'metastable']

    # Rate estimation from timestamps
    rate_per_hr = 0.0
    start_str = ckpt.get('start_time', '')
    if start_str:
        try:
            start = datetime.fromisoformat(start_str)
            now   = datetime.now()
            elapsed_hr = max((now - start).total_seconds() / 3600, 1e-6)
            rate_per_hr = n / elapsed_hr
        except ValueError:
            pass

    def safe_median(lst):
        if not lst:
            return 0.0
        s = sorted(lst)
        m = len(s) // 2
        return (s[m] + s[~m]) / 2

    # Top stable formulas by best (lowest) energy/atom
    stable_best: list[tuple[str, float]] = []
    seen_f: set[str] = set()
    for r in sorted(stable_rows, key=lambda x: x.get('energy_per_atom', 1e9)):
        f = r.get('formula', '')
        if f and f not in seen_f:
            stable_best.append((f, r.get('energy_per_atom', 0.0)))
            seen_f.add(f)

    # Estimated time remaining
    deadline_str = ''
    time_left_str = ''
    if start_str and ckpt.get('hours_target'):
        try:
            from datetime import timedelta
            start  = datetime.fromisoformat(start_str)
            target = ckpt['hours_target']
            end    = start + timedelta(hours=target)
            left   = (end - datetime.now()).total_seconds()
            if left > 0:
                h, rem = divmod(int(left), 3600)
                m, s   = divmod(rem, 60)
                time_left_str = f"{h}h {m:02d}m {s:02d}s"
            else:
                time_left_str = "complete"
        except Exception:
            pass

    return {
        "total":          n,
        "stable":         cls_counts.get('stable', 0),
        "metastable":     cls_counts.get('metastable', 0),
        "unstable":       cls_counts.get('unstable', 0),
        "collapsed":      cls_counts.get('collapsed', 0),
        "timeout":        cls_counts.get('timeout', 0),
        "tier_counts":    dict(tier_counts),
        "unique":         len(set(r.get('formula','') for r in rows)),
        "rate_per_hr":    rate_per_hr,
        "median_e":       safe_median(energies),
        "min_e":          min(energies) if energies else 0.0,
        "median_ms":      safe_median(times),
        "cycle":          ckpt.get('cycle', 0),
        "session_id":     ckpt.get('session_id', '—'),
        "start_time":     start_str,
        "hours_target":   ckpt.get('hours_target', 0),
        "time_left":      time_left_str,
        "promote_queue":  len(ckpt.get('promote_queue', [])),
        "deep_queue":     len(ckpt.get('deep_queue', [])),
        "stable_best":    stable_best[:20],
        "stable_formulas": ckpt.get('stable_formulas', []),
    }


# ── Display ───────────────────────────────────────────────────────────────────

_CLASS_COLOR = {
    'stable':     GREEN,
    'metastable': YELLOW,
    'unstable':   RED,
    'collapsed':  DIM,
    'timeout':    DIM,
}

def _bar(frac: float, width: int = 24) -> str:
    frac = max(0.0, min(1.0, frac))
    filled = int(round(frac * width))
    return "█" * filled + "░" * (width - filled)


def print_summary(info: dict, out_dir: str, top_n: int = 10):
    if info.get("total", 0) == 0:
        print(YELLOW("  No results yet — ledger is empty."))
        return

    n      = info["total"]
    stable = info["stable"]
    meta   = info["metastable"]
    good   = stable + meta

    # ── Header ────────────────────────────────────────────────────────────────
    print()
    print(BOLD(CYAN("  ┌─────────────────────────────────────────────────────────┐")))
    print(BOLD(CYAN("  │  Continual Formation Engine — Status                    │")))
    print(BOLD(CYAN("  └─────────────────────────────────────────────────────────┘")))
    print()

    # ── Session block ─────────────────────────────────────────────────────────
    sid   = info.get("session_id", "—")
    cyc   = info.get("cycle", 0)
    left  = info.get("time_left", "")
    tgt   = info.get("hours_target", 0)
    print(f"  {BOLD('Session')}   {CYAN(sid)}   "
          f"cycle {YELLOW(str(cyc))}   "
          f"target {tgt}h  "
          + (f"  remaining {GREEN(left)}" if left else ""))
    print(f"  {BOLD('Output')}    {DIM(out_dir)}")
    print()

    # ── Progress bar ──────────────────────────────────────────────────────────
    good_frac = good / n if n else 0
    bar = _bar(good_frac)
    print(f"  Formations   {BOLD(str(n).rjust(7))}   {bar}  "
          + GREEN(f"{good_frac*100:.1f}% productive"))
    print()

    # ── Classification table ─────────────────────────────────────────────────
    rows_data = [
        ("stable",     info["stable"],     "▇"),
        ("metastable", info["metastable"],  "▅"),
        ("unstable",   info["unstable"],    "▂"),
        ("timeout",    info["timeout"],     "·"),
        ("collapsed",  info["collapsed"],   "·"),
    ]
    print(f"  {'CLASS':<12}  {'COUNT':>7}  {'SHARE':>7}  {'BAR'}")
    print(f"  {'─'*12}  {'─'*7}  {'─'*7}  {'─'*24}")
    for cls, cnt, glyph in rows_data:
        pct   = cnt / n * 100
        b     = _bar(cnt / n, 20)
        color = _CLASS_COLOR.get(cls, lambda x: x)
        print(f"  {color(cls.ljust(12))}  {str(cnt).rjust(7)}  "
              f"{f'{pct:.1f}%'.rjust(7)}  {color(b)}")
    print()

    # ── Tiers ────────────────────────────────────────────────────────────────
    tc = info.get("tier_counts", {})
    if tc:
        parts = "  ".join(
            f"{CYAN(t.ljust(8))} {BOLD(str(c))}"
            for t, c in sorted(tc.items())
        )
        print(f"  Tiers:  {parts}")
        print()

    # ── Queues ────────────────────────────────────────────────────────────────
    pq = info.get("promote_queue", 0)
    dq = info.get("deep_queue", 0)
    if pq or dq:
        print(f"  Pending promotions:  "
              f"screen→medium {YELLOW(str(pq))}   "
              f"medium→deep   {YELLOW(str(dq))}")
        print()

    # ── Energy / speed ────────────────────────────────────────────────────────
    med_e  = info.get("median_e", 0.0)
    min_e  = info.get("min_e", 0.0)
    med_ms = info.get("median_ms", 0.0)
    rph    = info.get("rate_per_hr", 0.0)
    print(f"  Energy/atom   median {CYAN(f'{med_e:+.3f}'):>14} kcal/mol   "
          f"best {GREEN(f'{min_e:+.3f}'):>14} kcal/mol")
    print(f"  Speed         {CYAN(f'{med_ms:.2f} ms/form'):>14}              "
          f"rate  {YELLOW(f'{rph:.0f}/hr')}")
    print()

    # ── Top stable formulas ───────────────────────────────────────────────────
    best = info.get("stable_best", [])
    if best:
        print(f"  {BOLD('Top stable formulas')} (lowest E/atom):")
        for i, (formula, e) in enumerate(best[:top_n], 1):
            print(f"    {str(i).rjust(3)}.  {GREEN(formula.ljust(14))}  "
                  f"{f'{e:+.4f}':>12} kcal/mol/atom")
        if len(info.get("stable_formulas", [])) > top_n:
            extra = len(info["stable_formulas"]) - top_n
            print(f"         {DIM(f'  … and {extra} more')}")
        print()

    # ── Footer ────────────────────────────────────────────────────────────────
    ts = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    print(DIM(f"  Refreshed at {ts}"))
    print()


def print_json(info: dict):
    # Remove non-serialisable lambdas (stable_best list is fine)
    safe = {k: v for k, v in info.items() if not callable(v)}
    print(json.dumps(safe, indent=2))


# ── Entry point ───────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(
        description="CFE lightweight status reporter",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument('dir', nargs='?', default='continual_results',
                        help='Output directory to inspect (default: continual_results)')
    parser.add_argument('--watch', action='store_true',
                        help='Refresh every 5 seconds (Ctrl+C to exit)')
    parser.add_argument('--interval', type=int, default=5,
                        help='Watch interval in seconds (default: 5)')
    parser.add_argument('--json', action='store_true',
                        help='Emit machine-readable JSON')
    parser.add_argument('--top', type=int, default=10,
                        help='How many top stable formulas to show (default: 10)')
    args = parser.parse_args()

    out_dir   = args.dir
    ledger    = os.path.join(out_dir, 'ledger.csv')
    ckpt_file = os.path.join(out_dir, 'checkpoint.json')

    def once():
        rows = read_ledger(ledger)
        ckpt = read_checkpoint(ckpt_file)
        info = analyse(rows, ckpt)
        if args.json:
            print_json(info)
        else:
            print_summary(info, out_dir, top_n=args.top)

    if args.watch:
        try:
            while True:
                if _USE_COLOR:
                    print("\033[2J\033[H", end='')  # clear screen
                once()
                time.sleep(args.interval)
        except KeyboardInterrupt:
            print("\nStopped.")
    else:
        once()


if __name__ == '__main__':
    main()
