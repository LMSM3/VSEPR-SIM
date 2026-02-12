# VSEPR-Sim Quick Build Guide

**Version:** 2.4.0 - Day 11 Release  
**Last Updated:** January 30, 2026  

---

## Prerequisites

- **C++ Compiler:** GCC 7+, Clang 10+, or MSVC 2019+
- **CMake:** 3.15+
- **For 3D Visualization:** OpenGL 3.3+, GLFW3, GLEW

---

## Quick Build (WSL/Linux - Recommended)

```bash
# 1. Navigate to project
cd /mnt/c/Users/Liam/Desktop/vsepr-sim  # Or your path

# 2. Create build directory
mkdir -p build && cd build

# 3. Configure
cmake ..

# 4. Build core tools (fastest)
make meso-build meso-sim -j8

# 5. Optional: Build 3D viewers
make interactive-viewer simple-viewer -j8
```

**Build time:** ~2 minutes  
**Result:** Executables in `build/` directory

---

## Test the Build

```bash
# Test molecular builder
./meso-build
⚛ list
⚛ build water
⚛ info
⚛ save test.xyz
⚛ exit

# Verify file created
cat test.xyz

# Test 3D viewer (if built)
./interactive-viewer test.xyz
```

---

## Available Applications

### Core CLI Tools (No GUI Required)

| Tool | Build Command | Purpose |
|------|---------------|---------|
| **meso-build** | `make meso-build` | Create molecules interactively |
| **meso-sim** | `make meso-sim` | Run simulations |
| **meso-relax** | `make meso-relax` | Energy minimization |
| **meso-align** | `make meso-align` | Structure alignment |
| **meso-discover** | `make meso-discover` | Reaction discovery |

### Visualization Tools (Require OpenGL)

| Tool | Build Command | Purpose |
|------|---------------|---------|
| **interactive-viewer** | `make interactive-viewer` | Full 3D viewer with UI |
| **simple-viewer** | `make simple-viewer` | Lightweight viewer |

### Build Everything

```bash
cmake .. -DBUILD_APPS=ON -DBUILD_VIS=ON
make -j8
```

---

## Windows-Specific Setup

### Using WSL (Recommended)

```powershell
# 1. Install WSL
wsl --install

# 2. Inside WSL, follow Linux build steps above

# 3. Access Windows files
cd /mnt/c/Users/YourName/Desktop/vsepr-sim
```

### Native Windows Build

**Option A: PowerShell with WSL** (Easiest)
```powershell
wsl bash -c "cd /mnt/c/Users/Liam/Desktop/vsepr-sim/build && make meso-build -j8"
```

**Option B: Visual Studio** (GUI Development)
```powershell
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release -j 8
```

---

## Double-Click XYZ Files (Windows)

### Setup (One-Time)

```powershell
# Run as Administrator
.\install_xyz_association.ps1

# Or all-in-one
.\setup_xyz_doubleclick.ps1
```

### Test

Double-click any `.xyz` file in Windows Explorer → Interactive viewer opens!

**See:** `DOUBLE_CLICK_QUICKREF.md` for details

---

## Troubleshooting

### "make: command not found"

**Solution:** Use WSL or install MinGW/MSYS2

```powershell
# WSL
wsl --install

# Or install MSYS2 from msys2.org
```

### "CMake Error: Could not find CMAKE_C_COMPILER"

**Solution:** Install build tools

```bash
# Ubuntu/Debian
sudo apt-get install build-essential cmake

# MSYS2
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake
```

### "GL/glew.h: No such file"

**Only needed for visualization tools.**

```bash
# Ubuntu/Debian
sudo apt-get install libglfw3-dev libglew-dev

# Skip visualization if not needed:
make meso-build meso-sim -j8  # CLI tools only
```

### Build is Slow

```bash
# Use more parallel jobs
make -j16  # Or your CPU core count

# Build only what you need
make meso-build  # Just the builder
```

---

## Build Options

```bash
# Minimal (CLI tools only, no graphics)
cmake .. -DBUILD_APPS=ON -DBUILD_VIS=OFF
make meso-build meso-sim -j8

# With visualization
cmake .. -DBUILD_APPS=ON -DBUILD_VIS=ON
make interactive-viewer -j8

# Everything (tests, GUI, etc.)
cmake .. -DBUILD_ALL=ON
make -j8
```

---

## Quick Commands Reference

```bash
# Clean rebuild
rm -rf build && mkdir build && cd build && cmake .. && make meso-build -j8

# Rebuild after code changes
cd build && make -j8

# Test build
./meso-build <<< "build water\nsave test.xyz\nexit" && cat test.xyz

# Check what was built
ls -lh meso-* interactive-viewer simple-viewer 2>/dev/null
```

---

## Performance Notes

- **Release builds:** 3-5x faster than Debug
- **Parallel build:** Use `-j` with your CPU core count
- **Build time:** 
  - Core tools (CLI): ~1-2 minutes
  - With visualization: ~3-5 minutes
  - Full project: ~10-15 minutes

---

## Platform Notes

### Linux
- ✅ Recommended for development
- ✅ Best performance
- ✅ Easy dependency management

### Windows WSL
- ✅ Best of both worlds
- ✅ Access Windows files via `/mnt/c/`
- ✅ Linux build tools + Windows GUI integration

### Windows Native
- ⚠️ More complex setup
- ⚠️ Visual Studio recommended for GUI work
- ✅ PowerShell + WSL works for CLI tools

### macOS
- ✅ Works with Homebrew dependencies
- ⚠️ OpenGL 3.3 deprecated (use MoltenVK for Vulkan)

---

## Next Steps

After building:

1. **Try meso-build:**
   ```bash
   ./build/meso-build
   ⚛ help
   ```

2. **Create a molecule:**
   ```bash
   ⚛ build cisplatin
   ⚛ save cisplatin.xyz
   ```

3. **View in 3D:**
   ```bash
   ./build/interactive-viewer cisplatin.xyz
   ```

4. **Set up double-click** (Windows):
   ```powershell
   .\install_xyz_association.ps1
   ```

---

**Build successful?** Start with `./build/meso-build` and type `help`! 🚀
