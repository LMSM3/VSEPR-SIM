#pragma once
/**
 * ewald_sum.hpp — Particle-Mesh Ewald (real-space + k-space) for ionic crystals
 * ================================================================================
 *
 * Computes the long-range Coulomb energy and forces for a periodic system using
 * the standard Ewald splitting:
 *
 *   E_Coulomb = E_real + E_recip + E_self
 *
 * where:
 *   E_real   = sum_{i<j} q_i q_j erfc(alpha*r) / r         (real-space, short-range)
 *   E_recip  = (2*pi/V) sum_{k≠0} exp(-k²/4alpha²)/k² |S(k)|²  (k-space)
 *   E_self   = -(alpha/sqrt(pi)) sum_i q_i²                  (self-energy correction)
 *
 * Reference: Ewald (1921), Frenkel & Smit "Understanding Molecular Simulation" Ch.12
 *
 * Design:
 *  - Orthogonal box only (triclinic: future work).
 *  - Charges supplied as flat array q[i] parallel to coords[3*i..3*i+2].
 *  - Forces accumulated INTO forces[]; caller zero-fills before calling.
 *  - Units: charges in elementary charge (e), distances in Å,
 *           energies in kcal/mol (factor = 332.0637 kcal·Å/(mol·e²)).
 *
 * Beta-8 status: IMPLEMENTED — gates crystal golden tests (NaCl, CsCl, CaF2).
 */

#include "box/pbc.hpp"   // vsepr::BoxOrtho
#include "core/math_vec3.hpp"

#include <cmath>
#include <vector>
#include <stdexcept>

namespace vsepr {

// ============================================================================
// EwaldParams — tuning knobs
// ============================================================================

struct EwaldParams {
	double alpha      = 0.3;   // Ewald splitting parameter (Å⁻¹); ~0.3 is typical
	double rcut_real  = 10.0;  // Real-space cutoff (Å); must be < L/2
	int    kmax       = 5;     // k-vector components: -kmax..+kmax in each direction
	double coulomb_k  = 332.0637; // e²/(4πε₀) in kcal·Å/(mol·e²)
};

// ============================================================================
// EwaldSum — stateless evaluator; call evaluate() each step
// ============================================================================

class EwaldSum {
public:
	EwaldSum() = default;
	explicit EwaldSum(const EwaldParams& p) : params_(p) {}

	// -----------------------------------------------------------------------
	// evaluate() — accumulate Ewald energy and forces
	//
	// coords  : flat array [x0,y0,z0, x1,y1,z1, ...]  (must be pre-wrapped)
	// charges : charges[i] in elementary charge units
	// box     : orthogonal periodic box (must have enabled() == true)
	// forces  : flat force array (accumulated, NOT zeroed here)
	//
	// Returns total Coulomb energy in kcal/mol.
	// -----------------------------------------------------------------------
	double evaluate(const std::vector<double>& coords,
					const std::vector<double>& charges,
					const BoxOrtho& box,
					std::vector<double>& forces) const
	{
		if (!box.enabled)
			throw std::runtime_error("EwaldSum: box must be enabled (PBC required)");

		const size_t N = coords.size() / 3;
		if (charges.size() != N)
			throw std::runtime_error("EwaldSum: charges.size() != N_atoms");
		if (forces.size() != coords.size())
			throw std::runtime_error("EwaldSum: forces.size() != coords.size()");

		double E_real   = eval_real(coords, charges, box, forces);
		double E_recip  = eval_recip(coords, charges, box, forces);
		double E_self   = eval_self(charges);

		return E_real + E_recip + E_self;
	}

	const EwaldParams& params() const { return params_; }
	void set_params(const EwaldParams& p) { params_ = p; }

private:
	EwaldParams params_;

	// ------------------------------------------------------------------
	// Real-space sum: short-range erfc-screened Coulomb
	// ------------------------------------------------------------------
	double eval_real(const std::vector<double>& coords,
					 const std::vector<double>& charges,
					 const BoxOrtho& box,
					 std::vector<double>& forces) const
	{
		const size_t N = coords.size() / 3;
		const double alpha  = params_.alpha;
		const double rcut2  = params_.rcut_real * params_.rcut_real;
		const double Ck     = params_.coulomb_k;
		double E = 0.0;

		for (size_t i = 0; i < N; ++i) {
			Vec3 ri(coords[3*i], coords[3*i+1], coords[3*i+2]);
			for (size_t j = i + 1; j < N; ++j) {
				Vec3 rj(coords[3*j], coords[3*j+1], coords[3*j+2]);
				Vec3 dr = box.delta(ri, rj);  // minimum-image displacement
				double r2 = dr.norm2();
				if (r2 > rcut2 || r2 < 1e-12) continue;

				double r   = std::sqrt(r2);
				double ar  = alpha * r;
				double qi_qj = charges[i] * charges[j] * Ck;

				double erfc_ar = std::erfc(ar);
				double exp_ar2 = std::exp(-ar * ar);

				// Energy
				E += qi_qj * erfc_ar / r;

				// Force magnitude: dE/dr * (1/r)
				// d/dr [erfc(ar)/r] = -erfc(ar)/r² - 2a/sqrt(π)*exp(-a²r²)/r
				// dEdr = qi_qj * d/dr[erfc(ar)/r]
				double dEdr = qi_qj * (
					-(erfc_ar / r2)
					- (2.0 * alpha / (std::sqrt(M_PI) * r)) * exp_ar2
				);
				// F_i = -dE/dri.  With dr = rj-ri: dE/dri = -dEdr * dr/r
				// => F_i = +dEdr * dr/r;  F_j = -dEdr * dr/r  (Newton 3rd)
				double fscale = dEdr / r;
				forces[3*i + 0] += fscale * dr.x;
				forces[3*i + 1] += fscale * dr.y;
				forces[3*i + 2] += fscale * dr.z;
				forces[3*j + 0] -= fscale * dr.x;
				forces[3*j + 1] -= fscale * dr.y;
				forces[3*j + 2] -= fscale * dr.z;
			}
		}
		return E;
	}

	// ------------------------------------------------------------------
	// Reciprocal-space sum: k-space Gaussian-screened Coulomb
	// ------------------------------------------------------------------
	double eval_recip(const std::vector<double>& coords,
					  const std::vector<double>& charges,
					  const BoxOrtho& box,
					  std::vector<double>& forces) const
	{
		const size_t N = coords.size() / 3;
		const double alpha = params_.alpha;
		const int    kmax  = params_.kmax;
		const double Ck    = params_.coulomb_k;

		const double Lx = box.L.x, Ly = box.L.y, Lz = box.L.z;
		const double V  = box.volume();
		const double prefactor = 2.0 * M_PI * Ck / V;
		const double inv_4a2   = 1.0 / (4.0 * alpha * alpha);

		double E = 0.0;

		for (int nx = -kmax; nx <= kmax; ++nx) {
		for (int ny = -kmax; ny <= kmax; ++ny) {
		for (int nz = -kmax; nz <= kmax; ++nz) {
			if (nx == 0 && ny == 0 && nz == 0) continue;

			double kx = 2.0 * M_PI * nx / Lx;
			double ky = 2.0 * M_PI * ny / Ly;
			double kz = 2.0 * M_PI * nz / Lz;
			double k2 = kx*kx + ky*ky + kz*kz;

			double Ak = prefactor * std::exp(-k2 * inv_4a2) / k2;

			// Structure factor: S(k) = sum_i q_i * exp(i k · r_i)
			double S_re = 0.0, S_im = 0.0;
			for (size_t i = 0; i < N; ++i) {
				double kdotr = kx * coords[3*i] + ky * coords[3*i+1] + kz * coords[3*i+2];
				S_re += charges[i] * std::cos(kdotr);
				S_im += charges[i] * std::sin(kdotr);
			}

			double S2 = S_re * S_re + S_im * S_im;
			E += Ak * S2;

			// Force contribution: F_i = -2 * Ak * q_i * [S_re * sin - S_im * cos] * k
			for (size_t i = 0; i < N; ++i) {
				double kdotr = kx * coords[3*i] + ky * coords[3*i+1] + kz * coords[3*i+2];
				double sin_kr = std::sin(kdotr);
				double cos_kr = std::cos(kdotr);
				double fmag = -2.0 * Ak * charges[i] * (S_re * sin_kr - S_im * cos_kr);
				forces[3*i + 0] += fmag * kx;
				forces[3*i + 1] += fmag * ky;
				forces[3*i + 2] += fmag * kz;
			}
		}}}

		return E;
	}

	// ------------------------------------------------------------------
	// Self-energy correction (removes i=j term added by k-space sum)
	// ------------------------------------------------------------------
	double eval_self(const std::vector<double>& charges) const {
		const double alpha = params_.alpha;
		const double Ck    = params_.coulomb_k;
		double sum_q2 = 0.0;
		for (double q : charges) sum_q2 += q * q;
		return -(alpha / std::sqrt(M_PI)) * Ck * sum_q2;
	}
};

} // namespace vsepr
