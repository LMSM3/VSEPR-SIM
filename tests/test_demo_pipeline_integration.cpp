/**
 * tests/test_demo_pipeline_integration.cpp
 * ==========================================
 * WO-OUTPUT-P2  |  Group 76  |  Demo pipeline integration
 *
 * End-to-end tests that span DemoFrameSampler + DemoBundleWriter +
 * DynxSession::load().  Every test exercises real I/O and validates:
 *   - keyframe strategy selection correctness
 *   - source_hash propagation through sample()
 *   - manifest JSON schema correctness (schema field, generated_utc, source_hash)
 *   - DynxSession::load() happy path and error path
 *   - validate-after-sample: dynx_validate() on a demo archive
 *   - push_count() stat on DynxLiveCache
 *
 * Group 76 tests (P2-INT-01 .. P2-INT-08)
 */

#include "include/vsim/vsim_document.hpp"
#include "include/vsim/io/demo_frame_sampler.hpp"
#include "include/vsim/io/demo_bundle_writer.hpp"
#include "include/vsim/io/dynx_writer.hpp"
#include "include/vsim/io/dynx_reader.hpp"
#include "include/vsim/io/dynx_session.hpp"
#include "include/vsim/io/dynx_emitter.hpp"

#include <cassert>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

using vsim::ExportDemoSection;
using vsim::io::DynxHeader;
using vsim::io::DynxWriter;
using vsim::io::DynxRichFrame;
using vsim::io::DynxParticleState;
using vsim::io::DynxEventPacket;
using vsim::io::DynxReader;
using vsim::io::DynxSession;
using vsim::io::DynxLiveCache;
using vsim::io::DynxEmitContext;
using vsim::io::DemoFrameSampler;
using vsim::io::DemoBundleWriter;
using vsim::io::dynx_validate;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void check(bool cond, const char* msg) {
	if (!cond) {
		std::cerr << "FAIL: " << msg << "\n";
		std::exit(1);
	}
	std::cout << "  PASS: " << msg << "\n";
}

// Build a synthetic .dynx file.
// Events are added to every 3rd frame when add_events = true.
// source_hash is set to 'hash_abc123' so we can verify propagation.
static std::string make_dynx(const std::string& path, int n_frames,
							  bool add_events = false)
{
	DynxHeader hdr;
	hdr.source_path      = "integration_test.vsim";
	hdr.source_hash      = "hash_abc123";
	hdr.kernel_version   = "test-1.0";
	hdr.frame_interval_fs = 1.0;
	hdr.particle_count   = 3;

	DynxWriter w;
	if (!w.open(path, hdr)) return "";

	for (int i = 0; i < n_frames; ++i) {
		DynxRichFrame rf;
		rf.index   = i;
		rf.time_fs = static_cast<double>(i);
		DynxParticleState p1; p1.symbol = "C"; p1.pos = {0.0, 0.0, 0.0};
		DynxParticleState p2; p2.symbol = "H"; p2.pos = {1.5, 0.0, 0.0};
		DynxParticleState p3; p3.symbol = "O"; p3.pos = {0.0, 1.2, 0.0};
		rf.particles = {p1, p2, p3};

		if (add_events && i % 3 == 0) {
			DynxEventPacket ev;
			ev.kind     = "checkpoint";
			ev.event_id = static_cast<uint64_t>(i);
			ev.source   = "C";
			ev.value    = static_cast<double>(i) * 0.5;
			rf.events.push_back(ev);
		}
		w.write_rich_frame(rf);
	}
	w.close();
	return path;
}

// ---------------------------------------------------------------------------
// P2-INT-01  keyframe strategy: selects event-dense frames in order
// ---------------------------------------------------------------------------

static void test_keyframe_strategy() {
	const std::string src = "int76_src_keyframe.dynx";
	const std::string out = "int76_demo_keyframe.demo.dynx";
	make_dynx(src, 12, /*add_events=*/true);  // events at 0,3,6,9

	ExportDemoSection cfg;
	cfg.strategy    = "keyframe";
	cfg.demo_frames = 4;

	auto r = DemoFrameSampler::sample(src, out, cfg);
	check(r.ok,               "P2-INT-01 keyframe ok");
	check(r.frames_out == 4,  "P2-INT-01 keyframe frames_out=4");
	// All selected frames should be event frames (0,3,6,9)
	for (int idx : r.frame_indices)
		check(idx % 3 == 0, "P2-INT-01 keyframe index is event frame");

	std::remove(src.c_str());
	std::remove(out.c_str());
}

// ---------------------------------------------------------------------------
// P2-INT-02  keyframe fallback to uniform when no events
// ---------------------------------------------------------------------------

static void test_keyframe_fallback_uniform() {
	const std::string src = "int76_src_kf_fallback.dynx";
	const std::string out = "int76_demo_kf_fallback.demo.dynx";
	make_dynx(src, 10, /*add_events=*/false);

	ExportDemoSection cfg;
	cfg.strategy    = "keyframe";
	cfg.demo_frames = 3;

	auto r = DemoFrameSampler::sample(src, out, cfg);
	check(r.ok,              "P2-INT-02 keyframe fallback ok");
	check(r.frames_out == 3, "P2-INT-02 keyframe fallback frames_out=3");

	std::remove(src.c_str());
	std::remove(out.c_str());
}

// ---------------------------------------------------------------------------
// P2-INT-03  source_hash propagation through sample()
// ---------------------------------------------------------------------------

static void test_source_hash_propagation() {
	const std::string src = "int76_src_hash.dynx";
	const std::string out = "int76_demo_hash.demo.dynx";
	make_dynx(src, 8, false);

	ExportDemoSection cfg;
	cfg.strategy    = "uniform";
	cfg.demo_frames = 3;

	auto r = DemoFrameSampler::sample(src, out, cfg);
	check(r.ok,                              "P2-INT-03 sample ok");
	check(r.source_hash == "hash_abc123",    "P2-INT-03 source_hash propagated");

	std::remove(src.c_str());
	std::remove(out.c_str());
}

// ---------------------------------------------------------------------------
// P2-INT-04  manifest JSON schema field + generated_utc present
// ---------------------------------------------------------------------------

static void test_manifest_json_fields() {
	const std::string src     = "int76_src_manifest.dynx";
	const std::string demo_dx = "int76_demo_manifest.demo.dynx";
	const std::string bundle  = "int76_demo_manifest.demo.X";
	make_dynx(src, 6, false);

	ExportDemoSection cfg;
	cfg.strategy         = "uniform";
	cfg.demo_frames      = 3;
	cfg.include_manifest = true;
	cfg.include_source   = false;

	auto sr = DemoFrameSampler::sample(src, demo_dx, cfg);
	check(sr.ok, "P2-INT-04 sample ok");

	auto br = DemoBundleWriter::write(demo_dx, "", bundle, sr, cfg, "test_case");
	check(br.ok, "P2-INT-04 bundle ok");

	// Read the bundle file and verify the manifest JSON is present
	std::FILE* f = std::fopen(bundle.c_str(), "r");
	check(f != nullptr, "P2-INT-04 bundle file exists");

	bool found_schema = false;
	bool found_utc    = false;
	bool found_hash   = false;
	char line[512];
	while (std::fgets(line, sizeof(line), f)) {
		std::string s = line;
		if (s.find("\"schema\"") != std::string::npos &&
			s.find("demo_manifest_v1") != std::string::npos)
			found_schema = true;
		if (s.find("\"generated_utc\"") != std::string::npos)
			found_utc = true;
		if (s.find("\"source_hash\"") != std::string::npos)
			found_hash = true;
	}
	std::fclose(f);

	check(found_schema, "P2-INT-04 manifest contains schema field");
	check(found_utc,    "P2-INT-04 manifest contains generated_utc");
	check(found_hash,   "P2-INT-04 manifest contains source_hash");

	std::remove(src.c_str());
	std::remove(demo_dx.c_str());
	std::remove(bundle.c_str());
}

// ---------------------------------------------------------------------------
// P2-INT-05  DynxSession::load() happy path
// ---------------------------------------------------------------------------

static void test_session_load_happy() {
	const std::string src = "int76_src_session.dynx";
	make_dynx(src, 5, true);

	auto session = DynxSession::load(src);
	check(session.loaded,                "P2-INT-05 session.loaded");
	check(session.error.empty(),         "P2-INT-05 session.error empty");
	check(session.frame_count() == 5,    "P2-INT-05 frame count 5");
	check(session.has_frames(),          "P2-INT-05 has_frames");
	check(session.path == src,           "P2-INT-05 path stored");

	// Current frame starts at 0
	const auto* cf = session.current_frame();
	check(cf != nullptr,                 "P2-INT-05 current_frame not null");
	check(cf->index == 0,               "P2-INT-05 current_frame index=0");

	// Advance playback
	session.playback.mode = vsim::io::PlaybackMode::Playing;
	bool adv = session.playback.advance(session.frame_count());
	check(adv,                           "P2-INT-05 advance returned true");
	check(session.playback.current_frame == 1, "P2-INT-05 current_frame=1 after advance");

	std::remove(src.c_str());
}

// ---------------------------------------------------------------------------
// P2-INT-06  DynxSession::load() error path (file not found)
// ---------------------------------------------------------------------------

static void test_session_load_error() {
	auto session = DynxSession::load("this_file_does_not_exist.dynx");
	check(!session.loaded,               "P2-INT-06 not loaded on missing file");
	check(!session.error.empty(),        "P2-INT-06 error set on missing file");
	check(session.frame_count() == 0,   "P2-INT-06 no frames on error");
}

// ---------------------------------------------------------------------------
// P2-INT-07  dynx_validate() on a sampled demo archive
// ---------------------------------------------------------------------------

static void test_validate_after_sample() {
	const std::string src = "int76_src_validate.dynx";
	const std::string out = "int76_demo_validate.demo.dynx";
	make_dynx(src, 9, false);

	ExportDemoSection cfg;
	cfg.strategy    = "uniform";
	cfg.demo_frames = 4;

	auto r = DemoFrameSampler::sample(src, out, cfg);
	check(r.ok, "P2-INT-07 sample ok");

	auto vr = dynx_validate(out);
	check(vr.ok,                       "P2-INT-07 validate ok");
	check(vr.frame_count == 4,         "P2-INT-07 validate frame_count=4");
	check(vr.monotonic_time,           "P2-INT-07 validate monotonic_time");
	check(vr.particle_count == 3,      "P2-INT-07 validate particle_count=3");

	std::remove(src.c_str());
	std::remove(out.c_str());
}

// ---------------------------------------------------------------------------
// P2-INT-08  DynxLiveCache::push_count() stat
// ---------------------------------------------------------------------------

static void test_live_cache_push_count() {
	DynxLiveCache cache;
	check(cache.push_count() == 0, "P2-INT-08 push_count starts at 0");

	vsim::io::DynxRichFrame f1;
	f1.index = 0; f1.time_fs = 0.0;
	cache.push(f1);
	check(cache.push_count() == 1, "P2-INT-08 push_count=1 after first push");

	vsim::io::DynxRichFrame f2;
	f2.index = 1; f2.time_fs = 1.0;
	cache.push(f2);
	check(cache.push_count() == 2, "P2-INT-08 push_count=2 after second push");

	// poll() should not affect count
	auto polled = cache.poll();
	check(polled.has_value(),       "P2-INT-08 poll returns frame");
	check(cache.push_count() == 2, "P2-INT-08 push_count unchanged after poll");

	cache.reset_count();
	check(cache.push_count() == 0, "P2-INT-08 push_count=0 after reset");
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main() {
	std::cout << "=== Group 76: Demo Pipeline Integration ===\n";
	test_keyframe_strategy();
	test_keyframe_fallback_uniform();
	test_source_hash_propagation();
	test_manifest_json_fields();
	test_session_load_happy();
	test_session_load_error();
	test_validate_after_sample();
	test_live_cache_push_count();
	std::cout << "All Group 76 tests passed.\n";
	return 0;
}
