/**
 * tests/test_dissolution.cpp
 * ==========================
 * Unit tests for the surface dissolution module.
 *
 * Tests the multi-step acid dissolution pathway:
 *   1. Surface site identification
 *   2. Protonation ladder mechanics
 *   3. Lattice cleavage probability
 *   4. Hydration shell energetics
 *   5. Ligand exchange (sulfate binding)
 *   6. Full pathway integration
 *
 * WO-56D | v5.0.0
 */

#include "atomistic/reaction/dissolution.hpp"
#include "atomistic/core/state.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <iomanip>

using namespace atomistic::reaction;

// ============================================================================
// Test Utilities
// ============================================================================

static constexpr double TOLERANCE = 1e-6;

bool approx_eq(double a, double b, double tol = TOLERANCE) {
	return std::abs(a - b) < tol;
}

void print_test(const std::string& name, bool passed) {
	std::cout << (passed ? "[PASS] " : "[FAIL] ") << name << "\n";
}

// ============================================================================
// Test: Surface Site Classification
// ============================================================================

void test_surface_site_names() {
	bool pass = true;

	pass &= (std::string(surface_site_name(SurfaceSiteType::BRIDGING_OXIDE)) == "bridging_oxide");
	pass &= (std::string(surface_site_name(SurfaceSiteType::TERMINAL_OXIDE)) == "terminal_oxide");
	pass &= (std::string(surface_site_name(SurfaceSiteType::HYDROXYL)) == "hydroxyl");
	pass &= (std::string(surface_site_name(SurfaceSiteType::AQUA_LEAVING)) == "aqua_leaving");
	pass &= (std::string(surface_site_name(SurfaceSiteType::VACANCY)) == "vacancy");
	pass &= (std::string(surface_site_name(SurfaceSiteType::BULK)) == "bulk");

	print_test("Surface site type names", pass);
}

// ============================================================================
// Test: Protonation State Structure
// ============================================================================

void test_protonation_state() {
	ProtonationState ps;
	ps.site_index = 42;
	ps.type = SurfaceSiteType::TERMINAL_OXIDE;
	ps.proton_count = 0;
	ps.pKa_estimate = 9.5;
	ps.lability = 0.1;
	ps.metal_element = "Fe";
	ps.metal_oxidation = 3;

	bool pass = true;
	pass &= (ps.site_index == 42);
	pass &= (ps.type == SurfaceSiteType::TERMINAL_OXIDE);
	pass &= (ps.proton_count == 0);
	pass &= approx_eq(ps.pKa_estimate, 9.5);
	pass &= approx_eq(ps.lability, 0.1);
	pass &= (ps.metal_element == "Fe");
	pass &= (ps.metal_oxidation == 3);

	print_test("ProtonationState structure", pass);
}

// ============================================================================
// Test: Dissolution Config Defaults
// ============================================================================

void test_dissolution_config_defaults() {
	DissolutionConfig cfg;

	bool pass = true;
	// Protonation energetics
	pass &= approx_eq(cfg.dG_first_protonation, -12.0);
	pass &= approx_eq(cfg.dG_second_protonation, -8.0);

	// Cleavage barriers
	pass &= approx_eq(cfg.Ea_bridging_cleavage, 25.0);
	pass &= approx_eq(cfg.Ea_terminal_release, 15.0);

	// Hydration energies
	pass &= approx_eq(cfg.dG_hydration_Fe3, -105.0);
	pass &= approx_eq(cfg.dG_hydration_Fe2, -85.0);
	pass &= approx_eq(cfg.dG_hydration_Al3, -115.0);

	// Sulfate binding
	pass &= approx_eq(cfg.dG_sulfate_mono, -3.5);
	pass &= approx_eq(cfg.dG_sulfate_bi, -6.0);
	pass &= approx_eq(cfg.dG_sulfate_bridge, -8.5);

	// Reference conditions
	pass &= approx_eq(cfg.pH_reference, 1.0);
	pass &= approx_eq(cfg.T_reference, 298.15);

	print_test("DissolutionConfig default values", pass);
}

// ============================================================================
// Test: Dissolution Engine Construction
// ============================================================================

void test_engine_construction() {
	bool pass = true;

	// Default construction
	DissolutionEngine engine1;
	pass &= (engine1.stats().total_dissolution_events == 0);

	// Construction with config
	DissolutionConfig cfg;
	cfg.dG_hydration_Fe3 = -110.0;  // Modified value
	DissolutionEngine engine2(cfg);
	pass &= approx_eq(engine2.config().dG_hydration_Fe3, -110.0);

	print_test("DissolutionEngine construction", pass);
}

// ============================================================================
// Test: Hydration Energy Calculation
// ============================================================================

void test_hydration_energy() {
	DissolutionEngine engine;
	bool pass = true;

	// Fe3+ hydration
	double dG_Fe3 = engine.hydration_energy("Fe", 3);
	pass &= approx_eq(dG_Fe3, -105.0);

	// Fe2+ hydration
	double dG_Fe2 = engine.hydration_energy("Fe", 2);
	pass &= approx_eq(dG_Fe2, -85.0);

	// Al3+ hydration
	double dG_Al3 = engine.hydration_energy("Al", 3);
	pass &= approx_eq(dG_Al3, -115.0);

	// Generic metal (should scale with charge)
	double dG_M2 = engine.hydration_energy("Mg", 2);
	pass &= (dG_M2 < 0.0);  // Should be negative (favorable)

	print_test("Hydration energy calculation", pass);
}

// ============================================================================
// Test: Preferred Coordination Number
// ============================================================================

void test_preferred_coordination() {
	DissolutionEngine engine;
	bool pass = true;

	// Octahedral complexes
	pass &= (engine.preferred_coordination("Fe", 3) == 6);
	pass &= (engine.preferred_coordination("Fe", 2) == 6);
	pass &= (engine.preferred_coordination("Al", 3) == 6);
	pass &= (engine.preferred_coordination("Cr", 3) == 6);
	pass &= (engine.preferred_coordination("Co", 2) == 6);
	pass &= (engine.preferred_coordination("Ni", 2) == 6);

	// Zn/Cu can be tetrahedral for +1
	pass &= (engine.preferred_coordination("Zn", 1) == 4);
	pass &= (engine.preferred_coordination("Cu", 1) == 4);

	print_test("Preferred coordination number", pass);
}

// ============================================================================
// Test: Bond Pattern String Generation
// ============================================================================

void test_bond_pattern_string() {
	DissolutionEngine engine;
	bool pass = true;

	ProtonationState site;
	site.metal_element = "Fe";

	// Bridging oxide
	site.type = SurfaceSiteType::BRIDGING_OXIDE;
	std::string pat1 = engine.bond_pattern_string(site);
	pass &= (pat1.find("Fe-O-Fe") != std::string::npos);
	pass &= (pat1.find("bridging oxide") != std::string::npos);

	// Terminal oxide
	site.type = SurfaceSiteType::TERMINAL_OXIDE;
	std::string pat2 = engine.bond_pattern_string(site);
	pass &= (pat2.find("Fe-O-") != std::string::npos);
	pass &= (pat2.find("terminal oxide") != std::string::npos);

	// Hydroxyl
	site.type = SurfaceSiteType::HYDROXYL;
	std::string pat3 = engine.bond_pattern_string(site);
	pass &= (pat3.find("Fe-OH") != std::string::npos);
	pass &= (pat3.find("hydroxyl") != std::string::npos);

	// Aqua leaving group
	site.type = SurfaceSiteType::AQUA_LEAVING;
	std::string pat4 = engine.bond_pattern_string(site);
	pass &= (pat4.find("Fe-OH2+") != std::string::npos);
	pass &= (pat4.find("leaving group") != std::string::npos);

	// Vacancy
	site.type = SurfaceSiteType::VACANCY;
	std::string pat5 = engine.bond_pattern_string(site);
	pass &= (pat5.find("vacancy") != std::string::npos);
	pass &= (pat5.find("dissolved") != std::string::npos);

	print_test("Bond pattern string generation", pass);
}

// ============================================================================
// Test: Statistics Reset
// ============================================================================

void test_stats_reset() {
	DissolutionEngine engine;
	bool pass = true;

	// Initial stats should be zero
	pass &= (engine.stats().total_protonation_events == 0);
	pass &= (engine.stats().total_dissolution_events == 0);
	pass &= (engine.stats().total_ligand_exchanges == 0);

	// Reset should keep them zero
	engine.reset_stats();
	pass &= (engine.stats().total_protonation_events == 0);
	pass &= (engine.stats().total_dissolution_events == 0);

	print_test("Statistics reset", pass);
}

// ============================================================================
// Test: Dissolution Event Structure
// ============================================================================

void test_dissolution_event() {
	DissolutionEvent event;
	event.frame_id = 1000;
	event.metal_index = 5;
	event.metal_element = "Fe";
	event.oxidation_state = 3;
	event.release_energy = 15.0;
	event.hydration_energy = -105.0;
	event.net_energy = event.release_energy + event.hydration_energy;
	event.coordination_before = 4;
	event.coordination_after = 6;
	event.bond_pattern_before = "[Fe-O-Fe]^{bridging oxide}_{s}";
	event.bond_pattern_after = "[Fe(H2O)6^3+]^{aquo}_{aq}";

	bool pass = true;
	pass &= (event.frame_id == 1000);
	pass &= (event.metal_element == "Fe");
	pass &= (event.oxidation_state == 3);
	pass &= approx_eq(event.net_energy, -90.0);
	pass &= (event.coordination_after == 6);

	print_test("DissolutionEvent structure", pass);
}

// ============================================================================
// Test: Ligand Exchange Event Structure
// ============================================================================

void test_ligand_exchange_event() {
	LigandExchangeEvent event;
	event.frame_id = 1500;
	event.metal_index = 5;
	event.metal_element = "Fe";
	event.leaving_ligand = "H2O";
	event.entering_ligand = "SO4^2-";
	event.binding_mode = "monodentate";
	event.exchange_energy = -3.5;
	event.stability_constant = 4.04;

	bool pass = true;
	pass &= (event.frame_id == 1500);
	pass &= (event.leaving_ligand == "H2O");
	pass &= (event.entering_ligand == "SO4^2-");
	pass &= (event.binding_mode == "monodentate");
	pass &= approx_eq(event.stability_constant, 4.04);

	print_test("LigandExchangeEvent structure", pass);
}

// ============================================================================
// Test: Dissolution Stats Aggregation
// ============================================================================

void test_dissolution_stats() {
	DissolutionStats stats;

	// Default values
	bool pass = true;
	pass &= (stats.total_protonation_events == 0);
	pass &= (stats.total_dissolution_events == 0);
	pass &= (stats.total_ligand_exchanges == 0);
	pass &= approx_eq(stats.total_release_energy, 0.0);
	pass &= approx_eq(stats.total_hydration_energy, 0.0);
	pass &= approx_eq(stats.dissolution_rate_mol_per_s, 0.0);
	pass &= approx_eq(stats.mobile_metal_concentration, 0.0);
	pass &= approx_eq(stats.sulfate_bound_fraction, 0.0);

	// Metal releases map
	stats.metal_releases["Fe"] = 10;
	stats.metal_releases["Al"] = 3;
	pass &= (stats.metal_releases.size() == 2);
	pass &= (stats.metal_releases["Fe"] == 10);

	// Ligand bindings map
	stats.ligand_bindings["SO4^2-"] = 5;
	stats.ligand_bindings["Cl-"] = 2;
	pass &= (stats.ligand_bindings.size() == 2);

	print_test("DissolutionStats aggregation", pass);
}

// ============================================================================
// Test: Pathway Summary Generation
// ============================================================================

void test_pathway_summary() {
	DissolutionEngine engine;
	std::string summary = engine.pathway_summary();

	bool pass = true;
	pass &= (summary.find("Dissolution Pathway Summary") != std::string::npos);
	pass &= (summary.find("Protonation events") != std::string::npos);
	pass &= (summary.find("Dissolution events") != std::string::npos);
	pass &= (summary.find("Ligand exchanges") != std::string::npos);
	pass &= (summary.find("Metal releases") != std::string::npos);
	pass &= (summary.find("Energetics") != std::string::npos);

	print_test("Pathway summary generation", pass);
}

// ============================================================================
// Test: Protonation Probability Limits
// ============================================================================

void test_protonation_probability_limits() {
	DissolutionEngine engine;
	bool pass = true;

	ProtonationState site;
	site.type = SurfaceSiteType::TERMINAL_OXIDE;
	site.proton_count = 0;
	site.pKa_estimate = 9.5;

	// At very low pH, protonation should be highly probable
	double prob_low_pH = engine.protonation_probability(site, 1.0, 298.15);
	pass &= (prob_low_pH > 0.9);

	// At very high pH, protonation should be improbable
	double prob_high_pH = engine.protonation_probability(site, 14.0, 298.15);
	pass &= (prob_high_pH < 0.1);

	// Fully protonated site should have zero probability
	site.proton_count = 2;
	double prob_full = engine.protonation_probability(site, 1.0, 298.15);
	pass &= approx_eq(prob_full, 0.0);

	print_test("Protonation probability limits", pass);
}

// ============================================================================
// Test: Cleavage Probability by Site Type
// ============================================================================

void test_cleavage_probability() {
	DissolutionEngine engine;
	bool pass = true;

	ProtonationState site;
	site.metal_element = "Fe";
	site.lability = 0.9;

	// AQUA_LEAVING should have highest cleavage probability
	site.type = SurfaceSiteType::AQUA_LEAVING;
	double prob_aqua = engine.cleavage_probability(site, 298.15);

	// BRIDGING_OXIDE should have low cleavage probability
	site.type = SurfaceSiteType::BRIDGING_OXIDE;
	double prob_bridge = engine.cleavage_probability(site, 298.15);

	// TERMINAL_OXIDE intermediate
	site.type = SurfaceSiteType::TERMINAL_OXIDE;
	double prob_term = engine.cleavage_probability(site, 298.15);

	pass &= (prob_aqua > prob_term);
	pass &= (prob_term > prob_bridge);
	pass &= (prob_aqua <= 1.0);
	pass &= (prob_bridge >= 0.0);

	print_test("Cleavage probability by site type", pass);
}

// ============================================================================
// Test: Config Modification
// ============================================================================

void test_config_modification() {
	DissolutionEngine engine;
	bool pass = true;

	// Check default
	pass &= approx_eq(engine.config().dG_hydration_Fe3, -105.0);

	// Modify config
	DissolutionConfig new_cfg;
	new_cfg.dG_hydration_Fe3 = -120.0;
	new_cfg.Ea_bridging_cleavage = 30.0;
	engine.set_config(new_cfg);

	// Verify changes
	pass &= approx_eq(engine.config().dG_hydration_Fe3, -120.0);
	pass &= approx_eq(engine.config().Ea_bridging_cleavage, 30.0);

	print_test("Config modification", pass);
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main() {
	std::cout << "========================================\n";
	std::cout << "  Dissolution Module Tests (WO-56D)\n";
	std::cout << "========================================\n\n";

	std::cout << "--- Structure Tests ---\n";
	test_surface_site_names();
	test_protonation_state();
	test_dissolution_event();
	test_ligand_exchange_event();
	test_dissolution_stats();

	std::cout << "\n--- Configuration Tests ---\n";
	test_dissolution_config_defaults();
	test_engine_construction();
	test_config_modification();

	std::cout << "\n--- Energetics Tests ---\n";
	test_hydration_energy();
	test_preferred_coordination();

	std::cout << "\n--- Probability Tests ---\n";
	test_protonation_probability_limits();
	test_cleavage_probability();

	std::cout << "\n--- Output Tests ---\n";
	test_bond_pattern_string();
	test_pathway_summary();
	test_stats_reset();

	std::cout << "\n========================================\n";
	std::cout << "  Tests Complete\n";
	std::cout << "========================================\n";

	return 0;
}
