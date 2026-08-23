# UC-2 — Batch Crystal Sweep ("The Crystal Suite")
<!-- VSEPR-SIM | v5.0.14 | WO-MF-01 -->

---

## Overview

**Goal:** A researcher wants to explore how SiO₂ properties change across a
range of lattice parameters.  They write a single `[sweep]` block, run it
headlessly, and get back a `summary.csv` cross-referencing every run's
fingerprint, cluster assignment, and key metrics — ready to import into Python
for plotting.

This workflow exercises the **sweep runtime dispatcher** (MF-B01), the **export
writers** for cluster/fingerprint/CSV data (MF-A01/A02/A03), and the analysis
**gap classifier** (MF-D01/D02/D03).

---

## Actors

| Actor | Role |
|-------|------|
| Researcher | Authors the `.vsim` sweep script |
| `vsepr.exe` | Headless simulation CLI running the sweep |
| Sweep dispatcher | Runtime component that iterates `[sweep]` parameter grid |
| Export package | Writes per-run and aggregate output files |

---

## Preconditions

1. `build/vsepr.exe` present.
2. Test fixture: `tests/automation/vsim/uc2_sio2_sweep.vsim` exists.
3. `jq` available on PATH (for JSON validation steps).

---

## Workflow steps

```
1. Author writes uc2_sio2_sweep.vsim with:
   ├── [material] formula = "SiO2", prototype = "quartz"
   ├── [sweep]
   │     param      = "lattice_a"
   │     range      = [4.8, 4.9, 5.0, 5.1]          # 4 sweep points
   │     secondary  = "lattice_c"
   │     secondary_range = [5.3, 5.4, 5.5, 5.6]     # 4x4 = 16 runs
   ├── [run] mode = "minimize", max_steps = 200
   ├── [analysis.structure]
   │     compute_fingerprint = true
   │     cluster_method      = "dbscan"
   ├── [export]
   │     write_xyz              = true
   │     write_fingerprint_json = true    # MF-A02
   │     write_cluster_json     = true    # MF-A01
   │     write_summary_csv      = true    # MF-A03
   │     output_dir             = "out/uc2_sio2_sweep"

2. User runs:   vsepr run uc2_sio2_sweep.vsim
   └── [sweep] dispatcher iterates parameter grid               # MF-B01

3. Per run (16 times):
   ├── [material.*] sub-section generated with current params  # MF-B02
   ├── Minimization runs
   ├── Fingerprint computed
   ├── Cluster label assigned
   └── Run output written to out/uc2_sio2_sweep/run_NNN/

4. Aggregate export:
   ├── fingerprints.json  — all 16 fingerprint records        # MF-A02
   ├── clusters.json      — DBSCAN cluster assignments        # MF-A01
   ├── summary.csv        — param1, param2, energy, cluster, fp_hash  # MF-A03
   └── manifest.json      — file inventory

5. Researcher opens summary.csv in Excel / pandas:
   └── Plots energy surface across (lattice_a, lattice_c) space
```

---

## Expected outputs

| File | Source feature | MF tag | Status |
|------|---------------|--------|--------|
| `out/uc2_sio2_sweep/summary.csv` | `write_summary_csv` | MF-A03 | 🔴 PENDING |
| `out/uc2_sio2_sweep/fingerprints.json` | `write_fingerprint_json` | MF-A02 | 🔴 PENDING |
| `out/uc2_sio2_sweep/clusters.json` | `write_cluster_json` | MF-A01 | 🔴 PENDING |
| `out/uc2_sio2_sweep/manifest.json` | ManifestJsonWriter | — | 🟢 EXPECTED |
| `out/uc2_sio2_sweep/run_000/trajectory.xyz` | XYZWriter (per run) | — | Depends on MF-B01 |

---

## Missing features exercised

| MF tag | Feature | Automation result if absent |
|--------|---------|----------------------------|
| MF-A01 | `write_cluster_json` | `expect_pending` — PENDING |
| MF-A02 | `write_fingerprint_json` | `expect_pending` — PENDING |
| MF-A03 | `write_summary_csv` | `expect_pending` — PENDING |
| MF-B01 | `[sweep]` runtime | `expect_pending` — sweep may produce 0 runs |
| MF-B02 | `[material.*]` sub-sections | `expect_pending` — params not varied |
| MF-D01 | Gap classifier unification | Quality of cluster output affected |
| MF-D02 | Output filter hardening | Scale-layer gating missing |
| MF-D03 | Length-scale fitter | Analytic validation missing |
| MF-E04 | Sweep progress in desktop | RunVsimPanel has no step counter |

---

## Automation check sequence

Implemented in `tests/automation/uc2_run.sh`:

```
STEP 1  vsepr run uc2_sio2_sweep.vsim
STEP 2  expect_pending out/uc2_sio2_sweep/summary.csv
						"MF-A03: summary CSV writer"         MF-A03
STEP 3  expect_pending out/uc2_sio2_sweep/fingerprints.json
						"MF-A02: fingerprint JSON writer"    MF-A02
STEP 4  expect_pending out/uc2_sio2_sweep/clusters.json
						"MF-A01: cluster JSON writer"        MF-A01
STEP 5  expect_pending out/uc2_sio2_sweep/run_000/trajectory.xyz
						"MF-B01: sweep dispatcher"           MF-B01
STEP 6  if [sweep] dispatcher implemented:
		  expect_file  out/uc2_sio2_sweep/manifest.json   "Manifest present"
		  validate_json out/uc2_sio2_sweep/manifest.json  "Manifest valid JSON"
STEP 7  if summary.csv present:
		  validate_csv_columns summary.csv "lattice_a,lattice_c,energy,cluster_id,fp_hash"
STEP 8  count_sweep_runs out/uc2_sio2_sweep 16  "Expect 16 sweep points"
```

---

## Acceptance criteria

| Criterion | Pass condition |
|-----------|---------------|
| PENDING count matches ledger | 5 items pending (MF-A01/A02/A03/B01/B02) |
| No spurious FAILs | Implemented parts produce correct output |
| When sweep runs: 16 run dirs created | `run_000/` through `run_015/` present |
| CSV columns validated | `lattice_a`, `lattice_c`, `energy`, `cluster_id`, `fp_hash` present |

---

## Development priority sequence

To fully green-light UC-2, implement in this order:

1. **MF-B01** — `[sweep]` runtime dispatcher (creates run_NNN dirs, iterates params)
2. **MF-B02** — `[material.*]` sub-section generator (varies the actual parameter)
3. **MF-A02** — `write_fingerprint_json` writer (per run + aggregate)
4. **MF-A01** — `write_cluster_json` writer (DBSCAN labels)
5. **MF-A03** — `write_summary_csv` writer (cross-reference table)
6. **MF-D01/D02/D03** — gap classifier, output filter, length-scale fitter

Each item is a discrete 5-step checklist cycle (see `VSIM_DEVELOPMENT.md`).

---

## Fixture script

`tests/automation/vsim/uc2_sio2_sweep.vsim` — see that file.

---

*UC-2 | WO-MF-01 | VSEPR-SIM Desktop Kernel Legacy*
