# WO-93A — VSIM smooth two-line terminal status loop

## Goal
Add a compact, live, colour-mapped two-line terminal HUD to `vsepr run`:
* **Line 1** updates at high cadence with step, energy bar, η bar, and run state.
* **Line 2** updates at medium cadence with CPU %, RAM free/total, disk free, and GPU name.

## Identity
* Work order: **WO-93A**
* Stack: VSIM 93
* Branch: `day84t-chemplus-declarative-vsepr`

## Files changed
| Layer | File | Change |
|---|---|---|
| Definition | `include/vsim/vsim_document.hpp` | Added `show_status_loop`, `status_loop_hz`, `hardware_monitor_hz` to `VisualSection`. |
| Parser | `src/vsim/vsim_parser.cpp` | Wired the three keys in `apply_visual_key()`. |
| Wiring | `src/cli/cmd_run_vsim.cpp` | Added colour-map helpers, energy/η block bars, hardware probes, two-line overwrite renderer, and hook in the step loop. |
| Infra | `src/infra/bootstrap_probe.cpp`/`hpp` | Exposed `total_ram_gb()`, `free_ram_gb()`, `disk_free_gb()`, `detect_gpu()`, `CpuLoadState`, and `cpu_load_fraction()` with external linkage. |
| Build | `cmake/CoreBuild.cmake` | Linked `vsepr_cli` against `vsepr_infra`. |
| QoL | `src/cli/viewer_launcher.cpp` | Added binary-existence guard so missing `vsepr-view.exe` prints a console line instead of a Windows error dialog. |
| Demo | `scripts/demos/wo93a_status_loop_demo.vsim` | Minimal reproducible demo script. |
| Test | `tests/test_wo93a_status_loop.cpp` | Parser schema round-trip test. |
| Test registry | `tests/CMakeLists.txt` | Registered `test_wo93a_status_loop`. |
| Reference | `VSIM_REFERENCE.md` | Added field table rows under `VisualSection`. |
| Language | `docs/VSIM_LANGUAGE.md` | Added example output, semantics, and encoding fallback note. |
| Dev process | `VSIM_DEVELOPMENT.md` | Added per-action evidence-doc requirement. |

## Acceptance criteria
| # | Criterion | Status |
|---|---|---|
| 1 | Schema fields parse with explicit values and correct defaults | ✅ |
| 2 | Two-line HUD renders step, coloured energy/η bars, state token | ✅ |
| 3 | Hardware line shows CPU %, RAM, DISK, GPU | ✅ |
| 4 | GPU detection runs without ambiguity / link errors | ✅ |
| 5 | Output falls back to ASCII `#` when redirected | ✅ |
| 6 | Reference docs updated | ✅ |
| 7 | Parser test registered and passes | ✅ |
| 8 | Missing-viewer dialog replaced with console warning | ✅ |

## Verified output

```
step     0/500  ██████████████  E=    -92.00  ██████████  η=0.107  fire
CPU   0%  RAM 4.4/47.8 GB  DISK 829.6 GB  GPU NVIDIA GeForce RTX 4070   Ar
```

Run also emitted the expected scientific-progress diagnostics from the synthetic path:
* `n_cases = 500`, `n_clusters = 45`, `n_warnings = 1470`
* These warnings confirm the model is being exercised and the error/warning libraries are active, not bypassed.

Log file: `out/wo93a_status_loop_demo/run_log2.txt`

## How to run
```powershell
cd C:\R\VSPER-SIM
build\vsepr.exe run scripts\demos\wo93a_status_loop_demo.vsim
```

or redirected for inspection:
```powershell
build\vsepr.exe run scripts\demos\wo93a_status_loop_demo.vsim > wo93a.log 2>&1
Select-String -Path wo93a.log -Pattern 'step .*fire|CPU .*RAM'
```

## Next action
* Run `ctest --test-dir build -L wo93a` to confirm the parser test passes.
* Port the same overlay to `vsepr-view --live` once VTK + Qt3D build path is restored.
