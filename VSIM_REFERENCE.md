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
| `seed_base` | uint64 | `0` | ✅ | Base RNG seed. Same seed + same script = same run. |
| `determinism` | bool | `true` | ✅ | Always true in current implementation. Field kept for schema completeness. |
| `description` | string | `""` | * | Stored in `ProjectSection::description`. Not emitted to reports yet. |

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

**Example:**

```vsim
[environment]
temperature = 800.0

[chemistry]
chemistry              = "oxidation"
heat                   = -1       # auto-derive from 800 K → h ≈ 266
reaction_events        = true
track_species_state    = true
min_score_threshold    = 0.30
max_reactions_per_step = 4

[observe]
metrics       = ["reaction_events", "exothermic_count", "avg_delta_E"]
every_n_steps = 10
```

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

### `[seed]` fields (batch study context)

| Field | Type | Default | Notes |
|---|---|---|---|
| foundation | uint64 | `0` | Base seed; 0 = random |
| defect | uint64 | `0` | 0 = derive: foundation + 3000 |
| formation | uint64 | `0` | 0 = derive: foundation + 6000 |
| thermal | uint64 | `0` | 0 = derive: foundation + 8000 |
| placement | uint64 | `0` | 0 = derive: foundation + 11000 |

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
