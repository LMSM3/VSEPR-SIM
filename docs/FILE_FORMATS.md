# Molecular File Formats: XYZ, XYZA, XYZC

**Version:** 2.5.0-dev  
**Last Updated:** January 21, 2026  
**Purpose:** Day #11 Nano-Scale Simulation File I/O

---

## Overview

The VSEPR-Sim ecosystem uses three molecular file formats:

| Format | Description | Use Case |
|--------|-------------|----------|
| **XYZ** | Static molecular geometry | Single structures, snapshots |
| **XYZA** | Animated trajectory | Minimization traces, reaction paths |
| **XYZC** | Checkpointed simulation | Restartable MD simulations |

All formats are **human-readable text** for transparency and debugging.

---

## XYZ Format: Static Molecular Geometry

### Specification (IUPAC-compliant)

```
<atom_count>
<comment_line>
<element> <x> <y> <z>
<element> <x> <y> <z>
...
```

### Coordinate System & Units
- **Reference frame**: Right-handed Cartesian coordinates $(x,y,z)$
- **Units**: Ångströms (Å) where $1\,\text{Å} = 10^{-10}\,\text{m} = 0.1\,\text{nm}$
- **Precision**: Floating-point with 6 decimal places $\Rightarrow$ resolution $\sim 10^{-6}\,\text{Å} \approx 0.1\,\text{pm}$
- **Origin**: Arbitrary but should be near molecular center of mass for numerical stability
- **Typical molecular dimensions**: $1-100\,\text{Å}$ (small molecules to medium proteins)

### Example: Water Molecule

```
3
Water molecule (H2O) - optimized geometry
O  0.000000  0.000000  0.119262
H  0.000000  0.763239 -0.477049
H  0.000000 -0.763239 -0.477049
```

### Fields

1. **Line 1**: Atom count (integer)
2. **Line 2**: Comment (free text, often contains energy, timestamp, metadata)
3. **Lines 3+**: Element symbol, x, y, z coordinates (Angstroms)

### Comment Line Conventions

**Standard format (energy-annotated):**
```
<molecule_name> | E = <energy> kcal/mol | Method: <method> | <timestamp>
```

**Physical units and typical ranges:**
- **Energy** $E$: kcal/mol (1 kcal/mol $\approx 4.184$ kJ/mol $\approx 0.0434$ eV)
  - Typical molecular energies: $-10^2$ to $-10^5$ kcal/mol (bonded systems)
  - Relative energies (conformers): $0.1-10$ kcal/mol
  - Activation barriers: $5-50$ kcal/mol (kinetically accessible at $T=300\,$K)
- **Forces** $F$: kcal/(mol·Å) (gradient of energy)
  - Convergence criterion: $F_\text{max} < 0.01$ kcal/(mol·Å) for "relaxed" structures
  - Typical unrelaxed forces: $1-100$ kcal/(mol·Å)
- **RMSD**: Root-mean-square deviation in Å, $\text{RMSD} = \sqrt{\frac{1}{N}\sum_{i=1}^N |\mathbf{r}_i - \mathbf{r}_i^\text{ref}|^2}$

**Examples:**
```
Ethane | E = -12.345 kcal/mol | Method: FIRE minimization | 2026-01-21 15:42:33 UTC
Polyethylene 10-mer | Initial geometry | Step 0
Cisplatin | Optimized | RMSD = 0.0023 Å
```

### Use Cases

- **Input molecules** for simulation
- **Final optimized structures**
- **Regression test baselines**
- **Snapshot at specific timesteps**

---

## XYZA Format: Animated Trajectory

### Specification

Multiple XYZ frames concatenated in a single file:

```
<atom_count>
<comment_frame_1>
<element> <x> <y> <z>
...
<atom_count>
<comment_frame_2>
<element> <x> <y> <z>
...
```

### Example: Energy Minimization Trace

```
3
Water | Step 0 | E = -12.500 kcal/mol | F_max = 5.234 kcal/mol/Å
O  0.000000  0.000000  0.200000
H  0.000000  0.900000 -0.400000
H  0.000000 -0.900000 -0.400000
3
Water | Step 10 | E = -12.678 kcal/mol | F_max = 2.145 kcal/mol/Å
O  0.000000  0.000000  0.150000
H  0.000000  0.820000 -0.450000
H  0.000000 -0.820000 -0.450000
3
Water | Step 50 | E = -12.891 kcal/mol | F_max = 0.001 kcal/mol/Å
O  0.000000  0.000000  0.119262
H  0.000000  0.763239 -0.477049
H  0.000000 -0.763239 -0.477049
```

### Frame Metadata (Comment Line)

**Required fields for minimization trajectories:**
- `Step <N>` - Minimization iteration number (integer $\geq 0$)
- `E = <value>` - Total potential energy [kcal/mol]
- `F_max = <value>` - Maximum atomic force magnitude [kcal/(mol·Å)]
  - **Convergence criterion**: $F_\text{max} < 0.01$ kcal/(mol·Å) (force tolerance)
  - **Typical convergence**: FIRE algorithm reaches tolerance in $50-500$ steps

**Optional fields for MD trajectories:**
- `T = <value>` - Instantaneous temperature [K] from kinetic energy
  - **Thermal fluctuations**: $\sigma_T/T \sim 1/\sqrt{N_\text{dof}}$ where $N_\text{dof} = 3N-6$ (translational/rotational removal)
- `Time = <value>` - Simulation time [ps] where $1\,\text{ps} = 10^{-12}\,\text{s} = 1000\,\text{fs}$
- `RMSD = <value>` - Root-mean-square deviation from reference structure [Å]
- `Δt = <value>` - Integration timestep [fs], typical: $0.5-2\,\text{fs}$ (constrained bonds allow larger Δt)

### Use Cases

- **Minimization trajectories** (FIRE algorithm convergence)
- **Reaction pathways** (transition states)
- **Conformational sampling**
- **Polymer relaxation** (long-chain behavior)

---

## XYZC Format: Checkpointed Simulation

### Specification (Extended XYZ with phase-space state)

Extended XYZ with **velocity** and **thermodynamic metadata**:

```
<atom_count>
CHECKPOINT | <key1>=<val1> | <key2>=<val2> | ...
<element> <x> <y> <z> <vx> <vy> <vz>
...
```

### Physical Quantities
- **Positions** $(x,y,z)$: Cartesian coordinates [Å]
- **Velocities** $(v_x,v_y,v_z)$: Å/fs where $1\,\text{Å/fs} = 100\,\text{m/s}$
  - Typical thermal velocities at $T=300\,$K: $v_\text{rms} \approx 0.5-2\,\text{Å/fs}$ (depends on atomic mass)
  - Maxwell-Boltzmann distribution: $P(v) \propto \exp(-mv^2/2k_BT)$
- **Temperature** $T$: Kelvin [K], instantaneous from kinetic energy:
  $$T_\text{inst} = \frac{2E_\text{kin}}{3Nk_B} = \frac{\sum_i m_i v_i^2}{3Nk_B}$$
  where $k_B = 1.380649 \times 10^{-23}\,\text{J/K}$ (Boltzmann constant)
- **Pressure** $P$: atmospheres [atm] where $1\,\text{atm} = 101325\,\text{Pa}$
- **Energy partitioning**:
  - $E_\text{pot}$: Potential energy from force field [kcal/mol]
  - $E_\text{kin}$: Kinetic energy $= \frac{1}{2}\sum_i m_i v_i^2$ [kcal/mol]
  - $E_\text{tot} = E_\text{pot} + E_\text{kin}$ (conserved for microcanonical NVE ensemble)

### Example: MD Checkpoint

```
3
CHECKPOINT | Step=1000 | Time=10.5ps | E=-12.891 | T=300K | Method=Verlet
O  0.000000  0.000000  0.119262  0.0012 -0.0034  0.0008
H  0.000000  0.763239 -0.477049 -0.0023  0.0015 -0.0012
H  0.000000 -0.763239 -0.477049  0.0018  0.0021  0.0005
```

### Metadata Keys

| Key | Type | Description |
|-----|------|-------------|
| `Step` | int | Integration step |
| `Time` | float | Simulation time (ps) |
| `E` | float | Total energy (kcal/mol) |
| `T` | float | Temperature (K) |
| `P` | float | Pressure (atm) |
| `Method` | string | Integrator (Verlet/Langevin) |
| `Seed` | int | RNG seed for reproducibility |

### Use Cases

- **Restartable MD simulations**
- **Long-running jobs** (save progress)
- **Post-mortem debugging** (recover crashed state)

---

## User Interaction Workflows

### 1. **Building Molecules → XYZ Output**

**Command:**
```bash
./meso-build
⚛ build polyethylene 10
⚛ save pe10_initial.xyz
⚛ exit
```

**Result:** `pe10_initial.xyz` (single frame, initial geometry)

**Use Case:** Generate input for simulation

---

### 2. **Energy Minimization → XYZA Trajectory**

**Command:**
```bash
./meso-sim --mode minimize --input pe10_initial.xyz --output pe10_trace.xyza --steps 500
```

**Output:**
- `pe10_trace.xyza` - Full trajectory (500 frames)
- `pe10_final.xyz` - Final optimized structure

**Use Case:** Day #11 polymer relaxation testing

---

### 3. **View Minimization Progress**

**Interactive viewer:**
```bash
# View single structure
./interactive-viewer pe10_initial.xyz

# View animated trajectory
./interactive-viewer pe10_trace.xyza
```

**Features:**
- ⏪ Rewind, ⏸ Pause, ⏩ Fast-forward
- 🎚 Slider to jump to specific frame
- 📊 Energy plot overlay
- 🔍 Click atoms to see forces

**Windows users:**
```powershell
# Double-click pe10_trace.xyza in Explorer
# Opens directly in interactive-viewer (if file association installed)
```

---

### 4. **Regression Testing: Compare Structures**

**Command:**
```bash
./meso-align --ref pe10_baseline.xyz --target pe10_final.xyz
```

**Output:**
```
╔════════════════════════════════════════════════════════════════╗
║  STRUCTURE ALIGNMENT RESULTS                                   ║
╠════════════════════════════════════════════════════════════════╣
║  Reference:   pe10_baseline.xyz (10 atoms)                     ║
║  Target:      pe10_final.xyz (10 atoms)                        ║
║  Algorithm:   Kabsch (SVD rotation)                            ║
╠════════════════════════════════════════════════════════════════╣
║  RMSD:        0.0023 Å                                         ║
║  Max Δ:       0.0045 Å (atom 7)                                ║
║  Translation: (0.001, -0.002, 0.003) Å                         ║
║  Rotation:    0.12° about (0.98, 0.15, 0.05)                   ║
╠════════════════════════════════════════════════════════════════╣
║  Verdict:     ✓ PASS (RMSD < 0.01 Å)                           ║
╚════════════════════════════════════════════════════════════════╝
```

**Use Case:** Verify minimization reproducibility

---

### 5. **Reaction Discovery → Multiple XYZ**

**Command:**
```bash
./meso-discover --reactants A.xyz B.xyz --output reactions/
```

**Output directory structure:**
```
reactions/
├── reaction_001_reactants.xyz
├── reaction_001_products.xyz
├── reaction_001_ts.xyz
├── reaction_002_reactants.xyz
├── reaction_002_products.xyz
├── ...
└── summary.md
```

**Use Case:** Generate candidate reaction pathways

---

## Day #11 Regression Workflow

### A. Generate Polymer Test Case

```bash
# 1. Build polymer
./meso-build
⚛ build polyethylene 20
⚛ save pe20_initial.xyz
⚛ exit

# 2. Run minimization (save trajectory)
./meso-sim --mode minimize \
           --input pe20_initial.xyz \
           --output pe20_trace.xyza \
           --steps 1000 \
           --tol 0.001

# 3. Extract final frame
tail -n 22 pe20_trace.xyza > pe20_final.xyz

# 4. View trajectory
./interactive-viewer pe20_trace.xyza
```

### B. Analyze Results

```bash
# Extract energy trace
grep "E =" pe20_trace.xyza | awk '{print $5}' > energy.dat

# Plot in Python/Excel
python plot_energy.py energy.dat

# Check convergence
tail -n 1 pe20_trace.xyza
# Expected: F_max < 0.001 kcal/mol/Å
```

### C. Regression Snapshot

**Save baseline:**
```bash
mkdir -p regression/pe20/
cp pe20_initial.xyz regression/pe20/initial.xyz
cp pe20_final.xyz regression/pe20/baseline.xyz
cp energy.dat regression/pe20/energy_trace.dat
```

**Future testing:**
```bash
# Run new simulation
./meso-sim --mode minimize --input regression/pe20/initial.xyz --output new_trace.xyza

# Compare results
./meso-align --ref regression/pe20/baseline.xyz --target pe20_final.xyz

# Verify RMSD < 0.01 Å (pass) or > 0.1 Å (investigate)
```

---

## File Naming Conventions

### Recommended Patterns

**Input files:**
```
<molecule>_initial.xyz
<molecule>_ref.xyz
<molecule>_input.xyz
```

**Output files:**
```
<molecule>_final.xyz          # Last frame
<molecule>_optimized.xyz      # After minimization
<molecule>_trace.xyza         # Full trajectory
<molecule>_checkpoint.xyzc    # MD restart
```

**Regression tests:**
```
regression/<testname>/initial.xyz
regression/<testname>/baseline.xyz
regression/<testname>/energy_trace.dat
```

**Reaction discovery:**
```
reactions/rxn_<id>_reactants.xyz
reactions/rxn_<id>_products.xyz
reactions/rxn_<id>_ts.xyz
```

---

## Advanced Features

### Metadata Parsing

**Extract energy from trajectory:**
```bash
grep "E =" pe20_trace.xyza | awk '{print $5}'
```

**Count frames:**
```bash
grep -c "^20$" pe20_trace.xyza  # For 20-atom system
```

**Extract frame 42:**
```bash
awk '/^20$/{n++; if(n==42) print; next} n==42' pe20_trace.xyza
```

### Batch Processing

**Minimize all inputs:**
```bash
for f in inputs/*.xyz; do
    name=$(basename $f .xyz)
    ./meso-sim --mode minimize --input $f --output results/${name}_final.xyz
done
```

**Compare all to baseline:**
```bash
for f in results/*_final.xyz; do
    ./meso-align --ref baseline.xyz --target $f
done
```

---

## Integration with Tools

| Tool | Input | Output | Purpose |
|------|-------|--------|---------|
| **meso-build** | - | `.xyz` | Create molecules interactively |
| **meso-sim** | `.xyz` | `.xyz`, `.xyza`, `.xyzc` | Run simulations |
| **meso-align** | `.xyz`, `.xyz` | Alignment report | Compare structures |
| **meso-discover** | `.xyz`, `.xyz` | Multiple `.xyz` | Find reactions |
| **interactive-viewer** | `.xyz`, `.xyza` | - | Visualize (with animation) |
| **simple-viewer** | `.xyz` | - | Quick static view |

---

## Validation Rules

### XYZ Files Must:
- ✅ Have integer atom count on line 1
- ✅ Have exactly `atom_count + 2` lines
- ✅ Use valid element symbols (H, C, O, N, ...)
- ✅ Have numeric coordinates (Angstroms)
- ✅ Not contain NaN or Inf values

### XYZA Files Must:
- ✅ Contain multiple valid XYZ frames
- ✅ Have consistent atom count across frames
- ✅ Include energy/force metadata in comments
- ✅ Preserve element order (no atom permutation)

### XYZC Files Must:
- ✅ Include velocities (6 values per atom)
- ✅ Have CHECKPOINT keyword in comment
- ✅ Include step, time, and energy metadata
- ✅ Be restartable (deterministic with seed)

---

## Error Handling

### Common Issues

**Mismatch atom count:**
```
ERROR: Expected 20 atoms, found 19 in frame 42
```
**Solution:** Check if minimization broke bonds or lost atoms

**NaN coordinates:**
```
ERROR: NaN detected at atom 7, frame 103
```
**Solution:** Numerical instability, reduce timestep or check forces

**Missing metadata:**
```
WARNING: Frame 25 missing energy value
```
**Solution:** Add `E = <value>` to comment line

---

## Best Practices

1. **Always save initial geometry:**
   ```bash
   cp molecule.xyz molecule_initial.xyz
   ```

2. **Use descriptive comments:**
   ```
   # Good:
   Polyethylene 20-mer | E = -45.23 kcal/mol | Step 1000 | Converged
   
   # Bad:
   Output file
   ```

3. **Version control XYZ files:**
   ```bash
   git add regression/*/baseline.xyz
   git commit -m "Add PE20 regression baseline"
   ```

4. **Check convergence before using results:**
   ```bash
   tail -n 1 trace.xyza | grep "F_max"
   # Ensure F_max < 0.001 kcal/mol/Å
   ```

5. **Keep trajectories for debugging:**
   ```bash
   # Don't just save final frame
   ./meso-sim --output full_trace.xyza  # Keep intermediate steps
   ```

---

## Performance Notes

### File Sizes

| System | Frames | File Size (.xyza) |
|--------|--------|-------------------|
| Water (3 atoms) | 1000 | ~80 KB |
| Ethane (8 atoms) | 1000 | ~200 KB |
| PE 20-mer (62 atoms) | 1000 | ~1.5 MB |
| PE 100-mer (302 atoms) | 1000 | ~7 MB |

### Loading Times (interactive-viewer)

| File Size | Load Time | Smoothness |
|-----------|-----------|------------|
| < 1 MB | Instant | 60 FPS |
| 1-10 MB | ~1 sec | 60 FPS |
| 10-50 MB | ~5 sec | 30-60 FPS |
| > 50 MB | ~10 sec | 15-30 FPS |

**Recommendation:** For large systems, save every 10th or 100th frame to reduce file size.

---

## Future Extensions (Planned)

- **XYZB** - Binary XYZ format (faster I/O)
- **XYZP** - Periodic boundary conditions (box vectors)
- **XYZR** - Reaction coordinate tracking
- **XYZF** - Force field parameters embedded

---

## Example Scripts

### Extract Energy Trace
```python
#!/usr/bin/env python3
import sys
import re

energies = []
with open(sys.argv[1]) as f:
    for line in f:
        match = re.search(r'E = ([-\d.]+)', line)
        if match:
            energies.append(float(match.group(1)))

for i, e in enumerate(energies):
    print(f"{i:6d}  {e:12.6f}")
```

### Validate XYZ File
```bash
#!/bin/bash
file=$1
natoms=$(head -n 1 $file)
nlines=$(wc -l < $file)
expected=$((natoms + 2))

if [ "$nlines" -ne "$expected" ]; then
    echo "ERROR: Expected $expected lines, found $nlines"
    exit 1
fi

echo "✓ Valid XYZ file ($natoms atoms)"
```

---

## See Also

- [`MESO_SIM_GUIDE.md`](../apps/MESO_SIM_GUIDE.md) - Simulation workflows
- [`ALIGNMENT_GUIDE.md`](../apps/ALIGNMENT_GUIDE.md) - Structure comparison
- [`MESO_BUILD_GUIDE.md`](../apps/MESO_BUILD_GUIDE.md) - Interactive molecule builder
- [`INTERACTIVE_UI_GUIDE.md`](../src/vis/INTERACTIVE_UI_GUIDE.md) - Viewer controls

---

**Ready to use file formats?**

```bash
# Quick start
./meso-build                      # Create molecule → XYZ
./meso-sim --mode minimize        # Run simulation → XYZA
./interactive-viewer output.xyza  # Visualize trajectory
./meso-align --ref A.xyz --target B.xyz  # Compare structures
```

**All formats are human-readable. Open in any text editor to inspect!**
