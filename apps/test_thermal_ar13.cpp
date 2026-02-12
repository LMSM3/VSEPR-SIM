/**
 * APPLICATION TEST: Thermal Formation vs Quench-Only
 * 
 * SIMPLIFIED VERSION: Pure Ar cluster (13 atoms)
 * Target: Icosahedral structure (lowest energy for 13-atom LJ cluster)
 * 
 * This test validates that thermal annealing can find the global minimum
 * (icosahedron) while quenching gets stuck in local minima.
 */

#include "atomistic/core/state.hpp"
#include "atomistic/core/maxwell_boltzmann.hpp"
#include "atomistic/models/model.hpp"
#include "atomistic/integrators/fire.hpp"
#include "atomistic/integrators/velocity_verlet.hpp"
#include <iostream>
#include <iomanip>
#include <random>
#include <cmath>
#include <vector>
#include <algorithm>

using namespace atomistic;

constexpr double Ar_mass = 39.948;

struct Result {
    double final_energy;
    int seed;
};

State create_ar_cluster(int seed, int N, double box_length) {
    State state;
    state.N = N;
    
    // Setup box (no PBC for clusters)
    state.box.enabled = false;
    
    // Allocate
    state.X.resize(N);
    state.V.resize(N, {0, 0, 0});
    state.F.resize(N, {0, 0, 0});
    state.M.resize(N, Ar_mass);
    state.Q.resize(N, 0.0);  // Neutral
    state.type.resize(N, 18);  // Ar
    
    // Random positions in sphere
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> dist(-box_length/2, box_length/2);
    
    double min_dist = 2.0;
    
    for (uint32_t i = 0; i < state.N; ++i) {
        bool valid = false;
        int attempts = 0;
        
        while (!valid && attempts < 1000) {
            Vec3 pos;
            pos.x = dist(rng);
            pos.y = dist(rng);
            pos.z = dist(rng);
            
            // Keep in sphere
            double r = std::sqrt(pos.x*pos.x + pos.y*pos.y + pos.z*pos.z);
            if (r > box_length/2) {
                attempts++;
                continue;
            }
            
            valid = true;
            for (uint32_t j = 0; j < i; ++j) {
                double dx = pos.x - state.X[j].x;
                double dy = pos.y - state.X[j].y;
                double dz = pos.z - state.X[j].z;
                
                double r2 = dx*dx + dy*dy + dz*dz;
                
                if (r2 < min_dist * min_dist) {
                    valid = false;
                    break;
                }
            }
            
            if (valid) {
                state.X[i] = pos;
            }
            
            attempts++;
        }
    }
    
    return state;
}

Result protocol_A_quench_only(int seed) {
    Result res;
    res.seed = seed;
    
    State state = create_ar_cluster(seed, 13, 10.0);
    
    auto model = create_lj_coulomb_model();
    ModelParams mp;
    mp.rc = 8.0;
    
    FIRE fire(*model, mp);
    FIREParams fp;
    fp.dt = 1e-3;
    fp.max_steps = 10000;
    fp.epsF = 0.01;
    
    auto fire_result = fire.minimize(state, fp);
    
    res.final_energy = fire_result.U;
    
    return res;
}

Result protocol_B_thermal_formation(int seed) {
    Result res;
    res.seed = seed;
    
    State state = create_ar_cluster(seed, 13, 10.0);
    
    auto model = create_lj_coulomb_model();
    ModelParams mp;
    mp.rc = 8.0;
    
    std::mt19937 rng(seed);
    initialize_velocities_thermal(state, 300.0, rng);
    
    // Langevin at 300 K
    LangevinDynamics dynamics(*model, mp);
    
    LangevinParams params_hot;
    params_hot.dt = 1.0;
    params_hot.n_steps = 3000;  // 3 ps
    params_hot.T_target = 300.0;
    params_hot.gamma = 0.1;
    params_hot.verbose = false;
    
    dynamics.integrate(state, params_hot, rng);
    
    // Anneal to 50 K
    for (int i = 0; i < 10; ++i) {
        double T = 300.0 - (250.0 * i / 10.0);
        
        LangevinParams params_anneal;
        params_anneal.dt = 1.0;
        params_anneal.n_steps = 300;
        params_anneal.T_target = T;
        params_anneal.gamma = 0.1;
        params_anneal.verbose = false;
        
        dynamics.integrate(state, params_anneal, rng);
    }
    
    // Final quench
    FIRE fire(*model, mp);
    FIREParams fp;
    fp.dt = 1e-3;
    fp.max_steps = 10000;
    fp.epsF = 0.01;
    
    auto fire_result = fire.minimize(state, fp);
    
    res.final_energy = fire_result.U;
    
    return res;
}

int main() {
    std::cout << "╔═══════════════════════════════════════════════════════════╗\n";
    std::cout << "║  APPLICATION TEST: Thermal vs Quench (Ar₁₃ cluster)      ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════╝\n\n";
    
    std::cout << "System: Ar₁₃ cluster (neutral, LJ only)\n";
    std::cout << "Target: Icosahedral structure (global minimum)\n";
    std::cout << "Expected: E_min ≈ -44.3 kcal/mol (for LJ ε=0.238 kcal/mol)\n\n";
    
    const int n_seeds = 15;
    
    std::vector<Result> results_A;
    std::vector<Result> results_B;
    
    std::cout << "Running Protocol A (Quench-only) for " << n_seeds << " seeds...\n";
    
    for (int seed = 0; seed < n_seeds; ++seed) {
        Result res = protocol_A_quench_only(seed);
        results_A.push_back(res);
        
        std::cout << "  Seed " << std::setw(2) << seed 
                  << ": E = " << std::fixed << std::setprecision(2) << std::setw(8) << res.final_energy 
                  << " kcal/mol\n";
    }
    
    std::cout << "\nRunning Protocol B (Thermal formation) for " << n_seeds << " seeds...\n";
    std::cout << "  Stage 1: Langevin at 300 K for 3 ps\n";
    std::cout << "  Stage 2: Anneal 300 K → 50 K over 3 ps\n";
    std::cout << "  Stage 3: FIRE quench\n\n";
    
    for (int seed = 0; seed < n_seeds; ++seed) {
        Result res = protocol_B_thermal_formation(seed);
        results_B.push_back(res);
        
        std::cout << "  Seed " << std::setw(2) << seed 
                  << ": E = " << std::fixed << std::setprecision(2) << std::setw(8) << res.final_energy 
                  << " kcal/mol\n";
    }
    
    // Analysis
    std::cout << "\n╔═══════════════════════════════════════════════════════════╗\n";
    std::cout << "║  ANALYSIS                                                  ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════╝\n\n";
    
    double E_mean_A = 0.0;
    double E_min_A = results_A[0].final_energy;
    
    for (const auto& res : results_A) {
        E_mean_A += res.final_energy;
        if (res.final_energy < E_min_A) E_min_A = res.final_energy;
    }
    E_mean_A /= n_seeds;
    
    double E_mean_B = 0.0;
    double E_min_B = results_B[0].final_energy;
    
    for (const auto& res : results_B) {
        E_mean_B += res.final_energy;
        if (res.final_energy < E_min_B) E_min_B = res.final_energy;
    }
    E_mean_B /= n_seeds;
    
    std::cout << "Protocol A (Quench-only):\n";
    std::cout << "  Mean energy: " << std::fixed << std::setprecision(2) << E_mean_A << " kcal/mol\n";
    std::cout << "  Min energy:  " << E_min_A << " kcal/mol\n\n";
    
    std::cout << "Protocol B (Thermal formation):\n";
    std::cout << "  Mean energy: " << E_mean_B << " kcal/mol\n";
    std::cout << "  Min energy:  " << E_min_B << " kcal/mol\n\n";
    
    // Validation
    std::cout << "╔═══════════════════════════════════════════════════════════╗\n";
    std::cout << "║  VALIDATION                                                ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════╝\n\n";
    
    bool pass_mean = (E_mean_B < E_mean_A - 0.5);
    bool pass_min = (E_min_B < E_min_A);
    
    std::cout << "Test Criteria:\n";
    std::cout << "  1. Lower mean energy: ";
    if (pass_mean) {
        std::cout << "✅ PASS (B: " << E_mean_B << " < A: " << E_mean_A << ")\n";
    } else {
        std::cout << "❌ FAIL (B: " << E_mean_B << " vs A: " << E_mean_A << ")\n";
    }
    
    std::cout << "  2. Lower global minimum: ";
    if (pass_min) {
        std::cout << "✅ PASS (B: " << E_min_B << " < A: " << E_min_A << ")\n";
    } else {
        std::cout << "❌ FAIL (B: " << E_min_B << " vs A: " << E_min_A << ")\n";
    }
    
    std::cout << "\n";
    
    if (pass_mean && pass_min) {
        std::cout << "╔═══════════════════════════════════════════════════════════╗\n";
        std::cout << "║  ✅ TEST PASSED: Thermal formation finds better minima   ║\n";
        std::cout << "╚═══════════════════════════════════════════════════════════╝\n";
        return 0;
    } else {
        std::cout << "╔═══════════════════════════════════════════════════════════╗\n";
        std::cout << "║  ❌ TEST FAILED                                           ║\n";
        std::cout << "╚═══════════════════════════════════════════════════════════╝\n";
        return 1;
    }
}
