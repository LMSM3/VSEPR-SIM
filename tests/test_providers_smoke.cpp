/**
 * test_providers_smoke.cpp  --  WO-83F/G/H/J provider interface smoke tests
 *
 * Verifies that all four provider types:
 *   - default to null / unavailable
 *   - return nullopt when not set
 *   - return values when a lambda is installed
 *   - ProviderSet aggregates all four
 */

#include "atomistic/classify/providers.hpp"

#include <cassert>
#include <cstdio>
#include <optional>

using namespace atomistic::classify;

// ---- BondOrderProvider ----------------------------------------------------

static void test_bond_order_null() {
	BondOrderProvider p = BondOrderProvider::null();
	assert(!p.available());
	assert(!p.get(0, 1).has_value());
}

static void test_bond_order_with_fn() {
	BondOrderProvider p;
	p.fn = [](std::size_t /*i*/, std::size_t /*j*/) -> std::optional<double> {
		return 1.5; // aromatic
	};
	assert(p.available());
	auto v = p.get(0, 1);
	assert(v.has_value());
	assert(*v == 1.5);
}

// ---- LonePairProvider -----------------------------------------------------

static void test_lone_pair_null() {
	LonePairProvider p = LonePairProvider::null();
	assert(!p.available());
	assert(!p.get(0).has_value());
}

static void test_lone_pair_nh3() {
	LonePairProvider p;
	p.fn = [](std::size_t i) -> std::optional<int> {
		return (i == 0) ? std::optional<int>(1) : std::optional<int>(0);
	};
	assert(p.available());
	assert(p.get(0) == 1);  // N has 1 lone pair
	assert(p.get(1) == 0);  // H has 0
}

// ---- FormalChargeProvider -------------------------------------------------

static void test_formal_charge_null() {
	FormalChargeProvider p = FormalChargeProvider::null();
	assert(!p.available());
	assert(!p.get(0).has_value());
}

static void test_formal_charge_ammonium() {
	FormalChargeProvider p;
	p.fn = [](std::size_t i) -> std::optional<int> {
		return (i == 0) ? std::optional<int>(+1) : std::nullopt;
	};
	assert(p.available());
	assert(p.get(0) == +1);
	assert(!p.get(1).has_value()); // H has no charge set
}

// ---- RingProvider ---------------------------------------------------------

static void test_ring_null() {
	RingProvider p = RingProvider::null();
	assert(!p.available());
	assert(p.ring_sizes(0).empty());
	assert(!p.total_rings().has_value());
	assert(!p.in_ring(0));
	assert(!p.in_strained_ring(0));
}

static void test_ring_with_provider() {
	RingProvider p;
	p.ring_sizes_for_atom = [](std::size_t i) -> std::vector<int> {
		if (i < 6) return {6}; // benzene ring
		return {};
	};
	p.total_ring_count = []() -> std::optional<int> { return 1; };

	assert(p.available());
	assert(!p.ring_sizes(0).empty());
	assert(p.in_ring(0));
	assert(!p.in_strained_ring(0));   // 6-membered is not strained
	assert(p.total_rings() == 1);
	assert(!p.in_ring(7));            // atom outside ring
}

static void test_ring_strained() {
	RingProvider p;
	p.ring_sizes_for_atom = [](std::size_t i) -> std::vector<int> {
		if (i < 3) return {3}; // cyclopropane
		return {};
	};
	assert(p.in_strained_ring(0));
	assert(!p.in_strained_ring(4));
}

// ---- ProviderSet ----------------------------------------------------------

static void test_provider_set_all_null() {
	ProviderSet ps = ProviderSet::null();
	assert(!ps.bond_order.available());
	assert(!ps.lone_pair.available());
	assert(!ps.formal_charge.available());
	assert(!ps.ring.available());
}

static void test_provider_set_mixed() {
	ProviderSet ps;
	ps.bond_order.fn = [](std::size_t, std::size_t) -> std::optional<double> {
		return 2.0;
	};
	// Others remain null
	assert(ps.bond_order.available());
	assert(!ps.lone_pair.available());
	assert(!ps.formal_charge.available());
	assert(!ps.ring.available());
	assert(ps.bond_order.get(0, 1) == 2.0);
}

// ---------------------------------------------------------------------------

int main() {
	test_bond_order_null();
	test_bond_order_with_fn();
	test_lone_pair_null();
	test_lone_pair_nh3();
	test_formal_charge_null();
	test_formal_charge_ammonium();
	test_ring_null();
	test_ring_with_provider();
	test_ring_strained();
	test_provider_set_all_null();
	test_provider_set_mixed();

	std::puts("test_providers_smoke: PASS");
	return 0;
}
