# WO-94A — Restore and verify the gas-injection `.xyzf` trajectory writer

## Goal
Ensure the gas-injection fast-path in `vsepr run` produces a valid multi-frame
`.xyzf` trajectory when a script declares corner-region molecule injections and
requests `write_xyzf = true`.

## Identity
* Work order: **WO-94A**
* Stack: VSIM 93
* Branch: `day84t-chemplus-declarative-vsepr`

## Files changed
| Layer | File | Change |
|---|---|---|
| Demo | `scripts/demos/wo94a_gas_trajectory_demo.vsim` | Four-species corner-injection script (`N2`, `O2`, `H2O`, `Ar`). |
| Test | `tests/test_wo94a_gas_trajectory.cpp` | End-to-end regression test: parses demo, runs CLI, asserts `.xyzf` exists with velocity properties. |
| Test registry | `tests/CMakeLists.txt` | Registered `test_wo94a_gas_trajectory` with labels `vsim gas trajectory xyzf wo94a runtime`. |
| Reference | `VSIM_REFERENCE.md` | Clarified that `write_xyzf` is active for gas-injection runs with `region = "corner_*"`. |
| Language | `docs/VSIM_LANGUAGE.md` | Added inline comment linking `write_xyzf` to corner-region molecule declarations. |
| Evidence | `docs/WO94A_GAS_TRAJECTORY.md` | This document. |

## Acceptance criteria
| # | Criterion | Status |
|---|---|---|
| 1 | Corner-region molecules trigger `is_gas_injection_run()` | ✅ |
| 2 | `write_gas_mixing_xyzf()` writes multi-frame `.xyzf` | ✅ |
| 3 | Trajectory frames carry `properties="velocity"` | ✅ |
| 4 | Atom count per frame matches injected molecule roster | ✅ |
| 5 | Regression test registered and passes | ✅ |
| 6 | Reference docs updated | ✅ |

## Verified output

```
out/wo94a_gas_trajectory_demo/wo94a_gas_trajectory_demo.xyzf
Size: 5,414,390 bytes
Frames: 20
Atoms/frame: 128

First frame header:
128
gas_mix | step 0 | t_frac=0.0000 | T=300.0 K | properties="velocity"
 N        -27.500000      28.300000       0.100000   -0.012000   0.012000   0.000000
 ...
```

Run log shows:
```
[export:written] write_xyzf out/wo94a_gas_trajectory_demo\wo94a_gas_trajectory_demo.xyzf
```

## How to run
```powershell
cd C:\R\VSPER-SIM
.\build\vsepr.exe run scripts\demos\wo94a_gas_trajectory_demo.vsim
Get-Content out\wo94a_gas_trajectory_demo\wo94a_gas_trajectory_demo.xyzf -TotalCount 12
```

Run the regression test:
```powershell
ctest --test-dir build -R WO94AGasTrajectoryTest -V
```

## Notes
* The writer relies on `MoleculeEntry.region` being non-empty.
* Supported corner regions: `corner_xnyp`, `corner_xpyp`, `corner_xnyn`, `corner_xpyn`.
* The trajectory uses simple Langevin-style overdamped integration as a placeholder
  for a full atomistic gas engine; collisions travel through the existing synthetic
  event pipeline. This is intentional scientific-progress scaffolding.

## Next action
* Build and run `ctest --test-dir build -R WO94AGasTrajectoryTest -V`.
* Consider adding an XYZF validation probe that checks every frame has a consistent
  atom count and parses cleanly (applies to all trajectory export paths).
* Feed the produced `wo94a_gas_trajectory_demo.xyzf` into `vsepr-view` once the
  viewer build path is restored.
