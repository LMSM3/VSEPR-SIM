# VSIM Script Reference

> **Status:** Living document — updated with every `.vsim` language change.  
> **Source of truth:** `include/vsim/vsim_document.hpp`  
> **Parser:** `src/vsim/vsim_parser.cpp`  
> **Validator:** `vsper validate <script.vsim>`  
> **Language guide:** `docs/VSIM_LANGUAGE.md`

---

## How to read this document

Each section corresponds to a `[header]` block in a `.vsim` file.  
For each field: **name**, **type**, **default**, and what it does.

| Symbol | Meaning |
|---|---|
| ✅ | Parsed and wired to kernel |
| 🔶 | Parsed, stored, not yet wired |
| ❌ | Not yet parsed (planned) |

---

## `[project]`

**Struct:** `ProjectSection`  
**Required.** Identifies the script.

| Field | Type | Default | Status | Notes |
|---|---|---|---|---|
| `name` | string | `""` | ✅ | Required. Used in report filenames and dashboard labels. |
| `version` | string | `""` | ✅ | Informational. Stored, not validated against runtime version. |
| `seed_base` | uint64 | `0` | ✅ | Base RNG seed. Same seed + same script = same run. |
| `determinism` | bool | `true` | ✅ | Always true in current implementation. Field kept for schema completeness. |
| `description` | string | `""` | 🔶 | Stored in `ProjectSection::description`. Not emitted to reports yet. |

```toml
[project]
name        = "demo_03_graphite_stack"
version     = "v5.0.0-beta.8"
seed_base   = 3008
determinism = true
description = "5-layer AB Bernal graphite ERB modulation test"
```

---

## `[simulation]`

**Struct:** `SimulationSection`  
Controls the FIRE relaxation / dynamics run.

| Field | Type | Default | Status | Notes |
|---|---|---|---|---|
| `fire_max_steps` | int | `500` | ✅ | Maximum FIRE minimisation steps. Must be ≥ 1. |
| `fire_dt_fs` | double | `1.0` | ✅ | Initial FIRE timestep (femtoseconds). |
| `box_size_ang` | double | `50.0` | ✅ | Cubic box edge (Å). `0` = auto-size from particle count. |
| `periodic` | bool | `false` | ✅ | Enable PBC. Requires `[cell]` + `[boundary]` sections. |
| `formation_preset` | string | `""` | ✅ | Named potential preset: `"metal"`, `"ceramic"`, `"polymer"`, `"ionic"`. |
| `use_ewald` | bool | `false` | ✅ | Ewald long-range Coulomb. Requires `periodic = true`. |
| `ewald_alpha` | double | `0.3` | ✅ | Ewald splitting parameter (Å⁻¹). |
| `ewald_rcut` | double | `10.0` | ✅ | Real-space cutoff (Å). |
| `ewald_kmax` | int | `5` | ✅ | k-vector shell range per axis. |
| `step_delay_ms` | int | `0` | ✅ | Artificial sleep between FIRE steps (ms). `0` = off. |
| `resim_delay_ms` | int | `400` | ✅ | Pause before a resimulation (ms). |
| `smooth_resim` | bool | `true` | ✅ | Fade event spine between resimulations (terminal animation). |

### `[[simulation.molecule]]`

Repeatable. Declares a species or material component.

| Field | Type | Default | Status | Notes |
|---|---|---|---|---|
| `formula` | string | `""` | ✅ | Required. Chemical formula string, e.g. `"NaCl"`, `"C6H12"`. |
| `count` | int | `1` | ✅ | Number of copies / unit cells. Must be ≥ 1. |
| `temperature` | double | `300.0` | ✅ | Formation temperature (K). Must be ≥ 0. |
| `lattice` | string | `""` | ✅ | Lattice hint: `"hexagonal"`, `"bcc"`, `"fcc"`, `"fcc_ionic"`, `"none"`. |
| `layer_mode` | string | `""` | ✅ | Stacking mode: `"AB"` (Bernal), `"AA"`, `"turbostratic"`. |
| `n_layers` | int | `1` | ✅ | Layer count (graphene / graphite stacks). |

```toml
[[simulation.molecule]]
formula     = "C"
count       = 480
temperature = 300.0
lattice     = "hexagonal"
layer_mode  = "AB"
n_layers    = 5
```

---

## `[cell]`

**Struct:** `CellSection`  
**WO-57B.** Explicit simulation box dimensions for PBC runs.

| Field | Type | Default | Status | Notes |
|---|---|---|---|---|
| `type` | string | `"orthorhombic"` | ✅ | Only `"orthorhombic"` supported. `"triclinic"` reserved. |
| `lx` | double | `0.0` | ✅ | Box length X (Å). All three must be > 0 for PBC. |
| `ly` | double | `0.0` | ✅ | Box length Y (Å). |
| `lz` | double | `0.0` | ✅ | Box length Z (Å). |
| `units` | string | `"angstrom"` | ✅ | `"angstrom"` supported. `"nm"` reserved. |

```toml
[cell]
lx = 40.0
ly = 40.0
lz = 40.0
```

---

## `[boundary]`

**Struct:** `BoundarySection`  
**WO-57B.** Per-axis boundary condition.

| Field | Type | Default | Status | Notes |
|---|---|---|---|---|
| `x` | string | `"open"` | ✅ | `"periodic"`, `"open"`, `"reflective"` (reserved), `"absorbing"` (reserved). |
| `y` | string | `"open"` | ✅ | Same values as `x`. |
| `z` | string | `"open"` | ✅ | Same values as `x`. |

```toml
[boundary]
x = "periodic"
y = "periodic"
z = "periodic"
```

---

## `[pbc]`

**Struct:** `PBCSection`  
**WO-57B.** PBC runtime options.

| Field | Type | Default | Status | Notes |
|---|---|---|---|---|
| `minimum_image` | bool | `true` | ✅ | Apply minimum image convention to distances. |
| `wrap_positions` | string | `"after_step"` | ✅ | When to remap positions: `"after_step"`, `"after_force"`, `"on_export"`, `"never"`. |
| `track_images` | bool | `true` | ✅ | Track image counts per particle for unwrapped trajectories. |
| `unwrap_for_diffusion` | bool | `true` | ✅ | Unwrap coordinates before MSD / diffusion analysis. |

```toml
[pbc]
minimum_image        = true
wrap_positions       = "after_step"
track_images         = true
unwrap_for_diffusion = true
```

---

## `[export]`

**Struct:** `ExportSection`  
Controls which output files are written after a run.

### Atomistic state

| Field | Type | Default | Status | Notes |
|---|---|---|---|---|
| `write_xyz` | bool | `true` | ✅ | Final particle positions (XYZ format). Ground-truth state. |
| `write_xyzf` | bool | `false` | ✅ | Multi-frame trajectory (XYZF format). |
| `write_xyzfull` | bool | `false` | ✅ | Full state history. Doctrine: stores *what happened*, not inferred labels. |
| `write_pdb` | bool | `false` | 🔶 | PDB format for external viewers (VESTA, VMD). Parser wired; writer pending. |

### Analysis layer

| Field | Type | Default | Status | Notes |
|---|---|---|---|---|
| `write_analysis_json` | bool | `false` | ✅ | Derived metrics (AnalysisRecord). |
| `write_metrics_tsv` | bool | `false` | ✅ | Tab-separated per-run metric table. |
| `write_cluster_json` | bool | `false` | 🔶 | ClusterRecord assignments. |
| `write_fingerprint_json` | bool | `false` | 🔶 | FingerprintRecord feature vectors. |

### Kernel event spine

| Field | Type | Default | Status | Notes |
|---|---|---|---|---|
| `write_events_json` | bool | `false` | ✅ | KernelEventLog (JSON Lines — one event per line). |
| `write_symbolic_trace_json` | bool | `false` | ✅ | Symbolic equation trace per event. |

### Reporting layer

| Field | Type | Default | Status | Notes |
|---|---|---|---|---|
| `write_report_md` | bool | `false` | ✅ | Human-readable Markdown summary. |
| `write_summary_csv` | bool | `false` | 🔶 | Per-run summary CSV. |
| `write_dashboard_json` | bool | `false` | ✅ | DashboardRecord (beta-7 pipeline). |
| `write_manifest_json` | bool | `false` | ✅ | Run manifest with artifact registry. |
| `write_dashboard_svg` | bool | `false` | ✅ | Pipeline dashboard (SVG — text, diffable). |
| `write_pipeline_audit_jsonl` | bool | `false` | ✅ | Stage-by-stage audit JSONL. |
| `write_actual_hashes_tsv` | bool | `false` | ✅ | Golden suite: captured actual hashes. |

### Engineering geometry

| Field | Type | Default | Status | Notes |
|---|---|---|---|---|
| `write_step_file` | bool | `false` | ✅ | STEP geometry sidecar (ISO 10303-21 point cloud). Engineering truth — not analysis. |
| `write_vtp_mesh` | bool | `false` | ❌ | VTK PolyData mesh for ParaView. Not yet implemented. |

### Common

| Field | Type | Default | Status | Notes |
|---|---|---|---|---|
| `output_dir` | string | `""` | ✅ | Output directory. Empty = `out/<project.name>/`. |

```toml
[export]
write_xyz           = true
write_xyzf          = true
write_analysis_json = true
write_report_md     = true
write_dashboard_svg = true
write_manifest_json = true
output_dir          = "out/my_run"
```

---

## `[export.visual]`

**Struct:** `ExportVisualSection`  
Rendered visual artifact outputs. These are sidecar files rendered FROM simulation data. They are not ground-truth state.

### Static figures

| Field | Type | Default | Status | Notes |
|---|---|---|---|---|
| `write_svg_figures` | bool | `false` | ✅ | Per-material SVG metric figures. |
| `write_png_snapshots` | bool | `false` | 🔶 | PNG molecular snapshot (requires GL or OSMesa). |
| `write_rdf_svg` | bool | `false` | ✅ | Radial distribution function plot (SVG). |
| `write_energy_trace_svg` | bool | `false` | ✅ | Energy-per-step trace figure (SVG). |
| `write_packing_heatmap_svg` | bool | `false` | ✅ | 2D packing fraction heatmap (SVG). |
| `write_defect_map_svg` | bool | `false` | ✅ | Defect site map overlay (SVG). |
| `write_cluster_map_svg` | bool | `false` | ✅ | Cluster assignment scatter (SVG). |

### Animated exports

| Field | Type | Default | Status | Notes |
|---|---|---|---|---|
| `write_trajectory_gif` | bool | `false` | 🔶 | Animated GIF of trajectory playback. |
| `write_overlay_cycle_gif` | bool | `false` | 🔶 | Animated GIF of overlay cycle. |
| `gif_frame_skip` | int | `10` | 🔶 | Emit every Nth frame into GIF. |
| `gif_delay_cs` | int | `8` | 🔶 | GIF frame delay (centiseconds). |

### Web / streaming

| Field | Type | Default | Status | Notes |
|---|---|---|---|---|
| `write_html_dashboard` | bool | `false` | ✅ | Self-contained HTML dashboard. |
| `write_webgl_bundle` | bool | `false` | 🔶 | WebGL viewer bundle. |
| `write_sse_descriptor` | bool | `false` | 🔶 | SSE stream config for live viewer. |
| `sse_port` | int | `99998` | 🔶 | Port for SSE / HTTP server. |

### Composite documents

| Field | Type | Default | Status | Notes |
|---|---|---|---|---|
| `write_report_pdf` | bool | `false` | ❌ | PDF report (requires LaTeX / pandoc). |
| `write_report_html` | bool | `false` | ✅ | Standalone HTML report. |
| `visual_output_dir` | string | `""` | ✅ | Subdirectory for visual artifacts. Empty = `figures/`. |

---

## `[visual]`

**Struct:** `VisualSection`  
Controls interactive display during and after the simulation run. Separated from `[export.visual]` which controls rendered file artifacts.

### Primary mode

| Field | Type | Default | Status | Notes |
|---|---|---|---|---|
| `output_type` | string | `"none"` | ✅ | See output type catalog below. |
| `animation_mode` | string | `"none"` | ✅ | `"none"`, `"spark"`, `"bar"`, `"overlay"`. Terminal paths only. |
| `render_interval` | int | `1` | ✅ | **Emit a render / export frame every N simulation steps.** Orthogonal to `display_fps`. `0` treated as `1`. |

#### `output_type` values

| Value | Requires | Notes |
|---|---|---|
| `"none"` | — | Silent; no display. |
| `"terminal_chart"` | — | Live per-step convergence trace + proxy table. |
| `"terminal_snapshot"` | — | Post-run energy / eta bar-chart. |
| `"terminal_overlay_cycle"` | — | Full 6-panel kernel_viz_demo layout. |
| `"terminal_rdf"` | — | ASCII radial distribution function. |
| `"terminal_energy_heatmap"` | — | 2D ASCII energy landscape heatmap. |
| `"terminal_defect_map"` | — | ASCII defect site map (grid projection). |
| `"terminal_phase_diagram"` | — | ASCII phase field snapshot. |
| `"gl_overlay_cycle"` | `BUILD_VISUALIZATION` | CGVizViewer overlay cycle. |
| `"gl_live_60fps"` | `BUILD_VISUALIZATION` | SeedBeadViewer 60 fps live view. |
| `"gl_crystal_grid"` | `BUILD_VISUALIZATION` | Crystal grid viewer. |
| `"gl_interactive"` | `BUILD_VISUALIZATION` | Full interactive viewer with ImGui. |
| `"web_dashboard"` | network | HTTP server with auto-updating HTML dashboard. |
| `"sse_stream"` | network | SSE event stream to external client. |
| `"webgl_viewer"` | network | WebGL streamer bundle. |

### Terminal display flags

| Field | Type | Default | Status | Notes |
|---|---|---|---|---|
| `show_proxy_table` | bool | `true` | ✅ | EnsembleProxy summary table. |
| `show_convergence_trace` | bool | `true` | ✅ | Live per-step trace row. |
| `show_steady_state_marker` | bool | `true` | ✅ | `"✓ CONVERGED at step N"` banner. |
| `show_snapshot_chart` | bool | `false` | ✅ | Post-run energy / eta bar-chart. |
| `show_event_timeline` | bool | `false` | ✅ | ASCII kernel event timeline ruler. |
| `show_bar_chart` | bool | `false` | ✅ | Per-kind event count bar chart. |
| `show_symbolic_trace` | bool | `false` | ✅ | Symbolic equation trace per event. |
| `show_animation_cues` | bool | `false` | ✅ | Declarative animation cue table. |
| `show_audit_table` | bool | `false` | ✅ | Full event audit table. |
| `show_rdf_plot` | bool | `false` | ✅ | ASCII radial distribution function. |
| `show_energy_heatmap` | bool | `false` | ✅ | 2D ASCII energy landscape projection. |
| `show_defect_map` | bool | `false` | ✅ | ASCII defect site grid projection. |
| `show_phase_field` | bool | `false` | ✅ | ASCII phase field snapshot. |

### GL options

| Field | Type | Default | Status | Notes |
|---|---|---|---|---|
| `gl_show_axes` | bool | `true` | ✅ | Show coordinate axes. |
| `gl_show_neighbours` | bool | `true` | ✅ | Show neighbour bonds. |
| `gl_overlay_hold_s` | float | `2.5` | ✅ | Seconds per overlay pane. |
| `gl_auto_orbit` | bool | `true` | ✅ | Orbit camera between overlays. |
| `gl_window_width` | int | `1280` | ✅ | GL window width (px). |
| `gl_window_height` | int | `800` | ✅ | GL window height (px). |
| `overlay_sequence` | list | `[density, coordination, memory, orient_order]` | ✅ | Overlay pane order. |

### Web options

| Field | Type | Default | Status | Notes |
|---|---|---|---|---|
| `web_port` | int | `99998` | ✅ | HTTP / SSE server port. |
| `web_auto_open` | bool | `false` | ✅ | Open browser tab automatically on launch. |

---

## `[visual.external]`

**Struct:** `VisualExternalSection`  
Optional override for external visual backends. Requests rendered output artifacts from the current simulation state without running additional physics.

| Field | Type | Default | Status | Notes |
|---|---|---|---|---|
| `enabled` | bool | `false` | ✅ | Activate external render path. |
| `render` | list / string | `[]` | ✅ | Ordered render requests. See values below. |
| `export_format` | string | `"auto"` | ✅ | `"svg"`, `"png"`, `"html"`, `"auto"`. |
| `export_frame_png` | bool | `false` | ✅ | Shorthand: write PNG snapshot of current frame. |
| `export_trajectory` | bool | `false` | 🔶 | Shorthand: write trajectory GIF. |
| `show_progress` | bool | `true` | ✅ | Print `[render]` lines to terminal. |
| `render_interval` | int | `1` | ✅ | Steps per external render frame. Overrides `[visual].render_interval`. `0` falls back to `[visual].render_interval`. |

#### `render` target values

| Value | Output |
|---|---|
| `"state_current"` | Current particle positions as SVG / PNG. |
| `"trajectory_last"` | Last N frames of trajectory. |
| `"energy_trace"` | Energy-per-step trace. |
| `"rdf"` | Radial distribution function. |
| `"defect_map"` | Defect site overlay. |
| `"cluster_scatter"` | Cluster assignment scatter. |
| `"packing_heatmap"` | Packing fraction heatmap. |
| `"overlay_cycle"` | Full overlay-cycle figure. |
| `"dashboard"` | HTML dashboard. |
| `"report"` | HTML report. |

---

## `[kernel]`

**Struct:** parsed into `raw_sections` — no dedicated struct yet.  
Enables the central kernel pass-through and event registry.

| Field | Type | Default | Status | Notes |
|---|---|---|---|---|
| `pass_through` | bool | `false` | 🔶 | Route all calculations through kernel spine. |
| `symbolic_trace` | bool | `false` | 🔶 | Emit symbolic equation trace per event. |
| `event_registry` | bool | `false` | 🔶 | Populate `KernelEventLog`. |
| `continual_reporting` | bool | `false` | 🔶 | Emit `ContinualReportEvent` per declared interval. |

### `[kernel.trace]`

| Field | Type | Default | Status | Notes |
|---|---|---|---|---|
| `formation_events` | bool | `false` | 🔶 | Trace `FormationEvent` entries. |
| `defect_events` | bool | `false` | 🔶 | Trace `DefectEvent` entries. |
| `transport_events` | bool | `false` | 🔶 | Trace `TransportEvent` entries. |
| `dynamic_energy` | bool | `false` | 🔶 | Trace dynamic energy events. |

---

## `[variance]`

**Struct:** `VarianceSection` / `VarianceProbe`  
Declares variance probes evaluated over the kernel event log.

Each probe is declared as: `probe_name = "field" "window" threshold`

| Sub-field | Type | Notes |
|---|---|---|
| `field` | string | What to measure: `"energy.total"`, `"displacement"`, `"eta"`, `"coordination"`, `"position.x/y/z"`, `"result"`. |
| `window` | string | `"all"`, `"last N"`, `"frames M..N"`. |
| `threshold` | double | Used by `[while]` guard. `0` = no guard. |
| `particle_group` | string | Optional: filter to named particle group. |
| `print_results` | bool | `true` = print computed values. |

```toml
[variance]
energy_var = "energy.total" "last 50" 0.01
disp_var   = "displacement" "all" 0.0
```

---

## `[N_evolution]`

**Struct:** `NEvolutionSection` / `NEvolutionProbe`  
Tracks population growth rate (ΔN/Δt) for named entity populations.

| Sub-field | Type | Notes |
|---|---|---|
| `target` | string | `"cluster_count"`, `"defect_count"`, `"particle_count"`, `"event_count"`, `"vapor"`, `"solid"`. |
| `window` | string | Same as variance window. |
| `where_type` | string | Optional phase filter: `"vapor"`, `"solid"`. |
| `threshold` | double | For `[while]` guard. |
| `print_results` | bool | `true` = print computed values. |

---

## `[while]`

**Struct:** `WhileSection` / `WhileGuard`  
Conditional simulation continuation. Evaluated by `VsimRuntime::run_while_guards()`.

| Field | Type | Default | Status | Notes |
|---|---|---|---|---|
| `name` | string | `""` | ✅ | Guard label (used in log output). |
| `condition` | string | `""` | ✅ | Condition expression. See syntax below. |
| `body_steps` | int | `100` | ✅ | FIRE steps to run per loop iteration. |
| `max_iters` | int | `20` | ✅ | Safety ceiling. Loop exits when hit — warning printed. |
| `measure` | list | `[]` | ✅ | Probe names to re-evaluate after each body execution. |
| `iter_delay_ms` | int | `200` | ✅ | UX pause between iterations (ms). |

#### Condition syntax

```
"variance <probe_name> > <value>"
"N_evolution <probe_name> > <value>"
"energy_drift > <value>"
"iteration < N"
```

Reporting cadence inside a `[while]` loop is controlled by `[export]` flags. There is no background autonomous reporter.

---

## `[batch]`

**Struct:** `BatchSection` / `BatchJob`  
Parameter sweeps and queued job sets. Executed by `VsimRuntime::run_batch()`.

| Field | Type | Default | Status | Notes |
|---|---|---|---|---|
| `print_plan` | bool | `true` | ✅ | Print the full batch plan before executing. |
| `abort_on_fail` | bool | `false` | ✅ | Stop entire batch on first invalid run. |

### `[[batch.job]]`

| Field | Type | Default | Status | Notes |
|---|---|---|---|---|
| `name` | string | `""` | ✅ | Job label. |
| `seed_count` | int | `1` | ✅ | Seeds per parameter combination. |
| `export_each` | bool | `false` | 🔶 | Export artifacts for every run. |
| `aggregate` | bool | `true` | 🔶 | Produce aggregate report across sweep. |
| `per_run_actions` | list | `[]` | ✅ | Actions after each run: `"analyze.variance"`, `"analyze.N_evolution"`, `"analyze.rmsd"`, `"export"`. |

#### Sweep parameters (`[batch.job.sweep]`)

Keys: `"lattice"`, `"defect"`, `"temperature"`, `"seed"`, `"count"`, `"formula"`.  
Values: space-separated or list.

---

## `[post_step]`

**Struct:** `PostStepSection`  
**WO-57E.** Script block executed after each simulation step.

| Field | Type | Default | Status | Notes |
|---|---|---|---|---|
| `enabled` | bool | `false` | ✅ | True when block is present. |
| `script_block` | string | `""` | ✅ | Newline-separated VSIM interpreter expressions. |

Available builtins inside `script_block`:

| Builtin | Notes |
|---|---|
| `pbc.distance(a, b)` | Minimum-image distance between two positions. |
| `pbc.wrap(v)` | Wrap XYZVec3 into `[0, L)`. |
| `particle.position(i)` | Position of particle `i` as XYZVec3 (1-indexed). |

```toml
[post_step]
enabled = true
script_block = """
d = pbc.distance(particle.position(1), particle.position(2))
"""
```

---

## `[report]`

**Struct:** parsed into `raw_sections` — no dedicated struct for the script `[report]` section yet.  
Controls human-readable report content.

| Field | Type | Default | Status | Notes |
|---|---|---|---|---|
| `title` | string | `""` | 🔶 | Report title. |
| `include_material_cards` | bool | `false` | 🔶 | Include per-material property cards. |
| `include_metric_tables` | bool | `false` | 🔶 | Include quantitative metric tables. |
| `include_expected_trends` | bool | `false` | 🔶 | Include expected trend annotations. |
| `include_symbolic_trace` | bool | `false` | 🔶 | Include full symbolic equation trace. |

---

## Global variables and important constants

| Name | Location | Value | Notes |
|---|---|---|---|
| `default_seed_base` | `ProjectSection` | `0` | Base RNG seed when not specified. |
| `default_fire_max_steps` | `SimulationSection` | `500` | Default FIRE step limit. |
| `default_fire_dt_fs` | `SimulationSection` | `1.0` | Default FIRE timestep (fs). |
| `default_box_size_ang` | `SimulationSection` | `50.0` | Default cubic box edge (Å). |
| `default_ewald_alpha` | `SimulationSection` | `0.3` | Ewald splitting (Å⁻¹). |
| `default_ewald_rcut` | `SimulationSection` | `10.0` | Ewald real-space cutoff (Å). |
| `default_ewald_kmax` | `SimulationSection` | `5` | Ewald k-vector range. |
| `default_render_interval` | `VisualSection` | `1` | Every simulation step. `0` treated as `1`. |
| `default_gl_overlay_hold_s` | `VisualSection` | `2.5` | Seconds per GL overlay pane. |
| `default_gl_window_width` | `VisualSection` | `1280` | GL window width (px). |
| `default_gl_window_height` | `VisualSection` | `800` | GL window height (px). |
| `default_web_port` | `VisualSection` | `99998` | HTTP / SSE port. |
| `default_gif_frame_skip` | `ExportVisualSection` | `10` | GIF: emit every Nth frame. |
| `default_gif_delay_cs` | `ExportVisualSection` | `8` | GIF: frame delay (centiseconds). |
| `default_resim_delay_ms` | `SimulationSection` | `400` | Pause before resimulation (ms). |
| `default_while_body_steps` | `WhileGuard` | `100` | FIRE steps per while-loop iteration. |
| `default_while_max_iters` | `WhileGuard` | `20` | While-loop safety ceiling. |
| `default_iter_delay_ms` | `WhileGuard` | `200` | UX pause between iterations (ms). |
| `WrapMode::AfterStep` | `PBCSection` | — | Default position wrap timing. |
| `BoundarySection::x/y/z` | `BoundarySection` | `"open"` | Default boundary per axis. |

---

________________________________________

## `[material]` — WO-VSIM-03B

Declares material identity and structural intent. Replaces manual `[simulation.molecule]` for intent-driven authoring.

| Key | Type | Default | Description |
|---|---|---|---|
| `formula` | string | `""` | Chemical formula: `"NaCl"`, `"Si"`, `"Fe2O3"` |
| `prototype` | string | `""` | Deterministic generator key: `"B1_NaCl"`, `"A4_Si"` |
| `structure` | string | `""` | Casual alias — auto-resolved to prototype (see alias table below) |
| `space_group` | string | `""` | Crystallographic space group: `"Fm-3m"`, `"Fd-3m"` |
| `lattice` | string | `""` | Lattice type hint: `"fcc_ionic"`, `"bcc"`, `"hexagonal"` |
| `basis` | string | `""` | Atomic basis: `"Na:0,0,0; Cl:0.5,0.5,0.5"` |
| `cell` | string | `""` | Supercell spec: `"4x4x4"`, `"2x2x1"` |
| `phase` | string | `""` | `"solid"`, `"liquid"`, `"gas"`, `"amorphous"` |

**Resolution hierarchy** (highest wins): explicit `basis` + `space_group` → `prototype` → `structure` alias.

**Structure alias map** (`structure` key → resolved `prototype`):

_Ionic / salts:_

| Alias | Resolved prototype |
|---|---|
| `"rocksalt"`, `"halite"`, `"nacl"` | `"B1_NaCl"` |
| `"cesium_chloride"`, `"cscl"` | `"B2_CsCl"` |
| `"fluorite"` | `"C1_CaF2"` |
| `"antifluorite"` | `"Anti_C1_Li2O"` |
| `"zincblende"`, `"sphalerite"`, `"zinc_blende"` | `"B3_ZnS"` |
| `"wurtzite"` | `"B4_ZnS"` |
| `"rutile"` | `"C4_TiO2"` |
| `"perovskite"` | `"ABO3_perovskite"` |
| `"spinel"` | `"AB2O4_spinel"` |

_Elemental metals / simple crystals:_

| Alias | Resolved prototype |
|---|---|
| `"simple_cubic"`, `"sc"` | `"A_cP1"` |
| `"bcc"`, `"body_centered_cubic"` | `"A2_bcc"` |
| `"fcc"`, `"face_centered_cubic"` | `"A1_fcc"` |
| `"hcp"`, `"hexagonal_close_packed"` | `"A3_hcp"` |
| `"diamond"`, `"diamond_cubic"`, `"silicon"`, `"germanium"` | `"A4_diamond"` |
| `"graphite"` | `"A9_graphite"` |
| `"graphene"` | `"A9_graphene_2D"` |

_Covalent / semiconductor:_

| Alias | Resolved prototype |
|---|---|
| `"zinc_sulfide"` | `"B3_ZnS"` |
| `"cadmium_sulfide"` | `"B4_CdS"` |

_Oxides / ceramics:_

| Alias | Resolved prototype |
|---|---|
| `"alpha_alumina"`, `"corundum"` | `"D5_Al2O3_corundum"` |
| `"magnesia"` | `"B1_MgO"` |
| `"ceria"` | `"C1_CeO2_fluorite"` |
| `"zirconia"` | `"C1_ZrO2_fluorite_like"` |
| `"uraninite"` | `"C1_UO2_fluorite"` |
| `"thoria"` | `"C1_ThO2_fluorite"` |

_Molecular geometry:_

| Alias | Resolved prototype |
|---|---|
| `"linear"` | `"geom_linear"` |
| `"bent"` | `"geom_bent"` |
| `"trigonal_planar"` | `"geom_trigonal_planar"` |
| `"tetrahedral"` | `"geom_tetrahedral"` |
| `"trigonal_pyramidal"` | `"geom_trigonal_pyramidal"` |
| `"octahedral"` | `"geom_octahedral"` |
| `"square_planar"` | `"geom_square_planar"` |
| `"see_saw"` | `"geom_seesaw"` |
| `"t_shaped"` | `"geom_t_shaped"` |

_Polymers / organics:_

| Alias | Resolved prototype |
|---|---|
| `"linear_chain"` | `"polymer_linear_chain"` |
| `"branched_chain"` | `"polymer_branched"` |
| `"aromatic_ring"` | `"organic_aromatic_ring"` |
| `"benzene_ring"` | `"organic_benzene"` |
| `"alkane_chain"` | `"organic_alkane_chain"` |
| `"cycloalkane"` | `"organic_cycloalkane"` |

_Porous / framework materials:_

| Alias | Resolved prototype |
|---|---|
| `"zeolite"` | `"framework_zeolite"` |
| `"mof"` | `"framework_mof"` |
| `"cof"` | `"framework_cof"` |
| `"pba"` | `"framework_prussian_blue_analog"` |
| `"prussian_blue"` | `"framework_prussian_blue"` |

_Bead / premacro:_

| Alias | Resolved prototype |
|---|---|
| `"bead_chain"` | `"bead_linear_chain"` |
| `"bead_cluster"` | `"bead_cluster_random"` |
| `"powder_bed"` | `"premacro_powder_bed"` |
| `"packed_bed"` | `"premacro_packed_bed"` |
| `"granular_column"` | `"premacro_granular_column"` |
| `"fiber_bundle"` | `"premacro_fiber_bundle"` |
| `"pipe_flow"` | `"premacro_pipe_flow"` |

_Anything not in this table is passed through verbatim (forward-compatible)._


________________________________________

## `[run]` — WO-VSIM-03B

Declares the run mode and top-level execution controls.

| Key | Type | Default | Description |
|---|---|---|---|
| `mode` | string | `""` | Required: `"relax"`, `"md"`, `"npt"`, `"nvt"`, `"nve"`, `"scan"`, `"single_point"` |
| `max_steps` | int | `500` | Step / iteration limit |
| `dt_fs` | float | `1.0` | Timestep in femtoseconds (ignored for `"relax"`) |
| `temperature` / `temperature_K` | float | `300.0` | K |
| `pressure` / `pressure_GPa` | float | `0.0` | GPa (for NPT) |
| `converge` | bool | `true` | Stop early on convergence criterion |
| `output_level` | string | `"standard"` | `"minimal"`, `"standard"`, `"verbose"` |

________________________________________

## `[environment]` — WO-VSIM-03B

Describes the physical environment surrounding the simulation cell.

| Key | Type | Default | Description |
|---|---|---|---|
| `periodic` | bool | `false` | Enable periodic boundary conditions |
| `temperature` | float | `300.0` | K |
| `pressure` | float | `0.0` | GPa |
| `medium` | string | `""` | `"vacuum"`, `"water"`, `"argon_gas"`, … |
| `humidity` | float | `0.0` | 0–1 fraction |
| `field_x` / `field_y` / `field_z` | float | `0.0` | External electric field components (V/Å) |

________________________________________

## `[excite.<type>]` — WO-VSIM-03B

Named excitation subsection. `<type>` is the excitation kind (e.g., `laser`, `xray`, `electron_beam`, `thermal_spike`). Multiple `[excite.*]` blocks may appear; each is stored in `ExciteSection::entries` keyed by type name.

| Key | Type | Default | Description |
|---|---|---|---|
| `axis` | string | `""` | Propagation axis: `"x"`, `"y"`, `"z"` |
| `polarization` | string | `""` | `"x"`, `"y"`, `"z"`, `"circular"` |
| `intensity` | float | `1.0` | Arbitrary units (type-dependent) |
| `pulse_width_fs` | float | `100.0` | Pulse duration (fs) |
| `photon_energy_eV` | float | `0.0` | Photon energy for xray / e-beam |
| `fluence` | float | `0.0` | J/cm² |
| `profile` | string | `""` | `"gaussian"`, `"flat"`, `"sech2"` |

________________________________________

## `[observe]` — WO-VSIM-03B

Declares which physical observables to measure and how to emit them.

| Key | Type | Default | Description |
|---|---|---|---|
| `metrics` | list of strings | `[]` | e.g. `["energy_map", "interference", "spectral_response"]` |
| `output_format` | string | `"auto"` | `"csv"`, `"json"`, `"svg"`, `"auto"` |
| `every_n_steps` | int | `1` | Observation cadence (steps) |

________________________________________

## `[[override.particle]]` — WO-VSIM-03B

Array-of-tables. Each block selectively mutates one particle before or during a run. Double-bracket `[[…]]` syntax; multiple blocks allowed.

| Key | Type | Default | Description |
|---|---|---|---|
| `id` | int | `-1` | 1-indexed particle ID (required) |
| `velocity` | `[x, y, z]` | — | Override velocity components (Å/fs) |
| `position` | `[x, y, z]` | — | Override position (Å) |
| `charge` | float | `0.0` | Override charge (e) |
| `mass_scale` | float | `1.0` | Multiplicative mass modifier |
| `fixed` | bool | `false` | Freeze particle position |

________________________________________

## `[[raw.object]]` — WO-VSIM-03B

Array-of-tables. Explicit particle injection for tests, importers, file bridges, and debugging. **Not the main experience.** Double-bracket `[[…]]` syntax; multiple blocks allowed.

| Key | Type | Default | Description |
|---|---|---|---|
| `id` | string | `""` | Arbitrary label: `"debug_particle_001"` |
| `species` | string | `""` | Element symbol or reserved label: `"C"`, `"alpha"`, `"ghost"` |
| `position` | `[x, y, z]` | `[0,0,0]` | Position (Å) |
| `velocity` | `[x, y, z]` | `[0,0,0]` | Velocity (Å/fs) |
| `charge` | float | `0.0` | Charge (e) |
| `mass` | float | `0.0` | Mass (amu); 0 = derive from species |
| `label` | string | `""` | Optional display label |

________________________________________

## Planned features — no kernel wiring yet

The following are parsed and stored (or defined in the schema) but have no runtime effect in the current build.

| Feature | Section | Status |
|---|---|---|
| `description` in `[project]` | `ProjectSection::description` | 🔶 Stored, not emitted to reports |
| `write_pdb` | `ExportSection` | 🔶 Parsed; PDB writer not implemented |
| `write_cluster_json` | `ExportSection` | 🔶 Parsed; ClusterRecord writer pending |
| `write_fingerprint_json` | `ExportSection` | 🔶 Parsed; FingerprintRecord writer pending |
| `write_summary_csv` | `ExportSection` | 🔶 Parsed; CSV writer pending |
| `write_vtp_mesh` | `ExportSection` | ❌ Not yet parsed or implemented |
| `write_png_snapshots` | `ExportVisualSection` | 🔶 Parsed; requires GL or OSMesa |
| `write_trajectory_gif` | `ExportVisualSection` | 🔶 Parsed; requires gifenc or ffmpeg |
| `write_overlay_cycle_gif` | `ExportVisualSection` | 🔶 Parsed; requires gifenc or ffmpeg |
| `write_webgl_bundle` | `ExportVisualSection` | 🔶 Parsed; webgl_streamer not wired |
| `write_sse_descriptor` | `ExportVisualSection` | 🔶 Parsed; vsepr_live not wired |
| `write_report_pdf` | `ExportVisualSection` | ❌ Not implemented; requires LaTeX / pandoc |
| `export_trajectory` in `[visual.external]` | `VisualExternalSection` | 🔶 Parsed; GIF writer not wired |
| `render_targets` dispatch | `VisualExternalSection` | 🔶 Parsed; full dispatch table pending |
| `[kernel]` and `[kernel.trace]` | `raw_sections` | 🔶 No dedicated struct; all stored in raw |
| `[report]` script section | `raw_sections` | 🔶 No dedicated struct; stored in raw |
| `export_each` in `[batch.job]` | `BatchJob` | 🔶 Parsed; per-run export not enforced |
| `aggregate` in `[batch.job]` | `BatchJob` | 🔶 Parsed; aggregate report not implemented |
| `[material.*]` sub-sections (demo_07 style) | `raw_sections` | 🔶 Captured; runtime generation wiring pending (WO-VSIM-03B schema done) |
| `[sweep]` top-level declarative sweep | `raw_sections` | 🔶 Parsed; runtime wiring pending |
| `"triclinic"` cell type | `CellSection` | ❌ Reserved; not implemented |
| `"reflective"` / `"absorbing"` boundary | `BoundarySection` | ❌ Reserved; not implemented |
| `"nm"` cell units | `CellSection` | ❌ Reserved; not implemented |
| STEP B-Rep solid geometry | `ExportSection::write_step_file` | 🔶 Point-cloud only; B-Rep deferred to beta-8.1 |
| PNG raster dashboard | pipeline | 🔶 stb fallback active; PPM if stb not linked |
| Live continual reporting | — | ❌ Deprecated as autonomous engine. Use `[while]` + `[export]`. |

---

## Beta-9 owned items (registry resolution — not yet implemented)

The following are schema/parser-complete but require the **beta-9 registry resolution engine** (WO-VSIM-03C) before they have runtime effect.

| Feature | Schema / parser | Runtime |
|---|---|---|
| `structure` alias → full prototype expansion | ✅ `resolve_structure_alias()` | ⬜ beta-9 `RegistryResolver` |
| `[material]` → particle positions + masses + charges | ✅ `MaterialSection` parsed | ⬜ beta-9 generator wiring |
| `[run]` → simulation execution | ✅ `RunSection` parsed | ⬜ beta-9 runtime bridge |
| `[environment]` → boundary / PBC / temperature | ✅ `EnvironmentSection` parsed | ⬜ beta-9 runtime bridge |
| `[excite.*]` → excitation dispatch | ✅ `ExciteSection` parsed | ⬜ beta-9 |
| `[observe]` → metric collection | ✅ `ObserveSection` parsed | ⬜ beta-9 |
| `[[override.particle]]` → particle mutation | ✅ `ParticleOverrideEntry` parsed | ⬜ beta-9 |
| `[[raw.object]]` → explicit particle injection | ✅ `RawObjectEntry` parsed | ⬜ beta-9 |
| `[REGISTRY]` field resolution logging | — | ⬜ beta-9 |
