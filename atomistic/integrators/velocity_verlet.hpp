#pragma once

#include "atomistic/core/state.hpp"
#include "atomistic/models/model.hpp"
#include <random>

namespace atomistic {

// ============================================================================
// VELOCITY VERLET INTEGRATOR (NVE - Microcanonical)
// ============================================================================

/**
 * Velocity Verlet Parameters
 */
struct VelocityVerletParams {
    double dt = 1e-3;           // Timestep (fs)
    int n_steps = 1000;         // Number of steps
    int print_freq = 100;       // Print diagnostics every N steps
    bool verbose = false;       // Print detailed output
};

/**
 * Statistics from Velocity Verlet run
 */
struct VelocityVerletStats {
    int steps_completed;        // Total steps run
    double E_initial;           // Initial total energy (kcal/mol)
    double E_final;             // Final total energy (kcal/mol)
    double E_drift;             // Energy drift (kcal/mol)
    double T_avg;               // Average temperature (K)
    double KE_avg;              // Average kinetic energy (kcal/mol)
    double PE_avg;              // Average potential energy (kcal/mol)
};

/**
 * Velocity Verlet Integrator (NVE ensemble)
 * 
 * Algorithm:
 * 1. v(t+dt/2) = v(t) + F(t) * dt / (2m)
 * 2. x(t+dt) = x(t) + v(t+dt/2) * dt
 * 3. Compute F(t+dt) from new positions
 * 4. v(t+dt) = v(t+dt/2) + F(t+dt) * dt / (2m)
 * 
 * Symplectic, time-reversible, conserves energy (NVE).
 * 
 * Notes:
 * - State.V must be initialized (e.g., Maxwell-Boltzmann)
 * - State.M must be filled with atomic masses
 * - Energy should be conserved to ~1e-4 per particle per step
 */
class VelocityVerlet {
public:
    VelocityVerlet(IModel& model, const ModelParams& mp)
        : model(model), mp(mp) {}

    /**
     * Run velocity Verlet integration
     * 
     * @param state Initial state (positions, velocities, masses)
     * @param params Integration parameters
     * @return Statistics from the run
     */
    VelocityVerletStats integrate(State& state, const VelocityVerletParams& params);

private:
    IModel& model;
    ModelParams mp;
};

// ============================================================================
// LANGEVIN THERMOSTAT (NVT - Canonical)
// ============================================================================

/**
 * Langevin Thermostat Parameters
 */
struct LangevinParams {
    double dt = 1e-3;           // Timestep (fs)
    int n_steps = 1000;         // Number of steps
    double T_target = 300.0;    // Target temperature (K)
    double gamma = 0.1;         // Friction coefficient (1/fs)
    int print_freq = 100;       // Print diagnostics every N steps
    bool verbose = false;       // Print detailed output
    bool forces_valid = false;  // If true, skip initial force evaluation (for chained calls)
};

/**
 * Statistics from Langevin run
 */
struct LangevinStats {
    int steps_completed;        // Total steps run
    double T_avg;               // Average temperature (K)
    double T_std;               // Temperature std deviation (K)
    double KE_avg;              // Average kinetic energy (kcal/mol)
    double PE_avg;              // Average potential energy (kcal/mol)
    double E_total_avg;         // Average total energy (kcal/mol)
};

/**
 * Langevin Dynamics (NVT ensemble)
 * 
 * Stochastic equation of motion:
 * 
 *   m * dv/dt = F(x) - γ*m*v + sqrt(2*γ*m*k_B*T) * R(t)
 * 
 * Where:
 * - F(x) = deterministic force
 * - γ*m*v = friction (damping)
 * - sqrt(2*γ*m*k_B*T) * R(t) = random force (Gaussian white noise)
 * 
 * Discretized (Euler-Maruyama):
 * 
 *   v(t+dt) = v(t) + [F/m - γ*v] * dt + sqrt(2*γ*k_B*T/m) * sqrt(dt) * R
 *   x(t+dt) = x(t) + v(t+dt) * dt
 * 
 * Properties:
 * - Canonical ensemble (NVT)
 * - Temperature controlled by γ and random force
 * - Not symplectic (dissipative + stochastic)
 * - Simple, robust, widely used
 */
class LangevinDynamics {
public:
    LangevinDynamics(IModel& model, const ModelParams& mp)
        : model(model), mp(mp) {}

    /**
     * Run Langevin dynamics
     * 
     * @param state Initial state (positions, velocities, masses)
     * @param params Thermostat parameters
     * @param rng Random number generator
     * @return Statistics from the run
     */
    LangevinStats integrate(
        State& state, 
        const LangevinParams& params,
        std::mt19937& rng
    );

private:
    IModel& model;
    ModelParams mp;
};

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/**
 * Compute kinetic energy from velocities
 * 
 * KE = (1/2) * Σ m_i * v_i²
 * 
 * Units: kcal/mol (if velocities in Å/fs and masses in amu)
 */
double compute_kinetic_energy(const State& state);

/**
 * Compute instantaneous temperature from kinetic energy
 * 
 * T = 2 * KE / (3 * N * k_B)
 * 
 * From equipartition theorem: KE = (3/2) * N * k_B * T
 * 
 * Units: K (Kelvin)
 */
double compute_temperature(const State& state);

/**
 * Rescale velocities to target temperature
 * 
 * v_new = v_old * sqrt(T_target / T_current)
 * 
 * Simple thermostat (not recommended for production, use Langevin instead)
 */
void rescale_velocities(State& state, double T_target);

} // namespace atomistic
