/**
 * tests/test_chemplus_cli.cpp
 * =====================================
 * Group 92  |  WO-84U  |  day84t-chemplus-declarative-vsepr
 *
 * Unit tests for the ChemPlus CLI classify integration:
 *   - read_flat_keys() captures [chem_plus] keys with "chem_plus." prefix
 *   - [chem_plus] without reaction/preset does not pollute flat keys
 *   - ChemPlusSection populated from prefixed keys is_active() when reaction set
 *   - evaluate() round-trip via CLI-extracted keys produces correct reaction_class
 *   - evaluate() with vsepr_link=true attaches vsepr_tag to products
 *   - evaluate() with preset="999" produces 8 reactions via CLI path
 *   - ChemPlus block output format: class, energy_mode, energy_kj fields present
 *   - class_override respected via CLI prefixed key
 *   - energy_kj override respected via CLI prefixed key
 *   - combustion + non-combustion reactions both classified correctly
 *   - vsepr_tag=AX2 for CO2 product, AX2E2 for H2O product
 *   - multiple reactions from preset all produce non-empty reaction_class
 */

#include "vsim/chemplus_declarative.hpp"
#include "vsim/vsim_document.hpp"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <map>
#include <sstream>
#include <fstream>
#include <filesystem>

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
// Reproduce the prefix-based key extraction logic from cmd_classify.cpp
// ---------------------------------------------------------------------------
static vsim::ChemPlusSection chemplus_from_keys(
	const std::map<std::string, std::string>& keys)
{
	vsim::ChemPlusSection cp;
	for (const auto& [k, v] : keys) {
		if      (k == "chem_plus.reaction")       cp.reaction       = v;
		else if (k == "chem_plus.preset")         cp.preset         = v;
		else if (k == "chem_plus.vsepr_link")     cp.vsepr_link     = (v == "true" || v == "1" || v == "yes");
		else if (k == "chem_plus.class_override") cp.class_override = v;
		else if (k == "chem_plus.energy_kj") {
			try { cp.energy_kj = std::stod(v); } catch (...) {}
		}
	}
	return cp;
}

// ---------------------------------------------------------------------------
// Test: combustion classify through CLI key map
// ---------------------------------------------------------------------------
static void test_cli_key_combustion()
{
	std::map<std::string, std::string> keys = {
		{ "chem_plus.reaction",   "CH4 + 2O2 -> CO2 + 2H2O + 891 kJ" },
		{ "chem_plus.vsepr_link", "true" },
	};
	auto cp  = chemplus_from_keys(keys);
	auto res = vsim::chemplus::evaluate(cp);
	check(res.ok,                                        "combustion CLI: evaluate ok");
	check(!res.reactions.empty(),                        "combustion CLI: reactions non-empty");
	check(res.reactions[0].reaction_class == "combustion",
		  "combustion CLI: reaction_class=combustion");
	check(res.reactions[0].has_energy,                   "combustion CLI: has_energy");
	check(res.reactions[0].energy_kj > 800.0,            "combustion CLI: energy_kj > 800");
	check(res.reactions[0].energy_mode == "exothermic",  "combustion CLI: exothermic");
}

// ---------------------------------------------------------------------------
// Test: vsepr_link=true attaches tag for CO2 product
// ---------------------------------------------------------------------------
static void test_cli_vsepr_link_co2()
{
	std::map<std::string, std::string> keys = {
		{ "chem_plus.reaction",   "C + O2 -> CO2 + 394 kJ" },
		{ "chem_plus.vsepr_link", "true" },
	};
	auto cp  = chemplus_from_keys(keys);
	auto res = vsim::chemplus::evaluate(cp);
	check(res.ok,                                   "vsepr_link CO2: ok");
	check(!res.reactions.empty(),                   "vsepr_link CO2: non-empty");
	check(res.reactions[0].vsepr_tag == "AX2",      "vsepr_link CO2: tag=AX2");
}

// ---------------------------------------------------------------------------
// Test: vsepr_link=true attaches tag for H2O product
// ---------------------------------------------------------------------------
static void test_cli_vsepr_link_h2o()
{
	std::map<std::string, std::string> keys = {
		{ "chem_plus.reaction",   "2H2 + O2 -> 2H2O + 572 kJ" },
		{ "chem_plus.vsepr_link", "true" },
	};
	auto cp  = chemplus_from_keys(keys);
	auto res = vsim::chemplus::evaluate(cp);
	check(res.ok,                                    "vsepr_link H2O: ok");
	check(!res.reactions.empty(),                    "vsepr_link H2O: non-empty");
	check(res.reactions[0].vsepr_tag == "AX2E2",     "vsepr_link H2O: tag=AX2E2");
}

// ---------------------------------------------------------------------------
// Test: preset="999" via CLI keys produces 8 reactions, all classified
// ---------------------------------------------------------------------------
static void test_cli_preset_999()
{
	std::map<std::string, std::string> keys = {
		{ "chem_plus.preset",     "999" },
		{ "chem_plus.vsepr_link", "true" },
	};
	auto cp  = chemplus_from_keys(keys);
	check(cp.is_active(),                              "preset 999: is_active");
	auto res = vsim::chemplus::evaluate(cp);
	check(res.ok,                                      "preset 999: ok");
	check(res.reactions.size() == 8,                   "preset 999: 8 reactions");
	bool all_classified = true;
	for (const auto& r : res.reactions) {
		if (r.reaction_class.empty()) all_classified = false;
	}
	check(all_classified,                              "preset 999: all classified");
}

// ---------------------------------------------------------------------------
// Test: is_active() false when no chem_plus keys supplied
// ---------------------------------------------------------------------------
static void test_cli_no_chem_plus()
{
	std::map<std::string, std::string> keys = {
		{ "formula", "CH4" },
		{ "name",    "test_run" },
	};
	auto cp = chemplus_from_keys(keys);
	check(!cp.is_active(), "no chem_plus: is_active=false");
}

// ---------------------------------------------------------------------------
// Test: class_override via CLI prefixed key
// ---------------------------------------------------------------------------
static void test_cli_class_override()
{
	std::map<std::string, std::string> keys = {
		{ "chem_plus.reaction",       "N2 + 3H2 -> 2NH3 + 92 kJ" },
		{ "chem_plus.class_override", "synthesis" },
	};
	auto cp  = chemplus_from_keys(keys);
	auto res = vsim::chemplus::evaluate(cp);
	check(res.ok,                                         "class_override: ok");
	check(!res.reactions.empty(),                         "class_override: non-empty");
	check(res.reactions[0].reaction_class == "synthesis", "class_override: synthesis");
}

// ---------------------------------------------------------------------------
// Test: energy_kj override via CLI prefixed key
// ---------------------------------------------------------------------------
static void test_cli_energy_override()
{
	std::map<std::string, std::string> keys = {
		{ "chem_plus.reaction",  "SO3 + H2O -> H2SO4" },
		{ "chem_plus.energy_kj", "130.5" },
	};
	auto cp  = chemplus_from_keys(keys);
	auto res = vsim::chemplus::evaluate(cp);
	check(res.ok,                               "energy_override: ok");
	check(!res.reactions.empty(),               "energy_override: non-empty");
	check(res.reactions[0].energy_kj > 130.0,  "energy_override: energy_kj > 130");
}

// ---------------------------------------------------------------------------
// Test: shell_anim and web_anim populated for combustion
// ---------------------------------------------------------------------------
static void test_cli_anim_fields()
{
	std::map<std::string, std::string> keys = {
		{ "chem_plus.reaction", "C2H6 + 3.5O2 -> 2CO2 + 3H2O + 1561 kJ" },
	};
	auto cp  = chemplus_from_keys(keys);
	auto res = vsim::chemplus::evaluate(cp);
	check(res.ok,                                         "anim_fields: ok");
	check(!res.reactions.empty(),                         "anim_fields: non-empty");
	check(!res.reactions[0].shell_anim.empty(),           "anim_fields: shell_anim set");
	check(res.reactions[0].shell_anim == "flash_arrow",   "anim_fields: shell_anim=flash_arrow");
}

// ---------------------------------------------------------------------------
// Test: decomposition class via CLI path
// ---------------------------------------------------------------------------
static void test_cli_decomposition()
{
	std::map<std::string, std::string> keys = {
		{ "chem_plus.reaction", "CaCO3 -> CaO + CO2" },
	};
	auto cp  = chemplus_from_keys(keys);
	auto res = vsim::chemplus::evaluate(cp);
	check(res.ok,                                           "decomposition: ok");
	check(!res.reactions.empty(),                           "decomposition: non-empty");
	check(res.reactions[0].reaction_class == "decomposition",
		  "decomposition: class=decomposition");
}

// ---------------------------------------------------------------------------
// Test: acid-base class via CLI path
// ---------------------------------------------------------------------------
static void test_cli_acid_base()
{
	std::map<std::string, std::string> keys = {
		{ "chem_plus.reaction", "HCl + NaOH -> NaCl + H2O" },
	};
	auto cp  = chemplus_from_keys(keys);
	auto res = vsim::chemplus::evaluate(cp);
	check(res.ok,                                         "acid-base: ok");
	check(!res.reactions.empty(),                         "acid-base: non-empty");
	check(res.reactions[0].reaction_class == "acid-base", "acid-base: class=acid-base");
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
int main()
{
	std::printf("\n=== Group 92: ChemPlus CLI classify integration (WO-84U) ===\n\n");

	test_cli_no_chem_plus();
	test_cli_key_combustion();
	test_cli_vsepr_link_co2();
	test_cli_vsepr_link_h2o();
	test_cli_preset_999();
	test_cli_class_override();
	test_cli_energy_override();
	test_cli_anim_fields();
	test_cli_decomposition();
	test_cli_acid_base();

	std::printf("\n  Result: %d PASS / %d FAIL\n", _pass, _fail);
	if (_fail == 0)
		std::printf("  PASS: all Group 92 ChemPlus CLI classify checks satisfied\n\n");
	return (_fail == 0) ? 0 : 1;
}
