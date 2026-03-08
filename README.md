<div align="center">

# VSEPR-Sim
**Classical Atomistic Formation Engine**

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://isocpp.org/)
[![CMake](https://img.shields.io/badge/CMake-3.15+-blue.svg)](https://cmake.org/)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![Verification](https://img.shields.io/badge/Verification-485%2F485_PASS-brightgreen.svg)](docs/verification/milestone_A.md)
[![Deep](https://img.shields.io/badge/Deep_Suite-256%2F256-brightgreen.svg)](apps/deep_verification.cpp)
[![Docs](https://img.shields.io/badge/Methodology-200%2B_pages-blue.svg)](docs/INDEX.md)

**Deterministic structure generation from elemental identity + thermodynamic boundary conditions**

[Documentation](docs/INDEX.md) · [Verification](docs/verification/milestone_A.md) · [Methodology](docs/METHODOLOGY_12PAGE.tex) · [Quick Start](#quick-start)

</div>

---

## Overview

VSEPR-Sim is a classical atomistic formation engine that generates molecular and crystal structures deterministically from elemental composition and thermodynamic boundary conditions. All physics is explicitly modelled — no visual-only approximations.

**v2.7.1 — Deep Verification Milestone:**

- **485 / 485 verification checks passing** across 14 phased executables + deep verification suite
- **256 / 256 deep verification checks** covering LJ, Coulomb, dielectric screening, FIRE relaxation, NVE conservation, crystal geometry, bond detection, and randomised seeded consistency
- **42 UFF elements** fully parameterised (Rappé 1992), up from 19
- **Empirical reference database**: 57 bonds, 25 angles, 30 crystals, 10 diatomics, 19 solvents, 15 ions — all with literature sources
- **Live empirical cross-check** against PubChem and NIST WebBook (25 bond lengths, 20 diatomic constants)
- **FIRE convergence to Frms < 10⁻⁵** kcal/mol/Å on all Ar₂–Ar₇ noble gas clusters
- **NVE energy drift < 3 × 10⁻¹¹** kcal/mol on velocity-Verlet Ar clusters
- **Qt6 desktop application** scaffold (MainWindow, ObjectTree, PropertiesPanel, ViewportWidget, EngineAdapter bridge)

> **Guiding principle:**
> *Every state is reproducible. Every result is traceable. Structure is a primary simulation output, not an input assumption.*

---

## Verification Status

| Suite | What it tests | Checks |
|-------|---------------|--------|
| A | LJ parameter audit — 42 UFF elements vs Rappé 1992 | 42 |
| B | Homonuclear dimer sweeps — energy curves, r_min, smoothness | 20 |
| C | Ion-pair Coulomb + force consistency | 15 |
| D | Crystal geometry vs Wyckoff (30 structures) | 90 |
| E | Molecule bond detection (9 canonical molecules) | 9 |
| F | Composite force-energy consistency | 4 |
| G | Noble gas cluster FIRE — two-layer: descent + convergence | 30 |
| H | Dielectric screening (19 CRC 104 solvents) | 19 |
| N | Restoring-force scan: He₂–Xe₂ (direction + energy minimum) | 10 |
| Q | Crystal builder fidelity — 4 presets, 2×2×2 supercell | 8 |
| I–M | Randomised seeded: LJ F-E, Coulomb 1/r, dielectric, supercell, NVE | 16 |
| P | Downloaded empirical refs (PubChem + NIST cross-check) | 53 |
| | **Total** | **256 PASS** |

Full milestone record: [`docs/verification/milestone_A.md`](docs/verification/milestone_A.md)

Reproduce the exact run:
```bash
wsl bash -c "cd wsl-build && ./deep_verification --seed 1772952709 --rand-iters 500"
```

---

## Key Capabilities

### Physics Engine
- Lennard-Jones 12-6 + Coulomb + bonded MM (UFF parameterisation, 42 elements)
- Velocity Verlet (NVE), Langevin (NVT), FIRE minimisation
- Dielectric screening with full solvent model (CRC 104 reference data)
- Periodic boundary conditions (orthogonal boxes)
- Supercell replication with bond re-inference
- Explicit units throughout (Å, fs, kcal/mol, amu, K)

### Empirical Reference Infrastructure
- 57 bond lengths, 25 angles, 30 crystal lattices, 19 solvents — compile-time C++ arrays
- Live download from PubChem REST and NIST WebBook via `scripts/fetch_empirical.py`
- Disk-space guard with backup/rollback on write failure
- Optional-package detection (reports missing `requests`, `tqdm`, etc.)

### File Format Pipeline
```
.xyz  → Static geometry (element + Cartesian coordinates)
.xyza → Animated trajectory (sequential frames)
.xyzc → Checkpointed MD (positions + velocities + thermodynamics + SHA-256 hash)
```

### Tools
- **Interactive CLI**: Molecular construction and manipulation
- **Simulation engine**: MD/minimisation with FIRE, Verlet, and Langevin integrators
- **Deep verification**: Self-testing executable — 256 deterministic + randomised checks
- **Empirical fetcher**: PubChem/NIST bond-length and spectroscopic cross-check
- **Desktop application** (Qt6, in progress): 3D viewport, object tree, properties panel
- **Self-audit suite**: Python tools for failure analysis and regression detection

---

## Methodology (LaTeX)

The scientific foundation lives in **13 standalone LaTeX sections** (200+ pages total).

### Condensed Versions
| Document | Pages | Purpose |
|----------|-------|---------|
| [`METHODOLOGY_2PAGE.tex`](docs/METHODOLOGY_2PAGE.tex) | 2 | Conference handout (two-column summary) |
| [`METHODOLOGY_12PAGE.tex`](docs/METHODOLOGY_12PAGE.tex) | 12 | Quick reference with equations |

### Full Sections
| File | Sections | Content |
|------|----------|---------|
| [`section0`](docs/section0_identity_state_decomposition.tex) | §0 | Particle identity vectors, cell/world container ontology |
| [`section1`](docs/section1_foundational_thesis.tex) | §1 | Problem definition, scope, domain of validity |
| [`section2`](docs/section2_state_ontology.tex) | §2 | State tuple, identity/phase/scratch partitioning |
| [`section3`](docs/section3_interaction_model.tex) | §3 | LJ, Coulomb, UFF, switching, PBC |
| [`section4`](docs/section4_thermodynamics.tex) | §4 | Unit system, temperature, pressure, heat capacity |
| [`section5`](docs/section5_integration.tex) | §5 | Verlet, Langevin, FIRE algorithms |
| [`section6`](docs/section6_formation_physics.tex) | §6 | Bonded terms, formation pipeline, basin mapping |
| [`section7`](docs/section7_statistical_interpretation.tex) | §7 | Welford, stationarity, Kabsch, scoring |
| [`section8-9`](docs/section8_9_reaction_electronic.tex) | §8–9 | QEq, Fukui functions, HSAB, reaction templates |
| [`section8b`](docs/section8b_heat_gated_reaction_control.tex) | §8b | Heat-gated reaction control, 500-sim validation |
| [`section10-13`](docs/section10_12_13_closing.tex) | §10,12,13 | Multiscale, validation doctrine, roadmap |
| [`section11`](docs/section11_self_audit.tex) | §11 | Failure classifier, gap targeter, regression detector |
| [`phase_reports`](docs/section_phase_reports.tex) | PHR | Phase 1–14 + Milestone A verification (15 pp.) |
| [`bridge_arch`](docs/section_bridge_architecture.tex) | SBA | Three-layer architecture, EngineAdapter bridge |

**Compile:** `cd docs && for f in section*.tex; do pdflatex "$f"; done`

**Full index:** [`docs/INDEX.md`](docs/INDEX.md)

---

## Quick Start

> **Full usage reference:** [`docs/USAGE.md`](docs/USAGE.md)

### Full Verification (recommended first step)
```bash
# Quick local run (Suites A-N, Q, I-M — no network, ~10 s)
wsl bash scripts/run_all_verification.sh

# Full run — also fetches PubChem/NIST and runs Suite P
wsl bash scripts/run_all_verification.sh --full

# Verbose: print each individual PASS/FAIL line
wsl bash scripts/run_all_verification.sh --full --verbose
```

### Build
```bash
# Engine + verification executables
wsl bash -c "cmake -B wsl-build -DBUILD_VIS=OFF -DBUILD_TESTS=ON ; cmake --build wsl-build -j4"

# Desktop application (requires Qt6)
wsl bash -c "cmake -B wsl-build-desktop -DBUILD_DESKTOP=ON -DBUILD_VIS=OFF ; cmake --build wsl-build-desktop -j4"
```

### Deep Verification
```bash
# Standard run (seed printed at end for reproduction)
wsl bash -c "cd /mnt/c/Users/Liam/Desktop/vsepr-sim ; ./wsl-build/deep_verification"

# Reproduce the recorded Milestone A run exactly
wsl bash -c "cd /mnt/c/Users/Liam/Desktop/vsepr-sim ; ./wsl-build/deep_verification --seed 1772952709"

# Stress-test: 2 000 samples per randomised suite
wsl bash -c "cd /mnt/c/Users/Liam/Desktop/vsepr-sim ; ./wsl-build/deep_verification --rand-iters 2000 --verbose"
```

### Interactive molecular builder
```bash
./wsl-build/atomistic-build
```
```
>> build H2O
>> validate
>> save H2O.xyz
>> exit
```

### FIRE energy minimisation
```bash
./wsl-build/atomistic-relax molecule.xyz --force-tol 1e-5 --max-iter 5000 --output relaxed.xyza
```

### Molecular dynamics
```bash
./wsl-build/atomistic-sim md-nvt --temp 300 --steps 10000 molecule.xyz
./wsl-build/atomistic-sim energy molecule.xyz
```

### Desktop application
```bash
wsl bash -c "./wsl-build-desktop/vsepr-desktop"
```

Console quick-reference (type at the `>` prompt — full list at [`docs/USAGE.md § 12`](docs/USAGE.md#12-desktop--console-command-reference)):

| Command | Action |
|---------|--------|
| `relax` | FIRE minimisation (F5) |
| `md` | NVT Langevin MD 300 K (F6) |
| `sp` | Single-point energy (F4) |
| `crystal` | Crystal Preset dialog (Ctrl+K) |
| `bs` / `cpk` / `sticks` / `wire` | Render mode |
| `labels` / `axes` / `box` | Toggle overlays |
| `play` / `stop` / `next` / `prev` | Trajectory playback |
| `fit` / `reset` | Camera |

### Fetch empirical references
```bash
wsl python3 scripts/fetch_empirical.py           # live PubChem + NIST
wsl python3 scripts/fetch_empirical.py --offline   # bundled defaults only
```

---

## Repository Structure

```
vsepr-sim/
├── atomistic/            Atomistic kernel library
│   ├── core/             State, empirical reference DB, environment context
│   ├── models/           LJ+Coulomb, bonded MM, composite model
│   ├── integrators/      FIRE, velocity Verlet, Langevin (BAOAB)
│   ├── crystal/          Unit cell, supercell builder, presets
│   ├── parsers/          XYZ/XYZA/XYZC I/O
│   └── analysis/         RDF, coordination, geometry analysis
├── src/                  Legacy engine (core, sim, pot, box, cli)
├── apps/                 Executables
│   ├── deep_verification.cpp   # 256-check verification suite
│   ├── phase*.cpp              # Phase 1-14 verification executables
│   └── desktop/                # Qt6 desktop application (in progress)
├── scripts/
│   ├── run_all_verification.sh   # One-command tiered verification
│   ├── fetch_empirical.py        # PubChem + NIST reference downloader
│   └── gen_empirical_ref.py      # Compile-time reference generator
├── docs/                 LaTeX methodology + notebooks (200+ pages)
│   ├── section*.tex      (13 sections)
│   ├── verification/     Milestone records
│   └── INDEX.md          Reading order and document map
├── verification/         Runtime verification outputs (gitignored)
├── tests/                Legacy validation suite
├── tools/                Self-audit Python scripts
├── data/                 Reference geometries
├── examples/             Demo molecules (.xyz)
└── third_party/          ImGui (vendored)
```

---

## Version History

| Tag | Description |
|-----|-------------|
| **v2.7.1** | Deep verification milestone — 485/485 pass, FIRE bug-fix, 42 UFF elements, empirical DB |
| v2.7.0 | Desktop application scaffold (Qt6) |
| v2.6.3 | Empirical lookup table — ALPHA_EMPIRICAL[118] compile-time array |
| v2.6.2 | Plateau investigation and root-cause analysis |
| v2.6.1 | Alpha model generative baseline |
| v2.5.3 | Heat-gated reaction control system |
| v2.5.0 | Clean baseline release with methodology |

---

## Approved Domains

**✅ Verified and passing:**
- Noble gas systems (He, Ne, Ar, Kr, Xe) — LJ + FIRE + NVE fully verified
- Homonuclear diatomics (H₂, N₂, O₂, F₂, Cl₂, Br₂, I₂) — PubChem cross-checked
- Small molecules (H₂O, NH₃, CH₄, CO₂, HF, HCl, H₂S) — bond detection + geometry
- Hydrocarbon series (ethane, ethylene, acetylene, benzene) — bond lengths validated
- Organic functionalities (methanol, formaldehyde, formic acid, acetone)
- Crystal structures (Cu FCC, Fe BCC, NaCl rocksalt, Si diamond, MgO, CsCl) — geometry verified
- Dielectric screening (19 solvents: water through diethyl ether, CRC 104 reference)

**⚠️ Geometry-only (LJ model limitation):**
- Metal and covalent crystal FIRE relaxation — UFF LJ r_min exceeds actual bond lengths
- Ionic MD coupling — use FIRE minimisation until Coulomb-integrator coupling is extended

**❌ Out of scope for classical treatment:**
- Transition metal d-orbital effects
- Excited-state dynamics

---

## Design Principles

1. **No fake physics** — code reflects real physical modelling; no visual-only approximations
2. **Explicit units everywhere** — Å, kcal/mol, amu, K, fs. No reduced units
3. **Deterministic core** — identical inputs produce bit-identical outputs (seeded RNG)
4. **Periodic table as sole authority** — no molecular databases; parameters from UFF
5. **Verification before extension** — validated core remains intact; new physics adds, never replaces

---

## System Requirements

- **Compiler:** GCC 10+, Clang 12+, or MSVC 2022+ (C++20)
- **Build System:** CMake 3.15+
- **Python (optional):** 3.8+ for `fetch_empirical.py` (stdlib only; `requests`/`tqdm` improve but are not required)
- **Qt6 (optional):** For desktop application build (`-DBUILD_VIS=ON`)
- **OS:** Linux, Windows (WSL recommended), macOS

---

## License

MIT License — see [LICENSE](LICENSE) for details.

---

## References

- Universal Force Field: Rappé et al., *J. Am. Chem. Soc.* **114**, 10024 (1992)
- FIRE minimisation: Bitzek et al., *Phys. Rev. Lett.* **97**, 170201 (2006)
- Kabsch alignment: Kabsch, *Acta Cryst.* **A32**, 922 (1976)
- CRC Handbook of Chemistry and Physics, 104th Edition (solvent dielectrics)
- NIST WebBook (diatomic spectroscopic constants)
- PubChem REST API (3D conformer geometries)
- ImGui: Omar Cornut et al. (vendored, MIT license)

---

<div align="center">

**[Documentation](docs/INDEX.md) · [Verification](docs/verification/milestone_A.md) · [Methodology](docs/METHODOLOGY_12PAGE.tex) · [GitHub](https://github.com/LMSM3/VSEPR-SIM)**

*485 / 485 verification checks passing. Every state reproducible. Every result traceable.*

</div>
