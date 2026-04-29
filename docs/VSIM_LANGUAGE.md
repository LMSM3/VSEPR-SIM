# VSIM Language Documentation — Working Draft

## 0. Status

Early but stable push release. Working but missing features.

This document is the canonical reference for the `.vsim` scripting format used by VSEPR-SIM.
It will expand as the language surface grows. Gaps are noted explicitly rather than papered over.

---

## 1. Purpose

VSIM is the scripting and runtime control layer for VSEPR-SIM.

Its purpose is to let the user describe simulation workflows, material systems, molecular
systems, bead systems, crystals, particle fields, and analysis passes in a compact
TOML-like text format.

VSIM is **not** a replacement for the existing C++ simulation kernel. It controls it.

### Ownership boundary

| Layer | Owns |
|---|---|
| **C++ kernel** | physics, integration, analysis, file I/O, render dispatch, runtime execution |
| **VSIM** | system description, workflow control, runtime flags, analysis selection, visual/export settings, batch execution, scripted experiment setup |

In simple terms:

> C++ does the work.  
> VSIM tells it what work to do.

### Development cycle

1. Run scripts.
2. Find errors or missing complexity.
3. Add or fix the required C++ modules.
4. Update the VSIM layer to expose the new capability.

---

## 2. Format

`.vsim` files use a TOML-subset grammar. Supported constructs:

- `[section]` and `[section.subsection]` headers
- `key = value` assignments
- Inline lists: `key = [a, b, c]`
- Comments: `# line comment`

Unsupported TOML features (captured in `raw_sections` for forward compatibility):
- Inline tables `{}`
- Multi-line strings `"""`
- Array of tables `[[]]`

---

## 3. Section Reference

### `[project]`

Required. Identifies the script.

```toml
[project]
name        = "demo_01_minimal_hexene"   # required
version     = "v5.0.0-beta.8"
seed_base   = 1001
determinism = true                        # always true; kept for schema completeness
```

| Field | Type | Default | Notes |
|---|---|---|---|
| `name` | string | — | Required. Used in report filenames and dashboard labels. |
| `version` | string | `""` | Informational. |
| `seed_base` | int | `0` | Base RNG seed for all runs in this script. |
| `determinism` | bool | `true` | Always true in the current implementation. |

---

### `[simulation]`

Controls the FIRE relaxation / dynamics run.

```toml
[simulation]
fire_max_steps   = 800
fire_dt_fs       = 0.5
box_size_ang     = 0.0     # 0 = auto-size
periodic         = false
formation_preset = "ceramic"
use_ewald        = false
ewald_alpha      = 0.3
ewald_rcut       = 12.0
ewald_kmax       = 5
step_delay_ms    = 0
```

| Field | Type | Default | Notes |
|---|---|---|---|
| `fire_max_steps` | int | `500` | Maximum FIRE minimisation steps. |
| `fire_dt_fs` | double | `1.0` | FIRE timestep in femtoseconds. |
| `box_size_ang` | double | `0.0` | Simulation box edge. `0` = auto from particle count. |
| `periodic` | bool | `false` | Enable periodic boundary conditions. Requires beta-8+. |
| `formation_preset` | string | `""` | Named potential preset (e.g. `"ceramic"`, `"metallic"`). |
| `use_ewald` | bool | `false` | Ewald summation for ionic crystals. Requires `periodic = true`. |
| `ewald_alpha` | double | `0.3` | Ewald damping parameter. |
| `ewald_rcut` | double | `12.0` | Real-space cutoff (Å). |
| `ewald_kmax` | int | `5` | Max k-space shell index. |
| `step_delay_ms` | int | `0` | Artificial sleep between steps (ms). `0` = off. |

---

### `[[simulation.molecule]]`

Declares a molecular or material species. Repeatable.

```toml
[[simulation.molecule]]
formula     = "NaCl"
count       = 64
lattice     = "fcc_ionic"
temperature = 300.0
```

| Field | Type | Notes |
|---|---|---|
| `formula` | string | Chemical formula string. |
| `count` | int | Particle/unit-cell count. |
| `lattice` | string | Optional: `"hexagonal"`, `"fcc_ionic"`, `"bcc"`, etc. |
| `temperature` | double | Temperature in K for initialisation. |
| `layer_mode` | string | Stacking mode: `"AB"` (Bernal), `"AA"`, etc. |
| `n_layers` | int | Number of stacked layers. |

---

### `[cell]`

Defines the simulation box dimensions explicitly. Alternative to `box_size_ang`.

```toml
[cell]
lx = 40.0
ly = 40.0
lz = 40.0
```

| Field | Type | Notes |
|---|---|---|
| `lx`, `ly`, `lz` | double | Box edge lengths in Å. |

---

### `[boundary]`

Boundary condition per axis.

```toml
[boundary]
x = "periodic"
y = "periodic"
z = "periodic"
```

Values: `"periodic"`, `"reflective"`, `"open"`.

---

### `[pbc]`

Periodic boundary condition options.

```toml
[pbc]
minimum_image = true
track_images  = true
```

| Field | Type | Default | Notes |
|---|---|---|---|
| `minimum_image` | bool | `false` | Apply minimum image convention to distances. |
| `track_images` | bool | `false` | Track image counts per particle for unwrapped trajectories. |

---

### `[export]`

Controls which output files are written after a run.

```toml
[export]
write_xyz                 = true
write_xyzf                = true
write_analysis_json       = true
write_metrics_tsv         = true
write_report_md           = true
write_events_jsonl        = true
write_symbolic_trace_json = true
write_dashboard_svg       = true
write_pipeline_audit_jsonl = true
write_manifest_json       = true
write_step_file           = false
output_dir                = "out/run_01"
```

---

### `[visual]`

Controls what visual output mode is used and how often frames are emitted.

```toml
[visual]
output_type           = "terminal_overlay_cycle"
animation_mode        = "overlay"
render_interval       = 50
show_proxy_table      = true
show_convergence_trace = true
show_steady_state_marker = true
show_event_timeline   = true
show_bar_chart        = true
show_symbolic_trace   = true
show_audit_table      = true
overlay_sequence      = density,coordination,memory,orient_order
gl_overlay_hold_s     = 2.5
gl_auto_orbit         = true
gl_window_width       = 1280
gl_window_height      = 800
```

#### `render_interval` — render cadence

| Field | Type | Default | Notes |
|---|---|---|---|
| `render_interval` | int | `1` | Emit a render/export frame every N simulation steps. |

**Doctrine:**

`render_interval` controls **what data is emitted**, not how fast the viewer refreshes.

```
if (step % render_interval == 0) {
	dispatch_render_frame(...)
}
```

This is orthogonal to `display_fps` (UI refresh rate in Hz), which controls how often
the terminal or live viewer repaints. A script may simulate 10 000 steps, emit render
frames every 50 steps, and the terminal may redraw at 10 Hz — all three values are
independent.

| Field | Controls | Unit | Layer |
|---|---|---|---|
| `render_interval` | how often render/export events are emitted | simulation steps | simulation / runtime |
| `display_fps` | how often the console or live viewer repaints | Hz | UI / display |

**Guard:** `render_interval <= 0` is treated as `1`.

#### `output_type` values

| Value | Description |
|---|---|
| `none` | No visual output. |
| `terminal_chart` | ASCII chart in terminal. |
| `terminal_snapshot` | Single-frame terminal snapshot. |
| `terminal_overlay_cycle` | Cycling overlay panels in terminal. |
| `terminal_rdf` | Radial distribution function in terminal. |
| `gl_overlay_cycle` | OpenGL overlay cycle (requires `-DVSEPR_HAS_GL`). |
| `gl_live_60fps` | OpenGL 60 fps live view. |
| `web_dashboard` | HTTP/SSE dashboard (experimental). |

---

### `[visual.external]`

Optional override for external visual backends. Inherits from `[visual]` but can
specify a different `render_interval` for backends that have a different cadence
requirement.

```toml
[visual.external]
enabled         = true
render_interval = 100
export_format   = "png"
export_frame_png = false
export_trajectory = false
show_progress   = true
```

`render_interval` here overrides `[visual].render_interval` for the external dispatch path.
If set to `0` or omitted, falls back to `[visual].render_interval`.

---

### `[kernel]`

Enables the central kernel pass-through and event registry.

```toml
[kernel]
pass_through        = true
symbolic_trace      = true
event_registry      = true
continual_reporting = true
```

---

### `[kernel.trace]`

Selects which event classes are traced into `KernelEventLog`.

```toml
[kernel.trace]
formation_events  = true
defect_events     = true
transport_events  = true
dynamic_energy    = true
```

---

### `[while]`

Declares a conditional loop body executed by `VsimRuntime::run_while_guards()`.

```toml
[while]
name       = "equilibrate"
condition  = "energy_var > 0.01"
max_iters  = 20
body_steps = 500
measure    = energy_var
iter_delay_ms = 0
```

Reporting cadence within a `[while]` loop is controlled by `[export]` flags declared
in the same script. The loop body runs `body_steps` simulation steps per iteration.
Snapshot cadence is script-declared — there is no background autonomous reporter.

---

### `[batch]`

Declares a parameter sweep.

```toml
[batch]
print_plan = true

[[batch.job]]
name       = "temperature_sweep"
seed_count = 3
per_run_actions = ["analyze.variance", "export"]

[batch.job.sweep]
temperature = [300, 600, 900, 1200]
```

---

### `[post_step]`

Declares a VSIM interpreter script block executed after each simulation step.
Provides access to `pbc.*` and `particle.*` builtins.

```toml
[post_step]
enabled      = true
script_block = """
d = pbc.distance(particle.position(1), particle.position(2))
"""
```

---

### `[variance]`

Declares variance probes evaluated over the event log.

```toml
[variance]
energy_var = "energy.total" "last 50" 0.01
```

---

### `[report]`

Controls the human-readable report output.

```toml
[report]
title                   = "demo_07_crystal_defects"
include_material_cards  = true
include_metric_tables   = true
include_expected_trends = true
include_symbolic_trace  = true
```

---

## 4. Script Validation

```powershell
vsper validate scripts/demo_01_minimal_hexene.vsim
```

Exit codes:

| Code | Meaning |
|---|---|
| 0 | Valid |
| 1 | Validation errors |
| 2 | Parse / IO error |

---

## 5. Missing / Deferred Features

| Feature | Status |
|---|---|
| Inline tables `{}` | Not parsed — captured in `raw_sections` |
| Array of tables `[[table]]` for multi-molecule inline | Partial — `[[simulation.molecule]]` supported |
| `[sweep]` top-level declarative sweep | Parsed by parser; runtime wiring pending |
| `[material.*]` sub-sections (demo_07 style) | Captured in `raw_sections`; full wiring pending |
| `render_targets` list execution in `[visual.external]` | Parsed; dispatch wiring pending |
| Live continual reporting | Deprecated as autonomous engine. Use `[while]` + `[export]`. |

---

## 6. Canonical `.vsim` Examples

| Script | System | Purpose |
|---|---|---|
| `scripts/demo_01_minimal_hexene.vsim` | C₆H₁₂ | Minimal single-molecule formation baseline |
| `scripts/demo_02_graphene_sheet.vsim` | C (96 atoms) | Single-layer graphene, stacking suppression test |
| `scripts/demo_03_graphite_stack.vsim` | C (480 atoms) | 5-layer AB Bernal graphite, ERB modulation |
| `scripts/beta7_pipeline_smoke.vsim` | multi | Full pipeline gate test |
| `scripts/demo_07_crystal_defects.vsim` | Fe, Al₂O₃, NaCl, SiC | Solid-state defect and transport demo (scenarios 14–17) |

---

*Working draft — updated as language surface expands.*
