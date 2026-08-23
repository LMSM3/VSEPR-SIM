# VSIM Scripting Language — Examples
<!-- VSIM_EXAMPLES.md  |  VSEPR-SIM v5.14.1  |  companion: VSIM_EXAMPLES.tex -->

> **How to read this document**  
> Each example is self-contained.  
> Every script block is valid VSIM that can be saved to a `.vsim` file and run with  
> `vsper run <script>` or validated with `vsper validate <script>`.  
> Jump directly to the section you need — no serial reading required.

---

## Contents

| # | Title | Concepts covered |
|---|---|---|
| [E01](#e01--hello-atom) | Hello Atom | Absolute minimum · static GL spin viewer |
| [E02](#e02--h₂o-molecule-relax) | H₂O Molecule Relax | FIRE solver · terminal chart · per-atom override |
| [E03](#e03--nacl-crystal--level-0-intent) | NaCl Crystal — Level-0 Intent | Registry resolver · PBC · research\_report profile |
| [E04](#e04--silicon-diamond-with-rdf) | Silicon Diamond with RDF | Covalent prototype · Tersoff · `[observe]` · `[analysis.structure]` |
| [E05](#e05--argon-gas-nvt) | Argon Gas NVT | Thermostat · MSD · `[[simulation.molecule]]` |
| [E06](#e06--tio₂-oxidation-with-while-loop) | TiO₂ Oxidation + While Loop | `[chemistry]` · `[variance]` · `[while]` convergence guard |
| [E07](#e07--batch-temperature-sweep) | Batch Temperature Sweep | `[batch]` · parameter expansion · aggregated report |
| [E08](#e08--gl-visualizer-controls) | GL Visualizer Controls | `gl_interactive` · spin · orbit · overlay cycle |
| [E09](#e09--ikk-identity-vector) | IKK Identity Vector | `[analysis.ivec]` · `.identity.json` · WO-75B |
| [E10](#e10--mcf-cai-object-state-grid) | MCF-CAI Object State Grid | `[object.<layer>.<basis>]` · sidecar doctrine · WO-76 |
| [E11](#e11--full-research-pipeline) | Full Research Pipeline | All major sections combined end-to-end |
| [E12](#e12--minimal-rectangular-surface-probe) | Minimal Rectangular Surface Probe | `[[object.surface]]` · crossing counter · analysis-only doctrine |
| [E13](#e13--particle-number-flux-phi) | Particle Number Flux (Φ) | `compute_flux` · Phi metric · `.surface.json` sidecar |
| [E14](#e14--time-averaged-flux-statistics) | Time-Averaged Flux Statistics | Temporal sampling · rolling variance · quasi-steady-state |
| [E15](#e15--species-filtered-flux) | Species-Filtered Flux | `species_filter` · selective-permeability model · dual probes |
| [E16](#e16--kinetic-energy-flux-q) | Kinetic Energy Flux (Q) | `geometry = "disk"` · `compute_energy_flux` · thermal plane |
| [E17](#e17--multi-metric-single-probe) | Multi-Metric Single Probe | `mass_flux + energy_flux` · combined metrics · analysis locality |
| [E18](#e18--momentum-flux--pressure-proxy-p) | Momentum Flux / Pressure Proxy (P) | `compute_momentum_flux` · slab boundaries · pressure gradient |
| [E19](#e19--per-species-flux-probes) | Per-Species Flux Probes | Three co-located surfaces · species_flux · output_tag keying |
| [E20](#e20--identity-vector-ivec-flux) | Identity-Vector (IVec) Flux | `compute_ivec_flux` · WO-75B coupling · crystal boundary |
| [E21](#e21--spherical-surface-geometry) | Spherical Surface Geometry | `geometry = "sphere"` · droplet/bubble interface · closed shell |
| [E22](#e22--multi-probe-flux-pipeline) | Multi-Probe Flux Pipeline | Φ + Q + P in one script · probe ordering · combined sidecar |
| [E23](#e23--batch-parameter-sweep-with-surface-probes) | Batch Parameter Sweep + Surfaces | `[batch]` integration · flux-vs-temperature · cross-module coupling |
| [E24](#e24--while-loop-convergence-on-flux-stationarity) | While-Loop Convergence on Flux | `[while]` criteria · flux variance · iterative stationarity |
| [E25](#e25--surface-flux--rdf-correlation-analysis) | Surface Flux + RDF Correlation | `[analysis.structure]` coupling · RDF + flux · interface study |
| [E26](#e26--surface-flux-verification-with-verifystructure) | Surface Flux Verification | `[verify.structure]` · mass conservation · analysis→verify pipeline |
| [E27](#e27--full-surface-analysis-showcase) | Full Surface-Analysis Showcase | All surface metrics · five probes · capstone integration test |
| [E28](#e28--full-export-suite-all-formats--surface) | Full Export Suite | All formats + surface · `.xyz`, `.xyzFull`, `.rdf`, `.identity`, `.surface` |
| [E29](#e29--surface-error-analysis-and-diagnostic-patterns) | Surface Error Analysis | Validation warnings · parser robustness · diagnostic patterns |
| [E30](#e30--dual-seed-audit-and-dynx-intent) | Dual-Seed Audit | `[seed]` · Seed256 · PBC · Dynx/demo provenance |
| [E31](#e31--multiscale-identity-gate) | Multiscale Identity Gate | field projection · RVE · IKK end tag · I-vector sidecar |
| [E32](#e32--static-isomer-analysis) | Static Isomer Analysis | graph validity · canonical hash · RMSD · isomer reports |
| [E33](#e33--isomer-generation-and-tracking-probe) | Isomer Generation + Tracking Probe | parser contract · generator gap · trajectory-walker gap |
| [E34](#e34--chemplus-dissolution-overlay) | ChemPlus Dissolution Overlay | reaction classification · protonation ladder · overlay doctrine |
| [E35](#e35--external-verification-contract) | External Verification Contract | LAMMPS comparison · thresholds · output contract |
| [QR](#qr--quick-reference-card) | Quick-Reference Card | One-page field cheatsheet |

---

## E01 — Hello Atom

**Goal:** the absolute minimum script that opens a GL window with a spinning hydrogen atom.  
**Key concepts:** `[project]` · `[material]` · `[run]` · `[visual]` · `gl_spin`

```toml
# e01_hello_atom.vsim
# ─────────────────────────────────────────────────────────────────────────────
# Minimum viable VSIM script.
# One hydrogen atom, one MD step (no dynamics), spinning in a GL window.
# ─────────────────────────────────────────────────────────────────────────────

[project]
name    = "e01_hello_atom"
version = "v5.14.1"

[material]
formula   = "H"
prototype = "noble_gas"   # nearest single-atom archetype
phase     = "gas"

[run]
mode      = "md"
max_steps = 1
dt_fs     = 1.0
converge  = false          # no convergence needed; this is a display run

[[simulation.molecule]]
formula     = "H"
count       = 1
temperature = 0.0
lattice     = "none"

[export]
write_xyz  = true
output_dir = "out/e01"

[visual]
output_type       = "gl_live_60fps"
gl_spin           = true
gl_spin_axis      = "y"
gl_spin_deg_per_s = 45.0
gl_show_axes      = true
gl_window_width   = 960
gl_window_height  = 720
```

### What you should see

```
┌─────────────────────────────────────────────────┐
│  VSEPR-SIM  gl_live_60fps   e01_hello_atom       │
│                                                  │
│              ●   H                               │
│           (spinning around Y axis, 45°/s)        │
│                                                  │
│  X ──►   Y ──►   Z ──►   (axes shown)           │
└─────────────────────────────────────────────────┘
```

### Key fields

| Field | Value used | Effect |
|---|---|---|
| `mode` | `"md"` + `max_steps = 1` | One-step run; no integration |
| `converge` | `false` | Never exits on convergence |
| `output_type` | `"gl_live_60fps"` | Smooth 60 fps OpenGL window |
| `gl_spin` | `true` | Enables continuous viewer rotation |
| `gl_spin_deg_per_s` | `45.0` | Rotation speed — try `180.0` for fast spin |

> **Variation:** change `gl_spin_axis` to `"x"` or `"z"`, or set `gl_spin_deg_per_s = -30.0` for reverse spin.

---

## E02 — H₂O Molecule Relax

**Goal:** relax a water molecule to its energy minimum and display live convergence in the terminal.  
**Key concepts:** FIRE solver · `terminal_chart` · `[[override.particle]]` · export flags

```toml
# e02_h2o_relax.vsim
# ─────────────────────────────────────────────────────────────────────────────
# Single H2O molecule — structural relaxation with FIRE.
# Displays live convergence trace in the terminal.
# ─────────────────────────────────────────────────────────────────────────────

[project]
name        = "e02_h2o_relax"
version     = "v5.14.1"
seed_base   = 42
determinism = true

[material]
formula   = "H2O"
structure = "water_molecule"   # registry alias -> SPC/E geometry
phase     = "gas"

[run]
mode         = "relax"
max_steps    = 800
converge     = true
output_level = "verbose"

# FIRE solver parameters (optional — registry fills defaults)
[simulation]
fire_max_steps = 800
fire_dt_fs     = 0.5

[export]
write_xyz           = true
write_analysis_json = true
write_report_md     = true
output_dir          = "out/e02"

[visual]
output_type            = "terminal_chart"
render_interval        = 25
show_convergence_trace = true
show_proxy_table       = true
```

### Terminal output snapshot

```
  VSEPR-SIM  relax  e02_h2o_relax                    step 247 / 800
  ─────────────────────────────────────────────────────────────────
  E_total   -219.341 eV   ▼  converging
  F_max       0.0031 eV/Å ▼
  RMSD        0.0008 Å
  ─────────────────────────────────────────────────────────────────
  Energy  ╔═════════════════════════════════════════╗
		  ║▓▓▓▓▓▓▓▓▓░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░║  -219.3
		  ╚═════════════════════════════════════════╝
  ─────────────────────────────────────────────────────────────────
  CONVERGED at step 247   F_max = 0.0028 eV/Å
```

### Key fields

| Field | Effect |
|---|---|
| `converge = true` | Run stops as soon as force criterion is met |
| `output_type = "terminal_chart"` | Live ASCII energy + force plot in terminal |
| `show_convergence_trace` | Draws the rolling convergence sparkline |
| `show_proxy_table` | Shows E_total, F_max, RMSD per render interval |

### Add a per-atom override

To freeze the oxygen and let only the hydrogens relax, append:

```toml
[[override.particle]]
element = "O"
fixed   = true
```

---

## E03 — NaCl Crystal — Level-0 Intent

**Goal:** run a full PBC crystal relaxation with the minimum possible authoring — let the registry fill in the details.  
**Key concepts:** level-0 `structure` alias · `RegistryResolver` · PBC · `research_report` export profile

```toml
# e03_nacl_crystal.vsim
# ─────────────────────────────────────────────────────────────────────────────
# NaCl (rock-salt) crystal — level-0 intent.
# You name the material; the registry resolves prototype, solver,
# forcefield, export profile, and observables automatically.
# ─────────────────────────────────────────────────────────────────────────────

[project]
name        = "e03_nacl_crystal"
version     = "v5.14.1"
seed_base   = 3001
determinism = true
description = "NaCl 3x3x3 supercell — Ewald relaxation, research_report output"

# ── Level-0 authoring ──────────────────────────────────────────────────────
# "rocksalt" alias is resolved to prototype "B1_NaCl" by RegistryResolver.
# Registry then fills:  solver=fire, forcefield=ewald_formal,
#                       export_profile=research_report, observables=ionic_set
[material]
formula   = "NaCl"
structure = "rocksalt"
cell      = "3x3x3"
phase     = "solid"

[run]
mode         = "relax"
max_steps    = 1200
converge     = true
output_level = "verbose"

# ── Simulation box ─────────────────────────────────────────────────────────
[cell]
type = "orthorhombic"
lx   = 16.86    # 3 × a_NaCl (5.62 Å)
ly   = 16.86
lz   = 16.86

[boundary]
x = "periodic"
y = "periodic"
z = "periodic"

[pbc]
minimum_image        = true
wrap_positions       = "after_step"
track_images         = false
unwrap_for_diffusion = false

# ── Ewald summation ────────────────────────────────────────────────────────
[simulation]
periodic         = true
use_ewald        = true
ewald_alpha      = 0.3
ewald_rcut       = 8.0
ewald_kmax       = 5
formation_preset = "ionic"

# ── Output ─────────────────────────────────────────────────────────────────
# "research_report" is a named profile that sets:
#   write_xyz, write_analysis_json, write_metrics_tsv, write_report_md,
#   write_events_json, write_manifest_json, write_dashboard_svg
[export]
export_profile = "research_report"
output_dir     = "out/e03"

[visual]
output_type              = "terminal_overlay_cycle"
render_interval          = 50
show_proxy_table         = true
show_convergence_trace   = true
show_rdf_plot            = true
show_steady_state_marker = true
```

### Registry resolution log (example)

```
[REGISTRY] material.prototype    <- "B1_NaCl"
[REGISTRY] material.space_group  <- "Fm-3m"
[REGISTRY] run.solver            <- "fire"
[REGISTRY] run.forcefield        <- "ewald_formal"
[REGISTRY] export.profile        <- "research_report"
[REGISTRY] observe.metrics       <- "energy_map,coordination,rdf,defect"
```

### Key fields

| Field | Effect |
|---|---|
| `structure = "rocksalt"` | Level-0: registry resolves prototype automatically |
| `cell = "3x3x3"` | Supercell expansion factor |
| `export_profile = "research_report"` | Named export profile — sets 7 output flags at once |
| `output_type = "terminal_overlay_cycle"` | Cycles through RDF / energy / coordination panels |

> **Progression:** replace `structure = "rocksalt"` with `prototype = "B1_NaCl"` for level-1 deterministic authoring.

---

## E04 — Silicon Diamond with RDF

**Goal:** relax a silicon diamond-cubic supercell, compute RDF, and visualise the analysis overlay.  
**Key concepts:** covalent prototype · Tersoff forcefield · `[observe]` · `[analysis.structure]`

```toml
# e04_silicon_diamond.vsim
# ─────────────────────────────────────────────────────────────────────────────
# Si diamond-cubic 2x2x2 supercell.
# Tersoff potential | RDF | coordination overlay
# ─────────────────────────────────────────────────────────────────────────────

[project]
name        = "e04_silicon_diamond"
version     = "v5.14.1"
seed_base   = 4001
determinism = true

[material]
formula   = "Si"
prototype = "A4_Si"        # diamond cubic; registry fills Tersoff + Fd-3m
cell      = "2x2x2"
phase     = "solid"

[run]
mode         = "relax"
max_steps    = 1000
converge     = true
output_level = "normal"

[cell]
type = "orthorhombic"
lx   = 10.86    # 2 × a_Si (5.43 Å)
ly   = 10.86
lz   = 10.86

[boundary]
x = "periodic"
y = "periodic"
z = "periodic"

[pbc]
minimum_image  = true
wrap_positions = "after_step"

# ── Structural analysis ────────────────────────────────────────────────────
[analysis.structure]
enabled            = true
compute_rdf        = true
rdf_dr             = 0.05
rdf_rmax           = 5.0
compute_coordination = true
coordination_cutoff  = 2.8

[observe]
metrics       = ["rdf", "coordination", "energy_map", "formation"]
output_format = "json"
every_n_steps = 100

# ── Export ─────────────────────────────────────────────────────────────────
[export]
write_xyz           = true
write_analysis_json = true
write_report_md     = true
output_dir          = "out/e04"

[export.visual]
write_rdf_svg = true
visual_output_dir = "figures/e04"

[visual]
output_type            = "terminal_chart"
render_interval        = 50
show_convergence_trace = true
show_rdf_plot          = true
show_proxy_table       = true
```

### Expected RDF peaks for Si diamond

```
g(r)
 │       ▄
 │      █▀█
 │     ░   ░▄
 │    ░      ░▄
 │___░__________░___▄____
 0   2.35Å  3.84Å  4.50Å  r
		 1NN  2NN   3NN
```

### Key fields

| Field | Effect |
|---|---|
| `prototype = "A4_Si"` | Registry resolves Tersoff FF + Fd-3m space group |
| `compute_rdf = true` | Enables radial distribution function computation |
| `rdf_dr = 0.05` | Bin width in Å — smaller = finer but slower |
| `write_rdf_svg` | Outputs RDF as an SVG figure to `figures/e04/` |

---

## E05 — Argon Gas NVT

**Goal:** simulate an Ar₆₄ gas ensemble with a Nosé-Hoover thermostat and track MSD and diffusion.  
**Key concepts:** `[[simulation.molecule]]` multi-molecule block · NVT MD · MSD · `[observe]`

```toml
# e05_argon_gas_nvt.vsim
# ─────────────────────────────────────────────────────────────────────────────
# 64 argon atoms — NVT molecular dynamics at 300 K.
# Computes MSD, self-diffusion coefficient, and radial distribution function.
# ─────────────────────────────────────────────────────────────────────────────

[project]
name        = "e05_argon_gas"
version     = "v5.14.1"
seed_base   = 5001
determinism = true

[run]
mode          = "md"
max_steps     = 5000
dt_fs         = 2.0
temperature_K = 300.0
converge      = false
output_level  = "normal"

[[simulation.molecule]]
formula     = "Ar"
count       = 64
temperature = 300.0
lattice     = "random_pack"

[environment]
temperature = 300.0
pressure    = 0.0
medium      = "vacuum"

[cell]
type = "orthorhombic"
lx   = 20.0
ly   = 20.0
lz   = 20.0

[boundary]
x = "periodic"
y = "periodic"
z = "periodic"

[pbc]
minimum_image        = true
wrap_positions       = "after_step"
track_images         = true
unwrap_for_diffusion = true   # needed for correct MSD

[observe]
metrics       = ["msd", "rdf", "diffusion_coefficient", "temperature", "energy_map"]
output_format = "json"
every_n_steps = 50

[variance]
energy_var = "energy.total  last 200  0.02"

[export]
write_xyz           = true
write_xyzf          = true
write_analysis_json = true
write_metrics_tsv   = true
output_dir          = "out/e05"

[visual]
output_type            = "terminal_chart"
animation_mode         = "spark"
render_interval        = 100
show_convergence_trace = false
show_proxy_table       = true
show_event_timeline    = false
```

### MSD output snapshot

```
  MSD (Å²) vs time (fs)
  ┌────────────────────────────────┐
  │                          ╱     │  slope ≈ 6D (3D)
  │                     ╱         │
  │                ╱              │
  │           ╱                   │
  │      ╱                        │
  │ ╱                             │
  └────────────────────────────────┘
  0                          10000 fs
  D_self ~ 2.4 × 10⁻⁹ m²/s  (300 K, LJ Ar)
```

### Key fields

| Field | Effect |
|---|---|
| `count = 64` | 64 argon atoms in the `[[simulation.molecule]]` block |
| `lattice = "random_pack"` | Initial positions generated by random packing |
| `unwrap_for_diffusion = true` | PBC images unwrapped so MSD is not truncated |
| `track_images = true` | Required with `unwrap_for_diffusion` |
| `"diffusion_coefficient"` in metrics | Computes D from linear MSD slope |

---

## E06 — TiO₂ Oxidation + While Loop

**Goal:** run ambient oxidation MD on a TiO₂ rutile supercell and continue automatically until energy variance settles.  
**Key concepts:** `[environment]` · `[chemistry]` · `[variance]` · `[while]` convergence guard

```toml
# e06_tio2_oxidation.vsim
# ─────────────────────────────────────────────────────────────────────────────
# TiO2 rutile 2x2x2 supercell — ambient oxidation MD at 600 K.
# The [while] loop re-runs blocks of 200 steps until energy variance
# falls below 0.05 eV or 10 iterations are exhausted.
# ─────────────────────────────────────────────────────────────────────────────

[project]
name        = "e06_tio2_oxidation"
version     = "v5.14.1"
seed_base   = 6001
determinism = true

[material]
formula   = "TiO2"
structure = "rutile"           # registry: C4_TiO2, Tersoff, research_report
cell      = "2x2x2"
phase     = "solid"

[run]
mode          = "md"
max_steps     = 2000
dt_fs         = 0.5
temperature_K = 600.0
converge      = false
output_level  = "verbose"

[cell]
type = "orthorhombic"
lx   = 9.186    # 2 × 4.593 Å
ly   = 9.186
lz   = 5.918    # 2 × 2.959 Å

[boundary]
x = "periodic"
y = "periodic"
z = "periodic"

[pbc]
minimum_image        = true
wrap_positions       = "after_step"
track_images         = true
unwrap_for_diffusion = true

# ── Physical environment ───────────────────────────────────────────────────
[environment]
temperature = 600.0
pressure    = 0.0
medium      = "argon_gas"

# ── Reaction chemistry ─────────────────────────────────────────────────────
# heat = -1 means: auto-derive from environment.temperature
[chemistry]
chemistry              = "oxidation"
heat                   = -1
reaction_events        = true
track_species_state    = true
min_score_threshold    = 0.25
max_reactions_per_step = 4

# ── Observables ────────────────────────────────────────────────────────────
[observe]
metrics       = ["reaction_events", "exothermic_count", "avg_delta_E",
				 "formation", "transport", "defect"]
output_format = "json"
every_n_steps = 50

# ── Variance probe ─────────────────────────────────────────────────────────
# Defines the guard variable used by [while] below.
[variance]
energy_var = "energy.total  last 100  0.05"

# ── While loop ─────────────────────────────────────────────────────────────
# Runs 200-step blocks until energy_var ≤ 0.05 OR 10 iterations done.
[while]
name          = "settle_energy"
condition     = "variance energy_var > 0.05"
body_steps    = 200
max_iters     = 10
measure       = ["energy_var"]
iter_delay_ms = 150

# ── Export ─────────────────────────────────────────────────────────────────
[export]
write_xyz                  = true
write_xyzf                 = true
write_analysis_json        = true
write_events_json          = true
write_report_md            = true
write_dashboard_svg        = true
write_manifest_json        = true
write_pipeline_audit_jsonl = true
output_dir                 = "out/e06"

[export.visual]
write_rdf_svg             = true
write_energy_trace_svg    = true
write_packing_heatmap_svg = true
visual_output_dir         = "figures/e06"

[visual]
output_type              = "terminal_chart"
animation_mode           = "spark"
render_interval          = 50
show_proxy_table         = true
show_convergence_trace   = true
show_event_timeline      = true
show_rdf_plot            = true

[report]
title                  = "TiO2 Rutile — Ambient Oxidation at 600 K"
include_material_cards = true
include_metric_tables  = true
```

### While-loop execution trace

```
[while] settle_energy — iter 1/10   variance = 0.148  CONTINUE
[while] settle_energy — iter 2/10   variance = 0.091  CONTINUE
[while] settle_energy — iter 3/10   variance = 0.047  EXIT  ✓
```

### Key fields

| Field | Effect |
|---|---|
| `chemistry = "oxidation"` | Activates oxidation reaction rule family |
| `heat = -1` | Auto-derive reaction heat from temperature |
| `energy_var = "energy.total  last 100  0.05"` | Probe: variance of last 100 E_total readings, threshold 0.05 |
| `condition = "variance energy_var > 0.05"` | While continues while variance is above threshold |
| `max_iters = 10` | Hard cap — exits even if not converged |

---

## E07 — Batch Temperature Sweep

**Goal:** automatically run NaCl at five temperatures and aggregate the results into a single comparative report.  
**Key concepts:** `[batch]` · sweep axis · `seed_count` · `aggregate` · `[variance]` probes

```toml
# e07_batch_temperature_sweep.vsim
# ─────────────────────────────────────────────────────────────────────────────
# NaCl — NVT MD at five temperatures.
# Two batch jobs:
#   1. nacl_temp_sweep  — 300 K → 2000 K, 1 seed each, full aggregate
#   2. nacl_seed_stats  — 900 K × 5 seeds, variance statistics
# ─────────────────────────────────────────────────────────────────────────────

[project]
name        = "e07_batch_sweep"
version     = "v5.14.1"
seed_base   = 7001
determinism = true

# ── Shared material ────────────────────────────────────────────────────────
[material]
formula   = "NaCl"
prototype = "B1_NaCl"
cell      = "3x3x3"
phase     = "solid"

# ── Run defaults (overridden per sub-run by the batch engine) ──────────────
[run]
mode         = "md"
max_steps    = 1000
dt_fs        = 1.0
converge     = false
output_level = "minimal"

# ── Variance probe (evaluated per sub-run) ─────────────────────────────────
[variance]
energy_var = "energy.total  last 50  0.05"

# ── Batch definition ───────────────────────────────────────────────────────
[batch]
print_plan    = true
abort_on_fail = false

# Job 1: temperature sweep  (5 temps × 1 seed = 5 sub-runs)
job         = nacl_temp_sweep
temperature = ["300", "600", "900", "1300", "2000"]
seed_count  = 1
aggregate   = true
analyze     = variance energy.total
measure     = energy_var

# Job 2: seed statistics at 900 K  (1 temp × 5 seeds = 5 sub-runs)
job         = nacl_seed_stats
temperature = ["900"]
seed_count  = 5
aggregate   = true
analyze     = rmsd
analyze     = variance displacement
measure     = energy_var

# ── Output ─────────────────────────────────────────────────────────────────
[export]
write_analysis_json = true
write_metrics_tsv   = true
write_report_md     = true
write_manifest_json = true
output_dir          = "out/e07"

[visual]
output_type    = "terminal_chart"
animation_mode = "bar"
render_interval = 100
show_bar_chart  = true
show_proxy_table = true
```

### Batch plan print (--print_plan)

```
[BATCH] nacl_temp_sweep: 5 sub-runs
  sub-run 0  T=300 K  seed=7001
  sub-run 1  T=600 K  seed=7001
  sub-run 2  T=900 K  seed=7001
  sub-run 3  T=1300K  seed=7001
  sub-run 4  T=2000K  seed=7001
[BATCH] nacl_seed_stats: 5 sub-runs
  sub-run 0  T=900 K  seed=7001
  sub-run 1  T=900 K  seed=7002
  ...
```

### Key fields

| Field | Effect |
|---|---|
| `job = nacl_temp_sweep` | Starts a new named job entry |
| `temperature = ["300", "600", ...]` | Sweep axis — one sub-run per value |
| `seed_count = 5` | Repeats each parameter point with different seeds |
| `aggregate = true` | Merges sub-run results into a single report |
| `analyze = variance energy.total` | Post-processing action applied to each sub-run |

---

## E08 — GL Visualizer Controls

**Goal:** demonstrate all major interactive GL visualizer options on a copper FCC crystal.  
**Key concepts:** `gl_interactive` · `gl_auto_orbit` · `gl_overlay_cycle` · camera fields

```toml
# e08_gl_visualizer.vsim
# ─────────────────────────────────────────────────────────────────────────────
# Cu FCC crystal — demonstration of all GL visual controls.
# output_type = "gl_interactive" opens the full ImGui-equipped viewer.
# ─────────────────────────────────────────────────────────────────────────────

[project]
name    = "e08_gl_visualizer"
version = "v5.14.1"

[material]
formula   = "Cu"
structure = "fcc"
cell      = "3x3x3"
phase     = "solid"

[run]
mode      = "relax"
max_steps = 500
converge  = true

[export]
write_xyz  = true
output_dir = "out/e08"

# ── GL visual controls ─────────────────────────────────────────────────────
[visual]
output_type       = "gl_interactive"   # full ImGui viewer (requires BUILD_VISUALIZATION)

# Window
gl_window_width   = 1280
gl_window_height  = 800

# Orbit / spin  (mutually exclusive — gl_spin disables gl_auto_orbit)
gl_auto_orbit     = false   # set true for camera orbit between overlay panels
gl_spin           = true    # continuous scene rotation
gl_spin_axis      = "y"
gl_spin_deg_per_s = 20.0    # slow rotation — good for inspection

# Axes and rendering
gl_show_axes      = true
render_interval   = 10      # refresh every 10 steps
```

### output_type comparison table

| `output_type` | Window? | Interactive? | Best for |
|---|---|---|---|
| `"none"` | No | No | Headless batch, CI |
| `"terminal_chart"` | No | No (ASCII) | Live monitoring, SSH sessions |
| `"gl_interactive"` | Yes | Yes (ImGui) | Full 3D inspection and manual control |
| `"gl_live_60fps"` | Yes | No | Smooth presentations, demos |
| `"gl_crystal_grid"` | Yes | No | Crystal structures with unit-cell view |
| `"gl_overlay_cycle"` | Yes | No | Cycling density / coordination / energy panels |

### Camera / spin cheatsheet

```toml
# Auto-orbit between overlay panels (mutually exclusive with gl_spin)
gl_auto_orbit     = true

# Static frozen view (spin at 0°/s)
gl_spin           = true
gl_spin_deg_per_s = 0.0

# Reverse spin
gl_spin_deg_per_s = -45.0

# Large presentation window
gl_window_width   = 1920
gl_window_height  = 1080
```

---

## E09 — IKK Identity Vector

**Goal:** enable the IKK identity-vector analysis to output a `.identity.json` file tracking Ī_f across frames.  
**Key concepts:** `[analysis.ivec]` · I-vector axes · `.identity.json` schema · WO-75B Phase 1

```toml
# e09_ikk_identity_vector.vsim
# ─────────────────────────────────────────────────────────────────────────────
# NaCl crystal — identity-vector analysis enabled.
# Outputs out/e09/nacl_ikk.identity.json with per-frame Ī_f statistics.
#
# I-vector axis mapping (Phase 1 proxy — one aggregate per frame):
#   x = 1 - dataloss          (existence:  fraction of identity preserved)
#   y = 1 - hidden_channel    (EM:         complement of hidden-interaction proxy)
#   z = recoverable_info      (spatial:    recoverable information fraction)
#   t = 1 - projection_loss   (temporal:   complement of scale-crossing loss)
#   w = 1 - identity_residual (internal:   complement of prototype deviation)
# All components clamped to [0, 1].  Axis order is FIXED: (x,y,z,t,w).
# ─────────────────────────────────────────────────────────────────────────────

[project]
name    = "nacl_ikk"
version = "v5.14.1"

[material]
formula   = "NaCl"
structure = "rocksalt"
cell      = "2x2x2"
phase     = "solid"

[run]
mode      = "md"
max_steps = 500
dt_fs     = 1.0
converge  = false

[cell]
type = "orthorhombic"
lx   = 11.24
ly   = 11.24
lz   = 11.24

[boundary]
x = "periodic"
y = "periodic"
z = "periodic"

[pbc]
minimum_image  = true
wrap_positions = "after_step"

# ── IKK identity-vector analysis ───────────────────────────────────────────
[analysis.ivec]
enabled       = true
write_json    = true         # emit <run_id>.identity.json
include_delta = true         # include ΔĪ_f (frame-to-frame delta) in output
include_var   = false        # Phase 1: var is always zero; enable in Phase 4+
output_dir    = "out/e09"    # empty = use pipeline_output.output_dir

# ── Also emit the IKK end-tag to the MD report ─────────────────────────────
[analysis.ikk_end_tag]
enabled           = true
emit_markdown     = true
d_pass_threshold  = 0.65
d_warn_threshold  = 0.40
scale_regime      = "S3"
section_reference = "IV.9"

[export]
write_xyz      = true
write_report_md = true
output_dir     = "out/e09"

[visual]
output_type     = "terminal_chart"
render_interval = 50
show_proxy_table = true
```

### `.identity.json` schema (excerpt)

```json
{
  "run_id":      "nacl_ikk",
  "source_vsim": "e09_ikk_identity_vector.vsim",
  "frame_count": 500,
  "run_mean":    { "x": 0.923, "y": 0.887, "z": 0.741, "t": 0.896, "w": 0.952, "mag": 0.642 },
  "run_var_diag":{ "x": 0.001, "y": 0.003, "z": 0.005, "t": 0.002, "w": 0.001, "mag": 0.003 },
  "drift_per_frame": { "x": -0.00002, "y": 0.00001, "z": -0.00003, "t": 0.00000, "w": -0.00001, "norm": 0.000037, "valid": true },
  "frames": [
	{
	  "frame_index": 0, "time_fs": 0.0, "particle_count": 1,
	  "mean": { "x": 0.921, "y": 0.881, "z": 0.743, "t": 0.895, "w": 0.950, "mag": 0.640 },
	  "var":  { "x": 0.000, "y": 0.000, "z": 0.000, "t": 0.000, "w": 0.000, "mag": 0.000 },
	  "delta": { "x": 0.0, "y": 0.0, "z": 0.0, "t": 0.0, "w": 0.0, "norm": 0.0, "valid": false }
	},
	...
  ]
}
```

### Key fields

| Field | Effect |
|---|---|
| `enabled = true` | Must be explicitly set — section is silent otherwise |
| `include_delta = true` | Adds ΔĪ_f = Ī_{f+1} − Ī_f to every frame record |
| `include_var = false` | Phase 1: zero (single aggregate); enable in Phase 4 for per-particle spread |
| `output_dir` | Override output directory; empty = uses `pipeline_output.output_dir` |

> **Doctrine:** `[analysis.ivec]` is derived data only.  
> It reads from `IdentitySidecarSeries` and never writes back to `.xyz` or `.xyzFull`.

---

## E10 — MCF-CAI Object State Grid

**Goal:** populate the 3×3 MCF-CAI object state grid for a carbon diamond bead.  
**Key concepts:** `[object.<layer>.<basis>]` · Information-column doctrine · WO-76

```toml
# e10_mcf_cai.vsim
# ─────────────────────────────────────────────────────────────────────────────
# Carbon diamond — MCF-CAI object state grid demonstration.
#
# Grid layout:
#   rows   = Macro | Chemical | Fundamental
#   columns= Carrier | Action | Information (sidecar-only)
#
# DOCTRINE: Information-column cells are sidecar/audit data only.
#           They must NEVER be used as force inputs or written to truth-state.
# ─────────────────────────────────────────────────────────────────────────────

[project]
name    = "e10_mcf_cai"
version = "v5.14.1"

[material]
formula   = "C"
prototype = "A4_diamond"
cell      = "1x1x1"
phase     = "solid"

[run]
mode      = "relax"
max_steps = 400
converge  = true

[export]
write_xyz  = true
output_dir = "out/e10"

[visual]
output_type     = "terminal_chart"
render_interval = 50

# ══════════════════════════════════════════════════════════════════════════════
# MCF-CAI GRID
# ══════════════════════════════════════════════════════════════════════════════

# ── MACRO row ─────────────────────────────────────────────────────────────
[object.macro.carrier]
centre_x          = 0.0
centre_y          = 0.0
centre_z          = 0.0
orientation_proxy = 0.0
phase_label       = "diamond_cubic"
grain_count       = 1

[object.macro.action]
stress_proxy      = 0.0
heat_flux_proxy   = 0.0
fracture_prob     = 0.0
deformation_proxy = 0.0

[object.macro.information]        # sidecar only — audit layer
formation_age_fs  = 0.0
defect_memory     = 0.0
cg_residual       = 0.0

# ── CHEMICAL row ──────────────────────────────────────────────────────────
[object.chemical.carrier]
atomic_number       = 6           # Carbon
mass                = 12.011
coordination_number = 4           # sp3 tetrahedral
bond_order_proxy    = 1.0

[object.chemical.action]
reaction_active = false
oxidation_state = 0
ionisation_flag = false

[object.chemical.information]     # sidecar only — audit layer
bond_entropy_loss = 0.0
d_chem            = 0.0
formation_route   = "tetrahedral_sp3"

# ── FUNDAMENTAL row ────────────────────────────────────────────────────────
[object.fundamental.carrier]
net_charge    = 0.0
spin_proxy    = 0.0
nuclear_state = 0
isotope_label = "C12"

[object.fundamental.action]
em_coupling  = 0.0
strong_proxy = 1.0
weak_proxy   = 0.0
decay_flag   = false

[object.fundamental.information]  # sidecar only — audit layer
psi_hid         = 0.0
projection_loss = 0.0
entropy_trace   = 0.0
```

### MCF-CAI grid at a glance

```
				 CARRIER              ACTION               INFORMATION
			   ┌──────────────────┬──────────────────┬──────────────────────┐
  MACRO        │ centre_x/y/z     │ stress_proxy     │ formation_age_fs     │
			   │ phase_label      │ heat_flux_proxy  │ defect_memory        │
			   │ grain_count      │ fracture_prob    │ cg_residual          │
			   ├──────────────────┼──────────────────┼──────────────────────┤
  CHEMICAL     │ atomic_number    │ reaction_active  │ bond_entropy_loss    │
			   │ mass, CN         │ oxidation_state  │ d_chem               │
			   │ bond_order_proxy │ ionisation_flag  │ formation_route      │
			   ├──────────────────┼──────────────────┼──────────────────────┤
  FUNDAMENTAL  │ net_charge       │ em_coupling      │ psi_hid              │
			   │ spin_proxy       │ strong_proxy     │ projection_loss      │
			   │ isotope_label    │ decay_flag       │ entropy_trace        │
			   └──────────────────┴──────────────────┴──────────────────────┘
													  ▲
											   sidecar-only (never force input)
```

### Key fields

| Field | Effect |
|---|---|
| `[object.macro.carrier]` | Populates `grid[MACRO][CARRIER]` in `McfCaiSection` |
| `phase_label = "diamond_cubic"` | String tag for the macro structural phase |
| `[object.chemical.information]` | Sidecar-only cell — fields never enter force kernel |
| `d_chem` | Chemical identity distance proxy (sidecar layer only) |

> **Doctrine:** sections with `basis = information` are sidecar/audit layers.  
> Their fields are stored in `doc.mcf_cai` but must never be passed to the dynamics kernel.

---

## E11 — Full Research Pipeline

**Goal:** combine every major section into one production-grade research run.  
**Key concepts:** every section working together · `publication` export profile · IKK end-tag · MCF-CAI

```toml
# e11_full_pipeline.vsim
# ─────────────────────────────────────────────────────────────────────────────
# FeO wüstite — full research pipeline demonstration.
# Covers: material, run, cell, boundary, pbc, simulation,
#         environment, chemistry, observe, variance, while,
#         analysis.structure, analysis.ikk_end_tag, analysis.ivec,
#         object.* (MCF-CAI), export, export.visual, visual, report.
# ─────────────────────────────────────────────────────────────────────────────

[project]
name        = "e11_feo_full_pipeline"
version     = "v5.14.1"
seed_base   = 11001
determinism = true
description = "FeO wüstite — full research pipeline with IKK + MCF-CAI"

# ── Material ───────────────────────────────────────────────────────────────
[material]
formula   = "FeO"
structure = "wustite"     # registry: B1_FeO, Fm-3m, ewald_formal
cell      = "2x2x2"
phase     = "solid"

# ── Run ────────────────────────────────────────────────────────────────────
[run]
mode          = "md"
max_steps     = 3000
dt_fs         = 0.5
temperature_K = 800.0
converge      = false
output_level  = "verbose"

# ── Box ────────────────────────────────────────────────────────────────────
[cell]
type = "orthorhombic"
lx   = 8.60    # 2 × a_FeO (4.30 Å)
ly   = 8.60
lz   = 8.60

[boundary]
x = "periodic"
y = "periodic"
z = "periodic"

[pbc]
minimum_image        = true
wrap_positions       = "after_step"
track_images         = true
unwrap_for_diffusion = true

# ── Dynamics ───────────────────────────────────────────────────────────────
[simulation]
periodic         = true
use_ewald        = true
ewald_alpha      = 0.3
ewald_rcut       = 7.0
ewald_kmax       = 5
formation_preset = "ionic"

# ── Environment ────────────────────────────────────────────────────────────
[environment]
temperature = 800.0
pressure    = 0.0
medium      = "argon_gas"

# ── Chemistry (high-T oxidation) ───────────────────────────────────────────
[chemistry]
chemistry              = "oxidation"
heat                   = -1
reaction_events        = true
track_species_state    = true
min_score_threshold    = 0.20
max_reactions_per_step = 6

# ── Observables ────────────────────────────────────────────────────────────
[observe]
metrics       = ["rdf", "coordination", "msd", "energy_map",
				 "reaction_events", "defect", "transport"]
output_format = "json"
every_n_steps = 100

# ── Analysis: structural ───────────────────────────────────────────────────
[analysis.structure]
enabled              = true
compute_rdf          = true
rdf_dr               = 0.05
rdf_rmax             = 6.0
compute_coordination = true
coordination_cutoff  = 2.6

# ── Analysis: IKK end-tag ──────────────────────────────────────────────────
[analysis.ikk_end_tag]
enabled           = true
emit_markdown     = true
emit_latex        = true
d_pass_threshold  = 0.65
d_warn_threshold  = 0.40
scale_regime      = "S3"
section_reference = "IV.11"

# ── Analysis: IKK identity vector ─────────────────────────────────────────
[analysis.ivec]
enabled       = true
write_json    = true
include_delta = true
include_var   = false
output_dir    = "out/e11"

# ── MCF-CAI object state ───────────────────────────────────────────────────
[object.macro.carrier]
phase_label  = "wustite_B1"
grain_count  = 8

[object.macro.information]
formation_age_fs = 0.0
defect_memory    = 0.0

[object.chemical.carrier]
atomic_number       = 26    # Fe
mass                = 55.845
coordination_number = 6
bond_order_proxy    = 0.5

[object.chemical.action]
reaction_active = true
oxidation_state = 2

[object.chemical.information]
bond_entropy_loss = 0.0
d_chem            = 0.0
formation_route   = "ionic_B1"

[object.fundamental.carrier]
net_charge    = 2.0
spin_proxy    = 2.0    # Fe²⁺ high-spin
nuclear_state = 0
isotope_label = "Fe56"

[object.fundamental.information]
psi_hid         = 0.0
projection_loss = 0.0
entropy_trace   = 0.0

# ── Convergence guard ──────────────────────────────────────────────────────
[variance]
energy_var = "energy.total  last 150  0.04"

[while]
name          = "settle_energy"
condition     = "variance energy_var > 0.04"
body_steps    = 300
max_iters     = 8
measure       = ["energy_var"]
iter_delay_ms = 200

# ── Export ─────────────────────────────────────────────────────────────────
[export]
export_profile             = "publication"
write_pipeline_audit_jsonl = true
output_dir                 = "out/e11"

[export.visual]
write_svg_figures         = true
write_rdf_svg             = true
write_energy_trace_svg    = true
write_packing_heatmap_svg = true
visual_output_dir         = "figures/e11"

# ── Terminal display ───────────────────────────────────────────────────────
[visual]
output_type              = "terminal_overlay_cycle"
animation_mode           = "spark"
render_interval          = 50
show_proxy_table         = true
show_convergence_trace   = true
show_event_timeline      = true
show_rdf_plot            = true
show_steady_state_marker = true
show_audit_table         = true

# ── Report ─────────────────────────────────────────────────────────────────
[report]
title                  = "FeO Wüstite — Full Research Pipeline"
include_material_cards = true
include_metric_tables  = true
```

### Pipeline flow diagram

```
  e11_feo_full_pipeline.vsim
  │
  ├─[project]    seed, determinism
  ├─[material]   formula="FeO" + registry resolution
  ├─[run]        NVT MD 800 K, 3000 steps
  ├─[cell]       8.6 × 8.6 × 8.6 Å box
  ├─[boundary]   periodic all axes
  ├─[pbc]        minimum image, image tracking, MSD-unwrap
  ├─[simulation] Ewald α=0.3, kmax=5
  ├─[environment]T=800 K, argon medium
  ├─[chemistry]  oxidation, auto-heat, max 6 events/step
  ├─[observe]    7 metrics, every 100 steps → JSON
  ├─[analysis.structure] RDF + coordination
  ├─[variance]   energy.total guard probe
  ├─[while]      re-run 300 steps × 8 until variance ≤ 0.04
  ├─[analysis.ikk_end_tag] D_rec, η_ab, badge → MD + LaTeX
  ├─[analysis.ivec]        Ī_f series → .identity.json
  ├─[object.*.*]           MCF-CAI 3×3 grid
  ├─[export]     publication profile → all output files
  ├─[export.visual] SVG figures
  ├─[visual]     terminal_overlay_cycle live display
  └─[report]     Markdown report with material cards + tables
```

---

## QR — Quick-Reference Card

### Mandatory fields

```toml
[project]
name    = "<name>"       # required
version = "v5.14.1"      # recommended

[material]
formula = "<formula>"    # required; everything else can be registry-filled

[run]
mode    = "<relax|md>"   # required
```

### Common `[run]` patterns

| Intent | Fields |
|---|---|
| Energy minimisation | `mode="relax"`, `max_steps=1000`, `converge=true` |
| NVT molecular dynamics | `mode="md"`, `dt_fs=1.0`, `temperature_K=300.0`, `converge=false` |
| One-step static display | `mode="md"`, `max_steps=1`, `converge=false` |

### `[visual]` output_type cheatsheet

| Value | Use case |
|---|---|
| `"none"` | Headless batch / CI |
| `"terminal_chart"` | Live ASCII convergence in any terminal |
| `"gl_interactive"` | Full 3D viewer with ImGui (requires BUILD\_VISUALIZATION) |
| `"gl_live_60fps"` | Smooth 60 fps display — demos, presentations |
| `"gl_crystal_grid"` | Crystal structures with unit-cell repeat |
| `"gl_overlay_cycle"` | Cycling density / coordination / energy panels |
| `"terminal_overlay_cycle"` | ASCII overlay cycle |

### Export profile cheatsheet

| Profile | Files emitted |
|---|---|
| `minimal` | `.xyz` |
| `standard` | `.xyz` + `.analysis.json` + `.metrics.tsv` + `.report.md` |
| `research_report` | standard + events, manifest, dashboard SVG |
| `publication` | research\_report + symbolic trace + pipeline audit JSONL |

### PBC quick setup

```toml
[cell]
type = "orthorhombic"
lx = 10.0  ly = 10.0  lz = 10.0

[boundary]
x = "periodic"  y = "periodic"  z = "periodic"

[pbc]
minimum_image  = true
wrap_positions = "after_step"
```

### GL spin / orbit cheatsheet

```toml
# Spinning scene
gl_spin = true  gl_spin_axis = "y"  gl_spin_deg_per_s = 45.0

# Camera orbit between overlay panels
gl_auto_orbit = true   # (disables gl_spin)

# Static frozen view
gl_spin = true  gl_spin_deg_per_s = 0.0
```

### Analysis section summary

| Section | Purpose | WO |
|---|---|---|
| `[analysis.structure]` | RDF, coordination number | — |
| `[analysis.sampling]` | Scalar sampling, MSD | — |
| `[analysis.scale_sampling]` | Field projection, RVE, emergence | WO-61D |
| `[analysis.inference]` | Property inference | WO-61B |
| `[analysis.ikk_end_tag]` | IKK report end-tag badge | WO-75A |
| `[analysis.ivec]` | I-vector series → `.identity.json` | WO-75B |
| `[object.<layer>.<basis>]` | MCF-CAI 3×3 object state grid | WO-76 |

### `[analysis.ivec]` axis mapping (Phase 1)

| Axis | Sidecar field | Interpretation |
|---|---|---|
| `x` | `1 − dataloss` | Existence: identity preservation fraction |
| `y` | `1 − hidden_channel` | EM: complement of hidden-interaction proxy |
| `z` | `recoverable_info` | Spatial: recoverable fraction |
| `t` | `1 − projection_loss` | Temporal: complement of scale-crossing loss |
| `w` | `1 − identity_residual` | Internal: complement of prototype deviation |

Axis order `(x, y, z, t, w)` is **fixed and non-permutable**.

---

*See also: [`VSIM_REFERENCE.md`](VSIM_REFERENCE.md) — complete field reference  
[`docs/VSIM_LANGUAGE.md`](docs/VSIM_LANGUAGE.md) — language format and registry internals  
[`VSIM_DEVELOPMENT.md`](VSIM_DEVELOPMENT.md) — how to add new sections*

---

## Surface Analysis Examples (E12 – E27)

The `[[object.surface]]` section adds **analysis-only measurement probes** to any
running simulation.  Surfaces never modify particle positions, forces, or the
canonical `.xyz` truth-state — they write exclusively to a `.surface.json` sidecar.

### Metric symbols

| Symbol | Field | Meaning |
|---|---|---|
| Φ | `compute_flux` | Net particle crossings / (area · step) |
| Q | `compute_energy_flux` | Kinetic energy flux / (area · step) |
| P | `compute_momentum_flux` | Normal momentum transfer / (area · step) |
| J_s | `compute_species_flux` | Per-species crossing table |
| J_m | `compute_mass_flux` | Mass-weighted crossing flux |
| J_iv | `compute_ivec_flux` | IKK identity-vector crossing signature |

---

## E12 — Minimal Rectangular Surface Probe

**Goal:** the simplest possible surface probe — a single axis-aligned rectangle counts every particle crossing the XZ mid-plane.  No flux metric; crossing count only.  
**Key concepts:** `[object.surface]` master block · `[[object.surface]]` per-surface entry · `log_crossings` · analysis-only doctrine

```toml
# scripts/e12_surface_probe_minimal.vsim
[project]
name        = "e12_surface_probe_minimal"
version     = "v5.14.1"
seed_base   = 1200
determinism = true

[material]
formula   = "NaCl"
structure = "rocksalt"
cell      = "4x4x4"
phase     = "solid"

[run]
mode      = "md"
max_steps = 2000
dt_fs     = 1.0
converge  = false

[export]
write_xyz           = true
output_dir          = "out/e12"

[object.surface]
enabled    = true
write_json = true
output_dir = "out/e12/surfaces"

[[object.surface]]
name          = "midplane_XZ"
geometry      = "rectangle"
center_x      = 0.0
center_y      = 0.0
center_z      = 0.0
normal_x      = 0.0
normal_y      = 1.0
normal_z      = 0.0
width         = 20.0
height        = 20.0
log_crossings = true
compute_flux  = false
output_tag    = "surf_midXZ"

[visual]
output_type     = "terminal_chart"
render_interval = 100
```

> **Doctrine:** surfaces are analysis-only probes.  They never alter force kernels,
> particle positions, or `.xyz` output files.  Surface counts land in the `.surface.json` sidecar.

---

## E13 — Particle Number Flux (Φ)

**Goal:** compute the net signed particle flux Φ through a rectangular channel probe in an Ar gas.  
**Key concepts:** `compute_flux = true` · Φ = net crossings / (area · step) · sidecar output

```toml
# scripts/e13_surface_particle_flux.vsim
[project]
name        = "e13_surface_particle_flux"
version     = "v5.14.1"
seed_base   = 1300
determinism = true

[material]
formula   = "Ar"
prototype = "noble_gas"
phase     = "gas"

[[simulation.molecule]]
formula     = "Ar"
count       = 512
temperature = 300.0
lattice     = "random"

[run]
mode      = "md"
max_steps = 5000
dt_fs     = 2.0
converge  = false

[export]
write_xyz           = true
write_analysis_json = true
output_dir          = "out/e13"

[object.surface]
enabled    = true
write_json = true
output_dir = "out/e13/surfaces"

[[object.surface]]
name          = "channel_probe"
geometry      = "rectangle"
center_x      = 0.0
center_y      = 0.0
center_z      = 0.0
normal_x      = 1.0
normal_y      = 0.0
normal_z      = 0.0
width         = 30.0
height        = 30.0
log_crossings = true
compute_flux  = true
output_tag    = "phi_channel"

[visual]
output_type     = "terminal_chart"
render_interval = 250
```

---

## E14 — Time-Averaged Flux Statistics

**Goal:** compute rolling time-averaged flux metrics over a long MD run to identify steady-state vs fluctuating flow regimes.  
**Key concepts:** `compute_flux` generates per-step data in sidecar · post-analysis temporal statistics · quasi-steady-state detection

```toml
# scripts/e14_surface_time_averaged_flux.vsim
[object.surface]
enabled    = true
write_json = true
output_dir = "out/e14/surfaces"

[[object.surface]]
name          = "flux_temporal_probe"
geometry      = "rectangle"
center_y      = 0.0
normal_y      = 1.0
width         = 25.0
height        = 25.0
log_crossings = true
compute_flux  = true
output_tag    = "phi_temporal"
```

> **Tip:** extract the `flux` array from `.surface.json` and compute rolling mean/variance over a sliding window (e.g. 500 steps) to detect quasi-steady-state.

---

## E15 — Species-Filtered Flux

**Goal:** deploy two co-located probes that separately record Na⁺ and Cl⁻ flux through a NaCl mid-plane.  
**Key concepts:** `species_filter` · `compute_mass_flux` · dual-probe comparative output

```toml
# scripts/e15_surface_species_filtered_flux.vsim
[object.surface]
enabled    = true
write_json = true
output_dir = "out/e15/surfaces"

# Na-only probe
[[object.surface]]
name              = "na_membrane"
geometry          = "rectangle"
center_x          = 0.0
center_y          = 0.0
center_z          = 0.0
normal_x          = 0.0
normal_y          = 1.0
normal_z          = 0.0
width             = 25.0
height            = 20.0
log_crossings     = true
compute_flux      = true
compute_mass_flux = true
species_filter    = ["Na"]
output_tag        = "phi_Na"

# Cl-only probe (comparative)
[[object.surface]]
name              = "cl_membrane"
geometry          = "rectangle"
center_x          = 0.0
center_y          = 0.0
center_z          = 0.0
normal_x          = 0.0
normal_y          = 1.0
normal_z          = 0.0
width             = 25.0
height            = 20.0
log_crossings     = true
compute_flux      = true
compute_mass_flux = true
species_filter    = ["Cl"]
output_tag        = "phi_Cl"
```

> **Tip:** `species_filter` accepts a TOML inline array `["Na", "Cl"]` or a single bare string `"Na"`.

---

## E16 — Kinetic Energy Flux (Q)

**Goal:** measure the kinetic-energy flux Q through a disk probe in a hot Ar gas.  
**Key concepts:** `geometry = "disk"` · `radius` · `compute_energy_flux` · thermal measurement plane

```toml
# scripts/e16_surface_energy_flux.vsim
[object.surface]
enabled    = true
write_json = true
output_dir = "out/e16/surfaces"

[[object.surface]]
name                = "thermal_disk_z"
geometry            = "disk"
center_x            = 0.0
center_y            = 0.0
center_z            = 5.0
normal_x            = 0.0
normal_y            = 0.0
normal_z            = 1.0
radius              = 12.0
log_crossings       = true
compute_energy_flux = true
output_tag          = "Q_disk_z"
```

| Field | Disk-specific notes |
|---|---|
| `geometry = "disk"` | Activates circular geometry; `radius` required. |
| `width` / `height` | Ignored for disk geometry. |
| `radius` | Half-diameter in Å. |

---

## E17 — Multi-Metric Single Probe

**Goal:** show a single probe computing multiple flux metrics (mass + energy) simultaneously on one geometric plane.  
**Key concepts:** `compute_mass_flux = true` + `compute_energy_flux = true` · combined metrics in one sidecar entry · analysis locality

```toml
# scripts/e17_surface_multi_metric_probe.vsim
[object.surface]
enabled    = true
write_json = true
output_dir = "out/e17/surfaces"

[[object.surface]]
name                = "multi_metric_probe"
geometry            = "rectangle"
normal_x            = 1.0
width               = 30.0
height              = 30.0
log_crossings       = true
compute_flux        = true
compute_mass_flux   = true
compute_energy_flux = true
output_tag          = "multi_metric"
```

> **Tip:** a single `output_tag` holds all requested metrics; the sidecar entry contains `"flux"`, `"mass_flux"`, and `"energy_flux"` keys.

---

## E18 — Momentum Flux / Pressure Proxy (P)

**Goal:** estimate the kinetic pressure gradient across an Ar slab using two rectangular probes at ±15 Å.  
**Key concepts:** `compute_momentum_flux` · signed normal-velocity weighting · pressure gradient via top–bottom difference

```toml
# scripts/e18_surface_momentum_flux.vsim
[object.surface]
enabled    = true
write_json = true
output_dir = "out/e18/surfaces"

[[object.surface]]
name                  = "slab_top"
geometry              = "rectangle"
center_y              = 15.0
normal_y              = 1.0
width                 = 30.0
height                = 30.0
log_crossings         = true
compute_flux          = true
compute_momentum_flux = true
output_tag            = "P_top"

[[object.surface]]
name                  = "slab_bottom"
geometry              = "rectangle"
center_y              = -15.0
normal_y              = 1.0
width                 = 30.0
height                = 30.0
log_crossings         = true
compute_flux          = true
compute_momentum_flux = true
output_tag            = "P_bottom"
```

> **Pressure gradient:** `ΔP = P_top - P_bottom` read from `.surface.json["P_top"]["momentum_flux"]` vs `["P_bottom"]["momentum_flux"]`.

---

## E19 — Per-Species Flux Probes

**Goal:** three probes sharing the same geometric plane independently record Na, Cl, and Ar crossings in a mixed simulation.  
**Key concepts:** `compute_species_flux` · per-species sidecar subtables · `output_tag` keying

```toml
# scripts/e19_surface_species_flux_multi.vsim
[[object.surface]]
name                 = "probe_Na"
geometry             = "rectangle"
normal_z             = 1.0
width                = 20.0
height               = 20.0
log_crossings        = true
compute_species_flux = true
species_filter       = ["Na"]
output_tag           = "J_Na"

[[object.surface]]
name                 = "probe_Cl"
geometry             = "rectangle"
normal_z             = 1.0
width                = 20.0
height               = 20.0
log_crossings        = true
compute_species_flux = true
species_filter       = ["Cl"]
output_tag           = "J_Cl"

[[object.surface]]
name                 = "probe_Ar"
geometry             = "rectangle"
normal_z             = 1.0
width                = 20.0
height               = 20.0
log_crossings        = true
compute_species_flux = true
species_filter       = ["Ar"]
output_tag           = "J_Ar"
```

---

## E20 — Identity-Vector (IVec) Flux

**Goal:** couple the surface analysis layer to the IVec module (WO-75B) to record identity-vector crossings at a Si crystal boundary disk.  
**Key concepts:** `compute_ivec_flux` · `[analysis.ivec]` co-activation · crystal boundary measurement

```toml
# scripts/e20_surface_ivec_flux.vsim
[analysis.ivec]
enabled         = true
write_json      = true
convergence_eps = 1.0e-4

[object.surface]
enabled    = true
write_json = true
output_dir = "out/e20/surfaces"

[[object.surface]]
name              = "ivec_boundary_disk"
geometry          = "disk"
center_z          = 0.0
normal_z          = 1.0
radius            = 8.0
log_crossings     = true
compute_ivec_flux = true
output_tag        = "ivec_surf_z"
```

> **Coupling rule:** `compute_ivec_flux = true` is silently ignored unless `[analysis.ivec] enabled = true` is also present in the same script.

---

## E21 — Spherical Surface Geometry

**Goal:** measure particle flux across a closed spherical shell to model a droplet boundary or bubble interface.  
**Key concepts:** `geometry = "sphere"` · closed 3D shell with `radius` · isotropic flux measurement · droplet/bubble diagnostics

```toml
# scripts/e21_surface_sphere_geometry.vsim
[object.surface]
enabled    = true
write_json = true
output_dir = "out/e21/surfaces"

[[object.surface]]
name                = "droplet_shell"
geometry            = "sphere"
center_x            = 0.0
center_y            = 0.0
center_z            = 0.0
radius              = 10.0
log_crossings       = true
compute_flux        = true
compute_energy_flux = true
output_tag          = "sphere_evap"
```

> **Sphere note:** `geometry = "sphere"` activates a closed shell; positive flux = outward crossing (evaporation). Area = 4πr².

---

## E22 — Multi-Probe Flux Pipeline

**Goal:** combine Φ, Q, and P probes in a single script to build a simultaneous three-metric sidecar for a 2048-atom Ar gas.  
**Key concepts:** three `[[object.surface]]` blocks · `write_report_md` companion · combined `.surface.json` keyed by `output_tag`

```toml
# scripts/e22_surface_multi_probe_pipeline.vsim
[object.surface]
enabled    = true
write_json = true
output_dir = "out/e22/surfaces"

# Phi probe (particle flux) at Z = -10
[[object.surface]]
name          = "phi_probe_bottom"
geometry      = "rectangle"
center_z      = -10.0
normal_z      = 1.0
width         = 40.0
height        = 40.0
log_crossings = true
compute_flux  = true
output_tag    = "phi_z_neg10"

# Q probe (energy flux) disk at Z = 0
[[object.surface]]
name                = "Q_probe_mid"
geometry            = "disk"
center_z            = 0.0
normal_z            = 1.0
radius              = 15.0
log_crossings       = true
compute_energy_flux = true
output_tag          = "Q_disk_z0"

# P probe (momentum flux) at Z = +10
[[object.surface]]
name                  = "P_probe_top"
geometry              = "rectangle"
center_z              = 10.0
normal_z              = 1.0
width                 = 40.0
height                = 40.0
log_crossings         = true
compute_momentum_flux = true
output_tag            = "P_z_pos10"
```

---

## E23 — Batch Parameter Sweep with Surface Probes

**Goal:** integrate surface analysis with `[batch]` parameter expansion; sweep temperature {300, 400, 500, 600} K and record flux variation.  
**Key concepts:** `[batch]` matrix creates four jobs · each gets its own `.surface.json` · aggregated flux-vs-temperature post-analysis

```toml
# scripts/e23_surface_batch_sweep.vsim
[batch]
enabled = true
mode    = "matrix"

[[batch.parameter]]
section = "simulation.molecule"
key     = "temperature"
values  = [300.0, 400.0, 500.0, 600.0]

[object.surface]
enabled    = true
write_json = true
output_dir = "out/e23/surfaces"

[[object.surface]]
name          = "batch_flux_probe"
geometry      = "rectangle"
normal_z      = 1.0
width         = 30.0
height        = 30.0
log_crossings = true
compute_flux  = true
output_tag    = "phi_batch"
```

---

## E24 — While-Loop Convergence on Flux Stationarity

**Goal:** couple `[while]` convergence guard to surface flux variance; continue until quasi-steady-state flow is achieved.  
**Key concepts:** `[while]` criteria on flux variance · iterative simulation gated on analysis metrics · stationarity detection

```toml
# scripts/e24_surface_while_loop_convergence.vsim
[variance]
name         = "flux_variance"
target       = 0.0001
tolerance    = 0.00005
check_every  = 200
max_restarts = 5

[while]
criteria    = "flux_variance"
max_cycles  = 10
cycle_steps = 1000

[object.surface]
enabled    = true
write_json = true
output_dir = "out/e24/surfaces"

[[object.surface]]
name          = "flux_convergence_probe"
geometry      = "rectangle"
normal_y      = 1.0
width         = 25.0
height        = 25.0
log_crossings = true
compute_flux  = true
output_tag    = "phi_while"
```

---

## E25 — Surface Flux + RDF Correlation Analysis

**Goal:** simultaneously activate `[analysis.structure]` (RDF) and surface probes to correlate flux with local coordination.  
**Key concepts:** `compute_rdf = true` → `.rdf.json` · `compute_flux = true` → `.surface.json` · cross-module coupling · liquid–solid interface

```toml
# scripts/e25_surface_rdf_correlation.vsim
[analysis.structure]
enabled     = true
compute_rdf = true
rdf_rmax    = 10.0
rdf_nbins   = 200

[object.surface]
enabled    = true
write_json = true
output_dir = "out/e25/surfaces"

[[object.surface]]
name          = "interface_probe"
geometry      = "rectangle"
normal_z      = 1.0
width         = 20.0
height        = 20.0
log_crossings = true
compute_flux  = true
output_tag    = "phi_rdf"
```

---

## E26 — Surface Flux Verification with [verify.structure]

**Goal:** combine surface analysis with the empirical verification layer (WO-62A) to check flux consistency with mass conservation.  
**Key concepts:** `[verify.structure]` + surface probes · analysis → verification pipeline · co-activation

```toml
# scripts/e26_surface_flux_verification.vsim
[verify]
enabled = true

[verify.structure]
check_rdf                = true
rdf_first_peak_min       = 3.0
rdf_first_peak_max       = 4.0
rdf_coordination_min     = 10.0
rdf_coordination_max     = 14.0
rdf_coordination_species = "Ne"

[object.surface]
enabled    = true
write_json = true
output_dir = "out/e26/surfaces"

[[object.surface]]
name          = "verify_flux_probe"
geometry      = "rectangle"
normal_x      = 1.0
width         = 25.0
height        = 25.0
log_crossings = true
compute_flux  = true
output_tag    = "phi_verify"
```

---

## E27 — Full Surface-Analysis Showcase

**Goal:** capstone example deploying five probes (rectangle + disk, all metrics, all species, IVec) on a NaCl+Ar system.  
**Key concepts:** all surface metrics simultaneously · five named probes · `write_report_md` for post-analysis · integration smoke-test

```toml
# scripts/e27_surface_full_analysis_showcase.vsim
[analysis.ivec]
enabled         = true
write_json      = true
convergence_eps = 1.0e-5

[object.surface]
enabled    = true
write_json = true
output_dir = "out/e27/surfaces"

# surf_A: all metrics, rectangle, XY plane
[[object.surface]]
name                  = "surf_A_full"
geometry              = "rectangle"
normal_z              = 1.0
width                 = 30.0
height                = 30.0
log_crossings         = true
compute_flux          = true
compute_energy_flux   = true
compute_momentum_flux = true
compute_mass_flux     = true
output_tag            = "surf_A"

# surf_B: Na species flux, disk
[[object.surface]]
name                 = "surf_B_Na"
geometry             = "disk"
normal_z             = 1.0
radius               = 10.0
log_crossings        = true
compute_species_flux = true
species_filter       = ["Na"]
output_tag           = "surf_B_Na"

# surf_C: Cl species flux, disk
[[object.surface]]
name                 = "surf_C_Cl"
geometry             = "disk"
normal_z             = 1.0
radius               = 10.0
log_crossings        = true
compute_species_flux = true
species_filter       = ["Cl"]
output_tag           = "surf_C_Cl"

# surf_D: Ar mass flux, rectangle, XZ plane
[[object.surface]]
name              = "surf_D_Ar_mass"
geometry          = "rectangle"
normal_y          = 1.0
width             = 30.0
height            = 30.0
log_crossings     = true
compute_mass_flux = true
species_filter    = ["Ar"]
output_tag        = "surf_D_Ar"

# surf_E: IVec flux at crystal top z=+15
[[object.surface]]
name              = "surf_E_ivec_top"
geometry          = "disk"
center_z          = 15.0
normal_z          = 1.0
radius            = 8.0
log_crossings     = true
compute_ivec_flux = true
output_tag        = "surf_E_ivec"
```

### Sidecar output layout (`e27.surface.json`)

```json
{
  "run": "e27_surface_full_showcase",
  "surfaces": {
    "surf_A":    { "crossings": [...], "flux": ..., "energy_flux": ..., "momentum_flux": ..., "mass_flux": ... },
    "surf_B_Na": { "crossings": [...], "species_flux": { "Na": ... } },
    "surf_C_Cl": { "crossings": [...], "species_flux": { "Cl": ... } },
    "surf_D_Ar": { "crossings": [...], "mass_flux": ... },
    "surf_E_ivec":{ "crossings": [...], "ivec_flux": [...] }
  }
}
```

---

## E28 — Full Export Suite (All Formats + Surface)

**Goal:** activate every export format simultaneously alongside surface analysis to verify complete pipeline integration.  
**Key concepts:** `write_xyz`, `write_xyz_full`, `write_analysis_json`, `write_report_md`, `write_rdf` · surface sidecar coexists with all formats · integration smoke-test

```toml
# scripts/e28_surface_export_suite.vsim
[analysis.structure]
enabled     = true
compute_rdf = true
rdf_rmax    = 10.0
rdf_nbins   = 200

[analysis.ivec]
enabled         = true
write_json      = true
convergence_eps = 1.0e-4

[export]
write_xyz           = true
write_xyz_full      = true
write_analysis_json = true
write_report_md     = true
write_rdf           = true
output_dir          = "out/e28"

[object.surface]
enabled    = true
write_json = true
output_dir = "out/e28/surfaces"

[[object.surface]]
name                = "export_suite_probe"
geometry            = "disk"
normal_z            = 1.0
radius              = 15.0
log_crossings       = true
compute_flux        = true
compute_energy_flux = true
compute_ivec_flux   = true
output_tag          = "export_suite"
```

### Output files produced by E28

| File | Source |
|---|---|
| `e28.xyz` | `write_xyz = true` |
| `e28.xyzFull` | `write_xyz_full = true` |
| `e28.analysis.json` | `write_analysis_json = true` |
| `e28.report.md` | `write_report_md = true` |
| `e28.rdf.json` | `write_rdf = true` |
| `e28.identity.json` | `[analysis.ivec] write_json = true` |
| `e28.surface.json` | `[object.surface] write_json = true` |

---

## E29 — Surface Error Analysis and Diagnostic Patterns

**Goal:** intentionally misconfigure surface probes to demonstrate error handling, validation warnings, and diagnostic output.  
**Key concepts:** parser robustness · validation warnings (non-fatal) · diagnostic patterns · failure-mode documentation

```toml
# scripts/e29_surface_error_analysis.vsim
[object.surface]
enabled    = true
write_json = true
output_dir = "out/e29/surfaces"

# Probe 1: disk with ignored width/height fields
[[object.surface]]
name               = "disk_with_excess_fields"
geometry           = "disk"
radius             = 10.0
width              = 999.0  # Ignored for disk geometry
height             = 999.0  # Ignored for disk geometry
log_crossings      = true
compute_flux       = true
output_tag         = "disk_excess"

# Probe 2: species_filter referencing nonexistent species
[[object.surface]]
name              = "unknown_species_filter"
geometry          = "rectangle"
normal_y          = 1.0
width             = 20.0
height            = 20.0
log_crossings     = true
compute_flux      = true
species_filter    = ["Nonexistent", "Ar"]
output_tag        = "unknown_species"

# Probe 3: compute_ivec_flux without [analysis.ivec]
[[object.surface]]
name              = "orphan_ivec_flux"
geometry          = "rectangle"
normal_z          = 1.0
width             = 15.0
height            = 15.0
log_crossings     = false
compute_ivec_flux = true
output_tag        = ""  # Empty tag → fallback to "surface_<index>"
```

### Expected warnings (non-fatal)

- `"species_filter includes unknown species: Nonexistent"`
- `"compute_ivec_flux requires [analysis.ivec] enabled; ignored"`
- `"output_tag empty; using fallback 'surface_2'"`

---

## Advanced Native Examples (E30-E35)

These examples use both `VSIM_LANGUAGE_REFERENCE.md` for the stable language
surface and `VSIM_REFERENCE.md` for work-order extensions. Compatibility labels
are part of the example contract and must be preserved when scripts are copied.

| Example | Parser | Standard runner | Additional requirement |
|---|---|---|---|
| E30 | Supported | Partial | Dynx/demo dispatch depends on runner |
| E31 | Supported | Partial | Analysis pipeline/sidecar dispatch |
| E32 | Supported | Partial | Isomer analysis library invocation |
| E33 | Supported | Not wired | Generator and trajectory walker |
| E34 | Supported | Partial | Classify or chemistry/dissolution bridge |
| E35 | Supported | Adapter-required | External comparison artifacts |

---

## E30 — Dual-Seed Audit and Dynx Intent

**Goal:** make simulation seeds and world-context identity seeds explicit while
keeping them deterministic and auditable.  
**Key concepts:** `[seed]` · `world_seed` · `[dynx]` · `[export.demo]` · PBC  
**Compatibility:** `SUPPORTED_PARSE_PARTIAL_DISPATCH`  
**Canonical file:** `examples/advanced_native/e30_dual_seed_audit.vsim`

```toml
[project]
name        = "e30_dual_seed_audit"
version     = "v5.14.1-examples-1"
seed_base   = 30030
determinism = true

[seed]
foundation = 30030
defect     = 33030
formation  = 36030
thermal    = 38030
placement  = 41030
world_seed = "7f4a7c159e3779b97f4a7c159e3779b97f4a7c159e3779b97f4a7c159e3779b9"

[material]
formula   = "NaCl"
prototype = "B1_NaCl"
cell      = "3x3x3"
phase     = "solid"

[run]
mode         = "relax"
max_steps    = 600
dt_fs        = 0.5
temperature  = 100.0
converge     = true
output_level = "standard"

[dynx]
emit_mode        = "on_complete"
rich_frames      = true
live_cache       = false
output_path      = "out/advanced_native/e30/e30_dual_seed_audit.dynx"
frame_skip       = 2
write_provenance = true
compress         = false

[verify]
enabled             = true
profile             = "e30_dual_seed_nacl"
write_verify_report = true
write_verify_tsv    = true

[verify.mass]
enabled            = true
relative_tolerance = 1e-10

[export]
write_xyz           = true
write_analysis_json = true
write_events_json   = true
write_report_md     = true
write_manifest_json = true
output_dir          = "out/advanced_native/e30"

[export.demo]
enabled           = true
demo_frames       = 12
strategy          = "uniform"
write_demo_dynx   = true
write_demo_bundle = true
include_source    = true
include_manifest  = true
```

`foundation` is the instance seed. `world_seed` is a shared Seed256 context.
Dual-seed identity labels remain metadata; they do not replace forces, positions,
or other simulation truth.

---

## E31 — Multiscale Identity Gate

**Goal:** combine structural sampling, field projection, RVE windows, inference,
and derived identity reports in one auditable pipeline.  
**Key concepts:** `[analysis.scale_sampling]` · `[analysis.inference]` ·
`[analysis.ikk_end_tag]` · `[analysis.ivec]`  
**Compatibility:** `SUPPORTED_PIPELINE_PARTIAL_RUNNER_DISPATCH`  
**Canonical file:** `examples/advanced_native/e31_multiscale_identity_gate.vsim`

```toml
[project]
name        = "e31_multiscale_identity_gate"
version     = "v5.14.1-examples-1"
seed_base   = 31031
determinism = true

[material]
formula   = "Fe"
prototype = "A2_bcc"
cell      = "5x5x5"
phase     = "solid"

[run]
mode         = "relax"
max_steps    = 800
dt_fs        = 0.5
temperature  = 300.0
converge     = true
output_level = "verbose"

[analysis.structure]
enabled              = true
compute_coordination = true
compute_packing      = true
compute_displacement = true
compute_anisotropy   = true
neighbor_cutoff_A    = 3.2
contact_cutoff_A     = 2.7

[analysis.sampling]
enabled               = true
compute_rdf           = true
compute_msd           = true
min_frames_for_motion = 10
min_frames_for_msd    = 20
unwrap_pbc            = true

[analysis.scale_sampling]
enabled                          = true
compute_field_projection         = true
compute_rve_sampling             = true
compute_emergence_metrics        = true
field_grid                       = [10, 10, 10]
rve_window_lengths_A             = [2.48, 4.96, 7.00]
rve_windows_per_level            = 12
rve_window_placement             = "stratified"
min_particles_for_scale_sampling = 128
spatial_cv_threshold             = 0.22
temporal_drift_threshold         = 0.10
scale_drift_threshold            = 0.18

[analysis.inference]
enabled                   = true
mode                      = "rule_based_61d"
infer_macro_readiness     = true
min_particles_macro_candidate = 128
min_frames_macro_candidate    = 20

[analysis.ikk_end_tag]
enabled           = true
emit_markdown     = true
emit_latex        = false
d_pass_threshold  = 0.70
d_warn_threshold  = 0.40
scale_regime      = "S_mat"
section_reference = "E31"

[analysis.ivec]
enabled       = true
write_json    = true
include_delta = true
include_var   = true
output_dir    = "out/advanced_native/e31/identity"

[output]
output_dir                = "out/advanced_native/e31/pipeline"
output_prefix             = "e31"
write_structure_json      = true
write_sampling_json       = true
write_scale_sampling_json = true
write_inference_json      = true
write_sampling_manifest   = true

[export]
write_xyz           = true
write_analysis_json = true
write_metrics_tsv   = true
write_report_md     = true
write_manifest_json = true
output_dir          = "out/advanced_native/e31"
```

The I-vector and IKK badge are analysis products. They may describe projection
loss or recoverability, but they must never be used as force inputs or written
back into `.xyz`/`.xyzFull` truth-state records.

---

## E32 — Static Isomer Analysis

**Goal:** validate graph connectivity and valence before geometry comparison,
then derive a deterministic canonical hash.  
**Key concepts:** `[analysis.isomers]` · connectivity-before-valence · canonical
hash · Kabsch/RMSD  
**Compatibility:** `LIBRARY_WIRED_RUNNER_GAP`  
**Canonical file:** `examples/advanced_native/e32_static_isomer_analysis.vsim`

```toml
[project]
name        = "e32_static_isomer_analysis"
version     = "v5.14.1-examples-1"
seed_base   = 32032
determinism = true

[material]
formula   = "C2H6O"
structure = "molecular"
phase     = "gas"

[run]
mode         = "relax"
max_steps    = 600
dt_fs        = 0.5
temperature  = 0.0
converge     = true
output_level = "verbose"

[analysis.isomers]
enabled              = true
mode                 = "graph_geometry"
formula_guard        = true
graph_validation     = true
connectivity_check   = true
valence_check        = true
formal_charge_check  = true
canonical_hash       = true
geometry_rmsd        = true
relaxation_check     = true
known_database_check = false
allow_fragments      = false
allow_charged        = false
allow_radicals       = false
allow_strained       = true
max_bond_order       = 3
bond_tolerance_scale = 1.20
rmsd_tolerance       = 0.05
angle_tolerance_deg  = 3.0
bond_source          = "hybrid"
geometry_build       = "vsepr"
report_level         = "detailed"

[export]
write_xyz           = true
write_analysis_json = true
write_report_md     = true
write_manifest_json = true
output_dir          = "out/advanced_native/e32"
```

The analysis library and report writers are implemented. The standard `vsepr
run` path does not yet provide a complete isomer-analysis dispatch, so parser
success alone is not scientific acceptance.

---

## E33 — Isomer Generation and Tracking Probe

**Goal:** give the swarm a concrete parser-valid contract for the two remaining
WO-VSIM-04A runtime gaps.  
**Key concepts:** `[generator.isomers]` · `[analysis.isomer_tracking]` ·
canonical deduplication · JSONL transition log  
**Compatibility:** `PARSED_NOT_WIRED`  
**Canonical file:** `examples/advanced_native/e33_isomer_generation_tracking_probe.vsim`

```toml
[project]
name        = "e33_isomer_generation_tracking_probe"
version     = "v5.14.1-examples-1"
seed_base   = 33033
determinism = true

[material]
formula   = "C3H6O"
structure = "molecular"
phase     = "gas"

[run]
mode         = "single_point"
max_steps    = 1
dt_fs        = 0.5
temperature  = 300.0
converge     = false
output_level = "minimal"

[generator.isomers]
enabled         = true
formula         = "C3H6O"
allow_fragments = false
allow_charged   = false
allow_radicals  = false
allow_strained  = true
max_bond_order  = 3
deduplicate     = "canonical_graph_hash"

[analysis.isomer_tracking]
enabled              = true
source               = "out/advanced_native/e33/c3h6o_candidates.xyzf"
sample_every         = 10
detect_bond_changes  = true
detect_hash_changes  = true
write_transition_log = true

[export]
write_analysis_json = true
write_report_md     = true
write_manifest_json = true
output_dir          = "out/advanced_native/e33"
```

Expected development outputs are generated candidate geometries and
`reports/isomer_transitions.jsonl`. Until the generator dispatch and trajectory
walker are wired, this example is a contract test, not a runnable result.

---

## E34 — ChemPlus Dissolution Overlay

**Goal:** combine reaction classification with a parameterized protonation
ladder while preserving the simulation/analysis boundary.  
**Key concepts:** `[chem_plus]` · `[dissolution]` · `[chemistry]` · classify
preview · reaction-event observables  
**Compatibility:** `PARTIAL_CLASSIFY_AND_DISSOLUTION_BRIDGE`  
**Canonical file:** `examples/advanced_native/e34_chemplus_dissolution_overlay.vsim`

```toml
[project]
name        = "e34_chemplus_dissolution_overlay"
version     = "v5.14.1-examples-1"
seed_base   = 34034
determinism = true

[material]
formula   = "Fe2O3"
structure = "corundum"
cell      = "3x3x2"
phase     = "solid"

[run]
mode         = "md"
max_steps    = 2000
dt_fs        = 0.5
temperature  = 298.15
converge     = false
output_level = "verbose"

[environment]
periodic    = false
temperature = 298.15
pressure    = 0.000101325
medium      = "water"

[chemistry]
chemistry              = "hydrolysis"
heat                   = -1
reaction_events        = true
track_species_state    = true
event_registry         = true
min_score_threshold    = 0.20
max_reactions_per_step = 12

[dissolution]
enabled              = true
engine               = "protonation_ladder"
dG_first_protonation = -12.0
dG_second_protonation = -8.0
Ea_bridging_cleavage = 25.0
Ea_terminal_release  = 15.0
dG_hydration_Fe3     = -105.0
dG_sulfate_mono      = -3.5
dG_sulfate_bi        = -6.0
dG_sulfate_bridge    = -8.5
pH_reference         = 1.0
T_reference          = 298.15
site_density_per_nm2 = 5.0

[chem_plus]
reaction       = "Fe2O3 + 3H2SO4 -> Fe2(SO4)3 + 3H2O"
class_override = "acid-base"
vsepr_link     = false
emit_events    = true

[observe]
metrics       = ["formation", "reaction_events", "chemical_state", "exothermic_count", "avg_delta_E"]
output_format = "json"
every_n_steps = 20

[export]
write_xyz           = true
write_analysis_json = true
write_events_json   = true
write_report_md     = true
write_manifest_json = true
output_dir          = "out/advanced_native/e34"
```

Use `vsepr classify` to inspect the ChemPlus classification. Dissolution scores
and symbolic state are derived overlays; the atomistic kernel remains the owner
of positions, velocities, forces, and energies.

---

## E35 — External Verification Contract

**Goal:** define quantitative comparison gates for an external validator without
treating external output as injected truth.  
**Key concepts:** `[verify.compare]` · `[verify.thresholds]` ·
`[verify.outputs]` · LAMMPS adapter boundary  
**Compatibility:** `PARSED_EXTERNAL_ADAPTER_REQUIRED`  
**Canonical file:** `examples/advanced_native/e35_external_verification_contract.vsim`

```toml
[project]
name        = "e35_external_verification_contract"
version     = "v5.14.1-examples-1"
seed_base   = 35035
determinism = true

[material]
formula   = "NaCl"
prototype = "B1_NaCl"
cell      = "4x4x4"
phase     = "solid"

[run]
mode         = "md"
max_steps    = 2000
dt_fs        = 1.0
temperature  = 300.0
pressure_GPa = 0.0
converge     = false
output_level = "standard"

[verify]
enabled             = true
profile             = "e35_nacl_external_comparison"
target              = "lammps"
write_verify_report = true
write_verify_tsv    = true

[verify.compare]
energy       = true
temperature  = true
pressure     = true
rdf          = true
msd          = true
diffusion    = true
coordination = true
rmsd         = true

[verify.thresholds]
energy_rel_error       = 0.05
temperature_rel_error  = 0.05
pressure_rel_error     = 0.10
rdf_l1_error           = 0.15
msd_rel_error          = 0.20
diffusion_rel_error    = 0.25
coordination_abs_error = 0.25
rmsd_A                 = 0.20

[verify.outputs]
write_report         = true
write_json           = true
write_overlay_tables = true

[export]
write_xyz           = true
write_analysis_json = true
write_metrics_tsv   = true
write_report_md     = true
write_manifest_json = true
output_dir          = "out/advanced_native/e35"
```

This file declares comparison intent only. An adapter must supply external
artifacts with provenance before these thresholds can produce an empirical pass.
External results validate the simulation; they do not overwrite kernel state.

---

## QR — Quick-Reference Card

### Section inventory

| Section | Purpose | WO |
|---|---|---|
| `[project]` | Name, version, seed, determinism | — |
| `[seed]` | Subsystem seeds and optional Seed256 world context | WO-66K |
| `[material]` | Formula, prototype, cell, phase | — |
| `[[simulation.molecule]]` | Per-species molecule block | — |
| `[run]` | Mode, steps, dt, convergence | — |
| `[chemistry]` | Reaction rules, barriers | — |
| `[environment]` | Temperature, pressure, medium | — |
| `[simulation]` | PBC, Ewald, formation preset | — |
| `[observe]` | RDF, order params, per-atom flags | — |
| `[analysis.structure]` | Structure fingerprint | — |
| `[analysis.sampling]` | Scalar sampling, MSD | — |
| `[analysis.scale_sampling]` | Field projection, RVE, emergence | WO-61D |
| `[analysis.inference]` | Property inference | WO-61B |
| `[analysis.ikk_end_tag]` | IKK report end-tag badge | WO-75A |
| `[analysis.ivec]` | I-vector series → `.identity.json` | WO-75B |
| `[analysis.isomers]` | Static graph/geometry isomer analysis | WO-VSIM-04A |
| `[generator.isomers]` | Isomer generation contract (runner pending) | WO-VSIM-04A |
| `[analysis.isomer_tracking]` | Trajectory hash transitions (runner pending) | WO-VSIM-04A |
| `[object.<layer>.<basis>]` | MCF-CAI 3×3 object state grid | WO-76 |
| `[object.surface]` | Surface analysis master block | Surface |
| `[[object.surface]]` | Per-surface probe definition | Surface |
| `[export]` | File outputs, format, output dir | — |
| `[visual]` | GL/terminal output, spin, orbit | — |
| `[batch]` | Parameter sweep blocks | — |
| `[variance]` / `[while]` | Convergence guard loop | — |
| `[dynx]` | Dynamic session archive intent | WO-72C |
| `[chem_plus]` | Declarative reaction classification overlay | WO-84T |
| `[dissolution]` | Protonation/dissolution analysis parameters | WO-56D |
| `[verify.compare]` | External metric comparison switches | LAMMPS-VERIFY-01 |
| `[verify.thresholds]` | External comparison tolerances | LAMMPS-VERIFY-01 |
| `[verify.outputs]` | Verification report/JSON/table outputs | LAMMPS-VERIFY-01 |

### `[object.surface]` / `[[object.surface]]` field quick-reference

| Field | Type | Default | Notes |
|---|---|---|---|
| `enabled` | bool | `false` | Master on/off switch |
| `write_json` | bool | `false` | Emit `.surface.json` sidecar |
| `output_dir` | string | `""` | Directory for sidecar files |
| `name` | string | `""` | Surface label (per-probe) |
| `geometry` | string | `"rectangle"` | `"rectangle"` · `"disk"` · `"sphere"` |
| `center_x/y/z` | float | `0.0` | Probe centre in Å |
| `normal_x/y/z` | float | `0,0,1` | Unit normal (auto-normalised) |
| `width` / `height` | float | `10.0` | Rectangle half-extents in Å |
| `radius` | float | `5.0` | Disk / sphere radius in Å |
| `log_crossings` | bool | `false` | Record every crossing event |
| `compute_flux` | bool | `false` | Φ — particle number flux |
| `compute_mass_flux` | bool | `false` | J_m — mass-weighted flux |
| `compute_energy_flux` | bool | `false` | Q — kinetic energy flux |
| `compute_momentum_flux` | bool | `false` | P — momentum transfer / pressure proxy |
| `compute_species_flux` | bool | `false` | J_s — per-species crossing table |
| `compute_ivec_flux` | bool | `false` | J_iv — IVec crossing signature (needs `[analysis.ivec]`) |
| `species_filter` | string or list | `[]` | Restrict probe to named species |
| `output_tag` | string | `""` | Key in `.surface.json` for this probe |

### `[analysis.ivec]` axis mapping (Phase 1)

| Axis | Sidecar field | Interpretation |
|---|---|---|
| `x` | `1 − dataloss` | Existence: identity preservation fraction |
| `y` | `1 − hidden_channel` | EM: complement of hidden-interaction proxy |
| `z` | `recoverable_info` | Spatial: recoverable fraction |
| `t` | `1 − projection_loss` | Temporal: complement of scale-crossing loss |
| `w` | `1 − identity_residual` | Internal: complement of prototype deviation |

Axis order `(x, y, z, t, w)` is **fixed and non-permutable**.

---

*See also: [`VSIM_REFERENCE.md`](VSIM_REFERENCE.md) — complete field reference  
[`docs/VSIM_LANGUAGE.md`](docs/VSIM_LANGUAGE.md) — language format and registry internals  
[`VSIM_DEVELOPMENT.md`](VSIM_DEVELOPMENT.md) — how to add new sections*
