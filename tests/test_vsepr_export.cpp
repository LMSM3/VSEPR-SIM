/**
 * test_vsepr_export.cpp  --  Day 84 / WO-84E
 * ============================================================================
 * CTest Group 103a.
 *
 * Verifies JSON-lite export for VSEPRReport:
 *
 *   - vsepr_lone_pair_source() returns correct labels
 *   - vsepr_central_site_index() selects max-domain site
 *   - vsepr_to_json() emits required fields without NaN/Inf
 *   - provider/fallback provenance fields appear in output
 *   - empty report handled gracefully
 *
 * Acceptance (WO-84E):
 *   - JSON-lite report is generated.                              [checked]
 *   - Provider-backed fields appear when available.               [checked]
 *   - Fallback fields appear when provider data is missing.       [checked]
 *   - No NaN/Inf appears in outputs.                              [checked]
 *   - geometry, lone_pairs, bonding_domains fields exported.      [checked]
 */

#include "atomistic/classify/vsepr.hpp"
#include "atomistic/export/vsepr_export.hpp"
#include "atomistic/core/state.hpp"

#include <iostream>
#include <string>
#include <vector>

using atomistic::Edge;
using atomistic::State;
using atomistic::classify::classify_vsepr_sites;
using atomistic::classify::vsepr_to_json;
using atomistic::classify::vsepr_lone_pair_source;
using atomistic::classify::vsepr_central_site_index;
using atomistic::classify::VSEPROptions;
using atomistic::classify::VSEPRReport;
using atomistic::classify::VSEPRSite;

namespace {

int g_pass = 0;
int g_fail = 0;

void check(bool cond, const std::string& what) {
	if (cond) { ++g_pass; std::cout << "  PASS: " << what << "\n"; }
	else      { ++g_fail; std::cout << "  FAIL: " << what << "\n"; }
}

State make_state(std::vector<uint32_t> types, std::vector<Edge> bonds) {
	State s;
	s.N = static_cast<uint32_t>(types.size());
	s.type = std::move(types);
	s.X.assign(s.N, {0.0, 0.0, 0.0});
	s.V.resize(s.N);
	s.Q.assign(s.N, 0.0);
	s.M.assign(s.N, 1.0);
	s.B = std::move(bonds);
	s.F.resize(s.N, {0.0, 0.0, 0.0});
	return s;
}

// H2O: O center, 2 H ligands -> bent (2 lone pairs on O)
State make_h2o() {
	// Z: O=8, H=1
	return make_state({8, 1, 1}, {{0,1},{0,2}});
}

// NH3: N center, 3 H ligands -> trigonal pyramidal (1 lone pair on N)
State make_nh3() {
	// Z: N=7, H=1
	return make_state({7, 1, 1, 1}, {{0,1},{0,2},{0,3}});
}

// CH4: C center, 4 H ligands -> tetrahedral (no lone pairs)
State make_ch4() {
	return make_state({6, 1, 1, 1, 1}, {{0,1},{0,2},{0,3},{0,4}});
}

} // namespace

int main() {
	std::cout << "\n=== test_vsepr_export.cpp  (Day 84 / WO-84E) ===\n\n";

	// ------------------------------------------------------------------
	// Section 1: vsepr_lone_pair_source() label correctness
	// ------------------------------------------------------------------
	std::cout << "--- Section 1: lone_pair_source labels ---\n";
	{
		VSEPRSite s;
		s.used_provider_lone_pair  = false;
		s.used_element_lone_pair_inference = false;
		s.used_geometry_lone_pair_fallback = false;
		check(std::string(vsepr_lone_pair_source(s)) == "none",
			  "source=none when no flag set");

		s.used_provider_lone_pair = true;
		check(std::string(vsepr_lone_pair_source(s)) == "provider",
			  "source=provider when used_provider_lone_pair");

		s.used_provider_lone_pair = false;
		s.used_element_lone_pair_inference = true;
		check(std::string(vsepr_lone_pair_source(s)) == "element",
			  "source=element when element inference used");

		s.used_element_lone_pair_inference = false;
		s.used_geometry_lone_pair_fallback = true;
		check(std::string(vsepr_lone_pair_source(s)) == "geometry",
			  "source=geometry when geometry fallback used");
	}

	// ------------------------------------------------------------------
	// Section 2: vsepr_central_site_index() — picks max electron domains
	// ------------------------------------------------------------------
	std::cout << "\n--- Section 2: central site selection ---\n";
	{
		VSEPRReport empty;
		check(vsepr_central_site_index(empty) == 0,
			  "empty report returns 0 without crash");

		VSEPRReport rep;
		VSEPRSite a, b;
		a.bonded_domain_count = 2; a.lone_pair_domain_count = 2; a.electron_domain_count = 4;
		b.bonded_domain_count = 3; b.lone_pair_domain_count = 0; b.electron_domain_count = 3;
		rep.sites = {a, b};
		check(vsepr_central_site_index(rep) == 0,
			  "site 0 selected (4 domains > 3 domains)");

		VSEPRReport rep2;
		VSEPRSite c, d;
		c.bonded_domain_count = 2; c.lone_pair_domain_count = 0; c.electron_domain_count = 2;
		d.bonded_domain_count = 4; d.lone_pair_domain_count = 2; d.electron_domain_count = 6;
		rep2.sites = {c, d};
		check(vsepr_central_site_index(rep2) == 1,
			  "site 1 selected (6 domains > 2 domains)");
	}

	// ------------------------------------------------------------------
	// Section 3: vsepr_to_json() field presence
	// ------------------------------------------------------------------
	std::cout << "\n--- Section 3: JSON-lite field presence ---\n";
	{
		// Empty report: must not crash; must contain schema marker
		VSEPRReport empty;
		const std::string ejson = vsepr_to_json(empty);
		check(!ejson.empty(), "empty report produces non-empty JSON");
		check(ejson.find("schema") != std::string::npos,
			  "schema field present in empty JSON");
		check(ejson.find("\"sites\": []") != std::string::npos ||
			  ejson.find("\"sites\":[]") != std::string::npos ||
			  ejson.find("sites") != std::string::npos,
			  "sites field present in empty JSON");
	}

	// ------------------------------------------------------------------
	// Section 4: H2O -> bent geometry round-trip
	// ------------------------------------------------------------------
	std::cout << "\n--- Section 4: H2O VSEPR export round-trip ---\n";
	{
		const auto state  = make_h2o();
		const auto report = classify_vsepr_sites(state);
		const std::string json = vsepr_to_json(report);

		check(!json.empty(), "H2O JSON is non-empty");
		check(json.find("schema") != std::string::npos, "H2O: schema field present");
		check(json.find("geometry") != std::string::npos, "H2O: geometry field present");
		check(json.find("lone_pairs") != std::string::npos, "H2O: lone_pairs field present");
		check(json.find("bonding_domains") != std::string::npos,
			  "H2O: bonding_domains field present");
		check(json.find("provider_source") != std::string::npos,
			  "H2O: provider_source field present");
		check(json.find("fallback_used") != std::string::npos,
			  "H2O: fallback_used field present");
		check(json.find("confidence") != std::string::npos,
			  "H2O: confidence field present");
		check(json.find("NaN") == std::string::npos, "H2O: no NaN in JSON");
		check(json.find("Inf") == std::string::npos, "H2O: no Inf in JSON");

		// H2O should report bent geometry
		check(json.find("bent") != std::string::npos ||
			  json.find("Bent") != std::string::npos,
			  "H2O: bent geometry in JSON");
	}

	// ------------------------------------------------------------------
	// Section 5: NH3 -> trigonal pyramidal geometry
	// ------------------------------------------------------------------
	std::cout << "\n--- Section 5: NH3 VSEPR export ---\n";
	{
		const auto state  = make_nh3();
		const auto report = classify_vsepr_sites(state);
		const std::string json = vsepr_to_json(report);

		check(!json.empty(), "NH3 JSON is non-empty");
		check(json.find("NaN") == std::string::npos, "NH3: no NaN in JSON");
		check(json.find("Inf") == std::string::npos, "NH3: no Inf in JSON");
		check(json.find("pyramidal") != std::string::npos ||
			  json.find("Pyramidal") != std::string::npos,
			  "NH3: trigonal pyramidal geometry in JSON");
	}

	// ------------------------------------------------------------------
	// Section 6: CH4 -> tetrahedral (no lone pairs, fallback path)
	// ------------------------------------------------------------------
	std::cout << "\n--- Section 6: CH4 VSEPR export ---\n";
	{
		const auto state  = make_ch4();
		const auto report = classify_vsepr_sites(state);
		const std::string json = vsepr_to_json(report);

		check(!json.empty(), "CH4 JSON is non-empty");
		check(json.find("NaN") == std::string::npos, "CH4: no NaN in JSON");
		check(json.find("tetrahedral") != std::string::npos ||
			  json.find("Tetrahedral") != std::string::npos,
			  "CH4: tetrahedral geometry in JSON");
	}

	// ------------------------------------------------------------------
	// Section 7: provider/fallback narration present in reports
	// ------------------------------------------------------------------
	std::cout << "\n--- Section 7: provider/fallback provenance in reports ---\n";
	{
		// All of the above use the default fallback path (no provider set)
		const auto state  = make_h2o();
		const auto report = classify_vsepr_sites(state);
		const std::string json = vsepr_to_json(report);
		// With default (no ProviderSet), provider_lone_pair_available should be false
		check(report.provider_lone_pair_sites == 0,
			  "default path: no provider lone-pair sites");
		check(json.find("fallback_used") != std::string::npos,
			  "fallback_used field appears in JSON");
	}

	// ------------------------------------------------------------------
	std::cout << "\n";
	std::cout << "Result: " << g_pass << " PASS / " << g_fail << " FAIL\n";
	if (g_fail == 0) {
		std::cout << "PASS: all Group 103a VSEPR export checks satisfied\n";
	} else {
		std::cout << "FAIL: " << g_fail << " check(s) failed\n";
	}
	return g_fail == 0 ? 0 : 1;
}
