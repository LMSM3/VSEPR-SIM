# bridge_beta/

**Status: EXPERIMENTAL SCAFFOLD — not included in the active build**

This directory is a staging ground for next-generation bridge architecture work.
It is intentionally excluded from the root `CMakeLists.txt`.

`CMakeLists.disabled` exists here but is **not picked up by cmake** — it is
preserved only so the intended build structure is documented.

| Component | Purpose |
|-----------|---------|
| `dnf_sim/` | Mock dependency resolver (validates prerequisite modules before kernel ops) |
| `coarsegrain_3d_new/` | CG bead → glass molecule 3-D prerender pipeline |

**Shadow risk: NONE.** Root CMakeLists does not `add_subdirectory(bridge_beta)`.

To re-activate for development: rename `CMakeLists.disabled` back to
`CMakeLists.txt` and add `add_subdirectory(bridge_beta)` to the root.

Deprecation criteria: see `CONTRIBUTING-DEPRECATION.md` in the repo root.
