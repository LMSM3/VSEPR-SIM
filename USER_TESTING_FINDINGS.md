# VSEPR-Sim Demo Scripts - User Guide

## 🚨 Current Status (User Perspective)

### What ACTUALLY Works Right Now

**✅ Available:**
- CMake build system
- Core molecular simulation library
- Test suite (30+ tests)
- Thermal analysis
- PBC (Periodic Boundary Conditions)

**⚠️ Under Development:**
- `vsepr_opengl_viewer` - Continuous generation system (documented but source in examples/)
- Interactive visualization (OpenGL viewer not built by default)
- Command-line XYZ export

**🔨 Build Status:**
```
build/bin/
├── hydrate-demo    ✓ Compiled
├── md_demo         ✓ Compiled  
├── vsepr-cli       ✓ Compiled
└── vsepr_batch     ✓ Compiled
```

---

## 🎯 What Users Can Do NOW

### 1. Build the Project

**Windows:**
```batch
build_universal.bat
```

**Linux/WSL:**
```bash
./build_universal.sh
```

### 2. Run Tests

```bash
cd build
ctest --output-on-failure
```

Expected output:
```
100% tests passed, 0 tests failed out of 30
```

### 3. Thermal Analysis

**Windows:**
```batch
test_thermal.bat
```

**Linux:**
```bash
./test_thermal.sh
```

Tests H₂O, NH₃, CH₄, Br₂, NaCl, Cu₈, C₆H₆ thermal properties.

### 4. Run Hydrate Demo

```bash
./build/bin/hydrate-demo
```

Simulates water and calcium carbonate hydrates.

### 5. MD Simulation Demo

```bash
./build/bin/md_demo
```

Molecular dynamics demonstration.

---

## 📋 Demo Scripts Status

| Script | Status | Notes |
|--------|--------|-------|
| `build_universal.sh/.bat` | ✅ Works | Main build system |
| `test_thermal.sh/.bat` | ✅ Works | Thermal property tests |
| `debug.sh/.bat` | ✅ Works | Diagnostics and quick tests |
| `demo_realtime_watch.bat` | ⚠️ Needs fix | Requires vsepr CLI with XYZ export |
| `quick_demo_50.sh` | ⚠️ Needs fix | Requires continuous generation compile |
| `examples/demo_xyz_export.sh` | ⚠️ Needs update | References non-existent binary |

---

## 🔧 Issues Found (Acting as User)

### Issue 1: Missing `vsepr_opengl_viewer` Binary
**Problem:** Documentation references `vsepr_opengl_viewer` extensively, but it's not built by default.

**Why:** Source is in `examples/vsepr_opengl_viewer.cpp` (not `apps/`), not integrated in CMakeLists.txt

**Solutions:**
1. **Quick Fix (Manual Compile):**
   ```bash
   g++ -std=c++17 -O2 examples/vsepr_opengl_viewer.cpp \
       -o vsepr_opengl_viewer \
       -Iinclude -Ithird_party/glm -pthread
   ```

2. **Proper Fix:** Add to CMakeLists.txt:
   ```cmake
   if(BUILD_APPS)
       add_executable(vsepr_opengl_viewer examples/vsepr_opengl_viewer.cpp)
       target_link_libraries(vsepr_opengl_viewer vsepr_core)
       target_include_directories(vsepr_opengl_viewer PRIVATE include third_party/glm)
   endif()
   ```

### Issue 2: No C++ Compiler on Windows
**Problem:** Windows users without MinGW/MSVC can't compile manually.

**Symptoms:**
```
'g++' is not recognized as the name of a cmdlet
```

**Solutions:**
1. Install MinGW-w64: https://www.mingw-w64.org/
2. Install Visual Studio with C++ tools
3. Use WSL: `wsl` then run Linux commands

### Issue 3: vsepr-cli Doesn't Support --xyz Flag
**Problem:** Demo scripts assume `--xyz` flag exists for XYZ export.

**Current Status:** vsepr-cli has limited command-line interface

**Workaround:** Use the full `vsepr` binary (if built):
```bash
./build/bin/vsepr build H2O --viz
```

### Issue 4: Path Confusion (apps/ vs examples/)
**Problem:** Documentation inconsistent about where files are located.

**Facts:**
- Continuous generation: `examples/vsepr_opengl_viewer.cpp`
- Applications: `apps/cli.cpp`, `apps/hydrate_demo.cpp`, etc.
- Build outputs: `build/bin/`

**Fixed in:** QUICK_REFERENCE_CONTINUOUS.md, DEFAULT_MD.md (partially)

---

## ✅ Corrected Demo: Real-Time 50 Molecules

Since the continuous generation tool needs compilation, here's what ACTUALLY works:

### Option A: Build the Continuous Tool First

```bash
# 1. Check for compiler
g++ --version

# 2. Compile continuous generation tool
g++ -std=c++17 -O2 examples/vsepr_opengl_viewer.cpp \
    -o vsepr_opengl_viewer \
    -Iinclude -Ithird_party/glm -pthread

# 3. Run demo
./vsepr_opengl_viewer 50 every-other --watch realtime.xyz
```

### Option B: Use CMake Build (Recommended)

**Step 1:** Add to `CMakeLists.txt` (after line ~160, in BUILD_APPS section):
```cmake
# Continuous generation demo
add_executable(vsepr_opengl_viewer examples/vsepr_opengl_viewer.cpp)
target_include_directories(vsepr_opengl_viewer PRIVATE include third_party/glm)
target_link_libraries(vsepr_opengl_viewer vsepr_core)
```

**Step 2:** Rebuild:
```bash
cmake --build build --target vsepr_opengl_viewer
```

**Step 3:** Run:
```bash
./build/bin/vsepr_opengl_viewer 50 every-other --watch realtime.xyz
```

### Option C: Use Existing Demos (Works NOW)

```bash
# Thermal analysis (works)
./test_thermal.sh

# Hydrate demo (works)
./build/bin/hydrate-demo

# MD demo (works)  
./build/bin/md_demo

# Run all tests (works)
cd build && ctest
```

---

## 📝 Lessons Learned (User Testing Perspective)

### 1. **Documentation Assumes Too Much**
- References binaries that don't exist
- Assumes compiler availability
- Doesn't explain build prerequisites

### 2. **Path Inconsistencies**
- `apps/` vs `examples/` confusion
- Not all example code is built by default
- Build output locations not clearly documented

### 3. **Missing Quick Start**
- No "test if this works" one-liner
- Build instructions scattered across files
- No clear "Start Here" guide

### 4. **Demo Scripts Need Error Handling**
- Should check for prerequisites
- Should provide helpful error messages
- Should offer alternatives if something missing

---

## 🎓 Recommended User Flow (Fixed)

### First-Time Setup

1. **Clone & Prerequisites:**
   ```bash
   git clone <repo>
   cd vsepr-sim
   
   # Install dependencies
   # Linux: sudo apt install cmake g++ libglfw3-dev
   # macOS: brew install cmake glfw
   # Windows: Install Visual Studio or MinGW
   ```

2. **Build Everything:**
   ```bash
   ./build_universal.sh    # Linux/WSL
   build_universal.bat     # Windows
   ```

3. **Verify Build:**
   ```bash
   cd build
   ctest --output-on-failure
   ```

4. **Try Demos:**
   ```bash
   ./test_thermal.sh              # Thermal analysis
   ./build/bin/hydrate-demo       # Hydrate simulation
   ./build/bin/md_demo            # MD simulation
   ```

5. **Optional: Continuous Generation:**
   ```bash
   # Add to CMakeLists.txt first (see Option B above)
   cmake --build build --target vsepr_opengl_viewer
   ./build/bin/vsepr_opengl_viewer 1000 every-other
   ```

---

## 🔍 Files to Update

Based on user testing, these files need corrections:

- [x] `QUICK_REFERENCE_CONTINUOUS.md` - Fixed paths (apps→examples)
- [x] `DEFAULT_MD.md` - Fixed compilation path
- [x] `demo_realtime_watch.bat` - Added error checking
- [x] `quick_demo_50.sh` - Fixed compilation path
- [ ] `CMakeLists.txt` - Add vsepr_opengl_viewer target
- [ ] `examples/demo_xyz_export.sh` - Update for actual binaries
- [ ] `examples/demo_continuous_generation.sh` - Same
- [ ] `run_continuous_demo.bat` - Update for actual binaries
- [ ] `README.md` - Add "Quick Smoke Test" section

---

## ✨ Recommendations for Future

1. **Add to CMakeLists.txt:**
   - Build `vsepr_opengl_viewer` by default
   - Add install targets for all demos
   - Create `make install-demos` target

2. **Create Smoke Test:**
   ```bash
   #!/bin/bash
   # smoke_test.sh - Quick verification
   
   echo "Testing VSEPR-Sim installation..."
   
   # Test 1: Binaries exist
   for bin in hydrate-demo md_demo vsepr-cli vsepr_batch; do
       if [ -f "build/bin/$bin" ]; then
           echo "✓ $bin"
       else
           echo "✗ $bin (missing)"
       fi
   done
   
   # Test 2: Run quick test
   cd build && ctest --timeout 10 -R "BasicMolecule"
   
   echo "Smoke test complete!"
   ```

3. **User-Friendly Error Messages:**
   - Scripts should detect missing dependencies
   - Provide installation commands for each OS
   - Suggest alternatives if primary method fails

---

**Created by:** User testing simulation  
**Date:** January 18, 2026  
**Status:** Issues identified and documented  
**Next Steps:** Update CMakeLists.txt to build continuous generation by default
