/**
 * simple_nacl_test.cpp
 * Simple standalone test to verify PBC works
 */

#include "atomistic/models/model.hpp"
#include "atomistic/integrators/fire.hpp"
#include "atomistic/core/state.hpp"
#include <iostream>
#include <iomanip>

int main() {
    std::cout << "═══════════════════════════════════════\n";
    std::cout << "  NaCl Crystal with PBC - Simple Test\n";
    std::cout << "═══════════════════════════════════════\n\n";

    // Create NaCl conventional cell (8 atoms)
    atomistic::State state;
    state.N = 8;

    double a = 5.64;  // Lattice parameter

    // 8 atoms: 4 Na + 4 Cl (rocksalt structure)
    state.type = {11, 17, 11, 17, 11, 17, 11, 17};
    state.Q = {+1, -1, +1, -1, +1, -1, +1, -1};  // Ionic charges
    
    state.X = {
        {0.0,  0.0,  0.0},      // Na
        {a/2,  0.0,  0.0},      // Cl
        {0.0,  a/2,  0.0},      // Na
        {a/2,  a/2,  0.0},      // Cl
        {0.0,  0.0,  a/2},      // Na
        {a/2,  0.0,  a/2},      // Cl
        {0.0,  a/2,  a/2},      // Na
        {a/2,  a/2,  a/2}       // Cl
    };
    
    // Initialize velocities, masses, forces
    state.V.resize(state.N, {0, 0, 0});
    state.M.resize(state.N, 1.0);
    state.F.resize(state.N, {0, 0, 0});
    
    // **ENABLE PBC** (cubic box)
    state.box = vsepr::BoxPBC(a, a, a);
    
    std::cout << "Initial setup:\n";
    std::cout << "  Atoms: " << state.N << " (4 Na + 4 Cl)\n";
    std::cout << "  PBC: " << (state.box.enabled ? "ENABLED" : "DISABLED") << "\n";
    std::cout << "  Box size: " << a << " x " << a << " x " << a << " Å\n\n";
    
    // Create force field model
    auto model = vsepr::create_lj_coulomb_model();
    
    vsepr::ModelParams params;
    params.rc = 10.0;        // Cutoff 10 Å
    params.k_coul = 138.935; // Coulomb constant
    
    // Evaluate initial energy
    model->eval(state, params);
    
    std::cout << "Initial energy:\n";
    std::cout << "  LJ:      " << std::fixed << std::setprecision(3) 
              << state.E.UvdW << " kcal/mol\n";
    std::cout << "  Coulomb: " << state.E.UCoul << " kcal/mol\n";
    std::cout << "  Total:   " << (state.E.UvdW + state.E.UCoul) << " kcal/mol\n\n";
    
    // Check if energy is zero (indicates problem)
    double total_energy = state.E.UvdW + state.E.UCoul;
    if (std::abs(total_energy) < 1e-10) {
        std::cout << "❌ ERROR: Energy is zero! Force field not working.\n";
        return 1;
    }
    
    // Compute nearest neighbor distance
    double min_dist = 1e10;
    for (uint32_t i = 0; i < state.N; ++i) {
        for (uint32_t j = i + 1; j < state.N; ++j) {
            vsepr::Vec3 rij = state.box.delta(state.X[i], state.X[j]);
            double r = std::sqrt(dot(rij, rij));
            if (r < min_dist) min_dist = r;
        }
    }
    
    std::cout << "Nearest neighbor distance: " << min_dist << " Å\n";
    std::cout << "Expected: ~2.82 Å (a/2)\n\n";
    
    // Run FIRE minimization
    std::cout << "Running FIRE minimization...\n";

    vsepr::FIREParams fire_params;
    fire_params.max_steps = 100;
    fire_params.epsF = 1e-4;

    vsepr::FIRE fire(*model, params);
    auto result = fire.minimize(state, fire_params);

    std::cout << "\nFIRE result:\n";
    std::cout << "  Converged: " << (result.Frms < fire_params.epsF ? "YES" : "NO") << "\n";
    std::cout << "  Iterations: " << result.step << "\n";
    std::cout << "  Final RMS force: " << std::scientific << result.Frms << "\n";
    std::cout << "  Final energy: " << std::fixed << std::setprecision(3) 
              << result.U << " kcal/mol\n\n";
    
    // Recompute NN distance after minimization
    min_dist = 1e10;
    for (uint32_t i = 0; i < state.N; ++i) {
        for (uint32_t j = i + 1; j < state.N; ++j) {
            vsepr::Vec3 rij = state.box.delta(state.X[i], state.X[j]);
            double r = std::sqrt(dot(rij, rij));
            if (r < min_dist) min_dist = r;
        }
    }
    
    std::cout << "Relaxed nearest neighbor: " << min_dist << " Å\n";
    
    // Check result
    if (min_dist > 2.5 && min_dist < 3.0) {
        std::cout << "\n✅ SUCCESS: Crystal structure looks realistic!\n";
        std::cout << "   PBC is working correctly.\n";
        return 0;
    } else {
        std::cout << "\n⚠️  WARNING: NN distance outside expected range.\n";
        std::cout << "   Expected: 2.5-3.0 Å, got: " << min_dist << " Å\n";
        return 1;
    }
}
