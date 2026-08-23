/**
 * test_classify_safe_defaults.cpp  --  WO-83U safe-default / edge-case tests
 *
 * Proves that:
 *   - Empty State returns empty/safe reports
 *   - Single-atom State doesn't crash
 *   - Degenerate positions (all atoms at origin) don't divide by zero
 *   - Missing charge data gives safe polarity defaults
 *   - Missing connectivity produces reasonable organic output
 */

#include "atomistic/classify/vsepr.hpp"
#include "atomistic/classify/organic_candidate.hpp"
#include "atomistic/core/state.hpp"

#include <cassert>
#include <cstdio>
#include <cstring>

using namespace atomistic;
using namespace atomistic::classify;

static void test_empty_state_vsepr() {
	atomistic::State s;
	s.N = 0;
	const auto report = classify_vsepr_sites(s);
	assert(report.sites.empty());
	assert(report.linear_count == 0);
	assert(report.tetrahedral_count == 0);
	const std::string text = format_vsepr_report(report);
	assert(text.find("empty") != std::string::npos);
	std::puts("  empty state VSEPR: OK");
}

static void test_empty_state_organic() {
	atomistic::State s;
	s.N = 0;
	OrganicClassifier oc;
	const auto cand = oc.classify(s);
	assert(cand.sp3_count == 0);
	assert(cand.strain_score == 0.0);
	assert(cand.lipid_like_score == 0.0);
	assert(cand.decomposition_risk == 0.0);
	std::puts("  empty state organic: OK");
}

static void test_single_atom_state() {
	atomistic::State s;
	s.N = 1;
	s.X = {{0.0, 0.0, 0.0}};
	s.V.assign(1, {0,0,0});
	s.Q.assign(1, 0.0);
	s.M.assign(1, 12.0);
	s.type = {6u};  // C
	s.F.assign(1, {0,0,0});
	// no bonds

	const auto vreport = classify_vsepr_sites(s);
	assert(!vreport.sites.empty()); // 1 site for the one atom
	assert(vreport.sites[0].bonded_domain_count == 0);

	OrganicClassifier oc;
	const auto cand = oc.classify(s);
	// Should not crash; formula has at least "C"
	assert(!cand.formula.empty());
	std::puts("  single atom state: OK");
}

static void test_degenerate_positions() {
	// All atoms at (0,0,0) - all pairwise distances are zero
	// Should not divide by zero in angle calculation
	atomistic::State s;
	s.N = 3;
	s.X = {{0.0,0.0,0.0},{0.0,0.0,0.0},{0.0,0.0,0.0}};
	s.V.assign(3, {0,0,0});
	s.Q.assign(3, 0.0);
	s.M.assign(3, 1.0);
	s.type = {6u, 1u, 1u};
	s.F.assign(3, {0,0,0});
	s.B = {{0,1},{0,2}};

	// Must not crash or throw
	const auto vreport = classify_vsepr_sites(s);
	OrganicClassifier oc;
	const auto cand = oc.classify(s);

	assert(vreport.sites.size() == 3);
	assert(cand.strain_score >= 0.0 && cand.strain_score <= 1.0);
	std::puts("  degenerate positions: OK");
}

static void test_no_charge_data_polarity() {
	// Q vector empty or zero — polarity should be 0, no crash
	atomistic::State s;
	s.N = 3;
	s.X = {{0.0,0.0,0.0},{1.0,0.0,0.0},{-1.0,0.0,0.0}};
	s.V.assign(3, {0,0,0});
	s.Q.assign(3, 0.0);  // all zero charges
	s.M.assign(3, 1.0);
	s.type = {6u, 8u, 8u};
	s.F.assign(3, {0,0,0});
	s.B = {{0,1},{0,2}};

	OrganicClassifier oc;
	const auto cand = oc.classify(s);
	// With zero charges polarity should be 0
	assert(cand.polarity_score == 0.0);
	std::puts("  no charge data polarity: OK");
}

static void test_no_connectivity_organic() {
	// No bonds — still should produce a formula and not crash
	atomistic::State s;
	s.N = 4;
	s.X = {{0,0,0},{1,0,0},{0,1,0},{0,0,1}};
	s.V.assign(4, {0,0,0});
	s.Q.assign(4, 0.0);
	s.M.assign(4, 1.0);
	s.type = {6u, 1u, 1u, 1u};
	s.F.assign(4, {0,0,0});
	// B is empty — no connectivity

	OrganicClassifier oc;
	const auto cand = oc.classify(s);
	assert(!cand.formula.empty());
	assert(cand.rotatable_bond_count == 0); // no bonds = no rotatable bonds
	std::puts("  no connectivity organic: OK");
}

int main() {
	test_empty_state_vsepr();
	test_empty_state_organic();
	test_single_atom_state();
	test_degenerate_positions();
	test_no_charge_data_polarity();
	test_no_connectivity_organic();

	std::puts("test_classify_safe_defaults: PASS");
	return 0;
}
