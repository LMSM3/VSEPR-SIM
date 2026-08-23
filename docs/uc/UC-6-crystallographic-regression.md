# UC-6 — Crystallographic Regression Suite ("The Golden Suite")
<!-- VSEPR-SIM | v5.0.14 | WO-MF-01 -->

---

## Overview

**Goal:** Run all 8 crystallographic validation scripts (`val_01`–`val_08`)
and compare every computed property against its known-good reference value
from the `[expected]` block — automatically, with tolerance gating.  Any
deviation beyond the published tolerance is a **regression**.

This is the foundational **science regression gate**: before any release or
significant refactor, the Golden Suite must pass with zero regressions.

---

## Actors

| Actor | Role |
|-------|------|
| Researcher / CI | Triggers `bash tests/automation/uc6_run.sh` |
| `vsepr.exe` | Runs each val script headlessly |
| `pipeline_records.json` | Per-run output carrying computed properties |
| Golden reference table | `[expected]` blocks embedded in each val script |

---

## Preconditions

1. `build/vsepr.exe` present.
2. All 8 `scripts/val_0*.vsim` files present (they are — committed to repo).
3. `jq` recommended (falls back to grep-based parsing if absent).

---

## Validation matrix

| Script | Material | Key properties validated | Tolerances |
|--------|----------|------------------------|------------|
| `val_01_caf2_fluorite.vsim` | CaF₂ | Madelung const 5.03879, CN(Ca)=8, CN(F)=4, charge\_sum=0 | ±0.001, exact, 1e-10 |
| `val_02_zns_polymorphs.vsim` | ZnS | Blende: M=1.6381; Wurtzite: M=1.6413, CN=4 each | ±0.001 |
| `val_03_rutile_tio2.vsim` | TiO₂ | CN(Ti)=6, CN(O)=3, Ti-O eq=1.946 Å, ap=1.984 Å | ±0.05 Å, ±5° |
| `val_04_alpha_quartz_sio2.vsim` | SiO₂ | CN(Si)=4, Si-O=1.609 Å, Si-O-Si=144°, ring=6 | ±0.05 Å, ±8° |
| `val_05_mfi_zeolite.vsim` | SiO₂ MFI | FW density=17.9 T/1000Å³, pore 5.6/5.1 Å | ±0.5, ±0.3 Å |
| `val_06_graphite_ab_stack.vsim` | C | Interlayer=3.354 Å, C-C=1.421 Å, anisotropy≥2.0 | ±0.05, ±0.02 |
| `val_07_fcc_argon.vsim` | Ar | a₀=5.256 Å, Ecoh=0.0873 eV/atom, RDF peak=3.721 Å | ±0.05, ±0.005 |
| `val_08_benzene_dimer_s22.vsim` | C₆H₆ | T-shaped: -0.1184 eV, r=5.0 Å; PD: -0.1206 eV, r=3.5 Å | ±0.015, ±0.3 Å |

---

## Workflow steps

```
1. For each val_NN script (01 through 08):
   a. vsepr run scripts/val_NN_*.vsim
   b. Locate output: out/val_NN_*/pipeline_records.json
   c. Extract computed values using jq / grep
   d. Compare against reference table below
   e. |computed - reference| ≤ tolerance → PASS
	  |computed - reference| > tolerance → FAIL (regression)
	  output absent                      → PENDING (MF-A06..A10 naming gap)

2. Aggregate: count PASS, FAIL, PENDING per property per material
3. Print regression table and exit non-zero if any FAIL
```

---

## Output structure per val run

The `vsepr run val_NN.vsim` produces (actual naming observed in UC-1):

| Actual file | Contains |
|-------------|---------|
| `out/val_NN/pipeline_records.json` | All computed analysis values |
| `out/val_NN/pipeline_dashboard.md` | Human-readable summary |
| `out/val_NN/{name}.xyz` | Geometry (for visual inspection) |
| `out/val_NN/run_manifest.json` | Artifact list |

The `[expected]` block values are the **golden reference** — they do not change
between runs on the same code version.

---

## Missing features exercised

| MF tag | Feature | Impact |
|--------|---------|--------|
| MF-A06 | Canonical `trajectory.xyz` name | XYZ named after project; cosmetic |
| MF-A07 | Canonical `analysis.json` | Values in `pipeline_records.json` instead |
| MF-D01 | Gap classifier consolidation | Affects fingerprint quality in val_04/val_05 |
| MF-D02 | Output filter hardening | Scale-layer gating for crystal results |
| MF-D03 | Length-scale fitter | Analytic cases for val_07 |

---

## Automation check sequence

Implemented in `tests/automation/uc6_run.sh`:

```
For each of 8 val scripts:
  STEP A: vsepr run scripts/val_NN_*.vsim
  STEP B: expect_file out/val_NN/pipeline_records.json
  STEP C: extract_and_check  madelung_constant  5.03879  0.001    (val_01)
		   extract_and_check  coordination_Ca    8        0        (val_01)
		   extract_and_check  cohesive_energy    0.0873   0.005    (val_07)
		   extract_and_check  binding_energy     -0.1184  0.015    (val_08 T)
		   ... (full table per script)
  STEP D: report PASS/FAIL/PENDING per check
  STEP E: aggregate into regression table
```

---

## Regression table format (console output)

```
Material         Property               Reference   Computed    Delta    Result
--------         --------               ---------   --------    -----    ------
CaF2             madelung_constant      5.03879     5.038xx     0.000x   PASS
CaF2             coordination_Ca        8           8           0        PASS
TiO2             bond_Ti_O_eq (Ang)     1.946       ?           ?        PENDING
Ar-FCC           cohesive_energy (eV)   0.0873      ?           ?        PENDING
...
```

---

## Acceptance criteria

| Criterion | Pass condition |
|-----------|---------------|
| Zero regressions | All FAIL count = 0 |
| PENDING count matches naming gap | Properties absent = xsim::xport not wired |
| All 8 val scripts run without crash | `vsepr run` exits 0 for each |
| Deterministic results | Seed-locked scripts produce same output on re-run |

---

## Development priority

When MF-A07 is resolved (canonical `analysis.json` from xsim::xport), the
property extraction will switch from `pipeline_records.json` to `analysis.json`
and the full validation matrix will be exercisable end-to-end.

---

## Fixture script

`tests/automation/vsim/uc6_golden_suite.vsim` — thin project-level dispatcher
that lists all 8 val scripts. Individual val scripts remain in `scripts/`.

---

*UC-6 | WO-MF-01 | VSEPR-SIM Desktop Kernel Legacy*
