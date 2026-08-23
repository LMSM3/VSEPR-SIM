# WO-BRIDGE-65B — Dead Code Candidates

These files or code sections were identified during the audit as potentially
no longer useful, superseded, or disconnected from the active pipeline.

**Rule:** Nothing is deleted without explicit confirmation.
This list is input for the next work order, not a deletion manifest.

---

## Tier 1: Archive candidates (high confidence, not connected to runtime)

| File | Reason | Evidence | Action |
|---|---|---|---|
| `src/ufx_auto2/material_generator.cpp` | Offline Materials Project data fetcher. Not called by any runtime path. Does not consume formation events or defect records. | No call sites found in `src/cli/` or `src/vsim/`. Output goes to flat JSON, not connected to material sampler. | ARCHIVE — move to `archive/ufx_auto2_fetcher/` in a separate WO |
| `src/ufx_auto2/materials_project_fetcher.cpp` | Same as above. HTTP fetcher for Materials Project API. CI-hostile (requires network). Not gated behind a build flag. | No test coverage. No callers in main runtime. | ARCHIVE — same WO as above |

## Tier 2: Zombie code inside live files (code that exists but is never reached)

| Location | Description | Verdict |
|---|---|---|
| `include/kernel/kernel_event.hpp` — `DefectEvent::compute()` | Method defined, struct constructed only in header. No `src/` file pushes a `DefectEvent` to `KernelEventLog`. | WIRE — not dead, just disconnected. |
| `include/mission/sim_lattice.hpp` — `LatticeState::defects` vector | Populated by `insert_defect()` in the mission path, but never read by the event log or report engine. | WIRE — needs connection. |
| `include/mission/sim_lattice.hpp` — defect `dose_eV_atom` field | Set during insertion but never used in any analysis or output. | PATCH — wire to severity computation when P1 lands. |

## Tier 3: Needs inspection (unknown status)

| File | Concern |
|---|---|
| `src/molecule/report_prerender.cpp` | Referenced in formation report path; unclear if defect section is present or placeholder. Needs line-by-line inspection. |
| `include/core/bio_report_engine.hpp` | Bio-specific report engine. Unclear if it shares the defect section concern. May be orthogonal. |

## Not dead (confirmed active)

These were considered but are confirmed live:

- `include/kernel/kernel_event.hpp` (FormationEvent) — FIRE exit path wired, tested.
- `tests/test_formation_event_wiring.cpp` — 5 tests, compiled, all pass.
- `src/vsim/node_accessor.cpp` — reads KernelEventLog for export, active.
- `src/cli/cmd_run_vsim.cpp` — wires `.vsim` source_formula to FormationEvent, active.

---

*WO-BRIDGE-65B dead code audit — VSEPR-SIM v5.1.13 — 2026-05-19*
