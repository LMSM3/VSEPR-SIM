# VSEPR-SIM Usage Command Reference
<!-- v5.0.14 | branch: v5.0.0-main | Day 75 -->

This file collects real, runnable commands for every major executable and
subsystem in the VSEPR-SIM platform.  All paths assume you are working from
the repo root (`C:\R\VSPER-SIM`) with `build\` on your PATH (or call via the
full path shown).

---

## 0. Build & Rebuild

```powershell
# Configure + build (release preset — the canonical way)
cmake --preset release
cmake --build build --parallel

# Run the full CTest suite
ctest --test-dir build --output-on-failure

# Run only a named group
ctest --test-dir build -R "DynxEmitter" --output-on-failure
ctest --test-dir build -R "Group 6[5-9]|CtlEndToEnd" --output-on-failure

# Top-level build wrapper
.\build.ps1
```

---

## 1. `vsepr` — Main CLI

### 1.1 Version / info

```powershell
.\build\vsepr.exe --version
.\build\vsepr.exe --build-info
.\build\vsepr.exe --help
.\build\vsepr.exe modules
```

### 1.2 Doctor — installation health

```powershell
# Full health check (data files, PATH, pipeline)
.\build\vsepr.exe doctor

# Dependency-ordered integration test sequence
.\build\vsepr.exe doctor integratedtest

# Timed throughput benchmarks
.\build\vsepr.exe doctor benchmark
```

### 1.3 Run a `.vsim` script

```powershell
# Short form
.\build\vsepr.exe scripts\demos\water.vsim

# Explicit 'run' subcommand (identical result)
.\build\vsepr.exe run scripts\demos\water.vsim
.\build\vsepr.exe run scripts\demos\methane.vsim
.\build\vsepr.exe run scripts\demos\argon_gas.vsim
.\build\vsepr.exe run scripts\demos\crystal_defect.vsim
.\build\vsepr.exe run scripts\demos\graphite_surface.vsim
.\build\vsepr.exe run scripts\demos\surface_projectile.vsim
.\build\vsepr.exe run scripts\demos\simple_md_run.vsim

# Gallery scripts
.\build\vsepr.exe run scripts\gallery\nacl_pbc_supercell.vsim
.\build\vsepr.exe run scripts\gallery\helium_reactor.vsim
.\build\vsepr.exe run scripts\gallery\room_reactor.vsim
.\build\vsepr.exe run scripts\gallery\thermal_shell_demo.vsim
.\build\vsepr.exe run scripts\gallery\calibration_htgr.vsim
.\build\vsepr.exe run scripts\gallery\graphite_events_live.vsim

# Provided examples
.\build\vsepr.exe run examples\gas_mixing_demo.vsim
.\build\vsepr.exe run examples\wo61d_canonical.vsim
.\build\vsepr.exe run examples\wo61d_T1_field_projection.vsim
.\build\vsepr.exe run examples\wo61d_T2_rve_uniform.vsim
.\build\vsepr.exe run examples\wo61d_T3_heterogeneous.vsim
```

### 1.4 Validate without running

```powershell
.\build\vsepr.exe validate scripts\demos\water.vsim
.\build\vsepr.exe validate scripts\demos\argon_gas.vsim
.\build\vsepr.exe validate examples\gas_mixing_demo.vsim
```

### 1.5 View output files

```powershell
# Single-frame static geometry
.\build\vsepr.exe view out\water_demo\water_demo.xyz

# Multi-frame trajectory browser
.\build\vsepr.exe view out\argon_gas_demo\argon_gas_demo.xyzFull

# Dynamic session archive
.\build\vsepr.exe view out\argon_gas_demo\argon_gas_demo.dynx

# Compact 700x900 window
.\build\vsepr.exe view --small out\methane_demo\methane_demo.xyzFull
```

### 1.6 Demo — rotating molecule viewer

```powershell
# Random molecule each run
.\build\vsepr.exe --demo

# List all 20 demo molecules
.\build\vsepr.exe --demo --list

# Specific molecule by number
.\build\vsepr.exe --demo1        # H2O
.\build\vsepr.exe --demo2        # NH3
.\build\vsepr.exe --demo3        # CH4
.\build\vsepr.exe --demo4        # CO2
.\build\vsepr.exe --demo5        # BF3
.\build\vsepr.exe --demo6        # SF6
.\build\vsepr.exe --demo7        # PCl5
.\build\vsepr.exe --demo8        # XeF4
.\build\vsepr.exe --demo9        # C6H6
.\build\vsepr.exe --demo10       # H2O2
.\build\vsepr.exe --demo11       # SO3
.\build\vsepr.exe --demo12       # Rnd3[ClBN]
.\build\vsepr.exe --demo13       # NO2
.\build\vsepr.exe --demo16       # ClF3
.\build\vsepr.exe --demo19       # C2H4
.\build\vsepr.exe --demo20       # ICl3

# Force by formula name
.\build\vsepr.exe --demo --molecule H2O
.\build\vsepr.exe --demo --molecule SF6
.\build\vsepr.exe --demo --molecule CH4
.\build\vsepr.exe --demo --molecule NH3
```

### 1.7 Demo0 — element tour (Z=1..102)

```powershell
# Cycles all 102 elements at semi-random rate
.\build\vsepr.exe --demo0

# List all 102 elements
.\build\vsepr.exe --demo0 --list

# Start at a specific atomic number
.\build\vsepr.exe --demo0 --start 6     # Carbon
.\build\vsepr.exe --demo0 --start 26    # Iron
.\build\vsepr.exe --demo0 --start 79    # Gold
.\build\vsepr.exe --demo0 --start 92    # Uranium
```

### 1.8 Settings

```powershell
# Show current visual settings
.\build\vsepr.exe settings show

# Set individual values
.\build\vsepr.exe settings set theme dark
.\build\vsepr.exe settings set atom_radius_scale 1.6
.\build\vsepr.exe settings set bond_radius 0.12
.\build\vsepr.exe settings set trajectory_fps 60
.\build\vsepr.exe settings set show_bonds true
.\build\vsepr.exe settings set show_axes false
.\build\vsepr.exe settings set label_font_size 14
.\build\vsepr.exe settings set background_color 0x000000

# Reset all settings to defaults
.\build\vsepr.exe settings reset
```

### 1.9 Shell file associations (Windows)

```powershell
# Register .vsim and .x file associations
.\build\vsepr.exe install register-associations

# Dry run (preview what would be written)
.\build\vsepr.exe install register-associations --dry-run

# Remove associations
.\build\vsepr.exe install unregister-associations
```

---

## 2. `.X` Suite Bundle

```powershell
# Run a saved suite
.\build\vsepr.exe x run        my_suite.X

# Inspect bundle contents
.\build\vsepr.exe x inspect    my_suite.X

# Validate file presence and contracts
.\build\vsepr.exe x validate   my_suite.X

# Replay trajectory stored in a suite
.\build\vsepr.exe x replay     my_suite.X
```

---

## 3. Gas Module (`vsepr gas`)

```powershell
# Show full help
.\build\vsepr.exe gas help

# Properties at STP
.\build\vsepr.exe gas props Ar
.\build\vsepr.exe gas props N2
.\build\vsepr.exe gas props H2O
.\build\vsepr.exe gas props CO2
.\build\vsepr.exe gas props CH4
.\build\vsepr.exe gas props He

# Properties at custom conditions
.\build\vsepr.exe gas props Ar  -T 300  -P 1.0
.\build\vsepr.exe gas props CO2 -T 500  -P 2.0  -n 0.5
.\build\vsepr.exe gas props NH3 -T 400  -P 5.0

# Maxwell-Boltzmann velocity sampling
.\build\vsepr.exe gas sample N2  -T 300  -N 5000  --histogram
.\build\vsepr.exe gas sample Ar  -T 500  -N 10000 --histogram
.\build\vsepr.exe gas sample H2O -T 373  -N 2000  --seed 123
.\build\vsepr.exe gas sample O2  -T 300  -N 1000
```

---

## 4. Gas2 Module (`vsepr gas2`) — Advanced EOS Analysis

```powershell
# Full analysis: EOS + kinetic + thermal
.\build\vsepr.exe gas2 analyze Ar  -T 300  -P 1.0
.\build\vsepr.exe gas2 analyze CO2 -T 500  -P 10.0
.\build\vsepr.exe gas2 analyze N2  -T 77   -P 1.0    # near boiling point
.\build\vsepr.exe gas2 analyze H2O -T 373  -P 1.0    --json

# Thermal property report (DOF, Cp, Cv, gamma)
.\build\vsepr.exe gas2 thermal Ar
.\build\vsepr.exe gas2 thermal N2  -T 500
.\build\vsepr.exe gas2 thermal CO2 -T 800  -P 5.0
.\build\vsepr.exe gas2 thermal CH4 -T 300

# Three-EOS comparison: Ideal vs VdW vs Redlich-Kwong
.\build\vsepr.exe gas2 compare Ar  -T 300 -P 1.0
.\build\vsepr.exe gas2 compare CO2 -T 400 -P 50.0
.\build\vsepr.exe gas2 compare N2  -T 150 -P 30.0

# Phase equilibrium (A, G, mu, Maxwell construction)
.\build\vsepr.exe gas2 phase Ar
.\build\vsepr.exe gas2 phase CO2 -T 304
.\build\vsepr.exe gas2 phase H2O -T 373

# 8-channel potential decomposition
.\build\vsepr.exe gas2 potentials Ar  -T 300
.\build\vsepr.exe gas2 potentials N2  -T 300  --json
.\build\vsepr.exe gas2 potentials CO2 -T 500  --compact

# Maxwell-Boltzmann sampling (gas2 variant)
.\build\vsepr.exe gas2 sample Ar  -T 300 -N 10000
.\build\vsepr.exe gas2 sample N2  -T 500 -N 50000 --seed 7

# Heat maps (atom grid output, pipe to Python visualizer)
.\build\vsepr.exe gas2 heatmap Ar  -T 300 --grid 64
.\build\vsepr.exe gas2 heatmap CO2 -T 500 --grid 256

# Species database
.\build\vsepr.exe gas2 species
.\build\vsepr.exe gas2 species Ar
.\build\vsepr.exe gas2 species CO2
.\build\vsepr.exe gas2 species H2O

# Steady-state monitor (writes JSON to disk)
.\build\vsepr.exe gas2 monitor Ar  -T 300
.\build\vsepr.exe gas2 monitor N2  -T 500 --json

# Interactive terminal UI (species browser + live analysis + sweep)
.\build\vsepr.exe gas2 tui
.\build\vsepr.exe gas2 tui Ar
.\build\vsepr.exe gas2 tui CO2 -T 500
```

---

## 5. Gas3 Module (`vsepr gas3`) — Quality Pipeline

```powershell
# Quick sanity check: all species, all models, STP
.\build\vsepr.exe gas3 quick

# Linear deterministic sweep with CSV + HTML output
.\build\vsepr.exe gas3 sweep --T-min 100 --T-max 2000 --T-step 50
.\build\vsepr.exe gas3 sweep --species Ar,N2,CO2 --output out\gas3_sweep

# Random sampling stress test
.\build\vsepr.exe gas3 random --random 500 --seed 42
.\build\vsepr.exe gas3 random --random 2000 --seed 99 --verbose

# Full quality pipeline (linear + random + adaptive + fit + report)
.\build\vsepr.exe gas3 pipeline --output out\gas3_pipeline --name thermal_run_01
.\build\vsepr.exe gas3 pipeline --species Ar,N2,O2 --T-min 200 --T-max 1500

# Interactive terminal UI
.\build\vsepr.exe gas3 tui
.\build\vsepr.exe gas3 tui Ar
```

---

## 6. Coarse-Grain Module (`vsepr cg`)

```powershell
# Build bead scenes from presets
.\build\vsepr.exe cg scene --preset pair    --spacing 4.0
.\build\vsepr.exe cg scene --preset stack   --beads 8   --spacing 3.5
.\build\vsepr.exe cg scene --preset cloud   --beads 20  --spacing 15.0  --seed 42

# Inspect bead positions, environment, descriptors
.\build\vsepr.exe cg inspect
.\build\vsepr.exe cg inspect --all

# Environment update pipeline (eta relaxation)
.\build\vsepr.exe cg env --steps 200  --dt 1.0  --tau 100.0
.\build\vsepr.exe cg env --steps 500  --dt 0.5

# Pairwise interaction and energy decomposition
.\build\vsepr.exe cg interact
.\build\vsepr.exe cg interact --all

# LL-FIRE minimization on bead system
.\build\vsepr.exe cg fire

# Lightweight bead visualization
.\build\vsepr.exe cg viz
```

---

## 7. Live Analysis Server

```powershell
# Start live analysis HTTP server on port 99998
.\build\vsepr.exe serve

# Dual-port live viz stream server (ports 9999 + 10001)
.\build\vsepr.exe viz Ar
.\build\vsepr.exe viz N2
.\build\vsepr.exe viz H2O
.\build\vsepr.exe viz CO2
.\build\vsepr.exe viz C6H6
```

---

## 8. `atomistic-sim` — Full Simulation Engine

```powershell
# Single-point energy evaluation
.\build\atomistic-sim.exe energy    water.xyz
.\build\atomistic-sim.exe energy    nacl.xyz  --cutoff 12.0

# Geometry optimization (FIRE minimizer)
.\build\atomistic-sim.exe optimize  water.xyz
.\build\atomistic-sim.exe optimize  methane.xyz  --output opt_methane
.\build\atomistic-sim.exe optimize  crystal.xyz  --no-nonbonded

# Molecular dynamics — constant energy (NVE)
.\build\atomistic-sim.exe md-nve    argon.xyz   --steps 10000
.\build\atomistic-sim.exe md-nve    water.xyz   --temp 300  --steps 5000

# Molecular dynamics — constant temperature (NVT)
.\build\atomistic-sim.exe md-nvt    argon.xyz   --temp 300  --steps 50000
.\build\atomistic-sim.exe md-nvt    water.xyz   --temp 373  --steps 20000  --output out_nvt
.\build\atomistic-sim.exe md-nvt    protein.xyz --temp 310  --steps 100000

# Conformer ensemble generation and clustering
.\build\atomistic-sim.exe conformers  ethane.xyz  --output ethane_confs
.\build\atomistic-sim.exe conformers  hexene.xyz  --output hexene_confs

# Adaptive sampling with convergence detection
.\build\atomistic-sim.exe adaptive  nacl.xyz  --temp 500  --output out_adaptive

# Property prediction from VSEPR topology
.\build\atomistic-sim.exe predict  water.xyz
.\build\atomistic-sim.exe predict  methane.xyz
.\build\atomistic-sim.exe predict  benzene.xyz

# Reaction energy and barrier estimation
.\build\atomistic-sim.exe reaction  reactant_A.xyz  reactant_B.xyz
.\build\atomistic-sim.exe reaction  h2.xyz          o2.xyz  --output out_combustion

# Merge and analyze multiple output directories
.\build\atomistic-sim.exe merge  output1\  output2\  output3\
```

---

## 9. `atomistic-align` — Kabsch Structure Alignment

```powershell
# Align target onto reference (60 animation steps by default)
.\build\atomistic-align.exe  reference.xyz  target.xyz

# Specify number of animation steps
.\build\atomistic-align.exe  protein_ref.xyz  protein_tgt.xyz  --steps 120
.\build\atomistic-align.exe  nacl_ref.xyz     nacl_run.xyz     --steps 60
```

---

## 10. `atomistic-relax` — Standalone FIRE Relaxer

```powershell
# Default run (LJ + Coulomb, up to 1000 FIRE steps)
.\build\atomistic-relax.exe  input.xyz

# Custom force model
.\build\atomistic-relax.exe  input.xyz  --model lj

# Custom tolerances and output paths
.\build\atomistic-relax.exe  nacl.xyz   --model lj_coulomb  --max-iter 2000  --force-tol 0.005  --output nacl_relaxed.xyza  --report nacl_report.md

# Soft LJ parameters
.\build\atomistic-relax.exe  argon.xyz  --epsilon 0.05  --sigma 3.4  --output ar_relaxed.xyza
```

---

## 11. `atomistic-discover` — Reaction Discovery

```powershell
# Systematic reaction discovery loop
.\build\atomistic-discover.exe discover
.\build\atomistic-discover.exe discover  --molecules 200  --batches 20  --min-atoms 4  --max-atoms 15  --min-score 0.6  --output discovery_output

# Test all reaction templates on a molecule pair
.\build\atomistic-discover.exe test  reactant_A.xyz  reactant_B.xyz
.\build\atomistic-discover.exe test  h2.xyz  o2.xyz  --output out_h2_o2_test

# Mine patterns from an existing reaction database
.\build\atomistic-discover.exe analyze  reactions.csv
.\build\atomistic-discover.exe analyze  reactions.csv  --min-support 0.05

# Generate random test molecules
.\build\atomistic-discover.exe generate  100
.\build\atomistic-discover.exe generate  500  --output random_mols
```

---

## 12. `report_generator` — Autonomous Report Engine

```powershell
# Default run (1000 reports, seed 42)
.\build\report_generator.exe

# Custom count and seed
.\build\report_generator.exe  --count 500  --seed 7  --out reports\run_500

# Skip file writing (summary CSV only)
.\build\report_generator.exe  --count 200  --no-files

# Skip CSV (Markdown files only)
.\build\report_generator.exe  --count 100  --no-csv

# Quiet mode
.\build\report_generator.exe  --count 1000  --quiet  --interval 100

# Custom escalation thresholds
.\build\report_generator.exe  --count 2000  --l2 30  --l3 100  --l4 300  --l5 600
```

---

## 13. `nuclear-core-runner` — Z=94 (Pu-239) Report Runner

```powershell
# Default run (15 minutes, seed 94239)
.\build\nuclear-core-runner.exe

# Custom duration and output
.\build\nuclear-core-runner.exe  --minutes 5   --out reports\nuclear_quick
.\build\nuclear-core-runner.exe  --minutes 30  --out reports\nuclear_long  --seed 239
.\build\nuclear-core-runner.exe  --minutes 60  --quiet

# Short smoke test
.\build\nuclear-core-runner.exe  --minutes 1
```

---

## 14. `dynx` Commands (via vsepr CLI)

> **Note:** `dynx inspect` / `dynx validate` are implemented in `src/cli/cmd_dynx.cpp`
> but are not yet listed in `vsepr --help`. Use `vsepr-entry.exe` or the commands
> below if the routing is wired in your build; otherwise use `dynx_inspect()` /
> `dynx_validate()` directly from C++ (see `include/vsim/io/dynx_writer.hpp`).

```powershell
# Inspect a .dynx session archive
.\build\vsepr.exe dynx inspect  out\argon_gas_demo\argon_gas_demo.dynx

# Validate archive integrity
.\build\vsepr.exe dynx validate out\argon_gas_demo\argon_gas_demo.dynx

# Smoke-check the DynxEmitter tests directly
ctest --test-dir build -R "DynxEmitter" --output-on-failure
```

---

## 15. Batch Manifest Runner

```powershell
# Run with a manifest JSON (batch_runner.exe)
.\build\batch_runner.exe  batch_manifest.json

# With --help for usage
.\build\batch_runner.exe  --help

# ctest integration
ctest --test-dir build -R "BatchManifest" --output-on-failure
```

Example `batch_manifest.json`:
```json
{
  "batch_id":    "ARGON_SWEEP_001",
  "description": "Argon temperature sweep 300-900 K",
  "base_vsim":   "scripts/demos/argon_gas.vsim",
  "sweep": [
	{ "param": "temperature_K", "values": ["300", "600", "900"] },
	{ "param": "count",         "values": ["32", "64"] }
  ],
  "seeds":               2,
  "score_by":            "composite",
  "output_root":         "runs",
  "abort_on_fail":       false,
  "write_per_run_meta":    true,
  "write_per_run_metrics": true
}
```

---

## 16. Inline `.vsim` Script Quick-Start Examples

### Minimal H atom spin (from copilot-instructions.md)

```vsim
[project]
name    = "h_atom_spin"
version = "v5.0.0"

[material]
formula   = "H"
prototype = "noble_gas"
phase     = "gas"

[run]
mode      = "md"
max_steps = 1
dt_fs     = 1.0
converge  = false

[[simulation.molecule]]
formula     = "H"
count       = 1
temperature = 0.0
lattice     = "none"

[export]
write_xyz  = true
output_dir = "out/h_atom"

[visual]
output_type       = "gl_live_60fps"
gl_spin           = true
gl_spin_axis      = "y"
gl_spin_deg_per_s = 45.0
gl_show_axes      = true
gl_window_width   = 960
gl_window_height  = 720
```

### NaCl rocksalt with Ewald PBC (intent-driven, Level 0)

```vsim
[project]
name      = "nacl_relax"
version   = "v5.0.0"
seed_base = 2001

[material]
formula   = "NaCl"
structure = "rocksalt"
cell      = "4x4x4"

[cell]
lx = 22.44
ly = 22.44
lz = 22.44

[boundary]
x = "periodic"
y = "periodic"
z = "periodic"

[pbc]
minimum_image  = true
wrap_positions = "after_step"

[simulation]
periodic     = true
use_ewald    = true
ewald_alpha  = 0.3
ewald_rcut   = 10.0
ewald_kmax   = 5
fire_max_steps = 400

[run]
mode      = "relax"
max_steps = 400
converge  = true

[observe]
metrics       = ["energy.total", "rms_force", "coordination"]
every_n_steps = 10

[export]
write_xyz           = true
write_analysis_json = true
write_report_md     = true
output_dir          = "out/nacl_relax"

[visual]
output_type      = "terminal_chart"
show_proxy_table = true
```

### FCC Nickel crystal (objects API — beta-9 style)

```vsim
[project]
name    = "ni_fcc_relax"
version = "v5.0.0"

[objects]
system.crystal = CrystalModule(lattice = fcc, a = 3.52, species = [Ni])

[run]
mode      = "relax"
max_steps = 600

[export]
write_xyz       = true
write_report_md = true
output_dir      = "out/ni_fcc"

[visual]
output_type = "terminal_chart"
```

### Peptide + gas environment (organic domain)

```vsim
[project]
name      = "peptide_acdefg"
version   = "v5.0.0"
seed_base = 8001

[chemistry]
domain   = "peptide"
sequence = "ACDEFG"

[[simulation.molecule]]
formula = "N2"
count   = 12

[[simulation.molecule]]
formula = "H2O"
count   = 50

[simulation]
box_size_ang     = 40.0
periodic         = false
fire_max_steps   = 300

[run]
mode      = "relax"
max_steps = 300
converge  = true

[observe]
metrics       = ["energy.total", "rms_force", "reaction_events"]
every_n_steps = 10

[export]
write_xyz           = true
write_analysis_json = true
write_report_md     = true
output_dir          = "out/peptide_acdefg"
```

### Ambient oxidation chemistry

```vsim
[project]
name      = "fe_oxidation"
version   = "v5.0.0"
seed_base = 5001

[material]
formula   = "Fe"
structure = "bcc"
cell      = "3x3x3"

[environment]
temperature = 800.0
medium      = "vacuum"

[chemistry]
chemistry              = "oxidation"
heat                   = -1
reaction_events        = true
track_species_state    = true
min_score_threshold    = 0.30
max_reactions_per_step = 4

[simulation]
fire_max_steps = 400

[run]
mode      = "relax"
max_steps = 400
converge  = true

[observe]
metrics       = ["energy.total", "reaction_events", "exothermic_count", "avg_delta_E"]
every_n_steps = 10

[export]
write_xyz           = true
write_events_json   = true
write_analysis_json = true
write_report_md     = true
output_dir          = "out/fe_oxidation"
```

### Isomer analysis pass

```vsim
[project]
name    = "isomer_scan_c4h8"
version = "v5.0.0"

[analysis.isomers]
enabled             = true
mode                = "graph_geometry"
formula_guard       = true
graph_validation    = true
valence_check       = true
connectivity_check  = true
canonical_hash      = true
geometry_rmsd       = true
relaxation_check    = true
known_database_check = false

[[simulation.molecule]]
formula = "C4H8"

[export]
write_analysis_json = true
output_dir          = "out/isomer_c4h8"
```

### While-loop convergence guard

```vsim
[project]
name      = "graphite_while"
version   = "v5.0.0"
seed_base = 3008

[[simulation.molecule]]
formula    = "C"
count      = 480
temperature = 300.0
lattice    = "hexagonal"
layer_mode = "AB"
n_layers   = 5

[simulation]
fire_max_steps = 600
fire_dt_fs     = 1.0

[run]
mode      = "relax"
max_steps = 600
converge  = true

[while]
name         = "energy_settled"
condition    = "variance(energy.total, last 50) < 0.05"
body_steps   = 150
max_iters    = 3
measure      = ["energy.total"]
iter_delay_ms = 200

[observe]
metrics       = ["energy.total", "rms_force", "coordination"]
every_n_steps = 1

[export]
write_xyz     = true
write_report_md = true
output_dir    = "out/graphite_while"
```

### Batch temperature sweep (inline [batch])

```vsim
[project]
name      = "argon_batch_sweep"
version   = "v5.0.0"

[batch]
print_plan    = true
abort_on_fail = false

[[batch.job]]
name       = "argon_temp_sweep"
seed_count = 2
per_run_actions = ["analyze.variance", "export"]

[batch.job.sweep]
temperature = "300 500 700 900"
count       = "32 64"
```

---

## 17. Useful ctest Labels

```powershell
# WO-specific label runs
ctest --test-dir build -L "wo-72b"       --output-on-failure   # DynxEmitter
ctest --test-dir build -L "wo-72e"       --output-on-failure   # DubFactory
ctest --test-dir build -L "wo-vsim-ctl-test" --output-on-failure

# By group number
ctest --test-dir build -R "Group 70"     --output-on-failure   # XBundle
ctest --test-dir build -R "Group 71"     --output-on-failure   # DynxEmitter
ctest --test-dir build -R "Group 77"     --output-on-failure   # DubFactory

# All CTL integration groups
ctest --test-dir build -R "CtlEndToEnd|CtlRuntime|CtlNegative|CtlDeterminism|CtlWorkflow" --output-on-failure

# Full regression
ctest --test-dir build --output-on-failure

# Outcome filter: only failed tests
ctest --test-dir build --output-on-failure -R "" 2>&1 | Select-String "FAILED"
```

---

## 18. Deployment Helpers

```powershell
# Monitor / window sizing helper (live on 1920x1080)
.\deploy\windows\monitor_size_helper.ps1

# Double-click launcher bat (works standalone without Qt)
.\vsim_double_click_launcher.bat  scripts\demos\water.vsim

# Register view filetypes in Windows Explorer
.\tools\register_view_filetypes.ps1

# Launch viewer from PowerShell
.\tools\launch_viewer.ps1  out\argon_gas_demo\argon_gas_demo.xyzFull
```

---

## 19. Python / Analysis Tools

```powershell
# Bond graph web viewer (opens /graph in browser on port 8899)
python tools\viz_web.py

# Bead dynamics report pipeline
python tools\bead_dynamics_report_pipeline.py

# Heat map visualizer (receives gas2 heatmap stdout)
.\build\vsepr.exe gas2 heatmap Ar -T 300 | python tools\gas2_heatmap.py

# Gas2 steady-state monitor viewer
python tools\gas2_monitor.py

# 3D peptide visualization
python tools\viz_peptide_3d.py

# Crystal card popup
python tools\crystal_card_popup.py

# VSEPR xyz popup viewer
python tools\vsepr_xyz_popup.pyw  out\water_demo\water_demo.xyz
```

---

*Last updated: Day 75 (v5.13.5). Resume checklist: see `CLOSEOUT.md`.*
