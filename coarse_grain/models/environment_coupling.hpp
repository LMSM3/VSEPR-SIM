#pragma once
/**
 * environment_coupling.hpp — Kernel Modulation via Environment State
 *
 * Implements Option A (recommended) from the ERB specification:
 *
 *   K_k(l, r; eta_A, eta_B) = K_k(l, r) * (1 + gamma_k * eta_bar)
 *
 * where eta_bar = 0.5 * (eta_A + eta_B).
 *
 * Channel-specific modulations:
 *
 *   Dispersion:    enhancement under ordering   (gamma_disp > 0)
 *   Electrostatic: screening under crowding      (gamma_elec < 0)
 *   Steric:        hardening under compression   (gamma_steric > 0)
 *
 * Invariant (ERB §8.6):
 *   |1 + gamma_k * eta_bar| > 0 for all channels
 *   (kernel must not change sign)
 *
 * Anti-black-box: modulation factors are individually inspectable.
 *
 * Reference: "Environment-Responsive Coarse-Grained Bead Dynamics"
 *            section_environment_responsive_beads.tex, §6.1
 */

#include "coarse_grain/core/channel_kernels.hpp"
#include "coarse_grain/core/environment_state.hpp"
#include <cmath>

namespace coarse_grain {

// ============================================================================
// Modulation Factor
// ============================================================================

/**
 * Compute the environment modulation factor for a specific channel.
 *
 *   g_k(eta_A, eta_B) = 1 + gamma_k * eta_bar
 *
 * where eta_bar = 0.5 * (eta_A + eta_B).
 *
 * @param ch       Channel type
 * @param eta_A    Internal state of bead A
 * @param eta_B    Internal state of bead B
 * @param params   Environment parameters (contains gamma values)
 * @return Modulation factor (must be > 0)
 */
inline double kernel_modulation_factor(
    Channel ch,
    double eta_A,
    double eta_B,
    const EnvironmentParams& params)
{
    double eta_bar = 0.5 * (eta_A + eta_B);

    double gamma = 0.0;
    switch (ch) {
        case Channel::Steric:
            gamma = params.gamma_steric;
            break;
        case Channel::Electrostatic:
            gamma = params.gamma_elec;
            break;
        case Channel::Dispersion:
            gamma = params.gamma_disp;
            break;
        default:
            break;
    }

    double factor = 1.0 + gamma * eta_bar;

    // Invariant: kernel must not change sign
    if (factor <= 0.0) factor = 1e-10;

    return factor;
}

// ============================================================================
// Environment-Modulated Kernel
// ============================================================================

/**
 * Evaluate the environment-modulated per-channel, per-l radial kernel:
 *
 *   K_k(l, r; eta_A, eta_B) = K_k(l, r) * g_k(eta_A, eta_B)
 *
 * @param ch             Channel type
 * @param l              Angular momentum order
 * @param r              Bead-bead separation (A)
 * @param eta_A          Internal state of bead A
 * @param eta_B          Internal state of bead B
 * @param kernel_params  Kernel parameters (alpha_s, C6)
 * @param env_params     Environment parameters (gamma values)
 * @return Modulated kernel value
 */
inline double modulated_channel_kernel(
    Channel ch, int l, double r,
    double eta_A, double eta_B,
    const ChannelKernelParams& kernel_params,
    const EnvironmentParams& env_params)
{
    double K_base = channel_kernel(ch, l, r, kernel_params);
    double g = kernel_modulation_factor(ch, eta_A, eta_B, env_params);
    return K_base * g;
}

// ============================================================================
// Modulation Summary (Diagnostics)
// ============================================================================

/**
 * ModulationReport — per-channel modulation factors for inspection.
 */
struct ModulationReport {
    double eta_bar{};
    double g_steric{};
    double g_electrostatic{};
    double g_dispersion{};
};

/**
 * Compute the modulation report for a bead pair.
 */
inline ModulationReport compute_modulation_report(
    double eta_A, double eta_B,
    const EnvironmentParams& params)
{
    ModulationReport report;
    report.eta_bar = 0.5 * (eta_A + eta_B);
    report.g_steric = kernel_modulation_factor(
        Channel::Steric, eta_A, eta_B, params);
    report.g_electrostatic = kernel_modulation_factor(
        Channel::Electrostatic, eta_A, eta_B, params);
    report.g_dispersion = kernel_modulation_factor(
        Channel::Dispersion, eta_A, eta_B, params);
    return report;
}

} // namespace coarse_grain
