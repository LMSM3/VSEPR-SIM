#!/usr/bin/env python3
"""Validate colour_pillar_plants output."""
import json, pathlib, sys

sys.path.insert(0, str(pathlib.Path(__file__).parent.parent))

idx_path = pathlib.Path(__file__).parent.parent / "out/colour_pillar_plants/colour_pillar_index.json"
idx = json.loads(idx_path.read_text())

print("=" * 80)
print("COLOUR PILLAR OUTPUT VALIDATION")
print("=" * 80)
print(f"Base seed: {idx['base_seed']}")
print(f"Plants: {idx['n_plants']}")
print(f"Pillars defined: {list(idx['pillars'].keys())}\n")

print("Plants in index:")
for p in idx['plants']:
    salt = p['primary']['fuel_salt']
    sec_fluid = p['secondary']['fluid']
    ter_fluid = p['tertiary']['fluid']
    pillars = ','.join(p['pillar_tags'])
    print(f"  {p['plant_id']:15s} | salt={salt:12s} | sec={sec_fluid:6s} | ter={ter_fluid:6s} | pillars={pillars}")

print("\n" + "=" * 80)
print("SAMPLE PLANT DETAIL")
print("=" * 80)

plant_dir = pathlib.Path(__file__).parent.parent / "out/colour_pillar_plants" / idx['plants'][0]['plant_id']
if (plant_dir / 'plant_summary.json').exists():
    ps = json.loads((plant_dir / 'plant_summary.json').read_text())
    print(f"Plant ID: {ps['plant_id']}")
    print(f"Seed: {ps['seed']}")
    print(f"Network stats:")
    print(f"  - {ps['network']['n_segments']} segments")
    print(f"  - {ps['network']['total_length_m']:.1f} m total length")
    print(f"  - {ps['network']['total_dP_Pa']:.0f} Pa total pressure drop")
    print(f"  - {ps['network']['total_heat_loss_W']:.1f} W total heat loss")
    print(f"Fluids computed: {ps['fluids_computed']}")
    print(f"Pillar tags: {ps['pillar_tags']}")
    print(f"\nPillar details:")
    for pillar in ps['pillars']:
        print(f"  {pillar['pillar']:10s} {pillar['colour']:8s} {pillar['traits']}")

# Validate files
print("\n" + "=" * 80)
print("FILE VALIDATION")
print("=" * 80)

base_dir = pathlib.Path(__file__).parent.parent / "out/colour_pillar_plants"
errors = []
for p in idx['plants']:
    plant_dir = base_dir / p['plant_id']

    # Check required files
    required = ['pipe_network.csv', 'plant_summary.json']
    for fname in required:
        if not (plant_dir / fname).exists():
            errors.append(f"  Missing: {p['plant_id']}/{fname}")

    # Check fluid files
    for fluid in p['fluids_computed']:
        if not (plant_dir / f"pvt_grid_{fluid}.csv").exists():
            errors.append(f"  Missing: {p['plant_id']}/pvt_grid_{fluid}.csv")
        if not (plant_dir / f"saturation_{fluid}.csv").exists():
            errors.append(f"  Missing: {p['plant_id']}/saturation_{fluid}.csv")

if errors:
    print("ERRORS FOUND:")
    for err in errors:
        print(err)
else:
    print("All files present and valid!")
    print(f"✓ {idx['n_plants']} plant directories")
    print(f"✓ Master index JSON")
    total_pvt_files = sum(len(p['fluids_computed']) for p in idx['plants']) * 2
    print(f"✓ {total_pvt_files} PVT+saturation file pairs")

print("=" * 80)
