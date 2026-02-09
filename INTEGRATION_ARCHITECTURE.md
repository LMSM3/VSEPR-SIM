# INTEGRATION ARCHITECTURE
# TUI → GUI → WindowManager → Crystal

## System Overview

```
┌─────────────────────────────────────────────────────────────┐
│ vsepr-panel.ps1 (PowerShell launcher)                       │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  ┌────────────────┐         ┌──────────────────┐            │
│  │  tui.py        │ ◄─────► │  vsepr-gui.exe   │            │
│  │  (Command      │  stdin  │  (ImGui/OpenGL)  │            │
│  │   Backend)     │  stdout │                  │            │
│  └────────────────┘         └──────────────────┘            │
│         │                            │                       │
│         │                            │                       │
│         ▼                            ▼                       │
│  ┌────────────────┐         ┌──────────────────┐            │
│  │  Python Tools  │         │  WindowManager   │            │
│  │  - scoring     │         │  - 8 ViewModels  │            │
│  │  - classif.    │         │  - Snap/Free/FS  │            │
│  │  - formation   │         │  - Workspace     │            │
│  │  - baseline    │         │    (60-70%)      │            │
│  └────────────────┘         └──────────────────┘            │
│                                      │                       │
│                                      ▼                       │
│                             ┌──────────────────┐            │
│                             │  Crystal Objects │            │
│                             │  - xyzZ/A/C      │            │
│                             │  - Provenance    │            │
│                             │  - Watch system  │            │
│                             └──────────────────┘            │
│                                      │                       │
│                                      ▼                       │
│                             ┌──────────────────┐            │
│                             │  Visualization   │            │
│                             │  - Ball & stick  │            │
│                             │  - Surfaces      │            │
│                             │  - Crystal grid  │            │
│                             └──────────────────┘            │
└─────────────────────────────────────────────────────────────┘
```

---

## Component Breakdown

### 1. TUI (Command Backend)

**File:** `tui.py` (375 lines)

**Role:** Command registry + execution

**Interface:**
```python
# Commands exposed:
[1] Build all
[2] Build status
[3] Run ctest
[4-6] Validation (problem1, problem2, QA)
[7-10] CLI Tools (vsepr, meso-sim, meso-discover, meso-build)
[11-12] Python Tools (scoring, classification)
[13-16] Data Collection (formation, baseline, view, compare)
[s] Shell
[q] Quit
```

**Communication:**
- Stdin: Commands from GUI
- Stdout: Results + status
- Stderr: Errors

---

### 2. GUI (Main Application)

**Executable:** `vsepr-gui.exe` (to be built)

**Framework:** ImGui + OpenGL (existing card_viewer.cpp as base)

**Layout:**

```
┌──────────────────────────────────────────────────────────────┐
│ Menu Bar                                              [─][□][×]│
├──────────────────────────────────────────────────────────────┤
│ ┌────────────────────────────────────┬──────────────────────┐│
│ │ Workspace (65% width)              │ Instrument Stack     ││
│ │                                    │                      ││
│ │  ┌──────────┬──────────┐           │ ┌──────────────────┐││
│ │  │          │          │           │ │ Command Panel    │││
│ │  │  TL      │  TR      │           │ │  (TUI wrapper)   │││
│ │  │          │          │           │ └──────────────────┘││
│ │  ├──────────┼──────────┤           │ ┌──────────────────┐││
│ │  │          │          │           │ │ Parameters       │││
│ │  │  BL      │  BR      │           │ │  (inputs)        │││
│ │  │          │          │           │ └──────────────────┘││
│ │  └──────────┴──────────┘           │ ┌──────────────────┐││
│ │                                    │ │ Output Log       │││
│ │  (Subwindows: draggable,          │ │  (stdout/stderr) │││
│ │   snappable, fullscreen)           │ └──────────────────┘││
│ └────────────────────────────────────┴──────────────────────┘│
│ ┌──────────────────────────────────────────────────────────┐ │
│ │ Run Bar (bottom)                                         │ │
│ │  [Start] [Pause] [Stop] [Reset] Progress: [████░░░░░░]  │ │
│ └──────────────────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────────────────┘
```

**Subwindows (4 corners):**
- **TL:** Structure view (ball & stick)
- **TR:** Property plots (energy, RDF, etc.)
- **BL:** Crystal grid (unit cell)
- **BR:** Animation player (if trajectory)

---

### 3. WindowManager

**File:** `include/ui/WindowManager.hpp` (implemented above)

**Features:**
- 8 ViewModel presets (VM0–VM7)
- ±10% tuning per iteration
- Corner snapping (TL/TR/BL/BR)
- Free dragging with constraints
- Fullscreen toggle
- Deterministic layout (saved to config)

**API:**
```cpp
WorkspaceLayoutEngine wm(window_w, window_h);
wm.set_viewmodel(0, tune_iterations=0);  // VM0, no tuning

uint32_t win_id = wm.add_window(WindowMode::Snapped, Corner::TopLeft);
wm.toggle_fullscreen(win_id);
wm.snap_to_corner(win_id, Corner::BottomRight);

Rect ws = wm.workspace_rect();      // (0, 0, W*0.65, H)
Rect instr = wm.instrument_rect();  // (W*0.65, 0, W*0.35, H)
```

---

### 4. Crystal Objects

**File:** `include/data/Crystal.hpp` (implemented above)

**Workflow:**

```cpp
// Load raw input
Crystal cryst = Crystal::load_xyz("foo.xyz");

// Annotate (infer bonds)
cryst.get_bonds();  // Lazy computation
cryst.save_xyzA("foo.xyzA");

// Construct supercell
ConstructionRecipe recipe;
recipe.steps.push_back({"supercell", {{"a", "3"}, {"b", "3"}, {"c", "3"}}});
cryst.recipe = recipe;
cryst.replication = {3, 3, 3};
cryst.rebuild();
cryst.save_xyzC("foo.xyzC");

// Watch for changes
CrystalWatcher watcher;
watcher.on_changed = [](const Crystal& c) {
    // Update visualization
};
watcher.watch("foo.xyz");
```

**Provenance:**
- Every xyzC stores its construction recipe
- Hash = SHA256(source + recipe)
- Rebuild only if hash changes

---

### 5. Visualization Pipeline

**Existing:**
- `apps/card_viewer.cpp` — ImGui + OpenGL
- `src/render/scalable_renderer.cpp` — LOD, culling, instancing
- `src/vis/geometry/sphere.cpp` — Ball-and-stick

**New:**
- `src/vis/crystal_renderer.cpp` — Unit cell grid
- `src/vis/surface_renderer.cpp` — Isosurfaces (density, ESP)
- `src/vis/trajectory_player.cpp` — Animation

**Rendering modes:**
- Ball & stick (default)
- Stick only (bonds)
- Space-filling (VDW spheres)
- Surface (molecular surface)
- Ribbon (protein backbone)
- Crystal grid (unit cell + bonds)

---

## Data Flow Examples

### Example 1: Load and visualize

**User action:** Opens `nacl.xyz`

**Flow:**
1. GUI: Calls `Crystal::load_xyz("nacl.xyz")`
2. Crystal: Parses file → 2 atoms (Na, Cl)
3. WindowManager: Creates TL subwindow (Structure view)
4. Renderer: Draws 2 atoms (Na=purple, Cl=green)
5. Instrument stack: Shows properties (N=2, formula=NaCl)

### Example 2: Generate supercell

**User action:** Clicks "Supercell 3×3×3" button

**Flow:**
1. GUI: Calls TUI command `[7] vsepr emit nacl.xyz --crystal --a 3 --b 3 --c 3`
2. TUI: Spawns `meso-sim` subprocess
3. meso-sim: Generates `nacl.xyzC` (54 atoms)
4. Crystal: Loads `nacl.xyzC`
5. WindowManager: Updates TL subwindow
6. Renderer: Draws 54 atoms (3×3×3 grid)

### Example 3: Run formation frequency

**User action:** Clicks "Collect Formation Freq" (250 × 10)

**Flow:**
1. GUI: Opens parameter dialog (Count, Runs, Seed, Output)
2. User fills inputs → clicks OK
3. GUI: Calls TUI command `[13] Collect formation freq`
4. TUI: Spawns `python collect_formation_frequency.py --per-run 250 --runs 10 --output formation_output`
5. GUI: Displays progress bar + stdout in output log
6. On completion: GUI offers "View Results" button
7. User clicks → GUI calls TUI command `[15] View formation results`
8. TUI: Reads `formation_frequency_analysis.json`
9. GUI: Displays top formulas + element frequencies in TR subwindow

### Example 4: Watch mode

**User action:** Enables "Watch" checkbox

**Flow:**
1. GUI: Calls `CrystalWatcher::watch("foo.xyz")`
2. Watcher: Polls file every 500ms
3. External editor: User modifies `foo.xyz`
4. Watcher: Detects hash change → triggers callback
5. Callback: Reloads Crystal → regenerates xyzA/xyzC
6. WindowManager: Updates all subwindows
7. Renderer: Redraws updated structure

---

## Configuration Persistence

**File:** `~/.vsepr_gui_config.json`

```json
{
  "window": {
    "width": 1920,
    "height": 1080,
    "maximized": false
  },
  "viewmodel": {
    "current": 0,
    "tune_iterations": 0,
    "presets": {
      "0": {"workspace_ratio": 0.65, "font_scale": 1.0},
      "1": {"workspace_ratio": 0.70, "font_scale": 1.05},
      ...
    }
  },
  "subwindows": {
    "TL": {"mode": "snapped", "corner": 0},
    "TR": {"mode": "snapped", "corner": 1},
    "BL": {"mode": "snapped", "corner": 2},
    "BR": {"mode": "snapped", "corner": 3}
  },
  "recent_files": [
    "C:/Users/Liam/Desktop/vsepr-sim/nacl.xyz",
    ...
  ],
  "tui_path": "C:/Users/Liam/Desktop/vsepr-sim/tui.py"
}
```

---

## Build Targets

### New executables:

```cmake
# GUI Application (requires BUILD_VIS=ON)
add_executable(vsepr-gui
    apps/vsepr-gui/main.cpp
    apps/vsepr-gui/gui_main_window.cpp
    apps/vsepr-gui/tui_bridge.cpp
    src/ui/window_manager.cpp
    src/data/crystal.cpp
    src/io/xyzA_format.cpp
    src/io/xyzC_format.cpp
    src/io/crystal_watcher.cpp
    src/vis/crystal_renderer.cpp
    ${IMGUI_SOURCES}
)
target_link_libraries(vsepr-gui
    vsepr_core vsepr_io vsepr_vis vsepr_render
    OpenGL::GL glfw GLEW::GLEW
)
```

---

## Launch Commands

### PowerShell (Windows):
```powershell
.\vsepr-panel.ps1                    # TUI only
.\build\vsepr-gui.exe                 # GUI only
.\vsepr-panel.ps1 --gui               # Both (GUI with TUI backend)
```

### Bash (Linux/macOS):
```bash
python3 tui.py                        # TUI only
./build/vsepr-gui                     # GUI only
./build/vsepr-gui --tui-backend       # Both
```

---

## Next Steps

1. **Implement xyzA/xyzC parsers** (`src/io/xyzA_format.cpp`, `src/io/xyzC_format.cpp`)
2. **Implement Crystal class** (`src/data/crystal.cpp`)
3. **Implement WindowManager** (`src/ui/window_manager.cpp`)
4. **Create GUI main window** (`apps/vsepr-gui/main.cpp`)
5. **Integrate TUI bridge** (spawn tui.py subprocess, pipe I/O)
6. **Add crystal rendering** (`src/vis/crystal_renderer.cpp`)
7. **Add watch system** (`src/io/crystal_watcher.cpp`)
8. **Test end-to-end** (load xyz → annotate → construct → visualize → watch)

---

## Status

**Specifications:** ✅ Complete
- WindowManager.hpp
- Crystal.hpp
- XYZ_FORMAT_SPEC.md
- INTEGRATION_ARCHITECTURE.md (this file)

**Implementation:** ⏳ Ready to begin

**Dependencies met:**
- ✅ ImGui + OpenGL (card_viewer.cpp)
- ✅ TUI backend (tui.py)
- ✅ Python tools (scoring, formation, etc.)
- ✅ Build system (60/65 targets pass)

**Estimated effort:**
- xyzA/xyzC parsers: 1 day
- Crystal + WindowManager: 2 days
- GUI integration: 2 days
- Testing + polish: 1 day
- **Total: ~6 days** (full professional window system)
