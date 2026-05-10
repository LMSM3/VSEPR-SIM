# VSIM Language Documentation — Working Draft

## 0. Status

Early but stable push release. Working but missing features.

This document is the canonical reference for the `.vsim` scripting format used by VSEPR-SIM.
It will expand as the language surface grows. Gaps are noted explicitly rather than papered over.

---

## 0-B. Beta-9 Implementation Status  (`v5.0.0-beta.9`)

Beta-9 focus: **Registry Resolution and Minimal Lab Script Layer** (WO-VSIM-03B, WO-VSIM-03C).

### Gate Table

| Gate | Item | Status | Notes |
|---|---|:---:|---|
| B9-1 | RegistryResolver created | ✅ | `include/vsim/vsim_registry.hpp` |
| B9-2 | structure registry executable | ✅ | ionic, metallic, covalent, molecular, polymer, bead, porous families |
| B9-3 | material_class registry executable | ✅ | `RegistryBundle.material_class` populated for all families |
| B9-4 | run_mode registry executable | ✅ | `default_run_mode` derived from material_class |
| B9-5 | environment registry executable | ✅ | `default_medium`, `default_temperature`, `default_pressure` |
| B9-6 | solver registry executable | ✅ | `default_solver`: fire / verlet / bead_spring |
| B9-7 | forcefield registry executable | ✅ | `default_forcefield`: ewald_formal / eam / tersoff / lj_neutral / bead_spring |
| B9-8 | observables registry executable | ✅ | `default_observables`: family-appropriate metric list |
| B9-9 | export_profile registry executable | ✅ | `default_export_profile`: research_report (ionic/covalent) or standard |
| B9-10 | geometry_source registry executable | ✅ | `geometry_source`: lattice_builder / basis_expand / bead_builder / random_pack |
| B9-11 | radiation registry executable | ✅ | `default_radiation`: laser_visible (covalent), xray (ionic), none (other) |
| B9-12 | explicit user values override registry defaults | ✅ | `VsimRuntime::apply_registry_defaults()` — user values are never overwritten |
| B9-13 | `[REGISTRY]` logging implemented | ✅ | all resolved fields emitted with `[REGISTRY]` prefix |
| B9-14 | minimal NaCl relax script expands and runs | ✅ | R13/R15 in Group 38; `scripts/demo_01_nacl_level0.vsim`, `demo_03_pbc_nacl.vsim` |
| B9-15 | minimal UO2 fluorite relax script expands and runs | ✅ | R16 in Group 38; `scripts/demo_uo2_fluorite.vsim` |
| B9-16 | research_report output profile resolves flags | ✅ | R17 in Group 38; `VsimRuntime::resolve_export_profile()` |
| B9-17 | tests registered as RegistryCore | ✅ | `test_wo_03c` and `test_demo_scripts` carry `RegistryCore` label |
| B9-18 | VSIM_LANGUAGE_REFERENCE.md updated with implementation status | ✅ | this section |

### New runtime entry points (beta-9)

| Function | Location | Purpose |
|---|---|---|
| `VsimRuntime::resolve_material(doc, log)` | `vsim_runtime.hpp` | Resolve `[material]` → `RegistryBundle`; log all fields |
| `VsimRuntime::apply_registry_defaults(bundle, doc)` | `vsim_runtime.hpp` | Merge registry defaults into doc; skip explicit user values |
| `VsimRuntime::resolve_export_profile(name, exp, log)` | `vsim_runtime.hpp` | Expand named export profile → `ExportSection` flags |
| `RegistryResolver::resolve(mat, log)` | `vsim_registry.hpp` | Core resolver; maps prototype key → `RegistryBundle` |
| `RegistryResolver::resolve_proto(key, alias, log)` | `vsim_registry.hpp` | Low-level lookup used by `resolve()` |

### RegistryBundle sub-registry fields (new in beta-9)

| Field | Type | Example | Gate |
|---|---|---|:---:|
| `material_class` | string | `"ionic"`, `"metallic"`, `"molecular"` | B9-3 |
| `default_run_mode` | string | `"relax"`, `"md"` | B9-4 |
| `default_medium` | string | `"vacuum"`, `"water"` | B9-5 |
| `default_temperature` | double | `300.0` | B9-5 |
| `default_pressure` | double | `0.0` | B9-5 |
| `default_solver` | string | `"fire"`, `"verlet"` | B9-6 |
| `default_forcefield` | string | `"ewald_formal"`, `"tersoff"` | B9-7 |
| `default_observables` | string | `"energy_map,coordination,rdf"` | B9-8 |
| `default_export_profile` | string | `"research_report"`, `"standard"` | B9-9 |
| `geometry_source` | string | `"lattice_builder"`, `"bead_builder"` | B9-10 |
| `default_radiation` | string | `"xray"`, `"laser_visible"`, `"none"` | B9-11 |

### Export profiles (B9-16)

| Profile | Flags set |
|---|---|
| `minimal` | `write_xyz` |
| `standard` | `write_xyz`, `write_analysis_json`, `write_metrics_tsv`, `write_report_md` |
| `research_report` | standard + `write_events_json`, `write_manifest_json`, `write_dashboard_svg` |
| `publication` | research_report + `write_symbolic_trace_json`, `write_pipeline_audit_jsonl` |

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

---

### `[material]` — WO-VSIM-03B _(Level 0–1 intent authoring)_

Declares material identity and structural intent. This is the **primary entry point for beta-9 authoring**, replacing manual `[simulation.molecule]` for intent-driven workflows.

**Resolution hierarchy** (highest wins): `space_group + basis` > `prototype` > `structure` alias.

**Registry expansion (WO-VSIM-03C):** after parsing, call `VsimRuntime::resolve_material(doc)` to expand the resolved prototype into a `RegistryBundle` (space_group, basis, generator, coordination, charge_model). Each resolved field is logged as `[REGISTRY] material.<field> <- <value>`.

```toml
# Level 0 — casual (beta-9 registry resolves this automatically)
[material]
formula   = "NaCl"
structure = "rocksalt"   # alias resolved to prototype "B1_NaCl" → full RegistryBundle
cell      = "4x4x4"
```

```toml
# Level 1 — deterministic prototype key
[material]
formula   = "NaCl"
prototype = "B1_NaCl"
cell      = "4x4x4"
```

```toml
# Level 1 — crystallographic truth
[material]
formula     = "NaCl"
space_group = "Fm-3m"
lattice     = "fcc_ionic"
basis       = "Na:0,0,0; Cl:0.5,0.5,0.5"
cell        = "4x4x4"
```

| Field | Type | Default | Notes |
|---|---|---|---|
| `formula` | string | `""` | Chemical formula |
| `prototype` | string | `""` | Deterministic generator key: `"B1_NaCl"`, `"A4_Si"` |
| `structure` | string | `""` | Casual alias — auto-resolved via `resolve_structure_alias()` then `RegistryResolver` |
| `space_group` | string | `""` | Crystallographic space group: `"Fm-3m"`, `"Fd-3m"` |
| `lattice` | string | `""` | Lattice type hint: `"fcc_ionic"`, `"bcc"` |
| `basis` | string | `""` | Atomic basis: `"Na:0,0,0; Cl:0.5,0.5,0.5"` |
| `cell` | string | `""` | Supercell spec: `"4x4x4"`, `"2x2x1"` |
| `phase` | string | `""` | `"solid"`, `"liquid"`, `"gas"`, `"amorphous"` |

#### Registry resolution (WO-VSIM-03C)

`RegistryResolver::resolve(mat)` maps the resolved prototype key to a `RegistryBundle`:

| RegistryBundle field | Meaning |
|---|---|
| `prototype` | Canonical key (`"B1_NaCl"`) |
| `space_group` | Hermann-Mauguin group |
| `basis` | Fractional coordinate list |
| `generator` | Structure-builder backend tag |
| `coordination` | Typical coordination number |
| `default_charge_model` | `"formal"` / `"neutral"` / `"bader"` |
| `is_periodic` | `false` for molecules / 0-D |
| `populated` | `true` when at least one field resolved |

See `VSIM_REFERENCE.md § RegistryBundle` for the full group/alias table.

---

### `[run]` — WO-VSIM-03B _(Level 0 intent authoring)_

Declares the run mode and top-level execution controls.

```toml
[run]
mode      = "relax"   # required
max_steps = 500
converge  = true
```

| Field | Type | Default | Notes |
|---|---|---|---|
| `mode` | string | — | Required: `"relax"`, `"md"`, `"npt"`, `"nvt"`, `"nve"`, `"scan"`, `"single_point"` |
| `max_steps` | int | `500` | Step / iteration limit |
| `dt_fs` | float | `1.0` | Timestep (fs) — ignored for `"relax"` |
| `temperature` / `temperature_K` | float | `300.0` | K |
| `pressure` / `pressure_GPa` | float | `0.0` | GPa (NPT) |
| `converge` | bool | `true` | Stop early on convergence |
| `output_level` | string | `"standard"` | `"minimal"`, `"standard"`, `"verbose"` |

---

### `[environment]` — WO-VSIM-03B _(Level 2)_

Describes the physical environment surrounding the simulation cell.

```toml
[environment]
periodic    = true
temperature = 300
```

| Field | Type | Default | Notes |
|---|---|---|---|
| `periodic` | bool | `false` | Enable PBC |
| `temperature` | float | `300.0` | K |
| `pressure` | float | `0.0` | GPa |
| `medium` | string | `""` | `"vacuum"`, `"water"`, `"argon_gas"`, … |
| `humidity` | float | `0.0` | 0–1 fraction |
| `field_x` / `field_y` / `field_z` | float | `0.0` | External E-field components (V/Å) |

---

### `[excite.<type>]` — WO-VSIM-03B _(Level 2)_

Named excitation subsection. Multiple `[excite.*]` blocks may appear in one script.

```toml
[excite.laser]
axis           = "z"
polarization   = "x"
intensity      = 1.0
pulse_width_fs = 100
```

| Field | Type | Default | Notes |
|---|---|---|---|
| `axis` | string | `""` | `"x"`, `"y"`, `"z"` |
| `polarization` | string | `""` | `"x"`, `"y"`, `"z"`, `"circular"` |
| `intensity` | float | `1.0` | Arbitrary units (type-dependent) |
| `pulse_width_fs` | float | `100.0` | Pulse duration (fs) |
| `photon_energy_eV` | float | `0.0` | Photon energy for xray / e-beam |
| `fluence` | float | `0.0` | J/cm² |
| `profile` | string | `""` | `"gaussian"`, `"flat"`, `"sech2"` |

---

### `[observe]` — WO-VSIM-03B _(Level 2)_

Declares which physical observables to measure during the run.

```toml
[observe]
metrics = ["energy_map", "interference", "spectral_response"]
```

| Field | Type | Default | Notes |
|---|---|---|---|
| `metrics` | list of strings | `[]` | Observable names |
| `output_format` | string | `"auto"` | `"csv"`, `"json"`, `"svg"`, `"auto"` |
| `every_n_steps` | int | `1` | Observation cadence (steps) |

---

### `[[override.particle]]` — WO-VSIM-03B _(Level 3)_

Array-of-tables. Each block selectively mutates one particle before or during a run.

```toml
[[override.particle]]
id       = 14
velocity = [0.0, 0.0, 3.0]
charge   = -1.0
```

| Field | Type | Default | Notes |
|---|---|---|---|
| `id` | int | `-1` | 1-indexed particle ID (required) |
| `velocity` | `[x, y, z]` | — | Override velocity (Å/fs) |
| `position` | `[x, y, z]` | — | Override position (Å) |
| `charge` | float | `0.0` | Override charge (e) |
| `mass_scale` | float | `1.0` | Multiplicative mass modifier |
| `fixed` | bool | `false` | Freeze particle position |

---

### `[[raw.object]]` — WO-VSIM-03B _(Level 4 — debug / import only)_

Explicit particle injection. **Not the main experience** — reserved for tests, importers, file bridges, and debugging.

```toml
[[raw.object]]
id       = "debug_particle_001"
species  = "C"
position = [0, 0, 0]
velocity = [0, 0, 1]
```

| Field | Type | Default | Notes |
|---|---|---|---|
| `id` | string | `""` | Arbitrary label |
| `species` | string | `""` | Element symbol or reserved token: `"C"`, `"alpha"` |
| `position` | `[x, y, z]` | `[0,0,0]` | Position (Å) |
| `velocity` | `[x, y, z]` | `[0,0,0]` | Velocity (Å/fs) |
| `charge` | float | `0.0` | Charge (e) |
| `mass` | float | `0.0` | Mass (amu); 0 = derive from species |
| `label` | string | `""` | Optional display label |

---

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
| Array of tables `[[table]]` for multi-molecule inline | Partial — `[[simulation.molecule]]`, `[[override.particle]]`, `[[raw.object]]` supported (WO-VSIM-03B) |
| `[sweep]` top-level declarative sweep | Parsed by parser; runtime wiring pending |
| `[material.*]` runtime generation | Schema and parser done (WO-VSIM-03B); structure generator wiring pending |
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

**WO-VSIM-03B intent authoring — quick reference examples:**

Level 0 — ultra-minimal:
```toml
[project]
name = "nacl_relax"

[material]
formula   = "NaCl"
structure = "rocksalt"

[run]
mode = "relax"
```

Level 1 — deterministic prototype:
```toml
[project]
name = "nacl_lab"

[material]
formula   = "NaCl"
prototype = "B1_NaCl"
cell      = "4x4x4"

[run]
mode = "relax"
```

Level 2 — laser supercell:
```toml
[project]
name = "laser_supercell"

[material]
formula = "Si"
cell    = "6x6x6"

[environment]
periodic    = true
temperature = 300

[excite.laser]
axis           = "z"
polarization   = "x"
intensity      = 1.0
pulse_width_fs = 100

[observe]
metrics = ["energy_map", "interference", "spectral_response"]

[run]
mode = "md"
```

Level 3 — selective override:
```toml
[[override.particle]]
id       = 14
velocity = [0.0, 0.0, 3.0]
charge   = -1.0
```

Level 4 — raw explicit object (debug/import only):
```toml
[[raw.object]]
id       = "debug_particle_001"
species  = "C"
position = [0, 0, 0]
velocity = [0, 0, 1]
```

---

## Analysis pipeline sections (v2 grammar — WO-61D/61E)

The following sections are recognized only when `schema_version = 2`.

### `[analysis.structure]`

Structural analysis of the representative (last) frame.

```toml
[analysis.structure]
enabled              = true
neighbor_cutoff_A    = 5.0
contact_cutoff_A     = 3.0
```

### `[analysis.sampling]`

Scalar trajectory sampling: RDF and MSD.

```toml
[analysis.sampling]
enabled                = true
compute_rdf            = true
compute_msd            = true
min_frames_for_motion  = 2
min_frames_for_msd     = 10
unwrap_pbc             = false
```

> **Note:** The legacy section name `[analysis.property_sampling]` is not recognized
> in v2 grammar. Use `[analysis.sampling]`.

### `[analysis.scale_sampling]`

Field projection, RVE window sampling, and emergence metrics.

```toml
[analysis.scale_sampling]
enabled                            = true
compute_field_projection           = true
compute_rve_sampling               = true
compute_emergence_metrics          = true
field_grid                         = [8, 8, 8]
rve_window_lengths_A               = [4.0, 8.0, 12.0]
rve_windows_per_level              = 4
rve_window_placement               = "grid"   # grid | stratified | random
min_particles_for_scale_sampling   = 64
spatial_cv_threshold               = 0.3
temporal_drift_threshold           = 0.3
scale_drift_threshold              = 0.3
temporal_drift_metric              = "block_difference"
scale_drift_metric                 = "successive_window_difference"
```

`rve_window_placement = "random"` requires a non-zero `system.seed` when
`--strict-repro` is active.

### `[analysis.inference]`

Property inference consuming structure and sampling records.

```toml
[analysis.inference]
enabled = true
mode    = "rule_based_61d"   # rule_based_61b | rule_based_61d
```

| Mode | Scale records consumed |
|---|---|
| `rule_based_61b` | No — legacy behavior, scale sampling may still run |
| `rule_based_61d` | Yes — `ScaleSampleRecord` hard-blocks `macro_ready` |

> The deprecated section alias `[inference]` is accepted in v2 with a warning.

### Dependency auto-enable

The runtime automatically enables prerequisite sections when they are missing:

- `analysis.sampling` requires `analysis.structure`
- `analysis.scale_sampling` requires `analysis.structure`
- `analysis.scale_sampling` with `compute_emergence_metrics = true` requires `analysis.sampling`

Auto-enabled sections and their explanations are recorded in the manifest under
`auto_enabled` and `warnings`.

### `[output]`

```toml
[output]
output_dir    = "runs/my_run"
output_prefix = "run01"

write_structure_json      = true
write_sampling_json       = true
write_scale_sampling_json = true
write_inference_json      = true
write_sampling_manifest   = true
```

Output files follow the naming scheme:

```
{output_dir}/{output_prefix}.structure.json
{output_dir}/{output_prefix}.property_sampling.json
{output_dir}/{output_prefix}.scale_sampling.json
{output_dir}/{output_prefix}.property_inference.json
{output_dir}/{output_prefix}.sampling_manifest.json
```

---

## Glossary (Appendix B.2 — WO-61E)

**`macro_ready`**
Boolean readiness flag produced by the inference layer. It indicates that the
available structure, scalar sampling, and scale-sampling records satisfy the
required gates for later macro-scale interpretation. It does not itself represent
a validated macro material property.

**`macro_proxy_ready`**
*Deprecated.* Replaced by `macro_ready`.

**`rule_based_61b`**
Legacy inference mode. Consumes `StructureInferenceResult` and `PropertySampleRecord`
only. Ignores `ScaleSampleRecord` even when scale sampling has run.

**`rule_based_61d`**
Scale-aware inference mode. Consumes all three records. Hard-blocks `macro_ready`
when scale evidence fails (invalid field projection, non-conserved mass, missing
RVE candidate, or `emergent_candidate == false`).

---

## Implementation mapping (Appendix F — WO-61E)

| Operator | Meaning | Header | Record |
|---|---|---|---|
| S_op | structure inference | `structure_inference.hpp` | `StructureInferenceResult` |
| P_op | scalar property sampling | `property_sampling.hpp` | `PropertySampleRecord` |
| M_op | scale sampling / field projection / RVE | `scale_sampling.hpp` | `ScaleSampleRecord` |
| I_op | property inference | `property_inference.hpp` | `PropertyInferenceRecord` |

---

*Working draft — updated as language surface expands.*

---

## Empirical Verification ([verify] � WO-62A)

The [verify] family is a **separate layer** from [analysis].  
Analysis computes. Verification judges.

[verify] sections may appear in any analysis script. Verification runs after
all analysis operators and produces erify_report.json and erify_summary.tsv.

### [verify]

`sim
[verify]
enabled             = true
profile             = "nacl_rocksalt_short_md"   # informational label
write_verify_report = true
write_verify_tsv    = true
`

### [verify.structure]

Checks coordination number and nearest-neighbor distance against physical expectations.

`sim
[verify.structure]
enabled                      = true
expected_prototype           = "B1_NaCl"        # informational only
expected_coordination        = 6
coordination_tolerance       = 0
expected_nearest_neighbor_A  = 2.8
nearest_neighbor_tolerance_A = 0.15
expected_density_relation    = "rocksalt_supercell"  # informational only
`

### [verify.rdf]

Checks RDF peak positions. Use expected_peaks_A for multi-peak ordered checks.

`sim
# Two-field form:
[verify.rdf]
enabled                = true
expected_first_peak_A  = 2.8
first_peak_tolerance_A = 0.20
expected_second_peak_A = 3.96
second_peak_tolerance_A = 0.25

# List form (overrides two-field form):
[verify.rdf]
enabled            = true
expected_peaks_A   = [2.8, 3.96, 4.85]
peak_tolerance_A   = 0.25
require_peak_order = true
`

For NaCl rocksalt (a = 5.6 �):

| Shell | r (�) | Relation |
|---|---|---|
| 1st (Na�Cl) | 2.800 | a/2 |
| 2nd (same-ion) | 3.960 | a/v2 |
| 3rd (opposite-ion) | 4.850 | av3/2 |

### [verify.msd]

Checks that MSD is consistent with a solid (bounded vibration).

`sim
[verify.msd]
enabled              = true
expect_bounded_solid = true
max_msd_A2           = 2.0
max_slope_late       = 0.001
expect_regime        = "solid_bounded"  # informational
`

If MSD exceeds max_msd_A2, verification fails. If max_slope_late > 0, the
diffusion_proxy_A2_per_frame slope proxy is also checked.

### [verify.mass]

Checks mass conservation after scale sampling.

`sim
[verify.mass]
enabled            = true
relative_tolerance = 1e-10
`

Fails if ield_projection.mass_drift_fraction > relative_tolerance
or ield_projection.mass_conserved = false.

### Output

| File | Content |
|---|---|
| {prefix}.verify_report.json | Per-check JSON with status + detail strings |
| {prefix}.verify_summary.tsv | Tab-separated: check / status / detail |

empirical_pass = false if any enabled check fails.
