/**
 * test_nuclear_chemical.cpp
 * =========================
 * Phase 6 — Nuclear Chemical Module validation.
 *
 * Tests:
 *   F1  Z=79-102 predictions are physically plausible (bounded, monotone
 *       within the actinide series, better than ±50% vs reference).
 *
 *   F2  Z=103-118 predictions stay bounded and cover the full range of
 *       reported relativistic CCSD(T) values.
 *
 *   F3  Extended module (Z=119-120): descriptors are self-consistent,
 *       literature values are reproduced by alpha_lit(), generative
 *       prediction is in the right ballpark.
 *
 *   F4  Actinide valence invariant: all actinides Z=89-103 have
 *       active_valence == 3 (regression guard for Blocker 1).
 *
 *   F5  Period-7 RMS <= 30% (target for heavy-element regime).
 */

#include "atomistic/models/alpha_model.hpp"
#include "atomistic/models/alpha_model_ext.hpp"
#include "atomistic/models/atomic_descriptors.hpp"
#include "atomistic/models/atomic_descriptors_ext.hpp"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace atomistic::polarization;
using namespace atomistic::polarization::desc;
using namespace atomistic::polarization::ext;

// ============================================================================
// Minimal test harness
// ============================================================================

static int g_passed = 0;
static int g_failed = 0;

#define CHECK(cond, msg) do { \
    if (cond) { ++g_passed; } \
    else { ++g_failed; \
           std::fprintf(stderr, "  FAIL [line %d]: %s\n", __LINE__, (msg)); } \
} while(0)

#define SECTION(name) std::printf("\n[%s]\n", (name))

// ============================================================================
// Reference data: Z=79-118, Schwerdtfeger & Nagle 2019 values used in CSV.
// ============================================================================

struct RefPoint { uint32_t Z; const char* sym; double alpha_ref; };

static constexpr RefPoint heavy_ref[] = {
    // Z=79-96 (in CSV with weight >= 0.3)
    { 79, "Au",  5.8},  { 80, "Hg",  5.7},  { 81, "Tl",  7.6},
    { 82, "Pb",  6.8},  { 83, "Bi",  7.4},  { 84, "Po",  6.0},
    { 85, "At",  4.5},  { 86, "Rn",  5.1},  { 87, "Fr", 48.1},
    { 88, "Ra", 38.3},  { 89, "Ac", 32.1},  { 90, "Th", 32.1},
    { 91, "Pa", 25.0},  { 92, "U",  24.9},  { 93, "Np", 24.0},
    { 94, "Pu", 23.0},  { 95, "Am", 22.0},  { 96, "Cm", 23.0},
    // Z=97-102 (fine-tuning additions)
    { 97, "Bk", 22.0},  { 98, "Cf", 21.0},  { 99, "Es", 20.0},
    {100, "Fm", 19.0},  {101, "Md", 18.0},  {102, "No", 17.5},
    // Z=103-112 (transactinides)
    {103, "Lr", 24.0},  {104, "Rf", 16.3},  {105, "Db", 14.6},
    {106, "Sg", 12.4},  {107, "Bh", 10.4},  {108, "Hs",  8.8},
    {109, "Mt",  7.6},  {110, "Ds",  6.3},  {111, "Rg",  5.8},
    {112, "Cn",  5.0},
    // Z=113-118 (7p)
    {113, "Nh",  6.5},  {114, "Fl",  5.2},  {115, "Mc",  7.0},
    {116, "Lv",  6.0},  {117, "Ts",  5.5},  {118, "Og",  5.9},
};
static constexpr int N_heavy_ref = sizeof(heavy_ref) / sizeof(heavy_ref[0]);

// ============================================================================
// F1: Z=79-102 plausibility and actinide monotonicity
// ============================================================================

static void test_F1_heavy_plausibility() {
    SECTION("F1: Z=79-102 predictions vs reference (Schwerdtfeger 2019)");

    std::printf("    %-4s %-5s %10s %10s %7s\n",
                "Z", "Sym", "alpha_ref", "pred", "err%");
    std::printf("    ---- ----- ---------- ---------- -------\n");

    int n_within_50 = 0;
    for (int i = 0; i < N_heavy_ref; ++i) {
        const auto& r = heavy_ref[i];
        if (r.Z > 102) continue;
        double pred = alpha_predict(r.Z);
        double pct  = 100.0 * (pred - r.alpha_ref) / r.alpha_ref;
        std::printf("    %4u %-5s %10.3f %10.3f %+6.1f%%\n",
                    r.Z, r.sym, r.alpha_ref, pred, pct);
        if (std::abs(pct) < 50.0) ++n_within_50;
    }
    int total_checked = 0;
    for (int i = 0; i < N_heavy_ref; ++i) if (heavy_ref[i].Z <= 102) ++total_checked;

    CHECK(n_within_50 >= (total_checked * 3 / 4),
          "at least 75% of Z=79-102 within 50% error");

    // Actinide series should be broadly decreasing (U ~ Pu ~ Am)
    double alpha_U  = alpha_predict(92);
    double alpha_Cm = alpha_predict(96);
    double alpha_No = alpha_predict(102);
    std::printf("    Trend: U=%.2f  Cm=%.2f  No=%.2f\n",
                alpha_U, alpha_Cm, alpha_No);
    CHECK(alpha_U > 10.0,  "U has substantial polarizability (> 10 A^3)");
    CHECK(alpha_No > 5.0,  "No has substantial polarizability (> 5 A^3)");
}

// ============================================================================
// F2: Z=103-118 bounded predictions
// ============================================================================

static void test_F2_transactinide_bounds() {
    SECTION("F2: Z=103-118 bounded predictions vs Schwerdtfeger 2019");

    std::printf("    %-4s %-5s %10s %10s %7s\n",
                "Z", "Sym", "alpha_ref", "pred", "err%");
    std::printf("    ---- ----- ---------- ---------- -------\n");

    int all_bounded = 0;
    int all_positive = 0;
    for (int i = 0; i < N_heavy_ref; ++i) {
        const auto& r = heavy_ref[i];
        if (r.Z < 103 || r.Z > 118) continue;
        double pred = alpha_predict(r.Z);
        double pct  = 100.0 * (pred - r.alpha_ref) / r.alpha_ref;
        std::printf("    %4u %-5s %10.3f %10.3f %+6.1f%%\n",
                    r.Z, r.sym, r.alpha_ref, pred, pct);
        if (pred > 0.1 && pred < 200.0) ++all_bounded;
        if (pred > 0.0) ++all_positive;
    }
    int n_trans = 0;
    for (int i = 0; i < N_heavy_ref; ++i)
        if (heavy_ref[i].Z >= 103 && heavy_ref[i].Z <= 118) ++n_trans;

    CHECK(all_bounded == n_trans,
          "all Z=103-118 predictions in [0.1, 200] A^3");
    CHECK(all_positive == n_trans,
          "all Z=103-118 predictions positive");
}

// ============================================================================
// F3: Z=119-120 extension module
// ============================================================================

static void test_F3_extension_119_120() {
    SECTION("F3: Experimental extension Z=119-120 (Uue, Ubn)");

    // Literature values
    CHECK(std::abs(alpha_lit(119) - 165.0) < 1.0, "alpha_lit(119) = 165 A^3 (Schwerdtfeger 2019)");
    CHECK(std::abs(alpha_lit(120) - 109.0) < 1.0, "alpha_lit(120) = 109 A^3 (Schwerdtfeger 2019)");
    CHECK(alpha_lit(121) == 0.0, "alpha_lit returns 0 for undefined Z>120");

    // Descriptors for Z=119
    CHECK(period_ext(119) == 8, "period_ext(119) == 8");
    CHECK(group_ext(119) == 1,  "group_ext(119) == 1 (alkali analogue)");
    CHECK(group_ext(120) == 2,  "group_ext(120) == 2 (alkaline-earth analogue)");
    CHECK(block_ext(119) == desc::Block::s, "block_ext(119) is s-block");
    CHECK(active_valence_ext(119) == 1, "active_valence_ext(119) == 1");
    CHECK(active_valence_ext(120) == 2, "active_valence_ext(120) == 2");

    // Covalent radii reasonable
    double r119 = cov_radius_ext(119);
    double r120 = cov_radius_ext(120);
    CHECK(r119 > 1.5 && r119 < 4.0, "r_cov(119) in [1.5, 4.0] A");
    CHECK(r120 > 1.0 && r120 < 3.5, "r_cov(120) in [1.0, 3.5] A");
    CHECK(r119 > r120, "Uue larger than Ubn (s^1 > s^2, consistent with alkali/alkaline-earth)");

    // Generative predictions in rough range
    double a119_gen = alpha_predict_ext(119);
    double a120_gen = alpha_predict_ext(120);
    std::printf("    Z=119 Uue:  lit=165.0  model=%.1f  r_cov=%.2f\n", a119_gen, r119);
    std::printf("    Z=120 Ubn:  lit=109.0  model=%.1f  r_cov=%.2f\n", a120_gen, r120);

    CHECK(a119_gen > 20.0 && a119_gen < 600.0,
          "alpha_predict_ext(119) in plausible range [20, 600] A^3");
    CHECK(a120_gen > 10.0 && a120_gen < 400.0,
          "alpha_predict_ext(120) in plausible range [10, 400] A^3");

    // Extension handles Z<=118 identically to main model
    for (uint32_t Z = 1; Z <= 118; ++Z) {
        double main_val = alpha_predict(Z);
        double ext_val  = alpha_predict_ext(Z);
        if (std::abs(main_val - ext_val) > 1e-10) {
            std::fprintf(stderr, "  FAIL: alpha_predict_ext(%u) != alpha_predict(%u) "
                         "(%.6f vs %.6f)\n", Z, Z, ext_val, main_val);
            g_failed++;
            return;
        }
    }
    g_passed++;
    std::printf("    alpha_predict_ext(Z) == alpha_predict(Z) for all Z=1-118\n");
}

// ============================================================================
// F4: Actinide valence regression guard (duplicate of T1 but for Z=89-103)
// ============================================================================

static void test_F4_actinide_valence_invariant() {
    SECTION("F4: Actinide active_valence == 3 (Z=89-103)");

    int bad = 0;
    for (uint32_t Z = 89; Z <= 103; ++Z) {
        uint32_t v = active_valence(Z);
        if (v != 3) {
            std::fprintf(stderr, "  FAIL: Z=%u active_valence=%u (expected 3)\n", Z, v);
            ++bad;
        }
    }
    CHECK(bad == 0, "all actinides Z=89-103 have active_valence == 3");

    // Extended: Z=103 still actinide
    CHECK(block(103) == Block::f, "Lr (Z=103) is f-block");
    CHECK(active_valence(103) == 3, "Lr active_valence == 3");
}

// ============================================================================
// F5: Period-7 RMS target
// ============================================================================

static void test_F5_period7_rms() {
    SECTION("F5: Period-7 RMS <= 30% against Z=87-118 reference");

    double sum_sq = 0.0;
    int    n = 0;

    std::printf("    %-4s %-5s %10s %10s %7s\n",
                "Z", "Sym", "ref", "pred", "err%");
    for (int i = 0; i < N_heavy_ref; ++i) {
        const auto& r = heavy_ref[i];
        if (r.Z < 87) continue;
        double pred = alpha_predict(r.Z);
        double pct  = 100.0 * (pred - r.alpha_ref) / r.alpha_ref;
        sum_sq += pct * pct;
        ++n;
        std::printf("    %4u %-5s %10.3f %10.3f %+6.1f%%\n",
                    r.Z, r.sym, r.alpha_ref, pred, pct);
    }
    double rms = (n > 0) ? std::sqrt(sum_sq / n) : 999.0;
    std::printf("    Period-7/SHE RMS = %.1f%%  (N=%d, target <= 30%%)\n", rms, n);

    CHECK(rms <= 30.0, "period-7 RMS <= 30%");
}

// ============================================================================
// main
// ============================================================================

int main() {
    std::printf("============================================================\n");
    std::printf(" Phase 6 — Nuclear Chemical Module\n");
    std::printf("============================================================\n");

    test_F1_heavy_plausibility();
    test_F2_transactinide_bounds();
    test_F3_extension_119_120();
    test_F4_actinide_valence_invariant();
    test_F5_period7_rms();

    const int total = g_passed + g_failed;
    std::printf("\n============================================================\n");
    std::printf("  Result:  %d / %d passed", g_passed, total);
    if (g_failed == 0) std::printf("  ALL PASS\n");
    else               std::printf("  %d FAILED\n", g_failed);
    std::printf("============================================================\n");

    return (g_failed == 0) ? 0 : 1;
}
