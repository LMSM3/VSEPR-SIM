# VSEPR-Sim: Physics-First Molecular Simulation Engine

## 🚀 Quick Start - Universal Scripts

### One-Command Build & Test (Cross-Platform)

**Linux/WSL:**
```bash
./build_universal.sh           # Build everything
./test_thermal.sh              # Test thermal properties system
./debug.sh info                # System diagnostics
```

**Windows:**
```batch
build_universal.bat            # Build everything (auto-detects WSL)
test_thermal.bat               # Test thermal system
debug.bat info                 # System diagnostics
```

**📖 See [UNIVERSAL_SCRIPTS.md](UNIVERSAL_SCRIPTS.md) for complete documentation**  
**📝 See [QUICK_REFERENCE_UNIVERSAL.txt](QUICK_REFERENCE_UNIVERSAL.txt) for command reference**

---

## Quick Start (Original Scripts)

### 🎯 Grab-Their-Attention Quick Start

**WSL/Linux/macOS (30 seconds):**
```bash
./build.sh --clean && source activate.sh
vsepr build random --watch        # ← Interactive 3D visualization!
vsepr build discover --thermal    # ← 100 molecules + HGST + thermal analysis
```

**Windows (PowerShell):**
```powershell
.\build.ps1 -Clean
.\vsepr.bat build random --watch
```

### One-Command Build

**Linux / macOS / WSL:**
```bash
./build.sh --install --clean
source activate.sh               # Adds 'vsepr' command to PATH
vsepr build H2O --optimize --viz
```

**Windows (PowerShell or Command Prompt):**
```powershell
# PowerShell
.\build.ps1 -Install -Clean

# Or Command Prompt
build.bat --clean --viz
```

**Run the application:**
```bash
# After activation (WSL/Linux):
vsepr build H2O --optimize --viz

# Or use full path:
./build/bin/vsepr build H2O --optimize --viz

# Windows:
.\vsepr.bat build H2O --optimize --viz
```

**See [BUILD_INSTRUCTIONS.md](BUILD_INSTRUCTIONS.md) for detailed build documentation.**

---

## Overview

A molecular simulation engine built on **explicit classical mechanics** rather than machine-learned geometry. The system models chemical structure through validated physics—bond stretching, angle bending, torsion, and nonbonded interactions—with machine learning introduced only to refine force-field parameters, never to replace fundamental physics.

### Guiding Principle
**Physics enforces reality. ML fills in what we don't know yet.**

---

## Project Vision

### Core Goals
- Model molecular geometry from **first principles**: electron distribution, bonding topology, repulsion energetics
- Allow shape to **emerge naturally** from physics rather than hard-coded VSEPR rules
- Provide an **interactive 3D sandbox** for exploring molecular behavior in real time

### Development Roadmap

**v0.1–v0.3**: Foundation & Validation
- Implement classical energy terms (bond, angle, torsion, nonbonded)
- Analytic gradient computation
- Geometry optimization
- Validate against known structures (H₂O bend, CH₄ tetrahedral, C₂H₆ torsion)

**v0.4+**: ML Parameter Refinement
- Curated training data (organics, metals, salts, MOFs)
- Learn parameter trends while preserving physical stability
- Maintain full interpretability

**Future**: 3D Interactive Environment
- Real-time molecular relaxation
- Visual inspection of forces, strain, torsional barriers
- Dynamic property prediction
- Chemically literate exploration tools

---

## Technical Architecture

### Core Data Types
```cpp
Atom      { id, Z, mass, flags }
Bond      { i, j, order }
Angle     { i, j, k }
Torsion   { i, j, k, l }
Improper  { i, j, k, l }
Cell      { lattice_vectors, periodicity }  // For crystals, MOFs
```

### Energy Components
1. **Bond stretching**: Harmonic or Morse potential
2. **Angle bending**: Harmonic angular deviation
3. **Torsional rotation**: Fourier series barriers
4. **Van der Waals**: Lennard-Jones 12-6 with element-specific parameters
5. **Electrostatics**: Coulomb with screening/cutoffs

### Quantum Module (NEW - v1.0)
Electronic excitation and spectroscopy capabilities:
- **UV-Vis Absorption**: π→π*, n→π* transitions
- **Fluorescence**: Emission spectra with Stokes shift
- **Simple Hückel Theory**: Conjugated π-systems
- **HTML Export**: Interactive 3D + spectrum visualization
- **Integration**: Seamless tie-in to OpenGL and HTML outputs

**See:** [QunatumModel/README.md](QunatumModel/README.md) | [QUANTUM_INTEGRATION.md](docs/QUANTUM_INTEGRATION.md)

```cpp
// Example: Benzene spectroscopy
#include "QunatumModel/qm_output_bridge.hpp"
auto qm_data = quantum::QuantumCLI::analyze_molecule(benzene);
quantum::QuantumWebExport::export_with_spectrum(benzene, qm_data, "output.html");
```

### Optimization
- Gradient descent with line search
- Conjugate gradient / L-BFGS for efficiency
- Constrained optimization (frozen atoms, distance constraints)

---

## Building

```bash
mkdir build && cd build
cmake ..
make
```

---

## Current Status

**Version**: 0.1.0-dev  
**Stage**: Initial structure definition

### Completed
- [x] Project structure
- [x] Core type definitions
- [x] Molecule container class
- [x] Build system (CMake)

### In Progress
- [ ] Periodic table data
- [ ] Energy term implementations
- [ ] Gradient computation
- [ ] Geometry optimizer

### Planned
- [ ] Validation suite
- [ ] Force field parameter library
- [ ] 3D visualization
- [ ] ML parameter training pipeline

---

## Dependencies

### Current
- C++17 compiler
- CMake 3.15+

### Planned
- Eigen or similar (linear algebra)
- Python 3.8+ (data processing, web scraping)
- Visualization library (TBD: OpenGL, Three.js, etc.)

---

## License

TBD

---

## Contact

Project started: January 2026
