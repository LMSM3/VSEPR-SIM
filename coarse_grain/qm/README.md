# qm/ — Quantum Mechanical Descriptor Preparation Layer

**Status:** Preparation (stub infrastructure for Level 3 Beading)

## Purpose

This layer provides **QM-derived descriptors** that annotate each bead with
quantum-mechanical evidence variables. These descriptors are NOT a full QM
engine — they are a preparation layer that:

1. Holds per-bead QM proxy quantities computable from the CG state
2. Defines the interface contract a future QM backend must satisfy
3. Provides analytic/semi-empirical fallbacks where full QM is absent

## Physical quantities prepared

| Symbol | Name | Source |
|--------|------|--------|
| φᵢ | Electrostatic potential at bead i | Coulomb sum over neighbours |
| χ̄ᵢ | Mean electronegativity of parent atoms | Species table |
| αᵢ | Polarisability proxy | Empirical from atomic polarisabilities |
| HOMO_proxy | HOMO energy proxy | Ionisation potential estimate |
| LUMO_proxy | LUMO energy proxy | Electron affinity estimate |
| q_eff | Effective charge | Charge equilibration (Qeq-style, level-0) |
| Ω_overlap | Orbital overlap proxy | Distance-weighted electronegativity sum |

## Levels of fidelity (for future expansion)

```
Level 0 (this file):   Analytic/empirical from existing CG state + species tables
Level 1 (future):      Semi-empirical tight-binding (GFN-xTB interface)
Level 2 (future):      DFT single-point (ORCA/PySCF external call)
```

## Relationship to Level 3 Beading

Level 3 Beads carry a `QMDescriptor` field. The `qm/` layer populates it.
The macro-DM layer reads it for transport and reactivity precursors.

## Files

| File | Purpose |
|------|---------|
| `qm_descriptors.hpp` | `QMDescriptor` struct + Level-0 analytic computation |
| `qm_interface.hpp` | Abstract interface for future Level-1/2 backends |
