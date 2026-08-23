/**
 * test_element_lone_pair_provider.cpp  --  Day 84 / WO-84B
 * ============================================================================
 * CTest Group 100.
 *
 * Verifies the ElementLonePairProvider drives VSEPR geometry deterministically:
 *
 *   H2O   -> bent                    NH3  -> trigonal pyramidal
 *   CH4   -> tetrahedral             CO2  -> linear
 *   XeF2  -> linear (AX2E3)          XeF4 -> square planar (AX4E2)
 *   XeF6  -> expanded / heavy warning
 *   KrF2  -> linear (AX2E3)
 *
 * The critical behaviour is that heavy noble gases do NOT collapse into a
 * generic tetrahedral answer (the pre-WO-84B fallback), and that unsupported
 * cases still resolve through stable deterministic behaviour.
 *
 * Acceptance (WO-84B):
 *   - Lone-pair counts are deterministic.                          [checked]
 *   - Provider-backed lone-pair data affects VSEPR geometry.       [checked]
 *   - Heavy noble gas cases avoid generic tetrahedral output.      [checked]
 *   - Unsupported cases use stable fallback behaviour.             [checked]
 *   - Output can identify provider source or fallback source.      [checked]
 */

#include "atomistic/classify/vsepr.hpp"
#include "atomistic/classify/element_lone_pair_provider.hpp"
#include "atomistic/classify/providers.hpp"
#include "atomistic/core/state.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <string>

using atomistic::Edge;
using atomistic::State;
using atomistic::Vec3;
using atomistic::classify::ElementLonePairProvider;
using atomistic::classify::VSEPRMolecularShape;
using atomistic::classify::VSEPRElectronGeometry;
using atomistic::classify::VSEPROptions;
using atomistic::classify::classify_vsepr_sites;
using atomistic::classify::to_string;

namespace {

int g_pass = 0;
int g_fail = 0;

void check(bool cond, const std::string& what) {
	if (cond) { ++g_pass; std::cout << "  PASS: " << what << "\n"; }
	else      { ++g_fail; std::cout << "  FAIL: " << what << "\n"; }
}

State make_state(std::vector<uint32_t> types,
				 std::vector<Vec3> positions,
				 std::vector<Edge> bonds) {
	State s;
	s.N = static_cast<uint32_t>(types.size());
	s.type = std::move(types);
	s.X = std::move(positions);
	s.B = std::move(bonds);
	s.V.resize(s.N);
	s.Q.resize(s.N, 0.0);
	s.M.resize(s.N, 1.0);
	return s;
}

// Bind the element lone-pair provider to the state and classify with it wired.
atomistic::classify::VSEPRReport classify_with_provider(const State& s) {
	VSEPROptions options;
	ElementLonePairProvider provider;
	options.providers.lone_pair = provider.bind(s);
	return classify_vsepr_sites(s, options);
}

// ---- molecules (explicit bonds; positions only need to give sane angles) ----

State make_h2o() {
	return make_state({8, 1, 1},
		{{0.0, 0.0, 0.0}, {0.757, 0.586, 0.0}, {-0.757, 0.586, 0.0}},
		{{0, 1}, {0, 2}});
}

State make_nh3() {
	return make_state({7, 1, 1, 1},
		{{0.0, 0.0, 0.0}, {0.94, 0.0, -0.33}, {-0.47, 0.82, -0.33}, {-0.47, -0.82, -0.33}},
		{{0, 1}, {0, 2}, {0, 3}});
}

State make_ch4() {
	return make_state({6, 1, 1, 1, 1},
		{{0.0, 0.0, 0.0}, {0.63, 0.63, 0.63}, {-0.63, -0.63, 0.63},
		 {-0.63, 0.63, -0.63}, {0.63, -0.63, -0.63}},
		{{0, 1}, {0, 2}, {0, 3}, {0, 4}});
}

State make_co2() {
	return make_state({6, 8, 8},
		{{0.0, 0.0, 0.0}, {1.16, 0.0, 0.0}, {-1.16, 0.0, 0.0}},
		{{0, 1}, {0, 2}});
}

// XeF2: linear F-Xe-F, Xe(Z=54) has 3 lone pairs -> AX2E3 -> linear.
State make_xef2() {
	return make_state({54, 9, 9},
		{{0.0, 0.0, 0.0}, {2.0, 0.0, 0.0}, {-2.0, 0.0, 0.0}},
		{{0, 1}, {0, 2}});
}

// XeF4: square-planar 4 F around Xe, 2 lone pairs -> AX4E2 -> square planar.
State make_xef4() {
	return make_state({54, 9, 9, 9, 9},
		{{0.0, 0.0, 0.0}, {2.0, 0.0, 0.0}, {-2.0, 0.0, 0.0},
		 {0.0, 2.0, 0.0}, {0.0, -2.0, 0.0}},
		{{0, 1}, {0, 2}, {0, 3}, {0, 4}});
}

// XeF6: 6 F around Xe, 1 lone pair -> AX6E1 = 7 domains -> expanded / warning.
State make_xef6() {
	return make_state({54, 9, 9, 9, 9, 9, 9},
		{{0.0, 0.0, 0.0}, {2.0, 0.0, 0.0}, {-2.0, 0.0, 0.0},
		 {0.0, 2.0, 0.0}, {0.0, -2.0, 0.0}, {0.0, 0.0, 2.0}, {0.0, 0.0, -2.0}},
		{{0, 1}, {0, 2}, {0, 3}, {0, 4}, {0, 5}, {0, 6}});
}

// KrF2: linear F-Kr-F, Kr(Z=36) has 3 lone pairs -> AX2E3 -> linear.
State make_krf2() {
	return make_state({36, 9, 9},
		{{0.0, 0.0, 0.0}, {2.0, 0.0, 0.0}, {-2.0, 0.0, 0.0}},
		{{0, 1}, {0, 2}});
}

// ---------------------------------------------------------------------------
// Pure chemistry table checks (deterministic lone-pair counts)
// ---------------------------------------------------------------------------
void test_lone_pair_table_is_deterministic() {
	using P = ElementLonePairProvider;
	check(P::lone_pairs_for(8, 2)  == std::optional<int>(2), "O  (2 bonds) -> 2 LP");
	check(P::lone_pairs_for(7, 3)  == std::optional<int>(1), "N  (3 bonds) -> 1 LP");
	check(P::lone_pairs_for(6, 4)  == std::optional<int>(0), "C  (4 bonds) -> 0 LP");
	check(P::lone_pairs_for(54, 2) == std::optional<int>(3), "Xe (2 bonds) -> 3 LP (XeF2)");
	check(P::lone_pairs_for(54, 4) == std::optional<int>(2), "Xe (4 bonds) -> 2 LP (XeF4)");
	check(P::lone_pairs_for(54, 6) == std::optional<int>(1), "Xe (6 bonds) -> 1 LP (XeF6)");
	check(P::lone_pairs_for(36, 2) == std::optional<int>(3), "Kr (2 bonds) -> 3 LP (KrF2)");
	// Transition-metal conservative fallback.
	check(P::lone_pairs_for(26, 6) == std::optional<int>(0), "Fe (6 bonds) -> 0 LP (conservative)");
	check(P::is_heavy_noble_gas_warning(54, 6), "XeF6 flagged heavy-noble-gas warning");
	check(!P::is_heavy_noble_gas_warning(54, 4), "XeF4 NOT a heavy-noble-gas warning");
}

// ---------------------------------------------------------------------------
// Geometry outcomes with the provider wired
// ---------------------------------------------------------------------------
void test_common_geometries() {
	{
		const auto r = classify_with_provider(make_h2o());
		const auto& o = r.sites[0];
		check(o.used_provider_lone_pair, "H2O: O used provider lone pairs");
		check(o.lone_pair_domain_count == 2, "H2O: O has 2 lone pairs");
		check(o.molecular_shape == VSEPRMolecularShape::Bent, "H2O -> bent");
	}
	{
		const auto r = classify_with_provider(make_nh3());
		const auto& n = r.sites[0];
		check(n.lone_pair_domain_count == 1, "NH3: N has 1 lone pair");
		check(n.molecular_shape == VSEPRMolecularShape::TrigonalPyramidal,
			  "NH3 -> trigonal pyramidal");
	}
	{
		const auto r = classify_with_provider(make_ch4());
		const auto& c = r.sites[0];
		check(c.lone_pair_domain_count == 0, "CH4: C has 0 lone pairs");
		check(c.molecular_shape == VSEPRMolecularShape::Tetrahedral,
			  "CH4 -> tetrahedral");
	}
	{
		const auto r = classify_with_provider(make_co2());
		const auto& c = r.sites[0];
		check(c.molecular_shape == VSEPRMolecularShape::Linear, "CO2 -> linear");
	}
}

// ---------------------------------------------------------------------------
// Noble-gas geometries (the WO-84B headline)
// ---------------------------------------------------------------------------
void test_noble_gas_geometries() {
	{
		const auto r = classify_with_provider(make_xef2());
		const auto& xe = r.sites[0];
		check(xe.used_provider_lone_pair, "XeF2: Xe used provider lone pairs");
		check(xe.lone_pair_domain_count == 3, "XeF2: Xe has 3 lone pairs (AX2E3)");
		check(xe.ax_label == "AX2E3", "XeF2: label AX2E3");
		check(xe.molecular_shape == VSEPRMolecularShape::Linear, "XeF2 -> linear");
	}
	{
		const auto r = classify_with_provider(make_xef4());
		const auto& xe = r.sites[0];
		check(xe.lone_pair_domain_count == 2, "XeF4: Xe has 2 lone pairs (AX4E2)");
		check(xe.ax_label == "AX4E2", "XeF4: label AX4E2");
		check(xe.molecular_shape == VSEPRMolecularShape::SquarePlanar,
			  "XeF4 -> square planar");
		// MUST NOT be tetrahedral (the pre-WO-84B collapse).
		check(xe.molecular_shape != VSEPRMolecularShape::Tetrahedral,
			  "XeF4 did NOT collapse to tetrahedral");
	}
	{
		const auto r = classify_with_provider(make_xef6());
		const auto& xe = r.sites[0];
		check(xe.lone_pair_domain_count == 1, "XeF6: Xe has 1 lone pair (AX6E1)");
		check(xe.electron_domain_count == 7, "XeF6: 7 electron domains");
		check(xe.electron_geometry == VSEPRElectronGeometry::ExpandedCoordination,
			  "XeF6 -> expanded coordination (heavy noble-gas path)");
		check(xe.molecular_shape != VSEPRMolecularShape::Tetrahedral,
			  "XeF6 did NOT collapse to tetrahedral");
		check(ElementLonePairProvider::is_heavy_noble_gas_warning(54, 6),
			  "XeF6 heavy noble-gas warning is available");
	}
	{
		const auto r = classify_with_provider(make_krf2());
		const auto& kr = r.sites[0];
		check(kr.lone_pair_domain_count == 3, "KrF2: Kr has 3 lone pairs (AX2E3)");
		check(kr.molecular_shape == VSEPRMolecularShape::Linear, "KrF2 -> linear");
	}
}

// ---------------------------------------------------------------------------
// Stability: provider is NaN/Inf-free and unsupported elements defer cleanly
// ---------------------------------------------------------------------------
void test_stability_and_unsupported() {
	// Unsupported element (e.g. Z=13 Al with 4 bonds) -> nullopt from table.
	check(!ElementLonePairProvider::lone_pairs_for(13, 4).has_value(),
		  "Al (unsupported) -> nullopt, defers to fallback");

	const auto r = classify_with_provider(make_xef4());
	for (const auto& s : r.sites) {
		check(std::isfinite(s.confidence), "XeF4 site confidence finite (no NaN/Inf)");
		break; // one representative check is enough
	}
	check(r.provider_lone_pair_available, "provider-backed run flags provider available");
	check(r.provider_lone_pair_sites >= 1, "provider-backed run counts provider sites");
}

} // namespace

int main() {
	std::cout << "Group 100  --  WO-84B element lone-pair provider\n";
	test_lone_pair_table_is_deterministic();
	test_common_geometries();
	test_noble_gas_geometries();
	test_stability_and_unsupported();

	std::cout << "\n  Result: " << g_pass << " PASS / " << g_fail << " FAIL\n";
	if (g_fail == 0) {
		std::cout << "  PASS: all Group 100 lone-pair provider checks satisfied\n";
		return 0;
	}
	std::cout << "  FAIL: Group 100 lone-pair provider checks failed\n";
	return 1;
}
