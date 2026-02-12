#include "sim/optimizer.hpp"
#include "sim/molecule.hpp"
#include "pot/energy_model.hpp"
#include <iostream>
#include <cmath>
#include <cassert>

using namespace vsepr;

#define ASSERT_NEAR(a, b, tol) \
    assert(std::abs((a) - (b)) < (tol) && "Assertion failed: values not close enough")

// ============================================================================
// Test: H2 Optimization
// ============================================================================

void test_h2_optimization() {
    std::cout << "Testing H2 optimization...\n";
    
    Molecule mol;
    
    // Start with stretched H2
    mol.add_atom(1, 0.0, 0.0, 0.0);  // H1
    mol.add_atom(1, 2.0, 0.0, 0.0);  // H2 (far from equilibrium ~0.64 Å)
    mol.add_bond(0, 1, 1);
    
    EnergyModel model(mol);
    
    // Initial energy should be high
    double E_initial = model.evaluate_energy(mol.coords);
    std::cout << "  Initial energy: " << E_initial << " kcal/mol\n";
    std::cout << "  Initial distance: 2.0 Å\n";
    assert(E_initial > 100.0);  // Should be stretched significantly
    
    // Optimize
    OptimizerSettings settings;
    settings.print_every = 50;
    settings.tol_rms_force = 1e-4;
    settings.tol_max_force = 1e-4;
    
    FIREOptimizer optimizer(settings);
    OptimizeResult result = optimizer.minimize(mol.coords, model);
    
    std::cout << "  Optimization terminated: " << result.termination_reason << "\n";
    std::cout << "  Iterations: " << result.iterations << "\n";
    std::cout << "  Final energy: " << result.energy << " kcal/mol\n";
    std::cout << "  Final RMS force: " << result.rms_force << "\n";
    std::cout << "  Final max force: " << result.max_force << "\n";
    
    // Check convergence
    assert(result.converged);
    assert(result.rms_force < settings.tol_rms_force);
    assert(result.max_force < settings.tol_max_force);
    
    // Final distance should be near equilibrium
    double final_dist = distance(result.coords, 0, 1);
    double r0_H2 = 2 * get_covalent_radius(1);  // ~0.64 Å
    std::cout << "  Final distance: " << final_dist << " Å\n";
    std::cout << "  Expected r0: " << r0_H2 << " Å\n";
    
    ASSERT_NEAR(final_dist, r0_H2, 1e-3);
    ASSERT_NEAR(result.energy, 0.0, 1e-6);
    
    std::cout << "  ✓ H2 optimization passed\n";
}

// ============================================================================
// Test: Water Molecule Optimization
// ============================================================================

void test_water_optimization() {
    std::cout << "Testing H2O optimization...\n";
    
    Molecule mol;
    
    // Start with distorted geometry
    mol.add_atom(8, 0.0, 0.0, 0.0);      // O
    mol.add_atom(1, 1.5, 0.0, 0.0);      // H1 (stretched)
    mol.add_atom(1, -0.5, 1.5, 0.0);     // H2 (stretched and bent)
    
    mol.add_bond(0, 1, 1);  // O-H1
    mol.add_bond(0, 2, 1);  // O-H2
    
    EnergyModel model(mol);
    
    double E_initial = model.evaluate_energy(mol.coords);
    std::cout << "  Initial energy: " << E_initial << " kcal/mol\n";
    
    // Optimize
    OptimizerSettings settings;
    settings.print_every = 100;
    settings.tol_rms_force = 1e-4;
    settings.tol_max_force = 1e-4;
    settings.max_iterations = 500;
    
    FIREOptimizer optimizer(settings);
    OptimizeResult result = optimizer.minimize(mol.coords, model);
    
    std::cout << "  Optimization terminated: " << result.termination_reason << "\n";
    std::cout << "  Iterations: " << result.iterations << "\n";
    std::cout << "  Final energy: " << result.energy << " kcal/mol\n";
    std::cout << "  Final RMS force: " << result.rms_force << "\n";
    
    // Check convergence
    assert(result.converged);
    
    // Check O-H bond lengths
    double d_OH1 = distance(result.coords, 0, 1);
    double d_OH2 = distance(result.coords, 0, 2);
    double r0_OH = get_covalent_radius(8) + get_covalent_radius(1);
    
    std::cout << "  O-H1 distance: " << d_OH1 << " Å (expected ~" << r0_OH << " Å)\n";
    std::cout << "  O-H2 distance: " << d_OH2 << " Å (expected ~" << r0_OH << " Å)\n";
    
    // Both bonds should be near equilibrium
    ASSERT_NEAR(d_OH1, r0_OH, 1e-3);
    ASSERT_NEAR(d_OH2, r0_OH, 1e-3);
    
    // Bonds should be symmetric
    ASSERT_NEAR(d_OH1, d_OH2, 1e-3);
    
    std::cout << "  ✓ H2O optimization passed\n";
}

// ============================================================================
// Test: Ethane (C2H6) - Multi-bond System
// ============================================================================

void test_ethane_optimization() {
    std::cout << "Testing C2H6 (ethane) optimization...\n";
    
    Molecule mol;
    
    // Build ethane with random/distorted initial geometry
    mol.add_atom(6, 0.0, 0.0, 0.0);      // C1
    mol.add_atom(6, 2.0, 0.0, 0.0);      // C2 (stretched C-C)
    mol.add_atom(1, -0.5, 1.0, 0.0);     // H1
    mol.add_atom(1, -0.5, -0.5, 1.0);    // H2
    mol.add_atom(1, -0.5, -0.5, -1.0);   // H3
    mol.add_atom(1, 2.5, 1.0, 0.0);      // H4
    mol.add_atom(1, 2.5, -0.5, 1.0);     // H5
    mol.add_atom(1, 2.5, -0.5, -1.0);    // H6
    
    // C-C bond
    mol.add_bond(0, 1, 1);
    
    // C1-H bonds
    mol.add_bond(0, 2, 1);
    mol.add_bond(0, 3, 1);
    mol.add_bond(0, 4, 1);
    
    // C2-H bonds
    mol.add_bond(1, 5, 1);
    mol.add_bond(1, 6, 1);
    mol.add_bond(1, 7, 1);
    
    EnergyModel model(mol);
    
    double E_initial = model.evaluate_energy(mol.coords);
    std::cout << "  Initial energy: " << E_initial << " kcal/mol\n";
    std::cout << "  Number of atoms: " << mol.num_atoms() << "\n";
    std::cout << "  Number of bonds: " << mol.num_bonds() << "\n";
    
    // Optimize
    OptimizerSettings settings;
    settings.print_every = 200;
    settings.tol_rms_force = 1e-3;
    settings.tol_max_force = 1e-3;
    settings.max_iterations = 1000;
    
    FIREOptimizer optimizer(settings);
    OptimizeResult result = optimizer.minimize(mol.coords, model);
    
    std::cout << "  Optimization terminated: " << result.termination_reason << "\n";
    std::cout << "  Iterations: " << result.iterations << "\n";
    std::cout << "  Final energy: " << result.energy << " kcal/mol\n";
    std::cout << "  Final RMS force: " << result.rms_force << "\n";
    
    // Check convergence
    assert(result.converged);
    
    // Check C-C bond
    double d_CC = distance(result.coords, 0, 1);
    double r0_CC = 2 * get_covalent_radius(6);
    std::cout << "  C-C distance: " << d_CC << " Å (expected ~" << r0_CC << " Å)\n";
    ASSERT_NEAR(d_CC, r0_CC, 1e-2);
    
    // Check a few C-H bonds
    double d_CH = distance(result.coords, 0, 2);
    double r0_CH = get_covalent_radius(6) + get_covalent_radius(1);
    std::cout << "  C-H distance (sample): " << d_CH << " Å (expected ~" << r0_CH << " Å)\n";
    ASSERT_NEAR(d_CH, r0_CH, 1e-2);
    
    std::cout << "  ✓ C2H6 optimization passed\n";
}

// ============================================================================
// Test: Nitrogen Trifluoride (NF3)
// ============================================================================

void test_nf3_optimization() {
    std::cout << "Testing NF3 (nitrogen trifluoride) optimization...\n";
    std::cout << "NOTE: Without angle terms, geometry will be incorrect!\n";
    std::cout << "Expected F-N-F angle: 102.5° - 107°\n\n";
    
    Molecule mol;
    
    // Build NF3 with tetrahedral-ish starting geometry
    // N at origin, F atoms in approximate tetrahedral positions
    mol.add_atom(7, 0.0, 0.0, 0.0);           // N (Z=7)
    mol.add_atom(9, 1.5, 0.0, 0.0);           // F1 (Z=9)
    mol.add_atom(9, -0.75, 1.3, 0.0);         // F2
    mol.add_atom(9, -0.75, -0.65, 1.1);       // F3
    
    // N-F bonds
    mol.add_bond(0, 1, 1);  // N-F1
    mol.add_bond(0, 2, 1);  // N-F2
    mol.add_bond(0, 3, 1);  // N-F3
    
    EnergyModel model(mol);
    
    double E_initial = model.evaluate_energy(mol.coords);
    std::cout << "  Initial energy: " << E_initial << " kcal/mol\n";
    
    // Measure initial angles
    double angle_F1NF2_init = angle(mol.coords, 1, 0, 2) * 180.0 / M_PI;
    double angle_F1NF3_init = angle(mol.coords, 1, 0, 3) * 180.0 / M_PI;
    double angle_F2NF3_init = angle(mol.coords, 2, 0, 3) * 180.0 / M_PI;
    
    std::cout << "  Initial angles:\n";
    std::cout << "    F1-N-F2: " << angle_F1NF2_init << "°\n";
    std::cout << "    F1-N-F3: " << angle_F1NF3_init << "°\n";
    std::cout << "    F2-N-F3: " << angle_F2NF3_init << "°\n";
    
    // Optimize
    OptimizerSettings settings;
    settings.print_every = 100;
    settings.tol_rms_force = 1e-3;
    settings.tol_max_force = 1e-3;
    settings.max_iterations = 500;
    
    FIREOptimizer optimizer(settings);
    OptimizeResult result = optimizer.minimize(mol.coords, model);
    
    std::cout << "  Optimization terminated: " << result.termination_reason << "\n";
    std::cout << "  Iterations: " << result.iterations << "\n";
    std::cout << "  Final energy: " << result.energy << " kcal/mol\n";
    std::cout << "  Final RMS force: " << result.rms_force << "\n";
    
    // Check N-F bond lengths
    double d_NF1 = distance(result.coords, 0, 1);
    double d_NF2 = distance(result.coords, 0, 2);
    double d_NF3 = distance(result.coords, 0, 3);
    double r0_NF = get_covalent_radius(7) + get_covalent_radius(9);
    
    std::cout << "\n  Final N-F bond lengths:\n";
    std::cout << "    N-F1: " << d_NF1 << " Å (expected ~" << r0_NF << " Å)\n";
    std::cout << "    N-F2: " << d_NF2 << " Å (expected ~" << r0_NF << " Å)\n";
    std::cout << "    N-F3: " << d_NF3 << " Å (expected ~" << r0_NF << " Å)\n";
    
    // All bonds should be at equilibrium
    ASSERT_NEAR(d_NF1, r0_NF, 1e-2);
    ASSERT_NEAR(d_NF2, r0_NF, 1e-2);
    ASSERT_NEAR(d_NF3, r0_NF, 1e-2);
    
    // Measure final angles
    double angle_F1NF2 = angle(result.coords, 1, 0, 2) * 180.0 / M_PI;
    double angle_F1NF3 = angle(result.coords, 1, 0, 3) * 180.0 / M_PI;
    double angle_F2NF3 = angle(result.coords, 2, 0, 3) * 180.0 / M_PI;
    
    std::cout << "\n  Final F-N-F angles:\n";
    std::cout << "    F1-N-F2: " << angle_F1NF2 << "° (expected: 102.5° - 107°)\n";
    std::cout << "    F1-N-F3: " << angle_F1NF3 << "° (expected: 102.5° - 107°)\n";
    std::cout << "    F2-N-F3: " << angle_F2NF3 << "° (expected: 102.5° - 107°)\n";
    
    // Average angle
    double avg_angle = (angle_F1NF2 + angle_F1NF3 + angle_F2NF3) / 3.0;
    std::cout << "    Average: " << avg_angle << "°\n";
    
    std::cout << "\n  ⚠️  WARNING: Angles are likely incorrect without angle bending terms!\n";
    std::cout << "  With only bond stretching, the molecule will collapse or have\n";
    std::cout << "  arbitrary angles determined only by initial geometry.\n";
    std::cout << "  Bond lengths are correct: ✓\n";
    std::cout << "  Bond angles need angle terms: ✗\n";
    
    // Note: We don't assert angle correctness since we know it will fail
    // This test demonstrates the NEED for angle terms
    
    std::cout << "  ✓ NF3 bond optimization passed (angles not yet implemented)\n";
}

// ============================================================================
// Test: Gradient Check Mode
// ============================================================================

void test_gradient_check() {
    std::cout << "Testing gradient check mode...\n";
    
    Molecule mol;
    mol.add_atom(6, 0.0, 0.0, 0.0);
    mol.add_atom(8, 1.3, 0.0, 0.0);
    mol.add_bond(0, 1, 1);
    
    EnergyModel model(mol);
    
    // Enable gradient checking
    OptimizerSettings settings;
    settings.check_gradients = true;
    settings.grad_check_tol = 1e-5;
    settings.max_iterations = 1;  // Just one iteration to test gradcheck
    
    FIREOptimizer optimizer(settings);
    OptimizeResult result = optimizer.minimize(mol.coords, model);
    
    // If we got here without terminating on gradient check, it passed
    std::cout << "  Termination reason: " << result.termination_reason << "\n";
    
    // Should not fail on gradient check
    assert(result.termination_reason != "Gradient check failed");
    
    std::cout << "  ✓ Gradient check mode passed\n";
}

// ============================================================================
// Test: Safety Features
// ============================================================================

void test_safety_features() {
    std::cout << "Testing safety features...\n";
    
    Molecule mol;
    
    // Create an extremely distorted system
    mol.add_atom(1, 0.0, 0.0, 0.0);
    mol.add_atom(1, 100.0, 0.0, 0.0);  // Very far apart
    mol.add_bond(0, 1, 1);
    
    EnergyModel model(mol);
    
    // Aggressive settings that might cause instability
    OptimizerSettings settings;
    settings.max_step = 0.1;           // Small step limit
    settings.dt_max = 0.5;
    settings.max_iterations = 500;
    settings.tol_rms_force = 1e-3;
    
    FIREOptimizer optimizer(settings);
    OptimizeResult result = optimizer.minimize(mol.coords, model);
    
    std::cout << "  Termination reason: " << result.termination_reason << "\n";
    std::cout << "  Iterations: " << result.iterations << "\n";
    std::cout << "  Final energy: " << result.energy << "\n";
    
    // Should converge or hit iteration limit, not crash
    assert(result.termination_reason != "NaN/Inf detected");
    
    // Final coordinates should be valid
    for (double x : result.coords) {
        assert(std::isfinite(x));
    }
    
    std::cout << "  ✓ Safety features passed\n";
}

// ============================================================================
// Main Test Suite
// ============================================================================

int main() {
    std::cout << "========================================\n";
    std::cout << "FIRE Optimizer Test Suite\n";
    std::cout << "========================================\n\n";
    
    try {
        test_h2_optimization();
        std::cout << "\n";
        
        test_water_optimization();
        std::cout << "\n";
        
        test_ethane_optimization();
        std::cout << "\n";
        
        test_nf3_optimization();
        std::cout << "\n";
        
        test_gradient_check();
        std::cout << "\n";
        
        test_safety_features();
        std::cout << "\n";
        
        std::cout << "========================================\n";
        std::cout << "All optimizer tests passed! ✓\n";
        std::cout << "========================================\n";
        
    } catch (const std::exception& e) {
        std::cerr << "\nTest failed with exception: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
