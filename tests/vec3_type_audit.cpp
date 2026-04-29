// =============================================================================
// tests/vec3_type_audit.cpp — Day #56 Vec3 Unification Audit
// =============================================================================
// Static assertions that enforce the single-Vec3-type rule.
//
// This test is petty, hostile, and useful. The best kind of test.
//   - If atomistic::Vec3 is ever re-defined as a struct, this fails.
//   - If vsepr::ufx::Vec3 is ever re-defined as a struct, this fails.
//   - OnlineStats Welford accumulator is smoke-tested for correctness.
//   - StationarityGate is smoke-tested for basic gate logic.
//
// Build target: vec3_type_audit
// =============================================================================

#include <type_traits>
#include <cassert>
#include <cmath>

// Authoritative type
#include "core/math_vec3.hpp"

// Consumers that must alias, not re-define
#include "atomistic/core/state.hpp"
#include "v4/uff/ufx_material_record.hpp"

// Stats headers
#include "core/stats/online_stats.hpp"
#include "core/stats/stationarity_gate.hpp"

// ─── Vec3 alias audit ────────────────────────────────────────────────────────

static_assert(std::is_same_v<atomistic::Vec3, vsepr::Vec3>,
	"atomistic::Vec3 must be an alias for vsepr::Vec3 (Day #56)");

static_assert(std::is_same_v<vsepr::ufx::Vec3, vsepr::Vec3>,
	"vsepr::ufx::Vec3 must be an alias for vsepr::Vec3 (Day #56)");

// ─── OnlineStats smoke test ───────────────────────────────────────────────────

static void test_online_stats() {
	vsepr::OnlineStats s;
	assert(s.count() == 0);

	// Push known values: {1, 2, 3, 4, 5}
	// Mean = 3, variance = 2.5, stddev ≈ 1.5811
	for (double v : {1.0, 2.0, 3.0, 4.0, 5.0}) s.push(v);

	assert(s.count() == 5);

	const double mean_err = std::abs(s.mean - 3.0);
	assert(mean_err < 1e-12 && "OnlineStats mean wrong");

	const double var_err = std::abs(s.variance() - 2.5);
	assert(var_err < 1e-12 && "OnlineStats variance wrong");

	s.reset();
	assert(s.count() == 0);
}

// ─── StationarityGate smoke test ─────────────────────────────────────────────

static void test_stationarity_gate() {
	vsepr::StationarityGate gate;
	gate.min_samples = 10;
	gate.relative_tolerance = 0.01;  // 1%

	// Feed noisy values — gate should not open yet
	for (int i = 0; i < 9; ++i) gate.push(1.0 + 0.5 * (i % 3));
	assert(!gate.ready() && "Gate opened before min_samples");

	// Feed very stable values — gate should open
	vsepr::StationarityGate stable_gate;
	stable_gate.min_samples = 10;
	stable_gate.relative_tolerance = 0.01;
	for (int i = 0; i < 20; ++i) stable_gate.push(1.000 + 1e-6 * i);
	assert(stable_gate.ready() && "Gate did not open on stable signal");
}

// ─── Vec3 operator smoke test ────────────────────────────────────────────────

static void test_vec3_ops() {
	using vsepr::Vec3;
	Vec3 a{1.0, 0.0, 0.0};
	Vec3 b{0.0, 1.0, 0.0};

	Vec3 sum = a + b;
	assert(std::abs(sum.x - 1.0) < 1e-15);
	assert(std::abs(sum.y - 1.0) < 1e-15);

	Vec3 c = cross(a, b);
	assert(std::abs(c.z - 1.0) < 1e-15 && "cross product wrong");

	assert(std::abs(a.norm() - 1.0) < 1e-15 && "norm wrong");
	assert(std::abs(a.norm_sq() - 1.0) < 1e-15 && "norm_sq wrong");
}

int main() {
	test_vec3_ops();
	test_online_stats();
	test_stationarity_gate();
	return 0;
}
