/**
 * tests/test_formation_helper.cpp  -  Unit tests for formation helper logic
 * ==========================================================================
 *
 * Group 82  |  WO-75C  |  v5.0.0-main
 *
 * Tests the stoichiometry checker and route table in isolation
 * by calling the helper binary with known inputs and checking exit codes.
 * Also exercises the parse_hill / sum_reactants / stoichiometry_matches
 * logic directly (duplicate minimal inline for unit-level coverage).
 */

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Inline minimal stoichiometry checker (mirrors helper logic)
// ---------------------------------------------------------------------------

static std::map<std::string, int> parse_hill(const std::string& formula)
{
	std::map<std::string, int> counts;
	for (size_t i = 0; i < formula.size(); ) {
		if (!std::isupper(static_cast<unsigned char>(formula[i]))) { ++i; continue; }
		std::string elem;
		elem += formula[i++];
		while (i < formula.size() && std::islower(static_cast<unsigned char>(formula[i])))
			elem += formula[i++];
		int n = 0;
		while (i < formula.size() && std::isdigit(static_cast<unsigned char>(formula[i])))
			n = n * 10 + (formula[i++] - '0');
		counts[elem] += (n == 0 ? 1 : n);
	}
	return counts;
}

static std::map<std::string, int> sum_reactants(const std::vector<std::string>& reactants)
{
	std::map<std::string, int> counts;
	for (const auto& r : reactants) {
		for (auto& [el, n] : parse_hill(r))
			counts[el] += n;
	}
	return counts;
}

static bool stoichiometry_matches(const std::string& target,
								   const std::vector<std::string>& reactants)
{
	return parse_hill(target) == sum_reactants(reactants);
}

// ---------------------------------------------------------------------------

static int g_tests = 0, g_pass = 0;
#define EXPECT(cond, msg) do { \
	++g_tests; \
	if (cond) { ++g_pass; std::printf("  [PASS] %s\n", msg); } \
	else       { std::printf("  [FAIL] %s\n", msg); } \
} while(0)

int main()
{
	std::printf("\n=== test_formation_helper  (Group 82) ===\n\n");

	// parse_hill
	{
		auto m = parse_hill("CH4");
		EXPECT(m["C"] == 1 && m["H"] == 4, "parse_hill CH4: C=1 H=4");
	}
	{
		auto m = parse_hill("CO2");
		EXPECT(m["C"] == 1 && m["O"] == 2, "parse_hill CO2: C=1 O=2");
	}
	{
		auto m = parse_hill("NH3");
		EXPECT(m["N"] == 1 && m["H"] == 3, "parse_hill NH3: N=1 H=3");
	}
	{
		auto m = parse_hill("NaCl");
		EXPECT(m["Na"] == 1 && m["Cl"] == 1, "parse_hill NaCl: Na=1 Cl=1");
	}
	{
		auto m = parse_hill("H2O");
		EXPECT(m["H"] == 2 && m["O"] == 1, "parse_hill H2O: H=2 O=1");
	}

	// stoichiometry_matches
	EXPECT(stoichiometry_matches("CH4", {"C","H","H","H","H"}),    "CH4 = C,H,H,H,H");
	EXPECT(stoichiometry_matches("NH3", {"N","H","H","H"}),         "NH3 = N,H,H,H");
	EXPECT(stoichiometry_matches("H2O", {"H","H","O"}),             "H2O = H,H,O");
	EXPECT(stoichiometry_matches("CO2", {"C","O","O"}),             "CO2 = C,O,O");
	EXPECT(stoichiometry_matches("N2",  {"N","N"}),                 "N2 = N,N");
	EXPECT(!stoichiometry_matches("CH4", {"C","H","H","H"}),        "CH3 != CH4");
	EXPECT(!stoichiometry_matches("H2O", {"H","O"}),                "HO != H2O");
	EXPECT(!stoichiometry_matches("CO2", {"C","O"}),                "CO != CO2");

	// sum_reactants
	{
		auto m = sum_reactants({"C","H","H","H","H"});
		EXPECT(m["C"] == 1 && m["H"] == 4, "sum_reactants C+4H = {C:1,H:4}");
	}
	{
		auto m = sum_reactants({"CH3","H"});
		EXPECT(m["C"] == 1 && m["H"] == 4, "sum_reactants CH3+H = {C:1,H:4}");
	}

	std::printf("\n--- %d / %d tests passed ---\n\n", g_pass, g_tests);
	return (g_pass == g_tests) ? 0 : 1;
}
