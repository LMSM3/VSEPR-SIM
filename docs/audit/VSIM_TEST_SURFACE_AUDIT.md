# VSIM Test Surface Audit

**Date:** 2026-07-03
**Branch:** `day84t-chemplus-declarative-vsepr`
**Purpose:** Task A — inspect the VSIM-labelled test surface without executing it before v5.14.1 finalization.

## Scope correction

The reported `26 tests / 2.05 seconds` was not reproducible as a current CTest selection. Discovery with:

```text
ctest --test-dir build -N -L vsim
```

listed **41 tests**. CTest label matching is regular-expression based, so `-L vsim` also matches labels such as `wo-vsim-62c` and `wo-vsim-ctl`; it is not an exact-label query. No VSIM-labelled tests were executed for this audit.

The direct VSIM language/runtime surface is concentrated in the following registered targets:

| Target | WO / group | Code direction | Main dependency |
|---|---|---|---|
| `VsimRuntimeDay2Test` | Group 27 | while guards, batch isolation, visual cadence, N-evolution, boundaries | `vsepr_core` |
| `ExportFlushTest` | Group 29 | script-declared JSONL/Markdown export | `vsepr_core` |
| `PBCVsimParserTest` | Group 32 | `[cell]`, `[boundary]`, `[pbc]` parse/bind | direct `vsim_parser.cpp` + `vsepr_chem` |
| `PBCBindingsTest` | Group 32 | `pbc.*` binding semantics | direct `pbc_bindings.cpp` |
| `VsimValueXYZVec3Test` | Group 33 | XYZVec3/Int3 value layer | value headers |
| `VsimInterpreterPBCTest` | Group 33 | interpreter evaluation of PBC expressions | direct interpreter + builtins |
| `VsimXYZPBCBridgeTest` | Group 33 | XYZ comment metadata → runtime PBC bridge | direct interpreter + builtins |
| `VsimPostStepTest` | Group 34 | post-step script execution against state | direct interpreter + builtins |
| `RenderIntervalTest` | Group 35 | visual cadence defaults, parsing, override/fallback | direct `vsim_parser.cpp` |
| `IntentAuthoringTest` | Group 36 | intent/material/run/environment/excite/observe schema | direct `vsim_parser.cpp` |
| `VsimOutputFilterGroup79` | Group 79 | export-gated multi-scale output | `vsepr_vsim_filter` |
| `PrintConsoleGroup93` | Group 93 | console directive parsing | direct `vsim_parser.cpp` |
| `OutputFormatGroup94` | Group 94 | output-format selection and reactivity | direct `vsim_parser.cpp` |
| `StressLoopsGroup95` | Group 95 | while-loop stress under output formats | direct `vsim_parser.cpp` |

## Dissection

### 1. Parser is repeatedly compiled directly into tests

`PBCVsimParserTest`, `RenderIntervalTest`, `IntentAuthoringTest`, `PrintConsoleGroup93`, `OutputFormatGroup94`, and `StressLoopsGroup95` compile `src/vsim/vsim_parser.cpp` directly. This gives tight unit isolation, but it creates a foundation risk: these tests can pass while the production target uses a different translation-unit set, compile definitions, or registration path.

**Follow-up:** add one production-bound parser smoke test that invokes the same library/CLI path used by `vsepr validate` or `vsepr run`, rather than only direct-source test executables.

### 2. Runtime-day-2 coverage is mostly model-level, not a complete script-to-runtime path

`test_vsim_runtime_day2.cpp` checks guard iteration, batch event isolation, render decimation, N-evolution event counters, cached metrics, and boundary dispatch. The assertions are useful, but the target links only `vsepr_core`; it does not exercise the full parser → document → runner → artifact chain.

**Follow-up:** retain these fast tests, then add a separate integration gate that loads a fixture `.vsim` file and verifies the resulting artifact directory and output inventory.

### 3. Export coverage verifies event-log flushing, not complete XYZ artifact ownership

`ExportFlushTest` proves JSONL and Markdown flush behavior from `KernelEventLog`. It does not prove that a VSIM run’s configured `write_xyz`/`write_xyzfull` flags reach a real output artifact and then a viewer session.

**Follow-up:** pair export-flag tests with an artifact contract test covering path, existence, non-empty content, source/run label, and coordinate preservation.

### 4. PBC bridge coverage is layered and useful, but metadata is a seam

The Group 33 tests cover value types, interpreter builtins, XYZ comment-line metadata, and post-step execution. This is the strongest direct evidence for the PBC scripting path. The bridge depends on comment metadata conventions, which are stringly typed and therefore vulnerable to formatting drift between writers and readers.

**Follow-up:** centralize comment metadata keys or add a round-trip fixture test that uses the production XYZ writer and production reader together.

### 5. Visual cadence tests validate configuration more than rendering delivery

`RenderIntervalTest` checks default values, parser round-trip, zero handling, and external override/fallback. It does not verify that a live viewer or HTML host receives only the configured cadence.

**Follow-up:** add a transport-level cadence test after the HTML session contract is implemented; keep parser cadence tests unchanged.

### 6. Intent-authoring tests are schema-heavy and should be paired with runtime acceptance

`IntentAuthoringTest` covers broad document sections and aliases. The registration shows direct parser compilation, so it establishes schema acceptance but not whether each accepted field changes runtime behavior.

**Follow-up:** select a small set of high-value fields and assert their effect at the runtime boundary, especially fields that control output, visual mode, and observation/reporting.

### 7. Output-format and console work is current release foundation

Groups 93–95 are directly tagged `vsim` and were added after the older beta-era tests. They are the most relevant surface for the current HTML-hosting direction: console output, output format selection, relative reactivity, and loop behavior. Their current registration also compiles the parser directly rather than using a production parser library target.

**Follow-up:** expose structured console/output events from the production run path before adding same-window browser commands.

## Foundation findings

| Priority | Finding | Evidence | Risk |
|---|---|---|---|
| High | CTest `-L vsim` is not an exact VSIM selection | Current discovery returns 41 because labels include `wo-vsim-*` | Release reports can overstate or misidentify the VSIM suite |
| High | Most parser tests bypass the production executable/library boundary | Multiple CMake entries compile `src/vsim/vsim_parser.cpp` directly | Integration regressions can pass unit tests |
| High | Export flush tests do not cover XYZ/viewer ownership | Group 29 tests event-log files only | HTML/viewer pipeline can fail after parser tests pass |
| Medium | PBC metadata depends on free-form XYZ comments | Group 33 bridge design | Writer/reader drift can silently break runtime bindings |
| Medium | Visual tests stop at configuration/cadence | Group 35 registration | HTML/live transport may not honor declared cadence |
| Medium | Broad intent schema coverage lacks proportional runtime assertions | Group 36 registration | Accepted fields may be inert or only stored in raw sections |
| Low | The reported 26-test count is stale or refers to a different selection | Current discovery mismatch | Historical timing cannot be used as current release evidence |

## Recommended order after finalization

1. Define an exact CTest selection for the VSIM language/runtime surface instead of relying on `-L vsim`.
2. Add one production-bound `.vsim` parse/validate/run smoke test.
3. Add an XYZ artifact contract test that reaches the viewer session factory.
4. Add structured console/output events before implementing browser command input.
5. Add HTML transport tests for session isolation and render cadence.

## Validation performed

- CTest discovery only: `ctest --test-dir build -N -L vsim`
- No VSIM tests executed for this audit.
- GitHub CLI authentication checked successfully for `LMSM3/VSEPR-SIM`.
