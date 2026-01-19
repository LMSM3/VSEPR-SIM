# Batch File Quick Reference

## 🚀 Main Build Commands

### `build.bat` - Main Build Script
```batch
build.bat              # Standard Release build
build.bat --clean      # Clean rebuild
build.bat --debug      # Debug build
```

**What it does:**
- Configures CMake
- Builds the VSEPR-Sim executable
- Creates `build\bin\vsepr.exe`

---

### `build_universal.bat` - Advanced Build
```batch
build_universal.bat                    # Standard build
build_universal.bat --clean --jobs 16  # Clean rebuild with 16 cores
build_universal.bat --target vsepr     # Build specific target
build_universal.bat --debug --verbose  # Debug build with output
```

**Features:**
- WSL auto-detection (uses bash script if available)
- Parallel builds
- Target-specific builds
- Verbose output mode

---

## 🧪 Testing Scripts

### `test_thermal.bat` - Thermal Properties Tests
```batch
test_thermal.bat
```

**What it does:**
1. Builds test molecules: H2O, NH3, CH4, Br2
2. Runs thermal analysis on each
3. Reports pass/fail summary

**Output:** `outputs\thermal_tests\*.xyz`

---

### `debug.bat` - Diagnostic Tool
```batch
debug.bat              # Show system info (default)
debug.bat info         # System information
debug.bat build        # Debug build
debug.bat test         # Quick functionality tests
debug.bat clean        # Clean build directory
debug.bat rebuild      # Clean rebuild
debug.bat thermal      # Run thermal tests
debug.bat wsl thermal  # Use WSL for testing
```

---

## 📋 Complete Workflow Example

### 1. Initial Setup
```batch
# First time build
build.bat --clean

# Verify installation
debug.bat info
```

### 2. Build Molecules
```batch
# Using the executable directly
build\bin\vsepr.exe build H2O --output water.xyz
build\bin\vsepr.exe build CCl4 --output ccl4.xyz
build\bin\vsepr.exe build NH3 --output ammonia.xyz
```

### 3. Export to WebGL
```batch
# Single molecule
build\bin\vsepr.exe webgl water.xyz --output molecules.json --name "Water"

# Create batch file (molecules.txt):
#   H2O Water
#   NH3 Ammonia
#   CCl4 Carbon_Tetrachloride

# Batch export
build\bin\vsepr.exe webgl --batch molecules.txt --output webgl_molecules.json
```

### 4. View in Browser
```batch
# Open the viewer
start outputs\universal_viewer.html
```

### 5. Test Everything
```batch
# Run thermal tests
test_thermal.bat

# Quick diagnostics
debug.bat test
```

---

## 🔧 Troubleshooting

### Build Fails
```batch
# Clean rebuild
build.bat --clean

# Debug mode with verbose output
build_universal.bat --clean --debug --verbose

# Check system info
debug.bat info
```

### Binary Not Found
```batch
# Verify build completed
dir build\bin\vsepr.exe

# Rebuild if missing
build.bat --clean
```

### Tests Fail
```batch
# Check thermal system
debug.bat thermal

# Manual test
build\bin\vsepr.exe build H2O --output test.xyz
build\bin\vsepr.exe therm test.xyz
```

---

## 📁 Output Locations

| Output | Location |
|--------|----------|
| Executable | `build\bin\vsepr.exe` |
| XYZ Files | `outputs\*.xyz` |
| JSON Files | `outputs\*.json` |
| Thermal Tests | `outputs\thermal_tests\*.xyz` |
| WebGL Viewer | `outputs\universal_viewer.html` |

---

## ✅ All Batch Files Working

| Script | Status | Purpose |
|--------|--------|---------|
| `build.bat` | ✅ Fixed | Main build script |
| `build_universal.bat` | ✅ Working | Advanced build with WSL support |
| `test_thermal.bat` | ✅ Fixed | Thermal property tests |
| `debug.bat` | ✅ Working | Diagnostic tool |

---

## 🎯 Common Use Cases

### Development Workflow
```batch
# 1. Build
build.bat

# 2. Test changes
debug.bat test

# 3. Generate molecules
build\bin\vsepr.exe build H2O --output water.xyz

# 4. Export for visualization
build\bin\vsepr.exe webgl water.xyz -o molecules.json

# 5. View
start outputs\universal_viewer.html
```

### Clean Build Cycle
```batch
debug.bat clean
build.bat
test_thermal.bat
```

### Full System Test
```batch
build.bat --clean
debug.bat test
test_thermal.bat
```

---

## 💡 Pro Tips

1. **Use WSL when available** - Faster builds with bash scripts
   ```batch
   build_universal.bat  # Auto-detects WSL
   ```

2. **Parallel builds** - Speed up compilation
   ```batch
   build_universal.bat --jobs 16
   ```

3. **Target specific builds** - Build only what you need
   ```batch
   build_universal.bat --target vsepr
   ```

4. **Quick diagnostics** - Always check first
   ```batch
   debug.bat info
   ```

---

**All batch files now fully operational!** 🎉
