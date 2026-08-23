# WO-VSIM-67O — FEA Bridge Object

**Status:** Clear implementation work order  
**Priority:** After 66N, 66O, 66P, 66Q  
**Target:** v5.1.4.x → v5.1.5  
**Scope:** Bridge layer only — not a full FEA solver  
**Paired with:** WO-VSIM-67N (DEM Bridge)

---

## Dependency Context

| WO | Delivers |
|----|----------|
| 66N | Constructor objects, batching, upper-block references |
| 66O | Organic/peptide scale diagnostics |
| 66P | Universal translation + crystal/PBC reform |
| 66Q | Non-molecular objects + XBIT documentation |

The FEA bridge maps SPH/control-surface pressure and shear fields onto a structural mesh. The field schema from 66N/66P must be **frozen before Phase 2** of this work order. The VTK export path must be committed or dropped from acceptance criteria before implementation begins — it is currently under-specified.

---

## 0. Purpose

Map particle-fluid simulation outputs and control-surface fields into structural load maps suitable for external FEA solvers. Optionally compute a lightweight Von Mises stress proxy for rapid failure screening.

Target applications:

- Pipe wall stress under internal flow pressure
- Pressure vessel and gas chamber wall loading
- Bend and elbow failure prediction
- Fluid-structure interaction load mapping
- Thermal/pressure cycling and fatigue location identification
- Burst-risk screening

The bridge does **not** replace a validated FEA solver. All computed stress estimates are labeled as proxy values.

---

## 1. Input Sources

| Field | Symbol | Source |
|-------|--------|--------|
| Wall pressure | $P_w$ | `system.surface.wall` |
| Wall shear (x, y, z) | $\boldsymbol{\tau}_w$ | `system.surface.wall` |
| Temperature | $T$ | `environment.ambient` |
| Cavitation index | $\alpha_v$ | `system.surface.wall` |

---

## 2. VSIM Syntax

Minimal form:

```vsim
[bridge]
fea.pipe_wall = FEABridge(
    from   = system.surface.wall,
    target = system.geometry.pipe,
    fields = ["pressure", "shear"],
    export = true
)
```

Full form:

```vsim
[bridge]
fea.pipe_wall = FEABridge(
    from          = system.surface.wall,
    target        = system.geometry.pipe,
    material      = "steel",
    fields        = ["pressure", "shear", "temperature", "cavitation"],
    map           = "surface_to_mesh",
    criterion     = "von_mises",
    yield_strength = 250e6,
    fatigue       = true,
    export_format = ["json", "tsv"]
)
```

> **Critical:** `yield_strength` must be a required field whenever `criterion = "von_mises"` is set. Without a yield threshold, the Von Mises proxy produces numbers with no actionable interpretation. If `yield_strength` is absent, emit `[VSIM-W085]` and suppress the stress ratio output column.

> **VTK export:** If `"vtk"` is included in `export_format`, the VTK mesh format must be committed and spec'd before implementation. If not committed, remove VTK from the acceptance criteria and default to `["json", "tsv"]` only.

---

## 3. Object Model

```cpp
struct FEABridgeObject {
    ObjectPath source_surface;
    ObjectPath target_geometry;

    std::string material      = "unknown";
    std::vector<std::string> fields;

    std::string mapping_mode  = "surface_to_mesh";
    std::string criterion     = "von_mises";

    double yield_strength     = 0.0;   // Pa — required when criterion = "von_mises"
    bool   fatigue_enabled    = false;

    bool export_json          = true;
    bool export_tsv           = true;
    bool export_vtk           = false; // Commit format spec before enabling
};
```

---

## 4. Structural Load Mathematics

### 4.1 Surface-to-Mesh Pressure Mapping

Control-surface pressure samples $P_w(\mathbf{x}_s)$ are projected onto mesh nodes $\mathbf{x}_n$ via inverse-distance weighting:

$$P_n = \frac{\sum_s P_w(\mathbf{x}_s) \cdot w(\mathbf{x}_n, \mathbf{x}_s)}{\sum_s w(\mathbf{x}_n, \mathbf{x}_s)}$$

where the weight function is:

$$w(\mathbf{x}_n, \mathbf{x}_s) = \frac{1}{\lvert\mathbf{x}_n - \mathbf{x}_s\rvert^2 + \epsilon}$$

with $\epsilon$ a small regularization constant to prevent division by zero. The same mapping applies to shear components $\tau_{x}, \tau_{y}, \tau_{z}$.

### 4.2 Stress Tensor from Surface Loads

Given mapped wall pressure $P_n$ and shear components $\boldsymbol{\tau}_n = (\tau_x, \tau_y, \tau_z)$ at node $n$, the surface stress state is:

$$\boldsymbol{\sigma} = \begin{pmatrix} \sigma_x & \tau_{xy} & \tau_{xz} \\ \tau_{xy} & \sigma_y & \tau_{yz} \\ \tau_{xz} & \tau_{yz} & \sigma_z \end{pmatrix}$$

For a pipe under internal pressure, the dominant stress components from thin-wall approximation are:

$$\sigma_\theta = \frac{P_n \cdot r}{t}, \qquad \sigma_z = \frac{P_n \cdot r}{2t}$$

where $r$ is the pipe inner radius and $t$ is the wall thickness. These are supplemented by shear contributions $\tau_{xy}$ mapped from $\boldsymbol{\tau}_w$.

### 4.3 Von Mises Stress Criterion

The Von Mises equivalent stress is:

$$\sigma_{vm} = \sqrt{\frac{1}{2}\left[(\sigma_x - \sigma_y)^2 + (\sigma_y - \sigma_z)^2 + (\sigma_z - \sigma_x)^2\right] + 3\left(\tau_{xy}^2 + \tau_{yz}^2 + \tau_{zx}^2\right)}$$

For 2D plane-stress (common at pipe wall surface):

$$\sigma_{vm} = \sqrt{\sigma_x^2 - \sigma_x \sigma_y + \sigma_y^2 + 3\tau_{xy}^2}$$

The bridge computes $\sigma_{vm}$ at each mapped node and reports it as a **proxy estimate** requiring validation in an external FEA solver.

### 4.4 Yield Ratio and Failure Screening

When `yield_strength` $\sigma_Y$ is provided, the bridge computes the yield utilization ratio:

$$\eta_n = \frac{\sigma_{vm,n}}{\sigma_Y}$$

| $\eta_n$ range | Status |
|----------------|--------|
| $\eta_n < 0.8$ | Safe |
| $0.8 \le \eta_n < 1.0$ | Warning — approaching yield |
| $\eta_n \ge 1.0$ | Critical — yield criterion exceeded |

Nodes with $\eta_n \ge 1.0$ are flagged in the manifest and in the TSV output.

### 4.5 Fatigue Screening (Optional)

When `fatigue = true`, the bridge estimates stress amplitude cycles using a simplified Goodman relation. Given mean stress $\sigma_m$ and alternating stress amplitude $\sigma_a$:

$$\frac{\sigma_a}{\sigma_e} + \frac{\sigma_m}{\sigma_u} = 1$$

where $\sigma_e$ is the endurance limit and $\sigma_u$ is the ultimate tensile strength (both drawn from the material record). The fatigue safety factor is:

$$SF_f = \frac{1}{\dfrac{\sigma_a}{\sigma_e} + \dfrac{\sigma_m}{\sigma_u}}$$

$SF_f < 1.0$ indicates predicted fatigue failure. These values are reported per node as `fatigue_sf` in the TSV output and labeled as proxy estimates.

### 4.6 Thermal Stress Contribution

When `temperature` is included in `fields`, a thermal stress contribution is added at each node. For isotropic material with thermal expansion coefficient $\alpha_T$:

$$\sigma_{th} = -E \cdot \alpha_T \cdot (T_n - T_{ref})$$

where $T_{ref}$ is the reference (stress-free) temperature. This is added as a normal stress component before Von Mises computation:

$$\sigma_x \mathrel{+}= \sigma_{th}, \qquad \sigma_y \mathrel{+}= \sigma_{th}, \qquad \sigma_z \mathrel{+}= \sigma_{th}$$

Material parameters $E$, $\alpha_T$, and $T_{ref}$ are drawn from the material record. If the material record is absent, emit `[VSIM-W082]` and skip the thermal contribution.

### 4.7 Cavitation Damage Index

When `cavitation` is included in `fields`, the local cavitation index $\alpha_v$ is appended to each node record. A damage weighting is applied:

$$\sigma_{vm,\text{eff}} = \sigma_{vm} \cdot (1 + k_c \cdot \alpha_v)$$

where $k_c$ is a cavitation damage amplification constant (default: $k_c = 0.15$). This adjusts the yield ratio $\eta_n$ in regions subject to pitting or surface erosion. The unadjusted $\sigma_{vm}$ and adjusted $\sigma_{vm,\text{eff}}$ are both reported.

---

## 5. Output Files and Schema

### 5.1 Required Outputs

| File | Format |
|------|--------|
| `fea_bridge_manifest.json` | JSON |
| `fea_load_map.json` | JSON |
| `fea_surface_loads.tsv` | TSV |
| `fea_material_regions.json` | JSON |

### 5.2 Optional Outputs

| File | Format | Condition |
|------|--------|-----------|
| `fea_load_map.vtk` | VTK | `export_vtk = true` **and** format committed |
| `fea_wall_stress_preview.csv` | CSV | Debug/review use |

### 5.3 Surface Loads TSV Schema

| Column | Type | Description |
|--------|------|-------------|
| `node_id` | string | Mesh node identifier |
| `x`, `y`, `z` | float | Node position [m] |
| `pressure` | float | Mapped $P_n$ [Pa] |
| `shear_x`, `shear_y`, `shear_z` | float | Mapped $\tau$ components [Pa] |
| `temperature` | float | Mapped $T_n$ [K] |
| `sigma_vm` | float | Von Mises proxy $\sigma_{vm}$ [Pa] |
| `sigma_vm_eff` | float | Cavitation-adjusted $\sigma_{vm,\text{eff}}$ [Pa] |
| `yield_ratio` | float | $\eta_n = \sigma_{vm}/\sigma_Y$ |
| `yield_status` | string | `safe` / `warning` / `critical` |
| `fatigue_sf` | float | Fatigue safety factor (if enabled) |
| `cavitation_index` | float | $\alpha_v$ |

Example:

```
node_id   x      y      z      pressure   shear_x   sigma_vm    yield_ratio   yield_status
n001      0.10   0.00   0.02   152000     14.2      181400000   0.73          safe
n002      0.12   0.00   0.02   188000     24.8      263700000   1.05          critical
```

---

## 6. Diagnostics

| Code | Level | Condition |
|------|-------|-----------|
| `VSIM-E080` | ERROR | No valid control surface source assigned |
| `VSIM-E081` | ERROR | No target geometry object assigned |
| `VSIM-W082` | WARN | Material assignment missing; structural estimates incomplete |
| `VSIM-W083` | WARN | `map` mode not specified; defaulted to `surface_to_mesh` |
| `VSIM-W084` | WARN | Von Mises proxy enabled; validate in external FEA solver |
| `VSIM-W085` | WARN | `yield_strength` absent; yield ratio column suppressed |

---

## 7. Directory Skeleton

```
include/vsim/bridge/
  fea_bridge.hpp
  bridge_fields.hpp
  load_map.hpp

src/vsim/bridge/
  fea_bridge.cpp
  bridge_parse.cpp
  bridge_validate.cpp
  fea_export_json.cpp     ← FEA-specific (not shared with DEM exports)
  fea_export_tsv.cpp      ← FEA-specific

tests/
  test_fea_bridge_parse.cpp
  test_bridge_missing_refs.cpp
  test_pipe_sph_dem_fea_bridge.cpp
```

> **Note on file organization:** Export files are split by bridge type (`fea_export_*.cpp`, `dem_export_*.cpp`) rather than by format. Splitting by format (`bridge_export_json.cpp`) causes merge conflicts when both bridges require simultaneous work.

---

## 8. Implementation Phases

| Phase | Description |
|-------|-------------|
| 1 | Parse `FEABridge(...)`, validate object references and source fields, register in manifest |
| 2 | Extract pressure, shear, temperature, cavitation fields from control surface (**requires frozen 66N/66P field schema**) |
| 3 | Map fields to target geometry nodes via inverse-distance weighting |
| 4 | Bind material region data; compute Von Mises proxy, yield ratio, thermal and cavitation adjustments |
| 5 | Emit FEA load JSON, TSV table; optional VTK if format committed |
| 6 | Diagnostics: missing surface, missing geometry, missing material, missing yield strength |
| 7 | Integration test: full pipe SPH → control surface → FEA bridge script |

---

## 9. Closure Condition

The work order closes when the following minimal script parses and emits artifacts without error:

```vsim
[bridge]
fea.pipe_wall = FEABridge(
    from   = system.surface.wall,
    target = system.geometry.pipe
)
```

Minimum required outputs:

- `fea_bridge_manifest.json`
- `fea_surface_loads.tsv`

---

## 10. Acceptance Checklist

### Parsing and Validation
- [ ] `FEABridge(...)` constructor parses without error
- [ ] Validates that `from` references a live control surface object
- [ ] Validates that `target` references a live geometry object
- [ ] Emits `VSIM-E080` when source surface is missing
- [ ] Emits `VSIM-E081` when target geometry is missing
- [ ] Emits `VSIM-W082` when material assignment is absent
- [ ] Emits `VSIM-W083` when `map` mode is not specified
- [ ] Emits `VSIM-W084` when Von Mises proxy is active
- [ ] Emits `VSIM-W085` when `yield_strength` is absent

### Field Extraction (requires frozen 66N/66P schema)
- [ ] Extracts pressure field from control surface
- [ ] Extracts shear (x, y, z) components from control surface
- [ ] Extracts temperature field from ambient when present in `fields`
- [ ] Extracts cavitation index from control surface when present in `fields`

### Mapping and Computation
- [ ] Maps pressure and shear to target geometry nodes via inverse-distance weighting
- [ ] Computes Von Mises stress $\sigma_{vm}$ at each mapped node
- [ ] Computes cavitation-adjusted $\sigma_{vm,\text{eff}}$ when cavitation field is present
- [ ] Computes thermal stress contribution $\sigma_{th}$ when temperature field is present
- [ ] Computes yield utilization ratio $\eta_n = \sigma_{vm} / \sigma_Y$ when `yield_strength` is set
- [ ] Assigns yield status `safe` / `warning` / `critical` per node
- [ ] Computes fatigue safety factor $SF_f$ per node when `fatigue = true`
- [ ] All proxy values labeled as estimates in output metadata

### Export
- [ ] Emits `fea_bridge_manifest.json` with provenance metadata
- [ ] Emits `fea_load_map.json`
- [ ] Emits `fea_surface_loads.tsv` with all required columns
- [ ] Emits `fea_material_regions.json`
- [ ] VTK export gated behind committed format spec — not emitted by default
- [ ] Nodes with $\eta_n \ge 1.0$ flagged explicitly in manifest

### Integration
- [ ] Minimal closure script parses and emits required artifacts
- [ ] Full pipe SPH + FEA bridge script (`pipe_sph_dem_fea_bridge.vsim`) runs end-to-end
- [ ] FEA bridge manifest records upstream control-surface provenance
- [ ] Combined test passes alongside WO-VSIM-67N (DEM bridge)
- [ ] VTK export path decision recorded in work order before Phase 5
