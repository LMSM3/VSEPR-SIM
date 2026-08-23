// =============================================================================
// tests/test_mcf_cai.cpp   WO-76 Steps 3+5  (Group 89)
// =============================================================================
// MC1   parse_mcf_layer() "macro" -> MACRO
// MC2   parse_mcf_layer() "chemical" -> CHEMICAL
// MC3   parse_mcf_layer() "fundamental" -> FUND
// MC4   parse_mcf_layer() unknown string -> false
// MC5   parse_cai_basis() "carrier" / "action" / "information" round-trip
// MC6   parse_cai_basis() unknown string -> false
// MC7   parse_mcf_cai_section() "object.macro.carrier" -> MACRO,CARRIER
// MC8   parse_mcf_cai_section() "object.fundamental.information" -> FUND,INFO
// MC9   parse_mcf_cai_section() malformed string -> false
// MC10  McfCaiCell::cell_name() returns "<layer>.<basis>"
// MC11  McfCaiSection default: is_present() all false
// MC12  McfCaiSection::mark_present() / is_present() round-trip
// MC13  McfCaiSection::has_information_column() false when no info cell set
// MC14  McfCaiSection::has_information_column() true when one info cell set
// MC15  McfCaiCell::is_information() true only for INFORMATION basis
// MC16  McfCaiCell::is_dynamics_eligible() false for INFORMATION, true others
// MC17  Parser: [object.macro.carrier] centre_x stored
// MC18  Parser: [object.chemical.action] reaction_active stored
// MC19  Parser: [object.fundamental.information] psi_hid stored
// MC20  Parser: [object.macro.information] does NOT affect Carrier or Action cells
// MC21  Parser: has_information_column() true after parsing an information section
// MC22  Parser: phase_label string round-trip via [object.macro.carrier]
// MC23  Parser: [object.chemical.carrier] coordination_number round-trip
// MC24  Parser: [object.fundamental.carrier] net_charge round-trip
// =============================================================================

#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>

#include "include/vsim/vsim_document.hpp"
#include "include/vsim/vsim_parser.hpp"
#include "include/vsim/analysis/mcf_cai.hpp"

using vsim::VsimDocument;
using vsim::VsimParser;
using vsim::VsimMcfCaiSection;
using vsim::analysis::McfLayer;
using vsim::analysis::CaiBasis;
using vsim::analysis::McfCaiCell;
using vsim::analysis::McfCaiSection;
using vsim::analysis::parse_mcf_layer;
using vsim::analysis::parse_cai_basis;
using vsim::analysis::parse_mcf_cai_section;
using vsim::analysis::MCF_LAYERS;
using vsim::analysis::CAI_BASES;

static VsimDocument parse(const char* script) {
	return VsimParser::parse_string(script, "<test>");
}

static bool near(double a, double b, double tol = 1e-7) {
	return std::fabs(a - b) < tol;
}

// ---------------------------------------------------------------------------
// MC1  parse_mcf_layer() "macro" -> MACRO
// ---------------------------------------------------------------------------
static void MC1() {
	McfLayer l;
	assert(parse_mcf_layer("macro", l) && l == McfLayer::MACRO);
	printf("MC1  PASS\n");
}

// ---------------------------------------------------------------------------
// MC2  parse_mcf_layer() "chemical" -> CHEMICAL
// ---------------------------------------------------------------------------
static void MC2() {
	McfLayer l;
	assert(parse_mcf_layer("chemical", l) && l == McfLayer::CHEMICAL);
	printf("MC2  PASS\n");
}

// ---------------------------------------------------------------------------
// MC3  parse_mcf_layer() "fundamental" -> FUND
// ---------------------------------------------------------------------------
static void MC3() {
	McfLayer l;
	assert(parse_mcf_layer("fundamental", l) && l == McfLayer::FUND);
	printf("MC3  PASS\n");
}

// ---------------------------------------------------------------------------
// MC4  parse_mcf_layer() unknown string -> false
// ---------------------------------------------------------------------------
static void MC4() {
	McfLayer l;
	assert(!parse_mcf_layer("quark", l));
	assert(!parse_mcf_layer("", l));
	printf("MC4  PASS\n");
}

// ---------------------------------------------------------------------------
// MC5  parse_cai_basis() all three valid strings
// ---------------------------------------------------------------------------
static void MC5() {
	CaiBasis b;
	assert(parse_cai_basis("carrier",     b) && b == CaiBasis::CARRIER);
	assert(parse_cai_basis("action",      b) && b == CaiBasis::ACTION);
	assert(parse_cai_basis("information", b) && b == CaiBasis::INFORMATION);
	printf("MC5  PASS\n");
}

// ---------------------------------------------------------------------------
// MC6  parse_cai_basis() unknown -> false
// ---------------------------------------------------------------------------
static void MC6() {
	CaiBasis b;
	assert(!parse_cai_basis("entropy", b));
	assert(!parse_cai_basis("",        b));
	printf("MC6  PASS\n");
}

// ---------------------------------------------------------------------------
// MC7  parse_mcf_cai_section() "object.macro.carrier"
// ---------------------------------------------------------------------------
static void MC7() {
	McfLayer l; CaiBasis b;
	assert(parse_mcf_cai_section("object.macro.carrier", l, b));
	assert(l == McfLayer::MACRO && b == CaiBasis::CARRIER);
	printf("MC7  PASS\n");
}

// ---------------------------------------------------------------------------
// MC8  parse_mcf_cai_section() "object.fundamental.information"
// ---------------------------------------------------------------------------
static void MC8() {
	McfLayer l; CaiBasis b;
	assert(parse_mcf_cai_section("object.fundamental.information", l, b));
	assert(l == McfLayer::FUND && b == CaiBasis::INFORMATION);
	printf("MC8  PASS\n");
}

// ---------------------------------------------------------------------------
// MC9  parse_mcf_cai_section() malformed -> false
// ---------------------------------------------------------------------------
static void MC9() {
	McfLayer l; CaiBasis b;
	assert(!parse_mcf_cai_section("object.macro", l, b));          // no basis
	assert(!parse_mcf_cai_section("macro.carrier", l, b));         // no prefix
	assert(!parse_mcf_cai_section("object.bad.carrier", l, b));    // bad layer
	assert(!parse_mcf_cai_section("object.macro.bad", l, b));      // bad basis
	assert(!parse_mcf_cai_section("", l, b));
	printf("MC9  PASS\n");
}

// ---------------------------------------------------------------------------
// MC10  cell_name() returns "<layer>.<basis>"
// ---------------------------------------------------------------------------
static void MC10() {
	McfCaiCell c;
	c.layer = McfLayer::CHEMICAL; c.basis = CaiBasis::ACTION;
	assert(c.cell_name() == "chemical.action");
	c.layer = McfLayer::FUND; c.basis = CaiBasis::INFORMATION;
	assert(c.cell_name() == "fundamental.information");
	printf("MC10 PASS\n");
}

// ---------------------------------------------------------------------------
// MC11  McfCaiSection default: is_present() all false
// ---------------------------------------------------------------------------
static void MC11() {
	McfCaiSection sec;
	for (int l = 0; l < MCF_LAYERS; ++l)
		for (int b = 0; b < CAI_BASES; ++b)
			assert(!sec.present[l][b]);
	printf("MC11 PASS\n");
}

// ---------------------------------------------------------------------------
// MC12  McfCaiSection::mark_present / is_present round-trip
// ---------------------------------------------------------------------------
static void MC12() {
	McfCaiSection sec;
	sec.mark_present(McfLayer::CHEMICAL, CaiBasis::ACTION);
	assert(sec.is_present(McfLayer::CHEMICAL, CaiBasis::ACTION));
	assert(!sec.is_present(McfLayer::MACRO,   CaiBasis::ACTION));
	printf("MC12 PASS\n");
}

// ---------------------------------------------------------------------------
// MC13  has_information_column() false when no info cell set
// ---------------------------------------------------------------------------
static void MC13() {
	McfCaiSection sec;
	sec.mark_present(McfLayer::MACRO,    CaiBasis::CARRIER);
	sec.mark_present(McfLayer::CHEMICAL, CaiBasis::ACTION);
	assert(!sec.has_information_column());
	printf("MC13 PASS\n");
}

// ---------------------------------------------------------------------------
// MC14  has_information_column() true when one info cell set
// ---------------------------------------------------------------------------
static void MC14() {
	McfCaiSection sec;
	sec.mark_present(McfLayer::FUND, CaiBasis::INFORMATION);
	assert(sec.has_information_column());
	printf("MC14 PASS\n");
}

// ---------------------------------------------------------------------------
// MC15  is_information() only for INFORMATION basis
// ---------------------------------------------------------------------------
static void MC15() {
	McfCaiCell ci; ci.basis = CaiBasis::INFORMATION;
	McfCaiCell cc; cc.basis = CaiBasis::CARRIER;
	McfCaiCell ca; ca.basis = CaiBasis::ACTION;
	assert(ci.is_information());
	assert(!cc.is_information());
	assert(!ca.is_information());
	printf("MC15 PASS\n");
}

// ---------------------------------------------------------------------------
// MC16  is_dynamics_eligible() false for INFORMATION, true for Carrier/Action
// ---------------------------------------------------------------------------
static void MC16() {
	McfCaiCell ci; ci.basis = CaiBasis::INFORMATION;
	McfCaiCell cc; cc.basis = CaiBasis::CARRIER;
	McfCaiCell ca; ca.basis = CaiBasis::ACTION;
	assert(!ci.is_dynamics_eligible());
	assert(cc.is_dynamics_eligible());
	assert(ca.is_dynamics_eligible());
	printf("MC16 PASS\n");
}

// ---------------------------------------------------------------------------
// MC17  Parser: [object.macro.carrier] centre_x stored
// ---------------------------------------------------------------------------
static void MC17() {
	auto doc = parse(
		"[project]\n"
		"name = \"mc17\"\n"
		"[object.macro.carrier]\n"
		"centre_x = 3.14\n"
		"centre_y = 2.72\n"
		"centre_z = 1.41\n"
	);
	const auto& cell = doc.mcf_cai.cell(McfLayer::MACRO, CaiBasis::CARRIER);
	assert(near(cell.centre_x, 3.14));
	assert(near(cell.centre_y, 2.72));
	assert(near(cell.centre_z, 1.41));
	assert(doc.mcf_cai.is_present(McfLayer::MACRO, CaiBasis::CARRIER));
	printf("MC17 PASS\n");
}

// ---------------------------------------------------------------------------
// MC18  Parser: [object.chemical.action] reaction_active stored
// ---------------------------------------------------------------------------
static void MC18() {
	auto doc = parse(
		"[project]\n"
		"name = \"mc18\"\n"
		"[object.chemical.action]\n"
		"reaction_active = true\n"
		"oxidation_state = -2\n"
	);
	const auto& cell = doc.mcf_cai.cell(McfLayer::CHEMICAL, CaiBasis::ACTION);
	assert(cell.reaction_active == true);
	assert(cell.oxidation_state == -2);
	printf("MC18 PASS\n");
}

// ---------------------------------------------------------------------------
// MC19  Parser: [object.fundamental.information] psi_hid stored
// ---------------------------------------------------------------------------
static void MC19() {
	auto doc = parse(
		"[project]\n"
		"name = \"mc19\"\n"
		"[object.fundamental.information]\n"
		"psi_hid         = 0.12\n"
		"projection_loss = 0.08\n"
		"entropy_trace   = 0.35\n"
	);
	const auto& cell = doc.mcf_cai.cell(McfLayer::FUND, CaiBasis::INFORMATION);
	assert(near(cell.psi_hid,        0.12));
	assert(near(cell.projection_loss,0.08));
	assert(near(cell.entropy_trace,  0.35));
	printf("MC19 PASS\n");
}

// ---------------------------------------------------------------------------
// MC20  Parsing [object.macro.information] does NOT touch Carrier/Action cells
// ---------------------------------------------------------------------------
static void MC20() {
	auto doc = parse(
		"[project]\n"
		"name = \"mc20\"\n"
		"[object.macro.information]\n"
		"formation_age_fs = 100.0\n"
	);
	const auto& info    = doc.mcf_cai.cell(McfLayer::MACRO, CaiBasis::INFORMATION);
	const auto& carrier = doc.mcf_cai.cell(McfLayer::MACRO, CaiBasis::CARRIER);
	const auto& action  = doc.mcf_cai.cell(McfLayer::MACRO, CaiBasis::ACTION);
	assert(near(info.formation_age_fs, 100.0));
	// carrier and action must be at their defaults
	assert(near(carrier.centre_x, 0.0));
	assert(near(action.stress_proxy, 0.0));
	assert(!doc.mcf_cai.is_present(McfLayer::MACRO, CaiBasis::CARRIER));
	assert(!doc.mcf_cai.is_present(McfLayer::MACRO, CaiBasis::ACTION));
	printf("MC20 PASS\n");
}

// ---------------------------------------------------------------------------
// MC21  has_information_column() true after parsing an information section
// ---------------------------------------------------------------------------
static void MC21() {
	auto doc = parse(
		"[project]\n"
		"name = \"mc21\"\n"
		"[object.chemical.information]\n"
		"d_chem = 0.9\n"
	);
	assert(doc.mcf_cai.has_information_column());
	printf("MC21 PASS\n");
}

// ---------------------------------------------------------------------------
// MC22  phase_label string round-trip via [object.macro.carrier]
// ---------------------------------------------------------------------------
static void MC22() {
	auto doc = parse(
		"[project]\n"
		"name = \"mc22\"\n"
		"[object.macro.carrier]\n"
		"phase_label = \"FCC\"\n"
	);
	assert(doc.mcf_cai.cell(McfLayer::MACRO, CaiBasis::CARRIER).phase_label == "FCC");
	printf("MC22 PASS\n");
}

// ---------------------------------------------------------------------------
// MC23  [object.chemical.carrier] coordination_number
// ---------------------------------------------------------------------------
static void MC23() {
	auto doc = parse(
		"[project]\n"
		"name = \"mc23\"\n"
		"[object.chemical.carrier]\n"
		"atomic_number       = 6\n"
		"coordination_number = 4\n"
	);
	const auto& c = doc.mcf_cai.cell(McfLayer::CHEMICAL, CaiBasis::CARRIER);
	assert(c.atomic_number == 6);
	assert(c.coordination_number == 4);
	printf("MC23 PASS\n");
}

// ---------------------------------------------------------------------------
// MC24  [object.fundamental.carrier] net_charge
// ---------------------------------------------------------------------------
static void MC24() {
	auto doc = parse(
		"[project]\n"
		"name = \"mc24\"\n"
		"[object.fundamental.carrier]\n"
		"net_charge  = -1.0\n"
		"spin_proxy  =  0.5\n"
	);
	const auto& c = doc.mcf_cai.cell(McfLayer::FUND, CaiBasis::CARRIER);
	assert(near(c.net_charge,  -1.0));
	assert(near(c.spin_proxy,   0.5));
	printf("MC24 PASS\n");
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main() {
	printf("=== Group 89 : MCF-CAI State Vector (WO-76 Steps 3+5) ===\n");
	MC1();  MC2();  MC3();  MC4();  MC5();
	MC6();  MC7();  MC8();  MC9();  MC10();
	MC11(); MC12(); MC13(); MC14(); MC15();
	MC16(); MC17(); MC18(); MC19(); MC20();
	MC21(); MC22(); MC23(); MC24();
	printf("=== All MC tests passed ===\n");
	return 0;
}
