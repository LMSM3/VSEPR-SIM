# Day 73 Milestone — v5.13.3
## Wizard Full-Module Expansion  |  PR #5  |  `feature/wizard-full-module-expansion`

> **Status:** COMMITTED — merged to `v5.0.0-main`  
> **Tag:** `v5.13.3`  
> **Date:** Day 73  
> **WO:** WO-73  

---

## What Was Delivered

Wizard modules 6, 7, and 8 expanded to full field coverage.  Every field
is sourced directly from `include/vsim/vsim_document.hpp` — no invented
or speculative keys.  The wizard now walks a user through the complete
`.vsim` language surface in a single interactive session.

### Module 6 — Analysis  (`[analysis.*]`)

**+7 fields added to `wizard_modules()` in `src/cli/cmd_new_wizard.hpp`:**

| Field | Section | Type | Default | Notes |
|---|---|---|---|---|
| `neighbor_cutoff_A` | `[analysis.structure]` | Float | `3.5` | Neighbor shell radius for coordination number |
| `contact_cutoff_A` | `[analysis.structure]` | Float | `2.2` | Contact-only cutoff (bonding threshold) |
| `compute_rdf` | `[analysis.sampling]` | Bool | `true` | Radial distribution function |
| `compute_msd` | `[analysis.sampling]` | Bool | `true` | Mean-square displacement |
| `rdf_cutoff_A` | `[analysis.sampling]` | Float | `8.0` | RDF upper integration limit (Å) |
| `msd_max_lag_frames` | `[analysis.sampling]` | Integer | `50` | Max lag for MSD curve |
| `energy_var` / `N_evolution_var` | `[variance]` | Text | — | Variance probe strings (energy + species count) |

### Module 7 — Export  (`[export]` + `[export.visual]`)

**+10 `[export]` fields and +8 `[export.visual]` sub-fields:**

`[export]` additions:

| Field | Type | Default | Notes |
|---|---|---|---|
| `write_xyzfull` | Bool | `false` | Extended property-space XYZ (all NormalisedRecord fields) |
| `write_summary_csv` | Bool | `false` | EmpiricalDB-compatible CSV via CsvOutputHandler |
| `write_metrics_tsv` | Bool | `false` | Per-step metrics table |
| `write_report_md` | Bool | `false` | Structured Markdown report |
| `write_events_json` | Bool | `false` | KernelEventLog dump |
| `write_symbolic_trace_json` | Bool | `false` | IKK-I post-convergence symbolic trace |
| `write_verify_report` | Bool | `false` | Verification pass/fail ledger |
| `compression` | Choice | `"none"` | `none` \| `gz` \| `zst` |
| `split_by_species` | Bool | `false` | One file per species in multi-molecule runs |
| `output_dir` | Text | `"out"` | Root output directory |

`[export.visual]` sub-module (8 new fields):

| Field | Type | Default |
|---|---|---|
| `write_svg_figures` | Bool | `false` |
| `write_png_snapshots` | Bool | `false` |
| `write_rdf_svg` | Bool | `false` |
| `write_energy_trace_svg` | Bool | `false` |
| `write_packing_heatmap_svg` | Bool | `false` |
| `write_trajectory_gif` | Bool | `false` |
| `write_html_dashboard` | Bool | `false` |
| `visual_output_dir` | Text | `"figures"` |

### Module 8 — Visual  (`[visual]`)

**+18 fields — full `VisualSection` coverage:**

| Field | Type | Default | Notes |
|---|---|---|---|
| `output_type` | Choice | `"terminal_chart"` | 7 terminal + 4 GL choices (see below) |
| `animation_mode` | Choice | `"static"` | Fixed (was incorrectly `"none"` in earlier wizard) |
| `show_convergence_trace` | Bool | `true` | Live energy/eta trace |
| `show_bar_chart` | Bool | `true` | Post-step bar summary |
| `show_event_timeline` | Bool | `false` | KernelEventLog timeline |
| `show_symbolic_trace` | Bool | `false` | IKK-I symbolic projection trace |
| `show_steady_state_marker` | Bool | `false` | Stationarity flag overlay |
| `gl_auto_orbit` | Bool | `false` | Camera orbits between overlay panels |
| `gl_spin` | Bool | `false` | Continuous scene spin |
| `gl_spin_axis` | Choice | `"y"` | `"x"` \| `"y"` \| `"z"` |
| `gl_spin_deg_per_s` | Float | `30.0` | Spin rate; negative = reverse |
| `gl_show_axes` | Bool | `true` | XYZ coordinate axes in GL window |
| `gl_window_width` | Integer | `1280` | GL window width (px) |
| `gl_window_height` | Integer | `800` | GL window height (px) |
| `web_port` | Integer | `8080` | HTTP port for `gl_web_streamer` |
| `pacing_ms` | Integer | `0` | Artificial inter-frame delay (ms) |
| `overlay_cycle_interval` | Integer | `200` | Steps between overlay panel switches |

**`output_type` choices wired in module 8:**

| Value | Description |
|---|---|
| `"none"` | Headless; no display |
| `"terminal_chart"` | Live convergence trace + proxy table (default) |
| `"terminal_snapshot"` | Post-run energy/eta bar chart |
| `"terminal_overlay_cycle"` | Full 6-panel `kernel_viz_demo` layout |
| `"terminal_rdf"` | ASCII radial distribution function |
| `"terminal_energy_heatmap"` | 2D ASCII energy landscape |
| `"terminal_defect_map"` | ASCII defect site grid projection |
| `"gl_interactive"` | 3D interactive + ImGui controls |
| `"gl_live_60fps"` | Smooth 60 fps spinning demo |
| `"gl_crystal_grid"` | Periodic crystal unit cell display |
| `"gl_overlay_cycle"` | Cycling density/coordination/energy panels |

---

## Demo Scripts

All demonstration scripts added in `examples/`:

| Script | WO | Exercises |
|---|---|---|
| `wo73_wizard_modules_demo.vsim` | WO-73 | All module 6/7/8 fields annotated |
| `wo74a_gap_classifier_demo.vsim` | WO-74A | `adaptive` + `threshold` gap classifiers, NaCl ionic |
| `wo74b_output_filter_demo.vsim` | WO-74B | All 4 export handlers, WO-73A alias resolution |
| `wo74c_length_scale_fitter_demo.vsim` | WO-74C | 4 fit modes, graphene 5-layer, asymmetry rule |

---

## Counts

| Area | Before WO-73 | After WO-73 | Delta |
|---|---|---|---|
| Wizard fields total | ~47 | ~82 | +35 |
| `[visual]` fields in wizard | 6 | 24 | +18 |
| `[export]` fields in wizard | 8 | 26 | +18 |
| `[analysis.*]` fields in wizard | 4 | 11 | +7 |

---

## Files Changed

| File | Change |
|---|---|
| `src/cli/cmd_new_wizard.hpp` | +35 wizard fields across modules 6, 7, 8 |
| `examples/wo73_wizard_modules_demo.vsim` | New demo script |
| `examples/wo74a_gap_classifier_demo.vsim` | New demo script |
| `examples/wo74b_output_filter_demo.vsim` | New demo script |
| `examples/wo74c_length_scale_fitter_demo.vsim` | New demo script |
| `docs/DAY73_MILESTONE.md` | This file |

---

## Next: Day 74 (v5.13.4)

Open work orders after this milestone:

| WO | Title | Status |
|---|---|---|
| WO-74A | Gap Classifier Refinement | **DONE** — stub file confirms consolidation complete |
| WO-74B | Multi-Scale Output Filter | PENDING — harden `vsim_output_filter.cpp`; scale-layer gating |
| WO-74C | Length Scale Fitter Validation | PENDING — 3+ analytic fixtures in `test_length_scale_fitter.cpp` |
| WO-75A | CH4 Flat Supported Baseline | PENDING — IKK-I formation test |
| WO-75B–F | Precompiler design + asking loop | PENDING — see `VSIM_PRECOMPILER_DESIGN.md` |

Resume from **WO-74B** (output filter hardening).
