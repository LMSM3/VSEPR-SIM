# UC-7 — SiO₂ Pipe Simulation ("The Silicon Dioxide Pipe")
<!-- VSEPR-SIM | v5.0.14 | WO-MF-01 -->

---

## Overview

**Goal:** Simulate SiO₂ particulate flow through a cylindrical pipe using the
full SPH/DEM/FEA bridge stack — inlet source, wall surface, outlet sink,
Hertz-Mindlin contact model, and von Mises fatigue criterion on the pipe wall.
Produce flux records, energy traces, and a visual HTML dashboard.

This use-case resolves the `pipe_sph_dem_fea_bridge.vsim` template (which uses
unresolved `${var}` tokens) into a concrete, immediately runnable SiO₂ fixture
with fixed geometry and flow parameters grounded in realistic quartz/silica
engineering values.

---

## Actors

| Actor | Role |
|-------|------|
| Researcher | Runs the pipe simulation to study particle transport |
| `vsepr.exe` | Executes the resolved SPH/DEM/FEA bridge run |
| SPH solver | Fluid-phase carrier (N₂ gas at 300 K, 1 atm) |
| DEM bridge | Particle packing + Hertz-Mindlin contact |
| FEA bridge | Von Mises stress + fatigue assessment on SiO₂ wall |
| Export package | Produces flux JSON, energy SVG, HTML dashboard |

---

## Preconditions

1. `build/vsepr.exe` present.
2. Test fixture: `tests/automation/vsim/uc7_sio2_pipe.vsim` exists.
3. No `${var}` template tokens — all parameters resolved.

---

## SiO₂ pipe geometry

| Parameter | Value | Basis |
|-----------|-------|-------|
| `pipe.length` | 50.0 Å | ~5 nm nanofluidic tube |
| `pipe.inner_diameter` | 20.0 Å | Wide enough for ≥4 particles abreast |
| `pipe.outer_diameter` | 28.0 Å | 4 Å wall thickness (≈ 2 Si-O bond lengths) |
| `pipe.wall_thickness` | 4.0 Å | Thin-wall regime |
| `flow.species` | N₂ | Ambient carrier gas |
| `flow.inlet_rate` | 1.0e13 particles/s | Low-flux laminar regime |
| `flow.inlet_velocity` | 200.0 m/s | Subsonic; avoids shock formation |
| `sph.count` | 512 | Particle count for SPH carrier |
| Material | SiO₂ | formula = "SiO2", prototype = "quartz" |

---

## Workflow steps

```
1. Fixture resolves all pipe geometry and flow parameters
2. vsepr run uc7_sio2_pipe.vsim
   ├── PipeGeometry object constructed (length=50Å, ID=20Å, OD=28Å)
   ├── InletSource activated (N2 carrier, v=200 m/s, parabolic profile)
   ├── WallSurface binned (16 axial regions, axis_aligned)
   └── OutletSink armed (pressure=98000 Pa, record_flux=true)

3. Bridge execution:
   ├── DEMBridge: Hertz-Mindlin contacts, friction=0.35, restitution=0.20
   └── FEABridge: surface→mesh mapping, von Mises criterion, fatigue=true

4. Run: relax mode, 10 000 steps, dt=0.2 fs, T=300 K

5. Observe every 100 steps:
   ├── energy, formation, rdf metrics
   └── JSON observation records accumulated

6. Export:
   ├── {name}.xyz              -- particle trajectory (actual name)
   ├── pipeline_records.json  -- flux, energy, contact stats
   ├── pipeline_dashboard.md  -- convergence summary
   ├── run_manifest.json      -- artifact list
   ├── figures/pipe/          -- energy_trace.svg, html_dashboard.html
   └── (pending) xsim::xport canonical files: trajectory.xyz, analysis.json
```

---

## Expected outputs

| File | Source | MF tag | Status |
|------|--------|--------|--------|
| `out/uc7_sio2_pipe/{name}.xyz` | Core export | — | 🟢 EXPECTED |
| `out/uc7_sio2_pipe/pipeline_records.json` | Pipeline | — | 🟢 EXPECTED |
| `out/uc7_sio2_pipe/pipeline_dashboard.md` | Pipeline | — | 🟢 EXPECTED |
| `out/uc7_sio2_pipe/run_manifest.json` | Manifest | — | 🟢 EXPECTED |
| `out/uc7_sio2_pipe/figures/pipe/energy_trace.svg` | Visual export | MF-G01 | 🔴 PENDING |
| `out/uc7_sio2_pipe/figures/pipe/html_dashboard.html` | Visual export | MF-G02 | 🔴 PENDING |
| `out/uc7_sio2_pipe/pipeline_audit.jsonl` | Audit log | MF-G03 | 🔴 PENDING |

---

## Physics validation checks

| Check | Target | Tolerance | Source |
|-------|--------|-----------|--------|
| Outlet flux recorded | `record_flux = true` field present in records | — | run_manifest |
| Energy trace monotonic decrease | Final energy < initial energy | — | pipeline_records |
| Particle count conserved | inlet − outlet = in-flight count | ±2% | flux records |
| Wall region count | 16 axial bins present | exact | pipeline_records |
| Convergence reached | `converge = true` → run did not hit max_steps | — | pipeline_records |

---

## Missing features exercised (new MF-G series)

| MF tag | Feature | Automation result if absent |
|--------|---------|----------------------------|
| MF-G01 | `write_energy_trace_svg` visual export | `expect_pending` |
| MF-G02 | `write_html_dashboard` visual export | `expect_pending` |
| MF-G03 | `write_pipeline_audit_jsonl` | `expect_pending` |
| MF-G04 | DEMBridge runtime (SPH/DEM coupling) | `expect_pending` if no contact stats |
| MF-G05 | FEABridge von Mises / fatigue output | `expect_pending` if no stress records |

---

## Automation check sequence

Implemented in `tests/automation/uc7_run.sh`:

```
STEP 1  vsepr run uc7_sio2_pipe.vsim
STEP 2  expect_file  out/uc7_sio2_pipe/{name}.xyz          "Particle trajectory XYZ"
STEP 3  expect_file  out/uc7_sio2_pipe/pipeline_records.json "Pipeline records"
STEP 4  expect_file  out/uc7_sio2_pipe/pipeline_dashboard.md "Dashboard markdown"
STEP 5  expect_file  out/uc7_sio2_pipe/run_manifest.json    "Run manifest"
STEP 6  expect_pending figures/pipe/energy_trace.svg        "MF-G01: energy SVG"
STEP 7  expect_pending figures/pipe/html_dashboard.html     "MF-G02: HTML dashboard"
STEP 8  expect_pending pipeline_audit.jsonl                  "MF-G03: audit log"
STEP 9  check_flux_recorded  out/uc7_sio2_pipe/pipeline_records.json
STEP 10 check_energy_decreasing  out/uc7_sio2_pipe/pipeline_records.json
STEP 11 check_wall_regions  out/uc7_sio2_pipe/pipeline_records.json  16
STEP 12 validate_json  out/uc7_sio2_pipe/pipeline_records.json
STEP 13 validate_json  out/uc7_sio2_pipe/run_manifest.json
```

---

## Acceptance criteria

| Criterion | Pass condition |
|-----------|---------------|
| Run completes without crash | `vsepr run` exits 0 |
| 4 core files present | XYZ, pipeline_records, dashboard, manifest |
| PENDING count matches MF-G ledger | 5 items: MF-G01 through MF-G05 |
| Energy decreasing | Relaxation is converging, not diverging |
| Flux recorded | `record_flux` field present in output |
| Wall binned | 16 axial regions detected |

---

## Development priority sequence

To fully green-light UC-7:

1. **MF-G04** — DEMBridge runtime (contact physics coupling)
2. **MF-G05** — FEABridge von Mises / fatigue record emission
3. **MF-G01** — `write_energy_trace_svg` export writer
4. **MF-G02** — `write_html_dashboard` export writer
5. **MF-G03** — `write_pipeline_audit_jsonl` writer

---

## Physical interpretation

SiO₂ in a 5 nm nanofluidic tube is a proxy for:
- **Silica nanoparticle transport** in microfluidic channels
- **Zeolite pore entry** dynamics (MFI pore diameter ~5.6 Å, tube here is 20 Å)
- **Quartz wall erosion** under sustained particulate impact (FEA bridge)

The 4 Å wall thickness and Hertz-Mindlin contact model are physically meaningful
for amorphous SiO₂ at 300 K under low-flux laminar flow.

---

## Fixture script

`tests/automation/vsim/uc7_sio2_pipe.vsim` — fully resolved, no template tokens.

---

*UC-7 | WO-MF-01 | VSEPR-SIM Desktop Kernel Legacy*
