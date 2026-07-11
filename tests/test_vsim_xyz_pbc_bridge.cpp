/**
 * test_vsim_xyz_pbc_bridge.cpp — xyz comment-line → runtime → pbc bridge tests
 * ===============================================================================
 *
 * WO-VSEPR-SIM-57D gate — xyz/PBC bridge integration.
 *
 * Tests:
 *   B1  parse_xyz_comment_cell parses cell= key correctly
 *   B2  parse_xyz_comment_cell parses boundary= "p p p" correctly
 *   B3  boundary= "o o o" maps all axes to Open
 *   B4  Mixed boundary "p p o" maps correctly
 *   B5  load_xyz_frame_with_pbc builds correct PBCInterpreterRuntime
 *   B6  Script blocks [cell]/[boundary] take precedence over xyz comment
 *   B7  pbc.distance via interpreter on xyz-loaded particle positions
 *   B8  xyz comment cell= initializes runtime.cell.lengths correctly
 *
 * Style: standalone; matches 57B/57C/57D conventions.
 * Day #57D  |  WO-57D  |  beta-8 gate
 */

#include "vsim/vsim_interpreter.hpp"
#include "box/pbc.hpp"

#include <cmath>
#include <cstdio>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

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

// ── XYZ comment-line PBC parser ───────────────────────────────────────────────
// Parses keys of the form:
//   cell="Lx Ly Lz"         or  cell="Lx Ly Lz"
//   boundary="p p p"        or  boundary="o o o"
//   units="angstrom"
//
// Boundary tokens: p / periodic → Periodic;  o / open → Open

struct XYZCommentMeta {
	std::optional<std::array<double, 3>> cell_lengths;
	std::optional<std::array<BoundaryMode, 3>> boundary;
	std::string units = "angstrom";
};

static BoundaryMode parse_boundary_token(const std::string& tok) {
	if (tok == "p" || tok == "periodic") return BoundaryMode::Periodic;
	if (tok == "o" || tok == "open")     return BoundaryMode::Open;
	return BoundaryMode::Open;  // default
}

static std::string extract_quoted(const std::string& s, const std::string& key) {
	auto pos = s.find(key + "=");
	if (pos == std::string::npos) return {};
	pos += key.size() + 1;  // skip "="
	if (pos >= s.size()) return {};
	bool quoted = (s[pos] == '"');
	if (quoted) ++pos;
	auto end = quoted ? s.find('"', pos) : s.find(' ', pos);
	if (end == std::string::npos) end = s.size();
	return s.substr(pos, end - pos);
}

static XYZCommentMeta parse_xyz_comment_cell(const std::string& comment) {
	XYZCommentMeta meta;

	std::string cell_str = extract_quoted(comment, "cell");
	if (!cell_str.empty()) {
		std::istringstream ss(cell_str);
		double lx, ly, lz;
		if (ss >> lx >> ly >> lz) {
			meta.cell_lengths = {lx, ly, lz};
		}
	}

	std::string bnd_str = extract_quoted(comment, "boundary");
	if (!bnd_str.empty()) {
		std::istringstream ss(bnd_str);
		std::string tx, ty, tz;
		if (ss >> tx >> ty >> tz) {
			meta.boundary = {
				parse_boundary_token(tx),
				parse_boundary_token(ty),
				parse_boundary_token(tz)
			};
		}
	}

	std::string units_str = extract_quoted(comment, "units");
	if (!units_str.empty()) meta.units = units_str;

	return meta;
}

// ── load_xyz_frame_with_pbc ───────────────────────────────────────────────────
// Parses a minimal in-memory xyz string (N / comment / atom lines) and builds
// a PBCInterpreterRuntime. Caller may override cell/boundary after loading.
//
// XYZ format expected:
//   N_atoms
//   comment line  (may contain cell= / boundary= keys)
//   Symbol x y z
//   ...

static PBCInterpreterRuntime load_xyz_frame_with_pbc(const std::string& xyz_text) {
	PBCInterpreterRuntime rt;
	rt.pbc_config.track_images = true;

	std::istringstream ss(xyz_text);
	std::string line;

	// Line 1: atom count
	int n_atoms = 0;
	if (!std::getline(ss, line))
		throw std::runtime_error("load_xyz: missing atom count line");
	try { n_atoms = std::stoi(line); }
	catch (...) { throw std::runtime_error("load_xyz: invalid atom count"); }

	// Line 2: comment
	std::string comment;
	if (!std::getline(ss, comment))
		throw std::runtime_error("load_xyz: missing comment line");

	XYZCommentMeta meta = parse_xyz_comment_cell(comment);

	if (meta.cell_lengths) {
		rt.cell.lengths    = {(*meta.cell_lengths)[0],
							  (*meta.cell_lengths)[1],
							  (*meta.cell_lengths)[2]};
		rt.cell.periodic_x = true;
		rt.cell.periodic_y = true;
		rt.cell.periodic_z = true;
	}
	if (meta.boundary) {
		rt.cell.periodic_x = ((*meta.boundary)[0] == BoundaryMode::Periodic);
		rt.cell.periodic_y = ((*meta.boundary)[1] == BoundaryMode::Periodic);
		rt.cell.periodic_z = ((*meta.boundary)[2] == BoundaryMode::Periodic);
	}

	// Remaining lines: atoms
	for (int i = 0; i < n_atoms; ++i) {
		if (!std::getline(ss, line))
			throw std::runtime_error("load_xyz: fewer atom lines than declared");
		std::istringstream al(line);
		std::string sym;
		double x, y, z;
		if (!(al >> sym >> x >> y >> z))
			throw std::runtime_error("load_xyz: malformed atom line: " + line);
		rt.particles.push_back({{x, y, z}, sym, 0});
		rt.image_counts.push_back({0, 0, 0});
		rt.image_counts_prev.push_back({0, 0, 0});
	}

	return rt;
}

// ── B1: parse_xyz_comment_cell — cell lengths ─────────────────────────────────

static void test_B1_cell_lengths() {
	const char* name = "B1 — parse_xyz_comment_cell cell= key";
	auto meta = parse_xyz_comment_cell(
		R"(cell="10.0 12.0 8.0" boundary="p p p" units="angstrom")");
	REQUIRE(name, meta.cell_lengths.has_value(), "cell_lengths missing");
	REQUIRE_NEAR(name, (*meta.cell_lengths)[0], 10.0, 1e-12, "Lx");
	REQUIRE_NEAR(name, (*meta.cell_lengths)[1], 12.0, 1e-12, "Ly");
	REQUIRE_NEAR(name, (*meta.cell_lengths)[2],  8.0, 1e-12, "Lz");
	PASS(name);
}

// ── B2: parse_xyz_comment_cell — boundary "p p p" ────────────────────────────

static void test_B2_boundary_periodic() {
	const char* name = "B2 — boundary=\"p p p\" maps all axes to Periodic";
	auto meta = parse_xyz_comment_cell(
		R"(cell="10.0 10.0 10.0" boundary="p p p")");
	REQUIRE(name, meta.boundary.has_value(), "boundary missing");
	REQUIRE(name, (*meta.boundary)[0] == BoundaryMode::Periodic, "x not Periodic");
	REQUIRE(name, (*meta.boundary)[1] == BoundaryMode::Periodic, "y not Periodic");
	REQUIRE(name, (*meta.boundary)[2] == BoundaryMode::Periodic, "z not Periodic");
	PASS(name);
}

// ── B3: parse_xyz_comment_cell — boundary "o o o" ────────────────────────────

static void test_B3_boundary_open() {
	const char* name = "B3 — boundary=\"o o o\" maps all axes to Open";
	auto meta = parse_xyz_comment_cell(
		R"(cell="10.0 10.0 10.0" boundary="o o o")");
	REQUIRE(name, meta.boundary.has_value(), "boundary missing");
	REQUIRE(name, (*meta.boundary)[0] == BoundaryMode::Open, "x not Open");
	REQUIRE(name, (*meta.boundary)[1] == BoundaryMode::Open, "y not Open");
	REQUIRE(name, (*meta.boundary)[2] == BoundaryMode::Open, "z not Open");
	PASS(name);
}

// ── B4: Mixed boundary "p p o" ───────────────────────────────────────────────

static void test_B4_boundary_mixed() {
	const char* name = "B4 — mixed boundary \"p p o\"";
	auto meta = parse_xyz_comment_cell(
		R"(cell="10.0 10.0 10.0" boundary="p p o")");
	REQUIRE(name, meta.boundary.has_value(), "boundary missing");
	REQUIRE(name, (*meta.boundary)[0] == BoundaryMode::Periodic, "x not Periodic");
	REQUIRE(name, (*meta.boundary)[1] == BoundaryMode::Periodic, "y not Periodic");
	REQUIRE(name, (*meta.boundary)[2] == BoundaryMode::Open,     "z not Open");
	PASS(name);
}

// ── B5: load_xyz_frame_with_pbc builds correct runtime ───────────────────────

static void test_B5_load_frame() {
	const char* name = "B5 — load_xyz_frame_with_pbc populates runtime correctly";
	PBCInterpreterRuntime rt = load_xyz_frame_with_pbc(
		"2\n"
		"cell=\"10.0 10.0 10.0\" boundary=\"p p p\" units=\"angstrom\"\n"
		"Ar 9.9 5.0 5.0\n"
		"Ar 0.1 5.0 5.0\n"
	);

	REQUIRE(name, rt.cell.periodic_x, "x not periodic");
	REQUIRE(name, rt.cell.periodic_y, "y not periodic");
	REQUIRE(name, rt.cell.periodic_z, "z not periodic");
	REQUIRE_NEAR(name, rt.cell.lengths.x, 10.0, 1e-12, "Lx");
	REQUIRE(name, rt.particle_count() == 2, "particle count != 2");
	REQUIRE_NEAR(name, rt.particles[0].position.x, 9.9, 1e-12, "p0.x");
	REQUIRE_NEAR(name, rt.particles[1].position.x, 0.1, 1e-12, "p1.x");
	PASS(name);
}

// ── B6: script [cell]/[boundary] takes precedence over xyz comment ────────────

static void test_B6_script_overrides_xyz() {
	const char* name = "B6 — script cell takes precedence over xyz comment meta";
	// Load an xyz with cell=9.0, then override with a larger cell
	PBCInterpreterRuntime rt = load_xyz_frame_with_pbc(
		"1\n"
		"cell=\"9.0 9.0 9.0\" boundary=\"p p p\"\n"
		"Ar 0.0 0.0 0.0\n"
	);

	// Simulate script block override
	rt.cell.lengths = {20.0, 20.0, 20.0};

	REQUIRE_NEAR(name, rt.cell.lengths.x, 20.0, 1e-12,
				 "script override did not take effect");
	PASS(name);
}

// ── B7: pbc.distance on xyz-loaded positions ──────────────────────────────────

static void test_B7_pbc_distance_from_xyz() {
	const char* name = "B7 — pbc.distance via interpreter on xyz-loaded positions";
	PBCInterpreterRuntime rt = load_xyz_frame_with_pbc(
		"2\n"
		"cell=\"10.0 10.0 10.0\" boundary=\"p p p\" units=\"angstrom\"\n"
		"Ar 9.9 5.0 5.0\n"
		"Ar 0.1 5.0 5.0\n"
	);

	VsimInterpreter interp;
	interp.set_runtime(rt);
	register_pbc_builtins(interp);
	register_particle_builtins(interp);

	Value d = interp.eval("pbc.distance(particle.position(1), particle.position(2))");
	REQUIRE(name, value_is_double(d), "result should be double");
	REQUIRE_NEAR(name, as_double(d), 0.2, 1e-12, "pbc distance");
	PASS(name);
}

// ── B8: xyz comment cell= initializes runtime.cell.lengths ───────────────────

static void test_B8_cell_from_xyz_comment() {
	const char* name = "B8 — xyz comment cell= initializes runtime.cell.lengths";
	PBCInterpreterRuntime rt = load_xyz_frame_with_pbc(
		"1\n"
		"cell=\"9.0 9.0 9.0\" boundary=\"p p p\" units=\"angstrom\"\n"
		"Ar 0.0 0.0 0.0\n"
	);

	REQUIRE_NEAR(name, rt.cell.lengths.x, 9.0, 1e-12, "Lx");
	REQUIRE_NEAR(name, rt.cell.lengths.y, 9.0, 1e-12, "Ly");
	REQUIRE_NEAR(name, rt.cell.lengths.z, 9.0, 1e-12, "Lz");
	REQUIRE(name, rt.cell.periodic_x, "x not periodic");
	PASS(name);
}

// ── main ──────────────────────────────────────────────────────────────────────

int main() {
	std::printf(
		"\n"
		"╔══════════════════════════════════════════════════════════════╗\n"
		"║   test_vsim_xyz_pbc_bridge — WO-VSEPR-SIM-57D gate        ║\n"
		"║   xyz comment-line → PBCInterpreterRuntime → pbc.*        ║\n"
		"╚══════════════════════════════════════════════════════════════╝\n\n"
	);

	test_B1_cell_lengths();
	test_B2_boundary_periodic();
	test_B3_boundary_open();
	test_B4_boundary_mixed();
	test_B5_load_frame();
	test_B6_script_overrides_xyz();
	test_B7_pbc_distance_from_xyz();
	test_B8_cell_from_xyz_comment();

	std::printf("\nResults: %d passed, %d failed\n\n", g_pass, g_fail);
	if (g_fail == 0) {
		std::printf(
			"╔══════════════════════════════════════════════════════════════╗\n"
			"║  ALL TESTS PASS — WO-57D bridge gate: CLEAR              ║\n"
			"╚══════════════════════════════════════════════════════════════╝\n\n");
		return 0;
	}
	std::fprintf(stderr,
		"╔══════════════════════════════════════════════════════════════╗\n"
		"║  FAILURES DETECTED — see [FAIL] lines above               ║\n"
		"╚══════════════════════════════════════════════════════════════╝\n\n");
	return 1;
}
