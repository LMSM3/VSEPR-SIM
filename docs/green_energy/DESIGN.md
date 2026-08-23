# Green Loom Companion Scenarios

[W] GREEN_LOOM_COMPANIONS
[D] Independent follow-on to the Three Metallic Pillars parser-first path
[I] Two carbon-capture VSIM companion scenarios: humidity stress and regeneration retention
[V] Pending focused build, parser test, and script validation
[P] Scripts will be published under `scripts/green_energy/`
[N] Runtime porous-capture kernel remains a future implementation; this work establishes parser-valid declarative scenarios and visualization evidence

## Purpose

These two scenarios extend the imported **Green Loom Path** carbon/REE capture-fibre model without claiming calibrated adsorption or transport physics. They are declarative acceptance models designed to exercise cyclic capture configuration, species/reaction records, optimization metadata, dense replay artifacts, and end-of-run presentation controls.

## Scenarios

| Script | Focus | Distinguishing acceptance evidence |
|---|---|---|
| `green_loom_humidity_stress.vsim` | High-humidity post-combustion feed and H2O competition | Capture/selectivity loss, water occupancy, pore access, breakthrough, and REE-site utilization |
| `green_loom_regeneration_retention.vsim` | Repeated low-pressure thermal regeneration | CO2 release, heat duty, capacity retention, sintering proxy, and regeneration efficiency |

## Common visual evidence

Each scenario must dedicate approximately one-third of its configuration to useful end-of-run visualization:

- Dense trajectory truth: `write_xyzf = true`, `write_xyzfull = true`, an explicit snapshot cadence, and event-level capture.
- Analysis artifacts: SVG energy traces, RDF output, HTML dashboard, report, analysis JSON, event JSON, metrics TSV, and dashboard SVG.
- End-of-run summon: `[open] enabled = true`, advanced trajectory controls, data inspector, event overlay, and a generated-artifact fallback through terminal-chart output.
- Domain-specific show directives: porous-fibre scene with capture-state coloring, pore-access and gas-flow overlays; capture/regeneration time series; event timeline; and scalar audit panel.

## Acceptance criteria

1. Both scripts pass `build\vsepr.exe validate` with no errors.
2. Each script retains strict conservation/audit requirements and declares its scenario-specific acceptance criteria.
3. Each script writes dense replay truth and declares an explicit end-of-run open mode.
4. A headless test parses both scripts and verifies their identity, run bounds, export truth, end-of-run open state, and expected reaction-channel/stage presence.

## Scope boundaries

The current VSIM runtime does not yet execute a calibrated porous-fibre capture kernel. Existing parser support stores feature-specific sections and unrecognized keys as declarative/raw configuration. This work does not claim physical capture results until a dedicated runtime kernel consumes those structures.
