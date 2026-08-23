/**
 * test_provider_wiring.cpp  --  Day 84 / WO-84A: ProviderSet wiring
 * ============================================================================
 * CTest Group 99.
 *
 * Verifies that ProviderSet is actually consumed by the analysis path:
 *   - VSEPROptions carries a ProviderSet field (defaults to null()).
 *   - classify_vsepr_sites() uses an available lone-pair provider and reports
 *     provider vs fallback provenance.
 *   - A missing provider falls back to deterministic element/geometry inference
 *     (identical behaviour to before WO-84A).
 *   - An available provider OVERRIDES the element fallback.
 *   - OrganicClassifier accepts a ProviderSet and threads it into VSEPR,
 *     surfacing provenance on the OrganicCandidate.
 *   - Missing providers never crash; outputs stay finite (no NaN/Inf).
 *
 * Acceptance (WO-84A):
 *   - ProviderSet is visible from VSEPROptions.                       [checked]
 *   - OrganicClassifier can receive provider-backed options.          [checked]
 *   - Missing providers do not crash.                                 [checked]
 *   - Available providers override fallback logic.                    [checked]
 *   - CLI output reports provider/fallback source where practical.    [cmd_classify]
 */

#include "atomistic/classify/vsepr.hpp"
#include "atomistic/classify/organic_candidate.hpp"
#include "atomistic/classify/providers.hpp"
#include "atomistic/core/state.hpp"

#include <cassert>
#include <cmath>
#include <iostream>

using atomistic::Edge;
using atomistic::State;
using atomistic::Vec3;
using atomistic::classify::LonePairProvider;
using atomistic::classify::OrganicClassifier;
using atomistic::classify::ProviderSet;
using atomistic::classify::VSEPROptions;
using atomistic::classify::classify_vsepr_sites;

namespace {

static int g_pass = 0;
static int g_fail = 0;

static void check(bool cond, const char* what) {
	if (cond) {
		++g_pass;
		std::cout << "  PASS: " << what << "\n";
	} else {
		++g_fail;
		std::cout << "  FAIL: " << what << "\n";
	}
}

static State make_state(
	std::vector<uint32_t> types,
	std::vector<Vec3> positions,
	std::vector<Edge> bonds
) {
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

// Water with a LINEAR H-O-H connectivity so geometry alone cannot resolve the
// bend; the lone-pair source (element inference or provider) decides the shape.
static State make_h2o_linear() {
	return make_state(
		{8, 1, 1},
		{{0.0, 0.0, 0.0}, {0.96, 0.0, 0.0}, {-0.96, 0.0, 0.0}},
		{{0, 1}, {0, 2}}
	);
}

static bool all_finite(const atomistic::classify::VSEPRReport& r) {
	for (const auto& s : r.sites) {
		if (!std::isfinite(s.confidence)) return false;
		if (!std::isfinite(s.angle_stats.rms_deviation_deg)) return false;
	}
	return true;
}

// ---------------------------------------------------------------------------
// 1. ProviderSet is visible from VSEPROptions and defaults to null()
// ---------------------------------------------------------------------------
static void test_options_has_null_provider_by_default() {
	VSEPROptions options;
	check(!options.providers.lone_pair.available(),
		  "default VSEPROptions.providers.lone_pair is null");

	const auto report = classify_vsepr_sites(make_h2o_linear(), options);
	check(!report.provider_lone_pair_available,
		  "no-provider run reports provider_lone_pair_available == false");
	check(report.provider_lone_pair_sites == 0,
		  "no-provider run has zero provider sites");
	check(report.fallback_lone_pair_sites == report.sites.size(),
		  "no-provider run: every site used a fallback source");
}

// ---------------------------------------------------------------------------
// 2. Missing provider -> deterministic element fallback (unchanged behaviour)
// ---------------------------------------------------------------------------
static void test_missing_provider_uses_element_fallback() {
	const auto report = classify_vsepr_sites(make_h2o_linear());  // default opts
	const auto& o = report.sites[0];
	check(o.used_element_lone_pair_inference,
		  "missing provider: O uses element lone-pair inference");
	check(!o.used_provider_lone_pair,
		  "missing provider: O did NOT use provider path");
	check(o.lone_pair_domain_count == 2,
		  "missing provider: O infers 2 lone pairs (bent water)");
	check(o.ax_label == "AX2E2",
		  "missing provider: O is AX2E2");
}

// ---------------------------------------------------------------------------
// 3. Available provider OVERRIDES the element fallback
// ---------------------------------------------------------------------------
static void test_available_provider_overrides_fallback() {
	VSEPROptions options;
	// Provider claims oxygen (atom 0) has 3 lone pairs (deliberately different
	// from the element heuristic's 2) so we can prove the override took effect.
	options.providers.lone_pair.fn =
		[](std::size_t i) -> std::optional<int> {
			return (i == 0) ? std::optional<int>(3) : std::optional<int>(0);
		};

	const auto report = classify_vsepr_sites(make_h2o_linear(), options);
	const auto& o = report.sites[0];

	check(report.provider_lone_pair_available,
		  "available provider: report flags provider available");
	check(o.used_provider_lone_pair,
		  "available provider: O used provider lone-pair path");
	check(!o.used_element_lone_pair_inference,
		  "available provider: element inference was NOT used for O");
	check(o.lone_pair_domain_count == 3,
		  "available provider: O lone-pair count overridden to 3");
	check(report.provider_lone_pair_sites >= 1,
		  "available provider: at least one provider-sourced site counted");
}

// ---------------------------------------------------------------------------
// 4. Provider returning nullopt for an atom falls back deterministically
// ---------------------------------------------------------------------------
static void test_partial_provider_falls_back_per_atom() {
	VSEPROptions options;
	// Provider only answers for H atoms; oxygen gets nullopt -> element fallback.
	options.providers.lone_pair.fn =
		[](std::size_t i) -> std::optional<int> {
			if (i == 0) return std::nullopt;  // oxygen: no provider answer
			return 0;                          // hydrogens: 0 lone pairs
		};

	const auto report = classify_vsepr_sites(make_h2o_linear(), options);
	const auto& o = report.sites[0];
	check(!o.used_provider_lone_pair,
		  "partial provider: O (nullopt) did not use provider path");
	check(o.used_element_lone_pair_inference,
		  "partial provider: O fell back to element inference");
	check(o.lone_pair_domain_count == 2,
		  "partial provider: O element fallback still yields 2 lone pairs");
}

// ---------------------------------------------------------------------------
// 5. OrganicClassifier accepts a ProviderSet and threads it into VSEPR
// ---------------------------------------------------------------------------
static void test_organic_classifier_consumes_providers() {
	// Default (null) classifier: fallback provenance only.
	OrganicClassifier oc_null;
	const auto cand_null = oc_null.classify(make_h2o_linear());
	check(!cand_null.provider_set_available,
		  "null OrganicClassifier: provider_set_available == false");
	check(cand_null.used_fallback_lone_pair,
		  "null OrganicClassifier: used_fallback_lone_pair == true");
	check(!cand_null.used_provider_lone_pair,
		  "null OrganicClassifier: used_provider_lone_pair == false");

	// Provider-backed classifier.
	ProviderSet ps;
	ps.lone_pair.fn = [](std::size_t i) -> std::optional<int> {
		return (i == 0) ? std::optional<int>(2) : std::optional<int>(0);
	};
	OrganicClassifier oc_prov(ps);
	const auto cand_prov = oc_prov.classify(make_h2o_linear());
	check(cand_prov.provider_set_available,
		  "provider OrganicClassifier: provider_set_available == true");
	check(cand_prov.used_provider_lone_pair,
		  "provider OrganicClassifier: used_provider_lone_pair == true");

	// set_providers() path must also work.
	OrganicClassifier oc_set;
	oc_set.set_providers(ps);
	const auto cand_set = oc_set.classify(make_h2o_linear());
	check(cand_set.used_provider_lone_pair,
		  "set_providers() OrganicClassifier: used_provider_lone_pair == true");
}

// ---------------------------------------------------------------------------
// 6. Missing providers never crash; outputs are finite (no NaN/Inf)
// ---------------------------------------------------------------------------
static void test_no_crash_and_finite_outputs() {
	const auto report = classify_vsepr_sites(make_h2o_linear());
	check(all_finite(report), "no-provider report is NaN/Inf-free");

	VSEPROptions options;
	options.providers.lone_pair.fn =
		[](std::size_t) -> std::optional<int> { return -1; }; // sentinel unknown
	const auto report2 = classify_vsepr_sites(make_h2o_linear(), options);
	check(all_finite(report2), "sentinel(-1) provider report is NaN/Inf-free");
	// -1 means "unknown" -> must NOT be treated as a provider answer.
	check(report2.sites[0].used_element_lone_pair_inference,
		  "sentinel(-1) provider: O fell back to element inference");
}

} // namespace

int main() {
	std::cout << "Group 99  --  WO-84A provider wiring\n";
	test_options_has_null_provider_by_default();
	test_missing_provider_uses_element_fallback();
	test_available_provider_overrides_fallback();
	test_partial_provider_falls_back_per_atom();
	test_organic_classifier_consumes_providers();
	test_no_crash_and_finite_outputs();

	std::cout << "\n  Result: " << g_pass << " PASS / " << g_fail << " FAIL\n";
	if (g_fail == 0) {
		std::cout << "  PASS: all Group 99 provider-wiring checks satisfied\n";
		return 0;
	}
	std::cout << "  FAIL: Group 99 provider-wiring checks failed\n";
	return 1;
}
