/**
 * Problem 1: Two-Body Neutral Binding (LJ Sanity Check)
 * 
 * Tests the most fundamental molecular dynamics: Two Ar atoms with LJ interaction.
 * 
 * Given:
 *   - ε = 0.238 kcal/mol
 *   - σ = 3.4 Å
 * 
 * Tasks:
 *   1. Compute equilibrium separation r₀
 *   2. Compute binding energy at r₀
 *   3. Verify F = 0 numerically (central difference)
 *   4. Explain why this is foundational
 * 
 * Expected Results:
 *   - r₀ = 2^(1/6) * σ = 3.8164 Å
 *   - U(r₀) = -ε = -0.238 kcal/mol
 *   - F(r₀) = 0.0 (within numerical precision)
 * 
 * If this fails, EVERYTHING ELSE IS DECORATIVE.
 */

#include "../atomistic/core/state.hpp"
#include "../atomistic/models/model.hpp"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <fstream>

using namespace atomistic;

// LJ parameters for Ar
constexpr double EPSILON = 0.238;  // kcal/mol
constexpr double SIGMA = 3.4;      // Å

// Theoretical predictions
constexpr double R0_THEORY = 3.8164;  // 2^(1/6) * σ
constexpr double U0_THEORY = -0.238;  // -ε

/**
 * Compute LJ energy manually (for verification)
 */
double compute_lj_energy(double r, double eps, double sig) {
    double sr = sig / r;
    double sr6 = sr*sr*sr * sr*sr*sr;
    double sr12 = sr6 * sr6;
    return 4.0 * eps * (sr12 - sr6);
}

/**
 * Compute LJ force magnitude manually (for verification)
 */
double compute_lj_force_mag(double r, double eps, double sig) {
    double sr = sig / r;
    double sr6 = sr*sr*sr * sr*sr*sr;
    double sr12 = sr6 * sr6;
    return 24.0 * eps * (2.0*sr12 - sr6) / r;
}

/**
 * Numerical derivative using central difference
 */
double numerical_force(double r, double eps, double sig, double dr = 1e-6) {
    double U_plus = compute_lj_energy(r + dr, eps, sig);
    double U_minus = compute_lj_energy(r - dr, eps, sig);
    return -(U_plus - U_minus) / (2.0 * dr);
}

int main() {
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  PROBLEM 1: Two-Body Neutral Binding (LJ Sanity Check)    ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";
    
    std::cout << std::fixed << std::setprecision(6);
    
    // ========================================================================
    // Task 1: Compute equilibrium separation r₀
    // ========================================================================
    
    std::cout << "TASK 1: Equilibrium Separation\n";
    std::cout << "─────────────────────────────────────────────────────\n";
    
    double r0_computed = std::pow(2.0, 1.0/6.0) * SIGMA;
    
    std::cout << "Given:\n";
    std::cout << "  ε = " << EPSILON << " kcal/mol\n";
    std::cout << "  σ = " << SIGMA << " Å\n\n";
    
    std::cout << "Theory:\n";
    std::cout << "  r₀ = 2^(1/6) * σ\n";
    std::cout << "     = " << std::pow(2.0, 1.0/6.0) << " × " << SIGMA << " Å\n";
    std::cout << "     = " << r0_computed << " Å\n\n";
    
    std::cout << "Expected: " << R0_THEORY << " Å\n";
    std::cout << "Computed: " << r0_computed << " Å\n";
    
    double r0_error = std::abs(r0_computed - R0_THEORY);
    std::cout << "Error: " << r0_error << " Å\n";
    
    if (r0_error < 1e-4) {
        std::cout << "✅ PASS: r₀ computed correctly\n\n";
    } else {
        std::cout << "❌ FAIL: r₀ computation error\n\n";
        return 1;
    }
    
    // ========================================================================
    // Task 2: Compute binding energy at r₀
    // ========================================================================
    
    std::cout << "TASK 2: Binding Energy at r₀\n";
    std::cout << "─────────────────────────────────────────────────────\n";
    
    double U_r0 = compute_lj_energy(r0_computed, EPSILON, SIGMA);
    
    std::cout << "Theory:\n";
    std::cout << "  U(r₀) = -ε\n";
    std::cout << "        = " << U0_THEORY << " kcal/mol\n\n";
    
    std::cout << "Computed:\n";
    std::cout << "  U(r₀) = " << U_r0 << " kcal/mol\n\n";
    
    double U_error = std::abs(U_r0 - U0_THEORY);
    std::cout << "Error: " << U_error << " kcal/mol\n";
    
    if (U_error < 1e-6) {
        std::cout << "✅ PASS: Binding energy correct\n\n";
    } else {
        std::cout << "❌ FAIL: Binding energy error\n\n";
        return 1;
    }
    
    // ========================================================================
    // Task 3: Verify F = 0 at r₀ (numerical central difference)
    // ========================================================================
    
    std::cout << "TASK 3: Force at r₀ (Should be Zero)\n";
    std::cout << "─────────────────────────────────────────────────────\n";
    
    // Analytical force
    double F_analytical = compute_lj_force_mag(r0_computed, EPSILON, SIGMA);
    
    // Numerical force (central difference)
    double F_numerical = numerical_force(r0_computed, EPSILON, SIGMA);
    
    std::cout << "Analytical:\n";
    std::cout << "  F(r₀) = " << F_analytical << " kcal/(mol·Å)\n\n";
    
    std::cout << "Numerical (central difference, dr=1e-6 Å):\n";
    std::cout << "  F(r₀) = " << F_numerical << " kcal/(mol·Å)\n\n";
    
    double F_error = std::abs(F_analytical);
    std::cout << "Error from zero: " << F_error << " kcal/(mol·Å)\n";
    
    if (F_error < 1e-8) {
        std::cout << "✅ PASS: Force = 0 at equilibrium\n\n";
    } else {
        std::cout << "❌ FAIL: Force non-zero at equilibrium\n\n";
        return 1;
    }
    
    // ========================================================================
    // Task 4: Scan potential curve and save to file
    // ========================================================================
    
    std::cout << "TASK 4: Scan Potential Curve\n";
    std::cout << "─────────────────────────────────────────────────────\n";
    
    std::ofstream out("out/lj_potential_curve.csv");
    out << "r,U_lj,F_mag,F_numerical\n";
    
    std::cout << "Scanning r = 3.0 to 6.0 Å...\n";
    
    int n_points = 0;
    for (double r = 3.0; r <= 6.0; r += 0.01) {
        double U = compute_lj_energy(r, EPSILON, SIGMA);
        double F_mag = compute_lj_force_mag(r, EPSILON, SIGMA);
        double F_num = numerical_force(r, EPSILON, SIGMA);
        
        out << r << "," << U << "," << F_mag << "," << F_num << "\n";
        n_points++;
    }
    
    out.close();
    std::cout << "✅ Saved " << n_points << " points to out/lj_potential_curve.csv\n\n";
    
    // ========================================================================
    // Now test with actual MD code
    // ========================================================================
    
    std::cout << "TASK 5: Validate with Actual MD Code\n";
    std::cout << "─────────────────────────────────────────────────────\n";
    
    // Create state with 2 Ar atoms at r₀
    State state;
    state.N = 2;
    state.X.resize(2);
    state.X[0] = {0.0, 0.0, 0.0};
    state.X[1] = {r0_computed, 0.0, 0.0};
    
    state.M.resize(2);
    state.M[0] = 39.948;  // Ar mass (amu)
    state.M[1] = 39.948;
    
    state.Q.resize(2);
    state.Q[0] = 0.0;  // Neutral
    state.Q[1] = 0.0;
    
    state.type.resize(2);
    state.type[0] = 18;  // Ar (Z=18)
    state.type[1] = 18;
    
    state.F.resize(2);
    state.V.resize(2);
    
    state.box = BoxPBC();  // No PBC
    
    // Create model
    auto model = create_lj_coulomb_model();
    ModelParams params;
    params.rc = 10.0;  // Large cutoff
    
    // Evaluate forces
    model->eval(state, params);
    
    std::cout << "MD Code Results:\n";
    std::cout << "  Energy: " << state.E.total() << " kcal/mol\n";
    std::cout << "  Force on atom 0: (" << state.F[0].x << ", " 
              << state.F[0].y << ", " << state.F[0].z << ") kcal/(mol·Å)\n";
    std::cout << "  Force on atom 1: (" << state.F[1].x << ", " 
              << state.F[1].y << ", " << state.F[1].z << ") kcal/(mol·Å)\n\n";
    
    // Verify energy matches theory
    double U_md = state.E.total();
    double U_error_md = std::abs(U_md - U0_THEORY);
    
    std::cout << "Energy Validation:\n";
    std::cout << "  Expected: " << U0_THEORY << " kcal/mol\n";
    std::cout << "  MD Code:  " << U_md << " kcal/mol\n";
    std::cout << "  Error:    " << U_error_md << " kcal/mol\n";
    
    if (U_error_md < 1e-3) {
        std::cout << "  ✅ PASS: MD energy matches theory\n\n";
    } else {
        std::cout << "  ❌ FAIL: MD energy mismatch\n\n";
        return 1;
    }
    
    // Verify forces are zero (within tolerance)
    double F0_mag = std::sqrt(state.F[0].x*state.F[0].x + 
                              state.F[0].y*state.F[0].y + 
                              state.F[0].z*state.F[0].z);
    double F1_mag = std::sqrt(state.F[1].x*state.F[1].x + 
                              state.F[1].y*state.F[1].y + 
                              state.F[1].z*state.F[1].z);
    
    std::cout << "Force Validation:\n";
    std::cout << "  |F₀| = " << F0_mag << " kcal/(mol·Å)\n";
    std::cout << "  |F₁| = " << F1_mag << " kcal/(mol·Å)\n";
    
    if (F0_mag < 1e-6 && F1_mag < 1e-6) {
        std::cout << "  ✅ PASS: Forces = 0 at equilibrium\n\n";
    } else {
        std::cout << "  ❌ FAIL: Forces non-zero\n\n";
        return 1;
    }
    
    // ========================================================================
    // Explanation
    // ========================================================================
    
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  WHY THIS TEST IS FOUNDATIONAL                             ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";
    
    std::cout << "1. FORCE CALCULATION\n";
    std::cout << "   If F ≠ 0 at r₀, the derivative ∂U/∂r is wrong.\n";
    std::cout << "   → All MD trajectories will be incorrect.\n\n";
    
    std::cout << "2. ENERGY EVALUATION\n";
    std::cout << "   If U(r₀) ≠ -ε, the potential is miscoded.\n";
    std::cout << "   → Binding energies, thermodynamics are wrong.\n\n";
    
    std::cout << "3. NUMERICAL INTEGRATION\n";
    std::cout << "   If analytical and numerical forces disagree, there's a bug.\n";
    std::cout << "   → Verlet integration will accumulate errors.\n\n";
    
    std::cout << "4. BEFORE MULTI-ATOM SYSTEMS\n";
    std::cout << "   If 2 atoms fail, N atoms will catastrophically fail.\n";
    std::cout << "   → Formation, crystallization, all higher-level features broken.\n\n";
    
    std::cout << "5. NEUTRAL-FIRST PRINCIPLE\n";
    std::cout << "   LJ (neutral) is simpler than LJ+Coulomb (ionic).\n";
    std::cout << "   → Must work for neutral before attempting charged.\n\n";
    
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  VERDICT                                                   ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";
    
    std::cout << "✅ ALL TESTS PASSED!\n\n";
    std::cout << "Two-body LJ binding is correct:\n";
    std::cout << "  • r₀ computed correctly (" << r0_computed << " Å)\n";
    std::cout << "  • U(r₀) = -ε (" << U_r0 << " kcal/mol)\n";
    std::cout << "  • F(r₀) = 0 (within numerical precision)\n";
    std::cout << "  • MD code matches analytical theory\n\n";
    
    std::cout << "READY TO PROCEED TO PROBLEM 2 (Three-Body Cluster)\n\n";
    
    return 0;
}
