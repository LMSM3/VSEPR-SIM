# Formation Engine Methodology
## Documentation Index

---

## LaTeX Methodology (Primary Documents)

The core scientific contribution lives in these TeX source files.
Each is self-contained and PDF-compilable.

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
| `section10_12_13_closing.tex` | **SS10,12,13** | Multiscale projection, validation doctrine (35 tests), future work |
| `section11_self_audit.tex` | **SS11** | Failure classification, gap targeting, regression detection |

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
| `FILE_FORMATS.md` | XYZ / XYZA / XYZC / XYZF file format specification |
| `VALIDATION_REPORT.md` | 35-test validation campaign with results |
| `VALIDATION_CAMPAIGN_SUMMARY.md` | Executive summary and production certification |

---

## Reading Order

**For understanding the methodology:**
1. SS1 (Foundational Thesis) -- the problem definition
2. SS2 (State Ontology) -- data structures
3. SS3-7 -- physics, integration, statistics
4. SS12 (Validation) -- what has been tested
5. SS11 (Self-Audit) -- how failures are classified

**For using the code:**
1. `FILE_FORMATS.md` -- I/O specification
2. SS5 (Integration) -- algorithm details
3. `VALIDATION_REPORT.md` -- known limits

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
