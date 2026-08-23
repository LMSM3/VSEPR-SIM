# VSEPR-SIM — Master Stage & Gate Ledger

> Updated: v5.16.0 | Branch: day84t-chemplus-declarative-vsepr | Day 94 (VSIM 5.16 architectural cut active)

---

## 1. Executive Summary

VSEPR-SIM has completed the **revival/plumbing arc** (v5.15.0) and is now in
the **v5.16 integrated-runtime architectural cut**, followed by a language-level
rewrite (v6.0.0) and a new physics kernel (v6.1.0 DFAE).

The synthetic/formation path currently reports many warnings and only partial
convergence because the real potential/integrator is not yet wired to the
CLI `vsepr run` path. This is intentional progress: the error and warning
libraries are now being exercised rather than bypassed.

> **Scientific development note:** physics/math errors are progress signals.
> They demonstrate that error libraries are exercising the model rather than
> masking it, moving work from plumbing toward real mathematics and physics
> refinement.

---

## 2. Long-term Release Arc (v5.16.0 → v6.1.0)

| Version | Theme | Major deliverables | Exit gate |
|---|---|---|---|
| **v5.15.0** | VSIM parser/runtime parity | `[visual]` status loop (WO-93A), ChemPlus classify bridge, dense-record runtime, stable build. | ✅ COMPLETE |
| **v5.16.0** | Integrated live runtime | `RuntimeSession`/observer architecture; `[export.live]`; bounded compact render frames; gas-injection `.xyzf` restoration; stable runtime identities; final v5 compatibility baseline. | Live-on/live-off scientific hashes match and all v5 demo scripts reproduce v5.13.4 artefacts. |
| **v6.0.0** | Intent-first language rewrite | Intent-first `.vsim` surface; typed intermediate representation; v5-to-v6 translation shim; shared runtime/export architecture; existing-physics parity (all v5 results reproducible). | v5 golden tests pass unmodified through v6 translation. |
| **v6.1.0** | DFAE / electron lifecycle | DFAE runtime; electron lifecycle tracking; field coupling; comparative model benchmarks; live electron visualization. | DFAE benchmarks match or exceed v5 empirical fits. |

### 2.1 Arc philosophy

* **v5.16.0 is the last pure-v5 release.** After it ships, no new v5-only
  features are added; only bug fixes and translation regressions are allowed.
* **v6.0.0 is a language/runtime rewrite, not a physics rewrite.** Existing
  physics models are preserved behind a v5 translation shim so validation
  datasets remain authoritative.
* **v6.1.0 introduces the first new physics since the freeze.** DFAE and
  electron lifecycle are developed against v6.0.0 benchmarks to prove they
  improve on v5 baselines.

### 2.2 Work-order map per release

#### v5.16.0 — Expected WOs

| WO | Deliverable | Status |
|---|---|---|
| WO-94A | Gas-injection `.xyzf` writer restored and verified | complete |
| WO-94B | RuntimeSession + ObserverHub + `[export.live]` foundation | active |
| WO-95A | Export profiles (`[export.profile]`) | planned |
| WO-95B | Stable runtime identities (run_label, lineage UUIDs) | planned |
| WO-96A | Final v5 compatibility baseline and golden lock | planned |

#### v6.0.0 — Expected WOs

| WO | Deliverable | Status |
|---|---|---|
| WO-97A | Intent-first grammar design document | planned |
| WO-97B | Typed IR (`vsepr::ir::Module`) | planned |
| WO-98A | v5-to-v6 translator (`vsepr translate`) | planned |
| WO-98B | Shared runtime and export architecture (WO-98 build) | planned |
| WO-99A | Existing-physics parity gate | planned |
| WO-99B | v6 documentation and migration guide | planned |

#### v6.1.0 — Expected WOs

| WO | Deliverable | Status |
|---|---|---|
| WO-100A | DFAE runtime scaffold | planned |
| WO-100B | Electron lifecycle model | planned |
| WO-101A | Field coupling (EM ↔ atomic) | planned |
| WO-101B | Comparative model benchmarks | planned |
| WO-102A | Live electron visualization | planned |

### 2.3 Dependency graph

```
v5.15.0 ──┬──> WO-94A (gas restore) ──┬──> v5.16.0 compatibility freeze
          │                           │
          └──> WO-93A (status loop) ──┘      │
                                             ▼
                        v5.16.0 ───> WO-97A (intent grammar)
                                              │
                                              ▼
                        WO-97B (typed IR) <──┘
                          │
                          ├──> WO-98A (v5 translator)
                          │         │
                          │         ▼
                          │   WO-98B (shared runtime)
                          │         │
                          │         ▼
                          └──> v6.0.0 parity release
                                     │
                                     ▼
                        v6.0.0 ───> WO-100A (DFAE scaffold)
                                             │
                                             ▼
                        WO-100B (electron lifecycle) ──┬──> WO-101A (field coupling)
                                                              │
                                                              ▼
                        WO-101B (benchmarks) <───────────────┘
                          │
                          ▼
                        v6.1.0 DFAE release
```

---

## 3. Risk Register

| Risk | Impact | Mitigation | Owner |
|---|---|---|---|
| v5.16.0 gas restoration uncovers deeper trajectory-layer rot | High | Keep scope narrow; restore only the path used by `argon_gas_expanded_demo.vsim`; add regression test. | WO-94A |
| v6.0.0 translation shim drifts from v5 semantics | High | Run v5 golden artefacts through translator and diff outputs; fail release if diff > tolerance. | WO-98A |
| DFAE runtime in v6.1.0 cannot reproduce v5 empirical fits | High | Maintain v6.0.0 legacy-physics mode as fallback; benchmarks must beat, not just match. | WO-101B |
| CUDA/toolchain still unavailable for visual/CUDA features | Medium | Keep CUDA optional; visual features degrade to headless/terminal paths. | Infrastructure |
| Documentation falls behind schema/runtime changes | Medium | Per-action evidence docs required by `VSIM_DEVELOPMENT.md` since WO-93A. | Every WO |

---

## 4. Current Contamination / Warning Signature

Recent `vsepr run` outputs for `argon_gas_expanded_demo` show:

* `n_cases = 500`
* `n_clusters = 45`
* `n_warnings = 1470`
* `Converged: 30 / 500`

**Interpretation:** The CLI synthetic path is emitting formation events from
parsed molecule counts, but the integrator/optimizer is not yet producing
physically converged states. This is acceptable for v5.15.0 because the
pipeline is being exercised end-to-end; it is **not** acceptable for v5.16.0,
which must either restore the real gas MD path or clearly mark these outputs
as synthetic/debug with a new `[run] mode`.

---

## Day ~99 Beta7-8 carry-forward acceptance

[W] WO-99A-GR-GOLDEN — release observability, viewer fixture, and execution evidence
[D] Day ~99 — Beta7-8 legacy carry-forward
[R] GOLDEN — evidence is measured against this work order's criteria only
[T] Make build/test completion, NaCl viewer startup, demos, EHD, CLI, replay, and repeatability observable
[B] Build summary previously showed CUDA OFF and demos OFF; default NaCl resolution and focused CTest evidence were not visible
[I] Release now enables headless demos/viewer demos; `data/fixtures/nacl.xyz` is canonical; viewer resolves build/install fixture paths; CLI smoke tests are registered
[V] Full release build completed; CTest: 197/197 passed; headless viewer demo: 5/5 passed; `test_ehd`, `test_view_67b`, `vsepr`, `vsepr-view-demo-01`, and `vsepr-view-bench` built
[P] Evidence and two-page specification: `docs/day99/DAY99_A_ACCEPTANCE.tex`; commands are published in README
[N] Validate a PNG 3D export in an environment with visual dependencies; CUDA remains blocked until a CUDA compiler is configured

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
| v5.13.5        | IKK enrichment (Part A done) + identity vector + release gate             | 75  | COMMITTED   |
| **v5.13 FREEZE** | Branch frozen at Day 82. CI deferred (MF-F02 deprecated). WO-75A-B deferred to next arc. | **82** | **FROZEN** |
| v5.15.0        | VSIM parser/runtime parity + terminal HUD QoL (WO-93A)                    | 93  | COMMITTED   |
| **v5.16.0**    | Gas restoration; live refresh loop; export profiles; stable runtime identities; final v5 compatibility baseline | 94–96 | PLANNED     |
| **v6.0.0**     | Intent-first language; typed IR; v5 translation; shared runtime/export architecture; existing-physics parity | 97–100 | PLANNED     |
| **v6.1.0**     | DFAE runtime; electron lifecycle; field coupling; comparative benchmarks; live electron visualization | 101+ | PLANNED     |

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

- Branch: `day84t-chemplus-declarative-vsepr`
- Version: v5.14.1
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
**Status:** ✅ COMMITTED — Day 75 deliverables COMPLETE (Day 82 finalisation)

### WO-75A Part A — IKK Report End-Tag Scripting Layer
**Status:** ✅ COMPLETE

- `VsimIkkEndTagSection` struct in `include/vsim/vsim_document.hpp`
- `apply_ikk_end_tag_key()` parser wiring in `src/vsim/vsim_parser.cpp`
- `IKKEndTag` struct + `build_ikk_end_tag()` + `render_ikk_end_tag_md/tex()` in `include/vsim/analysis/ikk_end_tag.hpp` + `.cpp`
- `IkkEndTagModule` self-registering analysis module
- `AnalysisRecord::ikk_end_tag_md/tex` output fields in `include/vsim/analysis/i_analysis_module.hpp`
- `AnalysisRecord::ikk_sidecar` (`IdentitySidecarSeries`) sidecar attachment point — **completed Day 82**
- `IkkEndTagModule::run()` upgraded to use `rec.ikk_sidecar` (no longer stub) — **completed Day 82**
- `write_ikk_end_tag()` free function in `ikk_end_tag.hpp` / `.cpp` (Deliverable 1) — **completed Day 82**
- IKK macro set in `reporting/report.tex` preamble (Deliverable D) — **completed Day 82**
- Group 87 `IkkEndTagGroup87`: **20/20 PASS**
- `VSIM_REFERENCE.md` and `docs/VSIM_LANGUAGE.md` updated

### WO-75A Part B — IKK GL D-Colour Overlay
**Status:** 🔲 DEFERRED (next arc)

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

### WO-75A Part D — LaTeX Macro Set
**Status:** ✅ COMPLETE — **completed Day 82**

- `\Dfrak`, `\Ifrak`, `\etaab`, `\Psihid`, `\Sn{}` symbols
- `\ikkendsection{badge}{kvs}` report macro
- `\Dpass{}`, `\Dwarn{}`, `\Dfail{}` badge coloring
- Added to `reporting/report.tex` preamble

### WO-75A Deliverable 9 — Layering Report eta_ab Column
**Status:** ✅ COMPLETE — **completed Day 82**

- `discover_ikk_summaries()` added to `reporting/generate_layering_report.py`
- `\IKKSummaryTable` LaTeX command written to `reporting/layering_data.tex`
- Shows D_rec, η_ab, |Ī|, frame_count per run from `.identity.json` sidecars

### WO-75B — Identity Vector + Phase 1 I-vector series
**Status:** ✅ COMPLETE

- `IKKIdentityVector`, `IKKIdentitySeries`, `IKKIdentityFrameRecord` in `include/vsim/analysis/ikk_identity_vector.hpp`
- `from_sidecar_record()`: Phase 1 proxy mapping (x=existence, y=EM, z=spatial, t=temporal, w=internal)
- `build_ivec_series()` + `write_identity_json()` in `src/vsim/analysis/ikk_identity_vector.cpp`
- `[analysis.ivec]` section added to `docs/VSIM_LANGUAGE.md` — **completed Day 82**
- Group 88 `IkkIdentityVectorGroup88`: **28/28 PASS**

### WO-76 — MCF-CAI State Vector Integration
**Status:** ✅ COMPLETE

- `McfCaiStateVector`, MCF-CAI I-vector fields in `include/vsim/kernel_mcf/`
- Group 89 `McfCaiGroup89`: **24/24 PASS**

### WO-75B — v5.13.5 Release Gate
**Status:** ✅ COMMITTED (v5.13 FROZEN at Day 82)

| Gate              | Criterion                                      | Status  |
|-------------------|------------------------------------------------|---------|
| Build             | Zero new errors                                | ✅ PASS |
| Tests             | ≥ 143 pass (currently 167/167)                 | ✅ PASS |
| IKK end-tag       | Group 87 — 20/20 PASS                          | ✅ PASS |
| IKK identity vec  | Group 88 — 28/28 PASS                          | ✅ PASS |
| MCF-CAI           | Group 89 — 24/24 PASS                          | ✅ PASS |
| report.tex macros | IKK macro set in preamble                      | ✅ PASS |
| eta_ab column     | IKKSummaryTable in layering report             | ✅ PASS |
| [analysis.ivec]   | Documented in VSIM_LANGUAGE.md                 | ✅ PASS |
| ikk_sidecar field | AnalysisRecord attachment point wired          | ✅ PASS |
| IKK GL overlay    | WO-75A Part B implemented                      | ⏸ DEFERRED (next arc) |
| Dist timeseries   | `plot_dist_timeseries()` in generate_report.py | ✅ PASS |
| Doctor            | `vsepr doctor` all OK                          | ⏸ DEFERRED |
| STAGE.md          | Current (this file)                            | ✅ DONE |
| VSIM_REFERENCE.md | Up to date                                     | ✅ PASS |
| Tag               | `v5.13.5` pushed to origin                     | ⏸ DEFERRED |

---

## Day 82 — v5.13 Branch Freeze

**Date:** 2026-06-28
**Branch:** `feature/wizard-full-module-expansion`
**Final version:** `v5.13.5`
**Status:** ❄️ FROZEN

### v5.13 arc delivery summary (Days 57–82)

| Version | Day | Deliverable |
|---------|-----|-------------|
| v5.0.0-beta.7  | 57 | render_interval / step emission cadence |
| v5.0.0-beta.8  | 58 | PBC / cell / boundary / Ewald |
| v5.0.0-beta.9  | 59 | Registry resolution engine / CLI layer |
| v5.0.0-beta.10 | 60 | Variance / N_evolution / while / batch sweep |
| v5.0.0-beta.11 | 61 | Macro sampling / empirical verification |
| v5.0.0-beta.12 | 62 | Batching upgrade / study orchestration |
| v5.1.13        | 72 | Pillar F: empirical chemistry, Dynx, CTL pipeline |
| v5.13.3        | 73 | Wizard modules 6–8 full field coverage |
| v5.13.4        | 74 | Gap classifier + output filter + chemistry audit — 167/167 tests |
| v5.13.5        | 75 | IKK End-Tag (WO-75A-A), Identity Vector (WO-75B), MCF-CAI (WO-76) |

### Deferred to next arc (post-v5.13)

| Item | WO | Reason |
|------|-----|--------|
| IKK GL D-colour overlay | WO-75A-B | Renderer surface deferred intact |
| `vsepr doctor` full pass | WO-75B gate | Runtime data path work |
| GitHub Actions CI | MF-F02 | **DEPRECATED** — intentionally out of scope |
| `[sweep]` runtime dispatch | MF-B01 | Next arc |

### Freeze artefact

`docs/Theoretical/chapter_20_continual_report.tex` + `.pdf` authored as the formal
closure document for this arc. Covers: Continual Printing System doctrine,
KernelEventLog / event spine, IKK End-Tag (WO-75A), IKK Identity Vector (WO-75B),
and branch freeze summary.

---

*Ledger maintained per `VSIM_DEVELOPMENT.md` §4C. Update with every WO completion.*
*Last compiled: 2026-06-24 | Commit: 489d11cd | 167/167 tests passing | v5.13 FROZEN Day 82*

---

## Day 93 — WO-93A Status-Loop QoL Complete

**Date:** 2026-08-04  
**Branch:** `day84t-chemplus-declarative-vsepr`  
**Version:** `v5.15.0`  
**Status:** ✅ COMMITTED

### Evidence

| Check | Result |
|---|---|
| Build | `vsepr.exe` links with `vsepr_infra`; zero errors |
| Test | `WO93AStatusLoopTest` passes via CTest |
| Demo | `scripts/demos/wo93a_status_loop_demo.vsim` renders two-line HUD |
| GPU detection | `NVIDIA GeForce RTX 4070` detected without ambiguity |
| Viewer guard | Missing `vsepr-view.exe` prints console warning, no dialog |
| Docs | `VSIM_REFERENCE.md`, `docs/VSIM_LANGUAGE.md`, `docs/WO93A_STATUS_LOOP.md` updated |

### Forward pointer

Next: WO-94A (gas restoration) or WO-94B (live refresh thread ownership).

---

## Day 82 — v5.13 Branch Freeze

**Date:** 2026-06-28  
**Branch:** `feature/wizard-full-module-expansion`  
**Final version tag:** `v5.13.5`  
**Status:** ❄️ FROZEN

### What the v5.13 arc delivered (Days 57–82)

| Version | Day | Deliverable |
|---------|-----|-------------|
| v5.0.0-beta.7 | 57 | render_interval / step emission cadence |
| v5.0.0-beta.8 | 58 | PBC / cell / boundary / Ewald |
| v5.0.0-beta.9 | 59 | Registry resolution engine / CLI layer |
| v5.0.0-beta.10 | 60 | Variance / N_evolution / while / batch sweep |
| v5.0.0-beta.11 | 61 | Macro sampling / empirical verification |
| v5.0.0-beta.12 | 62 | Batching upgrade / study orchestration |
| v5.1.13 | 72 | Pillar F: empirical chemistry, Dynx, CTL pipeline |
| v5.13.3 | 73 | Wizard modules 6–8 full field coverage |
| v5.13.4 | 74 | Gap classifier + output filter + chemistry audit — 167/167 tests |
| v5.13.5 | 75 | IKK End-Tag (WO-75A-A), Identity Vector (WO-75B), MCF-CAI (WO-76) |

### Deferred to next arc (post-v5.13)

| Item | WO | Reason |
|------|-----|--------|
| IKK GL D-colour overlay | WO-75A-B | Renderer surface not yet wired; deferred intact |
| `vsepr doctor` full pass | WO-75B | Runtime data path work; next arc gate |
| GitHub Actions CI | MF-F02 | **DEPRECATED** — intentionally out of scope |
| `[sweep]` runtime dispatch | MF-B01 | Next arc |

### Freeze confirmation

- 167/167 CTest targets passing at freeze commit `489d11cd`
- All pending items are either DEFERRED (tracked) or DEPRECATED (intentional)
- Chapter 20 (`docs/Theoretical/chapter_20_continual_report.tex`) authored as the freeze documentation artifact
- Branch ready for archive; next work opens a new branch from this state

---

## Day 84 — VSEPR Revival Arc  (branch: `day84t-chemplus-declarative-vsepr`)

**Status:** 🟢 IN PROGRESS — WO-84S ✅  WO-84T ✅  WO-84U ✅

### Arc delivery log

| WO | Group | Day | Deliverable | Status |
|----|-------|-----|-------------|--------|
| WO-84S | 90 | 84 | VSEPR observe-sink metrics + 3 PNG exports (H2O, NH3, CO2) | ✅ COMPLETE |
| WO-84T | 91 | 84 | Chem+ declarative bridge (`ChemPlusSection`, `chemplus_declarative.hpp`, 39 PASS) | ✅ COMPLETE |
| WO-84U | 92 | 84 | ChemPlus CLI classify integration (`vsepr classify` surfaces `[ChemPlus]` block, 33 PASS, 15/15 demo) | ✅ COMPLETE |

### Gate criteria for WO-84U

| Check | Target | Result |
|-------|--------|--------|
| Group 92 C++ tests | 33/33 PASS | ✅ PASS |
| Python demo `demo_wo84u_chemplus_cli.py` | 15/15 PASS | ✅ PASS |
| `vsepr classify` `[ChemPlus]` block | present in output | ✅ PASS |
| PNG 3D energy landscape | `out/wo84u/chemplus_cli_energy_3d.png` | ✅ PASS |
| Build clean | zero errors | ✅ PASS |

### Commit log (Day 84 arc)

| Commit | WO | Description |
|--------|----|-------------|
| `4a1fbdb4` | WO-84S | VSEPR observe sink demo script (16/16 PASS, 3 PNGs) |
| `b495884d` | WO-84S | VSEPR observe sink metrics |
| `be285970` | — | Merge WO-84S into day84-vsepr-revival |
| `4c770688` | WO-84T | Chem+ declarative VSEPR bridge (Group 91, 39 PASS) |
| `0a2ce72b` | WO-84U | ChemPlus CLI classify integration (Group 92, 33 PASS, PNG) |

---

## Day 88 — Visual-System Repair and VSIM Integration

**Status:** 🟡 PLANNED — continuation after Day 87 finalization

| WO | Day | Deliverable | Status | Dependency |
|----|-----|-------------|--------|------------|
| WO-88 | 88 | Visual repair of new system: establish a reliable BGFX/GLFW visual-data and rendering-lifecycle boundary | TODO | Day 87 visual-system review |
| WO-88A | 88 | Improve and integrate VSIM modules through the repaired visual boundary | TODO | WO-88 supported contract |

### Day 88 scope boundary

- WO-88 repairs and validates the new visual frontend without changing authoritative scientific data.
- WO-88A follows only after WO-88 and wires existing VSIM modules through explicit parser, runtime, routing, artifact, test, and documentation contracts.
- The former WO-88A hosted-session UI reservation under WO-87A is deferred and requires a new unique child identifier before implementation.

*Ledger maintained per `VSIM_DEVELOPMENT.md` §4C. Update with every WO completion.*
*Last compiled: 2026-08-04 | Branch: day84t-chemplus-declarative-vsepr | Day 93 WO-93A COMPLETE | v5.15.0*

