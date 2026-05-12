<div align="center">

# VSEPR-SIM

**Deterministic Molecular and Materials Simulation**

Script-driven simulation pipelines via the VSIM language. Reproducible by design.

[VSIM Reference](docs/VSIM_LANGUAGE_REFERENCE.md) • [Methodology](docs/METHODOLOGY_12PAGE.tex) • [File Formats](docs/XYZ_FORMAT_REFERENCE.md) • [Validation](docs/VALIDATION_REPORT.md) • [Tour](TOUR.md)

</div>

---

> VSEPR-SIM v5.0.0 is a deterministic molecular and materials simulation environment built around VSIM scripting, reproducible trajectory files, analysis-only property inference, and structured scientific reporting.

---

## Start Here

| Where you want to go | Jump to |
|---|---|
| Run a simulation script | [Quick Start](#quick-start) |
| Write or understand VSIM scripts | [docs/VSIM_LANGUAGE_REFERENCE.md](docs/VSIM_LANGUAGE_REFERENCE.md) |
| Understand the build system and folder layout | [TOUR.md](TOUR.md) |
| File format reference | [docs/XYZ_FORMAT_REFERENCE.md](docs/XYZ_FORMAT_REFERENCE.md) |
| Validation status | [docs/VALIDATION_REPORT.md](docs/VALIDATION_REPORT.md) |
| Formal methodology | [docs/METHODOLOGY_12PAGE.tex](docs/METHODOLOGY_12PAGE.tex) |

---

## What's New in v5.0.0

v5.0.0 marks the transition from disconnected simulation modules into an integrated scripted simulation environment.

- **VSIM scripting language** — declarative setup, execution, analysis, and reporting in a single `.vsim` file
- **`vsper` launcher** — canonical command-line entry point for running `.vsim` scripts post-install
- **Qt 3D workstation** — interactive viewport for structure inspection and trajectory playback, opened automatically at end-of-run when `gl_auto_orbit = true`
- **Beta-7 pipeline integration** — `FormationOutput` flows through `KernelEventLog` into `DashboardRecord` and downstream reporting artifacts
- **`.xyzf` trajectory playback** — multi-frame trajectory files written during simulation and replayed in the Qt workstation
- **Gas-mixing MD demonstration** — four-corner directed injection of N₂, O₂, H₂O, and Ar converging to a mixed system at 1200 K, N > 1200 atoms
- **Material-property inference workflow** — trajectory history → structural metrics → macro-property analysis → structured reports

---

## Overview

VSEPR-SIM generates atomistic structures deterministically from elemental identity and thermodynamic boundary conditions. It requires no external molecular databases and no pre-supplied input geometries — structure is a primary simulation output, not an assumption.

The VSIM scripting language lets you author a complete simulation pipeline — build a system, relax it, run MD, analyze the trajectory, and export results — in a single `.vsim` file that produces identical output on every run.

**Core properties:**

- **Deterministic** — same inputs produce bit-identical outputs across platforms (seeded RNG, no floating-point non-determinism)
- **Reproducible** — every script execution produces a provenance record alongside output files
- **Traceable** — hash-based provenance carried through the full pipeline from script input to final report
- **Analysis-only inference** — derived quantities computed post-hoc from trajectory data, not baked into the integrator

> *Every state is reproducible. Every result is traceable. Structure is a primary simulation output, not an input assumption.*

---

## VSIM Scripting Language

A `.vsim` script replaces a sequence of CLI calls with a single, readable, version-controllable file.

```vsim
[project]
name    = "water_nvt"
version = "v5.0.0"

[simulation]
box_size_ang = 30.0

[[simulation.molecule]]
formula       = "H2O"
count         = 64
temperature_K = 300.0

[run]
mode      = "md_nvt"
max_steps = 5000
dt_fs     = 0.5

[export]
write_xyz           = true
write_xyzf          = true
write_analysis_json = true
output_dir          = "out/water_nvt"

[visual]
gl_auto_orbit   = true
render_interval = 100
```

Run it:

```sh
vsper run water_nvt.vsim
```

Scripts are deterministic, hash-stamped, and fully logged. Every execution produces a provenance record and a set of output artifacts declared in `[export]`.

**Full language reference:** [docs/VSIM_LANGUAGE_REFERENCE.md](docs/VSIM_LANGUAGE_REFERENCE.md)

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
.vsim    → VSIM simulation script — build, relax, simulate, export in one file
.vsxyz   → VSEPR-native XYZ wrapper with embedded metadata
.xyzf    → Frame sequence for trajectory playback
.xyzfull → Full replay and render state artifact
.xyzc    → Checkpointed MD state (positions + velocities + thermodynamics + SHA-256 hash)
.xyza    → Animated trajectory (sequential frames)
.xyz     → Static geometry (element + Cartesian coordinates)
```

The XYZ family is documented in detail in [docs/XYZ_FORMAT_REFERENCE.md](docs/XYZ_FORMAT_REFERENCE.md).

### Tools

| Tool | Purpose |
|---|---|
| `vsper run` | Execute a `.vsim` script |
| `vsper validate` | Parse and validate a script without running |
| `vsper doctor` | Check installation health |
| `vsepr-desktop` | Qt 3D workstation (launched automatically by `vsper run`) |

---

## 3D Output Gallery

Structures generated and rendered by the VSEPR-SIM engine. CPK colouring, Phong shading, automatic bond detection from covalent radii.

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

Build from source:

```powershell
cmake -B build -S . -DBUILD_VIS=ON
cmake --build build --parallel
```

Install (makes `vsper` available on PATH):

```powershell
.\dist\VSEPR-SIM-5.0.0-local\install-vsepr.ps1
```

Run a VSIM script:

```sh
vsper run examples/gas_mixing_demo.vsim
```

The Qt 3D workstation opens automatically when the script requests visual output (`gl_auto_orbit = true`).

Run any of the included examples:

```sh
vsper run examples/demo_01_nacl_level0.vsim
vsper run examples/gas_mixing_demo.vsim
vsper run examples/beta7_pipeline_smoke.vsim
```

Validate a script without running it:

```sh
vsper validate examples/gas_mixing_demo.vsim
```

Check your installation:

```sh
vsper doctor
```

> **Legacy binaries** such as `vseprw`, `atomistic-build`, and `atomistic-relax` may exist in older branches. v5 documentation and all new workflows use `vsper` as the primary entry point.

---

## Validation Status

Validation in v5 is tracked through explicit beta release gates rather than a single static pass-rate figure.

| Category | Status |
|---|---|
| Script parsing and document validation | PASS |
| State and trajectory generation | PASS |
| `.xyz` / `.xyzf` file output | PASS |
| Formation pipeline and `KernelEventLog` | PASS |
| `ClusterRecord` → `AnalysisRecord` → `DashboardRecord` | PASS |
| Report and artifact export | PASS |
| SVG dashboard export | PASS |
| Qt 3D workstation and trajectory playback | PASS |
| Periodic boundary conditions (crystal MD) | DEFERRED |
| PNG dashboard raster export | DEFERRED |

Each beta release records items as `PASS`, `DEFERRED`, or `BLOCKED`, keeping active-development features visible and separate from validated components.

**Approved for:** noble gas systems, small organic molecules, hydrocarbons, molecular clusters, gas-phase mixing simulations.

**Known limitations:** ionic MD (use FIRE only until Coulomb-integrator coupling is complete), transition metal complexes (classical approximation insufficient).

Full report: [`docs/VALIDATION_REPORT.md`](docs/VALIDATION_REPORT.md)

---

## Repository Structure

```plaintext
VSEPR-SIM/
├── apps/
│   ├── desktop/          Qt 3D workstation (viewport, object tree, console)
│   └── vsper/            v5 launcher and command entry points
├── src/
│   ├── cli/              VSIM command handlers (run, validate, doctor)
│   ├── vsim/             VSIM parser, document, registry, runtime
│   ├── sim/              Molecule builder, VSEPR topology, integrators
│   ├── core/             State, force evaluation, energy ledger
│   ├── pot/              Potentials (LJ, Coulomb, bonded MM, UFF)
│   ├── io/               XYZ / xyzf / xyzFull / xyzA readers and writers
│   ├── thermal/          Thermal runner and xyzC format
│   └── dynamic/          Real molecule generator, analysis pipeline
├── include/
│   ├── vsim/             VSIM document, parser, runtime headers
│   ├── io/               XYZ format headers
│   └── vsepr/            Formula parser, shared interfaces
├── examples/             Sample .vsim scripts and demo trajectories
├── docs/
│   ├── section*.tex      Methodology sections (11 files, 186 pages)
│   ├── VSIM_LANGUAGE.md  VSIM language specification
│   └── VSIM_REFERENCE.md VSIM schema reference
├── assets/
│   └── images/           Screenshots and generated visuals
├── tests/                Validation suite and beta gate tests
├── dist/
│   └── VSEPR-SIM-5.0.0-local/   Installer and packaged binaries
└── data/                 Reference datasets (PeriodicTableJSON.json, UFF)
```

---

## Design Principles

1. **Explicit units everywhere** — positions in Å, energies in kcal/mol, time in fs. No reduced units.
2. **No silent domain switching** — force field, integrator, and boundary conditions declared upfront in the script.
3. **Deterministic core** — identical inputs produce bit-identical outputs (seeded RNG).
4. **Periodic table as sole authority** — element data comes from `PeriodicTableJSON.json`. No hardcoded element arrays.
5. **Analysis-only inference** — derived properties computed post-hoc from trajectory data, never baked into the integrator.
6. **Extension without replacement** — validated core remains intact as new pipeline stages are added.

---

## System Requirements

- **Compiler:** GCC 12+, Clang 15+, or MSVC 2022+ (C++23)
- **Build system:** CMake 3.20+
- **Graphics:** Qt 6.4+ with OpenGL 3.3+ (for `vsepr-desktop`)
- **OS:** Windows 10/11 (MSYS2/MinGW or MSVC), Linux, macOS

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

[VSIM Reference](docs/VSIM_LANGUAGE_REFERENCE.md) • [Methodology](docs/METHODOLOGY_12PAGE.tex) • [Validation](docs/VALIDATION_REPORT.md) • [Tour](TOUR.md) • [GitHub](https://github.com/LMSM3/VSEPR-SIM)

*VSEPR-SIM v5.0.0 is a documented scientific simulation environment built around deterministic state files, reproducible workflows, scripted execution, trajectory analysis, and material-property inference. It is under active development, with each beta release expanding the validated pipeline.*

</div>
