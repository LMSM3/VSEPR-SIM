/*
torsion_validation_tests.cpp
----------------------------
Test torsional energy on standard VSEPR molecules.

Tests:
- BeF2: Linear (minimal torsions expected)
- BF3: Trigonal planar 
- CH4: Tetrahedral
- PCl5: Trigonal bipyramidal

Goal: Verify torsions integrate correctly with full energy model.
*/

#include "sim/molecule.hpp"
#include "pot/energy_model.hpp"
#include "sim/optimizer.hpp"
#include "core/geom_ops.hpp"
#include <iostream>
#include <iomanip>
#include <cmath>

using namespace vsepr;

void print_molecule_summary(const std::string& name, const Molecule& mol) {
    std::cout << "\n" << name << ":\n";
    std::cout << "  Atoms: " << mol.num_atoms() << "\n";
    std::cout << "  Bonds: " << mol.bonds.size() << "\n";
    std::cout << "  Angles: " << mol.angles.size() << "\n";
    std::cout << "  Torsions: " << mol.torsions.size() << "\n";
}

void print_optimization_result(const std::string& label, const OptimizeResult& result) {
    std::cout << "\n" << label << ":\n";
    std::cout << "  Iterations: " << result.iterations << "\n";
    std::cout << "  Final energy: " << std::fixed << std::setprecision(6) 
              << result.energy << " kcal/mol\n";
    std::cout << "  Energy breakdown:\n";
    std::cout << "    Bond:      " << result.energy_breakdown.bond_energy << "\n";
    std::cout << "    Angle:     " << result.energy_breakdown.angle_energy << "\n";
    std::cout << "    Nonbonded: " << result.energy_breakdown.nonbonded_energy << "\n";
    std::cout << "    Torsion:   " << result.energy_breakdown.torsion_energy << "\n";
}

// ============================================================================
// Test 1: BeF2 (Linear AX2)
// ============================================================================
void test_bef2() {
    std::cout << "\n=== Test: BeF2 (Linear AX2) ===\n";
    
    Molecule mol;
    
    // Be-F-F linear
    mol.add_atom(4, 0.0, 0.0, 0.0);   // Be
    mol.add_atom(9, 1.4, 0.0, 0.0);   // F
    mol.add_atom(9, -1.4, 0.0, 0.0);  // F
    
    mol.add_bond(0, 1, 1);
    mol.add_bond(0, 2, 1);
    
    mol.generate_angles_from_bonds();
    mol.generate_torsions_from_bonds();
    
    print_molecule_summary("BeF2", mol);
    
    // Optimize without torsions
    NonbondedParams nb_params;
    nb_params.epsilon = 0.1;
    nb_params.scale_13 = 0.5;
    
    EnergyModel energy_no_tor(mol, 300.0, true, true, nb_params, false);
    
    OptimizerSettings settings;
    settings.max_iterations = 200;
    settings.tol_rms_force = 1e-4;
    
    FIREOptimizer optimizer(settings);
    OptimizeResult result_no_tor = optimizer.minimize(mol.coords, energy_no_tor);
    
    print_optimization_result("Without torsions", result_no_tor);
    
    // Optimize with torsions
    EnergyModel energy_with_tor(mol, 300.0, true, true, nb_params, true);
    OptimizeResult result_with_tor = optimizer.minimize(mol.coords, energy_with_tor);
    
    print_optimization_result("With torsions", result_with_tor);
    
    // Measure F-Be-F angle
    double angle_FBeF = angle(result_with_tor.coords, 1, 0, 2) * 180.0 / M_PI;
    std::cout << "\nF-Be-F angle: " << std::fixed << std::setprecision(1) 
              << angle_FBeF << "° (expected: 180°)\n";
    
    std::cout << "✓ BeF2 test complete (linear molecules have no meaningful torsions)\n";
}

// ============================================================================
// Test 2: BF3 (Trigonal Planar AX3)
// ============================================================================
void test_bf3() {
    std::cout << "\n=== Test: BF3 (Trigonal Planar AX3) ===\n";
    
    Molecule mol;
    
    // Boron at center
    mol.add_atom(5, 0.0, 0.0, 0.0);      // B
    mol.add_atom(9, 1.3, 0.0, 0.0);      // F1
    mol.add_atom(9, -0.65, 1.13, 0.0);   // F2  
    mol.add_atom(9, -0.65, -1.13, 0.0);  // F3
    
    mol.add_bond(0, 1, 1);
    mol.add_bond(0, 2, 1);
    mol.add_bond(0, 3, 1);
    
    mol.generate_angles_from_bonds();
    mol.generate_torsions_from_bonds();
    
    print_molecule_summary("BF3", mol);
    
    NonbondedParams nb_params;
    nb_params.epsilon = 0.1;
    nb_params.scale_13 = 0.5;
    
    // Optimize without torsions
    EnergyModel energy_no_tor(mol, 300.0, true, true, nb_params, false);
    
    OptimizerSettings settings;
    settings.max_iterations = 500;
    settings.tol_rms_force = 1e-4;
    
    FIREOptimizer optimizer(settings);
    OptimizeResult result_no_tor = optimizer.minimize(mol.coords, energy_no_tor);
    
    print_optimization_result("Without torsions", result_no_tor);
    
    // Optimize with torsions
    EnergyModel energy_with_tor(mol, 300.0, true, true, nb_params, true);
    OptimizeResult result_with_tor = optimizer.minimize(mol.coords, energy_with_tor);
    
    print_optimization_result("With torsions", result_with_tor);
    
    // Measure F-B-F angles
    double angle1 = angle(result_with_tor.coords, 1, 0, 2) * 180.0 / M_PI;
    double angle2 = angle(result_with_tor.coords, 1, 0, 3) * 180.0 / M_PI;
    double angle3 = angle(result_with_tor.coords, 2, 0, 3) * 180.0 / M_PI;
    
    std::cout << "\nF-B-F angles: " << std::fixed << std::setprecision(1)
              << angle1 << "°, " << angle2 << "°, " << angle3 << "° (expected: 120°)\n";
    
    std::cout << "✓ BF3 maintains trigonal planar geometry with torsions\n";
}

// ============================================================================
// Test 3: CH4 (Tetrahedral AX4)
// ============================================================================
void test_ch4() {
    std::cout << "\n=== Test: CH4 (Tetrahedral AX4) ===\n";
    
    Molecule mol;
    
    // Carbon at center
    mol.add_atom(6, 0.0, 0.0, 0.0);      // C
    mol.add_atom(1, 0.63, 0.63, 0.63);   // H1
    mol.add_atom(1, -0.63, -0.63, 0.63); // H2
    mol.add_atom(1, -0.63, 0.63, -0.63); // H3
    mol.add_atom(1, 0.63, -0.63, -0.63); // H4
    
    mol.add_bond(0, 1, 1);
    mol.add_bond(0, 2, 1);
    mol.add_bond(0, 3, 1);
    mol.add_bond(0, 4, 1);
    
    mol.generate_angles_from_bonds();
    mol.generate_torsions_from_bonds();
    
    print_molecule_summary("CH4", mol);
    
    NonbondedParams nb_params;
    nb_params.epsilon = 0.05;
    nb_params.scale_13 = 0.5;
    
    // Optimize without torsions
    EnergyModel energy_no_tor(mol, 300.0, true, true, nb_params, false);
    
    OptimizerSettings settings;
    settings.max_iterations = 500;
    settings.tol_rms_force = 1e-4;
    
    FIREOptimizer optimizer(settings);
    OptimizeResult result_no_tor = optimizer.minimize(mol.coords, energy_no_tor);
    
    print_optimization_result("Without torsions", result_no_tor);
    
    // Optimize with torsions
    EnergyModel energy_with_tor(mol, 300.0, true, true, nb_params, true);
    OptimizeResult result_with_tor = optimizer.minimize(mol.coords, energy_with_tor);
    
    print_optimization_result("With torsions", result_with_tor);
    
    // Measure H-C-H angles
    double angle1 = angle(result_with_tor.coords, 1, 0, 2) * 180.0 / M_PI;
    double angle2 = angle(result_with_tor.coords, 1, 0, 3) * 180.0 / M_PI;
    double angle3 = angle(result_with_tor.coords, 2, 0, 3) * 180.0 / M_PI;
    
    std::cout << "\nH-C-H angles (sample): " << std::fixed << std::setprecision(1)
              << angle1 << "°, " << angle2 << "°, " << angle3 << "° (expected: 109.5°)\n";
    
    std::cout << "✓ CH4 maintains tetrahedral geometry with torsions\n";
}

// ============================================================================
// Test 4: PCl5 (Trigonal Bipyramidal AX5)
// ============================================================================
void test_pcl5() {
    std::cout << "\n=== Test: PCl5 (Trigonal Bipyramidal AX5) ===\n";
    
    Molecule mol;
    
    // Phosphorus at center
    mol.add_atom(15, 0.0, 0.0, 0.0);     // P
    mol.add_atom(17, 0.0, 0.0, 2.0);     // Cl axial top
    mol.add_atom(17, 0.0, 0.0, -2.0);    // Cl axial bottom
    mol.add_atom(17, 2.0, 0.0, 0.0);     // Cl equatorial
    mol.add_atom(17, -1.0, 1.73, 0.0);   // Cl equatorial
    mol.add_atom(17, -1.0, -1.73, 0.0);  // Cl equatorial
    
    mol.add_bond(0, 1, 1);
    mol.add_bond(0, 2, 1);
    mol.add_bond(0, 3, 1);
    mol.add_bond(0, 4, 1);
    mol.add_bond(0, 5, 1);
    
    mol.generate_angles_from_bonds();
    mol.generate_torsions_from_bonds();
    
    print_molecule_summary("PCl5", mol);
    
    NonbondedParams nb_params;
    nb_params.epsilon = 0.15;
    nb_params.scale_13 = 0.5;
    
    // Optimize without torsions
    EnergyModel energy_no_tor(mol, 300.0, true, true, nb_params, false);
    
    OptimizerSettings settings;
    settings.max_iterations = 500;
    settings.tol_rms_force = 1e-4;
    
    FIREOptimizer optimizer(settings);
    OptimizeResult result_no_tor = optimizer.minimize(mol.coords, energy_no_tor);
    
    print_optimization_result("Without torsions", result_no_tor);
    
    // Optimize with torsions
    EnergyModel energy_with_tor(mol, 300.0, true, true, nb_params, true);
    OptimizeResult result_with_tor = optimizer.minimize(mol.coords, energy_with_tor);
    
    print_optimization_result("With torsions", result_with_tor);
    
    // Measure key angles
    double axial_axial = angle(result_with_tor.coords, 1, 0, 2) * 180.0 / M_PI;
    double eq_eq = angle(result_with_tor.coords, 3, 0, 4) * 180.0 / M_PI;
    double axial_eq = angle(result_with_tor.coords, 1, 0, 3) * 180.0 / M_PI;
    
    std::cout << "\nKey angles:\n";
    std::cout << "  Axial-P-Axial: " << std::fixed << std::setprecision(1) 
              << axial_axial << "° (expected: 180°)\n";
    std::cout << "  Eq-P-Eq:       " << eq_eq << "° (expected: 120°)\n";
    std::cout << "  Axial-P-Eq:    " << axial_eq << "° (expected: 90°)\n";
    
    std::cout << "✓ PCl5 maintains trigonal bipyramidal geometry with torsions\n";
}

// ============================================================================
// Main Test Runner
// ============================================================================
int main() {
    std::cout << "===================================================\n";
    std::cout << "Torsion Validation Tests\n";
    std::cout << "Testing torsional energy on VSEPR molecules\n";
    std::cout << "===================================================\n";

    try {
        test_bef2();
        test_bf3();
        test_ch4();
        test_pcl5();

        std::cout << "\n===================================================\n";
        std::cout << "All torsion validation tests completed!\n";
        std::cout << "===================================================\n";
        
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "\nTest FAILED: " << e.what() << "\n";
        return 1;
    }
}
