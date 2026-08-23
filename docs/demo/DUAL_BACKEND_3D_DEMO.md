# Isolated Dual-Backend 3D Demonstration

This pipeline runs one deterministic coarse-grain data generator and shares its output with three independent rendering paths plus R/XLSX reporting.

## Components

| Component | Responsibility |
|---|---|
| `dual-backend-3d-data` | Headless deterministic scene/proxy generator |
| `cg-anim-demo` | Existing GLFW/OpenGL `CGVizViewer` presentation |
| `bgfx_random_values_demo` | Isolated BGFX presentation of shared particles |
| `native-particle-demo` | Native Win32/GDI CPU-projected and Lambert-shaded particle presentation |
| `dual_backend_postprocess.R` | PNG/PDF plots and XLSX workbook generation |
| `run_dual_backend_3d.ps1` | End-to-end orchestration |

The BGFX executable retains its historical target name, but it no longer generates random values. It loads `particles.csv`, selects a scene, and shows a rotating state-colored 3D projection.

## Run the headless pipeline

```powershell
cmake --preset release
cmake --build --preset release --target dual-backend-3d-data
cmake --build --preset bgfx-demo --target bgfx_random_values_demo
.\scripts\demos\run_dual_backend_3d.ps1 -Render None -SkipBuild
```

The default output directory is `out/dual_backend_3d`.

## Launch renderers

BGFX only:

```powershell
.\scripts\demos\run_dual_backend_3d.ps1 -Render BGFX -Scene 0 -SkipBuild
```

OpenGL only:

```powershell
cmake --preset vis
cmake --build --preset vis --target cg-anim-demo
.\scripts\demos\run_dual_backend_3d.ps1 -Render OpenGL -SkipBuild
```

Both:

```powershell
.\scripts\demos\run_dual_backend_3d.ps1 -Render Both -Scene 2 -SkipBuild
```

Native CPU/GDI renderer:

```powershell
.\scripts\demos\run_dual_backend_3d.ps1 -Render Native -Scene 0 -SkipBuild
```

The native renderer has no OpenGL, BGFX, Qt, or external rendering dependency. It rotates and projects the shared particle coordinates on the CPU, depth-sorts them, shades each particle as a Lambert-lit disc, and copies the framebuffer into a Win32 GDI window. Press `Space` to pause rotation and `Escape` to close.

## Generated products

```text
manifest.json          schema and run settings
scenes.csv             scene-level proxy table
particles.csv          shared 3D particle positions and state values
proxy_summary.png      R-generated raster visualization
proxy_summary.pdf      R-generated printable visualization
proxy_summary.xlsx     Scenes, Particles, and Manifest workbook sheets
```

The full artifact contract is documented in `docs/demo/DUAL_BACKEND_3D_CONTRACT.md`.

## R dependencies

Required:

- R / `Rscript.exe`
- `writexl` or `openxlsx` for real XLSX output

Optional:

- `jsonlite` for structured manifest ingestion

Install the lightweight XLSX writer with:

```r
install.packages("writexl")
```

## Noninteractive validation

```powershell
python .\tests\test_dual_backend_3d_artifacts.py out\dual_backend_3d
```

The validator checks schema identity, deterministic scene/particle counts, value ranges, and all reporting products.

## Current validation state

- Headless generator: built and executed successfully.
- Generated dataset: 4 scenes and 111 particles.
- R PNG/PDF: generated successfully.
- XLSX workbook: generated successfully with three sheets.
- BGFX target: built successfully; command-line artifact selection verified.
- Native CPU/GDI target: registered for build and consumes the same `particles.csv` contract.
- PowerShell orchestration: completed successfully without opening windows.
- OpenGL demo source: compiled successfully in isolation.
- Full OpenGL `vis` target: currently blocked by unrelated pre-existing repository compilation errors outside this demo.
