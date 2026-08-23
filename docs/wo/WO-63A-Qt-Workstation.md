# WO-63A — Qt Workstation: Transient Particle Rendering

**Work Order:** WO-VSEPR-SIM-63A  
**Branch:** v5.0.0-main  
**Component:** Desktop Workstation / Qt OpenGL Viewport  
**Status:** Implemented  

---

## 1. Overview

The VSEPR-SIM Qt desktop workstation (``apps/desktop/``) renders molecular
scenes through a single ``QOpenGLWidget`` subclass — ``ViewportWidget``.
Frame data flows through ``scene::SceneDocument`` as the canonical exchange
model; the viewport never touches kernel types directly.

WO-63A extends this pipeline to visualise **transient particles**: short-lived
carriers (electron-like, ion-like, neutron-like, gamma-like, alpha-like) that
run as a sidecar alongside the main bead kernel via ``CGSystemState``.

Each transient particle appears in the viewport as:

- A **coloured sphere** — hue is assigned deterministically from the particle
  type-code and ID; at high speed the sphere blends toward the velocity
  heat-gradient (blue → cyan → yellow-white).
- A **velocity-gradient trail** — a tapering chain of cylinder segments behind
  the sphere; oldest segments are coloured by the velocity gradient at that
  point, newest blend back to the particle's base colour.

---

## 2. Key files

| File | Role |
|---|---|
| ``include/coarse_grain/vis/transient_renderer.hpp`` | Header-only colour, trail, and overlay helpers (no Qt/GL dep) |
| ``apps/desktop/scene/SceneDocument.h`` | Added ``std::vector<vsepr::vis::TransientOverlay> transients`` to ``FrameData`` |
| ``apps/desktop/ViewportWidget.h`` | Declaration of ``drawTransients()``; includes ``transient_renderer.hpp`` |
| ``apps/desktop/ViewportWidget.cpp`` | Implementation of ``drawTransients()``; called at end of ``paintGL()`` |
| ``include/cli/system_state.hpp`` | Kernel-side ``CGSystemState`` with ``run_transient_optimizer_substeps()`` |
| ``src/cli/cmd_run_vsim.cpp`` | VSIM runtime: sidecar built before step loop; optimizer ticked per frame |

---

## 3. Colour assignment

Colours are assigned from a 14-colour high-saturation palette
(``vsepr::vis::TRANSIENT_PALETTE``).  The palette index is:

```
base_hue(type_code) + ((id * 2654435761) >> 28) % 3
```

Type-code defaults:

| Type code | Class        | Base hue    |
|-----------|-------------|-------------|
| -1        | electron-like | hot red     |
| -2        | ion-like      | orange      |
| -3        | neutron-like  | sky blue    |
| -4        | gamma-like    | yellow      |
| -5        | alpha-like    | violet      |
| other     | reserved      | powder blue |

---

## 4. Velocity gradient trail

**Trail storage:** ``TransientOverlay::trail`` holds up to 32
``TrailPoint`` records (oldest at front, newest at back). The kernel emitting
system calls ``vsepr::vis::make_overlay()`` each render frame to append the
current position as a new trail point, shifting the oldest out when full.

**Trail geometry:** ``drawTransients()`` iterates trail segments
oldest → newest, drawing one cylinder per segment.

- **Radius** tapers quadratically: ``r_base * (0.04 + 0.62 * t²)``
  where ``t`` = 0 at oldest, 1 at newest.
- **Colour** blends between the velocity-gradient colour at that sample point
  (older segments) and the particle's base colour (newer segments),
  using an ease-in factor ``a = age_t²``.

**Velocity gradient stops:**

| ``speed_norm`` | Colour |
|---|---|
| 0.0 | deep blue (#1a2dff) |
| 0.5 | cyan-green |
| 1.0 | hot yellow-white |

``speed_norm`` = ``|v| / reference_max_speed``; caller clamps to [0,1].

---

## 5. Head sphere shading

The head sphere uses the particle's assigned random colour as the base.
When ``speed_norm > 0`` it blends up to 55% toward the velocity heat colour:

```cpp
float blend = speed_norm * 0.55f;
hr = base.r + blend * (vel_colour.r - base.r);
```

The sphere is drawn with the existing Blinn-Phong shader (no shader change
required); visual identity is carried entirely through ``uColor``.

---

## 6. Sphere radii by type

| Type code | Class          | Radius (Å) |
|-----------|----------------|-----------|
| -1        | electron-like  | 0.18      |
| -2        | ion-like       | 0.30      |
| -3        | neutron-like   | 0.28      |
| -4        | gamma-like     | 0.14      |
| -5        | alpha-like     | 0.38      |
| other     | reserved       | 0.22      |

---

## 7. Data flow

```
CGSystemState (kernel)
  └─ run_transient_optimizer_substeps()   <- per step-loop frame
       └─ TransientParticle[]
            └─ vsepr::vis::make_overlay()   <- caller builds overlay
                 └─ TransientOverlay
                      └─ scene::FrameData::transients[]
                           └─ ViewportWidget::paintGL()
                                └─ drawTransients()
                                     ├─ drawCylinder()  (trail segments)
                                     └─ drawSphere()    (head sphere)
```

---

## 8. Extending / adding transient types

1. Add a new negative code to ``TransientTypeCode`` in ``system_state.hpp``.
2. Add a ``case`` to ``palette_index_for_type()`` in ``transient_renderer.hpp``.
3. Add a ``case`` to ``transient_sphere_radius()`` in ``transient_renderer.hpp``.
4. Populate ``TransientParticle`` instances in ``CGSystemState::build_preset()``
   or inject them from kernel event handlers.
5. Call ``vsepr::vis::make_overlay()`` in the render bridge to emit
   ``TransientOverlay`` records into ``FrameData::transients``.

No Qt, CMake, or shader changes are required for new types.

---

## 9. Camera and rendering notes

- Transients are drawn **after** atoms in ``paintGL()`` so they appear on top
  of the molecular structure.
- Wireframe mode (``W`` key) applies to transients as to all geometry.
- ``F`` / fit-camera does not yet account for transient positions; if particles
  migrate far from the scene centroid they may leave the view frustum.
  Call ``resetCamera()`` or ``fitCamera()`` after large displacements.
- Trail alpha-fading is approximated by shrinking radius (no GL blending
  state changes required).

---

## 10. Playback integration

When the desktop is in trajectory playback mode, each ``FrameData`` in
``SceneDocument::frames`` carries its own ``transients`` snapshot.  The
viewport replays trails frame-by-frame automatically; no additional
integration is required.

---

*Generated: WO-63A integration phase — Day 63+  |  v5.0.0-main*