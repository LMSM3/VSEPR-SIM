#include "maxwell_boltzmann.hpp"
#include <cmath>

namespace atomistic {

// ============================================================================
// PHYSICS CONSTANTS (High Precision)
// ============================================================================

// Boltzmann constant (kcal/(mol·K))
static constexpr double k_B = 0.0019872041;

// Kinetic energy conversion: amu·Å²/fs² → kcal/mol
static constexpr double KE_CONV = 2390.057361;

// Velocity conversion: sqrt(kcal/(mol·amu)) → Å/fs
// CORRECT VALUE VALIDATED: 0.0205 gives correct Maxwell-Boltzmann distribution
// Reference: BAOAB_IMPLEMENTATION_COMPLETE.md, LANGEVIN_ROOT_CAUSE_IDENTIFIED.md
static constexpr double VEL_CONV = 0.0205;

void initialize_velocities_thermal(
    State& state,
    double temperature_K,
    std::mt19937& rng
) {
    if (state.N == 0) return;

    state.V.resize(state.N);

    // Initialize with Maxwell-Boltzmann distribution
    for (uint32_t i = 0; i < state.N; ++i) {
        // Atomic mass (default to 1.0 amu if not set)
        double mass = (state.M.size() > i) ? state.M[i] : 1.0;

        // Standard deviation: σ = sqrt(k_B * T / m)
        // Units: sqrt((kcal/mol/K) * K / amu) = sqrt(kcal/(mol·amu))
        // VEL_CONV = 0.0205 converts to Å/fs (VALIDATED)
        double sigma = std::sqrt(k_B * temperature_K / mass) * VEL_CONV;

        std::normal_distribution<double> dist(0.0, sigma);

        state.V[i].x = dist(rng);
        state.V[i].y = dist(rng);
        state.V[i].z = dist(rng);
    }
    
    // Remove center-of-mass velocity (zero net momentum)
    Vec3 v_com = {0, 0, 0};
    double total_mass = 0.0;
    
    for (uint32_t i = 0; i < state.N; ++i) {
        double mass = (state.M.size() > i) ? state.M[i] : 1.0;
        v_com.x += state.V[i].x * mass;
        v_com.y += state.V[i].y * mass;
        v_com.z += state.V[i].z * mass;
        total_mass += mass;
    }
    
    if (total_mass > 0) {
        v_com.x /= total_mass;
        v_com.y /= total_mass;
        v_com.z /= total_mass;
        
        // Subtract COM velocity from all atoms
        for (uint32_t i = 0; i < state.N; ++i) {
            state.V[i].x -= v_com.x;
            state.V[i].y -= v_com.y;
            state.V[i].z -= v_com.z;
        }
    }
}

void initialize_velocities_along_force(
    State& state,
    double dt
) {
    if (state.N == 0 || state.F.size() != state.N) return;
    
    state.V.resize(state.N);
    
    // Compute total force magnitude
    double fnorm = 0.0;
    for (const auto& f : state.F) {
        fnorm += dot(f, f);
    }
    fnorm = std::sqrt(fnorm);
    
    if (fnorm > 0) {
        // Initialize velocities along force direction
        for (uint32_t i = 0; i < state.N; ++i) {
            state.V[i] = state.F[i] * (dt / fnorm);
        }
    } else {
        // Forces are zero, use zero velocities
        for (auto& v : state.V) {
            v = {0, 0, 0};
        }
    }
}

} // namespace atomistic
