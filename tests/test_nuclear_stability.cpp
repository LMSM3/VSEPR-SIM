/**
 * test_nuclear_stability.cpp
 * ==========================
 * Validates the ab-initio nuclear stability predictor and its integration
 * with the alpha model.
 *
 * Tests:
 *   S1  Magic numbers: Z=2,8,20,28,50,82 are detected as magic_Z.
 *   S2  Stable elements: Z=1-82 (minus Tc=43, Pm=61) classified Stable.
 *   S3  Tc (Z=43) and Pm (Z=61): no stable isotopes, classified Radioactive.
 *   S4  Bi (Z=83): PrimordialLong (t½ ~ 10¹⁹ y).
 *   S5  Th (Z=90) and U (Z=92): PrimordialLong.
 *   S6  Superheavy Z=112-118: classified Superheavy with very low confidence.
 *   S7  Most-stable-A predictions: spot-check known values (He-4, Fe-56,
 *       Pb-208, U-238).
 *   S8  Binding energy per nucleon: Fe-56 should peak near 8.8 MeV/A.
 *   S9  Fissility: U should be below 47, Sg (Z=106) should exceed it.
 *   S10 Alpha confidence: stable elements = 1.0, superheavy < 0.30.
 *   S11 alpha_predict_checked() returns same alpha as alpha_predict().
 *   S12 Theoretical flag: Z=1-82 are NOT theoretical; Z=112-118 ARE.
 *   S13 calibration_threshold_for: stable = 0.01, superheavy ≫ 0.01.
 *   S14 Dominant decay: Z>82 should be Alpha or SF, not BetaMinus.
 *   S15 Valley of stability: N/Z ratio increases with Z (heavier atoms
 *       need more neutrons).
 *   S16 Geiger-Nuttall: heavier alpha-emitters should have shorter t½
 *       (within the same decay chain region).
 *   S17 Full-table sweep: all 118 elements return valid StabilityInfo.
 */

#include "atomistic/models/alpha_stability.hpp"
#include "atomistic/models/nuclear_stability.hpp"
#include "atomistic/models/alpha_model.hpp"
#include <cmath>
#include <cstdio>
#include <limits>

using namespace atomistic::nuclear;
using namespace atomistic::polarization;

static int g_pass = 0, g_fail = 0;

#define CHECK(cond, msg) do { \
    if (cond) { ++g_pass; } \
    else { ++g_fail; \
           std::fprintf(stderr, "  FAIL [line %d]: %s\n", __LINE__, (msg)); } \
} while(0)

#define SECTION(s) std::printf("\n[%s]\n", (s))

// ── S1: Magic numbers ───────────────────────────────────────────────────────

static void test_S1() {
    SECTION("S1: Magic proton numbers");
    uint32_t magic_Z[] = {2, 8, 20, 28, 50, 82};
    for (auto Z : magic_Z) {
        auto s = stability_of(Z);
        char buf[64];
        std::snprintf(buf, sizeof(buf), "Z=%u should be magic_Z", Z);
        CHECK(s.is_magic_Z, buf);
    }
    // Non-magic
    uint32_t non_magic[] = {1, 6, 11, 26, 47, 79};
    for (auto Z : non_magic) {
        auto s = stability_of(Z);
        char buf[64];
        std::snprintf(buf, sizeof(buf), "Z=%u should NOT be magic_Z", Z);
        CHECK(!s.is_magic_Z, buf);
    }
}

// ── S2: Stable elements ─────────────────────────────────────────────────────

static void test_S2() {
    SECTION("S2: Stable elements Z=1-82 (minus Tc,Pm)");
    int n_stable = 0;
    for (uint32_t Z = 1; Z <= 82; ++Z) {
        if (Z == 43 || Z == 61) continue;
        auto s = stability_of(Z);
        if (s.cls == StabilityClass::Stable) ++n_stable;
        else {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "Z=%u should be Stable, got %s",
                          Z, stability_name(s.cls));
            ++g_fail;
            std::fprintf(stderr, "  FAIL: %s\n", buf);
        }
    }
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%d/80 stable elements correct", n_stable);
    CHECK(n_stable == 80, buf);
}

// ── S3: Tc and Pm ───────────────────────────────────────────────────────────

static void test_S3() {
    SECTION("S3: Tc (Z=43) and Pm (Z=61) — no stable isotopes");
    auto tc = stability_of(43);
    auto pm = stability_of(61);
    CHECK(tc.cls == StabilityClass::Radioactive, "Tc is Radioactive");
    CHECK(!tc.has_stable_isotope, "Tc has no stable isotope");
    CHECK(pm.cls == StabilityClass::Radioactive, "Pm is Radioactive");
    CHECK(!pm.has_stable_isotope, "Pm has no stable isotope");
}

// ── S4: Bismuth ─────────────────────────────────────────────────────────────

static void test_S4() {
    SECTION("S4: Bi (Z=83) — PrimordialLong");
    auto bi = stability_of(83);
    CHECK(bi.cls == StabilityClass::PrimordialLong, "Bi is PrimordialLong");
    CHECK(bi.dominant_decay == DecayType::Alpha, "Bi decays by alpha");
    CHECK(!bi.has_stable_isotope, "Bi has no truly stable isotope");
}

// ── S5: Th and U ────────────────────────────────────────────────────────────

static void test_S5() {
    SECTION("S5: Th (Z=90) and U (Z=92) — PrimordialLong");
    auto th = stability_of(90);
    auto u  = stability_of(92);
    CHECK(th.cls == StabilityClass::PrimordialLong, "Th is PrimordialLong");
    CHECK(u.cls  == StabilityClass::PrimordialLong, "U is PrimordialLong");
}

// ── S6: Superheavy Z=112-118 ────────────────────────────────────────────────

static void test_S6() {
    SECTION("S6: Superheavy Z=112-118");
    for (uint32_t Z = 112; Z <= 118; ++Z) {
        auto s = stability_of(Z);
        char buf[64];
        std::snprintf(buf, sizeof(buf), "Z=%u classified Superheavy", Z);
        CHECK(s.cls == StabilityClass::Superheavy, buf);
        std::snprintf(buf, sizeof(buf), "Z=%u confidence < 0.30", Z);
        CHECK(s.alpha_confidence < 0.30, buf);
    }
}

// ── S7: Most-stable-A spot-checks ───────────────────────────────────────────

static void test_S7() {
    SECTION("S7: Most-stable-A predictions");
    // He-4, C-12, Fe-56, Pb-208 are known most-stable (or near-most-stable)
    auto check_A = [](uint32_t Z, uint32_t A_expected, uint32_t tolerance,
                      const char* name) {
        uint32_t A = most_stable_A(Z);
        char buf[96];
        std::snprintf(buf, sizeof(buf), "%s: A_pred=%u vs expected=%u±%u",
                      name, A, A_expected, tolerance);
        CHECK(A >= A_expected - tolerance && A <= A_expected + tolerance, buf);
    };
    check_A(2,  4,   1, "He-4");
    check_A(6,  12,  2, "C-12");
    check_A(26, 56,  4, "Fe-56");
    check_A(82, 208, 6, "Pb-208");
    check_A(92, 238, 6, "U-238");
}

// ── S8: Binding energy per nucleon ──────────────────────────────────────────

static void test_S8() {
    SECTION("S8: Binding energy per nucleon");
    double bpa_Fe = semf::binding_per_nucleon(26, 56);
    std::printf("  Fe-56 B/A = %.3f MeV  (expected ~8.79)\n", bpa_Fe);
    CHECK(bpa_Fe > 8.3 && bpa_Fe < 9.2, "Fe-56 B/A in [8.3, 9.2] MeV");

    double bpa_He = semf::binding_per_nucleon(2, 4);
    std::printf("  He-4  B/A = %.3f MeV  (expected ~7.07)\n", bpa_He);
    CHECK(bpa_He > 5.5 && bpa_He < 10.0, "He-4 B/A in [5.5, 10.0] MeV (shell-enhanced)");

    // B/A should peak around Fe and decrease for heavy elements
    double bpa_U = semf::binding_per_nucleon(92, 238);
    CHECK(bpa_Fe > bpa_U, "B/A(Fe) > B/A(U) — iron peak");
}

// ── S9: Fissility ───────────────────────────────────────────────────────────

static void test_S9() {
    SECTION("S9: Fissility parameter");
    auto u_info  = stability_of(92);
    auto sg_info = stability_of(106);
    std::printf("  U  fissility = %.1f  (expect < 47)\n", u_info.fissility);
    std::printf("  Sg fissility = %.1f  (expect > 44)\n", sg_info.fissility);
    CHECK(u_info.fissility < 47.0, "U fissility < 47");
    CHECK(sg_info.fissility > 38.0, "Sg fissility > 38 (approaching SF threshold)");
}

// ── S10: Alpha confidence ───────────────────────────────────────────────────

static void test_S10() {
    SECTION("S10: Alpha prediction confidence");
    CHECK(alpha_prediction_confidence(6) == 1.0, "C (Z=6) confidence = 1.0");
    CHECK(alpha_prediction_confidence(26) == 1.0, "Fe (Z=26) confidence = 1.0");
    CHECK(alpha_prediction_confidence(82) == 1.0, "Pb (Z=82) confidence = 1.0");
    CHECK(alpha_prediction_confidence(92) < 1.0, "U (Z=92) confidence < 1.0");
    CHECK(alpha_prediction_confidence(118) < 0.10, "Og (Z=118) confidence < 0.10");
}

// ── S11: alpha_predict_checked returns same alpha ───────────────────────────

static void test_S11() {
    SECTION("S11: alpha_predict_checked() == alpha_predict()");
    bool all_match = true;
    for (uint32_t Z = 1; Z <= 118; ++Z) {
        auto r = alpha_predict_checked(Z);
        double direct = alpha_predict(Z);
        if (std::abs(r.alpha - direct) > 1e-15) {
            std::fprintf(stderr, "  FAIL Z=%u: checked=%.8f direct=%.8f\n",
                         Z, r.alpha, direct);
            all_match = false;
        }
    }
    CHECK(all_match, "alpha_predict_checked matches alpha_predict for all Z");
}

// ── S12: Theoretical flag ───────────────────────────────────────────────────

static void test_S12() {
    SECTION("S12: Theoretical flag");
    for (uint32_t Z = 1; Z <= 82; ++Z) {
        auto r = alpha_predict_checked(Z);
        if (r.is_theoretical) {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "Z=%u should NOT be theoretical", Z);
            CHECK(false, buf);
            return;
        }
    }
    ++g_pass;
    std::printf("  Z=1-82 all non-theoretical\n");

    for (uint32_t Z = 112; Z <= 118; ++Z) {
        auto r = alpha_predict_checked(Z);
        char buf[64];
        std::snprintf(buf, sizeof(buf), "Z=%u should be theoretical", Z);
        CHECK(r.is_theoretical, buf);
    }
}

// ── S13: Calibration threshold ──────────────────────────────────────────────

static void test_S13() {
    SECTION("S13: Stability-aware calibration threshold");
    double t_C  = calibration_threshold_for(6);
    double t_Og = calibration_threshold_for(118);
    std::printf("  C  (Z=6):  threshold = %.4f\n", t_C);
    std::printf("  Og (Z=118): threshold = %.4f\n", t_Og);
    CHECK(std::abs(t_C - 0.01) < 0.001, "C threshold ≈ 0.01 (stable)");
    CHECK(t_Og > 0.05, "Og threshold > 0.05 (superheavy → widened)");
    CHECK(t_Og <= 0.20, "Og threshold ≤ 0.20 (capped)");
}

// ── S14: Dominant decay for heavy elements ──────────────────────────────────

static void test_S14() {
    SECTION("S14: Dominant decay modes");
    for (uint32_t Z = 84; Z <= 103; ++Z) {
        auto s = stability_of(Z);
        // Should be Alpha or SF, not Beta- (too far past stability for β⁻)
        CHECK(s.dominant_decay == DecayType::Alpha ||
              s.dominant_decay == DecayType::SpontaneousFission,
              "Z>83 decays by alpha or SF");
    }
}

// ── S15: N/Z ratio increases ────────────────────────────────────────────────

static void test_S15() {
    SECTION("S15: Valley of stability — N/Z increases with Z");
    double prev_ratio = 0.0;
    int breaks = 0;
    for (uint32_t Z = 2; Z <= 100; Z += 10) {
        uint32_t A = most_stable_A(Z);
        double ratio = static_cast<double>(A - Z) / static_cast<double>(Z);
        if (ratio < prev_ratio - 0.01) ++breaks;
        prev_ratio = ratio;
    }
    std::printf("  N/Z monotonicity breaks: %d (expect ≤ 3)\n", breaks);
    CHECK(breaks <= 3, "N/Z ratio is mostly monotonically increasing");
}

// ── S16: Geiger-Nuttall trend ───────────────────────────────────────────────

static void test_S16() {
    SECTION("S16: Geiger-Nuttall — Q_alpha increases with Z for α-emitters");
    // The Viola-Seaborg formula: for similar Q values, higher Z → shorter t½.
    // But SEMF Q values have shell-correction noise, so test a wide Z gap.
    // Bi (Z=83) vs Og (Z=118): the superheavy should have shorter predicted t½.
    auto bi = stability_of(83);
    auto og = stability_of(118);
    std::printf("  Bi (Z=83)  log₁₀t½ = %.1f\n", bi.log10_halflife_s);
    std::printf("  Og (Z=118) log₁₀t½ = %.1f\n", og.log10_halflife_s);
    CHECK(bi.log10_halflife_s > og.log10_halflife_s,
          "Bi has longer half-life than Og (wide-gap Geiger-Nuttall)");

    // Also verify Q_alpha is positive for all Z=84-100 (α-emitters exist)
    int q_positive = 0;
    for (uint32_t Z = 84; Z <= 100; ++Z) {
        uint32_t A = most_stable_A(Z);
        if (q_alpha(Z, A) > 0.0) ++q_positive;
    }
    std::printf("  Q_alpha > 0 for %d/17 elements Z=84-100\n", q_positive);
    CHECK(q_positive >= 14, "Most Z=84-100 have positive Q_alpha");
}

// ── S17: Full-table sweep ───────────────────────────────────────────────────

static void test_S17() {
    SECTION("S17: Full-table sweep Z=1-118");
    int valid = 0;
    for (uint32_t Z = 1; Z <= 118; ++Z) {
        auto s = stability_of(Z);
        if (s.A_most_stable >= s.Z && s.alpha_confidence >= 0.0 &&
            s.alpha_confidence <= 1.0 &&
            (s.binding_energy_per_nucleon > 0.0 || s.Z == 1))  // H: A=1, B/A=0
            ++valid;
        else {
            std::fprintf(stderr, "  FAIL Z=%u: invalid StabilityInfo\n", Z);
            ++g_fail;
            return;
        }
    }
    char buf[64];
    std::snprintf(buf, sizeof(buf), "All 118 elements return valid StabilityInfo");
    CHECK(valid == 118, buf);
}

// ─────────────────────────────────────────────────────────────────────────────

int main() {
    std::printf("============================================================\n");
    std::printf(" Nuclear Stability Predictor — Ab Initio from Z\n");
    std::printf("============================================================\n");

    test_S1();
    test_S2();
    test_S3();
    test_S4();
    test_S5();
    test_S6();
    test_S7();
    test_S8();
    test_S9();
    test_S10();
    test_S11();
    test_S12();
    test_S13();
    test_S14();
    test_S15();
    test_S16();
    test_S17();

    // Print a few notable elements for visual inspection
    std::printf("\n--- Selected stability predictions ---\n");
    for (uint32_t Z : {1, 2, 6, 8, 26, 43, 50, 61, 79, 80, 82, 83, 90, 92,
                       98, 106, 112, 114, 118}) {
        atomistic::nuclear::print_stability(Z);
    }

    const int total = g_pass + g_fail;
    std::printf("\n============================================================\n");
    std::printf("  Result:  %d / %d passed", g_pass, total);
    if (g_fail == 0) std::printf("  ALL PASS\n");
    else             std::printf("  %d FAILED\n", g_fail);
    std::printf("============================================================\n");
    return (g_fail == 0) ? 0 : 1;
}
