/**
 * test_vsim_interpreter_pbc.cpp — VsimInterpreter pbc.* expression tests
 * =========================================================================
 *
 * WO-VSEPR-SIM-57D gate — interpreter expression evaluation.
 *
 * Tests:
 *   I1  pbc.wrap via eval() expression with xyzvec3 literal
 *   I2  pbc.delta via eval() — minimum-image across boundary
 *   I3  pbc.distance via eval()
 *   I4  pbc.image_count via eval() — returns Int3
 *   I5  pbc.unwrap via eval() — reconstructed continuous position
 *   I6  pbc.crossed_boundary via eval()
 *   I7  Field access: result.x, result.y, result.z on XYZVec3
 *   I8  exec() assigns variable, then eval() resolves it
 *   I9  particle.position(id) returns XYZVec3
 *   I10 Unknown namespace raises VsimRuntimeError
 *   I11 pbc.* with no periodic cell raises configuration error
 *   I12 pbc.image_count with track_images=false raises feature-gate error
 *   I13 particle.position(0) raises range error
 *   I14 particle.position(id > N) raises range error
 *
 * Style: standalone; matches 57B/57C/57D test conventions.
 * Day #57D  |  WO-57D  |  beta-8 gate
 */

#include "vsim/vsim_interpreter.hpp"

#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>

using namespace vsim;
using namespace vsepr;

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
		char _b[256]; std::snprintf(_b, sizeof(_b), "%s: got %.15g expected %.15g", \
			msg, (double)(got), (double)(expected)); FAIL(name, _b); return; } } while(0)
#define REQUIRE_THROWS(name, expr, substr) \
	do { bool _thrown = false; std::string _msg; \
		try { expr; } \
		catch (const VsimRuntimeError& _e) { _thrown = true; _msg = _e.what(); } \
		catch (const std::exception& _e)   { _thrown = true; _msg = _e.what(); } \
		catch (...) { FAIL(name, "unknown exception type"); return; } \
		if (!_thrown) { FAIL(name, "expected exception not thrown"); return; } \
		if (_msg.find(substr) == std::string::npos) { \
			char _b[512]; std::snprintf(_b, sizeof(_b), \
				"expected '%s' not found in: %s", substr, _msg.c_str()); \
			FAIL(name, _b); return; } \
	} while(0)

// ── Helpers ───────────────────────────────────────────────────────────────────

static PBCInterpreterRuntime make_rt(double lx, double ly, double lz,
									 bool px = true, bool py = true, bool pz = true)
{
	PBCInterpreterRuntime rt;
	rt.cell.lengths    = {lx, ly, lz};
	rt.cell.periodic_x = px;
	rt.cell.periodic_y = py;
	rt.cell.periodic_z = pz;
	rt.pbc_config.track_images = true;
	return rt;
}

static PBCInterpreterRuntime make_open_rt() {
	PBCInterpreterRuntime rt;
	rt.pbc_config.track_images = true;
	return rt;
}

static void setup_interp(VsimInterpreter& interp, PBCInterpreterRuntime& rt) {
	interp.set_runtime(rt);
	register_pbc_builtins(interp);
	register_particle_builtins(interp);
}

static void add_particle(PBCInterpreterRuntime& rt, double x, double y, double z,
						  int ix = 0, int iy = 0, int iz = 0)
{
	rt.particles.push_back({{x, y, z}, "Ar", 18});
	rt.image_counts.push_back({ix, iy, iz});
	rt.image_counts_prev.push_back({0, 0, 0});
}

// ── I1: pbc.wrap via eval() ───────────────────────────────────────────────────

static void test_I1_wrap_expr() {
	const char* name = "I1 — pbc.wrap via eval() with xyzvec3 literal";
	PBCInterpreterRuntime rt = make_rt(10, 10, 10);
	VsimInterpreter interp;
	setup_interp(interp, rt);

	Value result = interp.eval("pbc.wrap(xyzvec3(10.2, 5.0, 5.0))");
	REQUIRE(name, value_is_xyz(result), "result should be XYZVec3");
	XYZVec3 v = as_xyz_vec3(result);
	REQUIRE_NEAR(name, v.x, 0.2, 1e-12, "x");
	REQUIRE_NEAR(name, v.y, 5.0, 1e-12, "y");
	REQUIRE_NEAR(name, v.z, 5.0, 1e-12, "z");
	PASS(name);
}

// ── I2: pbc.delta via eval() ──────────────────────────────────────────────────

static void test_I2_delta_expr() {
	const char* name = "I2 — pbc.delta minimum-image across boundary";
	PBCInterpreterRuntime rt = make_rt(10, 10, 10);
	VsimInterpreter interp;
	setup_interp(interp, rt);

	Value result = interp.eval(
		"pbc.delta(xyzvec3(9.9, 5.0, 5.0), xyzvec3(0.1, 5.0, 5.0))");
	REQUIRE(name, value_is_xyz(result), "result should be XYZVec3");
	XYZVec3 dr = as_xyz_vec3(result);
	REQUIRE_NEAR(name, dr.x,  0.2, 1e-12, "x");
	REQUIRE_NEAR(name, dr.y,  0.0, 1e-12, "y");
	REQUIRE_NEAR(name, dr.z,  0.0, 1e-12, "z");
	PASS(name);
}

// ── I3: pbc.distance via eval() ───────────────────────────────────────────────

static void test_I3_distance_expr() {
	const char* name = "I3 — pbc.distance via eval()";
	PBCInterpreterRuntime rt = make_rt(10, 10, 10);
	VsimInterpreter interp;
	setup_interp(interp, rt);

	Value result = interp.eval(
		"pbc.distance(xyzvec3(9.9, 5.0, 5.0), xyzvec3(0.1, 5.0, 5.0))");
	REQUIRE(name, value_is_double(result), "result should be double");
	REQUIRE_NEAR(name, as_double(result), 0.2, 1e-12, "distance");
	PASS(name);
}

// ── I4: pbc.image_count via eval() ────────────────────────────────────────────

static void test_I4_image_count_expr() {
	const char* name = "I4 — pbc.image_count returns Int3";
	PBCInterpreterRuntime rt = make_rt(10, 10, 10);
	add_particle(rt, 0.5, 5.0, 5.0, 3, -1, 2);
	VsimInterpreter interp;
	setup_interp(interp, rt);

	Value result = interp.eval("pbc.image_count(1)");
	REQUIRE(name, value_is_int3(result), "result should be Int3");
	Int3 img = as_int3(result);
	REQUIRE(name, img.x ==  3, "ix");
	REQUIRE(name, img.y == -1, "iy");
	REQUIRE(name, img.z ==  2, "iz");
	PASS(name);
}

// ── I5: pbc.unwrap via eval() ─────────────────────────────────────────────────

static void test_I5_unwrap_expr() {
	const char* name = "I5 — pbc.unwrap reconstructed continuous position";
	PBCInterpreterRuntime rt = make_rt(10, 10, 10);
	// Particle wrapped at 0.5, has crossed x-boundary twice
	add_particle(rt, 0.5, 5.0, 5.0, 2, 0, 0);
	VsimInterpreter interp;
	setup_interp(interp, rt);

	Value result = interp.eval("pbc.unwrap(1)");
	REQUIRE(name, value_is_xyz(result), "result should be XYZVec3");
	XYZVec3 ru = as_xyz_vec3(result);
	REQUIRE_NEAR(name, ru.x, 20.5, 1e-12, "x unwrapped");
	REQUIRE_NEAR(name, ru.y,  5.0, 1e-12, "y");
	REQUIRE_NEAR(name, ru.z,  5.0, 1e-12, "z");
	PASS(name);
}

// ── I6: pbc.crossed_boundary via eval() ──────────────────────────────────────

static void test_I6_crossed_boundary_expr() {
	const char* name = "I6 — pbc.crossed_boundary detects crossing";
	PBCInterpreterRuntime rt = make_rt(10, 10, 10);
	add_particle(rt, 0.1, 5.0, 5.0, 1, 0, 0);
	rt.image_counts_prev[0] = {0, 0, 0};  // had not crossed yet
	VsimInterpreter interp;
	setup_interp(interp, rt);

	Value result = interp.eval("pbc.crossed_boundary(1)");
	REQUIRE(name, value_is_bool(result), "result should be bool");
	REQUIRE(name, as_bool(result) == true, "should have crossed");
	PASS(name);
}

// ── I7: Field access on XYZVec3 result ───────────────────────────────────────

static void test_I7_field_access() {
	const char* name = "I7 — field access .x/.y/.z on XYZVec3 result";
	PBCInterpreterRuntime rt = make_rt(10, 10, 10);
	VsimInterpreter interp;
	setup_interp(interp, rt);

	// Assign a wrapped position to a variable, then access its fields
	interp.exec("r = pbc.wrap(xyzvec3(10.2, 5.0, 5.0))");
	Value vx = interp.eval("r.x");
	Value vy = interp.eval("r.y");
	REQUIRE(name, value_is_double(vx), "r.x should be double");
	REQUIRE(name, value_is_double(vy), "r.y should be double");
	REQUIRE_NEAR(name, as_double(vx), 0.2, 1e-12, "r.x");
	REQUIRE_NEAR(name, as_double(vy), 5.0, 1e-12, "r.y");
	PASS(name);
}

// ── I8: exec() variable assignment ────────────────────────────────────────────

static void test_I8_exec_assign() {
	const char* name = "I8 — exec() assigns variable, eval() resolves it";
	PBCInterpreterRuntime rt = make_rt(10, 10, 10);
	VsimInterpreter interp;
	setup_interp(interp, rt);

	interp.exec(
		"a = xyzvec3(9.9, 5.0, 5.0)\n"
		"b = xyzvec3(0.1, 5.0, 5.0)\n"
		"dr = pbc.delta(a, b)\n"
	);

	Value dr = interp.scope.at("dr");
	REQUIRE(name, value_is_xyz(dr), "dr should be XYZVec3");
	XYZVec3 v = as_xyz_vec3(dr);
	REQUIRE_NEAR(name, v.x, 0.2, 1e-12, "dr.x");
	PASS(name);
}

// ── I9: particle.position(id) ────────────────────────────────────────────────

static void test_I9_particle_position() {
	const char* name = "I9 — particle.position(id) returns XYZVec3";
	PBCInterpreterRuntime rt = make_rt(10, 10, 10);
	add_particle(rt, 3.5, 4.5, 5.5);
	VsimInterpreter interp;
	setup_interp(interp, rt);

	Value result = interp.eval("particle.position(1)");
	REQUIRE(name, value_is_xyz(result), "result should be XYZVec3");
	XYZVec3 p = as_xyz_vec3(result);
	REQUIRE_NEAR(name, p.x, 3.5, 1e-12, "x");
	REQUIRE_NEAR(name, p.y, 4.5, 1e-12, "y");
	REQUIRE_NEAR(name, p.z, 5.5, 1e-12, "z");
	PASS(name);
}

// ── I10: Unknown namespace raises VsimRuntimeError ───────────────────────────

static void test_I10_unknown_namespace() {
	const char* name = "I10 — unknown namespace raises VsimRuntimeError";
	PBCInterpreterRuntime rt;
	VsimInterpreter interp;
	interp.set_runtime(rt);
	register_pbc_builtins(interp);

	REQUIRE_THROWS(name,
		interp.eval("ghost.something(1)"),
		"Unknown namespace");
	PASS(name);
}

// ── I11: pbc.* with no periodic cell ─────────────────────────────────────────

static void test_I11_no_periodic_cell() {
	const char* name = "I11 — pbc.wrap raises with no periodic cell";
	PBCInterpreterRuntime rt = make_open_rt();
	VsimInterpreter interp;
	setup_interp(interp, rt);

	REQUIRE_THROWS(name,
		interp.eval("pbc.wrap(xyzvec3(1.0, 1.0, 1.0))"),
		"no periodic cell");
	PASS(name);
}

// ── I12: pbc.image_count with track_images=false ─────────────────────────────

static void test_I12_no_track_images() {
	const char* name = "I12 — pbc.image_count raises with track_images=false";
	PBCInterpreterRuntime rt = make_rt(10, 10, 10);
	rt.pbc_config.track_images = false;
	add_particle(rt, 5.0, 5.0, 5.0);
	VsimInterpreter interp;
	setup_interp(interp, rt);

	REQUIRE_THROWS(name,
		interp.eval("pbc.image_count(1)"),
		"track_images");
	PASS(name);
}

// ── I13: particle.position(0) raises ─────────────────────────────────────────

static void test_I13_position_id_zero() {
	const char* name = "I13 — particle.position(0) raises range error";
	PBCInterpreterRuntime rt = make_rt(10, 10, 10);
	add_particle(rt, 5.0, 5.0, 5.0);
	VsimInterpreter interp;
	setup_interp(interp, rt);

	REQUIRE_THROWS(name,
		interp.eval("particle.position(0)"),
		"out of range");
	PASS(name);
}

// ── I14: particle.position(id > N) raises ────────────────────────────────────

static void test_I14_position_id_oob() {
	const char* name = "I14 — particle.position(id > N) raises range error";
	PBCInterpreterRuntime rt = make_rt(10, 10, 10);
	add_particle(rt, 5.0, 5.0, 5.0);  // 1 particle
	VsimInterpreter interp;
	setup_interp(interp, rt);

	REQUIRE_THROWS(name,
		interp.eval("particle.position(2)"),
		"out of range");
	PASS(name);
}

// ── main ──────────────────────────────────────────────────────────────────────

int main() {
	std::printf(
		"\n"
		"╔══════════════════════════════════════════════════════════════╗\n"
		"║   test_vsim_interpreter_pbc — WO-VSEPR-SIM-57D gate       ║\n"
		"║   VsimInterpreter pbc.* expression evaluation              ║\n"
		"╚══════════════════════════════════════════════════════════════╝\n\n"
	);

	test_I1_wrap_expr();
	test_I2_delta_expr();
	test_I3_distance_expr();
	test_I4_image_count_expr();
	test_I5_unwrap_expr();
	test_I6_crossed_boundary_expr();
	test_I7_field_access();
	test_I8_exec_assign();
	test_I9_particle_position();
	test_I10_unknown_namespace();
	test_I11_no_periodic_cell();
	test_I12_no_track_images();
	test_I13_position_id_zero();
	test_I14_position_id_oob();

	std::printf("\nResults: %d passed, %d failed\n\n", g_pass, g_fail);
	if (g_fail == 0) {
		std::printf(
			"╔══════════════════════════════════════════════════════════════╗\n"
			"║  ALL TESTS PASS — WO-57D interpreter gate: CLEAR          ║\n"
			"╚══════════════════════════════════════════════════════════════╝\n\n");
		return 0;
	}
	std::fprintf(stderr,
		"╔══════════════════════════════════════════════════════════════╗\n"
		"║  FAILURES DETECTED — see [FAIL] lines above               ║\n"
		"╚══════════════════════════════════════════════════════════════╝\n\n");
	return 1;
}
