# STAGE.md — VSEPR-SIM Work-Order Progress Ledger
> **Source of truth for all beta test groups, WO status, and milestone commits.**
> Updated: v5.0.0-beta.12 | Branch: v5.0.0-beta.7-step-attempt

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
| CellSection triclinic      | v5.1.0      | type = "orthorhombic" only for now |
| FieldRamp formation stage  | v5.2.0      | Parsed, no execution yet |
| Cycle formation stage      | v5.2.0      | Parsed, no execution yet |
