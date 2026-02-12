/*
alkane_torsion_tests.cpp
------------------------
Test torsional energy on real alkanes with conformational freedom.

Tests:
- Ethane (H3C-CH3): Should have 9 H-C-C-H torsions
- Butane (CH3-CH2-CH2-CH3): Should have C-C-C-C central torsion + H-C-C-H torsions

Goal: Verify torsions are generated and contribute to energy.
*/

#include "sim/molecule.hpp"
#include "pot/energy_model.hpp"
#include "sim/optimizer.hpp"
#include "core/geom_ops.hpp"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <cmath>

using namespace vsepr;

void print_molecule_summary(const std::string& name, const Molecule& mol) {
    std::cout << "\n" << name << ":\n";
    std::cout << "  Atoms: " << mol.num_atoms() << "\n";
    std::cout << "  Bonds: " << mol.bonds.size() << "\n";
    std::cout << "  Angles: " << mol.angles.size() << "\n";
    std::cout << "  Torsions: " << mol.torsions.size() << "\n";
    
    if (mol.torsions.size() > 0 && mol.torsions.size() <= 20) {
        std::cout << "  Torsion list:\n";
        for (size_t i = 0; i < mol.torsions.size(); ++i) {
            const auto& t = mol.torsions[i];
            std::cout << "    [" << i << "] " << t.i << "-" << t.j << "-" 
                      << t.k << "-" << t.l << "\n";
        }
    }
}

void print_optimization_result(const std::string& label, const OptimizeResult& result) {
    std::cout << "\n" << label << ":\n";
    std::cout << "  Iterations: " << result.iterations << "\n";
    std::cout << "  Converged: " << (result.converged ? "YES" : "NO") << " (" << result.termination_reason << ")\n";
    std::cout << "  Final RMS force: " << std::scientific << std::setprecision(3) << result.rms_force << "\n";
    std::cout << "  Final max force: " << result.max_force << "\n";
    std::cout << "  Final energy: " << std::fixed << std::setprecision(6) 
              << result.energy << " kcal/mol\n";
    std::cout << "  Energy breakdown:\n";
    std::cout << "    Bond:      " << std::setw(10) << result.energy_breakdown.bond_energy << "\n";
    std::cout << "    Angle:     " << std::setw(10) << result.energy_breakdown.angle_energy << "\n";
    std::cout << "    Nonbonded: " << std::setw(10) << result.energy_breakdown.nonbonded_energy << "\n";
    std::cout << "    Torsion:   " << std::setw(10) << result.energy_breakdown.torsion_energy << "\n";
}

// ============================================================================
// Test 1: Ethane (H3C-CH3)
// ============================================================================
void test_ethane() {
    std::cout << "\n=== Test: Ethane (H3C-CH3) ===\n";
    
    Molecule mol;
    
    // Two carbons
    mol.add_atom(6, 0.0, 0.0, 0.0);    // C1 (0)
    mol.add_atom(6, 1.54, 0.0, 0.0);   // C2 (1)
    
    // Hydrogens on C1 (staggered conformation)
    double r = 1.09; // C-H bond length
    mol.add_atom(1, -r*0.5, -r*0.866, 0.0);         // H (2)
    mol.add_atom(1, -r*0.5, r*0.433, r*0.75);       // H (3)
    mol.add_atom(1, -r*0.5, r*0.433, -r*0.75);      // H (4)
    
    // Hydrogens on C2 (staggered)
    mol.add_atom(1, 1.54+r*0.5, r*0.866, 0.0);      // H (5)
    mol.add_atom(1, 1.54+r*0.5, -r*0.433, r*0.75);  // H (6)
    mol.add_atom(1, 1.54+r*0.5, -r*0.433, -r*0.75); // H (7)
    
    // Bonds
    mol.add_bond(0, 1, 1);  // C-C
    mol.add_bond(0, 2, 1);  // C-H
    mol.add_bond(0, 3, 1);
    mol.add_bond(0, 4, 1);
    mol.add_bond(1, 5, 1);
    mol.add_bond(1, 6, 1);
    mol.add_bond(1, 7, 1);
    
    mol.generate_angles_from_bonds();
    mol.generate_torsions_from_bonds();
    
    print_molecule_summary("Ethane", mol);
    
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
    
    // Verify torsion energy is non-zero
    if (result_with_tor.energy_breakdown.torsion_energy > 0.1) {
        std::cout << "\n✓ Ethane has significant torsion energy (" 
                  << std::fixed << std::setprecision(2)
                  << result_with_tor.energy_breakdown.torsion_energy 
                  << " kcal/mol)\n";
    } else {
        std::cout << "\n⚠ Warning: Torsion energy is very small ("
                  << result_with_tor.energy_breakdown.torsion_energy 
                  << " kcal/mol)\n";
    }
    
    // Measure a representative H-C-C-H torsion angle
    if (mol.torsions.size() > 0) {
        const auto& t = mol.torsions[0];
        double phi = torsion(result_with_tor.coords, t.i, t.j, t.k, t.l);
        std::cout << "Sample H-C-C-H torsion angle: " << std::fixed << std::setprecision(1)
                  << phi * 180.0 / M_PI << "° (staggered ~60°, eclipsed ~0°)\n";
    }
}

// ============================================================================
// Test 2: Butane (CH3-CH2-CH2-CH3)
// ============================================================================
void test_butane() {
    std::cout << "\n=== Test: Butane (CH3-CH2-CH2-CH3) ===\n";
    
    Molecule mol;
    
    // Four carbons in anti conformation
    mol.add_atom(6, 0.0, 0.0, 0.0);      // C1 (0)
    mol.add_atom(6, 1.54, 0.0, 0.0);     // C2 (1)
    mol.add_atom(6, 2.31, 1.26, 0.0);    // C3 (2) - anti
    mol.add_atom(6, 3.85, 1.26, 0.0);    // C4 (3)
    
    // Hydrogens on C1
    mol.add_atom(1, -0.36, -0.51, 0.89);  // H (4)
    mol.add_atom(1, -0.36, -0.51, -0.89); // H (5)
    mol.add_atom(1, -0.36, 1.03, 0.0);    // H (6)
    
    // Hydrogens on C2
    mol.add_atom(1, 1.90, -0.51, 0.89);   // H (7)
    mol.add_atom(1, 1.90, -0.51, -0.89);  // H (8)
    
    // Hydrogens on C3
    mol.add_atom(1, 1.95, 1.77, 0.89);    // H (9)
    mol.add_atom(1, 1.95, 1.77, -0.89);   // H (10)
    
    // Hydrogens on C4
    mol.add_atom(1, 4.21, 0.74, 0.89);    // H (11)
    mol.add_atom(1, 4.21, 0.74, -0.89);   // H (12)
    mol.add_atom(1, 4.21, 2.28, 0.0);     // H (13)
    
    // C-C bonds
    mol.add_bond(0, 1, 1);
    mol.add_bond(1, 2, 1);
    mol.add_bond(2, 3, 1);
    
    // C-H bonds on C1
    mol.add_bond(0, 4, 1);
    mol.add_bond(0, 5, 1);
    mol.add_bond(0, 6, 1);
    
    // C-H bonds on C2
    mol.add_bond(1, 7, 1);
    mol.add_bond(1, 8, 1);
    
    // C-H bonds on C3
    mol.add_bond(2, 9, 1);
    mol.add_bond(2, 10, 1);
    
    // C-H bonds on C4
    mol.add_bond(3, 11, 1);
    mol.add_bond(3, 12, 1);
    mol.add_bond(3, 13, 1);
    
    mol.generate_angles_from_bonds();
    mol.generate_torsions_from_bonds();
    
    print_molecule_summary("Butane", mol);
    
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
    
    // Find the central C-C-C-C torsion
    int central_torsion_idx = -1;
    for (size_t i = 0; i < mol.torsions.size(); ++i) {
        const auto& t = mol.torsions[i];
        // Looking for C1-C2-C3-C4 torsion (atoms 0-1-2-3)
        if (t.i == 0 && t.j == 1 && t.k == 2 && t.l == 3) {
            central_torsion_idx = i;
            break;
        }
    }
    
    if (central_torsion_idx >= 0) {
        const auto& t = mol.torsions[central_torsion_idx];
        double phi = torsion(result_with_tor.coords, t.i, t.j, t.k, t.l);
        std::cout << "\nCentral C-C-C-C torsion angle: " << std::fixed << std::setprecision(1)
                  << phi * 180.0 / M_PI << "° (anti ~180°, gauche ~±60°)\n";
    } else {
        std::cout << "\n⚠ Warning: Central C-C-C-C torsion not found\n";
    }
    
    // Verify torsion energy is significant
    if (result_with_tor.energy_breakdown.torsion_energy > 0.5) {
        std::cout << "✓ Butane has significant torsion energy (" 
                  << std::fixed << std::setprecision(2)
                  << result_with_tor.energy_breakdown.torsion_energy 
                  << " kcal/mol)\n";
    } else {
        std::cout << "⚠ Warning: Torsion energy is small ("
                  << result_with_tor.energy_breakdown.torsion_energy 
                  << " kcal/mol)\n";
    }
}

// ============================================================================
// Main Test Runner
// ============================================================================
int main() {
    std::cout << "===================================================\n";
    std::cout << "Alkane Torsion Tests\n";
    std::cout << "Testing torsional energy on real molecules\n";
    std::cout << "===================================================\n";

    try {
        test_ethane();
        test_butane();

        std::cout << "\n===================================================\n";
        std::cout << "All alkane torsion tests completed!\n";
        std::cout << "===================================================\n";
        
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "\nTest FAILED: " << e.what() << "\n";
        return 1;
    }
}
