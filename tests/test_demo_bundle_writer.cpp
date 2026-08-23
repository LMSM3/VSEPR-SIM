/**
 * tests/test_demo_bundle_writer.cpp
 * =====================================
 * WO-OUTPUT-P2-C  |  Group 75  |  DemoBundleWriter round-trip
 */

#include "include/vsim/vsim_document.hpp"
#include "include/vsim/io/demo_frame_sampler.hpp"
#include "include/vsim/io/demo_bundle_writer.hpp"
#include "include/xbundle/xbundle_reader.hpp"
#include "include/xbundle/xbundle_document.hpp"
#include "include/vsim/io/dynx_writer.hpp"

#include <cstdio>

using vsim::ExportDemoSection;
using vsim::io::DynxHeader;
using vsim::io::DynxWriter;
#include <fstream>
#include <iostream>
#include <string>

static void check(bool cond, const char* msg) {
	if (!cond) {
		std::cerr << "FAIL: " << msg << "\n";
		std::exit(1);
	}
	std::cout << "  PASS: " << msg << "\n";
}

// ---------------------------------------------------------------------------
// helper: build a minimal .dynx for testing
// ---------------------------------------------------------------------------

static std::string make_dynx(const std::string& path, int n) {
	DynxHeader hdr;
	hdr.source_path    = "test.vsim";
	hdr.source_hash    = "none";
	hdr.kernel_version = "test";
	hdr.frame_interval_fs = 1.0;
	hdr.particle_count = 1;

	vsim::io::DynxWriter w;
	if (!w.open(path, hdr)) return "";
	for (int i = 0; i < n; ++i) {
		vsim::io::DynxRichFrame rf;
		rf.index = i; rf.time_fs = (double)i;
		vsim::io::DynxParticleState p; p.symbol = "C"; p.pos = {0,0,0};
		rf.particles = {p};
		w.write_rich_frame(rf);
	}
	w.close();
	return path;
}

static std::string make_vsim(const std::string& path) {
	std::ofstream f(path);
	f << "[simulation]\nname = bundle_test\n";
	return f.is_open() ? path : "";
}

// ---------------------------------------------------------------------------
// P2-C-01  full bundle has 3 members
// ---------------------------------------------------------------------------

static void test_full_bundle() {
	make_dynx("bw_src.dynx", 6);

	ExportDemoSection cfg;
	cfg.strategy    = "uniform";
	cfg.demo_frames = 3;
	auto sr = vsim::io::DemoFrameSampler::sample("bw_src.dynx", "bw.demo.dynx", cfg);
	check(sr.ok, "P2-C-01 sample ok");

	make_vsim("bw.vsim");
	auto br = vsim::io::DemoBundleWriter::write(
		"bw.demo.dynx", "bw.vsim", "bw.demo.X", sr, cfg, "case_01");
	check(br.ok,                "P2-C-01 bundle write ok");
	check(br.member_count == 3, "P2-C-01 member count 3");

	// Round-trip: read back with XBundleReader
	auto bundle = vsim::xbundle::XBundleReader::read_file("bw.demo.X");
	check(bundle.manifest.populated,                 "P2-C-01 xbundle read ok");
	check(bundle.entries.size() == 3,                "P2-C-01 3 entries in bundle");

	// Check member names
	bool has_dynx = false, has_vsim = false, has_manifest = false;
	for (const auto& e : bundle.entries) {
		if (e.name == "demo.dynx")          has_dynx     = true;
		if (e.name == "main.vsim")          has_vsim     = true;
		if (e.name == "demo_manifest.json") has_manifest = true;
	}
	check(has_dynx,     "P2-C-01 demo.dynx member present");
	check(has_vsim,     "P2-C-01 main.vsim member present");
	check(has_manifest, "P2-C-01 demo_manifest.json present");

	std::remove("bw_src.dynx");
	std::remove("bw.demo.dynx");
	std::remove("bw.vsim");
	std::remove("bw.demo.X");
}

// ---------------------------------------------------------------------------
// P2-C-02  bundle without source (.vsim omitted)
// ---------------------------------------------------------------------------

static void test_no_source_bundle() {
	make_dynx("bw2_src.dynx", 4);

	ExportDemoSection cfg;
	cfg.strategy        = "uniform";
	cfg.demo_frames     = 2;
	cfg.include_source  = false;
	cfg.include_manifest= false;
	auto sr = vsim::io::DemoFrameSampler::sample("bw2_src.dynx", "bw2.demo.dynx", cfg);
	check(sr.ok, "P2-C-02 sample ok");

	auto br = vsim::io::DemoBundleWriter::write(
		"bw2.demo.dynx", "", "bw2.demo.X", sr, cfg, "case_02");
	check(br.ok,                "P2-C-02 bundle write ok");
	check(br.member_count == 1, "P2-C-02 member count 1 (dynx only)");

	vsim::xbundle::XBundle b2 = vsim::xbundle::XBundleReader::read_file("bw2.demo.X");
	check(b2.manifest.populated,         "P2-C-02 xbundle read ok");
	check(b2.entries.size() == 1,        "P2-C-02 1 entry");

	std::remove("bw2_src.dynx");
	std::remove("bw2.demo.dynx");
	std::remove("bw2.demo.X");
}

// ---------------------------------------------------------------------------
// P2-C-03  manifest JSON contains expected fields
// ---------------------------------------------------------------------------

static void test_manifest_content() {
	make_dynx("bw3_src.dynx", 5);

	ExportDemoSection cfg;
	cfg.strategy    = "first";
	cfg.demo_frames = 2;
	auto sr = vsim::io::DemoFrameSampler::sample("bw3_src.dynx", "bw3.demo.dynx", cfg);
	check(sr.ok, "P2-C-03 sample ok");

	auto br = vsim::io::DemoBundleWriter::write(
		"bw3.demo.dynx", "", "bw3.demo.X", sr, cfg, "mycase");

	auto b3 = vsim::xbundle::XBundleReader::read_file("bw3.demo.X");
	std::string manifest_content;
	for (const auto& e : b3.entries) {
		if (e.name == "demo_manifest.json") manifest_content = e.content;
	}
	check(!manifest_content.empty(),                "P2-C-03 manifest not empty");
	check(manifest_content.find("mycase")    != std::string::npos, "P2-C-03 case_id in manifest");
	check(manifest_content.find("\"first\"") != std::string::npos, "P2-C-03 strategy in manifest");
	check(manifest_content.find("frames_in_demo") != std::string::npos, "P2-C-03 frames_in_demo key");

	std::remove("bw3_src.dynx");
	std::remove("bw3.demo.dynx");
	std::remove("bw3.demo.X");
}

// ---------------------------------------------------------------------------
// P2-C-04  custom bundle_name honoured
// ---------------------------------------------------------------------------

static void test_custom_bundle_name() {
	make_dynx("bw4_src.dynx", 4);
	ExportDemoSection cfg;
	cfg.strategy     = "uniform";
	cfg.demo_frames  = 2;
	cfg.bundle_name  = "custom_name.X";
	auto sr = vsim::io::DemoFrameSampler::sample("bw4_src.dynx", "bw4.demo.dynx", cfg);
	auto br = vsim::io::DemoBundleWriter::write(
		"bw4.demo.dynx", "", "custom_name.X", sr, cfg, "case04");
	check(br.ok, "P2-C-04 custom name bundle ok");

	std::remove("bw4_src.dynx");
	std::remove("bw4.demo.dynx");
	std::remove("custom_name.X");
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main() {
	std::cout << "=== Group 75: DemoBundleWriter Round-Trip ===\n";
	test_full_bundle();
	test_no_source_bundle();
	test_manifest_content();
	test_custom_bundle_name();
	std::cout << "All Group 75 tests passed.\n";
	return 0;
}
