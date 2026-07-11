# v4/

**Status: SUPERSEDED — v4 experimental outputs, not part of v5 build**

This directory contains the v4-era standalone experiment files used during
the v4 scoring and formation-record research phase. They are not included
in the root CMake build.

| File | Notes |
|------|-------|
| `core_run.cpp` | v4 standalone kernel runner |
| `v4_scores_test.cpp` | v4 scoring regression test |
| `*.hpp` | v4 correlation, compactness, gamma score headers |
| `build_and_test.sh` / `run_core.sh` | Standalone build scripts |
| `core_run`, `v4_scores_test` | Compiled binaries (local, not tracked) |

The v4 logic has been absorbed into `src/` and `include/` for v5.

Deprecation criteria: see `CONTRIBUTING-DEPRECATION.md` in the repo root.
