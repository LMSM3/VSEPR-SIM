# VSEPR-Sim Usage Reference

Complete usage documentation for all CLI executables and the Qt6 desktop application console.

> **Environment note** — All commands run inside WSL.  
> From a Windows PowerShell terminal, prefix every command with `wsl bash -c "..."` or open a WSL shell directly.

---

## Table of Contents

1. [Build](#1-build)
2. [CLI — Verification executables](#2-cli--verification-executables)
3. [CLI — `atomistic-build` interactive builder](#3-cli--atomistic-build-interactive-builder)
4. [CLI — `atomistic-relax` FIRE minimiser](#4-cli--atomistic-relax-fire-minimiser)
5. [CLI — `atomistic-sim` simulation engine](#5-cli--atomistic-sim-simulation-engine)
6. [CLI — `atomistic` unified dispatcher](#6-cli--atomistic-unified-dispatcher)
7. [CLI — `atomistic-align` structure alignment](#7-cli--atomistic-align-structure-alignment)
8. [CLI — `atomistic-discover` reaction pathways](#8-cli--atomistic-discover-reaction-pathways)
9. [CLI — Empirical reference scripts](#9-cli--empirical-reference-scripts)
10. [Desktop application — overview](#10-desktop-application--overview)
11. [Desktop — menus and toolbar](#11-desktop--menus-and-toolbar)
12. [Desktop — console command reference](#12-desktop--console-command-reference)
13. [Desktop — keyboard shortcuts](#13-desktop--keyboard-shortcuts)
14. [File formats](#14-file-formats)
15. [Environment variables](#15-environment-variables)
16. [Exit codes](#16-exit-codes)

---

## 1. Build

```bash
# Standard build (engine + phase executables + deep verification)
wsl bash -c "cmake -B wsl-build -DBUILD_VIS=OFF -DBUILD_TESTS=ON && cmake --build wsl-build -j4"

# Desktop application (requires Qt6)
wsl bash -c "cmake -B wsl-build-desktop -DBUILD_DESKTOP=ON -DBUILD_VIS=OFF && cmake --build wsl-build-desktop -j4"

# Clean rebuild
wsl bash -c "rm -rf wsl-build && cmake -B wsl-build -DBUILD_VIS=OFF -DBUILD_TESTS=ON && cmake --build wsl-build -j4"
```

### CMake options

| Option | Default | Effect |
|--------|---------|--------|
| `BUILD_TESTS` | `OFF` | Build phase verification executables |
| `BUILD_DESKTOP` | `OFF` | Build Qt6 desktop application |
| `BUILD_VIS` | `OFF` | Build legacy ImGui visualiser |
| `BUILD_TOOLS` | `OFF` | Build developer tools (`auto_fit_alpha`, etc.) |

---

## 2. CLI — Verification executables

### `run_all_verification.sh` — one-command tiered run

```bash
# Quick local run (Suites A–N, Q, I–M — no network, ~10 s)
wsl bash scripts/run_all_verification.sh

# Full run — also fetches PubChem/NIST and runs Suite P
wsl bash scripts/run_all_verification.sh --full

# Full run with per-check PASS/FAIL output
wsl bash scripts/run_all_verification.sh --full --verbose
```

Exit code 0 = all executed checks pass.

---

### `deep_verification` — 256-check deterministic suite

```
./wsl-build/deep_verification [OPTIONS]
```

| Option | Default | Description |
|--------|---------|-------------|
| `--seed N` | random | Fix RNG seed for randomised suites I–M |
| `--rand-iters N` | `500` | Samples per randomised suite |
| `--verbose` | off | Print each individual PASS/FAIL line |

**Examples:**

```bash
# Standard run (random seed, printed at end)
wsl bash -c "cd /mnt/c/Users/Liam/Desktop/vsepr-sim && ./wsl-build/deep_verification"

# Reproduce the recorded Milestone A run exactly
wsl bash -c "cd /mnt/c/Users/Liam/Desktop/vsepr-sim && ./wsl-build/deep_verification --seed 1772952709"

# Stress-test: 2 000 samples per randomised suite
wsl bash -c "cd /mnt/c/Users/Liam/Desktop/vsepr-sim && ./wsl-build/deep_verification --rand-iters 2000 --verbose"
```

**Output files** (written to `verification/deep/`):

| File | Content |
|------|---------|
| `report.txt` | Full per-suite log with pass/fail and numeric detail |
| `status.txt` | `PASS` or `FAIL`, seed, counts — for CI scripting |

---

### Phase executables (1–14)

Each phase is a self-contained executable that prints PASS/FAIL. Run individually or via the verification script.

```bash
# Run a single phase
./wsl-build/phase1_kernel_audit
./wsl-build/phase2_structural_energy
./wsl-build/phase3_relaxation
./wsl-build/phase4_formation_priors
./wsl-build/phase5_crystal_validation
./wsl-build/phase6_7_verification_sessions
./wsl-build/phase8_9_10_environment
./wsl-build/phase11_12_13_final
./wsl-build/phase14_milestones
```

No arguments required or accepted. Exit code 0 = all checks in that phase pass.

---

## 3. CLI — `atomistic-build` interactive builder

Builds molecules from chemical formulas using the formation pipeline:
`formula → parse → VSEPR placement → FIRE optimisation → validate`

```
./wsl-build/atomistic-build              # enter interactive prompt
./wsl-build/atomistic-build script.txt  # run commands from file
```

### Interactive commands

| Command | Example | Description |
|---------|---------|-------------|
| `build <formula>` | `build H2O` | Build from formula (VSEPR placement + FIRE) |
| `load <file.xyz>` | `load benzene.xyz` | Load structure from XYZ file |
| `save <file.xyz>` | `save result.xyz` | Save current structure |
| `info` | | Atom count, bonds, energy, formula |
| `validate` | | Run validation gates on current structure |
| `identity` | | Show canonical identity and fingerprints |
| `clear` | | Clear the current structure |
| `help` | | Show command reference |
| `exit` | | Exit |

### Script file format

Plain text, one command per line. Blank lines and lines starting with `#` are ignored.

```
# Build a water molecule and save it
build H2O
validate
save water.xyz

# Build methane
build CH4
info
save methane.xyz
```

Run a script:
```bash
wsl bash -c "./wsl-build/atomistic-build scripts/build_batch.txt"
```

---

## 4. CLI — `atomistic-relax` FIRE minimiser

```
./wsl-build/atomistic-relax <input.xyz> [OPTIONS]
```

| Option | Default | Description |
|--------|---------|-------------|
| `--model <name>` | `lj_coulomb` | Force model: `lj`, `lj_coulomb` |
| `--epsilon <val>` | `0.1` | LJ ε (kcal/mol) |
| `--sigma <val>` | `3.0` | LJ σ (Å) |
| `--max-iter <n>` | `1000` | Maximum FIRE steps |
| `--force-tol <f>` | `0.01` | Force RMS convergence threshold (kcal/mol/Å) |
| `--output <file>` | `relaxed.xyza` | Output XYZA trajectory |
| `--report <file>` | `report.md` | Markdown convergence report |
| `--help` | | Show usage |

**Examples:**

```bash
# Relax water with defaults
wsl bash -c "./wsl-build/atomistic-relax data/water.xyz"

# Tight convergence, custom output
wsl bash -c "./wsl-build/atomistic-relax molecule.xyz --force-tol 1e-5 --max-iter 5000 --output tight.xyza"

# LJ-only model
wsl bash -c "./wsl-build/atomistic-relax argon_cluster.xyz --model lj"
```

---

## 5. CLI — `atomistic-sim` simulation engine

```
./wsl-build/atomistic-sim <mode> [OPTIONS] <input.xyz>
```

### Modes

| Mode | Description |
|------|-------------|
| `energy` | Single-point energy evaluation — no motion |
| `optimize` | Geometry optimisation (FIRE minimiser) |
| `conformers` | Generate and cluster conformer ensemble |
| `md-nve` | Molecular dynamics, constant energy (NVE) |
| `md-nvt` | Molecular dynamics, constant temperature (NVT Langevin) |
| `adaptive` | Adaptive sampling with convergence detection |
| `predict` | Predict properties from VSEPR topology |
| `reaction` | Estimate reaction energy and barrier |
| `merge` | Merge and analyse multiple simulation outputs |

### Common options (all modes)

| Option | Default | Description |
|--------|---------|-------------|
| `--output <dir>` | `atomistic_output` | Output directory |
| `--cutoff <Å>` | `10.0` | Non-bonded cutoff radius |
| `--temp <K>` | `300` | Temperature (MD modes) |
| `--steps <n>` | mode-dependent | Number of integration steps |
| `--no-bonded` | off | Disable bonded interactions |
| `--no-nonbonded` | off | Disable non-bonded interactions |

### Mode-specific options

#### `optimize`
```bash
./wsl-build/atomistic-sim optimize --steps 2000 --cutoff 12.0 molecule.xyz
```

#### `md-nvt`
```bash
./wsl-build/atomistic-sim md-nvt --temp 300 --steps 10000 molecule.xyz
./wsl-build/atomistic-sim md-nvt --temp 350 --steps 50000 protein.xyz --output sim_350K
```

#### `md-nve`
```bash
./wsl-build/atomistic-sim md-nve --steps 5000 argon.xyz
```

#### `conformers`
```bash
./wsl-build/atomistic-sim conformers --output ethane_confs ethane.xyz
```

#### `energy`
```bash
./wsl-build/atomistic-sim energy h2o.xyz
```

---

## 6. CLI — `atomistic` unified dispatcher

```
./wsl-build/atomistic [GLOBAL FLAGS] <subcommand> [OPTIONS]
```

### Global flags

| Flag | Description |
|------|-------------|
| `--config <file>` | Load configuration file (default: `atomistic.yaml`) |
| `--seed <N>` | Global random seed |
| `--verbose` | Enable verbose output |
| `--quiet` | Suppress informational messages |
| `--version`, `-v` | Show version |
| `--help`, `-h` | Show help |

### Subcommands

| Subcommand | Description |
|------------|-------------|
| `build` | Build molecules interactively or from templates |
| `sim` | Run simulations (minimize, md, energy, torsion, conformers) |
| `align` | Align and compare structures (Kabsch, RMSD) |
| `discover` | Discover reaction pathways and transition states |
| `view` | Visualise molecules and trajectories (legacy ImGui) |
| `validate` | Validate XYZ/XYZA/XYZC file format |
| `inspect` | Inspection tools (stats, energy, forces, histogram) |
| `config` | Configuration management (init, show, validate) |

**Examples:**

```bash
./wsl-build/atomistic build --template cisplatin -o cisplatin.xyz
./wsl-build/atomistic sim minimize input.xyz -o output.xyza --steps 1000
./wsl-build/atomistic --seed 42 --verbose sim md-nvt molecule.xyz
```

---

## 7. CLI — `atomistic-align` structure alignment

Aligns two structures using the Kabsch algorithm and reports RMSD.

```
./wsl-build/atomistic-align <reference.xyz> <target.xyz> [OPTIONS]
```

| Option | Description |
|--------|-------------|
| `--output <file>` | Write aligned target to file |
| `--no-translate` | Skip centroid alignment |
| `--verbose` | Print per-atom deviation |

---

## 8. CLI — `atomistic-discover` reaction pathways

```
./wsl-build/atomistic-discover <reactant.xyz> [OPTIONS]
```

| Option | Description |
|--------|-------------|
| `--temp <K>` | Temperature for heat-gated reaction control |
| `--steps <n>` | Exploration steps |
| `--output <dir>` | Directory for pathway outputs |

---

## 9. CLI — Empirical reference scripts

### `fetch_empirical.py` — download PubChem + NIST references

```bash
# Live download (requires network; uses stdlib urllib, no pip needed)
wsl python3 scripts/fetch_empirical.py

# Offline fallback only (use bundled defaults, skip download)
wsl python3 scripts/fetch_empirical.py --offline

# Verbose: print each fetch URL and response status
wsl python3 scripts/fetch_empirical.py --verbose

# Custom output directory
wsl python3 scripts/fetch_empirical.py --output-dir data/refs
```

Downloads:
- 25 bond lengths from PubChem 3D conformers
- 20 diatomic spectroscopic constants from NIST WebBook
- 15 solvent dielectrics from CRC Handbook 104

Output: `atomistic/core/downloaded_refs.hpp` (regenerated on each run).

---

### `gen_empirical_ref.py` — regenerate compile-time reference header

```bash
wsl python3 scripts/gen_empirical_ref.py
```

Regenerates `atomistic/core/empirical_reference.hpp` from the source tables.
Run after editing the reference data tables in the script.

---

## 10. Desktop application — overview

The Qt6 desktop application provides a graphical interface to the same atomistic kernel.

```bash
# Launch (from WSL)
wsl bash -c "./wsl-build-desktop/vsepr-desktop"
```

### Window layout

```
┌──────────────────────────────────────────────────────────────────┐
│ Menu bar                                                          │
├──────────────────────────────────────────────────────────────────┤
│ Main toolbar    [Open][Save] | [SP][Relax][MD] | [Cam][BS][CPK…] │
├──────────────────────────────────────────────────────────────────┤
│ Trajectory bar  [◀][▶ Play][▶] ══════slider══════ Frame N / M    │
├─────────────┬────────────────────────────────┬───────────────────┤
│ Objects     │                                │ Properties        │
│ (left dock) │   3D Viewport                  │ (right dock)      │
│             │   OpenGL 3.3 Core              │                   │
│ Molecules   │   orbit / pan / zoom           │ Identity          │
│  └─ name    │   left-click = pick atom       │ Energy breakdown  │
│ Crystals    │                                │ Lattice           │
│ Results     │                                │ Simulation        │
│             │                                │ Selected Atom     │
├─────────────┴────────────────────────────────┴───────────────────┤
│ Console / Log  (bottom dock)                                      │
│ [hh:mm:ss] message…                                               │
│ > command_                                                        │
├──────────────────────────────────────────────────────────────────┤
│ Status bar    Atoms: N  |  E: X.XXXX kcal/mol  |  Frame: N/M     │
└──────────────────────────────────────────────────────────────────┘
```

### Viewport interaction

| Action | Gesture |
|--------|---------|
| Orbit | Left-drag |
| Pan | Right-drag or Middle-drag |
| Zoom | Scroll wheel |
| Pick atom | Left-click (no drag) |
| Deselect | `Esc` |
| Step frame forward | `→` or `.` |
| Step frame backward | `←` or `,` |
| Play / Pause | `Space` |

---

## 11. Desktop — menus and toolbar

### File menu

| Item | Shortcut | Action |
|------|----------|--------|
| Open XYZ… | `Ctrl+O` | Load `.xyz`, `.xyza`, or `.xyzc` file |
| Save XYZ… | `Ctrl+S` | Save current frame to `.xyz` |
| Export Image… | `Ctrl+Shift+E` | Save viewport as PNG or JPEG |
| Exit | `Ctrl+Q` | Close application |

### Simulation menu

| Item | Shortcut | Action |
|------|----------|--------|
| Single Point | `F4` | Evaluate energy and forces — no motion |
| FIRE Relax | `F5` | Run FIRE minimisation (2 000 steps, tol 1×10⁻⁴) |
| Run MD | `F6` | Run NVT Langevin dynamics (300 K, 1 000 steps, dt=1 fs) |
| Crystal Preset… | `Ctrl+K` | Open crystal builder dialog (7 presets, supercell na×nb×nc) |

**Crystal Preset dialog** — presets available:

| Key | Structure | a (Å) |
|-----|-----------|-------|
| `Cu` | FCC copper | 3.615 |
| `Fe` | BCC iron | 2.870 |
| `NaCl` | Rocksalt | 5.640 |
| `Si` | Diamond cubic | 5.431 |
| `Al` | FCC aluminium | 4.050 |
| `MgO` | Rocksalt | 4.212 |
| `CsCl` | CsCl-type | 4.123 |

### View menu

| Item | Shortcut | Action |
|------|----------|--------|
| Reset Camera | `R` | Reset orbit + fit to structure |
| Fit to Structure | `F` | Fit camera to current bounding sphere |
| Render Mode → Ball & Stick | | CPK spheres + two-tone cylinders |
| Render Mode → Space Fill (CPK) | | VDW radii, no bonds |
| Render Mode → Sticks | | Thin spheres + slim cylinders |
| Render Mode → Wireframe | `W` | GL polygon lines |
| Element Labels | `L` | Overlay element symbols on atoms |
| Unit Cell Box | `B` | Draw PBC lattice box (blue wireframe) |
| Axes Indicator | `A` | XYZ corner axes (red/green/blue) |

---

## 12. Desktop — console command reference

The console accepts single-word commands (case-insensitive). Type `help` at the `>` prompt for a live summary.

Navigate history with `↑` / `↓` arrow keys.

### File

| Command | Equivalent menu action |
|---------|----------------------|
| *(use menu)* | File → Open / Save |

### Simulation

| Command | Aliases | Action |
|---------|---------|--------|
| `relax` | `fire` | FIRE minimisation (F5) |
| `md` | `nvt` | Langevin MD 300 K (F6) |
| `sp` | `energy`, `single` | Single-point evaluation (F4) |
| `crystal` | `preset` | Open Crystal Preset dialog |

### Camera

| Command | Aliases | Action |
|---------|---------|--------|
| `reset` | | Reset camera to default orbit |
| `fit` | | Fit camera to structure |

### Render mode

| Command | Aliases | Render style |
|---------|---------|--------------|
| `bs` | `balls` | Ball & Stick |
| `cpk` | `spacefill` | Space Fill (VDW) |
| `sticks` | | Sticks |
| `wire` | `wireframe` | Wireframe |

### Overlays

| Command | Action |
|---------|--------|
| `labels` | Toggle element label overlay |
| `axes` | Toggle XYZ axes indicator |
| `box` | Toggle unit cell box |

### Trajectory playback

| Command | Action |
|---------|--------|
| `play` | Start / pause auto-play |
| `stop` | Stop playback |
| `next` | `>` — advance one frame |
| `prev` | `<` — go back one frame |

### Console

| Command | Action |
|---------|--------|
| `clear` | Clear console output |
| `help` | Print command reference in console |

---

## 13. Desktop — keyboard shortcuts

### Global

| Key | Action |
|-----|--------|
| `Ctrl+O` | Open XYZ file |
| `Ctrl+S` | Save XYZ file |
| `Ctrl+Shift+E` | Export viewport image |
| `Ctrl+K` | Crystal Preset dialog |
| `F4` | Single-point energy |
| `F5` | FIRE Relax |
| `F6` | Run MD |

### Viewport (when viewport has focus)

| Key | Action |
|-----|--------|
| `R` | Reset camera |
| `F` | Fit camera to structure |
| `W` | Toggle wireframe |
| `L` | Toggle element labels |
| `A` | Toggle axes indicator |
| `B` | Toggle unit cell box |
| `Space` | Play / pause trajectory |
| `→` or `.` | Step forward one frame |
| `←` or `,` | Step back one frame |
| `Esc` | Deselect atom |

---

## 14. File formats

| Extension | Name | Description |
|-----------|------|-------------|
| `.xyz` | XYZ | Standard Cartesian: `N\ncomment\nSym x y z\n…` |
| `.xyza` | Animated XYZ | Concatenated `.xyz` frames — trajectory output |
| `.xyzc` | Checkpointed XYZ | `.xyz` + velocities + thermodynamic metadata + SHA-256 hash |

### XYZ example

```
3
water H2O  E=-0.4821 kcal/mol
O   0.000000   0.000000   0.119262
H   0.000000   0.763239  -0.477047
H   0.000000  -0.763239  -0.477047
```

### XYZC metadata block (appended after coordinates)

```
VELOCITIES
H   0.001234  -0.000456   0.000789
...
THERMODYNAMICS
steps    5000
temperature  300.0
energy_total  -12.4831
force_rms    4.21e-05
SHA256   a3f7c8d1...
```

---

## 15. Environment variables

| Variable | Default | Effect |
|----------|---------|--------|
| `VSEPR_SEED` | (random) | Override RNG seed for all executables that accept it |
| `VSEPR_VERBOSE` | `0` | Set to `1` to enable verbose output globally |
| `VSEPR_DATA_DIR` | `./data` | Directory containing `elements.physics.json` |

---

## 16. Exit codes

| Code | Meaning |
|------|---------|
| `0` | All checks pass / operation successful |
| `1` | One or more checks failed (verification executables) |
| `1` | Usage error / file not found (simulation executables) |
| `2` | Internal engine error |

---

*See [`docs/INDEX.md`](../INDEX.md) for the full methodology document index.*  
*See [`docs/verification/milestone_A.md`](verification/milestone_A.md) for the recorded Milestone A baseline.*  
*See [`docs/deep_verification_paper.tex`](deep_verification_paper.tex) for the complete system paper.*
