# Day 89 Overview — VSIM Visual Scale and Bead Return

[W] Day 89: VSIM stress, VTK + Qt3D visual consolidation, and bead packet return | Authority: `docs/day89/DAY89_D_E_RENDERER_BOUNDARY.tex`
[D] Day 89 OPEN — A/B/C/D-G evidence consolidated; H onward is the next open boundary
[I] DONE — 89-A/B/C and 89-D/E/F/G complete locally; live viewer route changed to VTK + Qt3D; OpenGL/BGFX/CPU-GDI archived
[V] PASS — 1,442-particle artifact, Python validator, explicit Qt3D capability blocker, and full release build verified
[P] LOCAL
[N] Execute 89-H bead identity/packet schema onward; carry-forward for Qt3D component installation

## Day objective (revised)

Stress the VSIM scripting/runtime path above 1,000 particles, consolidate the visual consumer around a single VTK + Qt3D workstation, archive obsolete OpenGL/BGFX/native CPU-GDI routes, and restore beads as first-class control volumes containing resolved packets with explicit mass, momentum, energy, and species flux exchange.

## Letter map

| ID | Status | Large task | Required authority | Acceptance evidence |
|---|---|---|---|---|
| 89-A/B | DONE | Baseline plus deterministic >1,000-particle fixture | `docs/day89/DAY89_A_B_BASELINE_FIXTURE.tex` | 111 baseline rows, 1,442 stress rows, repeated CSV hashes |
| 89-C | DONE | Renderer-neutral large-particle artifact | `docs/day89/DAY89_C_ARTIFACT_CONTRACT.tex` | Manifest and particle rows agree; stress validator passes |
| 89-D/E/F/G | DONE | Consolidated VTK + Qt3D visual boundary | `docs/day89/DAY89_D_E_RENDERER_BOUNDARY.tex` | Single active vsepr-view target; deterministic artifact validation; explicit Qt3D blocker when unavailable |
| (89-F) | MERGED | Native CPU/GDI scale expansion merged into VTK + Qt3D target | — | Covered by vsepr-view --validate-only and CTest |
| (89-G) | MERGED | Cross-renderer parity merged into VTK + Qt3D target | — | Single consumer + explicit blocker replaces multi-renderer parity |
| 89-H | TODO | Bead identity and packet schema | `docs/day89/DAY89_H_BEAD_PACKET.tex` | Typed packet representation and serialization |
| 89-I | TODO | Resolved packet contents | `docs/day89/DAY89_I_RESOLVED_PACKET.tex` | Identity/state/mass/momentum/energy contents |
| 89-J | TODO | Mass flux entry/exit | `docs/day89/DAY89_J_MASS_FLUX.tex` | Source/sink events and residual checks |
| 89-K | TODO | Momentum flux | `docs/day89/DAY89_K_MOMENTUM_FLUX.tex` | Vector balance evidence |
| 89-L | TODO | Energy and heat/work exchange | `docs/day89/DAY89_L_ENERGY_FLUX.tex` | Energy ledger closure |
| 89-M | TODO | Species/chemistry exchange | `docs/day89/DAY89_M_SPECIES_FLUX.tex` | Species inventory and event trace |
| 89-N | TODO | Open/closed bead boundaries | `docs/day89/DAY89_N_BOUNDARIES.tex` | Boundary-mode tests |
| 89-O | TODO | VSIM bead authoring syntax | `docs/day89/DAY89_O_VSIM_BEADS.tex` | Parser/runtime tests and example script |
| 89-P | TODO | Bead trajectory and artifact output | `docs/day89/DAY89_P_BEAD_OUTPUT.tex` | Replayable packet/trajectory output |
| 89-Q | TODO | R/XLSX large-scene reporting | `docs/day89/DAY89_Q_REPORTING.tex` | Reports for >1,000-particle run |
| 89-R | TODO | HTML/static visual bundle | `docs/day89/DAY89_R_HTML_BUNDLE.tex` | Browser-readable artifact package |
| 89-S | TODO | Stress failure classification | `docs/day89/DAY89_S_FAILURES.tex` | Classified failures with reproductions |
| 89-T | TODO | Conservation and renderer tests | `docs/day89/DAY89_T_TESTS.tex` | Focused suite passes |
| 89-U | TODO | Performance/readiness evidence | `docs/day89/DAY89_U_READINESS.tex` | Measured thresholds and blocker list |
| 89-V | TODO | Day close and Day 90 handoff | `docs/day89/DAY89_V_HANDOFF.tex` | Close/open ledger with verified evidence |
| 89-W | RESERVED | RESERVED | Not assigned | Late critical work only |
| 89-X | RESERVED | RESERVED | Not assigned | Cross-cutting correction only |
| 89-Y | RESERVED | RESERVED | Not assigned | Release/yield evidence only |
| 89-Z | RESERVED | RESERVED | Not assigned | Final recovery/close only |

## Day 89 exit gate

- One deterministic VSIM path produces more than 1,000 particles.
- The VTK + Qt3D viewer consumes the scientific artifact or explicitly records a dependency blocker.
- Legacy OpenGL, BGFX, and native CPU/GDI code is removed from active configuration and archived.
- Bead packets have explicit represented mass, momentum, energy, identity summary, and provenance.
- Open-system exchange is event-driven and conservation residuals are reported.
- Each completed letter has an authoritative approximately two-page TeX.
- Day 90 receives a bounded carry-forward ledger rather than implicit unfinished work.
