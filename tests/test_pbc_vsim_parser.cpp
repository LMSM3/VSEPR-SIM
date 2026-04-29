/**
 * test_pbc_vsim_parser.cpp — VSIM parser tests for [cell], [boundary], [pbc]
 * =============================================================================
 *
 * WO-VSEPR-SIM-57B gate table — parser-level checks.
 *
 * Tests:
 *   P1  [cell] parses type, lengths, units correctly
 *   P2  [cell] type = triclinic raises ParseError
 *   P3  [cell] single-value lengths sets all three axes
 *   P4  [boundary] per-axis modes parsed correctly
 *   P5  [boundary] compact form (mode + axes) parsed correctly
 *   P6  [pbc] all four fields parsed with correct values
 *   P7  [pbc] wrap_positions enum mapped correctly for all values
 *   P8  [pbc] defaults apply when [pbc] block is absent
 *   P9  [boundary] reserved mode "reflective" raises ParseError
 *   P10 pbc_bindings::make_periodic_cell populates PeriodicCell correctly
 *   P11 pbc_bindings::require_periodic throws when cell is open
 *   P12 pbc_bindings::pbc_wrap returns correct wrapped position
 *   P13 pbc_bindings::pbc_delta returns minimum-image displacement
 *   P14 pbc_bindings::pbc_distance equals norm(pbc_delta)
 *   P15 pbc_bindings::pbc_image_count throws when track_images = false
 *   P16 pbc_bindings::pbc_unwrap returns r_wrapped + img * L
 *   P17 test_pbc_error_no_cell.vsim parses; require_periodic throws on call
 *   P18 test_pbc_error_track_images.vsim parses; binding throws on call
 *
 * Style: standalone (no framework); matches test_pbc_core.cpp conventions.
 * Day #57B  |  WO-57B  |  beta-8 gate
 */

#include "vsim/vsim_parser.hpp"
#include "vsim/bindings/pbc_bindings.hpp"
#include "box/pbc.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <vector>

using namespace vsim;
using namespace vsepr;

// ── Test infrastructure ───────────────────────────────────────────────────────

static int g_pass = 0;
static int g_fail = 0;

static void PASS(const char* name) {
	std::printf("  [PASS] %s\n", name);
	++g_pass;
}
static void FAIL(const char* name, const char* reason) {
	std::fprintf(stderr, "  [FAIL] %s — %s\n", name, reason);
	++g_fail;
}
#define REQUIRE(name, cond, msg) \
	do { if (!(cond)) { FAIL(name, msg); return; } } while(0)
#define REQUIRE_NEAR(name, got, expected, tol, msg) \
	do { if (std::abs((got)-(expected)) > (tol)) { \
		char _b[256]; std::snprintf(_b, sizeof(_b), "%s: got %.15g expected %.15g", msg, (double)(got), (double)(expected)); \
		FAIL(name, _b); return; } } while(0)

// ── P1: [cell] parses type, lengths, units ────────────────────────────────────

static void test_P1_cell_basic() {
	const char* name = "P1 — [cell] basic parse";
	const char* src = R"(
[project]
name = test

[cell]
type    = orthorhombic
lengths = 10.0, 12.0, 8.0
units   = angstrom
)";
	VsimDocument doc;
	try { doc = VsimParser::parse_string(src); }
	catch (const std::exception& e) { FAIL(name, e.what()); return; }

	REQUIRE(name, doc.cell.type == "orthorhombic", "type != orthorhombic");
	REQUIRE_NEAR(name, doc.cell.lx, 10.0, 1e-12, "lx");
	REQUIRE_NEAR(name, doc.cell.ly, 12.0, 1e-12, "ly");
	REQUIRE_NEAR(name, doc.cell.lz,  8.0, 1e-12, "lz");
	REQUIRE(name, doc.cell.units == "angstrom", "units != angstrom");
	PASS(name);
}

// ── P2: [cell] type = triclinic raises ParseError ─────────────────────────────

static void test_P2_cell_triclinic_error() {
	const char* name = "P2 — [cell] type=triclinic raises ParseError";
	const char* src = R"(
[project]
name = test

[cell]
type    = triclinic
lengths = 10.0, 10.0, 10.0
)";
	bool threw = false;
	try { VsimParser::parse_string(src); }
	catch (const ParseError&) { threw = true; }
	catch (const std::exception& e) { threw = true; (void)e; }

	if (!threw) FAIL(name, "expected ParseError for type=triclinic");
	else PASS(name);
}

// ── P3: [cell] single-value lengths sets cubic box ───────────────────────────

static void test_P3_cell_single_length() {
	const char* name = "P3 — [cell] single scalar lengths";
	const char* src = R"(
[project]
name = test

[cell]
type    = orthorhombic
lengths = 15.0
units   = angstrom
)";
	VsimDocument doc;
	try { doc = VsimParser::parse_string(src); }
	catch (const std::exception& e) { FAIL(name, e.what()); return; }

	REQUIRE_NEAR(name, doc.cell.lx, 15.0, 1e-12, "lx");
	REQUIRE_NEAR(name, doc.cell.ly, 15.0, 1e-12, "ly");
	REQUIRE_NEAR(name, doc.cell.lz, 15.0, 1e-12, "lz");
	PASS(name);
}

// ── P4: [boundary] per-axis modes ────────────────────────────────────────────

static void test_P4_boundary_per_axis() {
	const char* name = "P4 — [boundary] per-axis parse";
	const char* src = R"(
[project]
name = test

[cell]
type    = orthorhombic
lengths = 10.0, 10.0, 10.0
units   = angstrom

[boundary]
x = periodic
y = open
z = periodic
)";
	VsimDocument doc;
	try { doc = VsimParser::parse_string(src); }
	catch (const std::exception& e) { FAIL(name, e.what()); return; }

	REQUIRE(name, doc.boundary.x == "periodic", "x != periodic");
	REQUIRE(name, doc.boundary.y == "open",     "y != open");
	REQUIRE(name, doc.boundary.z == "periodic", "z != periodic");
	REQUIRE(name, doc.boundary.any_periodic(),  "any_periodic() should be true");
	REQUIRE(name, !doc.boundary.all_periodic(), "all_periodic() should be false");
	PASS(name);
}

// ── P5: [boundary] compact mode = periodic ───────────────────────────────────

static void test_P5_boundary_compact() {
	const char* name = "P5 — [boundary] compact form";
	const char* src = R"(
[project]
name = test

[cell]
type    = orthorhombic
lengths = 10.0, 10.0, 10.0
units   = angstrom

[boundary]
mode = periodic
axes = x,y,z
)";
	VsimDocument doc;
	try { doc = VsimParser::parse_string(src); }
	catch (const std::exception& e) { FAIL(name, e.what()); return; }

	REQUIRE(name, doc.boundary.all_periodic(), "all axes should be periodic");
	PASS(name);
}

// ── P6: [pbc] all four fields ────────────────────────────────────────────────

static void test_P6_pbc_all_fields() {
	const char* name = "P6 — [pbc] all fields parsed";
	const char* src = R"(
[project]
name = test

[pbc]
minimum_image        = false
wrap_positions       = never
track_images         = false
unwrap_for_diffusion = false
)";
	VsimDocument doc;
	try { doc = VsimParser::parse_string(src); }
	catch (const std::exception& e) { FAIL(name, e.what()); return; }

	REQUIRE(name, doc.pbc.minimum_image == false,            "minimum_image");
	REQUIRE(name, doc.pbc.wrap_positions == WrapMode::Never, "wrap_positions");
	REQUIRE(name, doc.pbc.track_images == false,             "track_images");
	REQUIRE(name, doc.pbc.unwrap_for_diffusion == false,     "unwrap_for_diffusion");
	PASS(name);
}

// ── P7: [pbc] wrap_positions enum values ─────────────────────────────────────

static void test_P7_pbc_wrap_modes() {
	const char* name = "P7 — [pbc] wrap_positions enum";

	auto parse_wrap = [](const char* val) {
		std::string src = std::string("[project]\nname=test\n[pbc]\nwrap_positions = ") + val + "\n";
		return VsimParser::parse_string(src).pbc.wrap_positions;
	};

	bool ok = true;
	if (parse_wrap("never")       != WrapMode::Never)      { FAIL(name, "never");       ok=false; }
	if (parse_wrap("after_step")  != WrapMode::AfterStep)  { FAIL(name, "after_step");  ok=false; }
	if (parse_wrap("after_force") != WrapMode::AfterForce) { FAIL(name, "after_force"); ok=false; }
	if (parse_wrap("on_export")   != WrapMode::OnExport)   { FAIL(name, "on_export");   ok=false; }
	if (ok) PASS(name);
}

// ── P8: defaults when [pbc] absent ───────────────────────────────────────────

static void test_P8_pbc_defaults() {
	const char* name = "P8 — [pbc] defaults when absent";
	const char* src = "[project]\nname=test\n";
	VsimDocument doc;
	try { doc = VsimParser::parse_string(src); }
	catch (const std::exception& e) { FAIL(name, e.what()); return; }

	REQUIRE(name, doc.pbc.minimum_image == true,                "default minimum_image");
	REQUIRE(name, doc.pbc.wrap_positions == WrapMode::AfterStep,"default wrap_positions");
	REQUIRE(name, doc.pbc.track_images == true,                 "default track_images");
	REQUIRE(name, doc.pbc.unwrap_for_diffusion == true,         "default unwrap_for_diffusion");
	PASS(name);
}

// ── P9: [boundary] reserved mode raises ParseError ───────────────────────────

static void test_P9_boundary_reserved_mode() {
	const char* name = "P9 — [boundary] reflective raises ParseError";
	const char* src = R"(
[project]
name = test

[boundary]
x = reflective
)";
	bool threw = false;
	try { VsimParser::parse_string(src); }
	catch (const ParseError&) { threw = true; }
	catch (const std::exception&) { threw = true; }

	if (!threw) FAIL(name, "expected ParseError for mode=reflective");
	else PASS(name);
}

// ── P10: make_periodic_cell builds PeriodicCell correctly ────────────────────

static void test_P10_make_periodic_cell() {
	const char* name = "P10 — make_periodic_cell from document";
	CellSection     cs; cs.lx = 10.0; cs.ly = 12.0; cs.lz = 8.0;
	BoundarySection bs; bs.x = "periodic"; bs.y = "open"; bs.z = "periodic";

	PeriodicCell pc;
	try { pc = vsim::pbc_bindings::make_periodic_cell(cs, bs); }
	catch (const std::exception& e) { FAIL(name, e.what()); return; }

	REQUIRE(name, pc.periodic_x == true,  "periodic_x");
	REQUIRE(name, pc.periodic_y == false, "periodic_y");
	REQUIRE(name, pc.periodic_z == true,  "periodic_z");
	REQUIRE_NEAR(name, pc.lengths.x, 10.0, 1e-12, "lengths.x");
	REQUIRE_NEAR(name, pc.lengths.y, 12.0, 1e-12, "lengths.y");
	REQUIRE_NEAR(name, pc.lengths.z,  8.0, 1e-12, "lengths.z");
	PASS(name);
}

// ── P11: require_periodic throws for open cell ───────────────────────────────

static void test_P11_require_periodic_throws() {
	const char* name = "P11 — require_periodic throws for open cell";
	PeriodicCell open_cell;  // default: enabled=false

	bool threw = false;
	try { vsim::pbc_bindings::require_periodic(open_cell); }
	catch (const std::runtime_error&) { threw = true; }

	if (!threw) FAIL(name, "expected runtime_error for open cell");
	else PASS(name);
}

// ── P12: pbc_wrap correct ────────────────────────────────────────────────────

static void test_P12_pbc_wrap() {
	const char* name = "P12 — pbc_bindings::pbc_wrap";
	PeriodicCell cell{{10.0, 10.0, 10.0}, true, true, true};

	Vec3 r_pos{10.2, 5.0, 5.0};
	Vec3 w_pos = vsim::pbc_bindings::pbc_wrap(r_pos, cell);
	REQUIRE_NEAR(name, w_pos.x, 0.2, 1e-12, "positive overflow x");

	Vec3 r_neg{-0.2, 5.0, 5.0};
	Vec3 w_neg = vsim::pbc_bindings::pbc_wrap(r_neg, cell);
	REQUIRE_NEAR(name, w_neg.x, 9.8, 1e-12, "negative overflow x");
	PASS(name);
}

// ── P13: pbc_delta minimum-image ─────────────────────────────────────────────

static void test_P13_pbc_delta() {
	const char* name = "P13 — pbc_bindings::pbc_delta";
	PeriodicCell cell{{10.0, 10.0, 10.0}, true, true, true};
	Vec3 ri{9.9, 5.0, 5.0};
	Vec3 rj{0.1, 5.0, 5.0};
	Vec3 dr = vsim::pbc_bindings::pbc_delta(ri, rj, cell);
	REQUIRE_NEAR(name, dr.x, 0.2, 1e-12, "dr.x");
	REQUIRE_NEAR(name, dr.y, 0.0, 1e-12, "dr.y");
	REQUIRE_NEAR(name, dr.z, 0.0, 1e-12, "dr.z");
	PASS(name);
}

// ── P14: pbc_distance = norm(pbc_delta) ──────────────────────────────────────

static void test_P14_pbc_distance() {
	const char* name = "P14 — pbc_bindings::pbc_distance";
	PeriodicCell cell{{10.0, 10.0, 10.0}, true, true, true};
	Vec3 ri{9.9, 5.0, 5.0};
	Vec3 rj{0.1, 5.0, 5.0};
	double d = vsim::pbc_bindings::pbc_distance(ri, rj, cell);
	REQUIRE_NEAR(name, d, 0.2, 1e-12, "distance");
	PASS(name);
}

// ── P15: pbc_image_count throws when track_images = false ────────────────────

static void test_P15_image_count_feature_gate() {
	const char* name = "P15 — pbc_image_count throws for track_images=false";
	PBCSection cfg;
	cfg.track_images = false;
	std::vector<ImageCount> imgs(5);

	bool threw = false;
	try { vsim::pbc_bindings::pbc_image_count(1, cfg, imgs); }
	catch (const std::runtime_error&) { threw = true; }

	if (!threw) FAIL(name, "expected runtime_error for track_images=false");
	else PASS(name);
}

// ── P16: pbc_unwrap returns r + img * L ──────────────────────────────────────

static void test_P16_pbc_unwrap() {
	const char* name = "P16 — pbc_bindings::pbc_unwrap";
	PeriodicCell cell{{10.0, 10.0, 10.0}, true, true, true};
	PBCSection cfg; cfg.track_images = true;

	std::vector<Vec3>       positions = { {2.0, 3.0, 4.0} };
	std::vector<ImageCount> images    = { {1, -2, 3} };

	Vec3 u = vsim::pbc_bindings::pbc_unwrap(1, cfg, cell, positions, images);
	// Expected: (2+10, 3-20, 4+30) = (12, -17, 34)
	REQUIRE_NEAR(name, u.x,  12.0, 1e-12, "unwrap x");
	REQUIRE_NEAR(name, u.y, -17.0, 1e-12, "unwrap y");
	REQUIRE_NEAR(name, u.z,  34.0, 1e-12, "unwrap z");
	PASS(name);
}

// ── P17: test_pbc_error_no_cell.vsim parses; binding throws on call ───────────

static void test_P17_no_cell_parse_then_binding_throws() {
	const char* name = "P17 — no-cell script parses; pbc_wrap throws at call";
	const char* src = R"(
[project]
name = test_pbc_error_no_cell
version = v5.0.0-beta.8
determinism = true
)";
	VsimDocument doc;
	try { doc = VsimParser::parse_string(src); }
	catch (const std::exception& e) { FAIL(name, e.what()); return; }

	// Build PeriodicCell from parsed (empty) sections
	PeriodicCell cell;
	try { cell = vsim::pbc_bindings::make_periodic_cell(doc.cell, doc.boundary); }
	catch (...) { /* expected when both are empty: cell has no lengths */ }

	// Calling pbc_wrap on open cell must throw
	bool threw = false;
	try { vsim::pbc_bindings::pbc_wrap({1.0, 1.0, 1.0}, cell); }
	catch (const std::runtime_error&) { threw = true; }

	if (!threw) FAIL(name, "pbc_wrap should throw for open cell");
	else PASS(name);
}

// ── P18: test_pbc_error_track_images.vsim: binding throws ────────────────────

static void test_P18_track_images_false_binding_throws() {
	const char* name = "P18 — track_images=false script; image_count+unwrap throw";
	const char* src = R"(
[project]
name = test_pbc_error_track_images

[cell]
type    = orthorhombic
lengths = 10.0, 10.0, 10.0
units   = angstrom

[boundary]
x = periodic
y = periodic
z = periodic

[pbc]
track_images = false
)";
	VsimDocument doc;
	try { doc = VsimParser::parse_string(src); }
	catch (const std::exception& e) { FAIL(name, e.what()); return; }

	std::vector<ImageCount> imgs(3);
	std::vector<Vec3>       pos(3, Vec3{1.0, 1.0, 1.0});
	PeriodicCell cell = vsim::pbc_bindings::make_periodic_cell(doc.cell, doc.boundary);

	bool ic_threw = false, uw_threw = false;
	try { vsim::pbc_bindings::pbc_image_count(1, doc.pbc, imgs); }
	catch (const std::runtime_error&) { ic_threw = true; }

	try { vsim::pbc_bindings::pbc_unwrap(1, doc.pbc, cell, pos, imgs); }
	catch (const std::runtime_error&) { uw_threw = true; }

	if (!ic_threw) FAIL(name, "pbc_image_count should throw for track_images=false");
	else if (!uw_threw) FAIL(name, "pbc_unwrap should throw for track_images=false");
	else PASS(name);
}

// ── Main ─────────────────────────────────────────────────────────────────────

int main() {
	std::printf("\n");
	std::printf("╔══════════════════════════════════════════════════════════════╗\n");
	std::printf("║   test_pbc_vsim_parser — WO-VSEPR-SIM-57B gate             ║\n");
	std::printf("║   [cell] / [boundary] / [pbc] parser + pbc_bindings        ║\n");
	std::printf("╚══════════════════════════════════════════════════════════════╝\n\n");

	test_P1_cell_basic();
	test_P2_cell_triclinic_error();
	test_P3_cell_single_length();
	test_P4_boundary_per_axis();
	test_P5_boundary_compact();
	test_P6_pbc_all_fields();
	test_P7_pbc_wrap_modes();
	test_P8_pbc_defaults();
	test_P9_boundary_reserved_mode();
	test_P10_make_periodic_cell();
	test_P11_require_periodic_throws();
	test_P12_pbc_wrap();
	test_P13_pbc_delta();
	test_P14_pbc_distance();
	test_P15_image_count_feature_gate();
	test_P16_pbc_unwrap();
	test_P17_no_cell_parse_then_binding_throws();
	test_P18_track_images_false_binding_throws();

	std::printf("\nResults: %d passed, %d failed\n\n", g_pass, g_fail);

	if (g_fail == 0) {
		std::printf("╔══════════════════════════════════════════════════════════════╗\n");
		std::printf("║  ALL TESTS PASS — WO-57B parser gate: CLEAR                 ║\n");
		std::printf("╚══════════════════════════════════════════════════════════════╝\n\n");
		return 0;
	} else {
		std::fprintf(stderr,
			"╔══════════════════════════════════════════════════════════════╗\n"
			"║  GATE FAILED — %d test(s) did not pass\n"
			"╚══════════════════════════════════════════════════════════════╝\n\n",
			g_fail);
		return 1;
	}
}
