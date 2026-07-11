/**
 * test_pbc_bindings.cpp — C++ binding tests for pbc.* namespace
 * ==============================================================
 *
 * WO-VSEPR-SIM-57C gate table — binding correctness and error paths.
 *
 * Tests:
 *   B1  pbc_wrap  — positive overflow wraps correctly
 *   B2  pbc_wrap  — negative overflow wraps correctly
 *   B3  pbc_wrap  — raises when no periodic cell configured
 *   B4  pbc_delta — returns minimum-image displacement for boundary pair
 *   B5  pbc_delta — non-periodic axis keeps raw displacement
 *   B6  pbc_distance — equals norm(pbc_delta(...))
 *   B7  pbc_crossed_boundary — detects crossing (image changed)
 *   B8  pbc_crossed_boundary — returns false when no crossing occurred
 *   B9  pbc_crossed_boundary — raises when track_images = false
 *   B10 pbc_image_count — returns correct (ix, iy, iz)
 *   B11 pbc_image_count — raises when track_images = false
 *   B12 pbc_unwrap — reconstructs continuous position from image + cell
 *   B13 Particle ID 0 raises range error (1-indexed)
 *   B14 Particle ID out of range raises range error
 *
 * Style: standalone (no framework); matches test_pbc_core.cpp conventions.
 * Day #57C  |  WO-57C  |  beta-8 gate
 */

#include "vsim/bindings/pbc_bindings.hpp"
#include "box/pbc.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <vector>

using namespace vsim;
using namespace vsim::pbc_bindings;
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
		char _b[256]; \
		std::snprintf(_b, sizeof(_b), "%s: got %.15g expected %.15g", \
					  msg, (double)(got), (double)(expected)); \
		FAIL(name, _b); return; } } while(0)
#define REQUIRE_THROWS(name, expr, exc_type, substr) \
	do { \
		bool _thrown = false; \
		try { expr; } \
		catch (const exc_type& _e) { \
			_thrown = true; \
			std::string _msg(_e.what()); \
			if (_msg.find(substr) == std::string::npos) { \
				char _b[512]; \
				std::snprintf(_b, sizeof(_b), \
					"expected substring '%s' not found in: %s", substr, _msg.c_str()); \
				FAIL(name, _b); return; \
			} \
		} \
		catch (...) { FAIL(name, "wrong exception type"); return; } \
		if (!_thrown) { FAIL(name, "expected exception not thrown"); return; } \
	} while(0)

// ── Helpers ───────────────────────────────────────────────────────────────────

static PeriodicCell make_cell(double lx, double ly, double lz,
							   bool px = true, bool py = true, bool pz = true)
{
	PeriodicCell cell;
	cell.lengths    = {lx, ly, lz};
	cell.periodic_x = px;
	cell.periodic_y = py;
	cell.periodic_z = pz;
	return cell;
}

static PeriodicCell make_open_cell() {
	PeriodicCell cell;
	cell.lengths    = {0.0, 0.0, 0.0};
	cell.periodic_x = false;
	cell.periodic_y = false;
	cell.periodic_z = false;
	return cell;
}

static PBCSection make_pbc_cfg(bool track = true) {
	PBCSection cfg;
	cfg.track_images = track;
	return cfg;
}

// ── B1: pbc_wrap positive overflow ───────────────────────────────────────────

static void test_B1_wrap_positive() {
	const char* name = "B1 — pbc_wrap positive overflow";
	PeriodicCell cell = make_cell(10.0, 10.0, 10.0);
	Vec3 r{10.2, 5.0, 5.0};
	Vec3 w = pbc_wrap(r, cell);
	REQUIRE_NEAR(name, w.x,  0.2, 1e-12, "x");
	REQUIRE_NEAR(name, w.y,  5.0, 1e-12, "y");
	REQUIRE_NEAR(name, w.z,  5.0, 1e-12, "z");
	PASS(name);
}

// ── B2: pbc_wrap negative overflow ───────────────────────────────────────────

static void test_B2_wrap_negative() {
	const char* name = "B2 — pbc_wrap negative overflow";
	PeriodicCell cell = make_cell(10.0, 10.0, 10.0);
	Vec3 r{-0.2, 5.0, 5.0};
	Vec3 w = pbc_wrap(r, cell);
	REQUIRE_NEAR(name, w.x, 9.8, 1e-12, "x");
	REQUIRE_NEAR(name, w.y, 5.0, 1e-12, "y");
	REQUIRE_NEAR(name, w.z, 5.0, 1e-12, "z");
	PASS(name);
}

// ── B3: pbc_wrap raises when no cell configured ───────────────────────────────

static void test_B3_wrap_no_cell() {
	const char* name = "B3 — pbc_wrap raises without periodic cell";
	PeriodicCell open = make_open_cell();
	REQUIRE_THROWS(name,
		pbc_wrap(Vec3{1.0, 1.0, 1.0}, open),
		std::runtime_error,
		"no periodic cell");
	PASS(name);
}

// ── B4: pbc_delta minimum-image across boundary ───────────────────────────────

static void test_B4_delta_minimum_image() {
	const char* name = "B4 — pbc_delta minimum-image displacement";
	PeriodicCell cell = make_cell(10.0, 10.0, 10.0);
	Vec3 ri{9.9, 5.0, 5.0};
	Vec3 rj{0.1, 5.0, 5.0};
	Vec3 dr = pbc_delta(ri, rj, cell);
	REQUIRE_NEAR(name, dr.x,  0.2, 1e-12, "x");
	REQUIRE_NEAR(name, dr.y,  0.0, 1e-12, "y");
	REQUIRE_NEAR(name, dr.z,  0.0, 1e-12, "z");
	PASS(name);
}

// ── B5: pbc_delta non-periodic axis keeps raw displacement ───────────────────

static void test_B5_delta_nonperiodic_axis() {
	const char* name = "B5 — pbc_delta non-periodic axis raw displacement";
	// Only x periodic; z open
	PeriodicCell cell = make_cell(10.0, 10.0, 10.0, true, true, false);
	Vec3 ri{2.0, 5.0, 1.0};
	Vec3 rj{3.0, 5.0, 9.0};
	Vec3 dr = pbc_delta(ri, rj, cell);
	REQUIRE_NEAR(name, dr.x,  1.0, 1e-12, "x (periodic)");
	REQUIRE_NEAR(name, dr.z,  8.0, 1e-12, "z (open — raw)");
	PASS(name);
}

// ── B6: pbc_distance equals norm(pbc_delta) ──────────────────────────────────

static void test_B6_distance_equals_norm_delta() {
	const char* name = "B6 — pbc_distance == norm(pbc_delta)";
	PeriodicCell cell = make_cell(10.0, 10.0, 10.0);
	Vec3 ri{1.0, 2.0, 3.0};
	Vec3 rj{8.5, 9.0, 7.0};
	Vec3 dr   = pbc_delta(ri, rj, cell);
	double d_delta = std::sqrt(dr.x*dr.x + dr.y*dr.y + dr.z*dr.z);
	double d_fn    = pbc_bindings::pbc_distance(ri, rj, cell);
	REQUIRE_NEAR(name, d_fn, d_delta, 1e-12, "distance vs norm(delta)");
	PASS(name);
}

// ── B7: pbc_crossed_boundary detects crossing ─────────────────────────────────

static void test_B7_crossed_boundary_true() {
	const char* name = "B7 — pbc_crossed_boundary detects crossing";
	PBCSection cfg = make_pbc_cfg(true);
	std::vector<ImageCount> curr  = {{1, 0, 0}};  // crossed x once
	std::vector<ImageCount> prev  = {{0, 0, 0}};  // had not crossed
	bool crossed = pbc_crossed_boundary(1, cfg, curr, prev);
	REQUIRE(name, crossed == true, "expected true");
	PASS(name);
}

// ── B8: pbc_crossed_boundary returns false when no crossing occurred ──────────

static void test_B8_crossed_boundary_false() {
	const char* name = "B8 — pbc_crossed_boundary false when no crossing";
	PBCSection cfg = make_pbc_cfg(true);
	std::vector<ImageCount> curr  = {{2, 1, 0}};
	std::vector<ImageCount> prev  = {{2, 1, 0}};  // same as current
	bool crossed = pbc_crossed_boundary(1, cfg, curr, prev);
	REQUIRE(name, crossed == false, "expected false");
	PASS(name);
}

// ── B9: pbc_crossed_boundary raises when track_images = false ─────────────────

static void test_B9_crossed_boundary_no_tracking() {
	const char* name = "B9 — pbc_crossed_boundary raises with track_images=false";
	PBCSection cfg = make_pbc_cfg(false);
	std::vector<ImageCount> curr  = {{0, 0, 0}};
	std::vector<ImageCount> prev  = {{0, 0, 0}};
	REQUIRE_THROWS(name,
		pbc_crossed_boundary(1, cfg, curr, prev),
		std::runtime_error,
		"track_images");
	PASS(name);
}

// ── B10: pbc_image_count returns correct (ix, iy, iz) ────────────────────────

static void test_B10_image_count_values() {
	const char* name = "B10 — pbc_image_count returns correct image indices";
	PBCSection cfg = make_pbc_cfg(true);
	std::vector<ImageCount> images = {{3, -1, 2}};
	ImageCount ic = pbc_image_count(1, cfg, images);
	REQUIRE(name, ic.ix ==  3, "ix");
	REQUIRE(name, ic.iy == -1, "iy");
	REQUIRE(name, ic.iz ==  2, "iz");
	PASS(name);
}

// ── B11: pbc_image_count raises when track_images = false ────────────────────

static void test_B11_image_count_no_tracking() {
	const char* name = "B11 — pbc_image_count raises with track_images=false";
	PBCSection cfg = make_pbc_cfg(false);
	std::vector<ImageCount> images = {{0, 0, 0}};
	REQUIRE_THROWS(name,
		pbc_image_count(1, cfg, images),
		std::runtime_error,
		"track_images");
	PASS(name);
}

// ── B12: pbc_unwrap reconstructs continuous position ─────────────────────────

static void test_B12_unwrap_position() {
	const char* name = "B12 — pbc_unwrap returns r_wrapped + img * L";
	PBCSection cfg = make_pbc_cfg(true);
	PeriodicCell cell = make_cell(10.0, 10.0, 10.0);

	// Particle wrapped to (0.5, 5.0, 5.0), has crossed x-boundary 2 times
	std::vector<Vec3>       positions = {{0.5, 5.0, 5.0}};
	std::vector<ImageCount> images    = {{2, 0, 0}};

	Vec3 ru = pbc_unwrap(1, cfg, cell, positions, images);
	// Unwrapped = wrapped + ix*Lx, iy*Ly, iz*Lz
	REQUIRE_NEAR(name, ru.x, 20.5, 1e-12, "x unwrapped");
	REQUIRE_NEAR(name, ru.y,  5.0, 1e-12, "y unchanged");
	REQUIRE_NEAR(name, ru.z,  5.0, 1e-12, "z unchanged");
	PASS(name);
}

// ── B13: Particle ID 0 raises (1-indexed) ────────────────────────────────────

static void test_B13_particle_id_zero() {
	const char* name = "B13 — particle ID 0 raises (1-indexed protocol)";
	PBCSection cfg = make_pbc_cfg(true);
	std::vector<ImageCount> images = {{0, 0, 0}};
	REQUIRE_THROWS(name,
		pbc_image_count(0, cfg, images),
		std::out_of_range,
		"1-indexed");
	PASS(name);
}

// ── B14: Particle ID out of range raises ─────────────────────────────────────

static void test_B14_particle_id_out_of_range() {
	const char* name = "B14 — particle ID out of range raises";
	PBCSection cfg = make_pbc_cfg(true);
	// One particle in system; request ID 2
	std::vector<ImageCount> images = {{0, 0, 0}};
	REQUIRE_THROWS(name,
		pbc_image_count(2, cfg, images),
		std::out_of_range,
		"out of range");
	PASS(name);
}

// ── main ──────────────────────────────────────────────────────────────────────

int main() {
	std::printf(
		"\n"
		"╔══════════════════════════════════════════════════════════════╗\n"
		"║   test_pbc_bindings — WO-VSEPR-SIM-57C gate               ║\n"
		"║   pbc.* binding correctness + error semantics              ║\n"
		"╚══════════════════════════════════════════════════════════════╝\n"
		"\n"
	);

	test_B1_wrap_positive();
	test_B2_wrap_negative();
	test_B3_wrap_no_cell();
	test_B4_delta_minimum_image();
	test_B5_delta_nonperiodic_axis();
	test_B6_distance_equals_norm_delta();
	test_B7_crossed_boundary_true();
	test_B8_crossed_boundary_false();
	test_B9_crossed_boundary_no_tracking();
	test_B10_image_count_values();
	test_B11_image_count_no_tracking();
	test_B12_unwrap_position();
	test_B13_particle_id_zero();
	test_B14_particle_id_out_of_range();

	std::printf("\nResults: %d passed, %d failed\n\n", g_pass, g_fail);

	if (g_fail == 0) {
		std::printf(
			"╔══════════════════════════════════════════════════════════════╗\n"
			"║  ALL TESTS PASS — WO-57C binding gate: CLEAR              ║\n"
			"╚══════════════════════════════════════════════════════════════╝\n\n"
		);
		return 0;
	} else {
		std::fprintf(stderr,
			"╔══════════════════════════════════════════════════════════════╗\n"
			"║  FAILURES DETECTED — see [FAIL] lines above               ║\n"
			"╚══════════════════════════════════════════════════════════════╝\n\n"
		);
		return 1;
	}
}
