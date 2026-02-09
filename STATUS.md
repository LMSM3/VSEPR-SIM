# VSEPR-SIM — System Status

## ✅ BUILD SUCCESS — 61/66 TARGETS (92.4%)

**Last build:** BUILD_VIS=OFF, BUILD_APPS=ON, BUILD_TESTS=ON

### ✅ Working (61 targets)

| Category | Count | Status |
|----------|-------|--------|
| Libraries | 15 | ✅ All pass |
| CLI Apps | 9 | ✅ All pass (including **compute_forces** NEW) |
| Validation | 3 | ✅ All pass |
| Tests | 34 | ✅ All pass |

**New since last build:**
- ✅ `compute_forces` — Force field computation CLI (stub)
- ✅ `card-viewer` — Already in CMake (needs BUILD_VIS=ON)
- 🔵 `vsepr-gui` — Prototype exists (commented out, needs implementation)

### ❌ Known Failures (5)

| Target | Reason |
|--------|--------|
| test_application_validation | Missing `atomistic/analysis/rdf.hpp` |
| test_safety_rails | `IModel::compute_energy_and_forces` removed |
| test_batch_processing | Needs `BUILD_VIS=ON` |
| test_continuous_generation | Needs `BUILD_VIS=ON` |
| simple_nacl_test | API mismatch |

All failures are **low-priority test executables**.

---

## Python Tools

| Module | Status |
|--------|--------|
| `tools/scoring.py` | ✅ Multi-factor weighting |
| `tools/classification.py` | ✅ 7 molecular classes |
| `tools/uniform_scoring.py` | ✅ Baseline (flat 50.0) |
| `tools/chemical_validator.py` | ✅ PubChem lookup |
| `tools/discovery_pipeline.py` | ✅ Generate→Test→Validate→Catalog |

## Data Collection

| Script | Purpose |
|--------|---------|
| `collect_formation_frequency.py` | N=250 × 10 runs, lightweight |
| `generate_baseline.py` | Full card generation |
| `compare_baselines.py` | Dataset comparison |

## TUI / Control Panel

| File | Purpose |
|------|---------|
| `tui.py` | Terminal TUI — 20 commands, 19 available |
| `vsepr-panel.ps1` | PowerShell launcher |

### TUI Commands (20 total)

```
Build (3):          [1] Build all  [2] Status  [3] CTest
Validation (3):     [4] Problem1  [5] Problem2  [6] QA
CLI Tools (4):      [7] vsepr  [8] meso-sim  [9] discover  [10] build
Python Tools (2):   [11] Scoring  [12] Classification
Data Collection (4):[13] Formation  [14] Baseline  [15] View  [16] Compare
Analysis (1):       [17] Compute forces ← NEW!
Visualization (1):  [18] Card catalog (needs BUILD_VIS=ON)
Utility (2):        [s] Shell  [q] Quit
```

**Availability: 19/20 (95%)**

### Test Results

**compute_forces:**
```bash
$ ./build/compute_forces --input test.xyz --model LJ --output test.xyzF
✓ Stub file created (correct xyzF format)
```

## Sourcable Helpers

```powershell
# PowerShell — anywhere
. C:\Users\Liam\Desktop\vsepr-sim\FormationFrequency.psm1
Collect-Formation -Count 250 -Runs 10
```

```bash
# Bash — anywhere
source ~/vsepr-sim/formation_frequency.sh
collect_formation --count 250 --runs 10
```

---

## Professional GUI Window — ARCHITECTURE COMPLETE

**Approach:** ImGui + OpenGL (cross-platform)  
**Prerequisite:** ✅ TUI backend (tui.py) — DONE  
**Status:** ✅ Specifications complete, ready to implement

### Delivered Specs

| File | Purpose |
|------|---------|
| `include/ui/WindowManager.hpp` | 500 lines, 8 ViewModels, snap/free/fullscreen |
| `include/data/Crystal.hpp` | Crystal object (xyzZ/A/C provenance) |
| **`include/data/Forces.hpp`** | **Force vector storage (xyzF)** |
| **`include/vis/ForceRenderer.hpp`** | **Force arrow visualization** |
| `apps/vsepr-gui/main.cpp` | 400-line ImGui prototype |
| **`apps/compute_forces.cpp`** | **CLI tool for force computation** |
| `XYZ_FORMAT_SPEC.md` | xyzZ → xyzA → xyzC **→ xyzF** file format |
| **`XYZF_SPECIFICATION.md`** | **Complete xyzF specification** |
| `INTEGRATION_ARCHITECTURE.md` | Full system integration plan |
| `GUI_ARCHITECTURE_COMPLETE.md` | Complete specification |
| `QUICKREF_GUI.md` | Quick reference card |

**Implementation plan:** 8 days (parsers → window manager → GUI → viz → **forces** → watch → polish)

See `GUI_ARCHITECTURE_COMPLETE.md` and `XYZF_SPECIFICATION.md` for full details.
