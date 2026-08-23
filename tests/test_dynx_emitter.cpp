/**
 * tests/test_dynx_emitter.cpp
 * ============================
 * WO-72B  |  Group 71  —  DynxEmitter smoke tests
 *
 * Covers:
 *   71-A  DynxLiveCache push / poll round-trip
 *   71-B  DynxLiveCache clear + has_frame
 *   71-C  DynxLiveCache get-and-clear (slot empties after poll)
 *   71-D  DynxEmitter open + emit + close produces valid .dynx archive
 *   71-E  Archive validates via dynx_validate()
 *   71-F  frame_count matches actual emitted frames
 *   71-G  Rich particle lines (pos+vel+energy) round-trip
 *   71-H  FORCE lines present in emitted archive
 *   71-I  BOND_FORCE lines present in emitted archive
 *   71-J  FIELD lines present in emitted archive
 *   71-K  EVENT lines present in emitted archive
 *   71-L  RENDER lines present in emitted archive
 *   71-M  CAMERA lines present in emitted archive
 *   71-N  Archive opens while live cache is also active
 *   71-O  emit_step without open() still updates live cache
 *
 * WO-72B | V5.1.4
 */

#include "include/vsim/io/dynx_emitter.hpp"
#include "include/vsim/io/dynx_writer.hpp"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

// ============================================================================
// Helpers
// ============================================================================

static std::string tmp_path(const std::string& name) {
	return std::string(std::getenv("TEMP") ? std::getenv("TEMP") : "/tmp") +
		   "/" + name;
}

static bool file_contains(const std::string& path, const std::string& needle) {
	std::ifstream ifs(path);
	std::string   line;
	while (std::getline(ifs, line))
		if (line.find(needle) != std::string::npos) return true;
	return false;
}

static vsim::io::DynxParticleState make_particle(const std::string& sym,
												  double x, double y, double z,
												  double vx, double vy, double vz,
												  double energy) {
	vsim::io::DynxParticleState p;
	p.symbol    = sym;
	p.pos       = {x, y, z};
	p.vel       = {vx, vy, vz};
	p.energy    = energy;
	p.has_vel   = true;
	p.has_energy = true;
	return p;
}

static vsim::io::DynxEmitContext make_ctx(int frame_idx, double time_fs) {
	vsim::io::DynxEmitContext ctx;
	ctx.frame_index = frame_idx;
	ctx.time_fs     = time_fs;
	ctx.particles.push_back(make_particle("C", 0.0, 0.0, 0.0, 0.1, 0.0, 0.0, -10.5));
	ctx.particles.push_back(make_particle("O", 1.2, 0.0, 0.0, -0.1, 0.0, 0.0, -8.3));

	vsim::io::DynxForceState fs;
	fs.particle_idx = 0;
	fs.force        = {0.5, 0.0, 0.0};
	ctx.forces.push_back(fs);

	vsim::io::DynxBondForce bf;
	bf.i     = 0;
	bf.j     = 1;
	bf.force = {-0.5, 0.0, 0.0};
	ctx.bond_forces.push_back(bf);

	vsim::io::DynxFieldVector fv;
	fv.label = "flux";
	fv.vec   = {0.0, 0.0, 1.0};
	ctx.field_vectors.push_back(fv);

	vsim::io::DynxEventPacket ev;
	ev.kind     = "Formation";
	ev.event_id = 42;
	ev.source   = "CO";
	ev.value    = -18.8;
	ctx.events.push_back(ev);

	vsim::io::DynxRenderMeta rm;
	rm.particle_idx = 0;
	rm.r = 255; rm.g = 0; rm.b = 0;
	rm.tag     = "hot";
	rm.visible = true;
	ctx.render.push_back(rm);

	vsim::io::DynxCameraState cam;
	cam.label = "front";
	cam.x = 0.0; cam.y = 0.0; cam.z = 10.0;
	cam.pitch = 0.0; cam.yaw = 0.0; cam.zoom = 1.0;
	ctx.cameras.push_back(cam);

	return ctx;
}

// ============================================================================
// Test runner
// ============================================================================

static int passed = 0;
static int failed = 0;

#define CHECK(label, expr) do { \
	if (expr) { std::cout << "[PASS] " label "\n"; ++passed; } \
	else       { std::cout << "[FAIL] " label "\n"; ++failed; } \
} while (0)

int main() {
	using namespace vsim::io;

	std::cout << "=== Group 71: DynxEmitter (WO-72B) ===\n\n";

	// -----------------------------------------------------------------------
	// 71-A  Live cache push / poll round-trip
	// -----------------------------------------------------------------------
	{
		DynxLiveCache cache;
		DynxRichFrame f;
		f.index   = 5;
		f.time_fs = 5.0;
		cache.push(f);
		auto out = cache.poll();
		CHECK("71-A: poll returns value after push", out.has_value());
		CHECK("71-A: frame index preserved", out && out->index == 5);
	}

	// -----------------------------------------------------------------------
	// 71-B  Clear + has_frame
	// -----------------------------------------------------------------------
	{
		DynxLiveCache cache;
		DynxRichFrame f;
		f.index = 1;
		cache.push(f);
		CHECK("71-B: has_frame true after push", cache.has_frame());
		cache.clear();
		CHECK("71-B: has_frame false after clear", !cache.has_frame());
	}

	// -----------------------------------------------------------------------
	// 71-C  Get-and-clear — slot empties after poll
	// -----------------------------------------------------------------------
	{
		DynxLiveCache cache;
		DynxRichFrame f;
		f.index = 2;
		cache.push(f);
		auto first  = cache.poll();
		auto second = cache.poll();
		CHECK("71-C: first poll returns value",  first.has_value());
		CHECK("71-C: second poll returns nullopt", !second.has_value());
	}

	// -----------------------------------------------------------------------
	// 71-D  Emitter open + emit + close produces valid archive
	// -----------------------------------------------------------------------
	const std::string path71 = tmp_path("test_wo72b_group71.dynx");
	{
		DynxEmitter emitter;
		DynxHeader  hdr;
		hdr.source_path      = "tests/smoke.vsim";
		hdr.source_hash      = "abc123";
		hdr.particle_count   = 2;
		hdr.frame_interval_fs = 1.0;

		bool ok = emitter.open(path71, hdr);
		CHECK("71-D: emitter opens file", ok);

		if (ok) {
			for (int i = 0; i < 3; ++i) {
				bool emit_ok = emitter.emit_step(make_ctx(i, static_cast<double>(i)));
				CHECK("71-D: emit_step succeeds", emit_ok);
			}
			bool close_ok = emitter.close();
			CHECK("71-D: close succeeds", close_ok);
		}
	}

	// -----------------------------------------------------------------------
	// 71-E  Archive validates
	// -----------------------------------------------------------------------
	{
		auto vr = dynx_validate(path71);
		CHECK("71-E: validation ok",              vr.ok);
		CHECK("71-E: frame_count == 3",           vr.frame_count == 3);
		CHECK("71-E: monotonic_time true",        vr.monotonic_time);
		CHECK("71-E: hash_present true",          vr.hash_present);
	}

	// -----------------------------------------------------------------------
	// 71-F  frame_count matches
	// -----------------------------------------------------------------------
	{
		auto ir = dynx_inspect(path71);
		CHECK("71-F: inspect ok",                 ir.ok);
		CHECK("71-F: inspect frame_count == 3",   ir.frame_count == 3);
		CHECK("71-F: source_hash preserved",      ir.source_hash == "abc123");
	}

	// -----------------------------------------------------------------------
	// 71-G  Rich particle lines (pos+vel+energy)
	// -----------------------------------------------------------------------
	CHECK("71-G: particle line contains vel+energy",
		  file_contains(path71, "C 0.000000 0.000000 0.000000  0.100000"));

	// -----------------------------------------------------------------------
	// 71-H  FORCE lines
	// -----------------------------------------------------------------------
	CHECK("71-H: FORCE line present", file_contains(path71, "FORCE 0 "));

	// -----------------------------------------------------------------------
	// 71-I  BOND_FORCE lines
	// -----------------------------------------------------------------------
	CHECK("71-I: BOND_FORCE line present", file_contains(path71, "BOND_FORCE 0 1 "));

	// -----------------------------------------------------------------------
	// 71-J  FIELD lines
	// -----------------------------------------------------------------------
	CHECK("71-J: FIELD line present", file_contains(path71, "FIELD flux "));

	// -----------------------------------------------------------------------
	// 71-K  EVENT lines
	// -----------------------------------------------------------------------
	CHECK("71-K: EVENT line present", file_contains(path71, "EVENT Formation "));

	// -----------------------------------------------------------------------
	// 71-L  RENDER lines
	// -----------------------------------------------------------------------
	CHECK("71-L: RENDER line present", file_contains(path71, "RENDER 0 255 0 0 "));

	// -----------------------------------------------------------------------
	// 71-M  CAMERA lines
	// -----------------------------------------------------------------------
	CHECK("71-M: CAMERA line present", file_contains(path71, "CAMERA front "));

	// -----------------------------------------------------------------------
	// 71-N  Archive open + live cache simultaneously active
	// -----------------------------------------------------------------------
	{
		DynxEmitter emitter;
		DynxHeader  hdr;
		hdr.source_path    = "tests/n.vsim";
		hdr.particle_count = 2;
		emitter.open(tmp_path("test_wo72b_71n.dynx"), hdr);

		emitter.emit_step(make_ctx(0, 0.0));
		auto live = emitter.live_cache().poll();
		CHECK("71-N: live cache has frame after emit", live.has_value());
		CHECK("71-N: live cache frame index == 0", live && live->index == 0);
		emitter.close();
	}

	// -----------------------------------------------------------------------
	// 71-O  emit_step without open() still updates live cache
	// -----------------------------------------------------------------------
	{
		DynxEmitter emitter;  // no open()
		bool emit_ok = emitter.emit_step(make_ctx(0, 0.0));
		CHECK("71-O: emit without open returns true",      emit_ok);
		CHECK("71-O: live cache has frame without archive", emitter.live_cache().has_frame());
	}

	// -----------------------------------------------------------------------
	// 71-P  Validator produces zero particle-count warnings on rich frames
	//        (WO-72D — rich-line prefixes must not be counted as particles)
	// -----------------------------------------------------------------------
	{
		const std::string path71p = tmp_path("test_wo72d_71p.dynx");
		DynxEmitter emitter;
		DynxHeader  hdr;
		hdr.source_path    = "tests/rich.vsim";
		hdr.source_hash    = "def456";
		hdr.particle_count = 2;   // exactly 2 particles per frame
		hdr.frame_interval_fs = 1.0;

		bool ok = emitter.open(path71p, hdr);
		CHECK("71-P: emitter opens", ok);
		if (ok) {
			for (int i = 0; i < 2; ++i)
				emitter.emit_step(make_ctx(i, static_cast<double>(i)));
			emitter.close();
		}

		auto vr = dynx_validate(path71p);
		CHECK("71-P: validation ok",                   vr.ok);
		CHECK("71-P: frame_count == 2",                vr.frame_count == 2);
		bool has_particle_warning = false;
		for (const auto& w : vr.warnings)
			if (w.find("particle count mismatch") != std::string::npos)
				has_particle_warning = true;
		CHECK("71-P: no particle-count mismatch warnings", !has_particle_warning);
		std::remove(path71p.c_str());
	}

	// -----------------------------------------------------------------------
	// Summary
	// -----------------------------------------------------------------------
	std::cout << "\n--- Group 71 ---  passed=" << passed
			  << "  failed=" << failed << "\n";

	// Clean up temp file
	std::remove(path71.c_str());

	return failed == 0 ? 0 : 1;
}
