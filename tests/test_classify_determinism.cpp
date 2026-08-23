/**
 * test_classify_determinism.cpp  --  WO-83T determinism regression
 *
 * Proves that classify output is identical across repeated calls on
 * the same input state.  Site ordering, report counts, and formatted
 * output must all be stable.
 */

#include "atomistic/classify/vsepr.hpp"
#include "atomistic/classify/organic_candidate.hpp"
#include "atomistic/core/state.hpp"

#include <cassert>
#include <cstdio>

using namespace atomistic;
using namespace atomistic::classify;

static atomistic::State make_nh3() {
	atomistic::State s;
	s.N = 4;
	// N center, 3 H at slightly distorted tetrahedral positions
	s.X = {
		{0.000,  0.000,  0.000},
		{0.940,  0.000,  0.000},
		{-0.470, 0.814,  0.000},
		{-0.470,-0.814,  0.000},
	};
	s.V.assign(4, {0,0,0});
	s.Q.assign(4, 0.0);
	s.M.assign(4, 1.0);
	s.type = {7u, 1u, 1u, 1u};
	s.F.assign(4, {0,0,0});
	s.B = {{0,1},{0,2},{0,3}};
	return s;
}

static atomistic::State make_co2() {
	atomistic::State s;
	s.N = 3;
	s.X = {
		{0.000, 0.0, 0.0},  // C
		{1.160, 0.0, 0.0},  // O
		{-1.160, 0.0, 0.0}, // O
	};
	s.V.assign(3, {0,0,0});
	s.Q.assign(3, 0.0);
	s.M.assign(3, 1.0);
	s.type = {6u, 8u, 8u};
	s.F.assign(3, {0,0,0});
	s.B = {{0,1},{0,2}};
	return s;
}

static void test_vsepr_same_each_call(const atomistic::State& s, const char* label) {
	const auto r1 = classify_vsepr_sites(s);
	const auto r2 = classify_vsepr_sites(s);
	const auto r3 = classify_vsepr_sites(s);

	assert(r1.sites.size()          == r2.sites.size());
	assert(r2.sites.size()          == r3.sites.size());
	assert(r1.linear_count          == r2.linear_count);
	assert(r1.tetrahedral_count     == r2.tetrahedral_count);
	assert(r1.planar_count          == r2.planar_count);
	assert(r1.pyramidal_count       == r2.pyramidal_count);

	// Formatted output must be byte-identical
	const std::string t1 = format_vsepr_report(r1);
	const std::string t2 = format_vsepr_report(r2);
	const std::string t3 = format_vsepr_report(r3);
	assert(t1 == t2);
	assert(t2 == t3);

	for (std::size_t i = 0; i < r1.sites.size(); ++i) {
		assert(r1.sites[i].center_index   == r2.sites[i].center_index);
		assert(r1.sites[i].ax_label       == r2.sites[i].ax_label);
		assert(r1.sites[i].molecular_shape == r2.sites[i].molecular_shape);
	}

	std::printf("  vsepr determinism [%s]: OK\n", label);
}

static void test_organic_same_each_call(const atomistic::State& s, const char* label) {
	OrganicClassifier oc;
	const auto c1 = oc.classify(s);
	const auto c2 = oc.classify(s);
	const auto c3 = oc.classify(s);

	assert(c1.formula          == c2.formula);
	assert(c2.formula          == c3.formula);
	assert(c1.sp3_count        == c2.sp3_count);
	assert(c1.sp2_count        == c2.sp2_count);
	assert(c1.sp_count         == c2.sp_count);
	assert(c1.strain_score     == c2.strain_score);
	assert(c1.lipid_like_score == c2.lipid_like_score);
	assert(c1.primary_family   == c2.primary_family);
	assert(c1.family_source    == c2.family_source);

	const std::string t1 = format_organic_candidate(c1);
	const std::string t2 = format_organic_candidate(c2);
	assert(t1 == t2);

	std::printf("  organic determinism [%s]: OK\n", label);
}

int main() {
	auto nh3 = make_nh3();
	auto co2 = make_co2();

	test_vsepr_same_each_call(nh3, "NH3");
	test_vsepr_same_each_call(co2, "CO2");
	test_organic_same_each_call(nh3, "NH3");
	test_organic_same_each_call(co2, "CO2");

	std::puts("test_classify_determinism: PASS");
	return 0;
}
