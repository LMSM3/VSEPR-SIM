# XSIM Export Package — Design Record
<!-- packages/export/EXPORT_PACKAGE_DESIGN.md -->
<!-- Branch: feature/wizard-full-module-expansion | v5.0.14 -->

| Field       | Value                                        |
|-------------|----------------------------------------------|
| Package     | `xsim::xport`                                |
| Skeleton    | `packages/export/`                           |
| Status      | Skeleton complete — implementation pending   |
| Relates to  | `vsim::ExportSection`, `VsimRuntime`, WO-74B |

---

## 1. What the skeleton is

`packages/export/` is a **self-contained, buildable static library** (`xsim_xport`)
that wraps the export capability of the simulation pipeline in a modular package form.

It compiles cleanly today. It does not yet write real simulation data.

```
export.x                          package identity card
include/xsim/export/
  xsim_export_config.hpp          ExportConfig  — flags + defaults
  xsim_export_job.hpp             ExportJob     — per-run parameters
  xsim_export_result.hpp          ExportResult  — written_files, warnings, errors
  xsim_export_registry.hpp        IExporter + ExportRegistry
  xsim_export_x.hpp               ExportXLoader — export.x parser
  xsim_export_xyz.hpp             XYZExporter   (built-in, skeleton)
  xsim_export.hpp                 ExportPackage — public entry point
src/
  xsim_export_x.cpp               .x format parser (complete)
  xsim_export_registry.cpp        add() / find() (complete)
  xsim_export_xyz.cpp             creates empty out/export/run.xyz (skeleton)
  xsim_export.cpp                 dispatch loop (complete)
```

The `.x` parser is complete. The registry wiring is complete.
`XYZExporter::run()` creates the output directory and an empty file — it does
not yet read frame data. Every other exporter (`analysis_json`, `metrics_tsv`,
`report_md`, `events_json`, `manifest_json`, `verify_report`, `verify_tsv`)
is declared in the registry dispatch but has no registered implementation.

---

## 2. Relationship to the existing pipeline

The project already has a working export system. This is the map:

| Layer              | Existing                           | Package (`xsim::xport`)             |
|--------------------|------------------------------------|--------------------------------------|
| Schema             | `vsim::ExportSection`              | `xsim::xport::ExportConfig`          |
| Parser wiring      | `vsim_parser.cpp` apply_export_key | `ExportXLoader` (reads `export.x`)   |
| Runtime dispatch   | `VsimRuntime::resolve_export_profile` | `ExportPackage::run(ExportJob)`   |
| Format interface   | `IOutputFormatHandler` (WO-74B)    | `IExporter` (this package)           |
| Format registry    | `ModuleRegistry<IOutputFormatHandler>` | `ExportRegistry`                 |

These two systems are **parallel, not yet connected.**

`ExportConfig` field names mirror `ExportSection` field names deliberately so a
bridge translation function can be written as a flat assignment — no inference needed.

---

## 3. Design decisions made

### 3.1 Namespace: `xsim::xport`

`export` is a reserved C++ keyword. `xsim::xport` is short, unambiguous,
and not fighting the language.

### 3.2 `ExportXLoader` returns `ExportConfig` only

The `export.x` file contains package metadata (provides, requires, entry) that
has no C++ type at this layer. The loader reads it, ignores it, and returns only
the `default:` section as `ExportConfig`. This is correct. The metadata is
informational for tooling and documentation, not for the runtime.

If metadata becomes required at runtime (e.g., dependency resolution), the return
type should expand to an `ExportPackageDecl` struct, with `ExportConfig` as a
field inside it. Do not change `ExportXLoader`'s signature until that decision is made.

### 3.3 `ExportJob` separates config from execution

`ExportConfig` = what the package is configured to produce (flags, stride, output dir).
`ExportJob` = the parameters of one specific export operation (which run dir, which frames).

This split matters because the same package can be run against different jobs in a batch
loop without reconfiguring each time. It mirrors the existing separation between
`ExportSection` (static config) and the runtime's per-run dispatch.

### 3.4 `ExportResult` — no silent failure

Every error is named. Every written file is listed. `elapsed_seconds` is recorded.
This makes the result inspectable by tests, by the dashboard layer, and by the manifest.

### 3.5 `XYZExporter` is a skeleton, not a stub

It creates the output directory and the output file. It does not write garbage.
It signals `success = true` and lists the path so the result is inspectable.
The missing piece is: what does it read from `job.run_dir`?

That question is answered in §4.1 below.

### 3.6 `export.x` uses space-separated `key value` in `default:`

Not `key = value` (that is the `.vsim` style). Not `key: value` (YAML-adjacent).
Space-separated is what the user's specification states and what the parser implements.
All other sections (provides, requires, entry) use bare indented tokens or
`key value` pairs. The parser handles all of these correctly.

### 3.7 `ExportPackage` owns the registry and pre-registers built-in exporters

The constructor registers `XYZExporter`. Downstream exporters are registered there
as they are implemented. This is a factory — not a plugin loader, not a god-class.
External packages can call `registry_.add()` if they need to, but that requires
making `registry_` public or adding a `register_exporter()` forwarding method.
That decision is deferred until there is an actual need.

---

## 4. Open decisions — implementation required

### 4.1 What does `ExportJob::run_dir` contain?

This is the most important unresolved question. `XYZExporter::run()` needs to
read particle data from somewhere. The current simulation pipeline writes:
- `out/<name>/<name>.xyz`
- `out/<name>/<name>.xyzf`
- `out/<name>/<name>.analysis.json`
- etc.

**Option A — run_dir is the simulation output directory**
`XYZExporter` reads `run_dir + "/*.xyz"` or a specific known filename.
Simple. Works today. Tight coupling between exporter and output layout.

**Option B — run_dir contains a binary frame cache (`.xsim.bin`)**
The simulation writes a compact binary frame log during the run.
`XYZExporter` reads that, re-emits `.xyz` with stride control.
Cleanest separation. Requires defining the binary format.

**Option C — ExportJob carries a live frame pointer**
`ExportJob` gets a `const SimState* state` field.
`XYZExporter` reads the live state directly.
Simplest for single-run use. Useless for post-run or batch replay.

**Recommendation:** Start with Option A. It requires zero new infrastructure.
Migrate to Option B when the `.dynx` / `.xsim.bin` format is defined.
Option C is a dead end for any replay or batch use case.

### 4.2 Connecting `ExportPackage` to `VsimRuntime`

Currently, `VsimRuntime::resolve_export_profile()` fills an `ExportSection`.
`ExportPackage` works from an `ExportConfig`. These are the same flags with
the same names. A bridge is one function:

```cpp
// vsim/export/xsim_export_bridge.hpp  (to be created)
ExportConfig config_from_export_section(const vsim::ExportSection& s);
ExportJob    job_from_vsim_doc(const vsim::VsimDocument& doc);
```

This bridge is the wiring point. Until it exists, `ExportPackage` is standalone
and not called by the interpreter.

**Decision needed:** Where does this bridge live?
- In `packages/export/` as a new header — clean, keeps the package self-contained.
- In `src/vsim/` as a translation layer — keeps `packages/` free of vsim includes.

Recommendation: `packages/export/include/xsim/export/xsim_export_bridge.hpp`
that includes `vsim/vsim_document.hpp`. The bridge is part of the package, not
part of the core runtime. If that header dependency is undesirable, use Option B
in §4.1 and pass data through `run_dir` files only — no header crossing needed.

### 4.3 Unimplemented exporters

These are declared in `ExportPackage::run()` but have no registered `IExporter`:

| Key              | Flag                    | Existing writer?                        |
|------------------|-------------------------|-----------------------------------------|
| `analysis_json`  | `write_analysis_json`   | `src/vsim/io/` — partial               |
| `metrics_tsv`    | `write_metrics_tsv`     | `IOutputFormatHandler` (WO-74B pending) |
| `report_md`      | `write_report_md`       | `src/vsim/io/` — partial               |
| `events_json`    | `write_events_json`     | `src/vsim/io/` — partial               |
| `manifest_json`  | `write_manifest_json`   | Not wired                               |
| `verify_report`  | `write_verify_report`   | Not implemented                         |
| `verify_tsv`     | `write_verify_tsv`      | Not implemented                         |

`ExportPackage::run()` emits a warning for each missing exporter but does not fail.
This is intentional — partial export runs are recoverable.

The right sequence for implementing these is: `metrics_tsv` first (WO-74B is
already targeting it), then `analysis_json`, `report_md`, `events_json`,
`manifest_json`. `verify_report` and `verify_tsv` are gate-level outputs
and should be last.

### 4.4 `master.x` minimal bundle format is not yet parsed

`examples/xsim_bundles/ikk1_nh3/master.x` uses a new syntax:

```
use package export
run dynamic
steps 100000
export xyz stride 10
```

This is **not** the `XBUNDLE <version>` format read by `XBundleReader`.
It is also not a `.vsim` script.

It is a new script format that needs its own parser if it is to be executed.

**Option A — add a `MasterXReader` alongside `XBundleReader`**
New `include/vsim/bundle/master_x_reader.hpp` + `src/xbundle/master_x_reader.cpp`.
Parses `use package`, `run`, `export` directives into a `MasterXDocument`.
The runtime translates `MasterXDocument` → `VsimDocument` + `ExportPackage`.

**Option B — the minimal bundle format compiles down to a `.vsim` script**
A pre-processor reads `master.x`, resolves `use package` declarations,
and emits a fully-expanded `.vsim` file. Then the normal interpreter runs it.
Simpler, but adds a build step.

**Option C — do not implement `master.x` execution, keep it documentation only**
`master.x` is a conceptual example showing what a user-facing bundle command
looks like. The `.X` bundle format (`xbundle/`) is what actually runs.
`master.x` serves as design intent, not production input.

**Recommendation:** Option C until the package system has real consumers.
Option A when at least two packages exist and the `use package` directive
would do real work (dependency resolution, registry loading).

### 4.5 Export profiles vs. package defaults

`VSIM_LANGUAGE.md` defines four export profiles:

| Profile          | Flags set                                                     |
|------------------|---------------------------------------------------------------|
| `minimal`        | `write_xyz`                                                   |
| `standard`       | `+ write_analysis_json`, `write_metrics_tsv`, `write_report_md` |
| `research_report`| `+ write_events_json`, `write_manifest_json`, `write_dashboard_svg` |
| `publication`    | `+ write_symbolic_trace_json`, `write_pipeline_audit_jsonl`   |

The package `export.x` `default:` section currently mirrors `standard` plus
`write_events_json` and `write_manifest_json` — roughly `research_report`
minus `write_dashboard_svg`.

**Decision:** Should `export.x` defer to a named profile
(`default_profile research_report`) rather than listing flags individually?

That would let a single `VsimRuntime::resolve_export_profile()` call populate
both `ExportSection` (legacy path) and `ExportConfig` (package path).

This is the cleanest long-term design but requires:
1. Adding profile name support to `ExportXLoader`
2. Calling `resolve_export_profile()` from the loader or from the bridge

Recommendation: Do not add this until the bridge (§4.2) exists. Adding profile
resolution to the loader without a bridge creates a hidden dependency on
`vsim_runtime.hpp` inside the package.

---

## 5. Integration path (recommended sequence)

```
Phase 1  XYZExporter reads from run_dir (Option A, §4.1)
		 Write 10 lines: atom count, comment, coordinates from a plain .xyz file
		 Add a test: ExportPackage::run() on a known run_dir, check written_files

Phase 2  Bridge to VsimRuntime (§4.2)
		 xsim_export_bridge.hpp — config_from_export_section + job_from_vsim_doc
		 Wire in vsim_interpreter.cpp after resolve_export_profile()
		 ExportPackage replaces direct VsimRuntime export dispatch for new paths

Phase 3  Implement metrics_tsv exporter (WO-74B alignment)
		 MetricsTsvExporter : IExporter — register in ExportPackage ctor
		 Reads AnalysisRecord from run_dir/*.analysis.json, writes *.metrics.tsv

Phase 4  Implement analysis_json, report_md, events_json, manifest_json
		 One exporter per flag, same pattern

Phase 5  master.x parser (Option A, §4.4) if use package sees real adoption
```

Each phase is one work order. None of them require changing the existing headers.

---

## 6. What must NOT change about the skeleton

These decisions are settled. Revisiting them creates churn for no gain.

- `namespace xsim::xport` — non-negotiable, `export` is a keyword
- Header naming: `xsim_export_<feature>.hpp` — globally unique, no exceptions
- `ExportResult` always populated — no silent failure, ever
- `ExportJob` and `ExportConfig` are separate types — do not merge them
- `export.x` is a package declaration, not a run script
- The `.x` parser does not link against `vsim_document.hpp` or `vsim_runtime.hpp`

---

## 7. Files that need to be updated when the first real exporter lands

| File                        | Change required                                            |
|-----------------------------|------------------------------------------------------------|
| `export.x`                  | No change needed — defaults are already correct            |
| `xsim_export_config.hpp`    | No change needed                                           |
| `xsim_export.cpp`           | Register new exporter in constructor                       |
| `packages/export/CMakeLists.txt` | Add new `.cpp` to source list                         |
| `VSIM_REFERENCE.md`         | Add xsim::xport package entry under Export layer           |
| `docs/VSIM_LANGUAGE.md`     | Note package path as alternative to direct ExportSection   |
| This document               | Update Phase status in §5                                  |

---

*This document is the authoritative design record for the `xsim::xport` skeleton.
It supersedes any inline comments in the source files on questions of intent.*
