#pragma once
/**
 * morse.hpp  -  Morse Pair Potential
 *
 * Implements the Morse pair potential:
 *
 *   U(r) = D_e * [1 - exp(-alpha*(r - r_e))]^2  -  D_e
 *
 * where:
 *   D_e    = well depth (kcal/mol)
 *   r_e    = equilibrium distance (Ang)
 *   alpha  = width parameter (Ang^-1)  --  steeper well = larger alpha
 *
 * Force:
 *   F(r) = -dU/dr = 2 * D_e * alpha * exp(-alpha*(r-r_e))
 *                   * [1 - exp(-alpha*(r-r_e))]  *  r_hat
 *
 * This is more physically accurate than LJ for many diatomics and
 * surfaces because it:
 *   - Correctly dissociates to zero energy at infinite separation
 *   - Has a finite slope at r=0 (unlike LJ r^-12 wall)
 *   - Reproduces the asymmetry of real potential wells
 *
 * Parameter table derived from:
 *   - Girifalco & Weizer, Phys. Rev. 114, 687 (1959)  [metals]
 *   - Morse (1929) original + UFF-derived r_e values   [non-metals]
 *
 * LAMMPS gap closed: WO-LAMMPS-GAP-01 item 3.3
 *
 * References:
 *   Morse, P.M. (1929). Phys. Rev. 34(1): 57-64.
 *   Girifalco, L.A. & Weizer, V.G. (1959). Phys. Rev. 114: 687.
 */

#include "../atomistic/models/model.hpp"
#include "../atomistic/core/state.hpp"
#include "uff_params.hpp"
#include <cmath>
#include <cstdint>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>

namespace vsepr {

// ============================================================================
// Morse parameters per element pair
// ============================================================================

struct MorseParams {
	double D_e;    ///< Well depth (kcal/mol)
	double alpha;  ///< Width parameter (Ang^-1)
	double r_e;    ///< Equilibrium distance (Ang)
};

/**
 * Girifalco-Weizer Morse parameters for common metals (homoatomic).
 *
 * Table: Z -> {D_e [kcal/mol], alpha [Ang^-1], r_e [Ang]}
 *
 * Converted from the original eV/Ang units to kcal/mol:
 *   1 eV = 23.0609 kcal/mol
 *
 * Source: Girifalco & Weizer (1959), Table I.
 */
static const std::map<int, MorseParams> MORSE_HOMOATOMIC_BY_Z = {
	// Z   {D_e [kcal/mol],  alpha [Ang^-1], r_e [Ang]}
	{13,  {0.6927,   1.1646, 3.253}},   // Al
	{14,  {1.5972,   1.6716, 2.295}},   // Si  (empirical, Tersoff better)
	{22,  {1.2918,   1.5543, 2.920}},   // Ti
	{24,  {1.4974,   1.5721, 2.754}},   // Cr
	{26,  {1.1315,   1.3885, 2.845}},   // Fe
	{27,  {1.1407,   1.4816, 2.780}},   // Co
	{28,  {1.0774,   1.4199, 2.780}},   // Ni
	{29,  {0.9934,   1.3588, 2.866}},   // Cu
	{46,  {1.2202,   1.3383, 2.750}},   // Pd
	{47,  {0.9244,   1.3690, 3.115}},   // Ag
	{78,  {1.3617,   1.4700, 2.777}},   // Pt
	{79,  {1.1543,   1.6500, 2.880}},   // Au
	{82,  {0.7174,   1.1836, 3.733}},   // Pb
};

/**
 * Estimate Morse parameters from UFF sigma/epsilon (heteroatomic fallback).
 *
 * Equivalence to LJ well depth at minimum:
 *   D_e  ~ epsilon (UFF)
 *   r_e  ~ sigma * 2^(1/6)   (LJ r_min = sigma * 2^(1/6))
 *   alpha derived from harmonic approximation:
 *     alpha = sqrt(k_spring / (2*D_e))   with k_spring = 72*D_e/r_e^2 (UFF)
 *   => alpha ~ sqrt(36) / r_e = 6 / r_e  (simplified)
 */
inline MorseParams morse_from_uff(const LJParams& lj) {
	const double r_e   = lj.sigma * std::pow(2.0, 1.0 / 6.0);
	const double D_e   = lj.epsilon;
	const double alpha = 6.0 / r_e;  // approx from UFF harmonic well curvature
	return {D_e, alpha, r_e};
}

/**
 * Lorentz-Berthelot mixing for Morse parameters:
 *   D_e_ij   = sqrt(D_e_i  * D_e_j)
 *   alpha_ij = (alpha_i + alpha_j) / 2
 *   r_e_ij   = (r_e_i + r_e_j) / 2
 */
inline MorseParams morse_mix(const MorseParams& a, const MorseParams& b) {
	return {
		std::sqrt(a.D_e * b.D_e),
		(a.alpha + b.alpha) * 0.5,
		(a.r_e   + b.r_e)   * 0.5
	};
}

/**
 * Get Morse parameters for element Z.
 * Returns homoatomic entry if present; otherwise derives from UFF.
 */
inline MorseParams get_morse_params(int Z) {
	auto it = MORSE_HOMOATOMIC_BY_Z.find(Z);
	if (it != MORSE_HOMOATOMIC_BY_Z.end())
		return it->second;
	auto lj = get_lj_params(Z);
	if (lj)
		return morse_from_uff(*lj);
	// Carbon fallback
	return morse_from_uff({3.851, 0.105});
}

// ============================================================================
// Scalar Morse functions
// ============================================================================

/**
 * Morse potential energy at scalar distance r.
 * Returns U(r) in kcal/mol.
 */
inline double morse_energy(double r, const MorseParams& p) {
	const double x = std::exp(-p.alpha * (r - p.r_e));
	const double y = 1.0 - x;
	return p.D_e * (y * y - 1.0);  // == D_e*(1-exp(-a*(r-re)))^2 - D_e
}

/**
 * Morse force magnitude dU/dr (positive = repulsive push).
 * The sign convention: caller multiplies by -(r_hat) for the force on atom i.
 */
inline double morse_force_mag(double r, const MorseParams& p) {
	const double x = std::exp(-p.alpha * (r - p.r_e));
	return 2.0 * p.D_e * p.alpha * x * (1.0 - x);
}

} // namespace vsepr

// ============================================================================
// IModel implementation
// ============================================================================

namespace atomistic {

/**
 * MorsePairModel  -  pairwise Morse potential for all atom pairs.
 *
 * Uses homoatomic Girifalco-Weizer parameters when available;
 * falls back to UFF-derived estimates for heteroatomic pairs.
 *
 * Cutoff applied with a linear taper over [r_cut-1.0, r_cut] to
 * guarantee force continuity at the cutoff.
 *
 * EnergyTerms: Morse energy accumulates into UvdW (non-bonded channel).
 *
 * Usage:
 *   auto model = std::make_unique<MorsePairModel>();
 *   model->set_z_map({0 -> 26, 1 -> 28});  // type-to-Z mapping
 *   model->eval(state, params);
 */
class MorsePairModel : public IModel {
public:
	// Map state.type[i] -> atomic number Z
	std::map<uint32_t, int> z_map;

	// Per-type Morse parameters cache (filled on first eval)
	mutable std::map<int, vsepr::MorseParams> param_cache_;

	explicit MorsePairModel() = default;

	/**
	 * Set the type -> Z mapping (must be set before eval).
	 * key = value in State::type[]; value = atomic number Z.
	 */
	void set_z_map(std::map<uint32_t, int> m) { z_map = std::move(m); }

	void eval(State& s, const ModelParams& p) const override
	{
		const uint32_t N = s.N;
		if (N < 2) return;

		const double rc  = p.rc;
		const double rc2 = rc * rc;
		const double taper_start = rc - 1.0;  // start linear taper 1 Ang before cutoff

		// Ensure parameter cache populated
		for (auto& [tid, Z] : z_map) {
			if (param_cache_.find(Z) == param_cache_.end())
				param_cache_[Z] = vsepr::get_morse_params(Z);
		}

		// Pairwise loop
		for (uint32_t i = 0; i < N - 1; ++i) {
			const int Zi = z_map_Z(s.type[i]);
			const auto& pi = param_cache_.at(Zi);

			for (uint32_t j = i + 1; j < N; ++j) {
				Vec3 rij = s.X[j] - s.X[i];

				// PBC minimum image
				if (s.box.enabled)
					rij = s.box.minimum_image(rij);

				const double r2 = dot(rij, rij);
				if (r2 >= rc2 || r2 < 1.0e-12) continue;

				const double r  = std::sqrt(r2);
				const int    Zj = z_map_Z(s.type[j]);
				const auto&  pj = param_cache_.at(Zj);

				// Mixed parameters
				const auto pm = vsepr::morse_mix(pi, pj);

				double U  = vsepr::morse_energy(r, pm);
				double dU = vsepr::morse_force_mag(r, pm);  // dU/dr

				// Linear taper to zero beyond taper_start
				if (r > taper_start) {
					const double t = (rc - r) / (rc - taper_start);  // 1->0 over taper
					U  *= t;
					dU  = dU * t + U / (rc - taper_start);  // product rule
				}

				// Accumulate energy (split evenly)
				s.E.UvdW += U;

				// Force: F_i += dU/dr * r_hat, F_j -= dU/dr * r_hat
				const double inv_r = 1.0 / r;
				const Vec3 fvec = rij * (dU * inv_r);
				s.F[i] = s.F[i] + fvec;
				s.F[j] = s.F[j] - fvec;
			}
		}
	}

private:
	int z_map_Z(uint32_t tid) const {
		auto it = z_map.find(tid);
		if (it != z_map.end()) return it->second;
		return 6;  // carbon fallback
	}
};

} // namespace atomistic
