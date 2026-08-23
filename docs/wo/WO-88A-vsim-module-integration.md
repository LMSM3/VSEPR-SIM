# WO-88A — VSIM Module Improvement and Visual Integration

## Identity
- Owner/context: Follow-up to WO-88 visual repair of new system
- Current day: Day 88 follow-up
- Release target: Post-v5.14.1 declarative VSIM-to-visual integration
- Branch: `day84t-chemplus-declarative-vsepr`
- Scope boundary: Improve and integrate existing VSIM modules with the repaired visual system through explicit contracts, parser/runtime wiring, artifact adapters, tests, and documentation. This WO consumes the repair boundary defined by WO-88.

## Objective

Connect VSIM’s declarative configuration and authoritative output artifacts to the stabilized visual system without bypassing parser/runtime contracts or altering scientific data.

```text
.vsim configuration
		|
		v
VsimDocument defaults -> VSIM parser -> runtime/module dispatch
		|
		+--> authoritative artifacts and structured state
		|
		v
VsimVizAdapter / VsimRenderLayer / visual-data adapter
		|
		v
repaired visual system
```

## Acceptance criteria
- [ ] Each visual-facing VSIM setting has one defined default, parser path, runtime consumer, and documented behavior.
- [ ] Visual module dispatch routes supported VSIM output through the repaired visual-data contract rather than a duplicate or ad hoc format.
- [ ] Existing terminal, SVG/HTML artifact, desktop-viewer, and new visual-system consumers remain compatible where their documented contracts overlap.
- [ ] Missing, unsupported, or invalid visual configuration produces an actionable diagnostic and a safe fallback; it never silently changes simulation state.
- [ ] Artifact provenance, coordinates, particle state, and run identity remain authoritative at the VSIM output boundary.
- [ ] Focused parser, routing, adapter, and end-to-end integration tests are registered and pass before closure.
- [ ] `VSIM_REFERENCE.md`, `docs/VSIM_LANGUAGE.md`, `VSIM_DEVELOPMENT.md`, and the applicable visual documentation reflect any delivered configuration or behavior changes.

## Status
[W] WO-88A: VSIM module improvement and visual integration
[D] Day 88 follow-up: execute after WO-88 visual repair establishes the supported visual contract
[I] TODO
[V] DISCOVERY: VSIM visual routing and render-layer interfaces reviewed; no integration validation performed
[P] LOCAL: planning record only; no commit or publication recorded
[N] BLOCKED: begin only after WO-88 records the repaired visual-data contract and supported frontend behavior

## Evidence log
- 2026-07-03: `include/vis/vsim_render_layer.hpp`, `include/vis/vsim_viz_adapter.hpp`, and `src/vis/viz_router.cpp` identified as the existing VSIM-facing visual integration boundary.
- 2026-07-03: The Day 87 hosted-session plan requires any new declarative host setting to be defined in `include/vsim/vsim_document.hpp`, parsed in `src/vsim/vsim_parser.cpp`, applied by runtime code, registered in tests, and documented.
- 2026-07-03: This record intentionally supersedes the unimplemented WO-88A hosted-session-UI placeholder in `docs/wo/WO-87A-bidirectional-vsim-html-hosting.md`; browser hosting remains planned under its parent plan and must receive a new identifier before implementation.

## Carry-forward
- Carry a stable visual-data contract from WO-88 into VSIM adapters and module dispatch.
- Re-plan the deferred hosted-session UI with a unique child identifier under WO-87A before implementation.
- Retain browser-session security requirements: structured allowlisted requests only; no raw shell or unvalidated VSIM input.

## Out of scope
- Repairing the BGFX/GLFW rendering lifecycle itself; that is WO-88.
- Replacing VSIM parsing, the simulation kernel, or authoritative scientific artifact formats.
- Browser-host UI, TLS/proxy deployment, or browser-to-host command execution previously outlined in the Day 87 hosting plan.
- Modifying coordinates, forces, or provenance as part of visual adaptation.
