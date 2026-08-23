# WO-BRIDGE-65
## Stable Classical-MD / Python-Empirical Bridge Before Subatomic Sampling

| Field | Value |
|---|---|
| **Work Order** | WO-BRIDGE-65 |
| **Version** | VSEPR-SIM v5.1.13 stable |
| **Branch** | v5.0.0-main |
| **Phase** | Post-classical MD stabilisation · Pre-subatomic CAI implementation |
| **Status** | COMPLETE |
| **Date opened** | 2026-05-19 |

---

## 1. Purpose

WO-BRIDGE-65 freezes the current system as:

```
Stable Classical MD
+ Python Empirical Layer (Pillar F, v5.1.3)
+ Matrix Force Policy
+ Per-Atom Event Logging
```

before adding:

```
Subatomic Sampling  (reserved — disabled)
```

The bridge proves that the current engine can:

1. Load empirical chemistry data deterministically.
2. Run classical MD and reproduce trajectory state hashes.
3. Compare matrix-force behaviour against the existing force path within tolerance.
4. Print and export per-atom energy events.
5. Detect collision, bonding, and unbonding events.
6. Limit and sample output during extreme event rates.
7. Export clean artifacts for future subatomic CAI validation.

Subatomic work is **not** quietly folded into classical MD. It has its own gate.

---

## 2. Bridge Architecture

```
B_65 = [ E_py, F_MD, M_force, L_atom, S_limiter, V_gate, A_artifacts ]
```

| Symbol | Meaning |
|---|---|
| `E_py` | Python empirical chemistry scripts |
| `F_MD` | Stable classical MD force engine |
| `M_force` | Matrix-force policy layer |
| `L_atom` | Per-atom energy/collision/bond event logger |
| `S_limiter` | High-rate event limiter/sampler |
| `V_gate` | Validation gate before subatomics |
| `A_artifacts` | JSONL, TSV, report, checkpoint, trajectory outputs |

---

## 3. New Files

### C++ headers
```
include/vsim/bridge65/atom_event.hpp
include/vsim/bridge65/event_limiter.hpp
include/vsim/bridge65/atom_event_logger.hpp
include/vsim/bridge65/bridge65_config.hpp
```

### C++ sources
```
src/bridge65/atom_event_logger.cpp
src/bridge65/event_limiter.cpp
src/bridge65/bridge65_runtime.cpp
```

### Python pillars
```
pykernel/pillars/bridge65_empirical.py
pykernel/pillars/bridge65_validate.py
pykernel/pillars/bridge65_matrix_export.py
```

### Data artifacts
```
data/bridge65.force_matrix.json
data/bridge65.validation_cases.json
data/bridge65.material_seed_cases.json
```

### Tests
```
tests/test_bridge65_empirical.py
tests/test_bridge65_matrix_force.cpp
tests/test_bridge65_atom_events.cpp
tests/test_bridge65_event_limiter.cpp
tests/test_bridge65_replay_hash.cpp
```

### Docs
```
docs/wo/WO-BRIDGE-65.md               (this file)
docs/audit/bridge65_stability_audit.md
docs/benchmarks/BENCH-BRIDGE-65.md
docs/theory/bridge65_pre_subatomic_policy.md
```

---

## 4. Per-Atom Event Schema

Every logged atom event carries:

```
E_i(t) = [ id_i, t, K_i, U_i, E_i_total, dE_i, F_i, C_i, B_i, R_i ]
```

### Event types
```
energy_update
collision
bond_created
bond_broken
bond_stretched
force_spike
thermal_spike
energy_anomaly
```

### Console format
```
[ATOM_EVENT] step=1240 t=1.240e-12 id=42 sym=Fe event=collision
  E_k=2.14e-2 eV E_u=-4.81e-1 eV dE=8.20e-3 eV
  |F|=1.92e+1 eV/A partner=77 r=2.18 A gate=pass
```

### JSONL schema
```json
{
  "schema": "vsepr.atom_event.v1",
  "version": "5.1.13",
  "run_id": "run_0001",
  "step": 1240,
  "time_s": 1.24e-12,
  "atom": { "id": 42, "symbol": "Fe", "class": "atom" },
  "event": { "type": "collision", "partner_ids": [77],
			 "reason": "distance_energy_gate", "confidence": 0.993 },
  "energy": { "kinetic_eV": 0.0214, "potential_local_eV": -0.481,
			  "total_local_eV": -0.4596, "delta_eV": 0.0082 },
  "force": { "fx_eV_A": 3.1, "fy_eV_A": -8.2, "fz_eV_A": 16.7,
			 "magnitude_eV_A": 19.2 },
  "geometry": { "partner_distance_A": 2.18, "cutoff_A": 2.35 },
  "hash": { "state_hash": "...", "event_hash": "..." }
}
```

---

## 5. High-Rate Limiter

```
R_event = N_events / dt_wall

If R_event > R_max → S_limiter = 1
```

Limiter console summary:
```
[EVENT_LIMITER] step=18400 rate=91200 events/s mode=sampled
  total_events=4812 sampled_console=25
  collisions=4301 bond_created=82 bond_broken=19 energy_anomaly=3 force_spike=7
  full_jsonl=true aggregate_tsv=true
```

**Rule:** Console output may be sampled. Artifact output must remain scientifically useful.

---

## 6. Benchmark Cases

| Case | Purpose |
|---|---|
| `bench_bridge65_argon_lj` | Collision logging without bonds |
| `bench_bridge65_water_bonding` | Bond/unbond event sanity |
| `bench_bridge65_nacl_collision` | Ionic collision/force matrix |
| `bench_bridge65_fe_lattice` | Material sampling seed |
| `bench_bridge65_high_rate_collision_box` | Limiter/sampler stress |
| `bench_bridge65_bond_breaking_stress` | Unbond detection |

---

## 7. Validation Gates

| Metric | Gate |
|---|---|
| Force agreement | ≥ 0.999 |
| Energy agreement | ≥ 0.999 |
| Position agreement (short run) | ≥ 0.999 |
| Replay hash (no logging) | exact |
| Replay hash (passive logging) | exact or documented no-state-change |
| Runtime overhead (normal logging) | ≤ 10% |
| Runtime overhead (limiter active) | ≤ 20% |
| Event accounting consistency | hard pass |

State hash must ignore log counters.
Event hash must include event stream.

---

## 8. Exit Criteria

```
[ ] v5.1.13 classical MD remains stable
[ ] Python empirical scripts load and freeze cleanly
[ ] Matrix force policy matches existing MD path within tolerance
[ ] Per-atom energy events print and export
[ ] Collision events print and export
[ ] Bonding events print and export
[ ] Unbonding events print and export
[ ] High-rate limiter activates correctly
[ ] Sampler preserves representative event statistics
[ ] Full JSONL archive available when configured
[ ] Replay state hash stable
[ ] Event hash changes only with event stream changes
[ ] Subatomic module remains disabled
```

Gate product: `G_bridge65 = G_MD · G_emp · G_matrix · G_events · G_limiter = 1`

No partial credit. This is infrastructure.

---

## 9. Subatomic Status

**DISABLED.** Reserved for the next work order after WO-BRIDGE-65 closes.
The `[bridge65]` config block includes `subatomic_enabled = false` as an explicit
policy assertion, not a default that someone might accidentally enable.

---

*WO-BRIDGE-65 — VSEPR-SIM v5.1.13 — 2026-05-19*
