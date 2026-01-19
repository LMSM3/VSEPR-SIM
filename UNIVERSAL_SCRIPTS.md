# Universal Build & Debug Scripts

Cross-platform build, test, and debug scripts for seamless operation on Windows and Linux/WSL.

## 📋 Quick Reference

| Script | Purpose | Windows | Linux/WSL |
|--------|---------|---------|-----------|
| `build_universal` | Build project | `.bat` | `.sh` |
| `test_thermal` | Test thermal system | `.bat` | `.sh` |
| `debug` | Debug & diagnostics | `.bat` | `.sh` |

## 🔨 Build Scripts

### Build (Bash)
```bash
./build_universal.sh                    # Standard release build
./build_universal.sh --debug            # Debug build
./build_universal.sh --clean --jobs 16  # Clean rebuild with 16 cores
./build_universal.sh --target vsepr     # Build only vsepr binary
./build_universal.sh --verbose          # Verbose output
```

### Build (Batch/Windows)
```batch
build_universal.bat                     # Standard release build
build_universal.bat --debug             # Debug build
build_universal.bat --clean --jobs 16   # Clean rebuild
build_universal.bat --target vsepr      # Build only vsepr binary
```

**Features:**
- ✅ Automatic CMake configuration
- ✅ Parallel compilation (default: 8 jobs)
- ✅ Clean build option
- ✅ Debug/Release modes
- ✅ Specific target selection
- ✅ **WSL auto-detection** (Windows batch calls bash if WSL available)
- ✅ Colored output with build status

## 🧪 Thermal Test Scripts

### Test Thermal System (Bash)
```bash
./test_thermal.sh
```

### Test Thermal System (Batch)
```batch
test_thermal.bat
```

**Test Coverage:**
- H₂O (water) - Molecular bonding
- NH₃ (ammonia) - Molecular bonding  
- CH₄ (methane) - Molecular bonding
- Br₂ (bromine) - Molecular bonding
- NaCl (salt at 800K) - Ionic bonding
- Cu₈ (copper cluster) - Metallic bonding
- C₆H₆ (benzene) - Covalent bonding

**Output:**
```
╔════════════════════════════════════════════════════════════════╗
║  Thermal Properties System Test Suite                         ║
╚════════════════════════════════════════════════════════════════╝

═══════════════════════════════════════════════════════════════
  Building Test Molecules
═══════════════════════════════════════════════════════════════

▶ Building H2O → water.xyz
  ✓ Created water.xyz
▶ Building NH3 → ammonia.xyz
  ✓ Created ammonia.xyz
...

═══════════════════════════════════════════════════════════════
  Running Thermal Analysis
═══════════════════════════════════════════════════════════════

▶ Testing water at 298.15K (expected: molecular bonding)
  ✓ Thermal analysis passed
    Thermal conductivity: 0.60 W/m·K
    Phase state: liquid
...

═══════════════════════════════════════════════════════════════
  Test Summary
═══════════════════════════════════════════════════════════════

  Tests passed: 7
  Tests failed: 0

╔════════════════════════════════════════════════════════════════╗
║  ✓ All thermal tests passed!                                  ║
╚════════════════════════════════════════════════════════════════╝
```

## 🐛 Debug Scripts

### Debug Tool (Bash)
```bash
./debug.sh info           # System information
./debug.sh build          # Debug build with verbose output
./debug.sh test           # Quick functionality tests
./debug.sh clean          # Clean build artifacts
./debug.sh rebuild        # Clean rebuild
./debug.sh thermal        # Test thermal system
./debug.sh colocation     # Test colocation validation
./debug.sh help           # Show help
```

### Debug Tool (Batch)
```batch
debug.bat info            # System information
debug.bat build           # Debug build
debug.bat test            # Quick tests
debug.bat clean           # Clean
debug.bat rebuild         # Clean rebuild
debug.bat thermal         # Thermal tests
debug.bat wsl thermal     # Use WSL bash version
debug.bat help            # Show help
```

### Debug Modes Explained

#### `info` - System Information
Shows:
- OS version
- CMake availability and version
- Compiler detection (g++, clang++, MSVC)
- Build directory status
- Binary locations

#### `build` - Debug Build
- Builds in Debug mode
- Enables verbose output
- Shows full compilation commands

#### `test` - Quick Tests
Runs:
1. `vsepr --version`
2. `vsepr build H2O` 
3. `vsepr therm water.xyz`

#### `thermal` - Thermal System Tests
Full thermal properties test suite (calls `test_thermal.sh/.bat`)

#### `colocation` - Validation Test
Tests the atom colocation prevention system with Br₂ (previously had bug)

## 🔄 Cross-Platform Features

### Automatic WSL Detection (Windows)
Both `build_universal.bat` and `debug.bat` automatically detect WSL:

```batch
where wsl >nul 2>&1
if %errorlevel%==0 (
    echo [INFO] WSL detected - using bash build script
    wsl bash -c "cd '%cd:\=/%' && ./build_universal.sh %*"
)
```

**Benefits:**
- Use familiar bash scripts on Windows
- Better colored output
- Consistent behavior across platforms
- Fallback to native batch if WSL unavailable

### Manual WSL Invocation
```batch
debug.bat wsl info        # Force WSL bash version
debug.bat wsl thermal     # Run thermal tests in WSL
```

## 📊 Example Workflows

### Fresh Build (Linux)
```bash
./build_universal.sh --clean
./test_thermal.sh
./debug.sh info
```

### Fresh Build (Windows with WSL)
```batch
build_universal.bat --clean
test_thermal.bat
debug.bat info
```

### Development Cycle
```bash
# 1. Make code changes
# 2. Quick rebuild specific target
./build_universal.sh --target vsepr

# 3. Test changes
./debug.sh test

# 4. Full thermal validation
./test_thermal.sh
```

### Debugging Build Issues
```bash
# Clean rebuild with full output
./debug.sh rebuild

# Check system info
./debug.sh info

# Verify colocation fix
./debug.sh colocation
```

## 🎯 Common Tasks

### Build and Test Everything
**Bash:**
```bash
./build_universal.sh --clean && ./test_thermal.sh
```

**Batch:**
```batch
build_universal.bat --clean && test_thermal.bat
```

### Quick Validation After Changes
```bash
./build_universal.sh --target vsepr && ./debug.sh test
```

### Debug Build with Full Testing
```bash
./build_universal.sh --debug --verbose
./test_thermal.sh
```

### Clean State Development
```bash
./debug.sh clean
./build_universal.sh
./test_thermal.sh
```

## 🚀 Performance Tips

### Faster Builds
```bash
# Use all CPU cores
./build_universal.sh --jobs $(nproc)

# Windows
build_universal.bat --jobs %NUMBER_OF_PROCESSORS%
```

### Incremental Builds
```bash
# Only rebuild changed files
./build_universal.sh --target vsepr
```

## 🔍 Troubleshooting

### Build Fails
```bash
# 1. Check system info
./debug.sh info

# 2. Clean rebuild
./debug.sh rebuild

# 3. Debug build with verbose output
./debug.sh build
```

### Tests Fail
```bash
# 1. Verify binary exists
./debug.sh info

# 2. Run basic tests
./debug.sh test

# 3. Check specific thermal tests
./test_thermal.sh
```

### WSL Issues (Windows)
```batch
# Force native batch version
set USE_WSL=0
build_universal.bat

# Or explicitly use WSL
debug.bat wsl thermal
```

## 📝 Script Options Reference

### build_universal Options
| Option | Short | Description |
|--------|-------|-------------|
| `--debug` | `-d` | Debug build mode |
| `--clean` | `-c` | Clean before building |
| `--jobs N` | `-j N` | Parallel jobs (default: 8) |
| `--target T` | `-t T` | Specific target (default: all) |
| `--verbose` | `-v` | Verbose output |
| `--help` | `-h` | Show help |

### debug Options
| Option | Short | Description |
|--------|-------|-------------|
| `info` | `-i` | System information |
| `build` | `-b` | Debug build |
| `test` | `-t` | Quick tests |
| `clean` | `-c` | Clean artifacts |
| `rebuild` | `-r` | Clean rebuild |
| `thermal` | `-th` | Thermal tests |
| `colocation` | `-col` | Colocation test (bash only) |
| `help` | `-h` | Show help |

### test_thermal Options
No options - runs full test suite automatically

## ✨ Features Summary

✅ **Cross-platform**: Bash and Batch versions with identical functionality  
✅ **WSL Integration**: Automatic detection and fallback on Windows  
✅ **Colored Output**: Clear visual feedback  
✅ **Error Handling**: Proper exit codes and error messages  
✅ **Parallel Builds**: Configurable job count  
✅ **Target Selection**: Build specific components  
✅ **Debug Modes**: Multiple diagnostic modes  
✅ **Test Automation**: Complete thermal system validation  
✅ **Clean Builds**: Easy artifact cleanup  
✅ **Help System**: Built-in documentation

---

**All scripts are located in the project root directory for easy access.**
