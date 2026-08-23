# VSIM Runtime Acceptance Probes

These flat `.vsim` scripts expose operator-visible runtime behavior. They are not design-only precompiler inputs.

## Probes

- `bug_exposer_headless_viewer_suppression.vsim`: proves `output_type = "none"` suppresses viewer launch.
- `bug_exposer_export_summary.vsim`: proves requested exports are written or reported with a reason.
- `bug_exposer_dissolution_bridge.vsim`: proves the dissolution section reaches a persistent runtime pass and kernel event output.
- `bug_exposer_material_sampling.vsim`: proves supported sampling fields are distinguished from raw-only expansion intent.

## Run

```powershell
./scripts/debugging/run_acceptance.ps1
```

Add `-IncludeVisualHandoff` to launch the visual probe and verify requested viewer handoff. The default command remains headless.
