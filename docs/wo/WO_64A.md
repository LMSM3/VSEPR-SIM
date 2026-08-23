# WO-64A — Kernel Theoretical Integration Pass

**Status:** Open  
**Type:** Kernel — Physics / Numerical Methods  
**Day:** 64  
**Beta:** v5.0.0-beta.13  
**Branch:** `v5.0.0-main`  
**Scope:** `src/`, `include/`, `apps/thermal_loss_explorer.cpp`, `core/kernel/`

---

## Objective

The thermal and excitation theoreticals have been validated through a working
prototype authored in the VSIM proprietary scripting language
(`thermal_shell_demo.vsim`, Day 63).  This WO gates their integration into
the central kernel — meaning every corrected calculation must produce a
`KernelEvent`, be registered into `KernelEventLog`, and be testable via the
standard group harness.

This is not a research pass.  The prototype proved the model.  
This WO makes it production.

---

## Doctrine

> **Rule (WO-56C):** No major calculation bypasses the kernel.  
> Every significant computed result produces a `KernelEvent` and is recorded
> into `KernelEventLog`.

The four numerical corrections and two architectural fixes below each map to
a distinct `KernelEventKind` entry.  The kernel is the only place these
values are computed.  Apps and scripts consume events — they do not recompute.

---

## Background: validated prototype

`scripts/gallery/thermal_shell_demo.vsim` (Day 63) demonstrated:

- 1-D radial FD solver for NaCl/MgCl2/UCl3 at 1073 K
- Gaussian thermal-spike excitation (`[excite.thermal_spike]`)
- `[while]` convergence gate on `variance(energy.total, last 40)`
- Full export/reporting pipeline

`docs/AUDIT_thermal_shell_demo.md` formally documented four numerical
inaccuracies in `thermal_loss_explorer.cpp` and two low-quality patterns
in the kernel event stack.  All six items are addressed below.

---

## Corrections — numerical

### 64A-N1 — Q_loss sub-step accumulation

**File:** `apps/thermal_loss_explorer.cpp`, `run_trial()`, frame loop  
**Current (incorrect):**
```cpp
Q_cumul += Q_loss * cfg.dt;
```
`Q_loss` is sampled once at the end of the frame after all sub-steps have
run.  For materials with high thermal diffusivity (Carbon Steel, FLiNaK)
`sub_steps` can reach 40+; the cumulative energy removed is underestimated
by 30–50 % because the intermediate surface temperatures are not integrated.

**Target:**
Accumulate Q_loss during the sub-step loop using the surface temperature at
each sub-step boundary:
```cpp
// Inside the sub-step loop:
double T_surf_sub = T[cfg.N_radial - 1];
double Q_sub = cfg.h_outer * cfg.geom.area_outer()
			   * std::max(T_surf_sub - cfg.T_ambient, 0.0);
Q_cumul += Q_sub * dt_sub;
```
Remove the post-loop `Q_cumul += Q_loss * cfg.dt` line.

**KernelEvent:** emit `KernelEventKind::ThermalEnergyBalance` per frame with
`result_value = Q_cumul`, `result_unit = "J"`, and `equation_symbolic` =
`"integral(h*A*(T_surf-T_amb), 0, t)"`.

---

### 64A-N2 — Th_decays volume proxy

**File:** `apps/thermal_loss_explorer.cpp`, `run_trial()`, Th_decays block  
**Current (incorrect):**
```cpp
double dTh = -Q_loss / (mat.rho * mat.Cp * cfg.geom.area_outer() * dr);
```
`area_outer * dr` ≈ surface area × radial step, not a volume.  The correct
thermal mass for the hot annular shell is the full shell volume:

```
V_shell = pi * (r_outer^2 - r_inner^2) * length
```

**Target:**
```cpp
double V_shell = PI * (cfg.geom.r_outer * cfg.geom.r_outer
					 - cfg.geom.r_inner * cfg.geom.r_inner)
			   * cfg.geom.length;
double dTh = -Q_loss / (mat.rho * mat.Cp * V_shell);
```

**Impact:** ~30 % correction at the default geometry
(`r_inner = 0.02 m`, `r_outer = 0.05 m`).

---

### 64A-N3 — RMS convergence threshold normalization

**File:** `apps/thermal_loss_explorer.cpp`, `run_trial()`, post-loop  
**Current (incorrect):**
```cpp
result.converged = result.rms_final < 1e4; // residual in T/m^2 units
```
The magic constant `1e4 T/m²` is unscaled.  On a coarse grid the residual
inflates naturally; on a fine grid it shrinks.  The `converged` flag is
unreliable across materials and grid sizes.

**Target:** normalize by the mean |d²T/dr²| magnitude of the initial
temperature profile to get a dimensionless relative residual:

```cpp
double T_span = std::abs(cfg.Th_init - cfg.Tc_init);
double dr2 = dr * dr;
double rms_scale = (T_span > 0.0 && dr2 > 0.0) ? T_span / dr2 : 1.0;
result.converged = (result.rms_final / rms_scale) < 1e-3;
```

The `1e-3` relative threshold is material- and grid-independent.

---

### 64A-N4 — Sub-step overflow guard with kernel warning

**File:** `apps/thermal_loss_explorer.cpp`, `run_trial()`, before frame loop  
**Current:** No warning when `sub_steps` is large; solver silently uses
40+ sub-steps per frame making frame timing unpredictable.

**Target:** After computing `sub_steps`, if `sub_steps > VSIM_SUBSTEP_WARN`
(default 20), emit a `KernelEvent` of kind `KernelEventKind::Warning` with
`warning = "dt >> dt_stable for material <name>; sub_steps=<N>"`.

This surfaces in the event log and dashboard without aborting the run.

---

## Corrections — architectural

### 64A-A1 — Metric validation in `[observe]` parser

**File:** `src/vsim/vsim_parser.cpp`, `apply_observe_key()`  
**Current:** Unknown metric strings are silently dropped to `raw_sections`.
A typo produces zero output, no error, no warning.

**Target:** Maintain a `static const std::unordered_set<std::string>
kKnownMetrics` in the observe handler.  When a metric name is not found,
emit to `stderr`:
```
[warn] vsim:observe: unknown metric "temperature.centre" at line N
	   (known: energy.total, rms_force, temperature.center, ...)
```
Drop to `raw_sections` as before (no hard error — forward compatibility).

---

### 64A-A2 — `ExciteEntry` type-subset enforcement

**File:** `include/vsim/vsim_document.hpp`, `ExciteEntry`  
**Current:** `photon_energy_eV` and `fluence` are present in `ExciteEntry`
regardless of the excitation type, making `thermal_spike` entries carry
photon fields that have no physical meaning.

**Target:** Add a `valid_for(const std::string& type) const` predicate that
returns `false` when a photon-domain field is non-zero on a `thermal_spike`
entry, and logs a `KernelEvent::Warning` during validate.  Do **not**
remove the fields — forward compatibility.  A VSIM validate pass should
surface the mismatch.

---

## New `KernelEventKind` values

Add to `include/core/kernel_event.hpp` (or the enum header):

```cpp
ThermalEnergyBalance,   // per-frame Q_cumul emission (64A-N1)
ThermalConvergence,     // end-of-trial converged/not flag (64A-N3)
SubStepOverflow,        // sub_steps > VSIM_SUBSTEP_WARN (64A-N4)
ExciteTypeMismatch,     // photon fields on thermal_spike (64A-A2)
```

---

## Test groups

| Group | Target binary | Tests | Tag |
|---|---|---|---|
| 44 | `test_thermal_kernel_accuracy` | T1–T8 | 64A numerical corrections |
| 45 | `test_kernel_event_thermal` | E1–E6 | 64A event emission |
| 46 | `test_observe_metric_validation` | M1–M4 | 64A-A1 warn path |

### Group 44 — Numerical accuracy (T1–T8)

| ID | Test | Pass condition |
|---|---|---|
| T1 | Q_loss single sub-step — matches old result | delta < 0.001 J |
| T2 | Q_loss multi sub-step (Carbon Steel) — old vs new | new > old by 20–60 % |
| T3 | Th_decays old volume vs corrected volume | delta ~ 30 % at default geom |
| T4 | RMS threshold — coarse grid converged flag | relative < 1e-3 |
| T5 | RMS threshold — fine grid converged flag | relative < 1e-3 |
| T6 | Sub-steps = 1: sub-step loop produces same Q_cumul as frame-level | exact match |
| T7 | Sub-step overflow warning event emitted for Carbon Steel at dt=0.5 | event present |
| T8 | Sub-step overflow NOT emitted for NaCl_MgCl2_UCl3 at dt=0.5 | event absent |

### Group 45 — Kernel event emission (E1–E6)

| ID | Test | Pass condition |
|---|---|---|
| E1 | `ThermalEnergyBalance` event present after run_trial | event_id assigned |
| E2 | `ThermalEnergyBalance.result_unit == "J"` | exact |
| E3 | `ThermalConvergence` event present at trial end | event_id assigned |
| E4 | `SubStepOverflow` event when sub_steps > 20 | warn field non-empty |
| E5 | `ExciteTypeMismatch` event when photon_energy_eV > 0 on thermal_spike | event present |
| E6 | All new events appear in exported `events.json` | keys present |

### Group 46 — Observe metric validation (M1–M4)

| ID | Test | Pass condition |
|---|---|---|
| M1 | Known metric `energy.total` — no warning | stderr empty |
| M2 | Unknown metric `"temperature.centre"` — warning on stderr | `[warn]` in stderr |
| M3 | Warning does not abort parse | `doc.observe.metrics` populated |
| M4 | Unknown metric dropped to `raw_sections["observe"]` | key present |

---

## Files changed

| File | Change |
|---|---|
| `apps/thermal_loss_explorer.cpp` | 64A-N1 sub-step Q accumulation; 64A-N2 volume fix; 64A-N3 normalized RMS; 64A-N4 sub-step guard |
| `include/core/kernel_event.hpp` | Four new `KernelEventKind` values |
| `include/vsim/vsim_document.hpp` | `ExciteEntry::valid_for()` predicate (64A-A2) |
| `src/vsim/vsim_parser.cpp` | `kKnownMetrics` set + warn path in `apply_observe_key` (64A-A1) |
| `tests/test_thermal_kernel_accuracy.cpp` | New — Group 44 |
| `tests/test_kernel_event_thermal.cpp` | New — Group 45 |
| `tests/test_observe_metric_validation.cpp` | New — Group 46 |
| `STAGE.md` | Day 64 / beta-13 section; Groups 44–46 added |
| `VSIM_REFERENCE.md` | `[observe]` metric warning note; `ExciteEntry` type-subset note |
| `docs/AUDIT_thermal_shell_demo.md` | Status column updated: all six items → RESOLVED in WO-64A |

---

## Gate table

| Gate | Item | Status |
|---|---|---|
| 64A-1 | Prototype validated via `thermal_shell_demo.vsim` | ✅ Day 63 |
| 64A-2 | All six audit items mapped to corrections | ✅ This WO |
| 64A-3 | New `KernelEventKind` values defined | ⬜ |
| 64A-4 | 64A-N1 Q_loss sub-step fix implemented | ⬜ |
| 64A-5 | 64A-N2 Th_decays volume fix implemented | ⬜ |
| 64A-6 | 64A-N3 RMS normalization implemented | ⬜ |
| 64A-7 | 64A-N4 sub-step overflow warning emits event | ⬜ |
| 64A-8 | 64A-A1 observe metric warn path implemented | ⬜ |
| 64A-9 | 64A-A2 ExciteEntry valid_for() predicate added | ⬜ |
| 64A-10 | Groups 44–46 all PASS | ⬜ |
| 64A-11 | STAGE.md / VSIM_REFERENCE.md updated | ⬜ |
| 64A-12 | AUDIT doc status column updated RESOLVED | ⬜ |
