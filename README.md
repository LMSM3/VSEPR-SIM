<div align="center">

# VSEPR-Sim  
**Classical Atomistic Formation Engine**

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://isocpp.org/)
[![CMake](https://img.shields.io/badge/CMake-3.15+-blue.svg)](https://cmake.org/)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![Tests](https://img.shields.io/badge/Tests-28%2F35_PASS-yellow.svg)](docs/VALIDATION_REPORT.md)
[![Docs](https://img.shields.io/badge/Methodology-186_pages-blue.svg)](docs/INDEX.md)

**Deterministic structure generation from elemental identity + thermodynamic boundary conditions**

[Documentation](docs/INDEX.md) • [Methodology](docs/METHODOLOGY_12PAGE.tex) • [Validation](docs/VALIDATION_REPORT.md) • [Quick Start](#quick-start)

</div>

---

## Start Here

| Where you want to go | Jump to |
|----------------------|---------|
| Understand the build system and folder layout | [TOUR.md](TOUR.md) |
| Day-to-day shell — browse, edit, compile, export | [scripts/doc_shell.py](scripts/doc_shell.py) — run `python scripts/doc_shell.py` |
| Shell command reference | [scripts/README.md](scripts/README.md) |
| Generate or compile reports | [reporting/README.md](reporting/README.md) |
| Formal 186-page methodology | [docs/ALPHA_MODEL_BOOKLET.tex](docs/ALPHA_MODEL_BOOKLET.tex) |
| Validation report | [docs/VALIDATION_REPORT.md](docs/VALIDATION_REPORT.md) |
| Python physics kernel | [pykernel/](pykernel/) |

```powershell
# Activate the Python environment (once per session)
.venv\Scripts\Activate.ps1

# Open the interactive maintenance shell
python scripts\doc_shell.py

# Or build the C++ engine directly
cmake -B build -S . && cmake --build build --parallel
```

---


---

## Overview

Long-term development targets reaction modeling and structured multiscale coupling while preserving reproducibility and auditability. Long-term development targets expanding reaction modeling, and creating multiscale coupling while preserving reproducibility and auditability.

**Included in this release:**
- **Formal methodology**: 13 LaTeX sections (186 pages) documenting every equation, parameter, and design decision
- **Validation-first**: 35 hierarchical tests across 5 levels (80% pass rate)
- **Production certified**: Approved for noble gases, hydrocarbons, and small organic molecules
- **Deterministic by design**: Same inputs → bit-identical output across platforms
- **Hash-based provenance**: Every structure carries full generation history (SHA-256)
- **Self-audit infrastructure**: Autonomous failure classification, gap targeting, regression detection

> **Guiding principle:**  
> *Every state is reproducible. Every result is traceable. Structure is a primary simulation output, not an input assumption.*

---

## 3D Output Gallery

Deterministic atomistic structures generated and rendered by the VSEPR-Sim engine. All outputs use CPK coloring, Phong shading, and automatic bond detection from covalent radii.

### Ball-and-Stick — VSEPR Geometries

<table>
<tr>
<td align="center"><img src="assets/images/ballstick_h2o.png" width="380"/><br/><b>H₂O — Water</b><br/>Bent · 104.5° bond angle</td>
<td align="center"><img src="assets/images/ballstick_ch4.png" width="380"/><br/><b>CH₄ — Methane</b><br/>Tetrahedral · 109.5° bond angles</td>
</tr>
<tr>
<td align="center"><img src="assets/images/ballstick_nh3.png" width="380"/><br/><b>NH₃ — Ammonia</b><br/>Trigonal pyramidal · lone pair compression</td>
<td align="center"><img src="assets/images/ballstick_sf6.png" width="380"/><br/><b>SF₆ — Sulfur Hexafluoride</b><br/>Octahedral · 6 equivalent bonds</td>
</tr>
<tr>
<td align="center"><img src="assets/images/ballstick_pf5.png" width="380"/><br/><b>PF₅ — Phosphorus Pentafluoride</b><br/>Trigonal bipyramidal · axial/equatorial</td>
<td align="center"><img src="assets/images/ballstick_xef4.png" width="380"/><br/><b>XeF₄ — Xenon Tetrafluoride</b><br/>Square planar · 2 lone pairs</td>
</tr>
</table>

### Ball-and-Stick — Organic & Larger Structures

<table>
<tr>
<td align="center"><img src="assets/images/ballstick_benzene.png" width="380"/><br/><b>C₆H₆ — Benzene</b><br/>Planar hexagonal ring · 12 atoms</td>
<td align="center"><img src="assets/images/ballstick_ethanol.png" width="380"/><br/><b>C₂H₅OH — Ethanol</b><br/>Multi-center organic · 9 atoms</td>
</tr>
<tr>
<td align="center"><img src="assets/images/ballstick_hexane.png" width="380"/><br/><b>C₆H₁₄ — Hexane</b><br/>Linear alkane chain · 20 atoms</td>
<td align="center"><img src="assets/images/ballstick_h2so4.png" width="380"/><br/><b>H₂SO₄ — Sulfuric Acid</b><br/>Tetrahedral S center · strong acid</td>
</tr>
<tr>
<td align="center"><img src="assets/images/ballstick_ikaite.png" width="380"/><br/><b>CaCO₃·6H₂O — Ikaite</b><br/>Hydrated mineral · 20 atoms</td>
<td align="center"><img src="assets/images/ballstick_ar13.png" width="380"/><br/><b>Ar₁₃ — Argon Cluster</b><br/>Icosahedral noble gas · Lennard-Jones</td>
</tr>
</table>

### Space-Filling (Van der Waals)

<table>
<tr>
<td align="center"><img src="assets/images/spacefill_h2o.png" width="380"/><br/><b>H₂O</b><br/>VdW radii · no bonds</td>
<td align="center"><img src="assets/images/spacefill_benzene.png" width="380"/><br/><b>C₆H₆</b><br/>Electron cloud overlap visible</td>
</tr>
<tr>
<td align="center"><img src="assets/images/spacefill_h2so4.png" width="380"/><br/><b>H₂SO₄</b><br/>Sulfur buried by oxygen shells</td>
<td align="center"><img src="assets/images/spacefill_ar13.png" width="380"/><br/><b>Ar₁₃</b><br/>Close-packed noble gas cluster</td>
</tr>
</table>

### Wireframe

<table>
<tr>
<td align="center"><img src="assets/images/wireframe_benzene.png" width="380"/><br/><b>C₆H₆ — Benzene</b><br/>Bond topology · split CPK coloring</td>
<td align="center"><img src="assets/images/wireframe_hexane.png" width="380"/><br/><b>C₆H₁₄ — Hexane</b><br/>Carbon backbone visible</td>
</tr>
</table>

### Render Style Comparison

Same molecule rendered in all three styles side-by-side.

<div align="center">
<img src="assets/images/multiview_ch4.png" width="760"/>
<br/>
<i>CH₄ — Ball-and-Stick · Space-Filling · Wireframe</i>
</div>

<br/>

<div align="center">
<img src="assets/images/multiview_benzene.png" width="760"/>
<br/>
<i>C₆H₆ — Ball-and-Stick · Space-Filling · Wireframe</i>
</div>

### PBC Supercell Visualization

<div align="center">
<img src="assets/images/pbc_h2o_supercell.png" width="600"/>
<br/>
<i>H₂O 3×3×3 supercell — central cell at full opacity, ghost replicas at 25%, PBC box edges in blue</i>
</div>

### Simulation Diagnostics

<table>
<tr>
<td align="center"><img src="assets/images/fire_convergence.png" width="380"/><br/><b>FIRE Minimization</b><br/>Energy + RMS force convergence · adaptive dt restarts</td>
<td align="center"><img src="assets/images/md_timeseries.png" width="380"/><br/><b>NVT Langevin MD</b><br/>Temperature, KE, PE time series · 300 K target</td>
</tr>
</table>

### Desktop Workstation

<div align="center">
<img src="assets/images/desktop_workstation.png" width="760"/>
<br/>
<i>Qt-based molecular workstation — 3D viewport, object tree, properties inspector, and interactive console</i>
</div>

---

## Key Capabilities

### Physics Engine
- Lennard-Jones 12-6 + Coulomb + bonded MM (UFF parameterization)
- Velocity Verlet (NVE), Langevin (NVT), FIRE minimization
- Periodic boundary conditions (orthogonal boxes)
- Supercell replication with bond re-inference
- Explicit units throughout (Å, fs, kcal/mol, amu, K)

### File Format Pipeline
```
.xyz  → Static geometry (element + Cartesian coordinates)
.xyza → Animated trajectory (sequential frames)
.xyzc → Checkpointed MD (positions + velocities + thermodynamics + SHA-256 hash)
```

### Tools
- **Interactive CLI**: Molecular construction and manipulation
- **Simulation engine**: MD/minimization with FIRE, Verlet, and Langevin integrators
- **OpenGL viewer**: 3D visualization with atom tooltips
- **Self-audit suite**: Python tools for failure analysis and regression detection

---

## Methodology (LaTeX)

The scientific foundation lives in **11 standalone LaTeX files** (186 pages total).

### Condensed Versions
| Document | Pages | Purpose |
|----------|-------|---------|
| [`METHODOLOGY_2PAGE.tex`](docs/METHODOLOGY_2PAGE.tex) | 2 | Conference handout (two-column summary) |
| [`METHODOLOGY_12PAGE.tex`](docs/METHODOLOGY_12PAGE.tex) | 12 | Quick reference with equations |

### Full Sections
| File | Sections | Content |
|------|----------|---------|
| [`section0_identity_state_decomposition.tex`](docs/section0_identity_state_decomposition.tex) | §0 | Particle identity vectors, cell/world container ontology |
| [`section1_foundational_thesis.tex`](docs/section1_foundational_thesis.tex) | §1 | Problem definition, scope, domain of validity |
| [`section2_state_ontology.tex`](docs/section2_state_ontology.tex) | §2 | State tuple, identity/phase/scratch partitioning |
| [`section3_interaction_model.tex`](docs/section3_interaction_model.tex) | §3 | LJ, Coulomb, UFF, switching, PBC |
| [`section4_thermodynamics.tex`](docs/section4_thermodynamics.tex) | §4 | Unit system, temperature, pressure, heat capacity |
| [`section5_integration.tex`](docs/section5_integration.tex) | §5 | Verlet, Langevin, FIRE algorithms |
| [`section6_formation_physics.tex`](docs/section6_formation_physics.tex) | §6 | Bonded terms, formation pipeline, basin mapping |
| [`section7_statistical_interpretation.tex`](docs/section7_statistical_interpretation.tex) | §7 | Welford, stationarity, Kabsch, scoring |
| [`section8_9_reaction_electronic.tex`](docs/section8_9_reaction_electronic.tex) | §8-9 | QEq, Fukui functions, HSAB, reaction templates |
| [`section10_12_13_closing.tex`](docs/section10_12_13_closing.tex) | §10,12,13 | Multiscale, validation doctrine (35 tests), roadmap |
| [`section11_self_audit.tex`](docs/section11_self_audit.tex) | §11 | Failure classifier, gap targeter, regression detector |

**Compile with:**
```bash
cd docs && for f in section*.tex; do pdflatex "$f"; done
```

**Full index:** [`docs/INDEX.md`](docs/INDEX.md)

---

## Quick Start

### Automated Build & Test (Recommended)

**Linux/macOS/WSL:**
```bash
./build_automated.sh                  # Build + run all tests
./build_automated.sh --clean          # Clean build + tests
./build_automated.sh --heat-only      # Test heat-gated reactions only (Item #7)
```

**Windows:**
```cmd
build_automated.bat                   # Build + run all tests
build_automated.bat --clean           # Clean build + tests
build_automated.bat --heat-only       # Test heat-gated reactions only (Item #7)
```

### WSL / Linux (Interactive CLI)
```bash
chmod +x vseprw
./vseprw H2O relax                    # First run: configure + build + minimize
./vseprw molecule.xyz sim --temp 300  # MD simulation at 300 K
./vseprw molecule.xyz view            # 3D visualization
```

### Manual Build
```bash
cmake -B build && cmake --build build -j8

# Interactive molecular builder (formula pipeline)
./build/atomistic-build
>> build H2O
>> info
>> save H2O.xyz
>> exit

# Energy minimization
./build/atomistic-relax H2O.xyz

# Molecular dynamics
./build/atomistic-sim simulate H2O.xyz --temp 300 --steps 10000

# 3D viewer
./build/interactive-viewer H2O.xyz
```

---

## Repository Structure

```
vsepr-sim/
├── src/              Core C++ engine (170 files)
│   ├── core/         State, force evaluation, energy ledger
│   ├── sim/          Integrators (Verlet, Langevin, FIRE)
│   ├── pot/          Potentials (LJ, Coulomb, bonded MM)
│   ├── box/          Periodic boundaries
│   ├── io/           XYZ/XYZA/XYZC parsers
│   └── cli/          Command-line interface
├── include/          Public headers (51 files)
├── apps/             Entry points (35 applications)
│   ├── cli.cpp           # Interactive builder
│   ├── relax.cpp         # Energy minimization
│   ├── simulate.cpp      # MD engine
│   └── viewer.cpp        # OpenGL visualization
├── tests/            Validation suite (56 files)
│   ├── energy_tests.cpp
│   ├── ensemble_consistency_test.cpp
│   └── basic_molecule_validation.cpp
├── docs/             LaTeX methodology + notebooks
│   ├── section*.tex  (11 files, 186 pages)
│   ├── METHODOLOGY_2PAGE.tex
│   ├── METHODOLOGY_12PAGE.tex
│   └── VALIDATION_REPORT.md
├── tools/            Self-audit Python scripts
│   ├── failure_classifier.py
│   ├── gap_targeter.py
│   └── regression_detector.py
├── data/             Reference geometries
├── examples/         Demo molecules (.xyz)
└── third_party/      ImGui (vendored)
```

**✅ Approved for:**
- Noble gas systems (Ar, Xe, Kr)
- Hydrocarbon molecules (CH₄, benzene, alkanes)
- Small organics (H₂O, NH₃, CH₃OH)
- Molecular clusters

**❌ Known limitations:**
- Ionic MD (NaCl, MgO) — Use FIRE only until Coulomb-integrator coupling is fixed
- Transition metal complexes — Classical approximation insufficient

**Full report:** [`docs/VALIDATION_REPORT.md`](docs/VALIDATION_REPORT.md)

---

## Design Principles

1. **Explicit units everywhere** — Positions in Å, energies in kcal/mol. No reduced units.
2. **No silent domain switching** — Force field, integrator, and BC declared upfront.
3. **Deterministic core** — Identical inputs produce bit-identical outputs (seeded RNG).
4. **Periodic table as sole authority** — No molecular databases. Parameters from UFF.
5. **Extension without replacement** — Validated core remains intact.

---

## System Requirements

- **Compiler:** GCC 7+, Clang 10+, or MSVC 2019+ (C++20 support)
- **Build System:** CMake 3.15+
- **Graphics (optional):** OpenGL 3.3+, GLFW, GLEW
- **GPU (optional):** CUDA toolkit (graceful CPU fallback)
- **OS:** Linux, Windows (WSL recommended), macOS

---

## License

MIT License — see [LICENSE](LICENSE) for details.

---

## Acknowledgments

- Universal Force Field (UFF) parameterization: Rappé et al., *J. Am. Chem. Soc.* **114**, 10024 (1992)
- FIRE minimization: Bitzek et al., *Phys. Rev. Lett.* **97**, 170201 (2006)
- Kabsch alignment: Kabsch, *Acta Cryst.* **A32**, 922 (1976)
- ImGui: Omar Cornut et al. (vendored under MIT license)

---

<div align="center">

**[Documentation](docs/INDEX.md) • [Methodology](docs/METHODOLOGY_12PAGE.tex) • [Validation](docs/VALIDATION_REPORT.md) • [GitHub](https://github.com/LMSM3/VSEPR-SIM)**

*This is not a theoretical proposal. This is a documented, validated, production-ready scientific instrument.*

</div>
