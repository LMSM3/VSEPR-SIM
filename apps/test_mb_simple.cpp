// Minimal test: Check Maxwell-Boltzmann initialization
#include "atomistic/core/state.hpp"
#include "atomistic/core/maxwell_boltzmann.hpp"
#include <iostream>
#include <iomanip>
#include <random>
#include <cmath>

using namespace atomistic;

constexpr double k_B = 0.001987204;  // kcal/mol/K
constexpr double KE_CONV = 2390.0;   // Correct conversion factor (NOT 1195!)

int main() {
    std::cout << "Testing Maxwell-Boltzmann initialization...\n\n";
    
    // Create simple system: 100 Ar atoms
    State state;
    state.N = 100;
    state.M.resize(100, 39.948);  // Ar mass
    state.V.resize(100);
    
    double T_target = 300.0;  // K
    
    // Initialize velocities
    std::mt19937 rng(42);
    initialize_velocities_thermal(state, T_target, rng);
    
    // Compute kinetic energy
    double KE = 0.0;
    for (uint32_t i = 0; i < state.N; ++i) {
        double v2 = state.V[i].x*state.V[i].x + state.V[i].y*state.V[i].y + state.V[i].z*state.V[i].z;
        KE += 0.5 * state.M[i] * v2 * KE_CONV;
    }
    
    // Compute temperature: T = 2*KE / (3N * k_B)
    double T_kin = (2.0 * KE) / (3.0 * state.N * k_B);
    
    // Print results
    std::cout << "Target T: " << T_target << " K\n";
    std::cout << "Computed T_kin: " << std::fixed << std::setprecision(2) << T_kin << " K\n";
    std::cout << "KE: " << KE << " kcal/mol\n";
    std::cout << "Sample velocity: (" << std::scientific << state.V[0].x << ", " << state.V[0].y << ", " << state.V[0].z << ") Å/fs\n";
    
    // Expected: T_kin ≈ 300 K
    if (std::abs(T_kin - T_target) < 30.0) {
        std::cout << "\n✅ PASS: Temperature within 10%\n";
        return 0;
    } else {
        std::cout << "\n❌ FAIL: Temperature error = " << (100.0 * (T_kin - T_target) / T_target) << "%\n";
        return 1;
    }
}
