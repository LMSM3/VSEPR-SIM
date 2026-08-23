# VSIM Known Problems

This file tracks operator-visible features that exist partially, route incompletely, or have not yet reached a reliably usable level. An item remains open until its complete path is demonstrated through the VSIM scripting surface. So there are severity levels of improvement, some are simply here as reminders of what to do next, others are explicit items with an obvious resolve.

## Status legend

- `OPEN` — confirmed problem; implementation incomplete or unusable
- `PARTIAL` — some layers work, but the operator-visible path is incomplete
- `VERIFY` — suspected fixed; awaiting end-to-end proof
- `CLOSED` — end-to-end behavior validated with a regression script or test
- `UNRESOLVED` — known problem retained for follow-up; no end-to-end resolution has been demonstrated

## Active problems

### VIS-001 — Visual output does not reach a usable final window

- **Status:** `CLOSED`
- **Area:** visual output routing / window dispatch / XYZ handoff
- **Observed:** The visual-output system appears to route correctly at the top level, but no final visualization window is pushed during normal output.
- **Secondary failure:** When a window is explicitly pushed open, the XYZ output is not routed into it.
- **Usable behavior required:** A `.vsim` run requesting visual output opens or targets the intended window and delivers the generated XYZ data to that window without a manual workaround.
- **Likely investigation boundary:** Treat top-level routing, output artifact generation, window creation, and XYZ consumer binding as separate checkpoints. Do not mark this closed merely because the request parses or an XYZ file exists.
- **Closure evidence, 2026-07-22:** `scripts/demos/bug_exposer_visual_xyz_handoff.vsim` now writes a real XYZ artifact, launches the canvas-only Qt/VTK viewer, and receives `vsepr.viewer_ack.v1` with `backend="qt-vtk"`, `ok=true`, the exact artifact path, `frame_count=1`, `max_particle_count=3`, run label `bug_exposer_visual_xyz_handoff`, and a deterministic content hash. Acceptance rejects a fallback backend and also requires the parallel Matplotlib PNG. `bug_exposer_matplotlib_png_only.vsim` proves static PNG output remains available without launching Qt/VTK. Nonblank molecular, generic FEA, and Matplotlib captures are recorded under `out/`.

### SMALL-001 — 

- **Status:** `UNRESOLVED`
- **Area:** export path reporting
- **Observed:** Export output from a completed script run does not clearly report where artifacts are stored, how many files were written, or their sizes.

- **Usable behavior required:** At least one helper program reports the export location, artifact count, and size of each exported file after a script run.

### UI 1 is an example of a continuously improving program.
### UI-001 — Interactive interface remains too shallow for practical operation

- **Status:** `UNRESOLVED`
- **Area:** interactive operator interface
- **Observed:** The interface exists and continues to improve, but its interaction model lacks enough depth for practical simulation inspection and control.
- **Current limitation:** Operators cannot yet rely on a sufficiently complete set of controls, state feedback, inspection tools, and output interaction to treat the interface as a primary workflow.
- **Usable behavior required:** The interface must support a coherent minimum workflow: load or receive a run, inspect current state, control playback/view behavior, select relevant outputs or observables, and receive clear failure/status feedback.
- **Proof required:** A defined minimum interaction checklist plus one repeatable demo exercising the entire operator workflow. Individual controls do not close this item unless they form a usable sequence.

## Intake template

### ID — Short problem title

- **Status:** `OPEN`
- **Area:**
- **Observed:**
- **Expected:**
- **Reproduction:**
- **Usable behavior required:**
- **Proof required:**
- **Owning day / WO:** Unassigned

## Next action

Create the smallest `.vsim` `BUG_EXPOSER` that requests visual output, confirms XYZ production, and records whether a window was created and whether that window received the XYZ payload.
