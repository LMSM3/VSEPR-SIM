/**
 * test_track2_robustness.cpp — Track 2 Phase 1: Randomized Robustness Harness
 *
 * Purpose: Run large ensembles of randomized local bead scenes and verify
 * statistical properties of the environment-responsive layer.
 *
 * This is NOT a unit test in the traditional sense. It is a property-based
 * statistical validation that answers:
 *
 *   1. Do random isotropic environments spuriously activate ordered state?
 *   2. Do aligned dense environments correctly activate it?
 *   3. Does eta exhibit stable bounded memory across stochastic variation?
 *   4. Do CG observables show expected monotonic trends?
 *   5. Do kernel modulations remain finite and sign-correct?
 *   6. Is there no catastrophic instability under randomised input?
 *
 * Reference: Track 2 Phase 1 specification
 */

#include <cstdio>
#include <cmath>
#include <vector>
#include <algorithm>
#include <numeric>

#include "coarse_grain/core/environment_state.hpp"
#include "coarse_grain/models/environment_coupling.hpp"
#include "coarse_grain/core/channel_kernels.hpp"
#include "tests/scene_factory.hpp"

// ============================================================================
// Test Infrastructure
// ============================================================================

static int g_pass = 0;
static int g_fail = 0;

static void check(bool condition, const char* label) {
    if (condition) {
        std::printf("  [PASS] %s\n", label);
        ++g_pass;
    } else {
        std::printf("  [FAIL] %s\n", label);
        ++g_fail;
    }
}

// ============================================================================
// Configuration
// ============================================================================

static constexpr int N_TRIALS_ISOTROPIC = 1000;
static constexpr int N_TRIALS_ALIGNED   = 1000;
static constexpr int N_TRIALS_MIXED     = 2000;
static constexpr int N_TRAJ_STEPS       = 200;
static constexpr double DT              = 10.0;  // fs

static coarse_grain::EnvironmentParams default_params() {
    coarse_grain::EnvironmentParams p;
    p.alpha = 0.5;
    p.beta = 0.5;
    p.tau = 100.0;
    p.gamma_steric = 0.2;
    p.gamma_elec = -0.1;
    p.gamma_disp = 0.5;
    p.sigma_rho = 3.0;
    p.r_cutoff = 8.0;
    p.delta_sw = 1.0;
    p.rho_max = 10.0;
    return p;
}

// ============================================================================
// Ensemble Generators
// ============================================================================

/**
 * Run ensemble of isotropic random scenes (align_bias = 0).
 */
static std::vector<test_util::RunRecord> run_isotropic_ensemble(int n_trials) {
    auto params = default_params();
    test_util::MCSceneConfig cfg;
    cfg.n_min = 3;
    cfg.n_max = 12;
    cfg.r_min = 2.0;
    cfg.r_max = 7.0;
    cfg.align_bias = 0.0;  // fully random orientations

    std::vector<test_util::RunRecord> records;
    records.reserve(n_trials);
    for (int i = 0; i < n_trials; ++i) {
        uint32_t seed = 10000 + static_cast<uint32_t>(i) * 7;
        records.push_back(test_util::run_robustness_trial(
            cfg, seed, params, DT, N_TRAJ_STEPS));
    }
    return records;
}

/**
 * Run ensemble of aligned dense scenes (align_bias = 1.0, close neighbours).
 */
static std::vector<test_util::RunRecord> run_aligned_ensemble(int n_trials) {
    auto params = default_params();
    test_util::MCSceneConfig cfg;
    cfg.n_min = 6;
    cfg.n_max = 12;
    cfg.r_min = 2.0;
    cfg.r_max = 5.0;  // closer packing
    cfg.align_bias = 1.0;  // fully aligned

    std::vector<test_util::RunRecord> records;
    records.reserve(n_trials);
    for (int i = 0; i < n_trials; ++i) {
        uint32_t seed = 50000 + static_cast<uint32_t>(i) * 13;
        records.push_back(test_util::run_robustness_trial(
            cfg, seed, params, DT, N_TRAJ_STEPS));
    }
    return records;
}

/**
 * Run mixed ensemble varying alignment and density for trend analysis.
 */
static std::vector<test_util::RunRecord> run_mixed_ensemble(int n_trials) {
    auto params = default_params();
    std::vector<test_util::RunRecord> records;
    records.reserve(n_trials);

    test_util::Xorshift32 rng(99999);
    for (int i = 0; i < n_trials; ++i) {
        test_util::MCSceneConfig cfg;
        cfg.n_min = 2 + static_cast<int>(rng.uniform() * 10);
        cfg.n_max = cfg.n_min + static_cast<int>(rng.uniform() * 6);
        cfg.r_min = 1.5 + rng.uniform() * 2.0;
        cfg.r_max = cfg.r_min + 1.0 + rng.uniform() * 5.0;
        cfg.align_bias = rng.uniform();
        uint32_t seed = 80000 + static_cast<uint32_t>(i) * 11;
        records.push_back(test_util::run_robustness_trial(
            cfg, seed, params, DT, N_TRAJ_STEPS));
    }
    return records;
}

// ============================================================================
// Helper: Extract field from RunRecord vector
// ============================================================================

static std::vector<double> extract(
    const std::vector<test_util::RunRecord>& recs,
    double test_util::RunRecord::*field)
{
    std::vector<double> out;
    out.reserve(recs.size());
    for (const auto& r : recs) out.push_back(r.*field);
    return out;
}

// ============================================================================
// Group A: Isotropic Baseline — Random Environments Must Not Spuriously
//          Activate Ordered State
// ============================================================================

static void test_group_A(const std::vector<test_util::RunRecord>& iso_recs) {
    std::printf("\n--- Group A: Isotropic Baseline (N=%d) ---\n",
                static_cast<int>(iso_recs.size()));

    auto etas = extract(iso_recs, &test_util::RunRecord::final_eta);
    auto stats = test_util::compute_stats(etas);

    // A01: No NaN/Inf in any eta
    check(stats.nan_count == 0, "A01: no NaN in isotropic eta");
    check(stats.inf_count == 0, "A02: no Inf in isotropic eta");

    // A03: All eta bounded [0,1]
    check(stats.min_val >= -1e-10 && stats.max_val <= 1.0 + 1e-10,
          "A03: all isotropic eta bounded [0,1]");

    // A04: Mean eta is low for random (isotropic) environments.
    // With random orientations, P2_hat is near 1/3 (since mean P2 ~ 0).
    // With moderate density, rho_hat varies. Mean eta should be moderate.
    // Key: it should NOT be near 1.0 (full activation).
    check(stats.mean < 0.8, "A04: mean isotropic eta < 0.8 (not fully activated)");

    // A05: 95th percentile is not near 1.0
    check(stats.p95 < 0.95, "A05: p95 of isotropic eta < 0.95");

    // A06: Variance is bounded (no wild spread)
    check(stats.variance < 0.25, "A06: isotropic eta variance < 0.25");

    // A07: All eta_bounded flags are true
    int bounded_count = 0;
    for (const auto& r : iso_recs) if (r.eta_bounded) ++bounded_count;
    check(bounded_count == static_cast<int>(iso_recs.size()),
          "A07: all isotropic trajectories bounded");

    // A08: All eta_finite flags are true
    int finite_count = 0;
    for (const auto& r : iso_recs) if (r.eta_finite) ++finite_count;
    check(finite_count == static_cast<int>(iso_recs.size()),
          "A08: all isotropic trajectories finite");

    // A09: Kernel modulations all positive
    bool all_pos = true;
    for (const auto& r : iso_recs) {
        if (r.g_steric <= 0 || r.g_elec <= 0 || r.g_disp <= 0)
            all_pos = false;
    }
    check(all_pos, "A09: all isotropic kernel modulations positive");
}

// ============================================================================
// Group B: Aligned Dense Baseline — Ordered Environments Must Activate
// ============================================================================

static void test_group_B(const std::vector<test_util::RunRecord>& ali_recs) {
    std::printf("\n--- Group B: Aligned Dense Baseline (N=%d) ---\n",
                static_cast<int>(ali_recs.size()));

    auto etas = extract(ali_recs, &test_util::RunRecord::final_eta);
    auto stats = test_util::compute_stats(etas);

    // B01: No NaN/Inf
    check(stats.nan_count == 0, "B01: no NaN in aligned eta");
    check(stats.inf_count == 0, "B02: no Inf in aligned eta");

    // B03: All bounded
    check(stats.min_val >= -1e-10 && stats.max_val <= 1.0 + 1e-10,
          "B03: all aligned eta bounded [0,1]");

    // B04: Mean eta is substantially higher than isotropic would give.
    // Aligned + dense should activate the target function strongly.
    check(stats.mean > 0.3, "B04: mean aligned eta > 0.3 (activated)");

    // B05: Median eta is positive
    check(stats.p50 > 0.1, "B05: median aligned eta > 0.1");

    // B06: All trajectories bounded
    int bounded_count = 0;
    for (const auto& r : ali_recs) if (r.eta_bounded) ++bounded_count;
    check(bounded_count == static_cast<int>(ali_recs.size()),
          "B06: all aligned trajectories bounded");

    // B07: All trajectories finite
    int finite_count = 0;
    for (const auto& r : ali_recs) if (r.eta_finite) ++finite_count;
    check(finite_count == static_cast<int>(ali_recs.size()),
          "B07: all aligned trajectories finite");

    // B08: Kernel modulations all positive
    bool all_pos = true;
    for (const auto& r : ali_recs) {
        if (r.g_steric <= 0 || r.g_elec <= 0 || r.g_disp <= 0)
            all_pos = false;
    }
    check(all_pos, "B08: all aligned kernel modulations positive");
}

// ============================================================================
// Group C: Regime Separation — Aligned vs Isotropic Must Be Distinguishable
// ============================================================================

static void test_group_C(
    const std::vector<test_util::RunRecord>& iso_recs,
    const std::vector<test_util::RunRecord>& ali_recs)
{
    std::printf("\n--- Group C: Regime Separation ---\n");

    auto iso_etas = extract(iso_recs, &test_util::RunRecord::final_eta);
    auto ali_etas = extract(ali_recs, &test_util::RunRecord::final_eta);

    // C01: Aligned mean eta > isotropic mean eta
    check(test_util::groups_separated(iso_etas, ali_etas, 0.0),
          "C01: mean aligned eta > mean isotropic eta");

    // C02: Gap is meaningful (at least 0.05)
    auto iso_stats = test_util::compute_stats(iso_etas);
    auto ali_stats = test_util::compute_stats(ali_etas);
    double gap = ali_stats.mean - iso_stats.mean;
    check(gap > 0.05,
          "C02: eta gap between aligned and isotropic > 0.05");
    std::printf("       (gap = %.4f, iso_mean=%.4f, ali_mean=%.4f)\n",
                gap, iso_stats.mean, ali_stats.mean);

    // C03: Dispersion modulation is stronger for aligned
    auto iso_gd = extract(iso_recs, &test_util::RunRecord::g_disp);
    auto ali_gd = extract(ali_recs, &test_util::RunRecord::g_disp);
    check(test_util::groups_separated(iso_gd, ali_gd, 0.0),
          "C03: mean aligned dispersion modulation > isotropic");

    // C04: P2 is higher for aligned
    auto iso_p2 = extract(iso_recs, &test_util::RunRecord::final_P2);
    auto ali_p2 = extract(ali_recs, &test_util::RunRecord::final_P2);
    check(test_util::groups_separated(iso_p2, ali_p2, 0.0),
          "C04: mean aligned P2 > mean isotropic P2");
}

// ============================================================================
// Group D: Monotonic Trend Validation
// ============================================================================

static void test_group_D(const std::vector<test_util::RunRecord>& mixed_recs) {
    std::printf("\n--- Group D: Monotonic Trends (N=%d) ---\n",
                static_cast<int>(mixed_recs.size()));

    // D01: More neighbours generally gives higher rho
    {
        std::vector<double> n_neigh, rhos;
        for (const auto& r : mixed_recs) {
            n_neigh.push_back(static_cast<double>(r.n_neighbours));
            rhos.push_back(r.final_rho);
        }
        check(test_util::mean_increases_with(n_neigh, rhos),
              "D01: mean rho increases with neighbour count");
    }

    // D02: Higher rho correlates with higher eta (via target function)
    {
        auto rhos = extract(mixed_recs, &test_util::RunRecord::final_rho);
        auto etas = extract(mixed_recs, &test_util::RunRecord::final_eta);
        check(test_util::mean_increases_with(rhos, etas),
              "D02: mean eta increases with rho");
    }

    // D03: Higher target_f correlates with higher eta
    {
        auto targets = extract(mixed_recs, &test_util::RunRecord::final_target_f);
        auto etas = extract(mixed_recs, &test_util::RunRecord::final_eta);
        check(test_util::mean_increases_with(targets, etas),
              "D03: mean eta increases with target_f");
    }

    // D04: Higher eta gives stronger dispersion modulation (gamma_disp > 0)
    {
        auto etas = extract(mixed_recs, &test_util::RunRecord::final_eta);
        auto gd = extract(mixed_recs, &test_util::RunRecord::g_disp);
        check(test_util::mean_increases_with(etas, gd),
              "D04: dispersion modulation increases with eta");
    }

    // D05: Higher eta gives stronger steric modulation (gamma_steric > 0)
    {
        auto etas = extract(mixed_recs, &test_util::RunRecord::final_eta);
        auto gs = extract(mixed_recs, &test_util::RunRecord::g_steric);
        check(test_util::mean_increases_with(etas, gs),
              "D05: steric modulation increases with eta");
    }
}

// ============================================================================
// Group E: Stability — No Catastrophic Failures
// ============================================================================

static void test_group_E(const std::vector<test_util::RunRecord>& all_recs) {
    std::printf("\n--- Group E: Stability (N=%d) ---\n",
                static_cast<int>(all_recs.size()));

    // E01: Zero NaN across entire ensemble
    int nan_total = 0;
    for (const auto& r : all_recs) {
        if (std::isnan(r.final_eta)) ++nan_total;
        if (std::isnan(r.final_rho)) ++nan_total;
        if (std::isnan(r.final_P2)) ++nan_total;
        if (std::isnan(r.g_steric)) ++nan_total;
        if (std::isnan(r.g_elec)) ++nan_total;
        if (std::isnan(r.g_disp)) ++nan_total;
    }
    check(nan_total == 0, "E01: zero NaN across entire ensemble");

    // E02: Zero Inf
    int inf_total = 0;
    for (const auto& r : all_recs) {
        if (std::isinf(r.final_eta)) ++inf_total;
        if (std::isinf(r.final_rho)) ++inf_total;
        if (std::isinf(r.final_P2)) ++inf_total;
        if (std::isinf(r.g_steric)) ++inf_total;
        if (std::isinf(r.g_elec)) ++inf_total;
        if (std::isinf(r.g_disp)) ++inf_total;
    }
    check(inf_total == 0, "E02: zero Inf across entire ensemble");

    // E03: All trajectories bounded
    int unbounded = 0;
    for (const auto& r : all_recs) if (!r.eta_bounded) ++unbounded;
    check(unbounded == 0, "E03: all trajectories bounded [0,1]");

    // E04: All trajectories finite
    int not_finite = 0;
    for (const auto& r : all_recs) if (!r.eta_finite) ++not_finite;
    check(not_finite == 0, "E04: all trajectories finite");

    // E05: All kernel modulations positive
    int neg_mod = 0;
    for (const auto& r : all_recs) {
        if (r.g_steric <= 0 || r.g_elec <= 0 || r.g_disp <= 0)
            ++neg_mod;
    }
    check(neg_mod == 0, "E05: all kernel modulations positive");

    // E06: No runaway eta (all final eta in [0, 1])
    auto etas = extract(all_recs, &test_util::RunRecord::final_eta);
    auto eta_stats = test_util::compute_stats(etas);
    check(eta_stats.min_val >= -1e-10, "E06: min eta >= 0");
    check(eta_stats.max_val <= 1.0 + 1e-10, "E07: max eta <= 1");

    // E08: Rho is always non-negative
    auto rhos = extract(all_recs, &test_util::RunRecord::final_rho);
    bool rho_pos = true;
    for (double r : rhos) if (r < -1e-10) rho_pos = false;
    check(rho_pos, "E08: all rho non-negative");

    // E09: Coordination always non-negative
    auto coords = extract(all_recs, &test_util::RunRecord::final_C);
    bool c_pos = true;
    for (double c : coords) if (c < -1e-10) c_pos = false;
    check(c_pos, "E09: all coordination non-negative");
}

// ============================================================================
// Group F: Biased Stack Cloud Validation
// ============================================================================

static void test_group_F() {
    std::printf("\n--- Group F: Biased Stack Cloud ---\n");

    auto params = default_params();

    // F01: Fully aligned stack gives higher eta than random cloud
    {
        auto stack = test_util::scene_biased_stack_cloud(8, 3.0, 1.0, 0.0, 42);
        auto cloud = test_util::scene_random_cloud(8, 10.0, 42);

        auto traj_stack = test_util::run_trajectory(stack, 0, 0.0, params, DT, N_TRAJ_STEPS);
        auto traj_cloud = test_util::run_trajectory(cloud, 0, 0.0, params, DT, N_TRAJ_STEPS);

        check(traj_stack.eta.back() > traj_cloud.eta.back(),
              "F01: aligned stack eta > random cloud eta");
    }

    // F02: Higher align_bias gives higher P2
    {
        auto low_bias = test_util::scene_biased_stack_cloud(10, 3.0, 0.2, 0.5, 100);
        auto high_bias = test_util::scene_biased_stack_cloud(10, 3.0, 0.9, 0.5, 100);

        auto nbs_low = test_util::build_neighbours(low_bias, 0);
        auto nbs_high = test_util::build_neighbours(high_bias, 0);

        auto state_low = coarse_grain::compute_fast_observables(
            low_bias[0].n_hat, true, nbs_low, params);
        auto state_high = coarse_grain::compute_fast_observables(
            high_bias[0].n_hat, true, nbs_high, params);

        check(state_high.P2 >= state_low.P2,
              "F02: higher align_bias gives higher P2");
    }

    // F03: Dense shell gives higher coordination than sparse
    {
        auto dense = test_util::scene_random_shell(12, 4.0, 200);
        auto sparse = test_util::scene_random_shell(3, 4.0, 200);

        auto nbs_dense = test_util::build_neighbours(dense, 0);
        auto nbs_sparse = test_util::build_neighbours(sparse, 0);

        auto state_dense = coarse_grain::compute_fast_observables(
            dense[0].n_hat, true, nbs_dense, params);
        auto state_sparse = coarse_grain::compute_fast_observables(
            sparse[0].n_hat, true, nbs_sparse, params);

        check(state_dense.C > state_sparse.C,
              "F03: dense shell C > sparse shell C");
    }

    // F04: Stack with no jitter has higher eta than stack with large jitter
    {
        auto tight = test_util::scene_biased_stack_cloud(8, 3.0, 1.0, 0.0, 77);
        auto loose = test_util::scene_biased_stack_cloud(8, 3.0, 1.0, 5.0, 77);

        auto traj_tight = test_util::run_trajectory(tight, 0, 0.0, params, DT, N_TRAJ_STEPS);
        auto traj_loose = test_util::run_trajectory(loose, 0, 0.0, params, DT, N_TRAJ_STEPS);

        check(traj_tight.eta.back() >= traj_loose.eta.back(),
              "F04: tight stack eta >= loose (jittered) stack eta");
    }
}

// ============================================================================
// Group G: Ensemble Statistics Summary
// ============================================================================

static void print_ensemble_summary(
    const char* label,
    const std::vector<test_util::RunRecord>& recs)
{
    auto etas = extract(recs, &test_util::RunRecord::final_eta);
    auto rhos = extract(recs, &test_util::RunRecord::final_rho);
    auto p2s  = extract(recs, &test_util::RunRecord::final_P2);
    auto tfs  = extract(recs, &test_util::RunRecord::final_target_f);

    auto s_eta = test_util::compute_stats(etas);
    auto s_rho = test_util::compute_stats(rhos);
    auto s_p2  = test_util::compute_stats(p2s);
    auto s_tf  = test_util::compute_stats(tfs);

    std::printf("\n  === %s (N=%d) ===\n", label, static_cast<int>(recs.size()));
    std::printf("  eta:      mean=%.4f  var=%.4f  [%.4f, %.4f]  p05=%.4f  p50=%.4f  p95=%.4f\n",
                s_eta.mean, s_eta.variance, s_eta.min_val, s_eta.max_val,
                s_eta.p05, s_eta.p50, s_eta.p95);
    std::printf("  rho:      mean=%.4f  var=%.4f  [%.4f, %.4f]\n",
                s_rho.mean, s_rho.variance, s_rho.min_val, s_rho.max_val);
    std::printf("  P2:       mean=%.4f  var=%.4f  [%.4f, %.4f]\n",
                s_p2.mean, s_p2.variance, s_p2.min_val, s_p2.max_val);
    std::printf("  target_f: mean=%.4f  var=%.4f  [%.4f, %.4f]\n",
                s_tf.mean, s_tf.variance, s_tf.min_val, s_tf.max_val);
    std::printf("  NaN=%d  Inf=%d\n", s_eta.nan_count, s_eta.inf_count);
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::printf("================================================================\n");
    std::printf("  Track 2 Phase 1: Randomized Robustness Harness\n");
    std::printf("================================================================\n");

    // --- Run ensembles ---
    std::printf("\n[Running isotropic ensemble: %d trials x %d steps]\n",
                N_TRIALS_ISOTROPIC, N_TRAJ_STEPS);
    auto iso_recs = run_isotropic_ensemble(N_TRIALS_ISOTROPIC);

    std::printf("[Running aligned ensemble: %d trials x %d steps]\n",
                N_TRIALS_ALIGNED, N_TRAJ_STEPS);
    auto ali_recs = run_aligned_ensemble(N_TRIALS_ALIGNED);

    std::printf("[Running mixed ensemble: %d trials x %d steps]\n",
                N_TRIALS_MIXED, N_TRAJ_STEPS);
    auto mixed_recs = run_mixed_ensemble(N_TRIALS_MIXED);

    // --- Combine for stability checks ---
    std::vector<test_util::RunRecord> all_recs;
    all_recs.insert(all_recs.end(), iso_recs.begin(), iso_recs.end());
    all_recs.insert(all_recs.end(), ali_recs.begin(), ali_recs.end());
    all_recs.insert(all_recs.end(), mixed_recs.begin(), mixed_recs.end());

    // --- Print summaries ---
    print_ensemble_summary("Isotropic", iso_recs);
    print_ensemble_summary("Aligned", ali_recs);
    print_ensemble_summary("Mixed", mixed_recs);
    print_ensemble_summary("All Combined", all_recs);

    // --- Run test groups ---
    test_group_A(iso_recs);
    test_group_B(ali_recs);
    test_group_C(iso_recs, ali_recs);
    test_group_D(mixed_recs);
    test_group_E(all_recs);
    test_group_F();

    // --- Summary ---
    std::printf("\n================================================================\n");
    std::printf("  Track 2 Phase 1 Results: %d passed, %d failed (of %d)\n",
                g_pass, g_fail, g_pass + g_fail);
    std::printf("================================================================\n");

    return g_fail > 0 ? 1 : 0;
}
