/**
 * test_bridge65_event_limiter.cpp
 * ================================
 * Unit tests for the WO-BRIDGE-65 EventLimiter.
 *
 * Tests:
 *   1. test_limiter_inactive_below_rate — no limiting below threshold
 *   2. test_limiter_activates_above_step_count — per-step cap enforced
 *   3. test_always_print_bypasses_limiter — BondBroken always printed
 *   4. test_aggregate_counts_correct — collision/bond/anomaly counters
 *   5. test_sample_rate_reduces_prints — < 100% printed in sample mode
 *   6. test_reset_step_clears_counters — state resets cleanly
 *
 * WO-BRIDGE-65  |  v5.1.13
 */

#include <cassert>
#include <cstdio>
#include <string>

#include "vsim/bridge65/event_limiter.hpp"

using namespace vsim::bridge65;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static AtomEvent make_ev(AtomEventType t, int id = 1) {
	AtomEvent ev;
	ev.step    = 1;
	ev.time_s  = 1e-12;
	ev.atom_id = id;
	ev.symbol  = "Ar";
	ev.type    = t;
	ev.energy.delta_eV       = 0.001;
	ev.force.magnitude_eV_A  = 1.0;
	return ev;
}

// ---------------------------------------------------------------------------
// Test 1: limiter inactive below step count
// ---------------------------------------------------------------------------

static void test_limiter_inactive_below_rate() {
	EventLimiterConfig cfg;
	cfg.enabled                     = true;
	cfg.max_console_events_per_step = 100; // high cap
	cfg.sample_rate                 = 1.0;

	EventLimiter lim(cfg);
	lim.reset_step();
	lim.begin_window();

	// Observe 10 events (well below cap of 100)
	for (int i = 0; i < 10; ++i) {
		auto ev = make_ev(AtomEventType::Collision);
		lim.observe(ev);
	}
	lim.end_window(1, 1e-12);

	// Should not be active
	auto st = lim.state();
	assert(st.total_events == 10);
	assert(st.collisions   == 10);
	std::puts("[PASS] test_limiter_inactive_below_rate");
}

// ---------------------------------------------------------------------------
// Test 2: per-step cap enforced
// ---------------------------------------------------------------------------

static void test_limiter_activates_above_step_count() {
	EventLimiterConfig cfg;
	cfg.enabled                     = true;
	cfg.max_console_events_per_step = 5;
	cfg.sample_rate                 = 1.0; // accept all when not active

	EventLimiter lim(cfg);
	lim.reset_step();
	lim.begin_window();

	int printed = 0;
	for (int i = 0; i < 20; ++i) {
		auto ev = make_ev(AtomEventType::EnergyUpdate);
		lim.observe(ev);
		if (lim.should_print(ev)) ++printed;
	}
	lim.end_window(1, 1e-12);

	// At most max_console_events_per_step should be printed
	assert(printed <= static_cast<int>(cfg.max_console_events_per_step));
	std::puts("[PASS] test_limiter_activates_above_step_count");
}

// ---------------------------------------------------------------------------
// Test 3: always-print bypasses cap
// ---------------------------------------------------------------------------

static void test_always_print_bypasses_limiter() {
	EventLimiterConfig cfg;
	cfg.enabled                     = true;
	cfg.max_console_events_per_step = 1; // very tight cap
	cfg.sample_rate                 = 0.0; // never sample otherwise

	EventLimiter lim(cfg);
	lim.reset_step();
	lim.begin_window();

	// First fill the per-step cap with energy updates
	for (int i = 0; i < 5; ++i) {
		auto ev = make_ev(AtomEventType::EnergyUpdate);
		lim.observe(ev);
		lim.should_print(ev); // consume the cap
	}

	// BondBroken is in always_print — must still print
	auto bond_ev = make_ev(AtomEventType::BondBroken);
	lim.observe(bond_ev);
	bool printed = lim.should_print(bond_ev);
	assert(printed);

	std::puts("[PASS] test_always_print_bypasses_limiter");
}

// ---------------------------------------------------------------------------
// Test 4: aggregate counters
// ---------------------------------------------------------------------------

static void test_aggregate_counts_correct() {
	EventLimiter lim;
	lim.reset_step();
	lim.begin_window();

	lim.observe(make_ev(AtomEventType::Collision));
	lim.observe(make_ev(AtomEventType::Collision));
	lim.observe(make_ev(AtomEventType::BondCreated));
	lim.observe(make_ev(AtomEventType::BondBroken));
	lim.observe(make_ev(AtomEventType::EnergyAnomaly));
	lim.observe(make_ev(AtomEventType::ForceSpike));

	lim.end_window(1, 1e-12);
	auto st = lim.state();

	assert(st.total_events   == 6);
	assert(st.collisions     == 2);
	assert(st.bond_created   == 1);
	assert(st.bond_broken    == 1);
	assert(st.energy_anomaly == 1);
	assert(st.force_spike    == 1);

	std::puts("[PASS] test_aggregate_counts_correct");
}

// ---------------------------------------------------------------------------
// Test 5: sample_rate reduces prints in active mode
// ---------------------------------------------------------------------------

static void test_sample_rate_reduces_prints() {
	EventLimiterConfig cfg;
	cfg.enabled                      = true;
	cfg.max_console_events_per_step  = 10000; // no per-step cap
	cfg.max_console_events_per_second = 1;    // force active
	cfg.sample_rate                  = 0.1;   // ~10% accepted
	cfg.always_print                 = {};    // disable always_print

	EventLimiter lim(cfg);
	lim.reset_step();
	lim.begin_window();

	// Observe enough to trigger rate limit
	for (int i = 0; i < 5000; ++i)
		lim.observe(make_ev(AtomEventType::EnergyUpdate));

	lim.end_window(1, 0.000001); // tiny wall time → huge rate

	int printed = 0;
	for (int i = 0; i < 1000; ++i) {
		auto ev = make_ev(AtomEventType::EnergyUpdate);
		if (lim.should_print(ev)) ++printed;
	}

	// In sample_rate=0.1, expect well under 50% printed
	assert(printed < 500);
	std::puts("[PASS] test_sample_rate_reduces_prints");
}

// ---------------------------------------------------------------------------
// Test 6: reset_step clears counters
// ---------------------------------------------------------------------------

static void test_reset_step_clears_counters() {
	EventLimiter lim;
	lim.reset_step();
	lim.begin_window();

	for (int i = 0; i < 10; ++i)
		lim.observe(make_ev(AtomEventType::Collision));
	lim.end_window(1, 1e-12);

	assert(lim.state().total_events == 10);

	lim.reset_step();
	assert(lim.state().total_events == 0);
	assert(lim.state().collisions   == 0);

	std::puts("[PASS] test_reset_step_clears_counters");
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main() {
	test_limiter_inactive_below_rate();
	test_limiter_activates_above_step_count();
	test_always_print_bypasses_limiter();
	test_aggregate_counts_correct();
	test_sample_rate_reduces_prints();
	test_reset_step_clears_counters();

	std::puts("\n[BRIDGE65] test_bridge65_event_limiter: ALL PASS");
	return 0;
}
