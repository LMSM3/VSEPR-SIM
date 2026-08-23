# AUDIT — COLLISION & PARTICLES Libraries
<!-- v5.1.3~2 | branch: v5.0.0-main | audit date: Day 63+ -->

## Scope

Audit of all transient-particle and QCD physics/vis code introduced in
WO-63A through WO-v513-QCD, with the goal of organizing the material
into two clean internal libraries: **COLLISION** and **PARTICLES**.

---

## Files Audited

| File | Lines | Layer |
|---|---|---|
| `include/coarse_grain/physics/qcd_transient.hpp` | 491 | Physics / Collision |
| `include/coarse_grain/vis/transient_renderer.hpp` | 250 | Vis / Particles |
| `include/coarse_grain/vis/qcd_colour_charge.hpp` | 250 | Vis / Particles |
| `include/cli/system_state.hpp` | 454 | Integration / both |
| `include/vsim/xsuite.hpp` | ~340 | Suite format (neither) |

---

## Public API Inventory

### qcd_transient.hpp (physics)

| Symbol | Kind | Notes |
|---|---|---|
| `ColorCharge` | enum | R/G/B/AntiR/AntiG/AntiB/Neutral/White |
| `is_quark_color()` | fn | predicate |
| `is_antiquark_color()` | fn | predicate |
| `complementary_color()` | fn | SU(3) complement |
| `GluonChannel` | struct | index 0–7, src/dst ColorCharge, `CHANNELS` table |
| `QuarkFlavor` | enum | Up..Top (-7..-12), Gluon (-13), AntiQuark (-14) |
| `quark_flavor_name()` | fn | string label |
| `is_qcd_type_code()` | fn | range check -14..-7 |
| `quark_electric_charge()` | fn | +2/3 or -1/3 by flavor |
| `quark_rest_mass()` | fn | dimensionless VSIM analog |
| `QCDParticle` | struct | id/flavor/color/channel/pos/vel/force/mass/energy/string fields |
| `CornellParams` | struct | kappa/sigma/r_min/r_conf/tau_scale |
| `cornell_force_magnitude()` | fn | κ/r² + σ |
| `cornell_potential()` | fn | -κ/r + σr |
| `color_casimir_factor()` | fn | +1/6 / -4/3 / -1/6 |
| `TABScheduler` | struct | dt_B, K, dt_A() |
| `QuarkGluonPlasma` | struct | particles + CornellParams + TABScheduler + counters |
| `qcd_accumulate_forces()` | fn | O(N²) Cornell + Casimir |
| `qcd_substep_pass()` | fn | velocity-Verlet T_A substep + hadronization gate |
| `qcd_run_tb_step()` | fn | K × qcd_substep_pass |
| `QCDPopulationConfig` | struct | seed params for plasma builder |
| `qcd_seed_plasma()` | fn | builds quark/gluon population |

### transient_renderer.hpp (vis)

| Symbol | Kind | Notes |
|---|---|---|
| `RGB` | struct | float r/g/b [0,1] |
| `TRANSIENT_PALETTE` | constexpr array | 14 hues |
| `palette_index_for_type()` | fn | type_code + ID scramble → palette index |
| `transient_colour()` | fn | main colour lookup |
| `velocity_gradient_colour()` | fn | blue→cyan→yellow heat ramp |
| `TrailPoint` | struct | x/y/z/speed_norm |
| `TransientOverlay` | struct | full render descriptor, MAX_TRAIL=32 |
| `trail_radius()` | fn | quadratic taper |
| `trail_segment_colour()` | fn | vel gradient × age blend |
| `transient_sphere_radius()` | fn | radius by type_code |
| `TransientKernelData` | struct | kernel-side data for factory |
| `make_overlay()` | fn | factory: kernel data → overlay with trail |

### qcd_colour_charge.hpp (vis)

| Symbol | Kind | Notes |
|---|---|---|
| `color_charge_to_rgb()` | fn | SU(3) → vivid RGB |
| `gluon_channel_colours()` | fn | channel → src/dst RGB pair |
| `gluon_trail_colour()` | fn | rainbow interpolation + midpoint flash |
| `confinement_string_colour()` | fn | tension → blue/orange/red/white |
| `QCDOverlay` | struct | full QCD render descriptor, MAX_TRAIL=48 |
| `qcd_sphere_radius()` | fn | radius by flavor |
| `QCDKernelData` | struct | kernel-side data for factory |
| `make_qcd_overlay()` | fn | factory: kernel data → overlay with trail |
| `quark_trail_colour()` | fn | SU(3) hue → velocity gradient blend |
| `gluon_trail_colour_aged()` | fn | rainbow trail by age |

### system_state.hpp (integration layer)

| Symbol | Kind | Notes |
|---|---|---|
| `TransientTypeCode` | enum | -1..-6 (electron/ion/neutron/gamma/alpha/reserved) |
| `is_transient_type_code()` | fn | code < 0 |
| `is_bead_type_code()` | fn | code > 0 |
| `transient_is_neutral()` | fn | neutron-like / gamma-like check |
| `TransientParticle` | struct | id/type_code/pos/vel/mass/charge/energy/active |
| `CGSystemState` | struct | full runtime container with both sidecars |
| `run_transient_optimizer_substeps()` | method | ballistic integrate + capture → bead delta |
| `run_qcd_tb_step()` | method | auto-seed + delegate to qcd_run_tb_step() |
| `update_environment()` | method | main step: calls both sidecars + bead env loop |

---

## Issues Found

### Issue 1 — Velocity-Verlet single-pass approximation (COLLISION)
**Location:** `qcd_substep_pass()` lines ~360–400  
**Description:** The second half-kick uses the same force computed before the
position update, not a re-accumulated force. This is a known first-order
approximation of velocity-Verlet; acceptable for a scaled analog but should
be explicitly documented.  
**Severity:** Low — by design for performance.  
**Recommendation:** Add a comment: `// NOTE: second half-kick uses pre-step force (single-pass approximation).`

### Issue 2 — Hadronization deactivation mid-loop (COLLISION)
**Location:** `qcd_substep_pass()` hadronization gate  
**Description:** The loop over particles marks `p.active = false` on both
the current and partner particle while still iterating. A deactivated particle
could still receive a force in the same or next call if the partner index
was processed before it.  
**Severity:** Low — in practice the particle won't move meaningfully
after deactivation (force is reset each pass), but it is an ordering hazard.  
**Recommendation:** Mark for deactivation in a separate pass, or use a
`to_deactivate` list that is applied after the integration loop.

### Issue 3 — TransientTypeCode / TransientParticle location (PARTICLES)
**Location:** `include/cli/system_state.hpp` lines 40–78  
**Description:** These type definitions belong in the PARTICLES layer, not
inside the CLI integration header. The CLI header should `#include` them.  
**Severity:** Medium — creates a dependency path: any file needing `TransientParticle`
must include the full CGSystemState machinery.  
**Recommendation:** Move to `include/PARTICLES/transient_particle.hpp`;
have `system_state.hpp` include that.

### Issue 4 — Hard-coded capture radius literal (COLLISION)
**Location:** `run_transient_optimizer_substeps()`, `capture_radius = 0.8`  
**Description:** Physics constant embedded inline without a named parameter.  
**Severity:** Low.  
**Recommendation:** Move to a `TransientOptimizerParams` struct in the
COLLISION library, parallel to `CornellParams`.

### Issue 5 — RGB defined in transient_renderer, used by qcd_colour_charge (PARTICLES)
**Location:** `transient_renderer.hpp:32`, consumed via `#include` in `qcd_colour_charge.hpp`  
**Description:** Coupling via include ordering is correct but fragile.
If `qcd_colour_charge.hpp` is ever included without `transient_renderer.hpp`,
`RGB` won't be defined.  
**Severity:** Low — currently guarded by include order.  
**Recommendation:** Define `RGB` in its own `include/PARTICLES/rgb.hpp` or
in the PARTICLES library root; both renderers include that.

### Issue 6 — MAX_TRAIL mismatch (PARTICLES)
**Location:** `TransientOverlay::MAX_TRAIL = 32` vs `QCDOverlay::MAX_TRAIL = 48`  
**Description:** Different trail lengths for the same conceptual ring buffer.
Not a bug (QCD particles move faster / need longer trails) but should be
documented as intentional.  
**Severity:** Informational.

---

## Library Split Map

### COLLISION library  (`include/COLLISION/`)

| Source symbol | Source file | Move to |
|---|---|---|
| `CornellParams` | qcd_transient.hpp | `collision/cornell.hpp` |
| `cornell_force_magnitude()` | qcd_transient.hpp | `collision/cornell.hpp` |
| `cornell_potential()` | qcd_transient.hpp | `collision/cornell.hpp` |
| `color_casimir_factor()` | qcd_transient.hpp | `collision/casimir.hpp` |
| `TABScheduler` | qcd_transient.hpp | `collision/tab_scheduler.hpp` |
| `qcd_accumulate_forces()` | qcd_transient.hpp | `collision/qcd_force.hpp` |
| `qcd_substep_pass()` | qcd_transient.hpp | `collision/qcd_force.hpp` |
| `qcd_run_tb_step()` | qcd_transient.hpp | `collision/qcd_force.hpp` |
| `qcd_seed_plasma()` | qcd_transient.hpp | `collision/qcd_force.hpp` |
| `run_transient_optimizer_substeps()` | system_state.hpp | `collision/transient_optimizer.hpp` |
| `TransientOptimizerParams` (new) | — | `collision/transient_optimizer.hpp` |

### PARTICLES library  (`include/PARTICLES/`)

| Source symbol | Source file | Move to |
|---|---|---|
| `RGB` | transient_renderer.hpp | `particles/rgb.hpp` |
| `TRANSIENT_PALETTE` | transient_renderer.hpp | `particles/transient_palette.hpp` |
| `TrailPoint` | transient_renderer.hpp | `particles/trail.hpp` |
| `TransientOverlay` | transient_renderer.hpp | `particles/trail.hpp` |
| `trail_radius()` | transient_renderer.hpp | `particles/trail.hpp` |
| `trail_segment_colour()` | transient_renderer.hpp | `particles/trail.hpp` |
| `TransientKernelData` | transient_renderer.hpp | `particles/transient_particle.hpp` |
| `TransientTypeCode` | system_state.hpp | `particles/transient_particle.hpp` |
| `TransientParticle` | system_state.hpp | `particles/transient_particle.hpp` |
| `transient_colour()` | transient_renderer.hpp | `particles/transient_particle.hpp` |
| `transient_sphere_radius()` | transient_renderer.hpp | `particles/transient_particle.hpp` |
| `make_overlay()` | transient_renderer.hpp | `particles/transient_particle.hpp` |
| `ColorCharge` | qcd_transient.hpp | `particles/qcd_particle.hpp` |
| `GluonChannel` | qcd_transient.hpp | `particles/qcd_particle.hpp` |
| `QuarkFlavor` | qcd_transient.hpp | `particles/qcd_particle.hpp` |
| `QCDParticle` | qcd_transient.hpp | `particles/qcd_particle.hpp` |
| `QuarkGluonPlasma` | qcd_transient.hpp | `particles/qcd_particle.hpp` |
| `QCDPopulationConfig` | qcd_transient.hpp | `particles/qcd_particle.hpp` |
| `color_charge_to_rgb()` | qcd_colour_charge.hpp | `particles/qcd_colour.hpp` |
| `gluon_trail_colour()` | qcd_colour_charge.hpp | `particles/qcd_colour.hpp` |
| `confinement_string_colour()` | qcd_colour_charge.hpp | `particles/qcd_colour.hpp` |
| `QCDOverlay` | qcd_colour_charge.hpp | `particles/qcd_colour.hpp` |
| `QCDKernelData` | qcd_colour_charge.hpp | `particles/qcd_colour.hpp` |
| `make_qcd_overlay()` | qcd_colour_charge.hpp | `particles/qcd_colour.hpp` |

---

## Migration Notes

1. **No destructive moves yet.** The existing headers remain in place and compile.
   The new library headers are umbrella `#include` aggregators that re-export
   everything through the new namespace layout. Original files are deprecated
   but not removed until the full tree compiles against the new paths.

2. **`include/COLLISION/collision.hpp`** — includes all COLLISION sub-headers.
   CMake can add this as an interface target `COLLISION` with
   `target_include_directories(... PUBLIC include/COLLISION)`.

3. **`include/PARTICLES/particles.hpp`** — includes all PARTICLES sub-headers.
   CMake interface target `PARTICLES`.

4. **`system_state.hpp`** keeps `CGSystemState` but will switch its includes:
   - Remove inline `TransientTypeCode`/`TransientParticle` definitions
   - `#include "PARTICLES/transient_particle.hpp"`
   - `#include "COLLISION/transient_optimizer.hpp"`

5. **`qcd_transient.hpp`** retains full physics until COLLISION sub-headers
   are ready; it will be shimmed to re-export from them.

6. **`transient_renderer.hpp`** and **`qcd_colour_charge.hpp`** retain all vis
   code until PARTICLES sub-headers are ready; they will be shimmed similarly.

---

## Promotion Gates

| Step | Gate |
|---|---|
| Library headers created | `get_errors` clean on umbrella headers |
| system_state switched | existing tests still pass |
| qcd_transient shimmed | build with `-DUSE_COLLISION_LIB` passes |
| transient_renderer shimmed | Qt viewport still renders |
| Full promotion | all tests green, old headers `#deprecated` |

---

*AUDIT-COLLISION-PARTICLES — v5.1.3~2*
