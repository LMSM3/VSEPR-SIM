# XYZ File Format Specification v1
# xyzZ (raw) → xyzA (annotated) → xyzC (constructed) → xyzF (forces)

## Overview

Four formats for molecular/crystalline structures:

| Format | Purpose | Extends |
|--------|---------|---------|
| `.xyz` (xyzZ) | Raw input (standard XYZ) | N/A |
| `.xyzA` | Annotated (bonds, IDs, metadata) | xyzZ |
| `.xyzC` | Constructed (supercells, relaxed, CG) | xyzA |
| `.xyzF` | Force vectors (MD analysis, interactions) | xyzZ/A/C |

**Key principle:** Geometry block stays plain XYZ for compatibility.  
All smart metadata lives in comment headers.

---

## xyzZ (Raw Input)

Standard XYZ format with optional metadata in line 2:

```
N
# xyzZ v1  units=angstrom  title="raw input"
C  0.000000  0.000000  0.000000
C  1.234567  0.000000  0.000000
...
```

**Rules:**
- Line 1: atom count (integer)
- Line 2: comment (freeform, may contain tags)
- Lines 3+: `Element x y z` (space-separated)

**Tags (optional):**
- `xyzZ v1` — format version
- `units=angstrom|bohr|nm` — coordinate units
- `title="..."` — structure name

---

## xyzA (Annotated)

Adds bonds, IDs, per-atom properties via structured comment metadata:

```
N
# xyzA v1  source="foo.xyz"  units=angstrom
# meta:
#   id_scheme: "a{index}"
#   created_utc: "YYYY-MM-DDTHH:MM:SSZ"
# atoms:
#   columns: [id, elem, x, y, z, group, flags]
# bonds:
#   columns: [a, b, order, type]
# groups:
#   - {name: "default", members: ["a1","a2","a3"]}
# per_atom_props:
#   q:     {units: "e", default: 0.0}
#   mass:  {units: "amu", default: null}
#   tag:   {units: "-", default: ""}
# data:
#   bonds: []
#   atom_props: {}
C  0.000000  0.000000  0.000000
C  1.234567  0.000000  0.000000
...
```

**Metadata blocks:**

### `# meta:`
- `id_scheme`: ID generation pattern (e.g., `"a{index}"` → `a1, a2, ...`)
- `created_utc`: ISO 8601 timestamp

### `# atoms:`
- `columns`: Extended atom data columns (beyond `elem x y z`)

### `# bonds:`
- `columns`: Bond data schema

### `# groups:`
- Named selection sets (YAML list format)

### `# per_atom_props:`
- Per-atom property definitions with units

### `# data:`
- Actual bond/property data (YAML format):

```yaml
# data:
#   bonds:
#     - {a: "a1", b: "a2", order: 1, type: "single"}
#     - {a: "a2", b: "a3", order: 2, type: "double"}
#   atom_props:
#     a1: {q: -0.03, mass: 12.011, tag: "ring"}
#     a2: {q: -0.02, mass: 12.011}
```

**Compatibility:** Geometry block stays plain XYZ — tools that only read xyzZ ignore metadata.

---

## xyzC (Constructed)

Derived structures with full provenance and reserved slots for bulk/CG properties:

```
M
# xyzC v1  source_raw="foo.xyz"  source_annotated="foo.xyzA"  units=angstrom
# provenance:
#   pipeline_id: "build-0001"
#   steps:
#     - {name: "supercell", params: {a: 2, b: 2, c: 2}}
#     - {name: "relax",     params: {method: "lj_quench", steps: 20000, dt_fs: 0.5}}
#     - {name: "cg",        params: {mapping: "ring_centroids", beads: null}}
# artifacts:
#   frame: "relaxed"
# lattice:
#   a: [10.0, 0.0, 0.0]
#   b: [0.0, 10.0, 0.0]
#   c: [0.0, 0.0, 10.0]
# reserved_slots:
#   bulk:
#     density:        {value: null, units: "g/cm3"}
#     elastic_modulus:{value: null, units: "GPa"}
#     rdf:            {value: null, units: "-"}
#   coarse_grained:
#     beads:          {value: null, units: "count"}
#     bead_types:     {value: null, units: "-"}
#     bead_bonds:     {value: null, units: "-"}
#     pmf:            {value: null, units: "kJ/mol"}
# results:
#   energy:          {value: null, units: "eV"}
#   converged:       null
#   notes:           ""
C  0.000000  0.000000  0.000000
C  1.234500  0.000100  0.000000
...
```

**Key sections:**

### `# provenance:`
- `pipeline_id`: Unique ID for this construction pipeline
- `steps`: Ordered list of operations with parameters
- Enables deterministic reproduction

### `# artifacts:`
- `frame`: Which frame is stored ("supercell", "relaxed", "cg")

### `# lattice:`
- Unit cell vectors (if periodic)

### `# reserved_slots:`
**Bulk properties:**
- `density`, `elastic_modulus`, `rdf` (radial distribution function ref)

**Coarse-grained properties:**
- `bead_count`, `bead_types`, `bead_bonds`, `pmf` (potential of mean force ref)

### `# results:`
- `energy`: Final energy (eV)
- `converged`: Boolean convergence flag
- `notes`: Freeform notes

**Multiple frames (trajectory style):**

```
M
# frame=supercell ...
...
M
# frame=relaxed ...
...
K
# frame=cg ...
...
```

---

## File Naming Convention

```
foo.xyz   — Raw input
foo.xyzA  — Annotated (bonds inferred, IDs assigned)
foo.xyzC  — Constructed (supercell, relaxed, coarse-grained)
```

**Workflow:**

1. User provides `foo.xyz`
2. System generates `foo.xyzA` (infer bonds, assign IDs)
3. User requests supercell → generates `foo.xyzC` (frame=supercell)
4. User requests relaxation → updates `foo.xyzC` (frame=relaxed)
5. User requests coarse-grain → adds frame to `foo.xyzC` (frame=cg)

---

## Validation & Hashing

**Hash computation:**
```
hash = SHA256(source_content + recipe_json)
```

**Rebuild trigger:**
- Source file (`foo.xyz`) modified
- Recipe parameters changed
- Hash mismatch detected

**Watch mode:**
```bash
vsepr --watch foo.xyz --viz
```

Watches `foo.xyz`, regenerates `foo.xyzA` and `foo.xyzC` on change, updates visualization.

---

## Implementation Notes

### Parsing Strategy

1. Read line 1 (atom count)
2. Read line 2 (comment)
3. If comment starts with `# xyzA` or `# xyzC`, continue reading metadata until non-comment line
4. Parse metadata as YAML blocks
5. Parse geometry as standard XYZ

### Compatibility

- **xyzZ readers** (e.g., VMD, Avogadro): Ignore metadata, read geometry only
- **xyzA/xyzC readers**: Parse metadata for enhanced features
- **Round-trip guarantee**: xyzZ → xyzA → xyzZ preserves geometry exactly

### Storage

- Keep all three files (`foo.xyz`, `foo.xyzA`, `foo.xyzC`) in same directory
- Source files are immutable once created
- Derived files can be regenerated deterministically

---

## Example Use Cases

### 1. NaCl Supercell
```
# Input: nacl.xyz (1 Na + 1 Cl)
# Command: vsepr emit nacl.xyz --crystal --a 3 --b 3 --c 3
# Output: nacl.xyzC (27 formula units, 54 atoms)
```

### 2. Organic Molecule with Bonds
```
# Input: benzene.xyz (12 atoms: 6C + 6H)
# Command: vsepr annotate benzene.xyz
# Output: benzene.xyzA (with aromatic bonds marked)
```

### 3. Coarse-Grained Polymer
```
# Input: polymer.xyz (1000 atoms)
# Command: vsepr cg polymer.xyz --beads ring_centroids
# Output: polymer.xyzC (frame=cg, 50 beads)
```

---

## Status

**Implemented:** xyzZ parsing (existing `xyz_format.cpp`)  
**To implement:**
- xyzA parser/writer
- xyzC parser/writer with provenance
- **xyzF parser/writer (force vectors)**
- Crystal class (C++ wrapper)
- Forces class (force field wrapper)
- Watch system (`--watch` flag)

**Files to create:**
- `src/io/xyzA_format.cpp`
- `src/io/xyzC_format.cpp`
- **`src/io/xyzF_format.cpp`**
- `src/data/crystal.cpp`
- **`src/data/forces.cpp`**
- `src/io/crystal_watcher.cpp`
- **`src/vis/force_renderer.cpp`**

---

## xyzF — Force Vector Format (NEW)

### Overview

**xyzF** stores computed force vectors for MD analysis and interaction visualization. Each atom has:
- Net force (sum of all contributions)
- Primary interaction (largest contributor)
- All pairwise contributions (optional detail)
- Decomposition (LJ, Coulomb, bonded)

### Format Specification

```yaml
N
# xyzF v1  source="nacl.xyz"  units="kcal_mol_A"  model="LJ+Coulomb"
# computation:
#   method: "pairwise_nonbonded"
#   cutoff: 12.0
#   coulomb_k: 332.0636
#   timestamp: "2026-01-08T20:45:00Z"
#   hash: "7a3f9e2b..."
# statistics:
#   max_force: 85.3
#   mean_force: 12.4
#   rms_force: 18.7
#   num_atoms: 54
# forces:
#   - atom: "a1"
#     net: [10.5, -3.2, 0.0]
#     magnitude: 11.0
#     primary:
#       source: "a2"
#       direction: [0.96, -0.29, 0.0]
#       magnitude: 8.5
#       decomposition:
#         lj: [2.1, -0.6, 0.0]
#         coulomb: [8.4, -2.6, 0.0]
#     contributions:
#       - {source: "a2", magnitude: 8.5, direction: [0.96, -0.29, 0.0]}
#       - {source: "a3", magnitude: 2.5, direction: [-0.38, -0.92, 0.0]}
#   - atom: "a2"
#     net: [-10.5, 3.2, 0.0]
#     magnitude: 11.0
#     primary:
#       source: "a1"
#       direction: [-0.96, 0.29, 0.0]
#       magnitude: 8.5
#     contributions:
#       - {source: "a1", magnitude: 8.5, direction: [-0.96, 0.29, 0.0]}
Na  0.0  0.0  0.0
Cl  2.8  0.0  0.0
...
```

### Key Metadata Blocks

#### `# computation:`
- `method`: "pairwise_nonbonded", "bonded_mm", "ewald", etc.
- `cutoff`: Interaction cutoff (Å)
- `coulomb_k`: Coulomb constant (332.0636 kcal·Å/mol·e² or 14.3996 eV·Å/e²)
- `timestamp`: ISO 8601
- `hash`: SHA256(geometry + params) → rebuild if changed

#### `# statistics:`
- `max_force`: max(|F|) over all atoms
- `mean_force`: mean(|F|)
- `rms_force`: sqrt(mean(|F|²))
- `num_atoms`: Atom count (should match line 1)

#### `# forces:` (per-atom)
- `atom`: Atom ID ("a1", "a2", ...)
- `net`: [Fx, Fy, Fz] — total force on this atom
- `magnitude`: |F_net| = sqrt(Fx² + Fy² + Fz²)
- `primary`: Largest single contributor
  - `source`: Atom ID exerting this force
  - `direction`: Unit vector (normalized F)
  - `magnitude`: |F| from this source
  - `decomposition` (optional):
    - `lj`: Lennard-Jones contribution
    - `coulomb`: Electrostatic contribution
    - `bonded`: Bonded (bond/angle/torsion) contribution
- `contributions`: List of all pairwise forces (for detailed analysis)

### Units

| Unit Code | Description | Example |
|-----------|-------------|---------|
| `kcal_mol_A` | kcal/mol/Å (MM standard) | 10.5 |
| `eV_A` | eV/Å (DFT standard) | 0.456 |
| `pN` | piconewtons (AFM/SMD) | 24.8 |

Conversion: 1 kcal/mol/Å = 0.04336 eV/Å = 69.479 pN

### Example: NaCl Dimer

```yaml
2
# xyzF v1  source="nacl.xyz"  units="kcal_mol_A"  model="LJ+Coulomb"
# computation:
#   method: "pairwise_nonbonded"
#   cutoff: 12.0
#   coulomb_k: 332.0636
#   lj_epsilon_NaCl: 0.15
#   lj_sigma_NaCl: 3.0
#   hash: "a1b2c3..."
# statistics:
#   max_force: 25.3
#   mean_force: 25.3
#   rms_force: 25.3
# forces:
#   - atom: "a1"  # Na
#     net: [25.3, 0.0, 0.0]
#     magnitude: 25.3
#     primary:
#       source: "a2"  # Cl attracts Na
#       direction: [1.0, 0.0, 0.0]
#       magnitude: 25.3
#       decomposition:
#         lj: [-2.1, 0.0, 0.0]      # Weak repulsion at r=2.8Å
#         coulomb: [27.4, 0.0, 0.0] # Strong attraction (+1 ⟷ -1)
#   - atom: "a2"  # Cl
#     net: [-25.3, 0.0, 0.0]
#     magnitude: 25.3
#     primary:
#       source: "a1"  # Newton's 3rd law
#       direction: [-1.0, 0.0, 0.0]
#       magnitude: 25.3
#       decomposition:
#         lj: [2.1, 0.0, 0.0]
#         coulomb: [-27.4, 0.0, 0.0]
Na  0.0  0.0  0.0
Cl  2.8  0.0  0.0
```

**Interpretation:**
- Na and Cl attract (net force toward each other)
- Coulomb dominates (27.4 vs 2.1 kcal/mol/Å)
- Small LJ repulsion (atoms slightly too close)
- Forces obey Newton's 3rd law: F₁ = -F₂

### Visualization

**Force arrows in GUI:**

1. **Primary only mode** (default for large systems):
   - One arrow per atom
   - Points from atom toward strongest interactor
   - Length ∝ log(magnitude) or scaled
   - Color: blue (weak) → yellow (medium) → red (strong)

2. **All contributors mode** (detailed):
   - Multiple arrows per atom
   - Shows all pairwise interactions
   - Can be overwhelming for N > 100

3. **Decomposed mode**:
   - Separate arrows for LJ (green), Coulomb (red), bonded (blue)
   - Shows physical origins of forces

4. **Interaction pairs**:
   - Single line between atom pairs
   - Bidirectional (shows Newton's 3rd law)
   - Useful for understanding ionic/covalent bonding

**Rendering in vsepr-gui:**
- Workspace subwindow (TL or TR)
- Toggle force overlay with checkbox
- Slider for arrow scale
- Dropdown for mode (primary/all/decomposed)
- Color map editor

### Force Computation

**From C++ potentials:**

```cpp
#include "data/Forces.hpp"
#include "data/Crystal.hpp"
#include "pot/lj_coulomb.hpp"

// Load geometry
Crystal cryst = Crystal::load_xyz("nacl.xyz");

// Compute forces
ForceComputer fc(cryst);
Forces forces = fc.compute("LJ+Coulomb");

// Save
forces.save_xyzF("nacl.xyzF");

// Analyze
auto strong = forces.find_strong_interactions(10.0);  // |F| > 10
for (auto [a, b] : strong) {
    std::cout << "Strong: " << a << " ⟷ " << b << "\n";
}

// Visualize (get arrows for renderer)
auto arrows = forces.get_primary_arrows();
for (auto [origin, direction, magnitude] : arrows) {
    // Draw arrow from origin in direction with magnitude
}
```

**From Python tools:**

```python
# tools/compute_forces.py
from ase.calculators.lj import LennardJones
from ase.io import read

atoms = read("nacl.xyz")
atoms.calc = LennardJones()
forces = atoms.get_forces()

# Write xyzF format
with open("nacl.xyzF", "w") as f:
    f.write(f"{len(atoms)}\n")
    f.write("# xyzF v1  source=\"nacl.xyz\"  units=\"eV_A\"  model=\"LJ\"\n")
    # ... write YAML metadata
    # ... write geometry
```

### Watch Mode with Forces

```bash
vsepr --watch nacl.xyz --forces --viz
```

**Behavior:**
1. Watches `nacl.xyz` for changes
2. On change:
   - Reloads geometry
   - Recomputes forces (`nacl.xyzF`)
   - Updates visualization (structure + force arrows)
3. Real-time feedback: edit geometry → see force response

**Use cases:**
- Interactive force field tuning
- Understanding MD trajectories
- Debugging unstable configurations
- Teaching electrostatics/bonding

### Provenance & Rebuild

**Hash-based rebuild:**

```cpp
Forces forces = Forces::load_xyzF("nacl.xyzF");

if (forces.needs_rebuild()) {
    // Geometry or params changed
    forces.rebuild();
    forces.save_xyzF("nacl.xyzF");
}
```

**Hash includes:**
- Source geometry (all atomic positions)
- Force computation parameters (cutoff, k_coulomb, etc.)
- Model type (LJ, LJ+Coulomb, etc.)

If any change → hash mismatch → regenerate xyzF.

### Integration with TUI

**New TUI commands:**

```
[17] Compute forces
[18] View forces (overlay)
```

**Usage:**

```python
# In tui.py
def action_compute_forces():
    xyz_path = input("  XYZ file: ").strip()
    model = input("  Model [LJ+Coulomb]: ").strip() or "LJ+Coulomb"
    cutoff = input("  Cutoff [12.0]: ").strip() or "12.0"

    xyzF_path = xyz_path.replace(".xyz", ".xyzF")

    subprocess.run([
        str(BUILD_DIR / "compute_forces"),
        "--input", xyz_path,
        "--model", model,
        "--cutoff", cutoff,
        "--output", xyzF_path
    ])

    ok(f"Forces computed: {xyzF_path}")

register("17", "Compute forces", "Analysis", action_compute_forces)
```

**In GUI:**

- Tools → Compute Forces
- Opens dialog: select xyz, choose model, set params
- Generates xyzF file
- Automatically loads force overlay in structure view

### Status

**Specification:** ✅ Complete  
**To implement:**
- `src/io/xyzF_format.cpp` — Parser/writer
- `src/data/forces.cpp` — Forces class implementation
- `src/vis/force_renderer.cpp` — Arrow rendering
- `apps/compute_forces.cpp` — CLI tool
- TUI integration (commands 17-18)
- GUI force overlay toggle

**Estimated effort:** 2-3 days
- Day 1: xyzF parser + Forces class
- Day 2: Force computation from potentials
- Day 3: Visualization (force arrows)

