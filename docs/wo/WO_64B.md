# WO-64B — VSIM Language Extension: Thermal & Excitation Theoretical Layer

**Status:** Open  
**Type:** Language / Schema / Parser  
**Day:** 64  
**Beta:** v5.0.0-beta.13  
**Branch:** `v5.0.0-main`  
**Scope:** `include/vsim/vsim_document.hpp`, `src/vsim/vsim_parser.cpp`,
`VSIM_REFERENCE.md`, `docs/VSIM_LANGUAGE.md`

---

## Objective

WO-64A makes the thermal theoreticals correct inside the kernel.  
This WO makes them **scriptable**.

The goal is a set of new schema fields and parser handlers that let a `.vsim`
author directly control the validated thermal model — heat-transfer
coefficients, material thermal properties, radial solver geometry, convergence
policy, and excitation type-gating — without writing C++ or touching a trial
config struct.

Every new field follows the 5-step VSIM workflow:

```
define → parse → wire → test → document
```

---

## Doctrine

> **Rule:** Every new VSIM feature defines in `vsim_document.hpp`, parses in
> `vsim_parser.cpp`, wires in apps, tests with a numbered group, and updates
> `VSIM_REFERENCE.md` + `docs/VSIM_LANGUAGE.md`.

New schema fields declared in this WO are additive.  No existing field
changes meaning.  All new fields have explicit defaults that reproduce the
pre-WO-64 behavior so existing scripts do not break.

---

## Background

The Day 63 prototype used `[environment]`, `[excite.thermal_spike]`, and
`[run]` to drive the thermal model indirectly.  Those sections are blunt
instruments for thermal work:

- `[environment]` has no heat-transfer coefficient or geometry fields
- `[excite.thermal_spike]` has no decay model or energy budget
- `[run]` has no radial solver controls
- Convergence policy is a hard-coded magic number in `run_trial()`

This WO introduces a `[thermal]` section and extends `[excite.<type>]` with
type-specific validation guards, giving scripts first-class access to the
corrected model.

---

## New section: `[thermal]`

### Struct — `ThermalSection`

Add to `include/vsim/vsim_document.hpp`:

```cpp
// ============================================================================
// [thermal] section  -  radial FD solver controls
//
// Exposes the 1-D cylindrical thermal solver to .vsim scripts.
// Defaults reproduce pre-WO-64 behavior.
//
// WO-64B
// ============================================================================

struct ThermalSection {
	// Geometry
	double r_inner_m       = 0.02;   // Inner wall radius (m)
	double r_outer_m       = 0.05;   // Outer wall radius (m)
	double length_m        = 1.0;    // Axial length for surface area (m)

	// Boundary conditions
	double h_outer         = 50.0;   // Outer convective coefficient W/(m^2 K)
	double T_ambient_K     = 300.0;  // Ambient / coolant temperature (K)

	// Solver controls
	int    N_radial        = 12;     // Radial grid nodes
	double dt_s            = 0.001;  // Frame timestep (s)
	int    n_frames        = 100;    // Number of frames to run
	bool   Th_decays       = false;  // Allow hot boundary to cool

	// Convergence policy (WO-64A-N3: relative threshold)
	double convergence_rel = 1e-3;   // Relative RMS residual threshold
	int    substep_warn    = 20;     // Emit SubStepOverflow event above this

	// Material override (empty = use [material] section)
	std::string material_key;        // e.g. "NaCl_MgCl2_UCl3", "FLiNaK"

	bool has_geometry() const { return r_outer_m > r_inner_m && r_inner_m > 0.0; }
};
```

Add `ThermalSection thermal;` to `VsimDocument`.

### Parser handler — `apply_thermal_key()`

Add to `src/vsim/vsim_parser.cpp`:

- Register `"thermal"` in the section dispatch table
- Map all `ThermalSection` fields by exact key name
- Unknown keys → `raw_sections["thermal"]`
- Validate `r_outer_m > r_inner_m` at parse time; emit `[warn]` if violated
- Validate `N_radial >= 3`; emit `[warn]` if `N_radial < 3` and clamp to 3

### Wire — runner / `thermal_loss_explorer`

The runner constructs a `TrialConfig` from `VsimDocument`:

```
doc.thermal.r_inner_m       → cfg.geom.r_inner
doc.thermal.r_outer_m       → cfg.geom.r_outer
doc.thermal.length_m        → cfg.geom.length
doc.thermal.h_outer         → cfg.h_outer
doc.thermal.T_ambient_K     → cfg.T_ambient
doc.thermal.N_radial        → cfg.N_radial
doc.thermal.dt_s            → cfg.dt
doc.thermal.n_frames        → cfg.n_frames
doc.thermal.Th_decays       → cfg.Th_decays
doc.thermal.convergence_rel → (new parameter replacing magic 1e4)
doc.thermal.substep_warn    → VSIM_SUBSTEP_WARN per-run
doc.environment.temperature → cfg.Tc_init (inner wall initial T)
doc.run.temperature_K       → cfg.Th_init (hot boundary T)
```

If `doc.thermal.material_key` is non-empty, look up the MATERIAL_DB by name.
If empty, derive thermal properties from `doc.material.formula` via the
existing alias map.

---

## Extended section: `[excite.<type>]` type-gating

### Schema change

Add to `ExciteEntry` in `vsim_document.hpp`:

```cpp
bool   budget_limited  = false;  // If true, total energy is capped
double energy_budget_J = 0.0;    // Max total energy delivered (J); 0 = unlimited
double decay_tau_fs    = 0.0;    // Exponential decay time constant (fs); 0 = no decay
```

These fields are meaningful for `thermal_spike` and are undefined (silently
ignored) for `laser`/`xray`/`electron_beam`.

### Validation (64A-A2 complement)

In `apply_excite_key()`, after the entry is fully populated, call
`entry.valid_for(current_excite_type_)`.  If it returns `false`:

```
[warn] vsim:excite.thermal_spike: field "photon_energy_eV" is not applicable
	   to type "thermal_spike" — value ignored.
```

Add `photon_energy_eV` and `fluence` to the `valid_for()` exclusion list for
`thermal_spike`.

### Parser: new keys

| Key | Type | Section | Notes |
|---|---|---|---|
| `budget_limited` | bool | `[excite.*]` | Cap total energy delivery |
| `energy_budget_J` | double | `[excite.*]` | Max J delivered; 0 = unlimited |
| `decay_tau_fs` | double | `[excite.*]` | Pulse tail decay (fs) |

---

## New section: `[thermal.report]`

Subsection of `[thermal]` for controlling which derived quantities appear in
the `report.md` and `metrics.tsv`.

### Struct — `ThermalReportSection`

```cpp
struct ThermalReportSection {
	bool Q_loss_per_frame  = true;   // Instantaneous heat loss (W) per frame row
	bool Q_cumulative      = true;   // Running integral (J)
	bool T_profile_final   = false;  // Full radial T profile at last frame
	bool convergence_trace = true;   // RMS residual per frame
	bool substep_count     = false;  // Number of sub-steps per frame (diagnostic)
};
```

Add `ThermalReportSection report;` inside `ThermalSection`.  
Parse via a nested section header `[thermal.report]`.

---

## VSIM script syntax (canonical example)

This is the minimum addition to `thermal_shell_demo.vsim` that exercises all
new WO-64B fields:

```toml
[thermal]
r_inner_m       = 0.020
r_outer_m       = 0.050
length_m        = 1.0
h_outer         = 55.0
T_ambient_K     = 300.0
N_radial        = 16
dt_s            = 0.001
n_frames        = 200
Th_decays       = true
convergence_rel = 1e-3
substep_warn    = 20
material_key    = "NaCl_MgCl2_UCl3"

[thermal.report]
Q_loss_per_frame  = true
Q_cumulative      = true
T_profile_final   = true
convergence_trace = true
substep_count     = false

[excite.thermal_spike]
type           = "thermal_spike"
axis           = "z"
intensity      = 2.5
pulse_width_fs = 200.0
profile        = "gaussian"
budget_limited = true
energy_budget_J = 50000.0
decay_tau_fs   = 500.0
```

---

## VSIM_REFERENCE.md additions

### `[thermal]` fields table

| Field | Type | Default | Notes |
|---|---|---|---|
| `r_inner_m` | double | `0.02` | Inner wall radius (m) |
| `r_outer_m` | double | `0.05` | Outer wall radius (m); must be > r_inner_m |
| `length_m` | double | `1.0` | Axial channel length (m) |
| `h_outer` | double | `50.0` | Outer convective coefficient W/(m² K) |
| `T_ambient_K` | double | `300.0` | Ambient / coolant temperature (K) |
| `N_radial` | int | `12` | Radial grid nodes (min 3) |
| `dt_s` | double | `0.001` | Frame timestep (s) |
| `n_frames` | int | `100` | Number of frames |
| `Th_decays` | bool | `false` | Allow hot boundary to lose energy |
| `convergence_rel` | double | `1e-3` | Relative RMS residual convergence threshold |
| `substep_warn` | int | `20` | Emit `SubStepOverflow` event above this sub-step count |
| `material_key` | string | `""` | Override material lookup key; empty = derive from `[material]` |

### `[thermal.report]` fields table

| Field | Type | Default | Notes |
|---|---|---|---|
| `Q_loss_per_frame` | bool | `true` | Instantaneous heat loss per frame |
| `Q_cumulative` | bool | `true` | Running integral Q (J) |
| `T_profile_final` | bool | `false` | Full radial T profile at last frame |
| `convergence_trace` | bool | `true` | RMS residual per frame |
| `substep_count` | bool | `false` | Sub-step count per frame (diagnostic) |

### `[excite.*]` new fields table

| Field | Type | Default | Applicable types | Notes |
|---|---|---|---|---|
| `budget_limited` | bool | `false` | `thermal_spike` | Cap total energy delivery |
| `energy_budget_J` | double | `0.0` | `thermal_spike` | Max J; 0 = unlimited |
| `decay_tau_fs` | double | `0.0` | `thermal_spike` | Exponential tail decay (fs) |

---

## Test groups

| Group | Target binary | Tests | Tag |
|---|---|---|---|
| 47 | `test_thermal_section_parse` | P1–P12 | Schema define + parse |
| 48 | `test_thermal_section_wire` | W1–W8 | Runner config population |
| 49 | `test_excite_type_gate` | G1–G6 | Type-gating validation |

### Group 47 — Parse (P1–P12)

| ID | Test |
|---|---|
| P1 | `[thermal]` section parses all fields by exact key |
| P2 | Defaults reproduce pre-WO-64 TrialConfig values |
| P3 | `r_outer_m <= r_inner_m` triggers `[warn]`, not hard error |
| P4 | `N_radial < 3` triggers `[warn]` and clamps to 3 |
| P5 | `material_key = "FLiNaK"` round-trips as string |
| P6 | `[thermal.report]` subsection parses all five flags |
| P7 | Unknown key in `[thermal]` → `raw_sections["thermal"]` |
| P8 | `budget_limited = true` parses in `[excite.thermal_spike]` |
| P9 | `energy_budget_J` parses as double |
| P10 | `decay_tau_fs` parses as double |
| P11 | `vsepr validate` accepts full `thermal_shell_demo.vsim` + new fields |
| P12 | `[thermal]` absent → defaults applied silently |

### Group 48 — Wire (W1–W8)

| ID | Test |
|---|---|
| W1 | `doc.thermal.r_inner_m` → `cfg.geom.r_inner` |
| W2 | `doc.thermal.h_outer` → `cfg.h_outer` |
| W3 | `doc.thermal.convergence_rel` → replaces hard-coded `1e4` threshold |
| W4 | `doc.thermal.substep_warn` → per-run VSIM_SUBSTEP_WARN |
| W5 | `doc.thermal.material_key = "FLiNaK"` → correct MATERIAL_DB lookup |
| W6 | `doc.thermal.material_key = ""` → falls through to `[material].formula` |
| W7 | `doc.thermal.Th_decays = true` → `cfg.Th_decays` true |
| W8 | `doc.thermal.report.T_profile_final` present in export JSON |

### Group 49 — Type gating (G1–G6)

| ID | Test |
|---|---|
| G1 | `photon_energy_eV > 0` on `thermal_spike` → `[warn]` on stderr |
| G2 | `fluence > 0` on `thermal_spike` → `[warn]` on stderr |
| G3 | Warning does not abort parse |
| G4 | `budget_limited = true` on `laser` → no warning |
| G5 | `ExciteTypeMismatch` KernelEvent emitted (64A-A2 complement) |
| G6 | `vsepr validate` reports type mismatch in output |

---

## Files changed

| File | Change |
|---|---|
| `include/vsim/vsim_document.hpp` | `ThermalSection`, `ThermalReportSection` structs; add to `VsimDocument`; `ExciteEntry` new fields |
| `src/vsim/vsim_parser.cpp` | `apply_thermal_key()`, `apply_thermal_report_key()`; section dispatch; `[excite.*]` new field handlers; type-gate warn path |
| `apps/thermal_loss_explorer.cpp` | Runner wiring: `VsimDocument → TrialConfig` mapping (coordinated with WO-64A) |
| `VSIM_REFERENCE.md` | `[thermal]`, `[thermal.report]`, excite new-fields tables |
| `docs/VSIM_LANGUAGE.md` | `[thermal]` section narrative; type-gating behaviour note |
| `scripts/gallery/thermal_shell_demo.vsim` | Add `[thermal]` + `[thermal.report]` blocks demonstrating new fields |
| `tests/test_thermal_section_parse.cpp` | New — Group 47 |
| `tests/test_thermal_section_wire.cpp` | New — Group 48 |
| `tests/test_excite_type_gate.cpp` | New — Group 49 |
| `STAGE.md` | Groups 47–49 added; beta-13 section |

---

## Gate table

| Gate | Item | Status |
|---|---|---|
| 64B-1 | WO-64A gate table reviewed — kernel corrections scoped | ✅ |
| 64B-2 | `ThermalSection` struct defined | ⬜ |
| 64B-3 | `ThermalReportSection` struct defined | ⬜ |
| 64B-4 | `ExciteEntry` new fields added | ⬜ |
| 64B-5 | `apply_thermal_key()` parser handler implemented | ⬜ |
| 64B-6 | Excite type-gate warn path implemented | ⬜ |
| 64B-7 | Runner wiring: `VsimDocument → TrialConfig` | ⬜ |
| 64B-8 | Groups 47–49 all PASS | ⬜ |
| 64B-9 | `vsepr validate thermal_shell_demo.vsim` PASS with new fields | ⬜ |
| 64B-10 | `VSIM_REFERENCE.md` and `VSIM_LANGUAGE.md` updated | ⬜ |
| 64B-11 | STAGE.md Day 64 / beta-13 section complete | ⬜ |
