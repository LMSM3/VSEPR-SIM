/**
 * tests/test_dynx_v1.cpp
 * ========================
 * Group 57 — Dynx v1 session archive (WO-VSIM-DYNX-V1-A / WO-VSIM-DYNX-V1-B)
 *
 * Acceptance tests:
 *
 *   DYNX-V1-01  DynxWriter::open creates a file
 *   DYNX-V1-02  write_frame emits FRAME / END_FRAME blocks
 *   DYNX-V1-03  close() finalizes and patches frame_count in header
 *   DYNX-V1-04  dynx_inspect reads metadata correctly
 *   DYNX-V1-05  dynx_validate passes on a good file
 *   DYNX-V1-06  dynx_validate detects non-monotonic frame times
 *   DYNX-V1-07  bad/missing file fails clearly in inspect and validate
 */

#include "vsim/io/dynx_writer.hpp"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

// ============================================================================
// Helpers
// ============================================================================

static std::string tmp_path(const char* name) {
	// Place temp files in the system temp dir
	const char* t = std::getenv("TEMP");
	if (!t) t = "/tmp";
	return std::string(t) + "/" + name;
}

static vsim::io::DynxFrame make_frame(int idx, double t_fs,
									   const std::string& sym = "Na",
									   double x = 0.0, double y = 0.0, double z = 0.0) {
	vsim::io::DynxFrame f;
	f.index   = idx;
	f.time_fs = t_fs;
	vsim::io::DynxParticleState p;
	p.symbol = sym;
	p.pos    = {x, y, z};
	f.particles.push_back(p);
	return f;
}

static std::string read_file(const std::string& path) {
	std::ifstream ifs(path);
	std::ostringstream oss;
	oss << ifs.rdbuf();
	return oss.str();
}

// ============================================================================
// Tests
// ============================================================================

static bool test_DYNX_V1_01() {
	std::string path = tmp_path("dynx_test_01.dynx");
	vsim::io::DynxHeader hdr;
	hdr.source_path   = "test.vsim";
	hdr.source_hash   = "abc123";
	hdr.kernel_version= "v5.1.x";
	hdr.frame_interval_fs = 0.5;
	hdr.particle_count = 1;

	vsim::io::DynxWriter w;
	if (!w.open(path, hdr)) {
		std::printf("FAIL DYNX-V1-01: open failed: %s\n", w.error().c_str());
		return false;
	}
	w.close();

	std::ifstream ifs(path);
	if (!ifs.good()) {
		std::puts("FAIL DYNX-V1-01: file not created");
		return false;
	}
	std::puts("PASS DYNX-V1-01: DynxWriter::open creates a file");
	return true;
}

static bool test_DYNX_V1_02() {
	std::string path = tmp_path("dynx_test_02.dynx");
	vsim::io::DynxHeader hdr;
	hdr.source_hash   = "none";
	hdr.kernel_version= "v5.1.x";
	hdr.particle_count = 1;

	vsim::io::DynxWriter w;
	w.open(path, hdr);
	w.write_frame(make_frame(0, 0.0, "Na", 1.1, 2.2, 3.3));
	w.write_frame(make_frame(1, 0.5, "Na", 1.2, 2.2, 3.3));
	w.close();

	std::string content = read_file(path);
	bool has_frame  = content.find("FRAME 0") != std::string::npos;
	bool has_end    = content.find("END_FRAME") != std::string::npos;
	bool has_end_dynx = content.find("#END_DYNX") != std::string::npos;
	if (!has_frame || !has_end || !has_end_dynx) {
		std::puts("FAIL DYNX-V1-02: missing FRAME/END_FRAME/#END_DYNX");
		return false;
	}
	std::puts("PASS DYNX-V1-02: frame blocks present");
	return true;
}

static bool test_DYNX_V1_03() {
	std::string path = tmp_path("dynx_test_03.dynx");
	vsim::io::DynxHeader hdr;
	hdr.source_hash   = "none";
	hdr.kernel_version= "v5.1.x";
	hdr.particle_count = 1;

	vsim::io::DynxWriter w;
	w.open(path, hdr);
	for (int i = 0; i < 5; ++i)
		w.write_frame(make_frame(i, static_cast<double>(i) * 0.5));
	w.close();

	auto r = vsim::io::dynx_inspect(path);
	if (!r.ok) {
		std::printf("FAIL DYNX-V1-03: inspect failed: %s\n", r.error.c_str());
		return false;
	}
	if (r.frame_count != 5) {
		std::printf("FAIL DYNX-V1-03: frame_count=%d expected 5\n", r.frame_count);
		return false;
	}
	std::puts("PASS DYNX-V1-03: close() patches frame_count correctly");
	return true;
}

static bool test_DYNX_V1_04() {
	std::string path = tmp_path("dynx_test_04.dynx");
	vsim::io::DynxHeader hdr;
	hdr.source_path       = "myscript.vsim";
	hdr.source_hash       = "deadbeef01234567";
	hdr.kernel_version    = "v5.1.x-test";
	hdr.frame_interval_fs = 2.0;
	hdr.particle_count    = 3;

	vsim::io::DynxWriter w;
	w.open(path, hdr);
	w.write_frame(make_frame(0, 0.0));
	w.close();

	auto r = vsim::io::dynx_inspect(path);
	if (!r.ok) {
		std::printf("FAIL DYNX-V1-04: inspect failed: %s\n", r.error.c_str());
		return false;
	}
	bool ok = (r.source_path       == "myscript.vsim")
		   && (r.source_hash        == "deadbeef01234567")
		   && (r.kernel_version     == "v5.1.x-test")
		   && (r.particle_count     == 3)
		   && (r.frame_interval_fs  >= 1.99 && r.frame_interval_fs <= 2.01);
	if (!ok) {
		std::printf("FAIL DYNX-V1-04: metadata mismatch\n"
			"  source=%s hash=%s version=%s count=%d interval=%.2f\n",
			r.source_path.c_str(), r.source_hash.c_str(),
			r.kernel_version.c_str(), r.particle_count, r.frame_interval_fs);
		return false;
	}
	std::puts("PASS DYNX-V1-04: dynx_inspect reads metadata correctly");
	return true;
}

static bool test_DYNX_V1_05() {
	std::string path = tmp_path("dynx_test_05.dynx");
	vsim::io::DynxHeader hdr;
	hdr.source_hash    = "cafebabe";
	hdr.kernel_version = "v5.1.x";
	hdr.particle_count = 1;

	vsim::io::DynxWriter w;
	w.open(path, hdr);
	for (int i = 0; i < 4; ++i)
		w.write_frame(make_frame(i, static_cast<double>(i) * 1.0));
	w.close();

	auto r = vsim::io::dynx_validate(path);
	if (!r.ok) {
		std::printf("FAIL DYNX-V1-05: validate failed: %s\n", r.error.c_str());
		for (const auto& w : r.warnings)
			std::printf("  WARN: %s\n", w.c_str());
		return false;
	}
	if (!r.monotonic_time) {
		std::puts("FAIL DYNX-V1-05: monotonic_time is false on good file");
		return false;
	}
	std::puts("PASS DYNX-V1-05: dynx_validate passes on good file");
	return true;
}

static bool test_DYNX_V1_06() {
	// Write a dynx file with non-monotonic frame times manually
	std::string path = tmp_path("dynx_test_06.dynx");
	{
		std::ofstream ofs(path);
		ofs << "#dynx v1\n";
		ofs << "#source none\n";
		ofs << "#source_hash none\n";
		ofs << "#kernel_version v5.1.x\n";
		ofs << "#frame_count                                   3\n";
		ofs << "#frame_interval 1.000000\n";
		ofs << "#particle_count 1\n";
		ofs << "#timestamp 2025-01-01T00:00:00Z\n";
		ofs << "FRAME 0 0.000000\n";
		ofs << "Na 0.000000 0.000000 0.000000\n";
		ofs << "END_FRAME\n";
		ofs << "FRAME 1 2.000000\n";   // forward
		ofs << "Na 0.100000 0.000000 0.000000\n";
		ofs << "END_FRAME\n";
		ofs << "FRAME 2 1.000000\n";   // BACKWARDS — non-monotonic
		ofs << "Na 0.200000 0.000000 0.000000\n";
		ofs << "END_FRAME\n";
		ofs << "#END_DYNX\n";
	}

	auto r = vsim::io::dynx_validate(path);
	if (r.monotonic_time) {
		std::puts("FAIL DYNX-V1-06: should have detected non-monotonic time");
		return false;
	}
	std::puts("PASS DYNX-V1-06: non-monotonic frame times detected");
	return true;
}

static bool test_DYNX_V1_07() {
	std::string bad = tmp_path("dynx_does_not_exist_xyz.dynx");

	auto ri = vsim::io::dynx_inspect(bad);
	if (ri.ok) {
		std::puts("FAIL DYNX-V1-07: inspect on missing file returned ok");
		return false;
	}
	auto rv = vsim::io::dynx_validate(bad);
	if (rv.ok) {
		std::puts("FAIL DYNX-V1-07: validate on missing file returned ok");
		return false;
	}
	std::puts("PASS DYNX-V1-07: bad/missing file fails clearly");
	return true;
}

// ============================================================================
// main
// ============================================================================

int main() {
	std::puts("\n=== Group 57 — Dynx v1 Session Archive ===\n");

	int pass = 0, fail = 0;
	auto run = [&](bool (*fn)()) {
		if (fn()) ++pass; else ++fail;
	};

	run(test_DYNX_V1_01);
	run(test_DYNX_V1_02);
	run(test_DYNX_V1_03);
	run(test_DYNX_V1_04);
	run(test_DYNX_V1_05);
	run(test_DYNX_V1_06);
	run(test_DYNX_V1_07);

	std::printf("\n  Results: %d passed  %d failed\n\n", pass, fail);
	return (fail == 0) ? 0 : 1;
}
