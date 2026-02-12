#pragma once

#include "state.hpp"
#include <random>

namespace atomistic {

/**
 * Initialize velocities from Maxwell-Boltzmann distribution at temperature T
 * 
 * Physical distribution:
 *   v_{i,α} ~ N(0, sqrt(k_B T / m_i))
 * 
 * Where:
 * - k_B = Boltzmann constant (in kcal/mol/K)
 * - T = temperature (K)
 * - m_i = atomic mass (amu)
 * - α = x, y, z component
 * 
 * Also removes center-of-mass drift to ensure zero net momentum.
 */
void initialize_velocities_thermal(
    State& state,
    double temperature_K,
    std::mt19937& rng
);

/**
 * Initialize velocities along force direction (for FIRE minimization)
 * 
 * Deterministic initialization:
 *   v_i = F_i * (dt / ||F||)
 * 
 * This guarantees P = F·v > 0 on first step, avoiding FIRE deadlock.
 * Use for pure minimization, not for realistic dynamics.
 */
void initialize_velocities_along_force(
    State& state,
    double dt
);

} // namespace atomistic
