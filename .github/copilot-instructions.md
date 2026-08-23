# Copilot Instructions

## General Guidelines
- When working on VSEPR-SIM, treat it as a deterministic, research-grade atomistic simulation, analysis, and reporting platform—not a toy or demonstration.
- Prioritize HTML hosting and HTTPS-like hosting work until project day 92.

## Current Release Information
- Current release: v5.16.0
- Active branch: day84t-chemplus-declarative-vsepr

## Strengthening the Pipeline
- Strengthen this pipeline:
  - formation → fingerprint → cluster → analysis → report → dashboard
- Do not invent unrelated subsystems.

## Toolchain
- Use only:
  - Compiler: C:/msys64/ucrt64/bin/g++.exe — GCC 15.2/UCRT64
  - Build: C:/msys64/ucrt64/bin/ninja.exe
  - CMake: C:/msys64/ucrt64/bin/cmake.exe
  - Configure through presets: cmake --preset release
  - Git/GitHub: C:\Program Files\GitHub CLI\gh.exe
  - Bash: C:\msys64\usr\bin\bash.exe -l
  - Standard: C++23
  - Canonical build directory: build/
- Treat CMakePresets.json as authoritative. Never use Make, MSVC, another CMake generator, git.exe, or WSL for Git operations. Prefer Bash/Linux-style commands; avoid PowerShell command composition unless required.

## Implementing VSIM Features
- For every feature:
  - Define: Add its field and default to include/vsim/vsim_document.hpp.
  - Parse: Wire its key through src/vsim/vsim_parser.cpp, normally via apply_*_key().
  - Wire: Apply it in runtime and demo applications, gated by its configured value.
  - Test: Add or extend tests and register them in tests/CMakeLists.txt.
  - Document: Update VSIM_REFERENCE.md, docs/VSIM_LANGUAGE.md, and VSIM_DEVELOPMENT.md.
- Never leave VSIM_REFERENCE.md stale after a schema change.
- For Day 84 onward, produce at least one PNG 3D export and verify that it exists during validation.
- For Days 88-92, each major daily item requires its own authoritative two-page LaTeX document:
  - Day 88 must finalize the worktree and update METHODOLOGY_12PAGE.tex, changing its font only after content updates are complete.
  - Day 89 focuses on VSIM visual stress testing above 1000 particles, expansion of all three 3D output systems, and renewed bead/near-meso work including resolved packets with mass and flux exchange.
- For Day 89 onward, prioritize delivered simulation capability:
  - Script expansion
  - Runtime ownership
  - Meaningful time advancement
  - Useful outputs
  - Visualization
- Retain useful Day 83 code, but do not let classification/provider abstractions control the architecture.
- Never hardcode element or atomic-number arrays when project data files already provide them.

## System Model
- Preserve these layers:
  - Input: names, formulas, aliases, scripts, presets, seed structures
  - Identity: canonical particle, molecule, and material identities; persistent and lineage IDs
  - Formation: generation, priors, relaxation, dynamics, energy tracking
  - State: position, velocity, orientation, time, events, energy traces
  - Analysis: Kabsch, RMSD, stationarity, defects, diffusion, packing, transport, macro inference
  - Classification: fingerprints, clusters, polymorphs, isomorphs, defect groups
  - Reporting: tables, figures, dashboards, SVG/PNG, warnings
  - Export: XYZ, xyzFull, CSV, JSON, XLSX, SVG, and reports
- Never use meso, mesoscopic, meso-scale, meso renderer, or meso model. Use atomistic, bead, coarse bead, premacro, macro, formation, trajectory, or analysis layer.

## Encoding Repairs
- Before bulk replacement, classify corruption as:
  - Unicode typography or box drawing → mapped ASCII
  - Double-encoded CP1252/UTF-8 math or Greek → ASCII names such as rho, alpha, or ~
  - U+FFFD/U+0081 corruption → contextual replacements
- Use explicit UTF-8 with [System.IO.File]::ReadAllText/WriteAllText. Use string Replace overloads, not character overloads. Finish by scanning and confirming zero remaining corruption hits.

## Viewer Behavior
- Interactive .vsim execution must open at least one viewer window, ideally a viewer plus status/report window. Headless execution is permitted only when explicitly requested with output_type = "none".
- Use:
  - terminal_chart for normal interactive runs
  - gl_interactive for controlled 3D
  - gl_live_60fps for smooth presentation
  - gl_crystal_grid for periodic structures
  - gl_overlay_cycle for cycling analysis overlays
- For frozen but visually spinning scenes, use MD mode with one step, disabled convergence, gl_spin = true, and gl_live_60fps. Export XYZ for replay.
- Treat gl_spin and gl_auto_orbit as mutually exclusive. Spin is viewer-side only and must never modify exported particle coordinates. A zero spin rate is a valid static view.

## Format Boundaries
- Treat .dynx as pipeline-generated, post-compiled output—never hand-authored source. It must contain state, forces, field vectors, event packets, render metadata, camera states, and provenance including source .vsim path, hash, timestamps, and seed.
- .xyz / .xyzFull: scientific and replay truth
- .dynx: live visual/session archive
- .X: bundled suite-execution container
- In short: preserve the scientific state as truth, keep visualization downstream, and update the damned reference whenever the schema changes—because apparently documentation cannot evolve by photosynthesis.

## Project Work (Days 87/88)
- Scope an overall work order first.
- Priorities: improve the VSIM-to-HTML pipeline and support typing input back into the same hosted browser window.
- Existing demonstrated capabilities include plain-text HTML console outputs and parallelized live sessions.

## Work Order Tracking (Days 91–100)
- Use `WO-<DAY><LETTER>-GR<READINESS_GATE>` only for work that directly contributes to Golden or Platinum readiness. Golden is below the final 98% threshold; Platinum requires objectively verified readiness greater than 98%.
- Readiness levels are DEVELOPMENT <90%, GOLDEN 90–98%, PLATINUM >98%; the percentage applies only to the work order's explicit acceptance criteria, never the whole gate.
- Required header markers are [W] work-order identity, [D] day, [R] readiness gate, [T] target, [B] baseline, [I] implementation, [V] verification, [P] publication/handoff, [N] next action/blocker.
- Preserve the existing [W][D][I][V][P][N] markers.
- Every [W] work-order entry must reference or include at least one authoritative LaTeX (.tex) document as supporting specification/evidence.

## Reporting and Visualization
- Prefer R as an alternative to Python plus third-party libraries when R is available in the deployment environment; keep the C++ simulation core independent of the reporting runtime.