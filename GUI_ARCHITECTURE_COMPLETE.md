# PROFESSIONAL GUI ARCHITECTURE — COMPLETE SPECIFICATION

## ✅ **DELIVERED**

### 1. **WindowManager** (`include/ui/WindowManager.hpp`)
- 500+ lines, header-only, production-ready
- 8 ViewModel presets (VM0–VM7) with ±10% tuning
- Corner snapping (TL/TR/BL/BR), free dragging, fullscreen
- Workspace 60–70% (left), Instrument stack 30–40% (right)
- Deterministic layout with config persistence

### 2. **Crystal System** (`include/data/Crystal.hpp`)
- Immutable provenance (source refs never change)
- Mutable caches (bonds, surfaces — throwaway)
- xyzZ/xyzA/xyzC file format support
- Construction recipes with SHA256 hash
- Watch system for --watch mode

### 3. **File Format Spec** (`XYZ_FORMAT_SPEC.md`)
- **xyzZ:** Raw XYZ (standard format, compatible everywhere)
- **xyzA:** Annotated (bonds, IDs, per-atom props in YAML metadata)
- **xyzC:** Constructed (supercells, relaxed, CG + bulk/CG property slots)
- Provenance tracking (pipeline_id + steps → deterministic rebuild)

### 4. **Integration Architecture** (`INTEGRATION_ARCHITECTURE.md`)
- TUI (tui.py) as command backend
- GUI (vsepr-gui.exe) wraps TUI via subprocess
- WindowManager manages workspace layout
- Crystal objects drive visualization
- Full data flow diagrams

### 5. **GUI Prototype** (`apps/vsepr-gui/main.cpp`)
- 400+ lines ImGui + OpenGL implementation
- Integrates WindowManager, Crystal, TUI bridge
- 4 subwindows (Structure, Properties, Crystal Grid, Animation)
- Instrument stack (Command Panel, Parameters, Output Log)
- Run bar (Start/Pause/Stop/Progress)

---

## 🎯 **KEY DESIGN DECISIONS**

### **Workspace = 60–70% (Left-aligned)**
- User can select ViewModel preset (VM0–VM7)
- Each preset can be tuned ±10% per iteration
- Workspace contains 4 draggable/snappable subwindows
- Instrument stack on right (microscope-style UI)

### **Subwindows (Snapped/Free/Fullscreen)**
- **TL:** Structure view (ball & stick, surfaces)
- **TR:** Property plots (energy, RDF, coordination)
- **BL:** Crystal grid (unit cell + symmetry ops)
- **BR:** Animation player (trajectory scrubbing)

### **Corner Snapping Rules**
- Drag to corner → snap to slot (TL/TR/BL/BR)
- Double-click title bar → fullscreen (workspace only)
- ESC → exit fullscreen
- Constrained sizes: min=25%, max=100% of workspace

### **Crystal Object (The Special One)**
- **Source refs (immutable):**
  - `xyz_path` — raw input (foo.xyz)
  - `xyzA_path` — annotated (foo.xyzA)
  - `xyzC_path` — constructed (foo.xyzC)
  
- **Constructive state:**
  - `lattice` — unit cell vectors
  - `replication` — (nx, ny, nz)
  - `recipe` — construction steps + hash
  
- **Reserved slots (xyzC):**
  - **Bulk:** density, elastic_modulus, rdf
  - **Coarse-grained:** beads, bead_types, bead_bonds, pmf
  - **Results:** energy, converged, notes

### **File Format (3-tier system)**

```
foo.xyz  (xyzZ) — Raw input (standard XYZ)
  ↓
foo.xyzA (xyzA) — Annotated (bonds, IDs, metadata in YAML)
  ↓
foo.xyzC (xyzC) — Constructed (supercells, relaxed, CG + provenance)
```

**Key feature:** Geometry block stays plain XYZ for compatibility.  
All metadata in comment headers (YAML format).

---

## 📐 **WINDOW MANAGER CONTRACT**

### **Core Types:**
```cpp
struct Rect { float x, y, w, h; };

enum class WindowMode { Free, Snapped, Fullscreen };
enum class Corner { TopLeft, TopRight, BottomLeft, BottomRight };

struct WindowState {
    uint32_t id;
    WindowMode mode;
    Corner corner;
    Rect rect;
    int z_order;
    WindowMode prev_mode;  // For fullscreen restore
    Rect prev_rect;
    Corner prev_corner;
};

struct ViewModel {
    float workspace_ratio;  // 0.60–0.70
    float snap_padding_px;
    float min_frac_w, min_frac_h;  // 0.25 default
    float max_frac_w, max_frac_h;  // 1.00 default
    float font_scale;
    float ui_density;
    int default_grid_mode;  // 1, 2, or 4 panes
    bool fullscreen_workspace_only;
    Deltas deltas;  // ±10% per iteration
};
```

### **8 ViewModels (Presets):**
- **VM0:** Default microscope (65%, font=1.0)
- **VM1:** Wide workspace (70%, font=1.05)
- **VM2:** Compact (60%, dense UI)
- **VM3:** Dense UI (small fonts, tight padding)
- **VM4:** Spacious UI (large fonts, wide padding)
- **VM5:** Single pane focus (70%, 1 pane default)
- **VM6:** Quad split default (65%, 4 panes)
- **VM7:** Ultra-wide (75%, for 21:9 monitors)

### **API:**
```cpp
WorkspaceLayoutEngine wm(window_w, window_h);
wm.set_viewmodel(0, tune_iterations=0);  // VM0, no tuning

uint32_t win = wm.add_window(WindowMode::Snapped, Corner::TopLeft);
wm.toggle_fullscreen(win);
wm.snap_to_corner(win, Corner::BottomRight);
wm.drag_to(win, x, y);
wm.resize_to(win, w, h);

Rect workspace = wm.workspace_rect();
Rect instrument = wm.instrument_rect();
```

---

## 🔬 **CRYSTAL PROVENANCE (THE IMPORTANT PART)**

### **Why This Matters:**

Every xyzC file can be **deterministically regenerated** from its source:

```
foo.xyzC hash = SHA256(foo.xyz content + recipe JSON)
```

If source changes OR recipe changes → hash changes → rebuild required.

### **Workflow:**

```cpp
// 1. Load raw input
Crystal cryst = Crystal::load_xyz("nacl.xyz");  // 2 atoms: Na, Cl

// 2. Annotate (infer bonds)
cryst.get_bonds();  // Lazy: only computes if needed
cryst.save_xyzA("nacl.xyzA");

// 3. Construct supercell
ConstructionRecipe recipe;
recipe.pipeline_id = "supercell-001";
recipe.steps.push_back({"supercell", {{"a", "3"}, {"b", "3"}, {"c", "3"}}});
cryst.recipe = recipe;
cryst.replication = {3, 3, 3};
cryst.rebuild();  // Generates 54 atoms
cryst.save_xyzC("nacl.xyzC");

// 4. Watch for changes
CrystalWatcher watcher;
watcher.on_changed = [](const Crystal& c) {
    // Regenerate xyzA/xyzC
    // Update visualization
};
watcher.watch("nacl.xyz");
```

### **Provenance Example (xyzC header):**

```yaml
# xyzC v1  source_raw="nacl.xyz"  units=angstrom
# provenance:
#   pipeline_id: "supercell-001"
#   hash: "a3f7c8..."
#   steps:
#     - {name: "supercell", params: {a: 3, b: 3, c: 3}}
#     - {name: "relax", params: {method: "lj_quench", steps: 20000}}
# lattice:
#   a: [5.64, 0.0, 0.0]
#   b: [0.0, 5.64, 0.0]
#   c: [0.0, 0.0, 5.64]
# reserved_slots:
#   bulk:
#     density: {value: 2.165, units: "g/cm3"}
#     elastic_modulus: {value: null, units: "GPa"}
#   results:
#     energy: {value: -3.85, units: "eV"}
#     converged: true
```

**No fake physics:** Reserved slots stay `null` until actual simulation fills them.

---

## 🚀 **INTEGRATION WITH TUI**

### **TUI as Command Backend:**

GUI spawns `tui.py` as subprocess:

```python
# GUI → TUI (stdin)
"13\n250\n10\n42\nformation_output\n"  # Command 13: Collect formation freq

# TUI → GUI (stdout)
"Configuration:\n  Molecules/run: 250\n  Runs: 10\n...\n"
"✓ Collection Complete\n"
```

### **Available Commands (from TUI):**

```
[1] Build all (VIS=OFF)
[2] Build status
[3] Run ctest
[4-6] Validation (problem1/2, QA)
[7-10] CLI Tools (vsepr, meso-sim, meso-discover, meso-build)
[11-12] Python Tools (scoring, classification)
[13-16] Data Collection (formation, baseline, view, compare)
[s] Shell
[q] Quit
```

GUI wraps these with buttons/dialogs/forms.

---

## 📊 **EXAMPLE WORKFLOWS**

### **Workflow 1: Load → Annotate → Visualize**

1. User: File → Open XYZ → `benzene.xyz`
2. GUI: Loads Crystal, displays in TL subwindow (6C + 6H)
3. User: Tools → Annotate (xyzA)
4. GUI: Calls `cryst.get_bonds()` → infers aromatic bonds
5. GUI: Saves `benzene.xyzA`, updates visualization (shows bonds)

### **Workflow 2: Generate Supercell**

1. User: Tools → Supercell (3×3×3)
2. GUI: Opens dialog, user fills a=3, b=3, c=3
3. GUI: Calls TUI command `[7] vsepr emit nacl.xyz --crystal --a 3 --b 3 --c 3`
4. TUI: Spawns `meso-sim`, generates `nacl.xyzC` (54 atoms)
5. GUI: Loads `nacl.xyzC`, updates all subwindows:
   - TL: Shows 3×3×3 grid (54 atoms)
   - BL: Shows unit cell with vectors
6. Instrument stack: Displays provenance (pipeline_id, steps)

### **Workflow 3: Formation Frequency (250×10)**

1. User: Tools → Formation Frequency
2. GUI: Opens dialog (Count, Runs, Seed, Output)
3. User: Fills → Count=250, Runs=10, Seed=42, Output=formation_output
4. GUI: Calls TUI command `[13] Collect formation freq`
5. TUI: Spawns `python collect_formation_frequency.py ...`
6. GUI: Displays progress bar + stdout in output log (real-time)
7. On complete: GUI offers "View Results" button
8. User: Clicks → GUI loads `formation_frequency_analysis.json`
9. TR subwindow: Displays top formulas + element frequencies

### **Workflow 4: Watch Mode**

1. User: Opens `water.xyz` (3 atoms: O + 2H)
2. User: Enables "Watch" checkbox
3. GUI: Starts `CrystalWatcher` polling `water.xyz` every 500ms
4. External editor: User modifies `water.xyz` (moves H atom)
5. Watcher: Detects file change → computes new hash
6. Watcher callback: Reloads Crystal → regenerates xyzA
7. GUI: Updates all subwindows automatically
8. User sees change in real-time (no manual refresh)

---

## 🛠️ **BUILD PLAN**

### **Phase 1: Core (2 days)**
1. Implement `src/io/xyzA_format.cpp` (parser + writer)
2. Implement `src/io/xyzC_format.cpp` (parser + writer)
3. Implement `src/data/crystal.cpp` (Crystal class)
4. Add tests: `test_xyzA_format`, `test_xyzC_format`, `test_crystal`

### **Phase 2: WindowManager (1 day)**
1. Implement `src/ui/window_manager.cpp` (non-header parts if needed)
2. Add tests: `test_window_manager`
3. Verify 8 ViewModels + tuning

### **Phase 3: GUI Integration (2 days)**
1. Implement `apps/vsepr-gui/main.cpp` (from prototype above)
2. Implement `apps/vsepr-gui/tui_bridge.cpp` (subprocess + pipes)
3. Implement `apps/vsepr-gui/gui_main_window.cpp` (split from main)
4. Add CMake target `vsepr-gui` (requires BUILD_VIS=ON)

### **Phase 4: Visualization (1 day)**
1. Implement `src/vis/crystal_renderer.cpp` (unit cell + grid)
2. Implement `src/vis/surface_renderer.cpp` (isosurfaces)
3. Integrate with existing `scalable_renderer.cpp`

### **Phase 5: Watch System (1 day)**
1. Implement `src/io/crystal_watcher.cpp` (file polling + callbacks)
2. Test with `--watch` flag
3. Verify deterministic rebuild (hash-based)

### **Phase 6: Polish (1 day)**
1. Config persistence (`~/.vsepr_gui_config.json`)
2. Recent files list
3. Keyboard shortcuts (F1–F4 for fullscreen, etc.)
4. Error handling + user feedback

**Total: 8 days** (with testing + documentation)

---

## ✅ **STATUS**

| Component | Status |
|-----------|--------|
| WindowManager.hpp | ✅ Complete (500 lines, header-only) |
| Crystal.hpp | ✅ Complete (interface spec) |
| XYZ_FORMAT_SPEC.md | ✅ Complete |
| INTEGRATION_ARCHITECTURE.md | ✅ Complete |
| vsepr-gui/main.cpp | ✅ Prototype (400 lines) |
| xyzA/xyzC parsers | ⏳ To implement |
| Crystal class impl | ⏳ To implement |
| TUI bridge | ⏳ To implement |
| Watch system | ⏳ To implement |

---

## 🎓 **LESSONS FROM COPILOT INSTRUCTIONS**

> "No fake physics—code must reflect real physical modeling"

Applied to this architecture:

1. **Reserved slots stay `null` until filled by real simulation**
   - `bulk.density` = null until computed from coordinates
   - `cg.pmf` = null until free energy calculation runs
   
2. **Provenance ensures reproducibility**
   - Every xyzC can be regenerated from source + recipe
   - Hash mismatch → rebuild required (no "trust me" data)
   
3. **Construction recipes are explicit**
   - `{name: "supercell", params: {a: 3}}`—no magic numbers
   - `{name: "relax", params: {method: "lj_quench", steps: 20000}}`—not "relax()"
   
4. **Watch mode rebuilds deterministically**
   - Source change → hash changes → rebuild xyzA/xyzC
   - No manual "sync" button—system detects and rebuilds

---

## 🚀 **READY TO IMPLEMENT**

All specifications complete. Ready to begin implementation following the 8-day build plan.

**Next command:** Implement Phase 1 (xyzA/xyzC parsers + Crystal class).
