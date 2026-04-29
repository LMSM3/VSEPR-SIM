# Copilot Instructions

Version: 2026-04-25
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

## 4. Permanent Terminology Rule

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

## 5. System Layers

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

**Architecture flow:**