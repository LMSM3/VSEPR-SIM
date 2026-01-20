# Build Instructions for VSEPR-SIM

## Quick Start

### Prerequisites
- CMake 3.15 or higher
- C++20 compatible compiler (GCC 13+, Clang 14+, MSVC 2022+)

### Building the Project

#### Option 1: Build without visualization (Recommended)
```bash
mkdir build
cd build
cmake .. -DBUILD_VIS=OFF -DBUILD_TESTS=OFF
cmake --build .
```

#### Option 2: Build with visualization (Requires OpenGL, GLFW, GLEW, ImGui)
```bash
mkdir build
cd build
cmake .. -DBUILD_VIS=ON -DBUILD_TESTS=OFF
cmake --build .
```

### Build Options
- `-DBUILD_VIS=OFF` - Disable visualization tools (default: ON)
- `-DBUILD_TESTS=OFF` - Disable test suite (default: ON, but test files don't exist yet)
- `-DBUILD_APPS=OFF` - Disable applications (default: ON)
- `-DBUILD_DEMOS=OFF` - Disable demo targets (default: OFF)

## Built Executables

After building, executables are located in `build/bin/`:

### Main CLI Tool
```bash
./build/bin/vsepr --help
./build/bin/vsepr help
./build/bin/vsepr version
./build/bin/vsepr build H2O
```

### Simple CLI
```bash
./build/bin/vsepr-cli H2O
```

### Batch Runner
```bash
./build/bin/vsepr_batch input_spec.txt
```

### API Test Suite
```bash
./build/bin/xyz_suite_test
./build/bin/xyz_suite_test --test-xyz
./build/bin/xyz_suite_test --test-xyzc
./build/bin/xyz_suite_test --test-api
```

## Troubleshooting

### CMake Configuration Fails
- **Issue**: Missing BUILD_TESTS=OFF
- **Solution**: Always use `-DBUILD_TESTS=OFF` as test source files don't exist yet

### Visualization Build Fails
- **Issue**: Missing OpenGL, GLFW, GLEW, or ImGui dependencies
- **Solution**: Either install dependencies or use `-DBUILD_VIS=OFF`

### Link Errors
- **Issue**: Missing library definitions
- **Solution**: This should be fixed now. All referenced libraries are defined.

## Project Structure

```
VSEPR-SIM/
├── CMakeLists.txt         # Main build configuration
├── src/                   # Source code
│   ├── core/             # Core types and utilities
│   ├── sim/              # Simulation engine
│   ├── io/               # XYZ file I/O
│   ├── thermal/          # XYZC thermal format
│   ├── api/              # Production API
│   ├── cli/              # CLI commands
│   └── ...               # Other modules
├── apps/                  # Application entry points
│   ├── cli.cpp           # Main CLI
│   ├── vsepr-cli/        # Simple CLI
│   ├── vsepr_batch.cpp   # Batch runner
│   └── xyz_suite_test.cpp # API tests
├── include/               # Public headers
│   ├── io_api.h          # C API
│   ├── xyz_format.h      # XYZ format
│   └── xyzc_format.h     # XYZC format
└── build/                 # Build output (gitignored)
    ├── bin/              # Executables
    └── lib/              # Libraries
```

## Current Implementation Status

### Implemented ✅
- Complete build system
- Directory structure
- Core data types (Vec3, Atom, Molecule)
- XYZ file I/O
- XYZC thermal format I/O
- C API wrapper
- CLI command routing
- All application entry points

### Stub/TODO ⚠️
- CLI command implementations (print TODO messages)
- Spec parser (DSL/JSON parsing)
- Simulation engine
- Visualization system
- Test suite

## Next Steps

1. Implement actual CLI command functionality
2. Add real simulation engine code
3. Create test source files for BUILD_TESTS=ON
4. Add ImGui dependencies for BUILD_VIS=ON
5. Replace stub implementations with full functionality

## Support

See BUILD_FIX_SUMMARY.md for detailed information about what was fixed and why.
