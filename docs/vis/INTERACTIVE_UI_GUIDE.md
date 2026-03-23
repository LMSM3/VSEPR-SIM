# Interactive Molecular Visualization Guide

## Overview

The interactive visualization system provides a modern Windows 11 style UI with mouse-driven molecular exploration. Hover over atoms and bonds to see detailed chemical information in rich, formatted tooltips.

---

## Features

### 🎨 Windows 11 Light Theme

Modern, clean UI matching Windows 11 design guidelines:
- **Rounded corners** (8px windows, 4px controls)
- **Blue accent colors** (RGB: 0, 122, 204)
- **Light gray backgrounds** (98% white)
- **Subtle shadows** for depth
- **High contrast** for readability

### 🖱️ Mouse Picking

Click or hover over molecular components to select them:
- **Ray-casting** from screen to world space
- **Sphere intersection** for atoms
- **Cylinder intersection** for bonds
- **Automatic detection** of closest object

### 💡 Rich Tooltips

#### Atom Tooltips (Lots of Information!)

When you hover over an atom, you see:

```
Carbon (C)
━━━━━━━━━━━━
Properties:
  Atomic Number:      6
  Atomic Mass:        12.01 u
  Electronegativity:  2.55 (Pauling)

Geometry:
  Position:           (1.23, -0.45, 0.78) Å
  vdW Radius:         1.70 Å
  Covalent Radius:    0.76 Å

Bonding:
  Coordination:       3 neighbors
  Bonded to:
    • H (#1) at 1.09 Å
    • C (#2) at 1.54 Å
    • O (#3) at 1.43 Å
```

**Data included:**
- Element name and symbol
- Atomic number and mass
- Electronegativity (Pauling scale)
- 3D position coordinates
- van der Waals radius
- Covalent radius
- Coordination number
- List of bonded atoms with distances

#### Bond Tooltips (Just One Number!)

When you hover over a bond, you see **only the essential information**:

```
C—O Bond
━━━━━━━━━━━━
Bond Length:    1.430 Å

Atoms: #2 ↔ #3
```

**Data included:**
- Bond type (element symbols)
- **Bond length in Ångströms** (the key number!)
- Atom indices

---

## Usage

### Running the Interactive Viewer

```bash
# Basic usage
interactive-viewer molecule.xyz

# With options
interactive-viewer water.xyz --quality high
```

### Keyboard Controls

| Key | Action |
|-----|--------|
| **Mouse Hover** | Show atom/bond tooltip |
| **SPACE** | Play/pause animation |
| **1-6** | Change animation type |
| **Q** | Decrease quality |
| **W** | Increase quality |
| **T** | Toggle tooltips on/off |
| **F** | Toggle depth cueing (fog) |
| **G** | Toggle glow effect |
| **P** | Toggle PBC visualization |
| **ESC** | Quit |

### Animation Types

1. **None** - Static molecule
2. **Rotate Y** - Spin around vertical axis
3. **Rotate XYZ** - Tumble in 3D space
4. **Oscillate** - Gentle breathing motion
5. **Zoom Pulse** - Scale pulsing effect
6. **Orbit Camera** - Camera circles molecule

---

## Implementation

### Quick Start (C++ API)

```cpp
#include "vis/ui_theme.hpp"
#include "vis/picking.hpp"
#include "vis/analysis_panel.hpp"

// 1. Setup Windows 11 theme
Windows11Theme theme;
theme.apply();

// 2. Create mouse picker
MoleculePicker picker;

// 3. Create analysis panel
AnalysisPanel panel;

// 4. In your render loop:
void update(AtomicGeometry& geom, float mouse_x, float mouse_y) {
    // Update picking each frame
    panel.update(geom, mouse_x, mouse_y, 
                 window_width, window_height,
                 view_matrix, proj_matrix);
    
    // Render tooltips
    panel.render();
}
```

### Advanced Usage

#### Custom Tooltip Styling

```cpp
// Disable tooltips temporarily
panel.set_tooltip_enabled(false);

// Adjust atom/bond scale for picking
panel.set_atom_scale(1.2f);  // 20% larger hit detection
panel.set_bond_scale(1.5f);  // 50% larger hit detection
```

#### Manual Picking

```cpp
MoleculePicker picker;

// Pick atom at mouse position
auto atom = picker.pick_atom(geometry, mouse_x, mouse_y, 
                             width, height, view, proj);

if (atom.has_value()) {
    std::cout << "Clicked atom #" << atom->index 
              << " (Z=" << atom->atomic_number << ")\n";
    std::cout << "Distance: " << atom->distance << " units\n";
}

// Pick bond at mouse position
auto bond = picker.pick_bond(geometry, mouse_x, mouse_y,
                             width, height, view, proj);

if (bond.has_value()) {
    std::cout << "Clicked bond: " << bond->atom_i 
              << " - " << bond->atom_j << "\n";
    std::cout << "Length: " << bond->length << " Å\n";
}
```

#### Custom Element Data

The system includes complete data for all 118 elements:

```cpp
// Get element properties
std::string name = panel.get_element_name(6);  // "Carbon"
std::string symbol = panel.get_element_symbol(6);  // "C"
double mass = panel.get_atomic_mass(6);  // 12.01 u
double en = panel.get_electronegativity(6);  // 2.55
```

---

## Architecture

### Components

```
Interactive UI System
│
├── Windows11Theme
│   ├── Color palette (50+ ImGui colors)
│   ├── Rounded corners (8px windows, 4px frames)
│   └── Utility functions (tooltips, headers, separators)
│
├── MoleculePicker
│   ├── Ray computation (screen → world)
│   ├── Ray-sphere intersection (atoms)
│   ├── Ray-cylinder intersection (bonds)
│   └── Closest object detection
│
└── AnalysisPanel
    ├── Element database (118 elements)
    ├── Atom tooltip rendering
    ├── Bond tooltip rendering
    └── Neighbor detection
```

### Data Flow

```
Mouse Move Event
      ↓
Compute Picking Ray (NDC → World)
      ↓
Test Intersections (Atoms + Bonds)
      ↓
Find Closest Object
      ↓
Retrieve Element Data
      ↓
Render Tooltip (ImGui)
```

---

## Element Database

### Complete Periodic Table

The system includes data for all 118 elements:

| Property | Coverage | Source |
|----------|----------|--------|
| **Element Names** | H → Og | IUPAC standard names |
| **Symbols** | H → Og | 1-2 letter codes |
| **Atomic Masses** | 1.008 → 294 u | Standard atomic weights |
| **Electronegativity** | Pauling scale | Chemical reference |

### Example Data

```cpp
// Element #6 (Carbon)
Name:             "Carbon"
Symbol:           "C"
Atomic Mass:      12.01 u
Electronegativity: 2.55 (Pauling)

// Element #118 (Oganesson)
Name:             "Oganesson"
Symbol:           "Og"
Atomic Mass:      294 u
Electronegativity: 0.0 (unknown)
```

---

## Styling Guidelines

### Windows 11 Theme Colors

```cpp
// Primary colors
Accent:     RGB(  0, 122, 204)  // Windows blue
Success:    RGB( 16, 124,  16)  // Green
Warning:    RGB(252, 111,   0)  // Orange
Error:      RGB(196,  43,  28)  // Red

// Backgrounds
Window:     RGB(250, 250, 250)  // Almost white
Popup:      RGB(255, 255, 255)  // Pure white
Header:     RGB(243, 243, 243)  // Light gray

// Text
Primary:    RGB( 32,  32,  32)  // Almost black
Secondary:  RGB( 96,  96,  96)  // Medium gray
Disabled:   RGB(160, 160, 160)  // Light gray
```

### Typography

```cpp
// Section headers (bold, blue)
theme.section_header("Properties");

// Color-coded values
ImGui::TextColored(theme.get_accent_color(), "Carbon (C)");
ImGui::TextColored(theme.get_success_color(), "✓ Complete");
ImGui::TextColored(theme.get_warning_color(), "⚠ Warning");
ImGui::TextColored(theme.get_error_color(), "✗ Error");
```

### Layout

```cpp
// Styled window with rounded corners
theme.begin_styled_window("Molecule Info");
{
    theme.section_header("Properties");
    ImGui::Text("Atoms: %d", n_atoms);
    
    theme.separator();  // Subtle divider
    
    theme.section_header("Rendering");
    ImGui::Text("FPS: %.1f", fps);
}
ImGui::End();

// Custom tooltip
if (ImGui::IsItemHovered()) {
    theme.tooltip("Hover over atoms to see details");
}
```

---

## Performance

### Optimization Tips

1. **Limit picking frequency**: Only update on mouse move
2. **Spatial culling**: Skip off-screen atoms/bonds
3. **LOD for tooltips**: Simplify data for large molecules
4. **Batch rendering**: Render all tooltips in one ImGui frame

### Typical Performance

| Molecule Size | Picking Cost | Tooltip Render |
|---------------|--------------|----------------|
| 10 atoms | <0.01 ms | <0.1 ms |
| 100 atoms | <0.1 ms | <0.2 ms |
| 1,000 atoms | <1 ms | <0.5 ms |
| 10,000 atoms | <10 ms | <1 ms |

**Note**: Use spatial acceleration (octrees, BVH) for >1,000 atoms.

---

## Examples

### Water Molecule (H₂O)

```
Hovering over oxygen atom:

Oxygen (O)
━━━━━━━━━━━━
Properties:
  Atomic Number:      8
  Atomic Mass:        16.00 u
  Electronegativity:  3.44 (Pauling)

Geometry:
  Position:           (0.00, 0.00, 0.12) Å
  vdW Radius:         1.52 Å
  Covalent Radius:    0.66 Å

Bonding:
  Coordination:       2 neighbors
  Bonded to:
    • H (#1) at 0.96 Å
    • H (#2) at 0.96 Å
```

```
Hovering over O-H bond:

O—H Bond
━━━━━━━━━━━━
Bond Length:    0.960 Å

Atoms: #0 ↔ #1
```

### Benzene (C₆H₆)

```
Hovering over carbon atom in ring:

Carbon (C)
━━━━━━━━━━━━
Properties:
  Atomic Number:      6
  Atomic Mass:        12.01 u
  Electronegativity:  2.55 (Pauling)

Geometry:
  Position:           (1.20, 0.69, 0.00) Å
  vdW Radius:         1.70 Å
  Covalent Radius:    0.76 Å

Bonding:
  Coordination:       3 neighbors
  Bonded to:
    • C (#1) at 1.40 Å  ← Aromatic C-C
    • C (#5) at 1.40 Å  ← Aromatic C-C
    • H (#6) at 1.08 Å  ← C-H bond
```

---

## Troubleshooting

### Tooltips Not Showing

**Problem**: Mouse hover doesn't display tooltips

**Solutions**:
1. Check if tooltips enabled: `panel.set_tooltip_enabled(true)`
2. Verify mouse position: `std::cout << mouse_x << ", " << mouse_y`
3. Check view/projection matrices are valid (not identity/zero)
4. Ensure ImGui frame is being rendered: `ImGui_ImplOpenGL3_RenderDrawData()`

### Incorrect Picking

**Problem**: Wrong atoms/bonds highlighted

**Solutions**:
1. Verify camera matrices are correct
2. Check coordinate transformations (Y-axis flipped?)
3. Adjust atom/bond scale: `panel.set_atom_scale(1.5f)`
4. Validate ray computation with debug output

### Performance Issues

**Problem**: Slow picking with large molecules

**Solutions**:
1. Use spatial acceleration (octree, grid)
2. Limit picking to visible atoms (frustum culling)
3. Reduce tooltip update frequency (every N frames)
4. Simplify tooltip content for large molecules

---

## Future Enhancements

### Planned Features

- [ ] **Selection highlighting** - Highlight picked atoms/bonds
- [ ] **Multi-selection** - Shift+click to select multiple atoms
- [ ] **Measurement tools** - Distance, angle, dihedral measurement
- [ ] **Bond order display** - Show single/double/triple bonds
- [ ] **Partial charges** - Display computed charges (QEq)
- [ ] **Hydrogen bonds** - Visualize H-bond networks
- [ ] **Click actions** - Edit atoms, delete bonds, add hydrogens

### Experimental Features

- **AR/VR support** - Windows Mixed Reality integration
- **Touch input** - Multi-touch gestures for tablet/Surface
- **Voice commands** - "Show carbon atoms", "Measure distance"
- **AI suggestions** - "This looks like a peptide bond"

---

## References

### Windows 11 Design Guidelines

- [Windows UI Design Principles](https://learn.microsoft.com/en-us/windows/apps/design/)
- [Fluent Design System](https://www.microsoft.com/design/fluent)
- [WinUI 3 Gallery](https://github.com/microsoft/WinUI-Gallery)

### Chemical Data Sources

- **IUPAC**: Element names and symbols
- **Pauling**: Electronegativity values
- **CRC Handbook**: Atomic masses, radii
- **Jmol**: CPK color scheme

### Related Documentation

- `RENDERER_FEATURES.md` - Complete rendering guide
- `IMPLEMENTATION_COMPLETE.md` - System overview
- `QUICK_REFERENCE.md` - Cheat sheet
- `BALLSTICK_GUIDE.md` - Renderer tutorial

---

## License

Part of the VSEPR molecular simulation framework.
See main repository for license details.

---

**Last Updated**: 2024
**Version**: 2.0
**Compatibility**: Windows 11, OpenGL 3.3+
