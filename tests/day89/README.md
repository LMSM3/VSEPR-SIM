# Day 89 Test Skeleton

This directory is reserved for tests of the Day 89 simulation execution path.

## Planned coverage

- Script expansion creates the concrete simulation work items required by a run.
- Runtime ownership remains explicit from expanded input through execution.
- A simulation step advances observable simulation time and state.
- Run outputs are emitted from the owned runtime state.
- Visual execution produces the required PNG 3D export.

## Layout

- `fixtures/` — minimal `.vsim` inputs and source structures.
- `expected/` — approved textual, structured, and image-output baselines.

Executable test targets will be registered in `tests/CMakeLists.txt` once the Day 89 public runtime and output contracts are implemented.
