# Physics Improvements & Multiscale Unification Strategy
## Formation Engine v0.1 — Research Roadmap

**Date:** January 17, 2025  
**Last Updated:** January 18, 2025 (Polarization decision)  
**Context:** Discussion on physics completeness and thermal/fluid multiscale coupling

---

## 🔗 Quick Links: Polarization Implementation (Priority 1)

**Decision made:** Self-Consistent Field (SCF) induced dipoles selected  
**Status:** Planning complete, implementation pending (Phase 1: 1-2 weeks)

**Key Documents:**
- 📘 **Theory:** `docs/section_polarization_models.tex` (7-page LaTeX with full derivations)
- 📋 **Implementation Guide:** `docs/SCF_POLARIZATION_IMPLEMENTATION_GUIDE.md`
- 📌 **Decision Record:** `docs/POLARIZATION_DECISION_RECORD.md`
- 💻 **Header File:** `atomistic/models/polarization_scf.hpp`

**Why SCF?** Best accuracy-to-cost ratio (1.5-2× overhead, 10-50% accuracy gain), excellent stability, deterministic, extensible. Rejected: Drude oscillators (stiff dynamics), composite atoms (computational catastrophe).

---

## 🔬 Current Physics Model — What's Implemented

### ✅ Well-Established Components

1. **Nonbonded Interactions**
   - Lennard-Jones 12-6 (dispersion + repulsion)
   - Coulomb electrostatics (point charges)
   - Switching functions (smooth cutoffs)
   - Periodic boundary conditions

2. **Bonded Interactions**
   - Harmonic bonds (UFF parameterization)
   - Harmonic angles
   - Torsional potentials (Fourier series)
   - Improper dihedrals (planarity)

3. **Integration**
   - Velocity Verlet (symplectic, energy-conserving)
   - Langevin dynamics (thermostat)
   - FIRE minimization

4. **Reactions**
   - Template-based discovery
   - Fukui function reactivity
   - HSAB site matching
   - BEP activation barriers
   - Heat-gated control (Item #7 — just implemented!)

---

## ⚠️ Missing Physics — Priority Ordered

### Priority 1: **Polarization & Induced Dipoles** ✅ Decision Made

**Current Limitation:**
- Fixed point charges (QEq or pre-assigned)
- No response to local electric field
- Underestimates H-bonds, π-stacking, metal-ligand interactions

**Implementation Decision (Jan 18, 2025):**
**Self-Consistent Field (SCF) induced dipoles** selected after rigorous evaluation

**Why SCF? (see `docs/section_polarization_models.tex` for full analysis)**
- ✅ Optimal accuracy/cost: 1.5-2× overhead vs. 10-100× for alternatives
- ✅ Excellent stability: Thole damping prevents polarization catastrophe
- ✅ Deterministic: Fixed convergence ensures reproducibility
- ✅ Extensible: Clear path to tensor polarizability, multipoles

**Alternatives Evaluated:**
1. **Drude oscillators** ❌ Rejected: Stiff dynamics (0.1 fs timestep), dual thermostat complexity
2. **Composite atoms** ❌ Rejected: 300+ uncalibratable parameters, catastrophic performance

**Formula (SCF):**
$$\mathbf{\mu}_i = \alpha_i \mathbf{E}_i^{\text{loc}}$$
$$\mathbf{E}_i^{\text{loc}} = \mathbf{E}_i^{\text{perm}} + \sum_{j \neq i} \mathbf{T}_{ij} \cdot \mathbf{\mu}_j$$

Where:
- $\alpha_i$ = atomic polarizability (Å³, from Miller 1990)
- $\mathbf{E}_i^{\text{perm}}$ = field from fixed charges
- $\mathbf{T}_{ij}$ = Thole-damped dipole tensor

**Implementation Status:**
- **Theoretical foundation:** Complete (7-page LaTeX document)
- **Implementation guide:** Complete (6-phase roadmap, 7-12 weeks)
- **Header file:** Complete (`atomistic/models/polarization_scf.hpp`)
- **Core implementation:** Pending (Phase 1, ETA 1-2 weeks)

**Performance cost:** 1.5-2× slower (iterative SCF convergence, 5-20 iterations)  
**Accuracy gain:** 10-30% for H-bonds, 50%+ for metal complexes

**References:**
- Thole, B.T. *Chem. Phys.* **59**, 341 (1981) — Damping function
- Miller, K.J. *J. Am. Chem. Soc.* **112**, 8533 (1990) — Polarizabilities
- Applequist, J. *J. Am. Chem. Soc.* **94**, 2952 (1972) — Induced dipole theory
- Ren & Ponder, *J. Phys. Chem. B* **107**, 5933 (2003) — AMOEBA validation target

---

### Priority 2: **Three-Body Dispersion (Axilrod-Teller)**

**Current Limitation:**
- Pairwise LJ only
- Misses cooperative effects in dense phases
- Underestimates cohesive energy by 5-10%

**Improvement:**
**Axilrod-Teller-Muto triple-dipole term**

**Formula:**
$$E_{\text{3-body}} = \sum_{i<j<k} C_9^{ijk} \frac{1 + 3\cos\theta_i\cos\theta_j\cos\theta_k}{r_{ij}^3 r_{jk}^3 r_{ik}^3}$$

Where:
- $C_9$ = 3-body dispersion coefficient
- $\theta$ angles defined by atom positions

**Affects:**
- Dense liquids (10% correction to density)
- Noble gas crystals (essential for correct structure)
- Protein folding (stabilizes compact states)

**Implementation complexity:** Medium  
**Performance cost:** $O(N^3)$ → needs neighbor-list optimization  
**Accuracy gain:** Critical for high-pressure phases

**References:**
- Axilrod & Teller, J. Chem. Phys. 1943
- Hermann et al., Chem. Rev. 2017 (modern review)

---

### Priority 3: **Anharmonic Bonding**

**Current Limitation:**
- Harmonic potentials only (quadratic)
- Bond breaking requires switching functions
- No dissociation curves

**Improvement:**
**Morse potential** for bonds

**Formula:**
$$U(r) = D_e \left[1 - e^{-\beta(r - r_e)}\right]^2$$

Where:
- $D_e$ = dissociation energy
- $\beta = \sqrt{k/(2D_e)}$ (stiffness parameter)
- $r_e$ = equilibrium distance

**Advantages:**
- Natural bond breaking (finite dissociation limit)
- Correct anharmonicity (high-temperature vibrations)
- Smooth reactive potential energy surfaces

**Implementation complexity:** Low  
**Performance cost:** Negligible (still pairwise)  
**Accuracy gain:** Essential for high-T reactions, bond scission

**References:**
- Morse, Phys. Rev. 1929
- Used extensively in ReaxFF, AIREBO potentials

---

### Priority 4: **Quantum Nuclear Effects**

**Current Limitation:**
- Classical nuclei (point masses)
- Misses zero-point energy (ZPE)
- H-atom tunneling ignored

**Improvement:**
**Path Integral Molecular Dynamics (PIMD)**

**Concept:**
Replace each nucleus with a "ring polymer" of $P$ beads:
$$\rho(q, q') \approx \left(\frac{m}{2\pi\beta\hbar^2}\right)^{P/2} \exp\left(-\frac{m}{2\beta\hbar^2}\sum_{k=1}^P (q_k - q_{k+1})^2\right)$$

**Affects:**
- H/D isotope effects (critical for proton transfer)
- Zero-point energy (stabilizes molecules by 5-10 kcal/mol)
- Tunneling rates (orders of magnitude for H)

**Implementation complexity:** High  
**Performance cost:** $P$× slower (typically $P = 8$-32)  
**Accuracy gain:** Essential for light atoms below 300 K

**References:**
- Marx & Parrinello, J. Chem. Phys. 1996 (PIMD)
- Ceriotti et al., Chem. Rev. 2016 (modern methods)

---

### Priority 5: **Long-Range Dispersion (vdW corrections)**

**Current Limitation:**
- LJ cutoff at 10-12 Å
- Missing tail contributions
- Underestimates bulk modulus, surface tension

**Improvement:**
**Grimme D3 or D4 dispersion correction**

**Formula:**
$$E_{\text{disp}} = -\sum_{n=6,8,10} s_n \sum_{i<j} \frac{C_n^{ij}}{r_{ij}^n} f_{\text{damp}}(r_{ij})$$

Where:
- $C_n$ = dispersion coefficients (element-dependent)
- $f_{\text{damp}}$ = damping function (avoids double-counting at short range)
- $s_n$ = scaling factors

**Advantages:**
- Accounts for long-range dispersion
- Cheap (post-processing or analytic correction)
- Element-specific (not just LJ mixing rules)

**Implementation complexity:** Low  
**Performance cost:** Negligible ($O(N^2)$ but analytical tail)  
**Accuracy gain:** 5-15% for condensed phases

**References:**
- Grimme et al., J. Chem. Phys. 2010 (D3)
- Caldeweyher et al., J. Chem. Phys. 2019 (D4)

---

### Priority 6: **Charge Transfer & HSAB Dynamics**

**Current Limitation:**
- Fixed oxidation states
- No electron flow during reactions
- HSAB matching is static (pre-reaction)

**Improvement:**
**Reactive QEq** (charge equilibration during dynamics)

**Formula:**
Update charges $q_i$ every timestep to minimize:
$$E[q] = \sum_i \chi_i q_i + \frac{1}{2}\sum_i J_i q_i^2 + \frac{1}{2}\sum_{i\neq j} \frac{q_i q_j}{r_{ij}} + \lambda\left(\sum_i q_i - Q_{\text{tot}}\right)$$

Where:
- $\chi_i$ = electronegativity
- $J_i$ = self-Coulomb integral (hardness)
- $\lambda$ = Lagrange multiplier (charge conservation)

**Affects:**
- Redox reactions (electron transfer explicitly modeled)
- Polarization (charges respond to geometry)
- Accurate reaction energetics

**Implementation complexity:** Medium  
**Performance cost:** Iterative solve each MD step (2× slower)  
**Accuracy gain:** Essential for accurate ΔE_rxn in polar reactions

**References:**
- Rappé & Goddard, J. Phys. Chem. 1991 (original QEq)
- Nakano, Comp. Phys. Comm. 2008 (reactive QEq in ReaxFF)

---

## 🧪 Inaccurate Reactions — Known Limitations

### 1. **Peptide Bond Formation (Condensation)**

**Current model:**
- Energy correction for H₂O release
- No explicit water molecule
- No solvent effects

**Reality:**
- Highly endergonic in gas phase (+15 kcal/mol)
- Favorable in aqueous solution due to entropy
- Requires enzymes (ribosome) *or* activated intermediates

**Fix:**
- Explicit solvent (TIP3P water model)
- Free energy methods (umbrella sampling, metadynamics)
- Enzymatic pocket parameterization

**Accuracy now:** 30-50% error in ΔG  
**Accuracy with fix:** 5-10% error

---

### 2. **Metal-Ligand Bonds**

**Current model:**
- UFF parameters (weak)
- No d-orbital effects
- No crystal field splitting

**Reality:**
- Directional d-orbitals (square planar, octahedral)
- Backbonding (π-acceptance in CO, CN⁻)
- High-spin vs. low-spin states

**Fix:**
- Ligand field molecular mechanics (LFMM)
- Angular dependence in bond potentials
- Spin state parameterization

**Accuracy now:** 50-100% error in coordination energies  
**Accuracy with fix:** 10-20% error

**References:**
- Comba & Remenyi, Coord. Chem. Rev. 2003 (LFMM)

---

### 3. **π-Conjugated Systems**

**Current model:**
- Single/double bonds treated independently
- No resonance
- No π-π stacking energetics

**Fix:**
- Bond order-dependent potentials (like AIREBO)
- Explicit π-orbital overlap terms
- Grimme dispersion for stacking

**Accuracy now:** Misses aromatic stabilization (15-30 kcal/mol)  
**Accuracy with fix:** Within 5 kcal/mol

---

### 4. **Proton Transfer Barriers**

**Current model:**
- BEP relation (E_a = 15 + 0.5·ΔE)
- No tunneling
- No solvent reorganization

**Reality:**
- Barrier depends on pKa difference
- Marcus theory (reorganization energy)
- Tunneling reduces barrier by factor of 2-10 for H

**Fix:**
- EVB (Empirical Valence Bond) surfaces
- Tunneling corrections (semi-classical)
- Explicit solvent (H-bond network)

**Accuracy now:** 5-10 kcal/mol error in E_a  
**Accuracy with fix:** 1-2 kcal/mol error

---

## 🌡️ Thermal Implementation — Optimization

### Current Status

Your thermal model (`include/thermal/thermal_model.hpp`):
- ✅ Per-atom heat capacity
- ✅ Thermal conductance graph
- ✅ Berendsen thermostat
- ✅ Langevin thermostat
- ⚠️ Ledger tracking (but no prediction)

### Optimization 1: **Nosé-Hoover Chains**

**Problem with Berendsen:**
- Not rigorous (doesn't sample canonical ensemble)
- Velocity rescaling is non-physical

**Problem with Langevin:**
- Adds stochastic forces (breaks determinism)
- Damping coefficient arbitrary

**Solution: Nosé-Hoover chains**

**Concept:**
Extended system with thermostat variables $\xi$:
$$\dot{\xi} = \frac{1}{Q}(T - T_0)$$

Where $Q$ is the thermostat "mass" (coupling strength).

**Advantages:**
- Rigorous canonical ensemble
- Deterministic (no random forces)
- Temperature control via extended Hamiltonian

**Implementation:**
- Replace Langevin friction in velocity_verlet.cpp
- Add $\xi$ to State (or separate thermostat struct)
- Update $\xi$ every timestep

**References:**
- Nosé, Mol. Phys. 1984
- Martyna et al., J. Chem. Phys. 1992 (chains)

---

### Optimization 2: **Thermal Conductivity Tensor**

**Current:**
- Scalar conductance $g_{ij}$ between atom pairs
- Isotropic heat flow

**Improvement:**
- Tensor conductivity $\mathbf{\kappa}_{ij}$ (directional)
- Follows crystallographic axes in anisotropic materials

**Formula:**
$$\mathbf{J} = -\mathbf{\kappa} \nabla T$$

Where $\mathbf{\kappa}$ is the 3×3 conductivity tensor.

**Use cases:**
- Graphene (in-plane >> out-of-plane)
- Layered materials (vdW solids)
- Fiber composites

**Implementation:**
- Replace `double gij` with `Mat3x3 kappa_ij`
- Compute directional flux `J_alpha = -kappa_alphabeta * dT/dx_beta`

---

### Optimization 3: **Non-Fourier Heat Transfer**

**Current: Fourier law**
$$\frac{\partial T}{\partial t} = \alpha \nabla^2 T$$

**Problem:**
- Infinite signal speed (unphysical for fast transients)
- Fails for ballistic phonons (nanoscale)

**Improvement: Cattaneo-Vernotte equation**
$$\tau \frac{\partial^2 T}{\partial t^2} + \frac{\partial T}{\partial t} = \alpha \nabla^2 T$$

Where $\tau$ is the relaxation time (phonon scattering time).

**When needed:**
- Ultrafast laser heating (ps timescale)
- Nanowires, graphene (ballistic regime)
- Shock waves

**Implementation:**
- Add second time derivative term to thermal_model.hpp
- Requires storing $\dot{T}_i$ (temperature velocity)

**References:**
- Cattaneo, Atti Sem. Mat. Fis. Univ. Modena 1948
- Tzou, Macro- to Microscale Heat Transfer, 1996

---

## 🌊 Multiscale Unification — Fluids

You already have `src/multiscale/molecular_fea_bridge.hpp`! Let's extend it for **fluids**.

### Approach 1: **Irving-Kirkwood Stress Tensor**

**Concept:**
Compute continuum stress $\sigma_{\alpha\beta}$ from atomistic forces:

$$\sigma_{\alpha\beta} = -\frac{1}{V}\sum_i m_i v_{i,\alpha} v_{i,\beta} + \frac{1}{2V}\sum_{i\neq j} f_{ij,\alpha} r_{ij,\beta}$$

**Terms:**
1. Kinetic (pressure): $m_i v_i \otimes v_i$
2. Virial (stress): $\mathbf{f}_{ij} \otimes \mathbf{r}_{ij}$

**Feeds into Navier-Stokes:**
$$\rho\frac{D\mathbf{v}}{Dt} = -\nabla p + \mu\nabla^2\mathbf{v} + \nabla\cdot\sigma^{\text{atom}}$$

**Use case:**
- MD in contact region
- Continuum away from interfaces
- Two-way coupling (MD → CFD, CFD → MD boundary conditions)

**References:**
- Irving & Kirkwood, J. Chem. Phys. 1950
- O'Connell & Thompson, Phys. Rev. E 1995 (modern review)

---

### Approach 2: **Dissipative Particle Dynamics (DPD)**

**Concept:**
Coarse-grain atoms into "fluid particles" with soft interactions:

$$\mathbf{F}_{ij} = \mathbf{F}_{ij}^C + \mathbf{F}_{ij}^D + \mathbf{F}_{ij}^R$$

Where:
- $\mathbf{F}^C$ = conservative (soft repulsion)
- $\mathbf{F}^D$ = dissipative (drag)
- $\mathbf{F}^R$ = random (thermal noise)

**Advantages:**
- Galilean invariant
- Conserves momentum locally
- Can simulate large length scales (μm)

**Implementation:**
- Replace LJ with soft potential: $F^C = a(1 - r/r_c)$
- Add velocity-dependent drag: $F^D = -\gamma (v_i - v_j)$
- Add random kick: $F^R = \sigma w(r)\xi(t)$

**Use case:**
- Blood flow (RBCs as DPD particles)
- Polymer melts (chains as connected beads)
- Colloidal suspensions

**References:**
- Hoogerbrugge & Koelman, Europhys. Lett. 1992
- Groot & Warren, J. Chem. Phys. 1997 (parameterization)

---

### Approach 3: **Lattice Boltzmann Method (LBM) Coupling**

**Concept:**
- Atomistic MD in regions with chemistry
- LBM for bulk fluid
- Schwarz domain decomposition at interface

**LBM equation:**
$$f_i(\mathbf{x} + \mathbf{c}_i\Delta t, t + \Delta t) = f_i(\mathbf{x}, t) - \frac{1}{\tau}[f_i(\mathbf{x}, t) - f_i^{\text{eq}}(\rho, \mathbf{u})]$$

Where:
- $f_i$ = distribution function (velocity space)
- $\mathbf{c}_i$ = lattice velocities
- $f^{\text{eq}}$ = Maxwell-Boltzmann equilibrium

**Coupling:**
- MD provides density $\rho$, velocity $\mathbf{u}$ at boundary
- LBM provides pressure $p$, stress $\sigma$ back to MD

**Advantages:**
- LBM is extremely fast (GPU-friendly)
- Natural for complex geometries (porous media)
- Captures hydrodynamics exactly (Navier-Stokes in limit)

**Implementation:**
- Add LBM grid in `src/multiscale/`
- Define overlap region (buffer zone)
- Exchange $\rho, \mathbf{u}, p, \sigma$ every N steps

**References:**
- Succi, The Lattice Boltzmann Equation, 2001
- Flekkøy et al., Phys. Rev. E 2000 (MD-LBM coupling)

---

## 🔬 Unifying Physics Across Scales

### The Scale Hierarchy

| Scale | Length | Time | Physics | Method |
|-------|--------|------|---------|--------|
| **Quantum** | pm | as | Electrons | DFT, CASSCF |
| **Atomistic** | Å | fs | Nuclei | **MD** (this project) |
| **Mesoscale** | nm | ps | Molecules | CG-MD, DPD |
| **Continuum** | μm | ns | Fluid | FEA, CFD, LBM |
| **Macro** | mm | μs | Bulk | Constitutive laws |

### Your Target: **Atomistic ↔ Continuum**

Bridge shown in Section 10 of your docs.

---

### Unifying Principle: **Generalized Fluctuation-Dissipation**

**Concept:**
All dissipative processes (friction, viscosity, heat conduction, diffusion) arise from microscopic fluctuations.

**Formula (Einstein relation):**
$$D = \frac{k_B T}{6\pi\eta r}$$

**Connects:**
- Diffusion coefficient $D$ (microscopic)
- Viscosity $\eta$ (continuum)
- Temperature $T$ (thermodynamics)

**Your implementation:**
1. Compute $D$ from MD (mean-squared displacement)
2. Compute $\eta$ from stress autocorrelation
3. Use both in continuum solver

**References:**
- Zwanzig, Nonequilibrium Statistical Mechanics, 2001
- Green-Kubo relations (1954, 1957)

---

## 📊 Recommended Priorities (Ordered)

### Short Term (Next 3 Months)

1. ✅ **Item #7 complete** (temperature → heat mapping)
2. 🚧 **Polarization (SCF induced dipoles)** — biggest accuracy gain
   - **Status:** Planning complete (Jan 18, 2025)
   - **Decision:** Self-consistent field method selected after rigorous evaluation
   - **Documentation:** `docs/section_polarization_models.tex` (7-page theory)
   - **Implementation:** `atomistic/models/polarization_scf.hpp` (header ready)
   - **Timeline:** 7-12 weeks to production (6 phases)
   - **Next:** Phase 1 - Extend State and implement SCF core
3. **Grimme D3 dispersion** — low cost, high benefit
4. **Nosé-Hoover thermostat** — rigorous ensemble

### Medium Term (3-6 Months)

5. **Axilrod-Teller 3-body** — essential for dense phases
6. **Morse bonds** — needed for bond-breaking reactions
7. **Irving-Kirkwood stress** — bridge to continuum

### Long Term (6-12 Months)

8. **PIMD** (quantum nuclei) — for accurate H-transfer
9. **DPD coarse-graining** — extend to mesoscale
10. **LBM coupling** — full multiscale fluid solver

---

## 🎓 Theoretical Foundation

### Why These Physics Unify

All arise from **statistical mechanics of interacting particles**:

**Hamiltonian:**
$$\mathcal{H} = \sum_i \frac{\mathbf{p}_i^2}{2m_i} + \sum_{i<j} U(\mathbf{r}_{ij}) + U_{\text{many-body}}$$

**Observables as ensemble averages:**
$$\langle A \rangle = \frac{1}{Z}\int A(\Gamma) e^{-\beta \mathcal{H}(\Gamma)} d\Gamma$$

**Continuum fields as coarse-grained densities:**
$$\rho(\mathbf{x}) = \sum_i m_i \delta(\mathbf{x} - \mathbf{r}_i) \quad \to \quad \rho(\mathbf{x}) = \langle \sum_i m_i \delta(\mathbf{x} - \mathbf{r}_i) \rangle_{\Delta V}$$

**Transport coefficients from time correlations:**
$$\eta = \frac{V}{k_B T}\int_0^\infty \langle \sigma_{xy}(t) \sigma_{xy}(0) \rangle dt$$

This is the **Green-Kubo formula** — the Rosetta Stone of multiscale physics.

---

## 📖 Implementation Sketch

### Example: Add Polarization to Your Code

```cpp
// File: atomistic/models/drude.hpp
#pragma once
#include "../core/state.hpp"

namespace atomistic {

struct DrudeParams {
    std::vector<double> alpha;  // Polarizabilities (Å³)
    double k_drude = 500.0;     // Spring constant (kcal/mol/Å²)
};

class DrudeModel {
public:
    void eval(State& s, const DrudeParams& params);
    
private:
    void compute_electric_field(const State& s, std::vector<Vec3>& E);
    void update_drude_positions(State& s, const std::vector<Vec3>& E);
};

} // namespace atomistic
```

**Algorithm:**
1. Compute electric field at each atom: $\mathbf{E}_i = \sum_{j\neq i} \frac{q_j \mathbf{r}_{ij}}{r_{ij}^3}$
2. Update Drude oscillator positions: $\mathbf{r}_{\text{drude},i} = \mathbf{r}_i + \alpha_i \mathbf{E}_i / k_{\text{drude}}$
3. Compute induced dipole forces: $\mathbf{F}_i = -k_{\text{drude}}(\mathbf{r}_{\text{drude},i} - \mathbf{r}_i)$

**Iterative solve** (self-consistency):
```cpp
for (int iter = 0; iter < max_iter; ++iter) {
    compute_electric_field(s, E);
    update_drude_positions(s, E);
    if (converged(E, E_old)) break;
    E_old = E;
}
```

---

## 🚀 Next Steps

1. **Prioritize polarization** — biggest bang for buck
2. **Read Green-Kubo theory** — unifies transport properties
3. **Implement Irving-Kirkwood** — stress tensor for fluids
4. **Test DPD** on simple system (polymer melt)
5. **Couple to LBM** for fluid boundaries

Want me to implement any of these? The polarization module would be a great next step after Item #7!

---

**References (Key Papers):**

1. **Polarization:** Lamoureux & Roux, *J. Chem. Phys.* 119, 5185 (2003)
2. **3-body dispersion:** Hermann et al., *Chem. Rev.* 117, 4714 (2017)
3. **Green-Kubo:** Evans & Morriss, *Statistical Mechanics of Nonequilibrium Liquids* (1990)
4. **MD-CFD coupling:** O'Connell & Thompson, *Phys. Rev. E* 52, R5792 (1995)
5. **DPD:** Groot & Warren, *J. Chem. Phys.* 107, 4423 (1997)
6. **LBM:** Succi, *The Lattice Boltzmann Equation* (Oxford, 2001)
7. **PIMD:** Ceriotti et al., *Chem. Rev.* 116, 7529 (2016)
8. **LFMM:** Comba & Remenyi, *Coord. Chem. Rev.* 238-239, 9 (2003)

---

**Last Updated:** January 17, 2025  
**Status:** Research roadmap for physics extensions  
**Priority:** Polarization → 3-body → Multiscale coupling
