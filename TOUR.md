# VSEPR-Sim -- Project Tour

> A fast orientation to the build system and directory layout.
> Read top-to-bottom for first-timers; use headings as a map for daily navigation.

---

## 1. CMake Architecture

    CMakeLists.txt          -- root orchestrator (start here)
    cmake/
      CoreBuild.cmake       -- Part 1: headless C++ libraries and apps
      VisBuild.cmake        -- Part 2: OpenGL / GLFW / ImGui / Qt6

### Root CMakeLists.txt -- three things it does

| Order | What happens |
|-------|-------------|
| 1 | Project identity, C++23, optional CUDA probe, build flags |
| 2 | include(cmake/CoreBuild.cmake) -- headless stack |
| 3 | include(cmake/VisBuild.cmake) -- graphics stack |
| 4 | Tests wired up (both parts must be defined first) |

### cmake/CoreBuild.cmake -- headless libraries (dependency order)

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

### cmake/VisBuild.cmake -- graphics (gated by BUILD_VIS=ON)

    OpenGL (system) --> GLFW (system or FetchContent 3.4)
                    --> GLEW (system or FetchContent)
                    --> GLM  (system or FetchContent)
                    --> ImGui (FetchContent)
                    --> Qt6  (system, optional)

If OpenGL is missing the entire Part 2 is skipped gracefully.

### Build options

    BUILD_TESTS   ON   -- CTest suite under tests/
    BUILD_APPS    ON   -- CLI apps in apps/
    BUILD_VIS     ON   -- OpenGL/GUI targets
    BUILD_DEMOS   OFF  -- demo targets (opt-in)

Quick build from PowerShell:

    cmake -B build -S . -DBUILD_VIS=ON
    cmake --build build --parallel
    ctest --test-dir build --output-on-failure

---

## 2. Directory Tree (annotated)

    VSPER-SIM/
    |-- CMakeLists.txt          Root build orchestrator
    |-- TOUR.md                 This file -- start here
    |-- cmake/
    |   |-- CoreBuild.cmake     Headless libraries + apps
    |    -- VisBuild.cmake      OpenGL/ImGui/Qt6 targets
    |-- src/
    |   |-- core/               Core kernel (tracker, trail, units, gas, report)
    |   |-- gas2/               Advanced EOS engine
    |    -- gas3/               Quality pipeline + export engine
    |-- include/                Public C++ headers
    |-- apps/                   CLI and GUI entry-points
    |   |-- atomistic*.cpp          Atomistic simulation suite
    |   |-- report_generator.cpp    Autonomous report runner
    |   |-- nuclear_core_runner.cpp Nuclear core simulation
    |   |-- phase*.cpp              Phase-by-phase verification apps
    |    -- desktop/                Qt6 desktop GUI
    |-- tests/                  CTest test suite (35 tests, 5 levels)
    |-- pykernel/               Python physics kernel
    |   |-- gas.py                  Ideal/real gas EOS and pipe-flow
    |   |-- gas3_plant_helper.py    RS-0001 plant gas-side analysis
    |   |-- pipe.py                 Typed synchronous data pipeline
    |   |-- pipe3_plant_helper.py   RS-0001 plant pipe-network analysis
    |   |-- helium_tables.py        Helium property tables (ambient to reactor)
    |   |-- room_sim.py             3D thermal diffusion solver
    |    -- thermo_pipe.py          Batch thermal runner
    |-- scripts/                Developer shell and automation
    |   |-- doc_shell.py            Interactive docs/reports REPL  <-- MAIN ENTRY
    |   |-- demos/                  Visual demo scripts (VisPy / 3D)
    |    -- build_*.sh / *.ps1      Build helpers
    |-- reporting/              Automatic report generation
    |   |-- generate_report.py          Python report generator
    |    -- report.tex / report.md      Output templates
    |-- docs/                   Formal documentation corpus (186-page PDF)
    |-- out/                    Generated artifacts (PDFs, CSVs, PNGs)
    |-- reports/                Simulation run reports
    |-- pipeline/               FastAPI + SSE continual reaction stream
    |-- data/                   Reference datasets
     -- .venv/                  Python virtual environment

---

## 3. Data Flow

    C++ kernel (src/core)
      gas/gas2/gas3 libs    --> physical property computation
      vsepr_report          --> collate results, write CSV/JSON to out/

    Python (pykernel)
      pipe.py               --> stream typed records
      thermo_pipe.py        --> batch thermal analysis
      gas3_plant_helper.py  --> plant-cycle RS-0001 analysis
      reporting/            --> render Markdown and LaTeX from results

    Interactive
      scripts/doc_shell.py  --> browse, edit, compile, export anything

---

## 4. Quick-Start Paths

| Goal | Command |
|------|---------|
| Build everything | cmake -B build -S . and cmake --build build --parallel |
| Run tests | ctest --test-dir build -V |
| Interactive docs shell | .venv/Scripts/python scripts/doc_shell.py |
| Generate a report | python reporting/generate_report.py |
| 3D helium room demo | python scripts/demos/demo_helium_room_3d.py |
| Plant gas analysis | python -m pykernel.gas3_plant_helper |
