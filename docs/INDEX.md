# VSEPR-SIM Documentation Index
## Version 3.0.0

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
| `section10b_defect_microstate_classification.tex` | **SS10b** | Defect microstate classification |
| `section11_self_audit.tex` | **SS11** | Failure classification, gap targeting, regression detection |
| `section_bridge_architecture.tex` | **SBA** | Canonical structural bridge: three-layer architecture, EngineAdapter, conversion pipeline |
| `section_phase_reports.tex` | **PHR** | Live phased revalidation reports (Phase 1+), pass/fail audit results |
| `section_coarse_grained_layer.tex` | **CGL** | Coarse-grained and multi-scale layer: CG model construction, statistical mechanics, emergent behavior, multi-scale coupling, validation, scale transition, thermostats, free energy, emergent effective medium mapping, macro property precursor channels, property learning pipeline, property calibration and target legitimacy |
| `section_anisotropic_beads.tex` | **ASB** | Anisotropic surface-mapped beads: spherical harmonic descriptors, inertia frames, probe-based sampling, orientation-coupled interactions, torques, multi-channel descriptors, adaptive complexity, unified descriptor strategy, atomistic preparation layer, formal bead state decomposition |
| `section_environment_responsive_beads.tex` | **ERB** | Environment-responsive coarse-grained bead dynamics: extended bead state, fast environment observables, slow internal state variable, coupling mechanisms, emergent behaviour |
| `section_testing_infrastructure.tex` | **TIF** | Validation infrastructure: testing philosophy, scene construction, variable hierarchy, test suite architecture (1013 tests), behavioural assertion framework, trajectory runner, empirical findings |
| `section_database_architecture.tex` | **DBA** | Database architecture and data management |
| `section_layer_stack.tex` | **LST** | Layer stack architecture |
| `section_polarization_models.tex` | **POL** | Polarization models |
| `section_seed_bead_model.tex` | **SBM** | Seed bead model |
| `section_fuzzy_ball_model.tex` | **FBM** | Fuzzy ball model |
| `section_petalized_compute_allocation.tex` | **PCA** | Petalized compute allocation |
| `section_32bit_hourglass_lookglass.tex` | **HGL** | 32-bit hourglass/lookglass architecture |
| `ALPHA_MODEL_BOOKLET.tex` | **AMB** | Alpha model booklet — consolidated atomistic model reference |

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
| `xyz_file_formats.tex` | **2** | Complete specification of `.xyz` / `.xyza` / `.xyzc` / `.xyzf` formats with grammars, unit tables, and implementation map |

---

## Reference Documents

| File | Content |
|------|---------|
| `FILE_FORMATS.md` | XYZ / XYZA / XYZC / XYZF file format specification (Markdown) |
| `VALIDATION_REPORT.md` | 35-test validation campaign with results |
| `BEAD_FIRE_REFERENCE.md` | FIRE algorithm reference for bead-level relaxation |
| `cg_mapping_report.md` | Coarse-grained mapping report |

---

## CLI Commands

The unified CLI is accessed via `vsepr <COMMAND>`. See `CLI_WALKTHROUGH.txt` for a full step-by-step guide.

| Command | Description |
|---------|-------------|
| `vsepr build` | Build molecules from chemical formulas |
| `vsepr viz` | Interactive atomistic visualization |
| `vsepr therm` | Analyze thermal properties, bonding, and evolution |
| `vsepr cg <cmd>` | Coarse-grained console (scene, inspect, env, interact, viz, help) |
| `vsepr help` | Display help information |
| `vsepr version` | Show version and component information |

---

## CG CLI Console

The coarse-grained operator console is accessed via `vsepr cg <COMMAND>`.

| Command | Description |
|---------|-------------|
| `vsepr cg scene` | Build a bead scene from a preset (pair, stack, shell, cloud, etc.) |
| `vsepr cg inspect` | Inspect per-bead state: position, mass, orientation, neighbours, environment |
| `vsepr cg env` | Run the environment update pipeline with trajectory and convergence reporting |
| `vsepr cg interact` | Evaluate pairwise interactions with per-channel (steric/elec/disp) and per-ℓ decomposition |
| `vsepr cg viz` | Launch lightweight visualization (requires BUILD_VIS=ON) |
| `vsepr cg help` | Display CG console help |

### Key Implementation Files

| File | Purpose |
|------|---------|
| `include/cli/system_state.hpp` | Universal interpretation layer — `CGSystemState` struct bridging CLI to kernel |
| `include/cli/cg_commands.hpp` | CG command dispatcher declaration |
| `src/cli/cg_commands.cpp` | Full implementations of all 5 CG commands |
| `tests/test_cg_cli.cpp` | 72 integration tests covering all CG CLI operations |

---

## PyKernel Modules

The Python kernel (`pykernel/`) provides scientific computation, automation, and batch processing.

### Core Infrastructure

| Module | Content |
|--------|---------|
| `pykernel/poly_fitter.py` | Polynomial fitting engine |
| `pykernel/eigen_counter.py` | Eigenvalue counting and spectral analysis |
| `pykernel/gpu_bridge.py` | GPU/CPU bridge with automatic fallback |
| `pykernel/runner.py` | Walk-away improvement runner |
| `pykernel/improvement_loop.py` | Autonomous improvement loop orchestration |
| `pykernel/pipe.py` | Pipe[T], Transform[T,U], FanOut, Accumulator, CSVSink, JSONSink pipeline infrastructure |
| `pykernel/test_forest.py` | Test forest — structured test discovery and execution |
| `pykernel/benchmark.py` | GPU vs CPU benchmarking harness |

### Visualization and TUI

| Module | Content |
|--------|---------|
| `pykernel/live_viewer.py` | Live xyzA viewer orchestrator (Python side) |
| `pykernel/crystal_tui.py` | Crystal lattice TUI — ANSI terminal renderer with lattice projection, force arrows, wind overlay, and math panel |

### Thermal and Materials

| Module | Content |
|--------|---------|
| `pykernel/metallic_cp.py` | Debye model + Sommerfeld electronic + Nernst-Lindemann Cp-Cv, 30-metal empirical database |
| `pykernel/heating_sim.py` | Time-stepping thermal evolution — `HeatingSimulation`, `HeatSchedule` (constant/ramp/pulse/custom), per-part energy tracking |
| `pykernel/step_parser.py` | SolidWorks STEP (ISO 10303-21) file parser — AP203/AP214, named PRODUCT extraction |
| `pykernel/thermo_pipe.py` | Batch thermal pipeline — `ThermoRunner`, `ThermoValidator`, `XLSXSink`, `BatchJob` orchestration |

### C++ Atomistic Components

| File | Content |
|------|---------|
| `atomistic/models/wind_particle.hpp` | Wind particle force field — directional force with Gaussian envelope, ramp schedule, F_max clamp |
| `atomistic/tui/crystal_tui.hpp` + `.cpp` | C++ crystal lattice TUI renderer |

---

## Visualization

| File | Content |
|------|---------|
| `docs/vis/ARCHITECTURE_DIAGRAM.md` | Visualization architecture overview |
| `docs/vis/BALLSTICK_GUIDE.md` | Ball-and-stick rendering guide |
| `docs/vis/RENDERER_FEATURES.md` | Renderer feature list |
| `docs/vis/INTERACTIVE_FEATURES.md` | Interactive features documentation |
| `docs/vis/INTERACTIVE_UI_GUIDE.md` | Interactive UI guide |
| `docs/vis/INTERACTIVE_SUMMARY.md` | Interactive feature summary |
| `docs/vis/IMPLEMENTATION_COMPLETE.md` | Implementation completion status |
| `docs/vis/QUICK_REFERENCE.md` | Quick reference for visualization |

---

## Verification Records (`docs/verification/`)

| File | Content |
|------|---------|
| `verification/milestone_A.md` | **Milestone A** — Deterministic Baseline Kernel Validation. 256/256 pass. Covers LJ, Coulomb, FIRE, NVE, dielectric, crystal geometry, restoring-force scans, empirical refs. |

Raw run artifacts: `verification/deep/milestone_A_run.txt`

---

## Test Coverage

| Suite | Tests | Status |
|-------|-------|--------|
| C++ kernel (Milestone A) | 256 | ✅ PASS |
| C++ CG CLI | 72 | ✅ PASS |
| C++ Wind/TUI | 8 | ✅ PASS |
| Python crystal TUI | 37 | ✅ PASS |
| Python STEP parser | 24 | ✅ PASS |
| Python metallic c_p | 52 | ✅ PASS |
| Python heating sim | 20 | ✅ PASS |
| Python thermo pipe | 27 | ✅ PASS |
| Python prior (pipe, benchmark, etc.) | 155 | ✅ PASS |

---

## Reading Order

**For understanding the methodology:**
1. SS1 (Foundational Thesis) — the problem definition
2. SS2 (State Ontology) — data structures
3. SS3-7 — physics, integration, statistics
4. SS8-9 (Reaction/Electronic) — reaction prediction, electronic properties
5. SS8b (Heat-Gated Control) — heat parameter, amino acid reference, 500-sim validation
6. SS12 (Validation) — what has been tested
7. SS11 (Self-Audit) — how failures are classified
8. CGL (Coarse-Grained Layer) — multi-scale extension architecture
9. ASB (Anisotropic Beads) — surface-mapped anisotropic bead descriptors
10. ERB (Environment-Responsive Beads) — environment-responsive dynamics, local observables, internal state evolution
11. TIF (Testing Infrastructure) — validation methodology, variable hierarchy, empirical findings

**For using the code:**
1. `CLI_WALKTHROUGH.txt` — step-by-step CLI walkthrough
2. `FILE_FORMATS.md` — I/O specification
3. `section_bridge_architecture.tex` — three-layer engine/bridge/desktop design
4. SS5 (Integration) — algorithm details
5. `VALIDATION_REPORT.md` — known limits
6. CG CLI Console — `vsepr cg <command>` scientific operator console

---

## Citation

```bibtex
@techreport{vsepr_sim_v3,
  title   = {VSEPR-SIM: Atomistic Simulation and Analysis Platform},
  author  = {VSEPR-SIM Development Team},
  institution = {VSEPR-SIM Project},
  year    = {2025},
  version = {3.0.0},
  note    = {Deterministic atomistic structure generation, thermal analysis, SolidWorks STEP import, batch pipelines}
}
```

---

**Last Updated:** June 2025  
**Version:** 3.0.0  
**Validation Status:** 256/256 kernel PASS · 72 CG CLI PASS · 315 Python tests PASS  
**Production Status:** ✅ CERTIFIED — Atomistic kernel operational · CG console operational · Thermal pipeline operational · STEP import operational

**This is not a theoretical proposal. This is a documented, validated, research-oriented scientific instrument.**
