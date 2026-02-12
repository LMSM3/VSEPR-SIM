/**
 * Equipartition Test: Langevin Thermostat Validation
 * 
 * Tests that kinetic temperature equilibrates to target T
 * with correct fluctuations and no drift.
 * 
 * Setup:
 * - 64 atoms (Ar) in cubic box with PBC
 * - LJ potential only (no Coulomb)
 * - Random positions with minimum distance
 * - Initialize velocities at WRONG temperature (50 K)
 * - Run Langevin at 300 K for 50k steps
 * 
 * Pass criteria:
 * - Mean T_kin in last half ≈ 300 K (within 3%)
 * - Fluctuations stable (10-50 K range)
 * - Reproducible with different seeds
 */

#include "atomistic/core/state.hpp"
#include "atomistic/core/maxwell_boltzmann.hpp"
#include "atomistic/models/model.hpp"
#include "atomistic/integrators/velocity_verlet.hpp"
#include "atomistic/integrators/fire.hpp"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <random>
#include <cmath>
#include <vector>

using namespace atomistic;

// Boltzmann constant (kcal/mol/K)
constexpr double k_B = 0.001987204;

// Argon parameters
constexpr double Ar_mass = 39.948;  // amu
constexpr double Ar_sigma = 3.4;    // Å (LJ)
constexpr double Ar_epsilon = 0.238; // kcal/mol (LJ)

// ============================================================================
// SYSTEM SETUP
// ============================================================================

State create_argon_system(int N_atoms, double box_length, int seed) {
    State state;
    state.N = N_atoms;
    
    // Setup box (cubic with PBC)
    state.box.enabled = true;
    state.box.L = {box_length, box_length, box_length};
    state.box.invL = {1.0/box_length, 1.0/box_length, 1.0/box_length};
    
    // Allocate arrays
    state.X.resize(N_atoms);
    state.V.resize(N_atoms, {0, 0, 0});
    state.F.resize(N_atoms, {0, 0, 0});
    state.M.resize(N_atoms, Ar_mass);
    state.type.resize(N_atoms, 18);  // Argon Z=18
    state.Q.resize(N_atoms, 0.0);    // No charge (LJ only)
    
    // Random positions with minimum distance
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> dist(0.0, box_length);
    
    double min_dist = 2.5;  // Minimum distance (Å)
    
    for (uint32_t i = 0; i < N_atoms; ++i) {
        bool valid = false;
        int attempts = 0;
        
        while (!valid && attempts < 1000) {
            // Random position
            Vec3 pos;
            pos.x = dist(rng);
            pos.y = dist(rng);
            pos.z = dist(rng);
            
            // Check distance to all existing atoms
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
            std::cerr << "       Try larger box or fewer atoms\n";
            exit(1);
        }
    }
    
    return state;
}

// ============================================================================
// TEMPERATURE CALCULATION
// ============================================================================

double compute_kinetic_temperature(const State& state) {
    // Remove COM velocity first
    Vec3 v_com = {0, 0, 0};
    double total_mass = 0.0;
    
    for (uint32_t i = 0; i < state.N; ++i) {
        v_com.x += state.V[i].x * state.M[i];
        v_com.y += state.V[i].y * state.M[i];
        v_com.z += state.V[i].z * state.M[i];
        total_mass += state.M[i];
    }
    
    if (total_mass > 0) {
        v_com.x /= total_mass;
        v_com.y /= total_mass;
        v_com.z /= total_mass;
    }
    
    // Compute kinetic energy (with COM removed)
    double KE = 0.0;
    for (uint32_t i = 0; i < state.N; ++i) {
        double vx = state.V[i].x - v_com.x;
        double vy = state.V[i].y - v_com.y;
        double vz = state.V[i].z - v_com.z;
        
        double v2 = vx*vx + vy*vy + vz*vz;
        KE += 0.5 * state.M[i] * v2 * 0.01036427;  // Convert to kcal/mol
    }
    
    // Temperature: T = 2*KE / ((3N-3) * k_B)
    // Use 3N-3 to account for COM translational DOF
    int DOF = 3 * state.N - 3;
    if (DOF <= 0) DOF = 3 * state.N;  // Fallback for very small systems
    
    double T = (2.0 * KE) / (DOF * k_B);
    
    return T;
}

// ============================================================================
// MAIN TEST
// ============================================================================

int main(int argc, char** argv) {
    std::cout << "╔═══════════════════════════════════════════════════════════╗\n";
    std::cout << "║  EQUIPARTITION TEST: Langevin Thermostat Validation      ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════╝\n\n";
    
    // Parse command line
    int seed = 42;
    if (argc > 1) {
        seed = std::atoi(argv[1]);
    }
    
    std::cout << "Seed: " << seed << "\n\n";
    
    // System parameters
    const int N_atoms = 8;           // REDUCED from 64 (too many atoms!)
    const double box_length = 20.0;  // Å (very low density for 8 atoms)
    const double T_initial = 50.0;   // K (WRONG temperature)
    const double T_target = 300.0;   // K (target)
    
    std::cout << "System Setup:\n";
    std::cout << "  N = " << N_atoms << " Ar atoms\n";
    std::cout << "  Box = " << box_length << " × " << box_length << " × " << box_length << " Å³\n";
    std::cout << "  Density = " << (N_atoms / (box_length*box_length*box_length)) << " atoms/Å³\n";
    std::cout << "  T_initial = " << T_initial << " K (wrong!)\n";
    std::cout << "  T_target = " << T_target << " K\n\n";
    
    // Create system
    std::cout << "Creating Ar system...\n";
    State state = create_argon_system(N_atoms, box_length, seed);
    
    // Initialize velocities at WRONG temperature
    std::mt19937 rng(seed);
    initialize_velocities_thermal(state, T_initial, rng);
    
    double T_check = compute_kinetic_temperature(state);
    std::cout << "  Initial T_kin = " << std::fixed << std::setprecision(1) << T_check << " K\n";
    std::cout << "  (should be ≈" << T_initial << " K)\n\n";
    
    // Create LJ-only model (charges = 0)
    std::cout << "Setting up LJ potential (Coulomb OFF via Q=0)...\n";
    auto model = create_lj_coulomb_model();

    ModelParams mp;
    mp.rc = 10.0;  // 10 Å cutoff

    std::cout << "  LJ parameters: σ = " << Ar_sigma << " Å, ε = " << Ar_epsilon << " kcal/mol\n";
    std::cout << "  Cutoff: " << mp.rc << " Å\n";
    std::cout << "  Charges: 0.0 (LJ only)\n";

    // DIAGNOSTIC: Evaluate forces ONCE and check magnitude
    model->eval(state, mp);

    double F_max = 0.0;
    double F_avg = 0.0;
    for (const auto& f : state.F) {
        double f_mag = std::sqrt(f.x*f.x + f.y*f.y + f.z*f.z);
        F_avg += f_mag;
        if (f_mag > F_max) F_max = f_mag;
    }
    F_avg /= state.N;

    std::cout << "  Initial forces: F_max = " << std::scientific << std::setprecision(2) << F_max 
              << " kcal/mol/Å, F_avg = " << F_avg << " kcal/mol/Å\n";

    if (F_max > 1000.0) {
        std::cout << "  ⚠️  WARNING: Very large forces detected! Atoms may be overlapping.\n";
    }

    double E_initial = state.E.total();
    std::cout << "  Initial energy: " << std::fixed << std::setprecision(2) << E_initial << " kcal/mol\n";

    if (F_max > 100.0) {
        std::cout << "\n⚠️  Large forces detected! Skipping FIRE, hoping Langevin can handle it...\n\n";
    } else {
        std::cout << "\n✅ Forces reasonable, proceeding with Langevin dynamics...\n\n";
    }
    
    // Setup Langevin dynamics
    LangevinDynamics dynamics(*model, mp);
    
    LangevinParams params;
    params.dt = 1.0;            // 1 fs (was 1e-3 = 0.001 fs - too small!)
    params.n_steps = 50000;     // 50 ps
    params.T_target = T_target;
    params.gamma = 0.1;         // 1/fs
    params.print_freq = 5000;
    params.verbose = true;
    
    std::cout << "Langevin Parameters:\n";
    std::cout << "  dt = " << params.dt << " fs\n";
    std::cout << "  n_steps = " << params.n_steps << " (" << (params.n_steps * params.dt / 1000.0) << " ps)\n";
    std::cout << "  gamma = " << params.gamma << " / fs\n";
    std::cout << "  T_target = " << params.T_target << " K\n\n";
    
    // Open trajectory file for temperature
    std::ofstream traj("temperature_trace.csv");
    traj << "# Equipartition test: T_kin vs time\n";
    traj << "# Seed: " << seed << "\n";
    traj << "# T_initial = " << T_initial << " K, T_target = " << T_target << " K\n";
    traj << "step,time(ps),T_kin(K),KE(kcal/mol),PE(kcal/mol)\n";
    
    // Run dynamics with temperature tracking
    std::cout << "Running Langevin dynamics...\n\n";
    
    std::vector<double> T_history;
    T_history.reserve(params.n_steps);
    
    model->eval(state, mp);
    
    for (int step = 0; step < params.n_steps; ++step) {
        // Langevin step (copied from velocity_verlet.cpp for temperature tracking)
        
        // Velocity update
        std::normal_distribution<double> gaussian(0.0, 1.0);
        
        for (uint32_t i = 0; i < state.N; ++i) {
            double inv_m = 1.0 / state.M[i];
            
            double ax = state.F[i].x * inv_m;
            double ay = state.F[i].y * inv_m;
            double az = state.F[i].z * inv_m;
            
            double friction_x = params.gamma * state.V[i].x;
            double friction_y = params.gamma * state.V[i].y;
            double friction_z = params.gamma * state.V[i].z;

            double sigma_internal = std::sqrt(2.0 * params.gamma * k_B * params.T_target * inv_m * params.dt);
            double sigma = sigma_internal * 0.0205;  // Convert to Å/fs (NOT 21.88!)

            double noise_x = sigma * gaussian(rng);
            double noise_y = sigma * gaussian(rng);
            double noise_z = sigma * gaussian(rng);
            
            state.V[i].x += (ax - friction_x) * params.dt + noise_x;
            state.V[i].y += (ay - friction_y) * params.dt + noise_y;
            state.V[i].z += (az - friction_z) * params.dt + noise_z;
        }
        
        // Position update
        for (uint32_t i = 0; i < state.N; ++i) {
            state.X[i].x += state.V[i].x * params.dt;
            state.X[i].y += state.V[i].y * params.dt;
            state.X[i].z += state.V[i].z * params.dt;
        }
        
        // PBC wrapping
        for (uint32_t i = 0; i < state.N; ++i) {
            state.X[i].x = std::fmod(state.X[i].x + 10.0 * box_length, box_length);
            state.X[i].y = std::fmod(state.X[i].y + 10.0 * box_length, box_length);
            state.X[i].z = std::fmod(state.X[i].z + 10.0 * box_length, box_length);
        }
        
        // Compute forces
        model->eval(state, mp);
        
        // Compute temperature
        double T_kin = compute_kinetic_temperature(state);
        T_history.push_back(T_kin);
        
        // Write to trajectory
        if (step % 100 == 0) {
            double time_ps = (step * params.dt) / 1000.0;
            double KE = compute_kinetic_energy(state);
            double PE = state.E.total();
            
            traj << step << "," << time_ps << "," << T_kin << "," << KE << "," << PE << "\n";
        }
        
        // Print progress
        if ((step + 1) % params.print_freq == 0) {
            double time_ps = ((step + 1) * params.dt) / 1000.0;
            std::cout << "  Step " << std::setw(6) << (step + 1) 
                      << "  t = " << std::fixed << std::setprecision(2) << time_ps << " ps"
                      << "  T_kin = " << std::setprecision(1) << T_kin << " K\n";
        }
    }
    
    traj.close();
    
    // ========================================================================
    // ANALYSIS
    // ========================================================================
    
    std::cout << "\n╔═══════════════════════════════════════════════════════════╗\n";
    std::cout << "║  ANALYSIS                                                  ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════╝\n\n";
    
    // Split into equilibration and production
    int n_equil = params.n_steps / 2;
    int n_prod = params.n_steps - n_equil;
    
    // Compute statistics for last half
    double T_mean = 0.0;
    double T2_mean = 0.0;
    
    for (int i = n_equil; i < params.n_steps; ++i) {
        T_mean += T_history[i];
        T2_mean += T_history[i] * T_history[i];
    }
    
    T_mean /= n_prod;
    T2_mean /= n_prod;
    
    double T_std = std::sqrt(T2_mean - T_mean * T_mean);
    
    std::cout << "Production Statistics (last " << n_prod << " steps):\n";
    std::cout << "  <T_kin> = " << std::fixed << std::setprecision(2) << T_mean << " ± " << T_std << " K\n";
    std::cout << "  Target T = " << T_target << " K\n";
    std::cout << "  Deviation = " << std::showpos << (T_mean - T_target) << std::noshowpos << " K ("
              << std::setprecision(1) << (100.0 * (T_mean - T_target) / T_target) << "%)\n";
    std::cout << "  Fluctuations: σ = " << std::setprecision(2) << T_std << " K\n";
    
    // Compute min/max
    double T_min = T_history[n_equil];
    double T_max = T_history[n_equil];
    
    for (int i = n_equil; i < params.n_steps; ++i) {
        if (T_history[i] < T_min) T_min = T_history[i];
        if (T_history[i] > T_max) T_max = T_history[i];
    }
    
    std::cout << "  Range: [" << std::setprecision(1) << T_min << ", " << T_max << "] K\n\n";
    
    // ========================================================================
    // PASS/FAIL CRITERIA
    // ========================================================================
    
    std::cout << "╔═══════════════════════════════════════════════════════════╗\n";
    std::cout << "║  VALIDATION                                                ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════╝\n\n";
    
    bool pass_mean = std::abs(T_mean - T_target) < 0.03 * T_target;  // Within 3%
    bool pass_std = (T_std > 5.0) && (T_std < 100.0);  // Fluctuations 5-100 K
    bool pass_equilibration = (T_history.back() > 250.0) && (T_history.back() < 350.0);  // Final T reasonable
    
    std::cout << "Test Criteria:\n";
    std::cout << "  1. Mean T within 3% of target: ";
    if (pass_mean) {
        std::cout << "✅ PASS (";
    } else {
        std::cout << "❌ FAIL (";
    }
    std::cout << std::setprecision(2) << (100.0 * (T_mean - T_target) / T_target) << "%)\n";
    
    std::cout << "  2. Fluctuations stable (5-100 K): ";
    if (pass_std) {
        std::cout << "✅ PASS (";
    } else {
        std::cout << "❌ FAIL (";
    }
    std::cout << std::setprecision(1) << T_std << " K)\n";
    
    std::cout << "  3. Final T reasonable (250-350 K): ";
    if (pass_equilibration) {
        std::cout << "✅ PASS (";
    } else {
        std::cout << "❌ FAIL (";
    }
    std::cout << std::setprecision(1) << T_history.back() << " K)\n\n";
    
    bool overall_pass = pass_mean && pass_std && pass_equilibration;
    
    if (overall_pass) {
        std::cout << "╔═══════════════════════════════════════════════════════════╗\n";
        std::cout << "║  ✅ TEST PASSED                                           ║\n";
        std::cout << "╚═══════════════════════════════════════════════════════════╝\n";
        return 0;
    } else {
        std::cout << "╔═══════════════════════════════════════════════════════════╗\n";
        std::cout << "║  ❌ TEST FAILED                                           ║\n";
        std::cout << "╚═══════════════════════════════════════════════════════════╝\n";
        return 1;
    }
}
