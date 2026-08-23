# WO-MF-01 — Missing Features Ledger
<!-- VSEPR-SIM | v5.0.14 | branch: feature/wizard-full-module-expansion -->
<!-- Status: ACTIVE | Created: 2026-06-18 | Owner: Desktop Kernel Legacy arc -->

---

## Purpose

This document is the single authoritative reference for **all features that are
parsed, schema-defined, or otherwise planned but not yet fully wired into the
runtime**.  Every entry carries:

- A unique `MF-NNN` tag that can be referenced in commits, PRs, and WO docs
- The subsystem and the exact blocking gap (parse-only, writer-stub, runtime-not-wired, etc.)
- The three **workflow use-cases** (UC-1 / UC-2 / UC-3) that exercise the feature
- A gate status: `PENDING`, `IN-PROGRESS`, or `COMPLETE`

This ledger is the input to the `tests/automation/run_all_uc.sh` harness which
maps PENDING features to a `PENDING` (not `FAIL`) result so CI never blocks on
work-in-progress items.

---

## Severity scale

| Symbol | Meaning |
|--------|---------|
| 🔴 | Blocks a UC workflow end-to-end |
| 🟠 | Degrades a UC output but does not block |
| 🟡 | Nice-to-have; no UC is blocked |
| 🟢 | Complete — kept for traceability |

---

## Feature ledger

### A — Export writers (xsim::xport)

| ID | Feature | Gap | Blocks | Status |
|----|---------|-----|--------|--------|
| MF-A01 | `write_cluster_json` | Writer declared, not implemented | UC-2 | 🔴 PENDING |
| MF-A02 | `write_fingerprint_json` | Writer declared, not implemented | UC-2 | 🔴 PENDING |
| MF-A03 | `write_summary_csv` | Writer declared, not implemented | UC-2 | 🔴 PENDING |
| MF-A04 | `write_pdb` | Parser wired; `PDBWriter` class missing | UC-1 (optional) | 🟠 PENDING |
| MF-A05 | STEP B-Rep solid export | Only point-cloud; B-Rep deferred | UC-3 (optional) | 🟡 PENDING |

#### A' — Export output naming gap (discovered 2026-06-18)

The `vsepr run` CLI currently emits files under the internal pipeline-stage naming
scheme (`pipeline_records.json`, `uc1_h2o_quick.xyz`, `run_manifest.json`).
The `xsim::xport` package is designed to emit canonical names
(`trajectory.xyz`, `analysis.json`, `manifest.json`, etc.).
The bridge from CLI → xport canonical output is not yet wired.

| ID | Feature | Gap | Blocks | Status |
|----|---------|-----|--------|--------|
| MF-A06 | Canonical `trajectory.xyz` filename | CLI emits `{name}.xyz` | UC-1 validation | 🟠 PENDING |
| MF-A07 | Canonical `analysis.json` | CLI emits `pipeline_records.json` | UC-1 validation | 🟠 PENDING |
| MF-A08 | Canonical `manifest.json` | CLI emits `run_manifest.json` | UC-1 validation | 🟠 PENDING |
| MF-A09 | Canonical `report.md` | CLI emits `pipeline_dashboard.md` | UC-1 validation | 🟠 PENDING |
| MF-A10 | Canonical `metrics.tsv` | Not emitted by headless CLI | UC-1 validation | 🔴 PENDING |

### B — VSIM runtime wiring

| ID | Feature | Gap | Blocks | Status |
|----|---------|-----|--------|--------|
| MF-B01 | `[sweep]` runtime execution | Parsed; no sweep dispatcher | UC-2 | 🔴 PENDING |
| MF-B02 | `[material.*]` sub-sections | Captured in `raw_sections`; generation engine not wired | UC-2 | 🔴 PENDING |
| MF-B03 | `render_targets` dispatch table | Parsed; routing table not built | UC-1 GL mode | 🟠 PENDING |
| MF-B04 | Formation libraries runtime | `BatchDocument::formation_library` parsed; executor missing | UC-3 | 🟠 PENDING |

### C — Isomer subsystem

| ID | Feature | Gap | Blocks | Status |
|----|---------|-----|--------|--------|
| MF-C01 | `[generator.isomers]` runtime | Schema + parser done (WO-VSIM-03B); generator engine pending (WO-VSIM-03C) | UC-3 | 🔴 PENDING |
| MF-C02 | `[analysis.isomer_tracking]` trajectory walker | Struct present; step-by-step walker not wired | UC-3 | 🔴 PENDING |
| MF-C03 | Geometric / chirality detection | Simplified CIP tetrahedral R/S implemented in `isomer_signature.hpp`; all isomer tests pass | UC-3 | ✅ COMPLETE |
| MF-C04 | Isomer-ranked output report | No ranked-results writer | UC-3 | 🔴 PENDING |

### D — Analysis layer

| ID | Feature | Gap | Blocks | Status |
|----|---------|-----|--------|--------|
| MF-D01 | WO-74A: Gap classifier consolidation | `gap_classifier_wo74c.cpp` not merged into `gap_classifier.cpp` | UC-2 | 🟠 PENDING |
| MF-D02 | WO-74B: Output filter hardening | `vsim_output_filter.cpp` missing 4-format gating | UC-2 | 🟠 PENDING |
| MF-D03 | WO-74C: Length-scale fitter validation | 3+ analytic test cases missing | UC-2 | 🟠 PENDING |

### E — Desktop / IKK

| ID | Feature | Gap | Blocks | Status |
|----|---------|-----|--------|--------|
| MF-E01 | IKK GL overlay (WO-75A Part B) | Scripting layer done; GL render pass not wired | UC-1 GL mode | 🟠 PENDING |
| MF-E02 | IKK Python time-series (WO-75A Part C) | `plot_dist_timeseries()` + PNG embed delivered in `reporting/generate_report.py` | UC-1 | ✅ COMPLETE |
| MF-E03 | MCF-CAI state vector integration (WO-76) | Depends on WO-75A | UC-3 | 🟡 PENDING |
| MF-E04 | `[run] mode = "sweep"` desktop progress reporting | RunVsimPanel has no sweep-step counter | UC-2 | 🟠 PENDING |

### F — Infrastructure

| ID | Feature | Gap | Blocks | Status |
|----|---------|-----|--------|--------|
| MF-F01 | STAGE.md missing | Recreated at repo root with Days 1-75, Groups 1-89, release gate table | All | ✅ COMPLETE |
| MF-F02 | GitHub Actions CI workflow | ~~No `.github/workflows/*.yml` in repo root~~ — **DEPRECATED**: intentionally out of scope for the v5.13 freeze arc; CI infra deferred to post-v5.13 work. Not a blocking gap for any existing UC. | All | ~~🟠 PENDING~~ **DEPRECATED** |
| MF-F03 | v5.13.5 release gate (WO-75B) | ≥143 tests, `vsepr doctor` all OK, tag not pushed | All | 🟠 PENDING |

### G — Pipe bridge stack (UC-7)

Gaps discovered when resolving `pipe_sph_dem_fea_bridge.vsim` into the SiO₂
pipe fixture. All five are PENDING because the SPH/DEM/FEA bridge runtime is
not yet wired into the headless CLI pipeline.

| ID | Feature | Gap | Blocks | Status |
|----|---------|-----|--------|--------|
| MF-G01 | `write_energy_trace_svg` export writer | Field parsed; SVG writer not implemented | UC-7 | 🔴 PENDING |
| MF-G02 | `write_html_dashboard` export writer | Field parsed; HTML writer not implemented | UC-7 | 🔴 PENDING |
| MF-G03 | `write_pipeline_audit_jsonl` writer | Field parsed; JSONL audit log not emitted | UC-7 | 🟠 PENDING |
| MF-G04 | DEMBridge / SPH coupling runtime | `DEMBridge` parsed; SPH↔DEM coupling not wired | UC-7 | 🔴 PENDING |
| MF-G05 | FEABridge von Mises / fatigue output | `FEABridge` parsed; stress record emission missing | UC-7 | 🔴 PENDING |

#### G′ — Pipe bridge / resolved-object gap tags (Day 85, WO-85H)

Second-generation pipe-bridge gaps discovered when formalizing the **resolved
material payload** view of UC-7 (`scripts/uc7_resolved_sio2_pipe.vsim`) and the
Day-85 golden dispatcher (UC-6). These use the dash-form `MF-G-0NN` IDs to stay
textually distinct from the legacy `MF-G0N` writers above. They describe object
*resolution* and *validation* gaps, not the SPH/DEM/FEA export writers.

| ID | Feature | Gap | Blocks | Status |
|----|---------|-----|--------|--------|
| MF-G-001 | Pipe material payload not fully resolved | Pipe accepts material/formula ref, but not all physical properties resolve | UC-7 | 🟠 PENDING |
| MF-G-002 | Pipe geometry profile lacks full validation | Linear profile parses, but geometric validation is shallow | UC-7 | 🟠 PENDING |
| MF-G-003 | Pipe flow model is placeholder/static only | Flow fields parsed; not connected to hydraulic/transport solving | UC-7 | 🟠 PENDING |
| MF-G-004 | Formula→material bridge lacks property DB | SiO₂ resolves as formula; density/phase/viscosity/transport not sourced | UC-7 | 🔴 PENDING |
| MF-G-005 | Solid payload role in pipe is ambiguous | Wall vs transported particles vs coating vs packed-bed must be explicit | UC-7 | 🟠 PENDING |
| MF-G-006 | Linear profile lacks segment diagnostics | Profile samples exist; no per-segment validation/reporting | UC-7 | 🟡 PENDING |
| MF-G-007 | Inlet/outlet not linked to plant graph | Endpoints exist geometrically; not linked into a plant network | UC-7 | 🟡 PENDING |
| MF-G-008 | Pipe bridge export lacks normalized schema | Validation output needs stable JSON/report schema for regression | UC-6, UC-7 | 🟠 PENDING |
| MF-G-009 | No tolerance-gated regression for pipe bridge | UC-7 should establish a permanent pipe-validation test | UC-6, UC-7 | 🟠 PENDING |
| MF-G-010 | No failure taxonomy for malformed pipes | Bad pipes should fail with classified errors, not vague parser misery | UC-7 | 🔴 PENDING |

---

## Workflow use-case cross-reference

| UC | Name | Primary features tested |
|----|------|------------------------|
| **UC-1** | Desktop Script-to-Results | MF-A04, MF-B03, MF-E01 |
| **UC-2** | Batch Crystal Sweep | MF-A01, MF-A02, MF-A03, MF-B01, MF-B02, MF-D01, MF-D02, MF-D03, MF-E04 |
| **UC-3** | Isomer Discovery Pipeline | MF-B04, MF-C01, MF-C02, MF-C03, MF-C04, MF-E03 |
| **UC-6** | Crystallographic Regression Suite | MF-A07, MF-D01, MF-D02, MF-D03 |
| **UC-6** | Golden Suite Dispatcher (Day 85) | MF-G-008, MF-G-009 |
| **UC-7** | SiO₂ Pipe Simulation | MF-G01, MF-G02, MF-G03, MF-G04, MF-G05 |
| **UC-7** | Resolved SiO₂ Pipe Bridge (Day 85) | MF-G-001..MF-G-010 |

See `docs/uc/UC-1-desktop-script-to-results.md`, `UC-2-batch-crystal-sweep.md`,
`UC-3-isomer-discovery-pipeline.md`, `UC-6-crystallographic-regression.md`,
`UC-6-golden-suite-dispatcher.md`, `UC-7-sio2-pipe-simulation.md`,
and `UC-7-resolved-sio2-pipe-bridge.md` for full designs.

---

## Automation contract

The harness `tests/automation/run_all_uc.sh` evaluates each UC with three result
classes:

| Class | Meaning |
|-------|---------|
| `PASS` | Expected outputs produced and validated |
| `PENDING` | Feature listed in this ledger as PENDING; output absent but not a failure |
| `FAIL` | Feature claims to be implemented but output is wrong or absent |

The mapping from `MF-NNN` → expected output file is encoded in each
`tests/automation/uc*_run.sh` script via `expect_file` / `expect_pending`
helper calls.

---

## How to promote a feature from PENDING to COMPLETE

1. Implement the feature following the **5-step developer checklist**
   (`VSIM_DEVELOPMENT.md`).
2. Change the status symbol in this ledger from `PENDING` → `COMPLETE` and
   swap the `expect_pending` call in the relevant `uc*_run.sh` to `expect_file`.
3. Run `tests/automation/run_all_uc.sh` — the PENDING count should drop by one
   and the PASS count should rise by one.
4. Update `VSIM_REFERENCE.md` and `STAGE.md`.

---

## Open items not yet assigned an MF tag

- `[[raw.object]]` / `[[override.particle]]` / `[excite.*]` blocks (deferred to BRIDGE-B/C)
- [sweep] declarative runtime — WO tracking number TBD
- MCF-CAI Phase 3+ (depends on WO-75A)

---

*Last updated: 2026-06-28 | WO-MF-01 | MF-F02 DEPRECATED (GitHub CI out of scope for v5.13 freeze); MF-F01 + MF-E02 COMPLETE; 167/167 tests passing*
