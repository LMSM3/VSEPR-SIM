# UC-3 — Isomer Discovery Pipeline ("The Chirality Hunt")
<!-- VSEPR-SIM | v5.0.14 | WO-MF-01 -->

---

## Overview

**Goal:** A researcher specifies a coordination formula (`[Co(NH3)4Cl2]+`) and
wants to enumerate all distinct isomers, relax each with MD, classify them by
geometry and chirality, and receive a ranked output table showing which isomer is
most stable and which are enantiomers.

This is the **most complex** of the three use cases.  It exercises the entire
pending isomer subsystem (MF-C01 through MF-C04) and the formation library
runtime (MF-B04).

---

## Actors

| Actor | Role |
|-------|------|
| Researcher | Writes the isomer discovery script |
| `vsepr.exe` | Headless CLI running generation + MD |
| Isomer generator | Enumerates distinct starting geometries (`[generator.isomers]`) |
| MD relaxer | Minimizes each isomer structure |
| Isomer tracker | Tracks structural identity across trajectory (`[analysis.isomer_tracking]`) |
| Chirality detector | Labels `GeometricVariant` / `StereoVariant` per structure |
| Ranked reporter | Writes final sorted table |

---

## Preconditions

1. `build/vsepr.exe` present.
2. Test fixture: `tests/automation/vsim/uc3_cobalt_isomers.vsim` exists.
3. `jq` available for JSON validation.

---

## Workflow steps

```
1. Researcher writes uc3_cobalt_isomers.vsim:
   ├── [material]
   │     formula   = "[Co(NH3)4Cl2]+"
   │     prototype = "coordination_complex"
   │     phase     = "solution"
   │
   ├── [generator.isomers]                   # MF-C01
   │     method           = "permutation"
   │     max_isomers      = 8
   │     symmetry_prune   = true
   │     detect_chirality = true             # MF-C03
   │
   ├── [run]
   │     mode      = "md"
   │     max_steps = 500
   │     dt_fs     = 0.5
   │     converge  = true
   │
   ├── [analysis.isomer_tracking]            # MF-C02
   │     rmsd_threshold   = 0.15
   │     track_chirality  = true
   │     output_trajectory = true
   │
   ├── [export]
   │     write_xyz            = true
   │     write_isomer_report  = true         # MF-C04
   │     write_analysis_json  = true
   │     output_dir           = "out/uc3_cobalt_isomers"

2. vsepr run uc3_cobalt_isomers.vsim
   ├── Isomer generator enumerates up to 8 structures   # MF-C01
   ├── Symmetry pruning removes duplicates
   └── Chirality flagged on each candidate              # MF-C03

3. Per isomer (N ≤ 8):
   ├── MD run, max_steps = 500
   ├── RMSD tracking vs isomer_000 reference
   ├── Identity preserved / merged if RMSD < threshold  # MF-C02
   └── Output: out/uc3_cobalt_isomers/isomer_NNN/trajectory.xyz

4. Post-run analysis:
   ├── Energy-ranked table built
   ├── Chirality / geometric variant labels applied      # MF-C03
   └── isomer_report.json written                       # MF-C04

5. Export:
   ├── isomer_report.json    — ranked table
   ├── analysis.json         — RMSD, energy, chirality per isomer
   ├── manifest.json         — file inventory
   └── (per isomer dir): trajectory.xyz, events.json
```

---

## Expected outputs

| File | Source feature | MF tag | Status |
|------|---------------|--------|--------|
| `out/uc3_cobalt_isomers/isomer_report.json` | Isomer ranked reporter | MF-C04 | 🔴 PENDING |
| `out/uc3_cobalt_isomers/analysis.json` | AnalysisJsonWriter + tracker | MF-C02 | 🔴 PENDING |
| `out/uc3_cobalt_isomers/isomer_000/trajectory.xyz` | XYZWriter | MF-C01 needed | 🔴 PENDING |
| `out/uc3_cobalt_isomers/manifest.json` | ManifestJsonWriter | — | 🟢 if run completes |

---

## Missing features exercised

| MF tag | Feature | Automation result if absent |
|--------|---------|----------------------------|
| MF-C01 | `[generator.isomers]` runtime | `expect_pending` — no isomers generated |
| MF-C02 | Isomer tracking trajectory walker | `expect_pending` — no RMSD-merged tracking |
| MF-C03 | Geometric / chirality detection | `expect_pending` — labels absent in report |
| MF-C04 | Isomer-ranked output report | `expect_pending` — no isomer_report.json |
| MF-B04 | Formation libraries runtime | `expect_pending` — formation library not resolved |
| MF-E03 | MCF-CAI state vector | `expect_pending` — phase 3+ not started |

---

## Automation check sequence

Implemented in `tests/automation/uc3_run.sh`:

```
STEP 1  vsepr run uc3_cobalt_isomers.vsim
STEP 2  expect_pending out/uc3_cobalt_isomers/isomer_000/trajectory.xyz
						"MF-C01: isomer generator"           MF-C01
STEP 3  expect_pending out/uc3_cobalt_isomers/isomer_report.json
						"MF-C04: isomer ranked report"       MF-C04
STEP 4  expect_pending out/uc3_cobalt_isomers/analysis.json
						"MF-C02: isomer tracking analysis"   MF-C02
STEP 5  if isomer_report.json present:
		  validate_json  out/uc3_cobalt_isomers/isomer_report.json
		  check_json_key out/uc3_cobalt_isomers/isomer_report.json ".isomers[0].chirality"
						 "MF-C03: chirality label present"   MF-C03
		  check_json_key out/uc3_cobalt_isomers/isomer_report.json ".isomers | length"
						 "At least 1 isomer in report"
		  check_sorted   out/uc3_cobalt_isomers/isomer_report.json ".isomers" "energy"
						 "Isomers sorted by energy"
STEP 6  count_isomer_dirs out/uc3_cobalt_isomers 1 8  "Between 1 and 8 isomer dirs"
STEP 7  if manifest.json present:
		  validate_json  out/uc3_cobalt_isomers/manifest.json
```

---

## Acceptance criteria

| Criterion | Pass condition |
|-----------|---------------|
| PENDING count matches ledger | 4 items pending (MF-C01/C02/C03/C04) |
| No spurious FAILs | Any implemented subset produces correct partial output |
| When generator runs: 1–8 isomers | Dirs `isomer_000/` through `isomer_NNN/` |
| Report sorted by energy | `isomers[0].energy ≤ isomers[1].energy` |
| Chirality labels valid | Values in `{R, S, achiral, unclassified}` |

---

## Development priority sequence

To fully green-light UC-3, implement in this order:

1. **MF-C01** — `[generator.isomers]` engine (permutation + symmetry prune)
2. **MF-C03** — Geometric / chirality detector (VSEPR angle analysis + handedness)
3. **MF-C02** — Trajectory walker for isomer tracking (RMSD windowed merge)
4. **MF-C04** — Ranked report writer (energy-sorted JSON with chirality labels)
5. **MF-B04** — Formation library executor (feeds initial geometries)
6. **MF-E03** — MCF-CAI integration (phase 3+, post WO-75A)

Each item follows the **5-step developer checklist** (`VSIM_DEVELOPMENT.md`).

---

## Scientific context

`[Co(NH3)4Cl2]+` is a classic coordination-chemistry isomer pair:

| Isomer | Geometry | Chirality |
|--------|----------|-----------|
| *cis*-[Co(NH3)4Cl2]+ | Cl axial adjacent | Achiral (meso) |
| *trans*-[Co(NH3)4Cl2]+ | Cl axial opposite | Achiral |
| *fac*-[Co(NH3)3Cl3] (if 3 Cl) | Facial | Chiral pair (R/S) |

The test uses 4 NH3 + 2 Cl so that cis/trans distinction is the primary
output.  Chirality detection is exercised on the cis isomer to ensure the
`achiral` label (not a false `R` or `S`) is returned.

---

## Fixture script

`tests/automation/vsim/uc3_cobalt_isomers.vsim` — see that file.

---

*UC-3 | WO-MF-01 | VSEPR-SIM Desktop Kernel Legacy*
