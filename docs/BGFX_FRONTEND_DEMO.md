# BGFX Frontend Demo

## Purpose

The BGFX frontend demo is a small, isolated rendering experiment for VSEPR-SIM. It provides a persistent native 3D window without modifying the existing OpenGL visualization stack. The demo is intended to answer a practical frontend question: can VSEPR-SIM use BGFX as a more flexible rendering layer while retaining a lightweight C++ application model?

The current demonstration does not render molecular geometry yet. Instead, it creates a stable rendering loop and presents a small set of slowly changing random values. This keeps the first integration focused on window ownership, renderer initialization, frame submission, resize handling, and clean shutdown rather than prematurely coupling BGFX to simulation data structures.

The demo is deliberately opt-in. Normal release, test, and OpenGL visualization presets do not fetch or build BGFX. This prevents an experimental frontend backend from increasing the dependency or build cost of ordinary VSEPR-SIM workflows.

## Source and build files

| File | Responsibility |
|---|---|
| `apps/bgfx_random_values_demo.cpp` | Persistent GLFW window and BGFX render loop |
| `cmake/BgfxDemo.cmake` | BGFX, BX, BIMG, and GLFW dependency setup |
| `CMakeLists.txt` | `BUILD_BGFX_DEMO` feature flag and module inclusion |
| `CMakePresets.json` | Dedicated `bgfx-demo` configure and build presets |
| `build_bgfx_demo/` | Generated configure and build directory |

The implementation is independent of `vsepr_vis`, `vsepr_render`, GLEW, ImGui, and the existing OpenGL renderer. It therefore provides a clean backend experiment rather than a second path through the current OpenGL code.

## Configuration

Use the dedicated preset from PowerShell:

```powershell
Set-Location C:\R\VSPER-SIM
C:\msys64\ucrt64\bin\cmake.exe --preset bgfx-demo
C:\msys64\ucrt64\bin\cmake.exe --build --preset bgfx-demo --target bgfx_random_values_demo
```

The preset uses the Ninja generator and the project’s configured GCC toolchain. It places generated files in `build_bgfx_demo`, leaving the normal `build`, `build_vis`, and other preset directories untouched.

`BUILD_BGFX_DEMO` defaults to `OFF`. It can also be enabled manually when using a compatible Ninja configuration:

```text
-DBUILD_BGFX_DEMO=ON
```

When enabled, CMake fetches `bgfx.cmake` at a pinned release tag. That repository supplies BGFX together with its BX and BIMG dependencies. GLFW is resolved from the system when available; otherwise, the module fetches GLFW 3.4 for the demo window.

The dependency fetch requires network access during the first configuration. Subsequent configurations reuse the populated dependency sources in the build directory.

## Running the demo

After a successful build, launch the executable directly:

```powershell
C:\R\VSPER-SIM\build_bgfx_demo\bgfx_random_values_demo.exe
```

The application opens a persistent window titled **VSEPR-SIM BGFX Random Value Demo**. The window remains open until it is closed normally or the Escape key is pressed.

The display contains:

- A demo title.
- A persistent-render-window status line.
- The configured refresh interval.
- Six labelled integer values in the range `000` through `999`.
- The active BGFX renderer name.

The values are generated from a deterministic Mersenne Twister seeded with `89`. They initially have a repeatable sequence and are replaced approximately every 1.2 seconds. The slow cadence makes changes visible while avoiding the appearance of a rapidly updating benchmark screen.

## Runtime flow

The application follows a deliberately small ownership model:

1. GLFW initializes the platform window layer.
2. GLFW creates a window with `GLFW_NO_API`, so it does not create an OpenGL context.
3. The native Windows window handle is passed to BGFX through `bgfx::PlatformData`.
4. BGFX initializes with automatic renderer selection.
5. Each loop iteration polls GLFW events and checks for Escape.
6. The framebuffer size is read and supplied to `bgfx::reset()` when rendering dimensions change.
7. A refresh timer replaces the six values at the configured slow cadence.
8. BGFX clears the view, writes debug text, and submits a frame.
9. Shutdown occurs in reverse ownership order: BGFX, GLFW window, then GLFW.

BGFX automatically selects the available renderer rather than forcing Vulkan. This is intentional for the first frontend experiment: the same demo can establish whether the backend works before a policy decision is made about Vulkan, Direct3D, or another renderer-specific path.

## Validation completed

The dedicated configuration completed successfully after pinning the BGFX CMake dependency to an available release tag. The target then built successfully with the project’s GCC 15.2 C++23 toolchain.

Validated artifact:

```text
build_bgfx_demo/bgfx_random_values_demo.exe
```

The build initially exposed an API-version mismatch: the selected BGFX release declares `PlatformData` and `setPlatformData()` in `bgfx/bgfx.h` rather than providing a separate `bgfx/platform.h` header. The demo now includes the current public header and uses the supported API.

The existing OpenGL visualization warning shown while configuring the isolated preset is unrelated to this demo. `BUILD_VIS` is intentionally disabled in the BGFX preset, so the OpenGL renderer is not required for this target.

## Current limitations

This is a frontend skeleton, not a production molecular viewer. It currently has the following limitations:

- Windows native window binding is implemented first.
- It renders BGFX debug text rather than a molecule, mesh, atom instancing field, or trajectory.
- There is no camera control, mouse interaction, UI layer, shader asset pipeline, or screenshot export.
- The random values are generated locally and are not connected to the simulation runtime.
- No runtime backend selector is exposed yet.
- The demo does not yet provide a CTest entry because it requires an interactive window.

These limitations are intentional. The experiment first establishes that a persistent frontend window can own a render loop independently of the existing OpenGL path. Adding molecular data before this contract is stable would make backend diagnosis harder.

## Recommended next increments

The next useful step is not a broad renderer rewrite. It is a narrow data bridge from a stable simulation snapshot into the existing BGFX loop. A suitable sequence is:

1. Replace one random-value row with a small in-memory particle buffer.
2. Render points or instanced low-cost markers using a minimal BGFX shader pair.
3. Add a camera orbit and a viewport resize path.
4. Feed the buffer from a frozen simulation result before connecting live stepping.
5. Add a PNG capture command for validation and comparison with the current viewer.
6. Introduce a backend capability report showing the selected BGFX renderer.

Only after those pieces work should the demo be evaluated for Vulkan-specific configuration or integration with the VSEPR-SIM frontend. The current isolated target keeps that decision reversible.
