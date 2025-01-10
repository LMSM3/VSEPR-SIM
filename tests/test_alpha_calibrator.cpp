/**
 * test_alpha_calibrator.cpp
 * =========================
 * v2.6.1 — AlphaCalibrator validation suite.
 *
 * Tests:
 *   C1  Accept a value within 1% — override is stored, get() returns it.
 *   C2  Reject a value outside 1% — override unchanged, false returned.
 *   C3  Progressive refinement — each accepted update is checked against
 *       the override (not the original model), converging step-by-step.
 *   C4  reset(Z) reverts to model; reset_all() clears everything.
 *   C5  set_override() bypasses threshold (forced assignment).
 *   C6  delta_pct is correctly computed and always non-negative.
 *   C7  update_count increments on each accepted call, not on rejection.
 *   C8  alpha_predict_calibrated() is an exact alias for cal.get().
 *   C9  global_calibrator() returns a stable singleton per-process.
 *   C10 Snapshot / restore round-trips cleanly.
 *   C11 Boundary: Z=0 and Z=119 are rejected gracefully.
 *   C12 Full periodic table: calibrate every element at model+0.5%,
 *       confirm all 118 are accepted, all values within 0.5% of model.
 *   C13 Threshold customisation: 0.005 (0.5%) rejects a 0.8% delta.
 *   C14 override_count() tracks correctly.
 */

#include "atomistic/models/alpha_calibrator.hpp"
#include "atomistic/models/alpha_model.hpp"
#include <cmath>
#include <cstdio>

using namespace atomistic::polarization;

// ── minimal harness ──────────────────────────────────────────────────────────

static int g_pass = 0, g_fail = 0;

#define CHECK(cond, msg) do { \
    if (cond) { ++g_pass; } \
    else { ++g_fail; \
           std::fprintf(stderr, "  FAIL [line %d]: %s\n", __LINE__, (msg)); } \
} while(0)

#define SECTION(s) std::printf("\n[%s]\n", (s))

// ── helpers ──────────────────────────────────────────────────────────────────

static double nudge(double val, double frac) { return val * (1.0 + frac); }

// ─────────────────────────────────────────────────────────────────────────────

static void test_C1_accept_within_1pct() {
    SECTION("C1: Accept value within 1%");
    AlphaCalibrator cal;
    double base = alpha_predict(8);           // O: ~0.802
    double new_val = nudge(base, +0.005);     // +0.5%

    auto r = cal.calibrate(8, new_val);
    std::printf("  O: model=%.4f  candidate=%.4f  delta=%.3f%%  accepted=%s\n",
                base, new_val, r.delta_pct, r.accepted ? "yes" : "no");

    CHECK(r.accepted,               "value within 1% accepted");
    CHECK(cal.is_overridden(8),     "Z=8 marked as overridden");
    CHECK(std::abs(cal.get(8) - new_val) < 1e-12, "get(8) returns new value");
    CHECK(r.update_count == 1,      "update_count == 1 after first accept");
}

static void test_C2_reject_outside_1pct() {
    SECTION("C2: Reject value outside 1%");
    AlphaCalibrator cal;
    double base = alpha_predict(6);           // C: 1.76
    double new_val = nudge(base, +0.05);      // +5%

    auto r = cal.calibrate(6, new_val);
    std::printf("  C: model=%.4f  candidate=%.4f  delta=%.3f%%  accepted=%s\n",
                base, new_val, r.delta_pct, r.accepted ? "yes" : "no");

    CHECK(!r.accepted,              "value outside 1% rejected");
    CHECK(!cal.is_overridden(6),    "Z=6 NOT marked as overridden");
    CHECK(std::abs(cal.get(6) - base) < 1e-10, "get(6) unchanged (model value)");
    CHECK(r.update_count == 0,      "update_count == 0 after rejection");
}

static void test_C3_progressive_refinement() {
    SECTION("C3: Progressive refinement — checked against override, not model");
    AlphaCalibrator cal;
    double v0 = alpha_predict(11);  // Na: ~24.1

    // Step 1: +0.5% from model
    double v1 = nudge(v0, +0.005);
    auto r1 = cal.calibrate(11, v1);
    CHECK(r1.accepted, "step 1 (+0.5%) accepted");

    // Step 2: +0.5% from v1 (cumulative +1.0% from model)
    // Would be rejected if checked against model, but should be accepted
    // because it is within 1% of the current override v1.
    double v2 = nudge(v1, +0.005);
    auto r2 = cal.calibrate(11, v2);
    std::printf("  Na model=%.4f  v1=%.4f  v2=%.4f\n", v0, v1, v2);
    std::printf("  v2 vs v1: %.3f%%  v2 vs model: %.3f%%\n",
                r2.delta_pct,
                std::abs(v2 - v0) / v0 * 100.0);
    CHECK(r2.accepted, "step 2 (+0.5% from override) accepted");
    CHECK(r2.update_count == 2, "update_count == 2 after two accepts");
    CHECK(std::abs(cal.get(11) - v2) < 1e-12, "get(11) == v2 after step 2");

    // Step 3: try to jump 2% from current — rejected
    double v3 = nudge(cal.get(11), +0.02);
    auto r3 = cal.calibrate(11, v3);
    CHECK(!r3.accepted, "step 3 (+2% from override) rejected");
    CHECK(r3.update_count == 2, "update_count still 2 after rejection");
}

static void test_C4_reset() {
    SECTION("C4: reset(Z) and reset_all()");
    AlphaCalibrator cal;
    double base_N  = alpha_predict(7);
    double base_Na = alpha_predict(11);

    cal.calibrate(7,  nudge(base_N,  0.005));
    cal.calibrate(11, nudge(base_Na, 0.005));
    CHECK(cal.override_count() == 2, "two overrides set");

    cal.reset(7);
    CHECK(!cal.is_overridden(7),    "Z=7 cleared by reset(7)");
    CHECK(cal.is_overridden(11),    "Z=11 still set after reset(7)");
    CHECK(std::abs(cal.get(7) - base_N) < 1e-10, "get(7) reverts to model");

    cal.reset_all();
    CHECK(cal.override_count() == 0, "all overrides cleared by reset_all()");
    CHECK(!cal.is_overridden(11),   "Z=11 cleared by reset_all()");
}

static void test_C5_set_override_bypass() {
    SECTION("C5: set_override() bypasses threshold");
    AlphaCalibrator cal;
    double base = alpha_predict(79);   // Au: ~5.8
    double forced = base * 2.0;        // 100% delta — would be rejected by calibrate()

    auto r = cal.calibrate(79, forced);
    CHECK(!r.accepted, "calibrate() rejects 100% delta");

    cal.set_override(79, forced);
    CHECK(cal.is_overridden(79),         "set_override() forces it in");
    CHECK(std::abs(cal.get(79) - forced) < 1e-12, "get(79) == forced value");
    CHECK(cal.update_count(79) == 0,     "update_count stays 0 for forced set");
}

static void test_C6_delta_pct_sign_and_magnitude() {
    SECTION("C6: delta_pct is always non-negative");
    AlphaCalibrator cal;
    double base = alpha_predict(16);   // S: ~2.90

    auto r_pos = cal.calibrate(16, nudge(base, +0.008));
    CHECK(r_pos.delta_pct >= 0.0, "delta_pct non-negative for positive nudge");
    CHECK(std::abs(r_pos.delta_pct - 0.8) < 0.05, "delta_pct ~0.8% for +0.8% nudge");

    cal.reset(16);
    auto r_neg = cal.calibrate(16, nudge(base, -0.008));
    CHECK(r_neg.delta_pct >= 0.0, "delta_pct non-negative for negative nudge");
    CHECK(std::abs(r_neg.delta_pct - 0.8) < 0.05, "delta_pct ~0.8% for -0.8% nudge");
}

static void test_C7_update_count() {
    SECTION("C7: update_count increments only on accepts");
    AlphaCalibrator cal;
    double v = alpha_predict(20);  // Ca

    for (int i = 0; i < 5; ++i) {
        double candidate = nudge(cal.get(20), +0.003);   // +0.3% each time
        cal.calibrate(20, candidate);
    }
    CHECK(cal.update_count(20) == 5, "update_count == 5 after 5 accepted calls");

    // one rejection doesn't increment
    cal.calibrate(20, cal.get(20) * 1.5);
    CHECK(cal.update_count(20) == 5, "update_count unchanged after rejection");
}

static void test_C8_alpha_predict_calibrated_alias() {
    SECTION("C8: alpha_predict_calibrated() == cal.get()");
    AlphaCalibrator cal;
    double base = alpha_predict(29);  // Cu
    double new_val = nudge(base, +0.007);
    cal.calibrate(29, new_val);

    for (uint32_t Z = 1; Z <= 118; ++Z) {
        double via_free  = alpha_predict_calibrated(Z, cal);
        double via_get   = cal.get(Z);
        if (std::abs(via_free - via_get) > 1e-15) {
            std::fprintf(stderr, "  FAIL Z=%u: free=%.8f get=%.8f\n",
                         Z, via_free, via_get);
            ++g_fail;
            return;
        }
    }
    ++g_pass;
    std::printf("  alpha_predict_calibrated(Z) == cal.get(Z) for all Z=1-118\n");
}

static void test_C9_global_calibrator() {
    SECTION("C9: global_calibrator() returns stable singleton");
    AlphaCalibrator& g1 = global_calibrator();
    AlphaCalibrator& g2 = global_calibrator();
    CHECK(&g1 == &g2, "global_calibrator returns same instance");

    g1.reset_all();  // clean state for this test
    double base = alpha_predict(1);  // H
    g1.calibrate(1, nudge(base, 0.005));
    CHECK(g2.is_overridden(1), "override set via g1 visible via g2 (same object)");
    g1.reset_all();
}

static void test_C10_snapshot_restore() {
    SECTION("C10: Snapshot / restore round-trip");
    AlphaCalibrator cal;
    cal.calibrate(1,  nudge(alpha_predict(1),  0.005));
    cal.calibrate(6,  nudge(alpha_predict(6),  0.005));
    cal.calibrate(79, nudge(alpha_predict(79), 0.005));

    auto snap = cal.snapshot();
    CHECK(snap.active[1]  && snap.active[6]  && snap.active[79], "snapshot captures active flags");

    cal.reset_all();
    CHECK(cal.override_count() == 0, "cleared before restore");

    cal.restore(snap);
    CHECK(cal.override_count() == 3, "restore brings back 3 overrides");
    CHECK(cal.is_overridden(79), "Z=79 active after restore");
    CHECK(std::abs(cal.get(1) - snap.overrides[1]) < 1e-12, "restored value correct");
}

static void test_C11_boundary_Z() {
    SECTION("C11: Boundary Z=0 and Z=119 rejected gracefully");
    AlphaCalibrator cal;
    auto r0   = cal.calibrate(0,   1.0);
    auto r119 = cal.calibrate(119, 1.0);
    CHECK(!r0.accepted,   "Z=0 rejected");
    CHECK(!r119.accepted, "Z=119 rejected");
    CHECK(cal.override_count() == 0, "no overrides created for out-of-range Z");

    // Negative alpha also rejected
    auto r_neg = cal.calibrate(6, -1.0);
    CHECK(!r_neg.accepted, "negative alpha rejected");
}

static void test_C12_full_table_0pt5pct_nudge() {
    SECTION("C12: All Z=1-118 accepted at +0.5% nudge");
    AlphaCalibrator cal;
    int accepted = 0, rejected = 0;

    for (uint32_t Z = 1; Z <= 118; ++Z) {
        double base = alpha_predict(Z);
        auto r = cal.calibrate(Z, nudge(base, +0.005));
        if (r.accepted) ++accepted;
        else {
            ++rejected;
            std::fprintf(stderr, "  FAIL Z=%u: +0.5%% nudge rejected (delta=%.3f%%)\n",
                         Z, r.delta_pct);
        }
    }
    std::printf("  Accepted: %d / 118\n", accepted);
    CHECK(accepted == 118, "all 118 elements accept a +0.5% nudge");
    CHECK(cal.override_count() == 118, "118 overrides active");

    // Verify values are within 0.5% of model
    int within = 0;
    for (uint32_t Z = 1; Z <= 118; ++Z) {
        double model  = alpha_predict(Z);
        double stored = cal.get(Z);
        if (std::abs(stored - model) / model * 100.0 <= 0.51) ++within;
    }
    CHECK(within == 118, "all overrides within 0.51% of model");
}

static void test_C13_custom_threshold() {
    SECTION("C13: Custom threshold 0.5% rejects 0.8% delta");
    AlphaCalibrator cal(0.005);   // 0.5% threshold
    double base = alpha_predict(17);  // Cl: ~2.18

    auto r_ok  = cal.calibrate(17, nudge(base, +0.004));  // 0.4% — accept
    auto r_bad = cal.calibrate(17, nudge(base, +0.008));  // 0.8% — reject
    // Note: r_bad is checked against the updated override (after r_ok), but
    // the delta from the override is still 0.8% - 0.4% ≈ 0.4%, so we need
    // to check from a fresh cal to get a clean rejection.
    AlphaCalibrator cal2(0.005);
    auto r_bad2 = cal2.calibrate(17, nudge(base, +0.008));  // 0.8% from model — reject

    CHECK(r_ok.accepted,   "0.4% accepted with 0.5% threshold");
    CHECK(!r_bad2.accepted, "0.8% rejected with 0.5% threshold");
}

static void test_C14_override_count() {
    SECTION("C14: override_count() tracks correctly");
    AlphaCalibrator cal;
    CHECK(cal.override_count() == 0, "starts at 0");

    cal.calibrate(1,  nudge(alpha_predict(1),  0.005));
    cal.calibrate(2,  nudge(alpha_predict(2),  0.005));
    cal.calibrate(3,  nudge(alpha_predict(3),  0.005));
    CHECK(cal.override_count() == 3, "count == 3 after 3 accepted");

    // Rejection doesn't change count
    cal.calibrate(4, alpha_predict(4) * 10.0);
    CHECK(cal.override_count() == 3, "count unchanged after rejection");

    cal.reset(1);
    CHECK(cal.override_count() == 2, "count decrements after reset(1)");
    cal.reset_all();
    CHECK(cal.override_count() == 0, "count == 0 after reset_all()");
}

// ─────────────────────────────────────────────────────────────────────────────

int main() {
    std::printf("============================================================\n");
    std::printf(" v2.6.1 — AlphaCalibrator (1%% auto-override gate)\n");
    std::printf("============================================================\n");

    test_C1_accept_within_1pct();
    test_C2_reject_outside_1pct();
    test_C3_progressive_refinement();
    test_C4_reset();
    test_C5_set_override_bypass();
    test_C6_delta_pct_sign_and_magnitude();
    test_C7_update_count();
    test_C8_alpha_predict_calibrated_alias();
    test_C9_global_calibrator();
    test_C10_snapshot_restore();
    test_C11_boundary_Z();
    test_C12_full_table_0pt5pct_nudge();
    test_C13_custom_threshold();
    test_C14_override_count();

    const int total = g_pass + g_fail;
    std::printf("\n============================================================\n");
    std::printf("  Result:  %d / %d passed", g_pass, total);
    if (g_fail == 0) std::printf("  ALL PASS\n");
    else             std::printf("  %d FAILED\n", g_fail);
    std::printf("============================================================\n");
    return (g_fail == 0) ? 0 : 1;
}
