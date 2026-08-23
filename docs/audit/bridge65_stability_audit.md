# Bridge65 Stability Audit
## Classical MD Stability Before Subatomic Implementation

| Field | Value |
|---|---|
| **Document** | `bridge65_stability_audit.md` |
| **Version** | VSEPR-SIM v5.1.13 |
| **WO** | WO-BRIDGE-65 |
| **Date** | 2026-05-19 |

---

## 1. Purpose

This document records the stability baseline for the classical MD engine
at the v5.1.13 freeze point, before the bridge layer adds event logging
and before subatomic sampling is considered.

---

## 2. Empirical data layer — status: FROZEN

| Check | Result |
|---|---|
| `data/elements.empirical.json` exists | PASS |
| Element count ≥ 90 | PASS — 118 elements |
| H ionization_energy_1_eV present and > 0 | PASS — 13.598 eV |
| Fe density_g_per_cm3 present and > 0 | PASS — 7.874 g/cm³ |
| O covalent_radius_pm present and > 0 | PASS — 66.0 pm |
| Source tags present on all elements | PASS |
| Audit report `docs/audit/chem_audit_empirical.md` present | PASS |
| Python verification suite `_verify_v5113.py` | 30/30 PASS |

**Empirical layer verdict: STABLE AND FROZEN**

---

## 3. Bridge65 data artifacts — status: GENERATED

| Artifact | Records | Status |
|---|---|---|
| `data/bridge65.force_matrix.json` | 118 elements | PASS |
| `data/bridge65.validation_cases.json` | 6 cases | PASS |
| `data/bridge65.material_seed_cases.json` | 3 seeds | PASS |

Bridge65 validation gate: **11/11 PASS**

Python test suite `tests/test_bridge65_empirical.py`: **52/52 PASS**

---

## 4. Formation event spine — status: WIRED AND TESTED

| Check | Result |
|---|---|
| `FormationEvent` emitted from FIRE exit path | PASS |
| `FormationEvent` carries `n_beads`, `fire_steps`, `converged` | PASS |
| `FormationEvent.converged == false` when `max_iterations = 1` | PASS |
| `KernelEventLog` accumulates multiple `FormationEvent` records | PASS |
| Event IDs are distinct (monotonic) | PASS |

Source: `tests/test_formation_event_wiring.cpp` — 5/5 tests pass.

---

## 5. DefectEvent spine — status: DEFINED, NOT YET WIRED

| Check | Result |
|---|---|
| `DefectEvent` struct defined in `kernel_event.hpp` | PASS |
| `DefectType` enum covers 7 types | PASS |
| `DefectEvent` emitted in any `src/` file | FAIL — zero emission sites |
| `sim_lattice.hpp` defects wired to `KernelEventLog` | FAIL — missing wire |
| `defect_events.jsonl` produced | FAIL — not yet |

**DefectEvent verdict: DEFINED — wiring deferred to post-bridge WO**
See `docs/audit/bridge65b_patch_queue.md` P1.

---

## 6. Bridge65 C++ implementation — status: CREATED

| File | Status |
|---|---|
| `include/vsim/bridge65/atom_event.hpp` | CREATED |
| `include/vsim/bridge65/event_limiter.hpp` | CREATED |
| `include/vsim/bridge65/atom_event_logger.hpp` | CREATED |
| `include/vsim/bridge65/bridge65_config.hpp` | CREATED |
| `src/bridge65/event_limiter.cpp` | CREATED |
| `src/bridge65/atom_event_logger.cpp` | CREATED |
| `src/bridge65/bridge65_runtime.cpp` | CREATED |

Build integration: pending CMakeLists.txt registration (next step).

---

## 7. Subatomic status

`Bridge65Config::subatomic_enabled = false` — **ENFORCED AT RUNTIME**.

Any attempt to construct `Bridge65Runtime` with `subatomic_enabled = true`
throws `std::logic_error`. This is not a default; it is a hard policy.

---

## 8. Known gaps (deferred, not blocking WO-BRIDGE-65)

| Gap | Severity | Deferred to |
|---|---|---|
| `DefectEvent` not emitted in any runtime path | HIGH | Post-bridge WO |
| No defect → property chain | HIGH | Post-bridge WO (P4) |
| No defect JSONL/TSV export | HIGH | Post-bridge WO (P3) |
| `FormationEvent` has no `defect_ids[]` link | MEDIUM | P5 |
| `ufx_auto2` material fetcher not connected | LOW | Archive WO |

---

## 9. Audit verdict

```
G_bridge65 bridge layer implementation = IN PROGRESS
G_MD (engine stability)                = PASS (existing test suite)
G_emp (empirical data)                 = PASS (11/11 validation)
G_matrix (force matrix)                = PENDING (benchmark cases defined)
G_events (event logging)               = PENDING (C++ code created, build pending)
G_limiter (event limiter)              = PENDING (C++ code created, build pending)
```

The bridge architecture is complete. The remaining work is CMakeLists
registration and benchmark execution.

---

*bridge65_stability_audit.md — VSEPR-SIM v5.1.13 — 2026-05-19*
