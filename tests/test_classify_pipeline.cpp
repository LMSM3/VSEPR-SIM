/**
 * test_classify_pipeline.cpp  --  WO-83R full classify pipeline smoke test
 *
 * Runs: fingerprints -> VSEPR -> OrganicCandidate -> formatters
 * on a small methane-like CH4 State.
 * Confirms no module circular ownership is visible and output contains
 * topology, local geometry, and organic descriptors.
 */

#include "atomistic/classify/fingerprints.hpp"
#include "atomistic/classify/vsepr.hpp"
#include "atomistic/classify/organic_candidate.hpp"
#include "atomistic/core/state.hpp"

#include <cassert>
#include <cstdio>
#include <string>

using namespace atomistic;
using namespace atomistic::classify;

// Build a tetrahedral CH4-like state (5 atoms: C center + 4 H at corners)
static atomistic::State make_ch4() {
	atomistic::State s;
	s.N = 5;
	s.X = {
		{0.0,  0.0,  0.0},   // C
		{0.89, 0.89, 0.89},  // H
		{-0.89,-0.89, 0.89}, // H
		{-0.89, 0.89,-0.89}, // H
		{0.89,-0.89,-0.89},  // H
	};
	s.V.assign(5, {0,0,0});
	s.Q.assign(5, 0.0);
	s.M.assign(5, 1.0);
	s.type = {6u, 1u, 1u, 1u, 1u};  // C, H, H, H, H
	s.F.assign(5, {0,0,0});
	s.B = {{0,1},{0,2},{0,3},{0,4}};
	return s;
}

static void test_fingerprint_builds() {
	auto s = make_ch4();
	auto ng = build_neighbor_graph(s, 2.0);
	assert(ng.N == 5);
	assert(ng.CN[0] == 4); // C has 4 neighbors
	auto fp = compute_proto_fingerprint(s, ng);
	assert(fp.topology_hash != 0);
	std::puts("  fingerprint: OK");
}

static void test_vsepr_pipeline() {
	auto s = make_ch4();
	const auto report = classify_vsepr_sites(s);
	assert(!report.sites.empty());
	// Carbon center should be tetrahedral-like
	assert(report.tetrahedral_count >= 1);
	const std::string text = format_vsepr_report(report);
	assert(text.find("tetrahedral") != std::string::npos);
	std::puts("  vsepr pipeline: OK");
}

static void test_organic_pipeline() {
	auto s = make_ch4();
	OrganicClassifier oc;
	const auto cand = oc.classify(s);
	assert(cand.sp3_count >= 1);
	assert(cand.formula.find("C") != std::string::npos);
	const std::string text = format_organic_candidate(cand);
	assert(text.find("organic_candidate") != std::string::npos);
	assert(text.find("sp3_count") != std::string::npos);
	assert(text.find("strain_score") != std::string::npos);
	std::puts("  organic pipeline: OK");
}

static void test_hybridization_hint_sp3() {
	auto s = make_ch4();
	const auto report = classify_vsepr_sites(s);
	// Find the carbon center site (bonded_domain_count == 4)
	bool found_sp3 = false;
	for (const auto& site : report.sites) {
		if (site.bonded_domain_count == 4) {
			HybridizationHint hint = hybridization_hint(site);
			assert(hint == HybridizationHint::SP3);
			found_sp3 = true;
			break;
		}
	}
	assert(found_sp3);
	std::puts("  hybridization sp3 hint: OK");
}

static void test_full_pipeline_summary() {
	auto s = make_ch4();

	// 1. Fingerprints
	auto ng = build_neighbor_graph(s, 2.0);
	auto fp = compute_proto_fingerprint(s, ng);
	assert(fp.topology_hash != 0);

	// 2. VSEPR
	const auto vreport = classify_vsepr_sites(s);
	assert(vreport.tetrahedral_count >= 1);

	// 3. Organic
	OrganicClassifier oc;
	const auto cand = oc.classify(s);
	assert(cand.sp3_count >= 1);
	assert(cand.strain_score >= 0.0 && cand.strain_score <= 1.0);
	assert(cand.lipid_like_score >= 0.0 && cand.lipid_like_score <= 1.0);

	// 4. Formatters
	const std::string vtext = format_vsepr_report(vreport);
	const std::string otext = format_organic_candidate(cand);
	assert(!vtext.empty());
	assert(!otext.empty());

	std::puts("  full pipeline summary: OK");
}

int main() {
	test_fingerprint_builds();
	test_vsepr_pipeline();
	test_organic_pipeline();
	test_hybridization_hint_sp3();
	test_full_pipeline_summary();

	std::puts("test_classify_pipeline: PASS");
	return 0;
}
