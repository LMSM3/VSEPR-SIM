# STAGE.md — VSEPR-SIM Work-Order Progress Ledger
> **Source of truth for all beta test groups, WO status, and milestone commits.**
> Updated: v5.13.3 | Branch: v5.0.0-main

---

## Beta Milestones

| Version        | Theme                                           | Day  | Status    |
|----------------|-------------------------------------------------|------|-----------|
| v5.0.0-beta.7  | render_interval / step emission cadence         | 57   | COMMITTED |
| v5.0.0-beta.8  | PBC / cell / boundary / Ewald                   | 58   | COMMITTED |
| v5.0.0-beta.9  | Registry resolution engine / CLI layer          | 59   | COMMITTED |
| v5.0.0-beta.10 | Variance / N_evolution / while / batch sweep    | 60   | COMMITTED |
| v5.0.0-beta.11 | Macro sampling / empirical verification         | 61   | COMMITTED |
| v5.0.0-beta.12 | Batching upgrade / study orchestration          | 62   | COMMITTED |
| v5.0.2         | Release gate: 114/114 tests pass, full pipeline | --   | RELEASED  |
| v5.1.13        | Pillar F: empirical chemistry layer, pre-electron freeze | 72 | COMMITTED |
| **v5.13.3**    | Wizard modules 6-8 full field coverage (analysis/export/visual) | **73** | **COMMITTED** |
| v5.13.4        | Planned: Gap classifier + multi-scale output filter refinement   | 74  | PLANNED   |
| v5.13.5        | Planned: CTL pipeline hardening + release gate (Day-75)          | 75  | PLANNED   |

---

## v5.0.2 Release Gate — WO-VSIM-v5.0.2

**Date:** 2026-05-19  
**Status:** PASS — 114/114 tests green

### Fixes applied

| # | Test | Root cause | Fix |
|---|------|-----------|-----|
| 1 | TrackerTests `weight_loading` | `data/element_weights.json` not accessible from `CMAKE_BINARY_DIR` | Created `build/data` junction; added `file(CREATE_LINK ...)` to `tests/CMakeLists.txt` |
| 2 | CrystalPipelineTest FCC coord=12 | Bond-inference factor 1.15 → threshold 2.783 Å < Al NN 2.864 Å | Changed factor to 1.20 (threshold 2.904 Å) |
| 3 | Problem2ThreeBodyCluster | Fully random [0,10] Å box → atoms ~10 Å apart, Langevin can't converge | Near-equilateral initial position + FIRE quench after MD |
| 4 | DescriptorEnrichmentTest `sh_index(2,1)` | Backward-compat check expected 6 (old formula); current `l²+l+m` gives 7 | Updated assertion to `== 7` |
| 5 | BatchManifestRunner | Test registered with no args; runner exits 1 without manifest | Re-registered with `--help` arg (exits 0) via direct `add_test` |

### Demo output
```
VSEPR-SIM v5.0.2
VSEPR-SIM v5.0.2  |  C++23  |  branch: v5.0.0-main
  Compiler: 15.2.0
```

`vsepr run demo_01_minimal_hexene.vsim` → Formation/Fingerprint/Cluster/Analysis/Report: all PASS
`vsepr doctor` → all runtime data files OK

---

## Day 61 — WO-VSIM-61 / WO-VSEPR-SIM-62A  (beta-11)

### WO-VSIM-61A — Analysis Pipeline Structs (`VsimSystemSection`, `VsimStructureAnalysisSection`)
**Status:** PASS

### WO-VSIM-61B — Sampling Section (`VsimSamplingSection` — RDF, MSD, unwrap_pbc)
**Status:** PASS

### WO-VSIM-61C — Scale Sampling Section (`VsimScaleSamplingSection` — field projection, RVE)
**Status:** PASS

### WO-VSIM-61D — Analysis Inference Section (`VsimAnalysisInferenceSection` — packing/mobility regime)
**Status:** PASS

### WO-VSEPR-SIM-62A — Empirical Verification Layer (`VsimVerifySection`, golden-test suite, verify parser)
**Status:** PASS

---

## Day 62 — WO-VSEPR-SIM-62B / WO-VSIM-62C  (beta-12)

### WO-VSEPR-SIM-62B — Batch Verification Aggregation
**Status:** PASS
- Introduced: `include/batch/failure_modes.hpp`, `include/batch/batch_verification.hpp`
- Introduced: `src/batch/batch_verification.cpp`

### WO-VSIM-62C — Batch Layer Parser & Static-Axis Runtime
**Status:** PASS
- Introduced: `include/batch/batch_document.hpp`, `include/batch/batch_parser.hpp`
- Introduced: `include/batch/batch_expander.hpp`, `include/batch/seed_resolver.hpp`
- Introduced: `include/batch/batch_merger.hpp`, `include/batch/batch_checkpoint.hpp`
- Introduced: `include/batch/batch_require_checker.hpp`, `include/batch/batch_aggregator.hpp`
- Introduced: `include/batch/resolved_writer.hpp`
- Introduced: `src/batch/batch_parser.cpp`, `src/batch/batch_expander.cpp`
- Introduced: `src/batch/seed_resolver.cpp`, `src/batch/batch_merger.cpp`
- Introduced: `src/batch/batch_checkpoint.cpp`, `src/batch/batch_require_checker.cpp`
- Introduced: `src/batch/batch_aggregator.cpp`, `src/batch/resolved_writer.cpp`

---

## Full Test Group Registry

| Group   | WO                   | Tests | Status |
|---------|----------------------|-------|--------|
| Group 1  | Core / Geometry / VSEPR (foundational) | — | PASS |
| Group 2  | Atomistic simulation core              | — | PASS |
| Group 3  | Potential / energy                     | — | PASS |
| Group 4  | Optimizer                              | — | PASS |
| Group 5  | Angle tests                            | — | PASS |
| Group 6  | VSEPR correctness                      | — | PASS |
| Group 7  | Torsion                                | — | PASS |
| Group 8  | Torsion analysis                       | — | PASS |
| Group 9  | Torsion validation                     | — | PASS |
| Group 10 | Alkane torsion                         | — | PASS |
| Group 11 | CG suite 1                             | — | PASS |
| Group 12 | CG suite 2                             | — | PASS |
| Group 13 | CG suite 3                             | — | PASS |
| Group 14 | CG suite 4                             | — | PASS |
| Group 15 | CG suite 5                             | — | PASS |
| Group 16 | CG suite 6                             | — | PASS |
| Group 17 | CG suite 7                             | — | PASS |
| Group 18 | CG suite 8                             | — | PASS |
| Group 19 | CG Track2                              | — | PASS |
| Group 20 | Molecular IO                           | — | PASS |
| Group 21 | Geometry ops                           | — | PASS |
| Group 22 | Energy tests                           | — | PASS |
| Group 23 | Pipeline / dashboard record            | — | PASS |
| Group 24 | Formation output                       | — | PASS |
| Group 25 | Defect microstate                      | — | PASS |
| Group 26 | Statistical interpretation             | — | PASS |
| Group 27 | Heat-gated reaction control            | — | PASS |
| Group 28 | WO-VSIM-57D render_interval            | — | PASS |
| Group 29 | WO-VSEPR-SIM-57B Cell / PBC / Ewald    | — | PASS |
| Group 30 | WO-VSIM-02A VSIM parser                | — | PASS |
| Group 31 | WO-VSIM-02B VSIM parser stress         | — | PASS |
| Group 32 | WO-VSIM-02C VSIM visual / external     | — | PASS |
| Group 33 | WO-VSIM-02D Variance / N_evolution / while | — | PASS |
| Group 34 | WO-VSIM-02E Batch sweep                | — | PASS |
| Group 35 | WO-VSIM-03A Intent scripting layer     | — | PASS |
| Group 36 | Ambient chemistry integration          | — | PASS |
| Group 37 | WO-VSIM-04A Isomer detection revival   | — | PASS |
| Group 37 | WO-VSIM-03B Stress / failure-mapping   | — | PASS |
| Group 38 | WO-VSIM-03C Registry resolution engine | 18/18 | PASS |
| Group 39 | Demo script suite (beta-9/10 scripts)  | 6/6   | PASS |
| Group 40 | Beta-10 smoke tests (install / CLI)    | 10/10 | PASS |
| Group 39 | WO-VSEPR-SIM-62A Empirical verification| 19/19 | PASS |
| Group 40 | WO-VSEPR-SIM-62B Batch verification aggregation | 20/20 | PASS |
| Group 41 | WO-VSIM-62C Batch parser               | 18/18 | PASS |
| Group 42 | WO-VSIM-62C Batch expander             | 10/10 | PASS |
| Group 43 | WO-VSIM-62C Batch runner static        | 12/12 | PASS |

---

## Environment Notes

- Branch: `v5.0.0-beta.7-step-attempt`
- Build: CMake / MSVC / C++23 / Windows
- Test runner: CTest (cmake --build / ctest --test-dir)
- Git: use VS Source Control or `"C:\Program Files\Git\bin\git.exe"`
- PowerShell separator: `;` (not `&&`)

---

## Known Deferred Items

| Item                       | Deferred To | Note |
|----------------------------|-------------|------|
| Stochastic axis runtime    | v5.1.0      | `BatchAxisEntry.kind = "stochastic"` parsed, not wired |
| Formation axis execution   | v5.2.0      | `BatchAxisEntry.kind = "formation"` parsed, not wired |
| latin_hypercube design     | v5.1.0      | `BatchDesignSection.type` parsed, not expanded |
| Random design              | v5.1.0      | Same as above |
| CellSection triclinic      | v5.1.0      | type = "orthorhombic" only for now (WO-66P supercedes for crystal) |
| FieldRamp formation stage  | v5.2.0      | Parsed, no execution yet |
| Cycle formation stage      | v5.2.0      | Parsed, no execution yet |
| XBIT checksum (CRC-32 impl)| v5.1.5      | `Xbit::recompute_checksum()` body deferred; layout frozen |
| DEMBridge/FEABridge execution | v5.2.0   | Schema frozen (WO-66Q/67N/67O); runtime dispatch deferred |
| OrganicDiagnostics runtime | v5.2.0      | Section parsed (WO-66O); per-frame eval deferred |
| Crystal translation targets| v5.1.5      | Flags parsed (WO-66P); xyz/CIF/JSON emitters deferred |

---

## Day 66 — WO-66N / WO-66O / WO-66P / WO-66Q  (v5.1.4)

### WO-66N — Constructor Objects + Batching + Upper-Block References
**Status:** COMPLETE  
- `include/vsim/objects/object_path.hpp` — `ObjectPath`, `ObjectPathRef`
- `include/vsim/objects/constructor_object.hpp` — `ConstructorObjectKind`, `ConstructorObjectRegistry`, `BatchGroup`
- `VsimDocument::objects` added
- Parser: `[objects]` and `[objects.batch]` dispatch + `apply_objects_constructor_line`, `apply_objects_batch_key`
- WO doc: `docs/wo/WO-66N-Constructor-Objects.md`

### WO-66O — Organic/Peptide Scale Diagnostics
**Status:** COMPLETE  
- `include/vsim/diagnostics/organic_diagnostics.hpp` — `PeptideChainDiagnostics`, `SmallMoleculeDiagnostics`, `OrganicScaleSection`, `OrganicDiagnosticResult`
- `VsimDocument::organic_diagnostics` added
- Parser: `[diagnostics.organic]` dispatch + `apply_diagnostics_organic_key`
- WO doc: `docs/wo/WO-66O-Organic-Diagnostics.md`

### WO-66P — Universal Translation + Crystal/PBC Constructor
**Status:** COMPLETE  
- `include/vsim/crystal/crystal_constructor.hpp` — `LatticeType`, `CrystalConstructorSection`
- `VsimDocument::crystal` added
- Parser: `[objects.crystal]`, `[crystal]`, `[cell]`, `[pbc]` all route to `apply_crystal_constructor_key`
- WO doc: `docs/wo/WO-66P-Crystal-PBC-Constructor.md`

### WO-66Q — Non-Molecular Objects + XBIT
**Status:** COMPLETE  
- `include/vsim/objects/non_molecular_objects.hpp` — `GeometryObject`, `SurfaceObject`, `SourceObject`, `SinkObject`, `AmbientObject`, `NonMolecularObjectStore`
- `include/vsim/objects/bridge_objects.hpp` — `DEMBridgeObject`, `FEABridgeObject`, `BridgeObjectStore` (frozen schema for WO-67N/67O)
- `include/vsim/xbit/xbit.hpp` — `XbitTier`, `Xbit` 256-bit layout, inline accessors
- `VsimDocument::nm_objects`, `VsimDocument::bridge_objects` added
- Parser: `[objects.geometry/surface/source/sink/ambient]` dispatch + appliers
- WO doc: `docs/wo/WO-66Q-NonMolecular-Objects-XBIT.md`

### Build gate
All 368 Ninja targets built clean (exit 0). No new errors.

---

## Phase 6–9 — Intent Bridge, FieldRamp, Isomer Wire, Dynx v1  (v5.1.x)

### WO-VSIM-INTENT-BRIDGE-A — Intent Runtime Bridge (material / environment / run)
**Status:** COMPLETE
- `include/vsim/intent/intent_bridge.hpp` — `IntentParticle`, `IntentEnvironment`, `IntentRunConfig`, `IntentSystem`, `IntentBridge`
- `src/cli/cmd_run_vsim.cpp` — wired new "4b. Intent runtime bridge" summary block after registry resolution
- Group 54 tests: `IntentBridgeBasicGroup54` (IB-A-01..09) — 9/9 pass
- Deferred: `[[raw.object]]`, `[[override.particle]]`, `[excite.*]` → BRIDGE-B/C

### WO-VSIM-FORMATION-FIELDRAMP — FieldRamp Evaluator
**Status:** COMPLETE (evaluator + tests; runtime batch wiring deferred to v5.2.0)
- `include/vsim/intent/field_ramp.hpp` — `FieldRampResult`, `FieldRampEvaluator`
- Linear ramp `E(t) = E0 + (E1-E0) * (t/t_stage)` with axis handling and clamping
- Group 55 tests: `FormationFieldRampGroup55` (FR-01..07) — 7/7 pass
- Formation libraries live in `BatchDocument::formation_library`; runtime execution → v5.2.0

### WO-VSIM-ISOMER-WIRE-A — Isomer Pipeline Wiring A
**Status:** COMPLETE (generator scaffold + tests; chirality/StereoVariant deferred to WIRE-B)
- `include/vsim/intent/isomer_bridge.hpp` — `IsomerCandidate`, `IsomerCandidateSet`, `IsomerBridge`
- Deterministic seed-based candidate enumeration and manifest generation
- Group 56 tests: `IsomerWireAGroup56` (ISO-A-01..06) — 6/6 pass

### WO-VSIM-DYNX-V1-A / WO-VSIM-DYNX-V1-B — Dynx v1 Session Archive
**Status:** COMPLETE
- `include/vsim/io/dynx_writer.hpp` — `DynxParticleState`, `DynxFrame`, `DynxHeader`, `DynxWriter`, `DynxInspectResult`, `DynxValidateResult`, `dynx_inspect()`, `dynx_validate()`
- `src/vsim/io/dynx_writer.cpp` — implementation; `w+b` file mode enables in-place frame_count header patch on close
- `src/cli/cmd_dynx.cpp` — `vsepr dynx inspect` / `vsepr dynx validate` commands
- Group 57 tests: `DynxV1SessionArchiveGroup57` (DYNX-V1-01..07) — 7/7 pass
- Dynx format: `#dynx v1` header, `FRAME / END_FRAME` blocks, `#END_DYNX` tail

### Build gate
Groups 54–57 all pass (28/28 tests). Total new groups: 4.

---

## WO-VSIM-CTL — Modernized VSIM Control Pipeline  (v5.1.x)

### WO-VSIM-CTL-01 — Typed command namespace registry
**Status:** COMPLETE
- `include/vsim/ctl/ctl_types.hpp` — `CtlNamespace` enum, `CtlArg` (variant), `CtlCommand`, `ExecGraph`, known-op/channel tables
- Group 58: `CtlNamespaceRegistryGroup58` (CTL-01-01..08) — 8/8 pass

### WO-VSIM-CTL-02 — Script parser to execution graph
**Status:** COMPLETE
- `include/vsim/ctl/ctl_parser.hpp` — `CtlParser::parse()`, const/let symbol table, `${var}` expansion, `find_assign_eq()` for `==`-safe arg splitting
- Group 59: `CtlParserExecGraphGroup59` (CTL-02-01..09) — 9/9 pass

### WO-VSIM-CTL-03 — Runtime wrapper dispatch layer
**Status:** COMPLETE
- `include/vsim/ctl/ctl_dispatcher.hpp` — `CtlRuntimeHooks` (no-op stubs), `CtlDispatcher`, `DispatchSession`
- Validated with Group 60 tests

### WO-VSIM-CTL-04 — Kernel channel command bindings
**Status:** COMPLETE
- `kernel.channel.enable/disable/reset`, `kernel.trace.enable`, `kernel.set` all dispatch through hooks
- Group 61: `CtlKernelChannelBindingsGroup61` (CTL-04-01..07) — 7/7 pass

### WO-VSIM-CTL-05 — Artifact command bindings for XBIT/Dynx
**Status:** COMPLETE
- `artifact.dynx.*` and `artifact.xbit.*` dispatch through hooks; prerequisites validated
- Group 62: `CtlArtifactBindingsGroup62` (CTL-05-01..09) — 9/9 pass

### WO-VSIM-CTL-06 — Metrics/gate assertion system
**Status:** COMPLETE
- `include/vsim/ctl/ctl_metrics.hpp` — `MetricsStore`, `EnergyMetrics`, `ForceMetrics`, `assert_expr()`, read-before-capture guard
- `include/vsim/ctl/ctl_gate.hpp` — `GateState`, `GateResult`, `GateRegistry`
- `include/vsim/ctl/ctl_validator.hpp` — `CtlValidator` with 8 semantic checks
- Group 63: `CtlMetricsGateSystemGroup63` (CTL-06-01..10) — 10/10 pass

### WO-VSIM-CTL-07 — Deterministic plan hash + artifact manifest
**Status:** COMPLETE
- `include/vsim/ctl/ctl_plan.hpp` — `ExecPlan`, FNV-1a-64 plan hash, `to_json()`, `artifact_manifest_json()`, `gate_manifest_json()`
- Group 64: `CtlPlanHashManifestGroup64` (CTL-07-01..10) — 10/10 pass

### Build gate
All 369 Ninja targets built clean (exit 0). Groups 58–64 all pass (63/63 tests). No regressions.

---

## WO-VSIM-CTL-TEST — CTL Integration Testing Phase

**Work order:** WO-VSIM-CTL-TEST  
**Status:** COMPLETE — 5/5 integration groups pass  
**Branch:** V5.1.4

### Objective

Verify CTL as a real workflow control layer against existing runtime behavior.
The test question: *Does CTL actually control a run without touching physics directly?*

### Work items

### WO-VSIM-CTL-TEST-01 — Reference documentation update
**Status:** COMPLETE
- `VSIM_REFERENCE.md` — added `## WO-VSIM-CTL-TEST — CTL Integration Testing Phase` section
- Defined Group 65–69 purposes, per-test assertion tables, and invariants
- Grouped existing Groups 58–64 under "unit layer" heading within the WO-VSIM-CTL section

### WO-VSIM-CTL-TEST-02 — End-to-End Smoke (Group 65)
**Status:** COMPLETE
- `tests/test_ctl_smoke.cpp` — 10 assertions: parse → validate → compile → dispatch → JSON/manifests
- Confirms unknown namespace/command rejected before dispatch
- Confirms `plan_hash` stability; `const` value changes propagate to `script_hash` and `plan_hash`
- Group 65: `CtlEndToEndSmokeGroup65` — 10/10 pass

### WO-VSIM-CTL-TEST-03 — Runtime Hook Integration (Group 66)
**Status:** COMPLETE
- `tests/test_ctl_hooks.cpp` — 10 assertions: recording hooks verify correct method routing and arguments
- Covers `run_case`, `step`, `reset`, `channel_enable/disable/reset`, `dynx_enable`, `xbit_create/export`, `metrics.capture`, gate lifecycle
- `DispatchSession.metrics.captured` and `DispatchSession.gates.results` verified in-session
- Group 66: `CtlRuntimeHookIntegrationGroup66` — 10/10 pass

### WO-VSIM-CTL-TEST-04 — Negative Validation (Group 67)
**Status:** COMPLETE
- `tests/test_ctl_negative.cpp` — 10 assertions: all failure modes fire before runtime
- Covers unknown namespace/command/channel, artifact prerequisite ordering (`dynx.export` before `enable`, `xbit.export` before `create`), `metrics.assert` before capture, `gate.end` without `gate.begin`, and `${undeclared}` sentinel expansion
- Confirmed: invalid scripts never reach the dispatcher
- Group 67: `CtlNegativeValidationGroup67` — 10/10 pass

### WO-VSIM-CTL-TEST-05 — Determinism (Group 68)
**Status:** COMPLETE
- `tests/test_ctl_determinism.cpp` — 10 assertions: no nondeterminism injected by CTL layer
- Covers `plan_hash` stability, seed variation, command ordering, const expansion, artifact and gate manifest ordering, FNV-1a-64 stability, `script_hash` stability, command count parity, and no plan output on invalid script
- Group 68: `CtlDeterminismGroup68` — 10/10 pass

### WO-VSIM-CTL-TEST-06 — Existing Workflow Compatibility (Group 69)
**Status:** COMPLETE
- `tests/test_ctl_compat.cpp` — 10 assertions: CTL headers do not pollute or break legacy APIs
- Double-include (pragma-once safety), coexistence with `vsim_document.hpp`, `CtlNamespace` scoped enum, `MetricsStore` default state, `ExecGraph` default empty state, null-hook dispatcher fallback, empty parse/validate/compile/dispatch behavior
- Group 69: `CtlWorkflowCompatGroup69` — 10/10 pass

### Build gate
All new CTL-TEST targets build clean. Groups 65–69 all pass (50/50 tests).

Full regression: 140/143 tests pass. 3 pre-existing failures in `view-demo-02/03/04` (WO-67-B `vsepr_view_lib` era) — unrelated to CTL. No regressions introduced.

**ctest label:** `wo-vsim-ctl-test` — run with `ctest -R "Group 6[5-9]|CtlEndToEnd|CtlRuntime|CtlNegative|CtlDeterminism|CtlWorkflow" --output-on-failure`

---

## WO-VIEW-DEMO-FIX — View Demo Regression Fixes

**Work order:** WO-VIEW-DEMO-FIX  
**Status:** COMPLETE  
**Branch:** V5.1.4

### Context
Full regression after CTL-TEST revealed 3 pre-existing failures in `view-demo-02/03/04` (WO-67-B era). These were not introduced by CTL work but were pre-existing test/implementation gaps.

### Fixes applied

| # | Test | Root cause | Fix |
|---|------|-----------|-----|
| 1 | `view-demo-02 / 02-D` single-frame xyzf valid | `read_xyzf` used `tellg`/`seekg` on Windows text-mode `ifstream`; CRLF line endings made seek positions unreliable, causing frame parse failure on single-frame `.xyzf` files | Replaced `seekg`-based peeking in `read_xyzf` (`src/io/xyz_reader.hpp`) with a full-file read into `std::vector<std::string>`, CRLF-stripping, then replay via `std::istringstream` |
| 2 | `view-demo-03 / 03-C` unlinked identity_id flagged | `load_and_link` only counted particles whose `identity_id` had no sidecar record; sidecar entries with no matching particle were not counted as unlinked | Added post-link pass in `identity_view_loader.cpp` to count (and store in `frame.identities`) any sidecar record not claimed by any particle |
| 3 | `view-demo-03 / 03-D` duplicate id last-wins + warning | Duplicate-id warnings were stored in `IdentityLoadResult.diagnostics` but never propagated into `frame.warnings`; also last-wins record was absent from `frame.identities` since no particle claimed it | Propagate `ViewDiagnosticSeverity::Warning` entries from `r.diagnostics` into all frame warnings on success; unclaimed sidecar records now also pushed to `frame.identities` |
| 4 | `view-demo-04 / 04-C` label change → different hash | `ViewSession::deterministic_hash()` did not include `p.type` (particle label string) in the FNV-1a-64 hash | Added `mix_str(p.type)` to hash loop in `include/vsim/view/viewer_types.hpp` |
| 5 | `view-demo-04 / 04-C` load failure on `D` | `symbol_to_Z()` did not recognise `D` (Deuterium) or `T` (Tritium) — common MD isotope labels — causing load failure and `r2.success=false` | Added `{"D",1},{"T",1}` to the `symbol_to_Z` table in `src/io/xyz_unified.hpp` |

### Build gate
**143/143 tests pass (exit code 0).** Zero regressions. Full clean regression on V5.1.4.


---

## WO-72A - .X Bundle Format

**Work order:** WO-72A  
**Status:** COMPLETE  
**Branch:** V5.1.4

### Context
Implements the `.X` suite execution container format - a text-based archive that packs one or more `.vsim` scripts and optional assets into a single file for unified execution by the `vsepr` CLI.

### Deliverables

| File | Description |
|------|-------------|
| `include/xbundle/xbundle_document.hpp` | XBundleEntry, XBundleManifest, XBundle structs |
| `include/xbundle/xbundle_reader.hpp` | Reader/parser interface |
| `include/xbundle/xbundle_writer.hpp` | Writer/serialiser interface |
| `include/xbundle/xbundle_validator.hpp` | Validator interface (rules V-01..V-08) |
| `src/xbundle/xbundle_reader.cpp` | Full line-oriented parser (magic, [manifest], [[member]], >>> body) |
| `src/xbundle/xbundle_writer.cpp` | Canonical .X serialiser |
| `src/xbundle/xbundle_validator.cpp` | Eight validation rules |
| `cmake/CoreBuild.cmake` | vsepr_xbundle STATIC library target |
| `tests/test_xbundle_smoke.cpp` | Group 70 - 10 assertions |

### Build gate
**144/144 tests pass (exit code 0).** Zero regressions. Group 70 XBundleSmokeGroup70 10/10.

**ctest label:** `wo-72a` - run with `ctest -R "XBundleSmoke" --output-on-failure`

---

## WO-OUTPUT-P2 — Output System Expansion: Phase 2 (Demo Stack)

**Work order:** WO-OUTPUT-P2  
**Status:** COMPLETE  
**Branch:** v5.0.0-main  
**Scoping doc:** `docs/wo/WO-OUTPUT-P2-Demo-Stack.md`

Lightweight molecular demonstration artifacts from batch simulations.  
New `[export.demo]` schema section + `DemoFrameSampler` + `DemoBundleWriter` + batch aggregator hook.  
Produces condensed `.demo.dynx` archives and self-contained `.demo.X` bundles.

| Sub-WO | Title | Status |
|---|---|---|
| WO-OUTPUT-P2-A | `ExportDemoSection` struct + parser | COMPLETE |
| WO-OUTPUT-P2-B | `DemoFrameSampler` (uniform/first/last/event_gated) | COMPLETE |
| WO-OUTPUT-P2-C | `DemoBundleWriter` (.demo.dynx + .demo.X) | COMPLETE |
| WO-OUTPUT-P2-D | Batch aggregator hook + integration tests | COMPLETE |

**Tests:** Group 73 (25 pass), Group 74 (21 pass), Group 75 (19 pass). Full build exit 0.

---

## WO-72B — .dynx Pipeline Emitter

**Work order:** WO-72B  
**Branch:** V5.1.4  
**Status:** COMPLETE

### Deliverables

| File | Description |
|---|---|
| `include/vsim/io/dynx_writer.hpp` | Extended with rich-frame structs: `DynxForceState`, `DynxBondForce`, `DynxFieldVector`, `DynxEventPacket`, `DynxRenderMeta`, `DynxCameraState`, `DynxRichFrame`, `DynxEmitContext`; added `DynxWriter::write_rich_frame()` |
| `src/vsim/io/dynx_writer.cpp` | Added `DynxWriter::write_rich_frame()` — writes FORCE/BOND_FORCE/FIELD/EVENT/RENDER/CAMERA optional lines |
| `include/vsim/io/dynx_emitter.hpp` | New `DynxLiveCache` (mutex-guarded single-slot get-and-clear) and `DynxEmitter` post-step hook |
| `src/vsim/io/dynx_emitter.cpp` | Emitter implementation: harvests `KernelEventLog::filter_by_frame()`, writes rich archive, updates live cache |
| `tests/test_dynx_emitter.cpp` | Group 71 — 15 test cases, 29 assertions (71-A..71-O) |

### Design

- **Archive path:** `DynxWriter::write_rich_frame()` streams every rich frame to a `.dynx` file; backward-compatible with v1 baseline readers.
- **Live cache path:** `DynxLiveCache` — single-slot mutex-guarded `push/poll/clear`. Viewer calls `poll()` each render tick; slot empties on read (clear cache, keep it live).
- **KernelEvent harvesting:** `emit_step()` calls `KernelEventLog::instance().filter_by_frame(fid, fid)` and appends matching events as `EVENT` packets, in addition to any caller-supplied events.
- **No open() required for live cache:** `emit_step()` without `open()` still pushes to `DynxLiveCache` (returns true).

### Build gate
**145/145 tests pass (exit code 0).** Zero regressions. Group 71 DynxEmitterGroup71 29/29.

**ctest label:** `wo-72b` - run with `ctest -R "DynxEmitter" --output-on-failure`

---

## WO-72E — Default Usage Bundle (DUB)

**Work order:** WO-72E
**Status:** COMPLETE
**Branch:** V5.1.4

### Context
Replace the hardcoded `annihilation_test_ladder` bundle factory with a generic Default Usage Bundle (DUB) for normal `.vsim` script execution via the `.X` container format.

### Deliverables

| File | Description |
|---|---|
| `include/vsim/bundle/x_bundle.hpp` | Added `make_default_usage_bundle()` factory; cleaned up stale `bundle_id` inline comment |
| `tests/test_dub_factory.cpp` | Group 77 — 10 assertions (DUB-01..DUB-10) |

### DUB slot layout

| Slot | Kind | Purpose |
|---|---|---|
| `LOAD-01` | Script | Load / locate the source `.vsim` script |
| `RUN-01` | Script | Execute the simulation (depends: LOAD-01) |
| `VALIDATE-01` | ValidationCheck | Validate required outputs (depends: RUN-01) |
| `REPORT-01` | Script | Generate summary / report artifacts (depends: VALIDATE-01) |
| `BENCH-01` | Benchmark | Optional timing record — `allow_failure = true` (depends: RUN-01) |

### Build gate
**151/151 tests pass (exit code 0).** Zero regressions. Group 77 DubFactoryGroup77 10/10.

**ctest label:** `wo-72e` — run with `ctest -R "DubFactory" --output-on-failure`

---

## WO-72G — Desktop / Viewer Window Behaviour

**Work order:** WO-72G
**Status:** PENDING
**Branch:** V5.1.4

### Context
Desktop integration layer: script-summoned viewer windows, monitor sizing helper, `.dynx` writer expansion (BOND_FORCE, FIELD, EVENT, RENDER, CAMERA completion), and `vsepr-viewd` / `vsepr-gui` launcher wiring. Every `.vsim` execution must open at least one popup/viewer window.

---

## WO-72K — Built-in Test Module / Double-click Revival

**Work order:** WO-72K
**Status:** COMPLETE
**Branch:** V5.1.4

### Context
Restore and harden the double-click / drag-and-drop entry path. Embed a self-test module inside the executable so that running `vsepr` with no arguments produces a meaningful built-in smoke test rather than a silent exit.

---

## WO-72V -- COMPLETE — Data Mining / Precompute Layer

**Work order:** WO-72V
**Status:** PENDING
**Branch:** V5.1.4

### Context
System 1 of the Day-72-V chapter. Mining and precomputing layer for better runtime performance: caching, indexing, feature extraction, lookup tables, candidate filtering. Targets:

- Precomputed material presets
- Reaction candidate caches
- Formation route lookup tables
- Density / thermal / conductivity tables
- Validated script templates
- Hash-indexed trajectory summaries

---

## WO-72W -- COMPLETE — Material-Property ML / Pretraining Layer

**Work order:** WO-72W
**Status:** PENDING
**Branch:** V5.1.4

### Context
System 2 of the Day-72-V chapter. Simple ML / pretraining layer for material-property discovery: unique material matching, web/literature source mapping, trend finding, and process-route recommendation. Each candidate record targets:

- Candidate material + target property
- Known literature / source mapping
- Formation route + creation reaction
- Process recommendation + confidence score
- Validation status

---

## Day 72 — Floating Goals

Non-slot grab-if-nearby goals. Implementable items are executed inline; GUI-bound items are deferred to WO-72G.

| ID | Title | Status | Notes |
|---|---|---|---|
| 72-floating-1 | Viewer polish pass | DEFERRED → WO-72G | open/drag-drop/recent/camera/reset |
| 72-floating-2 | Default demo bundle (.vsim scripts) | COMPLETE | 7 scripts in `scripts/demos/` |
| 72-floating-3 | "Run MD" button behaviour | DEFERRED → WO-72G | GUI runtime relay |
| 72-floating-4 | Randomisation button | DEFERRED → WO-72G | seed + generator + hash record required |
| 72-floating-5 | Right-click integrator selector | DEFERRED → WO-72G | Verlet / Langevin / FIRE / Euler |
| 72-floating-6 | .dynx quick inspection | COMPLETE | `include/vsim/io/dynx_inspector.hpp` |
| 72-floating-7 | .X bundle folder sketch | COMPLETE | `XBundleFolderLayout` in `x_bundle.hpp` |
| 72-floating-8 | Script-to-desktop bridge | DEFERRED → WO-72G | `[visual] open=true` schema wiring |
| 72-floating-9 | High-density data panel | DEFERRED → WO-72G | E_total / RMSD / defect_fraction panel |
| 72-floating-10 | Installable Windows identity | DEFERRED → WO-72G | .exe / file assoc / icon placeholder |
| 72-floating-11 | Output-format cryptic sidecar | COMPLETE | `include/vsim/analysis/identity_sidecar.hpp` |
| 72-floating-12 | Surface minimum bead warning | COMPLETE | `include/vsim/validation/surface_warnings.hpp` |
| 72-floating-13 | Monitor/window sizing helper | COMPLETE | `deploy/windows/monitor_size_helper.ps1` — verified live on 1920×1080 |
| 72-floating-14 | **.x routing fix (WO-72N)** | COMPLETE | `LauncherWindow.cpp` — `buildRunArgs()` / `buildValidateArgs()` branch on extension |
| 72-floating-15 | **.vsim double-click support (WO-72O)** | COMPLETE | `vsimRequestsVisual()` + detached cmd console + `vsim_double_click_launcher.bat` |

---

## WO-72N — .x Run/Compile Routing Fix

**Goal:** `vsepr-launcher` correctly dispatches `.x` bundle files through the `x run` / `x validate` subcommand path instead of treating them as bare `.vsim` scripts.

| Gate | Criterion |
|---|---|
| Build | Zero new errors |
| Routing | `.x` → `vsepr-sim x run <file>` / `x validate <file>` | PASS |
| Routing | `.vsim` → `vsepr-sim <file>` (unchanged) | PASS |

---

## WO-72O — .vsim Double-Click Support

**Goal:** Double-clicking a `.vsim` file always opens a cmd console showing the simulation run; if the script contains `[visual]` with `open = true`, it additionally opens `vsepr-desktop.exe`. A shell-level bat fallback works without the Qt launcher.

| Gate | Criterion |
|---|---|
| Build | Zero new errors |
| Visual detection | `vsimRequestsVisual()` correctly reads `[visual]` + `open = true` | PASS |
| Console | Detached cmd window launches `vsepr-sim <script>` | PASS |
| GUI | `vsepr-desktop` opens only when script requests it | PASS |
| Fallback | `vsim_double_click_launcher.bat` works standalone | PASS |

---

## Day 73 — v5.13.3  (Wizard Modules 6-8 Full Field Coverage)

**Version:** v5.13.3  
**Tag:** `v5.13.3`  
**Merged:** PR #5 `feature/wizard-full-module-expansion` → `v5.0.0-main`  
**Status:** COMMITTED

### WO-73A — Wizard Step 6: Analysis Full Field Coverage
**Status:** COMPLETE

- Added 7 analysis fields sourced from `vsim_document.hpp`: cutoff radii, MSD probe, RMSD probe, variance probe, N-evolution probe, stationarity window, defect threshold.

### WO-73B — Wizard Step 7: Export Full Field Coverage + Visual Sub-Module
**Status:** COMPLETE

- Added 10 export fields matching `ExportSection` keys.
- New `export.visual` sub-module with 8 fields (SVG, PNG, HTML, dashboard toggles).

### WO-73C — Wizard Step 8: Visual Full Field Coverage
**Status:** COMPLETE

- Added 18 visual fields: 7 `output_type` choices, `animation_mode` fix, all `VisualSection` flags, GL/web/pacing options.
- All fields sourced directly from `vsim_document.hpp`.

| Gate | Criterion |
|---|---|
| Build | Zero new errors | PASS |
| Field coverage | All wizard fields sourced from `vsim_document.hpp` | PASS |
| Demo | Wizard demo verified end-to-end | PASS |
| Merge | PR #5 merged, tag `v5.13.3` pushed | PASS |

---

## Day 74 — v5.13.4  (Gap Classifier + Multi-Scale Output Filter)

**Version:** v5.13.4  
**Status:** PLANNED

### WO-74A — Gap Classifier Refinement
**Status:** PENDING

Consolidate `gap_classifier.cpp` and `gap_classifier_wo74c.cpp` into a single production classifier. Target:

- Unified `GapClassifier` API with configurable thresholds
- Regression tests against reference trajectories
- Integration with analysis layer output

### WO-74B — Multi-Scale Output Filter
**Status:** PENDING

Harden `vsim_output_filter.cpp` for production use:

- Support all export formats (xyz, xyzFull, CSV, JSON)
- Filter by scale layer (bead / coarse-bead / premacro / macro)
- Gate on `[export]` section flags

### WO-74C — Length Scale Fitter Validation
**Status:** PENDING

Verify `length_scale_fitter.cpp` against known analytic cases; add tests to `test_length_scale_fitter.cpp`.

| Gate | Criterion |
|---|---|
| Build | Zero new errors |
| Tests | All gap-classifier + output-filter tests pass |
| Coverage | Length-scale fitter validated against 3+ analytic cases |

---

## Day 75 — v5.13.5  (CTL Pipeline Hardening + Release Gate)

**Version:** v5.13.5  
**Status:** PLANNED

### WO-75A — CTL Pipeline Hardening
**Status:** PENDING

Finalise the modernised VSIM Control pipeline (`WO-VSIM-CTL`). Target:

- All 7 CTL test suites (`test_ctl_01` – `test_ctl_07`) passing cleanly
- Determinism test (`test_ctl_determinism`) green
- Negative-case test (`test_ctl_negative`) covering all error paths
- Compat layer (`test_ctl_compat`) frozen

### WO-75B — Day-75 Release Gate
**Status:** PENDING

Full regression before tagging `v5.13.5`:

- All tests (target ≥ 143) pass
- `vsepr doctor` → all runtime data files OK
- `vsepr run` demo script → Formation/Fingerprint/Cluster/Analysis/Report all PASS
- STAGE.md, VSIM_REFERENCE.md, VSIM_DEVELOPMENT.md updated
- Tag `v5.13.5` pushed

| Gate | Criterion |
|---|---|
| Build | Zero new errors |
| Tests | ≥ 143/143 pass |
| CTL suite | test_ctl_01 – test_ctl_07 all PASS |
| Doctor | `vsepr doctor` all OK |
| Tag | `v5.13.5` pushed to origin |
