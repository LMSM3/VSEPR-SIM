/**
 * BAOAB Langevin Test - 8 atoms with forces
 * 
 * Tests that BAOAB integrator works with multi-atom systems
 */

#include "atomistic/core/state.hpp"
#include "atomistic/core/maxwell_boltzmann.hpp"
#include "atomistic/models/model.hpp"
#include "atomistic/integrators/velocity_verlet.hpp"
#include <iostream>
#include <iomanip>
#include <random>
#include <cmath>

using namespace atomistic;

constexpr double k_B = 0.001987204;
constexpr double KE_CONV = 2390.0;

// Ar parameters
constexpr double Ar_mass = 39.948;
constexpr double Ar_sigma = 3.4;
constexpr double Ar_epsilon = 0.238;

double compute_temp_local(const State& state) {
    double KE = 0.0;
    for (uint32_t i = 0; i < state.N; ++i) {
        double v2 = state.V[i].x*state.V[i].x + state.V[i].y*state.V[i].y + state.V[i].z*state.V[i].z;
        KE += 0.5 * state.M[i] * v2 * KE_CONV;
    }

    double T = (2.0 * KE) / (3.0 * state.N * k_B);
    return T;
}

int main() {
    std::cout << "╔═══════════════════════════════════════════════════════════╗\n";
    std::cout << "║  BAOAB TEST: 64 Ar atoms with LJ forces                  ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════╝\n\n";
    
    // Create system (INCREASED to 64 atoms)
    const int N = 64;
    State state;
    state.N = N;
    state.M.resize(N, Ar_mass);
    state.Q.resize(N, 0.0);  // No charge
    state.type.resize(N, 18);  // Ar

    // Setup box (larger for 64 atoms)
    double box_length = 30.0;  // Å
    state.box.enabled = true;
    state.box.L = {box_length, box_length, box_length};
    state.box.invL = {1.0/box_length, 1.0/box_length, 1.0/box_length};

    // Place atoms on a 4×4×4 grid
    state.X.resize(N);
    int idx = 0;
    for (int ix = 0; ix < 4; ++ix) {
        for (int iy = 0; iy < 4; ++iy) {
            for (int iz = 0; iz < 4; ++iz) {
                state.X[idx++] = {5.0 + ix*7.0, 5.0 + iy*7.0, 5.0 + iz*7.0};
            }
        }
    }
    
    // Initialize velocities
    double T_initial = 50.0;  // K (wrong temp)
    std::mt19937 rng(42);
    initialize_velocities_thermal(state, T_initial, rng);

    // Allocate force array
    state.F.resize(N, {0, 0, 0});
    state.V.resize(N, {0, 0, 0});  // Initialize velocities (will be overwritten)

    initialize_velocities_thermal(state, T_initial, rng);
    
    double T_check = compute_temp_local(state);
    std::cout << "Initial T = " << std::fixed << std::setprecision(1) << T_check << " K\n";
    std::cout << "Target T = 300 K\n\n";
    
    // Create LJ model
    auto model = create_lj_coulomb_model();
    ModelParams mp;
    mp.rc = 10.0;
    
    // Run Langevin dynamics
    LangevinDynamics dynamics(*model, mp);
    
    LangevinParams params;
    params.dt = 1.0;
    params.n_steps = 10000;  // 10 ps
    params.T_target = 300.0;
    params.gamma = 0.1;
    params.print_freq = 1000;
    params.verbose = true;
    
    auto stats = dynamics.integrate(state, params, rng);
    
    // Check results
    std::cout << "\n╔═══════════════════════════════════════════════════════════╗\n";
    std::cout << "║  VALIDATION                                                ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════╝\n\n";
    
    double error_pct = 100.0 * (stats.T_avg - params.T_target) / params.T_target;
    
    if (std::abs(error_pct) < 10.0) {
        std::cout << "✅ PASS: Temperature within 10% (" << std::showpos << std::fixed << std::setprecision(1) << error_pct << std::noshowpos << "%)\n";
        return 0;
    } else {
        std::cout << "❌ FAIL: Temperature error = " << std::showpos << error_pct << std::noshowpos << "%\n";
        return 1;
    }
}
