# WO-v513-QCD-Extreme — QCD Extreme-Environment Layer
<!-- v5.1.3~2 | branch: v5.0.0-main | WO-v513-QCD -->

## Purpose

Add a quark-gluon plasma (QGP) sidecar to the VSEPR-SIM kernel.  
Model dynamic quarks and gluons on **time-dilated** (slowed) scales inside existing  
bead simulations, using the **T_A|B dual-timescale** architecture (see WO-63A).

Not a lattice-QCD solver. A scaled-analog confinement model for extreme-environment testing.

---

## Source Tree (v5.1.3~2)

```
include/
  coarse_grain/
    physics/
      qcd_transient.hpp          # SU(3) color, Cornell potential, T_A|B scheduler
    vis/
      qcd_colour_charge.hpp      # color-charge → RGB, gluon rainbow trails, QCDOverlay
  cli/
    system_state.hpp             # CGSystemState: enable_qcd, qcd_plasma, run_qcd_tb_step()
apps/
  desktop/
    scene/
      SceneDocument.h            # FrameData.qcd_overlays
    ViewportWidget.h             # drawQCD() declaration
    ViewportWidget.cpp           # drawQCD(): strings + trails + spheres
docs/
  wo/
    WO-v513-QCD-Extreme.md       # this file
    WO-v513-QCD-Extreme.tex      # companion TeX (equations only)
    WO-VSEPR_SIM-63A-ARCHIVE.tex # T_A|B doctrine precedent
```

---

## T_A|B Architecture (QCD extension)

| Step | Symbol | Default | Description |
|---|---|---|---|
| Bead / hadronic | T_B | Δt_B = 1 fs | outer structural timestep (existing kernel) |
| QCD substep | T_A | Δt_A = Δt_B / K | inner color-charge / confinement substep |
| Substep count | K | 200 | tunable; lower K = faster, less accurate |
| Time dilation | τ_scale | 1×10⁻³ | 1 fm/c ↔ τ_scale fs in VSIM time |

Each `CGSystemState::update_environment()` call:
1. Runs K T_A substeps via `qcd_run_tb_step()`
2. Runs N transient-optimizer substeps (WO-63A, unchanged)

---

## Particle Type Codes (extends WO-63A table)

| Code | Particle | q (e) | mass analog |
|---|---|---|---|
| +1…+N | Beads | explicit | heavy structural |
| −1 | Charged transient (electron-like) | −1 | light |
| −3 | Neutral transient (neutron-like) | 0 | medium |
| **−7** | **Up quark** | **+2/3** | 0.0022 |
| **−8** | **Down quark** | **−1/3** | 0.0047 |
| **−9** | **Strange quark** | **−1/3** | 0.096 |
| **−10** | **Charm quark** | **+2/3** | 1.28 |
| **−11** | **Bottom quark** | **−1/3** | 4.18 |
| **−12** | **Top quark** | **+2/3** | 173.1 |
| **−13** | **Gluon** | 0 | 0 (massless) |
| **−14** | **Antiquark (generic)** | −q_quark | by flavor |

---

## Physics Model

### Cornell Potential

```
V(r) = −κ/r + σ·r
F(r) = κ/r² + σ         (magnitude; sign from Casimir factor)
```

Default: κ = 0.52, σ = 0.18, r_min = 0.05 Å, r_conf = 2.0 Å  
(All dimensionless analogs; scaled to VSIM length/energy units via τ_scale.)

### SU(3) Casimir Color Factors

| Pair | Factor | Behavior |
|---|---|---|
| same color (R–R, G–G, B–B) | +1/6 | repulsive |
| color–anticolor complement | −4/3 | **attractive** (singlet) |
| other pairs | −1/6 | weak attractive |

### Confinement Gate

When `string_length > r_conf`: both string partners deactivated,  
`hadronization_events` counter incremented.  
Rendered as white flash at that position.

### Gluon Channels (8 Gell-Mann basis)

| Index | Source | Destination |
|---|---|---|
| g0 | R | R̄ |
| g1 | R | Ḡ |
| g2 | G | R̄ |
| g3 | G | Ḡ |
| g4 | R | B̄ |
| g5 | B | R̄ |
| g6 | G | B̄ |
| g7 | B | B̄ |

---

## SU(3) Colour → RGB Map

| Charge | RGB | Appearance |
|---|---|---|
| Red | (1.00, 0.12, 0.12) | vivid red |
| Green | (0.10, 0.95, 0.18) | vivid green |
| Blue | (0.12, 0.28, 1.00) | cobalt blue |
| AntiRed | (0.00, 0.90, 0.90) | cyan |
| AntiGreen | (0.90, 0.10, 0.90) | magenta |
| AntiBlue | (0.95, 0.90, 0.00) | yellow |
| Neutral (gluon) | (1.00, 1.00, 1.00) | white |

Gluon trails: rainbow sweep from `color_src` hue → `color_dst` hue,  
with a white-yellow Gaussian flash at the midpoint.

String cylinder: blue (slack) → orange → red → white (breaking).

---

## Viewport Rendering Pass Order

```
paintGL():
  1. bonds / atoms  (existing)
  2. drawTransients()  (WO-63A transient overlays)
  3. drawQCD():
       pass 1: confinement string cylinders   (color = string tension)
       pass 2: trail cylinders               (color = SU(3) hue / rainbow)
       pass 3: head spheres                  (color = SU(3) + speed blend)
```

---

## Runtime Integration

### Enable in code

```cpp
cg_state.enable_qcd = true;
// auto-seeds K=200, κ=0.52, σ=0.18 on first update_environment() call
```

### Custom configuration

```cpp
cg_state.qcd_plasma.cornell.kappa     = 0.45;
cg_state.qcd_plasma.cornell.sigma     = 0.20;
cg_state.qcd_plasma.cornell.r_conf    = 1.8;
cg_state.qcd_plasma.cornell.tau_scale = 5e-4;
cg_state.qcd_plasma.scheduler.K       = 100;
cg_state.enable_qcd = true;

// Manual seed (optional — auto-seeds from bead count otherwise)
vsepr::qcd::QCDPopulationConfig cfg;
cfg.n_quarks   = 6;
cfg.n_gluons   = 4;
cfg.box_radius = 2.5;
cfg.seed       = 0x513'0001u;
vsepr::qcd::qcd_seed_plasma(cg_state.qcd_plasma, cfg);
```

### Bridge to SceneDocument

```cpp
// Kernel/CLI bridge side — populate qcd_overlays for current frame
frame.qcd_overlays.clear();
float v_ref = 0.3f;
for (int i = 0; i < (int)plasma.particles.size(); ++i) {
    const auto& qp = plasma.particles[i];
    vsepr::vis::QCDKernelData kd;
    kd.id       = qp.id;
    kd.flavor   = qp.flavor;
    kd.color    = qp.color;
    kd.gluon_channel = qp.gluon_channel;
    kd.px = (float)qp.position.x;
    kd.py = (float)qp.position.y;
    kd.pz = (float)qp.position.z;
    kd.vx = (float)qp.velocity.x;
    kd.vy = (float)qp.velocity.y;
    kd.vz = (float)qp.velocity.z;
    kd.string_length     = qp.string_length;
    kd.r_conf            = plasma.cornell.r_conf;
    kd.active            = qp.active;
    kd.string_partner_idx = qp.string_partner;
    const auto* prev = (!prev_frame.qcd_overlays.empty() && i < (int)prev_frame.qcd_overlays.size())
        ? &prev_frame.qcd_overlays[i] : nullptr;
    frame.qcd_overlays.push_back(vsepr::vis::make_qcd_overlay(kd, v_ref, prev));
}
```

---

## Claim Safety

This is a **scaled analog** confinement model, not a QFT/lattice QCD solver.

Valid claims:
- "The bead-level simulation couples a quark-gluon analog sidecar with a T_A|B dual-timescale scheduler."
- "Confinement events are modeled using a Cornell-analog potential and a configurable string-length gate."
- "The color-force sign follows SU(3) Casimir-factor projections at the single-pair level."

Invalid claims:
- "This predicts QCD vacuum structure."
- "Confinement is physical below r_conf."
- "Gluon color currents are gauge-invariant."

---

## Diagnostics

| Counter | Field | Description |
|---|---|---|
| Hadronization events | `qcd_plasma.hadronization_events` | pairs deactivated by string gate |
| Gluon emission events | `qcd_plasma.gluon_emission_events` | reserved; not yet auto-incremented |
| Step count | `qcd_plasma.step_count` | total T_A substeps executed |
| Active particles | `qcd_plasma.num_active()` | particles not yet deactivated |

---

## Non-Goals (v5.1.3~2)

- Vacuum polarization / sea quarks
- Parton shower / fragmentation
- Finite-temperature QCD phase diagram
- Wilson loops / gauge field
- Lattice discretization

These are documented as future research gates, not current-release features.

---

*WO-v513-QCD-Extreme — v5.1.3~2*
