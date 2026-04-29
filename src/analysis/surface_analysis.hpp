#pragma once
// =============================================================================
// src/analysis/surface_analysis.hpp
// =============================================================================
// Reference-free surface interaction analysis.
//
// Tracks incoming particle behavior and surface response from trajectory only.
// No erosion_rate, adsorption_probability, surface_damage, or roughness_score
// stored in state — those emerge from the analysis layer.
//
// Per-frame surface metrics
// ─────────────────────────
//  min_surface_distance    closest approach of incoming atom to surface plane
//  surface_z_ref           z-coordinate of ideal surface plane (from reference)
//  residence_frames        how many frames incoming atom stays within r_reside
//  reflection_angle        angle of outgoing velocity vs surface normal (deg)
//  KE_incoming             kinetic energy before first contact
//  KE_outgoing             kinetic energy after last contact
//  momentum_transfer       |Δp| to slab atoms
//  surface_structural_residual  RMS displacement of surface-layer atoms
//  roughness_proxy         stddev of z-coordinates in top surface layer
//  ejected_count           atoms that left slab beyond z_eject threshold
//  embedded_count          atoms that entered slab below z_embed threshold
//  E_rel_drift             energy drift of the combined system
//  stationary_flag
//  emergent_class          analysis label — never stored in state
//
// Emergent class vocabulary
//   reflection              incoming leaves surface with similar speed
//   grazing                 very shallow angle, little penetration
//   temporary_residence     stays near surface, then leaves
//   adsorption_like         stays near surface, KE_out << KE_in
//   embedding_like          penetrates into slab, embedded_count > 0
//   surface_disruption      surface residual spikes significantly
//   ejection_like           ejected_count > 0 at some frame
//   surface_relaxation      stable state after disturbance, residual decays
//   no_interaction          min_surface_distance stayed large
//
// Anti-black-box: every computation explicit and traceable.
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
#include <limits>

namespace vsepr::surface {

// ─────────────────────────────────────────────────────────────────────────────
// SurfaceMetricsRow  — one row of the per-frame output table
// ─────────────────────────────────────────────────────────────────────────────

struct SurfaceMetricsRow {
	std::string case_id;
	uint64_t    frame               = 0;
	double      time                = 0.0;
	uint32_t    incoming_id         = 0;       // atom index of incoming particle
	double      min_surface_distance= 0.0;     // Å
	int         residence_frames    = 0;
	double      reflection_angle_deg= 0.0;     // degrees (0=normal, 90=tangential)
	double      surface_residual    = 0.0;     // Å — RMS slab-surface atom displacement
	double      roughness_proxy     = 0.0;     // Å — stddev of surface-layer z
	int         ejected_count       = 0;
	int         embedded_count      = 0;
	double      E_rel_drift         = 0.0;
	bool        stationary_flag     = false;
	std::string emergent_class;

	std::string to_tsv() const {
		std::ostringstream ss;
		ss << std::fixed << std::setprecision(6)
		   << case_id              << '\t'
		   << frame                << '\t'
		   << time                 << '\t'
		   << incoming_id          << '\t'
		   << min_surface_distance << '\t'
		   << residence_frames     << '\t'
		   << reflection_angle_deg << '\t'
		   << surface_residual     << '\t'
		   << roughness_proxy      << '\t'
		   << ejected_count        << '\t'
		   << embedded_count       << '\t'
		   << E_rel_drift          << '\t'
		   << (stationary_flag ? 1 : 0) << '\t'
		   << emergent_class;
		return ss.str();
	}

	static std::string tsv_header() {
		return "case_id\tframe\ttime\tincoming_id"
			   "\tmin_surface_distance\tresidence_frames"
			   "\treflection_angle_deg"
			   "\tsurface_residual\troughness_proxy"
			   "\tejected_count\tembedded_count"
			   "\tE_rel_drift\tstationary_flag\temergent_class";
	}
};

// ─────────────────────────────────────────────────────────────────────────────
// classify_surface_event()  — trajectory label
// ─────────────────────────────────────────────────────────────────────────────

inline std::string classify_surface_event(
		double min_surface_distance,
		int    residence_frames,
		double reflection_angle_deg,
		double surface_residual,
		double surface_residual_ref,   // residual of undisturbed surface
		int    ejected_count,
		int    embedded_count,
		double KE_ratio,               // KE_outgoing / KE_incoming (1=elastic)
		double r_contact = 6.0)        // Å — within this = "contact"
{
	// Priority order: trajectory-confirmed events first, absence of interaction last.
	// An embedded atom is a harder fact than whether the approach distance was small.
	if (embedded_count > 0)
		return "embedded_event";
	if (ejected_count > 0)
		return "ejection_event";
	if (residence_frames > 5)
		return "temporary_residence";
	if (min_surface_distance > r_contact)
		return "no_interaction";
	if (surface_residual > surface_residual_ref * 3.0)
		return "surface_disruption";
	if (KE_ratio < 0.1 && residence_frames > 3)
		return "adsorption_like";
	if (reflection_angle_deg > 75.0)
		return "grazing";
	return "reflection";
}

// ─────────────────────────────────────────────────────────────────────────────
// SurfaceAnalyzer
// ─────────────────────────────────────────────────────────────────────────────
//
// Configuration:
//   slab_N          — number of atoms that belong to the slab
//   incoming_id     — index of the incoming atom in the combined State
//   surface_z_ref   — ideal z of the top surface layer (Å)
//   z_eject         — z above which atom is counted as ejected (Å)
//   z_embed         — z below which atom is counted as embedded (Å, from top layer)
//   r_reside        — distance from surface within which = "residing" (Å)
//   surface_layer_z_min — z threshold for "surface layer" atoms

struct SurfaceAnalyzer {
	uint32_t slab_N          = 0;
	uint32_t incoming_id     = 0;
	double   surface_z_ref   = 0.0;
	double   z_eject         = 0.0;    // slab atoms: if z > z_eject → ejected
	double   z_embed         = 0.0;    // incoming atom: if z < z_embed → embedded
	double   r_reside        = 6.0;    // Å — near surface threshold
	double   surface_layer_z_min = 0.0;
	double   E0              = 0.0;

	// Reference surface-layer positions (ideal, for residual calc)
	std::vector<vsepr::Vec3> surface_ref_pos;

	// Session tracking (updated each push)
	double KE_incoming_first  = 0.0;
	double KE_outgoing_last   = 0.0;
	int    residence_frames_total = 0;
	double min_surface_distance_ever = std::numeric_limits<double>::max();

	// Outgoing velocity of incoming atom (last seen)
	vsepr::Vec3 last_vel_incoming = {0,0,0};

	void set_baseline(double E_total_0) { E0 = E_total_0; }

	void set_surface_reference(const std::vector<vsepr::Vec3>& slab_pos) {
		surface_ref_pos.clear();
		for (const auto& p : slab_pos)
			if (p.z >= surface_layer_z_min)
				surface_ref_pos.push_back(p);
	}

	SurfaceMetricsRow compute(
			const std::string&               case_id,
			uint64_t                         frame_idx,
			double                           sim_time,
			double                           E_total,
			const std::vector<vsepr::Vec3>&  all_pos,
			const std::vector<vsepr::Vec3>&  all_vel,
			bool                             stationary = false)
	{
		SurfaceMetricsRow row;
		row.case_id     = case_id;
		row.frame       = frame_idx;
		row.time        = sim_time;
		row.incoming_id = incoming_id;
		row.stationary_flag = stationary;

		const double denom = std::abs(E0) > 1e-12 ? std::abs(E0) : 1.0;
		row.E_rel_drift = (E_total - E0) / denom;

		const vsepr::Vec3& inc_pos = all_pos[incoming_id];
		const vsepr::Vec3& inc_vel = all_vel[incoming_id];

		// ── Distance from surface plane ───────────────────────────────────────
		const double dist_to_surface = inc_pos.z - surface_z_ref;
		row.min_surface_distance = std::abs(dist_to_surface);
		min_surface_distance_ever = std::min(min_surface_distance_ever,
											 row.min_surface_distance);

		// ── Residence tracking ───────────────────────────────────────────────
		if (row.min_surface_distance < r_reside) {
			++residence_frames_total;
			KE_outgoing_last = 0.5 * (inc_vel.x*inc_vel.x +
									  inc_vel.y*inc_vel.y +
									  inc_vel.z*inc_vel.z) * 39.948 * 418.4;
		}
		row.residence_frames = residence_frames_total;
		last_vel_incoming = inc_vel;

		// ── Reflection angle (angle of vel w.r.t. surface normal = z-axis) ───
		const double v_mag = std::sqrt(inc_vel.x*inc_vel.x +
									   inc_vel.y*inc_vel.y +
									   inc_vel.z*inc_vel.z);
		if (v_mag > 1e-12) {
			const double cos_theta = std::abs(inc_vel.z) / v_mag;
			row.reflection_angle_deg = std::acos(std::min(1.0, cos_theta)) * 180.0
									   / 3.14159265358979;
		}

		// ── Surface structural residual (surface-layer atoms only) ────────────
		{
			std::vector<vsepr::Vec3> cur_surface;
			for (uint32_t i = 0; i < slab_N; ++i) {
				if (all_pos[i].z >= surface_layer_z_min)
					cur_surface.push_back(all_pos[i]);
			}

			double sum_sq = 0.0;
			const std::size_t N_sl = std::min(cur_surface.size(),
											  surface_ref_pos.size());
			for (std::size_t i = 0; i < N_sl; ++i) {
				const double dx = cur_surface[i].x - surface_ref_pos[i].x;
				const double dy = cur_surface[i].y - surface_ref_pos[i].y;
				const double dz = cur_surface[i].z - surface_ref_pos[i].z;
				sum_sq += dx*dx + dy*dy + dz*dz;
			}
			row.surface_residual = (N_sl > 0)
				? std::sqrt(sum_sq / static_cast<double>(N_sl)) : 0.0;

			// roughness_proxy — stddev of z among surface-layer atoms
			if (!cur_surface.empty()) {
				double mean_z = 0.0;
				for (const auto& p : cur_surface) mean_z += p.z;
				mean_z /= static_cast<double>(cur_surface.size());
				double var_z = 0.0;
				for (const auto& p : cur_surface) {
					const double dz = p.z - mean_z;
					var_z += dz * dz;
				}
				row.roughness_proxy = std::sqrt(var_z / static_cast<double>(cur_surface.size()));
			}
		}

		// ── Eject / embed counting ────────────────────────────────────────────
		{
			int ejected = 0, embedded = 0;
			for (uint32_t i = 0; i < slab_N; ++i)
				if (all_pos[i].z > z_eject) ++ejected;
			// Incoming atom embedded: moved into slab (below surface - some depth)
			if (inc_pos.z < z_embed) ++embedded;
			row.ejected_count  = ejected;
			row.embedded_count = embedded;
		}

		// ── Classification ────────────────────────────────────────────────────
		const double KE_ratio = (KE_incoming_first > 1e-12)
			? KE_outgoing_last / KE_incoming_first : 0.0;
		const double ref_residual = surface_ref_pos.empty() ? 0.01 : 0.01;
		row.emergent_class = classify_surface_event(
			row.min_surface_distance,
			row.residence_frames,
			row.reflection_angle_deg,
			row.surface_residual,
			ref_residual,
			row.ejected_count,
			row.embedded_count,
			KE_ratio,
			r_reside);

		return row;
	}
};

// ─────────────────────────────────────────────────────────────────────────────
// SurfaceSweepResult  — per-case summary for the full parameter sweep
// ─────────────────────────────────────────────────────────────────────────────

struct SurfaceSweepResult {
	std::string case_id;
	std::string velocity_class;        // low/med/high/grazing/angled/stream
	double      v_normal              = 0.0;   // Å/fs
	double      v_tangential          = 0.0;
	double      min_surface_distance  = 0.0;
	int         residence_frames      = 0;
	double      final_reflection_angle= 0.0;
	double      final_surface_residual= 0.0;
	double      final_roughness_proxy = 0.0;
	int         max_ejected           = 0;
	int         max_embedded          = 0;
	double      energy_drift          = 0.0;
	std::string final_class;

	std::string to_tsv() const {
		std::ostringstream ss;
		ss << std::fixed << std::setprecision(4)
		   << case_id               << '\t'
		   << velocity_class        << '\t'
		   << v_normal              << '\t'
		   << v_tangential          << '\t'
		   << min_surface_distance  << '\t'
		   << residence_frames      << '\t'
		   << final_reflection_angle<< '\t'
		   << final_surface_residual<< '\t'
		   << final_roughness_proxy << '\t'
		   << max_ejected           << '\t'
		   << max_embedded          << '\t'
		   << energy_drift          << '\t'
		   << final_class;
		return ss.str();
	}

	static std::string tsv_header() {
		return "case_id\tvelocity_class\tv_normal\tv_tangential"
			   "\tmin_surface_distance\tresidence_frames"
			   "\tfinal_reflection_angle"
			   "\tfinal_surface_residual\tfinal_roughness_proxy"
			   "\tmax_ejected\tmax_embedded"
			   "\tenergy_drift\tfinal_class";
	}
};

} // namespace vsepr::surface
