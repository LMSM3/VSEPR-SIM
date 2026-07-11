/**
 * test_vsim_value_xyzvec3.cpp — VsimValue XYZVec3 / Int3 type tests
 * ==================================================================
 *
 * WO-VSEPR-SIM-57D gate — value layer correctness.
 *
 * Tests:
 *   V1  XYZVec3 round-trip through Value variant
 *   V2  Int3 round-trip through Value variant
 *   V3  Wrong type raises VsimRuntimeError (double → as_xyz_vec3)
 *   V4  Wrong type raises VsimRuntimeError (double → as_int3)
 *   V5  XYZVec3 to_string format
 *   V6  Int3 to_string format
 *   V7  to_pbc_vec3 / from_pbc_vec3 round-trip (bridge helpers)
 *   V8  value_is_xyz / value_is_int3 type predicates
 *
 * Style: standalone; matches 57B/57C test conventions.
 * Day #57D  |  WO-57D  |  beta-8 gate
 */

#include "vsim/vsim_value.hpp"
#include "xyz/xyz_vec3.hpp"

#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>

using namespace vsim;

// ── Test infrastructure ───────────────────────────────────────────────────────

static int g_pass = 0, g_fail = 0;

static void PASS(const char* name) { std::printf("  [PASS] %s\n", name); ++g_pass; }
static void FAIL(const char* name, const char* reason) {
	std::fprintf(stderr, "  [FAIL] %s — %s\n", name, reason); ++g_fail;
}

#define REQUIRE(name, cond, msg) \
	do { if (!(cond)) { FAIL(name, msg); return; } } while(0)
#define REQUIRE_NEAR(name, got, expected, tol, msg) \
	do { if (std::abs((double)(got)-(double)(expected)) > (tol)) { \
		char _b[256]; std::snprintf(_b, sizeof(_b), "%s: got %.15g expected %.15g", msg, (double)(got), (double)(expected)); \
		FAIL(name, _b); return; } } while(0)
#define REQUIRE_THROWS_RT(name, expr) \
	do { bool _thrown = false; \
		try { expr; } catch (const VsimRuntimeError&) { _thrown = true; } catch (...) {} \
		if (!_thrown) { FAIL(name, "expected VsimRuntimeError not thrown"); return; } \
	} while(0)

// ── V1: XYZVec3 round-trip ────────────────────────────────────────────────────

static void test_V1_xyz_roundtrip() {
	const char* name = "V1 — XYZVec3 round-trip through Value";
	Value val = XYZVec3{1.0, 2.0, 3.0};
	REQUIRE(name, value_is_xyz(val), "value_is_xyz should be true");
	XYZVec3 out = as_xyz_vec3(val);
	REQUIRE_NEAR(name, out.x, 1.0, 1e-15, "x");
	REQUIRE_NEAR(name, out.y, 2.0, 1e-15, "y");
	REQUIRE_NEAR(name, out.z, 3.0, 1e-15, "z");
	PASS(name);
}

// ── V2: Int3 round-trip ───────────────────────────────────────────────────────

static void test_V2_int3_roundtrip() {
	const char* name = "V2 — Int3 round-trip through Value";
	Value val = Int3{1, -2, 3};
	REQUIRE(name, value_is_int3(val), "value_is_int3 should be true");
	Int3 out = as_int3(val);
	REQUIRE(name, out.x ==  1, "x");
	REQUIRE(name, out.y == -2, "y");
	REQUIRE(name, out.z ==  3, "z");
	PASS(name);
}

// ── V3: Wrong type → VsimRuntimeError (XYZVec3) ──────────────────────────────

static void test_V3_wrong_type_xyz() {
	const char* name = "V3 — as_xyz_vec3 raises on wrong type";
	Value val = 3.14;
	REQUIRE_THROWS_RT(name, as_xyz_vec3(val));
	PASS(name);
}

// ── V4: Wrong type → VsimRuntimeError (Int3) ─────────────────────────────────

static void test_V4_wrong_type_int3() {
	const char* name = "V4 — as_int3 raises on wrong type";
	Value val = 3.14;
	REQUIRE_THROWS_RT(name, as_int3(val));
	PASS(name);
}

// ── V5: XYZVec3 to_string ────────────────────────────────────────────────────

static void test_V5_xyz_to_string() {
	const char* name = "V5 — XYZVec3 to_string contains XYZVec3 prefix";
	Value val = XYZVec3{1.0, 2.0, 3.0};
	std::string s = to_string(val);
	REQUIRE(name, s.find("XYZVec3") != std::string::npos, "missing 'XYZVec3' prefix");
	PASS(name);
}

// ── V6: Int3 to_string ───────────────────────────────────────────────────────

static void test_V6_int3_to_string() {
	const char* name = "V6 — Int3 to_string contains Int3 prefix";
	Value val = Int3{1, -2, 3};
	std::string s = to_string(val);
	REQUIRE(name, s.find("Int3") != std::string::npos, "missing 'Int3' prefix");
	PASS(name);
}

// ── V7: Bridge helper round-trip ─────────────────────────────────────────────

static void test_V7_bridge_roundtrip() {
	const char* name = "V7 — to_pbc_vec3 / from_pbc_vec3 round-trip";
	XYZVec3 orig{4.5, 6.7, 8.9};
	vsepr::Vec3 pbc = to_pbc_vec3(orig);
	XYZVec3 back = from_pbc_vec3(pbc);
	REQUIRE_NEAR(name, back.x, orig.x, 1e-15, "x");
	REQUIRE_NEAR(name, back.y, orig.y, 1e-15, "y");
	REQUIRE_NEAR(name, back.z, orig.z, 1e-15, "z");
	PASS(name);
}

// ── V8: Type predicate sanity ─────────────────────────────────────────────────

static void test_V8_type_predicates() {
	const char* name = "V8 — type predicates value_is_xyz / value_is_int3";
	Value vxyz = XYZVec3{0, 0, 0};
	Value vi3  = Int3{0, 0, 0};
	Value vd   = 1.0;

	REQUIRE(name,  value_is_xyz(vxyz),  "vxyz should be xyz");
	REQUIRE(name, !value_is_int3(vxyz), "vxyz should not be int3");
	REQUIRE(name,  value_is_int3(vi3),  "vi3 should be int3");
	REQUIRE(name, !value_is_xyz(vi3),   "vi3 should not be xyz");
	REQUIRE(name, !value_is_xyz(vd),    "double should not be xyz");
	REQUIRE(name, !value_is_int3(vd),   "double should not be int3");
	PASS(name);
}

// ── main ──────────────────────────────────────────────────────────────────────

int main() {
	std::printf(
		"\n"
		"╔══════════════════════════════════════════════════════════════╗\n"
		"║   test_vsim_value_xyzvec3 — WO-VSEPR-SIM-57D gate         ║\n"
		"║   VsimValue XYZVec3 / Int3 type layer                     ║\n"
		"╚══════════════════════════════════════════════════════════════╝\n\n"
	);

	test_V1_xyz_roundtrip();
	test_V2_int3_roundtrip();
	test_V3_wrong_type_xyz();
	test_V4_wrong_type_int3();
	test_V5_xyz_to_string();
	test_V6_int3_to_string();
	test_V7_bridge_roundtrip();
	test_V8_type_predicates();

	std::printf("\nResults: %d passed, %d failed\n\n", g_pass, g_fail);
	if (g_fail == 0) {
		std::printf(
			"╔══════════════════════════════════════════════════════════════╗\n"
			"║  ALL TESTS PASS — WO-57D value gate: CLEAR               ║\n"
			"╚══════════════════════════════════════════════════════════════╝\n\n");
		return 0;
	}
	std::fprintf(stderr,
		"╔══════════════════════════════════════════════════════════════╗\n"
		"║  FAILURES DETECTED — see [FAIL] lines above               ║\n"
		"╚══════════════════════════════════════════════════════════════╝\n\n");
	return 1;
}
