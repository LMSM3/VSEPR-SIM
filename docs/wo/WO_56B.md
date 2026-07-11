# WO-56B — Development Day \#56B: Kernel Check-In

**Branch:** `v5.0.0-beta.7-step-attempt`
**Date:** Day \#56B
**Series:** WO (Work Order / Development Log)
**Status:** Active — beta-7 pipeline arc

---

## 1. Kernel Inventory by Category

All figures exclude `build_test/`, `third_party/`, `.venv/`, `.vs/`, `.git/`, and `out/`.

| Category | Files | Size |
|---|---|---|
| **C++ / H / HPP (native kernel)** | 914 | **15.64 MB** |
| **Python** | 2,449 | **32.49 MB** |
| **Data / DB** (json, csv, xyz, xyzf, xyzc, xyza) | 28,453 | **23.03 MB** |
| **SQLite databases** | 16 | **89.78 MB** |
| **Visual / UI code** (qml, glsl, vert, frag, svg, js, css, html, ui) | 890 | **3.67 MB** |
| **Shell / PS1 / bat / fish** | 99 | **0.53 MB** |
| **Documentation** (tex, md, txt, rst) | 698 | **3.53 MB** |
| **Compiled / media** (pdf, png, jpg, svg) | 483 | **100.69 MB** |
| **Fortran** | 86 | **0.04 MB** |
| **CUDA** | 1 | **0.009 MB** |

> **Total tracked kernel footprint: ~269 MB** (dominated by SQLite + media/PDF artifacts)

---

## 2. Visual Code — Status

Visual code (`.qml`, `.glsl`, `.vert`, `.frag`) is **minimal** in the native VSEPR-SIM kernel:

| File | KB |
|---|---|
| `src/vis/shaders/classic/cylinder.vert` | 4.4 |
| `src/vis/shaders/classic/sphere.frag` | 1.7 |
| `src/vis/shaders/classic/cylinder.frag` | 1.6 |
| `src/vis/shaders/classic/sphere.vert` | 1.1 |

**QML:** 695 files / 3.19 MB total — majority are Qt framework internals from the display layer.  
**Assessment:** Visual code is **not a kernel growth area**. The 4 classic shader files are stable. No QML is actively authored in the beta-7 arc.

---

## 3. C++ / C / H — Kernel Code Assessment

### 3.1 Top 20 Native Kernel Files by Size

| File | KB |
|---|---|
| `apps/qa_golden_tests.cpp` | 98.5 |
| `src/analysis/molecular_census.hpp` | 84.1 |
| `src/core/report_engine.cpp` | 70.9 |
| `apps/nuclear_core_z2_102.cpp` | 70.2 |
| `examples/vsepr_opengl_viewer.cpp` | 61.2 |
| `src/core/bio_report_engine.cpp` | 59.7 |
| `include/gas2/gas2_nuclear.hpp` | 54.9 |
| `src/gas2/gas2_engine.cpp` | 53.5 |
| `src/cli/cmd_build.cpp` | 47.2 |
| `apps/deep_verification.cpp` | 47.1 |
| `coarse_grain/vis/bead_inspector_view.hpp` | 45.9 |
| `coarse_grain/vis/cg_viz_viewer.hpp` | 45.6 |
| `tests/test_ensemble_proxy_suite5.cpp` | 45.4 |
| `apps/qa_random_tests.cpp` | 44.6 |
| `tests/test_property_calibration_suite8.cpp` | 44.6 |
| `apps/nuclear_core_runner.cpp` | 44.1 |
| `tests/test_formation_suite3.cpp` | 44.1 |
| `tests/test_ehd.cpp` | 44.0 |
| `tests/bench_ehd.cpp` | 41.8 |
| `tests/test_property_pipeline_suite7.cpp` | 41.7 |

### 3.2 TODO / FIXME / STUB / Placeholder Count (Top Files)

| File | Count |
|---|---|
| `apps/qa_golden_tests.cpp` | **29** |
| `apps/compute_forces.cpp` | 11 |
| `apps/vsepr-gui/main.cpp` | 11 |
| `atomistic/reaction/engine.cpp` | 10 |
| `tests/pbc_phase5_regression.cpp` | 9 |
| `src/render/scalable_renderer.cpp` | 8 |
| `src/sim/sim_state.cpp` | 8 |
| `src/thermal/thermal_runner.cpp` | 7 |
| `src/v4/uff/uff_autocreate.cpp` | 7 |
| `atomistic/reaction/discovery.cpp` | 7 |

**Critical observation:** `qa_golden_tests.cpp` holds 29 outstanding `PLACEHOLDER_` hash entries (H₂O, NH₃, CH₄, CO₂, SF₆, XeF₄, PCl₅, NaCl, Si, Al-FCC, Fe-BCC, Mg-HCP, Po-SC, CsCl, CaF₂, Al-FCC-Strained…). These are **QA gates that cannot pass** until STRICT-mode hashes are captured from actual runs. This is a known beta-7 open item.

---

## 4. Shell / Scripting

| Category | Files | MB |
|---|---|---|
| PowerShell (`.ps1`) | 31 | 0.18 |
| Shell (`.sh`) | 62 | 0.34 |
| Batch (`.bat`) | 6 | 0.016 |

Scripting layer is lean and appropriate. No anomalies.

---

## 5. Documentation — Size Breakdown

| Category | Size |
|---|---|
| `docs/report/main.tex` | **92.9 KB** — 2,563 lines, 15 material cards, 5 extended dossiers |
| All `.tex` in `docs/` | 2,194.2 KB |
| All `.md` in `docs/` | 205.6 KB |
| All `.pdf` in `docs/` | **70,060.9 KB** (70 MB) — compiled artifact bloat |

> **Note:** PDF artifacts at 70 MB are the dominant documentation weight. These are compile outputs and should not be tracked in git long-term. Consider adding `docs/**/*.pdf` to `.gitignore` after beta-7 closes.

---

## 6. Other

| Type | Files | MB | Notes |
|---|---|---|---|
| `.xyz` trajectory files | 14,240 | 8.98 | Active simulation output |
| `.xyzf` trajectory | 6 | 0.72 | Multi-frame records |
| `.xyzc` / `.xyza` | 6 each | 0.16 / 0.16 | Annotated variants |
| `.sqlite` databases | 16 | 89.78 | Largest single-category binary |
| `.pyc` compiled Python | 2,453 | 44.48 | Cache — not source |
| Fortran (`.f90`, `.f`) | 86 | 0.04 | Legacy, not active kernel |
| CUDA (`.cu`) | 1 | 0.009 | Stub — not wired |

---

## 7. Bead-Era C++ Deep Read — Density-Dependent Behaviour

> Reading target: `coarse_grain/` — bead-era code from 10+ development days prior.

### 7.1 Architecture Overview

The bead-era kernel is organized into these namespaces under `coarse_grain/`:

```
coarse_grain/
  core/         — bead, environment_state, channel_kernels, SH, orientation
  models/       — environment_coupling, seed_bead_stepper, interaction_engine, FIRE
  theory/       — energy_decomposition, material_kernel, graph_topology
  analysis/     — ensemble_proxy, property_pipeline, property_calibration, macro_precursor
  qm/           — qm_descriptors (alpha-proxy, charge modulation)
  chemistry/    — reaction, reaction_engine, reaction_library, species
  metals/       — metal_registry, scattering_pattern, radiation_interaction, Debye
  mapping/      — atom_to_bead_mapper, multi_channel_mapper, fragment_bridge
  database/     — input_record, material_record, precursor_record, seed_hash
  vis/          — bead_inspector_view, cg_viz_viewer, bead_visual_record (largest: 45-46 KB)
  report/       — formation_report, snapshot_graph, metal_gas_study, solidworks_export
  fire_smooth/  — smooth_sample, fire_smooth_runner
  level3/       — level3_builder, level3_bead, level3_handoff
```

### 7.2 The ERB — Environment-Responsive Bead Framework

The core density-dependent mechanism is the **Environment-Responsive Bead (ERB)** framework, defined in `coarse_grain/models/environment_coupling.hpp`:

```
K_k(l, r; η_A, η_B) = K_k(l, r) · (1 + γ_k · η̄)
```

where `η̄ = 0.5 · (η_A + η_B)` — the **mean internal state** (local ordering / density proxy).

**Three density-dependent channels:**

| Channel | γ parameter | Sign | Physical meaning |
|---|---|---|---|
| **Steric** | `gamma_steric` | `> 0` | **Hardening** under compression — steric repulsion increases with crowding |
| **Electrostatic** | `gamma_elec` | `< 0` | **Screening** under crowding — Coulomb interactions weakened by dense environment |
| **Dispersion** | `gamma_disp` | `> 0` | **Enhancement** under ordering — van der Waals strengthens with local order |

**Reference:** `section_environment_responsive_beads.tex`, §6.1 (ERB §8.6 invariant)

**Invariant enforced in code:**
```cpp
// kernel must not change sign
if (factor <= 0.0) factor = 1e-10;
```

### 7.3 Modulation Report — Scalar G Score

`ModulationReport` in `environment_coupling.hpp` produces a single diagnostics scalar:

```
G = (g_steric + g_electrostatic + g_dispersion) / 3
```

| G value | Interpretation |
|---|---|
| G = 1.0 | Unmodulated (γ = 0 or η = 0) |
| G > 1.0 | Net stiffening / enhancement |
| G < 1.0 | Net screening / softening |

This scalar is the cross-module comparison handle — logged, thresholded, and exported.

### 7.4 Pairwise Decomposition — `PairInteraction` (§3.3)

From `coarse_grain/theory/energy_decomposition.hpp`:

```
U_ij^{xy} = U_steric + U_elec + U_disp + U_orient + U_decay-coupled
```

All modulation factors are computed:
```cpp
pair.g_steric = 1.0 + params.gamma_steric * pair.eta_bar;
pair.g_elec   = 1.0 + params.gamma_elec   * pair.eta_bar;
pair.g_disp   = 1.0 + params.gamma_disp   * pair.eta_bar;
```

**Decay-coupled dissipation** (interfacial domain energy):
```
U_decay-coupled = −γ_d · |Δη_ij| · f(r)
```
where `Δη_ij = η_i − η_j` — pairs with mismatched order states experience dissipative coupling that drives them toward compatible environments. This is the CG equivalent of interfacial energy at domain boundaries.

### 7.5 — Phenomenon: Spontaneous Stacking, Electrostatic Screening, Steric Hardening

#### 7.5.1 Spontaneous Stacking

Spontaneous stacking in the bead era emerges from the **orientational coupling** channel (`U_orient`) combined with the dispersion enhancement pathway.

When beads enter close proximity and their **spherical harmonic (SH) overlap integral** produces aligned orientations, dispersion enhancement (`gamma_disp > 0`) reinforces the interaction: ordered beads attract more strongly. This creates a **positive feedback loop**:

```
Local ordering ↑ → η̄ ↑ → g_disp ↑ → U_disp more attractive → beads stack
```

The stacking geometry is encoded in the SH expansion coefficients of the `steric` channel (`unified_descriptor.hpp`): angular order `l_max` and `coeffs[]` define the directional shape of the bead. Stacking is favored when the P₂ orientational dot product is near 1 (face-to-face alignment).

In `main.tex` **graphene / graphite** (Section in Material Cards): AB Bernal stacking registry score is explicitly tracked as an output metric, where stacking mode (AA eclipsed, AB Bernal, turbostratic) is determined by the interlayer registry from trajectory analysis — not pre-assigned.

#### 7.5.2 Electrostatic Screening

Implemented in `environment_coupling.hpp` — Channel: `Electrostatic`, `gamma_elec < 0`.

```
g_elec = 1 + gamma_elec · η̄
```

Since `gamma_elec < 0`: **as η̄ increases (denser / more ordered local environment), g_elec decreases** — electrostatic interactions are suppressed. This is the bead-scale analog of **Debye screening** in dense ionic environments.

In `qm_descriptors.hpp` the charge modulation is:
```cpp
double delta_q = -gamma_elec * env_i.eta * sign_q;
// gamma_elec < 0 → delta_q > 0 in dense environments for cations
```

Crowding reduces the effective charge contrast between beads, suppressing long-range electrostatics and allowing denser packing — consistent with ion-screened behavior in molten salts and polyelectrolytes.

#### 7.5.3 Steric Hardening

Implemented via Channel: `Steric`, `gamma_steric > 0`.

```
g_steric = 1 + gamma_steric · η̄
```

As `η̄` increases (compression, crowding): `g_steric > 1` → the steric repulsion kernel strengthens. The SH-expanded steric channel (`unified_descriptor.hpp`) carries shape anisotropy via `l_max` and `coeffs`. Under compression, the steric wall rises faster than in dilute conditions.

The `fire_smooth_runner.hpp` draws `gamma_steric` from the descriptor:
```cpp
params.env_params.gamma_steric = desc.gamma_steric_drawn;
```
meaning each bead's steric hardening rate is sampled from a calibrated prior — making steric hardening **bead-type specific**, not globally fixed.

This produces **density-dependent stiffness** without encoding stiffness into the state file — the hardening emerges from the interaction during the run.

### 7.6 FIRE / FIRE-smooth Integration

`coarse_grain/models/bead_fire.hpp` and `fire_smooth/smooth_sample.hpp` host the relaxation engine. The smooth sample space draws:
- `tau` (relaxation time)
- `gamma_steric`, `gamma_elec`, `gamma_disp` — all three density-dependent channels

The FIRE minimizer drives beads toward equilibrium while the ERB modulations respond to the evolving `η` landscape in real time. This means **density-dependent effects are active during relaxation** — not just applied post-hoc.

---

## 8. Section 7.5 — `main.tex` Target

The report currently has no `\subsection{7.5}` — the section structure jumps from the material cards (Section 5) through cross-material comparison (Section 6) and discussion (Section 7). **Section 7.5 does not yet exist in `main.tex`.**

Based on this kernel read, Section 7.5 should be drafted as a **Phenomenon Card** inside the Discussion section, covering:

- Spontaneous Stacking as an emergent trajectory event (not pre-assigned)
- Electrostatic Screening as the ERB γ_elec < 0 channel visible in graphite / molten salt cards
- Steric Hardening as density-dependent kernel stiffening in compressed bead systems

> **Action for next session:** Draft `\subsection{Density-Dependent Phenomena: Spontaneous Stacking, Electrostatic Screening, and Steric Hardening}` in `docs/report/main.tex` §7.5 using the bead-era code findings above as the theoretical grounding.

---

## 9. Open Items Identified

| ID | Item | File | Priority |
|---|---|---|---|
| OI-56B-1 | 29 PLACEHOLDER hashes in QA golden tests | `apps/qa_golden_tests.cpp` | HIGH — beta-7 gate |
| OI-56B-2 | Section 7.5 not drafted in `main.tex` | `docs/report/main.tex` | HIGH — this session |
| OI-56B-3 | PDF artifact bloat (70 MB in `docs/`) | `.gitignore` | MEDIUM |
| OI-56B-4 | CUDA stub (1 file, 9 KB, not wired) | `*.cu` | LOW |
| OI-56B-5 | Fortran legacy layer (86 files, 40 KB) | `*.f90`, `*.f` | LOW — audit only |
| OI-56B-6 | `U_ext` (External field) is placeholder | `energy_decomposition.hpp` | MEDIUM — beta-8 |
| OI-56B-7 | PBC minimum-image convention TODO | `apps/qa_golden_tests.cpp:1440` | MEDIUM |

---

## 10. Branch Status

```
HEAD: 0daa00d
Branch: v5.0.0-beta.7-step-attempt
Tag: v5.0.0-beta7-pipeline-wired (5f15290)

Recent arc:
  0daa00d  docs(report): main.tex 46 pages, clean compile
  5f15290  feat(pipeline): beta-7 5-stage pipeline, 377/377 tests pass
  4acea35  chore: bump to v5.0.0-beta-7
  f1587c4  test+docs(vis): batch display suite + docs
```

**Beta-7 pipeline is wired. FormationOutput → DashboardRecord chain is complete. Next work is Section 7.5 draft in main.tex.**

---

*WO-56B generated: kernel deep-dive + bead-era density-dependent behaviour read.*
