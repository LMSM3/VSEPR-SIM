/**
 * atomistic/reaction/dissolution.cpp
 * ===================================
 * Implementation of surface dissolution and acid-attack reaction module.
 *
 * WO-56D | v5.0.0
 */

#include "dissolution.hpp"
#include "pot/periodic_db.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>

namespace atomistic {
namespace reaction {

namespace {

static const vsepr::PeriodicTable& periodic_table() {
    static const vsepr::PeriodicTable table = vsepr::PeriodicTable::load_default();
    return table;
}

static std::string z_to_symbol(uint32_t Z) {
    const auto* element = periodic_table().by_Z(Z);
    return element ? element->symbol : "?";
}

static uint32_t element_Z(const char* symbol) {
    const auto* element = periodic_table().by_symbol(symbol);
    return element ? element->Z : 0;
}

static uint8_t infer_oxide_oxidation(const State& state, uint32_t metal_Z) {
    double explicit_charge = 0.0;
    size_t charged_atoms = 0;
    size_t metal_atoms = 0;
    size_t oxygen_atoms = 0;
    const uint32_t oxygen_Z = element_Z("O");

    for (size_t i = 0; i < state.type.size(); ++i) {
        if (state.type[i] == metal_Z) {
            ++metal_atoms;
            if (i < state.Q.size() && std::abs(state.Q[i]) > 1e-9) {
                explicit_charge += std::abs(state.Q[i]);
                ++charged_atoms;
            }
        } else if (state.type[i] == oxygen_Z) {
            ++oxygen_atoms;
        }
    }

    if (charged_atoms > 0)
        return static_cast<uint8_t>(std::clamp(
            static_cast<int>(std::lround(explicit_charge / charged_atoms)), 1, 8));
    if (metal_atoms > 0 && oxygen_atoms > 0)
        return static_cast<uint8_t>(std::clamp(
            static_cast<int>(std::lround(2.0 * oxygen_atoms / metal_atoms)), 1, 8));
    return 1;
}

// Return all neighbour indices for atom idx by scanning State::B edge list.
static std::vector<uint32_t> neighbors_of(const State& s, uint32_t idx) {
    std::vector<uint32_t> result;
    for (const auto& e : s.B) {
        if (e.i == idx) result.push_back(e.j);
        else if (e.j == idx) result.push_back(e.i);
    }
    return result;
}

} // anonymous namespace

// ============================================================================
// Surface Analysis Implementation
// ============================================================================

std::vector<ProtonationState> DissolutionEngine::identify_surface_sites(
	const State& solid
) {
	std::vector<ProtonationState> sites;
	sites.reserve(solid.N / 4);  // Rough estimate: 25% are surface O

	for (uint32_t i = 0; i < solid.N; ++i) {
		// Only process oxygen atoms
		if (solid.type[i] != element_Z("O")) continue;

		// Check if surface-exposed
		if (!is_surface_oxygen(solid, i)) continue;

		ProtonationState ps;
		ps.site_index     = i;
		ps.type           = classify_site(solid, i);
		ps.proton_count   = (ps.type == SurfaceSiteType::HYDROXYL) ? 1 :
							(ps.type == SurfaceSiteType::AQUA_LEAVING) ? 2 : 0;
		ps.position       = solid.X[i];

		// Find bonded metal
		auto nbrs = neighbors_of(solid, i);
		for (auto n : nbrs) {
			const std::string elem = z_to_symbol(solid.type[n]);
			if (elem != "O" && elem != "H") {
				ps.metal_element = elem;
				ps.metal_oxidation = infer_oxide_oxidation(solid, solid.type[n]);
				break;
			}
		}

		constexpr double gas_constant_kcal = 1.987e-3;
		ps.pKa_estimate = -config_.dG_first_protonation /
			(2.303 * gas_constant_kcal * config_.T_reference);

		// Lability increases with protonation
		ps.lability = 0.1 + 0.4 * ps.proton_count;

		sites.push_back(ps);
	}

	surface_sites_ = sites;
	return sites;
}

bool DissolutionEngine::is_surface_oxygen(const State& solid, uint32_t oxygen_idx) {
	// Surface oxygen has fewer metal neighbors than bulk
	// Bulk O in Fe2O3: coordinated to 4 Fe atoms
	// Surface O: coordinated to 1-3 Fe atoms

	int metal_neighbors = 0;
	auto nbrs = neighbors_of(solid, oxygen_idx);

	for (auto n : nbrs) {
		const std::string elem = z_to_symbol(solid.type[n]);
		if (elem != "O" && elem != "H") {
			++metal_neighbors;
		}
	}

	if (!solid.box.enabled) return metal_neighbors > 0;

	int maximum_oxygen_coordination = 0;
	for (uint32_t i = 0; i < solid.N; ++i) {
		if (solid.type[i] != element_Z("O")) continue;
		int coordination = 0;
		for (const auto neighbor : neighbors_of(solid, i)) {
			const auto Z = solid.type[neighbor];
			if (Z != element_Z("O") && Z != element_Z("H")) ++coordination;
		}
		maximum_oxygen_coordination = std::max(maximum_oxygen_coordination, coordination);
	}
	return metal_neighbors < maximum_oxygen_coordination;
}

SurfaceSiteType DissolutionEngine::classify_site(
	const State& solid, uint32_t oxygen_idx
) {
	auto nbrs = neighbors_of(solid, oxygen_idx);

	int metal_count = 0;
	int hydrogen_count = 0;

	for (auto n : nbrs) {
		const std::string elem = z_to_symbol(solid.type[n]);
		if (elem == "H") {
			++hydrogen_count;
		} else if (elem != "O") {
			++metal_count;
		}
	}

	// Classification based on coordination and protonation
	if (hydrogen_count >= 2) {
		return SurfaceSiteType::AQUA_LEAVING;  // M-OH2+
	} else if (hydrogen_count == 1) {
		return SurfaceSiteType::HYDROXYL;      // M-OH
	} else if (metal_count >= 2) {
		return SurfaceSiteType::BRIDGING_OXIDE; // M-O-M
	} else if (metal_count == 1) {
		return SurfaceSiteType::TERMINAL_OXIDE; // M-O-
	} else if (metal_count == 0) {
		return SurfaceSiteType::VACANCY;
	}

	return SurfaceSiteType::BULK;
}

// ============================================================================
// Protonation Dynamics Implementation
// ============================================================================

double DissolutionEngine::protonation_probability(
	const ProtonationState& site,
	double pH,
	double T_K
) {
	// Henderson-Hasselbalch: fraction protonated = 1 / (1 + 10^(pH - pKa))
	// For multi-step protonation, use successive pKa values

	constexpr double gas_constant_kcal = 1.987e-3;
	const double protonation_dG = site.proton_count == 0
		? config_.dG_first_protonation : config_.dG_second_protonation;
	double pKa_effective = -protonation_dG /
		(2.303 * gas_constant_kcal * config_.T_reference);

	// Can't protonate further than 2
	if (site.proton_count >= 2) {
		return 0.0;
	}

	// Temperature correction (Van't Hoff)
	double T_factor = T_K / config_.T_reference;
	pKa_effective *= (2.0 - T_factor);  // pKa decreases with T

	// Protonation probability from equilibrium
	double fraction = 1.0 / (1.0 + std::pow(10.0, pH - pKa_effective));

	return fraction;
}

bool DissolutionEngine::step_protonation(
	ProtonationState& site,
	double pH,
	double T_K
) {
	double prob = protonation_probability(site, pH, T_K);

	// Stochastic protonation (simplified - in practice use RNG)
	// Here we use a threshold model
	if (prob > 0.5 && site.proton_count < 2) {
		site.proton_count++;

		// Update site type
		if (site.proton_count == 1) {
			site.type = SurfaceSiteType::HYDROXYL;
		} else if (site.proton_count == 2) {
			site.type = SurfaceSiteType::AQUA_LEAVING;
		}

		// Increase lability
		site.lability = 0.1 + 0.4 * site.proton_count;

		stats_.total_protonation_events++;
		return true;
	}

	return false;
}

// ============================================================================
// Lattice Cleavage Implementation
// ============================================================================

double DissolutionEngine::cleavage_probability(
	const ProtonationState& site,
	double T_K
) {
	// Only AQUA_LEAVING sites can cleave easily
	if (site.type != SurfaceSiteType::AQUA_LEAVING) {
		// Bridging and terminal oxides have high barriers
		double Ea = (site.type == SurfaceSiteType::BRIDGING_OXIDE)
			? config_.Ea_bridging_cleavage
			: config_.Ea_terminal_release;
		return arrhenius_rate(Ea, T_K) * 0.01;  // Much slower
	}

	// M-OH2+ has low barrier - H2O is a good leaving group
	double Ea = config_.Ea_terminal_release * 0.5;  // Reduced barrier
	double rate = arrhenius_rate(Ea, T_K);

	// Lability factor
	rate *= site.lability;

	return std::min(rate, 1.0);
}

DissolutionEvent DissolutionEngine::execute_cleavage(
	State& solid,
	const ProtonationState& site,
	uint64_t frame_id
) {
	DissolutionEvent event;
	event.frame_id = frame_id;
	event.metal_index = site.site_index;  // Will be updated to metal index
	event.metal_element = site.metal_element;
	event.oxidation_state = site.metal_oxidation;
	event.position = site.position;

	// Energetics
	event.release_energy = (site.type == SurfaceSiteType::AQUA_LEAVING)
		? config_.Ea_terminal_release * 0.5
		: config_.Ea_bridging_cleavage;

	event.hydration_energy = hydration_energy(
		site.metal_element, site.metal_oxidation
	);
	event.net_energy = event.release_energy + event.hydration_energy;

	// Coordination changes
	event.coordination_before = 4;  // Typical for oxide
	event.coordination_after = preferred_coordination(
		site.metal_element, site.metal_oxidation
	);

	// Bond pattern strings
	event.bond_pattern_before = bond_pattern_string(site);
	event.bond_pattern_after = "[" + site.metal_element + "(H2O)" +
		std::to_string(event.coordination_after) + "^" +
		std::to_string(site.metal_oxidation) + "+]^{aquo}_{aq}";

	// Update statistics
	update_stats(event);
	dissolution_events_.push_back(event);

	return event;
}

// ============================================================================
// Hydration Shell Implementation
// ============================================================================

double DissolutionEngine::hydration_energy(
	const std::string& metal_element,
	int oxidation_state
) {
	// Hydration enthalpies (kcal/mol, negative = favorable)
	if (metal_element == "Fe") {
		return (oxidation_state == 3) ? config_.dG_hydration_Fe3
									  : config_.dG_hydration_Fe2;
	} else if (metal_element == "Al") {
		return config_.dG_hydration_Al3;
	}

	// Generic estimate based on charge density
	// ΔH_hyd ∝ z²/r
	double z2 = oxidation_state * oxidation_state;
	return config_.dG_hydration_generic * (z2 / 4.0);
}

int DissolutionEngine::preferred_coordination(
	const std::string& metal_element,
	int oxidation_state
) {
	// Most 3d transition metals form octahedral aquo complexes
	if (metal_element == "Fe" || metal_element == "Co" ||
		metal_element == "Ni" || metal_element == "Cr" ||
		metal_element == "Mn" || metal_element == "Al") {
		return 6;  // Octahedral
	}

	// Some form tetrahedral
	if (metal_element == "Zn" || metal_element == "Cu") {
		return (oxidation_state == 1) ? 4 : 6;
	}

	// Default to octahedral
	return 6;
}

// ============================================================================
// Ligand Exchange Implementation
// ============================================================================

double DissolutionEngine::ligand_exchange_probability(
	const State& metal_state,
	const std::string& ligand,
	double concentration,
	double T_K
) {
	// Rate = k * [ligand] * exp(-Ea/RT)
	double k_base = 1e6;  // s^-1 for water exchange (labile)

	// Modify rate based on ligand identity
	double k_factor = 1.0;
	if (ligand == "SO4^2-" || ligand == "SO4") {
		k_factor = 0.1;  // Slower - charged ligand
	} else if (ligand == "Cl-" || ligand == "Cl") {
		k_factor = 0.5;
	} else if (ligand == "OH-" || ligand == "OH") {
		k_factor = 0.8;  // Fast - good nucleophile
	}

	double rate = k_base * k_factor * concentration * arrhenius_rate(5.0, T_K);

	return std::min(rate * 1e-12, 1.0);  // Convert to per-fs probability
}

LigandExchangeEvent DissolutionEngine::execute_ligand_exchange(
	State& metal_state,
	const std::string& ligand,
	const std::string& binding_mode,
	uint64_t frame_id
) {
	LigandExchangeEvent event;
	event.frame_id = frame_id;
	event.leaving_ligand = "H2O";
	event.entering_ligand = ligand;
	event.binding_mode = binding_mode;

	// Energy based on binding mode
	if (binding_mode == "monodentate") {
		event.exchange_energy = config_.dG_sulfate_mono;
		event.stability_constant = 2.0;
	} else if (binding_mode == "bidentate") {
		event.exchange_energy = config_.dG_sulfate_bi;
		event.stability_constant = 4.0;
	} else if (binding_mode == "bridging") {
		event.exchange_energy = config_.dG_sulfate_bridge;
		event.stability_constant = 5.5;
	} else {
		event.exchange_energy = -2.0;
		event.stability_constant = 1.0;
	}

	update_stats(event);
	ligand_events_.push_back(event);

	return event;
}

// ============================================================================
// Full Pathway Simulation
// ============================================================================

int DissolutionEngine::step_dissolution(
	State& solid,
	State& solution,
	double pH,
	double T_K,
	double dt_fs,
	uint64_t frame_id
) {
	int events_this_step = 0;

	// 1. Identify/update surface sites
	if (surface_sites_.empty() || frame_id % 100 == 0) {
		identify_surface_sites(solid);
	}

	// 2. Process each surface site
	for (auto& site : surface_sites_) {
		// Skip vacancies
		if (site.type == SurfaceSiteType::VACANCY) continue;

		// 2a. Try protonation
		step_protonation(site, pH, T_K);

		// 2b. Check for cleavage
		if (site.type == SurfaceSiteType::AQUA_LEAVING) {
			double prob = cleavage_probability(site, T_K);
			if (prob > 0.5) {  // Simplified threshold
				execute_cleavage(solid, site, frame_id);
				site.type = SurfaceSiteType::VACANCY;
				events_this_step++;
			}
		}
	}

	return events_this_step;
}

// ============================================================================
// Statistics Implementation
// ============================================================================

void DissolutionEngine::update_stats(const DissolutionEvent& event) {
	stats_.total_dissolution_events++;
	stats_.total_release_energy += event.release_energy;
	stats_.total_hydration_energy += event.hydration_energy;
	stats_.net_dissolution_energy += event.net_energy;
	stats_.metal_releases[event.metal_element]++;
}

void DissolutionEngine::update_stats(const LigandExchangeEvent& event) {
	stats_.total_ligand_exchanges++;
	stats_.total_ligand_exchange_energy += event.exchange_energy;
	stats_.ligand_bindings[event.entering_ligand]++;
}

std::string DissolutionEngine::pathway_summary() const {
	std::ostringstream ss;
	ss << std::fixed << std::setprecision(2);

	ss << "=== Dissolution Pathway Summary ===\n";
	ss << "Protonation events:    " << stats_.total_protonation_events << "\n";
	ss << "Dissolution events:    " << stats_.total_dissolution_events << "\n";
	ss << "Ligand exchanges:      " << stats_.total_ligand_exchanges << "\n";
	ss << "\n";

	ss << "Metal releases:\n";
	for (const auto& [elem, count] : stats_.metal_releases) {
		ss << "  " << elem << ": " << count << "\n";
	}

	ss << "\nLigand bindings:\n";
	for (const auto& [lig, count] : stats_.ligand_bindings) {
		ss << "  " << lig << ": " << count << "\n";
	}

	ss << "\nEnergetics (kcal/mol):\n";
	ss << "  Release energy:   " << stats_.total_release_energy << "\n";
	ss << "  Hydration energy: " << stats_.total_hydration_energy << "\n";
	ss << "  Net dissolution:  " << stats_.net_dissolution_energy << "\n";

	return ss.str();
}

} // namespace reaction
} // namespace atomistic
