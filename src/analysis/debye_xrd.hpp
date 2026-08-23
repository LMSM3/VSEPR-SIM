#pragma once
/**
 * debye_xrd.hpp  -  Atomistic Debye Scattering (XRD / neutron powder)
 *
 * Computes powder diffraction patterns directly from an atomistic::State
 * using the Debye scattering equation:
 *
 *   I(q) = Sum_i f_i(q)^2  +  2 * Sum_{i<j} f_i(q)*f_j(q) * sin(q*r_ij)/(q*r_ij)
 *
 * where:
 *   q      = 4*pi*sin(theta)/lambda   (Ang^-1)
 *   r_ij   = |X_i - X_j|             (Ang)
 *   f_i(q) = Gaussian form factor for element Z_i
 *
 * Gaussian form factor (Cromer-Mann approximation, 1-Gaussian):
 *   f(q, Z) = Z * exp(-B * q^2 / (16*pi^2))
 *   B = Debye-Waller factor (Ang^2). Default B=1.0 for room temperature.
 *
 * For PBC systems the sum is over all pairs within the simulation box.
 * For non-PBC systems the sum is over all N*(N-1)/2 unique pairs.
 *
 * This is an O(N^2 * n_q) algorithm; suitable for N < ~2000.
 *
 * Output: DebyeXrdProfile with:
 *   - Per-bin (q, 2theta, intensity, d-spacing) table
 *   - Peak position (q, d, 2theta) and crystallinity index
 *   - has_bragg_peaks flag (crystallinity_index > 5)
 *
 * LAMMPS gap closed: WO-LAMMPS-GAP-01 item 11.7
 *
 * References:
 *   Debye, P. (1915). Ann. Phys. 351(6): 809-823.
 *   Warren, B.E. (1969). X-ray Diffraction. Addison-Wesley.
 *   Cromer, D.T. & Mann, J.B. (1968). Acta Cryst. A24: 321.
 *   International Tables for Crystallography, Vol. C, Table 6.1.1.4.
 */

#include "../atomistic/core/state.hpp"
#include <cmath>
#include <cstdint>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace vsepr {
namespace xrd {

// ============================================================================
// Data types
// ============================================================================

struct XrdPoint {
	double q;              ///< Scattering vector (Ang^-1)
	double two_theta_deg;  ///< 2*theta (degrees) at lambda
	double intensity;      ///< I(q) -- Debye sum
	double d_spacing;      ///< d = 2*pi/q (Ang)
};

struct DebyeXrdProfile {
	uint32_t                n_atoms{};
	uint32_t                n_q_bins{};
	double                  q_min{};
	double                  q_max{};
	double                  lambda_ang{};     ///< X-ray wavelength (Ang)
	std::vector<XrdPoint>   points;

	// Diagnostics
	double  peak_q{};
	double  peak_d{};
	double  peak_two_theta{};
	double  peak_intensity{};
	double  crystallinity_index{};   ///< peak_I / mean_I
	bool    has_bragg_peaks{};       ///< crystallinity_index > 5
	bool    pbc_used{};              ///< true if PBC minimum image was applied
};

// ============================================================================
// Five-Gaussian Cromer-Mann form factors (partial, Z=1-30)
//
// Full table: International Tables Vol. C, Table 6.1.1.4.
// For elements not in this table, the 1-Gaussian approximation is used:
//   f(q) = Z * exp(-B * q^2 / (16*pi^2))
//
// Stored as {a1,b1, a2,b2, a3,b3, a4,b4, c} -- 9 coefficients per element.
// f(s) = sum_k a_k * exp(-b_k * s^2) + c,  where s = q/(4*pi) (Ang^-1)
// ============================================================================

struct CromerMannCoeffs {
	double a[4], b[4], c;
};

static const std::map<int, CromerMannCoeffs> CROMER_MANN = {
	//  Z     a1       a2       a3       a4       b1       b2       b3       b4       c
	{ 1, {{ 0.489918, 0.262003, 0.196767, 0.049879}, {20.6593, 7.74039, 49.5519, 2.20159}, 0.001305}},
	{ 6, {{ 2.31000,  1.02000,  1.58860,  0.865000}, {20.8439, 10.2075, 0.56870, 51.6512}, 0.215600}},
	{ 7, {{ 12.2126,  3.13220,  2.01250,  1.16630 }, {0.00570, 9.89330, 28.9975, 0.58260}, -11.5290}},
	{ 8, {{ 3.04850,  2.28680,  1.54630,  0.867000}, {13.2771, 5.70110, 0.32390, 32.9089}, 0.250800}},
	{11, {{ 4.76260,  3.17360,  1.26740,  1.11280 }, {3.28500, 8.84220, 0.31370, 129.424}, 0.676000}},
	{12, {{ 5.42040,  2.17350,  1.22690,  2.30730 }, {2.82750, 79.2611, 0.38080, 7.19370}, 0.858400}},
	{13, {{ 6.42020,  1.90020,  1.59360,  1.96460 }, {3.03870, 0.74260, 31.5472, 85.0886}, 1.11510}},
	{14, {{ 6.29150,  3.03530,  1.98910,  1.54100 }, {2.43860, 32.3337, 0.67850, 81.6937}, 1.14070}},
	{16, {{ 6.90530,  5.20340,  1.43790,  1.58630 }, {1.46790, 22.2151, 0.25360, 56.1720}, 0.866900}},
	{17, {{ 11.4604,  7.19640,  6.25560,  1.64550 }, {0.01040, 1.16620, 18.5194, 47.7784}, -9.55740}},
	{18, {{ 7.48450,  6.77230,  0.65390,  1.64420 }, {0.90720, 14.8407, 43.8983, 33.3929}, 1.44450}},
	{20, {{ 8.62660,  7.38730,  1.58990,  1.02110 }, {10.4421, 0.65990, 85.7484, 178.437}, 1.37510}},
	{26, {{ 11.7695,  7.35730,  3.52220,  2.30450 }, {4.76110, 0.30720, 15.3535, 76.8805}, 1.03690}},
	{28, {{ 12.8376,  7.29200,  4.44380,  2.38000 }, {3.87850, 0.25650, 12.1763, 66.3421}, 1.03410}},
	{29, {{ 13.3380,  7.16760,  5.61580,  1.67350 }, {3.58280, 0.24700, 11.3966, 64.8126}, 1.19100}},
	{30, {{ 14.0743,  7.03180,  5.16520,  2.41000 }, {3.26550, 0.23330, 10.3163, 58.7097}, 1.30410}},
	{47, {{ 19.2808,  16.6885,  4.80450,  1.04630 }, {0.64460, 7.47260, 24.6605, 99.8156}, 5.17900}},
	{79, {{ 16.8819,  18.5913,  25.5582,  5.86000 }, {0.46130, 8.62160, 1.48260, 36.3956}, 12.0658}},
};

// ============================================================================
// Form factor computation
// ============================================================================

/**
 * Compute atomic form factor f(q) for element Z.
 *
 * Uses 4-Gaussian Cromer-Mann if Z is in the table,
 * otherwise uses 1-Gaussian approximation:
 *   f(q, Z) = Z * exp(-B * q^2 / (16*pi^2))
 *
 * @param Z        Atomic number
 * @param q        Scattering vector magnitude (Ang^-1)
 * @param B_factor Debye-Waller factor (Ang^2, default 1.0)
 */
inline double atomic_form_factor(int Z, double q, double B_factor = 1.0) {
	constexpr double INV_16PI2 = 1.0 / (16.0 * M_PI * M_PI);
	const double s = q / (4.0 * M_PI);  // s = sin(theta)/lambda (Ang^-1)
	const double s2 = s * s;

	auto it = CROMER_MANN.find(Z);
	if (it != CROMER_MANN.end()) {
		const auto& cm = it->second;
		double f = cm.c;
		for (int k = 0; k < 4; ++k)
			f += cm.a[k] * std::exp(-cm.b[k] * s2);
		// Apply Debye-Waller attenuation
		f *= std::exp(-B_factor * q * q * INV_16PI2);
		return f;
	}

	// Gaussian fallback
	return static_cast<double>(Z) * std::exp(-B_factor * q * q * INV_16PI2);
}

// ============================================================================
// Main computation
// ============================================================================

/**
 * Options for Debye XRD computation.
 */
struct DebyeXrdOptions {
	uint32_t n_q_bins   = 512;       ///< Number of q bins
	double   q_min      = 0.3;       ///< Minimum q (Ang^-1)  -- 2pi/q ~ 21 Ang
	double   q_max      = 8.0;       ///< Maximum q (Ang^-1)  -- d ~ 0.79 Ang
	double   lambda_ang = 1.54056;   ///< Cu K-alpha wavelength (Ang)
	double   B_factor   = 1.0;       ///< Debye-Waller factor (Ang^2)
	bool     use_pbc    = true;      ///< Apply PBC minimum image to pair distances
};

/**
 * Compute Debye XRD powder pattern from an atomistic::State.
 *
 * @param s        Atomistic state (positions, types)
 * @param z_map    Map from state.type[i] -> atomic number Z
 * @param opts     Computation options
 * @return         DebyeXrdProfile with full pattern and diagnostics
 */
inline DebyeXrdProfile compute_debye_xrd(
	const atomistic::State&          s,
	const std::map<uint32_t, int>&   z_map,
	const DebyeXrdOptions&           opts = {})
{
	DebyeXrdProfile prof;
	prof.n_atoms   = s.N;
	prof.n_q_bins  = opts.n_q_bins;
	prof.q_min     = opts.q_min;
	prof.q_max     = opts.q_max;
	prof.lambda_ang = opts.lambda_ang;
	prof.pbc_used  = (opts.use_pbc && s.box.enabled);

	if (s.N == 0) return prof;

	const uint32_t N  = s.N;
	const double   dq = (opts.q_max - opts.q_min) / std::max(opts.n_q_bins - 1u, 1u);

	// -- Precompute form factors per atom per q bin --
	// f[i][k] = atomic_form_factor(Z_i, q_k)
	std::vector<std::vector<double>> ff(N, std::vector<double>(opts.n_q_bins));
	for (uint32_t i = 0; i < N; ++i) {
		auto zit = z_map.find(s.type[i]);
		const int Z = (zit != z_map.end()) ? zit->second : 6;
		for (uint32_t k = 0; k < opts.n_q_bins; ++k) {
			const double q = opts.q_min + k * dq;
			ff[i][k] = atomic_form_factor(Z, q, opts.B_factor);
		}
	}

	// -- Precompute pair distances --
	struct Pair { double r; uint32_t i; uint32_t j; };
	std::vector<Pair> pairs;
	pairs.reserve(N * (N - 1) / 2);

	for (uint32_t i = 0; i < N; ++i) {
		for (uint32_t j = i + 1; j < N; ++j) {
			atomistic::Vec3 rij = s.X[j] - s.X[i];
			if (prof.pbc_used)
				rij = s.box.minimum_image(rij);
			const double r = std::sqrt(atomistic::dot(rij, rij));
			if (r > 1.0e-6)
				pairs.push_back({r, i, j});
		}
	}

	// -- Compute I(q) for each bin --
	prof.points.resize(opts.n_q_bins);
	double sum_I = 0.0;
	double max_I = 0.0;
	uint32_t max_k = 0;

	for (uint32_t k = 0; k < opts.n_q_bins; ++k) {
		const double q = opts.q_min + k * dq;

		// Self-scattering: Sum_i f_i^2
		double I = 0.0;
		for (uint32_t i = 0; i < N; ++i)
			I += ff[i][k] * ff[i][k];

		// Cross-terms: 2 * Sum_{i<j} f_i * f_j * sinc(q*r_ij)
		for (const auto& pair : pairs) {
			const double qr   = q * pair.r;
			const double sinc = (qr > 1.0e-8) ? std::sin(qr) / qr : 1.0;
			I += 2.0 * ff[pair.i][k] * ff[pair.j][k] * sinc;
		}

		// 2*theta from Bragg's law: lambda = 2*d*sin(theta)
		const double sin_theta = q * opts.lambda_ang / (4.0 * M_PI);
		const double two_theta = (sin_theta <= 1.0)
			? 2.0 * std::asin(sin_theta) * 180.0 / M_PI
			: 180.0;
		const double d_spacing = (q > 1.0e-8) ? 2.0 * M_PI / q : 0.0;

		prof.points[k] = {q, two_theta, I, d_spacing};
		sum_I += I;
		if (I > max_I) { max_I = I; max_k = k; }
	}

	// -- Diagnostics --
	const double mean_I = sum_I / std::max(opts.n_q_bins, 1u);
	prof.peak_intensity     = max_I;
	prof.peak_q             = prof.points[max_k].q;
	prof.peak_d             = prof.points[max_k].d_spacing;
	prof.peak_two_theta     = prof.points[max_k].two_theta_deg;
	prof.crystallinity_index = (mean_I > 0) ? max_I / mean_I : 0.0;
	prof.has_bragg_peaks    = (prof.crystallinity_index > 5.0);

	return prof;
}

// ============================================================================
// Export helpers
// ============================================================================

/**
 * Export XRD profile as CSV string.
 * Columns: q_inv_ang, two_theta_deg, intensity, d_spacing_ang
 */
inline std::string xrd_to_csv(const DebyeXrdProfile& prof) {
	std::ostringstream oss;
	oss << "q_inv_ang,two_theta_deg,intensity,d_spacing_ang\n";
	char buf[128];
	for (const auto& p : prof.points) {
		std::snprintf(buf, sizeof(buf), "%.6f,%.4f,%.6e,%.4f\n",
					  p.q, p.two_theta_deg, p.intensity, p.d_spacing);
		oss << buf;
	}
	return oss.str();
}

/**
 * Export XRD profile as a minimal JSON string.
 */
inline std::string xrd_to_json(const DebyeXrdProfile& prof) {
	std::ostringstream j;
	j << "{\n"
	  << "  \"n_atoms\": " << prof.n_atoms << ",\n"
	  << "  \"n_q_bins\": " << prof.n_q_bins << ",\n"
	  << "  \"lambda_ang\": " << prof.lambda_ang << ",\n"
	  << "  \"peak_q\": " << prof.peak_q << ",\n"
	  << "  \"peak_d_ang\": " << prof.peak_d << ",\n"
	  << "  \"peak_two_theta_deg\": " << prof.peak_two_theta << ",\n"
	  << "  \"crystallinity_index\": " << prof.crystallinity_index << ",\n"
	  << "  \"has_bragg_peaks\": " << (prof.has_bragg_peaks ? "true" : "false") << ",\n"
	  << "  \"pbc_used\": " << (prof.pbc_used ? "true" : "false") << "\n"
	  << "}\n";
	return j.str();
}

/**
 * ANSI terminal summary of the XRD profile.
 */
inline std::string xrd_summary_terminal(const DebyeXrdProfile& prof) {
	const char* BOLD  = "\033[1m";
	const char* CYAN  = "\033[36m";
	const char* GREEN = "\033[32m";
	const char* RESET = "\033[0m";
	char buf[512];
	std::snprintf(buf, sizeof(buf),
		"%s\n  Debye XRD Profile  (N=%u, lambda=%.4f Ang)%s\n"
		"%s  Peak q = %.4f Ang^-1  ->  d = %.3f Ang  (2theta = %.2f deg)\n"
		"  Crystallinity index = %.2f  ->  %s%s%s\n"
		"  q range: [%.2f, %.2f] Ang^-1  (%u bins)%s\n",
		BOLD, prof.n_atoms, prof.lambda_ang, RESET,
		CYAN, prof.peak_q, prof.peak_d, prof.peak_two_theta,
		prof.crystallinity_index,
		GREEN,
		prof.has_bragg_peaks ? "CRYSTALLINE (Bragg peaks present)"
							 : "POLYCRYSTALLINE / AMORPHOUS",
		RESET,
		prof.q_min, prof.q_max, prof.n_q_bins, RESET);
	return buf;
}

} // namespace xrd
} // namespace vsepr
