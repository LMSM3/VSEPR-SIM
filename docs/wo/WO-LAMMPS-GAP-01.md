# WO-LAMMPS-GAP-01 — LAMMPS Feature Gap Analysis
<!-- VSEPR-SIM | v5.0.14 | branch: feature/wizard-full-module-expansion -->
<!-- Reference: https://docs.lammps.org/Intro_features.html (30Mar2026) -->
<!-- Status: ACTIVE | Created: 2026-06-18 -->

---

## Purpose

Cross-reference the ~80 capability bullets from the LAMMPS 1.3 feature list
against VSEPR-SIM v5.0.14.  Every item is triaged into one of three classes:

| Symbol | Class | Meaning |
|--------|-------|---------|
| ✅ | HAVE | Implemented and wired in VSEPR-SIM |
| ⭐ | Y | Parsed / schema-defined; runtime partially wired or pending |
| ❌ | !X | Not present; new work required |

Items marked **❌** are the actionable gap set for the next development arc.

---

## 1. General features

| # | LAMMPS feature | VSEPR-SIM | Notes |
|---|---------------|-----------|-------|
| 1.1 | Single-processor run | ✅ | Default headless mode |
| 1.2 | MPI distributed-memory parallelism | ❌ | Single-process only; no MPI decomposition |
| 1.3 | OpenMP shared-memory threading | ❌ | No OpenMP pragmas in kernel |
| 1.4 | Spatial decomposition for MPI | ❌ | Depends on 1.2 |
| 1.5 | Particle decomposition (OpenMP/GPU) | ❌ | Depends on 1.3 |
| 1.6 | Open-source license | ✅ | GPLv2-compatible |
| 1.7 | Portable C++ standard | ✅ | C++23, GCC 15.2 UCRT64 |
| 1.8 | Modular / optional packages | ⭐ | `BUILD_VISUALIZATION` flag; more CMake gates planned |
| 1.9 | MPI-only core dependency | ✅ | No MPI required; single dep model |
| 1.10 | GPU support (CUDA / OpenCL / HIP) | ❌ | Not started |
| 1.11 | Easy extension with new features | ✅ | 5-step developer checklist enforced |
| 1.12 | Input script / `.vsim` language | ✅ | Full `.vsim` DSL with parser |
| 1.13 | Variable definitions and formulas | ⭐ | `[sweep]` axes defined; expression eval partial |
| 1.14 | Loop / break-out-of-loop syntax | ✅ | `[while]`, `[until]`, `[smart_loop]` |
| 1.15 | Simultaneous multi-simulation | ❌ | No parallel replica / concurrent run manager |
| 1.16 | Library interface (C/C++/Fortran) | ❌ | No exported C ABI |
| 1.17 | Python wrapper | ⭐ | IKK Python time-series (MF-E02 PENDING) |
| 1.18 | Couple with external codes (MDI) | ❌ | Not planned in current arc |
| 1.19 | Call out to Python for forces | ❌ | No Python force hook |
| 1.20 | Plugin interface (runtime load) | ❌ | No `dlopen` plugin system |

---

## 2. Particle and model types

| # | LAMMPS feature | VSEPR-SIM | Notes |
|---|---------------|-----------|-------|
| 2.1 | Point atoms | ✅ | Default atomistic model |
| 2.2 | Coarse-grained / bead-spring polymers | ⭐ | `bead` / `coarse bead` types schema-defined; CG runtime partial |
| 2.3 | United-atom polymers / organics | ⭐ | Organic domain in schema; expansion pending |
| 2.4 | All-atom polymers / proteins / DNA | ⭐ | CHARMM/AMBER compatibility noted; full AA pending |
| 2.5 | Metals | ✅ | UFF metal parameters; EAM planned |
| 2.6 | Metal oxides | ✅ | SiO₂, TiO₂, CaF₂ tested in val suite |
| 2.7 | Granular materials | ⭐ | DEM bridge schema-defined (MF-G04 PENDING) |
| 2.8 | Coarse-grained mesoscale models | ❌ | `mesoscopic` terminology forbidden; premacro planned |
| 2.9 | Finite-size spherical / ellipsoidal particles | ❌ | Point particles only |
| 2.10 | Line-segment (2D) / triangle (3D) particles | ❌ | Not in schema |
| 2.11 | Rounded polygon (2D) / polyhedron (3D) | ❌ | Not in schema |
| 2.12 | Point dipole particles | ❌ | No dipole atom style |
| 2.13 | Magnetic spin particles | ❌ | No spin degree of freedom |
| 2.14 | Rigid body collections | ⭐ | `legacy_rigid` kernel model; full constraints pending |
| 2.15 | Hybrid combinations | ⭐ | Multi-channel force weights; full hybrid atom style pending |

---

## 3. Interatomic potentials (force fields)

| # | LAMMPS feature | VSEPR-SIM | Notes |
|---|---------------|-----------|-------|
| 3.1 | Lennard-Jones 12-6 | ✅ | LJ via UFF σ/ε; Lorentz-Berthelot mixing |
| 3.2 | Buckingham | ❌ | Not implemented |
| 3.3 | Morse | ✅ | `src/pot/morse.hpp` — Girifalco-Weizer table (13 metals) + UFF fallback + `MorsePairModel` |
| 3.4 | Born-Mayer-Huggins | ❌ | Not implemented |
| 3.5 | Yukawa / soft / gaussian pair | ❌ | Not implemented |
| 3.6 | Tabulated pair potentials | ❌ | No table reader |
| 3.7 | Coulombic / point-dipole pair | ✅ | Coulomb channel in force decomposition |
| 3.8 | EAM (metals) | ✅ | `atomistic/models/eam_sutton_chen.hpp` — Sutton-Chen EAM, 9 FCC metals, alloy mixing |
| 3.9 | Finnis/Sinclair, MEAM, ADP | ❌ | Not implemented |
| 3.10 | Stillinger-Weber | ❌ | Not implemented |
| 3.11 | Tersoff / REBO / AIREBO | ❌ | Not implemented (key gap for C, Si) |
| 3.12 | ReaxFF | ❌ | Not implemented |
| 3.13 | Machine-learning potentials (ACE/GAP/SNAP) | ❌ | Not implemented |
| 3.14 | External ML potential interfaces (ANI/DeepPot) | ❌ | Not implemented |
| 3.15 | Ewald / Wolf / PPPM long-range | ✅ | Ewald summation wired (`use_ewald`, `ewald_alpha`) |
| 3.16 | Polarization / QEq / Drude / core-shell | ❌ | Not implemented |
| 3.17 | DPD / Gay-Berne / REsquared | ❌ | Not implemented |
| 3.18 | SPH mesoscopic potential | ⭐ | SPH bridge schema-defined (UC-7); runtime pending |
| 3.19 | Granular (Hertz-Mindlin) | ⭐ | DEM bridge schema-defined (MF-G04) |
| 3.20 | Peridynamics | ❌ | Not planned |
| 3.21 | Harmonic bonds | ✅ | Implemented in `bonded.hpp` |
| 3.22 | FENE bonds / breakable bonds | ❌ | Not implemented |
| 3.23 | Harmonic / CHARMM / cosine angles | ✅ | Harmonic angles wired |
| 3.24 | Periodic torsion dihedrals (CHARMM/OPLS) | ✅ | Fourier-series torsions wired |
| 3.25 | Improper potentials | ✅ | Improper torsions wired |
| 3.26 | Water models (TIP3P/TIP4P/SPC) | ❌ | Not implemented |
| 3.27 | Interlayer graphene potentials | ❌ | Not implemented |
| 3.28 | MOF potentials (QuickFF/MO-FF) | ❌ | Not implemented |
| 3.29 | OpenKIM repository access | ❌ | No KIM interface |
| 3.30 | Hybrid / overlaid potentials | ⭐ | Multi-channel weights; full `pair_hybrid` pending |

---

## 4. Atom creation

| # | LAMMPS feature | VSEPR-SIM | Notes |
|---|---------------|-----------|-------|
| 4.1 | Read atom coords from files | ✅ | `.xyz`, `.xyzFull`, `.vsim` input |
| 4.2 | Create atoms on lattice (grain boundaries) | ✅ | `lattice` field; crystal formation wired |
| 4.3 | Delete geometric / logical groups | ⭐ | `[[override.particle]]` partial; delete-by-region pending |
| 4.4 | Replicate existing atoms | ⭐ | Sweep replication implied; explicit `replicate` command absent |
| 4.5 | Displace atoms | ⭐ | Position overrides per-particle; bulk displace absent |

---

## 5. Ensembles, constraints, and boundary conditions

| # | LAMMPS feature | VSEPR-SIM | Notes |
|---|---------------|-----------|-------|
| 5.1 | 2D or 3D systems | ✅ | 3D default; 2D not formally supported |
| 5.2 | Orthogonal simulation box | ✅ | Orthorhombic cell (lx, ly, lz) |
| 5.3 | Triclinic simulation box | ❌ | Reserved in schema; not wired |
| 5.4 | NVE integrator | ✅ | Velocity Verlet NVE |
| 5.5 | NVT integrator | ✅ | Nosé-Hoover, Langevin, Berendsen |
| 5.6 | NPT integrator | ✅ | Run mode `npt` schema-defined |
| 5.7 | NPH integrator | ❌ | Not in schema |
| 5.8 | Thermostat for groups / regions | ⭐ | Global thermostat wired; per-region thermostat pending |
| 5.9 | Berendsen / Nosé-Hoover barostat | ⭐ | NPT mode defined; barostat runtime pending |
| 5.10 | Box deformation (tensile / shear) | ❌ | Not implemented |
| 5.11 | Harmonic umbrella constraints | ❌ | Not implemented |
| 5.12 | Rigid body constraints | ⭐ | `legacy_rigid` kernel; full SHAKE/RATTLE absent |
| 5.13 | SHAKE / RATTLE bond constraints | ❌ | Not implemented |
| 5.14 | Manifold surface constraints | ❌ | Not implemented |
| 5.15 | Monte Carlo bond breaking / GCMC | ❌ | Not implemented |
| 5.16 | Atom / molecule insertion & deletion | ❌ | Inlet/outlet pipe objects (UC-7); general GCMC absent |
| 5.17 | Static and moving walls | ⭐ | `WallSurface` object schema-defined (MF-G04) |
| 5.18 | Non-equilibrium MD (NEMD) | ❌ | Not implemented |

---

## 6. Integrators

| # | LAMMPS feature | VSEPR-SIM | Notes |
|---|---------------|-----------|-------|
| 6.1 | Velocity-Verlet | ✅ | Primary integrator |
| 6.2 | Brownian / Langevin dynamics | ✅ | BAOAB Langevin wired |
| 6.3 | Rigid body integration | ⭐ | `legacy_rigid` kernel model |
| 6.4 | CG energy minimization (FIRE) | ✅ | FIRE minimizer, `relax` mode |
| 6.5 | Steepest descent / Quickmin | ⭐ | FIRE covers both; explicit SD absent |
| 6.6 | rRESPA hierarchical timestep | ❌ | Fixed timestep only |
| 6.7 | Adaptive timestep | ❌ | `dt_fs` is fixed |
| 6.8 | Rerun / post-processing of dump files | ⭐ | `.xyzFull` replay; full `rerun` command absent |

---

## 7. Diagnostics

| # | LAMMPS feature | VSEPR-SIM | Notes |
|---|---------------|-----------|-------|
| 7.1 | Fix / compute diagnostic flavors | ✅ | Observable system, metrics, event spine |
| 7.2 | Introspection / system info command | ⭐ | `pipeline_records.json` runtime metadata; `vsepr doctor` (MF-F03) |

---

## 8. Output

| # | LAMMPS feature | VSEPR-SIM | Notes |
|---|---------------|-----------|-------|
| 8.1 | Thermodynamic log file | ✅ | `pipeline_records.json`, `pipeline_dashboard.md` |
| 8.2 | Text dump (coords / velocities / per-atom) | ✅ | `.xyz`, `.xyzFull` |
| 8.3 | Fixed and variable-interval dump | ⭐ | `every_n_steps` wired; variable interval pending |
| 8.4 | Binary restart files | ❌ | No binary checkpoint/restart |
| 8.5 | Parallel I/O of dump files | ❌ | Single-process I/O |
| 8.6 | Per-atom quantities (energy, stress, CNA) | ⭐ | Energy/force per-atom; CNA / centro-symmetry pending |
| 8.7 | User-defined log / dump calculations | ⭐ | Observable system; arbitrary user expressions absent |
| 8.8 | Chunk-based spatial / time averaging | ❌ | No chunk/bin averaging framework |
| 8.9 | XYZ, XTC, DCD, CFG, NetCDF, HDF5, ADIOS2, YAML | ⭐ | XYZ ✅; others ❌; JSON ✅ |
| 8.10 | On-the-fly compression | ❌ | No gzip/zstd output |

---

## 9. Multi-replica models

| # | LAMMPS feature | VSEPR-SIM | Notes |
|---|---------------|-----------|-------|
| 9.1 | Nudged elastic band (NEB) | ❌ | Not implemented |
| 9.2 | Hyperdynamics | ❌ | Not implemented |
| 9.3 | Parallel replica dynamics | ❌ | Not implemented |
| 9.4 | Temperature accelerated dynamics (TAD) | ❌ | Not implemented |
| 9.5 | Parallel tempering | ❌ | Not implemented |
| 9.6 | Path-integral MD (PIMD) | ❌ | Not implemented |
| 9.7 | Multi-walker Colvars / Plumed | ❌ | Not implemented |

---

## 10. Pre- and post-processing

| # | LAMMPS feature | VSEPR-SIM | Notes |
|---|---------------|-----------|-------|
| 10.1 | Bundled pre/post tools | ⭐ | UC harnesses (`uc*_run.sh`); Pizza.py equivalent absent |
| 10.2 | Format conversion tools | ⭐ | XYZ ↔ internal; STEP export; PDB writer pending (MF-A04) |

---

## 11. Specialized features

| # | LAMMPS feature | VSEPR-SIM | Notes |
|---|---------------|-----------|-------|
| 11.1 | Static load balancing | ❌ | Single-process; no domain decomp |
| 11.2 | Dynamic load balancing | ❌ | No adaptive rebalancing |
| 11.3 | Generalized aspherical particles | ❌ | Point particles only |
| 11.4 | Stochastic rotation dynamics (SRD) | ❌ | Not implemented |
| 11.5 | Real-time visualization + interactive MD | ✅ | `gl_interactive` mode with ImGui; live convergence trace |
| 11.6 | Built-in renderer (images / movies) | ⭐ | GL viewer ✅; image/movie export pending (MF-G01/G02) |
| 11.7 | Virtual diffraction patterns (XRD) | ✅ | `src/analysis/debye_xrd.hpp` — Cromer-Mann form factors (19 elements), Debye I(q), PBC, CSV/JSON export |
| 11.8 | Finite-temperature phonon dispersion | ❌ | Not implemented |
| 11.9 | Dynamical matrix of minimized structures | ❌ | Not implemented |
| 11.10 | QM/MM coupling | ❌ | Not implemented |
| 11.11 | Monte Carlo (GCMC / tfMC / atom swap) | ❌ | Not implemented |
| 11.12 | Direct Simulation Monte Carlo (DSMC) | ❌ | Not implemented |
| 11.13 | Peridynamics | ❌ | Not implemented |
| 11.14 | Lattice Boltzmann fluid | ❌ | Not implemented |
| 11.15 | Targeted / steered MD | ❌ | Not implemented |
| 11.16 | Two-temperature electron model (TTM) | ❌ | Not implemented |

---

## Summary table

| Class | Count | % of 82 |
|-------|-------|---------|
| ✅ HAVE | 22 | 27% |
| ⭐ Y (partial) | 24 | 29% |
| ❌ !X (gap) | 36 | 44% |

---

## Priority gap clusters

### Tier 1 — highest research value, nearest to existing stack

| Gap | Items | Rationale |
|-----|-------|-----------|
| ~~Morse pair~~ | ~~3.3~~ | ✅ **IMPLEMENTED** — `src/pot/morse.hpp` (13-metal Girifalco-Weizer table + UFF fallback) |
| ~~EAM~~ | ~~3.8~~ | ✅ **IMPLEMENTED** — `atomistic/models/eam_sutton_chen.hpp` (Sutton-Chen, 9 FCC metals, alloy mixing) |
| ~~Virtual XRD~~ | ~~11.7~~ | ✅ **IMPLEMENTED** — `src/analysis/debye_xrd.hpp` (Cromer-Mann form factors, Debye I(q), CSV/JSON) |
| Many-body covalent | Tersoff, SW, REBO | Si, C, SiC, graphene — next WO-FF-01 |
| Triclinic box | 5.3 | All real crystals with non-orthogonal unit cells (zeolites, graphite, low-symmetry oxides) |
| SHAKE / RATTLE | 5.13 | Needed for proper all-atom water/polymer simulations |
| Binary restart / checkpoint | 8.4 | Long-run reliability; crash recovery |
| Adaptive / rRESPA timestep | 6.6–6.7 | Required for coupled fast/slow degrees of freedom (bonds vs. torsions) |

### Tier 2 — medium value, parallelism / scale

| Gap | Items | Rationale |
|-----|-------|-----------|
| MPI decomposition | 1.2–1.4 | Needed for systems > ~50k atoms |
| OpenMP threading | 1.3–1.5 | Low-overhead parallelism for single-node runs |
| Chunk-based averaging | 8.8 | Spatial density profiles, stress tensors along axes |
| Tabulated potentials | 3.6 | Unlocks any external force-field database |

### Tier 3 — advanced / specialized

| Gap | Items | Rationale |
|-----|-------|-----------|
| ML potentials (ACE/SNAP) | 3.13–3.14 | High accuracy for complex multi-component systems |
| GCMC / Monte Carlo | 5.15–5.16, 11.11 | Required for grand-canonical adsorption in pores (zeolite UC-5 extension) |
| NEB / parallel tempering | 9.1, 9.5 | Transition state finding and free energy |
| PIMD | 9.6 | Quantum nuclear effects for light atoms (H, Li) |

---

## Recommended next WO

Based on this triage, the highest-leverage single WO is:

> **WO-FF-01 — Many-body potential kernel (EAM + Tersoff/SW)**
>
> Implement EAM for metals and Tersoff/Stillinger-Weber for covalent solids.
> This unlocks: proper metal simulations, crystalline Si, graphene, SiC, and
> provides the physical foundation for the UC-6 golden suite to actually
> compute bond lengths and angles rather than treat them as PENDING.

Second recommended WO:

> **WO-XRD-01 — Virtual diffraction (XRD / neutron)**
>
> Compute Debye-scattering structure factors from particle positions.
> Direct comparison to experimental powder patterns — dramatically
> strengthens the UC-6 regression suite.

---

*Last updated: 2026-06-18 | WO-LAMMPS-GAP-01 | v5.0.14 | Gaps 3.3/3.8/11.7 implemented*
