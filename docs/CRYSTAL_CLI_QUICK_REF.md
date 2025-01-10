# VSEPR Crystal Commands — Quick Reference Card

## Basic Syntax
```bash
vsepr <Formula>@crystal emit --preset <ID> [OPTIONS]
```

## Atomistic Presets (Fixed Cell Parameters)
These use the validated crystal structures from `atomistic/crystal/unit_cell.cpp`:

| Preset | Structure | Formula | Cell (Å) | Atoms |
|--------|-----------|---------|----------|-------|
| `al` | FCC | Al | 4.05 (cubic) | 4 |
| `fe` | BCC | Fe | 2.87 (cubic) | 2 |
| `cu` | FCC | Cu | 3.61 (cubic) | 4 |
| `au` | FCC | Au | 4.08 (cubic) | 4 |
| `nacl_atomistic` | Rocksalt | NaCl | 5.64 (cubic) | 8 |
| `mgo` | Rocksalt | MgO | 4.21 (cubic) | 8 |
| `cscl` | CsCl | CsCl | 4.12 (cubic) | 2 |
| `si` | Diamond | Si | 5.43 (cubic) | 8 |
| `diamond` | Diamond | C | 3.57 (cubic) | 8 |
| `tio2` | Rutile | TiO₂ | 4.59×4.59×2.96 (tetragonal) | 6 |

### Usage
```bash
# Generate unit cell
vsepr Al@crystal emit --preset al --out al_unit.xyz

# Generate 2×2×2 supercell (4 × 8 = 32 atoms)
vsepr Al@crystal emit --preset al --supercell 2,2,2 --out al_32.xyz
```

## Motif-Based Presets (User-Supplied Cell)
These require `--cell a,b,c` and match composition stoichiometry:

| Preset | Stoichiometry | Example | Notes |
|--------|---------------|---------|-------|
| `rocksalt` | 1:1 | NaCl, MgO, LiF | Cubic |
| `rutile` | 1:2 | MgF₂, TiO₂ | Tetragonal (a=b≠c) |
| `tysonite` | 1:3 | CeF₃, LaF₃ | Hexagonal |

### Usage
```bash
# Custom NaCl with non-standard cell
vsepr NaCl@crystal emit --preset rocksalt --cell 5.8,5.8,5.8 --out nacl_expanded.xyz

# MgF2 rutile (tetragonal)
vsepr MgF2@crystal emit --preset rutile --cell 4.6,4.6,3.0 --out mgf2.xyz
```

## Supercell Construction

```bash
--supercell na,nb,nc
```

Replicates unit cell by `na × nb × nc` along lattice vectors.

**Formula**: `N_total = N_unit × na × nb × nc`

### Examples
| Replication | Unit Cell Atoms | Supercell Atoms | Command Fragment |
|-------------|-----------------|-----------------|------------------|
| 1×1×1 | 4 (Al FCC) | 4 | *(none)* |
| 2×2×2 | 4 (Al FCC) | 32 | `--supercell 2,2,2` |
| 3×3×3 | 8 (NaCl) | 216 | `--supercell 3,3,3` |
| 4×4×4 | 2 (Fe BCC) | 128 | `--supercell 4,4,4` |

## Complete Examples

### Metals
```bash
# Aluminum FCC 3×3×3 (108 atoms)
vsepr Al@crystal emit --preset al --supercell 3,3,3 --out al_108.xyz

# Iron BCC 4×4×4 (128 atoms)
vsepr Fe@crystal emit --preset fe --supercell 4,4,4 --out fe_128.xyz

# Gold nanoparticle simulation starter (256 atoms)
vsepr Au@crystal emit --preset au --supercell 4,4,4 --out au_256.xyz
```

### Ionics
```bash
# NaCl 2×2×2 (64 atoms)
vsepr NaCl@crystal emit --preset nacl_atomistic --supercell 2,2,2 --out nacl_64.xyz

# MgO 3×3×3 (216 atoms)
vsepr MgO@crystal emit --preset mgo --supercell 3,3,3 --out mgo_216.xyz
```

### Covalent Networks
```bash
# Silicon diamond 2×2×2 (64 atoms)
vsepr Si@crystal emit --preset si --supercell 2,2,2 --out si_64.xyz

# Carbon diamond unit cell
vsepr C@crystal emit --preset diamond --out diamond.xyz
```

### Custom Stoichiometry
```bash
# Custom NaCl with strained lattice
vsepr NaCl@crystal emit --preset rocksalt --cell 6.0,6.0,6.0 --supercell 2,2,2 --out nacl_strained.xyz

# MgF2 rutile with supercell
vsepr MgF2@crystal emit --preset rutile --cell 4.6,4.6,3.0 --supercell 2,2,3 --out mgf2_24.xyz
```

## Output Format

XYZ file with metadata-rich comment line:

```
64
Formula: NaCl | Mode: @crystal | Preset: nacl_atomistic | Supercell: 2x2x2 | Cell: 11.28x11.28x11.28 | PBC: ON
Na  0.000  0.000  0.000
Cl  2.820  0.000  0.000
Na  0.000  2.820  0.000
...
```

## Validation & Testing

```bash
# In WSL after build
./build/test_crystal_pipeline

# Expected output:
#   ✓ Cubic lattice math
#   ✓ Coordinate transforms
#   ✓ MIC distance calculations
#   ✓ All 10 presets
#   ✓ Supercell scaling
#   ✓ Bond inference
#   ✓ FCC coordination = 12
```

## Error Messages

| Error | Cause | Fix |
|-------|-------|-----|
| `ERROR: Crystal emission requires --preset` | Missing `--preset` | Add `--preset al` (or other ID) |
| `ERROR: Unknown preset: xyz` | Invalid preset name | Check available presets (omit `--preset` to list) |
| `ERROR: --supercell requires 3 integers` | Wrong supercell format | Use `--supercell 2,2,2` (comma-separated) |
| `ERROR: Supercell replication factors must be >= 1` | Zero or negative values | Use positive integers only |

## Performance Notes

| System | Atoms | Memory (est.) | Time (emit) |
|--------|-------|---------------|-------------|
| Al unit cell | 4 | < 1 KB | < 1 ms |
| Al 3×3×3 | 108 | ~10 KB | ~5 ms |
| NaCl 2×2×2 | 64 | ~5 KB | ~3 ms |
| NaCl 5×5×5 | 1000 | ~100 KB | ~50 ms |
| Si 10×10×10 | 8000 | ~1 MB | ~500 ms |

*Note: Times are for structure generation only, not including relaxation.*

## Tips

1. **Start small**: Test with unit cell before building large supercells
2. **Check cell params**: Atomistic presets ignore `--cell` (use built-in values)
3. **PBC is mandatory**: `@crystal` and `@bulk` modes enforce periodic boundaries
4. **Supercell = replication**: Not interpolation — atoms are copied exactly
5. **Relaxation recommended**: Large supercells may have artificial strain (future: add `--relax`)

## Next Steps

1. Build and test: `cd /path/to/vsepr-sim && cmake -B build && cmake --build build`
2. Run examples: `./build/vsepr NaCl@crystal emit --preset nacl_atomistic --out test.xyz`
3. Visualize: Use VMD, OVITO, or ASE to view XYZ files
4. Extend: Add custom presets to `atomistic/crystal/unit_cell.cpp`

---

**Documentation**: See `docs/CRYSTAL_CLI_INTEGRATION.md` for implementation details  
**Theory**: See `docs/section10_12_13_closing.tex` and `docs/FROM_MOLECULES_TO_MATTER.ipynb`  
**Source**: `src/cli/emit_crystal.cpp`, `atomistic/crystal/unit_cell.cpp`
