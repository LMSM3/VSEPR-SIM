/**
 * test_organic_formula_parser.cpp — Day 48B
 *
 * Tests for the three-strategy organic formula parser:
 *   1. Amino acid sequence expansion
 *   2. Trivial name lookup
 *   3. Condensed formula normalisation
 *   4. Top-level dispatcher
 *   5. Edge cases
 */

#include "organic/organic_formula_parser.hpp"

#include <cstdio>
#include <cstring>
#include <string>

// ============================================================================
// Minimal test harness (matches style of other chem tests)
// ============================================================================

static int g_total = 0;
static int g_passed = 0;
static int g_failed = 0;
static const char* g_test_name = "";

#define TEST(name) do { g_test_name = (name); ++g_total; } while(0)
#define CHECK(expr, msg) do { \
	if (!(expr)) { \
		std::fprintf(stderr, "  FAIL [%s]: %s\n", g_test_name, msg); \
		++g_failed; \
	} \
} while(0)
#define PASS() do { ++g_passed; } while(0)

// ============================================================================
// Test 1: Single amino acid residue lookup
// ============================================================================

static void test_single_residue_lookup() {
	TEST("Single residue formula lookup");
	using namespace vsepr::chem::organic;
	const ResidueFormula* gly = residue_formula('G');
	CHECK(gly != nullptr, "G residue should exist");
	CHECK(gly->C == 2, "Gly C count");
	CHECK(gly->H == 3, "Gly H count (residue)");
	CHECK(gly->N == 1, "Gly N count");
	CHECK(gly->O == 1, "Gly O count");
	CHECK(gly->S == 0, "Gly S count");

	const ResidueFormula* cys = residue_formula('C');
	CHECK(cys != nullptr, "C residue should exist");
	CHECK(cys->S == 1, "Cys should have 1 S");

	const ResidueFormula* bad = residue_formula('X');
	CHECK(bad == nullptr, "X is not a valid residue code");
	PASS();
}

// ============================================================================
// Test 2: Single amino acid sequence (free acid formula)
// ============================================================================

static void test_single_aa_free_acid() {
	TEST("Single AA sequence = free amino acid formula");
	using namespace vsepr::chem::organic;
	// G = residue C2H3N1O1 + H2O = C2H5NO2
	auto r = parse_amino_acid_sequence("G");
	CHECK(r.has_value(), "G sequence should parse");
	CHECK(r->ok, "result ok");
	CHECK(r->formula == "C2H5NO2", ("glycine formula: got " + r->formula).c_str());
	CHECK(r->residue_count == 1, "1 residue");
	CHECK(r->domain == "peptide", "domain is peptide");
	PASS();
}

// ============================================================================
// Test 3: Multi-residue sequence
// ============================================================================

static void test_sequence_gly_ala() {
	TEST("Gly-Ala dipeptide formula");
	using namespace vsepr::chem::organic;
	// G: C2H3NO  +  A: C3H5NO  +  H2O terminal
	// = C5H8N2O2 + H2O = C5H10N2O3
	auto r = parse_amino_acid_sequence("GA");
	CHECK(r.has_value(), "GA should parse");
	CHECK(r->formula == "C5H10N2O3", ("GA formula: got " + r->formula).c_str());
	CHECK(r->residue_count == 2, "2 residues");
	PASS();
}

// ============================================================================
// Test 4: Trivial name lookup — small molecules
// ============================================================================

static void test_trivial_names() {
	TEST("Trivial name lookup");
	using namespace vsepr::chem::organic;

	auto ethanol = lookup_trivial_name("ethanol");
	CHECK(ethanol.has_value(), "ethanol found");
	CHECK(ethanol->formula == "C2H6O", ("ethanol formula: got " + ethanol->formula).c_str());

	auto acetic = lookup_trivial_name("Acetic Acid");  // case-insensitive
	CHECK(acetic.has_value(), "acetic acid found (mixed case)");
	CHECK(acetic->formula == "C2H4O2", ("acetic acid formula: got " + acetic->formula).c_str());

	auto dmso = lookup_trivial_name("DMSO");
	CHECK(dmso.has_value(), "DMSO found");
	CHECK(dmso->formula == "C2H6OS", ("DMSO formula: got " + dmso->formula).c_str());

	auto missing = lookup_trivial_name("unobtainium");
	CHECK(!missing.has_value(), "unknown name returns nullopt");
	PASS();
}

// ============================================================================
// Test 5: Trivial name for free amino acid
// ============================================================================

static void test_trivial_amino_acid_name() {
	TEST("Trivial amino acid name lookup");
	using namespace vsepr::chem::organic;

	auto gly = lookup_trivial_name("glycine");
	CHECK(gly.has_value(), "glycine trivial name found");
	CHECK(gly->formula == "C2H5NO2", ("glycine trivial: got " + gly->formula).c_str());

	auto ala = lookup_trivial_name("alanine");
	CHECK(ala.has_value(), "alanine found");
	CHECK(ala->formula == "C3H7NO2", ("alanine: got " + ala->formula).c_str());
	PASS();
}

// ============================================================================
// Test 6: Condensed formula normalisation
// ============================================================================

static void test_condensed_formula() {
	TEST("Condensed formula normalisation");
	using namespace vsepr::chem::organic;

	// CH3COOH → C2H4O2
	auto acetic = normalise_condensed_formula("CH3COOH");
	CHECK(acetic.has_value(), "CH3COOH normalises");
	CHECK(acetic->formula == "C2H4O2", ("CH3COOH: got " + acetic->formula).c_str());

	// C2H5OH → C2H6O
	auto ethanol = normalise_condensed_formula("C2H5OH");
	CHECK(ethanol.has_value(), "C2H5OH normalises");
	CHECK(ethanol->formula == "C2H6O", ("C2H5OH: got " + ethanol->formula).c_str());

	// NaCl → ClNa (Hill: no C, so alphabetical)
	auto nacl = normalise_condensed_formula("NaCl");
	CHECK(nacl.has_value(), "NaCl normalises");
	CHECK(nacl->formula == "ClNa", ("NaCl Hill: got " + nacl->formula).c_str());

	PASS();
}

// ============================================================================
// Test 7: Top-level dispatcher
// ============================================================================

static void test_dispatcher() {
	TEST("Top-level dispatcher strategy ordering");
	using namespace vsepr::chem::organic;

	// Strategy 1: sequence
	auto seq = expand_organic_formula("ACDEFG");
	CHECK(seq.ok, "ACDEFG dispatches as sequence");
	CHECK(seq.domain == "peptide", "domain = peptide");
	CHECK(seq.residue_count == 6, "6 residues");

	// Strategy 2: trivial name
	auto triv = expand_organic_formula("caffeine");
	CHECK(triv.ok, "caffeine dispatches as trivial name");
	CHECK(triv.formula == "C8H10N4O2", ("caffeine: got " + triv.formula).c_str());

	// Strategy 3: condensed formula
	auto cond = expand_organic_formula("C6H12O6");
	CHECK(cond.ok, "C6H12O6 normalises");
	CHECK(cond.formula == "C6H12O6", "C6H12O6 already canonical");

	// Failure case
	auto bad = expand_organic_formula("???");
	CHECK(!bad.ok, "unrecognised input returns ok=false");
	CHECK(!bad.error.empty(), "error message is set");

	PASS();
}

// ============================================================================
// Test 8: Cysteine disulfide accounting (S count)
// ============================================================================

static void test_sulfur_containing_sequence() {
	TEST("Sulfur-containing sequence (CC)");
	using namespace vsepr::chem::organic;
	// CC: 2 x Cys residue (C3H5NOS each) + H2O terminal
	// = C6H10N2O2S2 + H2O = C6H12N2O3S2
	auto r = parse_amino_acid_sequence("CC");
	CHECK(r.has_value(), "CC parses");
	CHECK(r->formula == "C6H12N2O3S2", ("CC: got " + r->formula).c_str());
	PASS();
}

// ============================================================================
// main
// ============================================================================

int main() {
	std::printf("=============================================================\n");
	std::printf("  VSEPR-SIM  Day 48B  |  Organic Formula Parser Tests\n");
	std::printf("=============================================================\n");

	test_single_residue_lookup();
	test_single_aa_free_acid();
	test_sequence_gly_ala();
	test_trivial_names();
	test_trivial_amino_acid_name();
	test_condensed_formula();
	test_dispatcher();
	test_sulfur_containing_sequence();

	std::printf("-------------------------------------------------------------\n");
	std::printf("  Total: %d   Passed: %d   Failed: %d\n",
				g_total, g_passed, g_failed);
	std::printf("=============================================================\n");

	return g_failed == 0 ? 0 : 1;
}
