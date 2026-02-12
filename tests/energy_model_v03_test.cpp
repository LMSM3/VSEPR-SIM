// Test V0.3 Energy Model - Clean term composition
// Demonstrates:
// 1. Nonbonded exclusions (1-2, 1-3 properly excluded/scaled)
// 2. Angle terms weak or off by default
// 3. Clean term toggling

#include "sim/molecule.hpp"
#include "pot/energy_model.hpp"
#include <iostream>
#include <iomanip>

using namespace vsepr;

void print_breakdown(const std::string& label, const EnergyResult& result) {
    std::cout << "\n" << label << ":\n";
    std::cout << "  Total:     " << std::fixed << std::setprecision(4) << result.total_energy << " kcal/mol\n";
    std::cout << "  Bond:      " << result.bond_energy << " kcal/mol\n";
    std::cout << "  Angle:     " << result.angle_energy << " kcal/mol\n";
    std::cout << "  Nonbonded: " << result.nonbonded_energy << " kcal/mol\n";
    std::cout << "  Torsion:   " << result.torsion_energy << " kcal/mol\n";
    std::cout << "  VSEPR:     " << result.vsepr_energy << " kcal/mol\n";
}

void test_nonbonded_exclusions() {
    std::cout << "===================================================\n";
    std::cout << "Test 1: Nonbonded Exclusions\n";
    std::cout << "CH4 - should exclude 1-2 (C-H bonds) and scale 1-3 (H-H)\n";
    std::cout << "===================================================\n";
    
    Molecule mol;
    mol.add_atom(6, 0.0, 0.0, 0.0);      // C
    mol.add_atom(1, 1.09, 0.0, 0.0);     // H1
    mol.add_atom(1, -0.545, 0.943, 0.0); // H2
    mol.add_atom(1, -0.545, -0.471, 0.816); // H3
    mol.add_atom(1, -0.545, -0.471, -0.816); // H4
    
    for (int i = 1; i <= 4; ++i) {
        mol.add_bond(0, i, 1);
    }
    
    // Build with nonbonded only
    NonbondedParams nb_params;
    nb_params.epsilon = 0.1;
    nb_params.scale_13 = 0.5;
    nb_params.scale_14 = 0.8;
    
    // EnergyModel(mol, bond_k, use_angles, use_nonbonded, nb_params, use_torsions, use_vsepr, angle_scale)
    EnergyModel energy(mol, 300.0, false, true, nb_params, false, false, 0.1);
    auto breakdown = energy.evaluate_detailed(mol.coords);
    
    print_breakdown("CH4 with nonbonded (exclusions active)", breakdown);
    
    std::cout << "\nExpected behavior:\n";
    std::cout << "  - 5 bonds: 4x C-H, all should contribute bond energy\n";
    std::cout << "  - Nonbonded pairs: 4x C-H (1-2, excluded), 6x H-H (1-3, scaled 0.5)\n";
    std::cout << "  - Only H-H 1-3 pairs contribute (scaled) to nonbonded\n";
}

void test_angle_term_scaling() {
    std::cout << "\n\n===================================================\n";
    std::cout << "Test 2: Angle Term Scaling\n";
    std::cout << "NH3 - compare full angles vs weak angles\n";
    std::cout << "===================================================\n";
    
    Molecule mol;
    mol.add_atom(7, 0.0, 0.0, 0.0);      // N
    mol.add_atom(1, 1.0, 0.0, 0.0);      // H1
    mol.add_atom(1, -0.5, 0.866, 0.0);   // H2
    mol.add_atom(1, -0.5, -0.433, 0.75); // H3
    
    mol.add_bond(0, 1, 1);
    mol.add_bond(0, 2, 1);
    mol.add_bond(0, 3, 1);
    
    mol.generate_angles_from_bonds();
    
    // Full strength angles (old behavior)
    EnergyModel energy_full_angles(mol, 300.0, true, false, NonbondedParams(), false, false, 1.0);
    auto breakdown_full = energy_full_angles.evaluate_detailed(mol.coords);
    
    // Weak angles (v0.3 default)
    EnergyModel energy_weak_angles(mol, 300.0, true, false, NonbondedParams(), false, false, 0.1);
    auto breakdown_weak = energy_weak_angles.evaluate_detailed(mol.coords);
    
    // No angles
    EnergyModel energy_no_angles(mol, 300.0, false, false, NonbondedParams(), false, false, 0.1);
    auto breakdown_none = energy_no_angles.evaluate_detailed(mol.coords);
    
    print_breakdown("NH3 with FULL angles (scale=1.0)", breakdown_full);
    print_breakdown("NH3 with WEAK angles (scale=0.1)", breakdown_weak);
    print_breakdown("NH3 with NO angles", breakdown_none);
    
    std::cout << "\nAngle energy ratio:\n";
    std::cout << "  Weak/Full = " << std::fixed << std::setprecision(3) 
              << (breakdown_weak.angle_energy / breakdown_full.angle_energy) << "\n";
    std::cout << "  Expected: ~0.1 (10x reduction)\n";
}

void test_term_independence() {
    std::cout << "\n\n===================================================\n";
    std::cout << "Test 3: Clean Term Toggling\n";
    std::cout << "H2O - isolate each energy component\n";
    std::cout << "===================================================\n";
    
    Molecule mol;
    mol.add_atom(8, 0.0, 0.0, 0.0);      // O
    mol.add_atom(1, 0.96, 0.0, 0.0);     // H1
    mol.add_atom(1, -0.24, 0.93, 0.0);   // H2
    
    mol.add_bond(0, 1, 1);
    mol.add_bond(0, 2, 1);
    mol.generate_angles_from_bonds();
    
    // Only bonds
    EnergyModel energy_bonds_only(mol, 300.0, false, false, NonbondedParams(), false, false, 0.1);
    auto breakdown_bonds = energy_bonds_only.evaluate_detailed(mol.coords);
    
    // Bonds + weak angles
    EnergyModel energy_bonds_angles(mol, 300.0, true, false, NonbondedParams(), false, false, 0.1);
    auto breakdown_ba = energy_bonds_angles.evaluate_detailed(mol.coords);
    
    // Bonds + nonbonded
    NonbondedParams nb;
    nb.epsilon = 0.1;
    nb.scale_13 = 0.5;
    EnergyModel energy_bonds_nb(mol, 300.0, false, true, nb, false, false, 0.1);
    auto breakdown_bnb = energy_bonds_nb.evaluate_detailed(mol.coords);
    
    // All terms (V0.3 default: bonds + nonbonded, angles weak)
    EnergyModel energy_v03(mol, 300.0, true, true, nb, false, false, 0.1);
    auto breakdown_v03 = energy_v03.evaluate_detailed(mol.coords);
    
    print_breakdown("Bonds only", breakdown_bonds);
    print_breakdown("Bonds + weak angles", breakdown_ba);
    print_breakdown("Bonds + nonbonded", breakdown_bnb);
    print_breakdown("V0.3 default (bonds + NB + weak angles)", breakdown_v03);
    
    std::cout << "\nTerm isolation check:\n";
    std::cout << "  Bonds-only bond energy:  " << breakdown_bonds.bond_energy << "\n";
    std::cout << "  With angles bond energy: " << breakdown_ba.bond_energy << "\n";
    std::cout << "  Expected: Equal (terms independent)\n";
}

void test_policy_defaults() {
    std::cout << "\n\n===================================================\n";
    std::cout << "Test 4: V0.3 Policy Defaults\n";
    std::cout << "Default constructor behavior\n";
    std::cout << "===================================================\n";
    
    Molecule mol;
    mol.add_atom(6, 0.0, 0.0, 0.0);
    mol.add_atom(1, 1.0, 0.0, 0.0);
    mol.add_atom(1, 0.0, 1.0, 0.0);
    mol.add_bond(0, 1, 1);
    mol.add_bond(0, 2, 1);
    mol.generate_angles_from_bonds();
    
    // Old behavior (pre-v0.3): angles ON, nonbonded OFF
    EnergyModel energy_old(mol, 300.0, true, false, NonbondedParams(), false, false, 1.0);
    auto breakdown_old = energy_old.evaluate_detailed(mol.coords);
    
    // New behavior (v0.3): angles OFF by default, nonbonded ON with exclusions
    EnergyModel energy_new(mol, 300.0, false, true);  // V0.3 defaults
    auto breakdown_new = energy_new.evaluate_detailed(mol.coords);
    
    print_breakdown("Pre-V0.3 defaults (angles=ON, NB=OFF)", breakdown_old);
    print_breakdown("V0.3 defaults (angles=OFF, NB=ON)", breakdown_new);
    
    std::cout << "\nPolicy change:\n";
    std::cout << "  Old: Angle energy dominates, no NB exclusions\n";
    std::cout << "  New: Nonbonded with exclusions, angles off (or weak)\n";
    std::cout << "  Rationale: Avoid angle/domain term fighting\n";
}

int main() {
    std::cout << "===================================================\n";
    std::cout << "Energy Model V0.3 - Clean Term Composition\n";
    std::cout << "===================================================\n";
    std::cout << "Policy:\n";
    std::cout << "  1. Bonds: Always ON (essential)\n";
    std::cout << "  2. Nonbonded: ON by default with exclusions\n";
    std::cout << "     - 1-2 pairs (bonded): EXCLUDED\n";
    std::cout << "     - 1-3 pairs (angles): SCALED (0.5)\n";
    std::cout << "     - 1-4 pairs (torsions): SCALED (0.8)\n";
    std::cout << "  3. Angles: OFF by default (or weak, k*0.1)\n";
    std::cout << "  4. Domains: Optional geometry driver\n";
    std::cout << "  5. Torsions: Optional conformational term\n";
    std::cout << "===================================================\n\n";
    
    test_nonbonded_exclusions();
    test_angle_term_scaling();
    test_term_independence();
    test_policy_defaults();
    
    std::cout << "\n\n===================================================\n";
    std::cout << "All V0.3 energy model tests complete!\n";
    std::cout << "===================================================\n";
    
    return 0;
}
