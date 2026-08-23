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
| * | Parsed, stored, not yet wired |
| ❌ | Not yet parsed (planned) |

---

## `[project]`

**Struct:** `ProjectSection`  
**Required.** Identifies the script.

| Field | Type | Default | Status | Notes |
|---|---|---|---|---|
| `name` | string | `""` | ✅ | Required. Used in report filenames and dashboard labels. |
| `version` | string | `""` | ✅ | Informational. Stored, not validated against runtime version. |
| `seed_base` | uint64 | `0` | ✅ | Base RNG seed (low 64 bits). Same seed + same script = same run. |
| `world_seed` | Seed256 | `0` (unset) | ✅ | 256-bit world constant for dual-seed mode. See `[seed]` section. |
| `determinism` | bool | `true` | ✅ | Always true in current implementation. Field kept for schema completeness. |
| `description` | string | `""` | * | Stored in `ProjectSection::description`. Not emitted to reports yet. |

```toml
[project]
name        = "demo_03_graphite_stack"
version     = "v5.0.14"
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
| `write_pdb` | bool | `false` | * | PDB format for external viewers (VESTA, VMD). Parser wired; writer pending. |

### Analysis layer

| Field | Type | Default | Status | Notes |
|---|---|---|---|---|
| `write_analysis_json` | bool | `false` | ✅ | Derived metrics (AnalysisRecord). |
| `write_metrics_tsv` | bool | `false` | ✅ | Tab-separated per-run metric table. |
| `write_cluster_json` | bool | `false` | * | ClusterRecord assignments. |
| `write_fingerprint_json` | bool | `false` | * | FingerprintRecord feature vectors. |

### Kernel event spine

| Field | Type | Default | Status | Notes |
|---|---|---|---|---|
| `write_events_json` | bool | `false` | ✅ | KernelEventLog (JSON Lines — one event per line). |
| `write_symbolic_trace_json` | bool | `false` | ✅ | Symbolic equation trace per event. |

### Reporting layer

| Field | Type | Default | Status | Notes |
|---|---|---|---|---|
| `write_report_md` | bool | `false` | ✅ | Human-readable Markdown summary. |
| `write_summary_csv` | bool | `false` | * | Per-run summary CSV. |
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
| `write_png_snapshots` | bool | `false` | * | PNG molecular snapshot (requires GL or OSMesa). |
| `write_rdf_svg` | bool | `false` | ✅ | Radial distribution function plot (SVG). |
| `write_energy_trace_svg` | bool | `false` | ✅ | Energy-per-step trace figure (SVG). |
| `write_packing_heatmap_svg` | bool | `false` | ✅ | 2D packing fraction heatmap (SVG). |
| `write_defect_map_svg` | bool | `false` | ✅ | Defect site map overlay (SVG). |
| `write_cluster_map_svg` | bool | `false` | ✅ | Cluster assignment scatter (SVG). |

### Animated exports

| Field | Type | Default | Status | Notes |
|---|---|---|---|---|
| `write_trajectory_gif` | bool | `false` | * | Animated GIF of trajectory playback. |
| `write_overlay_cycle_gif` | bool | `false` | * | Animated GIF of overlay cycle. |
| `gif_frame_skip` | int | `10` | * | Emit every Nth frame into GIF. |
| `gif_delay_cs` | int | `8` | * | GIF frame delay (centiseconds). |

### Web / streaming

| Field | Type | Default | Status | Notes |
|---|---|---|---|---|
| `write_html_dashboard` | bool | `false` | ✅ | Self-contained HTML dashboard. |
| `write_webgl_bundle` | bool | `false` | * | WebGL viewer bundle. |
| `write_sse_descriptor` | bool | `false` | * | SSE stream config for live viewer. |
| `sse_port` | int | `99998` | * | Port for SSE / HTTP server. |
| `show_bond_graph` | bool | `false` | ✅ | Open bond graph viewer (`/graph` via `viz_web.py`). |
| `bond_graph_port` | int | `8899` | ✅ | `viz_web.py` HTTP port for the `/graph` route. |

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
| `live_switch` | bool | `false` | ✅ | **Live-switch feed.** Derived from the element carousel pattern. When `true`, the viewer window is refreshed in-place (cursor-up overwrite / GL content swap / SSE data push) instead of being torn down and reopened between simulation phases or data-source changes. Eliminates flash and preserves scroll context. |

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
| `gl_spin` | bool | `false` | ✅ | Continuously spin the scene around `gl_spin_axis` at `gl_spin_deg_per_s`. Overrides `gl_auto_orbit` when `true`. |
| `gl_spin_axis` | string | `"y"` | ✅ | Rotation axis for scene spin. Accepts `"x"`, `"y"`, or `"z"`. |
| `gl_spin_deg_per_s` | float | `30.0` | ✅ | Spin rate in degrees per second. Negative values reverse direction. |
| `gl_window_width` | int | `1280` | ✅ | GL window width (px). |
| `gl_window_height` | int | `800` | ✅ | GL window height (px). |
| `overlay_sequence` | list | `[density, coordination, memory, orient_order]` | ✅ | Overlay pane order. |

### Internal / hidden renderer options

> **Note:** These fields are intentionally absent from user-facing quick-start guides. They are documented here for renderer developers and power users. They have **no effect on physics, formation, or analysis correctness**.

| Field | Type | Default | Status | Notes |
|---|---|---|---|---|
| `shadow_type` | int | `0` | ✅ | **Depth-shading model** for ASCII and GL terminal renderers. `0` = off (flat, legacy). `1` = `ambient_soft` — soft ambient-occlusion gradient (`shade = 0.30 + 0.70 * sat((z+depth)/range)`); good for small molecules. `2` = `depth_fade` — linear perspective cue (`shade = 1.0 - 0.55 * sat((maxZ-z)/range)`); good for crystals. `3` = `contact` — proximity darkening (darkens atoms close to neighbours). |

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
| `export_trajectory` | bool | `false` | * | Shorthand: write trajectory GIF. |
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

## `[visual.workspace]` — WO-VSIM-VIS-OVERHAUL-01

**Struct:** `VisualWorkspaceSection`  
Controls workspace host behavior when a script is opened in the `vsper workspace` Qt application. Silently ignored in headless / pure CLI runs — no parse error, no warnings.

| Field | Type | Default | Status | Notes |
|---|---|---|---|---|
| `enabled` | bool | `false` | ✅ | Auto-open declared `show` windows when the script is loaded in the workspace. |
| `default_layout` | string | `"tabs"` | ✅ | `"tabs"` (windows as tabs in the central area) or `"detached"` (separate top-level windows). |
| `auto_open_tree` | bool | `false` | ✅ | Expand the object tree to the active run on load. |
| `live` | bool | `false` | ⏳ | Subscribe summoned windows to live kernel updates (stretch goal, v1 is snapshot). |

### `show` directive

Top-level statement; any number of occurrences allowed at script root or inside `[visual.workspace]`.

```vsim
show "<kind>" target = "<dot.path>" [options = { ... }]
```

Parsed into `VsimDocument::view_directives` as a `ViewDirective`. Unknown kinds produce a parse error with a list of valid kinds in the diagnostic. `options` is an optional JSON object of per-kind knobs (camera, colormap, subsample cap, etc.). In headless runs or when `[visual.workspace] enabled = false`, the directives are no-ops (no error, no warning).

#### Known view kinds (v1)

| Category | Kind | Source target | Notes |
|---|---|---|---|
| 3D scene | `scene.molecule` | `material.<i>`, `run.final_state` | Ball-and-stick. |
| 3D scene | `scene.cg_bead` | `run.cg_state` | Wraps CGVizViewer / SeedBeadViewer. |
| 3D scene | `scene.crystal_grid` | `material.<i>` when periodic | |
| 3D scene | `scene.trajectory` | `run.history` | Requires `write_xyzf` or `write_xyzfull`. |
| 3D scene | `overlay.cycle` | `run.history` + overlay fields | |
| Calibration | `calibration.helix` | `run.history` | Helix+bars+markers; visual constants ported from `demo_calibration_3d.py`. |
| Environment | `room.heatfield` | `room.solver` | Heat dust cloud; ported from `demo_helium_room_3d.py`. |
| Data | `data.history.table` | `run.history` | Sortable QTableView. |
| Data | `data.history.timeseries` | `run.history` numeric columns | QtCharts multi-series. |
| Data | `data.scalar.panel` | any scalar node | Read-only labeled values. |
| Data | `data.events.table` | `kernel.events` | Filterable by event kind. |
| Data | `data.events.timeline` | `kernel.events` | Horizontal ruler colored by kind. |
| Data | `data.events.bar_chart` | `kernel.events` | Per-kind count bar chart. |
| Data | `data.rdf` | `analysis.rdf` | |
| Data | `data.energy_trace` | `run.history.energy` | |
| Data | `data.defect_map` | `analysis.defects` | |
| Data | `data.cluster_scatter` | `analysis.clusters` | |
| Data | `data.batch.plan` | resolved `[batch.*]` plan | |
| Data | `data.batch.ranked` | `ranked_candidates.tsv` | |
| Data | `data.verify.matrix` | `failure_mode_matrix.tsv` | |

---

## `[room]` — WO-VSIM-VIS-OVERHAUL-01

**Struct:** `RoomSection`  
Drives the room heat-field simulation and exposes `room.solver` as a viewable node (target for `room.heatfield` and `data.scalar.panel`).  
Corresponds to the physics formerly exercised by `scripts/demos/demo_helium_room_3d.py` (now archived).

| Field | Type | Default | Status | Notes |
|---|---|---|---|---|
| `preset` | string | `""` | ✅ | `"ambient"` \| `"cold"` \| `"hot"` \| `"lab"` \| `"reactor"` \| `"large_volume"`. Empty = inactive. |
| `n_steps` | int | `200` | ✅ | Solver steps before snapshot. |
| `record_interval` | int | `50` | ✅ | Steps between recorded snapshots (exposed to `room.solver`). |

---

## `[kernel]`

**Struct:** parsed into `raw_sections` — no dedicated struct yet.  
Enables the central kernel pass-through and event registry.

| Field | Type | Default | Status | Notes |
|---|---|---|---|---|
| `pass_through` | bool | `false` | * | Route all calculations through kernel spine. |
| `symbolic_trace` | bool | `false` | * | Emit symbolic equation trace per event. |
| `event_registry` | bool | `false` | * | Populate `KernelEventLog`. |
| `continual_reporting` | bool | `false` | * | Emit `ContinualReportEvent` per declared interval. |

### `[kernel.trace]`

| Field | Type | Default | Status | Notes |
|---|---|---|---|---|
| `formation_events` | bool | `false` | * | Trace `FormationEvent` entries. |
| `defect_events` | bool | `false` | * | Trace `DefectEvent` entries. |
| `transport_events` | bool | `false` | * | Trace `TransportEvent` entries. |
| `dynamic_energy` | bool | `false` | * | Trace dynamic energy events. |

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
| `export_each` | bool | `false` | * | Export artifacts for every run. |
| `aggregate` | bool | `true` | * | Produce aggregate report across sweep. |
| `per_run_actions` | list | `[]` | ✅ | Actions after each run: `"analyze.variance"`, `"analyze.N_evolution"`, `"analyze.rmsd"`, `"export"`. |

#### Sweep parameters (`[batch.job.sweep]`)

Keys: `"lattice"`, `"defect"`, `"temperature"`, `"seed"`, `"count"`, `"formula"`.  
Values: space-separated or list.

---

## Batch Manifest Runner — `batch_manifest.json`

**WO-B9-001.** File-system-aware manifest runner that expands parameter sweeps,
creates isolated per-run folders, and emits ranked summary artefacts.

**Structs:** `BatchManifestSection`, `BatchManifestSweepAxis`, `BatchRunRecord`  
**Loader:** `src/batch/manifest_loader.hpp` / `.cpp`  
**Runner:** `src/batch/manifest_runner.hpp` / `.cpp`  
**CLI:** `apps/batch_runner.cpp`

### Manifest schema (`batch_manifest.json`)

```json
{
  "batch_id":    "B9_REACTOR_CHANNEL_001",
  "description": "Human-readable description",
  "base_vsim":   "scripts/reactor_channel_steady.vsim",
  "sweep": [
    { "param": "temperature_K", "values": ["300", "600", "900"] },
    { "param": "pressure_GPa",  "values": ["0.0", "1.0"] }
  ],
  "seeds":              3,
  "score_by":           "composite",
  "output_root":        "runs",
  "abort_on_fail":      false,
  "write_per_run_meta":    true,
  "write_per_run_metrics": true
}
```

### `BatchManifestSection` fields

| Field | Type | Default | Notes |
|---|---|---|---|
| `batch_id` | string | `""` | **Required.** Folder name under `output_root/`. |
| `description` | string | `""` | Human-readable label (informational). |
| `base_vsim` | string | `""` | Source `.vsim` path (informational; not executed by runner). |
| `score_by` | string | `"composite"` | `"energy"` \| `"convergence"` \| `"composite"` |
| `output_root` | string | `"runs"` | Root directory for the output tree. |
| `seeds` | int | `1` | Seeds per parameter combination. |
| `abort_on_fail` | bool | `false` | Stop on first non-converged run. |
| `write_per_run_meta` | bool | `true` | Write `run_meta.json` in each run folder. |
| `write_per_run_metrics` | bool | `true` | Write `metrics.tsv` in each run folder. |
| `sweep` | array | `[]` | List of `{param, values}` sweep axes. Total runs = product × seeds. |

### `BatchRunRecord` fields

| Field | Type | Notes |
|---|---|---|
| `run_id` | string | e.g. `"run_0001"` |
| `run_index` | int | 1-based |
| `params` | map | Active sweep params for this run |
| `seed` | int | Seed index (0-based) |
| `converged` | bool | FIRE convergence outcome |
| `final_energy` | double | kcal/mol |
| `rms_force` | double | Convergence residual |
| `steps_taken` | int | |
| `wall_ms` | double | Wall time (ms) |
| `score_energy` | double | Soft-max score: lower energy = higher |
| `score_convergence` | double | Budget + quality composite |
| `score_composite` | double | 0.4·energy + 0.4·convergence + 0.2·steady |
| `steady_pass` | bool | Gate result (WO-B9-002; stub = converged) |
| `rank` | int | 1 = best composite score |
| `failure_reason` | string | Non-empty on gate failure |

### Scoring helpers (`batch_score` namespace)

| Function | Notes |
|---|---|
| `energy_score(E, ref)` | 1/(1 + exp(ΔE/100)) — lower energy → higher score |
| `convergence_score(steps, max, rms, tol)` | 0.4·budget + 0.6·quality |
| `composite(es, cs, steady)` | 0.4·es + 0.4·cs + 0.2·(steady?1:0) |

### Output tree

```
runs/
└── B9_REACTOR_CHANNEL_001/
    ├── run_0001/
    │   ├── run_meta.json       ← provenance (params, seed, energy, timing)
    │   └── metrics.tsv         ← per-step energy / force trace
    ├── run_0002/ ... run_NNNN/
    ├── batch_summary.tsv       ← all runs, execution order
    ├── ranked_candidates.tsv   ← sorted by score_composite (desc)
    └── batch_report.md         ← Markdown summary + gate notes
```

### CLI usage

```
batch_runner <manifest.json> [options]

Options:
  --dry-run          Parse + validate + print plan; no execution.
  --quiet            Suppress per-run progress lines.
  --abort-on-fail    Stop on first non-converged run.
  --max-steps N      Override FIRE step limit (default 500).
  --output-root PATH Override manifest output_root field.
```

Exit codes: `0` = complete, `1` = manifest error, `2` = no results (abort triggered).

### Gate placeholders (WO-B9-002)

`steady_pass` is set to `converged` in the WO-B9-001 stub.
WO-B9-002 will wire real gates:

| Gate | Field | Notes |
|---|---|---|
| `energy_slope_gate` | slope of energy trace in final window | |
| `flux_balance_gate` | inward vs outward flux ratio | |
| `residence_time_stability_gate` | CV of per-species residence times | |
| `energy_drift_gate` | drift rate over final N steps | |
| `wall_residence_gate` | fraction of time at wall boundary | |

### Discovery ranking (WO-B9-003)

Will add `candidate_id`, `valid_energy`, `steady_pass`, `failure_reason` filter pass to
`ranked_candidates.tsv`. Supports workflows: `discover-material`, `discover-coolant`,
`defect-survival`, `phase-regime-map`.

---



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
| `title` | string | `""` | * | Report title. |
| `include_material_cards` | bool | `false` | * | Include per-material property cards. |
| `include_metric_tables` | bool | `false` | * | Include quantitative metric tables. |
| `include_expected_trends` | bool | `false` | * | Include expected trend annotations. |
| `include_symbolic_trace` | bool | `false` | * | Include full symbolic equation trace. |

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

## `[chemistry]` / `[system]` — Ambient Reaction Physics

Declares ambient reaction rules for the simulation. Chemistry is **always evaluated** whenever two or more molecule species are present. This block controls which rule family is active, the heat-gate level, scoring thresholds, and event verbosity. `[system]` is a legacy alias for the same block.

| Key | Type | Default | Description |
|---|---|---|---|
| `chemistry` | string | `""` | Rule-family registry alias. See registry values below. |
| `heat` | int | `-1` | Heat-gate integer `[0–999]`. `-1` = derive from `environment.temperature`. |
| `reaction_events` | bool | `true` | Emit `ReactionEvent` into `KernelEventLog`. |
| `track_species_state` | bool | `true` | Emit `ChemicalStateEvent` per species change. |
| `event_registry` | bool | `true` | Build per-step event registry (enables `reaction_scan` observe metric). |
| `min_score_threshold` | float | `0.25` | `overall_score` must exceed this to emit an event. |
| `max_reactions_per_step` | int | `8` | Cap on reaction events per step. `0` = unlimited. |
| `domain` | string | `""` | Organic shorthand domain. `"peptide"` enables sequence expansion. When set, the parser expands `sequence` into a canonical Hill formula and injects it as `molecules[0]`. |
| `sequence` | string | `""` | Domain-specific shorthand input. For `domain = "peptide"`: one-letter amino acid codes (e.g. `"ACDEFG"`). For other domains: trivial name or condensed formula passed to `expand_organic_formula()`. |
| `expanded_formula` | string | *(read-only)* | Hill-order formula produced by the organic parser after expansion. Populated automatically; also back-fills `material.formula` if that field is not explicitly set. Do not set this key manually. |

**Chemistry registry aliases** (§VSIM_LANGUAGE_REFERENCE §chemistry):

| Alias | Activates |
|---|---|
| `"oxidation"` | Metal/non-metal oxidation templates |
| `"hydration"` | Water-addition and hydration templates |
| `"corrosion"` | Surface corrosion / electrochemical templates |
| `"pyrolysis"` | High-temperature bond-cleavage templates |
| `"reduction"` | Electron-transfer reduction templates |
| `"hydrolysis"` | Hydrolysis (acid/base) templates |
| `"polymerization"` | Chain-growth and step-growth templates |
| `"salt_exchange"` | Double-displacement templates |
| `"fluorination"` | Electrophilic fluorination templates |
| `"isomer_scan"` | Conformer / isomer search templates |
| `"none"` | No template family active; heat gate still applied |

**Heat gate calibration:**  `T ≤ 0 K → h = 0`,  `T ≥ 3000 K → h = 999`,  otherwise `h = round((T / 3000) × 999)`.

**Architecture note:**  All reaction events are written to `KernelEventLog`, never directly to `.xyz` / `.xyzFull`. Observe metrics `"reaction_events"`, `"chemical_state"`, `"exothermic_count"`, and `"avg_delta_E"` read from the same log.

**Example — ambient reaction chemistry:**

```vsim
[environment]
temperature = 800.0

[chemistry]
chemistry              = "oxidation"
heat                   = -1       # auto-derive from 800 K -> h ~266
reaction_events        = true
track_species_state    = true
min_score_threshold    = 0.30
max_reactions_per_step = 4

[observe]
metrics       = ["reaction_events", "exothermic_count", "avg_delta_E"]
every_n_steps = 10
```

**Example — organic domain (peptide + gas environment):**

```vsim
[chemistry]
domain   = peptide
sequence = ACDEFG         # expands -> C26H36N6O10S; injected as molecules[0]

[[simulation.molecule]]
formula = N2
count   = 12

[[simulation.molecule]]
formula = H2O
count   = 50
```

The parser resolves `sequence` using the 20-residue table (backbone-subtracted Hill formulas + H2O terminal cap), then prepends the result to `simulation.molecules` so the runner sees it as the primary species. Explicit `[[simulation.molecule]]` blocks are preserved in declaration order after it.

________________________________________

## `[observe]` — WO-VSIM-03B

Declares which physical observables to measure and how to emit them.

| Key | Type | Default | Description |
|---|---|---|---|
| `metrics` | list of strings | `[]` | e.g. `["energy_map", "interference", "spectral_response"]` |
| `output_format` | string | `"auto"` | `"csv"`, `"json"`, `"svg"`, `"auto"` |
| `every_n_steps` | int | `1` | Observation cadence (steps) |

**Reaction-aware metrics** (evaluated from `KernelEventLog`):

| Metric name | Returns |
|---|---|
| `"reaction_events"` | Count of `Reaction` events in the log |
| `"chemical_state"` | Count of `ChemicalState` events |
| `"exothermic_count"` | Count of events where `delta_E < 0` |
| `"avg_delta_E"` | Mean `delta_E` (kcal/mol) over all reaction events |
| `"formation"` | Count of `Formation` events |
| `"transport"` | Count of `Transport` events |
| `"defect"` | Count of `Defect` events |

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
| `description` in `[project]` | `ProjectSection::description` | * Stored, not emitted to reports |
| `write_pdb` | `ExportSection` | * Parsed; PDB writer not implemented |
| `write_cluster_json` | `ExportSection` | * Parsed; ClusterRecord writer pending |
| `write_fingerprint_json` | `ExportSection` | * Parsed; FingerprintRecord writer pending |
| `write_summary_csv` | `ExportSection` | * Parsed; CSV writer pending |
| `write_vtp_mesh` | `ExportSection` | ❌ Not yet parsed or implemented |
| `write_png_snapshots` | `ExportVisualSection` | * Parsed; requires GL or OSMesa |
| `write_trajectory_gif` | `ExportVisualSection` | * Parsed; requires gifenc or ffmpeg |
| `write_overlay_cycle_gif` | `ExportVisualSection` | * Parsed; requires gifenc or ffmpeg |
| `write_webgl_bundle` | `ExportVisualSection` | * Parsed; webgl_streamer not wired |
| `write_sse_descriptor` | `ExportVisualSection` | * Parsed; vsepr_live not wired |
| `show_bond_graph` | `ExportVisualSection` | ✅ Parsed; opens `viz_bond_graph.html` via `viz_web.py /graph` |
| `bond_graph_port` | `ExportVisualSection` | ✅ Parsed; controls `viz_web.py` HTTP port |
| `write_report_pdf` | `ExportVisualSection` | ❌ Not implemented; requires LaTeX / pandoc |
| `export_trajectory` in `[visual.external]` | `VisualExternalSection` | * Parsed; GIF writer not wired |
| `render_targets` dispatch | `VisualExternalSection` | * Parsed; full dispatch table pending |
| `[kernel]` and `[kernel.trace]` | `raw_sections` | * No dedicated struct; all stored in raw |
| `[report]` script section | `raw_sections` | * No dedicated struct; stored in raw |
| `export_each` in `[batch.job]` | `BatchJob` | * Parsed; per-run export not enforced |
| `aggregate` in `[batch.job]` | `BatchJob` | * Parsed; aggregate report not implemented |
| `[material.*]` sub-sections (demo_07 style) | `raw_sections` | * Captured; runtime generation wiring pending (WO-VSIM-03B schema done) |
| `[sweep]` top-level declarative sweep | `raw_sections` | * Parsed; runtime wiring pending |
| `"triclinic"` cell type | `CellSection` | ❌ Reserved; not implemented |
| `"reflective"` / `"absorbing"` boundary | `BoundarySection` | ❌ Reserved; not implemented |
| `"nm"` cell units | `CellSection` | ❌ Reserved; not implemented |
| STEP B-Rep solid geometry | `ExportSection::write_step_file` | * Point-cloud only; B-Rep deferred to beta-8.1 |
| PNG raster dashboard | pipeline | * stb fallback active; PPM if stb not linked |
| Live continual reporting | — | ❌ Deprecated as autonomous engine. Use `[while]` + `[export]`. |

---

## Beta-9 owned items (registry resolution — WO-VSIM-03C)

The following were schema/parser-complete in beta-8 and are now **implemented** in beta-9 via `RegistryResolver` (WO-VSIM-03C).

| Feature | Schema / parser | Runtime |
|---|---|---|
| `structure` alias → full prototype expansion | ✅ `resolve_structure_alias()` | ✅ `RegistryResolver` |
| `RegistryBundle` crystallographic expansion | ✅ `RegistryBundle` struct | ✅ beta-9 |
| `[REGISTRY]` field resolution logging | ✅ `RegistryResolver::resolve()` | ✅ beta-9 |
| `VsimRuntime::resolve_material()` | ✅ section 6.5 | ✅ beta-9 |
| `[material]` → particle positions + masses + charges | ✅ `MaterialSection` parsed | ⬜ generator wiring (beta-10) |
| `[run]` → simulation execution | ✅ `RunSection` parsed | ⬜ runtime bridge (beta-10) |
| `[environment]` → boundary / PBC / temperature | ✅ `EnvironmentSection` parsed | ⬜ runtime bridge (beta-10) |
| `[chemistry]` / `[system]` → ambient reactions | ✅ `ChemistrySection` parsed | ✅ `ReactionBridge` + `run_chemistry_pass()` |
| `[excite.*]` → excitation dispatch | ✅ `ExciteSection` parsed | ⬜ beta-10 |
| `[observe]` → metric collection | ✅ `ObserveSection` parsed | ✅ reaction metrics via `eval_observe_metrics()` |
| `[[override.particle]]` → particle mutation | ✅ `ParticleOverrideEntry` parsed | ⬜ beta-10 |
| `[[raw.object]]` → explicit particle injection | ✅ `RawObjectEntry` parsed | ⬜ beta-10 |

---

## `RegistryBundle` — WO-VSIM-03C

Produced by `RegistryResolver::resolve(MaterialSection&, std::ostream&)`.

| Field | Type | Meaning |
|---|---|---|
| `prototype` | `string` | Canonical key resolved from alias or set directly |
| `space_group` | `string` | Hermann-Mauguin space group (empty for 0-D structures) |
| `basis` | `string` | Fractional coordinates: `"A:x,y,z; B:x,y,z"` |
| `generator` | `string` | Tag passed to the structure-builder backend |
| `coordination` | `int` | Typical coordination number (0 = unset) |
| `default_charge_model` | `string` | `"formal"`, `"neutral"`, `"bader"`, or `""` |
| `is_periodic` | `bool` | `false` for molecules and 0-D geometry |
| `populated` | `bool` | `true` when at least one field was resolved |

### Registry groups

| Group | Aliases | Prototype key family | Generator family |
|---|---|---|---|
| Ionic salts | `rocksalt`, `cesium_chloride`, `fluorite`, `antifluorite`, `zincblende`, `wurtzite`, `rutile`, `perovskite`, `spinel` | `B1_NaCl`, `B2_CsCl`, `C1_CaF2`, … | `ionic_*` |
| Oxides / ceramics | `alpha_alumina`, `magnesia`, `ceria`, `zirconia`, `uraninite`, `thoria` | `D5_Al2O3_corundum`, `B1_MgO`, `C1_*_fluorite` | `ionic_corundum`, `ionic_rocksalt`, `ionic_fluorite` |
| Elemental metals | `simple_cubic`, `bcc`, `fcc`, `hcp`, `diamond`, `graphite`, `graphene` | `A_cP1`, `A2_bcc`, `A1_fcc`, `A3_hcp`, `A4_diamond`, `A9_graphite`, `A9_graphene_2D` | `simple_cubic`, `bcc_metal`, `fcc_metal`, … |
| Molecular geometry | `linear`, `bent`, `trigonal_planar`, `tetrahedral`, `trigonal_pyramidal`, `octahedral`, `square_planar`, `see_saw`, `t_shaped` | `geom_*` | `geom_*` |
| Polymers / organics | `linear_chain`, `branched_chain`, `aromatic_ring`, `benzene_ring`, `alkane_chain`, `cycloalkane` | `polymer_*`, `organic_*` | same |
| Porous / framework | `zeolite`, `mof`, `cof`, `prussian_blue`, `pba` | `framework_*` | same |
| Bead / premacro | `bead_chain`, `bead_cluster`, `powder_bed`, `packed_bed`, `granular_column`, `fiber_bundle`, `pipe_flow` | `bead_*`, `premacro_*` | same |

### Usage

```vsim
[material]
formula   = "NaCl"
structure = "rocksalt"   # resolves to B1_NaCl

[run]
mode      = "relax"
```

In code:

```cpp
VsimDocument doc = VsimParser::parse_file("run.vsim");
RegistryBundle bundle = VsimRuntime::resolve_material(doc);
// [REGISTRY] material.prototype <- B1_NaCl  (from alias rocksalt)
// [REGISTRY] material.space_group <- Fm-3m  (from alias rocksalt)
// ...
```

---

## WO-VSIM-04A — Isomer Detection Revival

Schema version: **beta-7** • Work order: **WO-VSIM-04A**

### Overview

Three new VSIM language blocks expose the graph-validity isomer analysis pipeline:

```
formula → graph → connectivity → valence → charge/radical gate →
canonical hash → geometry/RMSD → relaxation/strain → validity class → report
```

> **Stage-order note (dev day 4 fix):** Connectivity check must run **before** valence
> validation. A disconnected graph (e.g. two isolated `C=O` fragments) must be classified
> `DisconnectedInvalid` before per-atom valence rules are applied. If valence ran first, the
> carbon atoms in an isolated `C=O` pair (valence 2, expected 4) would produce a false
> `InvalidValence` verdict. This ordering is enforced in `classify()` in
> `src/analysis/valence_rules.cpp`. Fix verified by `test_isomer_detection` Test 4.

### Validity classes (ordered by confidence)

| Class | Meaning |
|---|---|
| `InvalidFormula` | Atom composition does not match expected formula |
| `InvalidValence` | Bond orders violate neutral valence rules |
| `DisconnectedInvalid` | Graph is fragmented and `allow_fragments = false` |
| `ChargedValid` | Valid but requires formal charge |
| `RadicalValid` | Valid but requires unpaired electron |
| `NeutralValid` | Graph-valid, neutral, connected. Elevated to `UnknownPlausible` in batch mode when `known_database_check = false`. |
| `StrainedButValid` | Valid graph; relaxation flags high strain |
| `UnknownPlausible` | Valid graph + geometry; no known-DB confirmation. Not silently discarded — surfaced for review. |
| `KnownStable` | Valid + confirmed in known-compound database |

`is_graph_valid(v)` returns `true` for all classes except `InvalidFormula`, `InvalidValence`, and `DisconnectedInvalid`.

`UnknownPlausible ≠ InvalidFormula`. Novelty is not penalised.

---

### `[analysis.isomers]` — Static Isomer Analysis

Runs the full isomer detection pipeline over the `[[molecule]]` candidates declared in the same `.vsim` file.

| Key | Type | Default | Description |
|---|---|---|---|
| `enabled` | bool | `false` | Enable static isomer analysis |
| `mode` | string | `"graph_geometry"` | `"graph_only"` or `"graph_geometry"` |
| `formula_guard` | bool | `true` | Reject candidates whose derived formula does not match |
| `graph_validation` | bool | `true` | Enable graph-level checks |
| `valence_check` | bool | `true` | Check neutral valence rules per atom |
| `connectivity_check` | bool | `true` | Require connected graph unless `allow_fragments = true` |
| `formal_charge_check` | bool | `true` | Gate on formal charge presence |
| `radical_check` | bool | `false` | Gate on radical (unpaired electron) presence |
| `canonical_hash` | bool | `true` | Compute canonical graph hash for deduplication |
| `geometry_rmsd` | bool | `true` | Compute RMSD between candidates for conformer detection |
| `relaxation_check` | bool | `true` | Flag high-strain candidates after relaxation |
| `known_database_check` | bool | `false` | Query known-compound database for `KnownStable` promotion |
| `allow_fragments` | bool | `false` | Allow disconnected molecular graphs |
| `allow_charged` | bool | `false` | Allow formal-charge structures |
| `allow_radicals` | bool | `false` | Allow radical structures |
| `allow_strained` | bool | `true` | Allow strained candidates (do not discard) |
| `max_bond_order` | int | `3` | Cap inferred bond orders at this value |
| `bond_tolerance_scale` | double | `1.20` | Multiply covalent radius sum by this to set bond detection threshold |
| `rmsd_tolerance` | double | `0.05` | RMSD threshold (Å) below which two geometries are `SameStructure` |
| `angle_tolerance_deg` | double | `3.0` | Angle tolerance for geometry comparison |
| `bond_source` | string | `"hybrid"` | `"explicit"` / `"infer"` / `"hybrid"` |
| `geometry_build` | string | `"vsepr"` | Geometry builder for candidates without coordinates |
| `report_level` | string | `"detailed"` | `"minimal"` / `"standard"` / `"detailed"` |

**Example:**

```toml
[analysis.isomers]
enabled              = true
mode                 = "graph_geometry"
formula_guard        = true
bond_source          = "hybrid"
canonical_hash       = true
geometry_rmsd        = true
allow_strained       = true
report_level         = "detailed"
```

---

### `[generator.isomers]` — Isomer Generator

Generates constitutional isomer candidates from a molecular formula.

| Key | Type | Default | Description |
|---|---|---|---|
| `enabled` | bool | `false` | Enable isomer generation |
| `formula` | string | `""` | Target molecular formula (Hill system, e.g. `"C2H2O2"`) |
| `allow_fragments` | bool | `false` | Allow disconnected products |
| `allow_charged` | bool | `false` | Allow formal-charge candidates |
| `allow_radicals` | bool | `false` | Allow radical candidates |
| `allow_strained` | bool | `true` | Include strained candidates |
| `max_bond_order` | int | `3` | Maximum bond order in generated graphs |
| `deduplicate` | string | `"canonical_graph_hash"` | Deduplication strategy |

**Example:**

```toml
[generator.isomers]
enabled        = true
formula        = "C2H2O2"
allow_strained = true
max_bond_order = 3
deduplicate    = "canonical_graph_hash"
```

---

### `[analysis.isomer_tracking]` — Trajectory Isomer Tracking

Detects isomer transitions in a `.xyzf` / `.xyzFull` trajectory by computing the molecular graph hash at each sampled frame and comparing across frames.

Bond changes, hash changes, and validity transitions are logged as events. They are **never** written back into the raw trajectory files.

| Key | Type | Default | Description |
|---|---|---|---|
| `enabled` | bool | `false` | Enable trajectory isomer tracking |
| `source` | string | `""` | Path to `.xyzf` / `.xyzFull` trajectory file |
| `sample_every` | int | `10` | Sample one frame every N steps |
| `detect_bond_changes` | bool | `true` | Log when bond topology changes between sampled frames |
| `detect_hash_changes` | bool | `true` | Log when canonical hash changes between sampled frames |
| `write_transition_log` | bool | `true` | Write `reports/isomer_transitions.jsonl` |

**Example:**

```toml
[analysis.isomer_tracking]
enabled              = true
source               = "state/reactive_run.xyzf"
sample_every         = 10
detect_bond_changes  = true
detect_hash_changes  = true
write_transition_log = true
```

---

### Output files

| File | Format | Description |
|---|---|---|
| `reports/isomer_report.md` | Markdown | Human-readable summary + per-candidate detail |
| `reports/isomer_table.csv` | CSV | Tabular candidate list (sortable, importable) |
| `reports/isomer_graphs.json` | JSON | Graph topology + fingerprint data |
| `reports/isomer_transitions.jsonl` | JSON Lines | Per-frame hash transition log (trajectory mode) |

### C++ module layout

```
include/analysis/
  molecular_graph.hpp      — MolecularGraph, ValidityClass, IsomerRelation, IsomerRecord
  bond_inference.hpp        — BondInferenceConfig, infer_bonds(), build_graph()
  valence_rules.hpp         — check_valence(), check_connectivity(), classify()
  canonical_hash.hpp        — compute_fingerprint(), compare_fingerprints()
  isomer_analysis.hpp       — analyse_candidates(), analyse_pair()
  isomer_report.hpp         — write_reports(), write_markdown(), write_csv(), write_json()

src/analysis/
  bond_inference.cpp
  valence_rules.cpp
  canonical_hash.cpp        — wraps atomistic::classify::weisfeiler_lehman_hash()
  isomer_analysis.cpp
  isomer_report.cpp
```

### Implementation status

| Feature | Schema | Parser | Runtime |
|---|---|---|---|
| `[analysis.isomers]` | ✅ `IsomerAnalysisSection` | ✅ `apply_isomer_analysis_key()` | ✅ `analyse_candidates()` |
| `[generator.isomers]` | ✅ `IsomerGeneratorSection` | ✅ `apply_isomer_generator_key()` | ⬜ generator wiring pending |
| `[analysis.isomer_tracking]` | ✅ `IsomerTrackingSection` | ✅ `apply_isomer_tracking_key()` | ⬜ trajectory walker pending |
| Canonical hash | — | — | ✅ WL + Morgan via fingerprints.hpp |
| Valence validation | — | — | ✅ full neutral-valence table |
| Connectivity check | — | — | ✅ BFS; runs **before** valence (stage-order fix, dev day 4) |
| RMSD / Kabsch alignment | — | — | ✅ via kabsch.hpp |
| Report generation | — | — | ✅ MD + CSV + JSON |
| Geometric / chirality detection | — | — | ⬜ `GeometricVariant` / `StereoVariant` enum reserved, detection pending |

---

## WO-VSEPR-SIM-61D/61E — Analysis pipeline reference (schema v2)

### Canonical section names

| Section | Scope | Notes |
|---|---|---|
| `[analysis.structure]` | Structural analysis of representative frame | Required by sampling and scale-sampling |
| `[analysis.sampling]` | Scalar trajectory sampling: RDF, MSD | Canonical — replaces `[analysis.property_sampling]` (deprecated) |
| `[analysis.scale_sampling]` | Field projection, RVE, emergence metrics | WO-VSEPR-SIM-61D |
| `[analysis.inference]` | Property inference | Alias `[inference]` accepted in v2 with warning |
| `[analysis.ikk_end_tag]` | IKK report end-tag enrichment | WO-75A — D, eta_ab, \|Psi^hid\|, Delta-S, badge |
| `[analysis.ivec]` | IKK identity-vector series output | WO-75B Phase 1 — Ī_f, ΔĪ_f, `.identity.json` |
| `[object.<layer>.<basis>]` | MCF-CAI object state grid | WO-76 — 9 cells: macro/chemical/fundamental × carrier/action/information |
| `[output]` | Output paths and file switches | `output_dir` / `output_prefix` added WO-VSEPR-SIM-61D |

### `[analysis.scale_sampling]` fields

| Field | Type | Default | Notes |
|---|---|---|---|
| `enabled` | bool | `false` | |
| `compute_field_projection` | bool | `false` | Bins mass into field grid |
| `compute_rve_sampling` | bool | `false` | RVE window sweep |
| `compute_emergence_metrics` | bool | `false` | Temporal + scale drift |
| `field_grid` | [int,int,int] | `[8,8,8]` | |
| `rve_window_lengths_A` | float[] | `[]` | Must be nonempty, strictly increasing, max ≤ min_extent/2 |
| `rve_windows_per_level` | int | `8` | Must be ≥ 4 |
| `rve_window_placement` | string | `"grid"` | `grid` \| `stratified` \| `random` |
| `min_particles_for_scale_sampling` | int | `64` | |
| `spatial_cv_threshold` | float | `0.3` | [0, 1] |
| `temporal_drift_threshold` | float | `0.3` | [0, 1] |
| `scale_drift_threshold` | float | `0.3` | [0, 1] |
| `temporal_drift_metric` | string | `"block_difference"` | enum |
| `scale_drift_metric` | string | `"successive_window_difference"` | enum |

### `[analysis.inference]` fields

| Field | Type | Default | Notes |
|---|---|---|---|
| `enabled` | bool | `false` | |
| `mode` | string | `"rule_based_61b"` | `rule_based_61b` \| `rule_based_61d` |

### `[analysis.ikk_end_tag]` fields  — WO-75A

Controls per-section IKK (Identity-Knowledge-Kernel) end-tag generation.
End-tags summarise D_rec, eta_ab, |Psi^hid|, Delta-S, and a second-law badge
from an `IdentitySidecarSeries` and append them to each Markdown / LaTeX
report section.

**Doctrine:** all values are DERIVED from sidecar data only.  Never written
back into truth-state (`.xyz` / `.xyzFull`).

| Field | Type | Default | Status | Notes |
|---|---|---|---|---|
| `enabled` | bool | `false` | ✅ | Must be `true` for any output to be emitted |
| `emit_markdown` | bool | `true` | ✅ | Appends an IKK block to `.md` report sections |
| `emit_latex` | bool | `false` | ✅ | Emits `\ikkendsection{}{}` macro to `.tex` sections |
| `d_pass_threshold` | float | `0.60` | ✅ | D_rec ≥ this value → PASS badge.  Clamped to [0,1] |
| `d_warn_threshold` | float | `0.35` | ✅ | D_rec ≥ this value → WARN badge.  Clamped; guard enforces pass > warn |
| `scale_regime` | string | `"S3"` | ✅ | IKK Notation Registry v1.0: `S0`, `S2`, `S3`, `S4`, `S_mat` |
| `section_reference` | string | `""` | ✅ | Appended as `IKK IV §<label>` in the end-tag.  Empty → omit |

**Example script block:**

```toml
[analysis.ikk_end_tag]
enabled           = true
emit_markdown     = true
emit_latex        = false
d_pass_threshold  = 0.70
d_warn_threshold  = 0.40
scale_regime      = "S_mat"
section_reference = "IV.7"
```

**Struct:** `VsimIkkEndTagSection` (`include/vsim/vsim_document.hpp`)  
**Document member:** `doc.pipeline_ikk_end_tag`  
**Module key:** `"ikk_end_tag"` (self-registers via `AutoRegister`)  
**Implementation:** `include/vsim/analysis/ikk_end_tag.hpp` + `src/vsim/analysis/ikk_end_tag.cpp`  
**Tests:** Group 87 (`tests/test_ikk_end_tag.cpp`) — 20 cases, all PASS

---

### `[analysis.ivec]` fields  — WO-75B Phase 1

Enables per-frame IKK identity-vector (Ī_f) computation and serialisation to
`<output_dir>/<run_id>.identity.json`.

**Mathematical basis:**  
Each frame maps to one aggregate I-vector in ℝ⁵ with fixed axis order
`(x, y, z, t, w)` — existence, EM, spatial, temporal, internal.  
The frame mean `Ī_f = (1/N_f) Σ I_{p,f}` is permutation-invariant and
non-invertible (first-moment only; does not recover `{I_{p,f}}`).  
Phase 1 uses a scalar proxy: one aggregate per frame derived from
`IdentitySidecarRecord` fields (particle_count = 1).

**Phase 1 proxy mapping:**

| Axis | Sidecar field | Formula |
|---|---|---|
| `x` — existence (S0) | `dataloss` | `1 − dataloss` |
| `y` — EM (S2/S3) | `hidden_channel` | `1 − hidden_channel` |
| `z` — spatial (S3) | `recoverable_info` | `recoverable_info` |
| `t` — temporal (S4) | `projection_loss` | `1 − projection_loss` |
| `w` — internal | `identity_residual` | `1 − identity_residual` |

All values are clamped to [0, 1].

**Doctrine:** derived from sidecar data only; never written back to
truth-state (`.xyz` / `.xyzFull`); never used as force input.

| Field | Type | Default | Notes |
|---|---|---|---|
| `enabled` | bool | `false` | Must be `true` for output to be emitted |
| `write_json` | bool | `true` | Write `<run_id>.identity.json` |
| `include_delta` | bool | `true` | Include ΔĪ_f per frame |
| `include_var` | bool | `false` | Include diag(Var_f) — always zero in Phase 1 |
| `output_dir` | string | `""` | Override output dir; empty = use `pipeline_output.output_dir` |

**Example script block:**

```toml
[analysis.ivec]
enabled       = true
write_json    = true
include_delta = true
output_dir    = "out/identity"
```

**Struct:** `VsimIvecSection` (`include/vsim/vsim_document.hpp`)  
**Document member:** `doc.pipeline_ivec`  
**Implementation:** `include/vsim/analysis/ikk_identity_vector.hpp` + `src/vsim/analysis/ikk_identity_vector.cpp`  
**Tests:** Group 88 (`tests/test_ikk_identity_vector.cpp`) — 28 cases, all PASS

---

### `[object.<layer>.<basis>]` fields  — WO-76 Steps 3+5

Populates the MCF-CAI 3×3 object state grid (`McfCaiSection`).

**The grid:**

|  | `carrier` | `action` | `information` |
|---|---|---|---|
| **macro** | geometry, phase | stress, heat flux | formation age, defect memory *(sidecar)* |
| **chemical** | Z, mass, CN | reaction, oxidation | bond entropy, D_chem *(sidecar)* |
| **fundamental** | charge, spin | EM/strong/weak | \|Ψ^hid\|, projection loss *(sidecar)* |

**Doctrine:** `information` column fields are sidecar-only derived quantities;
they must never be used as force inputs and must never be written back into
truth-state files.

**Layer strings:** `macro` · `chemical` · `fundamental`  
**Basis strings:** `carrier` · `action` · `information`

**Example script block:**

```toml
[object.macro.carrier]
phase_label  = "FCC"
grain_count  = 12

[object.chemical.action]
reaction_active = true
oxidation_state = -2

[object.fundamental.information]
psi_hid         = 0.12
projection_loss = 0.08
entropy_trace   = 0.35
```

**Struct:** `VsimMcfCaiSection` = `vsim::analysis::McfCaiSection`
(`include/vsim/analysis/mcf_cai.hpp`)  
**Document member:** `doc.mcf_cai`  
**Implementation:** `src/vsim/analysis/mcf_cai.cpp`  
**Tests:** Group 89 (`tests/test_mcf_cai.cpp`) — 24 cases, all PASS

### Inference modes

| Mode | `ScaleSampleRecord` consumed | `macro_ready` hard-blocked by scale |
|---|---|---|
| `rule_based_61b` | No | No |
| `rule_based_61d` | Yes | Yes — invalid projection, non-conserved mass, missing RVE/emergence |

### Readiness field

| Field | Replaces | Status |
|---|---|---|
| `macro_ready` (bool) | `macro_proxy_ready` | **Canonical** |
| `macro_proxy_ready` | — | **Deprecated** — replaced by `macro_ready` in WO-VSEPR-SIM-61E |

### Implementation mapping (Appendix F)

| Operator | Header | Record |
|---|---|---|
| S_op — structure inference | `structure_inference.hpp` | `StructureInferenceResult` |
| P_op — scalar property sampling | `property_sampling.hpp` | `PropertySampleRecord` |
| M_op — scale sampling / field / RVE | `scale_sampling.hpp` | `ScaleSampleRecord` |
| I_op — property inference | `property_inference.hpp` | `PropertyInferenceRecord` |

### Manifest schema (WO-VSEPR-SIM-61E)

```json
{
  "wo": "WO-VSEPR-SIM-61E",
  "schema_version": 2,
  "sections": { "analysis.structure": true, "analysis.sampling": true, ... },
  "auto_enabled": ["analysis.structure", "analysis.sampling"],
  "warnings": ["analysis.structure auto-enabled because ..."],
  "ok": true
}
```

### Group 38 status (WO-VSEPR-SIM-61E verification)

All 19 tests passing: T1–T17 covering `macro_ready`, mass conservation failure,
invalid RVE windows, inference mode separation, schema compatibility, output naming,
seed determinism, auto-enable manifest, and enum/bounds validation.

---

## Surface Analysis — `[object.surface]` / `[[object.surface]]`

**Added v5.0.14.** Analysis-only measurement probes that record particle
crossing events and flux metrics at geometric surfaces during a simulation run.
Surfaces never affect the force kernel, particle truth-state, or `.xyz` output.

### Doctrine

- Surfaces are post-step analysis; they execute after every force integration step.
- All output lands in a `.surface.json` sidecar, never in `.xyz` / `.xyzFull`.
- `enabled = false` at the master level silently disables all probes.
- `compute_ivec_flux = true` is a no-op unless `[analysis.ivec] enabled = true`.

### Master fields (`[object.surface]`)

| Field | Type | Default | Description |
|---|---|---|---|
| `enabled` | bool | `false` | Master on/off switch for all surface probes |
| `write_json` | bool | `false` | Emit `.surface.json` sidecar |
| `output_dir` | string | `""` | Directory for sidecar files |

### Per-probe fields (`[[object.surface]]`)

| Field | Type | Default | Description |
|---|---|---|---|
| `name` | string | `""` | Human-readable probe label |
| `geometry` | string | `"rectangle"` | `"rectangle"` · `"disk"` · `"sphere"` |
| `center_x` | float | `0.0` | Probe centre X in Å |
| `center_y` | float | `0.0` | Probe centre Y in Å |
| `center_z` | float | `0.0` | Probe centre Z in Å |
| `normal_x` | float | `0.0` | Surface outward-normal X component |
| `normal_y` | float | `0.0` | Surface outward-normal Y component |
| `normal_z` | float | `1.0` | Surface outward-normal Z component |
| `width` | float | `10.0` | Rectangle half-width in Å (ignored for disk/sphere) |
| `height` | float | `10.0` | Rectangle half-height in Å (ignored for disk/sphere) |
| `radius` | float | `5.0` | Disk or sphere radius in Å (ignored for rectangle) |
| `log_crossings` | bool | `false` | Record every individual crossing event with step index |
| `compute_flux` | bool | `false` | Φ — net particle crossings / (area · step) |
| `compute_mass_flux` | bool | `false` | J_m — mass-weighted flux / (area · step) |
| `compute_energy_flux` | bool | `false` | Q — kinetic energy flux / (area · step) |
| `compute_momentum_flux` | bool | `false` | P — normal momentum transfer / (area · step) |
| `compute_species_flux` | bool | `false` | J_s — per-species crossing subtable |
| `compute_ivec_flux` | bool | `false` | J_iv — IKK identity-vector crossing signature |
| `species_filter` | list/string | `[]` | Restrict probe to named species; inline array or bare string |
| `output_tag` | string | `""` | Key used for this probe in the `.surface.json` sidecar |

### Geometry helpers

| Geometry | Active dimension fields | `area()` formula |
|---|---|---|
| `rectangle` | `width`, `height` | `4 × width × height` |
| `disk` | `radius` | `π × radius²` |
| `sphere` | `radius` | `4π × radius²` |

### Schema location

**Struct:** `VsimSurfaceObject`, `VsimSurfaceSection`
(`include/vsim/vsim_document.hpp`)  
**Document member:** `doc.surfaces`  
**Parser applier:** `VsimParser::apply_surface_key()`
(`src/vsim/vsim_parser.cpp`)  
**Section token:** `object.surface`  
**Double-bracket:** `[[object.surface]]` creates a new `VsimSurfaceObject`

### Sidecar output schema (`.surface.json`)

```json
{
  "run": "<project.name>",
  "surfaces": {
    "<output_tag>": {
      "crossings":       [ { "step": 0, "particle_id": 42, "sign": 1 }, ... ],
      "flux":            0.0014,
      "mass_flux":       0.0,
      "energy_flux":     0.0,
      "momentum_flux":   0.0,
      "species_flux":    { "Na": 0.0012, "Cl": 0.0002 },
      "ivec_flux":       [ 0.98, 0.95, 0.91, 0.89, 0.87 ]
    }
  }
}
```

Fields not requested (e.g. `compute_flux = false`) are absent from the sidecar entry.

---

## WO-VSEPR-SIM-62A — Empirical Verification Layer (`[verify]`)

**Added Day 61 / beta-11.**
Verification is a separate layer from analysis. Analysis computes; verification judges.

### Pipeline position

```
S_op (structure) → P_op (sampling) → M_op (scale_sampling) → I_op (inference)
                                                                     ↓
                                                          run_verification()
                                                                     ↓
                                                          verify_report.json
                                                          verify_summary.tsv
```

### Section table

| Section | Purpose |
|---|---|
| `[verify]` | Top-level enable/disable, profile label, output switches |
| `[verify.structure]` | Coordination number, nearest-neighbor distance, prototype label |
| `[verify.rdf]` | First and second RDF peak positions; ordered multi-peak list |
| `[verify.msd]` | Bounded-solid MSD check, slope proxy limit |
| `[verify.mass]` | Mass conservation relative tolerance |

### `[verify]` fields

| Field | Type | Default | Notes |
|---|---|---|---|
| enabled | bool | false | Master switch |
| profile | string | `""` | Informational label written to `verify_report.json` |
| write_verify_report | bool | true | Write `{prefix}.verify_report.json` |
| write_verify_tsv | bool | true | Write `{prefix}.verify_summary.tsv` |

### `[verify.structure]` fields

| Field | Type | Default | Notes |
|---|---|---|---|
| enabled | bool | false | — |
| expected_prototype | string | `""` | Informational only (e.g. `"B1_NaCl"`) |
| expected_coordination | int | -1 | -1 = not checked |
| coordination_tolerance | int | 0 | plus/minus tolerance on rounded mean_coordination |
| expected_nearest_neighbor_A | double | -1 | -1 = not checked |
| nearest_neighbor_tolerance_A | double | 0.15 | — |
| expected_density_relation | string | `""` | Informational only |

### `[verify.rdf]` fields

| Field | Type | Default | Notes |
|---|---|---|---|
| enabled | bool | false | — |
| expected_first_peak_A | double | -1 | Used when expected_peaks_A is empty |
| first_peak_tolerance_A | double | 0.20 | — |
| expected_second_peak_A | double | -1 | Used when expected_peaks_A is empty |
| second_peak_tolerance_A | double | 0.25 | — |
| expected_peaks_A | list | `[]` | Ordered list; overrides first/second fields |
| peak_tolerance_A | double | 0.25 | Tolerance applied to each entry in expected_peaks_A |
| require_peak_order | bool | true | Assert expected_peaks_A is strictly increasing |

### `[verify.msd]` fields

| Field | Type | Default | Notes |
|---|---|---|---|
| enabled | bool | false | — |
| expect_bounded_solid | bool | false | Assert msd_proxy_A2 <= max_msd_A2 |
| max_msd_A2 | double | 5.0 | Angstrom^2 |
| max_slope_late | double | 0.01 | Assert diffusion_proxy_A2_per_frame <= max_slope_late |
| expect_regime | string | `""` | Informational label (e.g. `"solid_bounded"`) |

### `[verify.mass]` fields

| Field | Type | Default | Notes |
|---|---|---|---|
| enabled | bool | false | — |
| relative_tolerance | double | 1e-10 | Assert mass_drift_fraction <= relative_tolerance |

### Output schema (`verify_report.json`)

```json
{
  "verification_schema": 1,
  "empirical_profile": "nacl_rocksalt_short_md",
  "empirical_pass": true,
  "checks": {
    "structure.coordination":       { "status": "pass", "detail": "expected=6 measured=6 tolerance=0" },
    "structure.nearest_neighbor_A": { "status": "pass", "detail": "expected=2.8 measured=2.81 tolerance=0.15" },
    "rdf.first_peak_A":             { "status": "pass", "detail": "expected=2.8 measured=2.8 tolerance=0.2" },
    "msd.bounded_solid":            { "status": "pass", "detail": "msd=0.12 max=2.0" },
    "mass.conservation":            { "status": "pass", "detail": "mass_conserved=1 drift=0 tolerance=1e-10" }
  }
}
```

### Group 39 status (WO-VSEPR-SIM-62A)

All 19 tests passing (T1–T19): structure coordination, RDF peak pass/fail,
multi-peak ordered checks, MSD bounded-solid pass/fail, mass conservation pass/fail,
full-pipeline integration, negative paths (missing scale, mass leak, invalid RVE),
parser round-trips for all `[verify.*]` sections, and disabled-verify behavior.

---
## WO-VSEPR-SIM-62B — Batch Verification Aggregation

**Added Day 62 / beta-12.**  
Aggregates per-run `verify_report.json` files across all runs of a batch study
into pass-rate tables, failure-mode matrices, and gate evaluation.

### `failure_modes.hpp` — canonical failure mode codes

| Code string                   | Meaning |
|-------------------------------|---------|
| `FAIL_MASS_DRIFT`             | Mass conservation outside tolerance |
| `FAIL_STRUCTURE_COORDINATION` | Mean coordination outside expected ± tolerance |
| `FAIL_NEAREST_NEIGHBOR`       | Nearest-neighbor distance outside tolerance |
| `FAIL_RDF_FIRST_PEAK`         | First RDF peak outside tolerance |
| `FAIL_RDF_SECOND_PEAK`        | Second RDF peak outside tolerance |
| `FAIL_MSD_BOUNDED_SOLID`      | MSD proxy exceeded max for solid regime |
| `FAIL_OUTPUT_MISSING`         | Required output file not found |
| `FAIL_CHECK_MISSING`          | Required check name absent from verify_report.json |
| `FAIL_RUN_CRASH`              | Run did not produce any verify output |

### `[batch.aggregate.verify]` fields

| Field | Type | Default | Notes |
|---|---|---|---|
| enabled | bool | false | Enable cross-run aggregation |
| group_by | list | [] | Axis names to group runs by (required when enabled) |
| statistics | list | [] | Stats to compute: mean, std, cv, min, max, median |
| emit_matrix | bool | false | Write failure_mode_matrix.tsv |
| emit_failure_modes | bool | false | Write per-group failure mode breakdown |

### `[batch.aggregate.verify.gates]` fields

| Field | Type | Default | Notes |
|---|---|---|---|
| min_overall_pass_rate | double | 0.0 | Fraction [0,1]; 0 = no gate |
| min_mass_pass_rate | double | 0.0 | |
| min_structure_pass_rate | double | 0.0 | |
| min_rdf_pass_rate | double | 0.0 | |
| min_msd_pass_rate | double | 0.0 | |

### Group 40 status (WO-VSEPR-SIM-62B)

All 20 tests passing (Groups 41–43 below inherit this infrastructure).

---

## WO-VSIM-62C — Batch Layer Parser & Static-Axis Runtime

**Added Day 62 / beta-12.**  
Study files (`.vsim` with a `[study]` block) define factorial axis sweeps.
The batch layer parses, expands, seeds, mutates, checkpoints, and aggregates.

### File layout

```
include/batch/batch_document.hpp      — BatchDocument top-level struct
include/batch/batch_parser.hpp        — BatchParser API
include/batch/batch_expander.hpp      — BatchExpander + SeedResolver API
include/batch/batch_merger.hpp        — Template load + dot-path mutation
include/batch/batch_checkpoint.hpp    — Checkpoint persistence
include/batch/batch_require_checker.hpp — [batch.require] enforcement
include/batch/batch_aggregator.hpp    — Post-run aggregation + writers
include/batch/resolved_writer.hpp     — run.vsim.resolved emitter
src/batch/batch_parser.cpp
src/batch/batch_expander.cpp
src/batch/seed_resolver.cpp
src/batch/batch_merger.cpp
src/batch/batch_checkpoint.cpp
src/batch/batch_require_checker.cpp
src/batch/batch_aggregator.cpp
src/batch/resolved_writer.cpp
```

### `[study]` fields

| Field | Type | Default | Notes |
|---|---|---|---|
| name | string | **Required** | Study identifier; used as root output directory |
| type | string | `"parameter_sweep"` | `parameter_sweep` \| `empirical_validation` \| `formation_study` \| `sensitivity_analysis` |
| goal | string | `""` | Informational |
| version | string | `""` | Informational |

### `[batch.base]` fields

| Field | Type | Default | Notes |
|---|---|---|---|
| mode | string | **Required** | `"inline"` — study file is the base script; `"template"` — load external script |
| script | string | `""` | Path to `.vsim` template (required when mode = `"template"`) |

### `[batch.design]` fields

| Field | Type | Default | Notes |
|---|---|---|---|
| type | string | `"factorial"` | `factorial` \| `single` \| `grid` \| `latin_hypercube` (v5.1.0) \| `random` (v5.1.0) |
| replicates_per_case | int | `1` | Replicate runs per combination |
| seed_policy | string | `"split"` | `"split"` — unique seeds per replicate; `"shift"` — shared + offset |
| abort_on_fail | bool | `false` | Stop expansion on first validation error |
| rank_by | string | `"composite"` | Metric key used for `ranked_candidates.tsv` |
| max_parallel | int | `1` | Max concurrent runs (runtime; not yet wired) |
| checkpoint | bool | `true` | Enable checkpoint/resume |
| n_samples | int | `0` | Required for `latin_hypercube` / `random` design types |

### `[[batch.axis]]` fields

| Field | Type | Default | Notes |
|---|---|---|---|
| name | string | **Required** | Human label for axis (used in plan headers) |
| target | string | **Required** | Dot-path into VsimDocument (e.g. `"environment.temperature"`) |
| kind | string | `"static"` | `"static"` (wired) \| `"stochastic"` (v5.1.0) \| `"formation"` (v5.2.0) |
| values | list | `[]` | Values for `kind = "static"` |
| units | string | `""` | Informational |
| seed_source | string | `""` | Required for `kind = "stochastic"` |
| distribution | string | `"uniform"` | `"uniform"` \| `"normal"` (stochastic) |
| mean / std | double | `0 / 1` | Normal distribution parameters |
| min / max | double | `0 / 1` | Uniform distribution bounds |

### `[[batch.case]]` fields

| Field | Type | Default | Notes |
|---|---|---|---|
| name | string | **Required** | Case label (used in plan TSV and output paths) |
| `<dot.path>` | string | — | Any dot-path key overrides the base doc for this case |

### `[batch.expand]` fields

| Field | Type | Default | Notes |
|---|---|---|---|
| cases | bool | `false` | Cross-expand cases × axis combos |
| axes | list | `[]` | Subset of axis names to include (empty = all static) |

### `[batch.verify_policy]` fields

| Field | Type | Default | Notes |
|---|---|---|---|
| on_run_fail | string | `"continue"` | `"continue"` \| `"abort"` |
| on_check_fail | string | `"continue"` | `"continue"` \| `"abort"` \| `"warn"` |
| save_resolved_scripts | bool | `false` | Save `run.vsim.resolved` per run |

### `[batch.override.verify]` fields

Override global `[verify.*]` tolerance values across all runs.

| Field | Type | Notes |
|---|---|---|
| tolerance_A | double | Override `nearest_neighbor_tolerance_A` globally |
| relative_tolerance | double | Override `relative_tolerance` for mass |
| max_msd_A2 | double | Override `max_msd_A2` |
| coordination_tolerance | int | Override `coordination_tolerance` |

### `[batch.score]` fields

| Field | Type | Default | Notes |
|---|---|---|---|
| rank_by | string | `"composite"` | Metric key for ranked output |

### `[batch.require]` fields

| Field | Type | Default | Notes |
|---|---|---|---|
| fail_if_missing | bool | `false` | Treat missing outputs as hard failures |
| files | list | `[]` | Output filenames that must exist per run dir |
| checks | list | `[]` | Check names that must appear in `verify_report.json` |

### `[seed]` fields — WO-66K-AUDIT Dual-Seed Assignment

The `[seed]` section controls all simulation RNG seeds. When `world_seed` is
provided alongside `foundation`, **dual-seed mode** is active: every particle
birth hash is computed in two FNV-1a passes — once with the instance context
(`foundation`) and once with the world constant (`world_seed`). This enables
deterministic uncertainty resolution in atomic bonding process evolution
equations that require a shared environmental constant.

**Single-seed mode** (backward compatible, default):
```toml
[seed]
foundation = 4201
```

**Dual-seed mode:**
```toml
[seed]
foundation = 4201
world_seed = 99887766
```

| Field | Type | Default | Notes |
|---|---|---|---|
| `foundation` | uint64 | `0` | Base seed; 0 = random |
| `defect` | uint64 | `0` | 0 = derive: foundation + 3000 |
| `formation` | uint64 | `0` | 0 = derive: foundation + 6000 |
| `thermal` | uint64 | `0` | 0 = derive: foundation + 8000 |
| `placement` | uint64 | `0` | 0 = derive: foundation + 11000 |
| `world_seed` | Seed256 | `0` (unset) | 256-bit world constant; enables dual-seed mode when nonzero |

**Seed256 accepted formats for `world_seed`:**

| Format | Example |
|---|---|
| Decimal integer | `world_seed = 4201` |
| Hex string (0x) | `world_seed = "0xdeadbeef"` |
| Hex string (64 chars, max) | `world_seed = "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"` |
| Base64 (44 chars, max) | `world_seed = "//////////////////////////////////////////8="` |
| Binary string (256 chars, max) | `world_seed = "1111...1111"` |

The canonical 256-bit maximum (2²⁵⁶ − 1) in all three full-width formats:
- **Hex**: `ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff`
- **Base64**: `//////////////////////////////////////////8=`
- **Binary**: 256 consecutive `1` characters

**Dual-seed mode acceptance rules:**
- If only `foundation` is set → single-seed mode; birth hash assigned once.
- If both `foundation` and `world_seed` are set → dual-seed mode; birth hash computed via two FNV-1a passes. Both must complete for birth_hash to be valid.
- `world_seed` may also be specified in `[project]` as `world_seed = ...` (applies globally before `[seed]` refinement).

See `include/vsim/seed256.hpp` for the `Seed256` type and `fnv1a_mix_seed256()`.
See `include/identity/particle_identity_seeder.hpp` for the two-pass hash contract.
See `docs/theory/extreme_identity_matrix_layer.md` Appendix C for full theory.

### Dot-path mutation table (`batch_merger.cpp`)

| Dot-path prefix | Mapped struct |
|---|---|
| `environment.*` | `EnvironmentSection` |
| `run.*` | `RunSection` |
| `material.*` | `MaterialSection` |
| `simulation.*` | `SimulationSection` |
| `pbc.*` | `PBCSection` |
| `cell.*` | `CellSection` |
| `observe.*` | `ObserveSection` |
| `analysis.structure.*` | `VsimStructureAnalysisSection` |
| `analysis.sampling.*` | `VsimSamplingSection` |
| `analysis.scale_sampling.*` | `VsimScaleSamplingSection` |
| `analysis.inference.*` | `VsimAnalysisInferenceSection` |

### Runtime-deferred axes

| kind | Wired | Notes |
|------|-------|-------|
| `"static"` | ✅ beta-12 | Factorial cartesian product |
| `"stochastic"` | ⏳ v5.1.0 | Distribution sampling; seed_source required |
| `"formation"` | ⏳ v5.2.0 | Draws from `[formation.library.*]` |

### Groups 41–43 status (WO-VSIM-62C)

| Group | Target | Tests | Status |
|-------|--------|-------|--------|
| 41 | `test_batch_parser` | P1–P18 (18) | PASS |
| 42 | `test_batch_expander` | E1–E10 (10) | PASS |
| 43 | `test_batch_runner_static` | R1–R12 (12) | PASS |

---

## WO-XSUITE-02B — Live Persistent Instance + Batch Presolve Layer

### Overview

Extends the `.X` / XSuite runtime with a persistent session object that
survives across rendering, checkpointing, replay, batch sweeps, and
eigen-basis presolve.  Seven new optional `.X` sections are supported.

### New `.X` Sections

| Section | Struct fields | Description |
|---|---|---|
| `[live]` | `live_enabled`, `live_persistent_instance`, `live_state_flush_interval`, `live_health_flush_interval` | Keep a `LiveXSuiteInstance` alive across command cycles |
| `[render]` | `render_enabled`, `render_atomic_stream`, `render_analysis_stream`, `render_fps_atomic`, `render_fps_analysis` | NDJSON stream outputs for live atom and analysis state.  **Render state must not mutate truth state.** |
| `[checkpoint]` | `checkpoint_enabled`, `checkpoint_format`, `checkpoint_directory`, `checkpoint_interval`, `checkpoint_keep_last`, `checkpoint_write_hash` | Periodic `.xyzc` snapshots with optional hash records |
| `[presolve]` | `presolve_enabled`, `presolve_mode`, `presolve_batch_count`, `presolve_seed_start`, `presolve_seed_stride`, `presolve_max_steps`, `presolve_extract_matrix`, `presolve_extract_eigen`, `presolve_write_basis`, `presolve_write_summary` | Batch eigen presolve; seeds run short simulations |
| `[eigenmine]` | `eigenmine_enabled`, `eigenmine_target_solves`, `eigenmine_batch_count`, `eigenmine_seeds_per_batch`, `eigenmine_systems_per_seed`, `eigenmine_matrix_source`, `eigenmine_mode_rank_limit`, `eigenmine_min_recurrence`, `eigenmine_max_recon_error`, `eigenmine_write_modes`, `eigenmine_write_basis`, `eigenmine_write_summary` | Large-scale recurrent eigen-mode mining (up to 10 M solves) |
| `[curvefit]` | `curvefit_enabled`, `curvefit_source`, `curvefit_target`, `curvefit_model`, `curvefit_max_order`, `curvefit_regularization`, `curvefit_train_fraction`, `curvefit_val_fraction`, `curvefit_write_model`, `curvefit_write_coefficients`, `curvefit_write_report` | Poly/log/eigen-hybrid surrogate model with physics penalty |
| `[release_gate]` | `release_gate_enabled`, `release_gate_baseline`, `release_gate_candidate`, `release_gate_require_hash`, `release_gate_require_curvefit`, `release_gate_require_eigen`, `release_gate_max_failure`, `release_gate_write_report`, `release_gate_write_comparison` | Release readiness gate; emits PASS or BLOCKED verdict |

### New Headers

| Header | Purpose |
|---|---|
| `include/vsim/eigenmine.hpp` | EigenMineConfig, ModeRecord, EigenMineResult, eigenmine_run() |
| `include/vsim/solve_batch.hpp` | LiveXSuiteInstance, BatchSolveConfig, batch_solve_run(), live flush helpers |
| `include/vsim/curvefit_presolve.hpp` | CurvefitConfig, CurvefitModel, curvefit_run(), physics penalty |
| `include/vsim/release_gate.hpp` | ReleaseGateConfig, VersionMetrics, release_gate_evaluate() |
| `include/vsim/basis_archive.hpp` | NDJSON / binary / TSV eigen-basis serialisation |

### New Library

`vsepr_presolve` — `src/presolve/*.cpp` — linked by all presolve test targets.

### Example `.X` Files

| File | Demonstrates |
|---|---|
| `examples/xsuite/eigenmine_10m.X` | 10 M solve eigen-mining campaign |
| `examples/xsuite/curvefit_presolve.X` | Surrogate model fitting from presolve archive |
| `examples/xsuite/release_gate_presolve.X` | PASS/BLOCKED release gate check |

### Acceptance Gates

| Gate | Test | Description |
|---|---|---|
| EIG-1 | `test_eigenmine_small` | `[eigenmine]` block parses from `.X` |
| EIG-2 | `test_eigenmine_small` | `target_solves = 10,000,000` as int64 metadata |
| EIG-3 | `test_eigenmine_small` | 1,000-solve small run succeeds |
| EIG-4 | `test_eigenmine_small` | Batch records carry seed index |
| EIG-5 | `test_eigenmine_small` | Retained modes non-empty after gate |
| EIG-6 | `test_eigenmine_small` | Recurrence score exported to ndjson |
| EIG-7 | `test_curvefit_presolve` | Curvefit module accepts eigenmine/batch input |
| EIG-8 | `test_curvefit_presolve` | Coefficients TSV and validation report written |
| EIG-9 | `test_release_gate` | PASS and BLOCKED scenarios evaluated correctly |
| EIG-10 | `test_release_gate` | Final report declares PASS or BLOCKED in file |

### Test Groups

| Group | Target | Tests | Status |
|-------|--------|-------|--------|
| 44 | `test_eigenmine_small` | EIG-1..6 (6) | added v5.1.3 |
| 45 | `test_curvefit_presolve` | EIG-7..8 (3) | added v5.1.3 |
| 46 | `test_release_gate` | EIG-9..10 (4) | added v5.1.3 |
| 47 | `test_basis_archive` | BA-1..5 (5) | added v5.1.3 |
| 53 | `test_view_67b` | VIEW-67B-01..08 + BONUS (9) | added WO-67-B; guarded: `BUILD_VIS=ON` required |

---

## Lightweight Viewer — WO-67-A / WO-67-B

### Build gates

| CMake option | Default | Effect |
|---|---|---|
| `BUILD_VIS` | `ON` | Required parent gate for all viewer targets |
| `BUILD_VIEWER` | `ON` | Builds `vsepr-light-view` and `vsepr_view_lib` |
| `BUILD_VIEWER_DAEMON` | `OFF` | Builds `vsepr-viewd` daemon (WO-67-A2, gated) |

### Canonical viewer data model (`include/vsim/view/viewer_types.hpp`)

The viewer is a **dumb renderer**. It consumes `ViewFrame` and only `ViewFrame`. It never invents bonds, particle identity, or physics. That belongs in the simulation kernel.

```
file parser / runtime stream
    |
canonical decoded state
    |
ViewFrame
    |
render only
```

#### ViewParticle

| Field | Type | Notes |
|---|---|---|
| `id` | `int64_t` | Sequential 1-based id assigned by loader |
| `identity_id` | `int64_t` | 0 = no sidecar; >0 = links to `ViewIdentity` |
| `type` | `string` | Element symbol or bead tag |
| `x y z` | `double` | Position in Å |
| `vx vy vz` | `double` | Velocity in Å/fs |
| `radius` | `double` | CPK radius in Å (filled by loader) |
| `charge` | `double` | Charge in e |
| `energy` | `double` | Energy in eV |
| `state_code` | `int` | 0=normal, 1=checkpoint, 2=defect |
| `type_code` | `uint32_t` | `ViewParticleType`: Atom/Bead/CoarseBead/Virtual |
| `flags` | `uint32_t` | `ViewFlags` bitmask (Selected, Hidden, IdentityLinked, …) |

#### ViewBond

| Field | Type | Notes |
|---|---|---|
| `a b` | `int64_t` | Particle ids |
| `order` | `double` | Bond order |
| `strength` | `double` | Normalized bond strength |
| `fx fy fz` | `double` | Bond-force vector in eV/Å |
| `lifetime` | `double` | -1 = permanent |
| `active` | `bool` | |
| `flags` | `uint32_t` | `ViewFlags` bitmask |

#### ViewIdentity — .z / .ee overlay (WO-67-B)

| Field | Type | Notes |
|---|---|---|
| `identity_id` | `int64_t` | Must match `ViewParticle::identity_id` |
| `Z2[4]` | `double[4]` | 2×2 identity matrix, row-major |
| `Y` | `double` | Yield / occupancy scalar |
| `loss` | `double` | Dissipation / entropy loss proxy |
| `flags` | `uint32_t` | `ViewFlags` bitmask (e.g. `IdentityUnlinked`) |

`ViewIdentity` is a **display overlay only**. The viewer links records from `.z`/`.ee` sidecars to particles by `identity_id`, sets flags, and pushes warnings for unlinked ids. It never evolves identity state.

#### ViewFrame

| Field | Type | Notes |
|---|---|---|
| `frame_index` | `int64_t` | |
| `time` | `double` | fs |
| `dt` | `double` | fs |
| `particles` | `vector<ViewParticle>` | |
| `bonds` | `vector<ViewBond>` | |
| `identities` | `vector<ViewIdentity>` | Populated by sidecar loader |
| `warnings` | `vector<string>` | Loader warnings surfaced to UI |
| `observables` | `map<string,double>` | energy, T, P, etc. |

#### ViewSession

`ViewSession` is the full loaded session (one file, all frames). It carries `source_path`, `file_type`, `hash` (SHA-256 of source), and `frames`.

`ViewSession::deterministic_hash()` — stable FNV-1a 64-bit hash over:
- `source_path`, `file_type`
- Per frame: `frame_index`, `time`, particle `id`/`identity_id`/`x`/`y`/`z`, identity `identity_id`

The same file must always produce the same hash. If it doesn't, the loader or parser is non-deterministic.

### File-type data contract (`include/vsim/view/view_contract.hpp`)

| Format | Role | Particles | Multi-frame | Overlay only | Bundle |
|---|---|---|---|---|---|
| `.xyz` | static positions | ✓ required | — | — | — |
| `.xyza` | enriched state | ✓ required | — | — | — |
| `.xyzf` | trajectory | ✓ required | ✓ | — | — |
| `.xyzFull` | rich replay + metadata | ✓ required | ✓ | — | — |
| `.xyzc` | checkpoint | ✓ required | ✓ | — | — |
| `.dynx` | visual/session archive | ✓ required | ✓ | — | — |
| `.z` | identity matrix overlay | — | — | ✓ | — |
| `.ee` | electron/subatomic overlay | — | — | ✓ | — |
| `.X` | bundled run/session | — | — | — | ✓ |

All types: `must_not_mutate_state = true`. The viewer never modifies particle positions, velocities, charges, energies, or identity matrix values.

### Identity sidecar format (`.z` / `.ee`)

Text file. Lines starting with `#` are comments; blank lines are ignored.
Data lines are space- or tab-separated:

```
identity_id  Z2_a  Z2_b  Z2_c  Z2_d  Y  loss
```

Lines with fewer than 7 fields are skipped with a warning. Duplicate `identity_id` rows: last wins with a warning.

Unlinked ids (particle has `identity_id > 0` but no matching sidecar record) produce a `frame.warnings` entry and set `ViewFlags::IdentityUnlinked` on the particle. This is non-fatal — the session remains viewable.

### Runtime bridge interface (`include/vsim/view/view_runtime_bridge.hpp`)

`IViewRuntimeBridge` — abstract interface for connecting the viewer to a live simulation stream:

| Method | Notes |
|---|---|
| `push_frame(ViewFrame)` | Returns false if bridge cannot accept |
| `pop_frame()` | Returns `nullopt` if no frame pending |
| `is_connected()` | Bridge health check |
| `description()` | Human-readable status for UI |
| `disconnect()` | Graceful teardown |

`NullViewBridge` — no-op implementation for headless / offline / test mode.

Transport implementations (`NdjsonViewBridge`, etc.) are gated by `BUILD_VIEWER_DAEMON` and belong to WO-67-A2.

### WO-67-B test suite (Group 53)

| Test | Description |
|---|---|
| VIEW-67B-01 | Load simple `.xyz` into `ViewFrame` |
| VIEW-67B-02 | Load `.xyzf` multi-frame trajectory |
| VIEW-67B-03 | Load `.xyzFull` with metadata |
| VIEW-67B-04 | Reject malformed frame with clear error |
| VIEW-67B-05 | Link `identity_id` from particle to `.z` state |
| VIEW-67B-06 | Handle missing identity sidecar as warning, not crash |
| VIEW-67B-07 | Confirm viewer does not mutate physical state |
| VIEW-67B-08 | Confirm deterministic same-file same-`ViewFrame` hash |
| BONUS | `contract_for()` covers all file types; mutation guard verified |

All tests passed (direct binary execution against `build_vview` viewer libs).



---

## WO-66K / WO-66L / WO-66M / WO-66J  —  Day 65-66 Bridge Layer

> Bridge paper: `docs/theory/v5114_bridge_paper.md`
> Notation reference: `docs/theory/identity_matrix_notation_reference.md`
> Version: v5.0.14  |  Branch: v5.0.0-main

---

### ParticleIdentity schema extension (WO-66K)

File: `include/identity/particle_identity.hpp`

New fields added:

| Field | Type | Description |
|---|---|---|
| `birth_hash` | `uint64_t` | Deterministic birth hash h_p = H(id, f, g, Q, B, L, J, m, tau, xi_p). Set at spawn; never updated during trajectory. |
| `birth_x` | `double` | Snapped birth position x at spawn. |
| `birth_y` | `double` | Snapped birth position y at spawn. |
| `birth_z` | `double` | Snapped birth position z at spawn. |

New method:

`apply_birth_record(birth_hash, bx, by, bz)` - copies the birth hash and snapped position from a BirthRecord produced by the seeder.

Distinct from `identity_hash`: `identity_hash` refreshes on quantum-number mutations; `birth_hash` is frozen at spawn for lifetime tracing and anti-partner identification.

---

### ParticleIdentitySeeder (WO-66K)

File: `include/identity/particle_identity_seeder.hpp`

One seeder per simulation run. Not a singleton.

**BirthRecord** fields:

| Field | Type | Description |
|---|---|---|
| `birth_hash` | `uint64_t` | Deterministic FNV-1a 64-bit hash |
| `birth_index` | `uint64_t` | Monotonic counter value at spawn |
| `birth_x/y/z/w` | `double` | Snapped birth position (1e-6 grid) |
| `type_code` | `int32_t` | Flavor / type |
| `charge` | `double` | Electric charge |
| `mass` | `double` | Rest mass |
| `spin_proxy` | `SpinProxy` | Half / Integer / Zero |
| `lepton_family` | `int8_t` | 0=none, 1=e, 2=mu, 3=tau |
| `baryon_number` | `int8_t` | +1=baryon, -1=antibaryon |
| `isospin_proxy` | `int8_t` | +1=up-type, -1=down-type |

**Key methods:**

`spawn(...)` - assign deterministic birth record, increment counter.

`build(record)` - fill a ParticleIdentity from a BirthRecord.

`spawn_and_build(...)` - one-shot convenience.

`are_anti_partners(a, b)` - returns true if A and B are a valid annihilation pair (identity gate C_id from bridge paper sec 13.3).

`anti_partner_type_hash(p)` - compute the type-level hash of the expected anti-partner without requiring a spawned instance.

Hash input: run_seed + birth_index + type_code + charge + mass + spin + lepton_family + baryon_number + isospin_proxy + snapped position (1e-6 grid as int64).

---

### AnnihilationEvent channel (WO-66J)

File: `include/vsim/bridge65/annihilation_event.hpp`

Separate event channel from ChemistryBondEvent, ChemistryNonBondEvent, DecayEvent, ParticleParticleEvent.

**AnnihilationGateResult** - diagnostic from the three-gate trigger:

| Field | Description |
|---|---|
| `passed_identity_gate` | C_id(i,j): are_anti_partners() |
| `passed_distance_gate` | r_ij < r_capture |
| `passed_energy_gate` | E_rel > energy_threshold |
| `r_ij` | Computed pair distance |
| `E_rel` | Relative kinetic energy (half-mu * dv^2) |
| `mu` | Reduced mass |

`triggered()` - true if all three gates pass.

`pp_line(step, t, id_a, id_b)` - emits `[PP]` live-print line.

**AnnihilationEvent** fields (E_ann from bridge paper sec 13.1):

| Field | Description |
|---|---|
| `step`, `time_s` | Simulation context |
| `id_i`, `id_j` | Particle ids |
| `hash_i`, `hash_j` | Birth hashes |
| `label_i`, `label_j` | Human labels |
| `xc_x/y/z` | Collision center |
| `E_in`, `E_out` | Energy in / out |
| `residual_E`, `residual_p` | Conservation residuals |
| `products` | Vector of AnnihilationProduct |
| `delta_S`, `delta_I` | Entropy / information change proxies |
| `gate` | Gate diagnostic |

`ann_line()` - emits `[ANN]` live-print line.

**Free functions:**

`check_annihilation_gate(pi, pj, xi..., vxi..., r_capture, energy_threshold)` - evaluate all three gates.

`build_annihilation_event(...)` - construct the event record from triggered gate; products appended by caller.

**v5.1.4 policy: M_ij^X = 0** - subatomic sampling disabled; annihilation channel defined but not enabled at the force level.

---

### .x Bundle manifest (WO-66M)

File: `include/vsim/bundle/x_bundle.hpp`

| Type | Description |
|---|---|
| `XBundleSlot` | One execution unit (script / inline / pykernel / validation / benchmark) |
| `XBundleManifest` | Full bundle descriptor with slots, seed, version, outputs |
| `XBundleSlotStatus` | Per-slot result (pending / running / passed / failed / skipped) |
| `XBundleSummary` | Aggregate result; `one_line()` for CI output |

Role: bundled suite execution container. Distinct from .vsim (source), .dynx (visual archive), .xyz/.xyzFull (scientific state).

Factory: `make_annihilation_test_bundle(seed, script_dir)` - builds the ANN-01..10 ladder manifest with dependency ordering.

**XBundleSlot** key fields:

| Field | Description |
|---|---|
| `id` | Unique slot id (e.g. "ANN-01") |
| `kind` | Script / InlineScript / PyKernel / ValidationCheck / Benchmark |
| `script_path` | External .vsim or .py path |
| `depends_on` | Slot ids that must pass before this runs |
| `expected_outputs` | Files that must exist after the slot |
| `allow_failure` | If true, failure does not abort bundle |
| `script_hash_sha1` | Integrity check at run time |

---

### Annihilation test ladder (ANN-01..10)

| Slot | Purpose |
|---|---|
| ANN-01 | Identity gate - only valid anti-pairs annihilate |
| ANN-02 | Distance gate - r_ij < r_c |
| ANN-03 | Energy gate - E_rel > E_c |
| ANN-04 | Product record creation |
| ANN-05 | Energy / momentum residual logging |
| ANN-06 | Live print integration ([ANN] stream) |
| ANN-07 | Glue-field disturbance response |
| ANN-08 | Eigen trend capture |
| ANN-09 | Batch sweep |
| ANN-10 | Heavy proxy annihilation (alpha + anti-alpha) |

Defined in bridge paper sec 14 and instantiated by `make_annihilation_test_bundle()`.

---

### Event channel separation policy

| Channel | Class | File |
|---|---|---|
| Classical MD collision | AtomEvent (Collision) | bridge65/atom_event.hpp |
| Chemistry bond create / break | ChemistryBondEvent | (existing) |
| Chemistry non-bond | ChemistryNonBondEvent | (existing) |
| Single-particle decay | DecayEvent | (planned) |
| Anti-pair annihilation | AnnihilationEvent | bridge65/annihilation_event.hpp |
| Glue-field disturbance | GlueFieldEvent | (planned) |
| Product emission | EmissionEvent | (planned) |

Annihilation is NOT stored as a bond break or chemistry event.

---

### Live print streams

| Prefix | Stream | Trigger |
|---|---|---|
| `[PP]` | Particle-particle interaction | Any pair interaction; ANNIHILATE action on gate pass |
| `[CHEM]` | Chemistry bond / nonbond | Bond or nonbond pair event |
| `[DECAY]` | Single-particle decay | Decay event |
| `[ANN]` | Annihilation | Triggered AnnihilationEvent |

All four streams are grep-able, console-visible, and archived in JSONL. Console may be sampled under event-limiter pressure; JSONL archive is never sampled (bridge paper sec 6).

---

## WO-66N / WO-66O / WO-66P / WO-66Q  —  Constructor Objects, Diagnostics, Crystal, XBIT

*v5.0.14 | branch: v5.0.0-main*

### Constructor objects — WO-66N

Functional constructor expressions in `[objects]`:

```vsim
[objects]
system.geometry.pipe = PipeGeometry(radius = 0.05, length = 2.0, material = steel)
system.surface.wall  = WallSurface(geometry = system.geometry.pipe)
dem.pipe_packing     = DEMBridge(from = system.surface.wall, geometry = system.geometry.pipe)
```

- `ConstructorObjectKind` enumeration covers geometry, surface, source, sink, ambient, DEM/FEA bridges, crystal
- `ConstructorObjectRegistry` stored in `VsimDocument::objects`
- Batch form: `[objects.batch]` with `base`, `count`, `constructor`
- Upper-block object-path references validated via `ObjectPathRef`

See: `include/vsim/objects/constructor_object.hpp`, `docs/wo/WO-66N-Constructor-Objects.md`

---

### Non-molecular object types — WO-66Q

| Type | Constructor | Document field |
|---|---|---|
| `GeometryObject` | `PipeGeometry`, `BoxGeometry`, `SphereGeometry`, `GenericGeometry` | `nm_objects.geometries` |
| `SurfaceObject` | `WallSurface` | `nm_objects.surfaces` |
| `SourceObject` | `InletSource` | `nm_objects.sources` |
| `SinkObject` | `OutletSink` | `nm_objects.sinks` |
| `AmbientObject` | `AmbientEnv` | `nm_objects.ambients` |

Bridge objects:

| Type | Constructor | Document field |
|---|---|---|
| `DEMBridgeObject` | `DEMBridge` | `bridge_objects.dem_bridges` |
| `FEABridgeObject` | `FEABridge` | `bridge_objects.fea_bridges` |

The `SurfaceObject` field contract (`pressure`, `shear`, `velocity`, `temperature`, `cavitation`, `flux`) is **frozen** — required by WO-67N/67O.

See: `include/vsim/objects/non_molecular_objects.hpp`, `include/vsim/objects/bridge_objects.hpp`, `docs/wo/WO-66Q-NonMolecular-Objects-XBIT.md`

---

### Organic/peptide diagnostics — WO-66O

Section: `[diagnostics.organic]`

| Key | Default | Purpose |
|---|---|---|
| `enabled` | `false` | Activate diagnostics |
| `run_on_peptide` | `true` | Run peptide checks |
| `run_on_small_molecule` | `true` | Run small-molecule checks |
| `sample_every_n_steps` | `100` | Sampling cadence |
| `violation_severity` | `warn` | `warn` or `error` |
| `peptide.check_phi_psi` | `true` | Ramachandran audit |
| `peptide.check_chirality` | `true` | L→D epimerisation detection |
| `peptide.check_hbonds` | `true` | H-bond geometry |
| `peptide.compute_rg` | `true` | Radius of gyration |
| `small_molecule.check_bond_lengths` | `true` | Bond length audit |
| `small_molecule.check_aromatic_planarity` | `true` | Ring planarity |

See: `include/vsim/diagnostics/organic_diagnostics.hpp`, `docs/wo/WO-66O-Organic-Diagnostics.md`

---

### Crystal/PBC functional constructor — WO-66P

Functional form (preferred):

```vsim
[objects]
system.crystal = CrystalModule(lattice = fcc, a = 3.52, species = [Ni], supercell = [4,4,4], relax = true)
```

Legacy `[cell]` and `[pbc]` keys continue to work and route to the same `CrystalConstructorSection`.

| Key | Purpose |
|---|---|
| `lattice` | Bravais type: `sc`, `bcc`, `fcc`, `diamond`, `hcp`, `hexagonal`, `tetragonal`, `bct`, `orthorhombic`, `monoclinic`, `triclinic`, `zincblende`, `wurtzite`, `custom` |
| `a`, `b`, `c` | Lattice parameters (Å) |
| `alpha`, `beta`, `gamma` | Angles (deg) |
| `supercell` | `[nx, ny, nz]` or `nx`/`ny`/`nz` separately |
| `pbc_x`, `pbc_y`, `pbc_z` | Periodic boundary toggles |
| `relax` | FIRE relaxation after construction |
| `species` | Element list |
| `vacancy_fraction` | Fraction of vacancies |
| `translate_to_xyz` | Emit CELL comment in xyz output |
| `translate_to_cif_summary` | Emit CIF summary block |
| `translate_to_json` | Emit JSON crystal manifest |

See: `include/vsim/crystal/crystal_constructor.hpp`, `docs/wo/WO-66P-Crystal-PBC-Constructor.md`

---

### XBIT — Extended Binary Identity Tag

32-byte deterministic identity tag for any simulation entity.

```
Bits 255-224  tier_tag     XbitTier enum
Bits 223-192  kind_tag     ConstructorObjectKind or particle Z
Bits 191-128  lineage_id   64-bit lineage hash
Bits 127-64   instance_id  64-bit instance hash
Bits  63-32   batch_tag    batch index + base hash
Bits  31-0    checksum     CRC-32
```

Serialisation: `XBIT:<64-char lowercase hex>`

See: `include/vsim/xbit/xbit.hpp`, `docs/wo/WO-66Q-NonMolecular-Objects-XBIT.md`


---

## VSIM Live Scripting State — WO-XSUITE-02B

Live scripting state governs how a running `.X` suite persists, streams, and checkpoints simulation progress without stopping the interpreter. All keys live in a `.X` file (not a `.vsim` file). The struct is `vsepr::xsuite::XSuiteFile`.

### `[live]` section — persistent instance control

| Key | Type | Default | Purpose |
|---|---|---|---|
| `enabled` | bool | `false` | Activate the live instance subsystem |
| `persistent_instance` | bool | `false` | Keep the interpreter alive between script reloads (hot-reload mode) |
| `state_flush_interval` | int | `25` | Flush interpreter state snapshot every N simulation steps |
| `health_flush_interval` | int | `10` | Flush health/diagnostics packet every N simulation steps |

**Hot-reload pattern** — set `persistent_instance = true` to keep all particle positions, velocity fields, and derived state in memory when the entry `.vsim` is re-executed. Only changed sections are re-applied. Useful for iterative scripting without restarting formation.

### `[render]` section — live streaming output

| Key | Type | Default | Purpose |
|---|---|---|---|
| `enabled` | bool | `false` | Activate streaming output |
| `atomic_stream` | string | `""` | Named stream target for atomic frames (connects to viz_server port 9999) |
| `analysis_stream` | string | `""` | Named stream target for analysis frames (connects to viz_server port 10001) |
| `fps_atomic` | int | `15` | Atomic frame rate (frames per second) |
| `fps_analysis` | int | `2` | Analysis frame rate (frames per second) |

The C++ streaming backend is `src/core/viz_server.cpp` (`vsepr::viz::VizServer`). Frames are pushed as NDJSON over TCP. Python viewers (`tools/viz_atomic.py`, `tools/viz_analysis.py`) are thin display consumers only.

### `[checkpoint]` section — periodic state save

| Key | Type | Default | Purpose |
|---|---|---|---|
| `enabled` | bool | `false` | Enable periodic checkpointing |
| `format` | string | `"xyzc"` | Checkpoint file format (`xyzc`, `xyz`, `json`) |
| `directory` | string | `"checkpoints/"` | Output directory |
| `interval` | int | `500` | Checkpoint every N simulation steps |
| `keep_last` | int | `5` | Retain only the most recent N checkpoints |
| `write_hash` | bool | `true` | Embed SHA-256 hash in checkpoint for integrity validation |

### `[presolve]` section — batch eigen precomputation

| Key | Type | Default | Purpose |
|---|---|---|---|
| `enabled` | bool | `false` | Run batch presolve before main simulation |
| `mode` | string | `"batch_eigen"` | Presolve strategy |
| `batch_count` | int | `16` | Number of parallel presolve batches |
| `seed_start` | int | `1000` | First seed for presolve ensemble |
| `seed_stride` | int | `1` | Seed increment between batches |
| `max_steps` | int | `2000` | Steps per presolve trajectory |
| `extract_matrix` | bool | `true` | Write force/Hessian matrices |
| `extract_eigen` | bool | `true` | Compute and store eigenvalues |
| `write_basis` | string | `""` | Path for basis archive output |
| `write_summary` | string | `""` | Path for presolve summary JSON |

### `[eigenmine]` section — large-scale eigen search

| Key | Type | Default | Purpose |
|---|---|---|---|
| `enabled` | bool | `false` | Activate eigenmine pass |
| `target_solves` | int64 | `10000000` | Total eigen solve target |
| `batch_count` | int | `1000` | Batches per mining cycle |
| `seeds_per_batch` | int | `100` | Seeds explored per batch |
| `systems_per_seed` | int | `100` | Systems per seed |
| `matrix_source` | string | `"state_force_event"` | Source matrix type |
| `mode_rank_limit` | int | `256` | Maximum eigen mode rank stored |
| `min_recurrence` | double | `0.70` | Minimum mode recurrence frequency to retain |
| `max_recon_error` | double | `0.05` | Maximum reconstruction error |
| `write_modes` | string | `""` | Path for eigen modes archive |
| `write_basis` | string | `""` | Path for basis output |
| `write_summary` | string | `""` | Path for eigenmine summary |

### `[curvefit]` section — empirical model fitting

| Key | Type | Default | Purpose |
|---|---|---|---|
| `enabled` | bool | `false` | Activate curve fitting pass |
| `source` | string | `""` | Input data path (JSONL or CSV) |
| `target` | string | `""` | Target field/quantity |
| `model` | string | `"poly_log_eigen_hybrid"` | Fitting model family |
| `max_order` | int | `3` | Maximum polynomial order |
| `regularization` | double | `1.0e-4` | L2 regularization weight |
| `train_fraction` | double | `0.80` | Training split fraction |
| `val_fraction` | double | `0.20` | Validation split fraction |
| `write_model` | string | `""` | Path for fitted model |
| `write_coefficients` | string | `""` | Path for coefficient output |
| `write_report` | string | `""` | Path for fit report |

### `[release_gate]` section — automated regression guard

| Key | Type | Default | Purpose |
|---|---|---|---|
| `enabled` | bool | `false` | Activate release gate validation |
| `baseline` | string | `""` | Path to baseline result archive |
| `candidate` | string | `""` | Path to candidate result archive |
| `require_hash` | double | `0.999` | Minimum hash similarity (0-1) |
| `require_curvefit` | double | `0.85` | Minimum curve-fit score |
| `require_eigen` | double | `0.80` | Minimum eigen overlap |
| `max_failure` | double | `0.01` | Maximum allowed failure fraction |
| `write_report` | string | `""` | Path for gate report |
| `write_comparison` | string | `""` | Path for baseline/candidate diff |

See: `include/vsim/xsuite.hpp`, `include/vsim/vsim_document.hpp`

---

## Bond Graph Web Viewer — WO-AUTO-01

### Activation (`.vsim` script)

```vsim
[export.visual]
show_bond_graph = true
bond_graph_port = 8899
```

When `show_bond_graph = true`, `vsepr run <script.vsim>` will after the simulation loop:

1. Locate `tools/viz_web.py` relative to the binary
2. Launch it on `bond_graph_port` via C++ `vsepr::bond_graph::launch_viz_web`
3. Wait up to 8 s for the server to answer
4. Open `http://localhost:<bond_graph_port>/graph` in the default browser

All molecule logic (random generation, bond detection, JSON frames) is handled in C++ (`src/core/bond_graph_gen.cpp`). Python is the HTTP transport layer only.

### C++ backend — `vsepr::bond_graph`

| Symbol | Purpose |
|---|---|
| `BondAtom` | Element symbol, Z, x/y/z position (Angstrom) |
| `BondEdge` | Atom index pair, distance, bond order (heuristic) |
| `BondGraphFrame` | Full frame: atoms + bonds + formula + provenance |
| `BondGraphFrame::to_json()` | Serialise to `/api/bond-graph` JSON format |
| `BondGraphGenerator` | xorshift64 RNG, random frames, XYZ parsing, bond detection |
| `BondGraphGenerator::detect_bonds()` | Covalent-radius sum x tolerance (default 1.2) |
| `BondGraphGenerator::derive_formula()` | Hill notation from atom list |
| `launch_viz_web(tools_dir, port)` | Spawn `viz_web.py`; returns PID |
| `open_graph_browser(host, port, timeout_ms)` | Open `/graph` in default browser |

### Python tools (dev/fallback only)

| File | Role |
|---|---|
| `tools/bond_graph_randomizer.py` | Dev convenience: stream random frames via UDP |
| `tools/bond_graph_auto.py` | Dev convenience: one-command stack launcher |

These files are NOT on the primary execution path. All automation logic lives in C++.

See: `include/core/bond_graph_gen.hpp`, `src/core/bond_graph_gen.cpp`, `tools/viz_web.py`, `tools/viz_bond_graph.html`

---

## WO-VSIM-CTL — Modernized VSIM Control Pipeline  (v5.1.x)

Replaces the flat ad-hoc command model with a typed namespace-qualified control language.

### Pipeline

```
.vsim
  ↓  Declarative parser
  ↓  VSIM-CTL script parser
  ↓  Semantic validator
  ↓  Typed execution graph (ExecGraph)
  ↓  Approved C++ wrapper dispatch (CtlDispatcher)
  ↓  Deterministic SMF-MD / legacy MD kernel
  ↓  Artifact registry
  ↓  Dynx / XBIT / report / manifest
  ↓  Gate validation
```

### Doctrine

> VSIM-CTL may schedule, configure, gate, and export. Only the C++ kernel may evolve physical state.

Scripts may NOT directly mutate particle arrays, forces, fields, or S-state.

### Namespaces

| Namespace | Purpose |
|---|---|
| `runtime.*` | execution, stepping, relaxation, reset, checkpoint |
| `kernel.*` | kernel configuration, force-channel toggles, trace profiles |
| `artifact.*` | XBIT, Dynx, reports, manifests, resolved configs |
| `metrics.*` | captured observables and validation quantities |
| `gate.*` | pass/fail validation gates and acceptance logic |
| control | `const` / `let` / `for` / `if` / `match` / `set` / `require` / `assert` |

### Command surface

```
const / let / for / if / match / set / require / assert

runtime.run_case(name)        runtime.step(n)
runtime.relax()               runtime.reset(scope)    runtime.checkpoint()

kernel.channel.enable(name, weight)    kernel.channel.disable(name)
kernel.channel.reset()                 kernel.trace.enable(profile)
kernel.set(name, value)

artifact.xbit.create(mode)    artifact.xbit.validate()    artifact.xbit.export(path)
artifact.dynx.enable(profile) artifact.dynx.include(payload)
artifact.dynx.validate()      artifact.dynx.export(path)
artifact.report.write(path)   artifact.manifest.write(path)

metrics.capture()             metrics.assert(expression)

gate.begin(name)              gate.assert(expression)
gate.pass()                   gate.fail()               gate.end()
```

### Key types

| Type | File | Description |
|---|---|---|
| `CtlNamespace` | `ctl_types.hpp` | enum: Runtime/Kernel/Artifact/Metrics/Gate/Control |
| `CtlArg` | `ctl_types.hpp` | variant(string, double, bool, int64) typed argument |
| `CtlCommand` | `ctl_types.hpp` | one resolved instruction: ns + op + sub_op + CtlArgMap |
| `ExecGraph` | `ctl_types.hpp` | ordered, immutable compiled command sequence |
| `CtlParser` | `ctl_parser.hpp` | script text → ExecGraph; const/let symbol table; ${var} expansion |
| `CtlValidator` | `ctl_validator.hpp` | semantic validation before dispatch (8 checks) |
| `MetricsStore` | `ctl_metrics.hpp` | observable store; throws `CtlMetricsError` before capture |
| `GateState` | `ctl_gate.hpp` | begin/gate_assert/explicit_fail/end lifecycle |
| `GateRegistry` | `ctl_gate.hpp` | session-level gate accumulator |
| `ExecPlan` | `ctl_plan.hpp` | compiled plan; `to_json()`, `artifact_manifest_json()`, `gate_manifest_json()` |
| `CtlRuntimeHooks` | `ctl_dispatcher.hpp` | interface for approved C++ runtime wrappers (no-op defaults) |
| `CtlDispatcher` | `ctl_dispatcher.hpp` | dispatch loop; populates MetricsStore/GateRegistry |

### Plan outputs

| File | Description |
|---|---|
| `script_plan.json` | typed namespace-qualified command list |
| `artifact_manifest.json` | artifact export ops extracted from graph |
| `gate_manifest.json` | gate names extracted from graph |

### Deterministic plan hash

`plan_hash = FNV-1a-64(script_text + "|" + kernel_version + "|" + seed)`

Same script + same seed + same kernel_version → same `plan_hash`.

### Validation rules enforced before dispatch

1. Unknown namespace fails  
2. Unknown command in namespace fails  
3. Unknown kernel channel name fails  
4. `artifact.dynx.*` before `artifact.dynx.enable` fails  
5. `artifact.xbit.*` before `artifact.xbit.create` fails  
6. `metrics.*` before `metrics.capture()` or `runtime.run_case()` fails  
7. `gate.end` without `gate.begin` fails  
8. `gate.begin` without `gate.end` fails  

### Known kernel channels

`repulsion` `dispersion` `coulomb` `bond` `field` `state` `lj` `ewald` `wall` `angle` `dihedral`

### Test groups — unit layer (Groups 58–64)

| Group | Target | Coverage |
|---|---|---|
| 58 | `CtlNamespaceRegistryGroup58` | Namespace enum, op registry, channel registry, CtlArg |
| 59 | `CtlParserExecGraphGroup59` | Parser, const/let, ${var} expansion, positional args |
| 60 | `CtlDispatchValidatorGroup60` | Validator + dispatcher integration |
| 61 | `CtlKernelChannelBindingsGroup61` | kernel.channel.* and kernel.trace.* hooks |
| 62 | `CtlArtifactBindingsGroup62` | artifact.dynx.* and artifact.xbit.* hooks |
| 63 | `CtlMetricsGateSystemGroup63` | MetricsStore capture guard, assert_expr, GateState |
| 64 | `CtlPlanHashManifestGroup64` | ExecPlan compile, plan_hash determinism, JSON output |

---

## WO-VSIM-CTL-TEST — CTL Integration Testing Phase  (v5.1.x)

Verifies CTL as a real workflow control layer, not just isolated parsing.

**The test question:** Does CTL actually control a run without touching physics directly?

### Test groups — integration layer (Groups 65–69)

| Group | Target | Purpose |
|---|---|---|
| 65 | `CtlEndToEndSmokeGroup65` | Parse → validate → dispatch → emit JSON/manifests → hash |
| 66 | `CtlRuntimeHookIntegrationGroup66` | Commands map to runtime hooks with correct args |
| 67 | `CtlNegativeValidationGroup67` | All failure modes fire before runtime |
| 68 | `CtlDeterminismGroup68` | CTL injects no nondeterminism |
| 69 | `CtlWorkflowCompatGroup69` | CTL headers do not break existing v5.1.x APIs |

### Group 65 — End-to-End Smoke (`test_ctl_smoke.cpp`)

Full pipeline: source text → parser → validator → ExecPlan → dispatcher → artifacts.

| Test | Assertion |
|---|---|
| SMOKE-01 | parse minimal CTL script succeeds |
| SMOKE-02 | validate minimal CTL script passes |
| SMOKE-03 | dispatch `runtime.run_case()` returns all-ok |
| SMOKE-04 | `to_json()` contains `"commands"` array |
| SMOKE-05 | `artifact_manifest_json()` is valid JSON structure |
| SMOKE-06 | `gate_manifest_json()` lists all gate names |
| SMOKE-07 | unknown namespace rejected before dispatch |
| SMOKE-08 | unknown command rejected before dispatch |
| SMOKE-09 | same script + same seed → same plan hash |
| SMOKE-10 | same script with changed `const` → same hash (const is text, not semantic hash) |

### Group 66 — Runtime Hook Integration (`test_ctl_hooks.cpp`)

Recording hooks verify every command routes to the correct method with correct args.

| Test | Assertion |
|---|---|
| HOOK-01 | `runtime.run_case()` calls hook exactly once |
| HOOK-02 | `runtime.step(n)` calls hook with correct `n` |
| HOOK-03 | `runtime.reset(scope)` calls hook with correct scope |
| HOOK-04 | `kernel.channel.enable()` calls hook with channel name + weight |
| HOOK-05 | `kernel.channel.disable()` calls hook with channel name |
| HOOK-06 | `kernel.channel.reset()` calls hook |
| HOOK-07 | `artifact.dynx.enable()` calls artifact hook |
| HOOK-08 | `artifact.xbit.create()` then `artifact.xbit.export()` calls export hook |
| HOOK-09 | `metrics.capture()` sets `MetricsStore::captured = true` |
| HOOK-10 | gate begin→assert→end lifecycle dispatches and produces `GateResult` |

### Group 67 — Negative Validation (`test_ctl_negative.cpp`)

All error paths fire at validation time, before any hook is called.

| Test | Assertion |
|---|---|
| NEG-01 | unknown namespace → validation error |
| NEG-02 | unknown command → validation error |
| NEG-03 | unknown kernel channel → validation error |
| NEG-04 | `artifact.dynx.export()` before `enable` → validation error |
| NEG-05 | `artifact.xbit.export()` before `create` → validation error |
| NEG-06 | `metrics.assert()` before capture/run → validation error |
| NEG-07 | `gate.assert()` outside a gate is flagged (gate depth = 0 when end is missing) |
| NEG-08 | `gate.end()` without `gate.begin()` → validation error |
| NEG-09 | invalid script does not produce a dispatch session |
| NEG-10 | invalid `${var}` expands to sentinel literal (not crash) |

### Group 68 — Determinism (`test_ctl_determinism.cpp`)

CTL must not inject nondeterminism into the pipeline.

| Test | Assertion |
|---|---|
| DET-01 | same script + same seed → same `plan_hash` across two compilations |
| DET-02 | same script + different seed → different `plan_hash` |
| DET-03 | `ExecGraph` command ordering matches source order |
| DET-04 | `const` expansion order is deterministic |
| DET-05 | artifact manifest ordering matches command order in graph |
| DET-06 | gate manifest ordering matches `gate.begin` order in graph |
| DET-07 | FNV-1a-64 hash produces identical output on repeated calls |
| DET-08 | `script_hash` is stable across repeated `compile()` calls |
| DET-09 | `ExecPlan` command count matches `ExecGraph` command count |
| DET-10 | rejected script (invalid validation) produces no `ExecPlan` output |

### Group 69 — Existing Workflow Compatibility (`test_ctl_compat.cpp`)

CTL headers must not pollute or break existing v5.1.x APIs.

| Test | Assertion |
|---|---|
| COMPAT-01 | `ctl_types.hpp` can be included alongside `vsim_document.hpp` |
| COMPAT-02 | `CtlNamespace` enum does not collide with existing VSIM enums |
| COMPAT-03 | `MetricsStore` does not conflict with existing metric names |
| COMPAT-04 | `ExecGraph` default constructor produces empty graph |
| COMPAT-05 | `CtlDispatcher` with null hooks falls back to default no-op hooks |
| COMPAT-06 | CTL headers are `#pragma once` safe (double-include is silent) |
| COMPAT-07 | `CtlParser::parse("")` returns ok=true, empty graph |
| COMPAT-08 | `CtlValidator::validate({})` returns ok=true on empty graph |
| COMPAT-09 | `ExecPlan::compile({}, "dev", 0)` does not crash on empty graph |
| COMPAT-10 | `CtlDispatcher::dispatch(empty_plan)` returns all-ok session |

### Invariants

- Scripts must not directly mutate particle state — all mutations route through `CtlRuntimeHooks`
- A validation failure must prevent `CtlDispatcher::dispatch()` from being called
- Partial artifacts from failed validation must not be emitted
- `plan_hash` is an identity commitment: same inputs, same hash, always

---

## Phase 6–9  |  v5.1.x Runtime Bridges & Dynx Archive  (WO-VSIM-INTENT-BRIDGE-A / WO-VSIM-FORMATION-FIELDRAMP / WO-VSIM-ISOMER-WIRE-A / WO-VSIM-DYNX-V1-A/B)

### Phase 6 — Intent Runtime Bridge A  (`include/vsim/intent/intent_bridge.hpp`)

Transforms parsed VSIM document sections into concrete runtime objects.

| Section bridged | Runtime type | Notes |
|---|---|---|
| `[material]` | `IntentParticle` × N | Basis → particles; mass from element table; formal charge for ionic prototypes |
| `[environment]` | `IntentEnvironment` | Temperature, PBC flag, boundary strings, E-field vector |
| `[run]` | `IntentRunConfig` | mode, max_steps, dt_fs, converge, output_level |

**Key types:** `IntentParticle`, `IntentEnvironment`, `IntentRunConfig`, `IntentSystem`, `IntentBridge`.

`IntentBridge::apply(doc)` — single-call bridge; returns `IntentSystem`.

Deferred to BRIDGE-B/C: `[[raw.object]]`, `[[override.particle]]`, `[excite.*]`.

**Test group:** Group 54 — `IntentBridgeBasicGroup54` (IB-A-01..09).

---

### Phase 7 — Formation FieldRamp  (`include/vsim/intent/field_ramp.hpp`)

Linear electric field ramp over a `FormationStage` of kind `field_ramp`.

```
E(t) = E0 + (E1 − E0) × (t / t_stage)
```

| Field | Source | Notes |
|---|---|---|
| E0 | `FormationStage::from_field_V_A` | Start value (V/Å) |
| E1 | `FormationStage::to_field_V_A` | End value (V/Å) |
| axis | `FormationStage::field_axis` | "x" / "y" / "z" (default "z") |
| t_stage | `FormationStage::duration_ps` | 0 → clamp to E0; negative → ok=false |

**Key types:** `FieldRampResult`, `FieldRampEvaluator`.

`FieldRampEvaluator::evaluate(stage, t_ps)` — returns `FieldRampResult` with field vector and progress fraction.

Formation libraries live in `BatchDocument::formation_library`; runtime execution deferred to v5.2.0.

**Test group:** Group 55 — `FormationFieldRampGroup55` (FR-01..07).

---

### Phase 8 — Isomer Pipeline Wiring A  (`include/vsim/intent/isomer_bridge.hpp`)

Drives `[generator.isomers]` to produce geometric isomer candidate sets.

| Output field | Description |
|---|---|
| `candidate_count` | Number of deduplicated candidates |
| `candidates` | `IsomerCandidate` list with descriptor, hash, geometry, CN |
| `deterministic` | Always true; same seed → same ordered list |

**Key types:** `IsomerCandidate`, `IsomerCandidateSet`, `IsomerBridge`.

`IsomerBridge::generate(cfg, seed)` — returns `IsomerCandidateSet`.  
`IsomerBridge::manifest_lines(set)` — returns TSV header + data rows.

Deferred to WIRE-B: `[analysis.isomer_tracking]`, chirality detection, GeometricVariant.

**Test group:** Group 56 — `IsomerWireAGroup56` (ISO-A-01..06).

---

### Phase 9 — Dynx v1 Session Archive  (`include/vsim/io/dynx_writer.hpp`)

Post-compiled dynamic session archive format for visual replay and provenance.

**Format (line-oriented text):**

```
#dynx v1
#source <path>
#source_hash <sha256-hex | "none">
#kernel_version <string>
#frame_count <N>
#frame_interval <dt_fs>
#particle_count <N>
#timestamp <ISO-8601-UTC>
FRAME <index> <time_fs>
<symbol> <x> <y> <z> [<vx> <vy> <vz>] [<energy>]
...
FORCE <particle_idx> <fx> <fy> <fz>          (optional per-particle)
BOND_FORCE <i> <j> <fx> <fy> <fz>            (optional per-bond)
FIELD <label> <fx> <fy> <fz>                 (optional field vector)
EVENT <kind> <event_id> <source> <value>      (optional KernelEvent packet)
RENDER <particle_idx> <r> <g> <b> <tag> <vis> (optional render metadata)
CAMERA <label> <x> <y> <z> <pitch> <yaw> <zoom> (optional camera state)
END_FRAME
#END_DYNX
```

| API | Description |
|---|---|
| `DynxWriter::open(path, hdr)` | Open file, write header block (frame_count placeholder) |
| `DynxWriter::write_frame(frame)` | Emit one FRAME block (v1 positions/velocities/energy only) |
| `DynxWriter::write_rich_frame(frame)` | Emit one rich FRAME block including forces, events, render, camera |
| `DynxWriter::close()` | Finalize: patch frame_count in header, close |
| `dynx_inspect(path)` | Read metadata header without frame parse |
| `dynx_validate(path)` | Full structural validation (monotonic time, particle count, hash presence) |

**WO-72B — `.dynx` Pipeline Emitter** (`include/vsim/io/dynx_emitter.hpp`):

| API | Description |
|---|---|
| `DynxEmitter::open(path, hdr)` | Open archive file; live cache always available |
| `DynxEmitter::emit_step(ctx)` | Post-step hook: harvests KernelEventLog, writes rich frame to archive, pushes to live cache |
| `DynxEmitter::close()` | Finalize archive; live cache unaffected |
| `DynxEmitter::live_cache()` | Access the `DynxLiveCache` for viewer polling |
| `DynxLiveCache::push(frame)` | Replace cached frame slot |
| `DynxLiveCache::poll()` | Get-and-clear: returns `optional<DynxRichFrame>`, empties slot |
| `DynxLiveCache::has_frame()` | True if a fresh frame is waiting |

**CLI:** `vsepr dynx inspect <file.dynx>` / `vsepr dynx validate <file.dynx>`  
**Source:** `src/vsim/io/dynx_writer.cpp`, `src/vsim/io/dynx_emitter.cpp`, `src/cli/cmd_dynx.cpp`

**Test groups:** Group 57 — `DynxV1SessionArchiveGroup57` (DYNX-V1-01..07) | Group 71 — `DynxEmitterGroup71` (71-A..71-O).
---

## .X Bundle Format  (WO-72A)

The `.X` format is a **suite execution container** - a single text archive that packages one or more `.vsim` scripts (and optional asset files) for unified execution by the `vsepr` CLI.

### Role separation

| Format | Role |
|--------|------|
| `.vsim` | Single simulation script |
| `.X`   | Bundled suite execution container |
| `.dynx` | Post-compiled live visual / session archive |

### File layout (text-based, line-oriented)

    XBUNDLE <version>
    [manifest]
      name = <string>
      entry_point = <member_name>
    [[member]]
      name = <logical-name>
      kind = vsim | asset
      size = <byte count>
      >>>
      <verbatim file content>
      <<<

### Validation rules (V-01..V-08)

V-01 manifest present; V-02 name non-empty; V-03 >= 1 entry; V-04 entry names non-empty; V-05 unique names; V-06 vsim content non-empty; V-07 entry_point names existing member; V-08 declared size matches content.

### API

`XBundleReader::read_file/read_string`, `XBundleWriter::write_file/write_string`, `XBundleValidator::validate`, `XBundle::entry_point()`, `XBundle::find(name)`  
**Library:** `vsepr_xbundle` | **Test group:** Group 70 - `XBundleSmokeGroup70` (XB-01..XB-10)

---

## WO-72V — Precomputed Cache Subsystem

**Headers:** `include/vsim/cache/cache.hpp` (umbrella)
**Library:** `vsepr_cache`
**CLI verb:** `vsepr cache <sub>`

### Sub-commands

| Sub-command | Args | Description |
|---|---|---|
| `list-presets` | `[formula]` | Show material preset(s) from built-in table |
| `list-routes` | `<formula>` | Formation routes for a product formula |
| `list-props` | `[formula]` | Physical property table entry/entries |
| `index-status` | — | Trajectory index file location and entry count |

### Components

| Header | Class | Purpose |
|---|---|---|
| `material_preset_cache.hpp` | `MaterialPresetCache` | Crystal-structure presets (lattice, density, Tm) |
| `formation_lut.hpp` | `FormationLUT` | Formation route records per product |
| `property_table.hpp` | `PropertyTable` | Density / thermal / electrical / bandgap table |
| `trajectory_index.hpp` | `TrajectoryIndex` | Hash-indexed run summary; save/load JSON |

### Built-in coverage

8 material presets (NaCl, Si, Fe, Al, Cu, MgO, TiO2, C, SiO2, Al2O3),
7 formation routes (NIST/standard), 10 property rows. Extended via `load(json_path)`.

---

## WO-72W — Material-Property ML / Pretraining Layer

**Headers:** `include/vsim/ml/ml.hpp` (umbrella)
**Library:** `vsepr_ml`
**CLI verb:** `vsepr mlprop <sub>`

### Sub-commands

| Sub-command | Args | Description |
|---|---|---|
| `recommend` | `<property> <value>` | Rank seed candidates near target property/value |
| `trends` | `<property>` | Print Pearson-r / slope / RMSE for each feature |
| `list` | — | List all seed candidates |

### Components

| Header | Class | Purpose |
|---|---|---|
| `material_candidate.hpp` | `MaterialCandidate` | Full candidate record (formula, property, route, confidence) |
| `property_trend.hpp` | `PropertyTrendFinder` | Pearson-r linear trend finder across elemental features |
| `route_recommender.hpp` | `RouteRecommender` | Confidence-scored recommendation engine |

### Seed set

8 validated bandgap candidates (Si, GaAs, GaN, ZnO, TiO2, CdS, InP, AlN).
Features computed: `electronegativity_mean`, `atomic_mass_mean`, `valence_mean`, `n_elements`.

---

## `[dissolution]` — WO-56D Surface Dissolution Module

**Struct:** `DissolutionSection`  
**Header:** `include/vsim/vsim_document.hpp`  
**Engine:** `atomistic/reaction/dissolution.hpp`

Models multi-step acid dissolution of solid oxide surfaces. Implements the protonation ladder pathway:  
M-O-M → M-OH → M-OH₂⁺ → M^n+(aq) → M(H₂O)ₓ^n+ → M-L (ligand-bound)

### Fields

| Field | Type | Default | Status | Notes |
|---|---|---|---|---|
| `enabled` | bool | `false` | ✅ | Enable dissolution pathway tracking |
| `engine` | string | `"protonation_ladder"` | ✅ | Engine type: `"protonation_ladder"` |
| `dG_first_protonation` | double | `-12.0` | ✅ | ΔG for M-O⁻ + H⁺ → M-OH (kcal/mol) |
| `dG_second_protonation` | double | `-8.0` | ✅ | ΔG for M-OH + H⁺ → M-OH₂⁺ (kcal/mol) |
| `Ea_bridging_cleavage` | double | `25.0` | ✅ | Activation energy for M-O-M cleavage (kcal/mol) |
| `Ea_terminal_release` | double | `15.0` | ✅ | Activation energy for terminal oxide release (kcal/mol) |
| `dG_hydration_Fe3` | double | `-105.0` | ✅ | ΔG hydration for Fe³⁺ hexaaquo (kcal/mol) |
| `dG_hydration_Fe2` | double | `-85.0` | ✅ | ΔG hydration for Fe²⁺ hexaaquo (kcal/mol) |
| `dG_hydration_Al3` | double | `-115.0` | ✅ | ΔG hydration for Al³⁺ hexaaquo (kcal/mol) |
| `dG_hydration_generic` | double | `-80.0` | ✅ | Default ΔG for other metals (kcal/mol) |
| `dG_sulfate_mono` | double | `-3.5` | ✅ | ΔG for monodentate sulfate binding (kcal/mol) |
| `dG_sulfate_bi` | double | `-6.0` | ✅ | ΔG for bidentate sulfate binding (kcal/mol) |
| `dG_sulfate_bridge` | double | `-8.5` | ✅ | ΔG for bridging sulfate formation (kcal/mol) |
| `pH_reference` | double | `1.0` | ✅ | Reference pH for rate constants |
| `pH_slope` | double | `-0.5` | ✅ | d(log rate)/d(pH) slope |
| `T_reference` | double | `298.15` | ✅ | Reference temperature (K) |
| `Ea_apparent` | double | `15.0` | ✅ | Apparent activation energy (kcal/mol) |
| `site_density_per_nm2` | double | `5.0` | ✅ | Reactive surface site density (sites/nm²) |

### Example

```toml
[dissolution]
enabled = true
engine  = "protonation_ladder"

# Protonation energetics (kcal/mol)
dG_first_protonation  = -12.0
dG_second_protonation = -8.0

# Lattice cleavage barriers (kcal/mol)
Ea_bridging_cleavage  = 25.0
Ea_terminal_release   = 15.0

# Hydration shell formation (kcal/mol)
dG_hydration_Fe3      = -105.0

# Ligand exchange thermodynamics (kcal/mol)
dG_sulfate_mono       = -3.5
dG_sulfate_bi         = -6.0
dG_sulfate_bridge     = -8.5

# Surface parameters
site_density_per_nm2  = 5.0
```

### Bond-Pattern Vocabulary (Chem+)

| Pattern | Description |
|---|---|
| `[M-O-M]^{bridging oxide}_{s}` | Lattice bridging oxygen |
| `[M-O-]^{terminal oxide}_{surface}` | Surface terminal oxide |
| `[M-OH]^{hydroxyl}_{surface}` | Singly protonated surface site |
| `[M-OH2+]^{leaving group}_{surface}` | Doubly protonated, labile site |
| `[M(H2O)n^m+]^{aquo complex}_{aq}` | Hydrated metal ion |
| `[M-O-SO3]^{monodentate}_{aq}` | Inner-sphere sulfate |
| `[M-(O)2-SO2]^{bidentate}_{aq}` | Chelating sulfate |
| `[M-O-SO2-O-M]^{bridging}_{aq}` | Bridging sulfate between metals |

### Related Components

- **Engine:** `atomistic::reaction::DissolutionEngine`
- **Bridge:** `vsim::DissolutionBridge`
- **Test:** `test_dissolution` (Group 4: Chemistry)
Scoring: 70% proximity to target (Gaussian, σ=20% of target) + 30% stored confidence.