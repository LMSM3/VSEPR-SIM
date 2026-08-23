# UC-7 — Resolved SiO₂ Pipe Bridge ("The Material Payload")
<!-- VSEPR-SIM | Day 85E | WO-85 -->

---

> **Note on numbering.** This document is the **Day 85** formalization of UC-7 as a
> *resolved-object bridge* validation: it proves a pipe object can carry a
> **resolved material payload (SiO₂)** through the VSIM pipe bridge with honest
> gap reporting. It is a companion to the earlier
> `docs/uc/UC-7-sio2-pipe-simulation.md`, which defines UC-7 as the full
> SPH/DEM/FEA particulate-flow simulation. This doc focuses on *material identity
> and bridge resolution*; that doc focuses on *transport physics*. They share the
> UC-7 identity intentionally and do not overwrite one another.

---

## Overview

**Goal:** Validate that a pipe object can carry a resolved material payload —
here **SiO₂** — through the VSIM pipe bridge. This is not "draw a pipe." UC-7
proves the pipe object can represent, and honestly report on:

```
geometry
material identity
chemical composition
flow metadata
resolved object state
validation warnings
export / report output
```

---

## Why SiO₂?

SiO₂ is deliberately awkward — it sits between worlds:

```
chemical formula:  SiO2
material identity: silica / quartz-like / glass-like (context dependent)
solid object:      yes
pipe payload:      possible
reactivity:        not a simple gas/liquid flow species
```

So it stress-tests whether the pipe system can carry a **resolved material
object**, not merely a string pretending to be chemistry.

---

## Actors

| Actor | Role |
|-------|------|
| Researcher / CI | Triggers `bash tests/run_uc7_pipe_validation.sh` |
| `vsepr.exe` | Parses the pipe fixture and runs bridge validation headlessly |
| Pipe object | Carries geometry + material payload + flow metadata |
| Material resolver | Resolves `SiO2` to a formula/material payload |
| Bridge validator | Resolves material/geometry/flow and reports gaps |
| Export package | Emits summary, JSON, and gap report |

---

## Preconditions

1. `build/vsepr.exe` present.
2. Fixture `scripts/uc7_resolved_sio2_pipe.vsim` present.
3. `bash` available (Git Bash / MSYS2 on Windows).

---

## Expected Inputs

```
scripts/uc7_resolved_sio2_pipe.vsim
material = "SiO2"
pipe geometry (linear profile, length, radius, inlet/outlet)
flow profile (placeholder / static allowed, warned)
bridge flags (resolve material / geometry / flow, report gaps)
validation options (fail-on-invalid, warn-on-missing)
```

---

## Expected Outputs

```
resolved pipe report
material resolution report
composition report
geometry validation report
flow / profile warning report
bridge gap report
```

Artifact layout:

```
exports/uc7_resolved_sio2_pipe/summary.txt
exports/uc7_resolved_sio2_pipe/results.json
exports/uc7_resolved_sio2_pipe/uc7_pipe_validation.log
```

---

## Expected Report (aspirational shape)

```
[UC-7] Resolved SiO2 Pipe Bridge

[Material]
formula: SiO2
name: silicon_dioxide
phase: solid
resolution: formula_resolved
property_status: partial

[Pipe]
name: sio2_resolved_pipe
geometry: linear_profile
length_m: 2.0
radius_m: 0.05
inlet: [0,0,0]
outlet: [2,0,0]

[Bridge]
material_bridge: resolved
geometry_bridge: resolved
flow_bridge: placeholder
gaps: present

[Warnings]
- static flow placeholder
- missing thermo properties
- unresolved solid transport model

[Gate]
uc7_pipe_validation: PASS_WITH_WARNINGS
```

> Because the pipe-bridge runtime is not yet fully wired (see MF-G extension
> tags in WO-MF-01), the current automation validates the fixture through the
> real `vsepr run` output plus the fixture's own declarative echo, and treats
> unresolved fields as *reported warnings*, not silent failures.

---

## Acceptance Criteria

```
[ ] pipe object parses successfully
[ ] SiO2 resolves as material/formula payload
[ ] pipe has inlet/outlet definition
[ ] linear profile object is accepted
[ ] pipe reports length/radius/profile
[ ] flow metadata is present or warned as missing
[ ] bridge validation runs
[ ] unresolved fields are reported honestly
[ ] export/report is generated
```

---

## Fixture + automation

| Artifact | Path |
|----------|------|
| Pipe fixture | `scripts/uc7_resolved_sio2_pipe.vsim` |
| Automation script | `tests/run_uc7_pipe_validation.sh` |
| CTest target | `uc7_resolved_sio2_pipe_validation` (Group 97) |
| CTest labels | `vsim pipe bridge material sio2 uc7 wo-85 quick` |

---

## Related

- `docs/uc/UC-7-sio2-pipe-simulation.md` — full SPH/DEM/FEA particulate-flow simulation.
- `docs/wo/WO-MF-01-missing-features.md` — MF-G / MF-G-0xx pipe bridge gap tags.

---

*UC-7 | Day 85E | WO-85 | VSEPR-SIM*
