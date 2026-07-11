# WO-56C — Central Kernel Pass-Through Consolidation

**Status:** In Progress  
**Type:** Consolidation / Audit / Routing  
**Branch:** `v5.0.0-beta.7-step-attempt`  
**Date:** 2025  
**Scope:** Architecture — kernel spine, event registry, script validation, neighbor list

---

## Objective

Stop treating reaction, chemical, HGST/dynamic-energy, and continual-report
calculations as separate little gremlins living in side files.

Everything important passes through the central kernel.  
The kernel is the spine. Everything else is an organ.

---

## Doctrine

### Before (bad)
```
module computes value
module prints result
report guesses what happened
```

Fragile. Unauditable. Future-you hates past-you.

### After (correct)
```
xyz / xyzf / xyzFull
		↓
Central Kernel  (routes calculation)
		↓
KernelEvent     (symbolic + numeric trace assigned stable event_id)
		↓
KernelEventLog  (append-only registry)
		↓
analysis layer → report / export / dashboard
```

---

## Permanent Rules Established by This WO

### 1. No major calculation bypasses the kernel.

Every significant computed result — reaction, chemical state change, formation
outcome, defect identification, transport metric, periodic report snapshot —
must produce a `KernelEvent` and be recorded into `KernelEventLog`.

### 2. Each KernelEvent stores a complete audit trail.

| Field | Contents |
|---|---|
| `event_id` | Monotonic uint64 assigned at record time |
| `kind` | `KernelEventKind` enum tag |
| `frame_id` | Trajectory step / frame that caused the event |
| `source_formula` | Molecule/material formula string |
| `source_particle_id` | Persistent particle ID (-1 = system-level) |
| `equation_symbolic` | Human-readable symbolic equation |
| `equation_numeric` | Numerically substituted string |
| `result_value` | Double scalar result |
| `result_unit` | Unit string (kcal/mol, Å, fraction, …) |
| `is_valid` | false if result is flagged |
| `warning` | Non-empty if flagged |

### 3. xyzFull stores what happened. Analysis determines what it means.

Inferred labels (diffusion class, material class, macro property) belong in
analysis records, reports, and dashboards. They must not appear in State, xyz,
or xyzFull.

### 4. Memory tiling deferred to ~5.0.1 post-release.

The ERB-aware Verlet neighbor list (WO-56C) is the correct O(N) scaling path
for the current release window. Memory tiling adds another complexity layer
that is not needed before 5.0.0 ships.

---

## Files Produced

### `.vsim` Script Layer

| File | Purpose |
|---|---|
| `include/vsim/vsim_document.hpp` | `VsimDocument` — parsed representation of a `.vsim` script |
| `include/vsim/vsim_parser.hpp` | `VsimParser` interface + `ParseError` |
| `src/vsim/vsim_parser.cpp` | Full parser implementation (TOML-subset grammar) |

**Sections handled structurally:** `[project]`, `[simulation]`, `[export]`, `[[simulation.molecule]]`  
**Forward compatibility:** unknown sections captured in `VsimDocument::raw_sections`

#### `.vsim` Schema (canonical field reference)

```toml
[project]
name        = "demo_03_graphite_stack"   # required
version     = "v5.0.0-beta.8"
seed_base   = 3008
determinism = true                        # always true; field kept for schema completeness

[simulation.molecule]
formula      = "C"
count        = 480
temperature  = 300.0                      # K
lattice      = "hexagonal"
layer_mode   = "AB"                       # Bernal stacking
n_layers     = 5

[simulation]
fire_max_steps   = 800
fire_dt_fs       = 0.5
box_size_ang     = 0.0                    # 0 = auto-size
periodic         = false
formation_preset = "ceramic"

[export]
write_xyz           = true
write_xyzf          = true
write_analysis_json = true
write_metrics_tsv   = true
write_report_md     = true
write_dashboard_json = false
```

### Demo Scripts

| File | System | Purpose |
|---|---|---|
| `scripts/demo_01_minimal_hexene.vsim` | C₆H₁₂ | Minimal single-molecule formation baseline |
| `scripts/demo_02_graphene_sheet.vsim` | C (96 atoms) | Single-layer graphene, no stacking — stacking suppression test |
| `scripts/demo_03_graphite_stack.vsim` | C (480 atoms) | 5-layer AB Bernal graphite — full ERB modulation test |

**Validate any script:**
```
vsper validate scripts/demo_01_minimal_hexene.vsim
vsper validate scripts/demo_02_graphene_sheet.vsim
vsper validate scripts/demo_03_graphite_stack.vsim
```

### Validate CLI Subcommand

| File | Purpose |
|---|---|
| `include/cli/cmd_validate.hpp` | `vsepr::cli::cmd_validate()` declaration + exit codes |
| `src/cli/cmd_validate.cpp` | Parse → validate → colorized report → exit code |

**Exit codes:** 0 = valid, 1 = validation errors, 2 = parse / IO error

**Wired into:** `apps/vsepr-cli/main.cpp` command dispatch

### ERB-Aware Verlet Neighbor List

| File | Purpose |
|---|---|
| `include/nl/verlet_list.hpp` | `VerletEntry`, `VerletParams`, `VerletList`, `erb_modulation()` |

#### Design: Why This Is Different

A naive Verlet list stores only geometry: `(j, r_ij, dx, dy, dz)`.

This list carries `eta_j` — the ERB environment state of the neighbor,
snapshotted at build time.

This means the modulation factor:

```
g_k(η_i, η_j) = 1 + γ_k · η̄,    η̄ = 0.5·(η_i + η_j)
```

…can be evaluated directly from the NL entry during the force/energy pass.
No O(N²) η scan. No additional lookup array. The density information travels
with the geometry.

#### Rebuild Triggers (two conditions, both checked)

| Trigger | Threshold |
|---|---|
| Position displacement | `|Δr_i| > r_skin / 2` for any bead |
| η drift | `|Δη_i| > eta_skin` for any bead |

The η drift trigger is the key addition: dense regions rebuild more often
because their local environment changes faster. This is not a performance
problem — it is the correct physical behavior.

#### Default Parameters

| Parameter | Default | Notes |
|---|---|---|
| `r_cutoff` | 12.0 Å | LJ / Coulomb cutoff |
| `r_skin` | 2.0 Å | Skin buffer |
| `eta_skin` | 0.1 | η drift threshold |
| `build_cutoff` | 14.0 Å | Pairs stored in NL |

#### Integration Point

```cpp
// Force evaluation loop (schematic):
for (int i = 0; i < N; ++i) {
	float eta_i = eta[i];
	for (const auto& e : nl.neighbors(i)) {
		float g_steric = erb_modulation(params.gamma_steric, eta_i, e);
		float g_elec   = erb_modulation(params.gamma_elec,   eta_i, e);
		float g_disp   = erb_modulation(params.gamma_disp,   eta_i, e);
		// ... apply modulated pair interaction
	}
}
```

### Central Kernel Event Hierarchy

| File | Purpose |
|---|---|
| `include/kernel/kernel_event.hpp` | Base `KernelEvent` + six derived types |
| `include/kernel/kernel_event_log.hpp` | Thread-safe append-only singleton registry |

#### Event Types

| Kind | Struct | Captures |
|---|---|---|
| `Reaction` | `ReactionEvent` | Reactants, products, energies, ΔE, symbolic equation |
| `ChemicalState` | `ChemicalStateEvent` | Bond change, coordination, local energy delta |
| `Formation` | `FormationEvent` | N beads, FIRE steps, convergence, packing fraction, lattice class |
| `Defect` | `DefectEvent` | Type (vacancy/interstitial/substitution/…), site ID, formation energy |
| `Transport` | `TransportEvent` | Displacement, MSD, diffusivity proxy, transport mode |
| `ContinualReport` | `ContinualReportEvent` | Rolling snapshot: U, T, η̄, coordination, RMSD |

#### Reaction Event — Example Symbolic Trace

```
A + B -> C
Delta_E = E_products - E_reactants
Delta_E = (E_C) - (E_A + E_B)
Delta_E = (-124.2) - (-80.1 + -39.7)
Delta_E = -4.4 kcal/mol
```

Generated automatically by `ReactionEvent::compute_delta_E()`.

#### Usage Pattern

```cpp
#include "kernel/kernel_event_log.hpp"

vsepr::kernel::ReactionEvent ev;
ev.source_formula = "C6H12";
ev.frame_id       = current_frame;
ev.reactants      = {"A", "B"};
ev.products       = {"C"};
ev.reactant_energies = {-80.1, -39.7};
ev.product_energies  = {-124.2};
ev.compute_delta_E();

auto handle = vsepr::kernel::kernel_record(ev);
// handle.event_id is the stable audit ID
```

#### Export Formats

```cpp
auto& log = vsepr::kernel::KernelEventLog::instance();

// JSON Lines (one event per line — append to analysis_events.jsonl)
std::string jsonl = log.to_jsonl();

// Markdown table (for report_md output)
std::string md = log.to_markdown();

// Filtered query
auto reactions = log.filter_by_kind(KernelEventKind::Reaction);
auto frame_50  = log.filter_by_frame(45, 55);
```

---

## Pipeline Integration Target (beta-7)

```
FormationOutput   → FormationEvent (kernel_record)
		↓
FingerprintRecord  linked via event_id
		↓
ClusterRecord      linked via event_id
		↓
AnalysisRecord     (DefectEvent, TransportEvent, …)
		↓
ReportRecord       (ContinualReportEvent every N steps)
		↓
DashboardRecord    (export to dashboard_events.jsonl)
```

A completed beta-7 run produces:
- formation log
- final structure (xyz)
- trajectory (xyzf)
- energy trace
- stationarity result
- fingerprint
- cluster assignment
- defect / surface interpretation
- diffusion / packing analysis
- validity warnings
- report tables (Markdown)
- dashboard export (JSONL)

All of these are now traceable back to `KernelEvent.event_id`.

> Pipeline wiring is a day-57+ task (2026-04-27 or later).

---

## What Was Deliberately Not Done

| Item | Reason |
|---|---|
| Memory tiling | Deferred to ~5.0.1 post-release |
| Grid-cell spatial hash | Deferred; Verlet with skin is sufficient for current N |
| Virtual dispatch on KernelEvent | Rejected — tag-based `KernelEventKind` is simpler and zero-cost |
| η stored inside xyzFull | Rejected — violates doctrine; η belongs in analysis layer |
| `determinism = false` path | Not yet useful; field kept in schema for forward compatibility |

---

## Verification Commands

```powershell
# Parse and validate each demo script
vsper validate scripts/demo_01_minimal_hexene.vsim
vsper validate scripts/demo_02_graphene_sheet.vsim
vsper validate scripts/demo_03_graphite_stack.vsim

# Build check
cmake --build build --target vsepr-cli

# Neighbor list header compile check (no .cpp required)
cl /std:c++latest /I include /c /Fo nul include\nl\verlet_list.hpp

# Kernel event header compile check
cl /std:c++latest /I include /c /Fo nul include\kernel\kernel_event.hpp
cl /std:c++latest /I include /c /Fo nul include\kernel\kernel_event_log.hpp
```

---

## Open Items for Day 57+

- [x] Wire `FormationEvent` into FIRE relaxation exit path in `src/sim/` — **DONE** (`src/sim/optimizer.hpp`, Group 28 test)
- [ ] Wire `ContinualReportEvent` into the continual-report module
- [ ] Connect `KernelEventLog::to_jsonl()` to `[export] write_analysis_json`
- [ ] Connect `KernelEventLog::to_markdown()` to `[export] write_report_md`
- [ ] Add `vsim_value.hpp` (thin tagged-union type used by the parser)
- [ ] Build verification of all WO-56C new files against beta-7 CMake
- [ ] Land `boundary_allows()` dispatch table as a first-class `VsimRuntime` primitive
- [x] Add `render_interval` field to `VisualExternalSection` (currently enforced only in runner loop) ← **DONE — WO-VSEPR-SIM-57D** (`VisualSection` + `VisualExternalSection`, `should_render()`, parser wired, dispatch gated in `beta10_demo.cpp` + `kernel_demo.cpp`, Group 35 tests)

---

## Completed — Day #2 Runtime Tests (Group 27)

The following scripting runtime guarantees have been validated and documented:

| Priority | Test | Result |
|---|---|---|
| 1 | `test_while_loop_safety_cap` | PASS — exits at `max_iters=20`, partial events preserved |
| 2 | `test_batch_artifact_isolation` | PASS — log cleared before each of 3 cases, zero bleed |
| 3 | `test_visual_decimation` | PASS — exactly 10 renders at frames 100…1000, none at 0 |
| 4 | `test_N_evolution_event_counter` | PASS — spawn >0, remove <0, stable ≈0 |
| 5 | `test_cached_metric_guard` | PASS — exits well before max_iters after variance collapse |
| 6 | `test_open_closed_boundary_dispatch` | PASS — all 12 allow/reject combinations correct |

**New files:**
- `tests/test_vsim_runtime_day2.cpp` — 6 tests, CMake Group 27
- `scripts/group_27_kernel_safety_tests.vsim` — canonical `.vsim` script
- `docs/section_testing_infrastructure.tex` — Group 27 section added; cumulative count 1019

## Completed — WO-56D-AuditChain Step 1: FormationEvent FIRE Wiring (Group 28)

Real FIRE relaxation runs now populate `KernelEventLog` with a `FormationEvent`.

| Test | Result |
|---|---|
| `test_fire_records_formation_event` | PASS — event appears after real FIRE run |
| `test_fire_event_fields_are_populated` | PASS — symbolic trace, result_value, unit all set |
| `test_fire_event_source_formula_threaded` | PASS — source_formula threads from OptimizerSettings |
| `test_fire_nonconverged_event_flagged` | PASS — is_valid=false, warning non-empty |
| `test_fire_multiple_runs_accumulate_events` | PASS — 2 runs → 2 events, monotonic IDs |

**Changed files:**
- `src/sim/optimizer.hpp` — FormationEvent recorded at single FIRE exit point; `source_formula` + `formation_preset` added to `OptimizerSettings`
- `tests/test_formation_event_wiring.cpp` — 5 acceptance tests, CMake Group 28

**Audit chain status:** real FIRE runs now produce a traceable `FormationEvent`.

---

## Architectural Decision — beta-7 Philosophical Overhaul: Autonomous Reporting Deprecated

**Title** (beta-7 overhaul)

Autonomous reporting is no longer a planned runtime primitive in VSEPR-SIM.

**Old assumption (now void):**
> Wire `ContinualReportEvent` into a background continual-report module that fires every N steps automatically.

**New doctrine:**
> Reporting cadence is *script-declared*.  
> If a researcher wants periodic snapshots, they encode the cadence in a `.vsim` script:
> ```
> [while]
> max_steps = 10000
> snapshot_every = 500
>
> [export]
> format = jsonl
> target = run_output/events.jsonl
> ```
> `VsimRuntime` interprets the script and emits `ContinualReportEvent` into `KernelEventLog` at each declared interval.  
> There is no background engine running on behalf of the user.

**Consequences:**
- `include/core/report_engine.hpp` — marked **LEGACY**; `AutonomousEngine` class marked `[[deprecated]]`.
- `ContinualReportEvent` in `kernel_event.hpp` remains valid as a kernel event type; its doc now documents script-declared emission only.
- `STAGE.md` step 3 ("Wire ContinualReportEvent into continual-report module") struck and replaced with the script-declared note.
- `bio_report_engine.hpp`, `bio_report_engine.cpp`, `report_engine.cpp` are treated as legacy reference implementations only.

---

*WO-56C closed for new architectural scope. Continuation targets wiring and build verification — day 57 (2026-04-27) or later.*
