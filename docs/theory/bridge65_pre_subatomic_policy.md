# Bridge65 Pre-Subatomic Policy
## Why Subatomic Sampling Is Separated From Classical MD

| Field | Value |
|---|---|
| **Document** | `bridge65_pre_subatomic_policy.md` |
| **Version** | VSEPR-SIM v5.1.13 |
| **WO** | WO-BRIDGE-65 |

---

## 1. The core separation principle

Classical MD describes:

- Atomic positions, velocities, forces
- Empirical potentials (LJ, Coulomb, bond/angle)
- Trajectory, energy, temperature, pressure

Subatomic sampling describes:

- Decay channels, branching ratios
- Running coupling constants `α_s(Q²)`, `α_em(Q²)`
- Scale-dependent force behaviour
- Quantum number conservation, threshold energies
- Gauge boson exchange at high momentum transfer

These are **not the same physics**. They operate at different energy scales,
require different formalisms, and have fundamentally different stability modes.

Mixing them without a gate would be like adding a particle accelerator to a
building before the building has walls. The walls are classical MD. The
accelerator is subatomic sampling.

---

## 2. What static force descriptions hide

A static force summary (textbook-style) gives:

```
Force: Coulomb → F = k q₁ q₂ / r²
```

This hides:

- Running coupling: `α_em(Q²) = α_em(0) / (1 - Δα(Q²))` — the coupling
  constant changes with momentum transfer.
- Threshold behaviour: weak force is contact-range at classical energies,
  but becomes long-range near the W/Z mass scale.
- Screening: Debye screening in classical MD is a phenomenological correction
  to Coulomb, not a true QED running coupling calculation.

At classical MD energies (room temperature, thermal velocities), these
effects are negligible. The static approximation is accurate.

At subatomic energies (keV+), running couplings, threshold effects, and
quantum corrections become dominant.

**Gate:** The classical MD engine must be validated at classical energies
before any subatomic energy regime is introduced.

---

## 3. Bridge65 as the validation gate

WO-BRIDGE-65 defines the gate conditions:

```
G_bridge65 = G_MD · G_emp · G_matrix · G_events · G_limiter = 1
```

All five factors must equal 1 before subatomic work begins.

| Factor | What it validates |
|---|---|
| `G_MD` | Classical MD trajectory reproducibility and replay hash stability |
| `G_emp` | Empirical chemistry data loaded, frozen, and validated (118 elements) |
| `G_matrix` | Matrix-force path agrees with classical MD path within ≥ 0.999 |
| `G_events` | Per-atom event logging does not perturb state hash |
| `G_limiter` | High-rate event limiter activates correctly, preserves JSONL archive |

---

## 4. What subatomic sampling will require (future WO)

When subatomic work begins, it will need:

1. A separate energy-scale router that activates only when particle energies
   exceed a configurable threshold (e.g., `E > 1 keV`).
2. Running coupling tables `α_s(Q²)` and `α_em(Q²)` as data, not hardcoded constants.
3. Decay channel branching ratios keyed by particle type and available energy.
4. A separate event type (`SubatomicEvent`) that extends the event spine but
   does not modify classical MD state hashes.
5. A clear truth-state doctrine: subatomic events are analysis/sampling
   records, not position updates on classical atoms.

None of these are present or active in v5.1.13. The `subatomic_enabled = false`
flag in the bridge config is an explicit policy assertion, enforced at runtime.

---

## 5. Why this cannot be quietly enabled

The `Bridge65Runtime` constructor enforces:

```cpp
if (cfg_.subatomic_enabled) {
	throw std::logic_error(
		"[WO-BRIDGE-65] subatomic_enabled=true is rejected. "
		"Subatomic sampling requires its own work order and gate.");
}
```

This is not a default that someone might accidentally leave enabled.
It is a hard rejection. The raccoon cannot enter the filing cabinet.

---

## 6. Policy summary

```
Classical MD scale:   thermal energies (meV–eV range)
					  empirical potentials, deterministic trajectories
					  fully validated by WO-BRIDGE-65

Subatomic scale:      keV–GeV range
					  running couplings, decay channels, gauge bosons
					  reserved — separate work order after G_bridge65 = 1
```

The bridge exists so that when subatomic work begins, it has a clean,
validated, artifact-rich classical baseline to compare against.

---

*bridge65_pre_subatomic_policy.md — VSEPR-SIM v5.1.13 — 2026-05-19*
