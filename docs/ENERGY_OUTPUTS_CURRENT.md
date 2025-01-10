# Current Energy Outputs & Reaction Capabilities
## Formation Engine v0.1 — What's Implemented

**Date:** January 17, 2025  
**Status:** Production system with ongoing reaction development

---

## ✅ Energy Components Currently Tracked

The system tracks **6 energy components** via the `EnergyTerms` struct:

### 1. **Bond Energy (`Ubond`)**

**Formula:**
$$E_{\text{bond}} = \sum_{i<j} k_{ij} (r_{ij} - r_0)^2$$

**What it tracks:**
- Covalent bond stretching/compression
- Based on UFF (Universal Force Field) parameters
- Harmonic potential approximation

**Units:** kcal/mol

**Typical values:**
- Single bond (C-C): ~80 kcal/mol
- Double bond (C=C): ~145 kcal/mol  
- Triple bond (C≡C): ~200 kcal/mol

---

### 2. **Angle Energy (`Uangle`)**

**Formula:**
$$E_{\text{angle}} = \sum_{\theta} k_\theta (\theta - \theta_0)^2$$

**What it tracks:**
- Bond angle bending
- Maintains molecular geometry
- Tetrahedral, trigonal planar, linear, etc.

**Units:** kcal/mol

**Typical values:**
- Methane (H-C-H): θ₀ = 109.5°, k ≈ 50 kcal/(mol·rad²)
- Water (H-O-H): θ₀ = 104.5°, k ≈ 75 kcal/(mol·rad²)

---

### 3. **Torsion Energy (`Utors`)**

**Formula:**
$$E_{\text{tors}} = \sum_{\phi} \frac{V_n}{2} [1 + \cos(n\phi - \delta)]$$

**What it tracks:**
- Dihedral angle rotation
- Conformational barriers (e.g., eclipsed vs. staggered)
- n = periodicity (2 for ethylene, 3 for ethane)

**Units:** kcal/mol

**Typical values:**
- Ethane rotation barrier: ~3 kcal/mol
- Peptide bond rotation: ~20 kcal/mol (partial double bond character)

---

### 4. **van der Waals (`UvdW`)**

**Formula:**
$$E_{\text{vdW}} = \sum_{i<j} 4\epsilon_{ij} \left[\left(\frac{\sigma_{ij}}{r_{ij}}\right)^{12} - \left(\frac{\sigma_{ij}}{r_{ij}}\right)^6\right]$$

**What it tracks:**
- Lennard-Jones 12-6 potential
- Dispersion (attractive) and Pauli repulsion
- Non-bonded interactions

**Units:** kcal/mol

**Typical values:**
- ε (well depth): 0.05-0.5 kcal/mol per atom pair
- σ (equilibrium distance): 3-4 Å

---

### 5. **Coulomb Energy (`UCoul`)**

**Formula:**
$$E_{\text{Coul}} = \sum_{i<j} \frac{q_i q_j}{4\pi\epsilon_0 r_{ij}}$$

**What it tracks:**
- Electrostatic interactions
- Partial charges (from QEq or fixed)
- Long-range interactions

**Units:** kcal/mol

**Typical values:**
- Ion pairs: ~100 kcal/mol at 2 Å
- Hydrogen bonds: 3-10 kcal/mol
- Dipole-dipole: 1-5 kcal/mol

---

### 6. **External Energy (`Uext`)**

**Formula:** User-defined

**What it tracks:**
- External fields (electric, magnetic)
- Constraints (position restraints)
- Reaction coordinate biasing

**Units:** kcal/mol

---

## 🧪 Total Energy

**Formula:**
$$E_{\text{total}} = U_{\text{bond}} + U_{\text{angle}} + U_{\text{tors}} + U_{\text{vdW}} + U_{\text{Coul}} + U_{\text{ext}}$$

**Accessed via:**
```cpp
State s = /* ... */;
double E_total = s.E.total();
```

**Or individually:**
```cpp
double bond_energy = s.E.Ubond;
double vdw_energy = s.E.UvdW;
// etc.
```

---

## ⚗️ Reaction Energy Calculations

The `ReactionEngine` calculates additional reaction-specific energies:

### 1. **Reaction Energy (ΔE_rxn)**

**Formula:**
$$\Delta E_{\text{rxn}} = E_{\text{products}} - E_{\text{reactants}}$$

**Interpretation:**
- **Negative:** Exothermic (releases energy, favorable)
- **Positive:** Endothermic (requires energy input)
- **Zero:** Thermoneutral

**How it's calculated:**
```cpp
ProposedReaction rxn;
rxn.reaction_energy = predict::predict_reaction_energy(
    rxn.reactant_A, rxn.reactant_B,
    rxn.product_C, rxn.product_D
);
```

**Uses:** Sum of `EnergyTerms.total()` for reactants vs. products

---

### 2. **Activation Barrier (E_a)**

**Formula (Bell-Evans-Polanyi):**
$$E_a = E_0 + \alpha \cdot \Delta E_{\text{rxn}}$$

**Where:**
- $E_0$ = Intrinsic barrier (reaction-type dependent)
- $\alpha$ = Transfer coefficient (~0.5)
- $\Delta E_{\text{rxn}}$ = Reaction energy

**How it's calculated:**
```cpp
rxn.activation_barrier = predict::predict_activation_barrier(
    rxn.reactant_A, rxn.product_C,
    15.0  // E₀ for this reaction type
);
```

**Typical values:**
- Proton transfer: 5-15 kcal/mol
- SN2 substitution: 15-25 kcal/mol
- Peptide bond formation: 18-25 kcal/mol
- Diels-Alder: 20-30 kcal/mol

---

### 3. **Rate Constant (k)**

**Formula (Arrhenius):**
$$k = A \cdot e^{-E_a / RT}$$

**Where:**
- $A$ = Pre-exponential factor (~10¹³ s⁻¹ for unimolecular)
- $R$ = Gas constant (1.987 cal/(mol·K))
- $T$ = Temperature (K)

**How it's calculated:**
```cpp
double A = 1e13;  // s⁻¹
double RT = BOLTZMANN_KCAL * TEMPERATURE;
rxn.rate_constant = A * std::exp(-rxn.activation_barrier / RT);
```

**Example at 298 K:**
- E_a = 10 kcal/mol → k ≈ 10⁹ s⁻¹ (fast)
- E_a = 20 kcal/mol → k ≈ 10³ s⁻¹ (moderate)
- E_a = 30 kcal/mol → k ≈ 10⁻³ s⁻¹ (slow)

---

## 📊 Reaction Scoring

The engine computes **4 scoring components**:

### 1. Reactivity Score (0-1)

**Based on:** Fukui function matching

**Formula:**
$$\text{score} = \frac{\min(f^+_{\text{attack}}, f^-_{\text{target}})}{0.5}$$

**Interpretation:**
- 1.0 = Perfect nucleophile-electrophile match
- 0.5 = Moderate reactivity
- 0.0 = No reactivity

### 2. Geometric Score (0-1)

**Based on:** Orbital overlap quality

**Formula:**
$$\text{score} = \exp\left(-\frac{(r - r_{\text{opt}})^2}{r_{\text{opt}}^2}\right)$$

**Factors:**
- Interatomic distance (must be 1.5-2.5 Å for bond formation)
- Bond angles (e.g., SN2 requires ~180° backside attack)
- Steric accessibility

### 3. Thermodynamic Score (0-1)

**Based on:** Barrier + exothermicity

**Formula:**
$$\text{score} = 0.6 \cdot e^{-E_a/20} + 0.4 \cdot (1 - e^{\Delta E/30})$$

**Favors:**
- Low activation barriers
- Exothermic reactions

### 4. Overall Score (0-1)

**Weighted combination:**
$$\text{score} = 0.4 \cdot \text{reactivity} + 0.3 \cdot \text{geometric} + 0.3 \cdot \text{thermodynamic}$$

**Usage:**
- Threshold for reaction acceptance (e.g., score ≥ 0.5)
- Ranking multiple candidate reactions
- Validation of template applicability

---

## 🔬 Example Energy Profile

### HCl + NH₃ → NH₄⁺Cl⁻ (Proton Transfer)

**Reactants:**
```
HCl:
  Ubond   = -50.0 kcal/mol
  Uangle  =   5.0
  UvdW    = -10.0
  UCoul   = -15.0
  Total   = -70.0 kcal/mol

NH₃:
  Ubond   = -60.0
  Uangle  =   8.0
  UvdW    =  -8.0
  UCoul   = -12.0
  Total   = -72.0 kcal/mol

Reactants Total = -142.0 kcal/mol
```

**Transition State:**
```
[H···Cl···H-NH₃]‡
  Ubond   = -85.0 (weakened H-Cl, forming N-H)
  Uangle  =  20.0 (strained)
  UvdW    = -15.0
  UCoul   = -20.0
  Total   = -100.0 kcal/mol

E_a = -100.0 - (-142.0) = 42.0 kcal/mol (barrier)
```

**Products:**
```
NH₄⁺Cl⁻:
  Ubond   = -95.0 (strong N-H bonds)
  Uangle  =  10.0
  UvdW    = -12.0
  UCoul   = -80.0 (ionic attraction!)
  Total   = -177.0 kcal/mol

ΔE_rxn = -177.0 - (-142.0) = -35.0 kcal/mol (exothermic)
```

---

## 🎯 Current Capabilities Summary

### ✅ What Works Now

1. **Energy evaluation** for all 6 components
2. **Reaction energy** (ΔE_rxn) calculation
3. **Activation barrier** estimation (BEP relation)
4. **Rate constant** prediction (Arrhenius)
5. **Reaction scoring** (reactivity, geometry, thermodynamics)
6. **Heat-gated control** (temperature → template selection)

### 🚧 In Development

1. **QEq charge equilibration** (dynamic partial charges)
2. **Explicit solvent** (water molecules, solvation energy)
3. **Transition state optimization** (saddle point search)
4. **IRC (Intrinsic Reaction Coordinate)** path finding
5. **Multi-step reaction pathways** (catalytic cycles)

### 📅 Future Enhancements

1. **NEB (Nudged Elastic Band)** for reaction paths
2. **Free energy calculations** (ΔG via thermodynamic integration)
3. **Solvent effects** (PCM, COSMO implicit models)
4. **Enzyme catalysis** (active site modeling)
5. **Machine learning** potentials (ANI, SchNet)

---

## 📖 Where to Find This in Code

### Energy Terms
```cpp
// File: atomistic/core/state.hpp
struct EnergyTerms {
    double Ubond, Uangle, Utors, UvdW, UCoul, Uext;
    double total() const;
};

struct State {
    EnergyTerms E;  // Current energy breakdown
};
```

### Reaction Energy
```cpp
// File: atomistic/reaction/engine.cpp
void ReactionEngine::estimate_energetics(ProposedReaction& rxn) {
    rxn.reaction_energy = predict::predict_reaction_energy(...);
    rxn.activation_barrier = predict::predict_activation_barrier(...);
    rxn.rate_constant = A * exp(-rxn.activation_barrier / RT);
}
```

### Energy Evaluation
```cpp
// File: src/pot/energy.hpp
struct EnergyResult {
    double total_energy;
    double bond_energy;
    double angle_energy;
    // ... etc
};
```

---

## 🧪 How to Run a Simple Reaction

### Method 1: Using the Example

```bash
# Build the example
cd examples
g++ -std=c++20 -I.. -o simple_reaction_example simple_reaction_example.cpp \
    ../atomistic/reaction/engine.cpp ../atomistic/reaction/heat_gate.cpp

# Run it
./simple_reaction_example
```

### Method 2: From Your Code

```cpp
#include "atomistic/reaction/engine.hpp"

// Create reactants
State hcl = create_hcl();
State nh3 = create_nh3();

// Initialize engine
ReactionEngine engine;
engine.load_standard_templates();

// Set temperature (affects heat parameter)
double T_kelvin = 298.0;
uint16_t h = temperature_to_heat(T_kelvin);
engine.set_heat(h);

// Discover reactions
auto reactions = engine.propose_reactions(hcl, nh3);

// Check energetics
for (const auto& rxn : reactions) {
    std::cout << "Reaction: " << rxn.description << "\n";
    std::cout << "  ΔE = " << rxn.reaction_energy << " kcal/mol\n";
    std::cout << "  E_a = " << rxn.activation_barrier << " kcal/mol\n";
    std::cout << "  k = " << rxn.rate_constant << " s⁻¹\n";
    std::cout << "  Score = " << rxn.overall_score << "\n";
}
```

---

## 📚 References

### Internal Documentation
- **Section 3:** `docs/section3_interaction_model.tex` — LJ, Coulomb, UFF
- **Section 8-9:** `docs/section8_9_reaction_electronic.tex` — Fukui, HSAB, BEP
- **Section 8b:** `docs/section8b_heat_gated_reaction_control.tex` — Heat parameter

### Code Files
- **Energy:** `atomistic/core/state.hpp`, `src/pot/energy.hpp`
- **Reactions:** `atomistic/reaction/engine.{hpp,cpp}`
- **Heat gate:** `atomistic/reaction/heat_gate.{hpp,cpp}`

### External References
- **UFF:** Rappé & Goddard, J. Phys. Chem. 1991
- **BEP:** Bell, Trans. Faraday Soc. 1931; Evans & Polanyi, Trans. Faraday Soc. 1938
- **HSAB:** Pearson, J. Am. Chem. Soc. 1963

---

**Last Updated:** January 17, 2025  
**Version:** 0.1  
**Status:** Production for simple organics, development for biochemistry
