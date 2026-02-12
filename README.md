# VSEPR-Sim: Molecular Simulation & Visualization

**Version:** 2.5.0  
**Focus:** Atomistic formation engine with formal methodology  
**License:** MIT

---

## Overview

VSEPR-Sim is a classical atomistic simulation engine for molecular formation,
energy minimization, and molecular dynamics. It ships with a complete
**13-section LaTeX methodology** documenting every equation, algorithm,
and design decision.

**Key capabilities:**
- Lennard-Jones + Coulomb + bonded force fields (UFF parameterization)
- Velocity Verlet (NVE), Langevin (NVT), FIRE minimization
- Periodic boundary conditions
- XYZ / XYZA / XYZC file format pipeline with hash-based provenance
- Self-audit infrastructure (failure classification, gap targeting, regression detection)
- Interactive CLI (`meso-build`) and 3D viewer (OpenGL)

---

## Methodology (LaTeX)

The scientific foundation is in `docs/`. Each file compiles standalone with `pdflatex`.

| File | Content |
|------|---------|
| `section0_identity_state_decomposition.tex` | Particle identity vectors, cell/world container ontology |
| `section1_foundational_thesis.tex` | Formation problem, scope, domain of validity |
| `section2_state_ontology.tex` | State tuple, energy ledger, file hierarchy |
| `section3_interaction_model.tex` | LJ, Coulomb, UFF, switching, PBC |
| `section4_thermodynamics.tex` | Unit system, temperature, pressure, heat capacity |
| `section5_integration.tex` | Verlet, Langevin, FIRE algorithms |
| `section6_formation_physics.tex` | Bonded terms, formation pipeline, basin mapping |
| `section7_statistical_interpretation.tex` | Welford, stationarity, Kabsch, scoring |
| `section8_9_reaction_electronic.tex` | QEq, Fukui functions, HSAB, reaction templates |
| `section10_12_13_closing.tex` | Multiscale, validation doctrine (35 tests), roadmap |
| `section11_self_audit.tex` | Failure classifier, gap targeter, regression detector |

```bash
cd docs && for f in section*.tex; do pdflatex "$f"; done
```

See [`docs/INDEX.md`](docs/INDEX.md) for the full documentation index.

---

## Quick Start

### Build (WSL / Linux)
```bash
chmod +x vseprw
./vseprw H2O relax          # configure + build + run (first time)
./vseprw water.xyz sim --temp 300
```

### Manual Build
```bash
cmake -B build && cmake --build build -j8
./build/meso-build           # interactive CLI
./build/interactive-viewer molecule.xyz
```

### Create a Molecule
```bash
./build/meso-build
build cisplatin
info
save cisplatin.xyz
exit
```

---

## Repository Structure

```
vsepr-sim/
  src/           C++ source (core, sim, pot, box, io, cli, gpu, ...)
  include/       Public headers
  apps/          Application entry points (meso-build, meso-sim, viewer, ...)
  tests/         Validation suite (56 test files, C++ and shell)
  docs/          LaTeX methodology (11 .tex files) + notebooks + specs
  examples/      Example molecules (.xyz) and demo programs
  tools/         Self-audit Python tools (failure_classifier, gap_targeter, ...)
  atomistic/     Atomistic module (crystal, predict, state)
  third_party/   ImGui (vendored)
  data/          Reference geometries and parameters
  scripts/       Build and utility scripts
  cmake/         CMake modules
  resources/     Icons and assets
  installer/     Packaging (WiX)
  EXPORT/        Archived development artifacts (not distributed)
```

---

## File Formats

| Format | Extension | Content |
|--------|-----------|---------|
| **XYZ** | `.xyz` | Static geometry (element + Cartesian coordinates in Angstrom) |
| **XYZA** | `.xyza` | Animated trajectory (sequential XYZ frames) |
| **XYZC** | `.xyzc` | Checkpointed MD (positions + velocities + thermodynamic state) |

Full specification: [`docs/FILE_FORMATS.md`](docs/FILE_FORMATS.md)

---

## System Requirements

- **Compiler:** GCC 7+, Clang 10+, or MSVC 2019+ (C++20)
- **CMake:** 3.15+
- **Graphics:** OpenGL 3.3+, GLFW, GLEW (for visualization targets)
- **Optional:** CUDA toolkit (GPU acceleration, graceful fallback)

---

## Design Principles

1. **No fake physics** -- every parameter has a literature origin
2. **Determinism** -- same inputs produce identical output
3. **Transparency** -- no hidden constants or magic numbers
4. **Honest labeling** -- all approximations documented
5. **Extension without replacement** -- new features layer on validated core

---

## License

MIT License. See [LICENSE](LICENSE).