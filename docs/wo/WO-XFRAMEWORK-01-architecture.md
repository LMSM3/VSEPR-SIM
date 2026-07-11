# WO-XFRAMEWORK-01 — `.x` / `.X` Framework Architecture & Desktop Environment
<!-- docs/wo/WO-XFRAMEWORK-01-architecture.md -->
<!-- Branch: feature/wizard-full-module-expansion | v5.0.14 -->

| Field      | Value                                           |
|------------|-------------------------------------------------|
| WO         | WO-XFRAMEWORK-01                                |
| Status     | REFERENCE — architecture mapped, gaps listed    |
| Relates to | WO-72A (xbundle), WO-72B (dynx), WO-72C (associations), WO-XSUITE-02B (live/presolve) |

---

## 1. Overview

The project uses **three distinct file formats** under the `.x`/`.X` umbrella, plus the
`.dynx` session archive. They are **not interchangeable** — each has its own parser and
serves a different stage of the pipeline.

| Extension | Kind | Parser namespace | Role |
|-----------|------|-----------------|------|
| `.x` (lowercase) | Package card *or* scene script | `xsim::xport::ExportXLoader` / vsim runtime | Export bundle declaration or frozen-scene visualizer |
| `.X` (uppercase, INI magic) | Saved run suite | `vsepr::xsuite::xsuite_parse()` (`include/vsim/xsuite.hpp`) | Top-level descriptor for a saved VSEPR-SIM run suite |
| `.X` (uppercase, XBUNDLE magic) | Bundle container | `vsim::xbundle::XBundleReader` (`include/xbundle/xbundle_reader.hpp`) | Multi-member text archive embedding `.vsim` scripts + assets |
| `.dynx` | Session archive | WO-72B | Post-compiled live visual / session archive (never hand-authored) |

---

## 2. `.x` (lowercase) — Package / Scene Card

### 2a. Export bundle (`kind = "xsim_export_bundle"`)

Declares which outputs to produce from a completed simulation run.

```
[package]
name    = "sio2_pipe_analysis_export"
kind    = "xsim_export_bundle"
version = "stable+0.0.1"

[run]
run_dir = "runs/sio2_pipe_case_001"      # completed run to export from

[export]
write_xyz                 = true
write_analysis_json       = true
write_metrics_tsv         = true
write_report_md           = true
write_events_json         = true
write_manifest_json       = true
write_verify_report       = true
output_dir                = "out/sio2_pipe_analysis/final_bundle"
```

- Parsed by: `packages/export/include/xsim/xport/xsim_xport.hpp` → `ExportXLoader`
- Mapped to: `xsim::xport::ExportConfig` (mirrors `vsim::ExportSection` field names)
- Library: `packages/export/` → `libxsim_xport.a` (static, self-contained)
- Status: **skeleton complete** — `XYZExporter` creates empty files; other exporters
  (analysis_json, metrics_tsv, report_md, events_json, manifest_json) are declared but
  not yet registered. See `EXPORT_PACKAGE_DESIGN.md` §5 for the 5-phase integration path.
- CLI: `xsim xport <file.x>`

Four export profiles are defined (mirroring real-world usage needs):

| Profile | Flags |
|---------|-------|
| `minimal` | `write_xyz` |
| `standard` | + `write_analysis_json`, `write_metrics_tsv`, `write_report_md` |
| `research_report` | + `write_events_json`, `write_manifest_json`, `write_dashboard_svg` |
| `publication` | + `write_symbolic_trace_json`, `write_pipeline_audit_jsonl` |

### 2b. Scene script (`kind = "xsim_scene"`)

A frozen-scene declaration with a `[visual]` block. Uses `.vsim`-like semantics.

```
[package]
name    = "cube_rotation"
kind    = "xsim_scene"

[scene]
formula     = "Fe"
prototype   = "A2_bcc"
count       = 54            # 3×3×3 BCC iron supercell

[run]
mode      = "md"
max_steps = 1
dt_fs     = 1.0
converge  = false           # physics frozen; viewer-only

[visual]
output_type       = "gl_live_60fps"
gl_spin           = true
gl_spin_axis      = "y"
gl_spin_deg_per_s = 40.0
gl_show_axes      = true

[export]
write_xyz           = true
write_manifest_json = true
output_dir          = "out/cube_rotation"
```

- Used by: `examples/rotation/cube.x`, `sphere.x`, `sphere_shaded.x`
- `max_steps = 1` + `converge = false` → one-step static scene; spin is viewer-only
- All `gl_spin_*` fields are viewer transforms — output `.xyz` is not rotated

### 2c. XSIM dependency script (master.x)

A third `.x` dialect used by `examples/xsim_bundles/ikk1_nh3/master.x`:

```
use package export
use package frames
use package identity

run dynamic
steps 100000
dt_fs 0.25

export xyz stride 10
export report_md true
```

This uses a REPL-like command format (`use`, `run`, `export`) rather than INI sections.
It is a **package dependency and run-control script** for the `xsim run` tool path.

---

## 3. `.X` (uppercase, INI style) — Saved Run Suite (`XSuiteFile`)

**Header**: `include/vsim/xsuite.hpp` — single-header, fully inline.
**Namespace**: `vsepr::xsuite`
**Parse entry**: `xsuite_parse(path)` → returns `XSuiteParseResult { ok, error, warnings, suite }`

### Required sections
```
[xsuite]   name, version, mode, created_by
[run]      entry (→ entry_script; required), mode, num_steps, dt
[files]    script, snapshot, xyza, checkpoint, trajectory, rich-replay
```

### Optional sections
| Section | Purpose |
|---------|---------|
| `[build]` | compile flag, compiler, target, config |
| `[hash]` | sha hashes for script, snapshot, checkpoint, trajectory, suite |
| `[actions]` | compile / run / validate / replay / export_outputs flags |
| `[outputs]` | report_path, json_path, log_path |
| `[live]` | persistent instance, state/health flush intervals |
| `[render]` | atomic/analysis stream URLs, fps |
| `[checkpoint]` | format, directory, interval, keep_last, write_hash |
| `[presolve]` | mode, batch_count, seeds_per_batch, etc. |
| `[eigenmine]` | target_solves, batch_count, mode_rank_limit, write_modes, etc. |
| `[curvefit]` | model, max_order, regularization, train_fraction, etc. |
| `[release_gate]` | baseline/candidate version, require_hash_success, etc. |

### Suite modes in the wild
| mode | Example file | Purpose |
|------|-------------|---------|
| `saved_run_suite` | *(general use)* | Standard run descriptor |
| `presolve_suite` | `eigenmine_10m.X`, `curvefit_presolve.X` | Long background pre-solve |
| `release_gate` | `release_gate_presolve.X` | CI/CD quality gate |

### CLI sub-commands
```
vsepr x inspect   <file.X>    # formatted summary + file status
vsepr x validate  <file.X>    # file presence + hash contract checks
vsepr x run       <file.X>    # compile (if enabled) then run entry script
vsepr x replay    <file.X>    # open replay in desktop viewer
vsepr x export    <file.X>    # regenerate reports/artifacts
vsepr x open      <file.X>    # double-click → 2 CMD + Qt desktop (WO-72A)
```

### Utility functions (inline in xsuite.hpp)
- `xsuite_inspect(suite, out)` — prints formatted summary with `[OK]`/`[MISSING]` file status
- `xsuite_validate(suite, out)` → int (0 = pass) — enforces required file presence

---

## 4. `.X` (uppercase, XBUNDLE format) — Bundle Container

**Magic line**: `XBUNDLE 1`
**Header**: `include/xbundle/xbundle_document.hpp`
**Namespace**: `vsim::xbundle`
**Work order**: WO-72A

### File layout
```
XBUNDLE 1
[manifest]
name         = <string>
description  = <string>         (optional)
author       = <string>         (optional)
created      = <ISO-8601>        (optional)
entry_count  = <uint>            (informational)
entry_point  = <member_name>    (optional; first vsim entry is default)

[[member]]
name         = run_a
kind         = vsim             (or: asset)
path         = scripts/run_a.vsim  (informational)
size         = 1024              (optional; validated if present)
>>>
[project]
name = "run_a"
...full .vsim content here...
<<<
```

### In-memory model
```cpp
XBundle {
	XBundleManifest manifest;           // name, description, author, entry_point
	vector<XBundleEntry> entries;       // name, kind(vsim|asset), content
	string source_path;

	const XBundleEntry* entry_point();  // honours manifest.entry_point or first vsim
	const XBundleEntry* find(name);
}
```

### API
```cpp
// Read
auto b = XBundleReader::read_file("suite.X");
auto b = XBundleReader::read_string(src, "suite.X");

// Write
XBundleWriter::write_file(bundle, "suite.X");
string text = XBundleWriter::write_string(bundle);

// Validate (8 rules V-01..V-08)
auto errors = XBundleValidator::validate(bundle);  // empty = valid
```

### Validation rules
| Rule | Check |
|------|-------|
| V-01 | `manifest.populated` must be true |
| V-02 | `manifest.name` must not be empty |
| V-03 | At least one entry must exist |
| V-04 | Every entry must have a non-empty name |
| V-05 | Entry names must be unique |
| V-06 | Every vsim entry must have non-empty content |
| V-07 | `entry_point` (if set) must name an existing entry |
| V-08 | `declared_size` (if non-zero) must match actual byte count |

### Tests
- `tests/test_xbundle_smoke.cpp` — Group 70 (XB-01..XB-10): full write → serialize → read → validate pipeline
- Registered in `tests/CMakeLists.txt` line ~1412 as `XBundleSmokeGroup70`
- `tests/test_xsuite_parse.cpp` — XP1..XP5: minimal valid, missing entry, optional trajectory, hash ignored, action flags
- **NOT YET registered in CMakeLists.txt** (file exists, no `add_executable` entry)

---

## 5. 3D Visual Desktop Environment (`vsepr-desktop`)

### What exists

| Component | File(s) | Status |
|-----------|---------|--------|
| Main window + docks | `apps/desktop/MainWindow.cpp/.h` (49 KB) | ✅ Built |
| 3D viewport | `apps/desktop/ViewportWidget.cpp/.h` | ✅ Built |
| Scene model | `apps/desktop/scene/SceneDocument.h` | ✅ Built |
| Engine bridge | `apps/desktop/bridge/EngineAdapter.cpp/.h` | ✅ Built |
| Run/Stop panel | `apps/desktop/RunVsimPanel.cpp/.h` | ✅ Built |
| Trajectory panel | `apps/desktop/TrajectoryPanel.cpp/.h` | ✅ Built |
| .dynx replay panel | `apps/desktop/DynxReplayPanel.cpp/.h` | ✅ Built |
| File network panel | `apps/desktop/FileNetworkPanel.cpp/.h` | ✅ Built |
| Demo cards panel | `apps/desktop/DemoCardsPanel.cpp/.h` | ✅ Built |
| Script editor panel | `apps/desktop/ScriptEditorPanel.cpp/.h` | ✅ Built |
| IPC server | `apps/desktop/DesktopIpcServer.cpp/.h` | ✅ Built |
| Showroom mode | `apps/desktop/ShowroomMode.cpp/.h` | ✅ Built |
| Object tree | `apps/desktop/ObjectTree.cpp/.h` | ✅ Built |
| Properties panel | `apps/desktop/PropertiesPanel.cpp/.h` | ✅ Built |
| Console panel | `apps/desktop/ConsolePanel.cpp/.h` | ✅ Built |
| Event log panel | `apps/desktop/EventLogPanel.cpp/.h` | ✅ Built |
| Live command panel | `apps/desktop/LiveCommandPanel.cpp/.h` | ✅ Built |
| Hardware monitor | `apps/desktop/HardwareMonitorPanel.cpp/.h` | ✅ Built |
| **Compiled binary** | `build/vsepr-desktop.exe` | ✅ EXISTS |

### ViewportWidget — 3D renderer specifics

- Inherits: `QOpenGLWidget` + `QOpenGLFunctions_3_3_Core`
- Camera: orbit (left-drag), pan (right/mid), scroll-zoom, F=fit, R=reset, W=wireframe
- Rendering: sphere mesh (subdivided) + cylinder mesh for bonds; Phong shading; cached
  view/proj/VP matrices; atom color + radius from element Z lookup
- Trajectory: `QTimer`-driven playback at configurable fps (default 24); `stepForward/Back`,
  `setFrameIndex`
- Overlays: `TransientOverlay` (WO-63A: electron/ion-like particles), `QCDOverlay` (v5.1.3~2:
  quarks, gluon rainbow trails, confinement strings)
- Screenshot: `grabScreenshot(path)` — saves GL framebuffer to PNG/JPG/BMP

### SceneDocument model

```
SceneDocument
  frames[]             → FrameData[]
	atoms[]            → AtomRecord { Z, symbol, pos, label, tag }
	bonds[]            → BondRecord { a, b, order, length }
	velocities[]       → Vec3d (Å/fs)
	forces[]           → Vec3d (kcal/mol·Å)
	charges[]          → double (e)
	box                → BoxInfo { a, b, c, alpha, beta, gamma, enabled }
	transients[]       → TransientOverlay (WO-63A)
	qcd_overlays[]     → QCDOverlay (v5.1.3~2)
	properties{}       → map<string, double>
	step, time, source_mode
  properties{}         → map<string, PropertyValue>
  provenance           → Provenance { mode, source_file, formula, hash, parameters{} }
```

### Build wiring

`cmake/VisBuild.cmake` lines 394–456:
```cmake
option(BUILD_DESKTOP "Build Qt-based desktop application" ON)
if(BUILD_DESKTOP)
	find_package(Qt6 COMPONENTS Widgets OpenGLWidgets Network QUIET)
	if(Qt6_FOUND)
		set(CMAKE_AUTOMOC ON)
		add_executable(vsepr-desktop
			apps/desktop/main.cpp
			apps/desktop/MainWindow.cpp
			apps/desktop/ViewportWidget.cpp
			# ... all 14 panels + bridge ...
		)
		target_link_libraries(vsepr-desktop PRIVATE
			Qt6::Widgets Qt6::OpenGLWidgets Qt6::Network
			vsepr_infra vsepr_sim vsepr_io vsepr_core coarse_grain
			pdh psapi  # Windows-only
		)
		install(TARGETS vsepr-desktop DESTINATION bin)
	endif()
endif()
```

### Qt Launcher (`vsepr-launcher`)

Separate, lighter Qt app — no OpenGL, no 3D. The double-click handler for `.vsim` and `.X` files.

```
apps/launcher/
  LauncherWindow    — main window; routes to validate/run/open-in-desktop
  FileInfoPanel     — shows .vsim / .X file metadata
  ConsolePanel      — live stdout/stderr from vsepr CLI subprocess
  ArtifactBrowser   — lists output artifacts after a run
  xsuite_launcher.bat — batch fallback: opens 3 CMD windows + vsepr-desktop
```

**`vsepr-launcher.exe`** — `build/vsepr-launcher.exe` ✅ EXISTS

---

## 6. Installer (`installer/setup.iss`)

**Format**: Inno Setup 6 — compiled with `iscc installer\setup.iss`
**Output**: `installer/output/vsepr-sim-5.0.0-setup.exe`

### Current installer feature set
- Default install dir: `{autopf}\VSEPR-SIM`
- No admin required (PrivilegesRequired=lowest)
- Post-install: optional file associations via `register-file-associations.ps1`
- Optional PATH addition
- Uninstaller included

### What the installer packages (as authored)

| Source | Dest | Status |
|--------|------|--------|
| `build\vsepr.exe` | `{app}\bin\` | ✅ Exists |
| `build\vsepr-cli.exe` | `{app}\bin\` | ❌ **MISSING** — binary is `vsepr.exe`, not `vsepr-cli.exe` |
| `build\vsepr_batch.exe` | `{app}\bin\` | ✅ Exists |
| `build\live-xyza-viewer.exe` | `{app}\bin\` | ❌ **MISSING** — requires `BUILD_VIS=ON` (GLFW+GLEW); not in current release preset |
| `build\vsepr-desktop.exe` | `{app}\bin\` | ✅ Exists (flagged `skipifsourcedoesntexist`) |
| `include\vsim\*` | `{app}\include\vsim\` | ✅ Exists |
| `data\*` | `{app}\data\` | ✅ Exists |
| `scripts\*` | `{app}\scripts\` | ✅ Exists |
| `docs\*` | `{app}\docs\` | ✅ Exists |
| `resources\vsepr.ico` | `{app}\resources\` | ✅ Exists |
| Qt runtime DLLs | — | ❌ **NOT PACKAGED** — no `windeployqt` step |
| `build\vsepr-launcher.exe` | — | ❌ **NOT IN INSTALLER** — binary exists, not added |

### Start menu entries authored
- `{group}\VSEPR-SIM` → `vsepr.exe`
- `{group}\VSEPR-SIM CLI` → `vsepr-cli.exe` (❌ wrong name)
- `{autodesktop}\VSEPR-SIM` → `vsepr.exe` (optional task)

---

## 7. File Association Chain

Managed by `installer/register-file-associations.ps1` (HKCU, no admin).
Also: `apps/launcher/register_associations.bat` / `register_x_extension.bat`.

| Extension | Default action | Verbs |
|-----------|---------------|-------|
| `.vsim` | `vsepr run "%1"` | Open, Validate, Inspect |
| `.X` | `vsepr x run "%1"` | Open, Inspect, Validate |
| `.dynx` | `vsepr view "%1"` | Open, Validate, Inspect |
| `.xyzFull` | `vsepr view "%1"` | Open, Inspect |
| `.vsxyz` `.xyza` `.xyzc` `.xyzf` | `vsepr open "%1"` | — |
| `.xyz` | **conservative** — context menu only; existing default preserved | Open with VSEPR-SIM |

Double-click on `.X` also supported by `xsuite_launcher.bat`:
1. CMD window: `vsepr x run "<file.X>"`
2. CMD window: validate + status
3. `vsepr-desktop.exe --xsuite "<file.X>"`

---

## 8. Gaps — What Does Not Yet Exist for Installation Media

### Gap 1 (BLOCKER): `installer/output/` — installer never compiled
The `installer/setup.iss` file is complete and correct in structure, but
`iscc installer\setup.iss` has never been run. The output directory does not exist.
The `.exe` installer for end users does not exist.

### Gap 2 (BLOCKER): Qt runtime DLLs not bundled
`vsepr-desktop.exe` and `vsepr-launcher.exe` require Qt6 DLLs and platform plugins at
runtime (`Qt6Widgets.dll`, `Qt6OpenGLWidgets.dll`, `Qt6Network.dll`,
`platforms/qwindows.dll`, OpenGL DLL, etc.).

**Required step** before installer can be built:
```powershell
# Run from the repo root after a successful build
& "C:\msys64\ucrt64\bin\windeployqt6.exe" `
	--dir installer\qt_runtime `
	build\vsepr-desktop.exe build\vsepr-launcher.exe
```
Then add to `setup.iss` `[Files]`:
```iss
Source: "installer\qt_runtime\*"; DestDir: "{app}\bin"; Flags: ignoreversion recursesubdirs
```

### Gap 3 (BLOCKER): `vsepr-cli.exe` name mismatch
`setup.iss` line 87 references `build\vsepr-cli.exe` but the binary is `build\vsepr.exe`.
The start-menu CLI shortcut also points to `vsepr-cli.exe`.

Fix options (pick one):
- **A** — Change setup.iss to reference `build\vsepr.exe` and rename the icon to "VSEPR-SIM (CLI)"
- **B** — Add `set_target_properties(vsepr PROPERTIES OUTPUT_NAME "vsepr-cli")` in CMakeLists
  and keep setup.iss as-is

### Gap 4 (NICE-TO-HAVE): `live-xyza-viewer.exe` not built
The GLFW/GLEW viewer requires `BUILD_VIS=ON`. The `release` preset does not enable this.
setup.iss uses `skipifsourcedoesntexist` so the installer builds without it, but the
file-association fallback chain for `.xyza` files loses its primary handler.

Fix: Either add a `release-vis` CMake preset or remove the fallback-chain dependency.

### Gap 5 (NICE-TO-HAVE): `vsepr-launcher.exe` not in installer
The Qt Lite launcher exists (`build\vsepr-launcher.exe`) and handles `.vsim`/`.X`
double-clicks, but it is not included in setup.iss `[Files]` or `[Icons]`.

### Gap 6 (TRACKED): `test_xsuite_parse.cpp` not registered in CMakeLists
`tests/test_xsuite_parse.cpp` (XP1..XP5) exists but has no `add_executable` / `vsepr_add_test`
entry in `tests/CMakeLists.txt`. It cannot be run via CTest.

### Gap 7 (TRACKED): `xsim::xport` skeleton only
`packages/export/` compiles cleanly but `XYZExporter` creates an empty file.
Five exporter implementations pending (analysis_json, metrics_tsv, report_md, events_json,
manifest_json). See `EXPORT_PACKAGE_DESIGN.md` §5 for the phased integration plan.

---

## 9. Recommended Next Steps (in priority order)

| # | Action | Effort |
|---|--------|--------|
| 1 | Fix `vsepr-cli.exe` name mismatch in setup.iss (Gap 3) | 5 min |
| 2 | Add `vsepr-launcher.exe` to setup.iss [Files] + [Icons] (Gap 5) | 15 min |
| 3 | Run `windeployqt6` and add Qt runtime [Files] entry (Gap 2) | 30 min |
| 4 | Install Inno Setup 6 and run `iscc installer\setup.iss` (Gap 1) | 10 min |
| 5 | Register `test_xsuite_parse.cpp` in CMakeLists.txt (Gap 6) | 10 min |
| 6 | Implement `XYZExporter` Phase 1 (Gap 7) | 2–4 hrs |

Steps 1–4 together produce the first distributable `vsepr-sim-5.0.0-setup.exe`.

---

## 10. Format Role Summary

```
.vsim      Single simulation script (source, always hand-authored)
.x         Package card: export bundle declaration or frozen-scene script
.X         Suite container: saved run descriptor (INI) or bundle archive (XBUNDLE)
.dynx      Post-compiled session archive (never hand-authored; emitted by pipeline)
.xyz       Scientific trajectory (plain XYZ)
.xyzFull   Rich replay (extended XYZ with all per-atom fields)
.xyzc      Checkpoint
.xyzf      Trajectory (force-annotated)
.xyza      Annotated XYZ (analysis overlays)
```

*This document is the authoritative architecture reference for the `.x` framework and
the desktop environment distribution gap. Update the "Gap" section as items are closed.*
