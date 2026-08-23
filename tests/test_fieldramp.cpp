/**
 * tests/test_fieldramp.cpp
 * ==========================
 * Group 55 — Formation FieldRamp (WO-VSIM-FORMATION-FIELDRAMP)
 *
 * Acceptance tests:
 *
 *   FR-01  field starts at from_field_V_A at t=0
 *   FR-02  field ends at to_field_V_A at t=duration
 *   FR-03  field axis "x" produces non-zero x component only
 *   FR-04  field axis "y" produces non-zero y component only
 *   FR-05  field value logged per stage frame (log_frame does not crash)
 *   FR-06  zero-duration stage clamps to from_field_V_A
 *   FR-07  negative duration returns ok=false
 */

#include "vsim/intent/field_ramp.hpp"
#include "vsim/vsim_document.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>

static vsim::FormationStage make_stage(double E0, double E1,
									   const std::string& axis = "z",
									   double dur_ps = 10.0)
{
	vsim::FormationStage s;
	s.kind          = vsim::FormationStageKind::FieldRamp;
	s.from_field_V_A = E0;
	s.to_field_V_A   = E1;
	s.field_axis     = axis;
	s.duration_ps    = dur_ps;
	return s;
}

static bool near(double a, double b, double tol = 1e-9) {
	return std::abs(a - b) < tol;
}

// ============================================================================

static bool test_FR_01() {
	auto s = make_stage(1.0, 5.0, "z", 10.0);
	auto r = vsim::FieldRampEvaluator::evaluate(s, 0.0);
	if (!r.ok || !near(r.magnitude, 1.0)) {
		std::printf("FAIL FR-01: E(t=0)=%.6f expected 1.0\n", r.magnitude);
		return false;
	}
	std::puts("PASS FR-01: field starts at from_field_V_A");
	return true;
}

static bool test_FR_02() {
	auto s = make_stage(1.0, 5.0, "z", 10.0);
	auto r = vsim::FieldRampEvaluator::evaluate(s, 10.0);
	if (!r.ok || !near(r.magnitude, 5.0)) {
		std::printf("FAIL FR-02: E(t=end)=%.6f expected 5.0\n", r.magnitude);
		return false;
	}
	std::puts("PASS FR-02: field ends at to_field_V_A");
	return true;
}

static bool test_FR_03() {
	auto s = make_stage(3.0, 3.0, "x", 5.0);
	auto r = vsim::FieldRampEvaluator::evaluate(s, 2.5);
	if (!near(r.field_V_A[0], 3.0) || !near(r.field_V_A[1], 0.0) || !near(r.field_V_A[2], 0.0)) {
		std::printf("FAIL FR-03: field_x axis gave (%.3f, %.3f, %.3f)\n",
			r.field_V_A[0], r.field_V_A[1], r.field_V_A[2]);
		return false;
	}
	std::puts("PASS FR-03: field_axis=x produces non-zero x only");
	return true;
}

static bool test_FR_04() {
	auto s = make_stage(2.0, 2.0, "y", 5.0);
	auto r = vsim::FieldRampEvaluator::evaluate(s, 1.0);
	if (!near(r.field_V_A[1], 2.0) || !near(r.field_V_A[0], 0.0) || !near(r.field_V_A[2], 0.0)) {
		std::printf("FAIL FR-04: field_y axis gave (%.3f, %.3f, %.3f)\n",
			r.field_V_A[0], r.field_V_A[1], r.field_V_A[2]);
		return false;
	}
	std::puts("PASS FR-04: field_axis=y produces non-zero y only");
	return true;
}

static bool test_FR_05() {
	auto s = make_stage(0.0, 4.0, "z", 8.0);
	bool ok = true;
	for (int i = 0; i <= 8; ++i) {
		double t = static_cast<double>(i);
		auto r = vsim::FieldRampEvaluator::evaluate(s, t);
		if (!r.ok) { ok = false; break; }
		vsim::FieldRampEvaluator::log_frame(i * 100, t, r);
	}
	if (!ok) {
		std::puts("FAIL FR-05: log_frame iteration failed");
		return false;
	}
	std::puts("PASS FR-05: field value logged per frame without crash");
	return true;
}

static bool test_FR_06() {
	auto s = make_stage(7.0, 2.0, "z", 0.0);
	// duration_ps == 0 → clamp to E0 for any t
	for (double t : {0.0, 1.0, 100.0}) {
		auto r = vsim::FieldRampEvaluator::evaluate(s, t);
		if (!r.ok || !near(r.magnitude, 7.0)) {
			std::printf("FAIL FR-06: t=%.1f  E=%.6f expected 7.0\n", t, r.magnitude);
			return false;
		}
	}
	std::puts("PASS FR-06: zero-duration clamps to from_field_V_A");
	return true;
}

static bool test_FR_07() {
	auto s = make_stage(1.0, 3.0, "z", -1.0);
	auto r = vsim::FieldRampEvaluator::evaluate(s, 0.0);
	if (r.ok) {
		std::puts("FAIL FR-07: negative duration should return ok=false");
		return false;
	}
	std::puts("PASS FR-07: negative duration fails clearly (ok=false)");
	return true;
}

// ============================================================================

int main() {
	std::puts("\n=== Group 55 — Formation FieldRamp ===\n");

	int pass = 0, fail = 0;
	auto run = [&](bool (*fn)()) {
		if (fn()) ++pass; else ++fail;
	};

	run(test_FR_01);
	run(test_FR_02);
	run(test_FR_03);
	run(test_FR_04);
	run(test_FR_05);
	run(test_FR_06);
	run(test_FR_07);

	std::printf("\n  Results: %d passed  %d failed\n\n", pass, fail);
	return (fail == 0) ? 0 : 1;
}
