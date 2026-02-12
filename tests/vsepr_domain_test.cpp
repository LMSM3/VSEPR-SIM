/*
vsepr_domain_test.cpp
---------------------
Test explicit VSEPR electron domain repulsion energy.

Tests:
- H2O with 2 lone pairs: should optimize to ~104° H-O-H
- NH3 with 1 lone pair: should optimize to ~107° H-N-H  
- CH4 with 0 lone pairs: should optimize to 109.5° H-C-H
*/

#include "sim/molecule.hpp"
#include "pot/energy_vsepr.hpp"
#include "sim/optimizer.hpp"
#include <iostream>
#include <iomanip>

using namespace vsepr;

void test_water() {
    std::cout << "\n=== Test: H2O with VSEPR Domain Repulsion ===\n";
    
    Molecule mol;
    
    // Oxygen with 2 lone pairs
    mol.add_atom(8, 0.0, 0.0, 0.0);
    mol.atoms[0].lone_pairs = 2;
    
    // Two hydrogens (rough geometry)
    mol.add_atom(1, 1.0, 0.0, 0.0);
    mol.add_atom(1, 0.0, 1.0, 0.0);
    
    mol.add_bond(0, 1, 1);
    mol.add_bond(0, 2, 1);
    
    std::cout << "Atoms: " << mol.num_atoms() << "\n";
    std::cout << "Bonds: " << mol.bonds.size() << "\n";
    std::cout << "Lone pairs on O: " << static_cast<int>(mol.atoms[0].lone_pairs) << "\n";
    
    // Create VSEPR energy
    VSEPREnergy vsepr_energy(mol.atoms, mol.bonds);
    
    std::cout << "Total lone pairs: " << vsepr_energy.count_total_lone_pairs() << "\n";
    
    // Extended coordinates: [atom coords, lone pair directions]
    std::vector<double> coords = mol.coords;  // Copy atom coordinates
    vsepr_energy.initialize_lone_pair_directions(coords);
    
    std::cout << "Extended coord size: " << coords.size() 
              << " (atoms: " << 3 * mol.num_atoms() 
              << " + LPs: " << 3 * vsepr_energy.count_total_lone_pairs() << ")\n";
    
    // Evaluate initial energy
    std::vector<double> gradient(coords.size());
    double E_init = vsepr_energy.evaluate(coords, gradient);
    
    std::cout << "\nInitial VSEPR energy: " << std::fixed << std::setprecision(4) 
              << E_init << " kcal/mol\n";
    
    // Measure initial H-O-H angle
    double angle_init = angle(coords, 1, 0, 2) * 180.0 / M_PI;
    std::cout << "Initial H-O-H angle: " << std::setprecision(1) << angle_init << "°\n";
    
    std::cout << "\n✓ H2O VSEPR domain energy initialized\n";
    std::cout << "  Expected: Lone pairs should push H-O-H to ~104°\n";
}

void test_ammonia() {
    std::cout << "\n=== Test: NH3 with VSEPR Domain Repulsion ===\n";
    
    Molecule mol;
    
    // Nitrogen with 1 lone pair
    mol.add_atom(7, 0.0, 0.0, 0.0);
    mol.atoms[0].lone_pairs = 1;
    
    // Three hydrogens
    mol.add_atom(1, 1.0, 0.0, 0.0);
    mol.add_atom(1, -0.5, 0.866, 0.0);
    mol.add_atom(1, -0.5, -0.866, 0.0);
    
    mol.add_bond(0, 1, 1);
    mol.add_bond(0, 2, 1);
    mol.add_bond(0, 3, 1);
    
    std::cout << "Atoms: " << mol.num_atoms() << "\n";
    std::cout << "Bonds: " << mol.bonds.size() << "\n";
    std::cout << "Lone pairs on N: " << static_cast<int>(mol.atoms[0].lone_pairs) << "\n";
    
    VSEPREnergy vsepr_energy(mol.atoms, mol.bonds);
    
    std::vector<double> coords = mol.coords;
    vsepr_energy.initialize_lone_pair_directions(coords);
    
    std::vector<double> gradient(coords.size());
    double E_init = vsepr_energy.evaluate(coords, gradient);
    
    std::cout << "\nInitial VSEPR energy: " << std::fixed << std::setprecision(4) 
              << E_init << " kcal/mol\n";
    
    double angle_init = angle(coords, 1, 0, 2) * 180.0 / M_PI;
    std::cout << "Initial H-N-H angle: " << std::setprecision(1) << angle_init << "°\n";
    
    std::cout << "\n✓ NH3 VSEPR domain energy initialized\n";
    std::cout << "  Expected: Lone pair should push H-N-H to ~107°\n";
}

void test_methane() {
    std::cout << "\n=== Test: CH4 with VSEPR Domain Repulsion ===\n";
    
    Molecule mol;
    
    // Carbon with 0 lone pairs
    mol.add_atom(6, 0.0, 0.0, 0.0);
    mol.atoms[0].lone_pairs = 0;
    
    // Four hydrogens
    mol.add_atom(1, 1.0, 0.0, 0.0);
    mol.add_atom(1, 0.0, 1.0, 0.0);
    mol.add_atom(1, 0.0, 0.0, 1.0);
    mol.add_atom(1, -1.0, -1.0, -1.0);
    
    mol.add_bond(0, 1, 1);
    mol.add_bond(0, 2, 1);
    mol.add_bond(0, 3, 1);
    mol.add_bond(0, 4, 1);
    
    std::cout << "Atoms: " << mol.num_atoms() << "\n";
    std::cout << "Bonds: " << mol.bonds.size() << "\n";
    std::cout << "Lone pairs on C: " << static_cast<int>(mol.atoms[0].lone_pairs) << "\n";
    
    VSEPREnergy vsepr_energy(mol.atoms, mol.bonds);
    
    std::vector<double> coords = mol.coords;
    vsepr_energy.initialize_lone_pair_directions(coords);
    
    std::cout << "Extended coord size: " << coords.size() 
              << " (no lone pairs, so equals atom coords)\n";
    
    std::vector<double> gradient(coords.size());
    double E_init = vsepr_energy.evaluate(coords, gradient);
    
    std::cout << "\nInitial VSEPR energy: " << std::fixed << std::setprecision(4) 
              << E_init << " kcal/mol\n";
    
    double angle_init = angle(coords, 1, 0, 2) * 180.0 / M_PI;
    std::cout << "Initial H-C-H angle: " << std::setprecision(1) << angle_init << "°\n";
    
    std::cout << "\n✓ CH4 VSEPR domain energy initialized\n";
    std::cout << "  Expected: 4 bonds should spread to 109.5° (tetrahedral)\n";
}

int main() {
    std::cout << "===================================================\n";
    std::cout << "VSEPR Electron Domain Repulsion Tests\n";
    std::cout << "Testing explicit LP-LP, LP-BP, BP-BP interactions\n";
    std::cout << "===================================================\n";
    
    try {
        test_water();
        test_ammonia();
        test_methane();
        
        std::cout << "\n===================================================\n";
        std::cout << "All VSEPR domain tests completed!\n";
        std::cout << "Next: Integrate with optimizer for full geometry opt\n";
        std::cout << "===================================================\n";
        
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "\nTest FAILED: " << e.what() << "\n";
        return 1;
    }
}
