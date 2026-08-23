# Green Loom Companion Final Validation and Visualization Readiness

[W] GREEN_LOOM_COMPANIONS
[D] Independent
[I] Two parser-valid carbon/REE fibre scenarios authored with dense replay and explicit end-of-run presentation controls
[V] Release configuration/build and focused parser test pass; both scripts validate without errors
[P] `scripts/green_energy/`, `docs/green_energy/`, and `tests/test_green_loom_vsim_parser.cpp`
[N] Install VTK + Qt6 Qt3D to enable the requested interactive live viewer; implement porous-capture runtime kernels to populate physical capture metrics

## Final gate

- `cmake --preset release` completed successfully.
- `ninja -C build test_green_loom_vsim_parser` completed successfully.
- `build\tests\test_green_loom_vsim_parser.exe` passed all checks.
- Both scripts pass `build\vsepr.exe validate` with no errors.
- Each script has one expected warning: `reactive_transport` is not yet a recognized dedicated runtime mode and passes through.

## Visualization contract

Both scenarios explicitly declare:

- Dense replay truth: `write_xyzf = true`, `write_xyzfull = true`, `snapshot_interval = 5`, `event_interval = 1`, and `metrics_interval = 5`.
- Analysis artifacts: XYZ/XYZF/XYZFull, analysis JSON, metrics TSV, event JSON, report Markdown, manifest JSON, dashboard SVG, energy SVG, RDF SVG, and HTML dashboard.
- Useful complex presentation: porous-fibre capture-state scene directives, gas-flow/pore-access/REE-cluster overlays, capture/regeneration time series, event timeline, scalar summary, and optimization Pareto view where relevant.
- End-of-run summon: `[open] enabled = true`, `mode = "advanced"`, `source_format = "xyzf"`, plus trajectory, data-inspector, event, and bond overlays.

## Verified runtime route and known blocker

`src/cli/cmd_run_vsim.cpp` launches the supported live viewer whenever `[open] enabled = true` and a verified XYZ artifact exists. The release configuration currently reports:

```
Live viewer: OFF -- requires VTK + Qt6 Qt3D
```

Therefore, the scripts correctly summon the viewer through the runtime route and are guaranteed to request dense, headless-safe artifacts. Actual interactive viewer launch is blocked by the environment's unavailable VTK/Qt6 Qt3D dependencies, not by script or parser configuration.

## Scope boundary

The porous fibre, REE-site, transport, cycle, and optimization configuration remains declarative until a dedicated porous-capture kernel consumes it. Current validation and test evidence prove grammar, replay/export configuration, and end-of-run launch intent—not calibrated adsorption, transport, or regeneration physics.
