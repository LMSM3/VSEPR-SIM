# v5/

**Status: EXPERIMENTAL DEMOS — not part of the main build**

This directory contains early v5 prototype files used during the initial
bead-transport and 3-D demo development phase. Not included in the root CMake build.

| File | Notes |
|------|-------|
| `v5_demo.cpp` / `v5_3d_demos.cpp` | Early v5 prototype executables |
| `bead_transport.hpp` | Bead transport header (prototype; production version in `src/`) |
| `build_v5.sh` | Standalone build script |
| `phase_*.csv` | Phase log output data from prototype runs |
| `v5_demo` | Compiled binary (local, not tracked) |

The production v5 kernel lives in `src/`, `include/vsim/`, and `VSEPR/`.

Deprecation criteria: see `CONTRIBUTING-DEPRECATION.md` in the repo root.
