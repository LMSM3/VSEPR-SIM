# VSEPR-Sim Visualization Architecture (CORRECTED)

## Complete HTML Visualization Workflow - Authoritative Version

### **Corrected Full Pipeline:**

```
┌─────────────────────────────────────────────────────────────────┐
│                    ENTRY POINTS                                  │
├─────────────────────────────────────────────────────────────────┤
│  1. batch/bash scripts (batch_builder.py, view_molecule.sh)    │
│  2. Manual Python call (viewer_generator.py)                    │
│  3. C++ CLI (cmd_viz.cpp, cmd_build.cpp --visualize)           │
└──────────────────┬──────────────────────────────────────────────┘
                   │
                   ▼
┌─────────────────────────────────────────────────────────────────┐
│                   BATCH AUTOMATOR                                │
│  • batch/batch_builder.py                                        │
│  • Parses XML/TXT build lists (batch/build_list.xml)           │
│  • Orchestrates C++ calls                                        │
│  • Aggregates results                                            │
│  • Generates summary reports (HTML, CSV, Jupyter notebooks)     │
└──────────────────┬──────────────────────────────────────────────┘
                   │
                   ▼
┌═════════════════════════════════════════════════════════════════┐
║           C++ SUBSYSTEM (AUTHORITATIVE)                         ║
╠═════════════════════════════════════════════════════════════════╣
║  • Builds atoms + coordinates from formula                      ║
║  • Generates bonds/angles/torsions (topology)                   ║
║  • Optimizes geometry (FIREOptimizer - deterministic)           ║
║  • Computes energy, forces, charges                             ║
║  • Applies physics constraints                                  ║
║                                                                  ║
║  OUTPUT: Molecule object (complete truth state)                 ║
╚══════════════════┬══════════════════════════════════════════════╝
                   │
                   ▼
┌─────────────────────────────────────────────────────────────────┐
│              EXPORT CONTRACT (Truth State)                       │
├─────────────────────────────────────────────────────────────────┤
│  PRIMARY FORMATS:                                                │
│  • .xyz          → Geometry only (element, x, y, z)             │
│  • .xyzA         → Extended (+ velocities, forces, charges)     │
│  • .xyzC         → Thermal pathways (binary, efficient)         │
│                                                                  │
│  METADATA FORMATS (Needed):                                      │
│  • webgl_molecules.json → Full topology for web viewers         │
│  • truth_state.json     → Complete state export:                │
│      - atoms (Z, position, flags)                               │
│      - bonds (explicit, with order)                             │
│      - angles, torsions                                          │
│      - constraints                                               │
│      - charge (as metadata, NOT formula hack)                   │
│      - source attribution (C++ generated vs inferred)           │
│                                                                  │
│  LIMITATION: .xyz cannot carry:                                  │
│    ✗ Bond order                                                  │
│    ✗ Charge state                                                │
│    ✗ Intended topology                                           │
│    ✗ Constraints                                                 │
│    ✗ Source-of-truth flags                                       │
└──────────────────┬──────────────────────────────────────────────┘
                   │
                   ▼
┌─────────────────────────────────────────────────────────────────┐
│           C++ VALIDATION / QA GATE                               │
│           (Deterministic, Reproducible)                          │
├─────────────────────────────────────────────────────────────────┤
│  GEOMETRIC SANITY CHECKS:                                        │
│  • Minimum distance checks (no atom overlap)                    │
│  • Maximum distance checks (no unrealistic gaps)                │
│  • Bond length validation (vs covalent radii tables)            │
│  • Bond angle reasonability                                      │
│  • Collision/clash detection                                     │
│                                                                  │
│  CHEMISTRY CHECKS:                                               │
│  • Expected VSEPR geometry validation                           │
│  • Coordination number verification                             │
│  • Valence consistency                                           │
│  • Charge balance (if metadata present)                         │
│                                                                  │
│  OUTPUT:                                                         │
│  • Pass/Fail flag                                                │
│  • Warning list (too close, too far, weird angles)              │
│  • Quality score (0-100, HGST-style if desired)                 │
│  • Annotated output for debugging                               │
└──────────────────┬──────────────────────────────────────────────┘
                   │
           ┌───────┴────────┐
           ▼                ▼
┌──────────────────┐  ┌─────────────────────────────────────┐
│  PYTHON LAYER    │  │  C++ NATIVE VIEWERS                 │
│  (Display Only)  │  │  (Authoritative Rendering)          │
└──────┬───────────┘  └─────────┬───────────────────────────┘
       │                        │
       ▼                        ▼
┌──────────────────────────────────────────────────────────┐
│  VIEWER + HEURISTIC BOND INFERENCE (for display)         │
│  ⚠ NON-AUTHORITATIVE - Rendering Convenience Only        │
├──────────────────────────────────────────────────────────┤
│  • scripts/viewer_generator.py (MolecularViewer class)   │
│  • Parses .xyz files (geometry only)                     │
│  • INFERS bonds using radius threshold (~1.6× covalent)  │
│     → NOT chemistry truth                                │
│     → Visual approximation for ball-and-stick rendering  │
│  • Applies CPK color scheme (element → color mapping)    │
│  • Generates Three.js-based HTML                         │
│                                                           │
│  NOTE: If .xyzA or truth_state.json exists, should       │
│        use explicit bonds instead of inference           │
└──────────────────┬───────────────────────────────────────┘
                   │
                   ▼
┌──────────────────────────────────────────────────────────┐
│              HTML VIEWER OUTPUT                           │
│  (Three.js WebGL - Interactive Browser-Based)            │
├──────────────────────────────────────────────────────────┤
│  • Generated: examples/viewers/*.html                     │
│  • Generated: outputs/*.html (universal_viewer.html)     │
│  • Technology: Three.js r128 + OrbitControls             │
│  • Render modes: ball-stick, spheres, sticks             │
│  • Interactive: rotate, zoom, pan                        │
│  • Features: FPS counter, keyboard shortcuts             │
└──────────────────────────────────────────────────────────┘

           ┌─────────────────────────────────────┐
           │  OPENGL SUBSYSTEM                   │
           │  (Native, Authoritative Rendering)  │
           ├─────────────────────────────────────┤
           │  • apps/vsepr-view (GLFW/ImGui)     │
           │  • apps/gl_molecule_viewer.cpp      │
           │  • src/vis/window.hpp               │
           │  • Uses C++ truth data directly     │
           │  • Real-time rendering              │
           │  • Command router integration       │
           │  • Can display:                     │
           │    - Explicit bonds (from Molecule) │
           │    - Forces, velocities             │
           │    - Energy surfaces                │
           │    - Optimization trajectories      │
           └─────────────────────────────────────┘

           ┌─────────────────────────────────────┐
           │  LOCAL MINIMAL HTML GENERATOR       │
           │  (Planned - C++ Embedded)           │
           ├─────────────────────────────────────┤
           │  • Direct C++ → HTML generation     │
           │  • No Python dependency             │
           │  • Embeds truth_state.json in HTML  │
           │  • Uses authoritative bonds         │
           │  • Template-based generation        │
           │  • Can run in headless environments │
           └─────────────────────────────────────┘

╔═════════════════════════════════════════════════════════════════╗
║              AI ASSIST (Optional, Offline)                      ║
║              ⚠ NOT IN RUNTIME PATH                              ║
╠═════════════════════════════════════════════════════════════════╣
║  ADVISORY FUNCTIONS:                                            ║
║  • Formula validation suggestions                               ║
║    Example: "Zn83+ is invalid, did you mean Zn with            ║
║             charge=+3 as metadata?"                             ║
║                                                                 ║
║  • Output sanity flagging                                       ║
║    Example: "Zn2 created 1 atom, did you want diatomic mode?"  ║
║                                                                 ║
║  • Parameter recommendations                                    ║
║    Example: "Try max_steps=5000 for better convergence"        ║
║                                                                 ║
║  • Report generation assistance                                 ║
║    - Auto-generate Jupyter notebooks                            ║
║    - Suggest visualizations                                     ║
║    - Create comparison tables                                   ║
║                                                                 ║
║  • Debugging helpers                                            ║
║    - Explain unusual geometries                                 ║
║    - Suggest constraint tweaks                                  ║
║    - Identify common errors                                     ║
║                                                                 ║
║  INTEGRATION: Post-processing only, never blocks builds         ║
╚═════════════════════════════════════════════════════════════════╝
```

---

## Critical Distinctions

### **1. Authority Hierarchy**

```
C++ SUBSYSTEM (Truth)
    ↓
    ├─ Builds geometry
    ├─ Generates explicit bonds
    ├─ Optimizes with physics
    └─ Exports authoritative state

QA GATE (Validation)
    ↓
    Checks against physical tables

VIEWERS (Display)
    ↓
    ├─ OpenGL: Uses C++ data directly
    ├─ Python: Infers bonds for rendering
    └─ HTML: Visual approximation
```

### **2. Bond Sources**

| Source | Type | Usage |
|--------|------|-------|
| **C++ Molecule::bonds** | Truth | Topology, energy, optimization |
| **Python detect_bonds()** | Heuristic | Display only, non-authoritative |
| **OpenGL direct render** | Truth | Uses Molecule::bonds |

### **3. File Format Capabilities**

| Format | Geometry | Bonds | Charge | Metadata | Use Case |
|--------|----------|-------|--------|----------|----------|
| `.xyz` | ✓ | ✗ | ✗ | ✗ | Simple geometry exchange |
| `.xyzA` | ✓ | ✗ | ✓ | Limited | Extended properties |
| `.xyzC` | ✓ | ✗ | ✗ | Thermal | MD trajectories |
| `truth_state.json` (needed) | ✓ | ✓ | ✓ | ✓ | Full state preservation |
| `webgl_molecules.json` (needed) | ✓ | ✓ | ✓ | ✓ | Web viewer with truth |

---

## Data Flow Example (Corrected)

```
User: "python batch/batch_builder.py build_list.xml"
  ↓
[BATCH AUTOMATOR]
Parses XML, finds "H2O"
  ↓
[C++ SUBSYSTEM - AUTHORITATIVE]
./build/bin/vsepr build H2O --optimize --output water.xyz
  • Builds: 1 O, 2 H atoms
  • Generates bonds: [0-1], [0-2] (explicit)
  • Optimizes geometry (FIRE algorithm)
  • Writes: water.xyz (geometry only)
  ↓
[EXPORT CONTRACT]
Created: water.xyz (3 atoms, no bond info)
Missing: water_truth.json (would contain explicit bonds)
  ↓
[C++ QA GATE]
Validates:
  • Bond lengths: O-H = 0.96 Å ✓
  • Angle: H-O-H = 104.5° ✓
  • No clashes ✓
  • Score: 98/100
  ↓
[PYTHON VIEWER - NON-AUTHORITATIVE]
python scripts/viewer_generator.py water.xyz
  • Parses 3 atoms from XYZ
  • INFERS bonds using 1.6× covalent radii
  • Finds 2 bonds (happens to match C++ in this case)
  • Generates water_viewer.html
  ↓
[HTML OUTPUT]
water_viewer.html
  • Three.js renders inferred structure
  • ⚠ Bonds are visual approximation, not chemistry truth
  • Works well for simple molecules
  • Can fail for complex/unusual structures
```

---

## Current vs. Needed Improvements

### **Current State:**
- ✓ C++ builds molecules correctly
- ✓ XYZ export works
- ✓ Python viewer generates nice HTML
- ✗ Bond information lost in .xyz export
- ✗ Viewer guesses structure (sometimes wrong)
- ✗ No validation gate
- ✗ No metadata preservation

### **Needed Additions:**

#### **1. Enhanced Export Formats**
```cpp
// In C++, add:
void Molecule::export_truth_state(const std::string& filename) {
    json j;
    j["atoms"] = /* Z, position, flags */;
    j["bonds"] = /* i, j, order, source="cpp" */;
    j["angles"] = /* angle list */;
    j["charge_metadata"] = /* NOT in formula */;
    // Write j to filename
}
```

#### **2. QA Gate Implementation**
```cpp
struct ValidationResult {
    bool passed;
    std::vector<std::string> warnings;
    double quality_score;
    std::map<std::string, bool> checks;
};

ValidationResult validate_molecule(const Molecule& mol);
```

#### **3. Python Viewer Enhancement**
```python
# In viewer_generator.py, add:
def load_truth_state(xyz_file):
    truth_file = xyz_file.replace('.xyz', '_truth.json')
    if exists(truth_file):
        # Use explicit bonds from truth
        return load_json(truth_file)
    else:
        # Fall back to inference (with warning)
        return detect_bonds_heuristic()
```

#### **4. Local C++ HTML Generator**
```cpp
// Planned feature:
class HTMLGenerator {
    std::string generate(const Molecule& mol, 
                        const std::string& template_path);
    // Embeds truth state directly in HTML
};
```

---

## Integration Points

### **Where Each Component Lives:**

```
C:\Users\Liam\Desktop\vsepr-sim\
├── src/
│   ├── sim/molecule.cpp         [C++ AUTHORITATIVE]
│   ├── sim/optimizer.cpp        [C++ AUTHORITATIVE]
│   ├── io/xyz_format.cpp        [EXPORT CONTRACT]
│   ├── cli/cmd_build.cpp        [ENTRY POINT]
│   └── vis/window.cpp           [OPENGL SUBSYSTEM]
│
├── scripts/
│   └── viewer_generator.py      [PYTHON VIEWER - NON-AUTH]
│
├── batch/
│   └── batch_builder.py         [BATCH AUTOMATOR]
│
├── apps/
│   ├── vsepr-view/main.cpp      [OPENGL SUBSYSTEM]
│   └── gl_molecule_viewer.cpp   [OPENGL SUBSYSTEM]
│
└── (needed)/
    ├── src/qa/validator.cpp     [QA GATE - MISSING]
    ├── src/export/truth_state.cpp [EXPORT ENHANCEMENT - MISSING]
    └── src/web/html_gen.cpp     [LOCAL HTML GEN - MISSING]
```

---

## Recommendations

### **Immediate:**
1. Add warning to `viewer_generator.py`: "Bonds inferred for display only"
2. Document XYZ limitations in README
3. Create `truth_state.json` format specification

### **Short-term:**
1. Implement C++ QA gate
2. Add `.xyzB` format (XYZ + Bonds)
3. Update Python viewer to use explicit bonds if available

### **Long-term:**
1. Implement `truth_state.json` export
2. Create C++ HTML generator (no Python dependency)
3. Add AI assist as optional post-processor
4. Integrate validation into CI/CD pipeline

---

## Testing the Pipeline

### **Verify Authority:**
```bash
# Build with C++
./vsepr build "Zn2" --output zn2.xyz

# Check what Python infers
python scripts/viewer_generator.py zn2.xyz

# Compare bond counts
# C++: How many bonds? (check molecule.bonds.size())
# Python: How many bonds? (viewer shows in HTML)
# If different → proves inference is wrong
```

### **Test QA Gate:**
```bash
# Create molecule with intentional error
./vsepr build "H100" --no-optimize --output bad.xyz

# QA gate should flag:
# - Too many H atoms for anything reasonable
# - Weird geometry
# - Low quality score
```

---

**Last Updated:** January 18, 2026  
**Status:** Architecture corrected, implementation partially complete  
**Next Steps:** Implement QA gate, add truth_state.json format
