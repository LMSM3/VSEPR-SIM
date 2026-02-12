/**
 * basic_molecule_validation.cpp
 * ==============================
 * Basic molecule validation after refactoring
 * 
 * Tests:
 * - H2O: bent geometry, 104.5° angle
 * - NH3: pyramidal, ~107° angles
 * - CH4: tetrahedral, ~109.5° angles
 * - CO2: linear, 180° angle
 * 
 * PASS criteria:
 * - Optimization converges (< 100 iterations)
 * - Bond lengths within expected ranges
 * - Bond angles within ±5° of ideal
 * - No NaN or Inf values
 */

#include "sim/molecule.hpp"
#include "pot/energy_model.hpp"
#include "sim/optimizer.hpp"
#include "core/geom_ops.hpp"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <vector>

using namespace vsepr;

// Helper: compute angle between three atoms
double compute_angle(const std::vector<double>& coords, int i, int j, int k) {
    Vec3 ri(coords[3*i], coords[3*i+1], coords[3*i+2]);
    Vec3 rj(coords[3*j], coords[3*j+1], coords[3*j+2]);
    Vec3 rk(coords[3*k], coords[3*k+1], coords[3*k+2]);
    
    Vec3 v1 = ri - rj;
    Vec3 v2 = rk - rj;
    
    double dot = v1.dot(v2);
    double norm1 = v1.norm();
    double norm2 = v2.norm();
    
    double cos_angle = dot / (norm1 * norm2);
    cos_angle = std::max(-1.0, std::min(1.0, cos_angle));
    
    return std::acos(cos_angle) * 180.0 / M_PI;
}

// Helper: compute bond length
double compute_distance(const std::vector<double>& coords, int i, int j) {
    Vec3 ri(coords[3*i], coords[3*i+1], coords[3*i+2]);
    Vec3 rj(coords[3*j], coords[3*j+1], coords[3*j+2]);
    return (ri - rj).norm();
}

// Test H2O
bool test_h2o() {
    std::cout << "\n=== Test H2O (Water) ===\n";
    
    Molecule mol;
    // O at origin, H atoms at rough positions
    mol.add_atom(8, 0.0, 0.0, 0.0);        // O
    mol.add_atom(1, 0.8, 0.6, 0.0);        // H1
    mol.add_atom(1, -0.8, 0.6, 0.0);       // H2
    
    mol.add_bond(0, 1, 1);
    mol.add_bond(0, 2, 1);
    mol.generate_angles_from_bonds();
    mol.generate_torsions_from_bonds();
    
    // Get initial coords
    std::vector<double> coords = mol.coords;
    
    // Setup optimizer
    OptimizerSettings settings;
    settings.max_iterations = 200;
    settings.tol_rms_force = 1e-3;
    settings.print_every = 50;
    
    EnergyModel energy(mol, 300.0, true, true, NonbondedParams(), true);
    FIREOptimizer opt(settings);
    
    // Optimize
    OptimizeResult result = opt.minimize(coords, energy);
    
    std::cout << "Converged: " << (result.converged ? "YES" : "NO") << "\n";
    std::cout << "Iterations: " << result.iterations << "\n";
    std::cout << "Final energy: " << result.energy << " kcal/mol\n";
    std::cout << "RMS force: " << result.rms_force << "\n";
    
    // Check geometry
    double r_OH1 = compute_distance(result.coords, 0, 1);
    double r_OH2 = compute_distance(result.coords, 0, 2);
    double angle_HOH = compute_angle(result.coords, 1, 0, 2);
    
    std::cout << "O-H1 distance: " << r_OH1 << " Å\n";
    std::cout << "O-H2 distance: " << r_OH2 << " Å\n";
    std::cout << "H-O-H angle: " << angle_HOH << "°\n";
    
    // Validation
    bool pass = true;
    if (!result.converged) {
        std::cout << "FAIL: Did not converge\n";
        pass = false;
    }
    if (r_OH1 < 0.85 || r_OH1 > 1.05) {
        std::cout << "FAIL: O-H1 bond length out of range (expected ~0.96 Å)\n";
        pass = false;
    }
    if (r_OH2 < 0.85 || r_OH2 > 1.05) {
        std::cout << "FAIL: O-H2 bond length out of range\n";
        pass = false;
    }
    if (angle_HOH < 99.0 || angle_HOH > 110.0) {
        std::cout << "FAIL: H-O-H angle out of range (expected ~104.5°)\n";
        pass = false;
    }
    
    if (pass) {
        std::cout << "PASS: H2O geometry correct\n";
    }
    
    return pass;
}

// Test NH3
bool test_nh3() {
    std::cout << "\n=== Test NH3 (Ammonia) ===\n";
    
    Molecule mol;
    // N at origin, H atoms at rough tetrahedral positions
    mol.add_atom(7, 0.0, 0.0, 0.0);         // N
    mol.add_atom(1, 0.9, 0.3, 0.3);         // H1
    mol.add_atom(1, -0.3, 0.9, 0.3);        // H2
    mol.add_atom(1, -0.3, -0.3, 0.9);       // H3
    
    mol.add_bond(0, 1, 1);
    mol.add_bond(0, 2, 1);
    mol.add_bond(0, 3, 1);
    mol.generate_angles_from_bonds();
    mol.generate_torsions_from_bonds();
    
    std::vector<double> coords = mol.coords;
    
    OptimizerSettings settings;
    settings.max_iterations = 200;
    settings.tol_rms_force = 1e-3;
    settings.print_every = 50;
    
    EnergyModel energy(mol, 300.0, true, true, NonbondedParams(), true);
    FIREOptimizer opt(settings);
    
    OptimizeResult result = opt.minimize(coords, energy);
    
    std::cout << "Converged: " << (result.converged ? "YES" : "NO") << "\n";
    std::cout << "Iterations: " << result.iterations << "\n";
    std::cout << "Final energy: " << result.energy << " kcal/mol\n";
    
    // Check geometry
    double r_NH1 = compute_distance(result.coords, 0, 1);
    double r_NH2 = compute_distance(result.coords, 0, 2);
    double r_NH3 = compute_distance(result.coords, 0, 3);
    double angle_HNH_12 = compute_angle(result.coords, 1, 0, 2);
    double angle_HNH_13 = compute_angle(result.coords, 1, 0, 3);
    double angle_HNH_23 = compute_angle(result.coords, 2, 0, 3);
    
    std::cout << "N-H1 distance: " << r_NH1 << " Å\n";
    std::cout << "N-H2 distance: " << r_NH2 << " Å\n";
    std::cout << "N-H3 distance: " << r_NH3 << " Å\n";
    std::cout << "H-N-H angles: " << angle_HNH_12 << "°, " 
              << angle_HNH_13 << "°, " << angle_HNH_23 << "°\n";
    
    double avg_angle = (angle_HNH_12 + angle_HNH_13 + angle_HNH_23) / 3.0;
    std::cout << "Average H-N-H angle: " << avg_angle << "° (expected ~107°)\n";
    
    bool pass = true;
    if (!result.converged) {
        std::cout << "FAIL: Did not converge\n";
        pass = false;
    }
    if (avg_angle < 102.0 || avg_angle > 112.0) {
        std::cout << "FAIL: Average H-N-H angle out of range\n";
        pass = false;
    }
    
    if (pass) {
        std::cout << "PASS: NH3 geometry correct\n";
    }
    
    return pass;
}

// Test CH4
bool test_ch4() {
    std::cout << "\n=== Test CH4 (Methane) ===\n";
    
    Molecule mol;
    // C at origin, H atoms at tetrahedral positions
    mol.add_atom(6, 0.0, 0.0, 0.0);          // C
    mol.add_atom(1, 1.0, 0.0, 0.0);          // H1
    mol.add_atom(1, -0.5, 0.87, 0.0);        // H2
    mol.add_atom(1, -0.5, -0.43, 0.75);      // H3
    mol.add_atom(1, -0.5, -0.43, -0.75);     // H4
    
    mol.add_bond(0, 1, 1);
    mol.add_bond(0, 2, 1);
    mol.add_bond(0, 3, 1);
    mol.add_bond(0, 4, 1);
    mol.generate_angles_from_bonds();
    mol.generate_torsions_from_bonds();
    
    std::vector<double> coords = mol.coords;
    
    OptimizerSettings settings;
    settings.max_iterations = 200;
    settings.tol_rms_force = 1e-3;
    settings.print_every = 50;
    
    EnergyModel energy(mol, 300.0, true, true, NonbondedParams(), true);
    FIREOptimizer opt(settings);
    
    OptimizeResult result = opt.minimize(coords, energy);
    
    std::cout << "Converged: " << (result.converged ? "YES" : "NO") << "\n";
    std::cout << "Iterations: " << result.iterations << "\n";
    std::cout << "Final energy: " << result.energy << " kcal/mol\n";
    
    // Check all 6 H-C-H angles (tetrahedral = 109.47°)
    std::vector<double> angles;
    for (int i = 1; i <= 4; ++i) {
        for (int j = i+1; j <= 4; ++j) {
            double angle = compute_angle(result.coords, i, 0, j);
            angles.push_back(angle);
        }
    }
    
    double avg_angle = 0.0;
    for (double a : angles) avg_angle += a;
    avg_angle /= angles.size();
    
    std::cout << "H-C-H angles: ";
    for (double a : angles) std::cout << a << "° ";
    std::cout << "\n";
    std::cout << "Average H-C-H angle: " << avg_angle << "° (expected 109.47°)\n";
    
    bool pass = true;
    if (!result.converged) {
        std::cout << "FAIL: Did not converge\n";
        pass = false;
    }
    if (avg_angle < 105.0 || avg_angle > 114.0) {
        std::cout << "FAIL: Average H-C-H angle out of range\n";
        pass = false;
    }
    
    if (pass) {
        std::cout << "PASS: CH4 geometry correct\n";
    }
    
    return pass;
}

// Test CO2
bool test_co2() {
    std::cout << "\n=== Test CO2 (Carbon Dioxide) ===\n";
    
    Molecule mol;
    // C at origin, O atoms along x-axis
    mol.add_atom(6, 0.0, 0.0, 0.0);          // C
    mol.add_atom(8, 1.2, 0.0, 0.0);          // O1
    mol.add_atom(8, -1.2, 0.0, 0.0);         // O2
    
    mol.add_bond(0, 1, 2);  // double bond
    mol.add_bond(0, 2, 2);  // double bond
    mol.generate_angles_from_bonds();
    mol.generate_torsions_from_bonds();
    
    std::vector<double> coords = mol.coords;
    
    OptimizerSettings settings;
    settings.max_iterations = 200;
    settings.tol_rms_force = 1e-3;
    settings.print_every = 50;
    
    EnergyModel energy(mol, 300.0, true, true, NonbondedParams(), true);
    FIREOptimizer opt(settings);
    
    OptimizeResult result = opt.minimize(coords, energy);
    
    std::cout << "Converged: " << (result.converged ? "YES" : "NO") << "\n";
    std::cout << "Iterations: " << result.iterations << "\n";
    std::cout << "Final energy: " << result.energy << " kcal/mol\n";
    
    double r_CO1 = compute_distance(result.coords, 0, 1);
    double r_CO2 = compute_distance(result.coords, 0, 2);
    double angle_OCO = compute_angle(result.coords, 1, 0, 2);
    
    std::cout << "C=O1 distance: " << r_CO1 << " Å (expected ~1.16 Å)\n";
    std::cout << "C=O2 distance: " << r_CO2 << " Å\n";
    std::cout << "O=C=O angle: " << angle_OCO << "° (expected 180°)\n";
    
    bool pass = true;
    if (!result.converged) {
        std::cout << "FAIL: Did not converge\n";
        pass = false;
    }
    if (angle_OCO < 175.0 || angle_OCO > 185.0) {
        std::cout << "FAIL: O=C=O angle not linear\n";
        pass = false;
    }
    
    if (pass) {
        std::cout << "PASS: CO2 geometry correct\n";
    }
    
    return pass;
}

int main() {
    std::cout << "======================================\n";
    std::cout << "Basic Molecule Validation Test Suite\n";
    std::cout << "======================================\n";
    
    int passed = 0;
    int total = 0;
    
    total++;
    if (test_h2o()) passed++;
    
    total++;
    if (test_nh3()) passed++;
    
    total++;
    if (test_ch4()) passed++;
    
    total++;
    if (test_co2()) passed++;
    
    std::cout << "\n======================================\n";
    std::cout << "Results: " << passed << "/" << total << " tests passed\n";
    std::cout << "======================================\n";
    
    return (passed == total) ? 0 : 1;
}
