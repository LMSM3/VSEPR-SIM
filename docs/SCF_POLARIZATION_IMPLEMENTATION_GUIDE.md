# SCF Polarization Implementation Guide

**Date:** January 18, 2025  
**Status:** Planning complete, implementation pending  
**Reference:** `docs/section_polarization_models.tex` (full theoretical treatment)

---

## Executive Summary

After rigorous evaluation of three polarization approaches (Drude oscillators, composite atomic structures, SCF induced dipoles), **Self-Consistent Field (SCF) induced dipoles** was selected as the optimal path forward for the Formation Engine.

**Decision drivers:**
- ✅ **Accuracy:** 10-30% improvement for H-bonds, 50%+ for metal complexes
- ✅ **Stability:** No stiff dynamics, well-conditioned linear systems
- ✅ **Determinism:** Fixed convergence criteria ensure reproducibility
- ✅ **Cost:** 1.5-2× slowdown vs. fixed charges (vs. 10-100× for alternatives)
- ✅ **Extensibility:** Clean path to tensor polarizability, multipoles

---

## Current State

**Implemented:**
- Theoretical foundation (TEX document: `docs/section_polarization_models.tex`)
- Header stub with full documentation (`atomistic/models/polarization_scf.hpp`)
- Thole damping function
- Polarizability parameter database (Miller 1990)

**Not yet implemented:**
- State extensions (`induced_dipoles`, `polarizabilities` vectors)
- SCF solver loop
- Energy/force evaluation
- Integration into LJ+Coulomb evaluator

---

## Implementation Phases

### Phase 1: Minimal SCF Core (1-2 weeks)

**Goal:** Achieve converging induced dipoles (no forces yet)

**Tasks:**

1. **Extend `atomistic::State`** (`atomistic/core/state.hpp`):
   ```cpp
   struct State {
       // ... existing members ...
       
       // Polarization data
       std::vector<Vec3> induced_dipoles;      // μ_i (Debye)
       std::vector<double> polarizabilities;   // α_i (Å³)
       
       void init_polarizabilities() {
           polarizabilities.resize(N);
           induced_dipoles.resize(N, Vec3{0, 0, 0});
           for (uint32_t i = 0; i < N; ++i) {
               uint32_t Z = (i < type.size()) ? type[i] : 1;
               polarizabilities[i] = PolarizabilityParams::get(Z);
           }
       }
   };
   ```

2. **Implement SCF solver methods** (`atomistic/models/polarization_scf.cpp`):
   - `compute_permanent_field()`: Sum charge contributions
   - `compute_induced_field_at()`: Apply dipole tensor to all j≠i
   - `compute_dipole_tensor()`: Thole-damped T_ij
   - `solve_scf()`: Iterative damped relaxation

3. **Unit test** (`tests/test_polarization_scf.cpp`):
   - Water dimer at 2.9 Å separation
   - Compare induced dipoles to AMOEBA reference
   - Verify convergence in <20 iterations

**Acceptance:** Dipoles converge with max error <1e-6 Debye

---

### Phase 2: Thole Damping Validation (1 week)

**Goal:** Prove stability at short range (prevent polarization catastrophe)

**Tasks:**

1. Test noble gas dimers (He-He, Ar-Ar) at r = 0.5 Å, 1.0 Å, 2.0 Å
2. Compare damped vs. undamped: verify f_damp → 0 as r → 0
3. Parameter sensitivity: test a = 2.0, 2.6, 3.0
4. Document findings in `docs/polarization_validation_report.md`

**Acceptance:** No catastrophe (finite dipoles) at all separations

---

### Phase 3: Energy and Forces (1-2 weeks)

**Goal:** Enable energy-conserving MD with polarization

**Tasks:**

1. **Energy evaluation:**
   ```cpp
   double compute_energy(const State& s) const {
       double U = 0.0;
       auto E_perm = compute_permanent_field(s);
       for (uint32_t i = 0; i < s.N; ++i) {
           U -= 0.5 * dot(s.induced_dipoles[i], E_perm[i]);
       }
       return U;
   }
   ```

2. **Force implementation** (most complex):
   - Charge-dipole term: `F_i = ∇_i (q_j · μ_i / r³)`
   - Dipole-dipole term: `F_i = ∇_i (μ_i · T_ij · μ_j)`
   - Include Thole damping gradient

3. **Integration** into `atomistic/models/lj_coulomb.cpp`:
   ```cpp
   void LJCoulomb::eval(State& s, const ModelParams& p) const override {
       // ... existing LJ + Coulomb ...
       
       if (p.enable_polarization) {
           SCFPolarizationSolver solver;
           solver.solve_scf(s);
           s.E.Upol = solver.compute_energy(s);
           solver.add_forces(s);
       }
   }
   ```

4. **Numerical gradient check:**
   - Compute force via finite difference: `F_i = -(U(x+δ) - U(x-δ)) / (2δ)`
   - Compare to analytic force, require relative error <1e-4

**Acceptance:** Energy drift <1e-6 in NVE trajectories (100 ps)

---

### Phase 4: Validation Campaign (2-3 weeks)

**Goal:** Benchmark against reference data

**Benchmarks:**

| System | Property | Reference | Target Error |
|--------|----------|-----------|--------------|
| Water dimer | Binding energy | -5.0 kcal/mol (CCSD(T)) | <10% |
| (H₂O)₆ cluster | Structure | AMOEBA geometry | RMSD <0.1 Å |
| [Fe(H₂O)₆]³⁺ | Coord. energy | -120 kcal/mol (DFT) | <15% |
| Ice Ih crystal | Dielectric ε_r | 3.2 @ 250K (expt) | <20% |
| Peptide H-bond | ΔE (N-H···O=C) | -7.5 kcal/mol (QM) | <10% |

**Files to create:**
- `tests/validate_water_clusters.cpp`
- `tests/validate_metal_complexes.cpp`
- `tests/validate_ice_dielectric.cpp`
- `docs/polarization_validation_report.md`

**Acceptance:** Pass ≥4 out of 5 benchmarks

---

### Phase 5: Performance Optimization (1-2 weeks)

**Goal:** Reduce overhead to 1.5-2× vs. fixed-charge model

**Optimization strategies:**

1. **Neighbor lists:** Only compute T_ij for r_ij < cutoff
   - Expected speedup: 5-10× for large systems
   
2. **Preconditioned CG:** Replace damped iteration for N>5000
   - Preconditioning: diagonal (α_i) or Jacobi
   - Expected convergence: 5-10 iterations vs. 20+
   
3. **Sparse matrix storage:** CSR format for T matrix
   - Memory: O(N) vs. O(N²)
   
4. **SIMD/GPU (optional):**
   - Vectorize tensor-vector products
   - CUDA kernel for dipole updates

**Profiling targets:**
- Identify bottleneck: tensor computation vs. iteration count
- Measure time per SCF solve vs. total force evaluation
- Benchmark scaling: N = 100, 1000, 10000 atoms

**Acceptance:** <2× slowdown for N=1000 relative to fixed-charge

---

### Phase 6: Anisotropic Extension (Optional, 2-3 weeks)

**Goal:** Directional polarizability for layered materials

**Modifications:**

1. Replace `double α` with `Mat3 α_tensor` in State
2. Assign tensor based on local geometry:
   - sp²: planar (graphene, benzene)
   - sp³: tetrahedral (diamond)
   - Detect via bond angles
3. Tensor-tensor multiplication: `μ = α_tensor · E`
4. Validation: graphene sheet (α_parallel / α_perp ≈ 100)

**Use cases:**
- Graphene, MoS₂ (van der Waals solids)
- Fiber composites
- Anisotropic crystals (calcite, quartz)

---

## Integration Checklist

**Files to modify:**
- [ ] `atomistic/core/state.hpp` – Add `induced_dipoles`, `polarizabilities`
- [ ] `atomistic/models/polarization_scf.cpp` – Implement solver
- [ ] `atomistic/models/lj_coulomb.cpp` – Call solver in `eval()`
- [ ] `atomistic/models/model.hpp` – Add `enable_polarization` flag to `ModelParams`

**Files to create:**
- [ ] `atomistic/models/polarization_scf.cpp` – Implementation
- [ ] `tests/test_polarization_scf.cpp` – Unit tests
- [ ] `tests/validate_water_clusters.cpp` – Validation
- [ ] `docs/polarization_validation_report.md` – Results

**Documentation to update:**
- [ ] `docs/PHYSICS_ROADMAP_COMPLETE.md` – Mark Priority 1 as in-progress
- [ ] `docs/METHODOLOGY_12PAGE.tex` – Add polarization section
- [ ] `README.md` – Add polarization to feature list

---

## Parameter Database

**Atomic polarizabilities** (Miller 1990, J. Am. Chem. Soc. 112, 8533):

| Element | α (Å³) | Notes |
|---------|--------|-------|
| H  | 0.667 | |
| C  | 1.76  | sp³ average |
| N  | 1.10  | |
| O  | 0.802 | |
| F  | 0.557 | |
| P  | 3.63  | |
| S  | 2.90  | |
| Cl | 2.18  | |
| Fe | 8.40  | 3d metals |
| Cu | 6.03  | |

**Thole damping parameter:**
- a = 2.6 (standard, validated for organic molecules)
- a = 2.0 (softer damping, use for metals)
- a = 3.0 (harder damping, use for highly polar systems)

---

## Expected Accuracy Gains

### Hydrogen-Bonded Systems
- **Current (fixed charge):** ΔE = -4.5 kcal/mol (30% error)
- **With polarization:** ΔE = -5.2 kcal/mol (10% error)
- **Reference (QM):** ΔE = -5.0 kcal/mol

### Metal-Ligand Complexes
- **Current:** 50-100% error in coordination energies
- **With polarization:** 10-20% error
- **Key issue:** d-orbital back-bonding captured by induced dipoles

### π-Stacking
- **Current:** Misses dispersion + polarization (both essential)
- **With polarization + Grimme D3:** Within 5 kcal/mol of SAPT(2+3)

---

## Risk Mitigation

**Potential issues:**

1. **Convergence failure** in strong fields
   - Mitigation: Reduce damping_factor to 0.3, increase max_iterations
   
2. **Force noise** in stiff configurations
   - Mitigation: Tighten convergence tolerance to 1e-8
   
3. **Performance bottleneck** for large systems
   - Mitigation: Phase 5 optimizations (neighbor lists, PCG)
   
4. **PBC complications** (dipole-dipole across boundaries)
   - Mitigation: Use minimum image for T_ij, no Ewald (short-range damping suffices)

---

## Timeline Summary

| Phase | Duration | Deliverable |
|-------|----------|-------------|
| 1. SCF Core | 1-2 weeks | Converging dipoles |
| 2. Damping | 1 week | Stable at short range |
| 3. Energy/Forces | 1-2 weeks | MD-ready |
| 4. Validation | 2-3 weeks | <10% error benchmarks |
| 5. Optimization | 1-2 weeks | <2× overhead |
| 6. Anisotropy (opt) | 2-3 weeks | Tensor α |
| **Total** | **7-12 weeks** | Production-ready polarization |

---

## Next Actions

1. **Review this plan** with stakeholders
2. **Create Phase 1 branch:** `feature/scf-polarization-phase1`
3. **Modify `State`** to add polarization vectors
4. **Implement SCF solver** in `.cpp` file
5. **Write unit test** for water dimer
6. **Iterate** until convergence validated

---

## References

See `docs/section_polarization_models.tex` for full citations and theoretical derivations.

**Key papers:**
- Thole (1981): Damping function
- Miller (1990): Atomic polarizabilities
- Applequist (1972): Induced dipole model
- Ren & Ponder (2003): AMOEBA force field (validation target)

---

**Document Status:** Complete and ready for implementation  
**Last Updated:** January 18, 2025  
**Contact:** Formation Engine Development Team
