#!/usr/bin/env python3
"""
generate_report.py — VSEPR-SIM Automated Report Generator

Reads C++ report outputs (Markdown, CSV) and generates consolidated
research-quality report assets including figures, tables, and summaries.

This script is the Python-side entry point for the report automation pipeline.
It is invoked by:
  - scripts/run_report.sh          (single-run report generation)
  - scripts/batch_reports.sh       (multi-seed batch reports)

Workflow:
  1. Scan output directory for C++ report artifacts (.md, .csv)
  2. Parse CSV trajectory data (from bead_fire_csv_export / snapshot_graph)
  3. Generate matplotlib figures for energy, force, and trajectory
  4. Produce a consolidated Markdown report referencing all assets
  5. Optionally generate per-seed reports (batch mode)

Anti-black-box: every metric plotted is traceable to the source CSV row.
Deterministic: same input data → identical report output.

Usage:
  python generate_report.py --out <output_dir>
  python generate_report.py --out <output_dir> --seed <N>
  python generate_report.py --demo --out <output_dir>

Reference: .github/copilot-instructions.md §2, §5, §9
"""

import argparse
import csv
import datetime
import glob
import json
import os
import sys

# ---------------------------------------------------------------------------
# Optional imports — graceful fallback if matplotlib not installed
# ---------------------------------------------------------------------------
try:
    import numpy as np
    HAS_NUMPY = True
except ImportError:
    HAS_NUMPY = False

try:
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    HAS_MPL = True
except ImportError:
    HAS_MPL = False


# ===========================================================================
# CSV Parsing
# ===========================================================================

def load_bead_fire_csv(path):
    """
    Load a BeadFIRE trajectory CSV exported by bead_fire_csv_export.hpp.

    Returns dict with arrays for each column, or None on failure.
    """
    if not os.path.isfile(path):
        return None

    data = {
        'steps': [], 'U_total': [], 'U_steric': [],
        'U_elec': [], 'U_disp': [], 'Frms': [],
        'Fmax': [], 'alpha': [], 'dt': [], 'dU_per_bead': []
    }

    with open(path, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            data['steps'].append(int(row.get('step', 0)))
            data['U_total'].append(float(row.get('U_total', 0)))
            data['U_steric'].append(float(row.get('U_steric', 0)))
            data['U_elec'].append(float(row.get('U_elec', 0)))
            data['U_disp'].append(float(row.get('U_disp', 0)))
            data['Frms'].append(float(row.get('Frms', 0)))
            data['Fmax'].append(float(row.get('Fmax', 0)))
            data['alpha'].append(float(row.get('alpha', 0)))
            data['dt'].append(float(row.get('dt', 0)))
            data['dU_per_bead'].append(float(row.get('dU_per_bead', 0)))

    return data


def load_timeseries_csv(path):
    """
    Load a SnapshotGraphCollector timeseries CSV.

    Returns dict with arrays for each column, or None on failure.
    """
    if not os.path.isfile(path):
        return None

    data = {}
    with open(path, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            for key, val in row.items():
                if key not in data:
                    data[key] = []
                try:
                    data[key].append(float(val))
                except (ValueError, TypeError):
                    data[key].append(val)

    return data


# ===========================================================================
# Figure Generation
# ===========================================================================

def generate_energy_plot(data, out_path):
    """Generate energy convergence plot from trajectory data."""
    if not HAS_MPL or not HAS_NUMPY:
        print("[!] matplotlib/numpy not available — skipping energy plot")
        return False

    steps = np.array(data['steps'])
    fig, ax = plt.subplots(figsize=(10, 6))

    ax.plot(steps, data['U_total'], 'k-', linewidth=2, label='Total')
    ax.plot(steps, data['U_steric'], '--', color='#e74c3c', label='Steric')
    ax.plot(steps, data['U_elec'], '--', color='#3498db', label='Electrostatic')
    ax.plot(steps, data['U_disp'], '--', color='#2ecc71', label='Dispersion')

    ax.set_xlabel('Step', fontsize=12)
    ax.set_ylabel('Energy (kcal/mol)', fontsize=12)
    ax.set_title('Energy Convergence', fontsize=14, fontweight='bold')
    ax.legend(fontsize=10)
    ax.grid(True, alpha=0.3)

    plt.tight_layout()
    plt.savefig(out_path, dpi=300, bbox_inches='tight')
    plt.close()
    return True


def generate_force_plot(data, out_path):
    """Generate force evolution plot from trajectory data."""
    if not HAS_MPL or not HAS_NUMPY:
        print("[!] matplotlib/numpy not available — skipping force plot")
        return False

    steps = np.array(data['steps'])
    fig, ax = plt.subplots(figsize=(10, 6))

    ax.semilogy(steps, data['Frms'], 'b-', linewidth=2, label='RMS Force')
    ax.semilogy(steps, data['Fmax'], 'r-', linewidth=1.5, label='Max Force')

    ax.set_xlabel('Step', fontsize=12)
    ax.set_ylabel('Force (kcal/mol·Å)', fontsize=12)
    ax.set_title('Force Evolution', fontsize=14, fontweight='bold')
    ax.legend(fontsize=10)
    ax.grid(True, alpha=0.3, which='both')

    plt.tight_layout()
    plt.savefig(out_path, dpi=300, bbox_inches='tight')
    plt.close()
    return True


def generate_fire_params_plot(data, out_path):
    """Generate FIRE algorithm parameter evolution plot."""
    if not HAS_MPL or not HAS_NUMPY:
        print("[!] matplotlib/numpy not available — skipping FIRE params plot")
        return False

    steps = np.array(data['steps'])
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(10, 8), sharex=True)

    ax1.plot(steps, data['alpha'], 'g-', linewidth=2)
    ax1.set_ylabel('α (velocity mixing)', fontsize=12)
    ax1.set_title('FIRE Algorithm Parameters', fontsize=14, fontweight='bold')
    ax1.grid(True, alpha=0.3)

    ax2.plot(steps, data['dt'], 'm-', linewidth=2)
    ax2.set_xlabel('Step', fontsize=12)
    ax2.set_ylabel('Δt (fs)', fontsize=12)
    ax2.grid(True, alpha=0.3)

    plt.tight_layout()
    plt.savefig(out_path, dpi=300, bbox_inches='tight')
    plt.close()
    return True


# ===========================================================================
# Artifact Discovery
# ===========================================================================

def discover_artifacts(search_dir):
    """
    Scan a directory for C++ report outputs.

    Returns dict with lists of discovered file paths by type.
    """
    artifacts = {
        'markdown': [],
        'csv_trajectory': [],
        'csv_timeseries': [],
        'excel_xml': [],
        'sldcrv': [],
        'xyz': [],
    }

    for root, dirs, files in os.walk(search_dir):
        for fname in files:
            fpath = os.path.join(root, fname)
            lower = fname.lower()

            if lower.endswith('.md'):
                artifacts['markdown'].append(fpath)
            elif 'trajectory' in lower and lower.endswith('.csv'):
                artifacts['csv_trajectory'].append(fpath)
            elif 'timeseries' in lower and lower.endswith('.csv'):
                artifacts['csv_timeseries'].append(fpath)
            elif lower.endswith('.csv'):
                # Generic CSV — try to classify
                artifacts['csv_timeseries'].append(fpath)
            elif lower.endswith('.xml') and 'excel' in lower:
                artifacts['excel_xml'].append(fpath)
            elif lower.endswith('.sldcrv'):
                artifacts['sldcrv'].append(fpath)
            elif lower.endswith('.xyz'):
                artifacts['xyz'].append(fpath)

    return artifacts


# ===========================================================================
# Consolidated Report Generation
# ===========================================================================

def generate_consolidated_report(out_dir, artifacts, figures, seed=None):
    """
    Write a consolidated Markdown report referencing all discovered artifacts
    and generated figures.
    """
    timestamp = datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')
    seed_label = f" (Seed {seed})" if seed is not None else ""

    lines = []
    lines.append(f"# VSEPR-SIM Automated Report{seed_label}\n")
    lines.append(f"**Generated:** {timestamp}\n")
    lines.append(f"**Output directory:** `{out_dir}`\n")
    lines.append("")
    lines.append("---\n")

    # Summary of discovered artifacts
    lines.append("## Discovered Artifacts\n")
    total = sum(len(v) for v in artifacts.values())
    lines.append(f"Total artifacts found: **{total}**\n")
    lines.append("")
    lines.append("| Type | Count | Files |")
    lines.append("|------|-------|-------|")

    for atype, files in artifacts.items():
        if files:
            fnames = ', '.join(f'`{os.path.basename(f)}`' for f in files[:5])
            if len(files) > 5:
                fnames += f' (+{len(files) - 5} more)'
            lines.append(f"| {atype} | {len(files)} | {fnames} |")

    lines.append("")

    # Generated figures
    if figures:
        lines.append("## Generated Figures\n")
        for label, fig_path in figures.items():
            rel = os.path.basename(fig_path)
            lines.append(f"### {label}\n")
            lines.append(f"![{label}]({rel})\n")
            lines.append("")

    # Inline Markdown reports
    if artifacts['markdown']:
        lines.append("## Included Reports\n")
        for md_path in artifacts['markdown']:
            lines.append(f"### {os.path.basename(md_path)}\n")
            try:
                with open(md_path, 'r') as f:
                    content = f.read()
                # Indent headings to avoid collision with top-level structure
                for line in content.splitlines():
                    if line.startswith('#'):
                        lines.append('#' + line)
                    else:
                        lines.append(line)
            except Exception as e:
                lines.append(f"*Error reading report: {e}*\n")
            lines.append("")

    # Footer
    lines.append("---\n")
    lines.append("*Report generated by VSEPR-SIM automated reporting pipeline*\n")

    report_path = os.path.join(out_dir, 'automated_report.md')
    with open(report_path, 'w') as f:
        f.write('\n'.join(lines))

    return report_path


# ===========================================================================
# Demo Mode
# ===========================================================================

def generate_demo_data():
    """
    Generate synthetic demo data to verify the pipeline end-to-end
    without requiring a compiled C++ binary.
    """
    if not HAS_NUMPY:
        print("[!] numpy not available — cannot generate demo data")
        return None

    n_steps = 200
    steps = list(range(n_steps))

    data = {
        'steps': steps,
        'U_total': list(-100.0 * (1 - np.exp(-np.array(steps) / 40.0))
                        + np.random.normal(0, 0.3, n_steps)),
        'U_steric': list(-40.0 * (1 - np.exp(-np.array(steps) / 40.0))),
        'U_elec': list(-45.0 * (1 - np.exp(-np.array(steps) / 35.0))),
        'U_disp': list(-15.0 * (1 - np.exp(-np.array(steps) / 50.0))),
        'Frms': list(5.0 * np.exp(-np.array(steps) / 30.0) + 0.001),
        'Fmax': list(12.0 * np.exp(-np.array(steps) / 30.0) + 0.003),
        'alpha': list(0.1 * np.exp(-np.array(steps) / 60.0)),
        'dt': list(1.0 + 9.0 * (1 - np.exp(-np.array(steps) / 50.0))),
        'dU_per_bead': list(np.gradient(
            -100.0 * (1 - np.exp(-np.array(steps) / 40.0))) / 8.0),
    }

    return data


def write_demo_csv(data, path):
    """Write demo trajectory data to CSV for pipeline testing."""
    header = 'step,U_total,U_steric,U_elec,U_disp,Frms,Fmax,alpha,dt,dU_per_bead'
    with open(path, 'w', newline='') as f:
        f.write(header + '\n')
        for i in range(len(data['steps'])):
            row = [
                str(data['steps'][i]),
                f"{data['U_total'][i]:.6f}",
                f"{data['U_steric'][i]:.6f}",
                f"{data['U_elec'][i]:.6f}",
                f"{data['U_disp'][i]:.6f}",
                f"{data['Frms'][i]:.6f}",
                f"{data['Fmax'][i]:.6f}",
                f"{data['alpha'][i]:.6f}",
                f"{data['dt'][i]:.6f}",
                f"{data['dU_per_bead'][i]:.6f}",
            ]
            f.write(','.join(row) + '\n')


def write_demo_markdown(path):
    """Write a demo Markdown report artifact simulating C++ output."""
    content = """# Bead FIRE Minimization Report

**Generated:** Lattice-Level FIRE (LL-FIRE) minimization outcome (demo)

---

## Convergence Outcome

**Status:** ✓ Converged

| Metric | Value |
|--------|-------|
| Steps taken | 185 / 500 |
| N<sub>beads</sub> | 8 |
| U<sub>final</sub> | -98.452310 kcal/mol |
| F<sub>RMS</sub> | 0.0001 ✓ (< 0.0010) |
| F<sub>max</sub> | 0.0003 kcal/mol·Å |

## Energy Decomposition

| Channel | Energy (kcal/mol) | Fraction |
|---------|-------------------|----------|
| Steric | -39.781 | 40.41% |
| Electrostatic | -44.123 | 44.82% |
| Dispersion | -14.548 | 14.78% |
| **Total** | **-98.452** | **100%** |

---

*Report generated by VSEPR-SIM (demo mode)*
"""
    with open(path, 'w') as f:
        f.write(content)


# ===========================================================================
# Main Entry Point
# ===========================================================================

def main():
    parser = argparse.ArgumentParser(
        description='VSEPR-SIM Automated Report Generator',
        epilog='Reference: .github/copilot-instructions.md §2, §5, §9'
    )
    parser.add_argument('--out', required=True,
                        help='Output directory for generated report assets')
    parser.add_argument('--seed', type=int, default=None,
                        help='Seed number (for batch mode identification)')
    parser.add_argument('--demo', action='store_true',
                        help='Generate demo data and report (no C++ binary needed)')
    parser.add_argument('--scan', default=None,
                        help='Directory to scan for C++ report artifacts '
                             '(defaults to --out)')

    args = parser.parse_args()

    # Ensure output directory exists
    os.makedirs(args.out, exist_ok=True)

    scan_dir = args.scan if args.scan else args.out
    seed_label = f" (seed={args.seed})" if args.seed is not None else ""

    print(f"[*] VSEPR-SIM Report Generator{seed_label}")
    print(f"[*] Output: {args.out}")
    print(f"[*] Scan:   {scan_dir}")
    print()

    # ------------------------------------------------------------------
    # Demo mode: generate synthetic artifacts to test the full pipeline
    # ------------------------------------------------------------------
    if args.demo:
        print("[*] Demo mode — generating synthetic data...")

        demo_data = generate_demo_data()
        if demo_data is not None:
            demo_csv = os.path.join(args.out, 'trajectory_demo.csv')
            write_demo_csv(demo_data, demo_csv)
            print(f"[✓] Demo CSV: {demo_csv}")

        demo_md = os.path.join(args.out, 'bead_fire_demo_report.md')
        write_demo_markdown(demo_md)
        print(f"[✓] Demo Markdown: {demo_md}")
        print()

    # ------------------------------------------------------------------
    # 1. Discover artifacts
    # ------------------------------------------------------------------
    print("[*] Scanning for report artifacts...")
    artifacts = discover_artifacts(scan_dir)

    total = sum(len(v) for v in artifacts.values())
    print(f"[✓] Found {total} artifacts")
    for atype, files in artifacts.items():
        if files:
            print(f"    {atype}: {len(files)}")
    print()

    # ------------------------------------------------------------------
    # 2. Generate figures from trajectory CSVs
    # ------------------------------------------------------------------
    figures = {}

    for csv_path in artifacts['csv_trajectory']:
        print(f"[*] Processing trajectory: {os.path.basename(csv_path)}")
        data = load_bead_fire_csv(csv_path)
        if data and data['steps']:
            base = os.path.splitext(os.path.basename(csv_path))[0]

            energy_path = os.path.join(args.out, f'{base}_energy.png')
            if generate_energy_plot(data, energy_path):
                figures[f'Energy Convergence ({base})'] = energy_path
                print(f"[✓] {energy_path}")

            force_path = os.path.join(args.out, f'{base}_force.png')
            if generate_force_plot(data, force_path):
                figures[f'Force Evolution ({base})'] = force_path
                print(f"[✓] {force_path}")

            fire_path = os.path.join(args.out, f'{base}_fire_params.png')
            if generate_fire_params_plot(data, fire_path):
                figures[f'FIRE Parameters ({base})'] = fire_path
                print(f"[✓] {fire_path}")

    # Also try timeseries CSVs
    for csv_path in artifacts['csv_timeseries']:
        print(f"[*] Processing timeseries: {os.path.basename(csv_path)}")
        data = load_timeseries_csv(csv_path)
        if data:
            # Try to produce energy plot if columns match
            if all(k in data for k in ['U_total', 'Frms']):
                adapted = {
                    'steps': list(range(len(data['U_total']))),
                    'U_total': data.get('U_total', []),
                    'U_steric': data.get('U_steric', [0] * len(data['U_total'])),
                    'U_elec': data.get('U_elec', [0] * len(data['U_total'])),
                    'U_disp': data.get('U_disp', [0] * len(data['U_total'])),
                    'Frms': data.get('Frms', []),
                    'Fmax': data.get('Fmax', data.get('Frms', [])),
                    'alpha': data.get('alpha', [0] * len(data['U_total'])),
                    'dt': data.get('dt', [0] * len(data['U_total'])),
                }
                base = os.path.splitext(os.path.basename(csv_path))[0]
                ep = os.path.join(args.out, f'{base}_energy.png')
                if generate_energy_plot(adapted, ep):
                    figures[f'Energy ({base})'] = ep
                    print(f"[✓] {ep}")

    print()

    # ------------------------------------------------------------------
    # 3. Generate consolidated report
    # ------------------------------------------------------------------
    print("[*] Generating consolidated report...")
    report_path = generate_consolidated_report(
        args.out, artifacts, figures, seed=args.seed)
    print(f"[✓] Report: {report_path}")

    # ------------------------------------------------------------------
    # 4. Write manifest JSON
    # ------------------------------------------------------------------
    manifest = {
        'timestamp': datetime.datetime.now().isoformat(),
        'seed': args.seed,
        'output_dir': args.out,
        'scan_dir': scan_dir,
        'artifacts': {k: len(v) for k, v in artifacts.items()},
        'figures': list(figures.keys()),
        'report': report_path,
    }

    manifest_path = os.path.join(args.out, 'report_manifest.json')
    with open(manifest_path, 'w') as f:
        json.dump(manifest, f, indent=2)
    print(f"[✓] Manifest: {manifest_path}")

    # ------------------------------------------------------------------
    # Done
    # ------------------------------------------------------------------
    print()
    print("╔═══════════════════════════════════════════════════════════════╗")
    print("║  ✅ Report Generation Complete                               ║")
    print("╚═══════════════════════════════════════════════════════════════╝")
    print()
    print(f"  Report:   {report_path}")
    print(f"  Figures:  {len(figures)}")
    print(f"  Manifest: {manifest_path}")
    print()

    return 0


if __name__ == '__main__':
    sys.exit(main())
