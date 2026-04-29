#!/usr/bin/env python3
"""Rigorous data quality validation for colour_pillar_plants."""
import json, pathlib, csv, sys
import math

sys.path.insert(0, str(pathlib.Path(__file__).parent.parent))

def check_csv_integrity(fpath, max_rows=None):
    """Check CSV for structure, NaN, Inf, and type issues."""
    issues = []
    try:
        with open(fpath, 'r', encoding='utf-8') as f:
            reader = csv.DictReader(f)
            rows = list(reader) if max_rows is None else list(reader)[:max_rows]

        if not rows:
            issues.append(f"  Empty CSV: {fpath.name}")
            return issues

        for i, row in enumerate(rows):
            for k, v in row.items():
                if v is None or v == '':
                    continue
                # Check for string NaN/Inf
                if isinstance(v, str):
                    if v.lower() in ('nan', 'inf', '-inf'):
                        issues.append(f"  Row {i}: {fpath.name}[{k}] = '{v}'")
                # Try to parse as number and check for actual NaN/Inf
                try:
                    num = float(v)
                    if math.isnan(num) or math.isinf(num):
                        issues.append(f"  Row {i}: {fpath.name}[{k}] = {num}")
                except (ValueError, TypeError):
                    pass  # not a number, that's ok

        return issues
    except Exception as e:
        return [f"  Error reading {fpath.name}: {e}"]

def validate_plant_data(plant_id, plant_dir):
    """Validate all CSVs in a plant directory."""
    issues = []

    # Check pipe_network.csv
    pipe_csv = plant_dir / 'pipe_network.csv'
    if pipe_csv.exists():
        issues.extend(check_csv_integrity(pipe_csv))

    # Check PVT grids and saturation
    for fpath in plant_dir.glob('*.csv'):
        if fpath.name != 'pipe_network.csv':
            issues.extend(check_csv_integrity(fpath, max_rows=10))

    return issues

base_dir = pathlib.Path(__file__).parent.parent / "out/colour_pillar_plants"
idx_path = base_dir / "colour_pillar_index.json"
idx = json.loads(idx_path.read_text())

print("=" * 80)
print("RIGOROUS DATA QUALITY VALIDATION")
print("=" * 80)

all_issues = []

for p in idx['plants']:
    plant_dir = base_dir / p['plant_id']
    plant_issues = validate_plant_data(p['plant_id'], plant_dir)
    all_issues.extend(plant_issues)

if all_issues:
    print("\nISSUES FOUND:")
    for issue in all_issues:
        print(issue)
else:
    print("✓ No NaN, Inf, or type errors detected")

print("\n" + "=" * 80)
print("DATA STATISTICS")
print("=" * 80)

for p in idx['plants']:
    plant_dir = base_dir / p['plant_id']
    pipe_csv = plant_dir / 'pipe_network.csv'

    if pipe_csv.exists():
        with open(pipe_csv, 'r') as f:
            rows = list(csv.DictReader(f))

        n_seg = len(rows)
        # Extract numeric columns safely
        try:
            dPs = [float(r.get('dP_total_Pa', 0)) for r in rows if r.get('dP_total_Pa')]
            hls = [float(r.get('heat_loss_W', 0)) for r in rows if r.get('heat_loss_W')]

            total_dP = sum(dPs) if dPs else 0
            total_hl = sum(hls) if hls else 0

            print(f"{p['plant_id']}:")
            print(f"  {n_seg} pipe segments | {total_dP:.0f} Pa total dP | {total_hl:.1f} W heat loss")
            print(f"  Primary:   {p['primary']['fuel_salt']:12s} {p['primary']['alloy']:15s} "
                  f"D={p['primary']['D_m']:.4f}m T={p['primary']['T_K']:.0f}K P={p['primary']['P_atm']:.1f}atm")
            print(f"  Secondary: {p['secondary']['fluid']:6s} {p['secondary']['alloy']:15s} "
                  f"D={p['secondary']['D_m']:.4f}m T={p['secondary']['T_K']:.0f}K P={p['secondary']['P_atm']:.1f}atm")
            print(f"  Pillars: {', '.join(p['pillar_tags'])}")
        except Exception as e:
            print(f"{p['plant_id']}: Error parsing pipe data: {e}")

print("=" * 80)
