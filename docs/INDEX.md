# Formation Engine Methodology
## Documentation Index

---

## Formation Engine Paper (Primary)

| File | Pages | Content |
|------|-------|---------|
| **`formation_engine_paper.tex`** | **56** | Full academic paper: scientific motivation, formation kernel as computational infrastructure, historical development, system architecture (formation kernel boundary, 6-layer pipeline, State, IModel, CompositeModel, EnvironmentContext, physics registry, 3-layer desktop bridge), energy model (LJ, Coulomb, bonded, torsion, switching, 1-2 exclusions, dielectric), structural systems (bond detection, crystal emitters, supercells), numerical methods (FIRE, velocity Verlet, Langevin, Maxwell–Boltzmann, scaling analysis), verification framework, deep verification suite (16 suites, 256 checks), verification results (485/485 PASS), **kernel-generated molecular structures** (H₂O, NH₃, CH₄, SF₆, XeF₄, PF₅, H₂SO₄, hexane, ikaite — TikZ 3D ball-and-stick figures from verbatim XYZ output), **crystal structure sessions** (FCC Al 108, BCC Fe 128, NaCl 216, Si 512 atoms — verbatim reports + TikZ unit cells), **raw numerical output** (H₂ sweep, H₂O stretch, Suite A/D/G/M verbatim), bugs, limitations (capability envelope table), **downstream workflows enabled**, future work, conclusion (scientific implications), references, 5 appendices. 20+ TikZ/pgfplots figures. |

Compile: `cd docs && pdflatex formation_engine_paper.tex` (run 3× for ToC + cross-refs)

## Deep Verification Paper (Technical Reference)

| File | Pages | Content |
|------|-------|---------|
| **`deep_verification_paper.tex`** | **18** | Compact verification-focused paper: architecture, energy model, numerical methods, 256-check verification suite, demonstration cases, FIRE convergence tables, NVE drift, empirical cross-checks, UFF parameter appendix |

Compile: `cd docs && pdflatex deep_verification_paper.tex` (run twice for ToC)

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

## Reference Documents

| File | Content |
|------|---------|
| **`USAGE.md`** | **Complete usage reference** — all CLI executables, desktop console commands, keyboard shortcuts, file formats, exit codes |
| `FILE_FORMATS.md` | XYZ / XYZA / XYZC / XYZF file format specification |
| `VALIDATION_REPORT.md` | 35-test validation campaign with results |
| `VALIDATION_CAMPAIGN_SUMMARY.md` | Executive summary and production certification |

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

**For using the code:**
1. **`USAGE.md`** — CLI and desktop command reference (start here)
2. `FILE_FORMATS.md` — I/O specification
3. `section_bridge_architecture.tex` — three-layer engine/bridge/desktop design
4. SS5 (Integration) — algorithm details
5. `VALIDATION_REPORT.md` — known limits

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

**Last Updated:** January 17, 2025  
**Methodology Version:** 0.1  
**Validation Status:** 28/35 PASS (80%)  
**Production Status:** ✅ CERTIFIED (LJ-dominated systems)

**This is not a theoretical proposal. This is a documented, validated, production-ready scientific instrument.**
