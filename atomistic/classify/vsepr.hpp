#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// WO-83F/G/H/J: provider interfaces.  WO-84A wires ProviderSet into
// VSEPROptions by value, so the full type is required here (not just a
// forward declaration).  providers.hpp is header-only and lightweight.
#include "atomistic/classify/providers.hpp"

namespace atomistic {

struct State;

namespace classify {

enum class VSEPRElectronGeometry : std::uint8_t {
	Unknown = 0,
	Linear,
	TrigonalPlanar,
	Tetrahedral,
	TrigonalBipyramidal,
	Octahedral,
	ExpandedCoordination
};

enum class VSEPRMolecularShape : std::uint8_t {
	Unknown = 0,
	Atom,
	Linear,
	Bent,
	TrigonalPlanar,
	TrigonalPyramidal,
	Tetrahedral,
	SeeSaw,
	TShaped,
	TrigonalBipyramidal,
	SquarePlanar,
	SquarePyramidal,
	Octahedral,
	Irregular,
	ExpandedCoordination
};

struct VSEPRAngleStats {
	double min_angle_deg = 0.0;
	double max_angle_deg = 0.0;
	double mean_angle_deg = 0.0;
	double rms_deviation_deg = 0.0;
};

struct VSEPRSite {
	std::size_t center_index = 0;
	int center_Z = 0;

	std::size_t bonded_domain_count = 0;
	std::size_t lone_pair_domain_count = 0;
	std::size_t electron_domain_count = 0;

	VSEPRElectronGeometry electron_geometry =
		VSEPRElectronGeometry::Unknown;

	VSEPRMolecularShape molecular_shape =
		VSEPRMolecularShape::Unknown;

	VSEPRAngleStats angle_stats{};

	bool is_linear_like = false;
	bool is_planar_like = false;
	bool is_tetrahedral_like = false;
	bool is_hypervalent = false;

	double confidence = 0.0;
	std::string ax_label;

	bool used_element_lone_pair_inference = false;
	bool used_geometry_lone_pair_fallback = false;
	bool used_provider_lone_pair = false;   // WO-84A: lone pairs came from ProviderSet
	bool used_d8_square_planar = false;      // WO-84D: d8 square-planar override applied
};

struct VSEPRReport {
	std::vector<VSEPRSite> sites;

	std::size_t linear_count = 0;
	std::size_t planar_count = 0;
	std::size_t tetrahedral_count = 0;
	std::size_t bent_count = 0;
	std::size_t pyramidal_count = 0;
	std::size_t hypervalent_count = 0;

	// WO-84A: provenance summary for CLI/report narration.
	bool        provider_lone_pair_available = false; // ProviderSet.lone_pair was set
	std::size_t provider_lone_pair_sites     = 0;     // sites that used provider data
	std::size_t fallback_lone_pair_sites     = 0;     // sites that used element/geometry
};

struct VSEPROptions {
	double neighbor_cutoff_angstrom = 3.5;
	bool allow_element_lone_pair_inference = true;
	bool allow_geometry_only_fallback = true;

	double linear_tolerance_deg = 15.0;
	double planar_tolerance_deg = 18.0;
	double tetrahedral_tolerance_deg = 20.0;
	double octahedral_tolerance_deg = 20.0;

	// WO-84D: when true, 4-coordinate d8 transition-metal centres (e.g. Ni(II),
	// Pd(II), Pt(II), Au(III), Rh(I), Ir(I)) are classified square planar
	// instead of tetrahedral.  This is a supported disambiguation branch; it
	// only affects d8 metals with exactly 4 bonded domains and 0 lone pairs.
	bool allow_d8_square_planar = true;

	// WO-84A: optional chemistry providers.  When providers.lone_pair is
	// available it overrides element/geometry inference; otherwise the
	// deterministic fallbacks below remain in force.  Defaults to null()
	// (all absent) so existing callers keep identical behaviour.
	ProviderSet providers = ProviderSet::null();
};

// WO-84D: true if Z is a d8 metal centre eligible for the square-planar branch.
bool is_d8_square_planar_metal(int z);

VSEPRReport classify_vsepr_sites(
	const State& state,
	const VSEPROptions& options = {}
);

const char* to_string(VSEPRElectronGeometry geometry);
const char* to_string(VSEPRMolecularShape shape);

// ============================================================================
// WO-83D: VSEPR report formatter
// ============================================================================

// Plain-text summary of a VSEPRReport suitable for CLI output and test probing.
// Output is deterministic and contains no external dependencies.
std::string format_vsepr_report(const VSEPRReport& report);

// ============================================================================
// WO-83I: Hybridization hints
// ============================================================================

// Hybridization hint derived from molecular geometry.
// These are geometry-inferred suggestions, NOT guaranteed chemical truth.
enum class HybridizationHint : std::uint8_t {
	Unknown = 0,
	SP,       // linear-like site (2 bonded domains, ~180 deg)
	SP2,      // trigonal-planar-like site (3 bonded domains, ~120 deg)
	SP3,      // tetrahedral-like site (4 bonded domains, ~109 deg)
	SP3D,     // trigonal bipyramidal / see-saw / T-shaped (5 domains)
	SP3D2     // octahedral / square pyramidal / square planar (6 domains)
};

const char* to_string(HybridizationHint hint);

// Infer hybridization hint for a single VSEPRSite.
// Returns Unknown when site has fewer than 2 bonded domains or confidence < 0.3.
HybridizationHint hybridization_hint(const VSEPRSite& site);

} // namespace classify
} // namespace atomistic
