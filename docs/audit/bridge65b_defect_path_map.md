# WO-BRIDGE-65B — Defect Path Map

## Current actual path

```
FIRE relaxation (src/sim/optimizer.cpp)
		│
		▼
FormationEvent emitted → KernelEventLog::push()
		│
		▼
node_accessor.cpp reads FormationEvent
		│
		▼
Export (JSONL / report)   ← formation only, no defects
```

## Gap: where DefectEvent should enter

```
sim_lattice.hpp LatticeState::defects[]
		│   (currently never connected)
		│
		▼ [MISSING WIRE P1]
DefectEvent constructed → KernelEventLog::push(DefectEvent)
		│
		├──→ defect_events.jsonl   [MISSING P3]
		│
		├──→ defect_summary.tsv    [MISSING P3]
		│
		├──→ material property sampler  [MISSING P4]
		│       (vacancy → diffusion, void → porosity, etc.)
		│
		└──→ bridge65_report.md    [MISSING P3]
```

## Required target path

```
Formation event (FormationEvent, frame_id, n_beads, converged)
		│
		├──→ Defect detection (geometry + energy + coordination)
		│         vacancy:      LatticeSite.occupied == false
		│         interstitial: local density spike
		│         strain:       LatticeSite.stress threshold
		│         misbond:      coordination residual
		│
		▼
DefectEvent (defect_type, site_id, formation_energy, severity, state_hash)
		│
		├──→ KernelEventLog
		│
		├──→ defect_events.jsonl
		│
		├──→ defect_summary.tsv
		│         step, n_vacancy, n_interstitial, n_strain, n_misbond,
		│         mean_severity, max_severity
		│
		├──→ material_sampling_report.md
		│         vacancy_density → diffusion estimate
		│         void_fraction → porosity estimate
		│         strain_bands → fracture risk
		│
		└──→ bridge65_report.md (defect section)
```

## Defect severity score formula

```
S_d = w_r * R_d + w_E * E_d + w_sigma * sigma_d + w_tau * tau_d
```

where:
- `R_d`     = geometric residual (Å from ideal site)
- `E_d`     = local energy anomaly (eV)
- `sigma_d` = local strain/stress proxy (GPa)
- `tau_d`   = persistence time (steps)
- `S_d`     = defect severity score ∈ [0, 1]

## State hash rule for defect records

```
state_hash = hash(positions + forces)     -- must NOT include defect log counters
event_hash = hash(defect_event_stream)    -- changes when defects change
```

Same invariant as WO-BRIDGE-65 atom events:
a simulation does not change because you looked at the defects.

---

*WO-BRIDGE-65B — VSEPR-SIM v5.1.13 — 2026-05-19*
