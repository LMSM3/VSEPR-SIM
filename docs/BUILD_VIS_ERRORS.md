# Visual Build Error Inventory

Captured from `build_vis` on the current worktree after running Ninja first and `cmake --preset vis` second.

## Command sequence

1. Cleared terminal and waited 30 seconds.
2. Ran `ninja -C build_vis -k 0`.
3. Ran `cmake --preset vis`.

## CMake result

`cmake --preset vis` completed configuration and generation successfully. Qt 6.10.1, GLFW, GLEW, and GLM were found, and the visual targets were generated.

Observed non-fatal configuration warning:

- `tests/CMakeLists.txt:56`: unable to create the test data junction (`Invalid switch - "R"`). This is a Windows command/junction fallback failure, not an R-language or `Rscript` failure. Tests that require runtime data may fail.

## Ninja blockers

The Ninja build did not reach a complete visual-target link. It exposed these source-level blockers outside the UI resource changes:

| File | Failure | Impact |
| --- | --- | --- |
| `atomistic/reaction/engine.cpp:11` | Stray token `wxsswsxaadsxx` | Immediate C++ parse failure. |
| `atomistic/reaction/engine.cpp:15` | `vsepr::chemistry_db` is not declared | Reaction-engine compile failure. |
| C++ standard-library `<format>` / locale instantiation | `numpunct<char>::string_type` conversion failure under GCC 15.2.0 | Existing toolchain/header compatibility failure encountered while compiling visual dependencies. |
| `src/sim/sim_state.cpp` via `src/pot/energy_model.hpp:104` | `-Wstringop-overflow` diagnostic | Potential vector-size/gradient allocation defect requiring review. |

Additional warnings observed in unrelated code:

- `include/cli/cmd_batch.hpp:10`: `/*` inside a comment.
- `src/cli/cmd_x_suite.cpp:24`: `NOMINMAX` redefined.
- `atomistic/reaction/dissolution.cpp`: unused parameters.

## UI resource status

The resource CMake integration no longer produces a configuration error. `CMAKE_AUTORCC` is enabled when Qt is available, and the shared `resources/vsim_assets.qrc` is attached to both `vsepr-view` and `vsepr-desktop`.

## Recommended repair order

1. Remove the stray token and restore the intended chemistry database API in `atomistic/reaction/engine.cpp`.
2. Resolve the GCC 15 `<format>`/locale incompatibility or isolate the source that instantiates it.
3. Investigate the vector-size calculation behind the `energy_model.hpp:104` overflow warning.
4. Repair the Windows junction command in `tests/CMakeLists.txt`; investigate the invalid `R` switch as a `cmd.exe`/link-creation issue, not as an R reporting dependency.
5. Re-run `ninja -C build_vis -k 0`, then build `vsepr-view` and `vsepr-desktop` specifically.
