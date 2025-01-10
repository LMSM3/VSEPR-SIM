# Crystal Visualization Integration — Complete Guide

## Overview

The `--viz` flag bridges the CLI crystal emission to your native OpenGL renderer (`CrystalGridRenderer`). This provides immediate, interactive visualization of generated crystal structures without intermediate XYZ files.

---

## Quick Start

```bash
# Build with visualization support
cmake -B build -DBUILD_VIS=ON
cmake --build build

# Generate and visualize NaCl crystal
./build/vsepr NaCl@crystal emit --preset nacl_atomistic --viz

# Build 2×2×2 supercell and visualize
./build/vsepr Al@crystal emit --preset al --supercell 2,2,2 --viz
```

---

## Architecture

### Data Flow

```
User Command
     ↓
vsepr CLI (apps/vsepr.cpp)
     ↓
ParsedCommand {preset, supercell, viz_enabled}
     ↓
emit_crystal() (src/cli/emit_crystal.cpp)
     ↓
CrystalEmissionResult {atoms, lattice, name}
     ↓  (if --viz)
launch_crystal_visualizer() (src/cli/crystal_visualizer.cpp)
     ↓
atoms_to_crystal_structure() — Convert CLI → render format
     ↓
CrystalGridRenderer::render() — OpenGL window
```

### Format Conversion

| Stage | Format | Coordinates | Use |
|-------|--------|-------------|-----|
| **Backend** | `atomistic::crystal::UnitCell` | Fractional | Math & physics |
| **CLI** | `cli::Atom` | Cartesian | XYZ output |
| **Render** | `render::CrystalStructure` | Fractional | Visualization |

**Key Insight**: The lattice is preserved through the pipeline to enable correct fractional coordinate conversion for rendering.

---

## Implementation Details

### 1. **Lattice Preservation** (`emit_crystal.cpp`)

**Problem**: Old code lost lattice information after generating atoms.

**Solution**: New `CrystalEmissionResult` struct:
```cpp
struct CrystalEmissionResult {
    std::vector<Atom> atoms;
    atomistic::crystal::Lattice lattice;  // ← Preserved!
    std::string name;
    int space_group;
    std::string space_symbol;
};
```

### 2. **Coordinate Conversion** (`crystal_visualizer.cpp`)

**Cartesian → Fractional**:
```cpp
atomistic::Vec3 cart = {atom.x, atom.y, atom.z};
atomistic::Vec3 frac = lattice.to_fractional(cart);  // A⁻¹·r

// Wrap to [0, 1)
frac.x = frac.x - std::floor(frac.x);
frac.y = frac.y - std::floor(frac.y);
frac.z = frac.z - std::floor(frac.z);

crystal_atom.fractional = {frac.x, frac.y, frac.z};
```

### 3. **CPK Coloring** (`crystal_visualizer.cpp`)

Standard Corey-Pauling-Koltun scheme:
```cpp
std::array<uint8_t, 3> cpk_color(const std::string& element) {
    // Na → {171, 92, 242} (purple)
    // Cl → { 31,240,  31} (green)
    // Al → {191,166, 166} (silver-gray)
    // Au → {255,209,  35} (gold)
    // ...
}
```

### 4. **Conditional Compilation** (`CMakeLists.txt`)

```cmake
if(BUILD_VIS)
    target_sources(vsepr_cli PRIVATE src/cli/crystal_visualizer.cpp)
    target_link_libraries(vsepr_cli PUBLIC vsepr_vis)
    target_compile_definitions(vsepr_cli PRIVATE ENABLE_CRYSTAL_VIZ)
endif()
```

**Fallback**: If built without `-DBUILD_VIS=ON`, `--viz` prints an error message:
```
ERROR: Visualization Not Enabled
The --viz flag requires: cmake -DBUILD_VIS=ON ...
Falling back to XYZ output instead.
```

---

## Usage Examples

### Basic Visualization

```bash
# Unit cell only
vsepr Al@crystal emit --preset al --viz
# Opens: "Aluminum FCC" with 4 atoms

# Supercell
vsepr NaCl@crystal emit --preset nacl_atomistic --supercell 3,3,3 --viz
# Opens: "NaCl [3×3×3]" with 216 atoms
```

### Comparison: XYZ vs Viz

```bash
# Traditional: Generate XYZ, then visualize separately
vsepr Al@crystal emit --preset al --supercell 2,2,2 --out al_32.xyz
# (manual step: open al_32.xyz in VMD/OVITO)

# New: Immediate visualization
vsepr Al@crystal emit --preset al --supercell 2,2,2 --viz
# Opens native viewer instantly
```

### Exploring Different Metals

```bash
# FCC metals (coordination 12)
vsepr Al@crystal emit --preset al --viz
vsepr Cu@crystal emit --preset cu --viz
vsepr Au@crystal emit --preset au --viz

# BCC metal (coordination 8)
vsepr Fe@crystal emit --preset fe --viz
```

### Ionic Crystals

```bash
# Rocksalt structure (Na-Cl octahedral coordination)
vsepr NaCl@crystal emit --preset nacl_atomistic --viz

# MgO (same structure, different lattice param)
vsepr MgO@crystal emit --preset mgo --viz

# CsCl (body-centered cubic, 8-fold coordination)
vsepr CsCl@crystal emit --preset cscl --viz
```

### Covalent Networks

```bash
# Silicon diamond (tetrahedral sp³)
vsepr Si@crystal emit --preset si --viz

# Carbon diamond
vsepr C@crystal emit --preset diamond --viz
```

### Oxide Structures

```bash
# Rutile TiO₂ (tetragonal)
vsepr TiO2@crystal emit --preset tio2 --viz
```

### Large Supercells

```bash
# 5×5×5 Al supercell (500 atoms)
vsepr Al@crystal emit --preset al --supercell 5,5,5 --viz

# Performance note: Renderer handles up to ~10,000 atoms smoothly
# Beyond that, consider --out xyz.file for external visualization
```

---

## Renderer Features

When `--viz` launches, you get:

### Automatic Features

1. **Coordination Polyhedra**
   - Calculated using 3.5 Å cutoff
   - Translucent (30% opacity)
   - Inverted mean RGB coloring (your custom scheme)

2. **Unit Cell Wireframe**
   - Cyan edges showing lattice vectors
   - Helps visualize periodicity

3. **CPK Element Coloring**
   - Standard chemistry convention
   - Na=purple, Cl=green, Al=gray, Au=gold, etc.

4. **Interactive Controls**
   - Mouse drag: Rotate view
   - Scroll: Zoom in/out
   - ESC: Close window

### Console Output

```
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
  Launching Native Crystal Visualizer
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Structure: Aluminum FCC [2×2×2]
  Atoms: 32
  Lattice: 8.10 × 8.10 × 8.10 Å
  Angles: α=90.0°, β=90.0°, γ=90.0°
  Volume: 531.4 Å³

Rendering features:
  ✓ Coordination polyhedra (translucent)
  ✓ Unit cell edges (cyan wireframe)
  ✓ CPK element coloring

Controls:
  Mouse drag: Rotate view
  Scroll: Zoom in/out
  ESC: Close window

Starting render loop...
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

---

## Deterministic Properties

### No Randomness
- All atomic positions are **deterministically** calculated from lattice math
- Same command → same structure, always
- No RNG, no stochastic placement

### No Hardcoded Outcomes
- Presets are **initial conditions**, not forced results
- User can:
  - Modify lattice parameters (`--cell`)
  - Build custom alloys (future: `--basis` flag)
  - Relax structures with FIRE (future: `--relax` flag)
  - Change supercell dimensions (`--supercell`)

### Universal Element Support
- Works with **any atomic number** (Z=1 to Z=118)
- CPK colors fall back to gray for unknown elements
- Covalent radii default to 1.5 Å

### Lattice Flexibility
- Supports **triclinic** lattices (most general)
- Cubic, tetragonal, orthorhombic, monoclinic, hexagonal all work
- Current CLI: orthogonal cells (future: add `--angles` flag)

---

## Build Configuration

### Full Visualization Support

```bash
cmake -B build \
  -DBUILD_VIS=ON \
  -DBUILD_TESTS=ON \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Without Visualization (CLI-only)

```bash
cmake -B build \
  -DBUILD_VIS=OFF \
  -DBUILD_TESTS=ON
cmake --build build
```

In this mode:
- XYZ output still works
- `--viz` flag prints error + suggests rebuild

---

## Error Messages

### Visualization Not Enabled

```
ERROR: Visualization Not Enabled
The --viz flag requires: cmake -DBUILD_VIS=ON ...
Falling back to XYZ output instead.
```

**Fix**: Rebuild with `-DBUILD_VIS=ON`

### OpenGL/GLFW Error

```
ERROR: Visualizer failed: Failed to initialize GLFW
Note: Native visualization requires OpenGL 3.3+ and GLFW
```

**Fix**: Ensure your system has:
- OpenGL 3.3+ drivers
- GLFW library installed
- Display server running (X11/Wayland on Linux, not headless SSH)

### Unknown Preset

```
ERROR: Unknown preset or generation failed: xyz
Run without --preset to see available options.
```

**Fix**: Check preset name spelling or omit `--preset` to list available options

---

## Testing

### Unit Tests

```bash
# Crystal module tests (lattice math, presets, supercells)
./build/test_crystal_pipeline

# Expected:
#   ✓ Cubic lattice math
#   ✓ Coordinate transforms
#   ✓ MIC calculations
#   ✓ All 10 presets load
#   ✓ Supercell construction (2×2×2)
#   ✓ Bond inference with PBC
#   ✓ FCC coordination = 12
```

### Manual Visual Inspection

```bash
# Known structures to verify
vsepr Al@crystal emit --preset al --viz  # Should show FCC with 12 neighbors
vsepr Fe@crystal emit --preset fe --viz  # Should show BCC with 8 neighbors
vsepr NaCl@crystal emit --preset nacl_atomistic --viz  # Octahedral Na, tetrahedral Cl
vsepr Si@crystal emit --preset si --viz  # Diamond structure, tetrahedral
```

---

## Future Extensions

### Planned Features

1. **Direct relaxation**:
   ```bash
   vsepr Al@crystal emit --preset al --supercell 3,3,3 --relax --viz
   ```

2. **Triclinic angles**:
   ```bash
   vsepr Custom@crystal emit --cell 5,6,7 --angles 80,85,95 --viz
   ```

3. **Custom basis from JSON**:
   ```bash
   vsepr FeCu@crystal emit --basis alloy.json --viz
   ```

4. **Animation (e.g., thermal vibrations)**:
   ```bash
   vsepr Al@crystal emit --preset al --animate-thermal --T 300 --viz
   ```

5. **Comparison view (multiple structures)**:
   ```bash
   vsepr Al@crystal emit --preset al --viz --compare fe
   # Split-screen: Al FCC vs Fe BCC
   ```

---

## Comparison to External Tools

| Feature | vsepr --viz | VMD | OVITO | ASE gui |
|---------|-------------|-----|-------|---------|
| **Immediate launch** | ✅ | ❌ (manual load) | ❌ | ❌ |
| **Coordination polyhedra** | ✅ | ⚠️ (scripting) | ✅ | ❌ |
| **Inverted RGB coloring** | ✅ | ❌ | ❌ | ❌ |
| **CLI integration** | ✅ | ❌ | ❌ | ⚠️ |
| **Built-in to engine** | ✅ | ❌ | ❌ | ❌ |
| **Cross-platform** | ✅ | ✅ | ✅ | ✅ |

**Advantage**: No context switch—generate and visualize in one command.

---

## Troubleshooting

### Issue: Window Opens but Black Screen

**Cause**: OpenGL context failure

**Fix**:
```bash
# Check OpenGL version
glxinfo | grep "OpenGL version"  # Should be >= 3.3

# Try forcing software rendering (slow but reliable)
export LIBGL_ALWAYS_SOFTWARE=1
vsepr Al@crystal emit --preset al --viz
```

### Issue: Polyhedra Not Showing

**Cause**: Coordination cutoff too small

**Fix**: The cutoff is hardcoded to 3.5 Å. For metals with large lattice params, increase in `crystal_visualizer.cpp`:
```cpp
renderer.set_coordination_cutoff(4.5);  // Adjust as needed
```

### Issue: Too Many Atoms (Slow Rendering)

**Cause**: Supercells > 10,000 atoms can lag on older GPUs

**Fix**:
1. Reduce supercell: `--supercell 3,3,3` instead of `5,5,5`
2. Use XYZ output for external visualization: `--out large.xyz` (no `--viz`)
3. Use level-of-detail (future feature)

---

## Summary

**Status**: ✅ Fully implemented and tested  
**Dependencies**: Requires `-DBUILD_VIS=ON`  
**Deterministic**: Yes (pure math, no RNG)  
**Universal**: Works with any elements, lattices, supercells  
**Native Integration**: Direct CLI → renderer pipeline  

**Quick Command**:
```bash
vsepr NaCl@crystal emit --preset nacl_atomistic --supercell 2,2,2 --viz
```

Enjoy seamless crystal visualization! 🎨🔬
