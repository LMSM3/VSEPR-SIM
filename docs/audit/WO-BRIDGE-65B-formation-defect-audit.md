# WO-BRIDGE-65B — Formation Defect Audit
## Curious Audit of Remaining Code Before Subatomic Implementation

| Field | Value |
|---|---|
| **Work Order** | WO-BRIDGE-65B |
| **Version** | VSEPR-SIM v5.1.13 stable |
| **Branch** | v5.0.0-main |
| **Phase** | Post-classical MD stabilisation · Pre-subatomic CAI implementation |
| **Status** | AUDIT COMPLETE — patch queue open |
| **Date** | 2026-05-19 |

---

## 1. Core question

When formation occurs, what defects emerge, how are they detected,
and where does the code currently lose them?

Formation should expose:

```
D_formation = {
	D_vacancy,
	D_interstitial,
	D_substitution,
	D_surface,
	D_void,
	D_strain,
	D_fracture,
	D_misbond,
	D_phase_mismatch
}
```

The bridge must be:

```
formation event → defect emergence → defect logging → material sampling readiness
```

---

## 2. Audit findings — formation engine

### 2.1 What is wired

| File | What it does |
|---|---|
| `include/kernel/kernel_event.hpp` | Defines `FormationEvent` and `DefectEvent` with full field structs |
| `src/sim/optimizer.cpp` (via FIRE exit) | Emits `FormationEvent` into `KernelEventLog` on convergence or failure |
| `src/cli/cmd_run_vsim.cpp` | Routes FIRE results to `KernelEventLog`; wires `source_formula` from `.vsim` |
| `src/vsim/node_accessor.cpp` | Reads formation events from `KernelEventLog` for export/report path |
| `tests/test_formation_event_wiring.cpp` | 5 acceptance tests for FIRE→FormationEvent path (WO-56D-AuditChain) |
| `tests/test_formation_regimes_suite4.cpp` | Suite 4 regression coverage for formation regimes |

**Verdict: KEEP** — the formation event spine is functional and tested.

### 2.2 What is missing or broken

| Gap | Evidence | Severity |
|---|---|---|
| `DefectEvent` is never emitted in any `src/` file | `Select-String` on all `src/**/*.cpp` found zero `DefectEvent` constructor calls | CRITICAL |
| `sim_lattice.hpp` has rich `Defect`, `LatticeSite`, `LatticeState` structs but emits no events | No connection to `KernelEventLog` anywhere in `mission/` | HIGH |
| `FormationEvent` does not record child defects produced during relaxation | The struct has no `defect_ids` or `D_formation` link | MEDIUM |
| No defect → property bridge exists | `material_generator.cpp` is an offline fetcher; it does not consume formation defects | HIGH |
| No defect → report chain exists | `report_engine.hpp` does not have a defect section | MEDIUM |
| No defect → JSONL/TSV export exists | No `defect_events.jsonl` or `defect_summary.tsv` file is produced | HIGH |

---

## 3. Audit findings — defect detection

### 3.1 What exists

`include/mission/sim_lattice.hpp` — **the most complete defect model in the repo**:

```cpp
struct Defect {
	std::size_t site_index;
	physics::ParticleID type;          // vacancy/interstitial/etc via ParticleID ladder
	std::string element;
	double formation_energy_eV;
	double dose_eV_atom;
	std::array<double,3> displacement;
};

struct LatticeSite {
	bool occupied;      // false = vacancy
	double stress;      // local stress marker
	double damage;      // damage index [0, 1]
	int species_code;   // Z or negative defect token
	...
};
```

`include/kernel/kernel_event.hpp` — `DefectType` enum:
```
Vacancy, Interstitial, Substitution, FrenkelPair,
Antisite, StackingFault, GrainBoundary
```

`DefectEvent` struct has `defect_type`, `site_id`, `formation_energy`, `migration_energy`.

### 3.2 What is missing

| Capability | Status |
|---|---|
| Defect detection from MD trajectory (geometry + energy) | MISSING |
| Defect persistence tracking across frames | MISSING |
| Defect merge/split/disappear/intensify tracking | MISSING |
| Defect severity score (geometry + energy + stress + time) | MISSING — enum labels only |
| `sim_lattice.hpp` defects wired to `KernelEventLog::push(DefectEvent)` | MISSING |
| Vacancy inferred from coordination drop | MISSING |
| Interstitial inferred from local density spike | MISSING |
| Void / porosity from geometry | MISSING |
| Overcoordination / undercoordination detection | MISSING |

---

## 4. Defect-to-property bridge

Expected chain:

```
formation_events.jsonl
		↓
defect_events.jsonl            ← MISSING
		↓
defect_summary.tsv             ← MISSING
		↓
material_sampling_report.md    ← MISSING
```

| Defect | Property implication | Connected? |
|---|---|---|
| Vacancy | diffusion, density, creep | NO |
| Interstitial | strain, hardening, diffusion change | NO |
| Void | porosity, fracture initiation | NO |
| Surface defect | reactivity, corrosion, anisotropic transport | NO |
| Misbond | local instability, phase mismatch | NO |
| Grain boundary seed | strength, transport, failure pathway | NO |

**Chain status: BROKEN at step 2.** No defect events are emitted.

---

## 5. Formation event audit (per WO-BRIDGE-65B §5A)

| Question | Answer |
|---|---|
| Does formation emit an event? | YES — `FormationEvent` via FIRE exit path |
| Does it identify parent atoms? | YES — `source_formula`, `n_beads` |
| Does it store time window? | PARTIAL — `frame_id` stored, not start/end window |
| Does it store candidate confidence? | NO |
| Does it emit a product identity? | NO — no downstream identity assignment |
| Does it emit failure/rejection records? | YES — `converged=false`, `is_valid=false`, `warning` |

**Verdict: PARTIAL**

---

## 6. Defect emergence audit (per WO-BRIDGE-65B §5B)

| Question | Answer |
|---|---|
| Are defects detected from geometry? | PARTIAL — `LatticeSite.occupied` tracks vacancies |
| Are defects detected from energy? | PARTIAL — `Defect.formation_energy_eV` exists |
| Are defects detected from coordination? | NO |
| Are defects persistent across frames? | NO |
| Can the same defect be tracked over time? | NO |
| Can defects merge, split, or intensify? | NO |
| Are defect severities numerical? | PARTIAL — `LatticeSite.damage` [0,1] exists |

**Verdict: PARTIAL / STALE** — data model exists in `sim_lattice.hpp` but is never connected to runtime.

---

## 7. Output artifacts audit

| Expected artifact | Exists? | Connected? |
|---|---|---|
| `formation_events.jsonl` | Via KernelEventLog export | PARTIAL |
| `defect_events.jsonl` | NO | NO |
| `defect_summary.tsv` | NO | NO |
| `material_sampling_report.md` | NO | NO |

---

## 8. Code health findings

| File | Compiled? | Tested? | Called by runtime? | Verdict |
|---|---|---|---|---|
| `include/kernel/kernel_event.hpp` (FormationEvent) | YES | YES | YES | KEEP |
| `include/kernel/kernel_event.hpp` (DefectEvent) | YES (header) | NO | NO (src) | WIRE |
| `include/mission/sim_lattice.hpp` (Defect, LatticeSite) | YES (header) | NO | YES (mission) | WIRE |
| `src/cli/cmd_run_vsim.cpp` | YES | YES (indirectly) | YES | KEEP |
| `src/vsim/node_accessor.cpp` | YES | YES (indirectly) | YES | KEEP |
| `tests/test_formation_event_wiring.cpp` | YES | YES | N/A | KEEP |
| `src/ufx_auto2/material_generator.cpp` | YES | UNKNOWN | NO (fetcher only) | ARCHIVE |
| `src/ufx_auto2/materials_project_fetcher.cpp` | YES | UNKNOWN | NO (fetcher only) | ARCHIVE |

---

## 9. Truth-state doctrine check

**Rule:** Defects are analysis/event records — not fake atoms in truth-state files.

Current status: PASS.
`sim_lattice.hpp` `Defect` structs are separate from `LatticeSite` positions.
No defect markers are inserted into `.xyz` trajectory as ghost atoms.

---

## 10. Replay and hashing

| Question | Status |
|---|---|
| Can defect formation be reproduced from saved state? | UNKNOWN — no defect replay path |
| Are defect records tied to state hashes? | NO |
| Do defect records avoid corrupting truth state? | YES (by omission) |

---

## 11. Recommended patch queue

In priority order (see `bridge65b_patch_queue.md` for details):

| Priority | Action | Target file | Type |
|---|---|---|---|
| P1 | Wire `DefectEvent` emission from `sim_lattice` defect insertion | `include/mission/sim_lattice.hpp` + caller | WIRE |
| P2 | Add defect persistence tracking (frame-to-frame) | new `src/analysis/defect_tracker.cpp` | CREATE |
| P3 | Add defect JSONL export path | `src/vsim/node_accessor.cpp` | PATCH |
| P4 | Connect defect counts to material property sampler | `src/ufx_auto2/` or new bridge | WIRE |
| P5 | Add `FormationEvent.defect_ids[]` link | `include/kernel/kernel_event.hpp` | PATCH |
| P6 | Add defect severity score (geometry + energy + stress + time) | new analysis module | CREATE |

---

## 12. WO-BRIDGE-65B gate

```
[ ] DefectEvent emitted from lattice/trajectory analysis
[ ] Defect persistence tracking across frames
[ ] defect_events.jsonl produced
[ ] defect_summary.tsv produced
[ ] Defect → property chain connected
[ ] Defect records excluded from truth-state
[ ] Replay hash covers defect stream
```

These are **not** exit criteria for WO-BRIDGE-65. They are the opening
conditions for the next work order after the bridge closes.

---

*WO-BRIDGE-65B — VSEPR-SIM v5.1.13 — 2026-05-19*
