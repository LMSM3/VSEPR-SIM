#include "atomistic/classify/vsepr.hpp"

#include "atomistic/core/state.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <numeric>
#include <optional>
#include <sstream>

namespace atomistic {
namespace classify {
namespace {

constexpr double kPi = 3.141592653589793238462643383279502884;
constexpr double kTetrahedralAngle = 109.47122063449069;
constexpr double kWaterLikeBentAngle = 104.5;

static double angle_deg(Vec3 a, Vec3 b) {
	const double na = norm(a);
	const double nb = norm(b);
	if (na == 0.0 || nb == 0.0) {
		return 0.0;
	}

	const double c = std::clamp(dot(a, b) / (na * nb), -1.0, 1.0);
	return std::acos(c) * 180.0 / kPi;
}

static double closest_delta(double angle, const std::vector<double>& ideal_angles) {
	if (ideal_angles.empty()) {
		return 0.0;
	}

	double best = std::numeric_limits<double>::max();
	for (double ideal : ideal_angles) {
		best = std::min(best, std::abs(angle - ideal));
	}
	return best;
}

static VSEPRAngleStats compute_angle_stats(
	const std::vector<double>& angles,
	const std::vector<double>& ideal_angles
) {
	VSEPRAngleStats stats{};
	if (angles.empty()) {
		return stats;
	}

	const auto [min_it, max_it] = std::minmax_element(angles.begin(), angles.end());
	stats.min_angle_deg = *min_it;
	stats.max_angle_deg = *max_it;
	stats.mean_angle_deg =
		std::accumulate(angles.begin(), angles.end(), 0.0) /
		static_cast<double>(angles.size());

	double sum_sq = 0.0;
	for (double angle : angles) {
		const double d = closest_delta(angle, ideal_angles);
		sum_sq += d * d;
	}

	stats.rms_deviation_deg = std::sqrt(sum_sq / static_cast<double>(angles.size()));
	return stats;
}

static VSEPRElectronGeometry electron_geometry_from_domain_count(std::size_t n) {
	switch (n) {
		case 0:
		case 1:
			return VSEPRElectronGeometry::Unknown;
		case 2:
			return VSEPRElectronGeometry::Linear;
		case 3:
			return VSEPRElectronGeometry::TrigonalPlanar;
		case 4:
			return VSEPRElectronGeometry::Tetrahedral;
		case 5:
			return VSEPRElectronGeometry::TrigonalBipyramidal;
		case 6:
			return VSEPRElectronGeometry::Octahedral;
		default:
			return VSEPRElectronGeometry::ExpandedCoordination;
	}
}

static VSEPRMolecularShape molecular_shape_from_AXE(std::size_t x, std::size_t e) {
	const std::size_t domains = x + e;

	if (x == 0) {
		return VSEPRMolecularShape::Atom;
	}
	if (x == 1) {
		return VSEPRMolecularShape::Linear;
	}
	if (domains == 2 && x == 2) {
		return VSEPRMolecularShape::Linear;
	}
	if (domains == 3) {
		if (x == 3) return VSEPRMolecularShape::TrigonalPlanar;
		if (x == 2) return VSEPRMolecularShape::Bent;
	}
	if (domains == 4) {
		if (x == 4) return VSEPRMolecularShape::Tetrahedral;
		if (x == 3) return VSEPRMolecularShape::TrigonalPyramidal;
		if (x == 2) return VSEPRMolecularShape::Bent;
	}
	if (domains == 5) {
		if (x == 5) return VSEPRMolecularShape::TrigonalBipyramidal;
		if (x == 4) return VSEPRMolecularShape::SeeSaw;
		if (x == 3) return VSEPRMolecularShape::TShaped;
		if (x == 2) return VSEPRMolecularShape::Linear;
	}
	if (domains == 6) {
		if (x == 6) return VSEPRMolecularShape::Octahedral;
		if (x == 5) return VSEPRMolecularShape::SquarePyramidal;
		if (x == 4) return VSEPRMolecularShape::SquarePlanar;
	}

	return domains > 6
		? VSEPRMolecularShape::ExpandedCoordination
		: VSEPRMolecularShape::Irregular;
}

static std::string make_ax_label(std::size_t x, std::size_t e) {
	std::string label = "AX" + std::to_string(x);
	if (e > 0) {
		label += "E";
		if (e > 1) {
			label += std::to_string(e);
		}
	}
	return label;
}

static double confidence_from_rms(double rms_deg) {
	return std::clamp(1.0 - (rms_deg / 45.0), 0.0, 1.0);
}

static std::optional<std::size_t> infer_lone_pairs_from_element(
	int center_z,
	std::size_t bonded_domains
) {
	switch (center_z) {
		case 1:
			return 0;
		case 5:
		case 6:
			return 0;
		case 7:
			if (bonded_domains <= 3) return 1;
			if (bonded_domains == 4) return 0;
			return std::nullopt;
		case 8:
			if (bonded_domains <= 2) return 2;
			if (bonded_domains == 3) return 1;
			if (bonded_domains == 4) return 0;
			return std::nullopt;
		case 9:
		case 17:
		case 35:
		case 53:
			if (bonded_domains <= 1) return 3;
			if (bonded_domains == 2) return 2;
			if (bonded_domains == 3 || bonded_domains == 4) return 1;
			return 0;
		case 15:
			if (bonded_domains >= 5) return 0;
			if (bonded_domains == 3 || bonded_domains == 4) return 1;
			if (bonded_domains == 2) return 2;
			return std::nullopt;
		case 16:
			if (bonded_domains >= 6) return 0;
			if (bonded_domains == 4 || bonded_domains == 5) return 1;
			if (bonded_domains == 2 || bonded_domains == 3) return 2;
			return std::nullopt;
		default:
			return std::nullopt;
	}
}

static std::vector<std::vector<std::size_t>> build_local_neighbors(
	const State& state,
	double cutoff
) {
	std::vector<std::vector<std::size_t>> neighbors(state.N);

	if (!state.B.empty()) {
		for (const Edge& edge : state.B) {
			if (edge.i >= state.N || edge.j >= state.N || edge.i == edge.j) {
				continue;
			}
			neighbors[edge.i].push_back(edge.j);
			neighbors[edge.j].push_back(edge.i);
		}
	} else {
		const double cutoff_sq = cutoff * cutoff;
		for (std::size_t i = 0; i < state.N; ++i) {
			for (std::size_t j = i + 1; j < state.N; ++j) {
				Vec3 dr = state.X[j] - state.X[i];
				if (state.box.enabled) {
					dr = state.box.delta(state.X[i], state.X[j]);
				}

				if (dot(dr, dr) <= cutoff_sq) {
					neighbors[i].push_back(j);
					neighbors[j].push_back(i);
				}
			}
		}
	}

	for (auto& adj : neighbors) {
		std::sort(adj.begin(), adj.end());
		adj.erase(std::unique(adj.begin(), adj.end()), adj.end());
	}

	return neighbors;
}

static std::vector<double> pairwise_angles_for_site(
	const State& state,
	std::size_t center,
	const std::vector<std::size_t>& neighbors
) {
	std::vector<double> angles;

	for (std::size_t a = 0; a < neighbors.size(); ++a) {
		for (std::size_t b = a + 1; b < neighbors.size(); ++b) {
			Vec3 va = state.X[neighbors[a]] - state.X[center];
			Vec3 vb = state.X[neighbors[b]] - state.X[center];

			if (state.box.enabled) {
				va = state.box.delta(state.X[center], state.X[neighbors[a]]);
				vb = state.box.delta(state.X[center], state.X[neighbors[b]]);
			}

			angles.push_back(angle_deg(va, vb));
		}
	}

	return angles;
}

static double rms_to_ideals(
	const std::vector<double>& angles,
	const std::vector<double>& ideals
) {
	return compute_angle_stats(angles, ideals).rms_deviation_deg;
}

static std::size_t infer_lone_pairs_from_geometry(
	std::size_t bonded_domains,
	const std::vector<double>& angles,
	const VSEPROptions& options
) {
	if (!options.allow_geometry_only_fallback || angles.empty()) {
		return 0;
	}

	if (bonded_domains == 2) {
		const double linear_rms = rms_to_ideals(angles, {180.0});
		if (linear_rms <= options.linear_tolerance_deg) {
			return 0;
		}

		const double tetra_bent_rms = rms_to_ideals(angles, {kWaterLikeBentAngle});
		if (tetra_bent_rms <= options.tetrahedral_tolerance_deg) {
			return 2;
		}

		return 1;
	}

	if (bonded_domains == 3) {
		const double planar_rms = rms_to_ideals(angles, {120.0});
		const double pyramidal_rms = rms_to_ideals(angles, {107.0, kTetrahedralAngle});

		if (pyramidal_rms <= options.tetrahedral_tolerance_deg &&
			pyramidal_rms < planar_rms) {
			return 1;
		}

		if (planar_rms <= options.planar_tolerance_deg) {
			return 0;
		}
	}

	return 0;
}

static std::vector<double> ideal_angles_for_shape(VSEPRMolecularShape shape) {
	switch (shape) {
		case VSEPRMolecularShape::Linear:
			return {180.0};
		case VSEPRMolecularShape::TrigonalPlanar:
			return {120.0};
		case VSEPRMolecularShape::TrigonalPyramidal:
			return {107.0, kTetrahedralAngle};
		case VSEPRMolecularShape::Tetrahedral:
			return {kTetrahedralAngle};
		case VSEPRMolecularShape::Bent:
			return {kWaterLikeBentAngle, 120.0};
		case VSEPRMolecularShape::TrigonalBipyramidal:
		case VSEPRMolecularShape::SeeSaw:
		case VSEPRMolecularShape::TShaped:
			return {90.0, 120.0, 180.0};
		case VSEPRMolecularShape::SquarePlanar:
		case VSEPRMolecularShape::SquarePyramidal:
		case VSEPRMolecularShape::Octahedral:
			return {90.0, 180.0};
		default:
			return {};
	}
}

static void update_report_counts(VSEPRReport& report, const VSEPRSite& site) {
	if (site.is_linear_like) {
		report.linear_count++;
	}
	if (site.is_planar_like) {
		report.planar_count++;
	}
	if (site.is_tetrahedral_like) {
		report.tetrahedral_count++;
	}
	if (site.molecular_shape == VSEPRMolecularShape::Bent) {
		report.bent_count++;
	}
	if (site.molecular_shape == VSEPRMolecularShape::TrigonalPyramidal) {
		report.pyramidal_count++;
	}
	if (site.is_hypervalent) {
		report.hypervalent_count++;
	}
}

} // namespace

VSEPRReport classify_vsepr_sites(
	const State& state,
	const VSEPROptions& options
) {
	VSEPRReport report;

	if (state.N == 0 || state.X.size() < state.N) {
		return report;
	}

	const auto neighbors = build_local_neighbors(state, options.neighbor_cutoff_angstrom);
	report.sites.reserve(state.N);

	// WO-84A: record whether a lone-pair provider is wired in for this run so
	// downstream CLI/report code can narrate provider vs fallback source.
	const bool lone_pair_provider_available = options.providers.lone_pair.available();
	report.provider_lone_pair_available = lone_pair_provider_available;

	for (std::size_t i = 0; i < state.N; ++i) {
		VSEPRSite site;
		site.center_index = i;
		site.center_Z = i < state.type.size()
			? static_cast<int>(state.type[i])
			: 0;
		site.bonded_domain_count = neighbors[i].size();

		const auto angles = pairwise_angles_for_site(state, i, neighbors[i]);

		// WO-84A: provider-first lone-pair resolution.  If a lone-pair
		// provider is available AND returns a value for this atom, it
		// overrides element/geometry inference.  Otherwise fall through to
		// the deterministic element and geometry heuristics unchanged.
		if (lone_pair_provider_available) {
			if (auto provided = options.providers.lone_pair.get(i)) {
				if (*provided >= 0) {
					site.lone_pair_domain_count =
						static_cast<std::size_t>(*provided);
					site.used_provider_lone_pair = true;
				}
			}
		}

		if (!site.used_provider_lone_pair &&
			options.allow_element_lone_pair_inference) {
			if (auto lone_pairs = infer_lone_pairs_from_element(
					site.center_Z,
					site.bonded_domain_count
				)) {
				site.lone_pair_domain_count = *lone_pairs;
				site.used_element_lone_pair_inference = true;
			}
		}

		if (!site.used_provider_lone_pair &&
			!site.used_element_lone_pair_inference) {
			site.lone_pair_domain_count =
				infer_lone_pairs_from_geometry(site.bonded_domain_count, angles, options);
			site.used_geometry_lone_pair_fallback =
				site.lone_pair_domain_count > 0;
		}

		if (site.used_provider_lone_pair) {
			++report.provider_lone_pair_sites;
		} else {
			++report.fallback_lone_pair_sites;
		}
		site.electron_domain_count =
			site.bonded_domain_count + site.lone_pair_domain_count;

		site.electron_geometry =
			electron_geometry_from_domain_count(site.electron_domain_count);
		site.molecular_shape =
			molecular_shape_from_AXE(site.bonded_domain_count, site.lone_pair_domain_count);

		// WO-84D: d8 square-planar disambiguation.  A bare AXE model maps a
		// 4-coordinate centre with no lone pairs to tetrahedral, but d8 metal
		// centres (Ni/Pd/Pt(II), Au(III), Rh/Ir(I)) are square planar.  Apply
		// the override only for that specific, supported case.
		if (options.allow_d8_square_planar &&
			site.bonded_domain_count == 4 &&
			site.lone_pair_domain_count == 0 &&
			is_d8_square_planar_metal(site.center_Z)) {
			site.molecular_shape = VSEPRMolecularShape::SquarePlanar;
			site.electron_geometry = VSEPRElectronGeometry::Octahedral;
			site.used_d8_square_planar = true;
		}

		site.ax_label =
			make_ax_label(site.bonded_domain_count, site.lone_pair_domain_count);

		site.angle_stats =
			compute_angle_stats(angles, ideal_angles_for_shape(site.molecular_shape));
		site.confidence = confidence_from_rms(site.angle_stats.rms_deviation_deg);
		// WO-83E: apply confidence penalty when lone pairs relied on geometry fallback
		if (site.used_geometry_lone_pair_fallback) {
			site.confidence *= 0.75;
		}

		site.is_linear_like =
			site.molecular_shape == VSEPRMolecularShape::Linear;
		site.is_planar_like =
			site.molecular_shape == VSEPRMolecularShape::TrigonalPlanar ||
			site.molecular_shape == VSEPRMolecularShape::SquarePlanar;
		site.is_tetrahedral_like =
			site.electron_geometry == VSEPRElectronGeometry::Tetrahedral;
		site.is_hypervalent = site.electron_domain_count > 4;

		update_report_counts(report, site);
		report.sites.push_back(site);
	}

	return report;
}

bool is_d8_square_planar_metal(int z) {
	// Common d8 centres that adopt square-planar 4-coordination:
	//   Ni(28), Pd(46), Pt(78)  -- group 10, +2
	//   Rh(45), Ir(77)          -- group  9, +1
	//   Co(27)                  -- d8 in some low-spin +1 complexes
	//   Au(79)                  -- +3
	switch (z) {
		case 27: // Co
		case 28: // Ni
		case 45: // Rh
		case 46: // Pd
		case 77: // Ir
		case 78: // Pt
		case 79: // Au
			return true;
		default:
			return false;
	}
}

const char* to_string(VSEPRElectronGeometry geometry) {
	switch (geometry) {
		case VSEPRElectronGeometry::Unknown:
			return "unknown";
		case VSEPRElectronGeometry::Linear:
			return "linear";
		case VSEPRElectronGeometry::TrigonalPlanar:
			return "trigonal_planar";
		case VSEPRElectronGeometry::Tetrahedral:
			return "tetrahedral";
		case VSEPRElectronGeometry::TrigonalBipyramidal:
			return "trigonal_bipyramidal";
		case VSEPRElectronGeometry::Octahedral:
			return "octahedral";
		case VSEPRElectronGeometry::ExpandedCoordination:
			return "expanded_coordination";
		default:
			return "unknown";
	}
}

const char* to_string(VSEPRMolecularShape shape) {
	switch (shape) {
		case VSEPRMolecularShape::Unknown:
			return "unknown";
		case VSEPRMolecularShape::Atom:
			return "atom";
		case VSEPRMolecularShape::Linear:
			return "linear";
		case VSEPRMolecularShape::Bent:
			return "bent";
		case VSEPRMolecularShape::TrigonalPlanar:
			return "trigonal_planar";
		case VSEPRMolecularShape::TrigonalPyramidal:
			return "trigonal_pyramidal";
		case VSEPRMolecularShape::Tetrahedral:
			return "tetrahedral";
		case VSEPRMolecularShape::SeeSaw:
			return "see_saw";
		case VSEPRMolecularShape::TShaped:
			return "t_shaped";
		case VSEPRMolecularShape::TrigonalBipyramidal:
			return "trigonal_bipyramidal";
		case VSEPRMolecularShape::SquarePlanar:
			return "square_planar";
		case VSEPRMolecularShape::SquarePyramidal:
			return "square_pyramidal";
		case VSEPRMolecularShape::Octahedral:
			return "octahedral";
		case VSEPRMolecularShape::Irregular:
			return "irregular";
		case VSEPRMolecularShape::ExpandedCoordination:
			return "expanded_coordination";
		default:
			return "unknown";
	}
}

// ============================================================================
// WO-83D: format_vsepr_report
// ============================================================================

std::string format_vsepr_report(const VSEPRReport& report) {
	if (report.sites.empty()) {
		return "vsepr_report: empty\n";
	}

	std::ostringstream oss;
	oss << std::fixed << std::setprecision(2);

	oss << "vsepr_report:\n";
	oss << "  sites:              " << report.sites.size() << "\n";
	oss << "  linear_count:       " << report.linear_count << "\n";
	oss << "  planar_count:       " << report.planar_count << "\n";
	oss << "  tetrahedral_count:  " << report.tetrahedral_count << "\n";
	oss << "  bent_count:         " << report.bent_count << "\n";
	oss << "  pyramidal_count:    " << report.pyramidal_count << "\n";
	oss << "  hypervalent_count:  " << report.hypervalent_count << "\n";
	oss << "\n";

	for (const auto& s : report.sites) {
		if (s.bonded_domain_count == 0) continue; // skip bare atoms
		oss << "  site[" << s.center_index << "]";
		if (s.center_Z > 0) {
			oss << "  Z=" << s.center_Z;
		}
		oss << "  " << s.ax_label;
		oss << "  shape=" << to_string(s.molecular_shape);
		oss << "  conf=" << s.confidence;
		oss << "  rms_dev=" << s.angle_stats.rms_deviation_deg << "deg";
		HybridizationHint hint = hybridization_hint(s);
		if (hint != HybridizationHint::Unknown) {
			oss << "  hybridization=" << to_string(hint);
		}
		if (s.used_geometry_lone_pair_fallback) {
			oss << "  [fallback_lp]";
		}
		if (s.is_hypervalent) {
			oss << "  [hypervalent]";
		}
		oss << "\n";
	}
	return oss.str();
}

// ============================================================================
// WO-83I: hybridization_hint + to_string
// ============================================================================

HybridizationHint hybridization_hint(const VSEPRSite& site) {
	if (site.confidence < 0.3) return HybridizationHint::Unknown;
	const auto edom = site.electron_domain_count;
	const auto bdom = site.bonded_domain_count;

	if (bdom < 2) return HybridizationHint::Unknown;

	switch (edom) {
		case 2:  return HybridizationHint::SP;
		case 3:  return HybridizationHint::SP2;
		case 4:  return HybridizationHint::SP3;
		case 5:  return HybridizationHint::SP3D;
		case 6:  return HybridizationHint::SP3D2;
		default: return HybridizationHint::Unknown;
	}
}

const char* to_string(HybridizationHint hint) {
	switch (hint) {
		case HybridizationHint::SP:    return "sp";
		case HybridizationHint::SP2:   return "sp2";
		case HybridizationHint::SP3:   return "sp3";
		case HybridizationHint::SP3D:  return "sp3d";
		case HybridizationHint::SP3D2: return "sp3d2";
		default:                       return "unknown";
	}
}

} // namespace classify
} // namespace atomistic
