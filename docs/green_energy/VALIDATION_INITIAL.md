# Green Loom Companion Initial Validation

[W] GREEN_LOOM_COMPANIONS
[D] Independent
[I] Installed `build\vsepr.exe` validation of both authored scenarios
[V] Both documents are valid; each reports one expected runtime-mode warning
[P] Source scripts: `scripts/green_energy/`
[N] Add parser regression coverage and retain `reactive_transport` warning until a porous-capture runtime mode is implemented

## Commands

```powershell
build\vsepr.exe validate scripts\green_energy\green_loom_humidity_stress.vsim
build\vsepr.exe validate scripts\green_energy\green_loom_regeneration_retention.vsim
```

## Results

| Script | Result | Diagnostics |
|---|---|---|
| `green_loom_humidity_stress.vsim` | Valid | One warning: `[run] mode 'reactive_transport' is unrecognized - will pass through to runtime` |
| `green_loom_regeneration_retention.vsim` | Valid | One warning: `[run] mode 'reactive_transport' is unrecognized - will pass through to runtime` |

Both documents preserve the required replay configuration: `write_xyzf`, `write_xyzfull`, five-step snapshots, event-level logging, analysis/metrics/event artifacts, SVG/RDF/HTML/dashboard requests, complex `show` directives, and `[open]` advanced replay controls.
