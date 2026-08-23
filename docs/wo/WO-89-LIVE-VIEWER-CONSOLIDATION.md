# WO-89 - Live Viewer Consolidation

## Identity
- Owner/context: ChronoDay 89 visual stack consolidation
- Supported frontend: `vsepr-view`
- Scope: retain the fixed-timestep live molecular viewer and archive prior
  interactive viewer implementations without deleting their history.
- Scientific boundary: visual state remains a derived snapshot of simulation
  state; the renderer does not author coordinates, forces, or provenance.

## Status
[W] WO-89: one supported live viewer
[D] ChronoDay 89: visual stack consolidation
[I] COMPLETE
[V] VERIFIED: `vsepr-view` is the sole active interactive viewer target; its
    live scheduling and density-domain regression pair remains registered
[P] LOCAL: legacy source and visual work-order history moved to
    `archive/viewers/legacy-2026-07-22`
[N] NEXT: extend the live renderer only through snapshot-driven scale profiles
    and validated visual representations

## Delivered Boundary
- `cmake/VisBuild.cmake` configures only `vsepr_vis` and `vsepr-view`.
- The CLI launcher resolves only `vsepr-view`; no VTK, lightweight, or daemon
  fallback remains.
- A supplied artifact is passed to the live viewer as a `load` bootstrap
  command. The worker retains ownership of state progression.
- The old Qt/VTK, lightweight, daemon, desktop, and viewer-contract material
  is retained under the dated archive and is not part of the install surface.
- The active visual-stack index now points to the live simulation and density
  domains record.

## Acceptance
- [x] One supported interactive viewer target is configured and installed.
- [x] Historical viewer entry points and build definitions are preserved in a
  dated archive rather than deleted.
- [x] Legacy viewer tests and VTK/Qt3D scene-payload contracts are removed
  from the active test registration.
- [x] Live simulation and visual-density regression coverage remains active.
- [x] Pd-H fitting remains out of scope.

## Out of Scope
- Reintroducing archived frontends as fallback binaries.
- Rewriting artifact schemas or changing simulation-state ownership.
- Pd-H fitting, parameterization, or validation.
