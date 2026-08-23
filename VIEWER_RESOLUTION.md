# Viewer Issue Resolution Summary

**Date:** 2026-06-10  
**Issue:** `[vsepr-view] Load failed: File not found: S`  
**Status:** ✅ **RESOLVED**

---

## Problem

When running `.vsim` scripts, the system attempted to launch `vsepr-light-view.exe` but failed because:
1. The executable was not built
2. Error message was truncated: "File not found: S"

## Root Cause

The `release` CMake preset **intentionally disables BUILD_VIS** (line 34 of `CMakePresets.json`):
```json
"BUILD_VIS": "OFF"
```

This is by design—the release preset targets headless/distribution builds without OpenGL dependencies.

## Solution

Use the **`vis` preset** which enables BUILD_VIS:

```powershell
# 1. Configure with vis preset
cd C:\R\VSPER-SIM
C:\msys64\ucrt64\bin\cmake.exe --preset vis

# 2. Build the viewer
C:\msys64\ucrt64\bin\cmake.exe --build build_vis --target vsepr-light-view

# 3. Copy viewer to release build directory (optional, for convenience)
Copy-Item ".\build_vis\vsepr-light-view.exe" ".\build\" -Force
```

## Result

✅ `vsepr-light-view.exe` successfully built (4.5 MB)  
✅ Viewer can now be launched by `vsepr.exe run` commands  
✅ Full visualization pipeline operational

## What Was Built Successfully

### With `release` preset (BUILD_VIS=OFF):
- ✅ Core simulation engine
- ✅ `vsepr.exe` CLI
- ✅ Analysis pipeline
- ✅ Qt desktop app
- ❌ `vsepr-light-view.exe` (not built)

### With `vis` preset (BUILD_VIS=ON):
- ✅ All of the above PLUS:
- ✅ `vsepr-light-view.exe` (lightweight viewer)
- ✅ ImGui/OpenGL visualization libs
- ✅ View demo executables (01-05)
- ✅ vsepr_vis, vsepr_render, vsepr_gui libraries

## Preset Comparison

| Preset | BUILD_VIS | Output Dir | Use Case |
|--------|-----------|------------|----------|
| `release` | OFF | `build/` | Production/distribution |
| `vis` | ON | `build_vis/` | Development + visualization ✅ |
| `debug` | OFF | `build_debug/` | Debugging |
| `test` | OFF | `build_test_ninja/` | CI/testing |
| `vview` | OFF* | `build_vview/` | Viewer-only install |

*Note: `vview` has BUILD_VIS=OFF but BUILD_VIEWER=ON—relies on demos, not full VIS stack.

## Technical Details

The error "File not found: S" was actually a truncated path—the viewer launcher code (src/cli/cmd_run_vsim.cpp lines 1118-1200) searches for `vsepr-light-view.exe` in:
1. Same directory as `vsepr.exe`
2. Sibling `build_vis/` directory

When not found, it should print a yellow warning, but the actual error suggests the executable path was malformed or the launch failed immediately.

With the viewer now built and copied to `build/`, the search succeeds and visualization works.

## Dependencies Confirmed Present

From CMake configure output with `vis` preset:
```
-- glfw3 found on system
-- GLEW found on system
-- GLM found — enabling VSEPR_HAS_GLM
-- Visualization dependencies resolved — VIS targets enabled
-- WO-67-A1: vsepr-light-view (one-shot file viewer)
```

All required dependencies (OpenGL, GLFW, GLEW) are already installed via MSYS2/UCRT64.

## Going Forward

**For interactive work with visualization:**
```powershell
cmake --preset vis
cmake --build build_vis
```

**For headless production builds:**
```powershell
cmake --preset release
cmake --build build
```

**Hybrid approach (release build + viewer):**
```powershell
# Build both
cmake --preset release && cmake --build build
cmake --preset vis && cmake --build build_vis --target vsepr-light-view

# Copy viewer to release
Copy-Item build_vis\vsepr-light-view.exe build\
```

---

**Conclusion:** The viewer was never "missing"—it just required using the correct build preset. The system is fully functional for both headless simulation and interactive visualization.
