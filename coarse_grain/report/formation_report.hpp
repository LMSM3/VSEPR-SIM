#pragma once
/**
 * formation_report.hpp — Five-Factor Formation Reporting System
 * =============================================================
 *
 * Implements the unified formation-factor scoring framework:
 *
 *   F_total = F_E + F_M + F_K + F_T + F_N
 *
 * Where:
 *   F_E : Energetic factors     — energy, convergence, basin depth
 *   F_M : Motif factors         — geometric motif detection / persistence
 *   F_K : Kinetic factors       — diffusion, rearrangement, trapping
 *   F_T : Thermodynamic factors — temperature schedule, free energy
 *   F_N : Network/topology      — connectivity, coordination, defects
 *
 * The system reports not just final energy and geometry, but also:
 *   - WHY a structure emerged
 *   - What blocked better formation
 *   - Whether the result is energetically favored but kinetically trapped
 *   - Whether motifs formed, persisted, or collapsed
 *   - Whether diffusion / anneal / stochasticity were sufficient
 *
 * Design:
 *   - All data structures are plain aggregates (no hidden state)
 *   - All scoring functions are inspectable (explicit weights)
 *   - Classification produces human-readable interpretation
 *   - Export to text, CSV, and JSON
 *
 * Anti-black-box: every weight, every threshold, every heuristic is
 * visible in the source. No magic numbers without comments.
 *
 * VSEPR-SIM  |  2026-04-17
 */

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace coarse_grain {

// ============================================================================
// Utility
// ============================================================================

inline double clamp01(double x) {
	return std::max(0.0, std::min(1.0, x));
}

// ============================================================================
// Module A: Energetic Report
// ============================================================================

struct EnergyReport {
	double total_energy{};
	double bond_energy{};
	double angle_energy{};
	double torsion_energy{};
	double vdW_energy{};
	double coulomb_energy{};
	double environment_energy{};

	double residual_force_norm{};
	double basin_depth_estimate{};
	bool   converged{false};
	int    fire_iterations{};
};

// ============================================================================
// Module B: Motif Formation Report
// ============================================================================

/**
 * GeometricMotifType — structural motifs detected in bead arrangements.
 *
 * Distinct from atomistic::reaction::MotifType which covers chemical
 * functional groups (amine, carboxyl, etc.). These are spatial/geometric.
 */
enum class GeometricMotifType : uint8_t {
	None,
	Tetrahedron,
	Octahedron,
	Icosahedron,
	Chain,
	Sheet,
	Cage,
	Custom
};

inline const char* geometric_motif_name(GeometricMotifType t) {
	switch (t) {
		case GeometricMotifType::Tetrahedron: return "Tetrahedron";
		case GeometricMotifType::Octahedron:  return "Octahedron";
		case GeometricMotifType::Icosahedron: return "Icosahedron";
		case GeometricMotifType::Chain:       return "Chain";
		case GeometricMotifType::Sheet:       return "Sheet";
		case GeometricMotifType::Cage:        return "Cage";
		case GeometricMotifType::Custom:      return "Custom";
		default:                              return "None";
	}
}

struct MotifStat {
	GeometricMotifType type{GeometricMotifType::None};
	int    detected_count{};
	int    completed_count{};
	int    failed_count{};
	double average_distortion{};
	double persistence_score{};   // [0, 1]
	double closure_score{};       // [0, 1]
};

struct MotifReport {
	std::vector<MotifStat> motif_stats;
	double total_motif_score{};
};

// ============================================================================
// Module C: Kinetic Accessibility Report
// ============================================================================

struct KineticReport {
	double translational_diffusion_proxy{};
	double rotational_diffusion_proxy{};
	int    neighbor_swap_events{};
	int    bond_rearrangement_events{};
	int    local_trap_escapes{};
	double average_settling_time{};
	double jammed_fraction{};
	bool   likely_kinetically_trapped{false};
};

// ============================================================================
// Module D: Thermodynamic / Environmental Report
// ============================================================================

struct ThermalReport {
	double initial_temperature{};
	double final_temperature{};
	double average_temperature{};
	double quench_rate{};
	double pressure_proxy{};

	double enthalpy_proxy{};
	double entropy_proxy{};
	double free_energy_proxy{};

	int latent_heat_events{};
};

// ============================================================================
// Module E: Network / Topology Report
// ============================================================================

enum class ConnectivityType : uint8_t {
	Unknown,
	Isolated,
	Chain1D,
	Layer2D,
	Framework3D,
	ClusterLinked,
	Mixed
};

inline const char* connectivity_name(ConnectivityType t) {
	switch (t) {
		case ConnectivityType::Isolated:      return "Isolated";
		case ConnectivityType::Chain1D:        return "Chain1D";
		case ConnectivityType::Layer2D:        return "Layer2D";
		case ConnectivityType::Framework3D:    return "Framework3D";
		case ConnectivityType::ClusterLinked:  return "ClusterLinked";
		case ConnectivityType::Mixed:          return "Mixed";
		default:                              return "Unknown";
	}
}

struct TopologyReport {
	ConnectivityType connectivity{ConnectivityType::Unknown};
	double average_coordination{};
	double coordination_variance{};
	int    connected_components{};
	int    bridge_success_count{};
	int    bridge_failure_count{};
	int    topological_defects{};
	double void_fraction{};
	double network_score{};
};

// ============================================================================
// Unified Formation Report
// ============================================================================

struct FormationReport {
	EnergyReport   energy;
	MotifReport    motif;
	KineticReport  kinetic;
	ThermalReport  thermal;
	TopologyReport topology;

	double energetic_score{};
	double motif_score{};
	double kinetic_score{};
	double thermal_score{};
	double topology_score{};

	double emergence_score{};
	std::string classification;
	std::string interpretation;
};

// ============================================================================
// Score Calculators
// ============================================================================

/**
 * Energetic score (F_E).
 *
 * Lower total energy, lower residual force, deeper basin, and
 * convergence all contribute positively.
 *
 * Weights: energy 0.35, force 0.30, basin 0.20, convergence 0.15
 */
inline double compute_energetic_score(const EnergyReport& r) {
	const double e_term = 1.0 / (1.0 + std::abs(r.total_energy));
	const double f_term = 1.0 / (1.0 + r.residual_force_norm);
	const double b_term = clamp01(r.basin_depth_estimate);
	const double c_term = r.converged ? 1.0 : 0.25;

	return clamp01(
		0.35 * e_term +
		0.30 * f_term +
		0.20 * b_term +
		0.15 * c_term
	);
}

/**
 * Motif score (F_M).
 *
 * Completed motifs matter more than merely detected ones.
 * Lower distortion and higher persistence contribute positively.
 * Failed motifs penalise.
 *
 * Per-motif weights: completion 0.40, persistence 0.30,
 *                    closure 0.25, distortion 0.15, failure -0.20
 */
inline double compute_motif_score(const MotifReport& r) {
	if (r.motif_stats.empty()) return 0.0;

	double total = 0.0;
	for (const auto& m : r.motif_stats) {
		const double completion =
			(m.detected_count > 0)
			? static_cast<double>(m.completed_count) / m.detected_count
			: 0.0;

		const double failure_penalty =
			(m.detected_count > 0)
			? static_cast<double>(m.failed_count) / m.detected_count
			: 0.0;

		const double distortion_term = 1.0 / (1.0 + m.average_distortion);

		const double score = clamp01(
			0.40 * completion +
			0.30 * m.persistence_score +
			0.25 * m.closure_score +
			0.15 * distortion_term -
			0.20 * failure_penalty
		);

		total += score;
	}

	return clamp01(total / static_cast<double>(r.motif_stats.size()));
}

/**
 * Kinetic score (F_K).
 *
 * Some rearrangement is good (exploration). Too much jamming is bad.
 * Trap escapes can signal healthy dynamics. Low diffusion blocks formation.
 * Kinetic trapping applies a 0.6x penalty.
 *
 * Weights: trans_diff 0.20, rot_diff 0.15, swaps 0.20,
 *          rearrange 0.20, escapes 0.10, settle 0.15, jam -0.25
 */
inline double compute_kinetic_score(const KineticReport& r) {
	const double td = clamp01(r.translational_diffusion_proxy);
	const double rd = clamp01(r.rotational_diffusion_proxy);

	const double swap_term  = 1.0 - std::exp(-0.05 * static_cast<double>(r.neighbor_swap_events));
	const double rearr_term = 1.0 - std::exp(-0.05 * static_cast<double>(r.bond_rearrangement_events));
	const double escape_term = 1.0 - std::exp(-0.20 * static_cast<double>(r.local_trap_escapes));
	const double settle_term = 1.0 / (1.0 + r.average_settling_time);
	const double jam_penalty = clamp01(r.jammed_fraction);

	double score = (
		0.20 * td +
		0.15 * rd +
		0.20 * swap_term +
		0.20 * rearr_term +
		0.10 * escape_term +
		0.15 * settle_term -
		0.25 * jam_penalty
	);

	if (r.likely_kinetically_trapped) {
		score *= 0.6;
	}

	return clamp01(score);
}

/**
 * Thermal score (F_T).
 *
 * Not "higher T good" or "lower T good" — scores based on schedule
 * plausibility and free-energy favourability.
 *
 * Weights: free_energy 0.50, quench 0.30, latent_heat 0.20
 */
inline double compute_thermal_score(const ThermalReport& r) {
	const double stable_free_energy = 1.0 / (1.0 + std::abs(r.free_energy_proxy));
	const double quench_term = 1.0 / (1.0 + std::abs(r.quench_rate));
	const double latent_term = 1.0 - std::exp(-0.15 * static_cast<double>(r.latent_heat_events));

	return clamp01(
		0.50 * stable_free_energy +
		0.30 * quench_term +
		0.20 * latent_term
	);
}

/**
 * Topology score (F_N).
 *
 * Better connectivity scores higher (Framework3D > Chain1D > Isolated).
 * Fewer defects, tighter coordination distribution, and successful
 * bridges all contribute.
 *
 * Weights: connectivity 0.30, coord_var 0.25, defects 0.25, bridges 0.20
 */
inline double compute_topology_score(const TopologyReport& r) {
	const double coord_term  = 1.0 / (1.0 + r.coordination_variance);
	const double defect_term = 1.0 / (1.0 + static_cast<double>(r.topological_defects));

	const double bridge_total =
		static_cast<double>(r.bridge_success_count + r.bridge_failure_count);

	const double bridge_term =
		(bridge_total > 0.0)
		? static_cast<double>(r.bridge_success_count) / bridge_total
		: 0.0;

	double connectivity_term = 0.0;
	switch (r.connectivity) {
		case ConnectivityType::Framework3D:   connectivity_term = 1.00; break;
		case ConnectivityType::ClusterLinked: connectivity_term = 0.90; break;
		case ConnectivityType::Layer2D:       connectivity_term = 0.75; break;
		case ConnectivityType::Chain1D:       connectivity_term = 0.55; break;
		case ConnectivityType::Mixed:         connectivity_term = 0.65; break;
		case ConnectivityType::Isolated:      connectivity_term = 0.20; break;
		default:                             connectivity_term = 0.30; break;
	}

	return clamp01(
		0.30 * connectivity_term +
		0.25 * coord_term +
		0.25 * defect_term +
		0.20 * bridge_term
	);
}

// ============================================================================
// Emergence Score — Unified Summary
// ============================================================================

/**
 * Compute the unified emergence score and populate all sub-scores.
 *
 * Weights: energetic 0.25, motif 0.25, kinetic 0.20,
 *          thermal 0.10, topology 0.20
 */
inline double compute_emergence_score(FormationReport& r) {
	r.energetic_score = compute_energetic_score(r.energy);
	r.motif_score     = compute_motif_score(r.motif);
	r.kinetic_score   = compute_kinetic_score(r.kinetic);
	r.thermal_score   = compute_thermal_score(r.thermal);
	r.topology_score  = compute_topology_score(r.topology);

	r.emergence_score = clamp01(
		0.25 * r.energetic_score +
		0.25 * r.motif_score +
		0.20 * r.kinetic_score +
		0.10 * r.thermal_score +
		0.20 * r.topology_score
	);

	return r.emergence_score;
}

// ============================================================================
// Auto-Classification
// ============================================================================

/**
 * Classify the formation outcome and generate a human-readable
 * interpretation string explaining the result.
 *
 * Thresholds:
 *   >= 0.85  High-confidence emergent structure
 *   >= 0.65  Plausible structured formation
 *   >= 0.45  Partial / metastable formation
 *   >= 0.25  Weakly organised or trapped
 *   <  0.25  Failed or disordered formation
 */
inline void classify_report(FormationReport& r) {
	const double s = r.emergence_score;

	if (s >= 0.85) {
		r.classification = "High-confidence emergent structure";
	} else if (s >= 0.65) {
		r.classification = "Plausible structured formation";
	} else if (s >= 0.45) {
		r.classification = "Partial / metastable formation";
	} else if (s >= 0.25) {
		r.classification = "Weakly organised or trapped";
	} else {
		r.classification = "Failed or disordered formation";
	}

	std::ostringstream oss;
	oss << std::fixed << std::setprecision(3);

	oss << "Energetic=" << r.energetic_score
		<< ", Motif=" << r.motif_score
		<< ", Kinetic=" << r.kinetic_score
		<< ", Thermal=" << r.thermal_score
		<< ", Topology=" << r.topology_score
		<< ". ";

	if (r.kinetic.likely_kinetically_trapped) {
		oss << "Likely limited by kinetic accessibility. ";
	}

	if (r.motif_score > r.energetic_score && r.topology_score < 0.5) {
		oss << "Local motifs formed, but network assembly remained incomplete. ";
	}

	if (r.energetic_score > 0.7 && r.kinetic_score < 0.4) {
		oss << "Energetically favorable state exists, but transport or rearrangement was insufficient. ";
	}

	if (r.topology.topological_defects > 0) {
		oss << "Topological defects persist in final state. ";
	}

	r.interpretation = oss.str();
}

// ============================================================================
// Export: Plain Text
// ============================================================================

inline std::string generate_report_text(const FormationReport& r) {
	std::ostringstream oss;
	oss << std::fixed << std::setprecision(4);

	oss << "=== Formation Report ===\n";
	oss << "Classification: " << r.classification << "\n";
	oss << "Emergence Score: " << r.emergence_score << "\n\n";

	oss << "[Energy]\n";
	oss << "  Total Energy: " << r.energy.total_energy << "\n";
	oss << "  Bond: " << r.energy.bond_energy << "\n";
	oss << "  Angle: " << r.energy.angle_energy << "\n";
	oss << "  Torsion: " << r.energy.torsion_energy << "\n";
	oss << "  vdW: " << r.energy.vdW_energy << "\n";
	oss << "  Coulomb: " << r.energy.coulomb_energy << "\n";
	oss << "  Environment: " << r.energy.environment_energy << "\n";
	oss << "  Residual Force Norm: " << r.energy.residual_force_norm << "\n";
	oss << "  Basin Depth: " << r.energy.basin_depth_estimate << "\n";
	oss << "  FIRE Iterations: " << r.energy.fire_iterations << "\n";
	oss << "  Converged: " << (r.energy.converged ? "true" : "false") << "\n\n";

	oss << "[Motifs]\n";
	if (r.motif.motif_stats.empty()) {
		oss << "  No motif data\n";
	} else {
		for (const auto& m : r.motif.motif_stats) {
			oss << "  " << geometric_motif_name(m.type) << ": "
				<< m.completed_count << "/" << m.detected_count << " completed"
				<< ", " << m.failed_count << " failed"
				<< ", distortion=" << m.average_distortion
				<< ", persist=" << m.persistence_score
				<< ", closure=" << m.closure_score << "\n";
		}
	}
	oss << "\n";

	oss << "[Kinetics]\n";
	oss << "  Translational Diffusion: " << r.kinetic.translational_diffusion_proxy << "\n";
	oss << "  Rotational Diffusion: " << r.kinetic.rotational_diffusion_proxy << "\n";
	oss << "  Neighbor Swaps: " << r.kinetic.neighbor_swap_events << "\n";
	oss << "  Bond Rearrangements: " << r.kinetic.bond_rearrangement_events << "\n";
	oss << "  Trap Escapes: " << r.kinetic.local_trap_escapes << "\n";
	oss << "  Settling Time: " << r.kinetic.average_settling_time << "\n";
	oss << "  Jammed Fraction: " << r.kinetic.jammed_fraction << "\n";
	oss << "  Kinetically Trapped: " << (r.kinetic.likely_kinetically_trapped ? "YES" : "no") << "\n\n";

	oss << "[Thermal]\n";
	oss << "  T_initial: " << r.thermal.initial_temperature << "\n";
	oss << "  T_final: " << r.thermal.final_temperature << "\n";
	oss << "  T_average: " << r.thermal.average_temperature << "\n";
	oss << "  Quench Rate: " << r.thermal.quench_rate << "\n";
	oss << "  Free Energy Proxy: " << r.thermal.free_energy_proxy << "\n";
	oss << "  Latent Heat Events: " << r.thermal.latent_heat_events << "\n\n";

	oss << "[Topology]\n";
	oss << "  Connectivity: " << connectivity_name(r.topology.connectivity) << "\n";
	oss << "  Avg Coordination: " << r.topology.average_coordination << "\n";
	oss << "  Coord Variance: " << r.topology.coordination_variance << "\n";
	oss << "  Connected Components: " << r.topology.connected_components << "\n";
	oss << "  Bridge Success/Fail: " << r.topology.bridge_success_count
		<< "/" << r.topology.bridge_failure_count << "\n";
	oss << "  Topological Defects: " << r.topology.topological_defects << "\n";
	oss << "  Void Fraction: " << r.topology.void_fraction << "\n\n";

	oss << "[Scores]\n";
	oss << "  Energetic: " << r.energetic_score << "\n";
	oss << "  Motif:     " << r.motif_score << "\n";
	oss << "  Kinetic:   " << r.kinetic_score << "\n";
	oss << "  Thermal:   " << r.thermal_score << "\n";
	oss << "  Topology:  " << r.topology_score << "\n";
	oss << "  Emergence: " << r.emergence_score << "\n\n";

	oss << "[Interpretation]\n";
	oss << "  " << r.interpretation << "\n";

	return oss.str();
}

// ============================================================================
// Export: CSV Summary Row
// ============================================================================

inline std::string csv_header() {
	return "emergence,energetic,motif,kinetic,thermal,topology,classification";
}

inline std::string generate_csv_row(const FormationReport& r) {
	std::ostringstream oss;
	oss << std::fixed << std::setprecision(6);
	oss << r.emergence_score << ","
		<< r.energetic_score << ","
		<< r.motif_score << ","
		<< r.kinetic_score << ","
		<< r.thermal_score << ","
		<< r.topology_score << ","
		<< "\"" << r.classification << "\"";
	return oss.str();
}

// ============================================================================
// Export: JSON
// ============================================================================

inline std::string generate_json_report(const FormationReport& r) {
	std::ostringstream oss;
	oss << std::fixed << std::setprecision(6);

	oss << "{\n";
	oss << "  \"emergence_score\": " << r.emergence_score << ",\n";
	oss << "  \"classification\": \"" << r.classification << "\",\n";
	oss << "  \"energetic_score\": " << r.energetic_score << ",\n";
	oss << "  \"motif_score\": " << r.motif_score << ",\n";
	oss << "  \"kinetic_score\": " << r.kinetic_score << ",\n";
	oss << "  \"thermal_score\": " << r.thermal_score << ",\n";
	oss << "  \"topology_score\": " << r.topology_score << ",\n";
	oss << "  \"energy\": {\n";
	oss << "    \"total\": " << r.energy.total_energy << ",\n";
	oss << "    \"bond\": " << r.energy.bond_energy << ",\n";
	oss << "    \"angle\": " << r.energy.angle_energy << ",\n";
	oss << "    \"torsion\": " << r.energy.torsion_energy << ",\n";
	oss << "    \"vdW\": " << r.energy.vdW_energy << ",\n";
	oss << "    \"coulomb\": " << r.energy.coulomb_energy << ",\n";
	oss << "    \"environment\": " << r.energy.environment_energy << ",\n";
	oss << "    \"residual_force_norm\": " << r.energy.residual_force_norm << ",\n";
	oss << "    \"basin_depth\": " << r.energy.basin_depth_estimate << ",\n";
	oss << "    \"converged\": " << (r.energy.converged ? "true" : "false") << ",\n";
	oss << "    \"fire_iterations\": " << r.energy.fire_iterations << "\n";
	oss << "  },\n";
	oss << "  \"kinetic\": {\n";
	oss << "    \"trans_diffusion\": " << r.kinetic.translational_diffusion_proxy << ",\n";
	oss << "    \"rot_diffusion\": " << r.kinetic.rotational_diffusion_proxy << ",\n";
	oss << "    \"neighbor_swaps\": " << r.kinetic.neighbor_swap_events << ",\n";
	oss << "    \"bond_rearrangements\": " << r.kinetic.bond_rearrangement_events << ",\n";
	oss << "    \"trap_escapes\": " << r.kinetic.local_trap_escapes << ",\n";
	oss << "    \"settling_time\": " << r.kinetic.average_settling_time << ",\n";
	oss << "    \"jammed_fraction\": " << r.kinetic.jammed_fraction << ",\n";
	oss << "    \"kinetically_trapped\": " << (r.kinetic.likely_kinetically_trapped ? "true" : "false") << "\n";
	oss << "  },\n";
	oss << "  \"thermal\": {\n";
	oss << "    \"T_initial\": " << r.thermal.initial_temperature << ",\n";
	oss << "    \"T_final\": " << r.thermal.final_temperature << ",\n";
	oss << "    \"quench_rate\": " << r.thermal.quench_rate << ",\n";
	oss << "    \"free_energy_proxy\": " << r.thermal.free_energy_proxy << ",\n";
	oss << "    \"latent_heat_events\": " << r.thermal.latent_heat_events << "\n";
	oss << "  },\n";
	oss << "  \"topology\": {\n";
	oss << "    \"connectivity\": \"" << connectivity_name(r.topology.connectivity) << "\",\n";
	oss << "    \"avg_coordination\": " << r.topology.average_coordination << ",\n";
	oss << "    \"coord_variance\": " << r.topology.coordination_variance << ",\n";
	oss << "    \"connected_components\": " << r.topology.connected_components << ",\n";
	oss << "    \"bridge_success\": " << r.topology.bridge_success_count << ",\n";
	oss << "    \"bridge_failure\": " << r.topology.bridge_failure_count << ",\n";
	oss << "    \"topological_defects\": " << r.topology.topological_defects << ",\n";
	oss << "    \"void_fraction\": " << r.topology.void_fraction << "\n";
	oss << "  },\n";
	oss << "  \"interpretation\": \"" << r.interpretation << "\"\n";
	oss << "}\n";

	return oss.str();
}

} // namespace coarse_grain
