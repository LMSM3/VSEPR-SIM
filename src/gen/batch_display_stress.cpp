/**
 * batch_display_stress.cpp
 * ========================
 * VSEPR-SIM | beta.7 | Stress test for BatchWindowBridge + ContinuousRunDisplay
 *
 * Tests:
 *   T1  push/pull basic round-trip
 *   T2  high-frequency producer (10 000 frames) — drop-oldest, no deadlock
 *   T3  frame count monotonicity
 *   T4  status snapshot consistency under concurrent updates
 *   T5  elapsed-time monotonicity
 *   T6  display_tick throttle (respects fps limit)
 *   T7  start / finish lifecycle; is_done() state machine
 *   T8  multi-cycle: start → finish → start → finish
 *   T9  ContinuousRunDisplay console loop — live threaded integration
 *   T10 push_frame with empty label (no status overwrite)
 *   T11 late consumer (producer finishes before display ever ticks)
 *   T12 zero-atom XYZFrame round-trip (edge case)
 *
 * Build:
 *   g++ -std=c++20 -O2 -I. -IC:\R\VSPER-SIM -IC:\R\VSPER-SIM\src
 *       batch_display_stress.cpp -o batch_display_stress.exe
 *
 * Exit 0 = all pass.
 */

#include "../../src/vis/batch_window_bridge.hpp"
#include "../../src/vis/continuous_run_display.hpp"

#include <cassert>
#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <string>
#include <cmath>

using namespace vsepr::vis;
using namespace vsepr::io;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static XYZFrame make_frame(int n_atoms, double energy = -1.0,
							const std::string& comment = "") {
	XYZFrame f;
	f.N = n_atoms;
	f.comment = comment;
	f.energy  = energy;
	for (int i = 0; i < n_atoms; ++i) {
		AtomRecord a;
		a.Z      = 22;          // Ti
		a.symbol = "Ti";
		a.x = static_cast<double>(i);
		a.y = 0.0;
		a.z = 0.0;
		f.atoms.push_back(a);
	}
	return f;
}

static bool approx_gte(double a, double b, double tol = 1e-9) {
	return a >= b - tol;
}

#define PASS(name) std::cout << "[PASS] " << name << "\n"
#define FAIL(name, msg) do { \
	std::cerr << "[FAIL] " << name << " -- " << msg << "\n"; \
	return false; \
} while(0)

// ---------------------------------------------------------------------------
// T1: basic push / latest_frame round-trip
// ---------------------------------------------------------------------------
static bool t1_basic_roundtrip() {
	BatchWindowBridge b;
	b.start();

	if (b.latest_frame() != nullptr)
		FAIL("T1", "expected nullptr before any push");

	auto f = make_frame(4, -42.0, "test");
	b.push_frame(f, "test_label");

	auto got = b.latest_frame();
	if (!got)
		FAIL("T1", "latest_frame() returned nullptr after push");
	if (got->N != 4)
		FAIL("T1", "N mismatch");
	if (!got->energy.has_value() || std::abs(*got->energy - (-42.0)) > 1e-9)
		FAIL("T1", "energy mismatch");
	if (got->comment != "test")
		FAIL("T1", "comment mismatch");

	auto snap = b.status_snapshot();
	if (snap.run_label != "test_label")
		FAIL("T1", "run_label not propagated");

	b.finish();
	if (!b.is_done())
		FAIL("T1", "is_done() should be true after finish()");

	PASS("T1 basic_roundtrip");
	return true;
}

// ---------------------------------------------------------------------------
// T2: high-frequency producer — 10 000 frames, no deadlock, no block
// ---------------------------------------------------------------------------
static bool t2_high_freq_producer() {
	BatchWindowBridge b;
	b.start();

	const int N = 10'000;
	for (int i = 0; i < N; ++i) {
		b.push_frame(make_frame(2, static_cast<double>(i)), "hf");
		b.push_progress(i, N, i, N, "spinning");
	}
	b.finish();

	// We must have at least one frame and the counter must be N
	if (b.frames_pushed_count() != static_cast<std::uint64_t>(N))
		FAIL("T2", "frames_pushed_count mismatch: got " +
			 std::to_string(b.frames_pushed_count()) + " want " + std::to_string(N));

	auto got = b.latest_frame();
	if (!got)
		FAIL("T2", "latest_frame null after 10k pushes");

	PASS("T2 high_freq_producer");
	return true;
}

// ---------------------------------------------------------------------------
// T3: frame count monotonicity across concurrent push/read
// ---------------------------------------------------------------------------
static bool t3_count_monotonic() {
	BatchWindowBridge b;
	b.start();

	std::atomic<std::uint64_t> last_seen{0};
	std::atomic<bool> consumer_ok{true};

	std::thread consumer([&]{
		for (int i = 0; i < 500; ++i) {
			auto c = b.frames_pushed_count();
			if (c < last_seen.load()) {
				consumer_ok.store(false);
				return;
			}
			last_seen.store(c);
			std::this_thread::sleep_for(std::chrono::microseconds(50));
		}
	});

	for (int i = 0; i < 2000; ++i)
		b.push_frame(make_frame(1, 0.0), "mono");

	b.finish();
	consumer.join();

	if (!consumer_ok.load())
		FAIL("T3", "frame count decreased (monotonicity broken)");

	PASS("T3 count_monotonic");
	return true;
}

// ---------------------------------------------------------------------------
// T4: status snapshot consistency (no torn reads)
// ---------------------------------------------------------------------------
static bool t4_status_consistency() {
	BatchWindowBridge b;
	b.start();

	std::atomic<bool> running{true};
	std::atomic<bool> torn{false};

	// Producer: rapidly update status with matching frame_index/total_frames
	std::thread producer([&]{
		for (int i = 0; i < 5000 && running.load(); ++i)
			b.push_progress(i, 5000, i/5, 1000, "stress");
		b.finish();
	});

	// Consumer: verify total_frames is always 5000 (never mid-write garbage)
	std::thread consumer([&]{
		while (!b.is_done()) {
			auto snap = b.status_snapshot();
			if (snap.total_frames != 0 && snap.total_frames != 5000) {
				torn.store(true);
				break;
			}
			std::this_thread::sleep_for(std::chrono::microseconds(10));
		}
		running.store(false);
	});

	producer.join();
	consumer.join();

	if (torn.load())
		FAIL("T4", "torn status read detected");

	PASS("T4 status_consistency");
	return true;
}

// ---------------------------------------------------------------------------
// T5: elapsed time monotonicity
// ---------------------------------------------------------------------------
static bool t5_elapsed_monotonic() {
	BatchWindowBridge b;
	b.start();

	double prev = b.elapsed_seconds();
	for (int i = 0; i < 20; ++i) {
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
		double now = b.elapsed_seconds();
		if (now < prev)
			FAIL("T5", "elapsed_seconds() decreased");
		prev = now;
	}
	b.finish();

	PASS("T5 elapsed_monotonic");
	return true;
}

// ---------------------------------------------------------------------------
// T6: display_tick throttle
// ---------------------------------------------------------------------------
static bool t6_tick_throttle() {
	BatchWindowBridge b;
	b.start();

	// At 100 fps limit, two ticks 1ms apart: second should be false
	bool first  = b.display_tick(100.0f);
	bool second = b.display_tick(100.0f);  // < 10ms later
	if (!first)
		FAIL("T6", "first display_tick() returned false");
	if (second)
		FAIL("T6", "second immediate display_tick() returned true (should throttle)");

	// Wait longer than the interval then retry
	std::this_thread::sleep_for(std::chrono::milliseconds(15));
	bool third = b.display_tick(100.0f);
	if (!third)
		FAIL("T6", "tick after 15ms still throttled at 100fps");

	b.finish();
	PASS("T6 tick_throttle");
	return true;
}

// ---------------------------------------------------------------------------
// T7: lifecycle state machine
// ---------------------------------------------------------------------------
static bool t7_lifecycle() {
	BatchWindowBridge b;

	if (b.is_done())
		FAIL("T7", "is_done() true before start()");

	b.start();
	if (b.is_done())
		FAIL("T7", "is_done() true immediately after start()");
	if (b.frames_pushed_count() != 0)
		FAIL("T7", "frames_pushed_count nonzero after start()");

	b.push_frame(make_frame(1), "lc");
	if (b.frames_pushed_count() != 1)
		FAIL("T7", "frames_pushed_count should be 1");

	b.finish();
	if (!b.is_done())
		FAIL("T7", "is_done() false after finish()");

	PASS("T7 lifecycle");
	return true;
}

// ---------------------------------------------------------------------------
// T8: multi-cycle restart
// ---------------------------------------------------------------------------
static bool t8_multi_cycle() {
	BatchWindowBridge b;

	for (int cycle = 0; cycle < 3; ++cycle) {
		b.start();
		if (b.frames_pushed_count() != 0)
			FAIL("T8", "count not reset on cycle " + std::to_string(cycle));
		if (b.is_done())
			FAIL("T8", "is_done true at start of cycle " + std::to_string(cycle));

		for (int i = 0; i < 10; ++i)
			b.push_frame(make_frame(2, 0.0), "cycle");

		b.finish();
		if (!b.is_done())
			FAIL("T8", "is_done false at end of cycle " + std::to_string(cycle));
		if (b.frames_pushed_count() != 10)
			FAIL("T8", "count wrong at end of cycle " + std::to_string(cycle));
	}

	PASS("T8 multi_cycle");
	return true;
}

// ---------------------------------------------------------------------------
// T9: ContinuousRunDisplay console loop — threaded integration
// ---------------------------------------------------------------------------
static bool t9_console_loop_integration() {
	ContinuousRunDisplay display;
	display.configure("stress_t9", 50.0f);  // 50fps so loop runs frequently
	// open() returns false (no GL) -> console fallback
	display.open(1, 1);

	auto& bridge = display.bridge();
	bridge.start();

	std::atomic<int> frames_sent{0};
	std::thread producer([&]{
		for (int i = 0; i < 30; ++i) {
			bridge.push_frame(make_frame(3, static_cast<double>(i)), "t9");
			bridge.push_progress(i, 30, i, 30, "writing");
			frames_sent.fetch_add(1);
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
		}
		bridge.finish();
	});

	// Suppress console output for this test by redirecting to /dev/null is
	// platform-dependent; instead just run it and verify it returns.
	auto t0 = std::chrono::steady_clock::now();
	display.run_event_loop();
	double elapsed = std::chrono::duration<double>(
		std::chrono::steady_clock::now() - t0).count();

	producer.join();

	if (frames_sent.load() != 30)
		FAIL("T9", "producer didn't finish (sent " +
			 std::to_string(frames_sent.load()) + "/30)");
	if (!bridge.is_done())
		FAIL("T9", "bridge not done after run_event_loop returned");
	if (elapsed > 5.0)
		FAIL("T9", "run_event_loop took too long: " + std::to_string(elapsed) + "s");

	PASS("T9 console_loop_integration");
	return true;
}

// ---------------------------------------------------------------------------
// T10: push_frame with empty label — no status.run_label overwrite
// ---------------------------------------------------------------------------
static bool t10_empty_label() {
	BatchWindowBridge b;
	b.start();

	// Set a label first
	b.push_frame(make_frame(1), "initial_label");
	auto snap1 = b.status_snapshot();
	if (snap1.run_label != "initial_label")
		FAIL("T10", "initial label not set");

	// Push again with no label
	b.push_frame(make_frame(1), "");
	auto snap2 = b.status_snapshot();
	if (snap2.run_label != "initial_label")
		FAIL("T10", "empty-label push overwrote existing run_label");

	b.finish();
	PASS("T10 empty_label");
	return true;
}

// ---------------------------------------------------------------------------
// T11: late consumer (producer done before first tick)
// ---------------------------------------------------------------------------
static bool t11_late_consumer() {
	BatchWindowBridge b;
	b.start();

	const int N = 100;
	for (int i = 0; i < N; ++i)
		b.push_frame(make_frame(2, static_cast<double>(i)), "late");
	b.finish();

	// Consumer wakes up long after producer is done
	std::this_thread::sleep_for(std::chrono::milliseconds(20));

	if (!b.is_done())
		FAIL("T11", "is_done false");

	auto f = b.latest_frame();
	if (!f)
		FAIL("T11", "latest_frame null after late read");

	// Should have the last-pushed frame (drop-oldest means most recent survives)
	if (f->N != 2)
		FAIL("T11", "unexpected frame atom count");

	PASS("T11 late_consumer");
	return true;
}

// ---------------------------------------------------------------------------
// T12: zero-atom XYZFrame edge case
// ---------------------------------------------------------------------------
static bool t12_zero_atom_frame() {
	BatchWindowBridge b;
	b.start();

	XYZFrame empty;
	empty.N = 0;
	empty.comment = "zero";
	b.push_frame(empty, "zero_test");

	auto got = b.latest_frame();
	if (!got)
		FAIL("T12", "nullptr after zero-atom push");
	if (got->N != 0)
		FAIL("T12", "N should be 0");
	if (got->comment != "zero")
		FAIL("T12", "comment mismatch");

	b.finish();
	PASS("T12 zero_atom_frame");
	return true;
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main() {
	std::cout << "=== batch_display_stress  VSEPR-SIM beta.7 ===\n\n";

	using TestFn = bool(*)();
	const std::vector<TestFn> tests = {
		t1_basic_roundtrip,
		t2_high_freq_producer,
		t3_count_monotonic,
		t4_status_consistency,
		t5_elapsed_monotonic,
		t6_tick_throttle,
		t7_lifecycle,
		t8_multi_cycle,
		t9_console_loop_integration,
		t10_empty_label,
		t11_late_consumer,
		t12_zero_atom_frame,
	};

	int passed = 0, failed = 0;
	for (auto fn : tests) {
		if (fn()) ++passed; else ++failed;
	}

	std::cout << "\n--- Results: " << passed << "/" << (passed + failed)
			  << " passed";
	if (failed == 0) std::cout << "  ALL PASS";
	std::cout << " ---\n";

	return (failed == 0) ? 0 : 1;
}
