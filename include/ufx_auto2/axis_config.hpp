// include/ufx_auto2/axis_config.hpp
// UFX_AUTO_2 Phases 2-9 -- Axis Configuration
// VSEPR-SIM v5 beta9
//
// AxisConfig defines the generation parameter space.
// Each axis is a discrete or continuous range that the RandomAxisSampler
// draws from when creating a candidate material state.
//
// Phase 2 active axes:
//   element_family, element, oxidation_state, coordination,
//   geometry, phase, temperature_K, pressure_atm, target_property
//
// Phase 6 axes (molecular):
//   canonical_smiles, heavy_atom_count, hbond_donor_count, hbond_acceptor_count
//
// Phase 7 axes (thermo):
//   delta_Hf_kJ_mol, cp_298_J_mol_K, eos_model,
//   T_min_K, T_max_K, phase_transition_flag, vapor_pressure_seed
//
// Phase 8 axes (crystal, solid only):
//   space_group, lattice_a_angstrom, packing_fraction,
//   energy_above_hull_eV_atom, structure_source
//
// Phase 9 axes (transport/mechanics):
//   strain_rate, test_environment, porosity, tortuosity,
//   thermal_gradient_K_m, flow_velocity_m_s
//
// Deferred to post-Phase 10:
//   radiation_field, corrosion_medium, surface_orientation,
//   dopant_fraction, defect_density

#pragma once

#include <string>
#include <vector>
#include <utility>   // pair
#include <optional>
#include <cstdint>

namespace vsepr::ufx {

// ============================================================================
// Discrete axis: one value sampled uniformly from a list of strings
// ============================================================================

struct DiscreteAxis {
	std::string              name;
	std::vector<std::string> values;

	bool empty() const noexcept { return values.empty(); }
};

// ============================================================================
// Continuous axis: uniform sample in [lo, hi]
// ============================================================================

struct ContinuousAxis {
	std::string name;
	double      lo  = 0.0;
	double      hi  = 1.0;
};

// ============================================================================
// AxisConfig -- full description of the Phase 2 generation space
// ============================================================================

struct AxisConfig {
	// Discrete axes
	DiscreteAxis element_family;      // "alkali", "alkaline_earth", "transition_metal",
									  // "post_transition", "metalloid", "nonmetal",
									  // "halogen", "noble_gas", "lanthanide", "actinide"
	DiscreteAxis element;             // "H","Li","C","N","O","F","Na","Mg","Al","Si",
									  // "P","S","Cl","K","Ca","Ti","V","Cr","Mn","Fe",
									  // "Co","Ni","Cu","Zn","Ga","Ge","As","Br","Zr",
									  // "Mo","Pd","Ag","Cd","Sn","I","Cs","Ba","La",
									  // "Ce","Pr","Nd","Gd","Tb","Dy","W","Re","Os",
									  // "Ir","Pt","Au","Hg","Pb","Bi","Th","U","Pu"
	DiscreteAxis geometry;            // "linear","trigonal_planar","tetrahedral",
									  // "square_planar","trigonal_bipyramidal",
									  // "octahedral","pentagonal_bipyramidal"
	DiscreteAxis phase;               // "gas","liquid","solid","powder","molten_salt"
	DiscreteAxis target_property;     // "force_field","thermo","crystal","transport","mechanical"

	// Integer-range axes (stored as continuous, rounded at sample time)
	ContinuousAxis oxidation_state;   // [-4, 8]
	ContinuousAxis coordination;      // [1, 12]

	// Continuous axes
	ContinuousAxis temperature_K;     // [50, 2500]
	ContinuousAxis pressure_atm;      // [0.001, 1000]

	// -----------------------------------------------------------------------
	// Factory: Phase 2 default configuration
	// -----------------------------------------------------------------------
	static AxisConfig default_phase2();

	// Validate that all axes are non-empty / ranges are sane.
	bool valid() const noexcept;
};

// ============================================================================
// AxisSample -- one drawn point from the full config space
// ============================================================================

struct AxisSample {
	// Phase 2 (mandatory)
	std::string element_family;
	std::string element;
	int         oxidation_state  = 0;
	int         coordination     = 4;
	std::string geometry;
	std::string phase;
	double      temperature_K    = 298.15;
	double      pressure_atm     = 1.0;
	std::string target_property;

	uint64_t    seed_used        = 0;   // seed that produced this sample
	int         sample_index     = 0;   // sequential index in this run

	// Phase 6 -- molecular descriptors
	std::optional<std::string> canonical_smiles;
	std::optional<int>         heavy_atom_count;
	std::optional<int>         hbond_donor_count;
	std::optional<int>         hbond_acceptor_count;

	// Phase 7 -- thermo/EOS
	std::optional<double>      delta_Hf_kJ_mol;
	std::optional<double>      cp_298_J_mol_K;
	std::optional<std::string> eos_model;
	std::optional<double>      T_min_K;
	std::optional<double>      T_max_K;
	std::optional<bool>        phase_transition_flag;
	std::optional<double>      vapor_pressure_seed;   // log10 at 298 K

	// Phase 8 -- crystal (solid-phase only)
	std::optional<std::string> space_group;
	std::optional<double>      lattice_a_angstrom;
	std::optional<double>      packing_fraction;
	std::optional<double>      energy_above_hull_eV_atom;
	std::optional<std::string> structure_source;      // "generated"|"mp_lookup"|"oqmd_lookup"

	// Phase 9 -- transport/mechanics
	std::optional<double>      strain_rate;            // s^-1
	std::optional<std::string> test_environment;       // "vacuum"|"air"|"molten_salt"|"brine"|"hydrogen"
	std::optional<double>      porosity;               // 0.0-0.60
	std::optional<double>      tortuosity;             // 1.0-5.0
	std::optional<double>      thermal_gradient_K_m;  // K/m
	std::optional<double>      flow_velocity_m_s;     // m/s
};

} // namespace vsepr::ufx
