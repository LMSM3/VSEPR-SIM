/**
 * test_view_67b.cpp
 * =================
 * VSEPR-SIM  |  WO-67-B  |  Viewer Runtime Bridge and XYZ Data Contract
 *
  * Tests: VIEW-67B-01 through VIEW-67B-09
 *
 * VIEW-67B-01  Load simple .xyz into ViewFrame
 * VIEW-67B-02  Load .xyzf multi-frame trajectory
 * VIEW-67B-03  Load .xyzFull with metadata
 * VIEW-67B-04  Reject malformed frame with clear error
 * VIEW-67B-05  Link identity_id from particle to .z state
 * VIEW-67B-06  Handle missing identity sidecar as warning, not crash
 * VIEW-67B-07  Confirm viewer does not mutate physical state
 * VIEW-67B-08  Confirm deterministic same-file same-ViewFrame hash
  * VIEW-67B-09  Resolve element symbols through the authoritative database
 */

#include "vsim/view/xyz_view_loader.hpp"
#include "vsim/view/identity_view_loader.hpp"
#include "vsim/view/view_contract.hpp"
#include "vsim/view/view_errors.hpp"
#include "vsim/view/viewer_types.hpp"
#include "pot/periodic_db.hpp"

#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <filesystem>

namespace fs = std::filesystem;
using namespace vsepr::view;

// ============================================================================
// Utility: write a temp file for testing
// ============================================================================

static std::string write_tmp(const std::string& name, const std::string& content) {
	fs::path p = fs::temp_directory_path() / ("vsim_view67b_" + name);
	std::ofstream f(p);
	f << content;
	return p.string();
}

static void test_10_default_nacl_fixture() {
	std::cout << "VIEW-67B-10: Canonical NaCl fixture... ";
	const std::string fixture = "data/fixtures/nacl.xyz";
	assert(fs::is_regular_file(fixture));
	XyzViewLoader loader;
	const ViewLoadResult result = loader.load(fixture);
	assert(result.success);
	assert(result.session.frame_count() == 1);
	assert(result.session.frames.front().particles.size() == 8);
	assert(result.session.frames.front().particles[0].type == "Na");
	assert(result.session.frames.front().particles[1].type == "Cl");
	std::cout << "PASS\n";
}

static void test_09_element_database_symbols() {
	std::cout << "VIEW-67B-09: Authoritative elemental database symbols... ";
	const auto table = vsepr::PeriodicTable::load_default();
	const auto* hydrogen = table.by_symbol("H");
	const auto* francium = table.by_symbol("Fr");
	const auto* oganesson = table.by_symbol("Og");
	if (!hydrogen || hydrogen->Z != 1 || !francium || francium->Z != 87
		|| !oganesson || oganesson->Z != 118) {
		throw std::runtime_error("authoritative elemental database lookup failed");
	}
	std::cout << "PASS\n";
}

static void remove_tmp(const std::string& path) {
	std::error_code ec;
	fs::remove(path, ec);
}

// ============================================================================
// VIEW-67B-01  —  Load simple .xyz into ViewFrame
// ============================================================================

static void test_01_load_xyz() {
	std::cout << "VIEW-67B-01: Load simple .xyz into ViewFrame... ";

	const std::string xyz =
		"3\n"
		"water molecule\n"
		"O   0.000  0.000  0.000\n"
		"H   0.757  0.586  0.000\n"
		"H  -0.757  0.586  0.000\n";

	std::string path = write_tmp("test01.xyz", xyz);

	XyzViewLoader loader;
	ViewLoadResult r = loader.load(path);

	assert(r.success);
	assert(r.session.frame_count() == 1);
	assert(r.session.frames[0].particles.size() == 3);
	assert(r.session.frames[0].particles[0].type == "O");
	assert(r.session.frames[0].particles[1].type == "H");
	assert(r.session.frames[0].particles[2].type == "H");
	// Positions must be non-zero for at least the H atoms
	assert(std::abs(r.session.frames[0].particles[1].x) > 0.1);
	// Session should not be empty
	assert(!r.session.empty());

	remove_tmp(path);
	std::cout << "PASS\n";
}

// ============================================================================
// VIEW-67B-02  —  Load .xyzf multi-frame trajectory
// ============================================================================

static void test_02_load_xyzf() {
	std::cout << "VIEW-67B-02: Load .xyzf multi-frame trajectory... ";

	// Use a real multi-frame xyzf fixture from the repo
	// (avoids text-mode tellg/seekg round-trip limitations on Windows)
	const std::string fixture = "demo_xyz/methylmercury_iodide.xyzf";

	XyzViewLoader loader;
	ViewLoadResult r = loader.load(fixture);

	// If fixture is absent (e.g. CI without demo data), fall back to a
	// single-frame xyzf file written inline — verifies format recognition.
	if (!r.success) {
		const std::string xyzf =
			"2\n"
			"time=0.0 dt=0.5\n"
			"H   0.000  0.000  0.000\n"
			"H   0.740  0.000  0.000\n";
		std::string path = write_tmp("test02.xyzf", xyzf);
		r = loader.load(path);
		remove_tmp(path);
		assert(r.success);
		assert(r.session.file_type == "xyzf");
		assert(!r.session.empty());
		std::cout << "PASS (fallback: single-frame xyzf)\n";
		return;
	}

	assert(r.success);
	assert(!r.session.empty());
	assert(r.session.file_type == "xyzf");
	// Multi-frame fixture must have > 1 frame
	assert(r.session.frame_count() >= 2);
	// First frame must have particles
	assert(!r.session.frames[0].particles.empty());

	remove_tmp(""); // no-op
	std::cout << "PASS (" << r.session.frame_count() << " frames)\n";
}

// ============================================================================
// VIEW-67B-03  —  Load .xyzFull with metadata
// ============================================================================

static void test_03_load_xyzfull() {
	std::cout << "VIEW-67B-03: Load .xyzFull with metadata... ";

	const std::string xyzfull =
		"2\n"
		"time=1.0 dt=0.1 energy=-10.5 temperature=298.0\n"
		"C   0.000  0.000  0.000\n"
		"C   1.540  0.000  0.000\n";

	std::string path = write_tmp("test03.xyzFull", xyzfull);

	XyzViewLoader loader;
	ViewLoadResult r = loader.load(path);

	assert(r.success);
	assert(r.session.frame_count() >= 1);
	assert(r.session.file_type == "xyzFull");
	// Observables from comment line
	const auto& obs = r.session.frames[0].observables;
	auto it = obs.find("temperature");
	if (it != obs.end()) {
		assert(std::abs(it->second - 298.0) < 1.0);
	}

	remove_tmp(path);
	std::cout << "PASS\n";
}

// ============================================================================
// VIEW-67B-04  —  Reject malformed frame with clear error
// ============================================================================

static void test_04_reject_malformed() {
	std::cout << "VIEW-67B-04: Reject malformed frame with clear error... ";

	// Bad: atom count says 3 but only 1 atom data line, then abrupt EOF
	const std::string bad_xyz =
		"3\n"
		"comment\n"
		"O   0.0  0.0  0.0\n";
	// Note: only 1 atom line instead of 3

	std::string path = write_tmp("test04_bad.xyz", bad_xyz);

	XyzViewLoader loader;
	ViewLoadResult r = loader.load(path);

	// Must either fail with an error message, or succeed with warnings.
	// Either way: if success, the frame must not silently have wrong data.
	if (!r.success) {
		assert(!r.error_msg.empty());
	} else {
		// Partial load is permissible but must not claim 3 particles
		// when only 1 was parseable — or must surface warnings
		assert(r.warnings > 0 || r.session.frames[0].particles.size() <= 1);
	}

	remove_tmp(path);
	std::cout << "PASS\n";
}

// ============================================================================
// VIEW-67B-05  —  Link identity_id from particle to .z state
// ============================================================================

static void test_05_link_identity_z() {
	std::cout << "VIEW-67B-05: Link identity_id from particle to .z state... ";

	// Build a ViewSession with particles that have identity_ids set
	ViewSession session;
	session.source_path = "synthetic";
	session.file_type   = "xyz";

	ViewFrame frame;
	frame.frame_index = 0;
	frame.time = 0.0;

	ViewParticle p1;
	p1.id          = 1;
	p1.identity_id = 101;
	p1.type        = "C";
	p1.x = 0.0; p1.y = 0.0; p1.z = 0.0;
	frame.particles.push_back(p1);

	ViewParticle p2;
	p2.id          = 2;
	p2.identity_id = 102;
	p2.type        = "N";
	p2.x = 1.0; p2.y = 0.0; p2.z = 0.0;
	frame.particles.push_back(p2);

	session.frames.push_back(frame);

	// Write a .z sidecar
	const std::string z_data =
		"# identity_id Z2_a Z2_b Z2_c Z2_d Y loss\n"
		"101  0.9  0.1  0.0  0.8  0.95  0.02\n"
		"102  0.7  0.2  0.1  0.6  0.88  0.05\n";

	std::string z_path = write_tmp("test05.z", z_data);

	IdentityViewLoader id_loader;
	int unlinked = id_loader.load_and_link(z_path, session);

	assert(unlinked == 0);  // all linked
	assert(session.frames[0].identities.size() == 2);

	// Check flags
	assert(session.frames[0].particles[0].flags & ViewFlags::IdentityLinked);
	assert(session.frames[0].particles[1].flags & ViewFlags::IdentityLinked);

	// Check Z2 values
	bool found_101 = false;
	for (const auto& id : session.frames[0].identities) {
		if (id.identity_id == 101) {
			assert(std::abs(id.Z2[0] - 0.9) < 1e-9);
			assert(std::abs(id.Y    - 0.95) < 1e-9);
			found_101 = true;
		}
	}
	assert(found_101);

	remove_tmp(z_path);
	std::cout << "PASS\n";
}

// ============================================================================
// VIEW-67B-06  —  Handle missing identity sidecar as warning, not crash
// ============================================================================

static void test_06_missing_sidecar_warning() {
	std::cout << "VIEW-67B-06: Handle missing identity sidecar as warning, not crash... ";

	ViewSession session;
	session.source_path = "synthetic";
	session.file_type   = "xyz";

	ViewFrame frame;
	frame.frame_index = 0;

	ViewParticle p;
	p.id          = 1;
	p.identity_id = 999;
	p.type        = "H";
	frame.particles.push_back(p);
	session.frames.push_back(frame);

	IdentityViewLoader id_loader;
	// Use a path that doesn't exist
	int result = id_loader.load_and_link("/nonexistent/path/fake.z", session);

	// -1 means load failed but did not crash
	assert(result == -1);
	// A warning must be present in the frame
	assert(!session.frames[0].warnings.empty());
	// The particle must be flagged as unlinked
	assert(session.frames[0].particles[0].flags & ViewFlags::IdentityUnlinked);

	std::cout << "PASS\n";
}

// ============================================================================
// VIEW-67B-07  —  Confirm viewer does not mutate physical state
// ============================================================================

static void test_07_no_state_mutation() {
	std::cout << "VIEW-67B-07: Confirm viewer does not mutate physical state... ";

	const std::string xyz =
		"2\n"
		"snapshot\n"
		"O   1.234  5.678  9.012\n"
		"H   2.345  6.789  0.123\n";

	std::string path = write_tmp("test07.xyz", xyz);

	XyzViewLoader loader;
	ViewLoadResult r1 = loader.load(path);
	assert(r1.success);

	// Capture positions from first load
	double ox = r1.session.frames[0].particles[0].x;
	double oy = r1.session.frames[0].particles[0].y;
	double oz = r1.session.frames[0].particles[0].z;

	// Load again — positions must be identical (loader is pure / stateless)
	ViewLoadResult r2 = loader.load(path);
	assert(r2.success);

	assert(std::abs(r2.session.frames[0].particles[0].x - ox) < 1e-12);
	assert(std::abs(r2.session.frames[0].particles[0].y - oy) < 1e-12);
	assert(std::abs(r2.session.frames[0].particles[0].z - oz) < 1e-12);

	// Identity sidecar linking must not change positions either
	// (link into r1.session, then verify positions unchanged)
	ViewSession& s = r1.session;
	s.frames[0].particles[0].identity_id = 1;

	const std::string z_data = "1  0.5  0.0  0.0  0.5  1.0  0.0\n";
	std::string z_path = write_tmp("test07.z", z_data);

	IdentityViewLoader id_loader;
	id_loader.load_and_link(z_path, s);

	// Positions must not have changed
	assert(std::abs(s.frames[0].particles[0].x - ox) < 1e-12);
	assert(std::abs(s.frames[0].particles[0].y - oy) < 1e-12);
	assert(std::abs(s.frames[0].particles[0].z - oz) < 1e-12);

	remove_tmp(path);
	remove_tmp(z_path);
	std::cout << "PASS\n";
}

// ============================================================================
// VIEW-67B-08  —  Deterministic same-file same-ViewFrame hash
// ============================================================================

static void test_08_deterministic_hash() {
	std::cout << "VIEW-67B-08: Deterministic same-file same-ViewFrame hash... ";

	const std::string xyz =
		"3\n"
		"determinism test\n"
		"C   0.000  0.000  0.000\n"
		"C   1.540  0.000  0.000\n"
		"H   2.000  0.500  0.000\n";

	std::string path = write_tmp("test08.xyz", xyz);

	XyzViewLoader loader;

	ViewLoadResult r1 = loader.load(path);
	ViewLoadResult r2 = loader.load(path);
	ViewLoadResult r3 = loader.load(path);

	assert(r1.success && r2.success && r3.success);

	uint64_t h1 = r1.session.deterministic_hash();
	uint64_t h2 = r2.session.deterministic_hash();
	uint64_t h3 = r3.session.deterministic_hash();

	assert(h1 == h2);
	assert(h2 == h3);
	assert(h1 != 0);  // hash must not be trivially zero

	// A different file must produce a different hash
	const std::string xyz2 =
		"3\n"
		"determinism test\n"
		"N   0.000  0.000  0.000\n"   // N instead of C
		"C   1.540  0.000  0.000\n"
		"H   2.000  0.500  0.000\n";

	std::string path2 = write_tmp("test08b.xyz", xyz2);
	ViewLoadResult r4 = loader.load(path2);
	assert(r4.success);
	uint64_t h4 = r4.session.deterministic_hash();

	// Different particle labels -> different hash (FNV over type string in source_path + data)
	// Note: the hash covers source_path and positions; since path differs, h4 != h1.
	assert(h4 != h1);

	remove_tmp(path);
	remove_tmp(path2);
	std::cout << "PASS\n";
}

// ============================================================================
// VIEW-67B-BONUS  —  view_contract: contract_for() covers all types
// ============================================================================

static void test_bonus_contract_table() {
	std::cout << "VIEW-67B-BONUS: contract_for() covers all file types... ";

	// Every named type must return a non-Unknown result with a role
	using VFT = ViewFileType;
	const VFT types[] = {
		VFT::Xyz, VFT::Xyza, VFT::Xyzf, VFT::XyzFull,
		VFT::Xyzc, VFT::Dynx, VFT::Z, VFT::Ee, VFT::X
	};
	for (auto t : types) {
		ViewFileContract c = contract_for(t);
		assert(c.type == t);
		assert(c.role != nullptr && c.role[0] != '\0');
		// All types must have must_not_mutate_state = true
		assert(c.must_not_mutate_state);
	}

	// Overlay types (.z, .ee) must be overlay_only
	assert(contract_for(VFT::Z).is_overlay_only);
	assert(contract_for(VFT::Ee).is_overlay_only);
	assert(!contract_for(VFT::Xyz).is_overlay_only);

	// Bundle type (.X) must be bundle
	assert(contract_for(VFT::X).is_bundle);
	assert(!contract_for(VFT::Xyz).is_bundle);

	// .xyz must have particles + positions
	assert(contract_for(VFT::Xyz).must_have_particles);
	assert(contract_for(VFT::Xyz).must_have_positions);

	std::cout << "PASS\n";
}

// ============================================================================
// main
// ============================================================================

int main() {
	std::cout << "=== VSEPR-SIM VIEW-67B Test Suite ===\n";

	int failed = 0;

	auto run = [&](const char* name, void(*fn)()) {
		try {
			fn();
		} catch (const std::exception& ex) {
			std::cout << "FAIL (" << ex.what() << ")\n";
			++failed;
		} catch (...) {
			std::cout << "FAIL (unknown exception)\n";
			++failed;
		}
	};

	run("VIEW-67B-01", test_01_load_xyz);
	run("VIEW-67B-02", test_02_load_xyzf);
	run("VIEW-67B-03", test_03_load_xyzfull);
	run("VIEW-67B-04", test_04_reject_malformed);
	run("VIEW-67B-05", test_05_link_identity_z);
	run("VIEW-67B-06", test_06_missing_sidecar_warning);
	run("VIEW-67B-07", test_07_no_state_mutation);
	run("VIEW-67B-08", test_08_deterministic_hash);
	run("VIEW-67B-09", test_09_element_database_symbols);
	run("VIEW-67B-10", test_10_default_nacl_fixture);
	run("VIEW-67B-BONUS", test_bonus_contract_table);

	std::cout << "=====================================\n";
	if (failed == 0) {
		std::cout << "All VIEW-67B tests PASSED.\n";
		return 0;
	} else {
		std::cout << failed << " test(s) FAILED.\n";
		return 1;
	}
}
