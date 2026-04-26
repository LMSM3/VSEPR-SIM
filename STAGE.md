# VSEPR-SIM — Development Stage Marker

> Version: **5.0.0-beta-7**  
> Date: 2025-07-11  
> Branch: `v5.0.0-beta.7-step-attempt`  
> Checkpoint tag: `v5.0.0-beta7-display`

---

## Stage: beta-7

### What beta-7 is

Wiring existing modules into a coherent research pipeline:

```
FormationOutput
→ FingerprintRecord
→ ClusterRecord
→ AnalysisRecord
→ ReportRecord
→ DashboardRecord
```

A completed beta-7 run must produce:

- formation log
- final structure
- trajectory
- energy trace
- stationarity result
- fingerprint
- cluster assignment
- defect or surface interpretation
- diffusion or packing analysis
- validity warnings
- report tables
- dashboard export

---

## Closed arc: beta-7 display (checkpoint `v5.0.0-beta7-display`, commit `f1587c4`)

The passive batch display layer is **complete and stable**.

| Artifact | Status |
|---|---|
| `VizMode::BATCH_PASSIVE` | ✓ |
| `BatchWindowBridge` (lock-free producer-consumer) | ✓ |
| `ContinuousRunDisplay` (GL + console fallback) | ✓ |
| `Window::run_batch()` | ✓ |
| `xyz_frame_to_snapshot()` | ✓ |
| `metal_gen` integration | ✓ run verified |
| Stress suite (`batch_display_stress`) | ✓ 12/12 |
| `docs/vis/batch_display_api.md` | ✓ |
| `docs/vis/batch_display_usage.md` | ✓ |

---

## Open arc: beta-7 pipeline

The report-generation pipeline wiring has **not started**. The following primitives are in place and ready to be connected:

| Primitive | File |
|---|---|
| Formation report (5-factor: E, M, K, T, N) | `coarse_grain/report/formation_report.hpp` |
| Autonomous report engine | `include/core/report_engine.hpp` |
| Stationarity gate | `src/core/stats/stationarity_gate.hpp` |
| RMSD tracker | `src/core/stats/rmsd_tracker.hpp` |
| Kabsch alignment | `src/analysis/kabsch.hpp` |
| Formation report test | `tests/test_formation_report.cpp` |
| Property pipeline suite | `tests/test_property_pipeline_suite7.cpp` |

### Next immediate work

1. Define `PipelineRecord` (or reuse existing record types) as the connective data type between stages
2. Wire `FormationOutput → FingerprintRecord` (fingerprint extraction after formation)
3. Wire `FingerprintRecord → ClusterRecord` (polymorph/isomorph grouping)
4. Wire `ClusterRecord → AnalysisRecord` (defect/surface/diffusion/packing per cluster)
5. Wire `AnalysisRecord → ReportRecord` (tables, CSV, JSON)
6. Wire `ReportRecord → DashboardRecord` (SVG/PNG export)

---

## Closed stages (prior to beta-7)

| Stage | Closed | Description |
|---|---|---|
| beta-6 | yes | Eigen bridge, Kabsch, RMSD, stationarity backbone, imperfection emergence, surface/diffusion/packing/transport/macro inference, xyzFull audit |
| v5.0.0 tag | `29ff7ad` | Environment-responsive bead transport (Phases A-H) |
| v4.0-LB | yes | Multi-scale property search, 3-5 scale bridge, C++23 |
| v3.0.1 | yes | Code audit, terminology purge |
| v2.9.2 | yes | 1013 tests, modular testing, CG layer |

---

## Permanent rules (from copilot-instructions)

- `xyzFull` stores **what happened** — analysis determines **what it means**
- Inferred properties belong in analysis records, reports, dashboards — **not** in State/xyz/xyzFull
- Terms `meso`, `mesoscopic`, `meso-scale` are **forbidden** — use `atomistic`, `bead`, `coarse bead`, `premacro`
- beta-7 is not the time to invent new ornamental subsystems
