#pragma once
/**
 * eam_sutton_chen.hpp  -  Sutton-Chen Embedded Atom Method (EAM)
 *
 * Implements the Sutton-Chen analytical EAM for FCC metals:
 *
 *   U_total = eps * [ (1/2) * Sum_{i!=j} V(r_ij)  -  c * Sum_i sqrt(rho_i) ]
 *
 * where:
 *   V(r_ij) = (a/r_ij)^n         -- pairwise repulsion
 *   rho_i   = Sum_{j!=i} phi(r_ij)
 *   phi(r)  = (a/r)^m            -- density contribution
 *
 * Forces:
 *   dU/dr_ij involves both the pairwise term and the embedding gradient:
 *
 *   F_i = -Sum_j { [dV/dr + (dF/drho_i + dF/drho_j) * dphi/dr] * r_hat_ij }
 *
 *   where dF/drho_i = -eps * c / (2 * sqrt(rho_i))
 *
 * This is an O(N^2) two-pass algorithm:
 *   Pass 1: Compute rho_i for all i
 *   Pass 2: Compute forces using rho_i
 *
 * Parameter table:
 *   Sutton & Chen (1990) + Rafii-Tabar & Sutton (1991) values.
 *
 * Units: Ang, kcal/mol throughout (consistent with the rest of the stack).
 *   eps in eV is converted: 1 eV = 23.0609 kcal/mol
 *
 * LAMMPS gap closed: WO-LAMMPS-GAP-01 item 3.8
 *
 * References:
 *   Sutton, A.P. & Chen, J. (1990). Phil. Mag. Lett. 61(3): 139-146.
 *   Rafii-Tabar, H. & Sutton, A.P. (1991). Phil. Mag. Lett. 63(4): 217-224.
 *   Kimura, Y. et al. (1998). JPSJ 67: 1100. [Au/Ag/Cu table]
 */

#include "model.hpp"
#include "../core/state.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace atomistic {

// ============================================================================
// Sutton-Chen parameter table
// ============================================================================

struct SuttonChenParams {
	std::string symbol;
	int         Z;
	double      a;    ///< Lattice parameter (Ang) -- sets the length scale
	double      eps;  ///< Energy scale (kcal/mol) -- converted from eV
	double      c;    ///< Dimensionless cohesion coefficient
	int         n;    ///< Repulsion exponent
	int         m;    ///< Density exponent (m < n always)
	double      rc;   ///< Default cutoff (Ang) -- typically 2*a
};

// eV -> kcal/mol conversion: 1 eV = 23.0609 kcal/mol
static constexpr double EV_TO_KCAL = 23.0609;

/**
 * Sutton-Chen parameters for FCC metals.
 *
 * Source: Sutton & Chen (1990) Table I; Rafii-Tabar & Sutton (1991) Table I.
 * eps column: original paper values in eV, multiplied by EV_TO_KCAL.
 */
static const std::map<int, SuttonChenParams> SC_PARAMS_BY_Z = {
	//  Z   sym     a(Ang)   eps(kcal/mol)           c        n   m   rc(Ang)
	{13, {"Al", 13, 4.050, 0.033147 * EV_TO_KCAL, 16.399,   7, 6,  8.5}},
	{28, {"Ni", 28, 3.520, 0.015707 * EV_TO_KCAL, 39.432,   9, 6,  8.0}},
	{29, {"Cu", 29, 3.615, 0.012382 * EV_TO_KCAL, 39.432,   9, 6,  8.0}},
	{46, {"Pd", 46, 3.890, 0.004179 * EV_TO_KCAL, 108.526, 12, 7,  8.5}},
	{47, {"Ag", 47, 4.090, 0.0025415 * EV_TO_KCAL, 144.410, 12, 6,  9.0}},
	{78, {"Pt", 78, 3.920, 0.019833 * EV_TO_KCAL, 34.408,  10, 8,  8.5}},
	{79, {"Au", 79, 4.080, 0.012793 * EV_TO_KCAL, 34.408,  10, 8,  9.0}},
	{82, {"Pb", 82, 4.950, 0.0055765 * EV_TO_KCAL, 45.778, 10, 7, 10.5}},
	// BCC approx: Fe (Morse is better, but SC gives reasonable cohesion)
	{26, {"Fe", 26, 2.870, 0.013386 * EV_TO_KCAL, 52.977,  8,  5,  7.0}},
};

/**
 * Look up SC parameters by atomic number Z.
 */
inline const SuttonChenParams* get_sc_params(int Z) {
	auto it = SC_PARAMS_BY_Z.find(Z);
	if (it != SC_PARAMS_BY_Z.end())
		return &it->second;
	return nullptr;
}

// ============================================================================
// Sutton-Chen pair functions
// ============================================================================

/// Pairwise repulsion: V(r) = (a/r)^n
inline double sc_pair_V(double r, const SuttonChenParams& p) {
	const double x = p.a / r;
	double result = 1.0;
	for (int k = 0; k < p.n; ++k) result *= x;
	return result;  // x^n
}

/// dV/dr = -n * a^n / r^(n+1) = -n/r * V(r)
inline double sc_pair_dV_dr(double r, const SuttonChenParams& p) {
	return -static_cast<double>(p.n) / r * sc_pair_V(r, p);
}

/// Density contribution: phi(r) = (a/r)^m
inline double sc_phi(double r, const SuttonChenParams& p) {
	const double x = p.a / r;
	double result = 1.0;
	for (int k = 0; k < p.m; ++k) result *= x;
	return result;
}

/// dphi/dr = -m/r * phi(r)
inline double sc_phi_dr(double r, const SuttonChenParams& p) {
	return -static_cast<double>(p.m) / r * sc_phi(r, p);
}

/// Embedding function derivative: dF/drho = -eps*c / (2*sqrt(rho))
inline double sc_dF_drho(double rho, const SuttonChenParams& p) {
	if (rho < 1.0e-12) return 0.0;
	return -p.eps * p.c / (2.0 * std::sqrt(rho));
}

// ============================================================================
// Smooth cutoff
// ============================================================================

/**
 * Cosine taper: f(r) = 0.5*(1+cos(pi*(r-r1)/(r2-r1)))  for r in [r1, r2].
 * Returns 1 at r <= r1, 0 at r >= r2.
 */
inline double cosine_taper(double r, double r1, double r2) {
	if (r <= r1) return 1.0;
	if (r >= r2) return 0.0;
	const double x = (r - r1) / (r2 - r1);
	return 0.5 * (1.0 + std::cos(M_PI * x));
}

/// Derivative of cosine taper
inline double cosine_taper_dr(double r, double r1, double r2) {
	if (r <= r1 || r >= r2) return 0.0;
	const double x = (r - r1) / (r2 - r1);
	return -0.5 * M_PI / (r2 - r1) * std::sin(M_PI * x);
}

// ============================================================================
// EAM model: IModel subclass
// ============================================================================

/**
 * SuttonChenEAM  -  Embedded Atom Method (Sutton-Chen) for FCC metals.
 *
 * Type-to-Z mapping must be set via set_z_map() before eval().
 * For mixed-metal alloys a simple arithmetic/geometric mixing is used:
 *   a_ij   = (a_i + a_j) / 2
 *   eps_ij = sqrt(eps_i * eps_j)
 *   n_ij   = (n_i + n_j) / 2  (rounded to nearest int)
 *   m_ij   = (m_i + m_j) / 2  (rounded to nearest int)
 *
 * Energy accumulates into EnergyTerms::UvdW (many-body non-bonded channel).
 * Note: EAM energy is non-pairwise decomposable. UvdW here is the full EAM
 * total; the Ubond/Uangle channels should be zero for pure metal runs.
 *
 * Usage:
 *   auto eam = std::make_unique<SuttonChenEAM>();
 *   eam->set_z_map({{0, 29}, {1, 28}});   // type 0 = Cu, type 1 = Ni
 *   eam->eval(state, params);
 */
class SuttonChenEAM : public IModel {
public:
	std::map<uint32_t, int> z_map;   ///< state.type[i] -> atomic number Z

	explicit SuttonChenEAM() = default;

	void set_z_map(std::map<uint32_t, int> m) { z_map = std::move(m); }

	void eval(State& s, const ModelParams& /*p*/) const override
	{
		const uint32_t N = s.N;
		if (N < 2) return;

		// ---- Pass 1: compute electron density rho_i for all i ----

		std::vector<double> rho(N, 0.0);

		for (uint32_t i = 0; i < N; ++i) {
			const auto& pi = sc_params_for(s.type[i]);

			for (uint32_t j = 0; j < N; ++j) {
				if (i == j) continue;

				Vec3 rij = s.X[j] - s.X[i];
				if (s.box.enabled) rij = s.box.delta(s.X[i], s.X[j]);

				const double r2 = dot(rij, rij);
				const double rc = pi.rc;
				if (r2 >= rc * rc || r2 < 1.0e-12) continue;

				const double r  = std::sqrt(r2);
				const auto&  pj = sc_params_for(s.type[j]);

				// phi contribution from j to density at i: use mean m
				const int m_ij = (pi.m + pj.m + 1) / 2;  // round up
				const double a_ij = (pi.a + pj.a) * 0.5;
				const double phi  = std::pow(a_ij / r, m_ij);
				const double tap  = cosine_taper(r, rc * 0.85, rc);
				rho[i] += phi * tap;
			}
		}

		// ---- Pass 2: forces and energy ----

		double U_total = 0.0;

		for (uint32_t i = 0; i < N - 1; ++i) {
			const auto&  pi     = sc_params_for(s.type[i]);
			const double rho_i  = rho[i];
			const double dFi    = sc_dF_drho(rho_i, pi);

			// Embedding energy for i (add once)
			if (rho_i > 0)
				U_total -= pi.eps * pi.c * std::sqrt(rho_i);

			for (uint32_t j = i + 1; j < N; ++j) {
				Vec3 rij = s.X[j] - s.X[i];
				if (s.box.enabled) rij = s.box.delta(s.X[i], s.X[j]);

				const double r2 = dot(rij, rij);
				const double rc = std::min(pi.rc, sc_params_for(s.type[j]).rc);
				if (r2 >= rc * rc || r2 < 1.0e-12) continue;

				const double r   = std::sqrt(r2);
				const auto&  pj  = sc_params_for(s.type[j]);
				const double dFj = sc_dF_drho(rho[j], pj);

				// Mixed pair parameters
				const double a_ij   = (pi.a  + pj.a)  * 0.5;
				const double eps_ij = std::sqrt(pi.eps * pj.eps);
				const int    n_ij   = (pi.n  + pj.n  + 1) / 2;
				const int    m_ij   = (pi.m  + pj.m  + 1) / 2;

				const double tap  = cosine_taper(r, rc * 0.85, rc);
				const double dtap = cosine_taper_dr(r, rc * 0.85, rc);

				// Repulsive pair V and its derivative
				const double V    = std::pow(a_ij / r, n_ij);
				const double dV   = -static_cast<double>(n_ij) / r * V;

				// Density phi and its derivative
				const double phi  = std::pow(a_ij / r, m_ij);
				const double dphi = -static_cast<double>(m_ij) / r * phi;

				// Pair energy contribution
				U_total += eps_ij * V * tap;

				// Total scalar force (dU/dr) on i from j:
				//   dU/dr = eps*[dV/dr * tap + V * dtap]
				//         + (dFi + dFj) * [dphi/dr * tap + phi * dtap]
				const double dU_dr =
					eps_ij * (dV * tap + V * dtap) +
					(dFi + dFj) * (dphi * tap + phi * dtap);

				// Force on i from j: F_i += -dU/dr * r_hat = -dU_dr/r * rij
				const double f_mag = -dU_dr / r;  // negative: force points along rij when attractive
				const Vec3 fvec = rij * f_mag;
				s.F[i] = s.F[i] + fvec;
				s.F[j] = s.F[j] - fvec;
			}
		}

		// Add embedding energy of last atom
		{
			const uint32_t i   = N - 1;
			const auto&    pi  = sc_params_for(s.type[i]);
			if (rho[i] > 0)
				U_total -= pi.eps * pi.c * std::sqrt(rho[i]);
		}

		s.E.UvdW += U_total;
	}

private:
	const SuttonChenParams& sc_params_for(uint32_t tid) const {
		auto zit = z_map.find(tid);
		int Z = (zit != z_map.end()) ? zit->second : 29;  // Cu fallback
		auto pit = SC_PARAMS_BY_Z.find(Z);
		if (pit != SC_PARAMS_BY_Z.end())
			return pit->second;
		static const SuttonChenParams cu_fallback =
			SC_PARAMS_BY_Z.at(29);
		return cu_fallback;
	}
};

} // namespace atomistic
