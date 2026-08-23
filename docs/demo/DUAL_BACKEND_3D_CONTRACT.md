# Isolated Dual-Backend 3D Demo Contract

## Purpose

The demo runs one deterministic coarse-grain scene generator and exposes the result to independent consumers:

1. GLFW/OpenGL through `CGVizViewer`.
2. BGFX through the isolated BGFX frontend.
3. Native Win32/GDI through the CPU particle renderer.
4. R for statistical plots.
5. An automatic XLSX workbook generator.

No renderer owns simulation data. Every consumer reads or receives the same generated artifact.

## Output directory

A run writes to a caller-selected directory, defaulting to:

```text
out/dual_backend_3d/
```

Expected contents:

```text
manifest.json
scenes.csv
particles.csv
proxy_summary.png
proxy_summary.pdf
proxy_summary.xlsx
```

Interactive renderer windows are transient and do not alter these files.

## `manifest.json`

The manifest identifies the schema and deterministic run settings:

```json
{
  "schema": "vsepr.dual_backend_3d.v1",
  "generator": "cg-anim-demo",
  "steps": 500,
  "dt_fs": 10.0,
	"generation_ms": 52.25,
  "scene_count": 4,
	"particle_count": 111,
  "scenes_csv": "scenes.csv",
  "particles_csv": "particles.csv"
}
```

Paths are relative to the manifest directory.

Stress packages use the same schema and append a deterministic scene. The manifest particle count must equal the sum of `scenes.csv` counts and the number of data rows in `particles.csv`.

## `scenes.csv`

One row per generated scene:

```text
scene_id,scene_name,particle_count,cohesion_proxy,texture_proxy,stabilization_proxy,density_mean,coordination_mean
```

## `particles.csv`

One row per rendered coarse-grain particle:

```text
scene_id,particle_id,x,y,z,radius,state_value
```

`state_value` is the converged per-particle environment value used for coloring. Every renderer must preserve positions and scene membership from this table.

## Reporting contract

The R postprocessor reads both CSV files and writes:

- `proxy_summary.png`: scene-level proxy comparison.
- `proxy_summary.pdf`: printable equivalent.
- `proxy_summary.xlsx`: workbook containing `Scenes`, `Particles`, and `Manifest` sheets.

The workbook writer must not require Java. The pipeline may install or request `writexl`; if no supported XLSX package is available it must fail with an actionable dependency message rather than writing a renamed CSV.

## Determinism

For identical source revision and command-line settings:

- scene order and identifiers remain stable;
- CSV rows use deterministic ordering;
- numeric values use locale-independent decimal points;
- renderers do not mutate generated artifacts.
