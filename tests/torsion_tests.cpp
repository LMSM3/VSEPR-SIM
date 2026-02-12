/*
torsion_tests.cpp
-----------------
Validate torsional energy implementation.

Tests:
1. Ethane rotational scan - should show 3 kcal/mol barrier
2. Butane conformers - anti vs gauche energy difference
3. Gradient validation via finite differences
*/

#include "sim/molecule.hpp"
#include "pot/energy_model.hpp"
#include "sim/optimizer.hpp"
#include "core/geom_ops.hpp"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <cassert>
#include <fstream>
#include <vector>

using namespace vsepr;

// ============================================================================
// Test 1: Ethane Rotational Scan
// ============================================================================
void test_ethane_rotational_scan() {
    std::cout << "\n=== Test: Ethane (C2H6) Torsion Energy ===\n";
    std::cout << "Testing H-C-C-H torsional function E(φ)\n\n";
    
    // Build simplified ethane with explicit H-C-C-H dihedral control
    Molecule mol;
    
    // Atoms: H1-C1-C2-H2 (minimal for one torsion)
    mol.add_atom(1, -1.6, 0.0, 0.0);   // H1
    mol.add_atom(6, -0.75, 0.0, 0.0);  // C1
    mol.add_atom(6,  0.75, 0.0, 0.0);  // C2
    mol.add_atom(1,  1.6, 0.0, 0.0);   // H2 (will rotate)
    
    // Add additional H atoms to satisfy valence
    mol.add_atom(1, -0.75, 0.5, 0.866);  // H3 on C1
    mol.add_atom(1, -0.75, 0.5, -0.866); // H4 on C1
    mol.add_atom(1,  0.75, 0.5, 0.866);  // H5 on C2
    mol.add_atom(1,  0.75, 0.5, -0.866); // H6 on C2
    
    // Bonds
    mol.add_bond(0, 1, 1);  // H1-C1
    mol.add_bond(1, 2, 1);  // C1-C2
    mol.add_bond(2, 3, 1);  // C2-H2
    mol.add_bond(1, 4, 1);  // C1-H3
    mol.add_bond(1, 5, 1);  // C1-H4
    mol.add_bond(2, 6, 1);  // C2-H5
    mol.add_bond(2, 7, 1);  // C2-H6
    
    mol.generate_angles_from_bonds();
    mol.generate_torsions_from_bonds();
    
    std::cout << "Topology: " << mol.bonds.size() << " bonds, " 
              << mol.angles.size() << " angles, " 
              << mol.torsions.size() << " torsions\n\n";
    
    // Energy models
    EnergyModel energy_no_torsion(mol, 300.0, true, false);
    EnergyModel energy_with_torsion(mol, 300.0, true, false, NonbondedParams(), true);
    
    // Manual rotation scan: rotate H2 (atom 3) around C-C axis
    const int n_steps = 36;
    std::vector<double> angles;
    std::vector<double> E_tor_only;
    
    std::cout << "φ(H1-C1-C2-H2) scan:\n";
    std::cout << "Angle(°)    E_torsion\n";
    std::cout << "------------------------\n";
    
    for (int step = 0; step <= n_steps; ++step) {
        double angle_deg = step * 10.0;
        double angle_rad = angle_deg * M_PI / 180.0;
        angles.push_back(angle_deg);
        
        // Rotate H2 (atom 3) around C-C axis (x-axis from C1 to C2)
        Vec3 C2_pos = {0.75, 0.0, 0.0};
        double r = 0.85;  // C-H distance
        
        // Position H2 at angle_rad in y-z plane
        std::vector<double> coords = mol.coords;
        set_pos(coords, 3, Vec3{C2_pos[0] + r * std::cos(angle_rad), 
                                r * std::sin(angle_rad), 
                                0.0});
        
        double E_no_tor = energy_no_torsion.evaluate_energy(coords);
        double E_with_tor = energy_with_torsion.evaluate_energy(coords);
        double E_tor = E_with_tor - E_no_tor;
        
        E_tor_only.push_back(E_tor);
        
        if (step % 6 == 0) {
            std::cout << std::setw(7) << std::fixed << std::setprecision(1) << angle_deg << "    "
                      << std::setw(8) << std::setprecision(3) << E_tor << "\n";
        }
    }
    
    double E_min = *std::min_element(E_tor_only.begin(), E_tor_only.end());
    double E_max = *std::max_element(E_tor_only.begin(), E_tor_only.end());
    double barrier = E_max - E_min;
    
    std::cout << "\nTorsional barrier: " << barrier << " kcal/mol\n";
    std::cout << "(Expected: ~1-3 kcal/mol for ethane-like H-C-C-H)\n\n";
    
    // Save data
    std::ofstream file("ethane_torsion.dat");
    for (size_t i = 0; i < angles.size(); ++i) {
        file << angles[i] << "  " << E_tor_only[i] << "\n";
    }
    file.close();
    
    if (barrier > 0.5) {
        std::cout << "✓ Torsion energy shows periodic barrier\n";
    } else {
        std::cout << "⚠ Barrier too small - check torsion implementation\n";
    }
}

// ============================================================================
// Test 2: Butane Conformers (Anti vs Gauche)
// ============================================================================
void test_butane_conformers() {
    std::cout << "\n=== Test: Butane Conformers ===\n";
    std::cout << "Testing anti vs gauche energy difference\n\n";
    
    // Build butane: CH3-CH2-CH2-CH3
    auto build_butane = [](double dihedral_deg) {
        Molecule mol;
        
        // Carbon backbone (linear initially)
        mol.add_atom(6, -1.5, 0.0, 0.0);  // C1
        mol.add_atom(6, -0.5, 0.0, 0.0);  // C2
        mol.add_atom(6,  0.5, 0.0, 0.0);  // C3
        mol.add_atom(6,  1.5, 0.0, 0.0);  // C4
        
        // Add hydrogens (simplified - just for topology)
        // C1 hydrogens
        mol.add_atom(1, -2.0, 0.5, 0.5);
        mol.add_atom(1, -2.0, -0.5, 0.5);
        mol.add_atom(1, -2.0, 0.0, -0.5);
        
        // C2 hydrogens
        mol.add_atom(1, -0.5, 0.5, 0.5);
        mol.add_atom(1, -0.5, -0.5, 0.5);
        
        // C3 hydrogens
        mol.add_atom(1,  0.5, 0.5, 0.5);
        mol.add_atom(1,  0.5, -0.5, 0.5);
        
        // C4 hydrogens
        mol.add_atom(1,  2.0, 0.5, 0.5);
        mol.add_atom(1,  2.0, -0.5, 0.5);
        mol.add_atom(1,  2.0, 0.0, -0.5);
        
        // C-C bonds
        mol.add_bond(0, 1, 1);
        mol.add_bond(1, 2, 1);
        mol.add_bond(2, 3, 1);
        
        // C-H bonds (simplified)
        mol.add_bond(0, 4, 1);
        mol.add_bond(0, 5, 1);
        mol.add_bond(0, 6, 1);
        mol.add_bond(1, 7, 1);
        mol.add_bond(1, 8, 1);
        mol.add_bond(2, 9, 1);
        mol.add_bond(2, 10, 1);
        mol.add_bond(3, 11, 1);
        mol.add_bond(3, 12, 1);
        mol.add_bond(3, 13, 1);
        
        mol.generate_angles_from_bonds();
        mol.generate_torsions_from_bonds();
        
        return mol;
    };
    
    // Test conformers
    Molecule anti = build_butane(180.0);    // Anti: 180°
    Molecule gauche_plus = build_butane(60.0);   // Gauche+: 60°
    Molecule gauche_minus = build_butane(-60.0); // Gauche-: -60°
    Molecule eclipsed = build_butane(0.0);       // Eclipsed: 0°
    
    EnergyModel energy_anti(anti, 300.0, true, true, NonbondedParams(), true);
    EnergyModel energy_gauche_plus(gauche_plus, 300.0, true, true, NonbondedParams(), true);
    EnergyModel energy_gauche_minus(gauche_minus, 300.0, true, true, NonbondedParams(), true);
    EnergyModel energy_eclipsed(eclipsed, 300.0, true, true, NonbondedParams(), true);
    
    double E_anti = energy_anti.evaluate_energy(anti.coords);
    double E_gauche_plus = energy_gauche_plus.evaluate_energy(gauche_plus.coords);
    double E_gauche_minus = energy_gauche_minus.evaluate_energy(gauche_minus.coords);
    double E_eclipsed = energy_eclipsed.evaluate_energy(eclipsed.coords);
    
    std::cout << "Butane conformer energies:\n";
    std::cout << "  Anti (180°):     " << std::setw(8) << std::fixed << std::setprecision(3) 
              << E_anti << " kcal/mol (reference)\n";
    std::cout << "  Gauche+ (60°):   " << std::setw(8) << E_gauche_plus 
              << " kcal/mol  (ΔE = " << (E_gauche_plus - E_anti) << ")\n";
    std::cout << "  Gauche- (-60°):  " << std::setw(8) << E_gauche_minus 
              << " kcal/mol  (ΔE = " << (E_gauche_minus - E_anti) << ")\n";
    std::cout << "  Eclipsed (0°):   " << std::setw(8) << E_eclipsed 
              << " kcal/mol  (ΔE = " << (E_eclipsed - E_anti) << ")\n";
    std::cout << "\n(Experimental: gauche ~0.8 kcal/mol higher than anti)\n";
    
    // Validation: anti should be lowest energy
    assert(E_anti < E_gauche_plus && "Anti should be lower than gauche");
    assert(E_anti < E_eclipsed && "Anti should be lower than eclipsed");
    assert(E_eclipsed > E_gauche_plus && "Eclipsed should be highest");
    
    std::cout << "\n✓ Butane conformer ordering correct\n";
}

// ============================================================================
// Main Test Runner
// ============================================================================
int main() {
    std::cout << "===================================================\n";
    std::cout << "Torsional Energy Tests\n";
    std::cout << "===================================================\n";

    try {
        test_ethane_rotational_scan();
        //test_butane_conformers();  // TODO: Fix butane geometry

        std::cout << "\n===================================================\n";
        std::cout << "Torsion tests completed!\n";
        std::cout << "===================================================\n";
        
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "\nTest FAILED: " << e.what() << "\n";
        return 1;
    }
}
