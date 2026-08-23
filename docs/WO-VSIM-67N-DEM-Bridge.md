# WO-VSIM-67N — DEM Bridge Object

**Status:** Clear implementation work order  
**Priority:** After 66N, 66O, 66P, 66Q  
**Target:** v5.1.4.x → v5.1.5  
**Scope:** Bridge layer only — not a DEM solver rewrite  

---

## Dependency Context

| WO | Delivers |
|----|----------|
| 66N | Constructor objects, batching, upper-block references |
| 66O | Organic/peptide scale diagnostics |
| 66P | Universal translation + crystal/PBC reform |
| 66Q | Non-molecular objects + XBIT documentation |

The DEM bridge consumes control-surface outputs produced by the SPH simulation chain. The field schema emitted by 66N/66P must be **frozen before Phase 2** of this work order. Any drift in the control-surface output contract invalidates the extraction layer.

---

## 0. Purpose

Convert SPH / bead-simulation outputs and control-surface fields into boundary condition packages for granular packing simulations. Target systems include:

- Slurry flow and sediment transport
- Powder and catalytic packed beds
- Pebble beds and nuclear particulate transport
- Granular blockage and bridging prediction

The bridge does **not** solve DEM. It produces a DEM-ready boundary condition package.

---

## 1. Input Sources

The DEM bridge consumes the following fields from upstream control surfaces and flow objects:

| Field | Symbol | Source Object |
|-------|--------|---------------|
| Wall pressure | $P_w$ | `system.surface.wall` |
| Wall shear stress | $\tau_w$ | `system.surface.wall` |
| Flow velocity | $\mathbf{u}$ | `system.surface.wall` |
| Particle crossing flux | $\dot{N}$ | `system.source.inlet` |
| Cavitation / void density | $\alpha_v$ | `system.surface.wall` |
| Ambient population state | — | `environment.ambient` |
| Inlet/outlet flux records | $Q_{in},\,Q_{out}$ | `system.source.inlet`, `system.sink.outlet` |

---

## 2. VSIM Syntax

Minimal form:

```vsim
[bridge]
dem.pipe_packing = DEMBridge(
    from     = system.surface.wall,
    geometry = system.geometry.pipe,
    carrier  = environment.ambient,
    mode     = "granular_packing",
    particles = "sediment",
    export   = true
)
```

Full form:

```vsim
[bridge]
dem.pipe_packing = DEMBridge(
    from           = system.surface.wall,
    fields         = ["pressure", "shear", "flow_rate", "cavitation"],
    geometry       = system.geometry.pipe,
    inlet          = system.source.inlet,
    outlet         = system.sink.outlet,
    packing_model  = "hard_sphere",
    contact_model  = "hertz_mindlin",
    friction       = 0.35,
    restitution    = 0.20,
    particle_density = 2500.0,
    export_format  = "dem_manifest"
)
```

> **Note:** `particle_density` must be exposed as a per-species override. A single hardcoded default of 2500 kg/m³ is insufficient for mixed-phase flows. If omitted, emit `[VSIM-W075]` and apply the default.

---

## 3. Object Model

```cpp
struct DEMBridgeObject {
    ObjectPath source_surface;
    ObjectPath geometry;
    ObjectPath inlet;
    ObjectPath outlet;
    ObjectPath ambient;

    std::vector<std::string> fields;

    std::string packing_model  = "hard_sphere";
    std::string contact_model  = "hertz_mindlin";

    double friction          = 0.30;
    double restitution       = 0.20;
    double particle_density  = 2500.0;  // kg/m³ — must support per-species override

    bool export_manifest = true;
    bool export_table    = true;
};
```

---

## 4. Packing and Jamming Mathematics

### 4.1 Random Close Packing Reference

The upper bound for random close packing (RCP) of monodisperse spheres is:

$$\phi_{RCP} \approx 0.6366$$

For polydisperse distributions, this limit shifts. The bridge uses the RCP bound as a **jamming onset threshold**. Computed $\hat{\phi}$ values approaching or exceeding this are flagged.

### 4.2 Local Packing Fraction Proxy

For each surface region $i$, the local packing fraction estimate is derived from the particle flux and flow velocity:

$$\hat{\phi}_i = \frac{\dot{N}_i \cdot V_p}{A_i \cdot \lvert\mathbf{u}_i\rvert}$$

where:

- $\dot{N}_i$ = particle crossing count per unit time in region $i$ [s⁻¹]
- $V_p = \tfrac{4}{3}\pi r_p^3$ = particle volume [m³]
- $A_i$ = region cross-sectional area [m²]
- $\lvert\mathbf{u}_i\rvert$ = local flow speed [m/s]

This is a **proxy estimate**, not a solved DEM quantity. It is labeled as such in all outputs.

### 4.3 Jamming Risk Classification

Define the jamming risk index $J_i$ as:

$$J_i = \frac{\hat{\phi}_i}{\phi_{RCP}}$$

| $J_i$ range | Risk label |
|-------------|------------|
| $J_i < 0.85$ | `low` |
| $0.85 \le J_i < 0.95$ | `moderate` |
| $J_i \ge 0.95$ | `high` |

Emit `[VSIM-W074]` when $J_i \ge 0.95$ in any region.

### 4.4 Bridging Probability

A simplified bridging probability for a constriction of width $W$ and particle diameter $d_p$ follows the relation:

$$P_{bridge} = 1 - \exp\!\left(-\lambda \cdot \frac{d_p}{W}\right)$$

where $\lambda$ is a geometry-dependent empirical constant (default: $\lambda = 3.5$ for cylindrical pipes). The bridge reports $P_{bridge}$ per bend/constriction region.

### 4.5 Hertz–Mindlin Contact Force

When `contact_model = "hertz_mindlin"`, the normal contact force is:

$$F_n = \frac{4}{3} E^* \sqrt{R^*} \,\delta^{3/2}$$

where:

$$\frac{1}{E^*} = \frac{1-\nu_1^2}{E_1} + \frac{1-\nu_2^2}{E_2}, \qquad \frac{1}{R^*} = \frac{1}{R_1} + \frac{1}{R_2}$$

- $\delta$ = normal overlap [m]
- $E_1, E_2$ = Young's moduli [Pa]
- $\nu_1, \nu_2$ = Poisson ratios
- $R_1, R_2$ = particle radii [m]

The tangential (Mindlin) force is bounded by Coulomb friction:

$$F_t \le \mu_s \cdot F_n$$

where $\mu_s$ is the static friction coefficient (mapped from `friction` parameter).

### 4.6 Granular Pressure

The granular pressure in a packed region is estimated as:

$$P_g = \rho_p \cdot \hat{\phi} \cdot \Theta$$

where $\Theta$ is the granular temperature (mean-square velocity fluctuation). For bridge purposes, $\Theta$ is estimated from the local shear field:

$$\Theta \approx \frac{\tau_w^2}{\rho_p^2 \cdot \hat{\phi}^2}$$

---

## 5. Output Files and Schema

### 5.1 Required Outputs

| File | Format |
|------|--------|
| `dem_bridge_manifest.json` | JSON |
| `dem_boundary_conditions.json` | JSON |
| `dem_particle_seed_table.tsv` | TSV |
| `dem_packing_regions.tsv` | TSV |

### 5.2 Packing Regions TSV Schema

| Column | Type | Description |
|--------|------|-------------|
| `region_id` | string | Surface region identifier |
| `x`, `y`, `z` | float | Region centroid [m] |
| `pressure` | float | Local wall pressure $P_w$ [Pa] |
| `shear` | float | Local wall shear $\tau_w$ [Pa] |
| `flow_velocity` | float | $\lvert\mathbf{u}\rvert$ [m/s] |
| `particle_flux` | float | $\dot{N}$ [s⁻¹] |
| `packing_phi` | float | $\hat{\phi}_i$ (proxy) |
| `jamming_risk` | string | `low` / `moderate` / `high` |
| `bridging_prob` | float | $P_{bridge}$ |
| `cavitation_index` | float | $\alpha_v$ |

Example:

```
region_id   x      y      z      pressure   shear   flux   packing_phi   jamming_risk   bridging_prob
wall_001    0.12   0.02   0.00   152000     43.2    900    0.58          low            0.04
bend_004    0.44   0.08   0.00   231000     88.7    1300   0.71          high           0.31
```

---

## 6. Diagnostics

| Code | Level | Condition |
|------|-------|-----------|
| `VSIM-E070` | ERROR | No valid control surface or particle-flow source |
| `VSIM-E071` | ERROR | No geometry object assigned |
| `VSIM-W072` | WARN | `packing_model` not specified; defaulted to `hard_sphere` |
| `VSIM-W073` | WARN | `contact_model` not specified; defaulted to `hertz_mindlin` |
| `VSIM-W074` | WARN | $J_i \ge 0.95$ in one or more regions — high jamming risk |
| `VSIM-W075` | WARN | `particle_density` not specified; defaulted to 2500 kg/m³ |

---

## 7. Directory Skeleton

```
include/vsim/bridge/
  dem_bridge.hpp
  bridge_fields.hpp
  packing_map.hpp

src/vsim/bridge/
  dem_bridge.cpp
  bridge_parse.cpp
  bridge_validate.cpp
  bridge_export_json.cpp    ← DEM-specific
  bridge_export_tsv.cpp     ← DEM-specific

tests/
  test_dem_bridge_parse.cpp
  test_bridge_missing_refs.cpp
  test_pipe_sph_dem_fea_bridge.cpp
```

---

## 8. Implementation Phases

| Phase | Description |
|-------|-------------|
| 1 | Parse `DEMBridge(...)`, validate object references and source fields, register in manifest |
| 2 | Extract pressure, shear, flow, crossing, cavitation from control surface (**requires frozen 66N/66P field schema**) |
| 3 | Compute $\hat{\phi}_i$, $J_i$, $P_{bridge}$ per region; emit boundary JSON and seed/region TSV |
| 4 | Diagnostics: missing surface, missing geometry, missing density, jamming threshold tests |
| 5 | Integration test: full pipe SPH → control surface → DEM bridge script |

---

## 9. Closure Condition

The work order closes when the following minimal script parses and emits artifacts without error:

```vsim
[bridge]
dem.pipe_packing = DEMBridge(
    from     = system.surface.wall,
    geometry = system.geometry.pipe
)
```

Minimum required outputs:

- `dem_bridge_manifest.json`
- `dem_packing_regions.tsv`

---

## 10. Acceptance Checklist

### Parsing and Validation
- [ ] `DEMBridge(...)` constructor parses without error
- [ ] Validates that `from` references a live control surface object
- [ ] Validates that `geometry` references a live geometry object
- [ ] Emits `VSIM-E070` when source surface is missing
- [ ] Emits `VSIM-E071` when geometry target is missing
- [ ] Emits `VSIM-W072` when `packing_model` is not specified
- [ ] Emits `VSIM-W073` when `contact_model` is not specified
- [ ] Emits `VSIM-W075` when `particle_density` is not specified

### Field Extraction (requires frozen 66N/66P schema)
- [ ] Extracts pressure field from control surface
- [ ] Extracts shear field from control surface
- [ ] Extracts flow velocity field from control surface
- [ ] Extracts particle crossing flux from inlet/wall records
- [ ] Extracts cavitation/void density markers if available
- [ ] Spatially bins all fields onto surface region grid

### Packing and Jamming Computation
- [ ] Computes $\hat{\phi}_i$ per region using flux-velocity proxy formula
- [ ] Computes jamming risk index $J_i = \hat{\phi}_i / \phi_{RCP}$
- [ ] Classifies jamming risk as `low` / `moderate` / `high`
- [ ] Emits `VSIM-W074` when $J_i \ge 0.95$
- [ ] Computes bridging probability $P_{bridge}$ per constriction region

### Export
- [ ] Emits `dem_bridge_manifest.json` with provenance metadata
- [ ] Emits `dem_boundary_conditions.json`
- [ ] Emits `dem_packing_regions.tsv` with all required columns
- [ ] Emits `dem_particle_seed_table.tsv`
- [ ] All proxy estimates labeled as such in output metadata

### Integration
- [ ] Minimal closure script parses and emits required artifacts
- [ ] Full pipe SPH + DEM bridge script (`pipe_sph_dem_fea_bridge.vsim`) runs end-to-end
- [ ] DEM bridge manifest records upstream control-surface provenance
- [ ] Combined test passes alongside WO-VSIM-67O (FEA bridge)
