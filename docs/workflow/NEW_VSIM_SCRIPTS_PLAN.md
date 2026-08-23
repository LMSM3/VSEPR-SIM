# Repeatable 9-Step Plan: Author Up to Three New VSIM Scripts

> Derived from the WO-89 Three Metallic Pillars parser-first implementation path.
> Use this workflow whenever a new feature needs multiple `.vsim` scripts and the
> current grammar may not accept them verbatim.

[W] NEW_VSIM_SCRIPTS_WORKFLOW  
[D] Day-independent reusable work order  
[I] Implementation path below  
[V] Verified against the Three Metallic Pillars deliverables  
[P] Markdown evidence in `docs/workflow/`  
[N] Execuited on demand; no external blocker

---

## 0. Ground rules

- Primary toolchain: `cmake --preset release`, `ninja -C build`, `build\vsepr.exe`.
- All source files must be UTF-8 NoBOM.
- No `.ps1` or `.sh` scripts via `create_file`; prefer in-place editing or `Set-Content`.
- Keep changes minimal; do not extend runtime beyond what the scripts need.
- Update `docs/VSIM_REFERENCE.md` whenever the schema changes.
- Allocate approximately 33% of feature effort to visualization: every script set must summon an explicit end-of-run presentation and produce replayable, analysis-rich visual evidence.
- Prefer dense `XYZF` trajectory truth (`write_xyzf = true`) with meaningful snapshot cadence, then pair it with SVG/PNG/HTML/dashboard overlays that expose the feature's state, events, and conservation or acceptance metrics.

---

## Step 1: Scope the new scripts

Author a one-page design note at:

```
docs/<feature>/DESIGN.md
```

Contents:

- Feature name and purpose.
- List of up to three target scripts and what each exercises.
- New sections, keys, list syntax, or arrays-of-tables expected.
- Output files and acceptance criteria per script.
- References to any supporting `.tex` document or audit evidence.

### Exit criteria

- [ ] `docs/<feature>/DESIGN.md` exists and is checked into version control.
- [ ] At least three expected sections are listed.

---

## Step 2: Draft the scripts

Create a dedicated directory:

```
mkdir -Force scripts/<feature>
```

Write up to three `.vsim` files:

```
scripts/<feature>/script_a.vsim
scripts/<feature>/script_b.vsim
scripts/<feature>/script_c.vsim
```

Guidelines:

- Follow existing VSIM style (`[project]`, `[simulation]`, `[visual]`, etc.).
- Use multi-line bracketed lists and quoted colonated tokens where needed.
- Keep unknown/new sections in place; validation will expose them.
- Do not simplify grammar for the parser; let the audit decide what is supported.

### Exit criteria

- [ ] Three draft scripts exist.
- [ ] Scripts are syntactically plausible to a human reader.

---

## Step 3: Validate with the installed runtime

Run validation for each draft:

```powershell
cd C:\R\VSPER-SIM
build\vsepr.exe validate scripts\<feature>\script_a.vsim
build\vsepr.exe validate scripts\<feature>\script_b.vsim
build\vsepr.exe validate scripts\<feature>\script_c.vsim
```

Capture:

- Parse errors with line numbers.
- Unknown-section warnings.
- Validation failures (e.g., `fire_max_steps < 1`).

Save a concise diagnostic log at:

```
docs/<feature>/VALIDATION_INITIAL.md
```

### Exit criteria

- [ ] Validation output for all three scripts is recorded.
- [ ] Each script has at least one diagnostic entry or is marked clean.

---

## Step 4: Audit parser and document-model gaps

Compare failures against:

- `src/vsim/vsim_parser.cpp` section dispatch (`handle_section`).
- `src/vsim/vsim_parser.cpp` key-value dispatch (`handle_key_value`).
- `include/vsim/vsim_document.hpp` for existing section structs.

Produce:

```
docs/<feature>/GRAMMAR_AUDIT.md
```

Include:

1. Section dispatch table: section name -> supported / unsupported.
2. Key table for new sections: key -> type (scalar, bool, list, array-of-tables).
3. Syntax notes: bracketed lists, quoted tokens, `[[double.bracket]]` tables.
4. Recommended parser/document changes.
5. Decision: extend parser vs. simplify scripts.

### Exit criteria

- [ ] Audit document is complete.
- [ ] A parser-first path is explicitly chosen or rejected with rationale.

---

## Step 5: Extend parser and document model

Files to modify:

- `include/vsim/vsim_document.hpp`: add new section structs and insert them into `VsimDocument`.
- `include/vsim/vsim_parser.hpp`: add new applier declarations and any required state flags.
- `src/vsim/vsim_parser.cpp`: add section dispatch and key-value dispatch.
- Optional: create `src/vsim/vsim_parser_<feature>.cpp` for appliers.
- `cmake/CoreBuild.cmake`: register any new `.cpp` files.

Typical parser fixes:

- Preserve quoted strings with `parse_value`.
- Accumulate multi-line bracketed lists in `parse_content`.
- Recognize `[[section.entry]]` arrays-of-tables.

### Exit criteria

- [ ] Parser and model compile without errors.
- [ ] New appliers populate new `VsimDocument` fields.
- [ ] Build succeeds after `cmake --preset release` and `ninja -C build`.

---

## Step 6: Re-validate scripts and fix configuration errors

Re-run:

```powershell
build\vsepr.exe validate scripts\<feature>\script_a.vsim
build\vsepr.exe validate scripts\<feature>\script_b.vsim
build\vsepr.exe validate scripts\<feature>\script_c.vsim
```

Fix only configuration-level issues in the scripts (e.g., numeric ranges, missing required keys). Do not change the intended grammar.

If a parse error persists and the cost of parser support is too high, record the concession in `GRAMMAR_AUDIT.md` and adjust the script minimally.

### Exit criteria

- [ ] All three scripts validate cleanly, except for expected warnings.
- [ ] Validation output is appended to `VALIDATION_INITIAL.md` or a new `VALIDATION_FINAL.md`.

---

## Step 7: Wire runtime/launcher behavior

Ensure the runner or launcher uses parsed values:

- Read new `VsimDocument` fields in `src/cli/cmd_run_vsim.cpp` or appropriate runtime file.
- Gate any new behavior on the presence/values of the new sections.
- If a new output/reporting mode is required, add it behind explicit fields.
- Explicitly summon an end-of-run visualization. Interactive runs must open at least a viewer/status window; headless runs must emit and announce a concrete visual artifact path.
- Emit dense replay truth with `write_xyzf = true`, a deliberately chosen `snapshot_interval`, and event/metric cadence sufficient to reconstruct transitions rather than only initial/final states.
- Configure useful complex output: combine trajectory replay with event timelines, state/proxy tables, energy/thermal traces, spatial overlays, and acceptance/conservation summaries appropriate to the domain.
- For viewer/OpenGL paths, avoid blocking the build on unavailable Qt6 dependencies; use `output_type = "terminal_chart"` plus generated SVG/PNG/HTML artifacts, or document the blocker.

### Exit criteria

- [ ] Running a script produces expected output files or deterministic runtime messages.
- [ ] No hard failure due to missing runtime support.
- [ ] Execution explicitly summons a viewer or announces generated visual artifact paths.
- [ ] Dense XYZF trajectory and domain-relevant visual overlays are present and usable.

---

## Step 8: Add tests and update documentation

Tests:

- Add parser-level validation test under `tests/` (e.g., `tests/test_<feature>_vsim_parser.cpp`).
- Register it in `tests/CMakeLists.txt`.
- Keep tests headless; do not require Qt/OpenGL.

Documentation:

- Update `docs/VSIM_LANGUAGE.md` with new section/key examples.
- Update `docs/VSIM_REFERENCE.md` with formal schema additions.
- Update `VSIM_DEVELOPMENT.md` if developer workflow changed.

### Exit criteria

- [ ] New test compiles and passes.
- [ ] All three reference documents are updated.
- [ ] Documentation cross-references `docs/<feature>/DESIGN.md` and `GRAMMAR_AUDIT.md`.

---

## Step 9: Final build/test gate and handoff

Run:

```powershell
cd C:\R\VSPER-SIM
cmake --preset release
ninja -C build
build\tests\test_<feature>_vsim_parser.exe
build\vsepr.exe validate scripts\<feature>\*.vsim
```

Produce final status markers:

```
[W] <feature> — new VSIM scripts and grammar extension
[D] <day or independent>
[I] Parser/document/runtime changes applied per GRAMMAR_AUDIT.md
[V] Build green; three scripts validate; new test passes
[P] docs/<feature>/ + reference docs updated; scripts committed
[N] Next: implement domain kernels for new sections or extend validation rules
```

### Exit criteria

- [ ] Release build passes.
- [ ] New unit test passes.
- [ ] All three scripts validate.
- [ ] Documentation is internally consistent and committed.

---

## Quick checklist

- [ ] Design document written.
- [ ] Three scripts drafted.
- [ ] Initial validation diagnostics captured.
- [ ] Grammar audit complete.
- [ ] Parser/document model extended.
- [ ] Scripts re-validated.
- [ ] Runtime wired.
- [ ] Tests and docs updated.
- [ ] Final build/test gate passed.

---

## Notes

- Preserve legacy behavior: never remove existing section dispatch unless explicitly requested.
- If a new feature requires runtime kernels that do not yet exist, complete the parser and validation path first; leave a stub or no-op runtime branch behind an explicit flag.
- This workflow intentionally separates grammar acceptance from scientific correctness so scripts can be merged and iterated independently of full model implementation.
