# VSIM Analysis Pipeline — 8-Part Reference
<!-- VSEPR-SIM v5.0.0-main | WO-J | Day 77 -->

| Field           | Value                                                              |
|---|---|
| **Document**    | `VSIM_ANALYSIS_PIPELINE.md`                                        |
| **Version**     | v5.1.13.5                                                          |
| **WO**          | WO-J (analysis text documentation)                                 |
| **Theory refs** | IKK I §2, IKK III, MIR II, v5114_bridge_paper.md §3–7             |
| **Showcase**    | `docs/scripts/vsim_3d_demo_showcase.vsim`                          |
| **Status**      | Authoritative — 8 parts, maps script blocks to theory and kernel   |

---

## Overview

The VSIM analysis pipeline transforms a simulation state `X_MD(t)` into a layered
evidence tree — from raw structural geometry up to macro-level inference.  It is
activated by the `[analysis.*]` section family in any `.vsim` script and executed
**after** the primary MD or single-point run completes.

Pipeline operator chain:

```
S_op  →  P_op  →  M_op  →  I_op
```

| Operator | Name              | Output                                        |
|---|---|---|
| `S_op`   | Structure         | bonds, coordination numbers, NN distances     |
| `P_op`   | Sampling          | RDF, MSD (if multi-frame), pair stats         |
| `M_op`   | Scale Sampling    | 3D field projection, RVE windows, cv metrics  |
| `I_op`   | Inference         | scale evidence, macro_ready gate, properties  |

The operators run in strict sequence.  Each one writes a JSON sidecar that the
next operator may read.  All outputs are emitted to `output_dir` as declared in
`[export]`.

---

## Part 1 — Structure Analysis (`[analysis.structure]`)

**Theory:** MD Bridge State §3, `X_MD(t).B(t)` bond state component.

**Purpose:** Compute the bonding topology of the frozen or current frame.

**Script block:**

```vsim
[analysis.structure]
enabled           = true
neighbor_cutoff_A = 5.6    # search radius for neighbour listing (Angstrom)
contact_cutoff_A  = 3.0    # bond / contact threshold
```

**What it produces:**
- Per-particle coordination number `z_i`
- Neighbour list (used by all downstream operators)
- Bond state matrix `B(t)` (active bonds, bond lengths, bond types)

**Key outputs:** `analysis_json["structure"]`, `metrics_tsv["coord_*"]`

**NaCl reference values:** `neighbor_cutoff_A = 5.6` ≈ 2× the 2.82 Å bond length.
At 300 K every Na/Cl should show `z_i = 6` (B1 rocksalt coordination).
Deviation triggers a `[verify.structure]` failure.

**When to tune:**
- Lower `contact_cutoff_A` for molecular crystals with weak intermolecular contacts.
- Raise `neighbor_cutoff_A` for loose structures or large unit cells.

---

## Part 2 — Sampling Analysis (`[analysis.sampling]`)

**Theory:** MD Bridge State §3, `X_MD(t).E(t)` energy traces; IKK I §2.3 scale S3.

**Purpose:** Compute statistical distributions over the particle ensemble.

**Script block:**

```vsim
[analysis.sampling]
enabled               = true
compute_rdf           = true
compute_msd           = false      # disable for single-frame (no trajectory)
min_frames_for_msd    = 999        # gate: MSD requires >= N frames
unwrap_pbc            = true
```

**What it produces:**
- **RDF** `g(r)` — radial distribution function, peak positions in Å
- **MSD** `<|Δr|²>(t)` — mean-squared displacement (multi-frame only)
- Pair statistics: avg NN distance, second-shell distance, peak broadening

**MSD gate rule (required for single-point runs):**

```vsim
compute_msd        = false
min_frames_for_msd = 999
```

Both lines are required.  `compute_msd = false` is the primary guard.
`min_frames_for_msd` is a secondary hard gate that blocks accidental MSD
computation if `compute_msd` is inadvertently set to `true` downstream.

**NaCl reference values:**
- RDF peak 1: 2.82 Å (Na–Cl NN)
- RDF peak 2: 3.99 Å (Na–Na / Cl–Cl NNN = a/√2 for a = 5.64 Å)
- RDF peak 3: 4.88 Å (Na–Cl second shell = a√3/2)

**PBC unwrapping:** Always enable `unwrap_pbc = true` for periodic systems.
Without unwrapping, particles near the cell boundary appear to have large
nearest-neighbour distances (image artefact).

---

## Part 3 — Scale Sampling (`[analysis.scale_sampling]`)

**Theory:** MIR II scale-sampling policy; IKK I §2, Scale Ladder §2; `v5114_bridge_paper.md` §2.

**Purpose:** Project the atomistic field onto a 3D volumetric grid and scan
Representative Volume Element (RVE) windows at multiple length scales.

**Script block:**

```vsim
[analysis.scale_sampling]
enabled                          = true
compute_field_projection         = true
compute_rve_sampling             = true
compute_emergence_metrics        = true
field_grid                       = [8, 8, 8]
rve_window_lengths_A             = [2.82, 5.64, 11.28, 22.56]
rve_windows_per_level            = 8
rve_window_placement             = "grid"
min_particles_for_scale_sampling = 64
spatial_cv_threshold             = 0.30
temporal_drift_threshold         = 0.10
scale_drift_threshold            = 0.25
scale_drift_metric               = "successive_window_difference"
```

**Field grid:** `[8, 8, 8]` creates a 512-cell volumetric density field.
Each cell accumulates mass, charge, coordination, and energy contributions
from particles within its spatial extent.  The resulting field is the `energy_map`
observable emitted by `[observe]`.

**RVE windows:** Four window sizes anchored to NaCl shell multiples:
- Level 0: 2.82 Å — single bond length (atomistic)
- Level 1: 5.64 Å — unit cell edge
- Level 2: 11.28 Å — 2× unit cell (mesoscale onset)
- Level 3: 22.56 Å — 4× unit cell (near-macro)

The scale sampler tests whether bulk properties converge as the window grows
(i.e., whether the `spatial_cv` falls below `spatial_cv_threshold` at the largest window).
Convergence signals **macro emergence** and unlocks the `I_op` macro_ready gate.

**Thresholds:**
- `spatial_cv_threshold = 0.30` — coefficient of variation for density uniformity
- `scale_drift_threshold = 0.25` — successive RVE window difference metric
- `temporal_drift_threshold = 0.10` — tight for single-point (no temporal evolution)

**Outputs:** `write_scale_sampling_json = true` in `[export]`

---

## Part 4 — Inference (`[analysis.inference]`)

**Theory:** IKK I §2.3 scale identity ladder; `v5114_bridge_paper.md` §2, §7.

**Purpose:** Apply rule-based hard gates over the scale-sampling evidence to
determine whether the system has achieved `macro_ready` emergence and to map
defect state to estimated material properties.

**Script block:**

```vsim
[analysis.inference]
enabled = true
mode    = "rule_based_61d"
```

**Mode `rule_based_61d`:**  The 61-dimensional rule evaluator applies hard gates
in sequence:

1. Structural prototype match (`expected_prototype`)
2. Coordination number within tolerance
3. RDF peak positions within tolerance
4. Spatial CV below `spatial_cv_threshold` at the largest RVE window
5. Scale drift below `scale_drift_threshold`
6. Defect severity `s_a` below per-type thresholds

All six gates must pass for `macro_ready = true`.

**Outputs:** `analysis_json["inference"]["macro_ready"]`, `properties` table with
`[rho, D_eff, k_eff, E_eff, G_eff, K_eff]`.

**Defect-to-property map** (from `v5114_bridge_paper.md §7`):

```
P_mat = Pi_P(T, D)
P_mat = [ rho, D_eff, k_eff, E_eff, G_eff, K_eff, phi, kappa, eta_f ]
```

---

## Part 5 — Observables (`[observe]`)

**Theory:** Event-energy state §5, `A_i^n` per-atom energy-energy state.

**Purpose:** Emit live named scalar metrics at each simulation step.  Used by
the GL viewer overlay and the workspace panels.

**Script block:**

```vsim
[observe]
metrics       = ["energy_map", "coordination", "rdf"]
output_format = "json"
every_n_steps = 1
```

**Metric names:**
- `energy_map` — 3D volumetric energy density field (feeds `gl_overlay_cycle`)
- `coordination` — per-particle coordination number series
- `rdf` — full `g(r)` histogram at each emit step

**Emit rate:** `every_n_steps = 1` emits on every step.  For long MD runs,
set `every_n_steps = 10` or higher to reduce output volume.

**GL viewer binding:** The `[visual] overlay_sequence` field must name exactly
the same metric keys:

```vsim
[visual]
overlay_sequence = energy_map,coordination,rdf
```

If a key in `overlay_sequence` is not in `[observe] metrics`, the overlay
panel for that key will be blank.

---

## Part 6 — Kernel + Event Trace (`[kernel]`)

**Theory:** `v5114_bridge_paper.md` §5–6: per-atom event-energy state, event limiter.

**Purpose:** Record the event stream from the simulation kernel for
post-analysis, reporting, and viewer timeline panels.

**Script block:**

```vsim
[kernel]
pass_through        = true
symbolic_trace      = true
event_registry      = true
continual_reporting = true

[kernel.trace]
formation_events = true
defect_events    = true
transport_events = true
dynamic_energy   = true
```

**Event channels** (from `v5114_bridge_paper.md §13.4`):

| Channel                 | What is captured                       |
|---|---|
| `ParticleParticleEvent` | Classical collision / interaction      |
| `ChemistryBondEvent`    | Bond formation / breaking              |
| `DecayEvent`            | Single-particle decay chains           |
| `AnnihilationEvent`     | Anti-pair annihilation (WO-66J)        |

**Event limiter rule (non-negotiable):** Console events may be rate-limited
(see §6 of bridge paper).  The JSONL event archive is **never** sampled:

```
E_jsonl    = E          (full archive — never sampled)
E_console  ⊆ E_jsonl   (console is a subset)
```

Enable with `write_events_json = true` in `[export]`.

**Symbolic trace:** When `symbolic_trace = true`, every kernel gate transition
is logged as a symbolic expression (`H(...)` notation matching `v5114_bridge_paper.md §5`).
This is the ground truth for debugging gate failures.

---

## Part 7 — Verification Suite (`[verify]`)

**Theory:** MIR II recoverability criterion; bridge paper §3 Velocity Verlet
integrator determinism.

**Purpose:** Assert that the final state matches known-good reference values.
Failures are fatal errors; warnings are non-fatal deviations.

**Script block:**

```vsim
[verify]
enabled             = true
profile             = "demo_showcase_nacl_300k"
write_verify_report = true
write_verify_tsv    = true

[verify.structure]
expected_prototype           = "B1_NaCl"
expected_coordination        = 6
coordination_tolerance       = 0
expected_nearest_neighbor_A  = 2.82
nearest_neighbor_tolerance_A = 0.15

[verify.rdf]
expected_peaks_A   = [2.82, 3.99, 4.88]
peak_tolerance_A   = 0.22
require_peak_order = true

[verify.mass]
enabled            = true
relative_tolerance = 1e-10
```

**Gate semantics:**
- `coordination_tolerance = 0` — zero miscoordinated atoms allowed (strict)
- `nearest_neighbor_tolerance_A = 0.15` — ±0.15 Å around 2.82 Å (5%)
- `peak_tolerance_A = 0.22` — ±0.22 Å per RDF peak
- `relative_tolerance = 1e-10` — mass conservation to floating-point precision

**Profile name:** `profile` is a human-readable tag written to the verify TSV
header so batched QA runs can be filtered by scenario.

**Outputs:** `write_verify_report = true` emits a markdown verify report;
`write_verify_tsv = true` emits a machine-readable TSV for CI ingestion.

---

## Part 8 — Export + Report (`[export]` + `[report]`)

**Theory:** `v5114_bridge_paper.md` §6 artifact preservation rule.

**Purpose:** Emit all output artifacts and generate human-readable reports.
The artifact preservation rule governs what may and may not be sub-sampled:

```
write_xyz / write_xyzf   — scientific truth (never sub-sampled)
write_events_json        — full event archive (never sub-sampled)
write_dashboard_svg      — display output (may be clipped for large runs)
```

**Full showcase export block:**

```vsim
[export]
write_xyz                  = true
write_xyzf                 = true
write_analysis_json        = true
write_metrics_tsv          = true
write_report_md            = true
write_events_json          = true
write_symbolic_trace_json  = true
write_scale_sampling_json  = true
write_dashboard_svg        = true
write_manifest_json        = true
output_dir                 = "out/demo_3d_showcase"
```

**Report block:**

```vsim
[report]
title                   = "VSEPR-SIM 3D Demonstration — NaCl 4×4×4"
include_material_cards  = true
include_metric_tables   = true
include_expected_trends = true
include_symbolic_trace  = true
```

**Manifest:** `write_manifest_json = true` emits a `manifest.json` listing
every output file with its path, size, and SHA-256 hash.  Required for replay
validation and CI artifact checking.

**Dashboard SVG:** Vector dashboard with energy trace, coordination histogram,
RDF plot, and scale-sampling heatmap in a single file.  Viewable in any browser.

---

## Quick Reference — Pipeline to Script Map

| Pipeline step    | Script block             | Theory ref                     |
|---|---|---|
| S_op Structure   | `[analysis.structure]`   | Bridge §3 `B(t)`, IKK I §2.3  |
| P_op Sampling    | `[analysis.sampling]`    | Bridge §3 `E(t)`, IKK I §2.3  |
| M_op Scale       | `[analysis.scale_sampling]` | MIR II, Bridge §2, §7       |
| I_op Inference   | `[analysis.inference]`   | IKK I §2, Bridge §7           |
| Observables      | `[observe]`              | Bridge §5 `A_i^n`             |
| Kernel trace     | `[kernel]` / `[kernel.trace]` | Bridge §5–6              |
| Verification     | `[verify]`               | MIR II recoverability          |
| Export + report  | `[export]` / `[report]`  | Bridge §6 artifact rule        |
