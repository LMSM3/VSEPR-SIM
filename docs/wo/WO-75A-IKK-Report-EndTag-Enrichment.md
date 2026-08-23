# WO-75A — IKK/MIR Physics Report End-Tag Enrichment
## Reporting Modules + Visual Model Elaboration — Distinguishability-Centred Output

**Work order:** WO-75A  
**Branch:** v5.0.0-main  
**Status:** ✅ PARTS A + C + D + Deliverable 9 COMPLETE — Part B deferred  
**Part A completed:** Day 75  
**Parts C / D / Del-9 completed:** Day 82 (v5.13 freeze finalisation)  
**Depends on:** WO-SM-IDENTITY, WO-OUTPUT-P2 (✅ done), identity_sidecar.hpp (v5.1.4)  
**Blocks:** WO-75B (Phase 3), WO-76 (Step 7), WO-77 (Phase 4)  
**Theory refs:** IKK I–IV, IKKIII Matrix-Wave Bridge, IKK Notation Registry, vsim_bridge_doc.tex  

### Part A — VSIM Scripting Layer  ✅ COMPLETE

| Deliverable | File | Status |
|---|---|---|
| `VsimIkkEndTagSection` struct in `vsim_document.hpp` | `include/vsim/vsim_document.hpp` | ✅ |
| `apply_ikk_end_tag_key()` parser wiring | `src/vsim/vsim_parser.cpp` + `.hpp` | ✅ |
| `IKKEndTag` struct + free functions | `include/vsim/analysis/ikk_end_tag.hpp` | ✅ |
| `IkkEndTagModule` self-registering class | `include/vsim/analysis/ikk_end_tag.hpp` + `.cpp` | ✅ |
| `AnalysisRecord::ikk_end_tag_md/tex` output fields | `include/vsim/analysis/i_analysis_module.hpp` | ✅ |
| `AnalysisRecord::ikk_sidecar` attachment point | `include/vsim/analysis/i_analysis_module.hpp` | ✅ Day 82 |
| `IkkEndTagModule::run()` uses `rec.ikk_sidecar` | `src/vsim/analysis/ikk_end_tag.cpp` | ✅ Day 82 |
| `write_ikk_end_tag()` file-writer free function | `include/vsim/analysis/ikk_end_tag.hpp` + `.cpp` | ✅ Day 82 |
| 20 tests Group 87 | `tests/test_ikk_end_tag.cpp` | ✅ ALL PASS |
| `VSIM_REFERENCE.md` field table | `VSIM_REFERENCE.md` | ✅ |
| `docs/VSIM_LANGUAGE.md` semantic section | `docs/VSIM_LANGUAGE.md` | ✅ |
| `CLOSEOUT.md` status update | `CLOSEOUT.md` | ✅ |

### Part B — GL Overlay  🔲 DEFERRED (next arc)

D-colour overlay pass (`RENDER_PASS_DIST`), ImGui legend panel, scale ladder bar.  
See §B in this document for spec.

### Part C — Python Time-Series Plot  ✅ COMPLETE

`plot_dist_timeseries()` + PNG embed in `reporting/generate_report.py`.  
Dual-axis D_rec / entropy proxy; amber identity-loss band; D = 0.5 threshold.

### Part D — LaTeX Macro Set  ✅ COMPLETE (Day 82)

IKK notation macros added to `reporting/report.tex` preamble:
`\Dfrak`, `\Ifrak`, `\etaab`, `\Psihid`, `\Sn{}`, `\ikkendsection{}{}`
badge macros `\Dpass{}`, `\Dwarn{}`, `\Dfail{}`.

### Deliverable 9 — Layering Report eta_ab Column  ✅ COMPLETE (Day 82)

`discover_ikk_summaries()` + `\IKKSummaryTable` added to
`reporting/generate_layering_report.py`; per-run D_rec / η_ab table
emitted to `reporting/layering_data.tex`.

---

---

## Objective

Enrich every simulation report end-tag block and all visual overlay models to actively
demonstrate IKK/MIR/MIT physics, especially fraktur-D (distinguishability), rather than
only logging it as a raw scalar. Reports should read as theoretical physics documents, not
just data dumps. The visual models should make projection loss, hidden residuals, and
scale-crossing immediately legible to a viewer with no prior IKK knowledge.

---

## Motivation

Current gap: the IdentitySidecarRecord (identity_sidecar.hpp) already computes dataloss,
dataentropy, recoverable_info, projection_loss, and hidden_channel per frame. The
report_engine.cpp writes structured CSV and JSON. The renderer renders atoms as plain
spheres with CPK colouring only.

None of these outputs explain what the numbers mean in terms of IKK theory, and none of
the visual passes show the identity layer at all. After WO-75A, a completed run must:

  1. Close every report section with a structured IKK End-Tag Block that interprets
     D / projection-loss / hidden-residual values in physical language.
  2. Render an optional D Overlay Pass in the GL viewer: per-atom colour mapped to
     distinguishability, with a scale bar, legend, and theory annotation.
  3. Emit a Scale Ladder Summary panel in both the Markdown report and the viewer
     showing the Sn regime, eta_ab efficiency, and what the hidden residual represents.
  4. Add a D Time Series plot (PNG + inline Markdown figure) to the Python report
     pipeline showing Delta-D_rec across the trajectory alongside Delta-S.

---

## Scope

### A — Report End-Tag Block

Every report section produced by report_engine.cpp, generate_report.py, and
generate_layering_report.py must append a standardised closing block.

Structure of the IKK End-Tag Block:

  Scale regime      : S_n = (D_n, M_n, L_n)   [atomistic | molecular | material]
  D mean            : <value>   recoverable distinguishability fraction
  D loss (Delta-D)  : <value>   identity content lost across this run
  Projection loss   : <value>   loss from atomistic to observable projection
  Hidden residual   : <value>   Psi^hid estimate
  eta_ab efficiency : <value>   cross-scale recovery fraction in [0,1]
  Entropy Delta     : <value>   Delta-S proportional to -Delta-D_rec (IKK 2nd law)
  Formation memory  : <value>   fraction of formation history still recoverable

  IKK interpretation: prose line generated from values above.
  Second-law check:  Delta-S > 0 iff Delta-D < 0 -> [PASS | WARN | FAIL]

New C++ surface in report_engine.hpp:

  struct IKKEndTag {
      double D_mean;
      double D_loss;
      double projection_loss;
      double hidden_residual;
      double eta_ab;
      double entropy_proxy;
      double formation_memory;
      std::string scale_regime;
      std::string interpretation;
      bool second_law_pass;
  };

  std::string render_ikk_end_tag_md(const IKKEndTag& tag);
  std::string render_ikk_end_tag_tex(const IKKEndTag& tag);
  IKKEndTag   build_ikk_end_tag(const IdentitySidecarSeries& sidecar,
                                 const std::string& scale_regime);

Implementation targets:

  src/core/report_engine.cpp          - add write_ikk_end_tag() after each section
  src/core/bio_report_engine.cpp      - bio variant with coalescence-loss emphasis
  reporting/generate_report.py        - append IKK block to each Markdown section
  reporting/generate_layering_report.py - add per-layer D + eta_ab row to table
  reporting/report.tex                - LaTeX template ikkendsection macro

---

### B — D Visual Overlay Pass (GL renderer)

Add RENDER_PASS_DIST to renderer.cpp and renderer_classic.cpp.

Colour mapping (cold=preserved, hot=lost — IKK convention):
  D = 1.0  ->  deep blue      full identity preserved
  D = 0.75 ->  cyan
  D = 0.5  ->  green
  D = 0.25 ->  amber
  D = 0.0  ->  red            identity fully dissolved

ImGui legend panel (right side):
  - D value per atom as colour
  - Run mean D, eta_ab, |Psi^hid|, entropy Delta-S with IKK 2nd law check badge
  - IKK IV section reference line

Scale ladder panel (bottom bar):
  S0(existence) --- S2(color) --- S3(EM/spatial) --- S4(weak) --- S_mat
                                        ^ YOU ARE HERE
    eta_ab = 0.91   hidden residual = 0.13   Delta-D = -0.11

New renderer surface (renderer.hpp):

  enum class RenderPass { STANDARD, DIST_OVERLAY, HIDDEN_RESIDUAL, SCALE_LADDER };

  void set_render_pass(RenderPass pass);
  void set_dist_data(const std::vector<float>& per_atom_dist);
  void set_hidden_residual_data(const std::vector<float>& per_atom_hid);
  void render_dist_legend(int width, int height);
  void render_scale_ladder_bar(int width, int height, const std::string& active_regime);

Toggle: D key cycles STANDARD -> DIST_OVERLAY -> HIDDEN_RESIDUAL -> SCALE_LADDER -> STANDARD

---

### C — D Time Series Plot (Python pipeline)

Add plot_dist_timeseries() to reporting/generate_report.py.

Reads .identity.json sidecar per run. Produces dual-axis matplotlib figure:
  - Left axis:  D_rec over simulation steps — blue line
  - Right axis: entropy_proxy (Delta-S) — red dashed line
  - Amber band: region where Delta-D < 0 (identity loss zone)
  - Reference line: D = 0.5 (IKK IV property activation threshold)

Saved as out/<run_id>/dist_timeseries.png and embedded in report.md.
Caption auto-generated from IKK end-tag data fields.

---

### D — LaTeX Report IKK Macro Set

Add to reporting/report.tex preamble:

  \newcommand{\Dfrak}{\mathfrak{D}}
  \newcommand{\Ifrak}{\mathfrak{I}}
  \newcommand{\etaab}{\eta_{ab}}
  \newcommand{\Psihid}{|\Psi^{\mathrm{hid}}\rangle}
  \newcommand{\ikkendsection}[2]{...}  % styled IKK end block with coloured badges
  \newcommand{\Dpass}[1]{\colorbox{green!20}{\texttt{#1}}}
  \newcommand{\Dwarn}[1]{\colorbox{yellow!40}{\texttt{#1}}}
  \newcommand{\Dfail}[1]{\colorbox{red!20}{\texttt{#1}}}

---

## Constraints

- IKK end-tag data is derived from IdentitySidecarRecord only — never written back
  into particle state or .xyz / .xyzFull truth files (identity_sidecar.hpp doctrine)
- D overlay is viewer-side only — no particle positions modified
- All symbols must follow IKK Notation Registry v1.0
- w != physical length scale (IKK IV coordinate correction) — never label w as scale axis

---

## Deliverables

  1  IKKEndTag struct + build/render functions     src/core/report_engine.hpp/.cpp
  2  Bio variant end-tag                           src/core/bio_report_engine.cpp
  3  Python end-tag block appender                 reporting/generate_report.py
  4  LaTeX macro set + ikkendsection               reporting/report.tex
  5  D colour overlay pass                         src/vis/renderer.cpp, renderer_classic.cpp
  6  ImGui D legend panel                          src/vis/renderer.hpp + ImGui calls
  7  Scale ladder bottom bar                       same renderer
  8  plot_dist_timeseries() + PNG embed            reporting/generate_report.py
  9  Layering report eta_ab column                 reporting/generate_layering_report.py

---

## Acceptance Criteria

- Every generated Markdown report contains an IKK end-tag block per section
- D overlay is toggleable via D key in the GL viewer
- Second-law check prints PASS/WARN/FAIL in report and console
- dist_timeseries.png is generated and embedded for any run with sidecar data
- Scale ladder panel shows correct Sn regime from the .vsim material block
- No IKK-layer variable is written back into .xyz or .xyzFull truth state