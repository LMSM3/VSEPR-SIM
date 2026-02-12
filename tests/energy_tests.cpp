#include "pot/energy_model.hpp"
#include "sim/molecule.hpp"
#include "core/geom_ops.hpp"
#include <iostream>
#include <cmath>
#include <cassert>

using namespace vsepr;

#define ASSERT_NEAR(a, b, tol) \
    assert(std::abs((a) - (b)) < (tol) && "Assertion failed: values not close enough")

// ============================================================================
// Finite Difference Gradient Check
// ============================================================================

// Compute numerical gradient using central differences
std::vector<double> compute_numerical_gradient(
    const EnergyModel& model,
    const std::vector<double>& coords,
    double h = 1e-6)
{
    std::vector<double> grad_num(coords.size(), 0.0);
    std::vector<double> coords_plus = coords;
    std::vector<double> coords_minus = coords;

    for (size_t i = 0; i < coords.size(); ++i) {
        // Forward step
        coords_plus[i] += h;
        double E_plus = model.evaluate_energy(coords_plus);
        coords_plus[i] = coords[i];  // restore

        // Backward step
        coords_minus[i] -= h;
        double E_minus = model.evaluate_energy(coords_minus);
        coords_minus[i] = coords[i];  // restore

        // Central difference
        grad_num[i] = (E_plus - E_minus) / (2.0 * h);
    }

    return grad_num;
}

// Compare analytical and numerical gradients
bool check_gradient(const EnergyModel& model,
                    const std::vector<double>& coords,
                    double tol = 1e-5,
                    bool verbose = true)
{
    std::vector<double> grad_analytic(coords.size());
    model.evaluate_energy_gradient(coords, grad_analytic);

    std::vector<double> grad_numeric = compute_numerical_gradient(model, coords);

    double max_error = 0.0;
    size_t max_error_idx = 0;

    for (size_t i = 0; i < coords.size(); ++i) {
        double error = std::abs(grad_analytic[i] - grad_numeric[i]);
        if (error > max_error) {
            max_error = error;
            max_error_idx = i;
        }
    }

    if (verbose) {
        std::cout << "  Max gradient error: " << max_error 
                  << " at index " << max_error_idx << "\n";
        std::cout << "    Analytic: " << grad_analytic[max_error_idx] << "\n";
        std::cout << "    Numeric:  " << grad_numeric[max_error_idx] << "\n";
    }

    return max_error < tol;
}

// ============================================================================
// Test: Single H2 Molecule
// ============================================================================

void test_h2_molecule() {
    std::cout << "Testing H2 molecule...\n";

    Molecule mol;
    
    // Two hydrogen atoms
    mol.add_atom(1, 0.0, 0.0, 0.0);  // H1
    mol.add_atom(1, 1.0, 0.0, 0.0);  // H2
    
    // Bond between them
    mol.add_bond(0, 1, 1);  // single bond
    
    // Create energy model
    EnergyModel model(mol);
    
    // Test 1: Energy at displaced positions
    double E = model.evaluate_energy(mol.coords);
    std::cout << "  H2 energy at r=1.0 Å: " << E << " kcal/mol\n";
    assert(E > 0.0);  // Should be positive (stretched from ~0.74 Å equilibrium)
    
    // Test 2: Gradient check
    std::cout << "  Checking gradients...\n";
    assert(check_gradient(model, mol.coords));
    
    // Test 3: Energy at equilibrium
    double r0_H2 = 2 * get_covalent_radius(1);  // ~0.64 Å
    mol.set_position(1, r0_H2, 0.0, 0.0);
    E = model.evaluate_energy(mol.coords);
    std::cout << "  H2 energy at r=" << r0_H2 << " Å: " << E << " kcal/mol\n";
    ASSERT_NEAR(E, 0.0, 1e-10);  // Should be zero at equilibrium
    
    std::cout << "  ✓ H2 molecule tests passed\n";
}

// ============================================================================
// Test: Translation Invariance
// ============================================================================

void test_translation_invariance() {
    std::cout << "Testing translation invariance...\n";

    Molecule mol;
    
    // Simple diatomic
    mol.add_atom(6, 0.0, 0.0, 0.0);  // C
    mol.add_atom(6, 1.5, 0.0, 0.0);  // C
    mol.add_bond(0, 1, 1);
    
    EnergyModel model(mol);
    
    double E1 = model.evaluate_energy(mol.coords);
    
    // Translate molecule
    Vec3 translation{10.0, -5.0, 3.0};
    std::vector<double> coords_shifted = mol.coords;
    for (size_t i = 0; i < mol.num_atoms(); ++i) {
        coords_shifted[3*i + 0] += translation.x;
        coords_shifted[3*i + 1] += translation.y;
        coords_shifted[3*i + 2] += translation.z;
    }
    
    double E2 = model.evaluate_energy(coords_shifted);
    
    std::cout << "  Energy before translation: " << E1 << "\n";
    std::cout << "  Energy after translation:  " << E2 << "\n";
    std::cout << "  Difference: " << std::abs(E1 - E2) << "\n";
    
    ASSERT_NEAR(E1, E2, 1e-10);
    
    std::cout << "  ✓ Translation invariance verified\n";
}

// ============================================================================
// Test: Rotation Invariance
// ============================================================================

void test_rotation_invariance() {
    std::cout << "Testing rotation invariance...\n";

    Molecule mol;
    
    // C-C bond along x-axis
    mol.add_atom(6, 0.0, 0.0, 0.0);
    mol.add_atom(6, 1.5, 0.0, 0.0);
    mol.add_bond(0, 1, 1);
    
    EnergyModel model(mol);
    
    double E1 = model.evaluate_energy(mol.coords);
    
    // Rotate 90° around z-axis: (x,y,z) -> (-y,x,z)
    std::vector<double> coords_rotated = mol.coords;
    for (size_t i = 0; i < mol.num_atoms(); ++i) {
        double x = mol.coords[3*i + 0];
        double y = mol.coords[3*i + 1];
        coords_rotated[3*i + 0] = -y;
        coords_rotated[3*i + 1] = x;
    }
    
    double E2 = model.evaluate_energy(coords_rotated);
    
    std::cout << "  Energy before rotation: " << E1 << "\n";
    std::cout << "  Energy after rotation:  " << E2 << "\n";
    std::cout << "  Difference: " << std::abs(E1 - E2) << "\n";
    
    ASSERT_NEAR(E1, E2, 1e-10);
    
    std::cout << "  ✓ Rotation invariance verified\n";
}

// ============================================================================
// Test: Force Balance (Newton's Third Law)
// ============================================================================

void test_force_balance() {
    std::cout << "Testing force balance (Newton's 3rd law)...\n";

    Molecule mol;
    
    // Two-atom system
    mol.add_atom(6, 0.0, 0.0, 0.0);
    mol.add_atom(8, 1.3, 0.0, 0.0);
    mol.add_bond(0, 1, 1);
    
    EnergyModel model(mol);
    
    std::vector<double> gradient;
    model.evaluate_energy_gradient(mol.coords, gradient);
    
    // Get forces on each atom (force = -gradient)
    Vec3 force_0 = -get_pos(gradient, 0);
    Vec3 force_1 = -get_pos(gradient, 1);
    
    std::cout << "  Force on atom 0: (" << force_0.x << ", " << force_0.y << ", " << force_0.z << ")\n";
    std::cout << "  Force on atom 1: (" << force_1.x << ", " << force_1.y << ", " << force_1.z << ")\n";
    
    // Forces should sum to zero (Newton's 3rd law)
    Vec3 total_force = force_0 + force_1;
    std::cout << "  Total force: (" << total_force.x << ", " << total_force.y << ", " << total_force.z << ")\n";
    
    ASSERT_NEAR(total_force.x, 0.0, 1e-10);
    ASSERT_NEAR(total_force.y, 0.0, 1e-10);
    ASSERT_NEAR(total_force.z, 0.0, 1e-10);
    
    std::cout << "  ✓ Force balance verified\n";
}

// ============================================================================
// Test: Water Molecule (3 atoms, 2 bonds)
// ============================================================================

void test_water_molecule() {
    std::cout << "Testing H2O molecule...\n";

    Molecule mol;
    
    // O-H bonds at ~0.96 Å, H-O-H angle ~104.5°
    mol.add_atom(8, 0.0, 0.0, 0.0);           // O
    mol.add_atom(1, 0.96, 0.0, 0.0);          // H1
    mol.add_atom(1, -0.24, 0.93, 0.0);        // H2 (approximate)
    
    mol.add_bond(0, 1, 1);  // O-H1
    mol.add_bond(0, 2, 1);  // O-H2
    
    EnergyModel model(mol);
    
    // Evaluate energy
    double E = model.evaluate_energy(mol.coords);
    std::cout << "  Water energy: " << E << " kcal/mol\n";
    
    // Check gradients
    std::cout << "  Checking gradients...\n";
    assert(check_gradient(model, mol.coords));
    
    // Check force balance (should be nearly zero if near equilibrium)
    std::vector<double> gradient;
    model.evaluate_energy_gradient(mol.coords, gradient);
    
    Vec3 total_force{0, 0, 0};
    for (size_t i = 0; i < mol.num_atoms(); ++i) {
        total_force += -get_pos(gradient, i);
    }
    
    std::cout << "  Total force magnitude: " << total_force.norm() << "\n";
    ASSERT_NEAR(total_force.norm(), 0.0, 1e-9);
    
    std::cout << "  ✓ Water molecule tests passed\n";
}

// ============================================================================
// Main Test Suite
// ============================================================================

int main() {
    std::cout << "========================================\n";
    std::cout << "Energy Model Test Suite\n";
    std::cout << "========================================\n\n";
    
    try {
        test_h2_molecule();
        std::cout << "\n";
        
        test_translation_invariance();
        std::cout << "\n";
        
        test_rotation_invariance();
        std::cout << "\n";
        
        test_force_balance();
        std::cout << "\n";
        
        test_water_molecule();
        std::cout << "\n";
        
        std::cout << "========================================\n";
        std::cout << "All tests passed! ✓\n";
        std::cout << "========================================\n";
        
    } catch (const std::exception& e) {
        std::cerr << "\nTest failed with exception: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
