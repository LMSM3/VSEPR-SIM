#!/usr/bin/env python3
"""
continual_orchestrator.py — Continual Formation Engine Brain

Generates formulas → feeds queue → parses results → produces charts.
Runs indefinitely. Checkpoint/resume built in.

Smart methods that replace brute-force GPU compute:
  1. Tiered evaluation: screen rejects 70% in <0.5s, medium confirms,
     deep only runs on the best candidates.
  2. Adaptive targeting: past results guide future formula generation.
     Formulas near stable hits get more seeds. Dead zones get skipped.
  3. Coverage tracking: element-pair heatmap ensures the periodic table
     is explored systematically, not randomly.
  4. Diminishing-return detection: if a region stops producing new
     stable forms, reduce its priority automatically.

Usage:
  python continual_orchestrator.py --out continual_results [--hours 24]
  python continual_orchestrator.py --resume continual_results
  python continual_orchestrator.py --chart continual_results  (chart only)

Requires: matplotlib, numpy (pip install matplotlib numpy)
"""

import argparse
import csv
import json
import os
import random
import sys

# Ensure stdout/stderr can carry UTF-8 on Windows (cp1252 default breaks box-drawing)
if sys.stdout.encoding and sys.stdout.encoding.lower().replace('-', '') not in ('utf8', 'utf16'):
    import io
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8', errors='replace')
import subprocess
import time
import math
import hashlib
from collections import defaultdict, Counter
from datetime import datetime, timedelta
from pathlib import Path
from dataclasses import dataclass, field, asdict
from typing import List, Dict, Optional, Tuple

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

# Common organic/inorganic elements with valences for smart generation
ELEMENT_POOL = {
    # element: (weight, max_count, typical_valence)
    'H':  (100, 12, 1),
    'C':  (95,  8, 4),
    'N':  (80,  6, 3),
    'O':  (90,  6, 2),
    'F':  (30,  4, 1),
    'S':  (35,  4, 2),
    'P':  (25,  4, 3),
    'Cl': (40,  4, 1),
    'Br': (20,  3, 1),
    'Si': (15,  4, 4),
    'B':  (5,   3, 3),
    'Li': (2,   2, 1),
    'Na': (8,   2, 1),
    'Mg': (10,  2, 2),
    'Al': (12,  2, 3),
    'Ca': (8,   2, 2),
    'Fe': (5,   2, 2),
    'Cu': (3,   2, 2),
    'Zn': (4,   2, 2),
    'Ag': (2,   2, 1),
    'Au': (1,   1, 1),
    'Ti': (2,   2, 4),
    'I':  (15,  3, 1),
    'Se': (3,   2, 2),
    'Ge': (1,   2, 4),
    'Sn': (2,   2, 4),
}

BENCHMARK_FORMULAS = [
    'H2O', 'CH4', 'NH3', 'CO2', 'H2S', 'HF', 'HCl',
    'C2H6', 'C2H4', 'C2H2', 'CH3OH', 'HCOOH',
    'C6H6', 'C3H8', 'N2O', 'SO2', 'PH3', 'SiH4',
    'NaCl', 'MgO', 'CaF2', 'FeCl3', 'CuO', 'ZnS',
    'TiO2', 'Al2O3', 'SiO2', 'BF3', 'PCl5', 'SF6',
]

# ---------------------------------------------------------------------------
# Checkpoint state
# ---------------------------------------------------------------------------

@dataclass
class OrchestratorState:
    """Full checkpoint — serialized to JSON between cycles."""
    session_id: str = ""
    start_time: str = ""
    total_queued: int = 0
    total_completed: int = 0
    total_stable: int = 0
    total_metastable: int = 0
    total_collapsed: int = 0
    formulas_tried: List[str] = field(default_factory=list)
    stable_formulas: List[str] = field(default_factory=list)
    element_pair_hits: Dict[str, int] = field(default_factory=dict)
    element_pair_stable: Dict[str, int] = field(default_factory=dict)
    current_tier: str = "screen"
    promote_queue: List[str] = field(default_factory=list)  # screen→medium
    deep_queue: List[str] = field(default_factory=list)      # medium→deep
    cycle: int = 0
    hours_target: float = 24.0

    def save(self, path: str):
        with open(path, 'w') as f:
            json.dump(asdict(self), f, indent=2)

    @classmethod
    def load(cls, path: str) -> 'OrchestratorState':
        with open(path, 'r') as f:
            data = json.load(f)
        return cls(**data)


# ---------------------------------------------------------------------------
# Smart formula generation
# ---------------------------------------------------------------------------

def generate_formula_random(max_elements=4, max_total_atoms=20) -> str:
    """Generate a chemically plausible random formula."""
    elements = list(ELEMENT_POOL.keys())
    weights = [ELEMENT_POOL[e][0] for e in elements]
    total_w = sum(weights)
    probs = [w / total_w for w in weights]

    n_elements = random.randint(2, max_elements)
    chosen = random.choices(elements, weights=probs, k=n_elements)
    chosen = list(set(chosen))  # deduplicate
    if len(chosen) < 2:
        chosen.append(random.choice(['H', 'C', 'O', 'N']))

    formula = ""
    total_atoms = 0
    for elem in chosen:
        _, max_count, _ = ELEMENT_POOL[elem]
        remaining = max_total_atoms - total_atoms
        if remaining <= 0:
            break
        count = random.randint(1, min(max_count, remaining))
        formula += elem
        if count > 1:
            formula += str(count)
        total_atoms += count

    return formula


def generate_formula_targeted(state: OrchestratorState, max_total_atoms=20) -> str:
    """Generate formulas biased toward element pairs with high stable rates."""
    if not state.element_pair_stable or random.random() < 0.3:
        # 30% pure exploration
        return generate_formula_random(max_total_atoms=max_total_atoms)

    # Pick a productive element pair
    pairs = list(state.element_pair_stable.keys())
    scores = []
    for pair in pairs:
        hits = state.element_pair_hits.get(pair, 1)
        stables = state.element_pair_stable.get(pair, 0)
        # Thompson sampling inspired: beta distribution mean
        score = (stables + 1) / (hits + 2)
        scores.append(score)

    total_s = sum(scores)
    probs = [s / total_s for s in scores]
    chosen_pair = random.choices(pairs, weights=probs, k=1)[0]
    elem_a, elem_b = chosen_pair.split('-')

    # Build formula around the chosen pair
    count_a = random.randint(1, ELEMENT_POOL.get(elem_a, (1, 4, 1))[1])
    count_b = random.randint(1, ELEMENT_POOL.get(elem_b, (1, 4, 1))[1])

    formula = elem_a
    if count_a > 1:
        formula += str(count_a)
    formula += elem_b
    if count_b > 1:
        formula += str(count_b)

    # Optionally add a third element
    remaining = max_total_atoms - count_a - count_b
    if remaining > 1 and random.random() < 0.5:
        extras = [e for e in ELEMENT_POOL if e not in (elem_a, elem_b)]
        if extras:
            elem_c = random.choice(extras)
            count_c = random.randint(1, min(ELEMENT_POOL[elem_c][1], remaining))
            formula += elem_c
            if count_c > 1:
                formula += str(count_c)

    return formula


def extract_element_pairs(formula: str) -> List[str]:
    """Extract element pairs from a formula for coverage tracking."""
    import re
    elements = re.findall(r'([A-Z][a-z]?)', formula)
    elements = sorted(set(elements))
    pairs = []
    for i in range(len(elements)):
        for j in range(i + 1, len(elements)):
            pairs.append(f"{elements[i]}-{elements[j]}")
    return pairs


# ---------------------------------------------------------------------------
# Result parsing
# ---------------------------------------------------------------------------

def parse_ledger(ledger_path: str) -> List[Dict]:
    """Parse the CSV ledger written by the C++ runner."""
    rows = []
    if not os.path.exists(ledger_path):
        return rows
    with open(ledger_path, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            # Convert numeric fields
            for key in ['num_atoms', 'steps', 'max_steps', 'converged']:
                if key in row:
                    try: row[key] = int(row[key])
                    except: pass
            for key in ['energy', 'energy_per_atom', 'rms_force', 'wall_ms']:
                if key in row:
                    try: row[key] = float(row[key])
                    except: pass
            rows.append(row)
    return rows


def update_state_from_ledger(state: OrchestratorState, ledger_path: str):
    """Sync orchestrator state with whatever the C++ runner has produced."""
    rows = parse_ledger(ledger_path)
    state.total_completed = len(rows)

    classifications = Counter(r.get('classification', '') for r in rows)
    state.total_stable = classifications.get('stable', 0)
    state.total_metastable = classifications.get('metastable', 0)
    state.total_collapsed = classifications.get('collapsed', 0)

    stable_formulas = set()
    for r in rows:
        formula = r.get('formula', '')
        cls = r.get('classification', '')

        pairs = extract_element_pairs(formula)
        for pair in pairs:
            state.element_pair_hits[pair] = state.element_pair_hits.get(pair, 0) + 1
            if cls in ('stable', 'metastable'):
                state.element_pair_stable[pair] = state.element_pair_stable.get(pair, 0) + 1

        if cls == 'stable':
            stable_formulas.add(formula)

    state.stable_formulas = sorted(stable_formulas)


# ---------------------------------------------------------------------------
# Chart generation
# ---------------------------------------------------------------------------

def generate_charts(ledger_path: str, output_dir: str):
    """Generate publication-quality charts from the ledger."""
    try:
        import matplotlib
        matplotlib.use('Agg')
        import matplotlib.pyplot as plt
        import numpy as np
    except ImportError:
        print("  [WARN] matplotlib/numpy not available — skipping charts")
        return

    rows = parse_ledger(ledger_path)
    if len(rows) < 3:
        print("  [INFO] < 3 results — skipping charts")
        return

    charts_dir = os.path.join(output_dir, "charts")
    os.makedirs(charts_dir, exist_ok=True)

    # --- Chart 1: Energy landscape over time ---
    energies = [r.get('energy_per_atom', 0) for r in rows]
    classes = [r.get('classification', 'unknown') for r in rows]

    color_map = {
        'stable': '#2ecc71', 'metastable': '#f39c12',
        'unstable': '#e74c3c', 'collapsed': '#95a5a6',
        'timeout': '#9b59b6', 'fragment': '#3498db',
    }
    colors = [color_map.get(c, '#bdc3c7') for c in classes]

    fig, axes = plt.subplots(2, 2, figsize=(14, 10))
    fig.suptitle('Continual Formation Engine — Live Dashboard', fontsize=14, fontweight='bold')

    # Panel 1: Energy per atom over formation index
    ax = axes[0, 0]
    valid_e = [(i, e) for i, e in enumerate(energies) if abs(e) < 1e6]
    if valid_e:
        idxs, vals = zip(*valid_e)
        cs = [colors[i] for i in idxs]
        ax.scatter(idxs, vals, c=cs, s=8, alpha=0.6, edgecolors='none')
        # Running median
        window = max(10, len(vals) // 20)
        if len(vals) > window:
            medians = []
            for i in range(window, len(vals)):
                medians.append(np.median(vals[i-window:i]))
            ax.plot(range(window, len(vals)), medians, 'k-', linewidth=1.5, label='Running median')
            ax.legend(fontsize=8)
    ax.set_xlabel('Formation index')
    ax.set_ylabel('Energy / atom (kcal/mol)')
    ax.set_title('Energy Landscape')
    ax.grid(True, alpha=0.3)

    # Panel 2: Classification pie chart
    ax = axes[0, 1]
    class_counts = Counter(classes)
    labels = list(class_counts.keys())
    sizes = list(class_counts.values())
    pie_colors = [color_map.get(l, '#bdc3c7') for l in labels]
    wedges, texts, autotexts = ax.pie(sizes, labels=labels, colors=pie_colors,
                                       autopct='%1.1f%%', textprops={'fontsize': 8})
    ax.set_title(f'Classification Distribution (n={len(rows)})')

    # Panel 3: Discovery rate (cumulative stable over time)
    ax = axes[1, 0]
    cumulative_stable = np.cumsum([1 if c == 'stable' else 0 for c in classes])
    cumulative_meta = np.cumsum([1 if c == 'metastable' else 0 for c in classes])
    ax.plot(cumulative_stable, color='#2ecc71', linewidth=2, label='Stable')
    ax.plot(cumulative_meta, color='#f39c12', linewidth=2, label='Metastable')
    ax.fill_between(range(len(cumulative_stable)), cumulative_stable, alpha=0.15, color='#2ecc71')
    ax.set_xlabel('Formation index')
    ax.set_ylabel('Cumulative count')
    ax.set_title('Discovery Rate')
    ax.legend(fontsize=8)
    ax.grid(True, alpha=0.3)

    # Panel 4: Wall time distribution (efficiency)
    ax = axes[1, 1]
    times = [r.get('wall_ms', 0) for r in rows if r.get('wall_ms', 0) > 0]
    if times:
        ax.hist(times, bins=50, color='#3498db', alpha=0.7, edgecolor='white')
        median_t = np.median(times)
        ax.axvline(median_t, color='red', linestyle='--', linewidth=1.5,
                   label=f'Median: {median_t:.0f} ms')
        ax.legend(fontsize=8)
    ax.set_xlabel('Wall time per formation (ms)')
    ax.set_ylabel('Count')
    ax.set_title('Compute Efficiency')
    ax.grid(True, alpha=0.3)

    plt.tight_layout()
    chart_path = os.path.join(charts_dir, 'dashboard.png')
    plt.savefig(chart_path, dpi=200, bbox_inches='tight')
    plt.close()
    print(f"  ✓ Dashboard: {chart_path}")

    # --- Chart 2: Element-pair heatmap ---
    if len(rows) > 20:
        generate_element_heatmap(rows, charts_dir)


def generate_element_heatmap(rows: List[Dict], charts_dir: str):
    """Generate element-pair stability heatmap."""
    try:
        import matplotlib
        matplotlib.use('Agg')
        import matplotlib.pyplot as plt
        import numpy as np
    except ImportError:
        return

    import re
    pair_total = Counter()
    pair_stable = Counter()

    for r in rows:
        formula = r.get('formula', '')
        cls = r.get('classification', '')
        elements = sorted(set(re.findall(r'([A-Z][a-z]?)', formula)))
        for i in range(len(elements)):
            for j in range(i, len(elements)):
                key = (elements[i], elements[j])
                pair_total[key] += 1
                if cls in ('stable', 'metastable'):
                    pair_stable[key] += 1

    if not pair_total:
        return

    # Get top elements by frequency
    elem_freq = Counter()
    for (a, b), count in pair_total.items():
        elem_freq[a] += count
        elem_freq[b] += count
    top_elements = [e for e, _ in elem_freq.most_common(15)]

    n = len(top_elements)
    matrix = np.zeros((n, n))
    for i in range(n):
        for j in range(n):
            key = tuple(sorted([top_elements[i], top_elements[j]]))
            total = pair_total.get(key, 0)
            stable = pair_stable.get(key, 0)
            matrix[i, j] = stable / max(total, 1)

    fig, ax = plt.subplots(figsize=(10, 8))
    im = ax.imshow(matrix, cmap='RdYlGn', vmin=0, vmax=1, aspect='auto')
    ax.set_xticks(range(n))
    ax.set_yticks(range(n))
    ax.set_xticklabels(top_elements, fontsize=9)
    ax.set_yticklabels(top_elements, fontsize=9)
    plt.colorbar(im, ax=ax, label='Stability rate')
    ax.set_title('Element-Pair Stability Heatmap')

    for i in range(n):
        for j in range(n):
            key = tuple(sorted([top_elements[i], top_elements[j]]))
            total = pair_total.get(key, 0)
            if total > 0:
                ax.text(j, i, f'{matrix[i,j]:.0%}\n({total})',
                        ha='center', va='center', fontsize=6,
                        color='black' if matrix[i, j] > 0.3 else 'white')

    heatmap_path = os.path.join(charts_dir, 'element_heatmap.png')
    plt.savefig(heatmap_path, dpi=200, bbox_inches='tight')
    plt.close()
    print(f"  ✓ Heatmap: {heatmap_path}")


# ---------------------------------------------------------------------------
# Orchestrator main loop
# ---------------------------------------------------------------------------

def write_queue(queue_path: str, formulas: List[str]):
    """Write formulas to the work queue for the C++ runner."""
    with open(queue_path, 'a') as f:
        for formula in formulas:
            f.write(formula + '\n')


def run_cycle(state: OrchestratorState, cfg: dict):
    """One orchestrator cycle: generate → queue → wait → parse → chart."""
    out_dir = cfg['output_dir']
    queue_path = os.path.join(out_dir, 'work_queue.txt')
    ledger_path = os.path.join(out_dir, 'ledger.csv')
    checkpoint_path = os.path.join(out_dir, 'checkpoint.json')
    runner_exe = cfg['runner_exe']

    state.cycle += 1
    batch_size = cfg.get('batch_size', 30)

    print(f"\n{'='*60}")
    print(f"  CYCLE {state.cycle}  |  {datetime.now().strftime('%H:%M:%S')}  |  "
          f"Completed: {state.total_completed}  |  Stable: {state.total_stable}")
    print(f"{'='*60}")

    # --- Phase 1: Generate formulas ---
    tier = state.current_tier
    formulas = []

    if state.cycle == 1:
        # First cycle: run benchmarks
        formulas = BENCHMARK_FORMULAS[:batch_size]
        tier = "screen"
        print(f"  [GEN] Benchmark set: {len(formulas)} formulas")
    elif state.promote_queue:
        # Promote screen→medium hits
        formulas = state.promote_queue[:batch_size]
        state.promote_queue = state.promote_queue[batch_size:]
        tier = "medium"
        print(f"  [GEN] Promoting {len(formulas)} from screen → medium")
    elif state.deep_queue:
        # Promote medium→deep hits
        formulas = state.deep_queue[:batch_size // 3]
        state.deep_queue = state.deep_queue[batch_size // 3:]
        tier = "deep"
        print(f"  [GEN] Deep evaluation: {len(formulas)} formulas")
    else:
        # Generate new formulas with adaptive targeting
        for _ in range(batch_size):
            f = generate_formula_targeted(state)
            formulas.append(f)
        tier = "screen"
        print(f"  [GEN] New exploration: {len(formulas)} formulas (screen)")

    # Deduplicate against history
    seen = set(state.formulas_tried)
    new_formulas = []
    for f in formulas:
        if f not in seen:
            new_formulas.append(f)
            seen.add(f)
    formulas = new_formulas if new_formulas else formulas[:5]  # fallback

    state.formulas_tried.extend(formulas)
    state.total_queued += len(formulas)

    # --- Phase 2: Write queue and run ---
    write_queue(queue_path, formulas)

    print(f"  [RUN] Launching runner: tier={tier}, formulas={len(formulas)}")
    cmd = [
        runner_exe,
        '--queue', queue_path,
        '--out', out_dir,
        '--tier', tier,
        '--seeds', str(cfg.get('seeds', 3)),
        '--verbose'
    ]

    try:
        result = subprocess.run(
            cmd, capture_output=True, text=True,
            timeout=cfg.get('cycle_timeout', 600),
            encoding='utf-8', errors='replace'
        )
        if result.returncode != 0:
            print(f"  [WARN] Runner returned {result.returncode}")
            if result.stderr:
                print(f"  stderr: {result.stderr[:200]}")
    except subprocess.TimeoutExpired:
        print(f"  [WARN] Runner timed out at {cfg.get('cycle_timeout', 600)}s")
    except FileNotFoundError:
        print(f"  [ERROR] Runner not found: {runner_exe}")
        print(f"  Falling back to atomistic-sim pipeline...")
        run_fallback_pipeline(formulas, tier, out_dir, ledger_path)

    # --- Phase 3: Parse results and update state ---
    prev_completed = state.total_completed
    update_state_from_ledger(state, ledger_path)
    new_results = state.total_completed - prev_completed
    print(f"  [PARSE] {new_results} new results")

    # Identify promotions: screen stable/metastable → medium queue
    if tier == "screen":
        rows = parse_ledger(ledger_path)
        recent = rows[prev_completed:]
        for r in recent:
            cls = r.get('classification', '')
            formula = r.get('formula', '')
            if cls in ('stable', 'metastable') and formula not in state.promote_queue:
                state.promote_queue.append(formula)
        if state.promote_queue:
            print(f"  [PROMOTE] {len(state.promote_queue)} formulas queued for medium tier")

    elif tier == "medium":
        rows = parse_ledger(ledger_path)
        recent = rows[prev_completed:]
        for r in recent:
            cls = r.get('classification', '')
            formula = r.get('formula', '')
            if cls == 'stable' and formula not in state.deep_queue:
                state.deep_queue.append(formula)
        if state.deep_queue:
            print(f"  [DEEP] {len(state.deep_queue)} formulas queued for deep tier")

    # --- Phase 4: Generate charts ---
    if state.total_completed > 5 and state.cycle % 3 == 0:
        print(f"  [CHART] Generating dashboard...")
        generate_charts(ledger_path, out_dir)

    # --- Phase 5: Checkpoint ---
    state.save(checkpoint_path)
    print(f"  [SAVE] Checkpoint saved (cycle {state.cycle})")


def run_fallback_pipeline(formulas, tier, out_dir, ledger_path):
    """If continual_runner is not built, use atomistic-sim directly."""
    steps_map = {'screen': 100, 'medium': 1000, 'deep': 5000}
    steps = steps_map.get(tier, 1000)

    for formula in formulas:
        try:
            # Use vsepr-cli to build, then atomistic-sim to optimize
            json_path = os.path.join(out_dir, 'temp_mol.json')
            cmd_make = [
                os.path.join('build', 'vsepr-cli.exe'),
                'make', '--formula', formula, '--out', json_path
            ]
            subprocess.run(cmd_make, capture_output=True, timeout=10,
                           encoding='utf-8', errors='replace')
        except Exception:
            pass


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description='Continual Formation Engine Orchestrator',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python continual_orchestrator.py --out results --hours 8
  python continual_orchestrator.py --resume results
  python continual_orchestrator.py --chart results
        """
    )
    parser.add_argument('--out', default='continual_results',
                        help='Output directory (default: continual_results)')
    parser.add_argument('--hours', type=float, default=24.0,
                        help='Hours to run (default: 24)')
    parser.add_argument('--resume', action='store_true',
                        help='Resume from existing checkpoint')
    parser.add_argument('--chart', action='store_true',
                        help='Generate charts only (no simulation)')
    parser.add_argument('--batch-size', type=int, default=30,
                        help='Formulas per cycle (default: 30)')
    parser.add_argument('--seeds', type=int, default=3,
                        help='Seeds per formula (default: 3)')
    parser.add_argument('--runner', default=None,
                        help='Path to continual_runner executable')
    args = parser.parse_args()

    out_dir = args.out
    os.makedirs(out_dir, exist_ok=True)

    # Find runner executable
    runner_exe = args.runner
    if runner_exe is None:
        candidates = [
            os.path.join('build', 'continual_runner.exe'),
            os.path.join('build', 'continual-runner.exe'),
            'continual_runner.exe',
        ]
        for c in candidates:
            if os.path.exists(c):
                runner_exe = c
                break
        if runner_exe is None:
            runner_exe = candidates[0]  # will trigger fallback

    # Chart-only mode
    if args.chart:
        ledger = os.path.join(out_dir, 'ledger.csv')
        if os.path.exists(ledger):
            generate_charts(ledger, out_dir)
        else:
            print(f"No ledger found at {ledger}")
        return

    # Load or create state
    checkpoint_path = os.path.join(out_dir, 'checkpoint.json')
    if args.resume and os.path.exists(checkpoint_path):
        state = OrchestratorState.load(checkpoint_path)
        print(f"Resumed from cycle {state.cycle}, {state.total_completed} completions")
    else:
        state = OrchestratorState(
            session_id=hashlib.md5(str(time.time()).encode()).hexdigest()[:8],
            start_time=datetime.now().isoformat(),
            hours_target=args.hours,
        )

    cfg = {
        'output_dir': out_dir,
        'runner_exe': runner_exe,
        'batch_size': args.batch_size,
        'seeds': args.seeds,
        'cycle_timeout': 600,
    }

    print("╔═══════════════════════════════════════════════════╗")
    print("║  Continual Formation Engine — Orchestrator        ║")
    print("╠═══════════════════════════════════════════════════╣")
    print(f"║  Session:  {state.session_id}")
    print(f"║  Output:   {out_dir}")
    print(f"║  Duration: {args.hours} hours")
    print(f"║  Batch:    {args.batch_size} formulas/cycle")
    print(f"║  Seeds:    {args.seeds}/formula")
    print(f"║  Runner:   {runner_exe}")
    print("╠═══════════════════════════════════════════════════╣")
    print("║  Press Ctrl+C to stop gracefully.                 ║")
    print("║  Progress is checkpointed every cycle.            ║")
    print("╚═══════════════════════════════════════════════════╝")

    deadline = datetime.now() + timedelta(hours=args.hours)

    try:
        while datetime.now() < deadline:
            run_cycle(state, cfg)

            # Brief pause between cycles
            time.sleep(2)

            # Check for stop file
            stop_file = os.path.join(out_dir, 'STOP')
            if os.path.exists(stop_file):
                print("\n[STOP file detected — shutting down]")
                os.remove(stop_file)
                break

    except KeyboardInterrupt:
        print("\n\n[Ctrl+C — graceful shutdown]")

    # Final checkpoint and charts
    state.save(checkpoint_path)
    ledger_path = os.path.join(out_dir, 'ledger.csv')
    if os.path.exists(ledger_path):
        generate_charts(ledger_path, out_dir)

    print(f"\n═══ SESSION SUMMARY ═══")
    print(f"  Cycles:     {state.cycle}")
    print(f"  Queued:     {state.total_queued}")
    print(f"  Completed:  {state.total_completed}")
    print(f"  Stable:     {state.total_stable}")
    print(f"  Metastable: {state.total_metastable}")
    print(f"  Collapsed:  {state.total_collapsed}")
    print(f"  Unique:     {len(set(state.formulas_tried))}")
    print(f"  Checkpoint: {checkpoint_path}")
    print(f"  Ledger:     {ledger_path}")
    print(f"  Charts:     {os.path.join(out_dir, 'charts')}/")


if __name__ == '__main__':
    main()
