# VSEPR-SIM — Master Stage & Gate Ledger

> Updated: v5.0.14 | Branch: feature/wizard-full-module-expansion | Day 75 (IN PROGRESS)

---

## Beta Milestones

| Version        | Description                                                              | Day | Status      |
|----------------|--------------------------------------------------------------------------|-----|-------------|
| v5.0.0-beta.7  | render_interval / step emission cadence                                   | 57  | COMMITTED   |
| v5.0.0-beta.8  | PBC / cell / boundary / Ewald                                             | 58  | COMMITTED   |
| v5.0.0-beta.9  | Registry resolution engine / CLI layer                                    | 59  | COMMITTED   |
| v5.0.0-beta.10 | Variance / N_evolution / while / batch sweep                              | 60  | COMMITTED   |
| v5.0.0-beta.11 | Macro sampling / empirical verification                                   | 61  | COMMITTED   |
| v5.0.0-beta.12 | Batching upgrade / study orchestration                                    | 62  | COMMITTED   |
| v5.1.13        | Pillar F: empirical chemistry layer, pre-electron freeze                  | 72  | COMMITTED   |
| **v5.13.3**    | Wizard modules 6-8 full field coverage (analysis/export/visual)           | **73** | **COMMITTED** |
| **v5.13.4**    | Gap classifier + multi-scale output filter + chemistry audit (100% tests) | **74** | **COMMITTED** |
| v5.13.5        | IKK enrichment (Part A done) + identity vector + release gate             | 75  | IN PROGRESS |

---

## v5.13.5 Release Gate — WO-75B

Full regression before tagging `v5.13.5`:

- All tests (target ≥ 143; currently 167/167) pass
- `vsepr doctor` → all runtime data files OK
- `vsepr run` demo script → Formation/Fingerprint/Cluster/Analysis/Report all PASS
- `STAGE.md`, `VSIM_REFERENCE.md`, `VSIM_DEVELOPMENT.md` updated
- Tag `v5.13.5` pushed

| Gate                    | Criterion                                       | Status  |
|-------------------------|-------------------------------------------------|---------|
| Build                   | Zero new errors                                 | ✅ PASS |
| Tests                   | ≥ 143/143 pass (167/167 as of Day 74)           | ✅ PASS |
| IKK End-Tag (WO-75A-A)  | Group 87 — 20/20 pass                           | ✅ PASS |
| IKK GL overlay (WO-75A-B)| D-colour overlay pass + ImGui legend            | 🔲 PENDING |
| IKK Python plot (WO-75A-C)| `plot_dist_timeseries()` + PNG embed           | 🔲 PENDING |
| Identity vector (WO-75B) | Group 88 IkkIdentityVectorGroup88 — pass       | ✅ PASS |
| MCF-CAI (WO-76)         | Group 89 McfCaiGroup89 — pass                  | ✅ PASS |
| Doctor                  | `vsepr doctor` all OK                           | 🔲 PENDING |
| Tag                     | `v5.13.5` pushed to origin                      | 🔲 PENDING |

---

## Full Test Group Registry

| Group    | Description                                          | Tests  | Status |
|----------|------------------------------------------------------|--------|--------|
| Group 1  | Core / Geometry / VSEPR (foundational)               | —      | PASS   |
| Group 2  | Atomistic simulation core                            | —      | PASS   |
| Group 3  | Potential / energy                                   | —      | PASS   |
| Group 4  | Optimizer                                            | —      | PASS   |
| Group 5  | Angle tests                                          | —      | PASS   |
| Group 6  | VSEPR correctness                                    | —      | PASS   |
| Group 7  | Torsion                                              | —      | PASS   |
| Group 8  | Torsion analysis                                     | —      | PASS   |
| Group 9  | Torsion validation                                   | —      | PASS   |
| Group 10 | Alkane torsion                                       | —      | PASS   |
| Group 11 | CG suite 1                                           | —      | PASS   |
| Group 12 | CG suite 2                                           | —      | PASS   |
| Group 13 | CG suite 3                                           | —      | PASS   |
| Group 14 | CG suite 4                                           | —      | PASS   |
| Group 15 | CG suite 5                                           | —      | PASS   |
| Group 16 | CG suite 6                                           | —      | PASS   |
| Group 17 | CG suite 7                                           | —      | PASS   |
| Group 18 | CG suite 8                                           | —      | PASS   |
| Group 19 | CG Track2                                            | —      | PASS   |
| Group 20 | Molecular IO                                         | —      | PASS   |
| Group 21 | Geometry ops                                         | —      | PASS   |
| Group 22 | Energy tests                                         | —      | PASS   |
| Group 23 | Pipeline / dashboard record                          | —      | PASS   |
| Group 24 | Formation output                                     | —      | PASS   |
| Group 25 | Defect microstate                                    | —      | PASS   |
| Group 26 | Statistical interpretation                           | —      | PASS   |
| Group 27 | Heat-gated reaction control                          | —      | PASS   |
| Group 28 | WO-VSIM-57D render_interval                          | —      | PASS   |
| Group 29 | WO-VSEPR-SIM-57B Cell / PBC / Ewald                  | —      | PASS   |
| Group 30 | WO-VSIM-02A VSIM parser                              | —      | PASS   |
| Group 31 | WO-VSIM-02B VSIM parser stress                       | —      | PASS   |
| Group 32 | WO-VSIM-02C VSIM visual / external                   | —      | PASS   |
| Group 33 | WO-VSIM-02D Variance / N_evolution / while           | —      | PASS   |
| Group 34 | WO-VSIM-02E Batch sweep                              | —      | PASS   |
| Group 35 | WO-VSIM-03A Intent scripting layer                   | —      | PASS   |
| Group 36 | Ambient chemistry integration                        | —      | PASS   |
| Group 37 | WO-VSIM-04A Isomer detection revival                 | —      | PASS   |
| Group 37 | WO-VSIM-03B Stress / failure-mapping                 | —      | PASS   |
| Group 38 | WO-VSIM-03C Registry resolution engine               | 18/18  | PASS   |
| Group 39 | Demo script suite / WO-VSEPR-SIM-62A Empirical verif | 19/19  | PASS   |
| Group 40 | WO-VSEPR-SIM-62B Batch verification aggregation      | 20/20  | PASS   |
| Group 41 | WO-VSIM-62C Batch parser                             | 18/18  | PASS   |
| Group 42 | WO-VSIM-62C Batch expander                           | 10/10  | PASS   |
| Group 43 | WO-VSIM-62C Batch runner static                      | 12/12  | PASS   |
| Group 44 | EigenMine small tests (EIG-1..6)                     | 6/6    | PASS   |
| Group 45 | CurveFit presolve tests (EIG-7..8)                   | 2/2    | PASS   |
| Group 46 | Release gate tests (EIG-9..10)                       | 2/2    | PASS   |
| Group 47 | Basis archive tests                                  | —      | PASS   |
| Group 48 | .xyza dynamic state gate (WO-VSIM-66A)               | —      | PASS   |
| Group 49 | Bridge-65 atom event records + logger routing        | —      | PASS   |
| Group 50 | Bridge-65 high-rate event limiter/sampler            | —      | PASS   |
| Group 51 | Bridge-65 matrix force agreement validation          | —      | PASS   |
| Group 52 | Bridge-65 replay hash independence                   | —      | PASS   |
| Group 53 | WO-67-B Viewer runtime bridge + XYZ data contract    | —      | PASS   |
| Group 54 | Intent runtime bridge basic                          | —      | PASS   |
| Group 55 | Formation FieldRamp                                  | —      | PASS   |
| Group 56 | Isomer pipeline wiring A                             | —      | PASS   |
| Group 57 | Dynx v1 session archive                              | —      | PASS   |
| Group 58 | CTL-01: Typed command namespace registry             | —      | PASS   |
| Group 59 | CTL-02: Script parser to execution graph             | —      | PASS   |
| Group 60 | CTL-03: Runtime wrapper dispatch + validator         | —      | PASS   |
| Group 61 | CTL-04: Kernel channel command bindings              | 7/7    | PASS   |
| Group 62 | CTL-05: Artifact command bindings (XBIT/Dynx)        | 9/9    | PASS   |
| Group 63 | CTL-06: Metrics/gate assertion system                | 10/10  | PASS   |
| Group 64 | CTL-07: Deterministic plan hash + artifact manifest  | 10/10  | PASS   |
| Group 65 | CTL End-to-End Smoke                                 | 10/10  | PASS   |
| Group 66 | CTL Runtime Hook Integration                         | 10/10  | PASS   |
| Group 67 | CTL Negative Validation                              | 10/10  | PASS   |
| Group 68 | CTL Determinism                                      | 10/10  | PASS   |
| Group 69 | CTL Existing Workflow Compatibility                  | 10/10  | PASS   |
| Group 70 | XFramework audit (WO-XFRAMEWORK-01)                  | —      | PASS   |
| Group 71 | DynxEmitter smoke — live cache + archive (WO-72B)    | 29/29  | PASS   |
| Group 72 | Dynx streaming write / read-back / session (WO-72D)  | —      | PASS   |
| Group 73 | ExportDemoSection (WO-72C demo pipeline)             | 25/25  | PASS   |
| Group 74 | DemoFrameSampler                                     | 21/21  | PASS   |
| Group 75 | DemoBundleWriter                                     | 19/19  | PASS   |
| Group 76 | DemoPipelineIntegration                              | —      | PASS   |
| Group 77 | DubFactory (WO-72E default usage bundle)             | 10/10  | PASS   |
| Group 78 | LengthScaleFitter (WO-73D / WO-74C)                  | —      | PASS   |
| Group 79 | VsimOutputFilter (WO-73E / WO-74B)                   | —      | PASS   |
| Group 80 | GapClassifier — threshold + adaptive (WO-74A / WO-74C) | 25/25 | PASS  |
| Group 81 | ObserveMetrics (WO-75A instrumentation)              | —      | PASS   |
| Group 82 | FormationHelper                                      | —      | PASS   |
| Group 83 | BioObject                                            | —      | PASS   |
| Group 84 | Replay (WO-NL0C)                                     | —      | PASS   |
| Group 85 | Discovery — Type 1 continuous random-materials (WO-NL0A) | —  | PASS   |
| Group 86 | CrystalMD — Type 2 deterministic MD/crystal (WO-NL0B)| —      | PASS   |
| Group 87 | IkkEndTag — IKK end-tag enrichment (WO-75A Part A)   | 20/20  | PASS   |
| Group 88 | IkkIdentityVector — I-vector series (WO-75B Phase 1) | —      | PASS   |
| Group 89 | McfCai — MCF-CAI state vector (WO-76 Steps 3+5)      | —      | PASS   |
| —        | ChemistryUniversalV2 — element DB + organic + coordination | —  | PASS   |
| —        | Phase2ComplexMolecules — coordination + hypervalent  | —      | PASS   |
| —        | IsomerTest — geometric isomers, CIP chirality        | —      | PASS   |

**Total: 167/167 CTest targets passing (100%) — as of Day 74 chemistry audit**

---

## Environment Notes

- Branch: `feature/wizard-full-module-expansion`
- Version: v5.0.14
- Build: CMakePresets.json → Ninja / GCC 15.2 UCRT64 / C++23 / `build/`
- Compiler: `C:/msys64/ucrt64/bin/g++.exe`
- Build preset: `cmake --preset release`
- Test runner: `ctest --test-dir build -j4`
- Git: GitHub CLI exclusively — `C:\Program Files\GitHub CLI\gh.exe`
- Shell: PowerShell; use `;` not `&&`

---

## Known Deferred Items

| Item                              | Deferred To | Note |
|-----------------------------------|-------------|------|
| IKK GL D-overlay pass             | v5.13.5     | `RENDER_PASS_DIST` + ImGui legend (WO-75A Part B) |
| IKK Python time-series plot       | v5.13.5     | `plot_dist_timeseries()` PNG embed (WO-75A Part C) |
| Stochastic axis runtime           | v5.2.0      | `BatchAxisEntry.kind = "stochastic"` parsed, not wired |
| Formation axis execution          | v5.2.0      | `BatchAxisEntry.kind = "formation"` parsed, not wired |
| latin_hypercube / random design   | v5.1.0      | `BatchDesignSection.type` parsed, not expanded |
| DEMBridge/FEABridge execution     | v5.2.0      | Schema frozen (WO-66Q); runtime dispatch deferred (MF-G04/G05) |
| OrganicDiagnostics per-frame eval | v5.2.0      | Section parsed (WO-66O); runtime wired but flush deferred |
| Crystal translation targets       | v5.1.5      | Flags parsed (WO-66P); CIF/JSON emitters deferred |
| write_cluster_json / fingerprint_json / summary_csv | v5.13.5 | Writers declared, not implemented (MF-A01/A02/A03) |
| Sweep dispatcher                  | v5.13.5     | Parsed; runtime wiring pending (MF-B01) |
| Isomer generator + walker + report| v5.2.0      | Detection done (MF-C03 ✅); generator/walker/report deferred |
| MCF-CAI kernel fork (WO-77)       | post-v5.13.5 | Depends on WO-75A completion |
| STAGE.md                          | ✅ RESTORED  | Deleted; restored Day 74–75 (MF-F01 COMPLETE) |

---

## Day 66 — WO-66N / WO-66O / WO-66P / WO-66Q  (v5.1.4)

**Status:** COMPLETE

- **WO-66N** Constructor Objects + Batching: `ObjectPath`, `ConstructorObjectRegistry`, `BatchGroup`, `VsimDocument::objects`
- **WO-66O** Organic/Peptide Diagnostics: `OrganicScaleSection`, `OrganicDiagnosticResult`, parser wired
- **WO-66P** Crystal/PBC Constructor: `CrystalConstructorSection`, `[objects.crystal]` / `[cell]` / `[pbc]` dispatch
- **WO-66Q** Non-Molecular Objects + XBIT: `DEMBridgeObject`, `FEABridgeObject`, `NonMolecularObjectStore`

WO docs: `docs/wo/WO-66N-Constructor-Objects.md`, `WO-66O`, `WO-66P`, `WO-66Q`

---

## Day 67 — WO-67-A / WO-67-A2 / WO-67-B  (v5.1.4)

**Status:** COMPLETE

- **WO-67-A** Lightweight viewer: persistent viewer daemon, NDJSON transport, `viewer_transport.hpp`
- **WO-67-A2** Persistent viewer daemon protocol: `viewer_transport_ndjson.cpp`, session lifecycle
- **WO-67-B** Viewer Runtime Bridge + XYZ Data Contract: `ViewRuntimeBridge`, `ViewSession`, `ViewerPacket` (Group 53)

WO docs: `docs/wo/WO-67-A-lightweight-viewer.md`, `WO-67-A2-persistent-viewer-daemon.md`

---

## Day 70 — X-Framework Audit (WO-XFRAMEWORK-01)

**Status:** COMPLETE

- Consolidated `.X` / `.vsim` / batch pipeline to single audit inlet
- 29/29 audit checks pass (Group 70 XFrameworkAudit)
- Installer alias registered

WO doc: `docs/wo/WO-XFRAMEWORK-01-architecture.md`

---

## Day 72 — WO-72A / WO-72B / WO-72D / WO-72E  (v5.1.13)

**Status:** COMPLETE

| WO     | Description                             | Group | Tests        |
|--------|-----------------------------------------|-------|--------------|
| WO-72A | `.X` bundle format                      | 70    | 10/10 PASS   |
| WO-72B | Dynx pipeline emitter                   | 71    | 29/29 PASS   |
| WO-72D | Dynx streaming write/read/session       | 72    | — PASS       |
| WO-72E | Default Usage Bundle (DUB) factory      | 77    | 10/10 PASS   |

Full build: 151/151 tests pass at end of Day 72.

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

| Gate          | Criterion                                         | Status  |
|---------------|---------------------------------------------------|---------|
| Build         | Zero new errors                                   | ✅ PASS |
| Field coverage| All wizard fields sourced from `vsim_document.hpp`| ✅ PASS |
| Demo          | Wizard demo verified end-to-end                   | ✅ PASS |
| Merge         | PR #5 merged, tag `v5.13.3` pushed                | ✅ PASS |

---

## Day 74 — v5.13.4  (Gap Classifier + Chemistry Audit + 100% Tests)

**Version:** v5.13.4
**Branch:** `feature/wizard-full-module-expansion`
**Commit:** `489d11cd`
**Status:** COMMITTED

### WO-74A — Gap Classifier Consolidation
**Status:** COMPLETE

- Merged `gap_classifier_wo74c.cpp` into `gap_classifier.cpp` (stub left in place).
- Unified `IGapClassifier` interface + `GapInput` struct in `include/vsim/analysis/i_gap_classifier.hpp`.
- Two self-registering strategies: `"threshold"` (3-label) and `"adaptive"` (14-label).
- `GapThresholds` configurable (small/moderate fractions).
- Group 80 `GapClassifierGroup80`: 25/25 assertions PASS.

### WO-74B — Multi-Scale Output Filter Hardening
**Status:** COMPLETE

- `src/analysis/vsim_output_filter.cpp` hardened for 4 export formats (xyz, xyzFull, CSV, JSON).
- Scale-layer gating: bead / coarse-bead / premacro / macro.
- Group 79 `VsimOutputFilterGroup79`: PASS.

### WO-74C — Length Scale Fitter Validation
**Status:** COMPLETE

- `src/analysis/length_scale_fitter.cpp` validated against 3+ analytic cases.
- Group 78 `LengthScaleFitterGroup78`: PASS.

### Chemistry Audit (Day 74 close — commit 489d11cd)
**Status:** COMPLETE

- Fixed `Molecule::add_atom()` mass lookup via `chemistry_db().get_mass(Z)`.
- Fixed `ReactionEngine` element lookup from `State::type` (`engine.cpp`).
- Wired `evaluate_organic_diagnostics` + `flush_organic_diagnostics` (`vsim_runtime.hpp`).
- Re-enabled `ChemistryUniversalV2`, `Phase2ComplexMolecules`, `IsomerTest` CTest targets.
- Added `init_chemistry_db()` to 14 pre-existing tests that lacked DB initialisation.
- Promoted **MF-C03** (CIP chirality detection) → COMPLETE in `WO-MF-01`.

| Gate          | Criterion                             | Status  |
|---------------|---------------------------------------|---------|
| Build         | Zero new errors                       | ✅ PASS |
| Tests         | 167/167 CTest targets (100%)          | ✅ PASS |
| Gap classifier| Group 80 — 25/25 PASS                 | ✅ PASS |
| Output filter | Group 79 — PASS                       | ✅ PASS |
| Fitter        | Group 78 — PASS                       | ✅ PASS |
| Chemistry     | ChemistryUniversalV2 / Phase2 / Isomer all PASS | ✅ PASS |

---

## Day 75 — v5.13.5  (IKK Enrichment + Identity Vector + Release Gate)

**Version:** v5.13.5
**Status:** IN PROGRESS

### WO-75A Part A — IKK Report End-Tag Scripting Layer
**Status:** ✅ COMPLETE

- `VsimIkkEndTagSection` struct in `include/vsim/vsim_document.hpp`
- `apply_ikk_end_tag_key()` parser wiring in `src/vsim/vsim_parser.cpp`
- `IKKEndTag` struct + `build_ikk_end_tag()` + `render_ikk_end_tag_md/tex()` in `include/vsim/analysis/ikk_end_tag.hpp` + `.cpp`
- `IkkEndTagModule` self-registering analysis module
- `AnalysisRecord::ikk_end_tag_md/tex` output fields in `include/vsim/analysis/i_analysis_module.hpp`
- Group 87 `IkkEndTagGroup87`: **20/20 PASS**
- `VSIM_REFERENCE.md` and `docs/VSIM_LANGUAGE.md` updated

### WO-75A Part B — IKK GL D-Colour Overlay
**Status:** 🔲 PENDING

`RENDER_PASS_DIST` overlay pass + ImGui legend panel + scale ladder bar.
See `docs/wo/WO-75A-IKK-Report-EndTag-Enrichment.md §B`.

### WO-75A Part C — D Time-Series Plot (Python)
**Status:** ✅ COMPLETE (MF-F01+WO-75A-C delivery)

`plot_dist_timeseries()` added to `reporting/generate_report.py`:
- Reads `.identity.json` sidecar per run
- Dual-axis matplotlib figure: D_rec (blue left) / entropy proxy (red right)
- Amber band for identity-loss zone (Δ-D < 0); reference line at D = 0.5
- Saved as `out/<run_id>/dist_timeseries.png`; embedded in consolidated report
- Caption auto-generated from IKK identity-vector run summary

### WO-75B — Identity Vector + Phase 1 I-vector series
**Status:** ✅ COMPLETE

- `IKKIdentityVector`, `IKKIdentitySeries`, `IKKIdentityFrameRecord` in `include/vsim/analysis/ikk_identity_vector.hpp`
- `from_sidecar_record()`: Phase 1 proxy mapping (x=existence, y=EM, z=spatial, t=temporal, w=internal)
- `build_ivec_series()` + `write_identity_json()` in `src/vsim/analysis/ikk_identity_vector.cpp`
- Group 88 `IkkIdentityVectorGroup88`: PASS

### WO-76 — MCF-CAI State Vector Integration
**Status:** ✅ COMPLETE

- `McfCaiStateVector`, MCF-CAI I-vector fields in `include/vsim/kernel_mcf/`
- Group 89 `McfCaiGroup89`: PASS

### WO-75B — v5.13.5 Release Gate
**Status:** 🔲 PENDING

| Gate              | Criterion                                      | Status  |
|-------------------|------------------------------------------------|---------|
| Build             | Zero new errors                                | ✅ PASS |
| Tests             | ≥ 143 pass (currently 167/167)                 | ✅ PASS |
| IKK end-tag       | Group 87 — 20/20 PASS                          | ✅ PASS |
| IKK GL overlay    | WO-75A Part B implemented                      | 🔲 PENDING |
| Dist timeseries   | `plot_dist_timeseries()` in generate_report.py | ✅ DONE |
| Doctor            | `vsepr doctor` all OK                          | 🔲 PENDING |
| STAGE.md          | Current (this file)                            | ✅ DONE |
| VSIM_REFERENCE.md | Up to date                                     | ✅ PASS |
| Tag               | `v5.13.5` pushed to origin                     | 🔲 PENDING |

---

*Ledger maintained per `VSIM_DEVELOPMENT.md` §4C. Update with every WO completion.*
*Last compiled: 2026-06-24 | Commit: 489d11cd | 167/167 tests passing*
