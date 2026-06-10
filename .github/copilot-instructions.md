# VSEPR-SIM Copilot Instructions
<!-- v5.0.14 | branch: v5.0.0-main -->

## Project identity

VSEPR-SIM is a deterministic atomistic simulation, analysis, and reporting platform.
Current release: **v5.0.14**. Active branch: `v5.0.0-main`.
Goal: research-grade digital-twin pipeline — formation → fingerprint → cluster → analysis → report → dashboard.

Not a toy. Not a demo. Every module must strengthen the research kernel.

---

## Toolchain (non-negotiable)

| Tool | Path |
|---|---|
| Compiler | `C:/msys64/ucrt64/bin/g++.exe` (GCC 15.2, UCRT64) |
| Build | `C:/msys64/ucrt64/bin/ninja.exe` — sole generator, no Make, no MSVC |
| CMake | `C:/msys64/ucrt64/bin/cmake.exe` — always via preset (`cmake --preset release`) |
| Git/GitHub | GitHub CLI only: `C:\Program Files\GitHub CLI\gh.exe` — no git.exe, no WSL for commits |
| Shell | PowerShell: use `;` not `&&`. No `head` — use `Select-Object -First N` |
| Bash | `C:\msys64\usr\bin\bash.exe -l` — available for build/test scripting |
| Standard | C++23 (`-std=c++23`). C++26 features tracked but not yet required |

CMakePresets.json is the single source of truth for configure/build. `build/` is the canonical output dir.

---

## Developer procedure — every new VSIM feature

1. **Define** — add field + default to `include/vsim/vsim_document.hpp`
2. **Parse** — wire key in `src/vsim/vsim_parser.cpp` (`apply_*_key()`)
3. **Wire** — apply in runtime/demo apps; gate on field value
4. **Test** — create/extend group in `tests/`; register in `tests/CMakeLists.txt`
5. **Document** — update `VSIM_REFERENCE.md`, `docs/VSIM_LANGUAGE.md`, `VSIM_DEVELOPMENT.md`

`VSIM_REFERENCE.md` must be updated with every schema change. No exceptions.

---

## Encoding cleanup standard

Three-class triage before any bulk replace:
1. Box-drawing / typography Unicode → ASCII-safe equivalents via replacement map
2. Double-encoded CP1252→UTF-8 symbols (Greek, math) → ASCII names (`rho`, `alpha`, `~`)
3. Hard `U+FFFD` / `U+0081` corruption → contextual replacement (`-`, `A`, `""`)

Always use `[System.IO.File]::ReadAllText/WriteAllText` with explicit UTF-8. Use string `Replace` overloads, not char overloads (empty replacement throws). Final verification scan must confirm zero hits before closing.

---

## Forbidden terminology

Never use: `meso`, `mesoscopic`, `meso-scale`, `meso renderer`, `meso model`

Use instead: `atomistic`, `bead`, `coarse bead`, `premacro`, `macro`, `formation`, `trajectory`, `analysis layer`

---

## System layers

| Layer | Contents |
|---|---|
| Input | names, formulas, aliases, scripts, presets, seed structures |
| Identity | canonical/particle/molecular/material identity, persistent IDs, lineage IDs |
| Formation | structure gen, priors, relaxation, dynamics, energy tracking |
| State | positions, velocities, orientations, time/event history, energy traces |
| Analysis | Kabsch, RMSD, stationarity, defect emergence, diffusion, packing, transport, macro inference |
| Classification | fingerprints, clustering, polymorph/isomorph/defect grouping |
| Reporting | tables, figures, dashboards, SVG/PNG, validation warnings |
| Export | xyz, xyzFull, CSV, JSON, XLSX, SVG, report documents |

---

## Key files

| File | Purpose |
|---|---|
| `include/vsim/vsim_document.hpp` | Schema structs and authoritative defaults |
| `src/vsim/vsim_parser.cpp` | `.vsim` key-to-field wiring |
| `VSIM_REFERENCE.md` | Living field reference — update with every change |
| `docs/VSIM_LANGUAGE.md` | Canonical `.vsim` language and section guide |
| `VSIM_DEVELOPMENT.md` | 5-step add/wire/test/document checklist |
| `STAGE.md` | Master stage and gate ledger |
| `CMakePresets.json` | Authoritative Ninja build presets |
| `build.ps1` | Top-level build wrapper (delegates to CMakePresets) |
| `.github/copilot-instructions.md` | This file |

---

## Do not

- Hardcode element/Z arrays — the data files exist; use them
- Invent subsystems outside the current pipeline arc
- Use WSL for git operations
- Use any CMake generator other than Ninja
- Leave `VSIM_REFERENCE.md` out of date after a schema change

---

## Viewer / window rules

Every `.vsim` script execution must open **at least one, ideally two** popup/viewer windows (e.g. the lightweight viewer + a status or report window). Silent headless-only runs are not acceptable as the default behaviour for `.vsim` scripts.

### Visualizer user guide

The visualizer is the interactive 3D display layer. It is activated by the `[visual]` block in any `.vsim` script. Key concepts:

**Output types** (`output_type` field)

| Value | When to use |
|---|---|
| `"none"` | Headless batch runs — no display |
| `"terminal_chart"` | Default for all interactive runs — live convergence trace |
| `"gl_interactive"` | Full 3D interactive viewer with ImGui controls (requires `BUILD_VISUALIZATION`) |
| `"gl_live_60fps"` | Smooth 60 fps 3D view; ideal for presentations and spinning demos |
| `"gl_crystal_grid"` | Periodic crystal structures — shows unit cell and repeats |
| `"gl_overlay_cycle"` | Cycling overlay panels (density, coordination, energy) |

**Camera / spin controls**

| Field | Type | Default | Notes |
|---|---|---|---|
| `gl_auto_orbit` | bool | `false` | Camera orbits the scene between overlay panels |
| `gl_spin` | bool | `false` | Continuously spin the scene at a fixed rate |
| `gl_spin_axis` | string | `"y"` | Spin axis: `"x"`, `"y"`, or `"z"` |
| `gl_spin_deg_per_s` | float | `30.0` | Spin rate in degrees/second; negative = reverse |
| `gl_show_axes` | bool | `true` | Show XYZ coordinate axes in the GL window |
| `gl_window_width` | int | `1280` | GL window width in pixels |
| `gl_window_height` | int | `800` | GL window height in pixels |

**Writing a static-scene visualizer script**

For a scene that is physically frozen (no force integration) but visually spinning:
1. Set `[run] mode = "md"` with `max_steps = 1` and `converge = false` — one step only; no dynamics.
2. Set `[visual] gl_spin = true` and choose `gl_spin_deg_per_s`.
3. Use `output_type = "gl_live_60fps"` for the smoothest spin.
4. Set `[export] write_xyz = true` so the static geometry is saved for replay.

**Minimal hydrogen atom template**

```vsim
[project]
name    = "h_atom_spin"
version = "v5.0.0"

[material]
formula   = "H"
prototype = "noble_gas"
phase     = "gas"

[run]
mode      = "md"
max_steps = 1
dt_fs     = 1.0
converge  = false

[[simulation.molecule]]
formula     = "H"
count       = 1
temperature = 0.0
lattice     = "none"

[export]
write_xyz  = true
output_dir = "out/h_atom"

[visual]
output_type      = "gl_live_60fps"
gl_spin          = true
gl_spin_axis     = "y"
gl_spin_deg_per_s = 45.0
gl_show_axes     = true
gl_window_width  = 960
gl_window_height = 720
```

**Rules for all visualizer scripts**
- `gl_spin = true` disables `gl_auto_orbit` for that run (the two are mutually exclusive).
- `gl_spin_deg_per_s = 0.0` is valid and produces a static frozen view.
- The spin is a **viewer-side** transform only — particle positions in the output `.xyz` are not rotated.
- `render_interval` controls how often the display is refreshed relative to simulation steps. For a 1-step static scene, it has no effect.

---

## .dynx format rules

`.dynx` is a **post-compiled artifact** — it must be emitted by the simulation pipeline as a compiled output file. It is never hand-authored source.

Required contents of every `.dynx` file:

- Particle positions, velocities, orientations
- Force vectors and bond-force vectors
- Field vectors (flux, stress, etc.)
- Event packets (reaction events, checkpoints, phase transitions)
- Render metadata (color overrides, bead tags, visibility flags)
- Camera states (saved view angles / zoom levels)
- Simulation provenance (source `.vsim` path, hash, timestamps, seed)

Role separation (non-negotiable):

| Format | Role |
|---|---|
| `.xyz` / `.xyzFull` | Scientific state / replay truth |
| `.dynx` | Live visual / session archive (post-compiled) |
| `.X` | Bundled suite execution container |
