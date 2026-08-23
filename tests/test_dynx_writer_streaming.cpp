/**
 * tests/test_dynx_writer_streaming.cpp
 * =======================================
 * Group 72 — DynxWriter streaming API + DynxReader round-trip (WO-72D)
 *
 * Acceptance tests:
 *
 *   72-A   begin_frame / write_particle / end_frame produces a valid .dynx
 *   72-B   FORCE lines emitted and read back correctly
 *   72-C   BOND_FORCE lines emitted and read back correctly
 *   72-D   FIELD lines emitted and read back correctly
 *   72-E   EVENT lines emitted and read back correctly
 *   72-F   RENDER lines emitted and read back correctly
 *   72-G   CAMERA lines emitted and read back correctly
 *   72-H   dynx_validate passes on streaming-written file
 *   72-I   DynxReader::read_all_frames recovers every frame without
 *          corrupting frame boundaries
 *   72-J   DynxSession helpers (current_frame, at_end, advance)
 *   72-K   end_frame auto-close: close() with open frame finalises cleanly
 */

#include "vsim/io/dynx_writer.hpp"
#include "vsim/io/dynx_reader.hpp"
#include "vsim/io/dynx_session.hpp"

// Re-enable assert even in Release builds — this test suite relies on assert
// as the primary check mechanism.
#undef NDEBUG
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

// ============================================================================
// Helpers
// ============================================================================

static std::string tmp_path(const char* name) {
#ifdef _WIN32
	char buf[512];
	std::snprintf(buf, sizeof(buf), "%s\\%s",
				  std::getenv("TEMP") ? std::getenv("TEMP") : ".", name);
	return buf;
#else
	return std::string("/tmp/") + name;
#endif
}

static void remove_tmp(const std::string& p) { std::remove(p.c_str()); }

inline bool file_contains(const std::string& path, const char* needle) {
	std::ifstream f(path);
	std::string line;
	while (std::getline(f, line))
		if (line.find(needle) != std::string::npos) return true;
	return false;
}

inline vsim::io::DynxHeader make_hdr(int particles = 0) {
	vsim::io::DynxHeader h;
	h.source_path     = "test.vsim";
	h.kernel_version  = "v5.1.x";
	h.particle_count  = particles;
	return h;
}

#define PASS(id) do { std::printf("  PASS  72-%s\n", id); } while(0)
#define FAIL(id, msg) do { std::fprintf(stderr, "  FAIL  72-%s  %s\n", id, msg); std::exit(1); } while(0)

// ============================================================================
// 72-A  begin_frame / write_particle / end_frame
// ============================================================================
static void test_72A() {
	using namespace vsim::io;
	auto path = tmp_path("72A.dynx");
	DynxWriter w;
	assert(w.open(path, make_hdr(2)));
	assert(w.begin_frame(0, 0.0, 1.0));
	assert(w.write_particle(0, 1.0, 2.0, 3.0));
	assert(w.write_particle(1, 4.0, 5.0, 6.0));
	assert(w.end_frame());
	assert(w.close());
	assert(file_contains(path, "FRAME 0 0.000000"));
	assert(file_contains(path, "END_FRAME"));
	assert(w.frame_count() == 1);
	remove_tmp(path);
	PASS("A");
}

// ============================================================================
// 72-B  FORCE lines
// ============================================================================
static void test_72B() {
	using namespace vsim::io;
	auto path = tmp_path("72B.dynx");
	DynxWriter w;
	assert(w.open(path, make_hdr()));
	assert(w.begin_frame(0, 0.0));
	assert(w.write_force(2, 1.1, -2.2, 3.3));
	assert(w.end_frame());
	assert(w.close());
	assert(file_contains(path, "FORCE 2 1.100000 -2.200000 3.300000"));
	remove_tmp(path);
	PASS("B");
}

// ============================================================================
// 72-C  BOND_FORCE lines
// ============================================================================
static void test_72C() {
	using namespace vsim::io;
	auto path = tmp_path("72C.dynx");
	DynxWriter w;
	assert(w.open(path, make_hdr()));
	assert(w.begin_frame(0, 0.0));
	assert(w.write_bond_force(0, 1, 0.5, -0.5, 0.0));
	assert(w.end_frame());
	assert(w.close());
	assert(file_contains(path, "BOND_FORCE 0 1 0.500000 -0.500000 0.000000"));
	remove_tmp(path);
	PASS("C");
}

// ============================================================================
// 72-D  FIELD lines
// ============================================================================
static void test_72D() {
	using namespace vsim::io;
	auto path = tmp_path("72D.dynx");
	DynxWriter w;
	assert(w.open(path, make_hdr()));
	assert(w.begin_frame(0, 0.0));
	assert(w.write_field("flux_x", 9.9, 0.0, 0.0));
	assert(w.end_frame());
	assert(w.close());
	assert(file_contains(path, "FIELD flux_x 9.900000 0.000000 0.000000"));
	remove_tmp(path);
	PASS("D");
}

// ============================================================================
// 72-E  EVENT lines
// ============================================================================
static void test_72E() {
	using namespace vsim::io;
	auto path = tmp_path("72E.dynx");
	DynxWriter w;
	assert(w.open(path, make_hdr()));
	assert(w.begin_frame(0, 0.0));
	assert(w.write_event("reaction", 42, "H2O", 3.14));
	assert(w.end_frame());
	assert(w.close());
	assert(file_contains(path, "EVENT reaction 42 H2O 3.140000"));
	remove_tmp(path);
	PASS("E");
}

// ============================================================================
// 72-F  RENDER lines
// ============================================================================
static void test_72F() {
	using namespace vsim::io;
	auto path = tmp_path("72F.dynx");
	DynxWriter w;
	assert(w.open(path, make_hdr()));
	assert(w.begin_frame(0, 0.0));
	assert(w.write_render(3, 255, 128, 0, "highlight", true));
	assert(w.end_frame());
	assert(w.close());
	assert(file_contains(path, "RENDER 3 255 128 0 highlight 1"));
	remove_tmp(path);
	PASS("F");
}

// ============================================================================
// 72-G  CAMERA lines
// ============================================================================
static void test_72G() {
	using namespace vsim::io;
	auto path = tmp_path("72G.dynx");
	DynxWriter w;
	assert(w.open(path, make_hdr()));
	assert(w.begin_frame(0, 0.0));
	assert(w.write_camera("default", 1.0, 2.0, 3.0, 15.0, 45.0, 2.5));
	assert(w.end_frame());
	assert(w.close());
	assert(file_contains(path, "CAMERA default 1.000000 2.000000 3.000000 15.000000 45.000000 2.500000"));
	remove_tmp(path);
	PASS("G");
}

// ============================================================================
// 72-H  dynx_validate on a streaming-written file
// ============================================================================
static void test_72H() {
	using namespace vsim::io;
	auto path = tmp_path("72H.dynx");
	DynxWriter w;
	assert(w.open(path, make_hdr(1)));
	for (int i = 0; i < 3; ++i) {
		assert(w.begin_frame(static_cast<std::size_t>(i),
							 static_cast<double>(i) * 1.0));
		assert(w.write_particle(0, 0.0, double(i), 0.0));
		assert(w.write_force(0, 0.1, 0.2, 0.3));
		assert(w.end_frame());
	}
	assert(w.close());

	auto vr = dynx_validate(path);
	if (!vr.ok) FAIL("H", vr.error.c_str());
	assert(vr.frame_count == 3);
	assert(vr.monotonic_time);
	// No particle-count mismatch warnings expected
	for (const auto& warn : vr.warnings)
		if (warn.find("particle count mismatch") != std::string::npos)
			FAIL("H", warn.c_str());
	remove_tmp(path);
	PASS("H");
}

// ============================================================================
// 72-I  DynxReader round-trip: all rich lines survive read-back
// ============================================================================
static void test_72I() {
	using namespace vsim::io;
	auto path = tmp_path("72I.dynx");

	// Write two frames with every rich line type
	{
		DynxWriter w;
		assert(w.open(path, make_hdr(1)));
		for (int i = 0; i < 2; ++i) {
			assert(w.begin_frame(static_cast<std::size_t>(i),
								 static_cast<double>(i) * 2.0));
			assert(w.write_particle(0, 1.0, 2.0, 3.0));
			assert(w.write_force(0, 0.1, 0.2, 0.3));
			assert(w.write_bond_force(0, 1, 1.0, -1.0, 0.0));
			assert(w.write_field("stress_xx", 0.5, 0.0, 0.0));
			assert(w.write_event("checkpoint", i, "test", double(i)));
			assert(w.write_render(0, 200, 100, 50, "tagged", i % 2 == 0));
			assert(w.write_camera("view0", double(i), 0.0, 5.0, 10.0, 20.0, 1.5));
			assert(w.end_frame());
		}
		assert(w.close());
	}

	// Read back
	DynxReader rd;
	assert(rd.open(path));
	std::vector<DynxRichFrame> frames;
	assert(rd.read_all_frames(frames));
	assert(frames.size() == 2u);

	for (std::size_t i = 0; i < 2u; ++i) {
		const DynxRichFrame& fr [[maybe_unused]] = frames[i];
		assert(fr.index == static_cast<int>(i));
		assert(fr.particles.size()    == 1u);
		assert(fr.forces.size()       == 1u);
		assert(fr.bond_forces.size()  == 1u);
		assert(fr.field_vectors.size()== 1u);
		assert(fr.events.size()       == 1u);
		assert(fr.render.size()       == 1u);
		assert(fr.cameras.size()      == 1u);

		assert(fr.forces[0].particle_idx == 0);
		assert(fr.bond_forces[0].i == 0 && fr.bond_forces[0].j == 1);
		assert(fr.field_vectors[0].label == "stress_xx");
		assert(fr.events[0].kind == "checkpoint");
		assert(fr.render[0].r == 200 && fr.render[0].g == 100 && fr.render[0].b == 50);
		assert(fr.cameras[0].label == "view0");
	}
	remove_tmp(path);
	PASS("I");
}

// ============================================================================
// 72-J  DynxSession helpers
// ============================================================================
static void test_72J() {
	using namespace vsim::io;
	DynxSession sess;
	sess.loaded = true;
	sess.frames.resize(5);
	for (int i = 0; i < 5; ++i) sess.frames[static_cast<std::size_t>(i)].index = i;

	assert(sess.frame_count() == 5);
	assert(sess.has_frames());
	assert(sess.current_frame() != nullptr);
	assert(sess.current_frame()->index == 0);

	assert(sess.playback.advance(sess.frame_count()));
	assert(sess.playback.current_frame == 1);

	// Wind to last frame
	sess.playback.current_frame = 4;
	assert(sess.playback.at_end(sess.frame_count()));

	// Advance past end (no loop) should pause
	assert(!sess.playback.advance(sess.frame_count()));
	assert(sess.playback.mode == PlaybackMode::Paused);

	// Loop mode
	sess.playback.loop = true;
	sess.playback.current_frame = 4;
	assert(sess.playback.advance(sess.frame_count()));
	assert(sess.playback.current_frame == 0);

	PASS("J");
}

// ============================================================================
// 72-K  close() with open frame auto-finalises
// ============================================================================
static void test_72K() {
	using namespace vsim::io;
	auto path = tmp_path("72K.dynx");
	DynxWriter w;
	assert(w.open(path, make_hdr()));
	assert(w.begin_frame(0, 0.0));
	assert(w.write_particle(0, 0.0, 0.0, 0.0));
	// Deliberately skip end_frame — close() must do it
	assert(w.close());
	assert(w.frame_count() == 1);
	assert(file_contains(path, "END_FRAME"));
	remove_tmp(path);
	PASS("K");
}

// ============================================================================
// main
// ============================================================================

int main() {
	std::printf("=== Group 72 — DynxWriter streaming + DynxReader round-trip ===\n");
	test_72A();
	test_72B();
	test_72C();
	test_72D();
	test_72E();
	test_72F();
	test_72G();
	test_72H();
	test_72I();
	test_72J();
	test_72K();
	std::printf("All Group 72 checks passed.\n");
	return 0;
}
