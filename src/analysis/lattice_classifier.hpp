#pragma once
// =============================================================================
// src/analysis/lattice_classifier.hpp
// =============================================================================
// Per-atom and system-level lattice type classification.
// Analysis-only — nothing is stored in State or xyzFull.
//
// Method: Steinhardt bond-order parameters (Q_l, W_l) + coordination number.
//
// Q_l = sqrt(4π/(2l+1) · Σ_m |q_lm_bar|²)
//   where q_lm_bar = (1/N_b) Σ_j Y_lm(r_ij)  — averaged over neighbors
//
// W_l = Σ_{m1+m2+m3=0} (l  l  l ) q_lm1 q_lm2 q_lm3
//                        (m1 m2 m3)
//   (Wigner 3j symbols — sign of W6 discriminates FCC from HCP)
//
// Reference fingerprints (noise-free ideal structures):
//   Structure    CN   Q4      Q6      W6 (sign)
//   ─────────────────────────────────────────────
//   SC            6   0.764   0.354   ≈0
//   BCC           8   0.036   0.511   +
//   FCC          12   0.191   0.575   −
//   HCP          12   0.097   0.485   +
//   Diamond       4   0.509   0.629   −
//   ZincBlende    4   0.509   0.629   − (two species)
//   Wurtzite      4   0.484   0.440   +
//   NaCl(rock)    6   0.764   0.354   ≈0 (two species)
//   CsCl          8   0.036   0.511   + (two species)
//   Icosahedral  12   0.000   0.663   −
//   Amorphous    var  <0.1    <0.25   ≈0
//
// Per-atom classification tolerance: ±0.08 on Q4, ±0.08 on Q6.
// CN is used as a primary gating condition before Q4/Q6/W6 checks.
// System-level: majority vote over all per-atom labels with a coverage report.
//
// No presets. No hardcoded outcomes. All labels derived from positions.
// =============================================================================

#include "core/math_vec3.hpp"
#include <vector>
#include <string>
#include <array>
#include <cmath>
#include <complex>
#include <map>
#include <algorithm>
#include <numeric>
#include <sstream>
#include <iomanip>
#include <cstdint>

namespace vsepr::xtal {

// =============================================================================
// Spherical Harmonics Y_lm — real form used in Steinhardt
// Implemented explicitly for l=4 and l=6 to avoid external dependencies.
// =============================================================================

// Y_lm(θ,φ) — complex spherical harmonics for l=4 and l=6
// Using the convention of Steinhardt et al. (1983).
// Returns complex Y_lm for given unit vector r̂.
static inline std::complex<double> Ylm(int l, int m, double x, double y, double z)
{
	// Convert Cartesian unit vector to spherical angles
	// r should be normalized before calling
	double r = std::sqrt(x*x + y*y + z*z);
	if (r < 1e-12) return {0.0, 0.0};
	double xi = x / r, yi = y / r, zi = z / r;

	double cos_theta = zi;
	double phi = std::atan2(yi, xi);

	// Associated Legendre polynomials P_l^|m|(cos_theta)
	// We compute only the (l,m) needed.
	// Using: Y_lm = sqrt((2l+1)/4π · (l-|m|)!/(l+|m|)!) · P_l^|m|(cos θ) · e^{imφ}
	// Note: for negative m: Y_l(-m) = (-1)^m conj(Y_lm)

	int am = std::abs(m);
	double ct = cos_theta;
	double st = std::sqrt(std::max(0.0, 1.0 - ct*ct));

	// Precomputed associated Legendre polynomial P_l^m(cos θ)
	double P = 0.0;

	if (l == 4) {
		switch (am) {
		case 0: P = (35*ct*ct*ct*ct - 30*ct*ct + 3) / 8.0; break;
		case 1: P = -5.0/2.0 * st * (7*ct*ct*ct - 3*ct); break;
		case 2: P = 15.0/2.0 * st*st * (7*ct*ct - 1); break;
		case 3: P = -105.0 * st*st*st * ct; break;
		case 4: P = 105.0 * st*st*st*st; break;
		default: P = 0.0; break;
		}
	} else if (l == 6) {
		switch (am) {
		case 0: P = (231*ct*ct*ct*ct*ct*ct - 315*ct*ct*ct*ct + 105*ct*ct - 5) / 16.0; break;
		case 1: P = -21.0/8.0 * st * (33*ct*ct*ct*ct*ct - 30*ct*ct*ct + 5*ct); break;
		case 2: P = 105.0/8.0 * st*st * (33*ct*ct*ct*ct - 18*ct*ct + 1); break;
		case 3: P = -315.0/2.0 * st*st*st * (11*ct*ct*ct - 3*ct); break;
		case 4: P =  945.0/2.0 * st*st*st*st * (11*ct*ct - 1); break;
		case 5: P = -10395.0 * st*st*st*st*st * ct; break;
		case 6: P = 10395.0 * st*st*st*st*st*st; break;
		default: P = 0.0; break;
		}
	}

	// Normalization constant
	// N_lm = sqrt((2l+1)/4π · (l-|m|)!/(l+|m|)!)
	auto factorial = [](int n) -> double {
		double f = 1.0;
		for (int i = 2; i <= n; ++i) f *= i;
		return f;
	};
	double norm = std::sqrt((2.0*l + 1.0) / (4.0 * M_PI)
							* factorial(l - am) / factorial(l + am));

	// Phase convention for negative m
	double condon_shortley = (m < 0) ? (((am % 2) == 0) ? 1.0 : -1.0) : 1.0;

	std::complex<double> exp_imphi = {std::cos(m * phi), std::sin(m * phi)};

	if (m < 0) {
		// Y_l(-m) = (-1)^m conj(Y_lm)
		std::complex<double> ylm_pos = {norm * P * std::cos(am * phi),
										norm * P * std::sin(am * phi)};
		return condon_shortley * std::conj(ylm_pos);
	}

	return norm * P * exp_imphi;
}

// Wigner 3j symbol (l l l / m1 m2 m3) — precomputed via Racah formula
// Only non-zero when m1+m2+m3=0 and triangle condition satisfied.
// We use a recursive implementation sufficient for l=4 and l=6.
static inline double wigner3j_lll(int l, int m1, int m2, int m3)
{
	if (m1 + m2 + m3 != 0) return 0.0;
	if (std::abs(m1) > l || std::abs(m2) > l || std::abs(m3) > l) return 0.0;

	// Use Racah formula. For the (l l l) case we use a direct summation.
	// Reference: Messiah, "Quantum Mechanics" appendix C.
	auto factorial = [](int n) -> double {
		if (n < 0) return 0.0;
		double f = 1.0;
		for (int i = 2; i <= n; ++i) f *= i;
		return f;
	};

	int J = 2 * l;    // j1+j2+j3 = 3l, but for (l l l): J = 3l
	// Check triangle condition: |j1-j2| <= j3 <= j1+j2 → always OK for (l l l)

	// Prefactor
	double num = factorial(J - 2*l) * factorial(J - 2*l) * factorial(J - 2*l);
	double den = factorial(J + 1);
	double prefactor = std::sqrt(num / den);

	prefactor *= std::sqrt(
		factorial(l + m1) * factorial(l - m1) *
		factorial(l + m2) * factorial(l - m2) *
		factorial(l + m3) * factorial(l - m3)
	);

	// Sum over t
	double sum = 0.0;
	int t_min = std::max({0, -(m3 + l), m1 - l});
	int t_max = std::min({2*l, l - m3, l + m1});

	for (int t = t_min; t <= t_max; ++t) {
		double term = factorial(t) * factorial(2*l - t)
					* factorial(l - m1 - t) * factorial(l + m2 - t)
					* factorial(m3 - m2 + t + l) * factorial(m1 - m3 - t + l);
		if (term < 1e-300) continue;
		double sign = ((t % 2) == 0) ? 1.0 : -1.0;
		sum += sign / term;
	}

	// Phase
	int phase_exp = l - m1 - m2;  // for (l l l / m1 m2 m3): phase = (-1)^(l-m1-m2)
	// Actually standard phase is (-1)^(j1-j2-m3) = (-1)^(l-l-m3) = (-1)^(-m3)
	double phase = ((-m3 % 2) == 0) ? 1.0 : -1.0;
	(void)phase_exp;

	return phase * prefactor * sum;
}

// =============================================================================
// Steinhardt Parameters — per-atom
// =============================================================================

struct SteinhardtParams {
	double Q4 = 0.0;   // l=4 bond-order parameter
	double Q6 = 0.0;   // l=6 bond-order parameter
	double W4 = 0.0;   // l=4 third-order invariant (sign matters)
	double W6 = 0.0;   // l=6 third-order invariant (sign matters: − for FCC, + for HCP/BCC)
	int    CN = 0;     // coordination number (neighbor count within cutoff)
};

// Compute Steinhardt Q_l and W_l for a single atom given its neighbor vectors.
// neighbor_vecs: Cartesian displacement vectors r_j - r_i (not normalized)
static inline SteinhardtParams compute_steinhardt(
	const std::vector<vsepr::Vec3>& neighbor_vecs)
{
	SteinhardtParams sp;
	sp.CN = static_cast<int>(neighbor_vecs.size());
	if (sp.CN == 0) return sp;

	auto compute_ql_wl = [&](int l, double& Ql, double& Wl) {
		int nm = 2 * l + 1;
		std::vector<std::complex<double>> qlm(nm, {0.0, 0.0});

		for (const auto& dr : neighbor_vecs) {
			double len = std::sqrt(dr.x*dr.x + dr.y*dr.y + dr.z*dr.z);
			if (len < 1e-12) continue;
			for (int m = -l; m <= l; ++m) {
				qlm[m + l] += Ylm(l, m, dr.x/len, dr.y/len, dr.z/len);
			}
		}
		// Normalize by neighbor count
		for (auto& v : qlm) v /= sp.CN;

		// Q_l = sqrt(4π/(2l+1) · Σ_m |q_lm|²)
		double sum2 = 0.0;
		for (const auto& v : qlm) sum2 += std::norm(v);
		Ql = std::sqrt(4.0 * M_PI / (2.0 * l + 1.0) * sum2);

		// W_l = Σ_{m1+m2+m3=0} (l l l / m1 m2 m3) q_lm1 q_lm2 q_lm3
		std::complex<double> w = {0.0, 0.0};
		for (int m1 = -l; m1 <= l; ++m1) {
			for (int m2 = -l; m2 <= l; ++m2) {
				int m3 = -(m1 + m2);
				if (std::abs(m3) > l) continue;
				double w3j = wigner3j_lll(l, m1, m2, m3);
				if (std::abs(w3j) < 1e-14) continue;
				w += w3j * qlm[m1 + l] * qlm[m2 + l] * qlm[m3 + l];
			}
		}
		// Normalize W_l by Q_l^3 to get the hat version W_l_hat
		double Ql3 = Ql * Ql * Ql;
		Wl = (Ql3 > 1e-12) ? w.real() / Ql3 : 0.0;
	};

	compute_ql_wl(4, sp.Q4, sp.W4);
	compute_ql_wl(6, sp.Q6, sp.W6);
	return sp;
}

// =============================================================================
// LatticeType enum
// =============================================================================

enum class LatticeType : uint8_t {
	Unknown        = 0,
	SC             = 1,   // Simple cubic
	BCC            = 2,   // Body-centered cubic
	FCC            = 3,   // Face-centered cubic
	HCP            = 4,   // Hexagonal close-packed
	Diamond        = 5,   // Diamond cubic (C, Si, Ge)
	ZincBlende     = 6,   // Zinc blende / sphalerite (GaAs, ZnS-cubic)
	Wurtzite       = 7,   // Wurtzite (ZnS-hex, GaN)
	NaCl           = 8,   // Rock salt (NaCl, MgO)
	CsCl           = 9,   // CsCl (B2 structure)
	Fluorite       = 10,  // Fluorite / antifluorite (CaF2, UO2)
	Perovskite     = 11,  // ABO3 perovskite (SrTiO3)
	Icosahedral    = 12,  // Icosahedral local order (quasicrystal-like)
	Graphite       = 13,  // Layered hexagonal (graphite)
	Amorphous      = 14,  // Amorphous / disordered
	Liquid         = 15,  // Liquid-like (very low Q6, high mobility implied)
	Interstitial   = 16,  // Interstitial site (atom not on any regular lattice point)
};

static inline const char* lattice_type_name(LatticeType t) {
	switch (t) {
	case LatticeType::SC:          return "SC";
	case LatticeType::BCC:         return "BCC";
	case LatticeType::FCC:         return "FCC";
	case LatticeType::HCP:         return "HCP";
	case LatticeType::Diamond:     return "Diamond";
	case LatticeType::ZincBlende:  return "ZincBlende";
	case LatticeType::Wurtzite:    return "Wurtzite";
	case LatticeType::NaCl:        return "NaCl";
	case LatticeType::CsCl:        return "CsCl";
	case LatticeType::Fluorite:    return "Fluorite";
	case LatticeType::Perovskite:  return "Perovskite";
	case LatticeType::Icosahedral: return "Icosahedral";
	case LatticeType::Graphite:    return "Graphite";
	case LatticeType::Amorphous:   return "Amorphous";
	case LatticeType::Liquid:      return "Liquid";
	case LatticeType::Interstitial:return "Interstitial";
	default:                       return "Unknown";
	}
}

// =============================================================================
// Per-atom classification record
// =============================================================================

struct AtomClassRecord {
	int         atom_index = -1;
	SteinhardtParams sp;
	LatticeType type       = LatticeType::Unknown;
	double      confidence = 0.0;   // [0,1] — how close to the reference fingerprint
	std::string note;               // disambiguation note
};

// =============================================================================
// Reference fingerprints
// =============================================================================

struct LatticeReference {
	LatticeType type;
	int    cn_min, cn_max;    // expected coordination number range
	double Q4, Q6;            // ideal values
	double Q4_tol, Q6_tol;   // match tolerance
	double W6_sign;           // +1, -1, or 0 (0 = don't check sign)
	double W6_abs_min;        // minimum |W6| to care about sign
};

static const std::vector<LatticeReference> LATTICE_REFS = {
	// type              cn    cn   Q4      Q6     dQ4    dQ6   W6sgn  W6amin
	//
	// NOTE: BCC Q6=0.511 is computed with the FULL first+second neighbor shell
	// (8 NN + 6 NNN = 14 neighbors). With only 8 NN, BCC gives Q6≈0.629 (same as
	// Diamond) — a geometric degeneracy. Always use a cutoff that captures CN=14.
	// CsCl (8 body-diagonal unlike-species neighbors) gives Q6≈0.629 like Diamond.
	{ LatticeType::SC,          5,  7,  0.764,  0.354,  0.10,  0.10,  0.0,  0.00 },
	{ LatticeType::BCC,        12, 16,  0.036,  0.511,  0.12,  0.10, +1.0,  0.02 },
	{ LatticeType::FCC,        11, 13,  0.191,  0.575,  0.10,  0.10, -1.0,  0.02 },
	{ LatticeType::HCP,        11, 13,  0.097,  0.485,  0.10,  0.10, +1.0,  0.02 },
	{ LatticeType::Diamond,     3,  5,  0.509,  0.629,  0.10,  0.10, -1.0,  0.01 },
	{ LatticeType::ZincBlende,  3,  5,  0.509,  0.629,  0.10,  0.10, -1.0,  0.01 },
	{ LatticeType::Wurtzite,    3,  5,  0.509,  0.629,  0.10,  0.10, +1.0,  0.01 },
	{ LatticeType::NaCl,        5,  7,  0.764,  0.354,  0.10,  0.10,  0.0,  0.00 },
	{ LatticeType::CsCl,        7,  9,  0.509,  0.629,  0.12,  0.12,  0.0,  0.00 },
	{ LatticeType::Icosahedral,11, 13,  0.000,  0.663,  0.10,  0.10, -1.0,  0.05 },
	{ LatticeType::Graphite,    2,  4,  0.000,  0.000,  0.20,  0.20,  0.0,  0.00 },
};

// =============================================================================
// classify_atom() — per-atom classification from SteinhardtParams
// =============================================================================

static inline AtomClassRecord classify_atom(int idx, const SteinhardtParams& sp)
{
	AtomClassRecord rec;
	rec.atom_index = idx;
	rec.sp = sp;

	// Liquid / gas — very low Q6 and high CN variability
	if (sp.Q6 < 0.15 && sp.Q4 < 0.15 && sp.CN >= 2) {
		rec.type = LatticeType::Liquid;
		rec.confidence = 0.5;
		return rec;
	}

	// Amorphous — low Q6
	if (sp.Q6 < 0.25 && sp.Q4 < 0.20) {
		rec.type = LatticeType::Amorphous;
		rec.confidence = 0.5;
		return rec;
	}

	// Interstitial — unusually high CN beyond any known bulk lattice
	// BCC with first+second shell has CN=14, so threshold is 16.
	if (sp.CN > 16) {
		rec.type = LatticeType::Interstitial;
		rec.confidence = 0.6;
		rec.note = "CN > 16";
		return rec;
	}

	// Match against reference table — find closest in Q4/Q6 space
	double best_dist = 1e9;
	const LatticeReference* best = nullptr;

	for (const auto& ref : LATTICE_REFS) {
		// Gate on coordination number
		if (sp.CN < ref.cn_min || sp.CN > ref.cn_max) continue;

		double dQ4 = std::abs(sp.Q4 - ref.Q4);
		double dQ6 = std::abs(sp.Q6 - ref.Q6);

		if (dQ4 > ref.Q4_tol || dQ6 > ref.Q6_tol) continue;

		// W6 sign check (FCC vs HCP, Diamond vs Wurtzite)
		if (ref.W6_sign != 0.0 && std::abs(sp.W6) >= ref.W6_abs_min) {
			double sign_match = sp.W6 * ref.W6_sign;
			if (sign_match < 0.0) continue;  // wrong sign — skip this candidate
		}

		double dist = std::sqrt(dQ4*dQ4 + dQ6*dQ6);
		if (dist < best_dist) {
			best_dist = dist;
			best = &ref;
		}
	}

	if (best) {
		rec.type = best->type;
		// Confidence: 1 at exact match, 0 at tolerance boundary
		double max_tol = std::sqrt(best->Q4_tol*best->Q4_tol + best->Q6_tol*best->Q6_tol);
		rec.confidence = std::max(0.0, 1.0 - best_dist / max_tol);

		// Disambiguation note for ambiguous pairs
		if (best->type == LatticeType::FCC || best->type == LatticeType::HCP)
			rec.note = (sp.W6 < 0) ? "W6<0→FCC" : "W6>0→HCP";
		else if (best->type == LatticeType::BCC || best->type == LatticeType::CsCl)
			rec.note = "W6>0→BCC/CsCl";
		else if (best->type == LatticeType::Diamond || best->type == LatticeType::ZincBlende)
			rec.note = (sp.W6 < 0) ? "W6<0→Diamond/ZB" : "W6>0→Wurtzite";
		else if (best->type == LatticeType::Wurtzite)
			rec.note = "W6>0→Wurtzite";
		return rec;
	}

	// No reference matched — call it Amorphous
	rec.type = LatticeType::Amorphous;
	rec.confidence = 0.2;
	return rec;
}

// =============================================================================
// System-level classification result
// =============================================================================

struct SystemLatticeRecord {
	// Per-atom results
	std::vector<AtomClassRecord> atoms;

	// Majority vote
	LatticeType dominant_type   = LatticeType::Unknown;
	double      dominant_frac   = 0.0;   // fraction of atoms with dominant label
	double      mean_confidence = 0.0;

	// Coverage breakdown: type → count
	std::map<LatticeType, int> type_counts;

	// Steinhardt distribution summary
	double mean_Q4 = 0.0, stddev_Q4 = 0.0;
	double mean_Q6 = 0.0, stddev_Q6 = 0.0;
	double mean_CN = 0.0;

	std::string summary_line() const {
		std::ostringstream ss;
		ss << std::fixed << std::setprecision(4);
		ss << lattice_type_name(dominant_type)
		   << "  frac=" << dominant_frac
		   << "  Q6=" << mean_Q6
		   << "  Q4=" << mean_Q4
		   << "  CN=" << mean_CN;
		return ss.str();
	}

	std::string tsv_header() const {
		return "dominant_type\tdominant_frac\tmean_Q4\tstddev_Q4"
			   "\tmean_Q6\tstddev_Q6\tmean_CN\tmean_confidence";
	}

	std::string to_tsv() const {
		std::ostringstream ss;
		ss << std::fixed << std::setprecision(6)
		   << lattice_type_name(dominant_type) << '\t'
		   << dominant_frac     << '\t'
		   << mean_Q4           << '\t'
		   << stddev_Q4         << '\t'
		   << mean_Q6           << '\t'
		   << stddev_Q6         << '\t'
		   << mean_CN           << '\t'
		   << mean_confidence;
		return ss.str();
	}

	void print_coverage() const {
		std::printf("  %-16s  %5s  %6s  %6s  %6s  %6s  %6s  %s\n",
					"type", "count", "frac", "Q4", "Q6", "W6", "conf", "note");
		for (const auto& ar : atoms) {
			// Just print summary — caller can iterate atoms for full detail
			(void)ar;
		}
		for (const auto& [lt, cnt] : type_counts) {
			double frac = (atoms.empty()) ? 0.0
						: static_cast<double>(cnt) / atoms.size();
			std::printf("  %-16s  %5d  %6.3f\n",
						lattice_type_name(lt), cnt, frac);
		}
	}
};

// =============================================================================
// LatticeClassifier — main entry point
// =============================================================================

class LatticeClassifier {
public:
	// Cutoff for neighbor search (Å)
	double cutoff = 4.0;

	// Classify a set of positions (no periodic boundary conditions)
	// positions: Cartesian coordinates in Å
	// types: atom type per position (0 = ignore, used only for multi-species disambiguation)
	SystemLatticeRecord classify(
		const std::vector<vsepr::Vec3>& positions,
		const std::vector<uint32_t>&    types = {}) const
	{
		SystemLatticeRecord result;
		int N = static_cast<int>(positions.size());
		if (N == 0) return result;

		double cutoff2 = cutoff * cutoff;

		for (int i = 0; i < N; ++i) {
			// Build neighbor vector list
			std::vector<vsepr::Vec3> nbrs;
			for (int j = 0; j < N; ++j) {
				if (i == j) continue;
				vsepr::Vec3 dr = {
					positions[j].x - positions[i].x,
					positions[j].y - positions[i].y,
					positions[j].z - positions[i].z,
				};
				double r2 = dr.x*dr.x + dr.y*dr.y + dr.z*dr.z;
				if (r2 < cutoff2 && r2 > 1e-6) {
					nbrs.push_back(dr);
				}
			}

			SteinhardtParams sp = compute_steinhardt(nbrs);
			AtomClassRecord rec = classify_atom(i, sp);

			// Multi-species disambiguation: NaCl vs SC, ZincBlende vs Diamond
			if (!types.empty() && (int)types.size() > i) {
				bool has_mixed_neighbors = false;
				if (!types.empty()) {
					for (int j = 0; j < N; ++j) {
						if (i == j) continue;
						if (types[j] != types[i]) {
							vsepr::Vec3 dr = {
								positions[j].x - positions[i].x,
								positions[j].y - positions[i].y,
								positions[j].z - positions[i].z,
							};
							double r2 = dr.x*dr.x + dr.y*dr.y + dr.z*dr.z;
							if (r2 < cutoff2) { has_mixed_neighbors = true; break; }
						}
					}
				}
				if (has_mixed_neighbors) {
					if (rec.type == LatticeType::SC)
						rec.type = LatticeType::NaCl;
					else if (rec.type == LatticeType::BCC || rec.type == LatticeType::CsCl)
						rec.type = LatticeType::CsCl;
					else if (rec.type == LatticeType::Diamond)
						rec.type = LatticeType::ZincBlende;
					else if (rec.type == LatticeType::Wurtzite)
						rec.type = LatticeType::Wurtzite; // stays Wurtzite
					rec.note += "|mixed_species";
				}
			}

			result.atoms.push_back(rec);
			result.type_counts[rec.type]++;
		}

		// Compute system-level statistics
		// Majority vote
		int best_count = 0;
		for (const auto& [lt, cnt] : result.type_counts) {
			if (cnt > best_count) { best_count = cnt; result.dominant_type = lt; }
		}
		result.dominant_frac = static_cast<double>(best_count) / N;

		// Steinhardt distribution stats
		double sQ4 = 0, sQ6 = 0, sCN = 0, sConf = 0;
		for (const auto& ar : result.atoms) {
			sQ4  += ar.sp.Q4;
			sQ6  += ar.sp.Q6;
			sCN  += ar.sp.CN;
			sConf += ar.confidence;
		}
		result.mean_Q4 = sQ4 / N;
		result.mean_Q6 = sQ6 / N;
		result.mean_CN = sCN / N;
		result.mean_confidence = sConf / N;

		double vQ4 = 0, vQ6 = 0;
		for (const auto& ar : result.atoms) {
			vQ4 += (ar.sp.Q4 - result.mean_Q4) * (ar.sp.Q4 - result.mean_Q4);
			vQ6 += (ar.sp.Q6 - result.mean_Q6) * (ar.sp.Q6 - result.mean_Q6);
		}
		result.stddev_Q4 = std::sqrt(vQ4 / N);
		result.stddev_Q6 = std::sqrt(vQ6 / N);

		return result;
	}

	// Classify with periodic boundary conditions (orthorhombic box)
	SystemLatticeRecord classify_pbc(
		const std::vector<vsepr::Vec3>& positions,
		double box_x, double box_y, double box_z,
		const std::vector<uint32_t>& types = {}) const
	{
		SystemLatticeRecord result;
		int N = static_cast<int>(positions.size());
		if (N == 0) return result;

		double cutoff2 = cutoff * cutoff;

		auto mic = [&](double dx, double L) -> double {
			dx -= L * std::round(dx / L);
			return dx;
		};

		for (int i = 0; i < N; ++i) {
			std::vector<vsepr::Vec3> nbrs;
			for (int j = 0; j < N; ++j) {
				if (i == j) continue;
				double dx = mic(positions[j].x - positions[i].x, box_x);
				double dy = mic(positions[j].y - positions[i].y, box_y);
				double dz = mic(positions[j].z - positions[i].z, box_z);
				double r2 = dx*dx + dy*dy + dz*dz;
				if (r2 < cutoff2 && r2 > 1e-6)
					nbrs.push_back({dx, dy, dz});
			}

			SteinhardtParams sp = compute_steinhardt(nbrs);
			AtomClassRecord rec = classify_atom(i, sp);

			// Multi-species disambiguation (PBC version)
			if (!types.empty() && (int)types.size() > i) {
				bool has_mixed = false;
				for (int j = 0; j < N && !has_mixed; ++j) {
					if (i == j || (int)types.size() <= j || types[j] == types[i]) continue;
					double dx = mic(positions[j].x - positions[i].x, box_x);
					double dy = mic(positions[j].y - positions[i].y, box_y);
					double dz = mic(positions[j].z - positions[i].z, box_z);
					if (dx*dx + dy*dy + dz*dz < cutoff2) has_mixed = true;
				}
				if (has_mixed) {
					if (rec.type == LatticeType::SC)
						rec.type = LatticeType::NaCl;
					else if (rec.type == LatticeType::BCC)
						rec.type = LatticeType::CsCl;
					else if (rec.type == LatticeType::Diamond)
						rec.type = LatticeType::ZincBlende;
					else if (rec.type == LatticeType::CsCl)
						rec.type = LatticeType::CsCl;
					rec.note += "|mixed_species";
				}
			}

			result.atoms.push_back(rec);
			result.type_counts[rec.type]++;
		}

		// Same stats
		int best_count = 0;
		for (const auto& [lt, cnt] : result.type_counts) {
			if (cnt > best_count) { best_count = cnt; result.dominant_type = lt; }
		}
		result.dominant_frac = static_cast<double>(best_count) / N;

		int n = N;
		double sQ4=0,sQ6=0,sCN=0,sConf=0;
		for (const auto& ar : result.atoms) { sQ4+=ar.sp.Q4; sQ6+=ar.sp.Q6; sCN+=ar.sp.CN; sConf+=ar.confidence; }
		result.mean_Q4 = sQ4/n; result.mean_Q6 = sQ6/n;
		result.mean_CN = sCN/n; result.mean_confidence = sConf/n;
		double vQ4=0,vQ6=0;
		for (const auto& ar : result.atoms) { vQ4+=(ar.sp.Q4-result.mean_Q4)*(ar.sp.Q4-result.mean_Q4); vQ6+=(ar.sp.Q6-result.mean_Q6)*(ar.sp.Q6-result.mean_Q6); }
		result.stddev_Q4 = std::sqrt(vQ4/n);
		result.stddev_Q6 = std::sqrt(vQ6/n);

		return result;
	}
};

} // namespace vsepr::xtal
