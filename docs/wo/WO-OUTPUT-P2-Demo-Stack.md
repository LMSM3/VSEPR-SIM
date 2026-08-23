# WO-OUTPUT-P2 — Output System Expansion: Phase 2
## Demo Stack — .dynx / .X Compatibility + Lightweight Molecular Demonstrations

**Work order:** WO-OUTPUT-P2  
**Branch:** v5.0.0-main  
**Status:** ✅ COMPLETE — all 4 sub-WOs delivered (v5.0.0-main)  
**Depends on:** WO-72A (.X Bundle), WO-72B (DynxEmitter)

---

## Objective

Establish a first-class technical stack for producing **lightweight molecular demonstration artifacts** from large batch simulations.  

A batch study produces many simulation cases.  Phase 2 adds an orchestration layer that can:
1. Harvest key frames from each case's .dynx archive (or produce a condensed demo archive directly)
2. Package the result as a self-contained .X bundle — one deliverable per study or per representative case
3. Do so with minimal overhead: no full trajectory retention required, configurable reduction strategy

The output is human-shareable, viewer-ready, and reproducible from the same seed + study file.

---

## Motivation

Current gap: WO-72B emits a complete rich .dynx archive per simulation run.  
WO-72A packages .vsim scripts into .X bundles.  
**Neither layer connects to the other at the batch orchestration level**, and neither produces a "demo-sized" artifact.

A large batch sweep (hundreds of cases) can produce gigabytes of .dynx data.  A reviewer, student, or collaborator needs a representative 5–20 frame demonstration bundle — not the full archive.

---

## Format Contract

### Demo Dynx (*.demo.dynx)

A condensed .dynx file that conforms fully to the v1 format (backward-compatible).  
Produced by the demo stack's **frame sampler** — not the live emitter.

| Field | Value |
|---|---|
| Format | #dynx v1 — identical spec, subset of frames |
| Source tag | #source_role demo header line added |
| Frame count | Configurable: demo_frames (default 10) |
| Selection strategy | irst, last, uniform, energy_min, event_gated |
| Rich data | Included verbatim from source frames (FORCE/RENDER/CAMERA lines preserved) |
| Provenance | #batch_case, #batch_study, #seed header lines added |

### Demo Bundle (*.demo.X)

An .X bundle that packages:
- The .demo.dynx archive as an sset member
- The originating .vsim script as a sim member  
- A demo_manifest.json asset member (case metadata, reduction parameters, frame indices)

| Field | Value |
|---|---|
| Format | XBUNDLE 1 — existing format, existing writer |
| Entry point | The .vsim member |
| Members | main.vsim + demo.dynx + demo_manifest.json |

---

## Schema Section Design

A new [export.demo] section in .vsim (and VsimDocument).

`
[export.demo]
enabled          = true
demo_frames      = 10
strategy         = uniform          # first | last | uniform | energy_min | event_gated
write_demo_dynx  = true             # emit *.demo.dynx
write_demo_bundle = true            # emit *.demo.X  (requires write_demo_dynx)
bundle_name      = ""               # override bundle name (default: <study_name>.demo.X)
include_source   = true             # embed originating .vsim in bundle
include_manifest = true             # embed demo_manifest.json in bundle
`

**Struct:** ExportDemoSection in include/vsim/vsim_document.hpp  
**Parser:** pply_export_demo_key() in src/vsim/vsim_parser.cpp  
**VsimDocument field:** ExportDemoSection export_demo

---

## Component Map

| Component | File | WO Sub-task |
|---|---|---|
| ExportDemoSection struct | include/vsim/vsim_document.hpp | WO-OUTPUT-P2-A |
| Parser wiring [export.demo] | src/vsim/vsim_parser.cpp | WO-OUTPUT-P2-A |
| DemoFrameSampler — selects N frames from a .dynx source | include/vsim/io/demo_frame_sampler.hpp | WO-OUTPUT-P2-B |
| DemoFrameSampler implementation | src/vsim/io/demo_frame_sampler.cpp | WO-OUTPUT-P2-B |
| DemoBundleWriter — assembles .demo.X from sampler output | include/vsim/io/demo_bundle_writer.hpp | WO-OUTPUT-P2-C |
| DemoBundleWriter implementation | src/vsim/io/demo_bundle_writer.cpp | WO-OUTPUT-P2-C |
| Batch orchestration hook — calls demo stack after each case | src/batch/batch_aggregator.cpp (extend) | WO-OUTPUT-P2-D |
| Test group 72 — ExportDemoSection parse + defaults | tests/test_export_demo_section.cpp | WO-OUTPUT-P2-A |
| Test group 73 — DemoFrameSampler all strategies | tests/test_demo_frame_sampler.cpp | WO-OUTPUT-P2-B |
| Test group 74 — DemoBundleWriter round-trip | tests/test_demo_bundle_writer.cpp | WO-OUTPUT-P2-C |
| Test group 75 — Batch demo integration | tests/test_batch_demo_integration.cpp | WO-OUTPUT-P2-D |

---

## Data-Reduction Strategies

| Strategy | Behaviour |
|---|---|
| irst | Frames 0..N-1 |
| last | Last N frames |
| uniform | Evenly spaced indices across full archive (default) |
| energy_min | N frames with lowest total kinetic energy (quietest states) |
| event_gated | Frames that contain at least one EVENT packet (reaction/checkpoint) |

The sampler reads the source .dynx via the existing DynxReader / dynx_inspect() API — no new reader surface needed.

---

## Batch Hook Point

After all cases in a study complete (atch_aggregator.cpp), if export_demo.enabled:
1. For each case (or for the representative case if strategy selects one): invoke DemoFrameSampler
2. Invoke DemoBundleWriter to produce the .demo.X bundle
3. Record bundle path in aggregate JSON (write_aggregate_json)

This is a **post-run hook** — no changes to simulation physics or CTL dispatch.

---

## Deferred Items

| Item | Deferred To |
|---|---|
| strategy = energy_min — requires per-frame energy index in .dynx reader | WO-OUTPUT-P2-B-ext |
| Multi-case comparison bundle (N cases → one .X) | v5.3.0 |
| Viewer auto-launch on .demo.X open | v5.3.0 |
| PNG thumbnail generation for bundle manifest | v5.3.0 |

---

## Acceptance Criteria

| Gate | Requirement |
|---|---|
| Schema | [export.demo] parses all 7 keys; defaults match spec above |
| Sampler | All 4 implemented strategies produce correct frame count and indices |
| Bundle | .demo.X round-trips through XBundleReader; all 3 members present |
| Batch hook | Demo bundle path appears in aggregate_json output |
| Regression | All existing tests continue to pass (zero regressions) |

---

## Sub-Work-Orders

| WO | Title | Status |
|---|---|---|
| WO-OUTPUT-P2-A | ExportDemoSection struct + parser | ✅ COMPLETE |
| WO-OUTPUT-P2-B | DemoFrameSampler (uniform + first + last + event_gated strategies) | ✅ COMPLETE |
| WO-OUTPUT-P2-C | DemoBundleWriter (.demo.dynx + .demo.X assembly) | ✅ COMPLETE |
| WO-OUTPUT-P2-D | Batch aggregator hook + integration tests | ✅ COMPLETE |

---

## Resolved Questions

1. **Representative case vs. all cases**: **One bundle per case.** The user selects the representative case post-hoc. Batch hook iterates all cases.
2. **energy_min feasibility**: **Deferred to WO-OUTPUT-P2-B-ext.** Requires lightweight per-frame energy index in DynxReader; implement as an extension, not in this WO.
3. **Bundle naming**: **`out/demo/<case_id>.demo.X`** flat layout inside an `out/demo/` subdirectory. Study-level bundles are v5.3.0.
4. **Viewer rules**: **Deferred to v5.3.0.** Opening a `.demo.X` does not auto-launch the viewer in this WO; see Deferred Items table.