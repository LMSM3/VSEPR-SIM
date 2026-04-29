# Copilot Instructions

Version: 2026-05-07
Status: Research-oriented active development  
Scope: High-level project direction, scientific framing, system philosophy, development priorities, and intended utilization context  

---

## 1. Identity

**VSEPR-SIM** is a deterministic atomistic simulation, analysis, and reporting platform for molecular, material, bead-based, and premacro systems.

It is a long-term research platform under active construction.

It is **not**:

- a finished product  
- a visualization toy  
- a chemistry gimmick  
- a static educational demo  
- a folder full of heroic but suspiciously undocumented experiments  

The final direction is:

- Digital twin support  
- Predictive modeling  
- Candidate evaluation  
- Report generation  
- Discovery-oriented workflows grounded in deterministic modeling and empirical comparison  

The project should be developed as a serious scientific software environment. Every module should make the research kernel stronger, not just add another shiny lever for future regret.

---

## 2. Current Stage: **beta-7**

**Beta-6 is closed.**

**Beta-6 established:**

- isolated Eigen bridge through `vsepr::eigen_bridge`  
- preservation of native `vsepr::Vec3`  
- production-ready Kabsch alignment  
- production-ready RMSD analysis  
- stationarity backbone  
- crystal imperfection emergence tests  
- surface interaction analysis  
- diffusion analysis  
- transport inference  
- packing analysis  
- macro property inference  
- report output  
- xyzFull audit  

**Beta-7 goal:**

Wire existing modules into a coherent research pipeline:


FormationOutput
→ FingerprintRecord
→ ClusterRecord
→ AnalysisRecord
→ ReportRecord
→ DashboardRecord


A completed beta-7 run should produce:

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

**Beta-7 is not the time to invent five new ornamental subsystems because the dopamine goblin demanded more complexity.**

**Beta-7 progress (as of Day 57):**

- WO-56C closed: central kernel pass-through consolidation complete
- WO-57D closed: `render_interval` step-count cadence added to `[visual]` and `[visual.external]`
- Group 35 acceptance tests live (`tests/test_render_interval.cpp`)
- Developer documentation infrastructure established:
  - `docs/VSIM_LANGUAGE.md` — canonical .vsim language guide
  - `VSIM_REFERENCE.md` — field reference adjacent to README (update with every schema change)
  - `VSIM_DEVELOPMENT.md` — 5-step add/wire/test/document checklist
- Commit: `142fb5e4` on branch `v5.0.0-beta.7-step-attempt`

---

## 3. Permanent Core Architectural Rule

`xyz` and `xyzFull` store **ground-truth state and trajectory only**.

They may store:

- particle identity  
- position  
- position history  
- timestep  
- orientation  
- velocity  
- persistent ID  
- lineage ID  
- decay seed  
- energy-layer trace  
- simulation metadata needed to reconstruct the state  

They must **not** store:

- inferred material class  
- inferred diffusion label  
- inferred permeability label  
- inferred packing label  
- inferred macro property  
- analysis-only classification result  

**Permanent doctrine:**

- `xyzFull` stores **what happened**  
- Analysis determines **what it means**  

Inferred properties belong in:

- analysis records  
- reports  
- dashboards  
- sidecar files  

They do **not** belong inside State, `xyz`, or `xyzFull`.

> Encoding conclusions into state and then “discovering” them later is not emergence. It is a magic trick for people who clap when Excel opens.

Engineering geometry truth: include CAD/export artifacts (.step) as required workflow artifacts representing engineering geometry truth. Treat .step files as export/sidecar artifacts that document intended engineering geometry; do not conflate them with inferred analysis results or embed analysis conclusions into these files.

---

## 4. render_interval Doctrine

`render_interval` is a **step-count integer cadence** controlling how often render and export emission is triggered during simulation. It is **orthogonal to `display_fps`**.

| Field | Scope | Meaning |
|---|---|---|
| `render_interval` | `[visual]`, `[visual.external]` | Emit render/export every N simulation steps (default: 1) |
| `display_fps` | `[visual]` | UI/console refresh rate — unrelated to emission cadence |

- `render_interval = 1` means emit every step (default behavior, no skipping)
- `render_interval = 0` is treated as 1 (guard against zero division)
- External backends may inherit or override the parent `[visual]` interval
- Gate render dispatch via `VisualSection::should_render(int step)` and `VisualExternalSection::should_render(int step, int visual_interval)`
- **Never conflate display refresh with emission cadence**

---

## 5. Permanent Terminology Rule

The following terms are **forbidden**:

- meso  
- mesoscopic  
- meso-scale  
- meso renderer  
- meso model  
- meso visualization  

Use these instead:

- atomistic  
- bead  
- coarse bead  
- premacro  
- macro  
- formation  
- trajectory  
- state history  
- analysis layer  
- inference layer  
- reporting layer  

**Correct examples:**

- atomistic model  
- atomistic structure  
- atomistic visualization  
- atomistic generator  
- atomistic analysis  
- bead dynamics  
- premacro inference  
- macro transport inference  

---

## 6. System Layers

The project is organized around scientific workflow layers:

| Layer           | Contents |
|----------------|----------|
| Input          | names, formulas, aliases, scripted runs, presets, seed structures |
| Identity       | canonical identity, particle identity, molecular identity, material identity, persistent IDs, lineage IDs |
| Formation      | structure generation, priors, relaxation, dynamics, temperature schedules, energy tracking |
| State          | positions, velocities, orientations, time history, event history, decay events, energy traces |
| Analysis       | Kabsch, RMSD, stationarity, defect emergence, surface interaction, diffusion, packing, transport inference, macro inference |
| Classification | fingerprints, structure clustering, polymorph grouping, isomorph grouping, defect grouping |
| Reporting      | tables, figures, dashboards, SVG/PNG, technical summaries, validation warnings |
| Export         | xyz, xyzFull, CSV, JSON, XLSX, SVG, report documents, .step (engineering geometry truth), future SolidWorks outputs |

---

## 7. Developer Procedure (add / wire / document)

Every new VSIM feature follows this sequence. No exceptions.

1. **Define** — Add field with default to the correct struct in `include/vsim/vsim_document.hpp`
2. **Parse** — Wire the key in `src/vsim/vsim_parser.cpp` (`apply_*_key()` function)
3. **Wire** — Apply the field in runtime/demo apps; gate behavior with the field value
4. **Test** — Create or extend a test group in `tests/`; register in `tests/CMakeLists.txt`
5. **Document** — Update `VSIM_REFERENCE.md`, `docs/VSIM_LANGUAGE.md`, and `VSIM_DEVELOPMENT.md`

`VSIM_REFERENCE.md` **must be updated with every schema change**, no exceptions. It is the living field-reference adjacent to the README.

---

## 8. Key Reference Files

| File | Purpose |
|---|---|
| `include/vsim/vsim_document.hpp` | Schema structs and authoritative defaults |
| `src/vsim/vsim_parser.cpp` | `.vsim` key-to-field wiring |
| `apps/beta10_demo.cpp` | Phase pipeline demo with auto render layer |
| `apps/kernel_demo.cpp` | Kernel demo with scenario/interactive render paths |
| `VSIM_REFERENCE.md` | Living field reference — update with every change |
| `docs/VSIM_LANGUAGE.md` | Canonical .vsim language and section guide |
| `VSIM_DEVELOPMENT.md` | 5-step add/wire/test/document checklist |
| `STAGE.md` | Master development stage and gate ledger |
| `docs/wo/` | Per-work-order implementation records |

**Architecture flow:**