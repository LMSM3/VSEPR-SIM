# WO-87A — Bidirectional VSIM-to-HTML Hosting Overall

**Status:** PLANNED
**Branch:** `day84t-chemplus-declarative-vsepr`
**WO:** WO-87A
**Day:** 87 (overall)
**Priority through Day 92:** HTML hosting and HTTPS-like deployment

---

## Problem

The project can already emit scientific `.xyz` / `.xyzFull` artifacts, expose plain-text console information through HTML, and run parallel live visualization sessions. Those paths remain fragmented:

- A VSIM run does not have one durable browser session that owns its artifacts, console, state, and provenance.
- The browser host is primarily one-way: it publishes frames through HTTP, WebSocket, and SSE, but it cannot safely accept a user request back into the same browser window.
- The current desktop-oriented 3D path can consume XYZ output but is not a reliable HTML delivery pipeline and does not meet the desired presentation quality.
- The host has plain HTTP only; development and deployment need an explicit HTTPS-like path.

The required outcome is a browser session that can display the authoritative output from a VSIM run and allow a user to submit bounded commands or script requests from that same window.

---

## Objective

Deliver a deterministic, bidirectional VSIM-to-HTML session pipeline:

```text
.vsim source / browser request
		|
		v
validated command request + run identity
		|
		v
VSIM parser and runtime
		|
		+--> scientific artifacts (.xyz, .xyzFull, .dynx, CSV, JSON)
		|
		+--> structured console/events/status
		v
browser session host (HTTP(S) + WebSocket/SSE)
		|
		v
single browser window: scene, report, console, and command input
```

Scientific artifacts remain the source of truth. The browser is a downstream display and interaction surface; it must not modify coordinates or silently synthesize simulation state.

---

## Scope

### In scope

1. **Run-bound browser sessions**
   - Stable `session_id`, run label, source `.vsim` path/hash, creation time, and output directory.
   - Artifact list and status updates bound to the same session.

2. **VSIM-to-HTML output contract**
   - Structured events for lifecycle, console output, warnings/errors, artifacts, frames, and completion.
   - An XYZ/XYZFull adapter that produces browser frames without changing scientific coordinates.
   - Render metadata is optional and downstream of the artifact truth.

3. **Same-window command input**
   - A browser command panel that submits requests to the host.
   - Explicit command types: validate a VSIM source, start a configured run, stop a session, request artifact reload, and query session status.
   - Requests are validated, size-limited, attributable to a session/request ID, and reported back as structured events.

4. **Hosted viewer experience**
   - One page containing a 3D scene, artifact/state panel, console stream, and command input.
   - Existing WebSocket, SSE, and polling fallback behavior remains available.
   - Parallel live sessions remain isolated by session identity and selected stream/port.

5. **HTTPS-like hosting**
   - Direct TLS mode for local development using an explicitly supplied certificate/key.
   - Reverse-proxy deployment documentation and configuration for a trusted production certificate.
   - HTTP-to-HTTPS redirect only when TLS is explicitly configured; no false security claims for plain HTTP.

6. **Tests and documentation**
   - Parser/runtime tests for new VSIM configuration fields, if fields are introduced.
   - Host tests for request validation, session isolation, XYZ conversion, event ordering, and TLS startup configuration.
   - Reference, language, development, and deployment documentation updates.

### Out of scope

- Replacing the simulation kernel or inventing a browser-side simulation engine.
- Editing coordinates, forces, or provenance from the browser.
- Anonymous remote execution without an operator-controlled access boundary.
- Production certificate issuance or DNS provisioning.
- Replacing existing desktop viewers; they remain supported downstream consumers.

---

## Architecture and Contracts

### 1. Session model

Each browser-visible run owns one immutable identity:

```text
HostedRunSession
  session_id            stable opaque identifier
  source_vsim_path      canonical source path when file-backed
  source_hash           content hash captured at submission
  run_label             operator-visible label
  state                 queued | validating | running | completed | failed | stopped
  started_at / ended_at timestamps
  output_directory      artifact root
  artifacts             authoritative output descriptors
  console_sequence      monotonic event ordering
```

A session may expose multiple output frames, but every frame and command acknowledgement carries the session ID and a monotonic sequence number.

### 2. Browser event envelope

All browser-facing events use a common JSON envelope:

```text
{
  "type": "session.status | console | artifact | frame | warning | error | command.result",
  "session_id": "...",
  "sequence": 42,
  "timestamp": "ISO-8601 UTC",
  "payload": { }
}
```

The existing frame transports remain supported. The new envelope prevents console text, artifacts, and frame updates from being ambiguously mixed between parallel sessions.

### 3. Browser-to-host command envelope

The browser submits JSON only to a dedicated command endpoint or WebSocket message type:

```text
{
  "request_id": "client-generated opaque ID",
  "session_id": "optional for new run; required otherwise",
  "command": "validate | start | stop | reload_artifact | status",
  "payload": { }
}
```

The host rejects malformed JSON, unknown commands, oversized bodies, missing required session IDs, invalid paths, and requests that escape the configured workspace/output roots. It must return a deterministic `command.result` event rather than executing shell text.

### 4. VSIM configuration boundary

If a new `[web_host]` or equivalent VSIM section is introduced, it follows the project’s required feature flow:

1. Define defaults in `include/vsim/vsim_document.hpp`.
2. Parse keys in `src/vsim/vsim_parser.cpp`.
3. Apply configured behavior in runtime/demo applications.
4. Add registered tests in `tests/CMakeLists.txt`.
5. Update `VSIM_REFERENCE.md`, `docs/VSIM_LANGUAGE.md`, and `VSIM_DEVELOPMENT.md`.

No unvalidated browser input becomes a raw VSIM fragment or shell argument.

---

## Delivery Breakdown

| Child WO | Deliverable | Primary areas | Gate |
|---|---|---|---|
| WO-87B | Hosted session/event model | `tools/viz_web.py`, runtime bridge, tests | Session isolation and event ordering tests pass |
| WO-87C | XYZ/XYZFull browser artifact adapter | XYZ IO, `tools/viz_web.py`, viewer page, tests | Known XYZ artifact renders with unchanged coordinates |
| WO-87D | Same-window command channel | host routes/WebSocket, runtime dispatch, tests | Validate/start/stop/reload requests are bounded and attributable |
| Deferred (formerly WO-88A) | HTML viewer session UI | hosted dashboard assets/templates | Reassign a unique child WO before implementation; WO-88A now tracks VSIM module integration |
| WO-88B | HTTPS-like local and proxy hosting | `tools/viz_web.py`, `deploy/`, docs | Direct TLS configuration and reverse-proxy smoke checks pass |
| WO-88C | Integration, reference, and release gate | tests, VSIM docs, deployment docs, `STAGE.md` | End-to-end VSIM-to-browser session test plus PNG verification pass |

The remaining child identifiers reserve Day 88 for hosting and final integration. The former WO-88A UI reservation is deferred because `docs/wo/WO-88A-vsim-module-integration.md` now owns that identifier; reassign a unique hosting child WO before implementation begins.

---

## Acceptance Criteria

| ID | Criterion |
|---|---|
| A1 | A completed VSIM run exposes its `.xyz` or `.xyzFull` output, source provenance, and status through one browser session. |
| A2 | The browser 3D scene receives the exact artifact coordinates through the adapter; it does not mutate scientific output. |
| A3 | The same browser page shows structured console/status events and accepts supported commands. |
| A4 | Malformed, unknown, oversized, and path-escaping requests are rejected without invoking the runtime. |
| A5 | Parallel sessions cannot receive another session’s frames, console events, artifacts, or command result. |
| A6 | WebSocket, SSE, and polling fallback continue to deliver session-scoped updates. |
| A7 | Direct TLS runs only when certificate/key paths are explicitly configured; plain HTTP is clearly identified as plain HTTP. |
| A8 | A reverse-proxy deployment path documents WebSocket/SSE forwarding and TLS termination. |
| A9 | Each new VSIM field, if any, is defined, parsed, wired, tested, and documented under the project schema rule. |
| A10 | Validation includes an existing Day 84+ PNG 3D export and confirms the output exists. |

---

## Validation Plan

1. Unit-test session IDs, sequence ordering, request validation, and command result envelopes.
2. Convert fixture XYZ and XYZFull artifacts to browser frames and assert coordinate equality.
3. Run concurrent fixture sessions and assert strict event/artifact isolation.
4. Exercise WebSocket, SSE, and polling fallback against one session.
5. Smoke-test direct TLS with an ephemeral development certificate and confirm the process reports HTTPS only in TLS mode.
6. Exercise reverse-proxy configuration syntax and document the certificate/operator prerequisites.
7. Run a configured VSIM script from the browser command panel, verify its artifacts and console status in the same page, and confirm the required PNG 3D export exists.
8. Run registered tests and the authoritative CMake preset build.

---

## Key Files

| File | Role |
|---|---|
| `tools/viz_web.py` | Existing HTTP/WebSocket/SSE host; primary host implementation target |
| `tools/viz_bond_graph.html` | Existing browser visualization surface; potential component/input source |
| `include/vsim/vsim_document.hpp` | VSIM schema defaults if hosting configuration becomes declarative |
| `src/vsim/vsim_parser.cpp` | VSIM parsing if hosting configuration becomes declarative |
| `src/cli/` | Bounded runtime command dispatch boundary |
| `include/vsim/view/viewer_output_session.hpp` | Existing artifact/provenance session concepts to reuse rather than duplicate |
| `deploy/nginx-viz.conf` | Reverse-proxy deployment configuration |
| `deploy/README_VIZ.md` | Local and deployment documentation |
| `tests/CMakeLists.txt` | Registered C++ test groups; Python host tests are added alongside the host |
| `VSIM_REFERENCE.md` | Canonical schema reference if new fields are added |
| `docs/VSIM_LANGUAGE.md` | User-facing language behavior documentation if new fields are added |
| `VSIM_DEVELOPMENT.md` | Delivery and validation record |

---

## Risks and Decisions

- **Security:** Browser input is a structured, allowlisted request protocol—not a remote shell or raw script execution endpoint.
- **Artifact truth:** XYZ/XYZFull and Dynx remain authoritative scientific/replay artifacts. HTML is not a replacement serialization format.
- **Compatibility:** Existing desktop viewer and raw NDJSON consumers remain supported while the hosted session path is added.
- **Hosting:** HTTPS-like development mode is not equivalent to a trusted public certificate. Production TLS requires operator-managed certificate and network configuration.
- **Rendering quality:** Improve material, lighting, bonds, labels, and camera controls only after the session/artifact contract is reliable; visual changes must not change scientific coordinates.

---

## Tracking Checklist

| Item | Status |
|---|---|
| WO-87A overall contract and scope | COMPLETE |
| WO-87B hosted session/event model | PLANNED |
| WO-87C XYZ/XYZFull browser artifact adapter | PLANNED |
| WO-87D same-window command channel | PLANNED |
| Hosted session UI (formerly WO-88A) | DEFERRED — requires a new unique child WO |
| WO-88B HTTPS-like hosting | PLANNED |
| WO-88C integration and release gate | PLANNED |
