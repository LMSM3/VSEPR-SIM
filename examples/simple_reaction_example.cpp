/**
 * simple_reaction_example.cpp
 * ---------------------------
 * Demonstrates a simple acid-base (proton transfer) reaction.
 * 
 * Shows current energy tracking capabilities and how heat parameter affects
 * reaction discovery.
 * 
 * Example: HCl + NH₃ → NH₄⁺Cl⁻
 * 
 * Build:
 *   g++ -std=c++20 -I.. -o simple_reaction_example simple_reaction_example.cpp \
 *       ../atomistic/reaction/engine.cpp ../atomistic/reaction/heat_gate.cpp
 */

#include "atomistic/core/state.hpp"
#include "atomistic/reaction/engine.hpp"
#include "atomistic/reaction/heat_gate.hpp"
#include <iostream>
#include <iomanip>

using namespace atomistic;
using namespace atomistic::reaction;

// ============================================================================
// Helper Functions
// ============================================================================

void print_header(const std::string& title) {
    std::cout << "\n╔════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  " << std::left << std::setw(62) << title << "║\n";
    std::cout << "╚════════════════════════════════════════════════════════════════╝\n\n";
}

void print_energy_terms(const EnergyTerms& E) {
    std::cout << "Energy Breakdown:\n";
    std::cout << "  Bond energy:      " << std::fixed << std::setprecision(4) 
              << std::setw(10) << E.Ubond << " kcal/mol\n";
    std::cout << "  Angle energy:     " << std::setw(10) << E.Uangle << " kcal/mol\n";
    std::cout << "  Torsion energy:   " << std::setw(10) << E.Utors << " kcal/mol\n";
    std::cout << "  van der Waals:    " << std::setw(10) << E.UvdW << " kcal/mol\n";
    std::cout << "  Coulomb:          " << std::setw(10) << E.UCoul << " kcal/mol\n";
    std::cout << "  External:         " << std::setw(10) << E.Uext << " kcal/mol\n";
    std::cout << "  ─────────────────────────────────────\n";
    std::cout << "  Total:            " << std::setw(10) << E.total() << " kcal/mol\n";
    std::cout << "\n";
}

State create_hcl_molecule() {
    State s;
    s.N = 2;
    
    // HCl: H at origin, Cl at 1.27 Å (experimental bond length)
    s.X = {
        {0.0, 0.0, 0.0},    // H
        {1.27, 0.0, 0.0}    // Cl
    };
    
    s.V.resize(2, {0.0, 0.0, 0.0});
    s.F.resize(2, {0.0, 0.0, 0.0});
    s.M = {1.008, 35.45};  // amu
    s.Q = {0.18, -0.18};   // Partial charges (H⁺, Cl⁻)
    s.type = {1, 17};      // Z values
    
    // Bond
    s.B.push_back({0, 1});
    
    return s;
}

State create_nh3_molecule() {
    State s;
    s.N = 4;
    
    // NH₃: Pyramidal geometry
    // N at origin, 3 H at tetrahedral positions
    double r_NH = 1.01;  // Å
    double angle = 107.3 * M_PI / 180.0;  // H-N-H angle
    
    s.X = {
        {0.0, 0.0, 0.0},                                     // N
        {r_NH, 0.0, 0.0},                                    // H1
        {r_NH * cos(angle), r_NH * sin(angle), 0.0},        // H2
        {r_NH * cos(angle), -r_NH * sin(angle), 0.0}        // H3
    };
    
    s.V.resize(4, {0.0, 0.0, 0.0});
    s.F.resize(4, {0.0, 0.0, 0.0});
    s.M = {14.007, 1.008, 1.008, 1.008};  // amu
    s.Q = {-0.4, 0.133, 0.133, 0.133};    // Partial charges
    s.type = {7, 1, 1, 1};                 // Z values
    
    // Bonds
    s.B.push_back({0, 1});
    s.B.push_back({0, 2});
    s.B.push_back({0, 3});
    
    return s;
}

// ============================================================================
// Current Energy Tracking Capabilities
// ============================================================================

void demonstrate_current_energy_system() {
    print_header("Current Energy Tracking System");
    
    std::cout << "The system tracks the following energy components:\n\n";
    
    std::cout << "1. **Bond Energy (Ubond)**\n";
    std::cout << "   Formula: E_bond = Σ k_ij (r_ij - r₀)²\n";
    std::cout << "   Where: k = force constant, r = distance, r₀ = equilibrium\n\n";
    
    std::cout << "2. **Angle Energy (Uangle)**\n";
    std::cout << "   Formula: E_angle = Σ k_θ (θ - θ₀)²\n";
    std::cout << "   Where: k_θ = force constant, θ = angle, θ₀ = equilibrium\n\n";
    
    std::cout << "3. **Torsion Energy (Utors)**\n";
    std::cout << "   Formula: E_tors = Σ V_n/2 [1 + cos(nφ - δ)]\n";
    std::cout << "   Where: V_n = barrier, n = periodicity, φ = dihedral\n\n";
    
    std::cout << "4. **van der Waals (UvdW)**\n";
    std::cout << "   Formula: E_vdW = Σ 4ε[(σ/r)¹² - (σ/r)⁶]\n";
    std::cout << "   Lennard-Jones 12-6 potential\n\n";
    
    std::cout << "5. **Coulomb (UCoul)**\n";
    std::cout << "   Formula: E_coul = Σ (q_i q_j)/(4πε₀ r_ij)\n";
    std::cout << "   Electrostatic interactions\n\n";
    
    std::cout << "6. **External (Uext)**\n";
    std::cout << "   User-defined external fields or constraints\n\n";
}

// ============================================================================
// Reaction Energy Calculations
// ============================================================================

void demonstrate_reaction_energetics() {
    print_header("Reaction Energy Calculations");
    
    std::cout << "For reactions, the system calculates:\n\n";
    
    std::cout << "1. **Reaction Energy (ΔE_rxn)**\n";
    std::cout << "   ΔE_rxn = E_products - E_reactants\n";
    std::cout << "   Units: kcal/mol\n";
    std::cout << "   Negative = exothermic (favorable)\n\n";
    
    std::cout << "2. **Activation Barrier (E_a)**\n";
    std::cout << "   Estimated via Bell-Evans-Polanyi (BEP) relation:\n";
    std::cout << "   E_a = E₀ + α·ΔE_rxn\n";
    std::cout << "   Where: E₀ = intrinsic barrier, α ≈ 0.5 (transfer coefficient)\n\n";
    
    std::cout << "3. **Rate Constant (k)**\n";
    std::cout << "   Arrhenius equation: k = A·exp(-E_a/RT)\n";
    std::cout << "   Where: A = pre-exponential factor (~10¹³ s⁻¹)\n";
    std::cout << "          R = gas constant, T = temperature\n\n";
    
    std::cout << "4. **Scoring Components:**\n";
    std::cout << "   • Reactivity score (0-1): Fukui function matching\n";
    std::cout << "   • Geometric score (0-1): Orbital overlap quality\n";
    std::cout << "   • Thermodynamic score (0-1): Barrier + exothermicity\n";
    std::cout << "   • Overall score: Weighted combination\n\n";
}

// ============================================================================
// Simple Proton Transfer Reaction
// ============================================================================

void demonstrate_proton_transfer() {
    print_header("Example: HCl + NH₃ → NH₄⁺Cl⁻ (Proton Transfer)");
    
    // Create molecules
    State hcl = create_hcl_molecule();
    State nh3 = create_nh3_molecule();
    
    std::cout << "Reactant A: HCl\n";
    std::cout << "  Atoms: " << hcl.N << "\n";
    std::cout << "  Bond: H-Cl (1.27 Å)\n";
    print_energy_terms(hcl.E);
    
    std::cout << "Reactant B: NH₃\n";
    std::cout << "  Atoms: " << nh3.N << "\n";
    std::cout << "  Geometry: Pyramidal (∠HNH = 107.3°)\n";
    print_energy_terms(nh3.E);
    
    // Initialize reaction engine
    ReactionEngine engine;
    
    std::cout << "Loading proton transfer template...\n";
    engine.add_template(proton_transfer_template());
    
    std::cout << "\nReaction template constraints:\n";
    std::cout << "  Mechanism: Acid-Base (proton transfer)\n";
    std::cout << "  Distance range: 1.2 - 2.5 Å (H-bond)\n";
    std::cout << "  Max barrier: 15 kcal/mol\n";
    std::cout << "  Min exothermicity: -3 kcal/mol\n\n";
    
    // Heat parameter effects
    std::cout << "Heat Parameter Effects:\n";
    std::cout << "  ℹ Proton transfer is in T_base (always active)\n";
    std::cout << "  ℹ Heat parameter doesn't gate acid-base reactions\n";
    std::cout << "  ℹ These are fundamental reactions at all temperatures\n\n";
}

// ============================================================================
// Heat-Gated Biochemical Example
// ============================================================================

void demonstrate_heat_gated_biochemical() {
    print_header("Heat-Gated Reaction: Peptide Bond Formation");
    
    std::cout << "Example: Gly + Ala → Gly-Ala + H₂O\n\n";
    
    std::cout << "This reaction is heat-gated:\n\n";
    
    // Different temperatures
    double temperatures[] = {77, 167, 298, 433, 666};
    
    std::cout << std::setw(15) << "Temperature"
              << std::setw(10) << "h"
              << std::setw(20) << "Mode"
              << std::setw(15) << "Peptide Active?\n";
    std::cout << "────────────────────────────────────────────────────────\n";
    
    for (double T : temperatures) {
        uint16_t h = temperature_to_heat(T);
        HeatGateController ctrl(h);
        
        std::string mode;
        std::string active;
        
        double m = ctrl.mode_index();
        if (m < 0.01) {
            mode = "Organic";
            active = "No";
        } else if (m < 0.99) {
            mode = "Transitional";
            active = "Partial";
        } else {
            mode = "Full Biochemical";
            active = "Yes";
        }
        
        std::cout << std::setw(15) << std::fixed << std::setprecision(0) << T << " K"
                  << std::setw(10) << h
                  << std::setw(20) << mode
                  << std::setw(15) << active << "\n";
    }
    
    std::cout << "\nEnergy considerations for peptide bond:\n";
    std::cout << "  ΔE_rxn ≈ +2 kcal/mol (endergonic in vacuum)\n";
    std::cout << "  E_a ≈ 18-25 kcal/mol (high barrier)\n";
    std::cout << "  Requires: T ≥ 433 K for h ≥ 650 (full activation)\n";
    std::cout << "  In biology: Enzyme-catalyzed (ribosome lowers E_a)\n\n";
}

// ============================================================================
// Energy Change During Reaction
// ============================================================================

void demonstrate_energy_tracking_during_reaction() {
    print_header("Energy Tracking During Reaction");
    
    std::cout << "Typical reaction energy profile:\n\n";
    
    std::cout << "  E\n";
    std::cout << "  │                   TS (transition state)\n";
    std::cout << "  │                   /\\\n";
    std::cout << "  │                  /  \\  E_a (activation)\n";
    std::cout << "  │                 /    \\\n";
    std::cout << "  │  Reactants ────       ──── Products\n";
    std::cout << "  │                          \\\n";
    std::cout << "  │                           \\__ΔE_rxn\n";
    std::cout << "  └────────────────────────────────────► Reaction coordinate\n\n";
    
    std::cout << "Energy components tracked:\n\n";
    
    EnergyTerms reactants;
    reactants.Ubond = -50.0;
    reactants.Uangle = 5.0;
    reactants.UvdW = -10.0;
    reactants.UCoul = -15.0;
    
    std::cout << "Reactants:\n";
    print_energy_terms(reactants);
    
    EnergyTerms transition_state;
    transition_state.Ubond = -35.0;   // Weakened old bond
    transition_state.Uangle = 15.0;   // Strained geometry
    transition_state.UvdW = -8.0;
    transition_state.UCoul = -12.0;
    
    std::cout << "Transition State:\n";
    print_energy_terms(transition_state);
    
    EnergyTerms products;
    products.Ubond = -55.0;   // Stronger new bond
    products.Uangle = 4.0;
    products.UvdW = -11.0;
    products.UCoul = -16.0;
    
    std::cout << "Products:\n";
    print_energy_terms(products);
    
    std::cout << "Reaction energy: ΔE_rxn = " 
              << std::fixed << std::setprecision(2)
              << (products.total() - reactants.total()) 
              << " kcal/mol (exothermic)\n";
    std::cout << "Activation barrier: E_a = "
              << (transition_state.total() - reactants.total())
              << " kcal/mol\n\n";
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "\n╔════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  Simple Reaction Example — Formation Engine v0.1              ║\n";
    std::cout << "║  Demonstrates current energy tracking and reaction discovery  ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════════╝\n";
    
    // Show what the system currently tracks
    demonstrate_current_energy_system();
    
    // Show reaction-specific calculations
    demonstrate_reaction_energetics();
    
    // Simple proton transfer example
    demonstrate_proton_transfer();
    
    // Heat-gated biochemical example
    demonstrate_heat_gated_biochemical();
    
    // Energy tracking during reaction
    demonstrate_energy_tracking_during_reaction();
    
    // Summary
    print_header("Summary");
    
    std::cout << "✅ Energy Components Tracked:\n";
    std::cout << "   • Bond, Angle, Torsion (bonded terms)\n";
    std::cout << "   • van der Waals (Lennard-Jones 12-6)\n";
    std::cout << "   • Coulomb (electrostatic)\n";
    std::cout << "   • External fields\n\n";
    
    std::cout << "✅ Reaction Energetics:\n";
    std::cout << "   • ΔE_rxn = E_products - E_reactants\n";
    std::cout << "   • E_a via Bell-Evans-Polanyi relation\n";
    std::cout << "   • Rate constant via Arrhenius equation\n\n";
    
    std::cout << "✅ Heat-Gated Control:\n";
    std::cout << "   • Temperature → heat parameter mapping\n";
    std::cout << "   • Organic vs. biochemical mode selection\n";
    std::cout << "   • Smooth template activation (Item #7)\n\n";
    
    std::cout << "📖 References:\n";
    std::cout << "   • Section 3: Interaction Model (LJ, Coulomb, UFF)\n";
    std::cout << "   • Section 8-9: Reaction prediction via Fukui/HSAB\n";
    std::cout << "   • Section 8b: Heat-gated reaction control\n\n";
    
    std::cout << "🚀 Next Steps:\n";
    std::cout << "   1. Run full MD with reaction discovery\n";
    std::cout << "   2. Integrate QEq charge equilibration\n";
    std::cout << "   3. Add explicit solvent for aqueous reactions\n";
    std::cout << "   4. Validate against experimental ΔH, E_a data\n\n";
    
    return 0;
}
