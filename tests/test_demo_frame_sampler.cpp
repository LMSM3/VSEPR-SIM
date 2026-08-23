/**
 * tests/test_demo_frame_sampler.cpp
 * =====================================
 * WO-OUTPUT-P2-B  |  Group 74  |  DemoFrameSampler all strategies
 */

#include "include/vsim/vsim_document.hpp"
#include "include/vsim/io/demo_frame_sampler.hpp"
#include "include/vsim/io/dynx_writer.hpp"
#include "include/vsim/io/dynx_reader.hpp"

#include <cassert>

using vsim::ExportDemoSection;
using vsim::io::DynxHeader;
using vsim::io::DynxWriter;
#include <cstdio>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

static void check(bool cond, const char* msg) {
	if (!cond) {
		std::cerr << "FAIL: " << msg << "\n";
		std::exit(1);
	}
	std::cout << "  PASS: " << msg << "\n";
}

// ---------------------------------------------------------------------------
// Build a synthetic .dynx file with N frames
// ---------------------------------------------------------------------------

static std::string make_temp_dynx(int n_frames, bool add_events = false) {
	std::string path = "test_sampler_src_" + std::to_string(n_frames)
					 + (add_events ? "_ev" : "") + ".dynx";
	DynxHeader hdr;
	hdr.source_path     = "test.vsim";
	hdr.source_hash     = "none";
	hdr.kernel_version  = "test-1.0";
	hdr.frame_interval_fs  = 1.0;
	hdr.particle_count  = 2;

	vsim::io::DynxWriter w;
	if (!w.open(path, hdr)) return "";

	for (int i = 0; i < n_frames; ++i) {
		vsim::io::DynxRichFrame rf;
		rf.index   = i;
		rf.time_fs = static_cast<double>(i);
		vsim::io::DynxParticleState p1; p1.symbol = "C"; p1.pos = {0,0,0};
		vsim::io::DynxParticleState p2; p2.symbol = "H"; p2.pos = {1,0,0};
		rf.particles = {p1, p2};
		if (add_events && i % 3 == 0) {
			vsim::io::DynxEventPacket ev;
			ev.kind = "checkpoint"; ev.event_id = i; ev.source = "test"; ev.value = 0.0;
			rf.events.push_back(ev);
		}
		w.write_rich_frame(rf);
	}
	w.close();
	return path;
}

// ---------------------------------------------------------------------------
// P2-B-01  compute_indices uniform
// ---------------------------------------------------------------------------

static void test_compute_uniform() {
	ExportDemoSection cfg;
	cfg.strategy    = "uniform";
	cfg.demo_frames = 5;
	auto idx = vsim::io::DemoFrameSampler::compute_indices(20, cfg);
	check(static_cast<int>(idx.size()) == 5, "P2-B-01 uniform count=5");
	check(idx.front() == 0,                  "P2-B-01 uniform first=0");
	check(idx.back()  == 19,                 "P2-B-01 uniform last=19");
}

// ---------------------------------------------------------------------------
// P2-B-02  compute_indices first
// ---------------------------------------------------------------------------

static void test_compute_first() {
	ExportDemoSection cfg;
	cfg.strategy    = "first";
	cfg.demo_frames = 3;
	auto idx = vsim::io::DemoFrameSampler::compute_indices(10, cfg);
	check(static_cast<int>(idx.size()) == 3, "P2-B-02 first count=3");
	check(idx[0] == 0 && idx[1] == 1 && idx[2] == 2, "P2-B-02 first indices");
}

// ---------------------------------------------------------------------------
// P2-B-03  compute_indices last
// ---------------------------------------------------------------------------

static void test_compute_last() {
	ExportDemoSection cfg;
	cfg.strategy    = "last";
	cfg.demo_frames = 3;
	auto idx = vsim::io::DemoFrameSampler::compute_indices(10, cfg);
	check(static_cast<int>(idx.size()) == 3, "P2-B-03 last count=3");
	check(idx[0] == 7 && idx[1] == 8 && idx[2] == 9, "P2-B-03 last indices");
}

// ---------------------------------------------------------------------------
// P2-B-04  compute_indices event_gated
// ---------------------------------------------------------------------------

static void test_compute_event_gated() {
	ExportDemoSection cfg;
	cfg.strategy    = "event_gated";
	cfg.demo_frames = 10;
	std::vector<int> events = {2, 5, 5, 9};  // dup intentional
	auto idx = vsim::io::DemoFrameSampler::compute_indices(15, cfg, events);
	check(static_cast<int>(idx.size()) == 3, "P2-B-04 event_gated dedup count=3");
	check(idx[0] == 2 && idx[1] == 5 && idx[2] == 9, "P2-B-04 event_gated indices");
}

// ---------------------------------------------------------------------------
// P2-B-05  clamp: fewer frames than requested
// ---------------------------------------------------------------------------

static void test_clamp() {
	ExportDemoSection cfg;
	cfg.strategy    = "uniform";
	cfg.demo_frames = 20;
	auto idx = vsim::io::DemoFrameSampler::compute_indices(5, cfg);
	check(static_cast<int>(idx.size()) <= 5, "P2-B-05 clamp <= total");
}

// ---------------------------------------------------------------------------
// P2-B-06  sample() uniform end-to-end
// ---------------------------------------------------------------------------

static void test_sample_uniform() {
	auto src = make_temp_dynx(12);
	check(!src.empty(), "P2-B-06 source created");

	ExportDemoSection cfg;
	cfg.strategy    = "uniform";
	cfg.demo_frames = 4;

	auto r = vsim::io::DemoFrameSampler::sample(src, "test_demo_out_uniform.demo.dynx", cfg);
	check(r.ok,               "P2-B-06 sample ok");
	check(r.frames_in == 12,  "P2-B-06 frames_in");
	check(r.frames_out == 4,  "P2-B-06 frames_out");
	check(static_cast<int>(r.frame_indices.size()) == 4, "P2-B-06 index count");

	// Verify output is readable
	vsim::io::DynxReader rd;
	check(rd.open("test_demo_out_uniform.demo.dynx"), "P2-B-06 output readable");
	std::vector<vsim::io::DynxRichFrame> frames;
	rd.read_all_frames(frames);
	rd.close();
	check(static_cast<int>(frames.size()) == 4, "P2-B-06 output frame count");

	std::remove(src.c_str());
	std::remove("test_demo_out_uniform.demo.dynx");
}

// ---------------------------------------------------------------------------
// P2-B-07  sample() event_gated fallback to uniform when no events
// ---------------------------------------------------------------------------

static void test_sample_event_gated_fallback() {
	auto src = make_temp_dynx(8, false);  // no events
	ExportDemoSection cfg;
	cfg.strategy    = "event_gated";
	cfg.demo_frames = 3;

	auto r = vsim::io::DemoFrameSampler::sample(src, "test_demo_ev_fallback.demo.dynx", cfg);
	check(r.ok,              "P2-B-07 event_gated fallback ok");
	check(r.frames_out == 3, "P2-B-07 fallback frames_out");

	std::remove(src.c_str());
	std::remove("test_demo_ev_fallback.demo.dynx");
}

// ---------------------------------------------------------------------------
// P2-B-08  sample() event_gated with real events
// ---------------------------------------------------------------------------

static void test_sample_event_gated_real() {
	auto src = make_temp_dynx(9, true);  // events at 0, 3, 6
	ExportDemoSection cfg;
	cfg.strategy    = "event_gated";
	cfg.demo_frames = 10;

	auto r = vsim::io::DemoFrameSampler::sample(src, "test_demo_ev_real.demo.dynx", cfg);
	check(r.ok,              "P2-B-08 event_gated real ok");
	check(r.frames_out == 3, "P2-B-08 event frames count (0,3,6)");

	std::remove(src.c_str());
	std::remove("test_demo_ev_real.demo.dynx");
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main() {
	std::cout << "=== Group 74: DemoFrameSampler ===\n";
	test_compute_uniform();
	test_compute_first();
	test_compute_last();
	test_compute_event_gated();
	test_clamp();
	test_sample_uniform();
	test_sample_event_gated_fallback();
	test_sample_event_gated_real();
	std::cout << "All Group 74 tests passed.\n";
	return 0;
}
