# VSEPR-SIM — Project Tour

> A fast orientation to the build system and directory layout.
> Read top-to-bottom for first-timers; use headings as a map for daily navigation.

---

## 1. CMake Architecture

    CMakeLists.txt          -- root orchestrator (start here)
    cmake/
      CoreBuild.cmake       -- Part 1: headless C++ libraries and apps
      VisBuild.cmake        -- Part 2: Qt6 / OpenGL / GLFW / ImGui

### Root CMakeLists.txt — what it does

| Order | What happens |
|-------|-------------|
| 1 | Project identity, C++23, optional CUDA probe, build flags |
| 2 | include(cmake/CoreBuild.cmake) — headless stack |
| 3 | include(cmake/VisBuild.cmake) — graphics stack |
| 4 | Tests wired up (both parts must be defined first) |

### cmake/CoreBuild.cmake — headless libraries (dependency order)

    vsepr_core       -- header-only interface, src/core/
    vsepr_tracker    -- element identity, random picker, alias resolver
    vsepr_trail      -- deterministic audit trail (CSV)
    vsepr_units      -- canonical Hartree unit system
    vsepr_gas        -- ideal/real gas, kinetic theory, Maxwell-Boltzmann
    vsepr_gas2       -- advanced EOS, heat, transport
    vsepr_gas3       -- quality pipeline, fitting, export, reporting
    vsepr_live_lib   -- HTTP analysis stream (port 9998)
    vsepr_viz_lib    -- dual-port viz stream (port 9999 + 10001)
    vsepr_report     -- autonomous thermal-materials report engine
    vsepr_bio_report -- organic/biochemical report engine

Apps live in apps/ and are gated by BUILD_APPS=ON.

### cmake/VisBuild.cmake — graphics (gated by BUILD_VIS=ON)

    Qt6  (system, required for vsepr-desktop)
    OpenGL (system) --> GLFW (system or FetchContent 3.4)
                    --> GLEW (system or FetchContent)
                    --> GLM  (system or FetchContent)
                    --> ImGui (FetchContent)

If Qt6 or OpenGL is missing the entire Part 2 is skipped gracefully.

### Build options

    BUILD_TESTS   ON   -- CTest suite under tests/
    BUILD_APPS    ON   -- CLI apps in apps/
    BUILD_VIS     ON   -- Qt6/OpenGL targets
    BUILD_DEMOS   OFF  -- demo targets (opt-in)

Quick build from PowerShell:

    cmake -B build -S . -DBUILD_VIS=ON
    cmake --build build --parallel
    ctest --test-dir build --output-on-failure

---

## 2. Directory Tree (annotated)

    VSEPR-SIM/
    |-- CMakeLists.txt          Root build orchestrator
    |-- TOUR.md                 This file
    |-- README.md               Project overview and quick start
    |-- cmake/
    |   |-- CoreBuild.cmake     Headless libraries + apps
    |    -- VisBuild.cmake      Qt6/OpenGL targets
    |-- src/
    |   |-- cli/                VSIM command handlers (run, validate, doctor)
    |   |-- vsim/               VSIM parser, document, registry, runtime
    |   |-- sim/                Molecule builder, VSEPR topology, integrators
    |   |-- core/               State, force evaluation, energy ledger
    |   |-- pot/                Potentials (LJ, Coulomb, bonded MM, UFF)
    |   |-- io/                 XYZ / xyzf / xyzFull / xyzA readers + writers
    |   |-- thermal/            Thermal runner and xyzC format
    |    -- dynamic/            Real molecule generator, analysis pipeline
    |-- include/
    |   |-- vsim/               VSIM document, parser, runtime headers
    |   |-- io/                 XYZ format headers
    |    -- vsepr/              Formula parser, shared interfaces
    |-- apps/
    |   |-- vsper/              v5 launcher (vsper run / validate / doctor)
    |    -- desktop/            Qt 3D workstation
    |       |-- MainWindow.*    Main window and dock layout
    |       |-- ViewportWidget.*    3D OpenGL viewport
    |       |-- bridge/         EngineAdapter: sim data -> SceneDocument
    |        -- main.cpp        Qt entry point
    |-- examples/               Sample .vsim scripts and demo trajectories
    |-- tests/                  Validation suite and beta gate tests
    |-- docs/
    |   |-- section*.tex        Methodology (11 files, 186 pages)
    |   |-- VSIM_LANGUAGE.md    VSIM language specification
    |    -- VSIM_REFERENCE.md   VSIM schema reference
    |-- assets/
    |    -- images/             Verified screenshots and generated visuals
    |-- dist/
    |    -- VSEPR-SIM-5.0.0-local/   Installer + packaged binaries
     -- data/                   Reference datasets (PeriodicTableJSON.json, UFF)

---

## 3. Data Flow

    VSIM script (.vsim)
      vsper run script.vsim
        |
        v
      VsimDocument (parsed, validated)
        |
        v
      Formation engine        --> initial geometry, force-field params
      Integrator (FIRE/NVT)   --> trajectory steps
      KernelEventLog          --> per-step events
        |
        v
      ClusterRecord
      AnalysisRecord          --> Welford stats, Kabsch, RDF, transport
      DashboardRecord         --> structured pipeline record
        |
        v
      Export (.xyz, .xyzf, .jsonl, .md, .tsv, .svg)
        |
        v
      Qt 3D workstation       --> auto-launched when gl_auto_orbit = true
        ViewportWidget        --> trajectory playback from .xyzf

---

## 4. Quick-Start Paths

| Goal | Command |
|------|---------|
| Build everything | `cmake -B build -S . -DBUILD_VIS=ON` then `cmake --build build --parallel` |
| Run tests | `ctest --test-dir build -V` |
| Run a VSIM script | `vsper run examples/gas_mixing_demo.vsim` |
| Validate a script | `vsper validate examples/gas_mixing_demo.vsim` |
| Check installation | `vsper doctor` |
| Install (adds vsper to PATH) | `.\\dist\\VSEPR-SIM-5.0.0-local\\install-vsepr.ps1` |
