# VSEPR-Sim: Complete Build & Integration Guide
**Default Master Documentation - All Build Systems, Scripts, and Linkages**

---

## 📚 Table of Contents

1. [Quick Start](#-quick-start)
2. [Build Systems](#-build-systems)
3. [Shell Scripts & Automation](#-shell-scripts--automation)
4. [C++ Compilation & Linking](#-c-compilation--linking)
5. [Project Structure](#-project-structure)
6. [Cross-Platform Support](#-cross-platform-support)
7. [Testing Infrastructure](#-testing-infrastructure)
8. [Continuous Generation System](#-continuous-generation-system)
9. [Common Workflows](#-common-workflows)
10. [Troubleshooting](#-troubleshooting)

---

## 🚀 Quick Start

### Universal Scripts (Recommended)

**Linux/WSL/macOS:**
```bash
./build_universal.sh              # Build entire project
./test_thermal.sh                 # Test thermal properties
./debug.sh info                   # System diagnostics
```

**Windows:**
```batch
build_universal.bat               # Build entire project (auto-detects WSL)
test_thermal.bat                  # Test thermal properties
debug.bat info                    # System diagnostics
```

### Traditional Build

**CMake (All platforms):**
```bash
mkdir build && cd build
cmake ..
cmake --build . --parallel 8
ctest --output-on-failure
```

**Quick Scripts:**
```bash
./build.sh --clean --install      # Linux/WSL
.\build.ps1 -Clean -Install       # Windows PowerShell
```

---

## 🔨 Build Systems

### 1. CMake (Primary Build System)

**Configuration:** [CMakeLists.txt](CMakeLists.txt)

#### Build Options
```cmake
option(BUILD_TESTS "Build test suite" ON)
option(BUILD_APPS "Build applications" ON)
option(BUILD_VIS "Build visualization tools" ON)
option(BUILD_DEMOS "Build demo targets" OFF)
```

#### Standard Workflow
```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build all targets
cmake --build build --parallel 8

# Run tests
cd build && ctest --output-on-failure

# Install
cmake --install build --prefix /usr/local
```

#### Key Targets

| Target | Description | Location |
|--------|-------------|----------|
| `vsepr` | Main CLI application | `build/bin/vsepr` |
| `vsepr-cli` | Lightweight CLI tool | `build/bin/vsepr-cli` |
| `vsepr-view` | OpenGL viewer | `build/bin/vsepr-view` |
| `vsepr_batch` | Batch processing | `build/bin/vsepr_batch` |
| `hydrate-demo` | Hydrate demo | `build/bin/hydrate-demo` |
| `md_demo` | MD simulation demo | `build/bin/md_demo` |

### 2. Universal Build Scripts

**Bash:** [build_universal.sh](build_universal.sh)  
**Batch:** [build_universal.bat](build_universal.bat)

```bash
# Full rebuild with 16 cores
./build_universal.sh --clean --jobs 16

# Debug build
./build_universal.sh --debug

# Build specific target
./build_universal.sh --target vsepr

# Verbose output
./build_universal.sh --verbose
```

**Features:**
- ✅ Automatic CMake configuration
- ✅ Parallel compilation (default: 8 jobs)
- ✅ Cross-platform (WSL auto-detection on Windows)
- ✅ Clean build option
- ✅ Debug/Release modes
- ✅ Colored output

### 3. Platform-Specific Scripts

#### Linux/WSL/macOS
**Script:** [build.sh](build.sh)  
**Canonical:** [scripts/build/build.sh](scripts/build/build.sh)

```bash
./build.sh --clean          # Clean build
./build.sh --install        # Install to system
./build.sh --viz            # Build with visualization
./build.sh --help           # Show options
```

#### Windows PowerShell
**Script:** [build.ps1](build.ps1)  
**Canonical:** [scripts/build/build.ps1](scripts/build/build.ps1)

```powershell
.\build.ps1 -Clean          # Clean build
.\build.ps1 -Install        # Install
.\build.ps1 -Verbose        # Verbose output
```

#### Windows Batch
**Script:** [build.bat](build.bat)

```batch
build.bat --clean           # Clean build
build.bat --viz             # Build with visualization
```

---

## 🐚 Shell Scripts & Automation

### Core Automation Scripts

| Script | Platform | Purpose |
|--------|----------|---------|
| [vsepr.bat](vsepr.bat) | Windows | Main entry point wrapper |
| [debug.sh](debug.sh) | Bash | Debug & diagnostics |
| [debug.bat](debug.bat) | Batch | Debug & diagnostics |
| [test_thermal.sh](test_thermal.sh) | Bash | Thermal system tests |
| [test_thermal.bat](test_thermal.bat) | Batch | Thermal system tests |
| [test.sh](test.sh) | Bash | Quick test runner |

### Testing Scripts

#### Unit & Integration Tests
```bash
# Run all tests
./tests/run_tests.sh              # Linux/WSL
.\tests\run_tests.bat             # Windows

# Specific test suites
./tests/phase2_test.sh            # Phase 2 validation
./tests/phase3_test.sh            # Phase 3 isomerism
./tests/phase3_isomer_ci.sh       # CI/CD isomer tests
```

#### Functional Tests
```bash
./tests/functional/quick_test.sh        # Quick sanity check
./tests/functional/test_demo.sh         # Demo functionality
./tests/functional/test_fluorides.sh    # Fluoride chemistry
```

#### Regression Tests
```bash
./tests/regression/test_phase1_build.sh  # Phase 1 regression
./tests/regression/test_phase2.sh        # Phase 2 regression
```

#### Integration Tests
```bash
./tests/integration/test_comprehensive.sh  # Comprehensive integration
```

### Utility Scripts

| Script | Purpose |
|--------|---------|
| [scripts/batch_chemistry_demo.sh](scripts/batch_chemistry_demo.sh) | Batch chemistry demos |
| [scripts/benchmark_complex.sh](scripts/benchmark_complex.sh) | Performance benchmarking |
| [scripts/cache.sh](scripts/cache.sh) | Build cache management |
| [scripts/output_LOGGER.sh](scripts/output_LOGGER.sh) | Output logging |
| [scripts/migrate_includes.sh](scripts/migrate_includes.sh) | Header migration |
| [scripts/rebuild_tests.sh](scripts/rebuild_tests.sh) | Rebuild test suite |
| [scripts/automate_review.sh](scripts/automate_review.sh) | Code review automation |

### Discovery & Visualization Scripts

| Script | Purpose |
|--------|---------|
| [scripts/random_discovery.sh](scripts/random_discovery.sh) | Random molecule discovery |
| [scripts/random_discovery.ps1](scripts/random_discovery.ps1) | Random discovery (Windows) |
| [scripts/view_molecule.sh](scripts/view_molecule.sh) | Molecule viewer launcher |
| [scripts/view_molecule.bat](scripts/view_molecule.bat) | Molecule viewer (Windows) |
| [scripts/demo_discovery.ps1](scripts/demo_discovery.ps1) | Discovery demo |

---

## ⚙️ C++ Compilation & Linking

### Compiler Requirements

**Minimum:**
- C++17 standard
- CMake 3.15+

**Supported Compilers:**
- GCC 7.0+
- Clang 5.0+
- MSVC 2019+

### Library Dependencies

#### Core Libraries (Header-Only)
```cmake
vsepr_core      # Core data types and utilities
vsepr_pot       # Potential energy functions
vsepr_box       # Periodic boundary conditions
vsepr_nl        # Neighbor lists
vsepr_int       # Integrators
vsepr_build     # Formula → Molecule builder
```

#### Static Libraries
```cmake
vsepr_sim       # Simulation engine
vsepr_vis       # Visualization (if BUILD_VIS=ON)
spec_parser     # Spec/DSL parser
```

#### External Dependencies

**Visualization (Optional):**
```cmake
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(GLEW REQUIRED)
```

**Third-Party:**
- **GLM** (OpenGL Mathematics) - `third_party/glm/`
- **Dear ImGui** - `third_party/imgui/`

### Linking Examples

#### Simple Application
```cpp
// main.cpp
#include "core/types.hpp"
#include "sim/molecule.hpp"

int main() {
    vsepr::Molecule mol = vsepr::MoleculeBuilder::from_formula("H2O");
    return 0;
}
```

**CMake:**
```cmake
add_executable(my_app main.cpp)
target_link_libraries(my_app vsepr_core vsepr_sim)
```

**Manual Compilation:**
```bash
g++ -std=c++17 -O2 main.cpp -o my_app \
    -Isrc/core -Isrc/sim \
    -Lbuild/lib -lvsepr_sim
```

#### With Visualization
```cmake
add_executable(my_viz_app main.cpp)
target_link_libraries(my_viz_app 
    vsepr_core 
    vsepr_sim 
    vsepr_vis
    OpenGL::GL 
    glfw 
    GLEW::GLEW
)
```

#### Continuous Generation System
**Standalone Compilation:**
```bash
g++ -std=c++17 -O2 examples/vsepr_opengl_viewer.cpp \
    -o vsepr_opengl_viewer \
    -Iinclude \
    -Ithird_party/glm \
    -pthread
```

**Result:** 130 KB executable with:
- Thread-safe statistics (`std::atomic`, `std::mutex`)
- XYZ export (watch mode)
- Checkpoint system
- Performance metrics (200-300 mol/sec)

---

## 📁 Project Structure

```
vsepr-sim/
├── CMakeLists.txt              # Main CMake configuration
├── README.md                   # Project overview
├── DEFAULT_MD.md               # This file
│
├── apps/                       # Application entry points
│   ├── cli.cpp                 # Unified CLI
│   ├── vsepr-cli/              # Lightweight CLI
│   ├── vsepr-view/             # OpenGL viewer
│   ├── vsepr_batch.cpp         # Batch processor
│   ├── vsepr_opengl_viewer.cpp # Continuous generation
│   ├── hydrate_demo.cpp        # Hydrate demo
│   ├── md_demo.cpp             # MD demo
│   └── molecule_builder.cpp    # Molecule builder
│
├── src/                        # Source code
│   ├── core/                   # Core data types
│   ├── sim/                    # Simulation engine
│   ├── pot/                    # Potentials
│   ├── box/                    # PBC
│   ├── nl/                     # Neighbor lists
│   ├── int/                    # Integrators
│   ├── build/                  # Formula builder
│   ├── vis/                    # Visualization
│   ├── cli/                    # CLI commands
│   ├── spec_parser.cpp         # Spec parser
│   └── command_router.cpp      # Command routing
│
├── include/                    # Public headers (deprecated)
│   └── ...                     # (migrating to src/)
│
├── tests/                      # Test suite
│   ├── run_tests.sh            # Test runner (bash)
│   ├── run_tests.bat           # Test runner (batch)
│   ├── phase2_test.sh          # Phase 2 tests
│   ├── phase3_test.sh          # Phase 3 tests
│   ├── functional/             # Functional tests
│   ├── regression/             # Regression tests
│   └── integration/            # Integration tests
│
├── third_party/                # External dependencies
│   ├── glm/                    # GLM mathematics
│   └── imgui/                  # Dear ImGui
│
├── scripts/                    # Utility scripts
│   ├── build/                  # Build scripts
│   ├── batch_chemistry_demo.sh
│   ├── benchmark_complex.sh
│   ├── random_discovery.sh
│   └── ...
│
├── examples/                   # Example scripts
│   ├── demo_continuous_generation.sh
│   └── demo_xyz_export.sh
│
├── docs/                       # Documentation
├── Documentation/              # Additional docs
├── batch/                      # Batch processing
├── outputs/                    # Output files
├── logs/                       # Log files
├── benchmark_results/          # Benchmark data
└── build/                      # Build directory (generated)
    ├── bin/                    # Executables
    └── lib/                    # Libraries
```

---

## 🌐 Cross-Platform Support

### Platform Detection

**Windows Batch with WSL:**
```batch
@echo off
REM Auto-detect WSL and use bash if available
where bash >nul 2>&1
if %ERRORLEVEL% == 0 (
    bash build_universal.sh %*
) else (
    REM Pure Windows build
    cmake -B build -G "MinGW Makefiles"
    cmake --build build
)
```

**Bash with WSL Check:**
```bash
#!/bin/bash
if grep -qi microsoft /proc/version 2>/dev/null; then
    echo "Running in WSL"
    # WSL-specific paths
else
    echo "Running in native Linux"
fi
```

### Path Handling

**CMake:**
```cmake
# Portable path separators
file(TO_NATIVE_PATH "${CMAKE_BINARY_DIR}/bin" BIN_DIR)

# Windows-specific resources
if(WIN32)
    list(APPEND SOURCES ${PROJECT_SOURCE_DIR}/resources/vsepr.rc)
endif()
```

**Bash:**
```bash
# Convert Windows paths
NATIVE_PATH=$(cygpath -w "$LINUX_PATH" 2>/dev/null || echo "$LINUX_PATH")
```

### Compiler Flags

**CMake:**
```cmake
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -pedantic -pthread)
elseif(MSVC)
    add_compile_options(/W4 /std:c++17)
endif()
```

---

## 🧪 Testing Infrastructure

### Test Categories

**1. Unit Tests** (in `tests/`)
- `geom_ops_tests.cpp` - Geometry operations
- `energy_tests.cpp` - Energy calculations
- `optimizer_tests.cpp` - Optimization algorithms
- `angle_tests.cpp` - Angle bending
- `vsepr_tests.cpp` - VSEPR theory
- `torsion_tests.cpp` - Torsional rotation

**2. Integration Tests** (`tests/integration/`)
- `test_comprehensive.sh` - Full system integration

**3. Regression Tests** (`tests/regression/`)
- `test_phase1_build.sh` - Phase 1 validation
- `test_phase2.sh` - Phase 2 validation

**4. Functional Tests** (`tests/functional/`)
- `quick_test.sh` - Quick sanity check
- `test_demo.sh` - Demo functionality
- `test_fluorides.sh` - Chemistry validation

### Running Tests

**All tests:**
```bash
cd build
ctest --output-on-failure
```

**Specific test:**
```bash
./build/bin/geom_ops_tests
./build/bin/energy_tests
```

**With wrapper scripts:**
```bash
./tests/run_tests.sh          # All tests
./debug.sh test               # Quick tests
./test_thermal.sh             # Thermal tests
```

### Test Output

```
Test project /path/to/build
      Start  1: GeometryOpsTest
 1/30 Test  #1: GeometryOpsTest .................   Passed    0.15 sec
      Start  2: EnergyTest
 2/30 Test  #2: EnergyTest ......................   Passed    0.08 sec
      Start  3: OptimizerTest
 3/30 Test  #3: OptimizerTest ...................   Passed    0.23 sec
[...]
100% tests passed, 0 tests failed out of 30
```

---

## 🔄 Continuous Generation System

### Architecture

**Source:** [apps/vsepr_opengl_viewer.cpp](apps/vsepr_opengl_viewer.cpp)

**Features:**
- Thread-safe statistics (`std::atomic`, `std::mutex`)
- XYZ export (individual files + watch mode)
- Checkpoint system (resume capability)
- Performance metrics (200-300 mol/sec)

### Compilation

```bash
g++ -std=c++17 -O2 apps/vsepr_opengl_viewer.cpp \
    -o vsepr_opengl_viewer \
    -Iinclude \
    -Ithird_party/glm \
    -pthread
```

**Result:** 130 KB executable

### Usage

```bash
# Quick test (1,000 molecules)
./vsepr_opengl_viewer 1000 every-other

# Medium run (100,000 molecules)
./vsepr_opengl_viewer 100000 every-other -c --watch results.xyz --checkpoint 5000

# Large-scale (1 million molecules)
./vsepr_opengl_viewer 1000000 every-other -c --watch million.xyz --checkpoint 10000

# Unlimited (until Ctrl+C)
./vsepr_opengl_viewer 0 every-other -c --watch infinite.xyz
```

### Documentation

- [CONTINUOUS_GENERATION_ARCHITECTURE.md](CONTINUOUS_GENERATION_ARCHITECTURE.md) - Full architecture
- [QUICK_REFERENCE_CONTINUOUS.md](QUICK_REFERENCE_CONTINUOUS.md) - Quick reference
- [CODE_ARCHITECTURE_CONTINUOUS.md](CODE_ARCHITECTURE_CONTINUOUS.md) - Implementation details
- [COMPLETION_SUMMARY.md](COMPLETION_SUMMARY.md) - Completion summary

---

## 🎯 Common Workflows

### 1. First-Time Setup

```bash
# Clone repository (if not already)
git clone <repo_url> vsepr-sim
cd vsepr-sim

# Install dependencies
sudo apt install cmake g++ libglfw3-dev libglew-dev  # Linux
# or
brew install cmake glfw glew  # macOS

# Build
./build_universal.sh --clean

# Run tests
./test_thermal.sh

# Try application
./build/bin/vsepr build H2O --viz
```

### 2. Development Cycle

```bash
# Make code changes in src/

# Rebuild specific target
cmake --build build --target vsepr

# Run specific test
./build/bin/vsepr_tests

# Run full test suite
cd build && ctest
```

### 3. Adding a New Test

```cmake
# In CMakeLists.txt
add_executable(my_new_test tests/my_new_test.cpp)
target_link_libraries(my_new_test vsepr_core vsepr_sim)
add_test(NAME MyNewTest COMMAND my_new_test)
```

```bash
# Rebuild and run
cmake --build build --target my_new_test
./build/bin/my_new_test
```

### 4. Creating a New Application

```cpp
// apps/my_app.cpp
#include "core/types.hpp"
#include "sim/molecule.hpp"

int main() {
    auto mol = vsepr::MoleculeBuilder::from_formula("CH4");
    mol.optimize();
    mol.export_xyz("methane.xyz");
    return 0;
}
```

```cmake
# In CMakeLists.txt
add_executable(my-app apps/my_app.cpp)
target_link_libraries(my-app vsepr_core vsepr_sim vsepr_build)
```

```bash
cmake --build build --target my-app
./build/bin/my-app
```

### 5. Batch Processing

```bash
# Create spec file: my_batch.json
{
  "molecules": [
    {"formula": "H2O", "optimize": true},
    {"formula": "NH3", "optimize": true},
    {"formula": "CH4", "optimize": true}
  ]
}

# Run batch processor
./build/bin/vsepr_batch my_batch.json --output results/
```

---

## 🔧 Troubleshooting

### Build Issues

**CMake not found:**
```bash
# Linux
sudo apt install cmake

# macOS
brew install cmake

# Verify
cmake --version
```

**Compiler not found:**
```bash
# Linux
sudo apt install build-essential

# macOS
xcode-select --install

# Verify
g++ --version
```

**Missing dependencies (GLM):**
```bash
# Clone GLM
git clone https://github.com/g-truc/glm.git third_party/glm
```

**Missing dependencies (GLFW/GLEW):**
```bash
# Linux
sudo apt install libglfw3-dev libglew-dev

# macOS
brew install glfw glew
```

### Compilation Errors

**`std::atomic` undefined:**
```bash
# Add -pthread flag
g++ -std=c++17 -pthread ...
```

**GLM headers not found:**
```bash
# Add include path
g++ -Ithird_party/glm ...
```

**Linker errors:**
```bash
# Check library order (dependencies last)
g++ ... -lvsepr_sim -lvsepr_core  # Correct
g++ ... -lvsepr_core -lvsepr_sim  # Wrong
```

### Runtime Issues

**Binary not found:**
```bash
# Check build directory
ls build/bin/

# Use full path
./build/bin/vsepr build H2O
```

**Shared library error:**
```bash
# Add to LD_LIBRARY_PATH
export LD_LIBRARY_PATH=$PWD/build/lib:$LD_LIBRARY_PATH
```

**Permission denied:**
```bash
chmod +x build/bin/vsepr
chmod +x build_universal.sh
```

### Platform-Specific

**Windows: 'g++' not recognized:**
```batch
REM Install MinGW-w64 or use Visual Studio
REM Or use WSL:
bash build_universal.sh
```

**WSL: Cannot execute binary:**
```bash
# Rebuild in WSL (don't use Windows-compiled binaries)
./build_universal.sh --clean
```

**macOS: Missing OpenGL:**
```bash
# Install Xcode Command Line Tools
xcode-select --install
```

---

## 📚 Documentation Index

### Core Documentation
- [README.md](README.md) - Project overview
- [DEFAULT_MD.md](DEFAULT_MD.md) - This file (master documentation)
- [CHANGELOG.md](CHANGELOG.md) - Version history

### Build & Setup
- [UNIVERSAL_SCRIPTS.md](UNIVERSAL_SCRIPTS.md) - Universal build scripts
- [QUICK_REFERENCE_UNIVERSAL.txt](QUICK_REFERENCE_UNIVERSAL.txt) - Quick reference
- [VSEPR_COMMAND_SETUP.md](VSEPR_COMMAND_SETUP.md) - Command setup

### Continuous Generation
- [CONTINUOUS_GENERATION_ARCHITECTURE.md](CONTINUOUS_GENERATION_ARCHITECTURE.md) - Architecture
- [QUICK_REFERENCE_CONTINUOUS.md](QUICK_REFERENCE_CONTINUOUS.md) - Quick reference
- [CODE_ARCHITECTURE_CONTINUOUS.md](CODE_ARCHITECTURE_CONTINUOUS.md) - Implementation
- [COMPLETION_SUMMARY.md](COMPLETION_SUMMARY.md) - Summary
- [DOCUMENTATION_INDEX.md](DOCUMENTATION_INDEX.md) - Doc index

### Feature Documentation
- [THERMAL_ANIMATIONS_DELIVERY_COMPLETE.md](THERMAL_ANIMATIONS_DELIVERY_COMPLETE.md) - Thermal system
- [THERMAL_ANIMATIONS_INDEX.md](THERMAL_ANIMATIONS_INDEX.md) - Thermal index
- [TRIPLE_OUTPUT_SYSTEM.md](TRIPLE_OUTPUT_SYSTEM.md) - Output system
- [STREAMING_VERIFICATION.md](STREAMING_VERIFICATION.md) - Streaming verification

### Technical References
- [OPENGL_QUICK_REFERENCE.md](OPENGL_QUICK_REFERENCE.md) - OpenGL reference
- [EXPERIMENTAL_README.md](EXPERIMENTAL_README.md) - Experimental features
- [BUILD_STATUS_20260117.md](BUILD_STATUS_20260117.md) - Build status

---

## 🎓 Learning Path

### Beginner: First Time User
1. Read [README.md](README.md) - Understand project goals
2. Run `./build_universal.sh` - Build the project
3. Run `./test_thermal.sh` - See it work
4. Try `./build/bin/vsepr build H2O --viz` - First molecule

### Intermediate: Developer
1. Read [CMakeLists.txt](CMakeLists.txt) - Build system
2. Explore [src/](src/) - Source code organization
3. Read [tests/](tests/) - Test examples
4. Create your own application (see [Common Workflows](#-common-workflows))

### Advanced: Contributor
1. Read [CONTINUOUS_GENERATION_ARCHITECTURE.md](CONTINUOUS_GENERATION_ARCHITECTURE.md) - Advanced systems
2. Study [apps/vsepr_opengl_viewer.cpp](apps/vsepr_opengl_viewer.cpp) - Complex implementation
3. Review [scripts/](scripts/) - Automation patterns
4. Contribute new features or tests

---

## 🚀 Summary

This project provides a comprehensive molecular simulation framework with:

✅ **Multiple Build Systems**: CMake, shell scripts, batch files  
✅ **Cross-Platform**: Linux, Windows, macOS, WSL  
✅ **Extensive Testing**: Unit, integration, regression, functional  
✅ **Rich Automation**: 70+ shell scripts for common tasks  
✅ **Modern C++17**: Thread-safe, efficient, scalable  
✅ **Visualization**: OpenGL, ImGui, XYZ export  
✅ **Performance**: 200-300 molecules/sec continuous generation  

**Start exploring with:**
```bash
./build_universal.sh && ./test_thermal.sh
```

**Happy building! 🎉**
