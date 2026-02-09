# VSEPR SYSTEM — QUICK REFERENCE

## Files Delivered

```
include/ui/WindowManager.hpp         500 lines  Window manager (8 ViewModels)
include/data/Crystal.hpp             200 lines  Crystal object (xyzZ/A/C)
apps/vsepr-gui/main.cpp             400 lines  GUI prototype (ImGui+OpenGL)
tui.py                              375 lines  Terminal TUI (18 commands)
XYZ_FORMAT_SPEC.md                            File format specification
INTEGRATION_ARCHITECTURE.md                   System integration plan
GUI_ARCHITECTURE_COMPLETE.md                  Complete specification
```

## Window Manager (60–70% workspace)

```
┌────────────────────────────────────┬──────────────┐
│ Workspace (65%)                    │ Instrument   │
│  ┌──────────┬──────────┐           │ Stack (35%)  │
│  │  TL      │  TR      │           │ - Commands   │
│  │          │          │           │ - Parameters │
│  ├──────────┼──────────┤           │ - Output Log │
│  │  BL      │  BR      │           │              │
│  └──────────┴──────────┘           │              │
└────────────────────────────────────┴──────────────┘
```

**8 ViewModels:** VM0–VM7 (default, wide, compact, dense, spacious, focus, quad, ultra-wide)  
**Tuning:** ±10% per iteration (workspace_ratio, padding, fonts)  
**Modes:** Snapped (TL/TR/BL/BR), Free (draggable), Fullscreen (ESC to exit)

## Crystal System (xyzZ → xyzA → xyzC → xyzF)

```
foo.xyz (xyzZ)  — Raw input (2 atoms: Na, Cl)
    ↓
foo.xyzA        — Annotated (bonds inferred, IDs assigned)
    ↓
foo.xyzC        — Constructed (3×3×3 supercell → 54 atoms)
    ↓
foo.xyzF        — Forces (computed force vectors for all atoms)
```

**Provenance:** `hash = SHA256(source + recipe)`  
**Watch mode:** Detects changes → rebuilds xyzA/xyzC/xyzF → updates viz  
**Reserved slots:** bulk (density, modulus, rdf), CG (beads, pmf), results  
**Force vectors:** Primary interaction + all contributors + decomposition (LJ/Coulomb)

## TUI Commands (20 total)

```
[1-3]   Build (all, status, ctest)
[4-6]   Validation (problem1/2, QA)
[7-10]  CLI Tools (vsepr, meso-sim/discover/build)
[11-12] Python Tools (scoring, classification)
[13-16] Data Collection (formation, baseline, view, compare)
[17-18] Analysis (compute forces, view forces)
[s]     Shell
[q]     Quit
```

## Launch

```powershell
.\vsepr-panel.ps1           # TUI only
.\build\vsepr-gui.exe       # GUI only (when built)
```

## Build Status

**60/65 targets pass** (92%)
- ✅ All CLI tools (vsepr, meso-sim, meso-discover, meso-build, meso-relax)
- ✅ All Python tools (scoring, classification, formation, baseline)
- ✅ All validation tests (problem1/2, QA)
- ❌ 5 known failures (missing headers, API mismatches, need BUILD_VIS=ON)

## Implementation Plan (8 days)

```
Day 1-2: xyzA/xyzC parsers + Crystal class
Day 3:   WindowManager implementation
Day 4-5: GUI integration (vsepr-gui.exe)
Day 6:   Visualization (crystal renderer, surfaces)
Day 7:   Watch system (file polling + rebuild)
Day 8:   Polish (config, recent files, shortcuts)
```

## Key Design Principles

1. **No fake physics** — Reserved slots stay `null` until computed
2. **Deterministic provenance** — Hash-based rebuild (source + recipe)
3. **Immutable sources** — .xyz never changes, .xyzC can regenerate
4. **Microscope-style UI** — Workspace + instrument stack (professional)

## Status: ✅ SPECIFICATIONS COMPLETE — READY TO IMPLEMENT
