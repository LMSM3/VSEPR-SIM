# VSEPR-SIM Live Console Reference

**Panel:** `Live Command` tab in the bottom dock of the Qt workstation  
**File:** `apps/desktop/LiveCommandPanel.h/.cpp`  
**Parser:** `apps/desktop/LiveCommandParser.hpp`  
**Dispatch:** `MainWindow::onLiveCommand()` in `apps/desktop/MainWindow.cpp`

---

## Overview

The Live Console provides a real-time REPL (read-eval-print loop) embedded in
the Qt workstation.  Commands typed here take effect immediately — no re-run,
no batch script, no restart.

Two grammars are accepted interchangeably:

| Grammar | Example |
|---|---|
| Imperative (shell-like) | `set temperature 400` |
| vsim section-key assignment | `simulation.temperature = 400` |

Both resolve to the same internal `live::LiveCommand` value and dispatch
through the same path.

---

## UI Controls

| Element | Behaviour |
|---|---|
| `vsim>` prompt | Type any command and press **Enter** |
| ↑ / ↓ arrow keys | Cycle command history (de-duplicated, session-scoped) |
| Inline hint bar | Shows matching parameter key names as you type |
| Output pane | Colour-coded: cyan = result, red = error, dim = info, italic = echo |
| `help` / `?` | Renders this section inline without leaving the panel |

---

## Commands

### Simulation

| Command | Aliases | Description |
|---|---|---|
| `relax` | `fire`, `min`, `minimize` | FIRE energy minimization |
| `md` | `nvt`, `dynamics` | Langevin NVT molecular dynamics |
| `sp` | `energy`, `single`, `singlepoint` | Single-point energy evaluation |

---

### Parameters

Parameters can be set with either grammar at any time.
Changes to `simulation.*` and `formation.*` keys are stored in the document
provenance and applied on the next run.  `visual.*` and `playback.*` keys
affect the running UI immediately.

#### Imperative syntax

```
set <key> <value>
```

#### vsim assignment syntax

```
section.key = value
```

#### Parameter table

| Short key (imperative) | Canonical key (vsim) | Range | Unit | Description |
|---|---|---|---|---|
| `temperature` / `temp` / `t` | `simulation.temperature` | 0 – 10 000 | K | NVT thermostat temperature |
| `steps` / `nsteps` | `simulation.steps` | 1 – 1 000 000 | — | Simulation step count |
| `dt` / `timestep` | `simulation.dt` | 0.001 – 10 | fs | Integration time step |
| `force_tol` / `ftol` / `tol` | `simulation.force_tol` | 1e-8 – 1 | kcal/mol/Å | FIRE convergence criterion |
| `render` / `render_interval` / `interval` | `visual.render_interval` | 1 – 1 000 | steps | Emit render every N steps |
| `display_fps` / `dfps` | `visual.display_fps` | 1 – 240 | fps | UI refresh rate |
| `fps` / `playback_fps` | `playback.fps` | 1 – 120 | fps | Trajectory playback speed |
| `ftemp` / `formation_temp` | `formation.temperature` | 0 – 10 000 | K | Formation thermostat temperature |
| `fsteps` / `formation_steps` | `formation.steps` | 1 – 1 000 000 | — | Formation step count |
| `cutoff` / `rcut` / `rc` | `analysis.cutoff` | 0.5 – 20 | Å | Neighbour graph cutoff radius |
| `ref` / `rmsd_ref` | `analysis.rmsd_ref` | 0 – 99 999 | frame | RMSD reference frame index |
| `diffusion_window` / `msd_window` | `analysis.diffusion_window` | 1 – 10 000 | steps | MSD window size |

**Examples:**

```
set temperature 600
simulation.temperature = 600

set tol 1e-6
simulation.force_tol = 1e-6

set cutoff 4.2
analysis.cutoff = 4.2
```

---

### Trajectory / Playback

| Command | Aliases | Description |
|---|---|---|
| `play` | — | Start automatic playback |
| `pause` | `stop` | Pause playback |
| `next` | `>>`, `f`, `forward` | Step one frame forward |
| `prev` | `<<`, `b`, `back`, `backward` | Step one frame back |
| `goto <N>` | `go <N>`, `seek <N>` | Jump to frame N (0-based) |

**Examples:**

```
play
goto 100
next
```

---

### Camera / View

| Command | Aliases | Description |
|---|---|---|
| `reset` | — | Reset camera to default position |
| `fit` | `zoom`, `zoomfit` | Fit camera to current structure bounding box |
| `wireframe` | `wire`, `wf` | Toggle wireframe rendering |

---

### File I/O

#### `load <path>`

Open a structure file (XYZ, XYZF, VSIM).  Opens the file dialog if no path
is given.

**Path formats — all accepted:**

```
load C:\sim\output\frame_final.xyz
load C:/sim/output/frame_final.xyz
load "C:\My Simulation Data\molecule with spaces.xyz"
load 'C:/Users/me/molecule.xyz'
load molecule.xyz
```

Rules:
- **Bare path** — no spaces in path, backslash or forward-slash both OK.
- **Double-quoted path** — use when the path contains spaces.
- **Single-quoted path** — alternative quoting style.
- **Relative path** — resolved relative to the working directory.
- **Drag-and-drop** — paste the path from Explorer; leading/trailing
  whitespace is stripped automatically.
- Qt normalises separators internally via `QDir::fromNativeSeparators`.

Aliases: `open`, `import`

#### `reload`

Reload the currently open file from disk.  Useful after an external tool
has modified the file.

Alias: `refresh`

#### `export <path>`

Export the current frame to an XYZ file.

```
export C:\results\final_frame.xyz
export "C:\My Results\output.xyz"
```

Alias: `save`

#### `screenshot [path]`

Save a PNG/image of the current viewport.  Opens a save dialog if no path
is given.

```
screenshot
screenshot C:\results\view.png
screenshot "C:\My Results\frame_100.png"
```

Aliases: `ss`, `snap`, `capture`

---

### Document Info

| Command | Aliases | Description |
|---|---|---|
| `info` | `status`, `doc`, `document` | Print frames, atom count, formula, mode, source file |

---

### Console

| Command | Aliases | Description |
|---|---|---|
| `clear` | `cls` | Clear the output pane and log |
| `help` | `?`, `h` | Show inline command reference |

---

## Architecture

```
User types in LiveInputEdit
		│
		▼
LiveCommandPanel::onReturn()
		│
		├─ "help" → renders helpText() inline, emits commandParsed
		│
		└─ live::parse(rawLine)
				│
				├─ vsim grammar:   "section.key = value"
				├─ imperative:     "set key value"
				├─ path-bearing:   "load <path>" (bare / quoted / backslash)
				└─ op keywords:    "relax", "play", "goto 42", …
				│
				└─ ParseResult { LiveCommand, error }
						│
						├─ error → postError() + emit rawError()
						└─ ok    → emit commandParsed(LiveCommand)
										│
										▼
							  MainWindow::onLiveCommand()
										│
							  dispatches to viewport, engine, doc, …
```

### Key types

```cpp
namespace live {
	enum class Op { Relax, MD, SinglePoint, Play, Pause, StepForward,
					StepBack, GotoFrame, SetPlaybackFps, ResetCamera,
					FitCamera, ToggleWireframe, LoadFile, Reload,
					ExportFile, Screenshot, Info, Clear, Help,
					SetParam, Unknown };

	struct LiveCommand {
		Op      op;       // what to do
		QString key;      // SetParam: canonical parameter key
		double  value;    // SetParam / GotoFrame: numeric argument
		QString strval;   // LoadFile / ExportFile / Screenshot: path
		QString raw;      // original input line
	};
}
```

### Adding a new command

1. Add an `Op` value to the `enum class Op` in `LiveCommandParser.hpp`.
2. Add a keyword branch inside `live::parse()`.
3. Add a `case Op::YourOp:` block in `MainWindow::onLiveCommand()`.
4. Update `live::helpText()` and this document.

### Adding a new parameter key

1. Add a `ParamDef` row to `KNOWN_PARAMS[]` in `LiveCommandParser.hpp`.
2. Add short aliases to the `ALIASES[]` table in `detail::normaliseKey()`.
3. Add a dispatch branch inside the `case Op::SetParam:` block in
   `MainWindow::onLiveCommand()`.
4. Update the parameter table in this document.

---

## Hardware Monitor Panel

The `Hardware` tab (tabified alongside `Console` and `Live Command`) shows
live system resource telemetry, updated every second.

| Row | Source | Notes |
|---|---|---|
| CPU % | `GetSystemTimes` (Windows) / `/proc/stat` (Linux) | System-wide, all cores |
| RAM used / total | `GlobalMemoryStatusEx` / `sysinfo` | Physical RAM |
| proc RAM | `GetProcessMemoryInfo` (Windows) / `/proc/self/status` | This process only |
| GPU % | PDH `GPU Engine(*)\Utilization Percentage` (Windows) / `nvidia-smi` (Linux) | Row hidden if unavailable |

The hardware panel is implemented in `apps/desktop/HardwareMonitorPanel.h/.cpp`
and requires no external GPU vendor SDK — it uses the PDH counter interface
available in all modern Windows versions.

---

*Updated: beta-7 — Live Console + Hardware Monitor integration*
