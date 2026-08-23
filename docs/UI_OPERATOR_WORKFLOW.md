# VSIM Minimum Operator Workflow

This checklist defines the minimum practical interactive workflow required by UI-001.

1. Run a `.vsim` script that requests visual output and `write_xyz = true`.
2. Confirm the console reports the generated, verified XYZ artifact path.
3. Confirm the session-bound viewer opens with that artifact as its input.
4. Inspect the loaded geometry and the run label in the viewer window.
5. Use the viewer playback/view controls appropriate to the artifact type.
6. Select an exported artifact from the reported output directory for replay or inspection.
7. When any stage fails, use the explicit console status: export failure, missing artifact, or viewer-launch failure.

The scripts in `scripts/demos/` named `bug_exposer_*` and `ui_workflow_*` are the repeatable demonstrations for this workflow.
