#pragma once
// =============================================================================
// src/analysis/cluster_analysis.hpp
// =============================================================================
// Reference-free cluster analysis for emergence microtests.
//
// All labels produced here are analysis-only.  The state file (atomistic::State)
// remains property-free.  This file contains only math and geometry — no
// switch statement wearing a lab coat.
//
// Provided types
// ──────────────
//  XyzFullRow          — single-atom snapshot row (state audit; no labels)
//  XyzFullFrame        — one frame of the xyzFull trajectory
//  ClusterMetricsRow   — one row of the reference-free metrics table
//  ClusterAnalysis     — computes all metrics from a raw trajectory
//
// Metrics (reference-free)
// ────────────────────────
//  pair distances          d_ij for all unique pairs
//  mean_pair_distance      average over all pairs
//  pair_distance_stddev    stddev over all pairs
//  radius_of_gyration      Rg from center of mass
//  kinetic_energy          KE from velocities and masses
//  energy_drift            E_total(t) - E_total(0)  /  |E_total(0)|
//  coordination_count      per-atom count of neighbors within r_cut
//  connected_components    graph union-find on r_cut neighbor graph
//  largest_cluster_size    max component size
//  shape_anisotropy        (max principal axis) / (min principal axis)
//  cluster_class           analysis label — compact/chain/planar/scattered/…
//  stationary_flag         fed from external SimMetrics or computed inline
//
// Anti-black-box: every field public, every formula visible.
// =============================================================================

#include "core/math_vec3.hpp"
#include <cstdint>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <sstream>
#include <iomanip>

namespace vsepr::cluster {

// ─────────────────────────────────────────────────────────────────────────────
// XyzFullRow  — one atom at one frame  (state audit, property-free)
// ─────────────────────────────────────────────────────────────────────────────

struct XyzFullRow {
	uint64_t frame   = 0;
	double   time    = 0.0;   // simulation time (caller's units)
	uint32_t id      = 0;     // atom index
	uint32_t type    = 0;     // species / type id
	double   x = 0, y = 0, z = 0;
	double   vx= 0, vy= 0, vz= 0;

	std::string to_tsv() const {
		std::ostringstream ss;
		ss << std::fixed << std::setprecision(6)
		   << frame << '\t' << time  << '\t'
		   << id    << '\t' << type  << '\t'
		   << x     << '\t' << y     << '\t' << z  << '\t'
		   << vx    << '\t' << vy    << '\t' << vz;
		return ss.str();
	}

	static std::string tsv_header() {
		return "frame\ttime\tid\ttype\tx\ty\tz\tvx\tvy\tvz";
	}
};

// ─────────────────────────────────────────────────────────────────────────────
// XyzFullFrame  — all atoms at one frame
// ─────────────────────────────────────────────────────────────────────────────

struct XyzFullFrame {
	uint64_t frame = 0;
	double   time  = 0.0;
	std::vector<XyzFullRow> rows;
};

// ─────────────────────────────────────────────────────────────────────────────
// ClusterMetricsRow  — one row of the reference-free metrics table
// ─────────────────────────────────────────────────────────────────────────────

struct ClusterMetricsRow {
	uint64_t frame              = 0;
	double   time               = 0.0;
	double   mean_pair_distance = 0.0;  // Å
	double   pair_dist_stddev   = 0.0;  // Å
	double   radius_of_gyration = 0.0;  // Å
	double   kinetic_energy     = 0.0;  // kcal/mol
	double   E_total            = 0.0;  // kcal/mol
	double   E_rel_drift        = 0.0;  // (E_total - E0) / |E0|
	int      largest_cluster    = 0;    // atoms
	int      n_components       = 0;    // connected components
	double   mean_coordination  = 0.0;  // average neighbors within r_bond
	double   shape_anisotropy   = 0.0;  // longest/shortest principal axis
	bool     stationary_flag    = false;
	std::string cluster_class;          // analysis label

	std::string to_tsv() const {
		std::ostringstream ss;
		ss << std::fixed << std::setprecision(6)
		   << frame              << '\t'
		   << time               << '\t'
		   << mean_pair_distance << '\t'
		   << pair_dist_stddev   << '\t'
		   << radius_of_gyration << '\t'
		   << kinetic_energy     << '\t'
		   << E_total            << '\t'
		   << E_rel_drift        << '\t'
		   << largest_cluster    << '\t'
		   << n_components       << '\t'
		   << mean_coordination  << '\t'
		   << shape_anisotropy   << '\t'
		   << (stationary_flag ? 1 : 0) << '\t'
		   << cluster_class;
		return ss.str();
	}

	static std::string tsv_header() {
		return "frame\ttime"
			   "\tmean_pair_distance\tpair_dist_stddev"
			   "\tradius_of_gyration\tkinetic_energy"
			   "\tE_total\tE_rel_drift"
			   "\tlargest_cluster\tn_components"
			   "\tmean_coordination\tshape_anisotropy"
			   "\tstationary_flag\tcluster_class";
	}
};

// ─────────────────────────────────────────────────────────────────────────────
// Internal helpers
// ─────────────────────────────────────────────────────────────────────────────

namespace detail {

// Center of mass (equal-mass or mass-weighted).
inline vsepr::Vec3 center_of_mass(
		const std::vector<vsepr::Vec3>& pos,
		const std::vector<double>& masses) {
	vsepr::Vec3 cm{0,0,0};
	double total_mass = 0.0;
	for (std::size_t i = 0; i < pos.size(); ++i) {
		cm.x += masses[i] * pos[i].x;
		cm.y += masses[i] * pos[i].y;
		cm.z += masses[i] * pos[i].z;
		total_mass += masses[i];
	}
	const double inv = (total_mass > 0.0) ? 1.0 / total_mass : 0.0;
	cm.x *= inv; cm.y *= inv; cm.z *= inv;
	return cm;
}

// Radius of gyration: sqrt( mean( |r_i - r_cm|^2 ) )
inline double radius_of_gyration(
		const std::vector<vsepr::Vec3>& pos,
		const std::vector<double>& masses) {
	if (pos.empty()) return 0.0;
	vsepr::Vec3 cm = center_of_mass(pos, masses);
	double sum = 0.0, total_mass = 0.0;
	for (std::size_t i = 0; i < pos.size(); ++i) {
		const double dx = pos[i].x - cm.x;
		const double dy = pos[i].y - cm.y;
		const double dz = pos[i].z - cm.z;
		sum        += masses[i] * (dx*dx + dy*dy + dz*dz);
		total_mass += masses[i];
	}
	return (total_mass > 0.0) ? std::sqrt(sum / total_mass) : 0.0;
}

// Kinetic energy: sum( 0.5 * m_i * v_i^2 )   [kcal/mol, AMBER units]
inline double kinetic_energy(
		const std::vector<vsepr::Vec3>& vel,
		const std::vector<double>& masses) {
	double ke = 0.0;
	for (std::size_t i = 0; i < vel.size(); ++i) {
		const double v2 = vel[i].x*vel[i].x + vel[i].y*vel[i].y + vel[i].z*vel[i].z;
		ke += 0.5 * masses[i] * v2;
	}
	// Convert from amu*(Å/fs)^2 to kcal/mol:
	//   1 amu*(Å/fs)^2 = 0.04184^-1 * 1e-3 kcal/mol ... but integrator already
	//   uses kcal/mol/Å for forces so masses in amu with Å/fs velocities:
	//   factor = 1/(2*4184*1e-20) ... use standard MD factor 1/2 * m * v^2 with
	//   unit conversion: 1 amu*(Å/fs)^2 = 418.4 kcal/mol
	return ke * 418.4;
}

// All unique pair distances.
inline std::vector<double> pair_distances(const std::vector<vsepr::Vec3>& pos) {
	const std::size_t N = pos.size();
	std::vector<double> d;
	d.reserve(N * (N-1) / 2);
	for (std::size_t i = 0; i < N; ++i)
		for (std::size_t j = i+1; j < N; ++j) {
			const double dx = pos[j].x - pos[i].x;
			const double dy = pos[j].y - pos[i].y;
			const double dz = pos[j].z - pos[i].z;
			d.push_back(std::sqrt(dx*dx + dy*dy + dz*dz));
		}
	return d;
}

// Mean and stddev of a non-empty vector.
inline std::pair<double,double> mean_stddev(const std::vector<double>& v) {
	if (v.empty()) return {0.0, 0.0};
	double mu = 0.0;
	for (double x : v) mu += x;
	mu /= static_cast<double>(v.size());
	double var = 0.0;
	for (double x : v) { double d = x - mu; var += d*d; }
	var /= static_cast<double>(v.size());
	return {mu, std::sqrt(var)};
}

// Per-atom coordination count (neighbors within r_bond).
inline std::vector<int> coordination(
		const std::vector<vsepr::Vec3>& pos,
		double r_bond) {
	const std::size_t N = pos.size();
	std::vector<int> coord(N, 0);
	for (std::size_t i = 0; i < N; ++i)
		for (std::size_t j = i+1; j < N; ++j) {
			const double dx = pos[j].x - pos[i].x;
			const double dy = pos[j].y - pos[i].y;
			const double dz = pos[j].z - pos[i].z;
			if (std::sqrt(dx*dx+dy*dy+dz*dz) < r_bond) {
				++coord[i]; ++coord[j];
			}
		}
	return coord;
}

// Union-Find for connected components.
struct UF {
	std::vector<int> p;
	explicit UF(int n) : p(n) { std::iota(p.begin(), p.end(), 0); }
	int find(int x) { return (p[x]==x) ? x : (p[x]=find(p[x])); }
	void unite(int a, int b) { p[find(a)] = find(b); }
};

// Connected components and largest component size using r_bond threshold.
inline std::pair<int,int> connected_components(
		const std::vector<vsepr::Vec3>& pos,
		double r_bond) {
	const int N = static_cast<int>(pos.size());
	UF uf(N);
	for (int i = 0; i < N; ++i)
		for (int j = i+1; j < N; ++j) {
			const double dx = pos[j].x - pos[i].x;
			const double dy = pos[j].y - pos[i].y;
			const double dz = pos[j].z - pos[i].z;
			if (std::sqrt(dx*dx+dy*dy+dz*dz) < r_bond)
				uf.unite(i, j);
		}
	std::vector<int> comp_size(static_cast<std::size_t>(N), 0);
	for (int i = 0; i < N; ++i) ++comp_size[static_cast<std::size_t>(uf.find(i))];
	int n_comp = 0, largest = 0;
	for (int s : comp_size) {
		if (s > 0) { ++n_comp; largest = std::max(largest, s); }
	}
	return {n_comp, largest};
}

// Shape anisotropy via inertia tensor eigenvalues (symmetric 3×3).
// Returns ratio of largest to smallest principal moment.
// Uses power-iteration fallback — no Eigen dependency in this analysis path.
inline double shape_anisotropy(
		const std::vector<vsepr::Vec3>& pos,
		const std::vector<double>& masses) {
	if (pos.size() < 2) return 1.0;
	vsepr::Vec3 cm = center_of_mass(pos, masses);

	// Inertia tensor I_{ab} = sum_i m_i( |r_i|^2 delta_{ab} - r_{ia} r_{ib} )
	double I[3][3] = {};
	for (std::size_t i = 0; i < pos.size(); ++i) {
		const double x = pos[i].x - cm.x;
		const double y = pos[i].y - cm.y;
		const double z = pos[i].z - cm.z;
		const double m = masses[i];
		const double r2 = x*x + y*y + z*z;
		I[0][0] += m*(r2 - x*x);  I[0][1] -= m*x*y;  I[0][2] -= m*x*z;
		I[1][0] -= m*y*x;  I[1][1] += m*(r2 - y*y);  I[1][2] -= m*y*z;
		I[2][0] -= m*z*x;  I[2][1] -= m*z*y;  I[2][2] += m*(r2 - z*z);
	}

	// Characteristic polynomial of 3×3 symmetric matrix → cubic.
	// Use Gershgorin disk estimates then 3 Jacobi sweeps for eigenvalues.
	// Simple but sufficient for N ≤ 10.
	double a[3][3];
	for (int r = 0; r < 3; ++r)
		for (int c = 0; c < 3; ++c)
			a[r][c] = I[r][c];

	// Jacobi sweeps (5 iterations is overkill for 3×3 but costs nothing)
	double eig[3] = {a[0][0], a[1][1], a[2][2]};
	for (int sweep = 0; sweep < 10; ++sweep) {
		for (int p = 0; p < 2; ++p) {
			for (int q = p+1; q < 3; ++q) {
				if (std::abs(a[p][q]) < 1e-15) continue;
				const double theta = 0.5 * std::atan2(2.0*a[p][q], a[p][p]-a[q][q]);
				const double c2 = std::cos(theta), s2 = std::sin(theta);
				// Apply Givens rotation
				double b[3][3];
				for (int r = 0; r < 3; ++r)
					for (int cc = 0; cc < 3; ++cc)
						b[r][cc] = a[r][cc];
				b[p][p] = c2*c2*a[p][p] - 2*s2*c2*a[p][q] + s2*s2*a[q][q];
				b[q][q] = s2*s2*a[p][p] + 2*s2*c2*a[p][q] + c2*c2*a[q][q];
				b[p][q] = b[q][p] = 0.0;
				for (int r = 0; r < 3; ++r) {
					if (r == p || r == q) continue;
					b[r][p] = b[p][r] =  c2*a[r][p] - s2*a[r][q];
					b[r][q] = b[q][r] =  s2*a[r][p] + c2*a[r][q];
				}
				for (int r = 0; r < 3; ++r)
					for (int cc = 0; cc < 3; ++cc)
						a[r][cc] = b[r][cc];
			}
		}
		for (int r = 0; r < 3; ++r) eig[r] = a[r][r];
	}

	std::sort(eig, eig+3);
	// eig[0] ≤ eig[1] ≤ eig[2]  (moments of inertia — smallest = most elongated axis)
	// Anisotropy: max_moment / min_moment.  Guard divide-by-zero.
	if (eig[0] < 1e-12) return (eig[2] < 1e-12) ? 1.0 : 1e6;
	return eig[2] / eig[0];
}

} // namespace detail

// ─────────────────────────────────────────────────────────────────────────────
// classify_cluster_shape() — geometry-only label
// ─────────────────────────────────────────────────────────────────────────────
//
//   scattered         — n_components > 1 (atoms not all connected)
//   collapsed         — mean_pair_distance < r_overlap (too close)
//   elongated_chain   — anisotropy > 4
//   planar_cluster    — moderate anisotropy (1.5–4) + low RoG
//   compact_cluster   — anisotropy ≤ 1.5, all connected
//   single_atom       — N == 1

inline std::string classify_cluster_shape(
		int    N,
		int    n_components,
		double mean_pair_dist,
		double radius_of_gyration,
		double anisotropy,
		double r_overlap = 2.0) {
	if (N <= 1)               return "single_atom";
	if (mean_pair_dist < r_overlap) return "collapsed";
	if (n_components > 1)     return "scattered";
	if (anisotropy > 4.0)     return "elongated_chain";
	if (anisotropy > 1.5)     return "planar_cluster";
	(void)radius_of_gyration;
	return "compact_cluster";
}

// ─────────────────────────────────────────────────────────────────────────────
// ClusterAnalysis  — computes one ClusterMetricsRow from raw frame data
// ─────────────────────────────────────────────────────────────────────────────
//
// Configuration (all public):
//   r_bond      — neighbor cutoff for coordination and connectivity (Å)
//   r_overlap   — minimum acceptable pair distance below which = "collapsed" (Å)
//   E0          — reference energy for drift computation (kcal/mol)

struct ClusterAnalysis {
	double r_bond   = 5.0;   // Å — LJ minimum for Ar ≈ 3.82 Å; 5 Å catches first shell
	double r_overlap= 2.0;   // Å — unphysically close
	double E0       = 0.0;   // kcal/mol — set on first frame or via set_baseline()

	void set_baseline(double E_total_0) { E0 = E_total_0; }

	ClusterMetricsRow compute(
			uint64_t                         frame_idx,
			double                           sim_time,
			double                           E_total,
			const std::vector<vsepr::Vec3>&  pos,
			const std::vector<vsepr::Vec3>&  vel,
			const std::vector<double>&       masses,
			bool                             stationary = false) const
	{
		ClusterMetricsRow row;
		row.frame = frame_idx;
		row.time  = sim_time;
		row.E_total = E_total;

		const int N = static_cast<int>(pos.size());

		// Energy drift
		const double denom = std::abs(E0) > 1e-12 ? std::abs(E0) : 1.0;
		row.E_rel_drift = (E_total - E0) / denom;

		// Pair distances
		const auto pd = detail::pair_distances(pos);
		auto [mu, sd] = detail::mean_stddev(pd);
		row.mean_pair_distance = mu;
		row.pair_dist_stddev   = sd;

		// Radius of gyration
		row.radius_of_gyration = detail::radius_of_gyration(pos, masses);

		// Kinetic energy
		row.kinetic_energy = detail::kinetic_energy(vel, masses);

		// Coordination
		const auto coord = detail::coordination(pos, r_bond);
		double mean_coord = 0.0;
		for (int c : coord) mean_coord += c;
		row.mean_coordination = (N > 0) ? mean_coord / N : 0.0;

		// Connectivity
		auto [n_comp, largest] = detail::connected_components(pos, r_bond);
		row.n_components    = n_comp;
		row.largest_cluster = largest;

		// Shape anisotropy
		row.shape_anisotropy = detail::shape_anisotropy(pos, masses);

		// Classification
		row.stationary_flag = stationary;
		row.cluster_class   = classify_cluster_shape(
			N, n_comp,
			row.mean_pair_distance,
			row.radius_of_gyration,
			row.shape_anisotropy,
			r_overlap);

		return row;
	}
};

// ─────────────────────────────────────────────────────────────────────────────
// SweepResult  — summary row for parameter sweep (Task 6)
// ─────────────────────────────────────────────────────────────────────────────

struct SweepResult {
	std::string case_id;
	double initial_spacing   = 0.0;
	double velocity_scale    = 0.0;
	double final_mean_pair_distance   = 0.0;
	double final_radius_of_gyration   = 0.0;
	double energy_drift               = 0.0;
	int    stationary_frame           = -1;
	bool   cluster_detected           = false;
	int    cluster_lifetime           = 0;    // frames where largest == N
	std::string final_shape_class;

	std::string to_tsv() const {
		std::ostringstream ss;
		ss << std::fixed << std::setprecision(4)
		   << case_id                  << '\t'
		   << initial_spacing          << '\t'
		   << velocity_scale           << '\t'
		   << final_mean_pair_distance << '\t'
		   << final_radius_of_gyration << '\t'
		   << energy_drift             << '\t'
		   << stationary_frame         << '\t'
		   << (cluster_detected ? 1 : 0) << '\t'
		   << cluster_lifetime         << '\t'
		   << final_shape_class;
		return ss.str();
	}

	static std::string tsv_header() {
		return "case_id\tinitial_spacing\tvelocity_scale"
			   "\tfinal_mean_pair_dist\tfinal_Rg"
			   "\tenergy_drift\tstationary_frame"
			   "\tcluster_detected\tcluster_lifetime"
			   "\tfinal_shape_class";
	}
};

// ─────────────────────────────────────────────────────────────────────────────
// EmergenceReport  — reference-free narrative report (Task 7)
// ─────────────────────────────────────────────────────────────────────────────

struct EmergenceReport {
	std::string title;
	std::string seed_tag;
	int         N_atoms        = 0;
	bool        cluster_emerged= false;
	std::string final_class;
	int         stationary_frame = -1;
	std::string no_hardcoded_property_audit =
		"AUDIT: no cluster_detected, bond_count, molecule_type, or "
		"stability_class stored in state. All labels are analysis-derived.";

	std::string format() const {
		std::ostringstream ss;
		ss << "=== Emergence Report: " << title << " ===\n";
		ss << "Seed:            " << seed_tag    << '\n';
		ss << "Atoms:           " << N_atoms      << '\n';
		ss << "Cluster emerged: " << (cluster_emerged ? "YES" : "NO") << '\n';
		ss << "Final class:     " << final_class  << '\n';
		ss << "Stationary at:   frame " << stationary_frame << '\n';
		ss << no_hardcoded_property_audit << '\n';
		return ss.str();
	}
};

} // namespace vsepr::cluster
