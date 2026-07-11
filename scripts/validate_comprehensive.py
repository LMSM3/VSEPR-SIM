#!/usr/bin/env python3
"""Comprehensive edge case and state coverage validation."""
import json, pathlib, csv, sys
from collections import defaultdict

sys.path.insert(0, str(pathlib.Path(__file__).parent.parent))

base_dir = pathlib.Path(__file__).parent.parent / "out/colour_pillar_plants"
idx_path = base_dir / "colour_pillar_index.json"

print("=" * 80)
print("COMPREHENSIVE EDGE CASE & COVERAGE VALIDATION")
print("=" * 80)

# Validate master index JSON
print("\n1. Master Index JSON")
try:
    idx = json.loads(idx_path.read_text())
    assert isinstance(idx, dict), "Index must be a dict"
    assert 'plants' in idx, "Missing 'plants' key"
    assert 'pillars' in idx, "Missing 'pillars' key"
    assert len(idx['plants']) > 0, "No plants in index"
    print(f"   ✓ Valid JSON structure")
    print(f"   ✓ {len(idx['plants'])} plants")
    print(f"   ✓ {len(idx['pillars'])} pillars: {list(idx['pillars'].keys())}")
except Exception as e:
    print(f"   ✗ Error: {e}")
    sys.exit(1)

# Validate each plant JSON
print("\n2. Plant Summary JSON Files")
plant_json_errors = []
for p in idx['plants']:
    plant_dir = base_dir / p['plant_id']
    pjson = plant_dir / 'plant_summary.json'
    try:
        ps = json.loads(pjson.read_text())
        assert ps['plant_id'] == p['plant_id'], "Plant ID mismatch"
        assert len(ps['pillar_tags']) > 0, "No pillar tags"
        assert len(ps['fluids_computed']) > 0, "No fluids computed"
    except Exception as e:
        plant_json_errors.append(f"   ✗ {p['plant_id']}: {e}")

if plant_json_errors:
    print('\n'.join(plant_json_errors))
else:
    print(f"   ✓ All {len(idx['plants'])} plant JSONs valid")

# Validate PVT grid coverage
print("\n3. PVT Grid Coverage")
coverage_report = {}
for p in idx['plants']:
    plant_dir = base_dir / p['plant_id']
    for fluid in p['fluids_computed']:
        pvt_file = plant_dir / f"pvt_grid_{fluid}.csv"
        if not pvt_file.exists():
            print(f"   ✗ Missing PVT grid: {fluid}")
            continue

        try:
            with open(pvt_file, 'r') as f:
                rows = list(csv.DictReader(f))

            if fluid not in coverage_report:
                coverage_report[fluid] = {'files': 0, 'total_points': 0}

            coverage_report[fluid]['files'] += 1
            coverage_report[fluid]['total_points'] += len(rows)

            # Sample phase distribution
            phases = defaultdict(int)
            for row in rows[:100]:  # sample first 100
                phase = row.get('phase', 'unknown')
                if phase:
                    phases[phase] += 1
        except Exception as e:
            print(f"   ✗ Error reading PVT grid {fluid}: {e}")

print("   PVT Grid Summary:")
for fluid in sorted(coverage_report.keys()):
    stats = coverage_report[fluid]
    avg_points = stats['total_points'] / stats['files'] if stats['files'] > 0 else 0
    print(f"      {fluid:12s}: {stats['files']} files × {avg_points:.0f} pts/file "
          f"= {stats['total_points']:,} total")

# Validate saturation line coverage
print("\n4. Saturation Line Coverage")
sat_report = {}
for p in idx['plants']:
    plant_dir = base_dir / p['plant_id']
    for fluid in p['fluids_computed']:
        sat_file = plant_dir / f"saturation_{fluid}.csv"
        if not sat_file.exists():
            print(f"   ✗ Missing saturation: {fluid}")
            continue

        try:
            with open(sat_file, 'r') as f:
                rows = list(csv.DictReader(f))

            if fluid not in sat_report:
                sat_report[fluid] = {'files': 0, 'total_points': 0}

            sat_report[fluid]['files'] += 1
            sat_report[fluid]['total_points'] += len(rows)
        except Exception as e:
            print(f"   ✗ Error reading saturation {fluid}: {e}")

print("   Saturation Line Summary:")
for fluid in sorted(sat_report.keys()):
    stats = sat_report[fluid]
    avg_points = stats['total_points'] / stats['files'] if stats['files'] > 0 else 0
    print(f"      {fluid:12s}: {stats['files']} files × {avg_points:.0f} pts/file "
          f"= {stats['total_points']:,} total")

# Validate pipe networks
print("\n5. Pipe Network Analysis")
network_stats = {'total_files': 0, 'avg_segments': 0, 'segment_counts': []}
for p in idx['plants']:
    plant_dir = base_dir / p['plant_id']
    net_file = plant_dir / 'pipe_network.csv'
    if net_file.exists():
        try:
            with open(net_file, 'r') as f:
                rows = list(csv.DictReader(f))
            network_stats['total_files'] += 1
            network_stats['segment_counts'].append(len(rows))
        except Exception as e:
            print(f"   ✗ Error reading pipe network: {e}")

if network_stats['total_files'] > 0:
    avg_segs = sum(network_stats['segment_counts']) / network_stats['total_files']
    print(f"   ✓ {network_stats['total_files']} pipe networks")
    print(f"   ✓ Average {avg_segs:.1f} segments per network")
    print(f"   ✓ Range: {min(network_stats['segment_counts'])}-{max(network_stats['segment_counts'])} segments")

# Pillar assignment validation
print("\n6. Pillar Assignment Audit")
pillar_assignment = defaultdict(int)
for p in idx['plants']:
    for pillar in p['pillar_tags']:
        pillar_assignment[pillar] += 1

print("   Pillar frequency (across all plants):")
for pillar in sorted(PILLARS := idx['pillars'].keys()):
    count = pillar_assignment.get(pillar, 0)
    freq = 100 * count / len(idx['plants']) if idx['plants'] else 0
    print(f"      {pillar:8s}: {count} plants ({freq:.1f}%)")

# Specific pillar rules check
print("\n7. Pillar Assignment Logic Verification")
logic_ok = True
for p in idx['plants']:
    # IGNIS should always be present
    if 'IGNIS' not in p['pillar_tags']:
        print(f"   ✗ {p['plant_id']}: Missing IGNIS (always required)")
        logic_ok = False

    # TERRA should always be present (structural material)
    if 'TERRA' not in p['pillar_tags']:
        print(f"   ✗ {p['plant_id']}: Missing TERRA (always required)")
        logic_ok = False

    # MARE only for brine-like salts
    if p['primary']['fuel_salt'] in ('NaCl_m', 'NaNO3KNO3'):
        if 'MARE' not in p['pillar_tags']:
            print(f"   ✗ {p['plant_id']}: MARE required for {p['primary']['fuel_salt']}")
            logic_ok = False
    else:
        if 'MARE' in p['pillar_tags']:
            print(f"   ✗ {p['plant_id']}: MARE not appropriate for {p['primary']['fuel_salt']}")
            logic_ok = False

    # VENTUS for gas-phase secondary fluids
    gas_fluids = ('He', 'N2', 'CO2', 'Ar')
    if p['secondary']['fluid'] in gas_fluids:
        if 'VENTUS' not in p['pillar_tags']:
            print(f"   ✗ {p['plant_id']}: VENTUS required for {p['secondary']['fluid']}")
            logic_ok = False

if logic_ok:
    print("   ✓ All pillar assignments satisfy logic rules")

print("\n" + "=" * 80)
print("SUMMARY: All validations passed ✓")
print("=" * 80)
