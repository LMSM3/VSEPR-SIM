#!/usr/bin/env python3
"""
generate_split_schemas.py
-------------------------
Convert Bowserinator PeriodicTableJSON.json → separated physics/visual schemas

PHYSICS (elements.physics.json):
- Identity: Z, symbol, name
- Atomic: atomic_weight, shells
- Chemistry: en_pauling
- Nuclear: radioactive, default_A

VISUAL (elements.visual.json):
- Identity: Z, symbol (for cross-reference)
- Rendering: cpk (color), covalent_radius, vdw_radius

Run from vsepr-sim root:
    python3 scripts/generate_split_schemas.py
"""

import json
from pathlib import Path

SOURCE = Path("data/PeriodicTableJSON.json")
OUTPUT_PHYSICS = Path("data/elements.physics.json")
OUTPUT_VISUAL = Path("data/elements.visual.json")

def main():
    if not SOURCE.exists():
        print(f"Error: {SOURCE} not found")
        return
    
    with open(SOURCE) as f:
        bowser = json.load(f)
    
    physics_elements = []
    visual_elements = []
    
    for e in bowser["elements"]:
        Z = e.get("number")
        symbol = e.get("symbol")
        name = e.get("name")
        
        if not Z or not symbol or not name:
            continue  # skip junk
        
        # === PHYSICS ===
        phys = {
            "Z": Z,
            "symbol": symbol,
            "name": name,
        }
        
        if "atomic_mass" in e and e["atomic_mass"] is not None:
            phys["atomic_weight"] = e["atomic_mass"]
        
        if "electronegativity_pauling" in e and e["electronegativity_pauling"] is not None:
            phys["en_pauling"] = e["electronegativity_pauling"]
        
        if "shells" in e and isinstance(e["shells"], list):
            phys["shells"] = e["shells"]
        
        # Simple radioactivity heuristic (Z >= 84, or later check isotopes)
        phys["radioactive"] = (Z >= 84)
        
        physics_elements.append(phys)
        
        # === VISUAL ===
        vis = {
            "Z": Z,
            "symbol": symbol,
        }
        
        if "cpk-hex" in e:
            vis["cpk"] = e["cpk-hex"]
        
        # Note: Bowserinator doesn't have covalent_radius or vdw_radius
        # Would need external source (e.g., Cordero 2008, Bondi 1964)
        # For now, leave optional
        
        visual_elements.append(vis)
    
    # Write physics
    physics_data = {
        "schema_version": 1,
        "description": "Element physics data for molecular simulation",
        "source": f"Generated from {SOURCE.name}",
        "elements": physics_elements
    }
    
    with open(OUTPUT_PHYSICS, 'w') as f:
        json.dump(physics_data, f, indent=2)
    
    print(f"✓ Generated {OUTPUT_PHYSICS}")
    print(f"  {len(physics_elements)} elements")
    
    # Write visual
    visual_data = {
        "schema_version": 1,
        "description": "Element visual data for molecular rendering",
        "source": f"Generated from {SOURCE.name} (CPK colors)",
        "note": "Add covalent_radius/vdw_radius from external sources",
        "elements": visual_elements
    }
    
    with open(OUTPUT_VISUAL, 'w') as f:
        json.dump(visual_data, f, indent=2)
    
    print(f"✓ Generated {OUTPUT_VISUAL}")
    print(f"  {len(visual_elements)} elements")
    print()
    print("Next steps:")
    print("  - Add covalent_radius to visual.json (Cordero 2008 values)")
    print("  - Add vdw_radius to visual.json (Bondi 1964 values)")
    print("  - Verify radioactive flags in physics.json")

if __name__ == "__main__":
    main()
