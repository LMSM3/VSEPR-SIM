/**
 * test_ring_provider.cpp  --  Day 84 / WO-84C
 * ============================================================================
 * CTest Group 101.
 *
 * Verifies State-based ring detection + RingProvider wiring:
 *
 *   benzene       -> 1 ring (size 6)
 *   cyclohexane   -> 1 ring (size 6)
 *   cyclopropane  -> 1 ring (size 3, strained)
 *   naphthalene   -> 2 rings (fused)
 *   linear hexane -> 0 rings
 *
 * Also verifies:
 *   - Ring bonds are excluded from rotatable-bond counts.
 *   - RingProvider reports per-atom ring sizes and total ring count.
 *   - OrganicClassifier::count_rings() uses the detector (and a wired provider).
 *
 * Acceptance (WO-84C):
 *   - Ring count is reported.                                     [checked]
 *   - Simple ring systems are detected.                           [checked]
 *   - Linear molecules remain non-ring candidates.                [checked]
 *   - Ring bonds are excluded from rotatable bond counts.         [checked]
 *   - Fused systems are detected or explicitly flagged.           [checked]
 */

#include "atomistic/classify/ring_detector.hpp"
#include "atomistic/classify/ring_provider.hpp"
#include "atomistic/classify/organic_candidate.hpp"
#include "atomistic/classify/providers.hpp"
#include "atomistic/core/state.hpp"

#include <iostream>
#include <string>
#include <vector>

using atomistic::Edge;
using atomistic::State;
using atomistic::classify::detect_ring_system;
using atomistic::classify::OrganicClassifier;
using atomistic::classify::ProviderSet;
using atomistic::classify::RingProviderBuilder;
using atomistic::classify::RingSystem;

namespace {

int g_pass = 0;
int g_fail = 0;

void check(bool cond, const std::string& what) {
	if (cond) { ++g_pass; std::cout << "  PASS: " << what << "\n"; }
	else      { ++g_fail; std::cout << "  FAIL: " << what << "\n"; }
}

// Build a State from carbon skeleton edges only (positions irrelevant to the
// graph-based detector).  `n_carbons` heavy atoms, all carbon (Z=6).
State make_carbon_skeleton(std::size_t n_carbons, std::vector<Edge> bonds) {
	State s;
	s.N = static_cast<uint32_t>(n_carbons);
	s.type.assign(n_carbons, 6u);
	s.X.assign(n_carbons, {0.0, 0.0, 0.0});
	s.V.resize(n_carbons);
	s.Q.assign(n_carbons, 0.0);
	s.M.assign(n_carbons, 12.0);
	s.B = std::move(bonds);
	return s;
}

// 6-membered carbon ring: 0-1-2-3-4-5-0
State make_benzene_ring() {
	return make_carbon_skeleton(6,
		{{0,1},{1,2},{2,3},{3,4},{4,5},{5,0}});
}

// Same topology as benzene ring for detection purposes (cyclohexane).
State make_cyclohexane_ring() {
	return make_carbon_skeleton(6,
		{{0,1},{1,2},{2,3},{3,4},{4,5},{5,0}});
}

// 3-membered carbon ring: 0-1-2-0
State make_cyclopropane_ring() {
	return make_carbon_skeleton(3, {{0,1},{1,2},{2,0}});
}

// Naphthalene skeleton: two fused 6-rings sharing edge 0-1.
//   Ring A: 0-1-2-3-4-5-0   Ring B: 0-1-6-7-8-9-0  (shared bond 0-1)
State make_naphthalene() {
	return make_carbon_skeleton(10, {
		{0,1},
		{1,2},{2,3},{3,4},{4,5},{5,0},          // ring A
		{1,6},{6,7},{7,8},{8,9},{9,0}           // ring B (closes via 9-0)
	});
}

// Linear hexane skeleton: 0-1-2-3-4-5 (no ring).
State make_linear_hexane() {
	return make_carbon_skeleton(6, {{0,1},{1,2},{2,3},{3,4},{4,5}});
}

void test_detector_counts() {
	{
		const RingSystem r = detect_ring_system(make_benzene_ring());
		check(r.total_rings() == 1, "benzene: 1 ring detected");
		check(r.circuit_rank == 1, "benzene: circuit rank 1");
		check(!r.rings.empty() && r.rings[0].size() == 6, "benzene: ring size 6");
		check(!r.has_fused_system(), "benzene: not fused");
	}
	{
		const RingSystem r = detect_ring_system(make_cyclohexane_ring());
		check(r.total_rings() == 1, "cyclohexane: 1 ring detected");
		check(!r.rings.empty() && r.rings[0].size() == 6, "cyclohexane: ring size 6");
	}
	{
		const RingSystem r = detect_ring_system(make_cyclopropane_ring());
		check(r.total_rings() == 1, "cyclopropane: 1 ring detected");
		check(!r.rings.empty() && r.rings[0].size() == 3, "cyclopropane: ring size 3");
		check(r.strained_rings() == 1, "cyclopropane: strained ring flagged");
	}
	{
		const RingSystem r = detect_ring_system(make_naphthalene());
		check(r.total_rings() == 2, "naphthalene: 2 rings detected");
		check(r.circuit_rank == 2, "naphthalene: circuit rank 2");
		check(r.has_fused_system(), "naphthalene: fused system flagged");
		check(r.fused_ring_pairs == 1, "naphthalene: exactly 1 fused pair");
	}
	{
		const RingSystem r = detect_ring_system(make_linear_hexane());
		check(r.total_rings() == 0, "linear hexane: 0 rings");
		check(r.circuit_rank == 0, "linear hexane: circuit rank 0");
		check(r.ring_bonds.empty(), "linear hexane: no ring bonds");
	}
}

void test_ring_bond_exclusion() {
	// Benzene ring: all 6 C-C bonds are ring bonds, so ZERO rotatable bonds.
	const State benzene = make_benzene_ring();
	OrganicClassifier clf;
	const int rot = clf.count_rotatable_bonds(benzene);
	check(rot == 0, "benzene: ring bonds excluded -> 0 rotatable bonds");

	// Linear hexane: internal single bonds are rotatable (ring exclusion is a
	// no-op because there are no ring bonds).  Bonds 1-2,2-3,3-4 qualify.
	const State hexane = make_linear_hexane();
	const int rot_hex = clf.count_rotatable_bonds(hexane);
	check(rot_hex >= 1, "linear hexane: interior bonds remain rotatable");
}

void test_ring_provider_and_classifier() {
	// RingProvider exposes per-atom ring sizes and a total count.
	RingSystem system;
	auto provider = RingProviderBuilder{}.build(make_naphthalene(), system);
	check(provider.available(), "naphthalene: ring provider available");
	check(provider.total_rings().value_or(-1) == 2, "naphthalene: provider total_rings == 2");
	// Shared atoms 0 and 1 belong to two rings each.
	check(provider.ring_sizes(0).size() == 2, "naphthalene: fusion atom 0 in 2 rings");
	check(provider.ring_sizes(1).size() == 2, "naphthalene: fusion atom 1 in 2 rings");
	check(provider.in_ring(3), "naphthalene: atom 3 is a ring atom");

	// OrganicClassifier::count_rings uses the wired provider.
	ProviderSet ps = ProviderSet::null();
	RingSystem sys2;
	ps.ring = RingProviderBuilder{}.build(make_benzene_ring(), sys2);
	OrganicClassifier clf(ps);
	const auto info = clf.count_rings(make_benzene_ring());
	check(info.total == 1, "provider-wired classifier: benzene ring count == 1");

	// And the default classifier (no provider) still detects rings via the
	// internal detector fallback.
	OrganicClassifier plain;
	const auto info2 = plain.count_rings(make_cyclopropane_ring());
	check(info2.total == 1, "fallback classifier: cyclopropane ring count == 1");
	check(info2.strained >= 1, "fallback classifier: cyclopropane strained flagged");
	const auto info3 = plain.count_rings(make_linear_hexane());
	check(info3.total == 0, "fallback classifier: linear hexane ring count == 0");
}

} // namespace

int main() {
	std::cout << "Group 101  --  WO-84C ring provider + ring detection\n";
	test_detector_counts();
	test_ring_bond_exclusion();
	test_ring_provider_and_classifier();

	std::cout << "\n  Result: " << g_pass << " PASS / " << g_fail << " FAIL\n";
	if (g_fail == 0) {
		std::cout << "  PASS: all Group 101 ring-detection checks satisfied\n";
		return 0;
	}
	std::cout << "  FAIL: Group 101 ring-detection checks failed\n";
	return 1;
}
