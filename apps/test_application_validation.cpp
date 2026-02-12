/**
 * APPLICATION VALIDATION SUITE - Formation Pipeline
 * 
 * Comprehensive tests for Section E of Formation Pipeline Checklist:
 * - E1: Emergence Test (10 independent runs)
 * - E2: Stability Test (heating cycles)
 * - E3: Parameter Sanity (5×5 T/ρ grid)
 * 
 * This test suite validates that the formation engine works correctly
 * across different conditions, seeds, and parameter regimes.
 * 
 * Pass criteria:
 * - E1: >80% of annealed runs beat quench (same seed)
 * - E2: System survives heating without crashes
 * - E3: All 25 parameter combinations complete successfully
 */

#include "atomistic/core/state.hpp"
#include "atomistic/core/maxwell_boltzmann.hpp"
#include "atomistic/models/model.hpp"
#include "atomistic/integrators/fire.hpp"
#include "atomistic/integrators/velocity_verlet.hpp"
#include "atomistic/analysis/rdf.hpp"
#include <iostream>
#include <iomanip>
#include <random>
#include <cmath>
#include <vector>
#include <algorithm>
#include <numeric>
#include <fstream>

using namespace atomistic;

// ============================================================================
// TEST CONFIGURATION
// ============================================================================

constexpr int N_EMERGENCE_RUNS = 10;
constexpr int N_GRID_POINTS = 5;

constexpr double Ar_mass = 39.948;
constexpr double Ar_sigma = 3.4;    // Å
constexpr double Ar_epsilon = 0.238; // kcal/mol

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

struct TestResult {
    int seed;
    double energy_quench;
    double energy_anneal;
    double rdf_peak_quench;
    double rdf_peak_anneal;
    double coord_quench;
    double coord_anneal;
    bool anneal_wins;
};

struct GridResult {
    double T;
    double rho;
    double msd;
    double rdf_peak;
    bool completed;
    std::string phase;
};

// Compute RDF peak height
double compute_rdf_peak(const State& state) {
    RDFCalculator rdf_calc;
    
    double r_max = 10.0;
    if (state.box.enabled) {
        r_max = std::min({state.box.L.x, state.box.L.y, state.box.L.z}) / 2.0;
    }
    
    auto rdf = rdf_calc.compute(state, r_max, 200);
    
    // Find first peak (between 0.8σ and 1.5σ)
    double peak_height = 0.0;
    for (size_t i = 0; i < rdf.r.size(); ++i) {
        if (rdf.r[i] > 0.8 * Ar_sigma && rdf.r[i] < 1.5 * Ar_sigma) {
            peak_height = std::max(peak_height, rdf.g[i]);
        }
    }
    
    return peak_height;
}

// Compute coordination number
double compute_coordination(const State& state, double cutoff = 4.0) {
    double total_coord = 0.0;
    
    for (uint32_t i = 0; i < state.N; ++i) {
        int coord = 0;
        
        for (uint32_t j = 0; j < state.N; ++j) {
            if (i == j) continue;
            
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
            
            if (r < cutoff) {
                coord++;
            }
        }
        
        total_coord += coord;
    }
    
    return total_coord / state.N;
}

// Compute mean squared displacement
double compute_msd(const State& state_initial, const State& state_final) {
    double msd = 0.0;
    
    for (uint32_t i = 0; i < state_initial.N; ++i) {
        double dx = state_final.X[i].x - state_initial.X[i].x;
        double dy = state_final.X[i].y - state_initial.X[i].y;
        double dz = state_final.X[i].z - state_initial.X[i].z;
        
        msd += dx*dx + dy*dy + dz*dz;
    }
    
    return msd / state_initial.N;
}

// Create Ar system
State create_ar_system(int N, double box_length, int seed) {
    State state;
    state.N = N;
    
    state.box.enabled = true;
    state.box.L = {box_length, box_length, box_length};
    state.box.invL = {1.0/box_length, 1.0/box_length, 1.0/box_length};
    
    state.X.resize(N);
    state.V.resize(N, {0, 0, 0});
    state.F.resize(N, {0, 0, 0});
    state.M.resize(N, Ar_mass);
    state.Q.resize(N, 0.0);
    state.type.resize(N, 18);
    
    // Random positions
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> dist(0.0, box_length);
    
    double min_dist = 2.5;
    
    for (uint32_t i = 0; i < state.N; ++i) {
        bool valid = false;
        int attempts = 0;
        
        while (!valid && attempts < 1000) {
            Vec3 pos = {dist(rng), dist(rng), dist(rng)};
            
            valid = true;
            for (uint32_t j = 0; j < i; ++j) {
                double dx = pos.x - state.X[j].x;
                double dy = pos.y - state.X[j].y;
                double dz = pos.z - state.X[j].z;
                
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
            std::cerr << "WARNING: Could not place atom " << i << " without overlap\n";
        }
    }
    
    return state;
}

// ============================================================================
// E1: EMERGENCE TEST (10 independent runs)
// ============================================================================

TestResult run_emergence_test(int seed) {
    TestResult result;
    result.seed = seed;
    
    const int N = 32;
    const double box_length = 12.0;
    
    auto model = create_lj_coulomb_model();
    ModelParams mp;
    mp.rc = 6.0;
    
    std::mt19937 rng(seed);
    
    // ========================================================================
    // PROTOCOL A: Quench Only
    // ========================================================================
    
    State state_quench = create_ar_system(N, box_length, seed);
    
    FIRE fire_quench(*model, mp);
    FIREParams fp;
    fp.dt = 1e-3;
    fp.max_steps = 5000;
    fp.epsF = 0.01;
    fp.verbose = false;
    
    auto fire_result_quench = fire_quench.minimize(state_quench, fp);
    
    result.energy_quench = fire_result_quench.U;
    result.rdf_peak_quench = compute_rdf_peak(state_quench);
    result.coord_quench = compute_coordination(state_quench);
    
    // ========================================================================
    // PROTOCOL B: Thermal Annealing + Quench
    // ========================================================================
    
    State state_anneal = create_ar_system(N, box_length, seed);
    
    // Initialize velocities
    initialize_velocities_thermal(state_anneal, 300.0, rng);
    
    // Langevin at 300K (equilibration)
    LangevinDynamics dynamics(*model, mp);
    
    LangevinParams params_eq;
    params_eq.dt = 1.0;
    params_eq.n_steps = 2000;
    params_eq.T_target = 300.0;
    params_eq.gamma = 0.1;
    params_eq.verbose = false;
    
    dynamics.integrate(state_anneal, params_eq, rng);
    
    // Anneal to 50K
    for (int i = 0; i < 10; ++i) {
        double T = 300.0 - (250.0 * i / 10.0);
        
        LangevinParams params_anneal;
        params_anneal.dt = 1.0;
        params_anneal.n_steps = 200;
        params_anneal.T_target = T;
        params_anneal.gamma = 0.1;
        params_anneal.verbose = false;
        
        dynamics.integrate(state_anneal, params_anneal, rng);
    }
    
    // Final quench
    FIRE fire_anneal(*model, mp);
    auto fire_result_anneal = fire_anneal.minimize(state_anneal, fp);
    
    result.energy_anneal = fire_result_anneal.U;
    result.rdf_peak_anneal = compute_rdf_peak(state_anneal);
    result.coord_anneal = compute_coordination(state_anneal);
    
    // Determine winner
    result.anneal_wins = (result.energy_anneal < result.energy_quench);
    
    return result;
}

void test_E1_emergence() {
    std::cout << "\n╔═══════════════════════════════════════════════════════════╗\n";
    std::cout << "║  E1: EMERGENCE TEST (10 independent runs)                 ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════╝\n\n";
    
    std::vector<TestResult> results;
    
    for (int i = 0; i < N_EMERGENCE_RUNS; ++i) {
        std::cout << "Run " << (i+1) << "/" << N_EMERGENCE_RUNS << " (seed=" << (1000+i) << ")..." << std::flush;
        
        TestResult result = run_emergence_test(1000 + i);
        results.push_back(result);
        
        std::cout << " E_quench=" << std::fixed << std::setprecision(2) << result.energy_quench
                  << " E_anneal=" << result.energy_anneal
                  << " " << (result.anneal_wins ? "✓" : "✗") << "\n";
    }
    
    // Analyze results
    std::cout << "\n--- ANALYSIS ---\n";
    
    double avg_E_quench = 0.0, avg_E_anneal = 0.0;
    double avg_rdf_quench = 0.0, avg_rdf_anneal = 0.0;
    double avg_coord_quench = 0.0, avg_coord_anneal = 0.0;
    int anneal_wins = 0;
    
    for (const auto& r : results) {
        avg_E_quench += r.energy_quench;
        avg_E_anneal += r.energy_anneal;
        avg_rdf_quench += r.rdf_peak_quench;
        avg_rdf_anneal += r.rdf_peak_anneal;
        avg_coord_quench += r.coord_quench;
        avg_coord_anneal += r.coord_anneal;
        if (r.anneal_wins) anneal_wins++;
    }
    
    avg_E_quench /= N_EMERGENCE_RUNS;
    avg_E_anneal /= N_EMERGENCE_RUNS;
    avg_rdf_quench /= N_EMERGENCE_RUNS;
    avg_rdf_anneal /= N_EMERGENCE_RUNS;
    avg_coord_quench /= N_EMERGENCE_RUNS;
    avg_coord_anneal /= N_EMERGENCE_RUNS;
    
    double success_rate = 100.0 * anneal_wins / N_EMERGENCE_RUNS;
    
    std::cout << "\nAverage Energy:\n";
    std::cout << "  Quench:  " << std::fixed << std::setprecision(3) << avg_E_quench << " kcal/mol\n";
    std::cout << "  Anneal:  " << avg_E_anneal << " kcal/mol\n";
    std::cout << "  Delta:   " << (avg_E_anneal - avg_E_quench) << " kcal/mol\n";
    
    std::cout << "\nAverage RDF Peak:\n";
    std::cout << "  Quench:  " << std::setprecision(2) << avg_rdf_quench << "\n";
    std::cout << "  Anneal:  " << avg_rdf_anneal << "\n";
    std::cout << "  Improvement: " << std::setprecision(1) 
              << (100.0 * (avg_rdf_anneal - avg_rdf_quench) / avg_rdf_quench) << "%\n";
    
    std::cout << "\nAverage Coordination:\n";
    std::cout << "  Quench:  " << std::setprecision(2) << avg_coord_quench << "\n";
    std::cout << "  Anneal:  " << avg_coord_anneal << "\n";
    
    std::cout << "\nSuccess Rate: " << anneal_wins << "/" << N_EMERGENCE_RUNS 
              << " (" << std::setprecision(1) << success_rate << "%)\n";
    
    // Pass criteria
    std::cout << "\n--- VALIDATION ---\n";
    
    bool test_energy = (avg_E_anneal < avg_E_quench);
    bool test_order = ((avg_rdf_anneal - avg_rdf_quench) / avg_rdf_quench > 0.10);
    bool test_consistency = (success_rate >= 80.0);
    
    std::cout << (test_energy ? "✅" : "❌") << " Annealed Energy < Quench Energy\n";
    std::cout << (test_order ? "✅" : "❌") << " RDF Peak Height Improved > 10%\n";
    std::cout << (test_consistency ? "✅" : "❌") << " Consistency >= 80%\n";
    
    if (test_energy && test_order && test_consistency) {
        std::cout << "\n✅ E1: EMERGENCE TEST PASSED\n";
    } else {
        std::cout << "\n❌ E1: EMERGENCE TEST FAILED\n";
    }
}

// ============================================================================
// E2: STABILITY TEST (heating cycles)
// ============================================================================

void test_E2_stability() {
    std::cout << "\n╔═══════════════════════════════════════════════════════════╗\n";
    std::cout << "║  E2: STABILITY TEST (heating cycle)                       ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════╝\n\n";
    
    const int N = 64;
    const double box_length = 15.0;
    const int seed = 42;
    
    State state = create_ar_system(N, box_length, seed);
    
    auto model = create_lj_coulomb_model();
    ModelParams mp;
    mp.rc = 7.0;
    
    std::mt19937 rng(seed);
    
    // Initial equilibration at 300K
    std::cout << "Equilibrating at 300K..." << std::flush;
    initialize_velocities_thermal(state, 300.0, rng);
    
    LangevinDynamics dynamics(*model, mp);
    
    LangevinParams params_eq;
    params_eq.dt = 1.0;
    params_eq.n_steps = 3000;
    params_eq.T_target = 300.0;
    params_eq.gamma = 0.1;
    params_eq.verbose = false;
    
    dynamics.integrate(state, params_eq, rng);
    
    double coord_initial = compute_coordination(state);
    std::cout << " Coord=" << std::fixed << std::setprecision(2) << coord_initial << "\n";
    
    // Heat to 600K
    std::cout << "Heating to 600K..." << std::flush;
    
    LangevinParams params_heat;
    params_heat.dt = 1.0;
    params_heat.n_steps = 3000;
    params_heat.T_target = 600.0;
    params_heat.gamma = 0.1;
    params_heat.verbose = false;
    
    try {
        dynamics.integrate(state, params_heat, rng);
        std::cout << " ✓ (no crash)\n";
    } catch (const std::exception& e) {
        std::cout << " ❌ CRASH: " << e.what() << "\n";
        std::cout << "\n❌ E2: STABILITY TEST FAILED (crash during heating)\n";
        return;
    }
    
    double coord_hot = compute_coordination(state);
    std::cout << "  Coord at 600K: " << coord_hot << "\n";
    
    // Cool back to 300K
    std::cout << "Cooling to 300K..." << std::flush;
    
    LangevinParams params_cool;
    params_cool.dt = 1.0;
    params_cool.n_steps = 3000;
    params_cool.T_target = 300.0;
    params_cool.gamma = 0.1;
    params_cool.verbose = false;
    
    dynamics.integrate(state, params_cool, rng);
    
    double coord_final = compute_coordination(state);
    std::cout << " Coord=" << coord_final << "\n";
    
    // Validation
    std::cout << "\n--- VALIDATION ---\n";
    
    bool test_survives = true;  // Didn't crash
    bool test_order_decreases = (coord_hot < 0.8 * coord_initial);
    bool test_partial_recovery = (coord_final >= 0.6 * coord_initial);
    
    std::cout << (test_survives ? "✅" : "❌") << " Survives Heating (no crash)\n";
    std::cout << (test_order_decreases ? "✅" : "❌") 
              << " Order Decreases (Coord_600K < 0.8 × Coord_300K): "
              << coord_hot << " < " << (0.8 * coord_initial) << "\n";
    std::cout << (test_partial_recovery ? "✅" : "❌") 
              << " Partial Recovery (Coord_final >= 0.6 × Coord_initial): "
              << coord_final << " >= " << (0.6 * coord_initial) << "\n";
    
    if (test_survives && test_order_decreases && test_partial_recovery) {
        std::cout << "\n✅ E2: STABILITY TEST PASSED\n";
    } else {
        std::cout << "\n❌ E2: STABILITY TEST FAILED\n";
    }
}

// ============================================================================
// E3: PARAMETER SANITY (5×5 T/ρ grid)
// ============================================================================

void test_E3_parameter_grid() {
    std::cout << "\n╔═══════════════════════════════════════════════════════════╗\n";
    std::cout << "║  E3: PARAMETER SANITY (5×5 T/ρ grid)                      ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════╝\n\n";
    
    const int N = 64;
    std::vector<double> temperatures = {50.0, 100.0, 200.0, 400.0, 800.0};
    std::vector<double> densities = {0.2, 0.4, 0.6, 0.8, 1.0};  // reduced units (ρσ³)
    
    std::vector<GridResult> results;
    
    int completed = 0;
    int gas_points = 0, liquid_points = 0, solid_points = 0;
    
    for (double T : temperatures) {
        for (double rho : densities) {
            // Calculate box length from density
            double volume = N / (rho / (Ar_sigma * Ar_sigma * Ar_sigma));
            double box_length = std::cbrt(volume);
            
            std::cout << "T=" << std::fixed << std::setprecision(0) << T 
                      << "K, ρ*=" << std::setprecision(2) << rho << " ... " << std::flush;
            
            GridResult gr;
            gr.T = T;
            gr.rho = rho;
            gr.completed = false;
            
            try {
                State state = create_ar_system(N, box_length, 42);
                State state_initial = state;  // Save for MSD
                
                auto model = create_lj_coulomb_model();
                ModelParams mp;
                mp.rc = std::min(box_length / 2.0, 10.0);
                
                std::mt19937 rng(42);
                initialize_velocities_thermal(state, T, rng);
                
                LangevinDynamics dynamics(*model, mp);
                
                LangevinParams params;
                params.dt = 1.0;
                params.n_steps = 2000;
                params.T_target = T;
                params.gamma = 0.1;
                params.verbose = false;
                
                dynamics.integrate(state, params, rng);
                
                gr.msd = compute_msd(state_initial, state);
                gr.rdf_peak = compute_rdf_peak(state);
                gr.completed = true;
                completed++;
                
                // Classify phase
                double msd_threshold_low = Ar_sigma * Ar_sigma;
                double msd_threshold_high = 10.0 * Ar_sigma * Ar_sigma;
                double rdf_threshold_low = 1.5;
                double rdf_threshold_high = 3.0;
                
                if (gr.msd > msd_threshold_high && gr.rdf_peak < rdf_threshold_low) {
                    gr.phase = "GAS";
                    gas_points++;
                } else if (gr.msd < msd_threshold_low && gr.rdf_peak > rdf_threshold_high) {
                    gr.phase = "SOLID";
                    solid_points++;
                } else {
                    gr.phase = "LIQUID";
                    liquid_points++;
                }
                
                std::cout << gr.phase << " (MSD=" << std::setprecision(1) << gr.msd 
                          << ", RDF=" << std::setprecision(2) << gr.rdf_peak << ")\n";
                
            } catch (const std::exception& e) {
                std::cout << "❌ FAILED: " << e.what() << "\n";
                gr.completed = false;
            }
            
            results.push_back(gr);
        }
    }
    
    std::cout << "\n--- PHASE DIAGRAM SUMMARY ---\n";
    std::cout << "Gas points:    " << gas_points << "\n";
    std::cout << "Liquid points: " << liquid_points << "\n";
    std::cout << "Solid points:  " << solid_points << "\n";
    std::cout << "Completed:     " << completed << "/25\n";
    
    // Validation
    std::cout << "\n--- VALIDATION ---\n";
    
    bool test_gas = (gas_points > 0);
    bool test_liquid = (liquid_points > 0);
    bool test_solid = (solid_points > 0);
    bool test_complete = (completed == 25);
    
    std::cout << (test_gas ? "✅" : "❌") << " Gas Region Identified\n";
    std::cout << (test_liquid ? "✅" : "❌") << " Liquid Region Identified\n";
    std::cout << (test_solid ? "✅" : "❌") << " Solid Region Identified\n";
    std::cout << (test_complete ? "✅" : "❌") << " Grid Completeness (25/25)\n";
    
    if (test_gas && test_liquid && test_solid && test_complete) {
        std::cout << "\n✅ E3: PARAMETER SANITY TEST PASSED\n";
    } else {
        std::cout << "\n❌ E3: PARAMETER SANITY TEST FAILED\n";
    }
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char** argv) {
    std::cout << "╔═══════════════════════════════════════════════════════════╗\n";
    std::cout << "║  APPLICATION VALIDATION SUITE - Formation Pipeline        ║\n";
    std::cout << "║  Section E: Application Tests (Target: ~99% Pass Rate)    ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════╝\n";
    
    // Run all tests
    test_E1_emergence();
    test_E2_stability();
    test_E3_parameter_grid();
    
    std::cout << "\n╔═══════════════════════════════════════════════════════════╗\n";
    std::cout << "║  APPLICATION VALIDATION COMPLETE                          ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════╝\n\n";
    
    return 0;
}
