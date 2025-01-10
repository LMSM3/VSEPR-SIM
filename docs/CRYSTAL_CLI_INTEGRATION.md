# Crystal CLI Integration Summary

## Overview

The crystal module (`atomistic/crystal`) has been integrated into the universal CLI routing system (`vsepr` command).

## Command Syntax

```bash
vsepr <SPEC> emit --preset <ID> [--supercell n,n,n] [--cell a,b,c] [--viz] --out <file>
```

### Components

**SPEC**: Formula with mode hint
- `Al@crystal` — aluminum crystal
- `NaCl@crystal` — sodium chloride crystal
- `TiO2@bulk` — titanium dioxide bulk material

**--preset**: Crystal structure template
- **Atomistic presets** (fixed cell parameters from atomistic/crystal module):
  - `al`, `fe`, `cu`, `au` — FCC/BCC metals
  - `nacl_atomistic`, `mgo`, `cscl` — ionic crystals
  - `si`, `diamond` — covalent networks
  - `tio2` — rutile oxide

- **Motif-based presets** (user-supplied cell via --cell):
  - `rocksalt` — 1:1 stoichiometry (requires composition like NaCl)
  - `rutile` — 1:2 stoichiometry (e.g., MgF2@crystal)
  - `tysonite` — 1:3 stoichiometry (e.g., CeF3@crystal)

**--supercell** (optional): Replication factors [na, nb, nc]
- Example: `--supercell 2,2,2` creates a 2×2×2 supercell
- Atoms are replicated geometrically: `r_{i,pqr} = r_i + p·a + q·b + r·c`

**--viz** (optional): Launch native OpenGL visualizer
- Opens interactive 3D window with coordination polyhedra
- CPK element coloring and unit cell wireframe
- Requires build with `-DBUILD_VIS=ON`

**--cell** (optional for atomistic presets, required for motif-based):
- Lattice parameters: `a,b,c` in Ångströms
- Example: `--cell 5.64,5.64,5.64`

**--out**: Output XYZ file path (skipped if `--viz` is used)

## Example Commands

### Atomistic Presets (Simplest)

```bash
# Generate NaCl unit cell (8 atoms) + visualize
vsepr NaCl@crystal emit --preset nacl_atomistic --viz

# Build 3×3×3 Al supercell + save to file
vsepr Al@crystal emit --preset al --supercell 3,3,3 --out al_108.xyz

# Silicon diamond structure + immediate visualization
vsepr Si@crystal emit --preset si --viz

# Gold FCC + 2×2×2 supercell + visualize
vsepr Au@crystal emit --preset au --supercell 2,2,2 --viz
```

### Native Visualization

```bash
# Launch interactive 3D viewer
vsepr NaCl@crystal emit --preset nacl_atomistic --supercell 2,2,2 --viz

# Features shown:
#   - Coordination polyhedra (translucent, inverted mean RGB coloring)
#   - Unit cell wireframe (cyan edges)
#   - CPK element colors (Na=purple, Cl=green)
#   - Real-time rotation, zoom, pan

# Controls:
#   Mouse drag: Rotate
#   Scroll: Zoom
#   ESC: Close
```

### Motif-Based Presets (Composition-Driven)

```bash
# Custom NaCl with specific cell parameter
vsepr NaCl@crystal emit --preset rocksalt --cell 5.7,5.7,5.7 --out nacl_custom.xyz

# MgF2 rutile structure (tetragonal: a=b≠c)
vsepr MgF2@crystal emit --preset rutile --cell 4.6,4.6,3.0 --out mgf2.xyz

# CeF3 tysonite structure (hexagonal)
vsepr CeF3@crystal emit --preset tysonite --cell 7.1,7.1,7.3 --out cef3.xyz
```

### Combining with Supercell

```bash
# NaCl 2×2×2 supercell (8 × 8 = 64 atoms)
vsepr NaCl@crystal emit --preset nacl_atomistic --supercell 2,2,2 --out nacl_64.xyz

# MgO 4×4×4 supercell with relaxation (future: add relax action)
vsepr MgO@crystal emit --preset mgo --supercell 4,4,4 --out mgo_512.xyz
```

## Implementation Details

### File Modifications

1. **`include/cli/parse.hpp`**
   - Added `std::vector<int> supercell` to `ActionParams`

2. **`src/cli/emit_crystal.cpp`**
   - Added `#include "atomistic/crystal/unit_cell.hpp"`
   - Added `#include "atomistic/crystal/supercell.hpp"`
   - New function: `unit_cell_to_atoms()` — converts atomistic::crystal::UnitCell to CLI Atom vector
   - New function: `emit_atomistic_preset()` — routes to 10 atomistic presets
   - Modified `generate_crystal_atoms()` — tries atomistic presets first, falls back to motif-based
   - Modified `emit_crystal()` — added supercell construction block
   - Updated help text with new preset list

3. **`apps/vsepr.cpp`**
   - Updated help text to document `--supercell` flag
   - Added crystal-specific examples

### Routing Logic

```
User types: vsepr NaCl@crystal emit --preset nacl_atomistic --supercell 2,2,2

↓ CommandParser::parse()
  → ParsedCommand {spec: "NaCl@crystal", action: Emit, preset: "nacl_atomistic", supercell: [2,2,2]}

↓ RunContext::from_parsed()
  → Validates @crystal mode → PBC mandatory

↓ action_emit(cmd, ctx)
  → Routes to emit_crystal() because preset is set

↓ emit_crystal()
  → generate_crystal_atoms("nacl_atomistic", ...)
    → emit_atomistic_preset("nacl_atomistic")
      → atomistic::crystal::presets::sodium_chloride()
        → Returns UnitCell with 8 atoms
      → unit_cell_to_atoms()
        → Converts to vector<Atom>
  
  → Supercell block (if --supercell present)
    → Replicates 8 atoms × 2×2×2 = 64 atoms
    → Shifts positions by (p·a, q·b, r·c)
  
  → write_xyz(atoms, ...)
```

### Preset Aliases

The system recognizes multiple aliases for convenience:

| Canonical | Aliases |
|-----------|---------|
| `aluminum_fcc` | `al_fcc`, `al` |
| `iron_bcc` | `fe_bcc`, `fe` |
| `copper_fcc` | `cu_fcc`, `cu` |
| `gold_fcc` | `au_fcc`, `au` |
| `sodium_chloride` | `nacl_atomistic`, `nacl_rock` |
| `magnesium_oxide` | `mgo` |
| `cesium_chloride` | `cscl` |
| `silicon_diamond` | `si` |
| `carbon_diamond` | `diamond`, `c` |
| `rutile_tio2` | `tio2` |

## Future Extensions

### Planned Features

1. **Relaxation after supercell** (§10 protocol):
   ```bash
   vsepr Al@crystal emit --preset al --supercell 3,3,3 --relax --out al_relaxed.xyz
   ```

2. **Triclinic cells** (beyond orthogonal):
   ```bash
   vsepr Custom@crystal emit --preset generic --cell 5,6,7 --angles 80,85,95 --out triclinic.xyz
   ```

3. **Direct atomistic State output**:
   ```bash
   vsepr NaCl@crystal emit --preset nacl_atomistic --format state --out nacl.state
   ```
   Output: Binary/JSON `atomistic::State` with PBC box, types, charges, masses

4. **Validation action** (§10 tests):
   ```bash
   vsepr Al@crystal test --preset al --validate-coordination --expect-cn 12
   ```

5. **Bond inference statistics**:
   ```bash
   vsepr NaCl@crystal emit --preset nacl_atomistic --supercell 2,2,2 --analyze-bonds
   ```
   Output: Coordination numbers, bond counts, strain metrics

## Testing

Once the build environment is configured, run:

```bash
# In WSL
cd /mnt/c/Users/Liam/Desktop/vsepr-sim
cmake -B build -DBUILD_TESTS=ON
cmake --build build
./build/test_crystal_pipeline
```

The test suite validates:
- Lattice math (cubic, triclinic, hexagonal)
- Coordinate transforms (fractional ↔ Cartesian)
- MIC distance calculations
- All 10 crystal presets
- Supercell construction (2×2×2 scaling)
- Bond inference with PBC
- FCC coordination number = 12

## Quick Reference

| Command | Description |
|---------|-------------|
| `vsepr Al@crystal emit --preset al --out al.xyz` | Generate Al FCC unit cell |
| `vsepr Al@crystal emit --preset al --supercell 3,3,3 --out al_108.xyz` | 3×3×3 supercell |
| `vsepr NaCl@crystal emit --preset nacl_atomistic --out nacl.xyz` | NaCl rocksalt |
| `vsepr Si@crystal emit --preset si --supercell 2,2,2 --out si_64.xyz` | Si diamond 2×2×2 |
| `vsepr MgF2@crystal emit --preset rutile --cell 4.6,4.6,3.0 --out mgf2.xyz` | Custom cell |

---

**Status**: ✅ Fully implemented and ready for testing
**Dependencies**: Requires WSL build environment configured
**Documentation**: See `docs/section10_12_13_closing.tex` for theoretical background
