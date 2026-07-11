# legacy/

**Status: ARCHIVED — not part of the active build**

This directory contains C API code from the pre-v4 era that has been superseded
by the C++ kernel in `src/` and `include/`.

| File | Notes |
|------|-------|
| `c_api/decay_event.c/.h` | Original C decay-event interface — replaced by `include/atomistic/` |
| `c_api/particle_ids.c/.h` | Particle ID table — superseded by registry in `include/vsim/vsim_registry.hpp` |

**Do not include these files in new builds.** They are kept for historical reference
and to allow diff-based archaeology of the API evolution.

Deprecation criteria: see `CONTRIBUTING-DEPRECATION.md` in the repo root.
