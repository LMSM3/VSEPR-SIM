# 2026-07-19 Debugging Ledger

## Purpose

This file is a dated debugging companion to `PROBLEMS.md`. It captures the current problem surface, the evidence checked today, and the smallest proof path needed before marking issues closed.

Operator-visible behavior must be proven through real `.vsim` scripts. C++ unit tests are useful for module-level confidence, but they do not close a user-facing problem unless the script surface also demonstrates the behavior.

## Source Context

- Problem tracker: `PROBLEMS.md`
- Missing-module audit: `MISSING_MODULES_AUDIT.md`
- Current source tree inspected: `C:\R\VSPER-SIM`
- Date reviewed: 2026-07-19

## First-Principles Reproducibility Boundary

Core simulation, material construction, Chem+ reasoning, sampling, validation, and reporting should be reproducible from first principles inside the project. The architectural rule is:

- no external simulation libraries as core dependencies
- no external chemistry/material datasets as hidden truth sources
- no externally imported algorithms that become uninspectable kernel behavior
- visual libraries and visual assets are the allowed exception, but only for display, inspection, and media output

External references may be used as comparison targets or validation mirrors, but they must not become injected truth. If a feature cannot be reproduced from internal state, internal rules, and declared script inputs, it should be marked incomplete or external-assisted.

This means MOOSE is useful here as an architectural analogy for object construction, scheduling, and sampling patterns. It is not a dependency target and should not be treated as a source of runtime logic.

## Missing Pipeline Map

The current codebase has strong local components, but the missing work is mostly in the connective pipeline between `.vsim` intent and operator-visible proof.

### What Is Present

- `.vsim` parser accepts the current debugging probes.
- `validate` reports runtime-wired and raw-only capability status for dissolution and material sampling.
- Material registry resolution prints useful provenance for some prototypes.
- The run loop prints a module execution plan before stepping.
- Dissolution has a compiled document-to-engine bridge, persistent per-run state, step traces, and kernel-event evidence.
- Pipeline reporting writes Markdown/JSON summaries.
- Export inventory reports existing artifacts plus requested-artifact status and reasons in `export_audit.tsv`.
- Qt/VTK viewer handoff is the primary callable visual path; the lightweight viewer is compatibility-only.
- Explicit visual `none` suppresses viewer dispatch; requested GL handoff still launches with the generated artifact.
- A PowerShell acceptance command validates and runs the real debugging scripts without Python.

### What Is Missing

| Pipeline stage | Current evidence | Missing link | Next proof |
|---|---|---|---|
| Script capability truth | `validate` reports dissolution and sampling as runtime-wired, disabled, or raw-only. | Capability reporting is scoped to the current debugging sections rather than every language section. | Generalize capability metadata without duplicating parser dispatch tables. |
| Section detail visibility | Sampling expansion keys are listed as raw-only and retained with no runtime effect. | Other forward-compatible sections do not yet receive the same field-level disclosure. | Extend the status contract through parser-owned capability metadata. |
| Runtime orchestration | Runs print a module execution plan for chemistry, dissolution, sampling, and export. | The plan is assembled in the CLI rather than a reusable scheduler object. | Move planning into a central module scheduler as more passes become real. |
| Module dispatch | Dissolution executes on persistent atomistic state and emits `dissolution.surface_pass` kernel events. | The current probe advances protonation but does not cross the declared cleavage barrier; ligand exchange remains inactive. | Add a physically justified cleavage case with event-level energy and coordination evidence. |
| Material construction | Registry resolves `Fe2O3` with `corundum`, but run output showed an Al/O corundum basis and only representative particles. | Prototype resolution is not yet composition-aware enough for engineering trust. | `Fe2O3 + corundum` should resolve Fe/O basis, expected stoichiometry, and generated particle counts. |
| Material sampling | Sampling probe runs and writes artifacts. | `sample_count`, `state_expansion`, `vary`, and variant values do not produce distinct sampled states. | Sampling probe should produce four named sample cases with resolved parameters and per-sample outputs. |
| Chemistry/process workflow | Basic chemistry and Chem+ sections exist elsewhere in examples. | Chemical-engineering workflow concepts are not yet first-class through the full run path: process objective, operating window, sweep variables, and decision metric. | A Chem+ workflow script should report process conditions, objective, candidates, acceptance criteria, and chosen result. |
| First-principles boundary | Project philosophy requires internal reproducibility, with visual-only external exceptions. | Missing modules must not be filled by opaque external libraries or hidden external datasets. | Each new module should declare internal inputs, internal derivation rules, and any visual-only exception. |
| Export contract | Metrics TSV, event JSONL, analysis JSON, report, manifest, and XYZ requests are written and audited. | Less-used export flags still report skipped where no writer is registered in this CLI path. | Add writers only where a real data contract exists; preserve explicit skipped reasons otherwise. |
| Viewer gating | `output_type = "none"` writes artifacts without a viewer launch. | None for the current explicit-headless path. | Keep the suppression probe as a regression gate. |
| Visual handoff | Visual probe launches `vsepr-vtk-view` and receives `vsepr.viewer_ack.v1` with `backend=qt-vtk`; molecular, engineering, and parallel Matplotlib PNG captures prove nonblank rendering. A PNG-only script proves static output does not require a viewer. | Native FE producer routing and automated pixel thresholds remain follow-on work. | Keep backend acknowledgement, PNG signatures, and capture fixtures as regression evidence. |
| UI workflow | Interface files and demos exist. | No minimum operator workflow is proven end to end. | One script plus checklist should prove load, inspect, control, select output, and status feedback. |
| Regression harness | `scripts/debugging/run_acceptance.ps1` validates/runs probes and checks output strings and artifacts. | It is not yet registered as a CTest target. | Add CTest registration after output-directory isolation is available. |

### Most Important Remaining Gaps

1. **Material construction gap:** the dissolution bridge currently constructs a molecular formula state, not a composition-correct resolved crystal surface.
2. **Material sampling gap:** unsupported expansion intent is now disclosed, but sampled state expansion is not implemented.
3. **Dissolution physics gap:** protonation state advances, while cleavage and ligand exchange still need physically justified proof cases.
4. **Capability registry gap:** explicit wired/raw status exists for the active probes but is not yet universal across the language.
5. **UI workflow gap:** artifacts and statuses are clearer, but the minimum operator interface is still unproven.
6. **Reproducibility boundary gap:** future fixes must preserve first-principles internal derivation rather than outsourcing core behavior.

### Current Working Definition Of Done

A feature is not considered working because it parses, appears in a document struct, or has a local C++ module. It is working only when a real `.vsim` script proves:

1. the intent validates without ambiguity,
2. the runtime prints the planned module/pass,
3. the module emits feature-specific events or metrics,
4. requested artifacts are written or skipped with reasons,
5. reports include the feature-specific result,
6. the behavior is reproducible without hidden external data or opaque external core libraries,
7. the behavior is repeatable through a regression command.

## Active Debug Threads

### DBG-VIS-001 - Visual output route

- **Related problem:** `VIS-001`
- **Status:** `RESOLVED - QT/VTK WINDOW AND PAYLOAD PROVEN`
- **Script referenced:** `scripts/demos/bug_exposer_visual_xyz_handoff.vsim`
- **Observed problem:** A `.vsim` run could request visual output, but the final operator-visible window path was not proven end to end.
- **Debug checkpoints:**
  - The script request parses as visual output.
  - The run produces an XYZ or compatible visual artifact.
  - The viewer process starts or an existing viewer is targeted.
  - The generated artifact is handed to the viewer.
  - The viewer acknowledges the intended state without manual file selection.
- **Proof recorded:** `scripts/debugging/run_acceptance.ps1 -IncludeVisualHandoff` validates the BUG_EXPOSER, runs it, requires `[view] Opening session-bound viewer`, requires `[view:ack]`, parses the newest `viewer_ack_*.json`, and checks `backend=qt-vtk`, `ok`, frame count, particle count, and run label.
- **Implementation result:** The optional visual acceptance pass proves a generated `.xyz` reached the Qt/VTK loader and produced `vsepr.viewer_ack.v1` with `ok=true`, `frame_count=1`, `max_particle_count=3`, and a path-independent file content hash. `out/qt_vtk_proof/h2o.png` and `fea.png` add nonblank molecular and engineering evidence.

### DBG-VIS-002 - Headless run still opens viewer

- **Related problem:** `VIS-001`
- **Status:** `RESOLVED`
- **Script added:** `scripts/debugging/bug_exposer_headless_viewer_suppression.vsim`
- **Observed during headless probes:** Runs with `[visual].output_type = "none"` still printed a session-bound viewer open request after writing XYZ output.
- **Run result:** `validate` passed cleanly. `run` wrote 5 artifacts to `out/debugging/headless_viewer_suppression`, then printed `[view] Opening session-bound viewer` with the generated XYZ artifact path.
- **Usable behavior target:** A script that explicitly requests no visual output should write requested artifacts without opening or targeting a viewer.
- **Proof required:** Run the suppression probe and confirm no `[view] Opening session-bound viewer` line appears.
- **Resolution:** Viewer dispatch now treats explicit visual `none` as authoritative unless `[open]` is explicitly enabled. The acceptance command confirms artifacts are written and no viewer-open line appears.

### DBG-EXP-001 - Export artifact reporting

- **Related problem:** `SMALL-001`
- **Status:** `RESOLVED FOR CURRENT EXPORT FLAGS`
- **Script added:** `scripts/debugging/bug_exposer_export_summary.vsim`
- **Observed problem:** Completed script runs do not clearly report where artifacts were written, how many files were written, or the size of each artifact.
- **Run result:** `validate` passed cleanly. `run` printed `Export inventory: out/debugging/export_summary`, listed 6 files with byte sizes, and printed `[export] 6 file(s), 5492 B total`.
- **Remaining gap:** The script requested `write_analysis_json`, `write_metrics_tsv`, and `write_events_json`, but the observed inventory did not include separately named analysis JSON, metrics TSV, or events JSON files. The export summary reports what exists, but does not yet explain requested artifacts that were not written.
- **Secondary gap:** The run still opened a session-bound viewer despite `[visual].output_type = "none"`; tracked separately as `DBG-VIS-002`.
- **Usable behavior target:** After a `.vsim` run, at least one helper or command reports:
  - export directory
  - artifact count
  - file names
  - byte sizes
  - failure reason if no artifacts were written
- **Proof required:** A real script run with captured console output that includes the export summary.
- **Resolution:** Final observe metrics and kernel events are flushed before pipeline reporting. Requested artifacts are audited as `written`, `missing`, or `skipped` in the console and `export_audit.tsv`. The export probe writes XYZ, analysis JSON, metrics TSV, events JSONL, Markdown, and a manifest.

### DBG-UI-001 - Practical operator interface

- **Related problem:** `UI-001`
- **Status:** `UNRESOLVED`
- **Observed problem:** Interface pieces exist, but a coherent minimum workflow is not yet proven.
- **Minimum workflow target:**
  - load or receive a run
  - inspect current state
  - control playback or view behavior
  - select relevant outputs or observables
  - receive clear failure/status feedback
- **Proof required:** One repeatable demo script plus a checklist showing every step was exercised.

### DBG-DISS-001 - Dissolution module bridge

- **Status:** `PARTIAL - RUNTIME WIRED`
- **Script added:** `scripts/debugging/bug_exposer_dissolution_bridge.vsim`
- **Observed today:** The older audit said the VSIM parser failed around `apply_dissolution_key`. The current tree appears to have parser support in place, and `atomistic/reaction/dissolution.cpp` has been updated to use the current `State` fields.
- **Remaining issue:** `include/vsim/dissolution_bridge.hpp` declares a document-to-engine adapter, but no implemented runtime path was found in the inspected source search.
- **Parser note:** `EnvironmentSection` currently exposes temperature, pressure, medium, humidity, and field components, but not `pH`. The dissolution probe keeps acidity controls in `[dissolution]` via `pH_reference` and `pH_slope`.
- **Run result:** `validate` passed cleanly. `run` completed and wrote 6 artifacts to `out/debugging/dissolution_bridge`, but artifact search found no dissolution/protonation/cleavage/hydration/ligand-specific evidence beyond the run label.
- **Probe adjustment:** The first draft used `H2SO4`; runtime printed a colocated-atom formula-build error for `H2SO4`. The probe now uses `HCl` so the dissolution bridge question is isolated from that formula-builder issue.
- **Why this matters:** This is a module-composition gap, not just a chemistry gap. Chem+ needs process-level workflow behavior, while material sampling needs MOOSE-like adapter and scheduler behavior.
- **Proof required:** A `.vsim` script with a `[dissolution]` section must parse, configure the module, run a dissolution pass, emit deterministic events or metrics, and report them.
- **Implementation result:** The bridge constructs solid and solution states with the internal formula parser, maintains one engine across the run, emits per-step summaries, and records `dissolution.surface_pass` events. The probe identifies three sites, advances two protonation stages, and then remains stable. Cleavage and ligand exchange remain zero under the declared conditions, so physics completion remains open.

### DBG-SAMP-001 - Material sampling composition layer

- **Status:** `PARTIAL`
- **Script added:** `scripts/debugging/bug_exposer_material_sampling.vsim`
- **Observed problem:** The project has material prototypes, registries, module interfaces, and script sections, but material sampling is not yet a first-class composition layer.
- **Target architecture:** Chem+ should express chemical-engineering workflow intent. The sampling layer should provide model construction: carrier graph, prototypes, site distributions, defect distributions, coupling rules, solver passes, and report bindings.
- **Parser note:** `[analysis.sampling]` currently accepts `enabled`, `compute_rdf`, `compute_msd`, `min_frames_for_motion`, `min_frames_for_msd`, and `unwrap_pbc`. The probe includes forward-facing sampling-intent keys so validation/runtime output can reveal whether they are ignored, retained raw, or eventually executed.
- **Run result:** `validate` passed cleanly. `run` wrote 6 artifacts to `out/debugging/material_sampling`, but outputs showed repeated generic `SiO2` records with `unknown-sparse` / `energy_nan` warnings rather than four resolved sampled states from the requested prototype variants.
- **Secondary gap:** The run still opened a session-bound viewer despite `[visual].output_type = "none"`; tracked separately as `DBG-VIS-002`.
- **Proof required:** A real `.vsim` sampling script that expands one material prototype into multiple sampled states and reports each sample's resolved configuration and output path.
- **Implementation result:** Validation and run output disclose `sample_count`, `state_expansion`, `vary`, and variant arrays as raw-only keys with no runtime effect. This closes silent acceptance, not sample expansion itself.

### DBG-PROOF-001 - Script-first testing discipline

- **Status:** `ACTIVE REGRESSION GATE`
- **Rule:** New progress should be tested through real `.vsim` scripts. If the script output is not detailed enough to diagnose behavior, that is itself a product defect.
- **Preferred proof stack:**
  - `.vsim` bug exposer or acceptance script
  - captured CLI output
  - generated report/export artifacts
  - focused C++ regression tests for the underlying module
- **Avoid:** Treating Python demos as the primary validation path for core VSIM/Chem+ behavior.
- **Command:** `./scripts/debugging/run_acceptance.ps1` for headless gates, or add `-IncludeVisualHandoff` for the requested viewer path.

### DBG-BUILD-001 - Shared element header contains a stray token

- **Status:** `OPEN - PRE-EXISTING LOCAL CHANGE`
- **Evidence:** A normal `vsepr` rebuild fails at `src/core/element_data.hpp:11` because the bare token `wxsswsxaadsxx` is parsed as a type declaration.
- **Preservation decision:** The source line was not removed because it predates this implementation and belongs to the existing dirty worktree.
- **Verification workaround:** The generated build configuration temporarily used `-Dwxsswsxaadsxx=` so the implementation could compile and link without modifying that source file. The macro was removed from the CMake cache after verification.
- **Required resolution:** Confirm the token is accidental, remove it intentionally, then reconfigure without the temporary macro and rebuild.

### DBG-BUILDER-001 - Builder expansion stops before state mutation

- **Status:** `PARTIAL - CANONICAL PREVIEW PROVEN`
- **Script added:** `scripts/improvement_acceptance/builder_distribution_preview.vsim`
- **Implemented:** The canonical parser now owns typed named distributions and material declaration provenance. `vsepr expand` reports each builder value as written or inferred and classifies its runtime status.
- **Observed proof:** The acceptance script validates and expands through the production CLI. Its material fields resolve through the internal registry, its distribution is complete, and preview reports zero executed records.
- **Intentional boundary:** A complete card is marked `future_runnable`; preview does not place particles, mutate state, or claim runtime execution.
- **Remaining issue:** Distribution placement and cross-domain adapters still need typed ownership, units, scheduling, events, and report-artifact contracts.
- **Proof required next:** A real `.vsim` script must execute one deterministic placement pass, emit resolved placement records and kernel events, and persist the same provenance in a report artifact.

## Builder Expansion Implementation Record

This table records the complete Chapter 5 implementation slice. Internal planning labels were kept out of runtime-facing filenames, schema keys, CLI names, and new core comments.

| File | State | Change |
|---|---|---|
| `include/vsim/vsim_document.hpp` | Modified | Added material declaration provenance, typed named distributions, compatibility warnings for incomplete cards, summary counts, and document storage. |
| `include/vsim/vsim_parser.hpp` | Modified | Added distribution dispatch state and the typed distribution key applier declaration. |
| `src/vsim/vsim_parser.cpp` | Modified | Added canonical named-distribution parsing, typed scalar/vector fields, unknown-key preservation, and material provenance tracking. Six invalid legacy punctuation bytes were normalized to ASCII so the file could be patched; unrelated existing parser changes were retained. |
| `include/cli/script_expansion.hpp` | Existing untracked file extended | Added builder origin/status types, explicit builder records, distribution summaries, and aggregate counts. |
| `src/cli/script_expansion.cpp` | Existing untracked file replaced | Removed the private expansion mini-parser and built previews from `VsimParser`, validation, and the internal material registry. No execution path was added. |
| `apps/vsepr.cpp` | Modified | Corrected the `expand` command route to invoke the expansion preview. |
| `tests/test_script_expansion.cpp` | Existing untracked file extended | Added a typed distribution fixture, provenance/status assertions, and runtime checks that remain active in Release builds. |
| `scripts/improvement_acceptance/builder_distribution_preview.vsim` | New | Added the real neutral-named material/distribution acceptance script. |
| `scripts/improvement_acceptance/README.md` | New | Documented the script contract, commands, expected evidence, and non-execution boundary. |
| `scripts/debugging/run_acceptance.ps1` | Existing untracked file extended | Added builder script validation and expansion-output assertions. |
| `2026-07-19_debugging.md` | Existing untracked file extended | Recorded implementation scope, exact files, verification, limitations, and next proof. |

## Implementation Record

| File | Change |
|---|---|
| `src/cli/cmd_run_vsim.cpp` | Added module execution plan, internal atomistic state construction for dissolution, persistent bridge calls, metrics/event flushing, export request audit, and explicit visual-none gating. |
| `include/vsim/dissolution_bridge.hpp` | Defined the document-to-engine bridge contract and pass result. |
| `src/vsim/dissolution_bridge.cpp` | Mapped declared parameters into the engine, advanced persistent state, and emitted deterministic kernel evidence. |
| `atomistic/reaction/dissolution.hpp` | Exposed cached surface-site count so the bridge does not reset protonation state each step. |
| `atomistic/reaction/dissolution.cpp` | Replaced the local element-symbol and per-metal pKa tables with the internal periodic database, state/composition oxidation inference, and free-energy-derived protonation equilibrium. |
| `src/cli/cmd_validate.cpp` | Added capability status and raw-only key disclosure for the active sections. |
| `cmake/CoreBuild.cmake` | Compiled the bridge and linked the existing analysis-helper library required by observe metric export. |
| `scripts/debugging/run_acceptance.ps1` | Added script-first validation/run assertions with optional visual handoff. |
| `scripts/debugging/README.md` | Documented probe purpose and execution. |
| `include/vsim/vsim_document.hpp` | Added typed distribution intent and material declaration provenance. |
| `include/vsim/vsim_parser.hpp` | Added named-distribution parser routing declarations and state. |
| `src/vsim/vsim_parser.cpp` | Parsed named distributions through the canonical parser and retained unknown fields for forward compatibility. |
| `include/cli/script_expansion.hpp` | Defined builder provenance, runtime status, distribution summaries, and preview counts. |
| `src/cli/script_expansion.cpp` | Built expansion records from the canonical document and internal material registry. |
| `apps/vsepr.cpp` | Routed `expand` to the builder expansion preview. |
| `tests/test_script_expansion.cpp` | Added executable Release-safe regression checks for the preview contract. |
| `scripts/improvement_acceptance/builder_distribution_preview.vsim` | Added the real script-first acceptance case. |
| `scripts/improvement_acceptance/README.md` | Documented the builder acceptance surface and current boundary. |

## Verification Record

- `vsepr` built and linked with GCC 15.2 after temporarily applying the generated-build-only macro described in `DBG-BUILD-001`; the cache was restored afterward.
- `run_acceptance.ps1 -IncludeVisualHandoff` passed.
- Headless probe wrote artifacts and did not emit a viewer-open request.
- Export probe wrote and audited all six requested artifact classes.
- Dissolution probe emitted persistent step traces and `dissolution.surface_pass` JSONL records.
- Sampling validation named every forward expansion key as raw-only with no runtime effect.
- `ScriptExpansionPreviewTest` passed through CTest.
- The builder acceptance script passed direct `validate` and `expand` commands.
- Builder preview reported `canonical_parser: true`, one complete distribution, written and inferred records, twelve future-runnable records, and zero executed records.
- The full PowerShell acceptance suite passed with its builder expansion audit enabled.
- `git diff --check` passed for the implementation files.

## Immediate Next Actions

1. Resolve `DBG-BUILD-001`, remove the generated-build macro, and prove a normal clean incremental build.
2. Replace the remaining ligand-rate and preferred-coordination heuristics with internal graph, charge, and declared-parameter derivations.
3. Build the solid from the resolved material prototype and cell rather than molecular formula geometry so surface coordination is engineering-grade.
4. Implement first-class sample expansion for `sample_count`, `state_expansion`, `vary`, and variant values; retain the raw-only warning until that path is real.
5. Add a requested-but-unavailable export probe so `missing` and `skipped` reason behavior remains regression-tested.
6. Prove the minimum operator UI workflow after the runtime artifacts and statuses are stable.
7. Define immutable resolved-builder and adapter records with units and ownership, then implement one deterministic distribution placement pass with script-visible events and report evidence.

## Change Log

- 2026-07-19: Created this dated debugging ledger beside `PROBLEMS.md`.
- 2026-07-19: Added debugging VSIM probes for export summary, dissolution bridge wiring, and material sampling composition.
- 2026-07-19: Added headless viewer suppression probe and recorded validation/run evidence from the debugging scripts.
- 2026-07-19: Implemented viewer intent gating, export accountability, dissolution runtime dispatch, sampling capability disclosure, and script-first acceptance automation.
- 2026-07-19: Built and ran all acceptance probes, including requested visual handoff; recorded the pre-existing shared-header build blocker and non-source workaround.
- 2026-07-19: Implemented typed distribution parsing and canonical builder expansion preview, added a real acceptance script, and proved zero-execution audit behavior through CTest and the full PowerShell suite.
- 2026-07-22: Rechecked `DBG-BUILD-001`; the stray shared-header token no longer exists in the live tree and `vsepr` builds normally from the `release` preset.
- 2026-07-22: Registered `scripts/debugging/run_acceptance.ps1` as the `VsimDebuggingAcceptance` CTest target. Focused CTest run passed, preserving the script-first gate for headless visual intent, export accountability, dissolution dispatch, sampling capability disclosure, and builder expansion preview.
- 2026-07-22: Next implementation lane remains material sampling expansion: turn the current raw-only disclosure for `sample_count`, `state_expansion`, `vary`, and variants into deterministic sampled-state records before promoting process or registry claims.
- 2026-07-22: Closed `DBG-VIS-001` / `VIS-001` for the `.vsim` visual-output route by making `vsepr-vtk-view` the default, rebuilding the visual preset, and passing `scripts/debugging/run_acceptance.ps1 -IncludeVisualHandoff` with backend-specific acknowledgement plus molecular and FEA capture evidence.
- 2026-07-22: Preserved and wired the Matplotlib PNG subsystem as a parallel post-run backend through trailing `[export.visual]` configuration. Acceptance now proves both combined Qt/VTK plus PNG output and independent PNG-only output.
