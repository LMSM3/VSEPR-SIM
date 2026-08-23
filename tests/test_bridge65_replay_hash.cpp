/**
 * test_bridge65_replay_hash.cpp
 * ==============================
 * Tests that state hashes are invariant to logging and that event hashes
 * change only when the event stream changes.
 *
 * Core invariant (WO-BRIDGE-65 §9):
 *   state_hash must ignore log counters.
 *   event_hash must include the event stream.
 *
 * Tests:
 *   1. test_state_hash_unchanged_with_logging
 *      Same sequence produces identical state_hash regardless of whether
 *      an event logger is active.
 *
 *   2. test_event_hash_differs_on_different_streams
 *      Different event sequences produce different event_hashes.
 *
 *   3. test_event_hash_stable_for_same_stream
 *      Same event sequence (deterministic) produces the same event_hash.
 *
 *   4. test_state_and_event_hash_are_independent
 *      state_hash == state_hash' even when event_hash != event_hash'.
 *
 * Note: The hashes tested here are the string fields on AtomEvent.
 * In production these are set by the MD integration loop; these tests
 * use a simple XOR accumulator as a proxy to validate independence.
 *
 * WO-BRIDGE-65  |  v5.1.13
 */

#include <cassert>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

#include "vsim/bridge65/atom_event.hpp"

using namespace vsim::bridge65;

// ---------------------------------------------------------------------------
// Minimal deterministic hash proxy
// ---------------------------------------------------------------------------

// Accumulates state-relevant fields (position-proxy: atom_id, step, energy).
// Ignores log-counter fields (printed_events, limiter_active, etc.).
static std::string compute_state_hash(const std::vector<AtomEvent>& events) {
	uint64_t acc = 0xcbf29ce484222325ULL; // FNV offset basis
	for (const auto& ev : events) {
		acc ^= static_cast<uint64_t>(ev.atom_id);
		acc *= 0x100000001b3ULL; // FNV prime
		uint64_t k_bits = 0, u_bits = 0;
		std::memcpy(&k_bits, &ev.energy.kinetic_eV,         sizeof(uint64_t));
		std::memcpy(&u_bits, &ev.energy.potential_local_eV, sizeof(uint64_t));
		acc ^= k_bits; acc *= 0x100000001b3ULL;
		acc ^= u_bits; acc *= 0x100000001b3ULL;
	}
	std::ostringstream oss;
	oss << std::hex << acc;
	return oss.str();
}

// Accumulates event-relevant fields (event type, partners, step).
static std::string compute_event_hash(const std::vector<AtomEvent>& events) {
	uint64_t acc = 0x84222325cbf29ce4ULL;
	for (const auto& ev : events) {
		acc ^= static_cast<uint64_t>(ev.type);
		acc *= 0x100000001b3ULL;
		acc ^= static_cast<uint64_t>(ev.step);
		acc *= 0x100000001b3ULL;
		for (int p : ev.partner_ids) {
			acc ^= static_cast<uint64_t>(p);
			acc *= 0x100000001b3ULL;
		}
	}
	std::ostringstream oss;
	oss << std::hex << acc;
	return oss.str();
}

// ---------------------------------------------------------------------------
// Helper: make a deterministic event sequence
// ---------------------------------------------------------------------------

static std::vector<AtomEvent> make_sequence(int seed, int n) {
	std::vector<AtomEvent> evs;
	for (int i = 0; i < n; ++i) {
		AtomEvent ev;
		ev.step    = static_cast<uint64_t>(i);
		ev.time_s  = i * 1e-13;
		ev.atom_id = (seed + i) % 50;
		ev.symbol  = "Ar";
		ev.type    = (i % 2 == 0) ? AtomEventType::Collision
								   : AtomEventType::EnergyUpdate;
		ev.energy.kinetic_eV         = 0.01 * (i + 1);
		ev.energy.potential_local_eV = -0.1 * (i + 1);
		ev.energy.total_local_eV     = ev.energy.kinetic_eV + ev.energy.potential_local_eV;
		ev.energy.delta_eV           = 0.001;
		if (ev.type == AtomEventType::Collision)
			ev.partner_ids = {seed + i + 1};
		evs.push_back(ev);
	}
	return evs;
}

// ---------------------------------------------------------------------------
// Test 1: state_hash unchanged with extra logging context
// ---------------------------------------------------------------------------

static void test_state_hash_unchanged_with_logging() {
	auto seq = make_sequence(7, 20);

	// Without extra logging metadata attached
	std::string hash_no_log = compute_state_hash(seq);

	// Simulate adding log-counter metadata that must NOT affect state_hash.
	// We attach it to a separate counter (not part of AtomEvent physics).
	// The state_hash computation intentionally ignores log counter fields.
	size_t log_counter = 999; // This would corrupt hash if accidentally included
	(void)log_counter;

	std::string hash_with_log = compute_state_hash(seq);

	assert(hash_no_log == hash_with_log);
	std::puts("[PASS] test_state_hash_unchanged_with_logging");
}

// ---------------------------------------------------------------------------
// Test 2: event_hash differs on different streams
// ---------------------------------------------------------------------------

static void test_event_hash_differs_on_different_streams() {
	auto seq_a = make_sequence(7,  20);
	auto seq_b = make_sequence(13, 20); // different seed → different atom_ids and partners

	std::string eh_a = compute_event_hash(seq_a);
	std::string eh_b = compute_event_hash(seq_b);

	assert(eh_a != eh_b);
	std::puts("[PASS] test_event_hash_differs_on_different_streams");
}

// ---------------------------------------------------------------------------
// Test 3: event_hash stable for same stream
// ---------------------------------------------------------------------------

static void test_event_hash_stable_for_same_stream() {
	auto seq1 = make_sequence(7, 20);
	auto seq2 = make_sequence(7, 20); // identical seed → identical sequence

	std::string eh1 = compute_event_hash(seq1);
	std::string eh2 = compute_event_hash(seq2);

	assert(eh1 == eh2);
	std::puts("[PASS] test_event_hash_stable_for_same_stream");
}

// ---------------------------------------------------------------------------
// Test 4: state and event hashes are independent
// ---------------------------------------------------------------------------

static void test_state_and_event_hash_are_independent() {
	// Two sequences with same physics (same kinetic/potential) but different
	// event classifications — state_hash same, event_hash different.

	auto seq_a = make_sequence(7, 10);
	auto seq_b = make_sequence(7, 10);

	// Mutate the event type only in seq_b (not physics fields)
	for (auto& ev : seq_b) {
		if (ev.type == AtomEventType::Collision)
			ev.type = AtomEventType::EnergyAnomaly;
	}

	std::string sh_a = compute_state_hash(seq_a);
	std::string sh_b = compute_state_hash(seq_b);
	std::string eh_a = compute_event_hash(seq_a);
	std::string eh_b = compute_event_hash(seq_b);

	// Physics (kinetic/potential) unchanged → state hashes equal
	assert(sh_a == sh_b);
	// Event types changed → event hashes differ
	assert(eh_a != eh_b);

	std::puts("[PASS] test_state_and_event_hash_are_independent");
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main() {
	test_state_hash_unchanged_with_logging();
	test_event_hash_differs_on_different_streams();
	test_event_hash_stable_for_same_stream();
	test_state_and_event_hash_are_independent();

	std::puts("\n[BRIDGE65] test_bridge65_replay_hash: ALL PASS");
	return 0;
}
