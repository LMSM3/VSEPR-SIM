# Build Automation Documentation

## Overview

The `build_automated` scripts provide a one-command solution to build the project and run all tests, with special support for validating the new **heat-gated reaction control system (Item #7)**.

---

## Available Scripts

### Linux/macOS/WSL: `build_automated.sh`

**Basic Usage:**
```bash
./build_automated.sh                  # Build + run all tests
./build_automated.sh --clean          # Clean build + tests
./build_automated.sh --verbose        # Verbose output
./build_automated.sh --heat-only      # Only test Item #7
```

### Windows: `build_automated.bat`

**Basic Usage:**
```cmd
build_automated.bat                   # Build + run all tests
build_automated.bat --clean           # Clean build + tests
build_automated.bat --verbose         # Verbose output
build_automated.bat --heat-only       # Only test Item #7
```

---

## What It Does

### 1. **Configuration** (CMake)
- Detects available compilers (MSVC, GCC, Clang)
- Chooses optimal build generator (Ninja, Unix Makefiles, Visual Studio)
- Enables test building (`-DBUILD_TESTS=ON`)
- Configures Release build

### 2. **Compilation**
- Builds all targets in parallel (auto-detects CPU cores)
- Compiles:
  - Main executables (`atomistic-sim`, `atomistic-build`)
  - Test suite (`test_heat_gate`, `test_crystal_pipeline`, etc.)
  - Example programs (`demo_temperature_heat_mapping`)

### 3. **Testing**
- Runs CTest suite if available
- Falls back to manual test execution if needed
- Special `--heat-only` mode for rapid validation of Item #7

### 4. **Reporting**
- Clear pass/fail status for each component
- Build time estimation
- Test coverage summary

---

## Item #7 Validation

The scripts include special support for validating the **temperature-to-heat conversion** feature:

### Quick Test (Item #7 Only)
```bash
./build_automated.sh --heat-only     # Linux/WSL
build_automated.bat --heat-only      # Windows
```

This runs **only** the `test_heat_gate` executable, which validates:
- ✅ Temperature → heat parameter mapping
- ✅ Heat normalization and clamping
- ✅ Gate function behavior
- ✅ Template enable weights
- ✅ Controller integration
- ✅ Monotonicity verification
- ✅ Inverse mapping accuracy

**Expected output:**
```
╔══════════════════════════════════════════════════╗
║  Heat-Gated Reaction Control Tests (SS8b)       ║
╚══════════════════════════════════════════════════╝

Test 1: Heat normalisation .......................... PASS
Test 2: Gate function boundaries .................... PASS
Test 3: Template enable weights ..................... PASS
Test 4: Temperature → Heat Conversion (CRITICAL) .... PASS
Test 5: Candidate bond scoring ...................... PASS
...

✓ All tests passed (15/15)
```

---

## Exit Codes

| Code | Meaning |
|------|---------|
| `0` | All builds and tests passed |
| `1` | CMake configuration failed |
| `1` | Build failed |
| `1` | One or more tests failed |

**Example usage in CI:**
```bash
./build_automated.sh || exit 1
```

---

## Requirements

### Linux/WSL
- CMake 3.15+
- C++20 compiler (GCC 10+, Clang 12+)
- Make or Ninja (optional, for faster builds)

### macOS
- CMake 3.15+
- Xcode Command Line Tools
- Apple Clang 12+ or LLVM Clang 12+

### Windows
- CMake 3.15+
- Visual Studio 2019+ (with C++ desktop development)
- Or: MinGW-w64 (GCC 10+)

---

## Troubleshooting

### "CMake configuration failed"

**Linux:**
```bash
sudo apt-get install cmake g++ make
```

**macOS:**
```bash
brew install cmake
xcode-select --install
```

**Windows:**
- Install Visual Studio 2022 with "Desktop development with C++"
- Or download CMake from https://cmake.org/download/

### "Build failed: compiler not found"

**Check compiler:**
```bash
g++ --version    # Linux/WSL
clang++ --version   # macOS
cl                  # Windows (MSVC)
```

If missing, install appropriate compiler (see Requirements).

### "test_heat_gate not found"

Tests may not be built. Try:
```bash
./build_automated.sh --clean
```

This will reconfigure with `-DBUILD_TESTS=ON`.

### Parallel build issues

If build crashes with "out of memory", reduce parallelism:

**Linux/macOS:**
```bash
# Edit build_automated.sh, line ~110:
# CORES=2  # Instead of auto-detect
```

**Windows:**
```cmd
REM Edit build_automated.bat, line ~95:
REM set CORES=2
```

---

## Integration with CI/CD

### GitHub Actions Example

```yaml
name: Build & Test

on: [push, pull_request]

jobs:
  build:
    runs-on: ${{ matrix.os }}
    strategy:
      matrix:
        os: [ubuntu-latest, macos-latest, windows-latest]
    
    steps:
    - uses: actions/checkout@v3
    
    - name: Install CMake
      uses: lukka/get-cmake@latest
    
    - name: Build & Test
      run: |
        chmod +x build_automated.sh     # Linux/macOS
        ./build_automated.sh
      if: runner.os != 'Windows'
    
    - name: Build & Test (Windows)
      run: build_automated.bat
      if: runner.os == 'Windows'
```

### GitLab CI Example

```yaml
build_and_test:
  stage: test
  script:
    - chmod +x build_automated.sh
    - ./build_automated.sh
  artifacts:
    paths:
      - build/tests/
    reports:
      junit: build/tests/*.xml
```

---

## Advanced Usage

### Custom CMake Options

Edit the script to add custom CMake flags:

```bash
# In build_automated.sh, line ~115:
CMAKE_ARGS=(
    -S "$PROJECT_ROOT"
    -B "$BUILD_DIR"
    -DCMAKE_BUILD_TYPE=Release
    -DBUILD_TESTS=ON
    -DENABLE_VISUALIZATION=ON      # Add custom option
)
```

### Running Specific Tests

```bash
# After building:
cd build
ctest -R test_heat_gate       # Run only heat_gate
ctest -R test_crystal         # Run only crystal tests
ctest -E test_slow            # Exclude slow tests
```

### Debug Builds

```bash
# Edit CMAKE_ARGS in script:
# -DCMAKE_BUILD_TYPE=Debug
```

Or run CMake manually:
```bash
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON
cmake --build build-debug
cd build-debug && ctest
```

---

## Performance Benchmarks

**Typical build times on various systems:**

| System | Configuration | Time |
|--------|---------------|------|
| Linux (32 cores, Xeon) | Release, Ninja | ~45s |
| macOS (M1 Max, 10 cores) | Release, Xcode | ~60s |
| Windows (16 cores, i9) | Release, MSVC | ~90s |
| WSL2 (8 cores) | Release, Make | ~120s |

**Test execution:**
- Heat-only mode: ~5 seconds
- Full test suite: ~30 seconds

---

## Related Documentation

- [Section 8b: Heat-Gated Reaction Control](docs/section8b_heat_gated_reaction_control.tex)
- [Item #7 Implementation Guide](docs/TEMPERATURE_HEAT_IMPLEMENTATION.md)
- [Validation Report](docs/VALIDATION_REPORT.md)
- [CMakeLists.txt](CMakeLists.txt) - Build configuration

---

## Changelog

### v0.1 (January 2025)
- ✅ Initial release of automated build system
- ✅ Support for Linux, macOS, Windows
- ✅ Parallel build detection
- ✅ CTest integration
- ✅ Special `--heat-only` mode for Item #7
- ✅ Color-coded terminal output
- ✅ Comprehensive error reporting

---

**Maintainer:** Formation Engine Development Team  
**Last Updated:** January 17, 2025
