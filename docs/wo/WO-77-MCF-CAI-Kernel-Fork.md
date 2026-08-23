# WO-77 — MCF-CAI Kernel Fork: Simplified Kernel Using MCF-CAI as Primary Persistent State
<!-- VSPER-SIM v5.0.0-main | Status: Phase 1+2+3 files ✅ ON DISK (BLUE) | Phase 4+5 pending | Priority: ARCHITECTURE -->

## Purpose

Create a clean-room fork of the simulation kernel where the **MCF-CAI 3×3 state grid
is the sole primary persistent state of every simulated object**.

The existing `SimulationState` in `src/sim/sim_state.hpp` (flat SoA arrays +
ad-hoc object structs) is **not modified** — this fork lives in a parallel
subtree and is developed independently.

Theory specification: `docs/Theoretical/MCF_CAI_State_Vector_Model.tex`
Integration specification: `docs/wo/WO-76-MCF-CAI-State-Vector-Integration.md`

---

## What was created (Phase 1 — complete)

| File | Size | Purpose |
|---|---|---|
| `include/vsim/kernel_mcf/mcf_cai_object.hpp` | 14 062 B | **Primary state struct**: all 9 MCF-CAI cells + McfCaiObject composite + factory helpers |
| `include/vsim/kernel_mcf/mcf_cai_world.hpp` | 10 430 B | World container, OrthoBox, McfCaiSoA, McfCaiFrame, McfCaiParams, McfCaiStats |
| `include/vsim/kernel_mcf/mcf_cai_integrator.hpp` | 5 560 B | IMcfCaiIntegrator, VelocityVerletIntegrator, FireIntegrator, make_integrator factory |
| `include/vsim/kernel_mcf/mcf_cai_sidecar.hpp` | 6 027 B | McfCaiSidecar (Information-column update logic), McfCaiSidecarSummary |
| `src/kernel_mcf/mcf_cai_world.cpp` | 8 932 B | World implementation: add, find, wrap, SoA, stats, sidecar dispatch, snapshot |

### Architectural rules encoded in Phase 1

- **Carrier cells** own all integrable ground-truth quantities.
  MacroCarrier::position is the positional truth.  ChemCarrier::mass_amu,
  charge_e, Z are the chemical truth.  FundCarrier owns charge/spin/colour proxy.

- **Action cells** hold per-step transient dynamics (forces, reaction events,
  field coupling flags).  They are overwritten every step.

- **Information cells** (MacroInfo, ChemInfo, FundInfo) are **sidecar-only**.
  McfCaiSidecar::update() reads Carrier + Action; writes only to Info cells.
  No integrator or force evaluator may write to Info cells.
  Info cells never enter the force pipeline.

- **caf_channel** (formerly cai_channel) lives in FundCarrier.
  It is the colour-averaged force proxy channel.  Its magnitude drives
  FundInfo::projection_loss via McfCaiSidecar.

---

## Phase 2 — Force evaluator (next 🔵 ACTIVE)

> **Prerequisite:** `cai_channel` → `caf_channel` rename (WO-76 Step 1) must be confirmed clean before any Phase 3 parser work. Phase 2 force evaluator does not depend on the rename.

Implement a concrete `ForceEvaluator` for the MCF-CAI kernel:

### 2a. `include/vsim/kernel_mcf/mcf_cai_force.hpp`

```cpp
// Pair potential evaluator operating on McfCaiWorld.
// Reads: obj.pos(), obj.charge(), obj.chem_c.Z
// Writes: obj.force (MacroAction indirectly via integrator)
// Returns: potential energy [eV]
double evaluate_lj_coulomb(McfCaiWorld& world,
                            double epsilon, double sigma,
                            double cutoff_A);
```

Supports:
- Lennard-Jones with per-species σ/ε lookup from element table
- Coulomb (1/r) with charge from ChemCarrier::charge_e
- Ewald summation (PBC) via existing `src/pot/ewald_sum.hpp`

### 2b. `src/kernel_mcf/mcf_cai_force.cpp`

Implementation.  Must not import SimulationState or Molecule.

---

## Phase 3 — I/O and VSIM script bridge 🟠 ON DISK (BLUE)

### 3a. `include/vsim/kernel_mcf/mcf_cai_io.hpp`

- `write_xyza_frame(const McfCaiFrame&, std::ostream&)`
  Writes an extended `.xyza` frame including `D_chem`, `D_fund`,
  `projection_loss` as additional property columns.

- `write_dynx_frame(const McfCaiFrame&, DynxEmitter&)`

### 3b. Parser bridge

- Recognise `[object.macro.carrier]`, `[object.chemical.*]`, `[object.fundamental.*]`
  blocks in `.vsim` scripts and populate a McfCaiWorld.
- Controlled by a new VsimDocument flag: `use_mcf_kernel = true`.
- When flag is false (default), the existing SimulationState path is used.

---

## Phase 4 — Report and visualizer integration

### 4a. End-tag enrichment (builds on WO-75A)

Extend `IKKEndTag` to carry `McfCaiSidecarSummary`:
- Add `mean_dist_chem`, `mean_dist_fund`, `total_entropy_loss` columns
- Add `D_chem / D_fund` per-object table to Markdown and LaTeX reports

### 4b. MCF-CAI grid overlay (builds on WO-75A visual passes)

In `src/vis/renderer.hpp`:
- `render_mcfcai_panel(const McfCaiObject&)` — ImGui 3×3 grid panel
  showing all nine cells, colour-coded by layer (green/blue/red)
  and shaded by Information-column D values

---

## Phase 5 — Migration path (long-term)

1. Run MCF-CAI kernel and legacy SimulationState **in parallel** for the same
   `.vsim` script.  Diff outputs to validate equivalence.
2. Once validation passes for standard molecules, gate `use_mcf_kernel`
   to `true` by default for non-legacy scripts.
3. Deprecate flat SoA arrays in `SimulationState` in favour of
   `McfCaiWorld::extract_soa()` for hot-path integration.
4. Eventually retire `SimulationState` (separate WO required).

---

## Constraints

- Fork is additive.  `src/sim/sim_state.*`, `molecule.hpp`, `vsim_document.hpp`
  are not modified by this WO.
- `caf_channel` rename from `cai_channel` (WO-76 Step 1) is a prerequisite
  for any Phase 3 parser work.
- Information column = sidecar only.  Zero exceptions.
- McfCaiWorld does not own an integrator in v1.0.
  The caller (simulation driver / runner) constructs and drives the integrator.

---

## Success Criteria

### Phase 1 (done)
- [x] `mcf_cai_object.hpp` — 9-cell struct compiles with `g++ -std=c++23`
- [x] `mcf_cai_world.hpp` — world container interface defined
- [x] `mcf_cai_integrator.hpp` — VelocityVerlet + FIRE built-in
- [x] `mcf_cai_sidecar.hpp` — Information column updater with D_chem, D_fund
- [x] `mcf_cai_world.cpp` — full world implementation

### Phase 2
- [~] LJ+Coulomb force evaluator compiles and produces physically reasonable
      forces for a 2-atom H2O test case *(file exists: `mcf_cai_force.hpp/.cpp`; gate test pending)*

### Phase 3
- [~] `.vsim` script with `[object.chemical.carrier]` block parsed into McfCaiWorld *(parser exists: `mcf_cai_parser.hpp/.cpp`; integration test pending)*
- [~] `.xyza` output includes `D_chem` and `projection_loss` columns *(io exists: `mcf_cai_io.hpp`; round-trip test pending)*

### Phase 4
- [ ] Report end-tag shows MCF-CAI sidecar summary table
- [ ] ImGui panel shows live 3×3 grid for selected object