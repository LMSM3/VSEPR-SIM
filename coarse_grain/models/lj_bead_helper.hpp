#pragma once
/**
 * lj_bead_helper.hpp — Environment-Modulated Lennard-Jones Force for Beads
 * ========================================================================
 *
 * Reusable LJ 12-6 pairwise force with per-channel kernel modulation.
 * Follows the canonical sign convention from orientation_potential.hpp:
 *
 *   U(r) = 4 eps [ g_ster (sigma/r)^12  -  g_disp (sigma/r)^6 ]
 *   F_on_i = -dU/dr * r_hat     (r_hat points from i to j)
 *
 * Channel modulation factors (g_steric, g_dispersion) are sourced
 * from environment_coupling.hpp :: kernel_modulation_factor().
 *
 * This module centralises LJ bead-bead force evaluation so that
 * v5/bead_transport.hpp, seed_bead_stepper, and future consumers
 * share one correct implementation.
 *
 * VSEPR-SIM  |  2026-04-17
 */

#include "atomistic/core/state.hpp"
#include "coarse_grain/core/environment_state.hpp"
#include "coarse_grain/models/environment_coupling.hpp"
#include <cmath>

namespace coarse_grain {

// ============================================================================
// LJ Bead Parameters
// ============================================================================

struct LJBeadParams {
	double sigma{3.0};      // LJ size parameter (A)
	double epsilon{0.000125}; // LJ well depth (energy units, standard 4*eps convention)
};

// ============================================================================
// LJ Force Result
// ============================================================================

struct LJForceResult {
	atomistic::Vec3 force{};       // Force on bead i due to bead j
	double E_repulsive{};          // Repulsive (sr12) contribution
	double E_attractive{};         // Attractive (sr6) contribution
	double E_total{};              // Net LJ energy for this pair
	double g_steric{1.0};          // Applied steric modulation factor
	double g_dispersion{1.0};      // Applied dispersion modulation factor
};

// ============================================================================
// Modulated LJ Force
// ============================================================================

/**
 * Compute the environment-modulated LJ 12-6 force on bead i due to bead j.
 *
 *   U = 4 eps [ g_ster (s/r)^12  -  g_disp (s/r)^6 ]
 *   dU/dr = 4 eps [ -12 g_ster (s/r)^12 + 6 g_disp (s/r)^6 ] / r
 *   F_i = -dU/dr * r_hat   (r_hat = (Rj - Ri) / |Rj - Ri|)
 *
 * Returns zero force when r < 1e-10 or r > r_cutoff.
 *
 * Sign convention matches orientation_potential.hpp (line 170):
 *   F_i = -dU/dr * r_hat
 *   -> short range: repulsive (pushes i away from j)
 *   -> long range:  attractive (pulls i toward j)
 *
 * @param pos_i       Position of bead i
 * @param pos_j       Position of bead j
 * @param eta_i       Environment state eta of bead i
 * @param eta_j       Environment state eta of bead j
 * @param env_params  Environment parameters (gamma values, r_cutoff)
 * @param lj          LJ parameters (sigma, epsilon)
 * @return LJForceResult with force, energies, and applied g factors
 */
inline LJForceResult compute_lj_bead_force(
	const atomistic::Vec3& pos_i,
	const atomistic::Vec3& pos_j,
	double eta_i,
	double eta_j,
	const EnvironmentParams& env_params,
	const LJBeadParams& lj = {})
{
	LJForceResult result;

	auto dr = pos_j - pos_i;
	double r = atomistic::norm(dr);

	if (r < 1e-10 || r > env_params.r_cutoff)
		return result;

	// Channel modulation from environment coupling
	result.g_steric = kernel_modulation_factor(
		Channel::Steric, eta_i, eta_j, env_params);
	result.g_dispersion = kernel_modulation_factor(
		Channel::Dispersion, eta_i, eta_j, env_params);

	double sr  = lj.sigma / r;
	double sr2 = sr * sr;
	double sr6 = sr2 * sr2 * sr2;
	double sr12 = sr6 * sr6;

	// Energy decomposition
	result.E_repulsive  =  4.0 * lj.epsilon * result.g_steric    * sr12;
	result.E_attractive = -4.0 * lj.epsilon * result.g_dispersion * sr6;
	result.E_total = result.E_repulsive + result.E_attractive;

	// dU/dr = 4 eps [-12 g_ster sr12 + 6 g_disp sr6] / r
	double inv_r = 1.0 / r;
	double dU_dr = 4.0 * lj.epsilon
		* (-12.0 * result.g_steric * sr12 + 6.0 * result.g_dispersion * sr6)
		* inv_r;

	// F_i = -dU/dr * r_hat  (canonical convention)
	auto r_hat = dr * inv_r;
	result.force = r_hat * (-dU_dr);

	return result;
}

} // namespace coarse_grain
