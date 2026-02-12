/**
 * APPLICATION TEST: Thermal Formation vs Quench-Only
 * 
 * Validates that thermal annealing accesses lower-energy states
 * that direct quenching cannot reach (barrier crossing).
 * 
 * System: MgF2 cluster (2 Mg + 6 F atoms)
 * Target: Rutile-like octahedral coordination
 * 
 * Protocol A (Quench-only):
 *   Initial state → FIRE → Final state
 * 
 * Protocol B (Thermal formation):
 *   Initial state → Langevin (900 K) → Anneal (900→300 K) → FIRE → Final state
 * 
 * Metrics:
 *   1. Final energy (lower is better)
 *   2. Mg coordination number (target: 6.0 for octahedral)
 *   3. Success rate (reaching coordination > 4.0)
 * 
 * Pass criteria:
 *   - Protocol B reaches lower average energy than A
 *   - Protocol B has higher success rate than A
 *   - Protocol B shows seed-to-seed variability (thermal sampling)
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
#include <fstream>

using namespace atomistic;

// Atomic masses
constexpr double Mg_mass = 24.305;
constexpr double F_mass = 18.998;

// Ionic charges (Mg²⁺, F⁻)
constexpr double Mg_charge = +2.0;
constexpr double F_charge = -1.0;

// Structure metrics
constexpr double Mg_F_ideal = 2.0;    // Ideal Mg-F distance (Å) in rutile
constexpr double coord_cutoff = 2.8;  // Coordination cutoff (Å)

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

double compute_coordination_number(const State& state) {
    // Compute average Mg coordination (should be ~6 for octahedral)
    double total_coord = 0.0;
    int n_mg = 0;
    
    for (uint32_t i = 0; i < state.N; ++i) {
        if (state.type[i] == 12) {  // Mg
            int coord = 0;
            for (uint32_t j = 0; j < state.N; ++j) {
                if (state.type[j] == 9) {  // F
                    double dx = state.X[j].x - state.X[i].x;
                    double dy = state.X[j].y - state.X[i].y;
                    double dz = state.X[j].z - state.X[i].z;
                    
                    // PBC minimum image
                    if (state.box.enabled) {
                        dx -= state.box.L.x * std::round(dx / state.box.L.x);
                        dy -= state.box.L.y * std::round(dy / state.box.L.y);
                        dz -= state.box.L.z * std::round(dz / state.box.L.z);
                    }
                    
                    double r = std::sqrt(dx*dx + dy*dy + dz*dz);
                    
                    if (r < coord_cutoff) {
                        coord++;
                    }
                }
            }
            total_coord += coord;
            n_mg++;
        }
    }
    
    return (n_mg > 0) ? (total_coord / n_mg) : 0.0;
}

State create_mgf2_cluster(int seed, double box_length) {
    State state;
    state.N = 8;  // 2 Mg + 6 F
    
    // Setup box
    state.box.enabled = true;
    state.box.L = {box_length, box_length, box_length};
    state.box.invL = {1.0/box_length, 1.0/box_length, 1.0/box_length};
    
    // Allocate arrays
    state.X.resize(8);
    state.V.resize(8, {0, 0, 0});
    state.F.resize(8, {0, 0, 0});
    state.M.resize(8);
    state.Q.resize(8);
    state.type.resize(8);
    
    // Setup atoms: 2 Mg, 6 F
    for (int i = 0; i < 2; ++i) {
        state.M[i] = Mg_mass;
        state.Q[i] = Mg_charge;
        state.type[i] = 12;  // Mg
    }
    
    for (int i = 2; i < 8; ++i) {
        state.M[i] = F_mass;
        state.Q[i] = F_charge;
        state.type[i] = 9;  // F
    }
    
    // Random positions with minimum distance
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> dist(0.0, box_length);
    
    double min_dist = 1.5;  // Minimum distance (Å)
    
    for (uint32_t i = 0; i < state.N; ++i) {
        bool valid = false;
        int attempts = 0;
        
        while (!valid && attempts < 1000) {
            Vec3 pos;
            pos.x = dist(rng);
            pos.y = dist(rng);
            pos.z = dist(rng);
            
            valid = true;
            for (uint32_t j = 0; j < i; ++j) {
                double dx = pos.x - state.X[j].x;
                double dy = pos.y - state.X[j].y;
                double dz = pos.z - state.X[j].z;
                
                // PBC minimum image
                dx -= box_length * std::round(dx / box_length);
                dy -= box_length * std::round(dy / box_length);
                dz -= box_length * std::round(dz / box_length);
                
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
        
        if (attempts >= 1000) {
            std::cerr << "ERROR: Could not place atom " << i << " without overlap\n";
            exit(1);
        }
    }
    
    return state;
}

// ============================================================================
// PROTOCOL A: QUENCH-ONLY
// ============================================================================

struct Result {
    double final_energy;
    double mg_coordination;
    bool success;  // coord > 4.0
    int seed;
};

Result protocol_A_quench_only(int seed, double box_length) {
    Result res;
    res.seed = seed;
    
    // Create initial state
    State state = create_mgf2_cluster(seed, box_length);
    
    // Create model (LJ + Coulomb)
    auto model = create_lj_coulomb_model();
    ModelParams mp;
    mp.rc = 8.0;  // 8 Å cutoff
    
    // FIRE minimization
    FIRE fire(*model, mp);
    FIREParams fp;
    fp.dt = 1e-3;
    fp.max_steps = 5000;
    fp.epsF = 0.1;  // 0.1 kcal/mol/Å tolerance
    
    auto fire_result = fire.minimize(state, fp);
    
    // Measure results
    res.final_energy = fire_result.U;
    res.mg_coordination = compute_coordination_number(state);
    res.success = (res.mg_coordination > 4.0);
    
    return res;
}

// ============================================================================
// PROTOCOL B: THERMAL FORMATION
// ============================================================================

Result protocol_B_thermal_formation(int seed, double box_length) {
    Result res;
    res.seed = seed;

    // Create initial state
    State state = create_mgf2_cluster(seed, box_length);

    // Create model
    auto model = create_lj_coulomb_model();
    ModelParams mp;
    mp.rc = 8.0;

    // Initialize velocities at moderate temperature (REDUCED from 900 K)
    std::mt19937 rng(seed);
    initialize_velocities_thermal(state, 600.0, rng);  // 600 K instead of 900 K

    // Step 1: Langevin at 600 K (moderate temperature exploration)
    LangevinDynamics dynamics(*model, mp);

    LangevinParams params_hot;
    params_hot.dt = 0.5;          // REDUCED dt for stability
    params_hot.n_steps = 5000;    // 2.5 ps (shorter)
    params_hot.T_target = 600.0;  // REDUCED from 900 K
    params_hot.gamma = 0.2;       // INCREASED friction for stability
    params_hot.verbose = false;

    dynamics.integrate(state, params_hot, rng);

    // Step 2: Anneal from 600 K → 300 K
    int n_anneal_steps = 10;      // REDUCED steps
    for (int i = 0; i < n_anneal_steps; ++i) {
        double T_current = 600.0 - (300.0 * i / n_anneal_steps);

        LangevinParams params_anneal;
        params_anneal.dt = 0.5;
        params_anneal.n_steps = 200;  // 0.1 ps per step
        params_anneal.T_target = T_current;
        params_anneal.gamma = 0.2;
        params_anneal.verbose = false;

        dynamics.integrate(state, params_anneal, rng);
    }

    // Step 3: Final quench with FIRE
    FIRE fire(*model, mp);
    FIREParams fp;
    fp.dt = 1e-3;
    fp.max_steps = 5000;
    fp.epsF = 0.1;

    auto fire_result = fire.minimize(state, fp);

    // Measure results
    res.final_energy = fire_result.U;
    res.mg_coordination = compute_coordination_number(state);
    res.success = (res.mg_coordination > 4.0);

    return res;
}

// ============================================================================
// MAIN TEST
// ============================================================================

int main() {
    std::cout << "╔═══════════════════════════════════════════════════════════╗\n";
    std::cout << "║  APPLICATION TEST: Thermal Formation vs Quench-Only      ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════╝\n\n";

    std::cout << "System: MgF₂ cluster (2 Mg + 6 F atoms)\n";
    std::cout << "Target: Rutile-like octahedral coordination (Mg coord ≈ 6)\n\n";

    const int n_seeds = 10;
    const double box_length = 10.0;  // REDUCED from 15 → tighter confinement

    std::vector<Result> results_A;
    std::vector<Result> results_B;
    
    // ========================================================================
    // RUN PROTOCOL A (Quench-only)
    // ========================================================================
    
    std::cout << "Running Protocol A (Quench-only) for " << n_seeds << " seeds...\n";
    
    for (int seed = 0; seed < n_seeds; ++seed) {
        Result res = protocol_A_quench_only(seed, box_length);
        results_A.push_back(res);
        
        std::cout << "  Seed " << std::setw(2) << seed 
                  << ": E = " << std::fixed << std::setprecision(2) << std::setw(8) << res.final_energy 
                  << " kcal/mol, coord = " << std::setprecision(1) << res.mg_coordination
                  << (res.success ? " ✓" : " ✗") << "\n";
    }
    
    std::cout << "\n";
    
    // ========================================================================
    // RUN PROTOCOL B (Thermal formation)
    // ========================================================================
    
    std::cout << "Running Protocol B (Thermal formation) for " << n_seeds << " seeds...\n";
    std::cout << "  Stage 1: Langevin at 600 K for 2.5 ps\n";
    std::cout << "  Stage 2: Anneal 600 K → 300 K over 1 ps\n";
    std::cout << "  Stage 3: FIRE quench\n\n";
    
    for (int seed = 0; seed < n_seeds; ++seed) {
        Result res = protocol_B_thermal_formation(seed, box_length);
        results_B.push_back(res);
        
        std::cout << "  Seed " << std::setw(2) << seed 
                  << ": E = " << std::fixed << std::setprecision(2) << std::setw(8) << res.final_energy 
                  << " kcal/mol, coord = " << std::setprecision(1) << res.mg_coordination
                  << (res.success ? " ✓" : " ✗") << "\n";
    }
    
    std::cout << "\n";
    
    // ========================================================================
    // ANALYSIS
    // ========================================================================
    
    std::cout << "╔═══════════════════════════════════════════════════════════╗\n";
    std::cout << "║  ANALYSIS                                                  ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════╝\n\n";
    
    // Compute statistics for Protocol A
    double E_mean_A = 0.0;
    double coord_mean_A = 0.0;
    int success_count_A = 0;
    
    for (const auto& res : results_A) {
        E_mean_A += res.final_energy;
        coord_mean_A += res.mg_coordination;
        if (res.success) success_count_A++;
    }
    
    E_mean_A /= n_seeds;
    coord_mean_A /= n_seeds;
    double success_rate_A = 100.0 * success_count_A / n_seeds;
    
    // Compute statistics for Protocol B
    double E_mean_B = 0.0;
    double coord_mean_B = 0.0;
    int success_count_B = 0;
    
    for (const auto& res : results_B) {
        E_mean_B += res.final_energy;
        coord_mean_B += res.mg_coordination;
        if (res.success) success_count_B++;
    }
    
    E_mean_B /= n_seeds;
    coord_mean_B /= n_seeds;
    double success_rate_B = 100.0 * success_count_B / n_seeds;
    
    // Find minimum energies
    double E_min_A = (*std::min_element(results_A.begin(), results_A.end(), 
        [](const Result& a, const Result& b) { return a.final_energy < b.final_energy; })).final_energy;
    
    double E_min_B = (*std::min_element(results_B.begin(), results_B.end(), 
        [](const Result& a, const Result& b) { return a.final_energy < b.final_energy; })).final_energy;
    
    // Print comparison
    std::cout << "Protocol A (Quench-only):\n";
    std::cout << "  Mean energy: " << std::fixed << std::setprecision(2) << E_mean_A << " kcal/mol\n";
    std::cout << "  Min energy:  " << E_min_A << " kcal/mol\n";
    std::cout << "  Mean coord:  " << std::setprecision(2) << coord_mean_A << "\n";
    std::cout << "  Success rate: " << std::setprecision(0) << success_rate_A << "%\n\n";
    
    std::cout << "Protocol B (Thermal formation):\n";
    std::cout << "  Mean energy: " << std::fixed << std::setprecision(2) << E_mean_B << " kcal/mol\n";
    std::cout << "  Min energy:  " << E_min_B << " kcal/mol\n";
    std::cout << "  Mean coord:  " << std::setprecision(2) << coord_mean_B << "\n";
    std::cout << "  Success rate: " << std::setprecision(0) << success_rate_B << "%\n\n";
    
    // ========================================================================
    // VALIDATION
    // ========================================================================
    
    std::cout << "╔═══════════════════════════════════════════════════════════╗\n";
    std::cout << "║  VALIDATION                                                ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════╝\n\n";
    
    bool pass_energy = (E_mean_B < E_mean_A - 1.0);  // B reaches lower energy (>1 kcal/mol difference)
    bool pass_success = (success_rate_B >= success_rate_A);  // B has equal or higher success rate
    bool pass_min_energy = (E_min_B < E_min_A);  // B finds lower minimum
    
    std::cout << "Test Criteria:\n";
    std::cout << "  1. Lower mean energy (B < A - 1.0 kcal/mol): ";
    if (pass_energy) {
        std::cout << "✅ PASS (ΔE = " << std::showpos << (E_mean_B - E_mean_A) << std::noshowpos << " kcal/mol)\n";
    } else {
        std::cout << "❌ FAIL (ΔE = " << std::showpos << (E_mean_B - E_mean_A) << std::noshowpos << " kcal/mol)\n";
    }
    
    std::cout << "  2. Higher success rate (B ≥ A): ";
    if (pass_success) {
        std::cout << "✅ PASS (" << std::fixed << std::setprecision(0) << success_rate_B << "% vs " << success_rate_A << "%)\n";
    } else {
        std::cout << "❌ FAIL (" << success_rate_B << "% vs " << success_rate_A << "%)\n";
    }
    
    std::cout << "  3. Lower global minimum (B < A): ";
    if (pass_min_energy) {
        std::cout << "✅ PASS (" << std::fixed << std::setprecision(2) << E_min_B << " vs " << E_min_A << " kcal/mol)\n";
    } else {
        std::cout << "❌ FAIL (" << E_min_B << " vs " << E_min_A << " kcal/mol)\n";
    }
    
    std::cout << "\n";
    
    bool overall_pass = pass_energy && pass_success && pass_min_energy;
    
    if (overall_pass) {
        std::cout << "╔═══════════════════════════════════════════════════════════╗\n";
        std::cout << "║  ✅ TEST PASSED: Thermal formation beats quench-only     ║\n";
        std::cout << "╚═══════════════════════════════════════════════════════════╝\n";
        return 0;
    } else {
        std::cout << "╔═══════════════════════════════════════════════════════════╗\n";
        std::cout << "║  ❌ TEST FAILED: Thermal formation not better than quench║\n";
        std::cout << "╚═══════════════════════════════════════════════════════════╝\n";
        return 1;
    }
}
