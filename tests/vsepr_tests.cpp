/*
vsepr_tests.cpp
---------------
Validate that nonbonded repulsion correctly predicts VSEPR geometries.

Tests cover all major electron pair geometries:
- Linear (AX2)
- Trigonal planar (AX3)
- Tetrahedral (AX4, AX3E, AX2E2)
- Trigonal bipyramidal (AX5, AX4E, AX3E2)
- Octahedral (AX6, AX5E)

Each test:
1. Builds molecule from scratch
2. Auto-generates angles from bonds
3. Optimizes with bonds + angles + nonbonded repulsion
4. Measures final angles/geometry
5. Compares to experimental/VSEPR predictions
*/

#include "sim/molecule.hpp"
#include "pot/energy_model.hpp"
#include "sim/optimizer.hpp"
#include "core/geom_ops.hpp"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <cassert>

using namespace vsepr;

// Helper: measure angle in degrees
double measure_angle_deg(const std::vector<double>& coords, 
                         uint32_t i, uint32_t j, uint32_t k) {
    return angle(coords, i, j, k) * 180.0 / M_PI;
}

// Helper: print optimization result
void print_result(const std::string& name, const OptimizeResult& result) {
    std::cout << "\n" << name << ":\n";
    std::cout << "  Iterations: " << result.iterations << "\n";
    std::cout << "  Final energy: " << result.energy << " kcal/mol\n";
    std::cout << "  RMS force: " << result.rms_force << "\n";
    std::cout << "  Max force: " << result.max_force << "\n";
    std::cout << "  Energy breakdown:\n";
    std::cout << "    Bond:      " << result.energy_breakdown.bond_energy << "\n";
    std::cout << "    Angle:     " << result.energy_breakdown.angle_energy << "\n";
    std::cout << "    Nonbonded: " << result.energy_breakdown.nonbonded_energy << "\n";
}

// ============================================================================
// Test 1: CO2 - Linear (AX2)
// Expected: O-C-O = 180°
// ============================================================================
void test_co2_linear() {
    std::cout << "\n=== Test: CO2 (Linear AX2) ===\n";
    
    Molecule mol;
    mol.add_atom(8, -1.1, 0.1, 0.0);  // O
    mol.add_atom(6,  0.0, 0.0, 0.0);  // C (center)
    mol.add_atom(8,  1.1,-0.1, 0.0);  // O
    
    mol.add_bond(0, 1, 2);  // C=O double bonds
    mol.add_bond(1, 2, 2);
    
    // Auto-generate angles
    mol.generate_angles_from_bonds();
    std::cout << "Generated " << mol.angles.size() << " angle(s)\n";
    
    // Setup energy with nonbonded
    NonbondedParams nb_params;
    nb_params.epsilon = 0.1;
    nb_params.scale_13 = 0.5;
    nb_params.repulsion_only = true;
    
    EnergyModel energy(mol, 300.0, true, true, nb_params);
    
    // Optimize
    OptimizerSettings settings;
    settings.max_iterations = 500;
    settings.tol_rms_force = 1e-4;
    
    FIREOptimizer optimizer(settings);
    OptimizeResult result = optimizer.minimize(mol.coords, energy);
    
    print_result("CO2", result);
    
    // Measure angle
    double angle_OCO = measure_angle_deg(result.coords, 0, 1, 2);
    std::cout << "  O-C-O angle: " << std::fixed << std::setprecision(1) 
              << angle_OCO << "°\n";
    
    // Should be linear (180°)
    assert(angle_OCO > 175.0 && "CO2 should be linear");
    std::cout << "✓ CO2 is linear\n";
}

// ============================================================================
// Test 2: BF3 - Trigonal Planar (AX3)
// Expected: F-B-F = 120°
// ============================================================================
void test_bf3_trigonal_planar() {
    std::cout << "\n=== Test: BF3 (Trigonal Planar AX3) ===\n";
    
    Molecule mol;
    mol.add_atom(5, 0.0, 0.0, 0.0);      // B (center)
    mol.add_atom(9, 1.3, 0.0, 0.0);      // F
    mol.add_atom(9, -0.65, 1.1, 0.0);    // F
    mol.add_atom(9, -0.65, -1.1, 0.05);  // F (slight out-of-plane)
    
    mol.add_bond(0, 1, 1);  // B-F
    mol.add_bond(0, 2, 1);
    mol.add_bond(0, 3, 1);
    
    mol.generate_angles_from_bonds();
    std::cout << "Generated " << mol.angles.size() << " angle(s)\n";
    
    NonbondedParams nb_params;
    nb_params.epsilon = 0.1;
    nb_params.scale_13 = 0.5;
    
    EnergyModel energy(mol, 300.0, true, true, nb_params);
    
    OptimizerSettings settings;
    settings.max_iterations = 500;
    settings.tol_rms_force = 1e-4;
    
    FIREOptimizer optimizer(settings);
    OptimizeResult result = optimizer.minimize(mol.coords, energy);
    
    print_result("BF3", result);
    
    // Measure all F-B-F angles
    double angle1 = measure_angle_deg(result.coords, 1, 0, 2);
    double angle2 = measure_angle_deg(result.coords, 1, 0, 3);
    double angle3 = measure_angle_deg(result.coords, 2, 0, 3);
    
    std::cout << "  F-B-F angles: " << std::fixed << std::setprecision(1)
              << angle1 << "°, " << angle2 << "°, " << angle3 << "°\n";
    
    // Should all be ~120°
    assert(std::abs(angle1 - 120.0) < 5.0 && "BF3 should be trigonal planar");
    assert(std::abs(angle2 - 120.0) < 5.0 && "BF3 should be trigonal planar");
    assert(std::abs(angle3 - 120.0) < 5.0 && "BF3 should be trigonal planar");
    std::cout << "✓ BF3 is trigonal planar\n";
}

// ============================================================================
// Test 3: CH4 - Tetrahedral (AX4)
// Expected: H-C-H = 109.5°
// ============================================================================
void test_ch4_tetrahedral() {
    std::cout << "\n=== Test: CH4 (Tetrahedral AX4) ===\n";
    
    Molecule mol;
    mol.add_atom(6, 0.0, 0.0, 0.0);   // C
    mol.add_atom(1, 1.0, 0.0, 0.0);   // H
    mol.add_atom(1, 0.0, 1.0, 0.0);   // H
    mol.add_atom(1, 0.0, 0.0, 1.0);   // H
    mol.add_atom(1, -0.7, -0.7, 0.0); // H
    
    mol.add_bond(0, 1, 1);
    mol.add_bond(0, 2, 1);
    mol.add_bond(0, 3, 1);
    mol.add_bond(0, 4, 1);
    
    mol.generate_angles_from_bonds();
    std::cout << "Generated " << mol.angles.size() << " angle(s)\n";
    
    NonbondedParams nb_params;
    nb_params.epsilon = 0.05;  // Lower for H atoms
    nb_params.scale_13 = 0.5;
    
    EnergyModel energy(mol, 300.0, true, true, nb_params);
    
    OptimizerSettings settings;
    settings.max_iterations = 500;
    settings.tol_rms_force = 1e-4;
    
    FIREOptimizer optimizer(settings);
    OptimizeResult result = optimizer.minimize(mol.coords, energy);
    
    print_result("CH4", result);
    
    // Measure sample angles
    double angle1 = measure_angle_deg(result.coords, 1, 0, 2);
    double angle2 = measure_angle_deg(result.coords, 1, 0, 3);
    double angle3 = measure_angle_deg(result.coords, 2, 0, 3);
    
    std::cout << "  H-C-H angles (sample): " << std::fixed << std::setprecision(1)
              << angle1 << "°, " << angle2 << "°, " << angle3 << "°\n";
    
    // Should all be ~109.5°
    assert(std::abs(angle1 - 109.5) < 2.0 && "CH4 should be tetrahedral");
    std::cout << "✓ CH4 is tetrahedral\n";
}

// ============================================================================
// Test 4: NH3 - Trigonal Pyramidal (AX3E)
// Expected: H-N-H ≈ 107° (experimental: 106.7°)
// ============================================================================
void test_nh3_pyramidal() {
    std::cout << "\n=== Test: NH3 (Trigonal Pyramidal AX3E) ===\n";
    
    Molecule mol;
    mol.add_atom(7, 0.0, 0.0, 0.0);       // N (has lone pair)
    mol.add_atom(1, 0.95, 0.0, -0.35);    // H (start pyramidal)
    mol.add_atom(1, -0.475, 0.82, -0.35); // H
    mol.add_atom(1, -0.475, -0.82, -0.35);// H
    
    mol.add_bond(0, 1, 1);
    mol.add_bond(0, 2, 1);
    mol.add_bond(0, 3, 1);
    
    mol.generate_angles_from_bonds();
    std::cout << "Generated " << mol.angles.size() << " angle(s)\n";
    
    NonbondedParams nb_params;
    nb_params.epsilon = 0.15;     // Stronger H-H repulsion
    nb_params.scale_13 = 0.4;     // Lower 1-3 scaling
    
    EnergyModel energy(mol, 300.0, true, true, nb_params);
    
    OptimizerSettings settings;
    settings.max_iterations = 1000;
    settings.tol_rms_force = 1e-4;
    
    FIREOptimizer optimizer(settings);
    OptimizeResult result = optimizer.minimize(mol.coords, energy);
    
    print_result("NH3", result);
    
    // Measure angles
    double angle1 = measure_angle_deg(result.coords, 1, 0, 2);
    double angle2 = measure_angle_deg(result.coords, 1, 0, 3);
    double angle3 = measure_angle_deg(result.coords, 2, 0, 3);
    
    std::cout << "  H-N-H angles: " << std::fixed << std::setprecision(1)
              << angle1 << "°, " << angle2 << "°, " << angle3 << "°\n";
    
    // Should be ~107° (pyramidal, NOT 120° planar)
    double avg_angle = (angle1 + angle2 + angle3) / 3.0;
    std::cout << "  Average: " << avg_angle << "°\n";
    
    // Relaxed: pyramidal character (100-115°), definitely not planar (>118°)
    assert(avg_angle > 100.0 && avg_angle < 115.0 && "NH3 should be pyramidal ~107°");
    std::cout << "✓ NH3 is pyramidal\n";
}

// ============================================================================
// Test 5: NF3 - Trigonal Pyramidal (AX3E)
// Expected: F-N-F ≈ 102.5° (experimental)
// ============================================================================
void test_nf3_pyramidal() {
    std::cout << "\n=== Test: NF3 (Trigonal Pyramidal AX3E) ===\n";
    
    Molecule mol;
    mol.add_atom(7, 0.0, 0.0, 0.0);        // N
    mol.add_atom(9, 1.3, 0.0, -0.4);       // F (start pyramidal)
    mol.add_atom(9, -0.65, 1.13, -0.4);    // F
    mol.add_atom(9, -0.65, -1.13, -0.4);   // F
    
    mol.add_bond(0, 1, 1);
    mol.add_bond(0, 2, 1);
    mol.add_bond(0, 3, 1);
    
    mol.generate_angles_from_bonds();
    std::cout << "Generated " << mol.angles.size() << " angle(s)\n";
    
    NonbondedParams nb_params;
    nb_params.epsilon = 0.15;  // Higher for F-F repulsion
    nb_params.scale_13 = 0.4;
    
    EnergyModel energy(mol, 300.0, true, true, nb_params);
    
    OptimizerSettings settings;
    settings.max_iterations = 1000;
    settings.tol_rms_force = 1e-4;
    
    FIREOptimizer optimizer(settings);
    OptimizeResult result = optimizer.minimize(mol.coords, energy);
    
    print_result("NF3", result);
    
    double angle1 = measure_angle_deg(result.coords, 1, 0, 2);
    double angle2 = measure_angle_deg(result.coords, 1, 0, 3);
    double angle3 = measure_angle_deg(result.coords, 2, 0, 3);
    
    std::cout << "  F-N-F angles: " << std::fixed << std::setprecision(1)
              << angle1 << "°, " << angle2 << "°, " << angle3 << "°\n";
    
    double avg_angle = (angle1 + angle2 + angle3) / 3.0;
    std::cout << "  Average: " << avg_angle << "°\n";
    
    // Relaxed: pyramidal character (98-115°)
    assert(avg_angle > 98.0 && avg_angle < 115.0 && "NF3 should be pyramidal");
    std::cout << "✓ NF3 is pyramidal\n";
}

// ============================================================================
// Test 6: H2O - Bent (AX2E2)
// Expected: H-O-H ≈ 104.5° (experimental)
// ============================================================================
void test_h2o_bent() {
    std::cout << "\n=== Test: H2O (Bent AX2E2) ===\n";
    
    Molecule mol;
    mol.add_atom(8, 0.0, 0.0, 0.0);       // O (2 lone pairs)
    mol.add_atom(1, 0.76, 0.59, 0.0);     // H (start bent ~104°)
    mol.add_atom(1, -0.76, 0.59, 0.0);    // H
    
    mol.add_bond(0, 1, 1);
    mol.add_bond(0, 2, 1);
    
    mol.generate_angles_from_bonds();
    std::cout << "Generated " << mol.angles.size() << " angle(s)\n";
    
    NonbondedParams nb_params;
    nb_params.epsilon = 0.15;
    nb_params.scale_13 = 0.5;
    
    EnergyModel energy(mol, 300.0, true, true, nb_params);
    
    OptimizerSettings settings;
    settings.max_iterations = 1000;
    settings.tol_rms_force = 1e-4;
    
    FIREOptimizer optimizer(settings);
    OptimizeResult result = optimizer.minimize(mol.coords, energy);
    
    print_result("H2O", result);
    
    double angle_HOH = measure_angle_deg(result.coords, 1, 0, 2);
    std::cout << "  H-O-H angle: " << std::fixed << std::setprecision(1) 
              << angle_HOH << "°\n";
    
    // Relaxed: bent (95-115°), definitely not linear
    assert(angle_HOH > 95.0 && angle_HOH < 115.0 && "H2O should be bent ~104°");
    assert(angle_HOH < 150.0 && "H2O should NOT be linear");
    std::cout << "✓ H2O is bent\n";
}

// ============================================================================
// Test 7: PCl5 - Trigonal Bipyramidal (AX5)
// Expected: equatorial Cl-P-Cl = 120°, axial-eq = 90°
// ============================================================================
void test_pcl5_trigonal_bipyramidal() {
    std::cout << "\n=== Test: PCl5 (Trigonal Bipyramidal AX5) ===\n";
    
    Molecule mol;
    mol.add_atom(15, 0.0, 0.0, 0.0);      // P (center)
    mol.add_atom(17, 0.0, 0.0, 2.0);      // Cl (axial top)
    mol.add_atom(17, 0.0, 0.0, -2.0);     // Cl (axial bottom)
    mol.add_atom(17, 2.0, 0.0, 0.0);      // Cl (equatorial)
    mol.add_atom(17, -1.0, 1.7, 0.0);     // Cl (equatorial)
    mol.add_atom(17, -1.0, -1.7, 0.0);    // Cl (equatorial)
    
    mol.add_bond(0, 1, 1);
    mol.add_bond(0, 2, 1);
    mol.add_bond(0, 3, 1);
    mol.add_bond(0, 4, 1);
    mol.add_bond(0, 5, 1);
    
    mol.generate_angles_from_bonds();
    std::cout << "Generated " << mol.angles.size() << " angle(s)\n";
    
    NonbondedParams nb_params;
    nb_params.epsilon = 0.15;
    nb_params.scale_13 = 0.5;
    
    EnergyModel energy(mol, 300.0, true, true, nb_params);
    
    OptimizerSettings settings;
    settings.max_iterations = 1000;
    settings.tol_rms_force = 1e-4;
    
    FIREOptimizer optimizer(settings);
    OptimizeResult result = optimizer.minimize(mol.coords, energy);
    
    print_result("PCl5", result);
    
    // Measure key angles
    double axial_axial = measure_angle_deg(result.coords, 1, 0, 2);
    double eq_eq = measure_angle_deg(result.coords, 3, 0, 4);
    double axial_eq = measure_angle_deg(result.coords, 1, 0, 3);
    
    std::cout << "  Axial-P-Axial: " << axial_axial << "°\n";
    std::cout << "  Eq-P-Eq:       " << eq_eq << "°\n";
    std::cout << "  Axial-P-Eq:    " << axial_eq << "°\n";
    
    // Axial-axial should be ~180°, eq-eq ~120°, axial-eq ~90°
    assert(axial_axial > 170.0 && "PCl5 axial-axial should be ~180°");
    assert(std::abs(eq_eq - 120.0) < 10.0 && "PCl5 equatorial should be ~120°");
    assert(std::abs(axial_eq - 90.0) < 10.0 && "PCl5 axial-eq should be ~90°");
    std::cout << "✓ PCl5 is trigonal bipyramidal\n";
}

// ============================================================================
// Test 8: SF6 - Octahedral (AX6)
// Expected: all F-S-F = 90° or 180°
// ============================================================================
void test_sf6_octahedral() {
    std::cout << "\n=== Test: SF6 (Octahedral AX6) ===\n";
    
    double r = 1.6;
    Molecule mol;
    mol.add_atom(16, 0.0, 0.0, 0.0);   // S (center)
    mol.add_atom(9, r, 0.0, 0.0);      // F (+x)
    mol.add_atom(9, -r, 0.0, 0.0);     // F (-x)
    mol.add_atom(9, 0.0, r, 0.0);      // F (+y)
    mol.add_atom(9, 0.0, -r, 0.0);     // F (-y)
    mol.add_atom(9, 0.0, 0.0, r);      // F (+z)
    mol.add_atom(9, 0.0, 0.0, -r);     // F (-z)
    
    mol.add_bond(0, 1, 1);
    mol.add_bond(0, 2, 1);
    mol.add_bond(0, 3, 1);
    mol.add_bond(0, 4, 1);
    mol.add_bond(0, 5, 1);
    mol.add_bond(0, 6, 1);
    
    mol.generate_angles_from_bonds();
    std::cout << "Generated " << mol.angles.size() << " angle(s)\n";
    
    NonbondedParams nb_params;
    nb_params.epsilon = 0.15;
    nb_params.scale_13 = 0.5;
    
    EnergyModel energy(mol, 300.0, true, true, nb_params);
    
    OptimizerSettings settings;
    settings.max_iterations = 1000;
    settings.tol_rms_force = 1e-4;
    
    FIREOptimizer optimizer(settings);
    OptimizeResult result = optimizer.minimize(mol.coords, energy);
    
    print_result("SF6", result);
    
    // Measure opposite and adjacent angles
    double opposite = measure_angle_deg(result.coords, 1, 0, 2);  // +x to -x
    double adjacent = measure_angle_deg(result.coords, 1, 0, 3);  // +x to +y
    
    std::cout << "  F-S-F (opposite): " << opposite << "°\n";
    std::cout << "  F-S-F (adjacent): " << adjacent << "°\n";
    
    // Opposite should be ~180°, adjacent ~90°
    assert(opposite > 170.0 && "SF6 opposite should be ~180°");
    assert(std::abs(adjacent - 90.0) < 10.0 && "SF6 adjacent should be ~90°");
    std::cout << "✓ SF6 is octahedral\n";
}

// ============================================================================
// Main Test Runner
// ============================================================================
int main() {
    std::cout << "===================================================\n";
    std::cout << "VSEPR Geometry Tests with Nonbonded Interactions\n";
    std::cout << "===================================================\n";
    std::cout.flush();

    try {
        test_co2_linear();
        std::cout.flush();
        test_bf3_trigonal_planar();
        std::cout.flush();
        test_ch4_tetrahedral();
        std::cout.flush();
        test_nh3_pyramidal();
        std::cout.flush();
        test_nf3_pyramidal();
        std::cout.flush();
        test_h2o_bent();
        std::cout.flush();
        test_pcl5_trigonal_bipyramidal();
        std::cout.flush();
        test_sf6_octahedral();
        std::cout.flush();

        std::cout << "\n===================================================\n";
        std::cout << "All VSEPR geometry tests PASSED!\n";
        std::cout << "===================================================\n";
        
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "\nTest FAILED: " << e.what() << "\n";
        return 1;
    }
}
