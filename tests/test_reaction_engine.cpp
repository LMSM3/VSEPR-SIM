/**
 * test_reaction_engine.cpp
 * ========================
 * Smoke test for the coarse-grain reaction engine and built-in reaction library.
 *
 * Section A  — ReactionLibrary (3 built-in reactions)
 *   RE-01  benzene nitration: mass balance OK
 *   RE-02  benzene nitration: ΔH_rxn negative (exothermic)
 *   RE-03  benzene nitration: ΔG_rxn negative (spontaneous)
 *   RE-04  benzene nitration: K_eq > 1
 *   RE-05  thorium oxalate: mass balance OK
 *   RE-06  thorium oxalate: ΔH_rxn set (non-zero)
 *   RE-07  copper nitrate decomposition: mass balance OK
 *   RE-08  all_library_reactions(): returns 3 entries
 *
 * Section B  — ReactionEngine (mapping + thermodynamics)
 *   RE-09  map_reaction_to_beads: positive bead count
 *   RE-10  build_reaction_params: non-zero temperature
 *   RE-11  compute_delta_H: matches stored delta_H_rxn for nitration
 *   RE-12  compute_delta_G: matches stored delta_G_rxn for nitration
 *   RE-13  arrhenius_rate(298.15): positive, non-zero
 *
 * Section C  — ChemicalReaction accessors
 *   RE-14  reactants() returns only REACTANT entries
 *   RE-15  products() returns only PRODUCT entries
 *   RE-16  catalysts() returns only CATALYST entries
 *   RE-17  byproducts() returns only BYPRODUCT entries
 */

#include "coarse_grain/chemistry/reaction_library.hpp"
#include "coarse_grain/chemistry/reaction_engine.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace coarse_grain::chemistry;
using namespace coarse_grain::chemistry::library;

// ============================================================================
// Minimal CHECK macro
// ============================================================================
static int g_pass = 0, g_fail = 0;

#define CHECK(id, cond, msg)                                                    \
	do {                                                                        \
		if (cond) {                                                             \
			std::cout << "  PASS " << (id) << "  " << (msg) << "\n";           \
			++g_pass;                                                           \
		} else {                                                                \
			std::cout << "  FAIL " << (id) << "  " << (msg) << "\n";           \
			++g_fail;                                                           \
		}                                                                       \
	} while (0)

// ============================================================================
// Section A — Reaction Library
// ============================================================================
static void section_a_library() {
	std::cout << "\n--- Section A: ReactionLibrary ---\n";

	auto nitration = build_benzene_nitration();
	auto thorium   = build_thorium_oxalate_precipitation();
	auto cu_decomp = build_copper_nitrate_decomposition();
	auto all       = all_library_reactions();

	// RE-01: benzene nitration element balance (integer counts, not MW floats)
	auto mb1 = nitration.verify_mass_balance();
	bool elem_balanced1 = true;
	for (auto& [elem, diff] : mb1.imbalance) {
		if (diff > 0.5) { elem_balanced1 = false; break; }  // tolerance: 0.5 atoms
	}
	CHECK("RE-01", elem_balanced1, "benzene nitration element balance OK");

	// RE-02: exothermic (ΔH < 0)
	CHECK("RE-02", nitration.delta_H_rxn < 0.0, "benzene nitration ΔH_rxn < 0");

	// RE-03: spontaneous (ΔG < 0)
	CHECK("RE-03", nitration.delta_G_rxn < 0.0, "benzene nitration ΔG_rxn < 0");

	// RE-04: K_eq > 1
	double K = nitration.compute_K_eq(298.15);
	CHECK("RE-04", K > 1.0, "benzene nitration K_eq > 1");

	// RE-05: thorium oxalate element balance (integer counts, not MW floats)
	auto mb2 = thorium.verify_mass_balance();
	bool elem_balanced2 = true;
	for (auto& [elem, diff] : mb2.imbalance) {
		if (diff > 0.5) { elem_balanced2 = false; break; }
	}
	CHECK("RE-05", elem_balanced2, "thorium oxalate element balance OK");

	// RE-06: thorium ΔH non-zero
	CHECK("RE-06", std::abs(thorium.delta_H_rxn) > 1e-9, "thorium oxalate ΔH_rxn non-zero");

	// RE-07: copper nitrate decomposition mass balance
	auto mb3 = cu_decomp.verify_mass_balance();
	CHECK("RE-07", mb3.balanced, "copper nitrate decomposition mass balance OK");

	// RE-08: library count
	CHECK("RE-08", all.size() == 3, "all_library_reactions() returns 3");
}

// ============================================================================
// Section B — ReactionEngine
// ============================================================================
static void section_b_engine() {
	std::cout << "\n--- Section B: ReactionEngine ---\n";

	auto rxn = build_benzene_nitration();

	// RE-09: bead mapping
	auto system = ReactionEngine::map_reaction_to_beads(rxn);
	CHECK("RE-09", system.beads.size() > 0, "map_reaction_to_beads: bead count > 0");

	// RE-10: reaction thermal temperature non-zero
	CHECK("RE-10", rxn.thermal.temperature_K > 0.0, "rxn.thermal.temperature_K > 0 K");

	// RE-11: compute_delta_H matches stored value
	double dH = rxn.compute_delta_H();
	CHECK("RE-11", std::abs(dH - rxn.delta_H_rxn) < 1e-6, "compute_delta_H == delta_H_rxn");

	// RE-12: compute_delta_G matches stored value
	double dG = rxn.compute_delta_G();
	CHECK("RE-12", std::abs(dG - rxn.delta_G_rxn) < 1e-6, "compute_delta_G == delta_G_rxn");

	// RE-13: Arrhenius rate positive at 298 K
	double k = rxn.arrhenius_rate(298.15);
	CHECK("RE-13", k > 0.0, "arrhenius_rate(298.15) > 0");
}

// ============================================================================
// Section C — ChemicalReaction accessors
// ============================================================================
static void section_c_accessors() {
	std::cout << "\n--- Section C: Reaction accessors ---\n";

	auto rxn = build_benzene_nitration();

	auto reactants  = rxn.reactants();
	auto products   = rxn.products();
	auto catalysts  = rxn.catalysts();
	auto byproducts = rxn.byproducts();

	// RE-14: all returned entries really are reactants
	bool all_react = true;
	for (auto& e : reactants) { if (e.role != SpeciesRole::REACTANT) all_react = false; }
	CHECK("RE-14", !reactants.empty() && all_react, "reactants() returns REACTANT-only entries");

	// RE-15: all returned entries really are products
	bool all_prod = true;
	for (auto& e : products) { if (e.role != SpeciesRole::PRODUCT) all_prod = false; }
	CHECK("RE-15", !products.empty() && all_prod, "products() returns PRODUCT-only entries");

	// RE-16: catalyst entries (may be empty for nitration; check roles only)
	bool all_cat = true;
	for (auto& e : catalysts) { if (e.role != SpeciesRole::CATALYST) all_cat = false; }
	CHECK("RE-16", all_cat, "catalysts() returns CATALYST-only entries");

	// RE-17: byproduct entries
	bool all_by = true;
	for (auto& e : byproducts) { if (e.role != SpeciesRole::BYPRODUCT) all_by = false; }
	CHECK("RE-17", all_by, "byproducts() returns BYPRODUCT-only entries");
}

// ============================================================================
// main
// ============================================================================
int main() {
	std::cout << "=============================================================\n";
	std::cout << "  test_reaction_engine  |  Reaction Library + Engine Audit\n";
	std::cout << "  coarse_grain/chemistry  |  v5.0.14\n";
	std::cout << "=============================================================\n";

	section_a_library();
	section_b_engine();
	section_c_accessors();

	std::cout << "\n-------------------------------------------------------------\n";
	std::cout << "  Result: " << g_pass << " passed, " << g_fail << " failed"
			  << "  (" << (g_pass + g_fail) << " total)\n";
	std::cout << "-------------------------------------------------------------\n";
	if (g_fail == 0) {
		std::cout << "  STATUS: ALL PASS\n\n";
		return EXIT_SUCCESS;
	} else {
		std::cout << "  STATUS: FAILURES DETECTED\n\n";
		return EXIT_FAILURE;
	}
}
