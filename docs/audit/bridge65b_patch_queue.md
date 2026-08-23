# WO-BRIDGE-65B — Patch Queue

Ordered by priority. Nothing here is immediate work for WO-BRIDGE-65.
These are the **opening conditions** for the work order that follows after
the bridge gate closes.

---

## P1 — Wire DefectEvent emission from sim_lattice

**Type:** WIRE  
**Target:** `include/mission/sim_lattice.hpp` + calling code in `src/mission/`  
**Status:** OPEN

**What to do:**
After `LatticeState::defects` is populated (by `insert_defect()` or MD analysis),
emit a `KernelEventLog::push(DefectEvent)` for each detected defect.

Minimum DefectEvent fields to populate:
```cpp
DefectEvent de;
de.defect_type       = /* map from ParticleID to DefectType */;
de.site_id           = static_cast<int>(site.index);
de.formation_energy  = defect.formation_energy_eV * 23.06; // eV → kcal/mol
de.host_element      = site.element;
de.frame_id          = current_step;
de.source_formula    = state.formula;
de.compute();
log.push(de);
```

**Gate:** DefectEvent must appear in `KernelEventLog` during a lattice run.

---

## P2 — Defect persistence tracker

**Type:** CREATE  
**Target:** new `src/analysis/defect_tracker.cpp` + `include/analysis/defect_tracker.hpp`  
**Status:** OPEN

**What to do:**
Track defects across frames. A defect persists if it remains within a cutoff
distance of the previous-frame defect site. Assign stable defect IDs.

Output:
```
defect_id: "def_000001"
type: vacancy
first_frame: 100
last_frame: 240
persistence_steps: 140
site_trajectory: [(4.2, 1.1, 7.9), ...]
severity_max: 0.82
```

**Gate:** Defect IDs must be stable across frames; merge/split events logged.

---

## P3 — Defect JSONL and TSV export

**Type:** PATCH  
**Target:** `src/vsim/node_accessor.cpp` + report path  
**Status:** OPEN

**What to do:**
Add a `DefectEvent` read loop alongside the existing `FormationEvent` loop.
Emit:
- `events/defect_events.jsonl` — one line per defect event
- `events/defect_summary.tsv` — per-step aggregate

JSONL schema (consistent with WO-BRIDGE-65 atom_event schema):
```json
{
  "schema": "vsepr.defect_event.v1",
  "version": "5.1.13",
  "step": 1402,
  "defect_id": "def_000018",
  "formation_id": "form_000421",
  "defect_type": "vacancy",
  "position_A": [4.2, 1.1, 7.9],
  "severity": 0.82,
  "confidence": 0.971,
  "evidence": {
	"coordination_drop": 2,
	"local_density_ratio": 0.61,
	"energy_residual_eV": 0.38
  },
  "state_hash": "..."
}
```

TSV columns:
```
step  time_s  n_vacancy  n_interstitial  n_strain  n_misbond  mean_severity  max_severity
```

**Gate:** Both files present and non-empty after a lattice run.

---

## P4 — Defect → material property sampler bridge

**Type:** WIRE  
**Target:** `src/ufx_auto2/material_generator.cpp` (or new `src/analysis/defect_property_bridge.cpp`)  
**Status:** OPEN — depends on P3

**What to do:**
Map defect counts to material property estimates:

| Defect | Property | Formula |
|---|---|---|
| Vacancy density `n_v` | Diffusion multiplier | `D_eff = D_0 * (1 + alpha * n_v)` |
| Void fraction `f_void` | Porosity estimate | `phi = f_void / V_cell` |
| Strain band count | Fracture risk flag | `risk = strain_bands > threshold` |
| Misbond count | Phase mismatch flag | `mismatch = n_misbond > 0` |

Output: new section in `material_sampling_report.md`.

**Gate:** Defect section present in report with at least vacancy and void entries.

---

## P5 — FormationEvent.defect_ids[] field

**Type:** PATCH  
**Target:** `include/kernel/kernel_event.hpp`  
**Status:** OPEN — depends on P1

**What to do:**
Add a `std::vector<uint64_t> defect_ids;` field to `FormationEvent` so that
the defects produced during a formation event are traceable from the parent
formation record.

This closes the audit trail:
```
formation_id → defect_ids[] → DefectEvent records
```

**Gate:** `test_formation_event_wiring.cpp` extended test confirms field populated.

---

## P6 — Defect severity score

**Type:** CREATE  
**Target:** new `include/analysis/defect_severity.hpp`  
**Status:** OPEN — depends on P1

**What to do:**
Implement:
```
S_d = w_r * R_d + w_E * E_d + w_sigma * sigma_d + w_tau * tau_d
```

Default weights: `w_r=0.25, w_E=0.35, w_sigma=0.25, w_tau=0.15`

Output: `double compute_severity(const Defect& d, const LatticeSite& site, uint64_t tau)`

**Gate:** Severity in [0, 1] for all defect types; unit test verifies boundary cases.

---

*WO-BRIDGE-65B patch queue — VSEPR-SIM v5.1.13 — 2026-05-19*
