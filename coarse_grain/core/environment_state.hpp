#pragma once
/**
 * environment_state.hpp — Environment-Responsive Bead State (X_B)
 *
 * Implements the environment-responsive extension as specified in
 * "Environment-Responsive Coarse-Grained Bead Dynamics" (ERB document).
 *
 * The extended bead state:
 *
 *   BB_B = { B (intrinsic), X_B (environment) }
 *
 * where:
 *
 *   X_B = { rho_B, C_B, P_{2,B}, eta_B }
 *
 * Fast observables (rho, C, P2) are recomputed each step.
 * Slow state (eta) evolves via first-order relaxation with memory.
 *
 * Design rules (from ERB §2):
 *   Rule 1: X_B is derived from measurable local structure
 *   Rule 2: Fast observables vs slow state are strictly separated
 *   Rule 3: X_B modifies behaviour, not identity
 *   Rule 4: Low-dimensional (4 variables, 6 parameters)
 *
 * Anti-black-box: all parameters, observables, and state variables
 * are explicitly inspectable. No hidden constants.
 *
 * Reference: "Environment-Responsive Coarse-Grained Bead Dynamics"
 *            section_environment_responsive_beads.tex
 */

#include "atomistic/core/state.hpp"
#include "coarse_grain/core/bead.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace coarse_grain {

// ============================================================================
// Environment Parameters
// ============================================================================

/**
 * EnvironmentParams — inspectable parameters for environment-responsive
 * observables and coupling.
 *
 * Parameter set (ERB §8.5):
 *   alpha:         density weight in target function        [0, 1]
 *   beta:          orientational order weight                [0, 1], alpha + beta = 1
 *   tau:           relaxation timescale (fs)                 10 -- 10^4
 *   gamma_steric:  steric kernel modulation                 [0, 1]
 *   gamma_elec:    electrostatic kernel modulation           [-1, 0]
 *   gamma_disp:    dispersion kernel modulation              [0, 2]
 *
 * Observable parameters:
 *   sigma_rho:     Gaussian width for density weighting (A)
 *   r_cutoff:      cutoff radius for all observables (A)
 *   delta_sw:      switching function width at cutoff (A)
 *   rho_max:       normalisation constant for rho -> [0,1]
 */
struct EnvironmentParams {
    // --- Target function weights ---
    double alpha{0.5};          // Density weight
    double beta{0.5};           // Orientational order weight

    // --- Relaxation ---
    double tau{100.0};          // Relaxation timescale (fs)

    // --- Kernel modulation ---
    double gamma_steric{0.2};   // Steric hardening
    double gamma_elec{-0.1};    // Electrostatic screening (negative: damping)
    double gamma_disp{0.5};     // Dispersion enhancement

    // --- Observable parameters ---
    double sigma_rho{3.0};      // Gaussian width for density (A)
    double r_cutoff{8.0};       // Cutoff radius for all observables (A)
    double delta_sw{1.0};       // Switching function width (A)
    double rho_max{10.0};       // Normalisation for rho_hat
};

// ============================================================================
// Environment State (X_B)
// ============================================================================

/**
 * EnvironmentState — per-bead environment-responsive state.
 *
 * Fast observables (recomputed each step):
 *   rho:   local density
 *   C:     coordination number
 *   P2:    orientational order parameter
 *
 * Slow state (evolved over time):
 *   eta:   internal state / memory variable, eta in [0, 1]
 */
struct EnvironmentState {
    // --- Fast observables ---
    double rho{};               // Local density
    double C{};                 // Coordination number
    double P2{};                // Orientational order parameter P_{2,B}

    // --- Normalised fast observables ---
    double rho_hat{};           // rho normalised to [0, 1]
    double P2_hat{};            // P2 mapped to [0, 1]: (P2 + 0.5) / 1.5

    // --- Slow state ---
    double eta{};               // Internal state variable, [0, 1]

    // --- Diagnostics ---
    double target_f{};          // Current target function f(rho, C, P2)
    int    neighbour_count{};   // Number of neighbours within cutoff
};

// ============================================================================
// Switching Function
// ============================================================================

/**
 * Smooth switching function for cutoff continuity.
 *
 *   f_sw(r; r_c, delta) = 0.5 * (1 + cos(pi * (r - r_c + delta) / delta))
 *
 * Returns 1.0 for r <= r_c - delta, 0.0 for r >= r_c,
 * smooth transition in between.
 */
inline double switching_function(double r, double r_c, double delta) {
    if (r <= r_c - delta) return 1.0;
    if (r >= r_c) return 0.0;
    constexpr double pi = 3.14159265358979323846;
    double x = (r - r_c + delta) / delta;
    return 0.5 * (1.0 + std::cos(pi * x));
}

// ============================================================================
// Density Weighting Function
// ============================================================================

/**
 * Gaussian weighting with smooth cutoff:
 *
 *   w(r) = exp(-r^2 / (2 sigma^2)) * f_sw(r; r_c, delta)
 */
inline double density_weight(double r, double sigma, double r_c, double delta) {
    if (r >= r_c) return 0.0;
    double g = std::exp(-r * r / (2.0 * sigma * sigma));
    return g * switching_function(r, r_c, delta);
}

// ============================================================================
// Fast Observable Computation
// ============================================================================

/**
 * NeighbourInfo — minimal per-neighbour data for observable computation.
 *
 * Avoids requiring full Bead objects; only the fields needed for
 * rho, C, and P2 are carried.
 */
struct NeighbourInfo {
    double distance{};          // |R_C - R_B| (A)
    atomistic::Vec3 n_hat{};    // Primary orientation axis of neighbour
    bool has_orientation{false}; // Whether orientation data is valid
};

/**
 * Compute all fast observables for a single bead given its neighbours.
 *
 * @param n_hat_B        Primary orientation axis of bead B
 * @param has_orient_B   Whether bead B has valid orientation
 * @param neighbours     List of neighbours with distance and orientation
 * @param params         Environment parameters
 * @return Updated EnvironmentState with fast observables filled in
 */
inline EnvironmentState compute_fast_observables(
    const atomistic::Vec3& n_hat_B,
    bool has_orient_B,
    const std::vector<NeighbourInfo>& neighbours,
    const EnvironmentParams& params)
{
    EnvironmentState state;

    double rho_sum = 0.0;
    double C_sum = 0.0;
    double P2_sum = 0.0;
    int P2_count = 0;
    int n_count = 0;

    for (const auto& nb : neighbours) {
        if (nb.distance >= params.r_cutoff || nb.distance < 1e-10)
            continue;

        ++n_count;

        // Local density: rho_B = sum_C w(r_BC)
        rho_sum += density_weight(nb.distance, params.sigma_rho,
                                   params.r_cutoff, params.delta_sw);

        // Coordination number: C_B = sum_C 1_{r_BC < r_c}
        C_sum += 1.0;

        // Orientational order: P_{2,B}
        if (has_orient_B && nb.has_orientation) {
            double cos_angle = atomistic::dot(n_hat_B, nb.n_hat);
            // Clamp for numerical safety
            cos_angle = std::clamp(cos_angle, -1.0, 1.0);
            double p2_pair = 0.5 * (3.0 * cos_angle * cos_angle - 1.0);
            P2_sum += p2_pair;
            ++P2_count;
        }
    }

    state.rho = rho_sum;
    state.C = C_sum;
    state.P2 = (P2_count > 0) ? P2_sum / P2_count : 0.0;
    state.neighbour_count = n_count;

    // Normalise to [0, 1]
    state.rho_hat = std::clamp(state.rho / params.rho_max, 0.0, 1.0);
    // P2 in [-0.5, 1.0] -> P2_hat in [0, 1]
    // When no oriented neighbours exist, P2_hat = 0 (no information)
    if (P2_count > 0) {
        state.P2_hat = std::clamp((state.P2 + 0.5) / 1.5, 0.0, 1.0);
    } else {
        state.P2_hat = 0.0;
    }

    // Target function: f = alpha * rho_hat + beta * P2_hat
    state.target_f = std::clamp(
        params.alpha * state.rho_hat + params.beta * state.P2_hat,
        0.0, 1.0);

    return state;
}

// ============================================================================
// Slow State Integration
// ============================================================================

/**
 * Integrate eta via forward Euler (ERB §5.6):
 *
 *   eta(t + dt) = eta(t) + (dt/tau) * (f(t) - eta(t))
 *
 * Suitable when dt/tau < 0.5.
 */
inline double integrate_eta_euler(double eta, double target_f,
                                   double dt, double tau)
{
    if (tau <= 0.0) return std::clamp(target_f, 0.0, 1.0);
    double eta_new = eta + (dt / tau) * (target_f - eta);
    return std::clamp(eta_new, 0.0, 1.0);
}

/**
 * Integrate eta via exact exponential solution (ERB §5.6):
 *
 *   eta(t + dt) = f + (eta(t) - f) * exp(-dt/tau)
 *
 * Unconditionally stable for all dt/tau ratios.
 */
inline double integrate_eta_exact(double eta, double target_f,
                                    double dt, double tau)
{
    if (tau <= 0.0) return std::clamp(target_f, 0.0, 1.0);
    double decay = std::exp(-dt / tau);
    double eta_new = target_f + (eta - target_f) * decay;
    return std::clamp(eta_new, 0.0, 1.0);
}

/**
 * Full environment state update for a single bead.
 *
 * Steps:
 *   1. Compute fast observables (rho, C, P2)
 *   2. Compute target function f
 *   3. Integrate eta (exact method)
 *   4. Return updated EnvironmentState
 *
 * @param prev_eta       Previous eta value
 * @param n_hat_B        Primary orientation axis of bead B
 * @param has_orient_B   Whether bead B has valid orientation
 * @param neighbours     Neighbour list
 * @param params         Environment parameters
 * @param dt             Timestep (fs)
 * @return Updated EnvironmentState
 */
inline EnvironmentState update_environment_state(
    double prev_eta,
    const atomistic::Vec3& n_hat_B,
    bool has_orient_B,
    const std::vector<NeighbourInfo>& neighbours,
    const EnvironmentParams& params,
    double dt)
{
    // Step 1-2: fast observables and target function
    EnvironmentState state = compute_fast_observables(
        n_hat_B, has_orient_B, neighbours, params);

    // Step 3: integrate eta (exact method for unconditional stability)
    state.eta = integrate_eta_exact(prev_eta, state.target_f, dt, params.tau);

    return state;
}

} // namespace coarse_grain
