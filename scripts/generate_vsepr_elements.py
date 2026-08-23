#!/usr/bin/env python3
"""
generate_vsepr_elements.py
--------------------------
Convert Bowserinator PeriodicTableJSON.json → lean elements.vsepr.json

Extracts only the fields needed for simulation/visualization:
- Identity: Z, symbol, name
- Physical: atomic_weight (standard)
- Chemistry: en_pauling, shells
- Visualization: cpk-hex, (future: covalent_radius)
- Nuclear: is_radioactive, default_isotope

Run from vsepr-sim root:
    python3 scripts/generate_vsepr_elements.py
"""

import json
from pathlib import Path

SOURCE = Path("data/PeriodicTableJSON.json")
OUTPUT = Path("data/elements.vsepr.json")

def main():
    if not SOURCE.exists():
        print(f"Error: {SOURCE} not found")
        return
    
    with open(SOURCE) as f:
        bowser = json.load(f)
    
    elements = []
    for e in bowser["elements"]:
        Z = e.get("number")
        symbol = e.get("symbol")
        name = e.get("name")
        
        if not Z or not symbol or not name:
            continue  # skip junk
        
        el = {
            "Z": Z,
            "symbol": symbol,
            "name": name,
        }
        
        # Atomic weight (average)
        if "atomic_mass" in e and e["atomic_mass"] is not None:
            el["atomic_weight"] = e["atomic_mass"]
        
        # Electronegativity
        if "electronegativity_pauling" in e and e["electronegativity_pauling"] is not None:
            el["en_pauling"] = e["electronegativity_pauling"]
        
        # Electron shells
        if "shells" in e and isinstance(e["shells"], list):
            el["shells"] = e["shells"]
        
        # CPK color
        if "cpk-hex" in e:
            el["cpk"] = e["cpk-hex"]
        
        # Radioactivity (simple heuristic: Z >= 84 or known radioactive)
        # (In reality, check isotope stability; this is a placeholder)
        el["radioactive"] = (Z >= 84)
        
        elements.append(el)
    
    output_data = {
        "schema_version": 1,
        "description": "Lean element database for VSEPR sim (auto-generated)",
        "source": f"Generated from {SOURCE.name}",
        "elements": elements
    }
    
    with open(OUTPUT, 'w') as f:
        json.dump(output_data, f, indent=2)
    
    print(f"✓ Generated {OUTPUT}")
    print(f"  {len(elements)} elements")

if __name__ == "__main__":
    main()
