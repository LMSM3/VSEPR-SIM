#pragma once
/**
 * atomistic/reaction/dissolution.hpp
 * ===================================
 * Surface dissolution and acid-attack reaction module.
 *
 * Models the multi-step pathway of solid-to-aqueous dissolution:
 *   1. Surface site identification (terminal oxide, bridging oxide)
 *   2. Protonation ladder (M-O- -> M-OH -> M-OH2+)
 *   3. Lattice cleavage (M-O-M bond breaking)
 *   4. Hydration shell formation (M^n+ -> M(H2O)x^n+)
 *   5. Ligand exchange (aquo -> sulfato, chloro, etc.)
 *
 * Chem+ bond-pattern vocabulary:
 *   [M-O-M]^{bridging oxide}_{s}
 *   [M-O-]^{terminal oxide}_{surface}
 *   [M-OH]^{surface hydroxyl}_{surface}
 *   [M-OH2+]^{protonated leaving group}_{surface}
 *   [M(H2O)n^m+]^{aquo complex}_{aq}
 *   [M-O-L]^{ligand-bound}_{aq}
 *
 * WO-56D | v5.0.0
 */

#include "../core/state.hpp"
#include "engine.hpp"
#include "heat_gate.hpp"

#include <vector>
#include <string>
#include <map>
#include <cstdint>
#include <cmath>
#include <algorithm>

namespace atomistic {
namespace reaction {

// ============================================================================
// Surface Site Classification
// ============================================================================

/**
 * Classification of surface oxygen coordination environments
 */
enum class SurfaceSiteType : uint8_t {
	BRIDGING_OXIDE,      // M-O-M  (coordinatively saturated)
	TERMINAL_OXIDE,      // M-O-   (undercoordinated, reactive)
	HYDROXYL,            // M-OH   (protonated once)
	AQUA_LEAVING,        // M-OH2+ (protonated twice, labile)
	VACANCY,             // Empty site after dissolution
	BULK                 // Interior atom, not surface-exposed
};

inline const char* surface_site_name(SurfaceSiteType t) {
	switch (t) {
		case SurfaceSiteType::BRIDGING_OXIDE:  return "bridging_oxide";
		case SurfaceSiteType::TERMINAL_OXIDE:  return "terminal_oxide";
		case SurfaceSiteType::HYDROXYL:        return "hydroxyl";
		case SurfaceSiteType::AQUA_LEAVING:    return "aqua_leaving";
		case SurfaceSiteType::VACANCY:         return "vacancy";
		case SurfaceSiteType::BULK:            return "bulk";
		default: return "unknown";
	}
}

// ============================================================================
// Protonation State Tracking
// ============================================================================

/**
 * Protonation ladder position for a surface oxygen site
 */
struct ProtonationState {
	uint32_t    site_index;
	SurfaceSiteType type;
	int         proton_count;     // 0, 1, or 2
	double      pKa_estimate;     // Estimated acid dissociation constant
	double      lability;         // 0-1 scale: how easily the site detaches
	Vec3        position;
	std::string metal_element;    // Bonded metal (Fe, Al, Ti, etc.)
	uint8_t     metal_oxidation;  // Oxidation state of bonded metal
};

// ============================================================================
// Dissolution Event Record
// ============================================================================

/**
 * Record of a single dissolution event (metal release from surface)
 */
struct DissolutionEvent {
	uint64_t    frame_id;
	uint32_t    metal_index;
	std::string metal_element;
	int         oxidation_state;
	double      release_energy;   // kcal/mol - energy cost of detachment
	double      hydration_energy; // kcal/mol - energy gain from hydration
	double      net_energy;       // release_energy - hydration_energy
	int         coordination_before;
	int         coordination_after; // Typically 6 for octahedral aquo complex
	std::string bond_pattern_before; // e.g. "Fe-O-Fe"
	std::string bond_pattern_after;  // e.g. "Fe(H2O)6^3+"
	Vec3        position;
};

// ============================================================================
// Ligand Exchange Record
// ============================================================================

/**
 * Record of ligand substitution on dissolved metal center
 */
struct LigandExchangeEvent {
	uint64_t    frame_id;
	uint32_t    metal_index;
	std::string metal_element;
	std::string leaving_ligand;   // e.g. "H2O"
	std::string entering_ligand;  // e.g. "SO4^2-"
	std::string binding_mode;     // "monodentate", "bidentate", "bridging"
	double      exchange_energy;  // kcal/mol
	double      stability_constant; // log K
};

// ============================================================================
// Dissolution Pathway Configuration
// ============================================================================

/**
 * Configuration for dissolution kinetics and thermodynamics
 */
struct DissolutionConfig {
	// Protonation energetics (kcal/mol)
	double dG_first_protonation   = -12.0;  // M-O- + H+ -> M-OH
	double dG_second_protonation  = -8.0;   // M-OH + H+ -> M-OH2+

	// Lattice cleavage barriers (kcal/mol)
	double Ea_bridging_cleavage   = 25.0;   // M-O-M bond breaking
	double Ea_terminal_release    = 15.0;   // M-O- detachment

	// Hydration shell formation (kcal/mol)
	double dG_hydration_Fe3       = -105.0; // Fe3+ hexaaquo formation
	double dG_hydration_Fe2       = -85.0;  // Fe2+ hexaaquo formation
	double dG_hydration_Al3       = -115.0; // Al3+ hexaaquo formation
	double dG_hydration_generic   = -80.0;  // Default for other metals

	// Ligand exchange thermodynamics (kcal/mol)
	double dG_sulfate_mono        = -3.5;   // Monodentate sulfate binding
	double dG_sulfate_bi          = -6.0;   // Bidentate sulfate binding
	double dG_sulfate_bridge      = -8.5;   // Bridging sulfate formation

	// pH dependence
	double pH_reference           = 1.0;    // Reference pH for rate constants
	double pH_slope               = -0.5;   // d(log rate)/d(pH)

	// Temperature dependence
	double T_reference            = 298.15; // K
	double Ea_apparent            = 15.0;   // kcal/mol - apparent activation energy

	// Surface site density
	double site_density_per_nm2   = 5.0;    // Reactive sites per nm²
};

// ============================================================================
// Dissolution Statistics
// ============================================================================

/**
 * Aggregated dissolution statistics for a simulation
 */
struct DissolutionStats {
	uint64_t total_protonation_events    = 0;
	uint64_t total_dissolution_events    = 0;
	uint64_t total_ligand_exchanges      = 0;

	// Site-type breakdown
	uint64_t bridging_cleaved            = 0;
	uint64_t terminal_released           = 0;

	// Metal species tracking
	std::map<std::string, uint64_t> metal_releases;  // Element -> count
	std::map<std::string, uint64_t> ligand_bindings; // Ligand -> count

	// Energetics
	double total_release_energy          = 0.0;
	double total_hydration_energy        = 0.0;
	double total_ligand_exchange_energy  = 0.0;
	double net_dissolution_energy        = 0.0;

	// Rates
	double dissolution_rate_mol_per_s    = 0.0;
	double surface_recession_A_per_ns    = 0.0;

	// Contamination metrics
	double mobile_metal_concentration    = 0.0;  // mol/L
	double sulfate_bound_fraction        = 0.0;  // Fraction with SO4 ligands
};

// ============================================================================
// DISSOLUTION ENGINE
// ============================================================================

/**
 * Core dissolution engine: identifies surface sites, tracks protonation,
 * models lattice cleavage, and records contamination pathway events.
 */
class DissolutionEngine {
public:
	DissolutionEngine();
	explicit DissolutionEngine(const DissolutionConfig& cfg);

	// ========================================================================
	// Surface Analysis
	// ========================================================================

	/**
	 * Identify and classify surface sites in a solid structure
	 *
	 * @param solid  State representing the solid (oxide, hydroxide, etc.)
	 * @return List of classified surface sites
	 */
	std::vector<ProtonationState> identify_surface_sites(const State& solid);

	/**
	 * Determine if an oxygen atom is surface-exposed
	 *
	 * @param solid       Solid state
	 * @param oxygen_idx  Index of oxygen atom to check
	 * @return True if surface-exposed (coordination < bulk coordination)
	 */
	bool is_surface_oxygen(const State& solid, uint32_t oxygen_idx);

	/**
	 * Classify surface site type based on coordination
	 *
	 * @param solid       Solid state
	 * @param oxygen_idx  Index of oxygen atom
	 * @return Site classification
	 */
	SurfaceSiteType classify_site(const State& solid, uint32_t oxygen_idx);

	// ========================================================================
	// Protonation Dynamics
	// ========================================================================

	/**
	 * Evaluate protonation probability for a surface site
	 *
	 * Uses the protonation ladder model:
	 *   M-O- + H+ <-> M-OH     (pKa1)
	 *   M-OH + H+ <-> M-OH2+   (pKa2)
	 *
	 * @param site  Current protonation state
	 * @param pH    Solution pH
	 * @param T_K   Temperature in Kelvin
	 * @return Probability of next protonation step occurring
	 */
	double protonation_probability(
		const ProtonationState& site,
		double pH,
		double T_K
	);

	/**
	 * Advance protonation state of a site
	 *
	 * @param site  Site to modify (in-place)
	 * @param pH    Solution pH
	 * @param T_K   Temperature
	 * @return True if protonation occurred
	 */
	bool step_protonation(ProtonationState& site, double pH, double T_K);

	// ========================================================================
	// Lattice Cleavage
	// ========================================================================

	/**
	 * Evaluate cleavage probability for a protonated site
	 *
	 * M-OH2+ sites have high lability and can detach as H2O,
	 * releasing M^n+ into solution.
	 *
	 * @param site  Protonation state (must be AQUA_LEAVING type)
	 * @param T_K   Temperature
	 * @return Probability of cleavage per timestep
	 */
	double cleavage_probability(const ProtonationState& site, double T_K);

	/**
	 * Execute lattice cleavage and generate dissolution event
	 *
	 * @param solid     Solid state (modified in-place)
	 * @param site      Site undergoing cleavage
	 * @param frame_id  Simulation frame for event logging
	 * @return Dissolution event record
	 */
	DissolutionEvent execute_cleavage(
		State& solid,
		const ProtonationState& site,
		uint64_t frame_id
	);

	// ========================================================================
	// Hydration Shell Formation
	// ========================================================================

	/**
	 * Build hydration shell around released metal ion
	 *
	 * @param metal_element   Element symbol (Fe, Al, etc.)
	 * @param oxidation_state Charge on metal ion
	 * @return Hydration energy (negative = favorable)
	 */
	double hydration_energy(const std::string& metal_element, int oxidation_state);

	/**
	 * Determine preferred coordination number for aquo complex
	 *
	 * @param metal_element   Element symbol
	 * @param oxidation_state Charge
	 * @return Coordination number (typically 4-6)
	 */
	int preferred_coordination(const std::string& metal_element, int oxidation_state);

	// ========================================================================
	// Ligand Exchange
	// ========================================================================

	/**
	 * Evaluate ligand exchange probability
	 *
	 * Models substitution of aquo ligands by other species (SO4, Cl, etc.)
	 *
	 * @param metal_state     Current metal complex state
	 * @param ligand          Incoming ligand formula
	 * @param concentration   Ligand concentration (mol/L)
	 * @param T_K             Temperature
	 * @return Exchange probability per timestep
	 */
	double ligand_exchange_probability(
		const State& metal_state,
		const std::string& ligand,
		double concentration,
		double T_K
	);

	/**
	 * Execute ligand exchange and record event
	 *
	 * @param metal_state  Metal complex (modified in-place)
	 * @param ligand       Incoming ligand
	 * @param binding_mode "monodentate", "bidentate", or "bridging"
	 * @param frame_id     Simulation frame
	 * @return Ligand exchange event record
	 */
	LigandExchangeEvent execute_ligand_exchange(
		State& metal_state,
		const std::string& ligand,
		const std::string& binding_mode,
		uint64_t frame_id
	);

	// ========================================================================
	// Full Pathway Simulation
	// ========================================================================

	/**
	 * Run one dissolution timestep
	 *
	 * Evaluates all surface sites, advances protonation states,
	 * executes cleavage events, and tracks ligand exchanges.
	 *
	 * @param solid      Solid substrate state (modified)
	 * @param solution   Aqueous solution state (modified)
	 * @param pH         Solution pH
	 * @param T_K        Temperature
	 * @param dt_fs      Timestep in femtoseconds
	 * @param frame_id   Current simulation frame
	 * @return Number of dissolution events this step
	 */
	int step_dissolution(
		State& solid,
		State& solution,
		double pH,
		double T_K,
		double dt_fs,
		uint64_t frame_id
	);

	// ========================================================================
	// Statistics and Reporting
	// ========================================================================

	/**
	 * Get accumulated dissolution statistics
	 */
	const DissolutionStats& stats() const { return stats_; }
	size_t surface_site_count() const { return surface_sites_.size(); }

	/**
	 * Reset statistics for a new simulation
	 */
	void reset_stats();

	/**
	 * Generate bond-pattern string for current state
	 *
	 * Chem+ notation: [species]^{annotation}_{phase}
	 */
	std::string bond_pattern_string(const ProtonationState& site) const;

	/**
	 * Generate contamination pathway summary
	 */
	std::string pathway_summary() const;

	// ========================================================================
	// Configuration
	// ========================================================================

	void set_config(const DissolutionConfig& cfg) { config_ = cfg; }
	const DissolutionConfig& config() const { return config_; }

private:
	DissolutionConfig config_;
	DissolutionStats stats_;

	// Cached surface site data
	std::vector<ProtonationState> surface_sites_;

	// Event history
	std::vector<DissolutionEvent> dissolution_events_;
	std::vector<LigandExchangeEvent> ligand_events_;

	// Internal helpers
	double arrhenius_rate(double Ea, double T_K) const;
	double ph_factor(double pH) const;
	void update_stats(const DissolutionEvent& event);
	void update_stats(const LigandExchangeEvent& event);
};

// ============================================================================
// Inline Implementations
// ============================================================================

inline DissolutionEngine::DissolutionEngine()
	: config_(), stats_() {}

inline DissolutionEngine::DissolutionEngine(const DissolutionConfig& cfg)
	: config_(cfg), stats_() {}

inline void DissolutionEngine::reset_stats() {
	stats_ = DissolutionStats{};
	dissolution_events_.clear();
	ligand_events_.clear();
}

inline double DissolutionEngine::arrhenius_rate(double Ea, double T_K) const {
	// k = A * exp(-Ea / RT)
	// Using R = 1.987 cal/(mol·K)
	constexpr double R = 1.987e-3; // kcal/(mol·K)
	return std::exp(-Ea / (R * T_K));
}

inline double DissolutionEngine::ph_factor(double pH) const {
	// Rate increases with decreasing pH (more acidic)
	return std::pow(10.0, config_.pH_slope * (pH - config_.pH_reference));
}

inline std::string DissolutionEngine::bond_pattern_string(
	const ProtonationState& site
) const {
	switch (site.type) {
		case SurfaceSiteType::BRIDGING_OXIDE:
			return "[" + site.metal_element + "-O-" + site.metal_element +
				   "]^{bridging oxide}_{s}";
		case SurfaceSiteType::TERMINAL_OXIDE:
			return "[" + site.metal_element + "-O-]^{terminal oxide}_{surface}";
		case SurfaceSiteType::HYDROXYL:
			return "[" + site.metal_element + "-OH]^{surface hydroxyl}_{surface}";
		case SurfaceSiteType::AQUA_LEAVING:
			return "[" + site.metal_element + "-OH2+]^{leaving group}_{surface}";
		case SurfaceSiteType::VACANCY:
			return "[vacancy]^{dissolved}_{surface}";
		default:
			return "[" + site.metal_element + "]^{bulk}_{s}";
	}
}

} // namespace reaction
} // namespace atomistic
