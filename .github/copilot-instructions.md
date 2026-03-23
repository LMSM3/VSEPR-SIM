# Copilot Instructions

Version: 2026-03-14  
Status: Research-oriented active development  
Scope: High-level project direction, scientific framing, system philosophy, development priorities, and intended utilization context  

---

## 1. Project Identity

**VSEPR-SIM** is a research-oriented, deterministic, atomistic simulation and analysis platform intended to support the study, generation, interpretation, and reporting of molecular and material structures.

At its highest level, the project exists to bridge:

- scientific theory
- molecular identity
- deterministic structural generation
- atomistic analysis
- visualization
- computational reporting
- future discovery workflows

The system is not merely a visualization toy, a chemistry parser, or a static educational demo. It is being developed as a broader scientific software environment capable of supporting structured research and repeatable computational investigation.

---

## 2. Primary Mission

The primary mission of VSEPR-SIM is:

> **To create a scientifically serious atomistic platform for deterministic molecular and structural interpretation, generation, visualization, and research-oriented reporting.**

This includes support for:

- interpreting molecular inputs
- resolving names, formulas, and canonical representations
- generating atomistic structures
- producing scientifically useful outputs
- enabling comparison, analysis, and eventual discovery workflows
- integrating computational outputs into research documentation

The project should be understood as a **research platform under active construction**, not as a finished product.

---

## 3. Current Development Stage

VSEPR-SIM is in a **research-oriented development stage**.

This means the project is currently focused on:

- building core scientific infrastructure
- refining system architecture
- establishing reliable terminology
- improving input and output workflows
- creating research-usable tooling
- making the platform more coherent and less dependent on ad hoc internal conventions

At this stage, the system should prioritize:
- clarity of purpose
- scientific consistency
- modular architecture
- deterministic behavior
- portable tooling
- credible research utility

It should **not** prioritize:
- visual polish over scientific substance
- decorative complexity
- marketing-style claims
- unfinished abstraction layers presented as complete systems

---

## 4. Research Context

The project is intended for use in a research context where users may need to:

- evaluate molecular structures
- compare candidate systems
- inspect atomistic properties
- generate deterministic structural outputs
- produce figures and data for reports
- organize molecule-level scientific information
- build toward higher-confidence computational screening

VSEPR-SIM should therefore be designed as a platform that can contribute to:

- exploratory computational chemistry workflows
- atomistic modeling support
- material and molecular investigation
- report generation for technical and academic use
- future structured discovery pipelines

The project should be usable by someone who is not merely “running a cool simulation,” but who is trying to **extract meaningful scientific value** from computational structure generation and analysis.

---

## 5. Utilization Philosophy

VSEPR-SIM is meant to be used as a **scientific instrument**, not as a black-box gimmick.

Outputs should be:
- explicit
- inspectable
- deterministic
- modular
- explainable
- suitable for technical interpretation

The software should support workflows where a user may:
1. provide a molecular input
2. resolve it into a structured internal representation
3. generate atomistic interpretation
4. inspect geometry, radius, or structural data
5. export outputs for further analysis
6. embed results into broader research documentation

This means the platform should eventually function as both:
- a computational engine
- a reporting and research support environment

The project follows an **anti-black-box** design philosophy: every mapping decision, every metric, every intermediate result must be explicitly inspectable, traceable, and deterministic. No hidden state. This applies to all layers — atomistic, coarse-grained, visualization, and reporting.

---

## 6. Scientific Terminology Rules

### 6.1 Forbidden terminology
The term **"meso"** must never appear in the codebase or documentation.

This includes:
- meso
- mesoscopic
- meso-scale
- meso renderer
- meso model
- meso visualization

### 6.2 Required terminology
Use **"atomistic"** instead.

Examples:
- atomistic model
- atomistic structure
- atomistic visualization
- atomistic radius output
- atomistic generator
- atomistic analysis

### 6.3 Scope of enforcement
This terminology rule applies to:
- code
- comments
- documentation
- diagrams
- file names
- function names
- CLI help text
- internal notes intended for project use

This is a permanent project rule.

---

## 7. System Philosophy

The project should follow these principles:

### 7.1 Deterministic over theatrical
The system should prefer reproducible outputs over vague “AI-like” or ornamental behavior.

### 7.2 Scientific coherence over feature sprawl
New features should strengthen the scientific core, not distract from it.

### 7.3 Research utility over interface vanity
A plain but useful output is better than a polished but meaningless one.

### 7.4 Architecture must reflect actual use
The system should be organized around how scientific workflows actually occur:
- input
- normalization
- resolution
- generation
- analysis
- reporting

### 7.5 Frontends should remain lightweight
User-facing tools should route, interpret, and display. Scientific computation should remain in the kernel or core engine.

---

## 8. Broad Scope of the Project

At a high level, the long-term scope of VSEPR-SIM includes:

- deterministic atomistic structure generation
- molecular identity handling
- formula and alias resolution
- geometry and radius analysis
- atomistic visualization
- structured scientific output
- computational reporting
- support for molecule and material investigation
- future extension toward broader discovery-oriented workflows

The broad scope is intentionally larger than a single CLI tool or a single rendering mode.

The project should be developed with the understanding that:
- visualization is one layer
- reporting is one layer
- structure generation is one layer
- kernel science is one layer
- research utilization is the real destination

---

## 9. High-Level Targets

### 9.1 Near-term targets
Current near-term targets include:

- improving the coherence of the visualization system
- building a dedicated atomistic visualization frontend
- supporting common-name and formula-based molecular input
- connecting user-facing tools more cleanly to the scientific kernel
- generating research-usable figures and structured outputs
- improving portability and install reliability

### 9.2 Mid-term targets
Mid-term targets include:

- stronger canonical molecule handling
- cleaner routing between frontend and kernel
- improved atomistic data generation
- better report-ready outputs
- more robust scientific metadata and export support
- reduction of legacy naming and structural inconsistencies

### 9.3 Long-term targets
Long-term targets include:

- a mature atomistic simulation and analysis environment
- reliable structure-generation pipelines
- broad scientific reporting integration
- a platform useful for computational investigation and candidate evaluation
- eventual support for discovery-oriented workflows grounded in deterministic modeling and empirical comparison

---

## 10. Current Active Direction

The current active direction is to strengthen the **visualization and interpretation layer** of the project so that it becomes a proper research-facing interface rather than just a bolt-on mode.

This includes the introduction of a dedicated visualization command such as:
viz "benzene carbonate" --radius

---

## 11. Development Phases

### 11.1 Working Structure
The development phases should follow this structure:
1. Review and implement theoretical foundation documents (add LaTeX section)
2. Expand or edit the code (implement in headers/source)
3. Perform tests if finishing a development phase (create test file, add CMake target, verify compilation)

New sections are appended to documents rather than replacing previous ones, showing model growth over time.

---

## General Instructions
- Do not ask for confirmation before continuing to the next phase — just proceed automatically after completing each phase.