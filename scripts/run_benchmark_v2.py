#!/usr/bin/env python3
"""
run_benchmark_v2.py -- Enhanced 100-Iteration CFE Benchmark Campaign v2

Campaign design
---------------
  FAST    : 50 iterations  |  50-101 formulas/batch  |   50 FIRE steps  |  60s cap
  MEDIUM  : 35 iterations  |  11-100 formulas/batch  | 1000 FIRE steps  |  60s cap
  HIGH_N  : 15 iterations  |   5-15  formulas/batch  | 10000 FIRE steps |  60s cap

All 100 iterations run in deterministic order (seed-controlled).
Each batch emits a per-batch Markdown report.
Full summary + 4 charts written at the end.

Output layout
-------------
  <out>/
    batch_001_fast.md  ...  batch_100_highn.md
    summary.md
    charts/
      energy_landscape.png
      classification_breakdown.png
      batch_throughput.png
      convergence_by_tier.png
    fast/
      ledger.csv  queue_NNN.txt  structs/
    medium/
      ledger.csv  queue_NNN.txt  structs/
    highn/
      ledger.csv  queue_NNN.txt  structs/

Usage
-----
  python scripts/run_benchmark_v2.py
  python scripts/run_benchmark_v2.py --out bench_v2 --iterations 100
  python scripts/run_benchmark_v2.py --report-only --out bench_v2
  python scripts/run_benchmark_v2.py --fast-only --iterations 50
  python scripts/run_benchmark_v2.py --highn-only --iterations 20
"""

import argparse
import csv
import io
import json
import os
import random
import subprocess
import sys
import time
from collections import Counter, defaultdict
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Tuple

# ── encoding fix (Windows cp1252 consoles) ────────────────────────────────────
if sys.stdout.encoding and sys.stdout.encoding.lower().replace('-', '') not in ('utf8', 'utf16'):
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8', errors='replace')

# ── Paths ─────────────────────────────────────────────────────────────────────
_SCRIPT_DIR   = Path(__file__).parent.resolve()
_PROJECT_ROOT = _SCRIPT_DIR.parent
_RUNNER       = _PROJECT_ROOT / 'build' / 'continual_runner.exe'

# ── Formula pools ─────────────────────────────────────────────────────────────
_ORGANIC = [
    'CH4','C2H6','C3H8','C4H10','C2H4','C2H2','C6H6','C6H12',
    'CH3OH','C2H5OH','HCOOH','CH3COOH','C2H5COOH','C3H7OH',
    'NH3','N2H4','HCN','CH3NH2','C2H5NH2',
    'H2O','H2O2','CO','CO2','CH2O','CH3CHO','CH3COCH3',
    'H2S','SO2','SO3','H2SO4','HNO3','HNO2','HF','HCl','HBr',
    'PH3','PCl3','PCl5','P4O10',
    'SiH4','Si2H6','SiO2','BF3','BCl3','B2H6',
    'N2O','NO','NO2','N2O4','N2O5',
    'C2H5Cl','CH3Cl','CH2Cl2','CHCl3','CCl4','CH3Br',
    'C6H5Cl','C6H5OH','C6H5CH3','C6H5NH2','C6H5COOH',
    'C3H6','C3H4','C4H8','C4H6','C5H10','C5H12',
]

_INORGANIC = [
    'NaCl','KCl','LiF','NaF','MgO','CaO','Al2O3','TiO2',
    'Fe2O3','Fe3O4','FeO','CuO','Cu2O','ZnO','NiO','CoO',
    'CaF2','BaF2','SrF2','MgF2','AlF3',
    'Na2O','K2O','Li2O','BaO','SrO',
    'FeCl2','FeCl3','CuCl','CuCl2','ZnCl2','NiCl2','CoCl2',
    'MgCl2','CaCl2','AlCl3','TiCl4',
    'Na2SO4','K2SO4','CaSO4','MgSO4','FeSO4','CuSO4','ZnSO4',
    'Na2CO3','K2CO3','CaCO3','MgCO3','FeCO3','ZnCO3',
    'Na3PO4','K3PO4','Ca3PO4','AlPO4',
    'NaOH','KOH','Ca(OH)2','Mg(OH)2','Al(OH)3','Fe(OH)3',
    'FeS','FeS2','CuS','Cu2S','ZnS','PbS','NiS',
    'KMnO4','K2CrO4','K2Cr2O7','KIO3',
    'SiC','TiC','WC','AlN','Si3N4','TiN','BN',
    'Li2CO3','Na2SiO3','Ca2SiO4',
]

_LARGE = [
    # polycyclic aromatics
    'C10H8','C12H10','C14H10','C16H10',
    # long-chain alkanes
    'C8H18','C10H22','C12H26','C16H34',
    # functionalized organics
    'C6H5OH','C6H5CH3','C6H5NH2','C6H5Cl','C6H5COOH',
    'C6H4(COOH)2','C6H10O','C6H12O6',
    'CH3COCH3','CH3CH2COCH3',
    'C2H5OH','C3H7OH','C4H9OH',
    'C4H4O','C5H5N',
    # metal oxides / silicates / spinels
    'Fe2O3','Al2O3','Ti2O3','Cr2O3',
    'Al2SiO5','Ca2SiO4','Ca3Al2O6','Na2SiO3',
    'ZnFe2O4','CoFe2O4','NiFe2O4',
    'BaTiO3','SrTiO3','PbTiO3','TiO2',
    # sulfates / chromates
    'Na2SO4','K2Cr2O7','CuSO4','FeSO4',
    # nitrides / carbides
    'Si3N4','AlN','TiN','WC','SiC',
]

ALL_FORMULAS = list(dict.fromkeys(_ORGANIC + _INORGANIC + _LARGE))

# ── Batch mode configuration ──────────────────────────────────────────────────
BATCH_MODES: Dict[str, dict] = {
    'fast':   {'steps': 50,    'batch_min': 50,  'batch_max': 101, 'seeds': 1},
    'medium': {'steps': 1000,  'batch_min': 11,  'batch_max': 100, 'seeds': 2},
    'highn':  {'steps': 10000, 'batch_min': 5,   'batch_max': 15,  'seeds': 1},
}

BATCH_WALL_CAP = 60.0  # hard wall-clock cap per batch (seconds)


# ── Formula selection ─────────────────────────────────────────────────────────
def _pick_formulas(mode: str, n: int, rng: random.Random) -> List[str]:
    if mode == 'highn':
        pool = _LARGE + _INORGANIC[:30]
    elif mode == 'medium':
        pool = ALL_FORMULAS
    else:
        pool = ALL_FORMULAS
    sample = rng.sample(pool, min(n, len(pool)))
    if len(sample) < n:
        sample += rng.choices(pool, k=n - len(sample))
    return sample


# ── Ledger helpers ────────────────────────────────────────────────────────────
def _count_ledger_rows(path: Path) -> int:
    if not path.exists():
        return 0
    with open(path, encoding='utf-8', errors='replace') as f:
        return max(0, sum(1 for _ in f) - 1)


def _read_ledger_rows(path: Path) -> List[dict]:
    if not path.exists():
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


# ── Runner invocation ─────────────────────────────────────────────────────────
def run_batch(
    formulas: List[str],
    mode: str,
    steps: int,
    seeds: int,
    out_dir: Path,
    batch_id: int,
) -> Tuple[List[dict], float]:
    """
    Write queue, invoke continual_runner, read new ledger rows.
    Each mode writes into out_dir/mode/ so ledger.csv is per-mode.
    Returns (new_rows, wall_seconds).
    """
    mode_dir = out_dir / mode
    mode_dir.mkdir(parents=True, exist_ok=True)
    ledger_path = mode_dir / 'ledger.csv'

    existing = _count_ledger_rows(ledger_path)

    queue_path = mode_dir / f'queue_{batch_id:03d}.txt'
    with open(queue_path, 'w', encoding='utf-8') as f:
        for formula in formulas:
            f.write(formula + '\n')

    cmd = [
        str(_RUNNER),
        '--queue', str(queue_path),
        '--out',   str(mode_dir),
        '--steps', str(steps),
        '--seeds', str(seeds),
        '--tier',  mode,
        '--verbose',
    ]

    t0 = time.perf_counter()
    try:
        subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            encoding='utf-8',
            errors='replace',
            timeout=BATCH_WALL_CAP + 30,
        )
    except subprocess.TimeoutExpired:
        pass
    wall = time.perf_counter() - t0

    rows = _read_ledger_rows(ledger_path)
    return rows[existing:], wall


# ── Per-batch Markdown report ─────────────────────────────────────────────────
def _safe_median(lst: list) -> float:
    if not lst:
        return 0.0
    s = sorted(lst)
    m = len(s) // 2
    return (s[m] + s[~m]) / 2


def write_batch_report(
    batch_id: int,
    mode: str,
    steps: int,
    formulas: List[str],
    rows: List[dict],
    wall: float,
    out_dir: Path,
) -> Path:
    cls   = Counter(r.get('classification', '?') for r in rows)
    n     = len(rows)
    good  = cls.get('stable', 0) + cls.get('metastable', 0)

    energies = [r['energy_per_atom'] for r in rows
                if isinstance(r.get('energy_per_atom'), float)
                and abs(r['energy_per_atom']) < 1e6]
    times    = [r['wall_ms'] for r in rows
                if isinstance(r.get('wall_ms'), float) and r['wall_ms'] > 0]

    best_e  = min(energies) if energies else float('nan')
    med_e   = _safe_median(energies)
    med_ms  = _safe_median(times)
    rate_s  = n / wall if wall > 0 else 0.0
    pct_ok  = good / n * 100 if n else 0.0

    top_rows = sorted(
        [r for r in rows if isinstance(r.get('energy_per_atom'), float)
         and abs(r['energy_per_atom']) < 1e6],
        key=lambda r: r['energy_per_atom']
    )[:5]

    fname = out_dir / f'batch_{batch_id:03d}_{mode}.md'
    ts    = datetime.now().strftime('%Y-%m-%d %H:%M:%S')

    with open(fname, 'w', encoding='utf-8') as f:
        f.write(f'# Batch {batch_id:03d} -- {mode.upper()}  ({steps} FIRE steps)\n\n')
        f.write(f'**Generated:** {ts}  \n')
        f.write(f'**Formulas queued:** {len(formulas)}  \n')
        f.write(f'**Results recorded:** {n}  \n')
        f.write(f'**Wall time:** {wall:.2f} s  \n')
        f.write(f'**Throughput:** {rate_s:.2f} formations/s  \n\n')

        f.write('## Classification\n\n')
        f.write('| Class | Count | Share |\n|---|---:|---:|\n')
        for cls_name in ('stable', 'metastable', 'unstable', 'timeout', 'collapsed', 'fragment'):
            cnt = cls.get(cls_name, 0)
            pct = cnt / n * 100 if n else 0.0
            f.write(f'| {cls_name} | {cnt} | {pct:.1f}% |\n')
        f.write(f'| **total** | **{n}** | 100% |\n\n')

        f.write('## Energy & Speed\n\n')
        f.write('| Metric | Value |\n|---|---:|\n')
        f.write(f'| Median E/atom (kcal/mol) | {med_e:+.4f} |\n')
        f.write(f'| Best E/atom (kcal/mol)   | {best_e:+.4f} |\n')
        f.write(f'| Median wall ms/form      | {med_ms:.3f} |\n')
        f.write(f'| Productive (stable+meta) | {good} ({pct_ok:.1f}%) |\n\n')

        if top_rows:
            f.write('## Top 5 by Energy/Atom\n\n')
            f.write('| Rank | Formula | E/atom | Class | Steps | ms |\n')
            f.write('|---:|---|---:|---|---:|---:|\n')
            for i, r in enumerate(top_rows, 1):
                f.write(f"| {i} | {r.get('formula','')} "
                        f"| {r.get('energy_per_atom',0):+.4f} "
                        f"| {r.get('classification','')} "
                        f"| {r.get('steps',0)} "
                        f"| {r.get('wall_ms',0):.1f} |\n")
            f.write('\n')

        f.write('<details>\n<summary>All queued formulas</summary>\n\n')
        f.write('`' + '`, `'.join(formulas) + '`\n\n')
        f.write('</details>\n')

    return fname


# ── Charts ────────────────────────────────────────────────────────────────────
def generate_charts(
    all_rows: Dict[str, List[dict]],
    batch_records: List[dict],
    charts_dir: Path,
):
    try:
        import matplotlib
        matplotlib.use('Agg')
        import matplotlib.pyplot as plt
        import matplotlib.patches as mpatches
        import numpy as np
    except ImportError:
        print('  [WARN] matplotlib/numpy not available -- skipping charts')
        return

    charts_dir.mkdir(parents=True, exist_ok=True)
    colors = {'fast': '#3498db', 'medium': '#2ecc71', 'highn': '#e74c3c'}
    cls_colors = {
        'stable':     '#27ae60',
        'metastable': '#f39c12',
        'unstable':   '#e74c3c',
        'timeout':    '#9b59b6',
        'collapsed':  '#95a5a6',
        'fragment':   '#3498db',
    }

    # Chart 1: Energy landscape per tier
    fig, ax = plt.subplots(figsize=(13, 5))
    offset = 0
    for mode, rows in all_rows.items():
        energies = [r['energy_per_atom'] for r in rows
                    if isinstance(r.get('energy_per_atom'), float)
                    and abs(r['energy_per_atom']) < 1e4]
        idxs = list(range(offset, offset + len(energies)))
        ax.scatter(idxs, energies, c=colors[mode], s=5, alpha=0.45,
                   label=mode.upper(), edgecolors='none')
        if len(energies) >= 3:
            win = max(5, len(energies) // 10)
            med_line = [float(np.median(energies[max(0, i - win):i + 1]))
                        for i in range(len(energies))]
            ax.plot(idxs, med_line, color=colors[mode], linewidth=2.0, alpha=0.85)
        offset += len(energies) + 15

    ax.set_xlabel('Formation index (per tier)')
    ax.set_ylabel('Energy / atom  (kcal/mol)')
    ax.set_title('Energy Landscape -- 100-Iteration Benchmark v2')
    ax.legend(fontsize=9)
    ax.grid(True, alpha=0.22)
    plt.tight_layout()
    _save(fig, charts_dir / 'energy_landscape.png')

    # Chart 2: Classification breakdown per tier
    cls_names  = ['stable', 'metastable', 'unstable', 'timeout', 'collapsed']
    mode_names = list(all_rows.keys())
    x     = np.arange(len(mode_names))
    width = 0.15
    fig, ax = plt.subplots(figsize=(10, 5))
    for i, cls in enumerate(cls_names):
        vals = [Counter(r.get('classification', '?')
                        for r in all_rows[m]).get(cls, 0) for m in mode_names]
        ax.bar(x + i * width, vals, width, label=cls,
               color=cls_colors.get(cls, '#aaa'), alpha=0.85)
    ax.set_xticks(x + width * (len(cls_names) - 1) / 2)
    ax.set_xticklabels([m.upper() for m in mode_names])
    ax.set_ylabel('Count')
    ax.set_title('Classification Breakdown by Tier')
    ax.legend(fontsize=8)
    ax.grid(True, alpha=0.22, axis='y')
    plt.tight_layout()
    _save(fig, charts_dir / 'classification_breakdown.png')

    # Chart 3: Batch throughput -- wall time & rate
    if batch_records:
        ids     = [b['id']   for b in batch_records]
        walls   = [b['wall'] for b in batch_records]
        rates   = [b['rate'] for b in batch_records]
        bcolors = [colors.get(b['mode'], '#aaa') for b in batch_records]

        fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(13, 7), sharex=True)
        ax1.bar(ids, walls, color=bcolors, alpha=0.72, edgecolor='none')
        ax1.axhline(BATCH_WALL_CAP, color='red', linestyle='--',
                    linewidth=1.2, label=f'{BATCH_WALL_CAP}s cap')
        ax1.set_ylabel('Wall time (s)')
        ax1.set_title('Batch Throughput -- Wall Time & Rate')
        ax1.legend(fontsize=8)
        ax1.grid(True, alpha=0.22, axis='y')

        ax2.bar(ids, rates, color=bcolors, alpha=0.72, edgecolor='none')
        ax2.set_xlabel('Batch ID')
        ax2.set_ylabel('Formations / s')
        ax2.grid(True, alpha=0.22, axis='y')

        patches = [mpatches.Patch(color=c, label=m.upper()) for m, c in colors.items()]
        ax2.legend(handles=patches, fontsize=8)
        plt.tight_layout()
        _save(fig, charts_dir / 'batch_throughput.png')

    # Chart 4: Convergence fraction by tier
    fig, ax = plt.subplots(figsize=(8, 4))
    for mode, rows in all_rows.items():
        conv  = [r for r in rows
                 if isinstance(r.get('converged'), int) and r['converged']]
        total = len(rows)
        pct   = len(conv) / total * 100 if total else 0.0
        ax.bar(mode.upper(), pct, color=colors[mode], alpha=0.82, edgecolor='none')
        ax.text(mode.upper(), pct + 1.2, f'{pct:.1f}%', ha='center',
                fontsize=9, fontweight='bold')
    ax.set_ylabel('Converged (%)')
    ax.set_ylim(0, 110)
    ax.set_title('FIRE Convergence Rate by Tier')
    ax.grid(True, alpha=0.22, axis='y')
    plt.tight_layout()
    _save(fig, charts_dir / 'convergence_by_tier.png')

    # Bonus Chart 5: per-mode best-energy progression (rolling min)
    fig, ax = plt.subplots(figsize=(12, 5))
    for mode, rows in all_rows.items():
        ee = [r['energy_per_atom'] for r in rows
              if isinstance(r.get('energy_per_atom'), float)
              and abs(r['energy_per_atom']) < 1e4]
        if not ee:
            continue
        running_min = []
        cur_min = float('inf')
        for e in ee:
            cur_min = min(cur_min, e)
            running_min.append(cur_min)
        ax.plot(running_min, color=colors[mode], linewidth=1.8,
                label=f'{mode.upper()} ({len(ee)} forms)', alpha=0.9)
    ax.set_xlabel('Formation index (per tier)')
    ax.set_ylabel('Running best E/atom  (kcal/mol)')
    ax.set_title('Best Energy Progression by Tier')
    ax.legend(fontsize=9)
    ax.grid(True, alpha=0.22)
    plt.tight_layout()
    _save(fig, charts_dir / 'best_energy_progression.png')

    # Bonus Chart 6: HIGH_N detailed scatter (formula vs E/atom)
    highn_rows = all_rows.get('highn', [])
    if highn_rows:
        valid = [(r.get('formula', '?'), r['energy_per_atom'], r.get('classification', '?'))
                 for r in highn_rows
                 if isinstance(r.get('energy_per_atom'), float)
                 and abs(r['energy_per_atom']) < 1e4]
        if valid:
            valid.sort(key=lambda x: x[1])
            labels, vals, clss = zip(*valid)
            c_map = {
                'stable': '#27ae60', 'metastable': '#f39c12',
                'unstable': '#e74c3c', 'timeout': '#9b59b6',
                'collapsed': '#95a5a6', 'fragment': '#3498db',
            }
            bar_colors = [c_map.get(c, '#aaa') for c in clss]
            fig, ax = plt.subplots(figsize=(max(10, len(labels) * 0.35 + 2), 5))
            ax.bar(range(len(labels)), vals, color=bar_colors, alpha=0.85, edgecolor='none')
            ax.set_xticks(range(len(labels)))
            ax.set_xticklabels(labels, rotation=75, ha='right', fontsize=7)
            ax.set_ylabel('E/atom  (kcal/mol)')
            ax.set_title('HIGH_N Formations -- Energy by Formula')
            ax.grid(True, alpha=0.22, axis='y')
            patches = [mpatches.Patch(color=v, label=k) for k, v in c_map.items()
                       if k in set(clss)]
            ax.legend(handles=patches, fontsize=8)
            plt.tight_layout()
            _save(fig, charts_dir / 'highn_formula_detail.png')


def _save(fig, path: Path):
    fig.savefig(path, dpi=180, bbox_inches='tight')
    import matplotlib.pyplot as plt
    plt.close(fig)
    print(f'  chart: {path}')


# ── Summary report ────────────────────────────────────────────────────────────
def write_summary(
    all_rows: Dict[str, List[dict]],
    batch_records: List[dict],
    out_dir: Path,
    total_wall: float,
    iterations: int,
) -> Path:
    ts       = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
    all_flat = [r for rows in all_rows.values() for r in rows]
    total_n  = len(all_flat)
    cls_all  = Counter(r.get('classification', '?') for r in all_flat)
    good     = cls_all.get('stable', 0) + cls_all.get('metastable', 0)

    energies = [r['energy_per_atom'] for r in all_flat
                if isinstance(r.get('energy_per_atom'), float)
                and abs(r['energy_per_atom']) < 1e6]
    best_e   = min(energies) if energies else float('nan')
    med_e    = _safe_median(energies)

    top10 = sorted(
        [r for r in all_flat
         if isinstance(r.get('energy_per_atom'), float)
         and abs(r['energy_per_atom']) < 1e6],
        key=lambda r: r['energy_per_atom']
    )[:10]

    path = out_dir / 'summary.md'
    with open(path, 'w', encoding='utf-8') as f:
        f.write('# 100-Iteration Benchmark v2 -- Summary\n\n')
        f.write(f'**Generated:** {ts}  \n')
        f.write(f'**Total iterations:** {iterations}  \n')
        f.write(f'**Total formations:** {total_n}  \n')
        f.write(f'**Total wall time:** {total_wall:.1f} s  ({total_wall / 60:.1f} min)  \n\n')

        # Per-tier overview
        f.write('## Per-Tier Overview\n\n')
        f.write('| Tier | Batches | Formations | Stable | Meta | '
                'Best E/atom | Median E/atom | Avg ms/form |\n')
        f.write('|---|---:|---:|---:|---:|---:|---:|---:|\n')
        for mode in ('fast', 'medium', 'highn'):
            rows = all_rows.get(mode, [])
            c    = Counter(r.get('classification', '?') for r in rows)
            ee   = [r['energy_per_atom'] for r in rows
                    if isinstance(r.get('energy_per_atom'), float)
                    and abs(r['energy_per_atom']) < 1e6]
            tt   = [r['wall_ms'] for r in rows
                    if isinstance(r.get('wall_ms'), float) and r['wall_ms'] > 0]
            nb   = sum(1 for b in batch_records if b['mode'] == mode)
            be   = min(ee) if ee else float('nan')
            me   = _safe_median(ee)
            avg  = sum(tt) / len(tt) if tt else 0.0
            f.write(f'| {mode.upper()} | {nb} | {len(rows)} | '
                    f'{c.get("stable",0)} | {c.get("metastable",0)} | '
                    f'{be:+.4f} | {me:+.4f} | {avg:.2f} |\n')
        f.write(f'| **ALL** | **{len(batch_records)}** | **{total_n}** | '
                f'**{cls_all.get("stable",0)}** | '
                f'**{cls_all.get("metastable",0)}** | '
                f'**{best_e:+.4f}** | **{med_e:+.4f}** | -- |\n\n')

        # Classification totals
        f.write('## Classification Totals\n\n')
        f.write('| Class | Count | Share |\n|---|---:|---:|\n')
        for cls_name in ('stable', 'metastable', 'unstable', 'timeout', 'collapsed', 'fragment'):
            cnt = cls_all.get(cls_name, 0)
            pct = cnt / total_n * 100 if total_n else 0.0
            f.write(f'| {cls_name} | {cnt} | {pct:.1f}% |\n')
        f.write(f'| **total** | **{total_n}** | 100% |\n\n')

        # Top 10
        if top10:
            f.write('## Top 10 Formations (Lowest E/atom)\n\n')
            f.write('| Rank | Formula | E/atom (kcal/mol) | Class | Tier/Steps | ms |\n')
            f.write('|---:|---|---:|---|---|---:|\n')
            for i, r in enumerate(top10, 1):
                f.write(f"| {i} | `{r.get('formula','')}` "
                        f"| {r.get('energy_per_atom',0):+.4f} "
                        f"| {r.get('classification','')} "
                        f"| {r.get('tier','')}/{r.get('max_steps',0)} "
                        f"| {r.get('wall_ms',0):.1f} |\n")
            f.write('\n')

        # Batch timing log
        f.write('## Batch Timing Log\n\n')
        f.write('| Batch | Mode | Steps | Formulas | Results | '
                'Wall (s) | Rate (f/s) | Best E/atom |\n')
        f.write('|---:|---|---:|---:|---:|---:|---:|---:|\n')
        for b in batch_records:
            f.write(f"| {b['id']:03d} | {b['mode'].upper()} | {b['steps']} "
                    f"| {b['n_formulas']} | {b['n_results']} "
                    f"| {b['wall']:.2f} | {b['rate']:.2f} "
                    f"| {b['best_e']:+.4f} |\n")
        f.write('\n')

        # Charts
        f.write('## Charts\n\n')
        for chart in ('energy_landscape', 'classification_breakdown',
                      'batch_throughput', 'convergence_by_tier',
                      'best_energy_progression', 'highn_formula_detail'):
            f.write(f'![{chart}](charts/{chart}.png)\n\n')

    return path


# ── Console helpers ───────────────────────────────────────────────────────────
def _bar(frac: float, w: int = 28) -> str:
    frac = max(0.0, min(1.0, frac))
    f = int(round(frac * w))
    return '#' * f + '-' * (w - f)


def _print_header(iterations: int, mode_counts: dict):
    print()
    print('=' * 68)
    print('  CFE Benchmark Campaign v2  --  100-Iteration Run')
    print(f'  Total iterations: {iterations}   Wall cap: {BATCH_WALL_CAP}s/batch')
    print(f'  FAST x{mode_counts.get("fast",0)}  '
          f'MEDIUM x{mode_counts.get("medium",0)}  '
          f'HIGH_N x{mode_counts.get("highn",0)}')
    print('=' * 68)
    print(f"  {'#':>4}  {'Mode':>8}  {'Steps':>6}  {'Forms':>5}  "
          f"{'Res':>4}  {'Wall':>6}  {'f/s':>6}  {'BestE/N':>10}")
    print(f"  {'-'*4}  {'-'*8}  {'-'*6}  {'-'*5}  "
          f"{'-'*4}  {'-'*6}  {'-'*6}  {'-'*10}")


def _print_batch(b: dict):
    print(f"  {b['id']:>4}  {b['mode'].upper():>8}  {b['steps']:>6}  "
          f"{b['n_formulas']:>5}  {b['n_results']:>4}  "
          f"{b['wall']:>5.1f}s  {b['rate']:>5.1f}/s  "
          f"{b['best_e']:>+10.4f}")


def _print_progress(batch_id: int, iterations: int, total_res: int, total_wall: float, all_rows: dict):
    cls_all = Counter(r.get('classification', '?')
                      for rows in all_rows.values() for r in rows)
    done = batch_id / iterations
    print(f'\n  Progress  [{_bar(done)}]  {batch_id}/{iterations}  '
          f'{total_res} results  {total_wall:.0f}s  '
          f'stable={cls_all.get("stable",0)}  '
          f'meta={cls_all.get("metastable",0)}\n')


# ── Campaign ──────────────────────────────────────────────────────────────────
def _build_schedule(iterations: int, rng: random.Random, fast_only: bool, highn_only: bool) -> List[str]:
    if fast_only:
        return ['fast'] * iterations
    if highn_only:
        return ['highn'] * iterations
    n_fast   = int(iterations * 0.50)
    n_medium = int(iterations * 0.35)
    n_highn  = iterations - n_fast - n_medium
    schedule = ['fast'] * n_fast + ['medium'] * n_medium + ['highn'] * n_highn
    rng.shuffle(schedule)
    return schedule


def run_campaign(
    out_dir: Path,
    iterations: int,
    report_only: bool,
    seed: int,
    fast_only: bool,
    highn_only: bool,
):
    out_dir.mkdir(parents=True, exist_ok=True)
    charts_dir = out_dir / 'charts'

    if report_only:
        print('  Report-only mode -- reading existing ledgers...')
        all_rows: Dict[str, List[dict]] = {}
        batch_records: List[dict] = []
        for mode in ('fast', 'medium', 'highn'):
            ledger = out_dir / mode / 'ledger.csv'
            all_rows[mode] = _read_ledger_rows(ledger)
            print(f'    {mode}: {len(all_rows[mode])} rows')
        generate_charts(all_rows, batch_records, charts_dir)
        return

    if not _RUNNER.exists():
        print(f'  [ERROR] Runner not found: {_RUNNER}')
        print('  Build it:  cmake --build build --target continual_runner')
        sys.exit(1)

    rng      = random.Random(seed)
    schedule = _build_schedule(iterations, rng, fast_only, highn_only)
    mode_counts = Counter(schedule)

    all_rows: Dict[str, List[dict]]   = {'fast': [], 'medium': [], 'highn': []}
    batch_records: List[dict]          = []
    batch_id   = 0
    total_wall = 0.0

    _print_header(iterations, mode_counts)

    for mode in schedule:
        batch_id += 1
        cfg      = BATCH_MODES[mode]
        n_f      = rng.randint(cfg['batch_min'], cfg['batch_max'])
        formulas = _pick_formulas(mode, n_f, rng)

        rows, wall = run_batch(
            formulas, mode, cfg['steps'], cfg['seeds'],
            out_dir, batch_id,
        )
        total_wall += wall

        ee     = [r['energy_per_atom'] for r in rows
                  if isinstance(r.get('energy_per_atom'), float)
                  and abs(r['energy_per_atom']) < 1e6]
        best_e = min(ee) if ee else float('nan')
        rate   = len(rows) / wall if wall > 0 else 0.0

        rec = {
            'id': batch_id, 'mode': mode, 'steps': cfg['steps'],
            'n_formulas': len(formulas), 'n_results': len(rows),
            'wall': wall, 'rate': rate, 'best_e': best_e,
        }
        batch_records.append(rec)
        all_rows[mode].extend(rows)
        _print_batch(rec)

        write_batch_report(
            batch_id, mode, cfg['steps'], formulas, rows, wall, out_dir
        )

        if batch_id % 10 == 0:
            total_res = sum(len(v) for v in all_rows.values())
            _print_progress(batch_id, iterations, total_res, total_wall, all_rows)

    # Final charts + summary
    generate_charts(all_rows, batch_records, charts_dir)
    summary_path = write_summary(all_rows, batch_records, out_dir, total_wall, iterations)

    total_res = sum(len(v) for v in all_rows.values())
    cls_all   = Counter(r.get('classification', '?')
                        for rows in all_rows.values() for r in rows)

    print()
    print('=' * 68)
    print('  CAMPAIGN COMPLETE')
    print(f'  Batches:       {len(batch_records)}')
    print(f'  Formations:    {total_res}')
    print(f'  Wall time:     {total_wall:.1f} s  ({total_wall / 60:.1f} min)')
    print(f'  Stable:        {cls_all.get("stable", 0)}')
    print(f'  Metastable:    {cls_all.get("metastable", 0)}')
    print(f'  Timeout:       {cls_all.get("timeout", 0)}')
    all_e = [r['energy_per_atom'] for rows in all_rows.values() for r in rows
             if isinstance(r.get('energy_per_atom'), float)
             and abs(r['energy_per_atom']) < 1e6]
    if all_e:
        best_form = min(
            (r for rows in all_rows.values() for r in rows
             if isinstance(r.get('energy_per_atom'), float)
             and abs(r['energy_per_atom']) < 1e6),
            key=lambda r: r['energy_per_atom']
        )
        print(f'  Best:          {best_form.get("formula","")}  '
              f'{best_form["energy_per_atom"]:+.4f} kcal/mol/atom  '
              f'({best_form.get("classification","?")})')
    print(f'  Summary:       {summary_path}')
    print(f'  Charts:        {charts_dir}/')
    print('=' * 68)
    print()


# ── Entry point ───────────────────────────────────────────────────────────────
def main():
    ap = argparse.ArgumentParser(
        description='CFE Benchmark Campaign v2 -- 100 Iterations',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Mode schedule (default 100 iterations):
  FAST    x50   50-101 formulas/batch    50 FIRE steps   60s cap
  MEDIUM  x35   11-100 formulas/batch  1000 FIRE steps   60s cap
  HIGH_N  x15    5-15  formulas/batch 10000 FIRE steps   60s cap

Examples:
  python scripts/run_benchmark_v2.py
  python scripts/run_benchmark_v2.py --out bench_v2 --iterations 100
  python scripts/run_benchmark_v2.py --fast-only  --iterations 50
  python scripts/run_benchmark_v2.py --highn-only --iterations 20
  python scripts/run_benchmark_v2.py --report-only --out bench_v2
""",
    )
    ap.add_argument('--out',         default='benchmark_v2',
                    help='Output directory (default: benchmark_v2)')
    ap.add_argument('--iterations',  type=int, default=100,
                    help='Total batch iterations (default: 100)')
    ap.add_argument('--seed',        type=int, default=2025,
                    help='RNG seed (default: 2025)')
    ap.add_argument('--fast-only',   action='store_true',
                    help='Run only FAST batches')
    ap.add_argument('--highn-only',  action='store_true',
                    help='Run only HIGH_N batches')
    ap.add_argument('--report-only', action='store_true',
                    help='Skip simulation; rebuild reports from existing ledgers')
    args = ap.parse_args()

    run_campaign(
        out_dir    = Path(args.out),
        iterations = args.iterations,
        report_only= args.report_only,
        seed       = args.seed,
        fast_only  = args.fast_only,
        highn_only = args.highn_only,
    )


if __name__ == '__main__':
    main()
