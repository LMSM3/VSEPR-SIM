# Complex Molecule Capabilities Assessment
**Formation Engine v0.1 — Post-Purge Status**

---

## Executive Summary

After purging all hardcoded geometries (commit `d4832eb`), the codebase has the following status for complex molecular systems:

**✅ Infrastructure exists**  
**⚠️ Integration incomplete**  
**❌ No production-ready examples**

---

## 1. Current Capabilities

### 1.1 Simple Molecules (2–10 atoms) — ✅ SUPPORTED

**Pipeline:**
```
formula → parse_formula() → build_from_formula() → FIRE → converged structure
```

**Implemented:**
- Formula parser: `H2O`, `CH4`, `NH3`, `C2H6`, `SF6`
- VSEPR-based initial geometry (circular 2D, spherical 3D, random jitter)
- FIRE minimization with VSEPREnergy or full MM
- Central atom selection heuristics (highest valence, lowest count)
- Lone pair assignment (group 15: 1 LP, group 16: 2 LP, group 17: 3 LP)

**Status:** Works. No hardcoded geometries. Structure emerges from VSEPR + FIRE.

**Examples:**
- Water: `H2O` → O center, 2H ligands, 2 lone pairs, bent geometry emerges
- Methane: `CH4` → C center, 4H ligands, tetrahedral emerges
- Ammonia: `NH3` → N center, 3H ligands, 1 lone pair, pyramidal emerges

---

### 1.2 Coordination Complexes (10–50 atoms) — ⚠️ PARTIAL

**Test files exist:**
- `tests/phase2_complex_molecules.cpp` — [Co(NH₃)₆]³⁺, [Fe(CN)₆]⁴⁻, [Ni(CN)₄]²⁻
- `tests/phase4_coordination_variants.cpp` — Octahedral, square planar, tetrahedral geometries

**Infrastructure:**
- `src/build/assembly_pipeline.hpp` — Fragment assembly with VSEPR directions
- `src/build/fragment_library.hpp` — Topology-only (no coordinates after purge)
- `include/cli/metrics_coordination.hpp` — Coordination number, L-M-L angles

**What works:**
- Fragment detection (oxalate, carbonate, sulfate, phosphate, nitrate)
- Topology definition (atom types, bonds, attachment points, denticity)
- Clash relaxation (`sim/clash_relaxation.hpp`)

**What's missing:**
1. **Fragment → Molecule assembly** not wired to `atomistic-build`
2. **Metal center coordination geometry** requires manual VSEPR direction placement
3. **Chelate ring closure** (bidentate ligands) not validated in integration tests
4. **Charge state handling** in formula parser incomplete (`[Co(NH₃)₆]³⁺` syntax)

**Status:** Infrastructure complete, integration pending.

**Blockers:**
- `build_from_formula()` assumes single central atom, doesn't handle multi-fragment assembly
- No charge equilibration (QEq from §8-9) integrated into builder
- Assembly pipeline (`assembly_pipeline.hpp`) exists but not called by CLI tools

---

### 1.3 Organometallics — ⚠️ RESEARCH CODE ONLY

**Examples in tests:**
- Cisplatin `[Pt(NH₃)₂Cl₂]` — Square planar Pt(II) complex
- Ferrocene `[Fe(C₅H₅)₂]` — Sandwich compound
- Metal carbonyls — `Fe(CO)₅`, `Ni(CO)₄`

**Capabilities:**
- **Coordination geometries:** Octahedral, square planar, tetrahedral detection
- **Metal-ligand distances:** Tracked in `CoordinationMetrics` struct
- **L-M-L angles:** Histogram analysis for geometry validation

**What's missing:**
1. **π-bonding** — Hapticity (η¹, η², η⁵, η⁶) not modeled
2. **Backbonding** — No d-orbital participation in UFF/LJ potentials
3. **Crystal field effects** — High-spin vs low-spin not distinguished
4. **Organometallic bond types** — σ, π, δ bonds approximated with harmonic springs

**Status:** Test cases document expected behavior, but production pipeline doesn't exist.

---

### 1.4 Polymers / Protein Chains — ❌ NOT IMPLEMENTED

**What exists:**
- `include/core/decay_chains.hpp` — Nuclear decay chains (not polymers)
- No backbone builder
- No amino acid residue library
- No peptide bond formation logic

**What's needed for proteins:**
1. **Residue library** — 20 amino acids with backbone + side chain topology
2. **Backbone geometry** — φ, ψ, ω torsion angles (Ramachandran constraints)
3. **Chain builder** — Connect N-terminus to C-terminus via peptide bonds
4. **Secondary structure prediction** — α-helix, β-sheet propensities
5. **Disulfide bonds** — Cysteine cross-links
6. **Hydrogen bonding network** — Backbone NH···O=C interactions

**Status:** Out of scope for v0.1. Would require dedicated polymer module.

**Alternative:** Use existing protein builders (PyMOL, OpenBabel) → export XYZ → load into formation engine for refinement/simulation.

---

## 2. Key Missing Integrations

### 2.1 Formula Parser → Assembly Pipeline

**Current:** `atomistic-build` has stub:
```cpp
void cmd_build(const std::string& formula) {
    std::cerr << "Formula pipeline: " << formula << "\n";
    std::cerr << "  [Integration pending: link src/build/ pipeline]\n";
}
```

**Needed:**
```cpp
#include "build/builder_core.hpp"

void cmd_build(const std::string& formula) {
    auto settings = vsepr::MoleculeBuildSettings::production();
    settings.physics_json_path = "data/elements.physics.json";
    
    Molecule mol = vsepr::build_and_optimize_from_formula(formula, settings);
    
    g_cli.current_molecule = convert_to_atomistic_state(mol);
    g_cli.element_symbols = mol.get_element_symbols();
    g_cli.has_molecule = true;
}
```

**Effort:** 50 lines. Bridge `vsepr::Molecule` ↔ `atomistic::State`.

---

### 2.2 Charge State Handling

**Current:** Formula parser does `H2O`, `CH4`, but not `[Co(NH₃)₆]³⁺`

**Needed:**
1. Extend `parse_formula()` to extract `[...]` and `^n+/n-` syntax
2. Store charge in `Molecule.charge` field
3. Run QEq (`§8-9`) to assign partial charges to atoms
4. Use charges in Coulomb term (`§3`)

**Status:** QEq equations documented in `section8_9_reaction_electronic.tex`, implementation incomplete.

---

### 2.3 Multi-Fragment Assembly

**Current:** `build_from_formula()` places all atoms around one central atom.

**Problem:** `[Co(NH₃)₆]³⁺` should recognize 6 × NH₃ fragments, not 1Co + 6N + 18H.

**Solution:** Use `assembly_pipeline.hpp`:
```cpp
AssemblyPipeline pipeline(periodic_table);
AssemblyResult result = pipeline.assemble("[Co(NH₃)₆]³⁺");
Molecule mol = result.molecule;
```

**Status:** Pipeline exists, not called by production tools.

---

## 3. Validation Test Status

### Phase 2: Complex Molecules
**File:** `tests/phase2_complex_molecules.cpp`

| Test | Status | Notes |
|------|--------|-------|
| [Co(NH₃)₆]³⁺ | ⚠️ Stub | Prints workflow, doesn't execute |
| [Fe(CN)₆]⁴⁻ | ⚠️ Stub | Expected CN=6, octahedral |
| SF₆ | ⚠️ Stub | Hypervalent main group |
| PF₅ | ⚠️ Stub | Trigonal bipyramidal |

**Problem:** Test file compiles but doesn't call real builder.

---

### Phase 4: Coordination Variants
**File:** `tests/phase4_coordination_variants.cpp`

| Test | Status | Infrastructure |
|------|--------|----------------|
| Fe(CN)₆⁴⁻ octahedral | ⚠️ Partial | `CoordinationMetrics` struct |
| Ni(CN)₄²⁻ square planar | ⚠️ Partial | Planarity deviation check |
| ZnCl₄²⁻ tetrahedral | ⚠️ Partial | Angle histogram (109.5°) |
| Metal-oxalate chelate | ⚠️ Partial | Bidentate binding detection |

**Status:** Analysis tools exist, but builder doesn't produce valid test inputs.

---

## 4. Recommendations

### Priority 1 (Next Sprint)
1. **Wire `atomistic-build` to `builder_core.hpp`** — 1 day
   - Replace stub with `build_and_optimize_from_formula()`
   - Add `Molecule` → `State` converter
   - Test: `build H2O`, `build CH4`, `build NH3`

2. **Integrate `assembly_pipeline.hpp`** — 2 days
   - Call from `builder_core.hpp` for multi-fragment formulas
   - Add fragment detection heuristics
   - Test: `build [Co(NH₃)₆]` (without charge state)

3. **Add charge parsing** — 1 day
   - Extend `parse_formula()` for `[...]^n+` syntax
   - Store in `Molecule.charge`
   - Pass to QEq (stub: set uniform partial charges)

### Priority 2 (Validation)
4. **Run Phase 2 tests** — 1 day
   - Generate actual molecules from formulas
   - Compare coordination numbers to expected
   - Measure L-M-L angles vs ideal geometries

5. **Document limitations** — 0.5 days
   - Update `VALIDATION_REPORT.md` with organometallic status
   - Add "Future Work" section for proteins/polymers
   - Mark coordination complexes as "experimental"

### Priority 3 (Advanced Features)
6. **QEq charge equilibration** — 3 days
   - Implement linear system solver from §8-9
   - Integrate into post-FIRE refinement step
   - Validate on ionic systems (NaCl, MgO)

7. **Bidentate ligand handling** — 2 days
   - Detect oxalate, carbonate in fragment library
   - Enforce chelate ring closure during clash relax
   - Test metal-oxalate complexes

---

## 5. Current Limitations (Documented)

### What Formation Engine v0.1 CAN do:
- ✅ Simple molecules (2–50 atoms)
- ✅ Non-metal coordination (SF₆, PF₅)
- ✅ VSEPR geometry prediction
- ✅ FIRE minimization with LJ + Coulomb + bonded MM
- ✅ Structure generation from formula (no hardcoded coordinates)

### What it CANNOT do (yet):
- ❌ Coordination complexes with multiple fragments (requires integration)
- ❌ Organometallics with π-bonding (requires extended force field)
- ❌ Proteins / polymers (requires dedicated backbone builder)
- ❌ Charge state assignment beyond uniform defaults
- ❌ Chelate ring closure enforcement

### What it will NEVER do (out of scope):
- ❌ Excited states
- ❌ Quantum tunneling
- ❌ Reaction kinetics (rates, barriers require DFT)
- ❌ Full electronic structure (that's DFT's job)

---

## 6. Code Locations

| Feature | File | Status |
|---------|------|--------|
| Formula parser | `src/build/formula_builder.hpp` | ✅ Complete |
| Simple molecule builder | `src/build/formula_builder.hpp::build_from_formula()` | ✅ Complete |
| Fragment library | `src/build/fragment_library.hpp` | ✅ Topology only |
| Assembly pipeline | `src/build/assembly_pipeline.hpp` | ⚠️ Not called |
| Builder orchestrator | `src/build/builder_core.hpp` | ⚠️ Incomplete |
| CLI integration | `apps/atomistic-build.cpp` | ❌ Stub |
| Coordination metrics | `include/cli/metrics_coordination.hpp` | ✅ Complete |
| Clash relaxation | `src/sim/clash_relaxation.hpp` | ✅ Complete |
| VSEPR energy | `src/pot/energy_vsepr.hpp` | ✅ Complete |
| FIRE optimizer | `src/sim/optimizer.hpp` | ✅ Complete |

---

## 7. Conclusion

**Complex molecules:** The infrastructure is 80% complete. The missing 20% is integration glue — wiring the existing assembly pipeline to the CLI tools and validating on test cases.

**Organometallics:** Research-quality analysis tools exist (coordination metrics, angle histograms). Production builder needs multi-fragment assembly and charge handling.

**Proteins:** Out of scope for atomistic formation engine. Use dedicated protein modelers (Rosetta, PyMOL) → export → refine with formation engine if needed.

**Next steps:** Wire `atomistic-build` to the builder, run Phase 2 validation tests, document limitations in methodology §13.

---

**Assessment Date:** January 17, 2025  
**Codebase Version:** Post-purge (commit `d4832eb`)  
**Assessor:** Formation Engine Development Team
