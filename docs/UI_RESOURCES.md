# Visual UI Resources

The active visual migration boundary is `vsepr-view`, `vsepr-desktop`, and shared Qt UI support. Solver, integrator, and reporting code must expose semantic outcomes only; they must not reference icon paths.

## Stable resource API

All classic raster assets are compiled from `resources/vsim_assets.qrc` and exposed under `:/classic/...`. These aliases are internal compatibility API and must not be renamed casually.

`apps/desktop/ui_resources.hpp` centralizes `UiIcon`, `RunStatus`, `ui_icon()`, and status-to-icon mapping:

- Passed maps to `checkmark`.
- Warning maps to `warning`.
- Failed maps to `critical`.
- Unknown maps to `question`.
- Idle and running map to `information`.

Visible desktop actions retain text labels alongside icons. `vsepr-view` validates the compiled resource set at startup; `vsepr-desktop` consumes the same manifest through `QIcon`.

## Build and portability

When Qt6 is found, `CMAKE_AUTORCC` compiles `resources/vsim_assets.qrc` into both active targets. No active visual executable depends on `.venv`, `site-packages`, PyQt, or QtQuick dialog image paths for these icons. Non-UI installer and reporting references are outside this migration boundary.

The classic icons are original project raster artwork; provenance is in [`THIRD_PARTY_ASSETS.md`](../THIRD_PARTY_ASSETS.md). No themes, recoloring, SVG conversion, animation, toolbar redesign, or abandoned-viewer migration is included in this work order.

## Verification

The `vsepr-view` startup check verifies required resource aliases. A dedicated CTest smoke executable was intentionally not added because the work-order request explicitly allows skipping smoke tests; build and launch validation remains required before release.
