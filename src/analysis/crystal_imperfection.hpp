#pragma once
// =============================================================================
// src/analysis/crystal_imperfection.hpp
// =============================================================================
// Reference-based crystal imperfection analysis.
//
// Computes structural deviation from an ideal lattice reference frame.
// All defect labels are analysis-only — nothing is stored in State.
//
// Metrics
// ───────
//  RMSD_ref              Kabsch RMSD vs reference (shape deviation)
//  RMSD_step             Kabsch RMSD between consecutive frames
//  mean_displacement     average |r_i(t) - r_ref_i| without alignment
//  occupancy_mismatch    fraction of reference sites with no atom within r_occ
//  coordination_mismatch fraction of atoms whose CN differs from reference CN
//  structural_residual   RMS lattice-site deviation (per-site displacement)
//  defect_fraction       fraction of sites displaced beyond defect_threshold
//  N_excess              N_current - N_ref  (>0 = interstitial, <0 = vacancy)
//  E_rel_drift           (E_total - E0) / |E0|
//  stationary_flag       fed from external stationarity gate
//  emergent_class        analysis label — never stored in state
//
// Emergent class vocabulary
//   ideal_crystal         low residual, zero N_excess, low mismatch
//   thermal_disorder      increased residual, zero N_excess, uniform distrib
//   interstitial_like     N_excess > 0, high local residual
//   vacancy_like          N_excess < 0, high occupancy mismatch
//   substitution_like     N_excess == 0, identity mismatch or high coord mismatch
//   localized_strain      high defect_fraction, clustered rather than uniform
//   unstable_crystal      energy drift significant
//   relaxing              stationary_flag = false, residual decreasing
//
// Anti-black-box: every field public, every formula inline.
// =============================================================================

#include "core/math_vec3.hpp"
#include "analysis/kabsch.hpp"
#include <cstdint>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <sstream>
#include <iomanip>

namespace vsepr::xtal {

// ─────────────────────────────────────────────────────────────────────────────
// CrystalImperfectionRow  — one row of the output table
// ─────────────────────────────────────────────────────────────────────────────

struct CrystalImperfectionRow {
	uint64_t frame                = 0;
	double   time                 = 0.0;
	double   RMSD_ref             = 0.0;   // Å — Kabsch aligned
	double   RMSD_step            = 0.0;   // Å — frame-to-frame Kabsch
	double   mean_displacement    = 0.0;   // Å — raw site displacement
	double   occupancy_mismatch   = 0.0;   // [0,1] — fraction of ref sites unoccupied
	double   coordination_mismatch= 0.0;   // [0,1] — fraction of atoms with wrong CN
	double   structural_residual  = 0.0;   // Å — RMS per-site deviation
	double   defect_fraction      = 0.0;   // [0,1] — sites > defect_threshold
	int      N_excess             = 0;     // N_current - N_ref
	double   E_total              = 0.0;
	double   E_rel_drift          = 0.0;
	bool     stationary_flag      = false;
	int      identity_mismatch_count = 0;  // sites where atom type != ref type
	std::string structure_class;           // geometry verdict
	std::string identity_class;            // identity/type verdict
	std::string emergent_class;            // combined label (structure_class when identical, else structure_class|identity_class)

	std::string to_tsv() const {
		std::ostringstream ss;
		ss << std::fixed << std::setprecision(6)
		   << frame               << '\t'
		   << time                << '\t'
		   << RMSD_ref            << '\t'
		   << RMSD_step           << '\t'
		   << mean_displacement   << '\t'
		   << occupancy_mismatch  << '\t'
		   << coordination_mismatch << '\t'
		   << structural_residual << '\t'
		   << defect_fraction     << '\t'
		   << N_excess            << '\t'
		   << identity_mismatch_count << '\t'
		   << E_total             << '\t'
		   << E_rel_drift         << '\t'
		   << (stationary_flag ? 1 : 0) << '\t'
		   << structure_class     << '\t'
		   << identity_class      << '\t'
		   << emergent_class;
		return ss.str();
	}

	static std::string tsv_header() {
		return "frame\ttime"
			   "\tRMSD_ref\tRMSD_step"
			   "\tmean_displacement"
			   "\toccupancy_mismatch\tcoordination_mismatch"
			   "\tstructural_residual\tdefect_fraction"
			   "\tN_excess\tidentity_mismatch_count"
			   "\tE_total\tE_rel_drift"
			   "\tstationary_flag"
			   "\tstructure_class\tidentity_class\temergent_class";
	}
};

// ─────────────────────────────────────────────────────────────────────────────
// classify_imperfection()  — geometry + N_excess → analysis label
// ─────────────────────────────────────────────────────────────────────────────

// classify_imperfection_pair() — returns {structure_class, identity_class}
// structure_class: geometry verdict derived from displacements / N_excess
// identity_class:  type/identity verdict derived from identity_mismatch_count
//
// These are kept separate so a substituted atom that has not yet geometrically
// distorted anything is not falsely promoted to a geometry defect.
inline std::pair<std::string,std::string> classify_imperfection_pair(
		int    N_excess,
		double occupancy_mismatch,
		double coordination_mismatch,
		double defect_fraction,
		int    identity_mismatch_count,
		double E_rel_drift,
		bool   stationary_flag,
		double defect_threshold    = 0.05,
		double drift_threshold     = 0.05)
{
	// ── Structure class ───────────────────────────────────────────────────────
	std::string sc;
	if (std::abs(E_rel_drift) > drift_threshold)
		sc = "unstable_crystal";
	else if (stationary_flag && defect_fraction < defect_threshold && N_excess == 0)
		sc = "ideal_crystal";
	else if (!stationary_flag && defect_fraction > defect_threshold)
		sc = "relaxing";
	else if (N_excess > 0)
		sc = "interstitial_like";
	else if (N_excess < 0)
		sc = "vacancy_like";
	else if (N_excess == 0 && defect_fraction > defect_threshold * 3)
		sc = "localized_strain";
	else if (N_excess == 0 && defect_fraction > defect_threshold)
		sc = "thermal_disorder";
	else
		sc = "ideal_crystal";

	// ── Identity class ────────────────────────────────────────────────────────
	// Derived purely from type comparison — does not require geometry distortion.
	std::string ic;
	if (identity_mismatch_count > 0)
		ic = "substitutional_site";
	else
		ic = "identity_match";

	return {sc, ic};
}

// classify_imperfection() — convenience wrapper that returns the combined
// emergent_class string used in the legacy single-field output.
inline std::string classify_imperfection(
		int    N_excess,
		double occupancy_mismatch,
		double coordination_mismatch,
		double defect_fraction,
		int    identity_mismatch_count,
		double E_rel_drift,
		bool   stationary_flag,
		double defect_threshold = 0.05,
		double drift_threshold  = 0.05)
{
	auto [sc, ic] = classify_imperfection_pair(
		N_excess, occupancy_mismatch, coordination_mismatch,
		defect_fraction, identity_mismatch_count,
		E_rel_drift, stationary_flag,
		defect_threshold, drift_threshold);
	if (ic == "identity_match")
		return sc;
	return sc + "|" + ic;
}

// ─────────────────────────────────────────────────────────────────────────────
// CrystalImperfectionAnalyzer
// ─────────────────────────────────────────────────────────────────────────────

struct CrystalImperfectionAnalyzer {
	// Configuration (all public)
	double r_occ          = 1.5;   // Å — site considered occupied if any atom within r_occ
	double r_bond         = 5.0;   // Å — neighbor cutoff for coordination number
	double defect_threshold = 0.5; // Å — site displacement to call a defect
	double E0             = 0.0;

	// Reference state (ideal lattice)
	std::vector<vsepr::Vec3> ref_positions;
	std::vector<int>          ref_coord;    // reference CN per site
	std::vector<int>          ref_types;    // reference atom type per site (optional)

	void set_baseline(double E_total_0) { E0 = E_total_0; }

	// Set reference: record ideal positions and compute reference CN
	void set_reference(const std::vector<vsepr::Vec3>& ref) {
		ref_positions = ref;
		ref_coord.resize(ref.size());
		for (std::size_t i = 0; i < ref.size(); ++i) {
			int cn = 0;
			for (std::size_t j = 0; j < ref.size(); ++j) {
				if (i == j) continue;
				const double dx = ref[j].x - ref[i].x;
				const double dy = ref[j].y - ref[i].y;
				const double dz = ref[j].z - ref[i].z;
				if (std::sqrt(dx*dx+dy*dy+dz*dz) < r_bond) ++cn;
			}
			ref_coord[i] = cn;
		}
	}

	CrystalImperfectionRow compute(
			uint64_t                         frame_idx,
			double                           sim_time,
			double                           E_total,
			const std::vector<vsepr::Vec3>&  cur,
			const std::vector<vsepr::Vec3>&  prev_cur,  // previous frame (for RMSD_step)
			bool                             stationary = false,
			const std::vector<int>&          cur_types  = {}) const
	{
		CrystalImperfectionRow row;
		row.frame    = frame_idx;
		row.time     = sim_time;
		row.E_total  = E_total;
		const double denom = std::abs(E0) > 1e-12 ? std::abs(E0) : 1.0;
		row.E_rel_drift   = (E_total - E0) / denom;
		row.stationary_flag = stationary;
		row.N_excess  = static_cast<int>(cur.size()) - static_cast<int>(ref_positions.size());

		const int N_ref = static_cast<int>(ref_positions.size());
		const int N_cur = static_cast<int>(cur.size());

		// ── RMSD_ref — Kabsch on min(N_ref, N_cur) atoms ────────────────────
		// If sizes differ, we align the common atoms only.
		if (N_ref > 0 && N_cur > 0) {
			int N_common = std::min(N_ref, N_cur);
			std::vector<vsepr::Vec3> ref_sub(ref_positions.begin(),
											 ref_positions.begin() + N_common);
			std::vector<vsepr::Vec3> cur_sub(cur.begin(),
											 cur.begin() + N_common);
			row.RMSD_ref = vsepr::analysis::kabsch_rmsd(ref_sub, cur_sub);

			// RMSD_step — only meaningful if sizes match prev frame
			if (!prev_cur.empty() && static_cast<int>(prev_cur.size()) == N_cur) {
				std::vector<vsepr::Vec3> prev_sub(prev_cur.begin(),
												  prev_cur.begin() + N_common);
				row.RMSD_step = vsepr::analysis::kabsch_rmsd(cur_sub, prev_sub);
			}
		}

		// ── Mean displacement (no alignment) ─────────────────────────────────
		{
			int N_common = std::min(N_ref, N_cur);
			double sum_disp = 0.0;
			for (int i = 0; i < N_common; ++i) {
				const double dx = cur[i].x - ref_positions[i].x;
				const double dy = cur[i].y - ref_positions[i].y;
				const double dz = cur[i].z - ref_positions[i].z;
				sum_disp += std::sqrt(dx*dx + dy*dy + dz*dz);
			}
			row.mean_displacement = (N_common > 0) ? sum_disp / N_common : 0.0;
		}

		// ── Structural residual + defect fraction (per-site) ─────────────────
		{
			double sum_sq = 0.0;
			int    defect_count = 0;
			const int N_common = std::min(N_ref, N_cur);
			for (int i = 0; i < N_common; ++i) {
				const double dx = cur[i].x - ref_positions[i].x;
				const double dy = cur[i].y - ref_positions[i].y;
				const double dz = cur[i].z - ref_positions[i].z;
				const double d = std::sqrt(dx*dx + dy*dy + dz*dz);
				sum_sq += d * d;
				if (d > defect_threshold) ++defect_count;
			}
			row.structural_residual = (N_common > 0)
				? std::sqrt(sum_sq / N_common) : 0.0;
			row.defect_fraction = (N_common > 0)
				? static_cast<double>(defect_count) / N_common : 0.0;
		}

		// ── Occupancy mismatch — how many ref sites have no atom nearby ───────
		{
			int unoccupied = 0;
			for (int ri = 0; ri < N_ref; ++ri) {
				bool occupied = false;
				for (int ci = 0; ci < N_cur && !occupied; ++ci) {
					const double dx = cur[ci].x - ref_positions[ri].x;
					const double dy = cur[ci].y - ref_positions[ri].y;
					const double dz = cur[ci].z - ref_positions[ri].z;
					if (std::sqrt(dx*dx+dy*dy+dz*dz) < r_occ) occupied = true;
				}
				if (!occupied) ++unoccupied;
			}
			row.occupancy_mismatch = (N_ref > 0)
				? static_cast<double>(unoccupied) / N_ref : 0.0;
		}

		// ── Coordination mismatch — atoms whose CN ≠ reference CN ────────────
		{
			// Compute current CN for each atom
			std::vector<int> cur_coord(static_cast<std::size_t>(N_cur), 0);
			for (int i = 0; i < N_cur; ++i)
				for (int j = i+1; j < N_cur; ++j) {
					const double dx = cur[j].x - cur[i].x;
					const double dy = cur[j].y - cur[i].y;
					const double dz = cur[j].z - cur[i].z;
					if (std::sqrt(dx*dx+dy*dy+dz*dz) < r_bond) {
						++cur_coord[i]; ++cur_coord[j];
					}
				}
			// Compare to reference (for common atoms only)
			int mismatch = 0;
			const int N_common = std::min(N_ref, N_cur);
			for (int i = 0; i < N_common; ++i) {
				if (cur_coord[i] != ref_coord[i]) ++mismatch;
			}
			row.coordination_mismatch = (N_common > 0)
				? static_cast<double>(mismatch) / N_common : 0.0;
		}

		// ── Identity mismatch — sites where current type differs from reference ──
		// Requires ref_types to be populated (e.g. via set_reference_types).
		// If ref_types is empty the count stays zero (no type information given).
		{
			const int N_common = std::min(
				static_cast<int>(ref_types.size()),
				static_cast<int>(cur_types.size()));
			int id_mis = 0;
			for (int i = 0; i < N_common; ++i)
				if (cur_types[i] != ref_types[i]) ++id_mis;
			row.identity_mismatch_count = id_mis;
		}

		// ── Emergent class ────────────────────────────────────────────────────
		auto [sc, ic] = classify_imperfection_pair(
			row.N_excess,
			row.occupancy_mismatch,
			row.coordination_mismatch,
			row.defect_fraction,
			row.identity_mismatch_count,
			row.E_rel_drift,
			row.stationary_flag);
		row.structure_class = sc;
		row.identity_class  = ic;
		row.emergent_class  = (ic == "identity_match") ? sc : sc + "|" + ic;

		return row;
	}
};

} // namespace vsepr::xtal
