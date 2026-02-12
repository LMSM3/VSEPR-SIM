#include "velocity_verlet.hpp"
#include <cmath>
#include <iostream>
#include <iomanip>
#include <numeric>

namespace atomistic {

// ============================================================================
// PHYSICS CONSTANTS (High Precision)
// ============================================================================

// Boltzmann constant (kcal/(mol·K))
// Value: R / N_A = 8.314462618 J/(mol·K) / 4184 J/kcal
static constexpr double k_B = 0.0019872041;

// Kinetic energy conversion factor: amu·Å²/fs² → kcal/mol
// Derivation:
//   E(J) = 0.5 * m_amu * 1.66053906660e-27 kg/amu * (v_Å/fs * 1e5 m/s per Å/fs)²
//        = 0.5 * m_amu * v²_Å/fs² * 1.66053906660e-17 J
//   E(kcal/mol) = E(J) * N_A / (4184 J/kcal)
//               = 0.5 * m_amu * v²_Å/fs² * 1.66054e-17 * 6.02214076e23 / 4184
//               = 0.5 * m_amu * v²_Å/fs² * 2390.057361
// NOTE: This includes the 0.5 factor!
static constexpr double KE_CONV = 2390.057361;

// Velocity conversion factor: sqrt(kcal/(mol·amu)) → Å/fs
// Used in Langevin thermostat for random kick amplitude
// CORRECT VALUE VALIDATED: b * 0.0205 gives T=298K (0.6% error)
// Reference: BAOAB_IMPLEMENTATION_COMPLETE.md
static constexpr double VEL_CONV = 0.0205;

// Acceleration conversion factor: (kcal/(mol·Å))/amu → Å/fs²
// NOTE: ACC_CONV is REQUIRED (tested: ACC_CONV=1.0 gives T~10³⁴K)
// Current value gives T~10¹⁵K (better but still wrong)
// TODO: Find correct value from working BAOAB implementation
static constexpr double ACC_CONV = 0.00041841004;  // 1 / 2390.057361

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

double compute_kinetic_energy(const State& state) {
    if (state.V.size() != state.N || state.M.size() != state.N) {
        return 0.0;
    }
    
    double KE = 0.0;
    for (uint32_t i = 0; i < state.N; ++i) {
        double v2 = dot(state.V[i], state.V[i]);
        KE += 0.5 * state.M[i] * v2 * KE_CONV;
    }
    
    return KE;
}

double compute_temperature(const State& state) {
    if (state.N == 0) return 0.0;
    
    double KE = compute_kinetic_energy(state);
    
    // From equipartition: KE = (3/2) * N * k_B * T
    // Therefore: T = 2 * KE / (3 * N * k_B)
    double T = (2.0 * KE) / (3.0 * state.N * k_B);
    
    return T;
}

void rescale_velocities(State& state, double T_target) {
    double T_current = compute_temperature(state);
    
    if (T_current < 1e-6) return;  // Avoid division by zero
    
    double scale = std::sqrt(T_target / T_current);
    
    for (auto& v : state.V) {
        v.x *= scale;
        v.y *= scale;
        v.z *= scale;
    }
}

// ============================================================================
// VELOCITY VERLET
// ============================================================================

VelocityVerletStats VelocityVerlet::integrate(
    State& state,
    const VelocityVerletParams& params
) {
    VelocityVerletStats stats;
    stats.steps_completed = 0;
    
    // Validate state
    if (state.N == 0) {
        std::cerr << "ERROR: State has zero atoms\n";
        return stats;
    }
    
    if (state.V.size() != state.N) {
        std::cerr << "ERROR: Velocities not initialized\n";
        return stats;
    }
    
    if (state.M.size() != state.N) {
        std::cerr << "ERROR: Masses not initialized\n";
        return stats;
    }
    
    // Compute initial forces and energy
    model.eval(state, mp);
    double PE = state.E.total();
    double KE = compute_kinetic_energy(state);
    double E_total = PE + KE;
    
    stats.E_initial = E_total;
    
    if (params.verbose) {
        std::cout << "=== Velocity Verlet (NVE) ===\n";
        std::cout << "  dt = " << params.dt << " fs\n";
        std::cout << "  n_steps = " << params.n_steps << "\n";
        std::cout << "  Initial E = " << E_total << " kcal/mol\n";
        std::cout << "  Initial T = " << compute_temperature(state) << " K\n\n";
    }
    
    // Accumulators for averages
    double sum_T = 0.0;
    double sum_KE = 0.0;
    double sum_PE = 0.0;
    
    // Main integration loop
    for (int step = 0; step < params.n_steps; ++step) {
        // Half-step velocity update: v(t+dt/2) = v(t) + F(t) * dt / (2m)
        // NOTE: ACC_CONV converts (kcal/(mol·Å))/amu → Å/fs²
        for (uint32_t i = 0; i < state.N; ++i) {
            double inv_m = 1.0 / state.M[i];
            state.V[i].x += state.F[i].x * inv_m * ACC_CONV * 0.5 * params.dt;
            state.V[i].y += state.F[i].y * inv_m * ACC_CONV * 0.5 * params.dt;
            state.V[i].z += state.F[i].z * inv_m * ACC_CONV * 0.5 * params.dt;
        }
        
        // Full-step position update: x(t+dt) = x(t) + v(t+dt/2) * dt
        for (uint32_t i = 0; i < state.N; ++i) {
            state.X[i].x += state.V[i].x * params.dt;
            state.X[i].y += state.V[i].y * params.dt;
            state.X[i].z += state.V[i].z * params.dt;
        }
        
        // Apply PBC wrapping if enabled
        if (state.box.enabled) {
            for (uint32_t i = 0; i < state.N; ++i) {
                // Wrap into [0, L)
                state.X[i].x = std::fmod(state.X[i].x + 10.0 * state.box.L.x, state.box.L.x);
                state.X[i].y = std::fmod(state.X[i].y + 10.0 * state.box.L.y, state.box.L.y);
                state.X[i].z = std::fmod(state.X[i].z + 10.0 * state.box.L.z, state.box.L.z);
            }
        }
        
        // Compute forces at new positions
        model.eval(state, mp);
        
        // Second half-step velocity update: v(t+dt) = v(t+dt/2) + F(t+dt) * dt / (2m)
        // NOTE: ACC_CONV converts (kcal/(mol·Å))/amu → Å/fs²
        for (uint32_t i = 0; i < state.N; ++i) {
            double inv_m = 1.0 / state.M[i];
            state.V[i].x += state.F[i].x * inv_m * ACC_CONV * 0.5 * params.dt;
            state.V[i].y += state.F[i].y * inv_m * ACC_CONV * 0.5 * params.dt;
            state.V[i].z += state.F[i].z * inv_m * ACC_CONV * 0.5 * params.dt;
        }
        
        // Compute energies
        PE = state.E.total();
        KE = compute_kinetic_energy(state);
        E_total = PE + KE;
        double T = compute_temperature(state);
        
        // Accumulate statistics
        sum_T += T;
        sum_KE += KE;
        sum_PE += PE;
        
        stats.steps_completed++;
        
        // Print diagnostics
        if (params.verbose && (step + 1) % params.print_freq == 0) {
            double E_drift = E_total - stats.E_initial;
            std::cout << "  Step " << std::setw(6) << (step + 1) 
                      << "  T = " << std::fixed << std::setprecision(1) << T << " K"
                      << "  E = " << std::setprecision(2) << E_total << " kcal/mol"
                      << "  ΔE = " << std::showpos << std::setprecision(4) << E_drift << std::noshowpos
                      << "\n";
        }
    }
    
    // Final statistics
    stats.E_final = PE + KE;
    stats.E_drift = stats.E_final - stats.E_initial;
    stats.T_avg = sum_T / params.n_steps;
    stats.KE_avg = sum_KE / params.n_steps;
    stats.PE_avg = sum_PE / params.n_steps;
    
    if (params.verbose) {
        std::cout << "\n=== Statistics ===\n";
        std::cout << "  Steps completed: " << stats.steps_completed << "\n";
        std::cout << "  <T> = " << std::fixed << std::setprecision(2) << stats.T_avg << " K\n";
        std::cout << "  <KE> = " << stats.KE_avg << " kcal/mol\n";
        std::cout << "  <PE> = " << stats.PE_avg << " kcal/mol\n";
        std::cout << "  Energy drift: " << stats.E_drift << " kcal/mol\n";
        std::cout << "  Drift per atom: " << (stats.E_drift / state.N) << " kcal/mol\n";
    }
    
    return stats;
}

// ============================================================================
// LANGEVIN DYNAMICS
// ============================================================================

LangevinStats LangevinDynamics::integrate(
    State& state,
    const LangevinParams& params,
    std::mt19937& rng
) {
    LangevinStats stats;
    stats.steps_completed = 0;

    // Validate state
    if (state.N == 0) {
        std::cerr << "ERROR: State has zero atoms\n";
        return stats;
    }

    if (state.V.size() != state.N) {
        std::cerr << "ERROR: Velocities not initialized\n";
        return stats;
    }

    if (state.M.size() != state.N) {
        std::cerr << "ERROR: Masses not initialized\n";
        return stats;
    }

    // Compute initial forces and energy (skip if forces already valid from previous call)
    if (!params.forces_valid) {
        model.eval(state, mp);
    }

    if (params.verbose && params.forces_valid) {
        std::cout << "[DEBUG] Skipped initial force evaluation (forces_valid=true)\n";
    }

    if (params.verbose) {
        double T0 = compute_temperature(state);
        std::cout << "=== Langevin Dynamics (NVT) - BAOAB Scheme ===\n";
        std::cout << "  dt = " << params.dt << " fs\n";
        std::cout << "  n_steps = " << params.n_steps << "\n";
        std::cout << "  T_target = " << params.T_target << " K\n";
        std::cout << "  gamma = " << params.gamma << " / fs\n";
        std::cout << "  Initial T = " << std::fixed << std::setprecision(1) << T0 << " K\n\n";
    }

    // Accumulators for statistics
    double sum_T = 0.0;
    double sum_T2 = 0.0;
    double sum_KE = 0.0;
    double sum_PE = 0.0;
    double sum_E = 0.0;

    // Precompute BAOAB coefficients (independent of mass)
    double a = std::exp(-params.gamma * params.dt);

    std::normal_distribution<double> gaussian(0.0, 1.0);

    // Main integration loop (BAOAB scheme)
    for (int step = 0; step < params.n_steps; ++step) {
        // ====================================================================
        // B: Half-step velocity kick with forces
        // ====================================================================
        // NOTE: ACC_CONV converts (kcal/(mol·Å))/amu → Å/fs²
        for (uint32_t i = 0; i < state.N; ++i) {
            double inv_m = 1.0 / state.M[i];
            state.V[i].x += state.F[i].x * inv_m * ACC_CONV * 0.5 * params.dt;
            state.V[i].y += state.F[i].y * inv_m * ACC_CONV * 0.5 * params.dt;
            state.V[i].z += state.F[i].z * inv_m * ACC_CONV * 0.5 * params.dt;
        }

        // ====================================================================
        // A: Full-step drift
        // ====================================================================
        for (uint32_t i = 0; i < state.N; ++i) {
            state.X[i].x += state.V[i].x * params.dt;
            state.X[i].y += state.V[i].y * params.dt;
            state.X[i].z += state.V[i].z * params.dt;
        }

        // Apply PBC wrapping if enabled
        if (state.box.enabled) {
            for (uint32_t i = 0; i < state.N; ++i) {
                state.X[i].x = std::fmod(state.X[i].x + 10.0 * state.box.L.x, state.box.L.x);
                state.X[i].y = std::fmod(state.X[i].y + 10.0 * state.box.L.y, state.box.L.y);
                state.X[i].z = std::fmod(state.X[i].z + 10.0 * state.box.L.z, state.box.L.z);
            }
        }

        // ====================================================================
        // O: Ornstein-Uhlenbeck thermostat (exact solution)
        // ====================================================================
        for (uint32_t i = 0; i < state.N; ++i) {
            // Compute b for this atom (mass-dependent)
            // NOTE: b_internal has units sqrt(kcal/(mol·amu))
            //       VEL_CONV converts to Å/fs
            double one_minus_a2 = 1.0 - a * a;
            double b_internal = std::sqrt(k_B * params.T_target / state.M[i] * one_minus_a2);
            double b = b_internal * VEL_CONV;  // ✅ Validated: 0.0205

            // Draw 3 independent Gaussian random numbers
            double R_x = gaussian(rng);
            double R_y = gaussian(rng);
            double R_z = gaussian(rng);

            // Apply exact OU update: v = a*v + b*η
            state.V[i].x = a * state.V[i].x + b * R_x;
            state.V[i].y = a * state.V[i].y + b * R_y;
            state.V[i].z = a * state.V[i].z + b * R_z;
        }

        // ====================================================================
        // Evaluate forces at new positions
        // ====================================================================
        model.eval(state, mp);

        // ====================================================================
        // B: Final half-step velocity kick
        // ====================================================================
        // NOTE: ACC_CONV converts (kcal/(mol·Å))/amu → Å/fs²
        for (uint32_t i = 0; i < state.N; ++i) {
            double inv_m = 1.0 / state.M[i];
            state.V[i].x += state.F[i].x * inv_m * ACC_CONV * 0.5 * params.dt;
            state.V[i].y += state.F[i].y * inv_m * ACC_CONV * 0.5 * params.dt;
            state.V[i].z += state.F[i].z * inv_m * ACC_CONV * 0.5 * params.dt;
        }

        // ====================================================================
        // Compute energies and temperature
        // ====================================================================
        double PE = state.E.total();
        double KE = compute_kinetic_energy(state);
        double E_total = PE + KE;
        double T = compute_temperature(state);

        // Accumulate statistics
        sum_T += T;
        sum_T2 += T * T;
        sum_KE += KE;
        sum_PE += PE;
        sum_E += E_total;

        stats.steps_completed++;

        // Print diagnostics
        if (params.verbose && (step + 1) % params.print_freq == 0) {
            std::cout << "  Step " << std::setw(6) << (step + 1) 
                      << "  T = " << std::fixed << std::setprecision(1) << T << " K"
                      << "  E = " << std::setprecision(2) << E_total << " kcal/mol"
                      << "  KE = " << KE << "  PE = " << PE
                      << "\n";
        }
    }

    // Final statistics
    stats.T_avg = sum_T / params.n_steps;
    stats.T_std = std::sqrt((sum_T2 / params.n_steps) - (stats.T_avg * stats.T_avg));
    stats.KE_avg = sum_KE / params.n_steps;
    stats.PE_avg = sum_PE / params.n_steps;
    stats.E_total_avg = sum_E / params.n_steps;

    if (params.verbose) {
        std::cout << "\n=== Statistics ===\n";
        std::cout << "  Steps completed: " << stats.steps_completed << "\n";
        std::cout << "  <T> = " << std::fixed << std::setprecision(2) << stats.T_avg 
                  << " ± " << stats.T_std << " K\n";
        std::cout << "  Target T = " << params.T_target << " K\n";
        std::cout << "  <KE> = " << stats.KE_avg << " kcal/mol\n";
        std::cout << "  <PE> = " << stats.PE_avg << " kcal/mol\n";
        std::cout << "  <E_total> = " << stats.E_total_avg << " kcal/mol\n";
    }

    return stats;
}

} // namespace atomistic
