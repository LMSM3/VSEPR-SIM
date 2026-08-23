# VSIM-83 Script Expansion Audit

**Branch:** `feature/wizard-full-module-expansion`
**Area:** `apps/`, `include/cli/`, `src/cli/`, `tests/`, `scripts/`
**Status:** Part A audit complete; Part B/D preview scaffold implemented
**Created:** 2026-07-02

---

## Planning Boundary

The VSIM-83 label is planning metadata. Runtime-facing source names use neutral
terms such as `script_expansion`, `ScriptExpansion`, and `expand`.

---

## Current Script Read Path

The located source repo did not have the documented full `.vsim`
validate/run/audit runtime path wired into the unified CLI.

Before this pass:

```text
vsepr <SPEC> <ACTION> [OPTIONS]
```

was the active CLI shape. Passing a `.vsim` file path as either the first or
second positional argument fell into formula/action parsing and failed before
any script-aware handling could happen.

After this pass:

```text
vsepr expand <input.vsim>
```

is handled before formula parsing. It reads a flat `.vsim` file, infers
lightweight expansion candidates, prints a preview, and does not execute the
script or any derived candidate.

---

## Current Parse/Run Boundary

The new preview path is deliberately separate from the existing direct command
path:

```text
vsepr H2O@molecule relax ...
```

continues through the existing `CommandParser`, `RunContext`, and action
handlers.

```text
vsepr expand file.vsim
```

routes to `run_script_expansion_preview()` before `CommandParser` sees the
arguments. This is the safe pause point between script read and execution.

---

## Proposed Expansion Insertion Point

The expansion layer belongs between file read/parse and simulation execution:

```text
script path
  -> lightweight flat-script preview parser
  -> ScriptExpansion
  -> formatted candidate summary
  -> no selected execution
```

Future runtime wiring can replace the lightweight preview parser with the real
`.vsim` document parser while keeping `ScriptExpansion` as the boundary object.

---

## Data Structures Available After Parsing

Added neutral source-facing types:

```cpp
enum class ExpansionKind;
struct ExpansionCandidate;
struct ScriptExpansion;
```

Current candidate fields:

```text
kind
label
reason
confidence
executable
selected
```

Current expansion summary fields:

```text
source_name
candidates
missing_property_count
executable_candidate_count
report_annotation_count
ai_ready
```

The implementation also has a private lightweight preview shape that stores
sections, scalar key/value pairs, and observed metric names. It is not exposed
as a stable runtime API.

---

## Missing Data Needed For Expansion

Current preview can infer from text only. It does not yet have:

- a real `.vsim` AST or document object
- atomistic `State` from the script
- registry-resolved material records
- bond order
- formal charge
- selected/default expansion policy
- report/export packet writer
- AI inlet/outlet schema

These are intentionally not faked. Candidates that would require runtime state
are marked non-executable in preview output.

---

## AI Hook Point Notes

The first AI-ready boundary is the `ScriptExpansion` object:

```text
source script metadata
candidate labels
candidate reasons
confidence values
selected/unselected state
execution-safe flags
```

Future AI input should consume this object plus parsed script sections and any
resolved material/classify outputs. Future AI output should suggest annotations,
missing-property patches, candidate selection, or next-run proposals without
mutating simulation truth state directly.

---

## Risks Of Accidental Full Fanout Execution

The preview implementation blocks fanout by design:

- `direct_execution` prints `not run`
- every generated candidate defaults to `selected = false`
- current runtime-derived candidates are `executable = false`
- `selected_execution` prints `none`
- there is no loop that dispatches candidates into action handlers

This keeps the original script as the anchor and prevents a preview command from
launching many simulations.

---

## New Files

| File | Purpose |
|---|---|
| `include/cli/script_expansion.hpp` | Declares `ExpansionKind`, `ExpansionCandidate`, `ScriptExpansion`, preview formatting, and CLI preview entry point. |
| `src/cli/script_expansion.cpp` | Implements flat `.vsim` preview parsing, candidate inference, summary counting, and formatted output. |
| `tests/test_script_expansion.cpp` | C++ regression proving preview reads a real-like `.vsim` fixture, generates candidates, and does not select execution. |
| `scripts/demo_script_expansion_preview.py` | Runnable demo wrapper for `vsepr expand` against the real classify acceptance script. |
| `docs/VSIM-83-script-expansion-audit.md` | This audit, insertion-point record, and complete change ledger. |

---

## Changed Files

| File | Change |
|---|---|
| `apps/vsepr.cpp` | Added `expand <input.vsim>` help text and early dispatch before formula parsing. |
| `cmake/CoreBuild.cmake` | Added `src/cli/script_expansion.cpp` to the `vsepr_cli` static library. This file is currently ignored by `.gitignore` through the `*.cmake` rule, but the local build uses it. |
| `tests/CMakeLists.txt` | Added `test_script_expansion` and registered `ScriptExpansionPreviewTest`. |
| `src/cli/actions_form.cpp` | Added missing `<algorithm>` include needed by existing `std::min` initializer-list and `std::clamp` usage. |

---

## Current Preview Output Contract

The preview command prints:

```text
Script expansion preview

source: <filename>
parse_status: ok
direct_execution: not run
expansion_candidates: <N>
missing_properties: <N>
executable_candidates: <N>
report_annotations: <N>
ai_ready: true|false

[0] <ExpansionKind>
    label: <label>
    reason: <reason>
    confidence: <0.00-1.00>
    executable: true|false
    selected: true|false

selected_execution: none
result: PASS
```

---

## Verification Commands

```text
cmake --build build --target vsepr test_script_expansion -j 4
ctest --test-dir build -R ScriptExpansionPreviewTest --output-on-failure
python scripts/demo_script_expansion_preview.py
.\build\vsepr.exe H2O@gas emit --cloud 3 --box 10,10,10 --out build\script_expansion_direct_check.xyz
```

Expected result:

```text
direct_execution: not run
selected_execution: none
result: PASS
normal formula/action routing still reaches the existing emit handler
```

---

## Exit Gate Status

```text
[x] current script read path documented
[x] parse/run boundary documented
[x] expansion insertion point identified
[x] ScriptExpansion type exists
[x] ExpansionCandidate type exists
[x] ExpansionKind enum exists
[x] expansion summary formatter exists
[x] safe preview mode exists
[x] preview does not execute all candidates
[x] demo script exists with neutral runtime-facing name
[x] direct run path remains unchanged by dispatch order
[ ] real .vsim validate/run/audit runtime bridge exists
[ ] classifier data attaches from atomistic State after runtime parse
[ ] report/export packet writer emits expansion results
```
