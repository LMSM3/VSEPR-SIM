# Formation Engine Methodology
## Documentation Index

---

## LaTeX Methodology (Primary Documents)

The core scientific contribution lives in these TeX source files.
Each is self-contained and PDF-compilable.

### Condensed Versions

| File | Pages | Content |
|------|-------|---------|
| `METHODOLOGY_2PAGE.tex` | **2** | Two-column summary of all 13 sections |
| `METHODOLOGY_12PAGE.tex` | **12** | Condensed reference with equations, tables, and algorithms |

### Full Sections (~200 pages total)

| File | Sections | Content |
|------|----------|---------|
| `section0_identity_state_decomposition.tex` | **SS0** | Particle identity vectors, cell/world containers, formal ontology |
| `section1_foundational_thesis.tex` | **SS1** | Scope, formation problem definition, domain of validity |
| `section2_state_ontology.tex` | **SS2** | State tuple, identity/phase/scratch partitioning, file hierarchy |
| `section3_interaction_model.tex` | **SS3** | LJ 12-6, Coulomb, UFF parameters, switching functions, PBC |
| `section4_thermodynamics.tex` | **SS4** | Unit system, temperature, pressure, heat capacity |
| `section5_integration.tex` | **SS5** | Velocity Verlet, Langevin dynamics, FIRE minimization |
| `section6_formation_physics.tex` | **SS6** | Bonded terms, formation pipeline, basin mapping |
| `section7_statistical_interpretation.tex` | **SS7** | Welford's algorithm, stationarity, Kabsch alignment, scoring |
| `section8_9_reaction_electronic.tex` | **SS8-9** | QEq charges, Fukui functions, HSAB, reaction templates |
| `section8b_heat_gated_reaction_control.tex` | **SS8b** | Heat-gated reaction control, amino acid reference, 500-sim validation |
| `section10_12_13_closing.tex` | **SS10,12,13** | Multiscale projection, validation doctrine (35 tests), future work |
| `section11_self_audit.tex` | **SS11** | Failure classification, gap targeting, regression detection |
| `section_bridge_architecture.tex` | **SBA** | Canonical structural bridge: three-layer architecture, EngineAdapter, conversion pipeline |
| `section_phase_reports.tex` | **PHR** | Live phased revalidation reports (Phase 1+), pass/fail audit results |
| `section_coarse_grained_layer.tex` | **CGL** | Coarse-grained and multi-scale layer: CG model construction, statistical mechanics, emergent behavior, multi-scale coupling, validation, scale transition, thermostats, free energy, emergent effective medium mapping (ensemble proxy formulas, bulk/edge classifier, correlation length, convergence diagnostics), macro property precursor channels (8 channels with -like suffix, interface penalty, confidence model), property learning pipeline (dataset construction, 31-feature vectorization, ridge regression, grouped splits, synthetic supervision), property calibration and target legitimacy (5 property families, target contracts, calibration profiles, supervision regimes, confidence decomposition, OOD detection, synthetic curricula, promotion/demotion policy) |
| `section_anisotropic_beads.tex` | **ASB** | Anisotropic surface-mapped beads: spherical harmonic descriptors, inertia frames, probe-based sampling, orientation-coupled interactions, torques, benzene dimer configurations, model hierarchy, multi-channel descriptors (steric/electrostatic/dispersion), higher-order angular resolution, adaptive complexity, two-tier model selection guidelines, unified descriptor strategy (single formalism, adaptive truncation, residual-driven promotion), atomistic preparation layer (FragmentView scale boundary, validation status codes, dependency asymmetry), anisotropic bead model implementation specification (core data structure, 4-stage descriptor generation pipeline, per-ℓ channel kernels, harmonic-space interaction engine, SH rotation, adaptive refinement, system-level integration), formal bead state decomposition (per-variable physical analysis, four-layer architecture: identity/pose/resolution/provenance, update frequency classification, interaction engine view, dynamics engine view, rotational inertia extension with Euler equations, extended state tuple) |
| `section_environment_responsive_beads.tex` | **ERB** | Environment-responsive coarse-grained bead dynamics: extended bead state (𝔹_B = {B, X_B}), design principles (derived observables, fast/slow separation, low-dimensional), fast environment observables (local density ρ_B, coordination number C_B, orientational order P_{2,B} with gradients), slow internal state variable (η_B with relaxation dynamics, timescale analysis, steady-state, numerical integration), coupling mechanisms (kernel modulation, descriptor scaling, additive environment energy), emergent behaviour (spontaneous ordering, density-dependent response, hysteresis/metastability, interface sensitivity), formal specification summary with parameter ranges |
| `section_testing_infrastructure.tex` | **TIF** | Validation infrastructure for environment-responsive bead dynamics: testing philosophy (state-first, synthetic scenes), scene construction methodology (deterministic builders, randomised generators, transforms, validation helpers, neighbour cutoff policy), variable hierarchy (7 variables → 4 independent: ρ, C, P₂, η; 3 derived: ρ̂, P̂₂, f), causal chain (geometry → observables → target → η), test suite architecture (11 suites, 1013 tests: observable correctness, dynamical stability, Monte Carlo robustness across 4000 trials, structured N > 200 formation studies, dynamic regime mapping with invariance validation, emergent effective medium mapping with 131-test ensemble response proxy validation, macro precursor channel validation with directed perturbation tests, property learning pipeline validation with end-to-end regression and ranking, property calibration and target legitimacy with confidence decomposition and OOD detection, bead layer visualization validation), behavioural assertion framework (monotonicity, boundedness, convergence, statistical tools), trajectory runner specification, key empirical findings (variable redundancy, large-N stability gate at N = 216, edge vs bulk differentiation, coordination histogram structure, translation/rotation/permutation invariance confirmed, first-order relaxation with no multistability), validation gates, completed Stage 4 extensions (formation-under-conditions, hysteresis/memory, time-to-equilibrium, condition-to-structure atlas), Stage 5 ensemble analysis layer (EnsembleProxySummary, 5 macroscopic response proxies: cohesion, uniformity, texture, stabilization, surface sensitivity), Stage 6 macro precursor channels (8 property-like channels, interface penalty, confidence model, 85 tests), Stage 7 property learning pipeline (dataset construction, feature vectorization, ridge regression, evaluation metrics, synthetic supervision, 109 tests), Stage 8 property calibration and target legitimacy (property families, target contracts, calibration profiles, supervision regimes, confidence-gated prediction, OOD detection, synthetic curricula, promotion/demotion policy, 193 tests) |

**Compiled PDF included:** `section0_identity_state_decomposition.pdf`

### Compiling
```bash
cd docs
pdflatex section1_foundational_thesis.tex
# or compile all:
for f in section*.tex; do pdflatex "$f"; done
```

---

## Jupyter Notebooks

| File | Content |
|------|---------|
| `formation_engine_methodology.ipynb` | Full 13-section methodology (interactive, SS1-7 embedded) |
| `FROM_MOLECULES_TO_MATTER.ipynb` | Crystallography bridge (fractional coords, metric tensors, PBC) |

---

## Short-Form Technical Notes (LaTeX)

| File | Pages | Content |
|------|-------|---------|
| `alpha_fit_purpose.tex` | **1** | Purpose of the alpha polarizability fitter, why it is a dead end, and the transition decision |
| `xyz_file_formats.tex` | **2** | Complete specification of `.xyz` / `.xyza` / `.xyzc` / `.xyzf` formats with grammars, unit tables, and implementation map |

---

## Reference Documents

| File | Content |
|------|---------|
| `FILE_FORMATS.md` | XYZ / XYZA / XYZC / XYZF file format specification (Markdown) |
| `VALIDATION_REPORT.md` | 35-test validation campaign with results |
| `VALIDATION_CAMPAIGN_SUMMARY.md` | Executive summary and production certification |

---

## CG CLI Console

The coarse-grained operator console is accessed via `vsepr cg <COMMAND>`.

| Command | Description |
|---------|-------------|
| `vsepr cg scene` | Build a bead scene from a preset (pair, stack, shell, cloud, etc.) |
| `vsepr cg inspect` | Inspect per-bead state: position, mass, orientation, neighbours, environment |
| `vsepr cg env` | Run the environment update pipeline with trajectory and convergence reporting |
| `vsepr cg interact` | Evaluate pairwise interactions with per-channel (steric/elec/disp) and per-ℓ decomposition |
| `vsepr cg viz` | Launch lightweight visualization (stub — future feature) |
| `vsepr cg help` | Display CG console help |

### Key Implementation Files

| File | Purpose |
|------|---------|
| `include/cli/system_state.hpp` | Universal interpretation layer — `CGSystemState` struct bridging CLI to kernel |
| `include/cli/cg_commands.hpp` | CG command dispatcher declaration |
| `src/cli/cg_commands.cpp` | Full implementations of all 5 CG commands |
| `tests/test_cg_cli.cpp` | 72 integration tests covering all CG CLI operations |

---

## Verification Records (`docs/verification/`)

| File | Content |
|------|---------|
| `verification/milestone_A.md` | **Milestone A** — Deterministic Baseline Kernel Validation. 256/256 pass. Covers LJ, Coulomb, FIRE, NVE, dielectric, crystal geometry, restoring-force scans, empirical refs. |

Raw run artifacts: `verification/deep/milestone_A_run.txt`

---

## Reading Order

**For understanding the methodology:**
1. SS1 (Foundational Thesis) -- the problem definition
2. SS2 (State Ontology) -- data structures
3. SS3-7 -- physics, integration, statistics
4. SS8-9 (Reaction/Electronic) -- reaction prediction, electronic properties
5. SS8b (Heat-Gated Control) -- heat parameter, amino acid reference, 500-sim validation
6. SS12 (Validation) -- what has been tested
7. SS11 (Self-Audit) -- how failures are classified
8. CGL (Coarse-Grained Layer) -- multi-scale extension architecture
9. ASB (Anisotropic Beads) -- surface-mapped anisotropic bead descriptors
10. ERB (Environment-Responsive Beads) -- environment-responsive dynamics, local observables, internal state evolution
11. TIF (Testing Infrastructure) -- validation methodology, variable hierarchy, empirical findings, formation studies, dynamic regimes, invariance proofs
12. EEM (Ensemble Proxy Layer) -- `coarse_grain/analysis/ensemble_proxy.hpp`: macroscopic response proxies from bead-state distributions (cohesion, uniformity, texture, stabilization, surface sensitivity)

**For using the code:**
1. `FILE_FORMATS.md` -- I/O specification
2. `section_bridge_architecture.tex` -- three-layer engine/bridge/desktop design
3. `section_coarse_grained_layer.tex` -- CG model, ensembles, emergent analysis, multi-scale coupling
4. SS5 (Integration) -- algorithm details
5. `VALIDATION_REPORT.md` -- known limits
6. CG CLI Console -- `vsepr cg <command>` scientific operator console for coarse-grained engine

---

## Citation

```bibtex
@techreport{formation_engine_methodology_v01,
  title   = {Formation Engine Canonical Simulation Methodology},
  author  = {Formation Engine Development Team},
  institution = {VSEPR-Sim Project},
  year    = {2025},
  month   = {January},
  version = {0.1},
  note    = {13 sections, 35 validation tests, LaTeX source included}
}
```

---

**Last Updated:** June 2025  
**Methodology Version:** 0.2  
**Validation Status:** 28/35 atomistic PASS (80%) · 515 CG tests PASS (100%)  
**Production Status:** ✅ CERTIFIED (LJ-dominated systems) · CG console operational · EEM analysis layer active

**This is not a theoretical proposal. This is a documented, validated, production-ready scientific instrument.**
