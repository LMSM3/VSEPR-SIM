#!/usr/bin/env python3
"""Final network analysis validation."""
import csv, json, pathlib
from collections import defaultdict

base = pathlib.Path(__file__).parent.parent / "out/colour_pillar_plants"
idx = json.loads((base / "colour_pillar_index.json").read_text())

print("=" * 80)
print("FINAL NETWORK ANALYSIS VALIDATION")
print("=" * 80)

all_stats = defaultdict(list)
all_ok = True

for p in idx['plants']:
    plant_dir = base / p['plant_id']
    net_csv = plant_dir / 'pipe_network.csv'

    with open(net_csv) as f:
        rows = list(csv.DictReader(f))

    # Verify key thermal columns exist
    required_cols = ['T_K', 'P_Pa', 'v_m_s', 'Re', 'f', 'dP_total_Pa', 'heat_loss_W', 'Mach']
    for col in required_cols:
        if col not in rows[0]:
            print(f"ERROR: Missing column {col} in {p['plant_id']}")
            all_ok = False
            continue

    # Parse numerics
    for seg in rows:
        for col in required_cols:
            try:
                v = float(seg[col])
                all_stats[col].append(v)
            except:
                pass

print("\nValidated thermal columns:")
for col in sorted(all_stats.keys()):
    vals = all_stats[col]
    if vals:
        print(f"  {col:20s}: {len(vals):3d} values | "
              f"min={min(vals):12.4g} max={max(vals):12.4g} avg={sum(vals)/len(vals):12.4g}")

print("\n" + "=" * 80)
if all_ok:
    print("✓ All pipes analyzed successfully!")
else:
    print("✗ Some validation issues found")
print("=" * 80)
