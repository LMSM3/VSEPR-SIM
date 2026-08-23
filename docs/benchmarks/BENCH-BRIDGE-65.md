# BENCH-BRIDGE-65
## Bridge65 Benchmark Suite Reference

| Field | Value |
|---|---|
| **Document** | `BENCH-BRIDGE-65.md` |
| **Version** | VSEPR-SIM v5.1.13 |
| **WO** | WO-BRIDGE-65 |

---

## 1. Purpose

The bridge65 benchmark suite validates that:

1. The classical MD engine remains numerically stable through the bridge layer.
2. The matrix-force policy agrees with the existing force path within tolerance.
3. The per-atom event logger does not perturb simulation state.
4. The high-rate event limiter activates and suppresses correctly.
5. Replay state hashes are invariant to logging configuration.

---

## 2. Gate thresholds

| Metric | Gate |
|---|---|
| Force agreement (Pearson r) | ≥ 0.999 |
| Energy agreement | ≥ 0.999 |
| Position agreement (short run) | ≥ 0.999 |
| Replay hash (no logging) | exact |
| Replay hash (passive logging) | exact or documented no-state-change |
| Runtime overhead (normal logging) | ≤ 10% |
| Runtime overhead (limiter active) | ≤ 20% |
| Event accounting consistency | hard pass |

---

## 3. Benchmark cases

### bench_bridge65_argon_lj

**Purpose:** Collision logging without bonds. Simplest possible test case.

```
Species: Ar (LJ, sigma=3.40 Å, eps=0.0104 eV)
Geometry: Two Ar atoms at 3.76 Å separation (LJ equilibrium)
Expected: Net force ≈ 0 eV/Å. No bond events. Collision rate ≈ 0.
Logger: collision_events=true, bond_events=false
Gate: force_magnitude < 0.05 eV/Å on both atoms
```

### bench_bridge65_water_bonding

**Purpose:** Bond/unbond event sanity on a minimal molecule.

```
Species: H2O (toy bond model)
Geometry: O at origin, H at ±(0.757, 0.586, 0.0) Å
Expected: 2 bond_created events at step 0.
Logger: bond_events=true, unbond_events=true
Gate: bond_created events emitted for both O-H pairs
```

### bench_bridge65_nacl_collision

**Purpose:** Ionic collision and force matrix agreement.

```
Species: Na+, Cl- (Coulomb + LJ)
Geometry: 2.36 Å separation (NaCl equilibrium)
Expected: Attractive Coulomb force ≈ 6.0 eV/Å at this distance.
Matrix gate: pearson_r(F_matrix, F_md) ≥ 0.999
```

### bench_bridge65_fe_lattice

**Purpose:** Material sampling seed. Force matrix on a BCC Fe 2×2×2 cell.

```
Species: Fe (BCC, a=2.87 Å)
Atoms: 8 (2×2×2)
Expected: Symmetric force distribution. Max force < 0.1 eV/Å at equilibrium.
Matrix gate: force_agreement ≥ 0.999
Logger: per_atom_energy=true, collision_events=true
```

### bench_bridge65_high_rate_collision_box

**Purpose:** Stress test for event limiter and sampler.

```
Species: Ar (50 atoms)
Box: 10×10×10 Å (highly compressed)
Temperature: 2000 K
Expected: >1000 collision events/step → limiter activates
Gate: limiter.active == true after first step
Gate: sampled_console < max_console_events_per_step
Gate: full JSONL archive written
```

### bench_bridge65_bond_breaking_stress

**Purpose:** Bond-broken event detection during stretch.

```
Species: H2 (stretched from 0.74 Å to 2.50 Å)
Break threshold: 1.65 × r_eq = 1.65 × 0.74 = 1.22 Å
Expected: bond_broken event emitted when r > 1.22 Å
Gate: event type == bond_broken in JSONL
Gate: event reason == stretch_threshold
```

---

## 4. Aggregate TSV format

When limiter activates, the event_summary.tsv captures:

```
step	time_s	total_events	collisions	bond_created	bond_broken
energy_anomaly	force_spike	mean_dE_eV	max_dE_eV	mean_force_eV_A
max_force_eV_A	limiter_active
```

Example (bench_bridge65_high_rate_collision_box, step 18400):
```
18400	1.84e-11	4812	4301	82	19	3	7	0.0021	1.92	4.41	88.2	true
```

---

## 5. Replay hash test

```
Run A: no logger          → state_hash_A
Run B: passive logger on  → state_hash_B

Required: state_hash_A == state_hash_B
```

Event hash test:

```
Run C: no events          → event_hash_C
Run D: 10 collision events → event_hash_D

Required: event_hash_C != event_hash_D
```

---

## 6. Runtime overhead measurement

```
t_baseline = wall time for N steps, no logger
t_logger   = wall time for N steps, logger active
t_limiter  = wall time for N steps, limiter active

overhead_normal  = (t_logger  - t_baseline) / t_baseline
overhead_limiter = (t_limiter - t_baseline) / t_baseline

Gate: overhead_normal  ≤ 0.10
Gate: overhead_limiter ≤ 0.20
```

---

## 7. Required output artifacts

After all bench cases pass:

```
events/atom_events.jsonl       — full event archive
events/event_summary.tsv       — per-step aggregate
reports/bridge65_report.md     — human-readable summary
state/run.xyzf                 — trajectory
state/run.xyzc                 — checkpoint
```

---

*BENCH-BRIDGE-65 — VSEPR-SIM v5.1.13 — 2026-05-19*
