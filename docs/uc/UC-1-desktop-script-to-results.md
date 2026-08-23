# UC-1 — Desktop Script-to-Results ("The Daily Driver")
<!-- VSEPR-SIM | v5.0.14 | WO-MF-01 -->

---

## Overview

**Goal:** A researcher has a `.vsim` script on disk.  They open the desktop app,
load the script, press **Run [F7]**, watch the file-network panel populate in
real time, then double-click an output file to open it — all without touching a
terminal.

This is the **primary daily workflow** for interactive desktop use and the
benchmark against which all desktop improvements are evaluated.

---

## Actors

| Actor | Role |
|-------|------|
| Researcher | Interacts with the Qt6 desktop app |
| `vsepr-desktop.exe` | Qt6 application |
| `vsepr.exe` | Headless simulation CLI, spawned by RunVsimPanel |
| Output directory | Filesystem path resolved from `[export] output_dir` |

---

## Preconditions

1. `build/vsepr-desktop.exe` is present (built via `cmake --preset release`).
2. `build/vsepr.exe` is on `PATH` or set via `RunVsimPanel::setCliPath()`.
3. Test fixture: `tests/automation/vsim/uc1_h2o_quick.vsim` exists.

---

## Workflow steps

```
1. Launch vsepr-desktop.exe
   └── MainWindow shows; ObjectTree / File-Network / Run docks visible

2. File > Open VSIM Script  (or drag-drop uc1_h2o_quick.vsim)
   ├── ScriptEditorPanel loads the file
   ├── RunVsimPanel::setVsimPath() called
   ├── resolveOutputDir() parses [export] output_dir
   └── FileNetworkPanel::setRootDir() pointed at resolved dir

3. User clicks ▶ Run [F7]
   ├── RunVsimPanel::startRun() spawns vsepr.exe
   ├── Progress bar activates (indeterminate)
   ├── ConsolePanel streams stdout/stderr
   └── QFileSystemWatcher armed on output dir

4. Files appear in output dir (live)
   ├── onWatcherDirChanged → emitNewArtifacts()
   ├── artifactList_ (QListWidget) populates: trajectory.xyz, analysis.json, manifest.json, ...
   ├── ObjectTree::addResult() called per file
   └── FileNetworkPanel tree refreshes (QFileSystemModel, no poll)

5. Run completes (exit 0)
   ├── Green tick appears in status label
   ├── FileNetworkPanel dock raised
   └── DynxReplayPanel auto-opened if .dynx present

6. User double-clicks trajectory.xyz in FileNetworkPanel
   └── onFileActivated → importXyzPath → viewport shows molecule
```

---

## Expected outputs

| File | Source | Validates |
|------|--------|-----------|
| `out/uc1_h2o/trajectory.xyz` | XYZWriter | Core simulation ran |
| `out/uc1_h2o/analysis.json` | AnalysisJsonWriter | Analysis layer ran |
| `out/uc1_h2o/manifest.json` | ManifestJsonWriter | Export package ran |
| `out/uc1_h2o/report.md` | ReportMdWriter | Report generation ran |
| `out/uc1_h2o/metrics.tsv` | MetricsTsvWriter | Metrics exported |

---

## Missing features exercised

| MF tag | Feature | Impact if absent |
|--------|---------|-----------------|
| MF-A04 | `write_pdb` | No `.pdb` in output — marked PENDING, not FAIL |
| MF-B03 | `render_targets` dispatch | GL overlay inactive — visual only, not tested |
| MF-E01 | IKK GL overlay | Overlay panel empty — marked PENDING |

---

## Automation check sequence

Implemented in `tests/automation/uc1_run.sh`:

```
STEP 1  vsepr run uc1_h2o_quick.vsim --emit-dynx out/uc1_h2o/session.dynx
STEP 2  expect_file  out/uc1_h2o/trajectory.xyz   "Core XYZ output"
STEP 3  expect_file  out/uc1_h2o/analysis.json    "Analysis JSON"
STEP 4  expect_file  out/uc1_h2o/manifest.json    "Export manifest"
STEP 5  expect_file  out/uc1_h2o/report.md        "Markdown report"
STEP 6  expect_file  out/uc1_h2o/metrics.tsv      "Metrics TSV"
STEP 7  expect_pending out/uc1_h2o/h2o.pdb        "MF-A04: PDB writer"   MF-A04
STEP 8  validate_xyz out/uc1_h2o/trajectory.xyz   2   "H2O has 3 atoms per frame"
STEP 9  validate_json out/uc1_h2o/analysis.json       "Analysis JSON is valid"
```

`validate_xyz N` checks that every frame has exactly N+1 atom lines (atom-count
line + N atom lines).  For H2O that is 3.

---

## Acceptance criteria

| Criterion | Pass condition |
|-----------|---------------|
| All PASS checks green | `expect_file` calls all succeed |
| PENDING count matches ledger | Exactly the MF-tagged items are absent |
| FAIL count = 0 | No implemented feature produces wrong output |
| Run wall-clock ≤ 30 s | Headless H2O run is fast |

---

## Fixture script

`tests/automation/vsim/uc1_h2o_quick.vsim` — see that file for the full script.

---

## Future enhancements (post-PENDING resolution)

- MF-A04 resolved → add `expect_file` for `.pdb`
- MF-E01 resolved → add screenshot comparison for GL overlay panel
- Add a second fixture (`uc1_nacl_crystal.vsim`) to cover crystal mode

---

*UC-1 | WO-MF-01 | VSEPR-SIM Desktop Kernel Legacy*
