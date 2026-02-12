/**
 * Surgical Langevin Debugging
 * 
 * Isolate the source of temperature explosion by:
 * 1. Testing with NO noise (friction only)
 * 2. Testing with NO forces (one atom, no LJ)
 * 3. Printing a, b, dt, gamma for first 5 steps
 */

#include "atomistic/core/state.hpp"
#include "atomistic/core/maxwell_boltzmann.hpp"
#include <iostream>
#include <iomanip>
#include <random>
#include <cmath>

using namespace atomistic;

constexpr double k_B = 0.001987204;  // kcal/mol/K
constexpr double KE_CONV = 2390.0;

double compute_temperature(const State& state) {
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
    std::cout << "║  SURGICAL LANGEVIN DEBUG                                  ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════╝\n\n";
    
    // ONE ATOM TEST (no forces, no neighbors)
    State state;
    state.N = 1;
    state.M.resize(1, 39.948);  // Ar mass
    state.V.resize(1, {0.001, 0.0, 0.0});  // Small initial velocity
    state.X.resize(1, {0, 0, 0});
    state.F.resize(1, {0, 0, 0});  // NO FORCES
    
    double T_target = 300.0;
    double gamma = 0.1;     // 1/fs
    double dt = 1.0;        // fs
    
    std::cout << "Setup:\n";
    std::cout << "  N = 1 (single atom, no forces)\n";
    std::cout << "  m = " << state.M[0] << " amu\n";
    std::cout << "  T_target = " << T_target << " K\n";
    std::cout << "  gamma = " << gamma << " / fs\n";
    std::cout << "  dt = " << dt << " fs\n\n";
    
    // Compute a and b
    double a = std::exp(-gamma * dt);
    double one_minus_a2 = 1.0 - a*a;
    double b_internal = std::sqrt(k_B * T_target / state.M[0] * one_minus_a2);
    double b = b_internal * 0.0205;  // Convert to Å/fs
    
    std::cout << "Langevin coefficients:\n";
    std::cout << "  a = exp(-γ dt) = " << std::scientific << std::setprecision(6) << a << "\n";
    std::cout << "  1 - a² = " << one_minus_a2 << "\n";
    std::cout << "  b_internal = sqrt(k_B T / m * (1-a²)) = " << b_internal << " (internal units)\n";
    std::cout << "  b = b_internal * 0.0205 = " << b << " Å/fs\n\n";
    
    // Sanity checks
    std::cout << "Sanity checks:\n";
    if (a < 0 || a > 1.0) {
        std::cout << "  ❌ a out of range [0, 1]: " << a << "\n";
    } else {
        std::cout << "  ✅ a in range [0, 1]: " << a << "\n";
    }
    
    if (one_minus_a2 < 0 || one_minus_a2 > 1.0) {
        std::cout << "  ❌ 1-a² out of range [0, 1]: " << one_minus_a2 << "\n";
    } else {
        std::cout << "  ✅ 1-a² in range [0, 1]: " << one_minus_a2 << "\n";
    }
    
    if (b > 0.1) {
        std::cout << "  ⚠️  b is large: " << b << " Å/fs (expected ~0.0001-0.01)\n";
    } else {
        std::cout << "  ✅ b is reasonable: " << b << " Å/fs\n";
    }
    std::cout << "\n";
    
    // TEST 1: NO NOISE (friction only)
    std::cout << "╔═══════════════════════════════════════════════════════════╗\n";
    std::cout << "║  TEST 1: Friction Only (No Noise)                        ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════╝\n\n";
    
    State state1 = state;  // Copy
    
    std::cout << "Running 20 steps with friction only...\n";
    std::cout << "Step    v_x (Å/fs)    T (K)\n";
    std::cout << "────────────────────────────────\n";
    
    for (int step = 0; step < 20; ++step) {
        // Friction only: v_new = a * v_old
        state1.V[0].x *= a;
        state1.V[0].y *= a;
        state1.V[0].z *= a;
        
        double T = compute_temperature(state1);
        
        if (step % 5 == 0 || step < 5) {
            std::cout << std::setw(4) << step 
                      << "    " << std::scientific << std::setprecision(3) << state1.V[0].x 
                      << "    " << std::fixed << std::setprecision(1) << T << "\n";
        }
    }
    
    std::cout << "\nExpected: T should decay toward 0 K\n";
    double T_final_1 = compute_temperature(state1);
    if (T_final_1 < 10.0) {
        std::cout << "✅ PASS: Temperature decayed to " << T_final_1 << " K\n";
    } else {
        std::cout << "❌ FAIL: Temperature did not decay (still " << T_final_1 << " K)\n";
    }
    std::cout << "\n";
    
    // TEST 2: FULL LANGEVIN (friction + noise)
    std::cout << "╔═══════════════════════════════════════════════════════════╗\n";
    std::cout << "║  TEST 2: Full Langevin (Friction + Noise)                ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════╝\n\n";
    
    State state2 = state;  // Fresh copy
    std::mt19937 rng(42);
    std::normal_distribution<double> gaussian(0.0, 1.0);
    
    std::cout << "Running 1000 steps with full Langevin...\n";
    std::cout << "Step    v_x (Å/fs)    T (K)      noise_x\n";
    std::cout << "───────────────────────────────────────────────\n";
    
    std::vector<double> T_history;
    
    for (int step = 0; step < 1000; ++step) {
        // Langevin update: v_new = a * v_old + b * R
        double R_x = gaussian(rng);
        double R_y = gaussian(rng);
        double R_z = gaussian(rng);
        
        double noise_x = b * R_x;
        double noise_y = b * R_y;
        double noise_z = b * R_z;
        
        state2.V[0].x = a * state2.V[0].x + noise_x;
        state2.V[0].y = a * state2.V[0].y + noise_y;
        state2.V[0].z = a * state2.V[0].z + noise_z;
        
        double T = compute_temperature(state2);
        T_history.push_back(T);
        
        if (step < 10 || step % 100 == 0) {
            std::cout << std::setw(4) << step 
                      << "    " << std::scientific << std::setprecision(3) << state2.V[0].x 
                      << "    " << std::fixed << std::setprecision(1) << T 
                      << "    " << std::scientific << std::setprecision(3) << noise_x << "\n";
        }
    }
    
    // Compute mean T over last 500 steps
    double T_mean = 0.0;
    for (int i = 500; i < 1000; ++i) {
        T_mean += T_history[i];
    }
    T_mean /= 500.0;
    
    std::cout << "\nExpected: <T> ≈ " << T_target << " K\n";
    std::cout << "Actual: <T> = " << std::fixed << std::setprecision(1) << T_mean << " K\n";
    
    double error_pct = 100.0 * (T_mean - T_target) / T_target;
    
    if (std::abs(error_pct) < 20.0) {
        std::cout << "✅ PASS: Temperature within 20% (" << std::showpos << error_pct << std::noshowpos << "%)\n";
        return 0;
    } else {
        std::cout << "❌ FAIL: Temperature error = " << std::showpos << error_pct << std::noshowpos << "%\n";
        return 1;
    }
}
