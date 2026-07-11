# `src/gen/` — Metal / Alloy Supercell Generator

> **Branch:** `v5.0.0-beta.7-step-attempt`  
> **Status:** Beta attempt — not yet wired into the main CMake build

---

## Purpose

Produces the full artifact pipeline for metal and alloy supercells:

| Artifact | Format | Role |
|---|---|---|
| `<tag>.xyz` | `.xyz` | Coordinates only — static geometry truth |
| `<tag>.xyza` | `.xyza` | + charge / velocity / per-atom energy columns |
| `<tag>.xyzc` | `.xyzc` | Checkpoint header + `.xyza` frame (restartable) |
| `<tag>.xyzf` | `.xyzf` | 3-frame thermal trajectory (0 K, 300 K, 600 K) |
| `<tag>.step` | `.step` | STEP AP203 engineering geometry truth |
| `geometry_map.json` | JSON | Provenance manifest binding all artifacts |

All coordinate data is in **Å**; energy in **kcal/mol**; velocity in **Å/fs** —
consistent with the unit system defined in `src/io/xyz_unified.hpp §1.1`.

---

## Files

### `metal_presets.hpp`
Static compile-time material database.

| Preset tag | Material | Lattice | a (Å) | c (Å) |
|---|---|---|---|---|
| `NiTi_B2` | Nitinol (shape-memory) | B2 ordered BCC | 3.015 | 3.015 |
| `W_BCC` | Tungsten (pure refractory) | BCC | 3.165 | 3.165 |
| `IN625_FCC` | Inconel 625 (Ni-Cr-Mo-Nb superalloy) | FCC | 3.575 | 3.575 |
| `Ti64_HCP` | Ti-6Al-4V (aerospace alloy) | HCP | 2.921 | 4.620 |

Key types:
- `MaterialPreset` — lattice parameters, basis sites, alloy composition label
- `SiteDesc` — per-Wyckoff-site species, Z, fractional coords, default charge
- `OccupancyCycle` — deterministic round-robin substitution list for random alloys
- `default_presets()` — returns the full database as `std::vector<MaterialPreset>`
- `occupancy_cycle(tag)` — returns the substitution sequence for a given tag

### `lattice_builder.hpp`
Converts a `MaterialPreset` into a tiled `vsepr::io::XYZFrame` supercell.

Key functions:
- `build_supercell(mat, nx, ny, nz, apply_cycle)` → `XYZFrame`  
  Tiles the primitive cell `nx × ny × nz` times.  Applies alloy occupancy
  cycle deterministically; attaches `q`, `v` (zero), and `e` columns so the
  frame is `.xyza`-ready.  Sets `XYZBox` with PBC on all axes.
- `build_thermal_trajectory(mat, nx, ny, nz, temps)` → `vector<XYZFrame>`  
  Returns one frame per temperature in `temps` (default 0 / 300 / 600 K)
  with isotropic placeholder velocities scaled to `sqrt(kB·T / m_avg)`.

Lattice geometry helpers (`fcc_motif`, `bcc_motif`, `b2_motif`, `hcp_motif`)
return the fractional basis positions for the conventional primitive cell.
`CellVectors::to_cart()` converts fractional → Cartesian for both cubic
(BCC/FCC/B2) and hexagonal (HCP, a-b at 60°) cells.

### `step_writer.hpp`
Minimal **ISO 10303-21 STEP AP203** writer — engineering geometry truth artifact.

Key functions:
- `write_step_ap203(ostream&, frame, name, author)` — stream overload
- `write_step_ap203(path, frame, name, author)` — file convenience overload

Output structure:
```
ISO-10303-21;
HEADER;  FILE_DESCRIPTION / FILE_NAME / FILE_SCHEMA
ENDSEC;
DATA;
  PRODUCT / PRODUCT_DEFINITION_FORMATION / PRODUCT_DEFINITION
  SHAPE_REPRESENTATION
  CARTESIAN_POINT  ← one per atom (x y z in Å)
  SHAPE_DEFINITION_REPRESENTATION
ENDSEC;
END-ISO-10303-21;
```

> **Future (beta.9+):** Replace the CARTESIAN_POINT cloud with
> `ADVANCED_FACE` shells once a convex-hull or mesh builder is available.

### `metal_gen.cpp`
Standalone driver.  Iterates `default_presets()`, builds 3×3×3 supercells,
and writes all five artifact types plus `geometry_map.json`.

---

## Building

```sh
# From src/gen/ — no CMake needed for the attempt branch
g++ -std=c++20 -O2 -I../../ -I../../src -o metal_gen metal_gen.cpp
```

Requires: GCC ≥ 12 or Clang ≥ 15 (C++20 structured bindings + `<filesystem>`).  
On MSVC: `/std:c++20 /I..\..\` (untested; the include guards are compatible).

---

## Running

```sh
./metal_gen                 # output → ./gen_out/
./metal_gen /path/to/dir    # output → specified directory
```

Expected output (3×3×3 supercell):

```
[NiTi_B2]   Nitinol     B2   a=3.015 Å   54 atoms
[W_BCC]     Tungsten    BCC  a=3.165 Å   54 atoms
[IN625_FCC] Inconel625  FCC  a=3.575 Å  108 atoms   (4-species occupancy cycle)
[Ti64_HCP]  Ti-6Al-4V   HCP  a=2.921 Å   54 atoms   (Ti/Al/V substitution)
```

21 artifact files + `geometry_map.json` in the output directory.

---

## Adding a New Material

1. Add a `MaterialPreset` entry in `metal_presets.hpp :: default_presets()`.
2. If substitutional, add its tag to `occupancy_cycle()`.
3. No changes needed in `lattice_builder.hpp` or `metal_gen.cpp` for standard
   lattice types (BCC/FCC/HCP/B2).  For a new lattice type, add a motif
   function and a `CellVectors` branch.

---

## Workflow Contract (beta.8 reference)

```
.xyz / .xyza / .xyzc / .xyzf  →  simulation truth  (forces, velocities, charges)
.step                          →  engineering truth  (geometry, provenance, identity)
geometry_map.json              →  manifest           (binds both artifact sets)
```

The generator intentionally does **not** compute forces or run dynamics.
It produces clean initial-state artifacts for downstream `SpawnType::CRYSTAL`
or workflow-backed initialization paths in `src/sim/sim_state.cpp`.

---

## Notes

- `gen_out/` and compiled binaries are excluded by `.gitignore`.
- All output uses the VSEPR-SIM unit system (Å, kcal/mol, Å/fs, e).
- Reference energies and partial charges are DFT-PBE cohesive approximations,
  suitable for geometry seeding but **not** for quantitative MD without
  re-parameterization.
- Alloy occupancy is **deterministic** (round-robin, no RNG) for
  reproducibility; Monte-Carlo disorder should be added in a future
  `src/gen/disorder.hpp` helper.
