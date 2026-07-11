/**
 * tests/test_chemplus_declarative.cpp
 * =====================================
 * Group 91  |  WO-84T  |  day84t-chemplus-declarative-vsepr
 *
 * Unit tests for the Chem+ declarative bridge:
 *   - ChemPlusSection::classify() matches known reaction patterns
 *   - ChemPlusSection::parse_energy() extracts kJ values correctly
 *   - evaluate() returns ok=true for active sections
 *   - evaluate() populates reaction_class, energy_kj, shell_anim
 *   - vsepr_link=true populates vsepr_tag on each result
 *   - preset "999" loads 8 reactions and all classify successfully
 *   - preset "998" loads 8 reactions
 *   - class_override replaces auto-classify
 *   - energy_kj override replaces parse_energy
 *   - inactive section (no reaction, no preset) returns ok=true with note
 *   - unknown preset returns ok=false
 *   - parser round-trip: [chem_plus] keys survive parse -> doc.chem_plus
 */

#include "vsim/chemplus_declarative.hpp"
#include "vsim/vsim_document.hpp"
#include "vsim/vsim_parser.hpp"

#include <cassert>
#include <cstring>
#include <cstdio>
#include <string>
#include <cmath>

// ---------------------------------------------------------------------------
// Minimal test harness
// ---------------------------------------------------------------------------
static int _pass = 0;
static int _fail = 0;

static void check(bool cond, const char* label)
{
	if (cond) {
		++_pass;
		std::printf("  PASS: %s\n", label);
	} else {
		++_fail;
		std::printf("  FAIL: %s\n", label);
	}
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

static void test_classify_combustion()
{
	using RC = vsim::ChemPlusSection::ReactionClass;
	const auto c = vsim::ChemPlusSection::classify("CH4 + 2O2 -> CO2 + 2H2O + 891 kJ");
	check(c == RC::combustion, "classify: CH4 combustion");
}

static void test_classify_decomposition()
{
	using RC = vsim::ChemPlusSection::ReactionClass;
	const auto c = vsim::ChemPlusSection::classify("CaCO3 -> CaO + CO2");
	check(c == RC::decomposition, "classify: CaCO3 decomposition");
}

static void test_classify_acidbase()
{
	using RC = vsim::ChemPlusSection::ReactionClass;
	const auto c = vsim::ChemPlusSection::classify("HCl + NaOH -> NaCl + H2O");
	check(c == RC::acid_base, "classify: HCl + NaOH acid-base");
}

static void test_classify_general()
{
	using RC = vsim::ChemPlusSection::ReactionClass;
	const auto c = vsim::ChemPlusSection::classify("Fe + S -> FeS");
	check(c == RC::general, "classify: Fe+S general");
}

static void test_parse_energy_positive()
{
	const double e = vsim::ChemPlusSection::parse_energy("CH4 + 2O2 -> CO2 + 2H2O + 891 kJ");
	check(std::abs(e - 891.0) < 1.0, "parse_energy: CH4 combustion 891 kJ");
}

static void test_parse_energy_zero()
{
	const double e = vsim::ChemPlusSection::parse_energy("CaCO3 -> CaO + CO2");
	check(std::abs(e) < 1e-9, "parse_energy: no kJ -> 0.0");
}

static void test_parse_energy_synthesis()
{
	const double e = vsim::ChemPlusSection::parse_energy("N2 + 3H2 -> 2NH3 + 92 kJ");
	check(std::abs(e - 92.0) < 1.0, "parse_energy: N2+H2 92 kJ");
}

static void test_evaluate_inactive()
{
	vsim::ChemPlusSection cfg;  // nothing set
	const auto r = vsim::chemplus::evaluate(cfg);
	check(r.ok,                "evaluate inactive: ok=true");
	check(r.reactions.empty(), "evaluate inactive: zero reactions");
}

static void test_evaluate_single_reaction()
{
	vsim::ChemPlusSection cfg;
	cfg.reaction = "CH4 + 2O2 -> CO2 + 2H2O + 891 kJ";
	const auto r = vsim::chemplus::evaluate(cfg);
	check(r.ok,                         "single reaction: ok");
	check(r.reactions.size() == 1,      "single reaction: 1 result");
	check(r.reactions[0].reaction_class == "combustion",    "single reaction: class=combustion");
	check(r.reactions[0].has_energy,    "single reaction: has_energy");
	check(std::abs(r.reactions[0].energy_kj - 891.0) < 1.0, "single reaction: energy=891");
	check(r.reactions[0].energy_mode == "exothermic",       "single reaction: mode=exothermic");
	check(r.reactions[0].shell_anim == "flash_arrow",        "single reaction: shell_anim");
	check(r.reactions[0].web_anim   == "heat_burst",         "single reaction: web_anim");
}

static void test_evaluate_vsepr_link()
{
	vsim::ChemPlusSection cfg;
	cfg.reaction   = "CH4 + 2O2 -> CO2 + 2H2O + 891 kJ";
	cfg.vsepr_link = true;
	const auto r = vsim::chemplus::evaluate(cfg);
	check(r.ok,                              "vsepr_link: ok");
	check(!r.reactions[0].vsepr_tag.empty(), "vsepr_link: vsepr_tag populated");
	// CO2 is the first product -> AX2
	check(r.reactions[0].vsepr_tag == "AX2", "vsepr_link: CO2 -> AX2");
}

static void test_evaluate_preset_999()
{
	vsim::ChemPlusSection cfg;
	cfg.preset = "999";
	const auto r = vsim::chemplus::evaluate(cfg);
	check(r.ok,                          "preset 999: ok");
	check(r.reactions.size() == 8,       "preset 999: 8 reactions");
	bool all_classed = true;
	for (const auto& rr : r.reactions)
		if (rr.reaction_class.empty()) { all_classed = false; break; }
	check(all_classed, "preset 999: all have reaction_class");
}

static void test_evaluate_preset_998()
{
	vsim::ChemPlusSection cfg;
	cfg.preset = "998";
	const auto r = vsim::chemplus::evaluate(cfg);
	check(r.ok,                          "preset 998: ok");
	check(r.reactions.size() == 8,       "preset 998: 8 reactions");
}

static void test_evaluate_unknown_preset()
{
	vsim::ChemPlusSection cfg;
	cfg.preset = "777";
	const auto r = vsim::chemplus::evaluate(cfg);
	check(!r.ok,          "unknown preset: ok=false");
	check(!r.error.empty(), "unknown preset: error message set");
}

static void test_class_override()
{
	vsim::ChemPlusSection cfg;
	cfg.reaction       = "CaCO3 -> CaO + CO2";  // would be decomposition
	cfg.class_override = "synthesis";
	const auto r = vsim::chemplus::evaluate(cfg);
	check(r.ok, "class_override: ok");
	check(r.reactions[0].reaction_class == "synthesis", "class_override: forced to synthesis");
}

static void test_energy_override()
{
	vsim::ChemPlusSection cfg;
	cfg.reaction  = "Fe + S -> FeS";  // no kJ in string
	cfg.energy_kj = 250.5;
	const auto r = vsim::chemplus::evaluate(cfg);
	check(r.ok, "energy_override: ok");
	check(std::abs(r.reactions[0].energy_kj - 250.5) < 0.01, "energy_override: value applied");
	check(r.reactions[0].has_energy, "energy_override: has_energy=true");
}

static void test_format_result()
{
	vsim::ChemPlusSection cfg;
	cfg.reaction   = "2H2 + O2 -> 2H2O + 572 kJ";
	cfg.vsepr_link = true;
	const auto r = vsim::chemplus::evaluate(cfg);
	check(r.ok, "format: ok");
	const std::string s = vsim::chemplus::format_result(r.reactions[0]);
	check(s.find("reaction") != std::string::npos, "format: contains 'reaction'");
	check(s.find("class")    != std::string::npos, "format: contains 'class'");
	check(s.find("kJ")       != std::string::npos, "format: contains 'kJ'");
}

static void test_parser_roundtrip()
{
	// Construct a minimal .vsim string with a [chem_plus] block and parse it.
	const std::string vsim_src =
		"[project]\n"
		"name = \"chemplus_rt_test\"\n"
		"version = \"test\"\n"
		"\n"
		"[material]\n"
		"formula = \"CH4\"\n"
		"\n"
		"[chem_plus]\n"
		"reaction   = \"CH4 + 2O2 -> CO2 + 2H2O + 891 kJ\"\n"
		"vsepr_link = true\n"
		"emit_events = false\n"
		"preset     = \"\"\n";

	const auto doc = vsim::VsimParser::parse_string(vsim_src);

	check(doc.chem_plus.reaction == "CH4 + 2O2 -> CO2 + 2H2O + 891 kJ",
		  "parser roundtrip: reaction field");
	check(doc.chem_plus.vsepr_link == true,
		  "parser roundtrip: vsepr_link=true");
	check(doc.chem_plus.emit_events == false,
		  "parser roundtrip: emit_events=false");
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
int main()
{
	std::printf("\n=== Group 91: Chem+ Declarative Bridge (WO-84T) ===\n\n");

	test_classify_combustion();
	test_classify_decomposition();
	test_classify_acidbase();
	test_classify_general();
	test_parse_energy_positive();
	test_parse_energy_zero();
	test_parse_energy_synthesis();
	test_evaluate_inactive();
	test_evaluate_single_reaction();
	test_evaluate_vsepr_link();
	test_evaluate_preset_999();
	test_evaluate_preset_998();
	test_evaluate_unknown_preset();
	test_class_override();
	test_energy_override();
	test_format_result();
	test_parser_roundtrip();

	std::printf("\n  Result: %d PASS / %d FAIL\n", _pass, _fail);
	if (_fail == 0)
		std::printf("  PASS: all Group 91 Chem+ declarative checks satisfied\n\n");
	return (_fail == 0) ? 0 : 1;
}
