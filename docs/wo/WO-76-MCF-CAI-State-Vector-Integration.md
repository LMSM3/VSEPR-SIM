# WO-76 — MCF-CAI State Vector Model: Schema + Sidecar Integration
<!-- VSPER-SIM v5.0.0-main | Status: 🔵 ACTIVE | Step 1 caf_channel rename ✅ done in WO-77 kernel fork | Priority: DESIGN-FIRST -->

## Purpose

Formally integrate the MCF-CAI State Vector Model into the VSPER-SIM
architecture. The model defines every simulated object as a 3×3 state grid:

```
              C (Carrier)      A (Action)      I (Information)
Macro         𝓜_C              𝓜_A             𝓜_I
Chemical      𝓒_C              𝓒_A             𝓒_I
Fundamental   𝓕_C              𝓕_A             𝓕_I
```

MCF = body plan (where state lives).
CAI = identity/entropy bookkeeping inside each layer.

Theoretical specification: `docs/Theoretical/MCF_CAI_State_Vector_Model.tex`

---

## Dependencies

- WO-75A (IKK Report End-Tag Enrichment) — Information column maps to sidecar end-tag
- WO-75B (IKK Identity Vector Phased Impl) — I_* channels map onto Fundamental Carrier / Chemical Carrier cells
- `include/vsim/analysis/identity_sidecar.hpp` — Information column lives here
- `include/vsim/vsim_document.hpp` — VSIM schema structs and defaults
- `src/vsim/vsim_parser.cpp` — Key wiring

---

## Terminology: Namespace Collision Resolution (REQUIRED FIRST)

The abbreviation `CAI` is currently used in the codebase for **Colour-Averaged
Interaction** (force channel, `WO-VSEPR-SIM-EXTREME.tex` lines ~242, 671, 682,
691, 931–936, 953, 1564). This collides with the new CAI = Carrier/Action/Information basis.

### Resolution: rename the force channel

| Old name | New name | Where to change |
|---|---|---|
| `cai_channel` | `caf_channel` | `vsim_document.hpp`, `vsim_parser.cpp`, all `.vsim` scripts using it |
| `CAI` (LaTeX macro for force) | `\CAForce` or `\caiForce` | `WO-VSEPR-SIM-EXTREME.tex`, `IKK_Notation_Registry.tex` |

This rename is a prerequisite for all subsequent steps. Do not introduce
CAI-basis fields until the force-channel rename is confirmed clean.

---

## Steps

### Step 1 — ✅ Rename `cai_channel` → `caf_channel` in source *(done via WO-77 Phase 1)*

- `include/vsim/vsim_document.hpp`: field rename
- `src/vsim/vsim_parser.cpp`: key string rename
- Any `.vsim` example scripts that use `cai_channel`
- Update `VSIM_REFERENCE.md` (field name change)

### Step 2 — Rename LaTeX macros in theory docs

- `docs/Theoretical/WO-VSEPR-SIM-EXTREME.tex`: `\CAI` → `\CAForce` (or similar)
- `docs/Theoretical/IKK_Notation_Registry.tex`: add CAI = Carrier/Action/Information entry

### Step 3 — Add `McfCaiCell` struct to `vsim_document.hpp`

Proposed minimal struct for one grid cell:

```cpp
struct McfCaiCell {
    // Carrier / Configuration
    // Action / Coupling
    // Information / Entropy-loss
    //   ^ information sub-fields are sidecar-only; never force inputs
};
```

Full field list to be derived from `MCF_CAI_State_Vector_Model.tex` Section 4.

### Step 4 — Extend `IdentitySidecarRecord` for Information column

The three `*.information` sub-blocks (`macro.information`, `chemical.information`,
`fundamental.information`) map onto the sidecar, not the particle kernel.

New sidecar fields (additive, do not remove existing):

```cpp
// Macro information column
float formation_age        = 0.0f;
float defect_memory        = 0.0f;

// Chemical information column
std::string formation_route;
float reaction_entropy_loss = 0.0f;

// Fundamental information column
float hidden_W             = 0.0f;
float projection_loss      = 0.0f;  // already exists; confirm mapping
float entropy_loss         = 0.0f;  // already exists; confirm mapping
```

Doctrine: Information column fields remain in sidecar. Never written back to
particle state. Never used as force inputs.

### Step 5 — Wire VSIM parser for `[object.*.carrier/action/information]` blocks

Add to `vsim_parser.cpp`:
- Detect `[object.macro.carrier]`, `[object.macro.action]`, `[object.macro.information]`
- Detect `[object.chemical.*]`, `[object.fundamental.*]` variants
- Route `*.information` keys to sidecar; route `*.carrier` and `*.action` to particle kernel

### Step 6 — Update `VSIM_REFERENCE.md` with MCF-CAI schema

Add a new section: **MCF-CAI Object State Grid**. Document all 9 sub-blocks,
their fields, defaults, and the sidecar doctrine for the Information column.

### Step 7 — Add MCF-CAI grid panel to report end-tag (builds on WO-75A)

In `src/core/report_engine.cpp` and `reporting/generate_report.py`:

- Add `render_mcfcai_grid_md()` — ASCII 3×3 table in Markdown report
- Add `render_mcfcai_grid_tex()` — coloured longtable in LaTeX report
- Values sourced from `IKKEndTag` (WO-75A) + new sidecar Information-column fields

### Step 8 — Add MCF-CAI overlay panel to renderer (builds on WO-75A visual passes)

In `src/vis/renderer.hpp` / `renderer.cpp`:

- `render_mcfcai_overlay()` — ImGui panel showing live 3×3 grid for selected atom/bead
- Each cell shows: value, unit, and whether it is Carrier / Action / Information
- Colour-coded by layer row (green = Macro, blue = Chemical, red = Fundamental)
- Information column cells additionally show `𝔇` and residual if non-zero

### Step 9 — Add `.vsim` example script using full MCF-CAI object block

Create `examples/mcf_cai/carbon_bead_full_state.vsim` demonstrating all 9
sub-blocks with the canonical `carbon_bead` object from the theory document.

---

## Constraints

- Information column = sidecar only. Zero exceptions.
- `caf_channel` rename must be confirmed before introducing any new `cai_*` field names.
- MCF-CAI grid panel in reports is read-only derived output, not editable input.
- Partial grids (only Macro + Chemical rows, no Fundamental) must be valid.
  Absent rows default to zero/null and must not cause parser errors.

---

## Success Criteria

- [x] `cai_channel` fully renamed to `caf_channel` in source and docs; build clean.
- [ ] `[object.*.carrier/action/information]` blocks parseable in VSIM scripts.
- [ ] Information column fields appear in sidecar records; confirmed not in particle kernel.
- [ ] MCF-CAI 3×3 table appears in Markdown and LaTeX report outputs.
- [ ] ImGui MCF-CAI overlay panel renders for selected atom in GL viewer.
- [ ] `VSIM_REFERENCE.md` updated with MCF-CAI schema section.
- [ ] Theory doc `MCF_CAI_State_Vector_Model.tex` compiles cleanly with pdflatex.