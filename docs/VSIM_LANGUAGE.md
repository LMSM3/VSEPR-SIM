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
Observables are sampled every `every_n_steps` steps and written to the
format selected by `output_format`.

```toml
[observe]
metrics = [
  "energy_map",
  "coordination",
  "rdf",
  "msd",
  "displacement",
  "packing_fraction",
  "virial_stress",
  "diffusion_proxy",
  "spectral_response",
  "order_param",
  "phase_label",
  "pair_distribution",
  "cluster_count",
  "defect_map",
  "velocity_autocorr",
  "angular_distribution",
  "bond_angle",
  "cn_distribution",
  "thermal_conductivity_proxy",
  "radial_velocity"
]
output_format = "json"
every_n_steps = 25
```

| Field | Type | Default | Notes |
|---|---|---|---|
| `metrics` | list of strings | `[]` | Observable names (see catalogue below) |
| `output_format` | string | `"auto"` | `"csv"`, `"json"`, `"svg"`, `"auto"` |
| `every_n_steps` | int | `1` | Observation cadence (steps) |

#### Full Observable Catalogue

| Key | Layer | Description | Phases |
|---|---|---|---|
| `energy_map` | kernel | Per-particle potential energy field | solid, gas, liquid |
| `coordination` | structure | Average coordination number per frame | solid, liquid |
| `rdf` | structure | Radial distribution function g(r) | all |
| `msd` | dynamics | Mean-squared displacement vs time | all |
| `displacement` | dynamics | Per-particle displacement from reference | all |
| `packing_fraction` | geometry | Local volume packing fraction | solid, liquid |
| `virial_stress` | mechanics | Per-particle virial stress tensor proxy | solid |
| `diffusion_proxy` | dynamics | Self-diffusion coefficient proxy D* | gas, liquid |
| `spectral_response` | excitation | Frequency-domain response post-excitation | all |
| `order_param` | structure | Orientational order parameter η | solid, liquid |
| `phase_label` | thermodynamics | Per-particle phase assignment | all |
| `pair_distribution` | structure | Pair distribution function (alias: rdf) | all |
| `cluster_count` | topology | Number of distinct clusters | gas, liquid |
| `defect_map` | structure | Defect site spatial map | solid |
| `velocity_autocorr` | dynamics | Velocity autocorrelation function | all |
| `angular_distribution` | structure | Bond-angle distribution ADF | solid, liquid |
| `bond_angle` | structure | Per-triplet bond-angle histogram | solid |
| `cn_distribution` | structure | Coordination-number distribution P(CN) | solid, liquid |
| `thermal_conductivity_proxy` | transport | Green–Kubo heat-flux correlation proxy | solid |
| `radial_velocity` | dynamics | Radial velocity component histogram | gas |
| `interference` | excitation | Quantum interference pattern (laser path) | molecular |
| `energy_drift` | kernel | Energy drift per step (stability check) | all |
| `eta` | structure | Alias for `order_param` | all |
| `kinetic_energy` | kernel | Per-particle kinetic energy | all |
| `potential_energy` | kernel | Per-particle potential energy (alias: energy_map) | all |
| `temperature_field` | thermodynamics | Local temperature field T(r) | all |
| `pressure_field` | mechanics | Local pressure tensor field P(r) | solid, gas |

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
Every flag is `false` by default unless overridden by a named `export_profile`.
Enabling all flags is valid and produces the maximum data set — use that for
first-pass exploration, then trim for production runs.

```toml
[export]
# ── Atomistic state ──────────────────────────────────────────────────────────
write_xyz                    = true    # final particle positions (XYZ)
write_xyzf                   = true    # multi-frame trajectory (XYZF)
                                         # Active for gas-injection runs:
                                         # define [[simulation.molecule]]
                                         # with region = "corner_*".
write_xyzfull                = true    # full state history (xyzFull doctrine)
write_pdb                    = true    # PDB format (VESTA, VMD compatible)

# ── Analysis layer ────────────────────────────────────────────────────────────
write_analysis_json          = true    # AnalysisRecord (coordination, RDF peaks, ...)
write_metrics_tsv            = true    # tab-separated per-run metric table
write_cluster_json           = true    # ClusterRecord assignments
write_fingerprint_json       = true    # FingerprintRecord feature vectors

# ── Kernel event spine ────────────────────────────────────────────────────────
write_events_json            = true    # KernelEventLog (JSON Lines)
write_symbolic_trace_json    = true    # SymbolicTrace per-event

# ── Reporting layer ───────────────────────────────────────────────────────────
write_report_md              = true    # human-readable Markdown summary
write_summary_csv            = true    # per-run summary CSV
write_dashboard_json         = true    # DashboardRecord (pipeline)
write_manifest_json          = true    # run manifest with artifact registry
write_dashboard_svg          = true    # pipeline dashboard (SVG — text, diffable)
write_pipeline_audit_jsonl   = true    # stage-by-stage audit JSONL

# ── Engineering geometry ──────────────────────────────────────────────────────
write_step_file              = false   # STEP geometry (engineering sidecar)
write_vtp_mesh               = true    # VTK PolyData mesh for ParaView

output_dir = "out/my_run"
```

#### Export flag groups

| Group | Flags | Purpose |
|---|---|---|
| State truth | `write_xyz`, `write_xyzf`, `write_xyzfull`, `write_pdb` | Replay-safe atomistic record |
| Analysis | `write_analysis_json`, `write_metrics_tsv`, `write_cluster_json`, `write_fingerprint_json` | Derived scalar and vector metrics |
| Event spine | `write_events_json`, `write_symbolic_trace_json` | Kernel event log for audit and debug |
| Reports | `write_report_md`, `write_summary_csv`, `write_dashboard_json`, `write_manifest_json`, `write_dashboard_svg`, `write_pipeline_audit_jsonl` | Human and machine reports |
| Geometry | `write_step_file`, `write_vtp_mesh` | External tool integration (ParaView, CAD) |

#### Named export profiles (resolved by `VsimRuntime::resolve_export_profile`)

| Profile | Flags set |
|---|---|
| `minimal` | `write_xyz` only |
| `standard` | `write_xyz`, `write_analysis_json`, `write_metrics_tsv`, `write_report_md` |
| `research_report` | standard + `write_events_json`, `write_manifest_json`, `write_dashboard_svg` |
| `publication` | research_report + `write_symbolic_trace_json`, `write_pipeline_audit_jsonl`, `write_xyzfull` |
| `max_sampling` | all flags enabled — use for first-pass exploration and maximum data collection |

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

# Uless observation-length indicator (VTK + Qt3D viewer)
uless_indicator_enabled = true
uless_indicator_label   = obs
uless_indicator_value   = 0.75
uless_indicator_max     = 1.00

# WO-93A live two-line terminal HUD
show_status_loop      = true
status_loop_hz        = 30.0
hardware_monitor_hz   = 2.0
```

#### `render_interval` — render cadence

| Field | Type | Default | Notes |
|---|---|---|---|
| `render_interval` | int | `1` | Emit a render/export frame every N simulation steps. |
| `uless_indicator_enabled` | bool | `false` | Enable the Uless vertical observation-length bar in the VTK + Qt3D viewer. |
| `uless_indicator_label` | string | `"obs"` | Label shown above the X.XX numeric readout. |
| `uless_indicator_value` | float | `0.0` | Current observation metric (displayed to X.XX precision). |
| `uless_indicator_max` | float | `1.0` | Upper bound; the bar fill is clamped to `[0, 1]`. |
| `show_status_loop` | bool | `false` | Enable the WO-93A two-line live terminal status HUD. |
| `status_loop_hz` | float | `30.0` | Max line-1 updates/sec (step / energy / η / state). |
| `hardware_monitor_hz` | float | `2.0` | Max line-2 updates/sec (CPU / RAM / disk / GPU). |

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
| `qt3d_overlay_cycle` | VTK + Qt3D overlay cycle (requires `BUILD_VIS=ON` and Qt6 Qt3D). |
| `gl_live_60fps` | OpenGL 60 fps live view. |
| `web_dashboard` | HTTP/SSE dashboard (experimental). |

#### WO-93A status loop (`show_status_loop`)

When `output_type` is a terminal mode and `show_status_loop = true`, the runner
draws a compact two-line HUD that overwrites itself each tick with ANSI cursor-up
moves (first frame is emitted normally so redirected logs remain readable):

```
step   147/500  ██████████████  E=    -63.09  ██████████  η=0.303  warm
CPU   8%  RAM 4.4/47.8 GB  DISK 829.6 GB  GPU NVIDIA GeForce RTX 4070   Ar
```

Line 1 (high Hz, default 30 Hz):
* current step / `num_steps`
* energy bar with colour mapped to the observed `emin..emax` range
* η bar with colour mapped to `[0,1]`
* interpretive state token: `fire` (red), `warm` (yellow), `relax` (cyan), `steady` (green)

Line 2 (medium Hz, default 2 Hz):
* CPU % sampled from system idle/total deltas
* free / total RAM in GB
* free disk in GB on the current drive
* GPU name (NVIDIA via `nvidia-smi`, Windows `Get-CimInstance`, Linux `lspci`, macOS `system_profiler`

In non-terminal output (piped/redirected) the block characters fall back to ASCII `#`
so logs stay encoding-clean.

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

### `[visual.workspace]`

Controls the Qt workspace host (launched via `vsper workspace`). Silently ignored in headless runs.

```vsim
[visual.workspace]
enabled        = true    # auto-open show directives on load
default_layout = "tabs"  # "tabs" | "detached"
auto_open_tree = true    # expand object tree to active run
live           = false   # stretch: live-update open windows
```

#### `show` directive

Declares a view to summon when the script runs in the workspace. Any number of `show` statements may appear at script root or inside `[visual.workspace]`.

```vsim
show "calibration.helix"    target = "run.history"
show "data.events.timeline" target = "kernel.events"
show "data.scalar.panel"    target = "run.summary"
show "room.heatfield"       target = "room.solver" options = { "subsample": 80000 }
```

`target` is a dot-path node produced by the runtime. `options` is an optional inline JSON object. In headless runs or without `enabled = true`, `show` directives are parsed but have no effect.

See `VSIM_REFERENCE.md - [visual.workspace]` for the full view-kind catalogue.

---

### `[room]`

Drives the room heat-field simulation. Exposes `room.solver` as a viewable node.

```vsim
[room]
preset          = "reactor"  # ambient | cold | hot | lab | reactor | large_volume
n_steps         = 200
record_interval = 50
```

See `VSIM_REFERENCE.md - [room]` for field details and `scripts/gallery/room_reactor.vsim` for an example.

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

#### Supported `condition` forms

| Syntax | Meaning |
|---|---|
| `variance <probe_name> > <value>` | continue while probe variance exceeds threshold |
| `variance <probe_name> < <value>` | continue while probe variance is below threshold |
| `N_evolution <probe_name> > <value>` | continue while population rate exceeds threshold |
| `energy_drift > <value>` | continue while energy variance proxy exceeds threshold |
| `iteration < N` | fixed-count iteration guard |

---

### `[until]` — Day-72 stop-loop  (WO-72U)

Inverted `[while]`: the loop body runs **until** the condition becomes `true`.
Useful for driving a structure toward a target order parameter or threshold property.

```toml
[until]
name          = "order_anneal"
condition     = "molecule[0].order_param >= 0.98"
max_iters     = 50
body_steps    = 200
measure       = [order_param]
iter_delay_ms = 0
export_each   = false
```

The `condition` field accepts the same probe expressions as `[while]` plus two new
forms that query the running event log directly:

| Syntax | Meaning |
|---|---|
| `molecule[i].<property> >= <value>` | stop when molecule `i` property reaches threshold |
| `property[<name>] >= <value>` | stop when any event matching `<name>` reaches threshold |
| `molecule[i].<property> <= <value>` | stop when property drops to threshold |
| `property[<name>] == <value>` | stop when property equals value (epsilon 1e-12) |

All six comparison operators (`>`, `<`, `>=`, `<=`, `==`) are supported everywhere.

Named-property lookup searches the event log in reverse for the most-recent event
whose `source_formula` or `equation_symbolic` contains the key string, then returns
`result_value`.

| Field | Type | Default | Notes |
|---|---|---|---|
| `name` | string | — | User label for progress output |
| `condition` | string | — | Stop expression (see table above) |
| `max_iters` | int | `50` | Safety ceiling |
| `body_steps` | int | `200` | Simulation steps per iteration |
| `measure` | list of strings | `[]` | Probes to re-evaluate before each check |
| `iter_delay_ms` | int | `0` | UX pause between iterations (ms) |
| `export_each` | bool | `false` | Flush `[export]` artifacts per iteration |

**Example — anneal until crystalline order ≥ 98 %:**

```vsim
[project]
name = "zircaloy_anneal_study"

[material]
formula   = "Zr"
structure = "hcp"
cell      = "4x4x4"

[run]
mode      = "md"
max_steps = 5000

[until]
name          = "crystallize"
condition     = "molecule[0].order_param >= 0.98"
max_iters     = 40
body_steps    = 500
measure       = [order_param, energy_var]
export_each   = true
```

---

### `[loop]` — Day-72 smart loop with variable stepping  (WO-72L)

A fixed-count loop that steps one or more variables across each iteration.
Unlike `[while]` / `[until]`, the termination count is known at authoring time;
variable stepping enables semi-off-rails exploration of parameter space.

```toml
[loop]
name         = "temperature_ramp"
iterations   = 20
body_steps   = 300
stop_condition = "energy_var < 0.001"
record_each  = true
export_each  = true
rand_seed    = 42
perturb_mode = "combo"

var = "temperature  start=300  stop=1500  delta=63  mode=linear"
var = "defect_rate  start=0.0  stop=0.05  delta=0.0025  mode=random  noise=0.001"
```

Multiple `var` lines may appear; each advances independently.

#### `var` compact syntax

```
var = "<name>  start=<f>  stop=<f>  delta=<f>  [mode=<mode>]  [noise=<f>]  [factor=<f>]"
```

| Token | Meaning |
|---|---|
| `name` | Variable label used in per-iteration output |
| `start` | Initial value at iteration 0 |
| `stop` | Advisory ceiling (not hard-clamped by default) |
| `delta` | Linear step per iteration |
| `mode` | `linear` (default), `geometric`, `random`, `combo` |
| `noise` | ±noise amplitude around base value for `random` / `combo` modes |
| `factor` | Multiplicative factor per step for `geometric` mode |

**Mode semantics:**

| Mode | Behaviour |
|---|---|
| `linear` | `value = start + iter × delta` |
| `geometric` | `value = start × factor^iter` |
| `random` | `value = start + iter × delta ± noise` (seeded by `rand_seed`) |
| `combo` | same as `random`; the name communicates intent (exploration + trend) |

`rand_seed` is combined with the iteration index via a mixing hash, so each run
with the same seed produces identical jitter sequences (reproducible exploration).

| Field | Type | Default | Notes |
|---|---|---|---|
| `name` | string | — | Loop label |
| `iterations` | int | `10` | Total iterations to run |
| `body_steps` | int | `200` | Simulation steps per iteration |
| `stop_condition` | string | `""` | Early-exit condition (same syntax as `[until]`) |
| `record_each` | bool | `false` | Print variable values each iteration |
| `export_each` | bool | `false` | Flush `[export]` artifacts per iteration |
| `rand_seed` | int | `0` | Seed for reproducible random/combo jitter |
| `perturb_mode` | string | `"combo"` | Default mode applied to all `var` steps |
| `var` | string (repeatable) | — | Compact variable-step declaration |

---

### `[select]` — Day-72 weighted decision  (WO-72S  Uitt2 method)

Weighted multi-criteria scoring table. Reproduces a decision matrix (like the
material-selection table from the reference image) as a first-class VSIM construct.
The runtime prints the weighted score table and announces the winner.

```toml
[select]
name        = "cladding_material"
candidates  = ["Zircaloy-4", "ZIRLO/M5FeCrAl", "SiC/SiC", "SS316"]
print_table  = true
print_winner = true
output_json  = false

[[select.criterion]]
name   = "Strength and stiffness"
weight = 0.25
scores = [3, 4, 5, 3]

[[select.criterion]]
name   = "Creep and thermal stability"
weight = 0.20
scores = [3, 3, 5, 3]

[[select.criterion]]
name   = "Toughness and fracture resistance"
weight = 0.15
scores = [4, 4, 2, 5]

[[select.criterion]]
name   = "Corrosion and oxidation resistance"
weight = 0.15
scores = [3, 4, 5, 3]

[[select.criterion]]
name   = "Density"
weight = 0.10
scores = [3, 2, 5, 2]

[[select.criterion]]
name   = "Cost and manufacturability"
weight = 0.15
scores = [5, 5, 2, 5]
```

The runtime computes `weighted_score[i] = Σ (weight[c] × score[c][i])` and prints
a formatted table followed by the winning candidate.

| Field | Type | Default | Notes |
|---|---|---|---|
| `name` | string | — | Selection label for report output |
| `candidates` | list of strings | `[]` | Material / variant names (columns) |
| `print_table` | bool | `true` | Print the full weighted scoring table |
| `print_winner` | bool | `true` | Announce winning candidate |
| `output_json` | bool | `false` | Write `<prefix>.select_<name>.json` |
| `score_scale_max` | int | `5` | Advisory max score (informational) |

Each `[[select.criterion]]` block:

| Field | Type | Notes |
|---|---|---|
| `name` | string | Row label |
| `weight` | float | Fractional weight (0–1 range; auto-normalised if sum ≠ 1) |
| `scores` | list of floats | One score per candidate, same order as `candidates` |

**Combination with loops:** `[select]` can appear inside the same script as a `[loop]`
or `[until]` to re-evaluate candidate rankings as material state evolves across
iterations. The runtime re-runs `run_select()` each time the loop body completes when
`export_each = true` is set on the loop.

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
| `[until]` runtime execution | Schema + parser + runtime done (WO-72U); execution only triggered when `doc.until_cfg` is non-empty via `run_until()` |
| `[loop]` runtime execution | Schema + parser + runtime done (WO-72L); execution via `run_smart_loop()` |
| `[select]` runtime execution | Schema + parser + runtime done (WO-72S); execution via `run_select()` |
| `[select]` + loop feedback coupling | Planned — re-run `run_select()` per loop body when `export_each = true` |
| `max_sampling` export profile | Named profile defined in docs and schema; `VsimRuntime::resolve_export_profile()` wiring pending for the `max_sampling` token |
| `[observe]` per-metric output file routing | `output_format` field parsed; per-metric file dispatch pending |

---

## 6. Canonical `.vsim` Examples

| Script | System | Purpose |
|---|---|---|
| `scripts/demo_01_minimal_hexene.vsim` | C₆H₁₂ | Minimal single-molecule formation baseline |
| `scripts/demo_02_graphene_sheet.vsim` | C (96 atoms) | Single-layer graphene, stacking suppression test |
| `scripts/demo_03_graphite_stack.vsim` | C (480 atoms) | 5-layer AB Bernal graphite, ERB modulation |
| `scripts/beta7_pipeline_smoke.vsim` | multi | Full pipeline gate test |
| `scripts/demo_07_crystal_defects.vsim` | Fe, Al₂O₃, NaCl, SiC | Solid-state defect and transport demo (scenarios 14–17) |
| `scripts/suite_metal_steam_properties.vsim` | Fe BCC + H₂O steam | **Max-sampling dual-phase suite** — 1024-atom metal + 400-molecule gas; all outputs enabled |

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

---

### `[analysis.ikk_end_tag]`  — WO-75A

**Purpose:** Append an IKK (Identity-Knowledge-Kernel) end-tag block to each
Markdown or LaTeX report section.  The block summarises five IKK v1.0 headline
metrics derived from the run's `IdentitySidecarSeries`:

| Symbol | Meaning | Source field |
|---|---|---|
| D_rec | Distinguishability (fraction of identity preserved) | `1 - dataloss` |
| eta_ab | Identity coupling coefficient | `1 - projection_loss` |
| \|Psi^hid\| | Hidden residual norm (unmodelled interaction) | `hidden_channel` |
| Delta-S | Entropy proxy change (nats) | `entropy_loss_proxy` |
| Delta-D | Per-frame D drift | `(D_last - D_first) / (frames - 1)` |

**Second-law check:**  
  `PASS`  → D_rec ≥ `d_pass_threshold` (default 0.60)  
  `WARN`  → D_rec ≥ `d_warn_threshold` (default 0.35)  
  `FAIL`  → D_rec < `d_warn_threshold`

**Scale-regime labels** (IKK Notation Registry v1.0):
`"S0"` existence · `"S2"` colour · `"S3"` EM/spatial · `"S4"` weak · `"S_mat"` material

**Doctrine:**  Values are always DERIVED from sidecar data.  Never written back
into truth-state (`.xyz` / `.xyzFull`).  Orthogonal to all other analysis
sections — can be enabled independently.

```toml
[analysis.ikk_end_tag]
enabled           = true        # default: false
emit_markdown     = true        # append IKK block to .md sections
emit_latex        = false       # emit \ikkendsection{}{} to .tex
d_pass_threshold  = 0.60        # [0,1]; PASS when D_rec >= this
d_warn_threshold  = 0.35        # [0,1]; WARN when D_rec >= this; FAIL below
scale_regime      = "S3"        # active rung of scale ladder
section_reference = "IV.3"      # IKK IV reference label; empty = omit
```

**Guard rules enforced by the parser:**
- `d_pass_threshold` and `d_warn_threshold` are clamped to [0, 1].
- If `d_warn_threshold` is set ≥ `d_pass_threshold`, the parser adjusts
  the two so that `pass > warn` is always true (never an error, always silent).

**Module self-registration** (`"ikk_end_tag"` key):
```cpp
// In src/vsim/analysis/ikk_end_tag.cpp (at file scope):
static vsim::AutoRegister<IkkEndTagModule, IAnalysisModule> s_reg("ikk_end_tag");
```
The runtime creates and dispatches this module via `ModuleRegistry<IAnalysisModule>`
with no hard-coded `if` / `switch` chains.

**Output written to `AnalysisRecord`:**
| Field | Content |
|---|---|
| `ikk_end_tag_md` | Markdown fenced block (empty when `emit_markdown=false`) |
| `ikk_end_tag_tex` | LaTeX `\ikkendsection{}{}` call (empty when `emit_latex=false`) |

**Implementation:** `include/vsim/analysis/ikk_end_tag.hpp` +
`src/vsim/analysis/ikk_end_tag.cpp` · Tests: Group 87 (20 cases)

---

### `[analysis.ivec]`  — WO-75B Phase 1

**Purpose:** Compute per-frame IKK identity-vector statistics (Ī_f ∈ ℝ⁵) from
the `IdentitySidecarSeries` and write them to `<run_id>.identity.json`.

**Parser key:** `analysis.ivec`  
**Added:** WO-75B Phase 1  
**Doctrine:** All values are derived from sidecar fields only. They are never
written back to truth-state (`.xyz` / `.xyzFull`) files.

**Axis ordering (non-permutable):**

| Index | Axis | Semantic | Phase 1 proxy (from sidecar) |
|---|---|---|---|
| 0 | x | existence (S0) | `1 - dataloss` |
| 1 | y | EM (S2/S3) | `1 - hidden_channel` |
| 2 | z | spatial (S3) | `recoverable_info` |
| 3 | t | temporal (S4) | `1 - projection_loss` |
| 4 | w | internal (spin) | `1 - identity_residual` |

```toml
[analysis.ivec]
enabled       = true    # default: false
write_json    = true    # write <run_id>.identity.json
include_delta = true    # include ΔĪ_f per frame
include_var   = false   # include diag(Var_f) per frame (always 0 in Phase 1)
output_dir    = ""      # override output dir; empty = use [output] output_dir
```

**Output:** `<output_dir>/<run_id>.identity.json`

```json
{
  "run_id": "...",
  "frame_count": N,
  "run_mean":   { "x": 0.0, "y": 0.0, "z": 0.0, "t": 0.0, "w": 0.0, "mag": 0.0 },
  "run_var_diag": { ... },
  "drift_per_frame": { ... },
  "frames": [
    { "frame_index": 0, "time_fs": 0.0,
      "mean": { "x": ..., "mag": ... },
      "var":  { "x": ..., ... },
      "delta": { "x": ..., "norm": ..., "valid": false } },
    ...
  ]
}
```

**Implementation:** `include/vsim/analysis/ikk_identity_vector.hpp` +
`src/vsim/analysis/ikk_identity_vector.cpp` · Tests: Group 88 (28 cases)

---

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

## Empirical Verification ([verify] - WO-62A)

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

For NaCl rocksalt (a = 5.6 A):

| Shell | r (A) | Relation |
|---|---|---|
| 1st (Na-Cl) | 2.800 | a/2 |
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

---

## Organic Domain Shorthand (`[chemistry]` - domain / sequence)

The `[chemistry]` block supports a `domain` + `sequence` shorthand that lets you
describe complex molecules without writing a Hill formula by hand. The VSIM parser
expands the input through `expand_organic_formula()` and injects the result into
`simulation.molecules` as the primary species (index 0).

### Supported domains

| `domain` value | `sequence` input | Expansion strategy |
|---|---|---|
| `"peptide"` | One-letter amino acid codes, all uppercase (e.g. `"ACDEFG"`) | 20-residue backbone table; each residue contributes its backbone-subtracted Hill formula; one H2O added for terminal caps |
| *(any)* | Trivial/common name, lowercase (e.g. `"caffeine"`, `"glucose"`) | Trivial-name lookup table |
| *(any)* | Condensed formula (e.g. `"C6H12O6"`, `"CH4"`) | Hill-order canonicalization |

If only `sequence` is set and `domain` is omitted, the parser auto-infers
`domain = "peptide"` when every character in `sequence` is a valid one-letter
amino acid code (uppercase).

### Molecule injection rules

1. The expanded Hill formula is stored in `chemistry.expanded_formula` (read-only).
2. `material.formula` is back-filled with the expanded formula **only** if the script
   did not set it explicitly - so registry-resolver lookups still work.
3. A `MoleculeEntry` with `count = 1` and `temperature_K` from `[environment]` is
   **prepended** to `simulation.molecules` unless a molecule with the same formula
   is already declared in the script.
4. Explicit `[[simulation.molecule]]` blocks are always preserved in declaration order
   after the injected primary species.

### Residue table (peptide domain)

Each amino acid contributes its residue formula (free amino acid ? H2O):

| Code | Name | Residue formula |
|---|---|---|
| A | Alanine | C3H5NO |
| C | Cysteine | C3H5NOS |
| D | Aspartic acid | C4H5NO3 |
| E | Glutamic acid | C5H7NO3 |
| F | Phenylalanine | C9H9NO |
| G | Glycine | C2H3NO |
| H | Histidine | C6H7N3O |
| I | Isoleucine | C6H11NO |
| K | Lysine | C6H12N2O |
| L | Leucine | C6H11NO |
| M | Methionine | C5H9NOS |
| N | Asparagine | C4H6N2O2 |
| P | Proline | C5H7NO |
| Q | Glutamine | C5H8N2O2 |
| R | Arginine | C6H12N4O |
| S | Serine | C3H5NO2 |
| T | Threonine | C4H7NO2 |
| V | Valine | C5H9NO |
| W | Tryptophan | C11H10N2O |
| Y | Tyrosine | C9H9NO2 |

Terminal cap adds H2O to the total.

### Full example

```vsim
[chemistry]
domain   = peptide
sequence = ACDEFG   # -> C26H36N6O10S (Ala-Cys-Asp-Glu-Phe-Gly hexapeptide)

[environment]
temperature = 310.0

[[simulation.molecule]]
formula = N2
count   = 12

[[simulation.molecule]]
formula = H2O
count   = 50
```

Resulting `simulation.molecules` after parsing:

| Index | Formula | Count | Source |
|---|---|---|---|
| 0 | C26H36N6O10S | 1 | auto-injected from domain expansion |
| 1 | N2 | 12 | explicit script block |
| 2 | H2O | 50 | explicit script block |

---

## Data Sampling Depth  (v5.0.2 — WO-VSIM-DATA-01)

This chapter describes how to maximise the data collected by a VSIM run.
The design goal is *script-declared data density*: the user turns on exactly
the probes and output flags they need; the runtime emits those and nothing more.

### Philosophy

| Concern | Control |
|---|---|
| **What physics is measured** | `[observe] metrics` — the observable catalogue |
| **How often samples are taken** | `[observe] every_n_steps` |
| **What files are written** | `[export]` flags / named export profile |
| **What rendered artifacts are produced** | `[export.visual]` flags |
| **What statistical probes run post-sim** | `[variance]`, `[N_evolution]` |
| **What pipeline analysis runs** | `[analysis.sampling]`, `[analysis.scale_sampling]`, `[analysis.inference]` |
| **What pass/fail gates apply** | `[verify.*]` blocks |

These seven layers are orthogonal. Any combination is valid.

---

### Sampling depth levels

| Level | Config | Data produced |
|---|---|---|
| **0 — minimal** | `write_xyz = true` only | Final positions only; no metrics |
| **1 — standard** | export profile `standard` + `[observe]` basics | XYZ + analysis JSON + TSV report |
| **2 — research** | profile `research_report` + `[analysis.sampling]` + `[variance]` | Adds events, RDF/MSD, variance series |
| **3 — publication** | profile `publication` + all analysis sections + `[verify.*]` | Full audit trail, scale sampling, verification report |
| **4 — max** | all export flags + `[observe]` full catalogue + all analysis sections | Maximum dataset; use for first-pass exploration |

---

### Widened Observable Catalogue  (v5.0.2)

The following observable names are valid in `[observe] metrics`. The kernel
emits a record per particle group per sampled step for each enabled metric.

#### Structural observables

| Key | Unit | Description |
|---|---|---|
| `rdf` | dimensionless | Radial distribution function g(r) |
| `coordination` | integer | Per-particle coordination number |
| `cn_distribution` | histogram | CN distribution P(CN) per species |
| `bond_angle` | degrees | Per-triplet bond-angle histogram |
| `angular_distribution` | degrees | Full angular distribution function ADF |
| `pair_distribution` | dimensionless | Pair distribution (alias: rdf) |
| `order_param` | 0–1 | Orientational order parameter η |
| `packing_fraction` | 0–1 | Local volume packing fraction φ |
| `defect_map` | count/grid | Defect site density on grid |
| `cluster_count` | integer | Number of distinct clusters |
| `phase_label` | enum | Per-particle phase assignment: solid / liquid / gas |

#### Dynamical / transport observables

| Key | Unit | Description |
|---|---|---|
| `msd` | Å² | Mean-squared displacement vs frame |
| `displacement` | Å | Per-particle displacement from reference position |
| `diffusion_proxy` | Å²/frame | Self-diffusion coefficient proxy D* |
| `velocity_autocorr` | Å²/fs² | Velocity autocorrelation function VACF |
| `radial_velocity` | Å/fs | Radial velocity component histogram |
| `thermal_conductivity_proxy` | W/(m·K) proxy | Green–Kubo heat-flux correlation proxy |

#### Energy / mechanical observables

| Key | Unit | Description |
|---|---|---|
| `energy_map` | kcal/mol | Per-particle potential energy field |
| `kinetic_energy` | kcal/mol | Per-particle kinetic energy |
| `potential_energy` | kcal/mol | Alias: `energy_map` |
| `energy_drift` | kcal/mol/step | Energy drift per step (stability check) |
| `virial_stress` | GPa proxy | Per-particle virial stress tensor |
| `pressure_field` | GPa | Local pressure tensor field P(r) |
| `temperature_field` | K | Local temperature field T(r) |

#### Excitation / spectral observables

| Key | Unit | Description |
|---|---|---|
| `spectral_response` | arb. | Frequency-domain response post-excitation |
| `interference` | arb. | Quantum interference pattern (laser excitation path) |

**Recommendation for a full first-pass run:**

```toml
[observe]
metrics = [
  "energy_map", "coordination", "rdf", "msd", "displacement",
  "packing_fraction", "virial_stress", "diffusion_proxy",
  "order_param", "phase_label", "cluster_count", "defect_map",
  "velocity_autocorr", "angular_distribution", "bond_angle",
  "cn_distribution", "thermal_conductivity_proxy", "radial_velocity",
  "energy_drift", "temperature_field"
]
output_format = "json"
every_n_steps = 25
```

---

### `[analysis.sampling]` — widened reference

```toml
[analysis.sampling]
enabled               = true
compute_rdf           = true
compute_msd           = true
min_frames_for_motion = 2
min_frames_for_msd    = 10
unwrap_pbc            = true   # required for diffusion in PBC runs
```

| Field | Type | Default | Notes |
|---|---|---|---|
| `enabled` | bool | `false` | Enable this analysis pass |
| `compute_rdf` | bool | `true` | Radial distribution function |
| `compute_msd` | bool | `true` | Mean-squared displacement series |
| `min_frames_for_motion` | int | `2` | Minimum trajectory frames to attempt motion analysis |
| `min_frames_for_msd` | int | `10` | Minimum frames for a meaningful MSD curve |
| `unwrap_pbc` | bool | `false` | Unwrap PBC image coordinates before MSD (required for diffusion) |

---

### `[analysis.scale_sampling]` — widened reference

Scale sampling bridges particle-level data to field-level representations.
Use it whenever you need field projections, RVE windows, or emergence metrics.

```toml
[analysis.scale_sampling]
enabled                          = true
compute_field_projection         = true
compute_rve_sampling             = true
compute_emergence_metrics        = true
field_grid                       = [12, 12, 12]   # finer grid = more spatial resolution
min_particles_for_scale_sampling = 64
rve_windows_per_level            = 24
rve_window_placement             = "stratified"   # grid | stratified | random
temporal_drift_metric            = "block_difference"
scale_drift_metric               = "successive_window_difference"
spatial_cv_threshold             = 0.15
temporal_drift_threshold         = 0.08
scale_drift_threshold            = 0.12
```

| Field | Type | Default | Notes |
|---|---|---|---|
| `field_grid` | `[nx, ny, nz]` | `[8,8,8]` | Field projection grid. `[12,12,12]` recommended for >500 atoms |
| `rve_windows_per_level` | int | `16` | RVE windows per length scale. Increase for better statistics |
| `rve_window_placement` | string | `"grid"` | `"stratified"` gives best spatial coverage |
| `spatial_cv_threshold` | float | `0.20` | Coefficient of variation threshold for spatial uniformity |
| `temporal_drift_threshold` | float | `0.10` | Maximum allowed temporal drift fraction |
| `scale_drift_threshold` | float | `0.15` | Maximum allowed scale-to-scale drift fraction |

---

### Variance and N_evolution probes

`[variance]` and `[N_evolution]` are **loop-control and data-collection** probes.
They feed `[while]` / `[until]` stop conditions AND emit their own time series.

```toml
[variance]
print_results = true

# compact form:  <probe_name> = "<field>" "<window>" <threshold>
energy_var       = "energy.total"  "last 100"  0.010
displacement_var = "displacement"  "last 100"  0.050
order_var        = "eta"           "last 100"  0.020
coord_var        = "coordination"  "last  50"  0.100
```

```toml
[N_evolution]
print_results = true

cluster_growth = "cluster_count"  "last 80"  0.05
vapor_fraction = "vapor"          "last 80"  0.02
event_rate     = "event_count"    "last 50"  0.0
```

**Field tokens for variance:**

| Token | Measured quantity |
|---|---|
| `energy.total` | Total potential energy per frame |
| `position.x` / `position.y` / `position.z` | Coordinate component across particles |
| `displacement` | Per-particle displacement from initial position |
| `eta` | Order parameter η per frame |
| `coordination` | Average coordination number per frame |
| `result` | Event `result_value` series |

**Target tokens for N_evolution:**

| Token | Population tracked |
|---|---|
| `cluster_count` | Number of clusters |
| `defect_count` | Number of defect sites |
| `particle_count` | Total particle count |
| `event_count` | Total kernel events |
| `vapor` / `solid` / `liquid` | Particles in a given phase label |

---

### `[thermal_probe]` — regional thermal / heat-flux sampling  (v5.0.2 — WO-OUTPUT-PHASE2)

Collects per-region temperature gradients, heat-flux vectors, and thermal-conductivity
proxies at a configurable step interval.  Results are appended to the analysis layer.

```toml
[thermal_probe]
enabled                     = true
compute_gradient            = true   # dT/dx, dT/dy, dT/dz
compute_heat_flux           = true   # J_x, J_y, J_z (kinetic + virial)
compute_conductivity_proxy  = true   # kappa estimate via Green-Kubo HACF
compute_local_temperature   = true   # per-bin mean kinetic temperature
compute_temperature_variance = true  # variance of per-atom temperature
compute_heat_capacity_proxy = false  # dE/dT finite-difference (expensive)
every_n_steps = 50
gradient_dx   = 1.5        # finite-difference width in Angstrom
region_bins   = 6          # spatial bins per axis
region        = "all"      # "all" | region label
output_format = "json"     # "json" | "tsv"
```

| Field | Type | Default | Notes |
|---|---|---|---|
| `enabled` | bool | `false` | Must be `true` to activate |
| `compute_gradient` | bool | `true` | Spatial temperature gradient |
| `compute_heat_flux` | bool | `true` | Heat-flux vector via virial + kinetic |
| `compute_conductivity_proxy` | bool | `true` | Green-Kubo kappa proxy |
| `compute_local_temperature` | bool | `true` | Per-bin mean temperature |
| `compute_temperature_variance` | bool | `true` | Per-bin temperature variance |
| `compute_heat_capacity_proxy` | bool | `false` | dE/dT estimate (double the sampling cost) |
| `every_n_steps` | int | `50` | Sampling interval |
| `gradient_dx` | float | `1.0` | Finite-difference width (Å) |
| `region_bins` | int | `8` | Spatial bins along each axis |
| `region` | string | `"all"` | Region label or `"all"` |
| `output_format` | string | `"json"` | `"json"` or `"tsv"` |

---

### `[field_probe]` — stress tensor / density field sampling  (v5.0.2 — WO-OUTPUT-PHASE2)

Collects volumetric mechanical fields (stress tensor, von Mises, density, pressure)
on a configurable voxel grid.  Provides the field data required for continuum upscaling.

```toml
[field_probe]
enabled                    = true
compute_stress_tensor      = true   # full 3x3 virial stress per voxel
compute_stress_eigenvalues = true   # principal stresses sigma1, sigma2, sigma3
compute_von_mises          = true   # von Mises stress scalar
compute_density_field      = true   # volumetric number / mass density
compute_pressure_field     = true   # local pressure P = -Tr(sigma)/3
compute_velocity_field     = false  # mean velocity per voxel (optional)
compute_charge_density     = false  # requires charge on particles
compute_electric_field     = false  # Ewald/PPPM E-field (reserved)
every_n_steps              = 50
output_format              = "json"  # "json" | "tsv" | "vtp"
write_vtp                  = false   # write VTK PolyData alongside JSON
```

| Field | Type | Default | Notes |
|---|---|---|---|
| `enabled` | bool | `false` | Must be `true` to activate |
| `compute_stress_tensor` | bool | `true` | Full 3×3 virial stress tensor |
| `compute_stress_eigenvalues` | bool | `true` | Principal stresses σ1,σ2,σ3 |
| `compute_von_mises` | bool | `true` | Von Mises scalar |
| `compute_density_field` | bool | `true` | Volumetric number/mass density |
| `compute_pressure_field` | bool | `true` | Local pressure |
| `compute_velocity_field` | bool | `false` | Mean velocity per voxel |
| `compute_charge_density` | bool | `false` | Charge density (requires charged particles) |
| `compute_electric_field` | bool | `false` | E-field (reserved; requires Ewald/PPPM) |
| `every_n_steps` | int | `50` | Sampling interval |
| `output_format` | string | `"json"` | `"json"`, `"tsv"`, or `"vtp"` |
| `write_vtp` | bool | `false` | Emit VTK PolyData file (`.vtp`) in addition to JSON |

Both probes are fully compatible with all material classes, loop types, and batch sweeps.

---

### Visual output profiles  (v5.0.2 — WO-OUTPUT-PHASE2)

Named profiles for `[export.visual]` can be applied at runtime via `visual_profile` in `[batch.expand]`.

| Profile | Effect |
|---|---|
| `"minimal_visual"` | All visual outputs disabled |
| `"figures_only"` | Static SVG/PNG figures only; no animation, no HTML |
| `"web_only"` | `write_html_dashboard` + `write_report_html` only |
| `"max_sampling_visual"` | All static figures + animation GIFs + HTML dashboard + HTML report |

---

### Batch expand and axis subsections  (v5.0.2 — WO-OUTPUT-PHASE2)

`[batch.expand]` declares a named export/visual profile shortcut applied to every case
generated from `[[batch.axis]]` blocks.

```toml
[batch.expand]
export_profile = "max_sampling"          # named export profile for all cases
visual_profile = "max_sampling_visual"   # named visual profile for all cases

[[batch.axis]]
name   = "formula"
values = ["Fe", "H2O", "Ar", "Al2O3"]

[[batch.axis]]
name   = "temperature"
values = [500, 1000, 2000]
```

The axes form a factorial cross-product of cases.  Each case receives the resolved
`export_profile` and `visual_profile` flags, the shared thermal/field probe config,
and the global analysis pipeline sections.

| Field (`[batch.expand]`) | Type | Notes |
|---|---|---|
| `export_profile` | string | Named export profile (`max_sampling`, `publication`, …) |
| `visual_profile` | string | Named visual profile (`max_sampling_visual`, `figures_only`, …) |

| Field (`[[batch.axis]]`) | Type | Notes |
|---|---|---|
| `name` | string | Axis label (becomes part of case ID) |
| `values` | list | Discrete values to sweep across |
| `seed_offset` | int | Per-axis seed shift (default `0`) |

---

### Reference scripts summary  (v5.0.2)

| Script | Material system | Purpose |
|---|---|---|
| `suite_metal_steam_properties.vsim` | Fe BCC + H2O steam | Canonical metal+steam max-sampling reference |
| `suite_gas_plasma_properties.vsim` | Ar noble gas + N2 plasma-proxy | Gas/plasma class max-sampling reference |
| `suite_ceramic_oxide_properties.vsim` | Al2O3 corundum + MgO rocksalt | Ceramic oxide max-sampling reference (Ewald, charge density, VTP) |
| `suite_batch_sweep_max_sampling.vsim` | Fe / H2O / Ar / Al2O3 × 3 temperatures | Multi-material batch grid, 12 cases, max_sampling profiles |

---

### Dual-phase metal + steam reference: `suite_metal_steam_properties.vsim`

`scripts/suite_metal_steam_properties.vsim` is the canonical max-sampling
reference script for a generic metal solid plus steam gas run.

| Section | Config |
|---|---|
| Phase A | Fe BCC, 8×8×8 supercell, 1024 atoms, `formation_preset = "metallic"`, periodic PBC |
| Phase B | H2O steam, 400 molecules (1200 atoms), open region, `velocity_drift = 0.15` |
| Temperature | Metal: 1100 K ramp to 1400 K; steam: 650 K ramp to 900 K |
| Observables | 20 metrics, `every_n_steps = 25` |
| Variance probes | 6 probes across both phases |
| N_evolution probes | 5 probes including `vapor_fraction`, `solid_fraction`, `event_rate` |
| Loops | `[while]` metal equilibration; `[until]` steam diffusion; `[loop]` 16-iteration temperature ramp with 4 sweeping variables |
| Analysis | `analysis.sampling` + `analysis.scale_sampling` (12×12×12 grid, 24 RVE windows) + `analysis.inference` |
| Verification | `verify.structure`, `verify.rdf`, `verify.msd`, `verify.mass` |
| Export | All data flags enabled; full visual export (SVG figures, GIF, HTML dashboard, HTML report) |
| Output tree | `out/metal_steam_suite/` with `pipeline/` and `figures/` subdirectories |

Run it with:

```
VSEPR scripts/suite_metal_steam_properties.vsim
```

---

### Output file tree (max_sampling profile)

```
out/<run_name>/
  <name>.xyz                     # final positions
  <name>.xyzf                    # full trajectory
  <name>.xyzfull                 # full state history
  <name>.pdb                     # PDB for VESTA / VMD
  <name>.analysis.json           # AnalysisRecord (coordination, RDF peaks, ...)
  <name>.metrics.tsv             # per-run scalar table
  <name>.cluster.json            # ClusterRecord
  <name>.fingerprint.json        # FingerprintRecord
  <name>.events.jsonl            # KernelEventLog (JSON Lines)
  <name>.symbolic_trace.json     # SymbolicTrace per-event
  <name>.report.md               # human-readable Markdown summary
  <name>.summary.csv             # per-run summary CSV
  <name>.dashboard.json          # DashboardRecord
  <name>.manifest.json           # artifact registry
  <name>.dashboard.svg           # pipeline dashboard (diffable SVG)
  <name>.pipeline_audit.jsonl    # stage-by-stage audit JSONL
  <name>.vtp                     # VTK PolyData mesh
  pipeline/
    <prefix>.structure.json      # StructureInferenceResult
    <prefix>.property_sampling.json
    <prefix>.scale_sampling.json
    <prefix>.property_inference.json
    <prefix>.sampling_manifest.json
  verify/
    <prefix>.verify_report.json  # per-check status + detail
    <prefix>.verify_summary.tsv
  figures/
    <name>.rdf.svg
    <name>.energy_trace.svg
    <name>.packing_heatmap.svg
    <name>.defect_map.svg
    <name>.cluster_map.svg
    <name>.trajectory.gif
    <name>.overlay_cycle.gif
    <name>.dashboard.html
    <name>.report.html
```

---

A `.X` file is a **suite execution container** - a plain-text archive that embeds one or more `.vsim` scripts and optional asset files into a single file.

### File structure

    XBUNDLE <version>
    [manifest]
      name         = <string>         # required
      description  = <string>         # optional
      author       = <string>         # optional
      created      = <ISO-8601>       # optional
      entry_point  = <member-name>    # optional; default = first vsim member
    [[member]]
      name  = <logical-name>  # required; unique
      kind  = vsim | asset    # default: vsim
      path  = <orig-path>     # optional; informational
      size  = <byte-count>    # optional; validated if present
      >>>
      <verbatim content>
      <<<

### Comments and blank lines

Lines where the first non-whitespace character is `#` are comments and are ignored everywhere except inside a body block. Blank lines are also ignored outside body blocks.

### Entry point resolution

When `entry_point` is set, `sepr xbundle run` executes that named member. When it is absent, the first member with `kind = vsim` is executed.

### Validation rules

V-01 manifest present; V-02 manifest.name non-empty; V-03 >= 1 entry; V-04 entry names non-empty; V-05 unique names; V-06 vsim content non-empty; V-07 entry_point names existing member; V-08 declared size matches content length.

### CLI

    vsepr xbundle run      <file.X> [--entry <name>]
    vsepr xbundle validate <file.X>
    vsepr xbundle list     <file.X>

### Role in the pipeline

| Format | Role |
|--------|------|
| `.vsim` | Single simulation script |
| `.X` | Bundled suite execution container |
| `.dynx` | Post-compiled live visual / session archive |