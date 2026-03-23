/**
 * test_ensemble_proxy_suite5.cpp — Suite #5: Emergent Effective Medium Mapping
 *
 * Validates the ensemble-level macroscopic response proxy layer.
 *
 * Phase 5A: Computation correctness
 *   Mean, variance, mismatch, bulk/edge gaps, occupancy fractions.
 *
 * Phase 5B: Physical sense
 *   Proxy ordering under controlled conditions: dense > sparse for
 *   cohesion, aligned > random for texture, converged > fresh for
 *   stabilization, large > small for surface sensitivity reduction.
 *
 * Phase 5C: Large-N proxy stability
 *   N = 64, 125, 216 — all proxies finite, bounded, responsive.
 *
 * Phase 5D: Invariance
 *   Translation, rotation, permutation produce identical proxies.
 *
 * Phase 5E: Proxy sensitivity
 *   Sweeps across spacing, alignment, alpha/beta verify proxy response.
 *
 * Architecture position:
 *   Environment-state evaluation
 *       ↓
 *   Ensemble statistics / spatial field summaries
 *       ↓
 *   Macroscopic response proxies
 *       ↓
 *   Later constitutive or transport models
 *
 * Reference: Emergent Effective Medium Mapping specification
 */

#include "coarse_grain/analysis/ensemble_proxy.hpp"
#include "tests/scene_factory.hpp"
#include "tests/test_viz.hpp"
#include "coarse_grain/core/environment_state.hpp"
#include "atomistic/core/state.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
#include <algorithm>

static int g_pass = 0;
static int g_fail = 0;

static void check(bool cond, const char* name) {
    if (cond) {
        std::printf("  [PASS] %s\n", name);
        ++g_pass;
    } else {
        std::printf("  [FAIL] %s\n", name);
        ++g_fail;
    }
}

// ============================================================================
// Helpers
// ============================================================================

static coarse_grain::EnvironmentParams default_params() {
    coarse_grain::EnvironmentParams p;
    p.alpha = 0.5;
    p.beta  = 0.5;
    p.tau   = 100.0;
    p.gamma_steric = 0.2;
    p.gamma_elec   = -0.1;
    p.gamma_disp   = 0.5;
    p.sigma_rho = 3.0;
    p.r_cutoff  = 8.0;
    p.delta_sw  = 1.0;
    p.rho_max   = 10.0;
    return p;
}

static std::vector<atomistic::Vec3> extract_positions(
    const std::vector<test_util::SceneBead>& scene)
{
    std::vector<atomistic::Vec3> pos;
    pos.reserve(scene.size());
    for (const auto& b : scene) pos.push_back(b.position);
    return pos;
}

/**
 * Run a scene to convergence and compute ensemble proxy.
 */
static coarse_grain::EnsembleProxySummary run_and_proxy(
    const std::vector<test_util::SceneBead>& scene,
    const coarse_grain::EnvironmentParams& params,
    double dt = 10.0,
    int n_steps = 500)
{
    auto states = test_util::run_all_beads(scene, params, dt, n_steps);
    auto positions = extract_positions(scene);
    return coarse_grain::compute_ensemble_proxy(
        states, positions, -1.0, params.r_cutoff);
}

// ============================================================================
// Phase 5A — Computation Correctness
// ============================================================================

static void phase_5a_computation() {
    std::printf("\n=== Phase 5A: Computation Correctness ===\n");

    auto params = default_params();
    double dt = 10.0;
    int n_steps = 500;

    // ---- 5A.1: Mean values match manual computation ----
    {
        std::printf("\n--- 5A.1: Mean correctness ---\n");

        auto scene = test_util::scene_pair(4.0);
        auto states = test_util::run_all_beads(scene, params, dt, n_steps);
        auto positions = extract_positions(scene);
        auto proxy = coarse_grain::compute_ensemble_proxy(states, positions);

        double manual_mean_rho = (states[0].rho + states[1].rho) / 2.0;
        double manual_mean_eta = (states[0].eta + states[1].eta) / 2.0;
        double manual_mean_C   = (states[0].C + states[1].C) / 2.0;
        double manual_mean_P2  = (states[0].P2 + states[1].P2) / 2.0;

        check(std::abs(proxy.mean_rho - manual_mean_rho) < 1e-12,
              "mean_rho matches manual");
        check(std::abs(proxy.mean_eta - manual_mean_eta) < 1e-12,
              "mean_eta matches manual");
        check(std::abs(proxy.mean_C - manual_mean_C) < 1e-12,
              "mean_C matches manual");
        check(std::abs(proxy.mean_P2 - manual_mean_P2) < 1e-12,
              "mean_P2 matches manual");
        check(proxy.bead_count == 2, "bead count correct");
    }

    // ---- 5A.2: Variance matches manual computation ----
    {
        std::printf("\n--- 5A.2: Variance correctness ---\n");

        auto scene = test_util::scene_linear_stack(5, 3.5);
        auto states = test_util::run_all_beads(scene, params, dt, n_steps);
        auto positions = extract_positions(scene);
        auto proxy = coarse_grain::compute_ensemble_proxy(states, positions);

        // Manual variance of eta
        double sum = 0;
        for (const auto& s : states) sum += s.eta;
        double mu = sum / states.size();
        double var = 0;
        for (const auto& s : states) {
            double d = s.eta - mu;
            var += d * d;
        }
        var /= states.size();

        check(std::abs(proxy.var_eta - var) < 1e-12,
              "var_eta matches manual");
        check(proxy.var_eta >= 0.0, "var_eta non-negative");
        check(proxy.var_rho >= 0.0, "var_rho non-negative");
        check(proxy.var_C >= 0.0,   "var_C non-negative");
        check(proxy.var_P2 >= 0.0,  "var_P2 non-negative");
    }

    // ---- 5A.3: State mismatch correctness ----
    {
        std::printf("\n--- 5A.3: State mismatch ---\n");

        auto scene = test_util::scene_cubic_lattice(3, 4.0);
        auto states = test_util::run_all_beads(scene, params, dt, n_steps);
        auto positions = extract_positions(scene);
        auto proxy = coarse_grain::compute_ensemble_proxy(states, positions);

        double manual_mismatch = 0;
        for (const auto& s : states)
            manual_mismatch += std::abs(s.eta - s.target_f);
        manual_mismatch /= states.size();

        check(std::abs(proxy.mean_state_mismatch - manual_mismatch) < 1e-12,
              "mean_state_mismatch matches manual");
        check(proxy.mean_state_mismatch >= 0.0,
              "mean_state_mismatch non-negative");
    }

    // ---- 5A.4: Bulk/edge gap sign ----
    {
        std::printf("\n--- 5A.4: Bulk/edge gaps ---\n");

        auto scene = test_util::scene_cubic_lattice(4, 4.0);
        auto proxy = run_and_proxy(scene, params);

        check(proxy.n_bulk + proxy.n_edge == 64,
              "bulk + edge = total beads");
        // Bulk beads should have >= coordination than edge
        check(proxy.bulk_edge_C_gap >= -0.01,
              "bulk C >= edge C (gap non-negative)");
    }

    // ---- 5A.5: Occupancy fractions in [0, 1] ----
    {
        std::printf("\n--- 5A.5: Occupancy fractions ---\n");

        auto scene = test_util::scene_cubic_lattice(3, 4.0);
        auto proxy = run_and_proxy(scene, params);

        check(proxy.frac_high_coord >= 0.0 && proxy.frac_high_coord <= 1.0,
              "frac_high_coord in [0,1]");
        check(proxy.frac_high_eta >= 0.0 && proxy.frac_high_eta <= 1.0,
              "frac_high_eta in [0,1]");
        check(proxy.frac_high_alignment >= 0.0 && proxy.frac_high_alignment <= 1.0,
              "frac_high_alignment in [0,1]");

        // At least some beads should have C >= mean_C
        check(proxy.frac_high_coord > 0.0,
              "some beads have high coordination");
    }

    // ---- 5A.6: Spatial autocorrelation bounded ----
    {
        std::printf("\n--- 5A.6: Spatial autocorrelation ---\n");

        auto scene = test_util::scene_cubic_lattice(3, 4.0);
        auto proxy = run_and_proxy(scene, params);

        check(proxy.eta_spatial_autocorr >= -1.0 && proxy.eta_spatial_autocorr <= 1.0,
              "eta autocorrelation in [-1, 1]");
        check(proxy.rho_spatial_autocorr >= -1.0 && proxy.rho_spatial_autocorr <= 1.0,
              "rho autocorrelation in [-1, 1]");
        check(proxy.P2_spatial_autocorr >= -1.0 && proxy.P2_spatial_autocorr <= 1.0,
              "P2 autocorrelation in [-1, 1]");
    }

    // ---- 5A.7: All proxies in [0, 1] ----
    {
        std::printf("\n--- 5A.7: Proxy bounds ---\n");

        auto scene = test_util::scene_cubic_lattice(3, 4.0);
        auto proxy = run_and_proxy(scene, params);

        check(proxy.cohesion_proxy >= 0.0 && proxy.cohesion_proxy <= 1.0,
              "cohesion_proxy in [0,1]");
        check(proxy.uniformity_proxy >= 0.0 && proxy.uniformity_proxy <= 1.0,
              "uniformity_proxy in [0,1]");
        check(proxy.texture_proxy >= 0.0 && proxy.texture_proxy <= 1.0,
              "texture_proxy in [0,1]");
        check(proxy.stabilization_proxy >= 0.0 && proxy.stabilization_proxy <= 1.0,
              "stabilization_proxy in [0,1]");
        check(proxy.surface_sensitivity_proxy >= 0.0 && proxy.surface_sensitivity_proxy <= 1.0,
              "surface_sensitivity_proxy in [0,1]");
    }

    // ---- 5A.8: Empty input ----
    {
        std::printf("\n--- 5A.8: Empty input ---\n");

        std::vector<coarse_grain::EnvironmentState> empty_states;
        std::vector<atomistic::Vec3> empty_pos;
        auto proxy = coarse_grain::compute_ensemble_proxy(empty_states, empty_pos);

        check(proxy.bead_count == 0, "empty: bead_count = 0");
        check(proxy.mean_rho == 0.0, "empty: mean_rho = 0");
        check(proxy.all_finite,      "empty: all_finite = true");
    }

    // ---- 5A.9: Diagnostics ----
    {
        std::printf("\n--- 5A.9: Diagnostics ---\n");

        auto scene = test_util::scene_cubic_lattice(3, 4.0);
        auto proxy = run_and_proxy(scene, params);

        check(proxy.all_finite,  "diagnostics: all finite");
        check(proxy.all_bounded, "diagnostics: all bounded");
    }
}

// ============================================================================
// Phase 5B — Physical Sense
// ============================================================================

static void phase_5b_physical_sense() {
    std::printf("\n=== Phase 5B: Physical Sense ===\n");

    auto params = default_params();

    // ---- 5B.1: Dense > sparse for cohesion ----
    {
        std::printf("\n--- 5B.1: Cohesion ordering ---\n");

        auto scene_dense  = test_util::scene_cubic_lattice(3, 3.0);
        auto scene_sparse = test_util::scene_cubic_lattice(3, 7.0);

        auto proxy_dense  = run_and_proxy(scene_dense,  params);
        auto proxy_sparse = run_and_proxy(scene_sparse, params);

        check(proxy_dense.cohesion_proxy > proxy_sparse.cohesion_proxy,
              "dense lattice more cohesive than sparse");
        check(proxy_dense.mean_rho > proxy_sparse.mean_rho,
              "dense lattice higher mean rho");
    }

    // ---- 5B.2: Aligned > random for texture ----
    {
        std::printf("\n--- 5B.2: Texture ordering ---\n");

        auto scene_aligned = test_util::scene_biased_stack_cloud(
            20, 3.5, 1.0, 0.0, 42);   // all z-aligned
        auto scene_random  = test_util::scene_biased_stack_cloud(
            20, 3.5, 0.0, 0.0, 42);   // random orientations

        auto proxy_aligned = run_and_proxy(scene_aligned, params);
        auto proxy_random  = run_and_proxy(scene_random,  params);

        check(proxy_aligned.texture_proxy > proxy_random.texture_proxy,
              "aligned beads have higher texture proxy");
        check(proxy_aligned.mean_P2_hat > proxy_random.mean_P2_hat,
              "aligned beads have higher mean P2_hat");
    }

    // ---- 5B.3: Converged > unconverged for stabilization ----
    {
        std::printf("\n--- 5B.3: Stabilization ordering ---\n");

        auto scene = test_util::scene_cubic_lattice(3, 4.0);

        // Converged: many steps
        auto proxy_converged = run_and_proxy(scene, params, 10.0, 500);

        // Unconverged: very few steps
        auto proxy_fresh = run_and_proxy(scene, params, 10.0, 2);

        check(proxy_converged.stabilization_proxy >= proxy_fresh.stabilization_proxy,
              "converged system more stabilized");
        check(proxy_converged.mean_state_mismatch <= proxy_fresh.mean_state_mismatch + 0.01,
              "converged system has lower mismatch");
    }

    // ---- 5B.4: Lattice > cloud for uniformity ----
    {
        std::printf("\n--- 5B.4: Uniformity ordering ---\n");

        auto scene_lattice = test_util::scene_cubic_lattice(3, 4.0);
        auto scene_cloud   = test_util::scene_random_cluster(27, 12.0, 42);

        auto proxy_lattice = run_and_proxy(scene_lattice, params);
        auto proxy_cloud   = run_and_proxy(scene_cloud,   params);

        // Lattice is geometrically regular → lower variance → higher uniformity
        check(proxy_lattice.uniformity_proxy >= proxy_cloud.uniformity_proxy - 0.05,
              "lattice at least as uniform as random cloud");
    }

    // ---- 5B.5: Isolated bead has zero coordination ----
    {
        std::printf("\n--- 5B.5: Isolated bead ---\n");

        auto scene = test_util::scene_isolated();
        auto proxy = run_and_proxy(scene, params);

        check(proxy.mean_C == 0.0,  "isolated: zero coordination");
        check(proxy.mean_rho == 0.0, "isolated: zero density");
        check(proxy.cohesion_proxy < 0.5, "isolated: low cohesion");
    }

    // ---- 5B.6: Higher density produces higher occupancy fractions ----
    {
        std::printf("\n--- 5B.6: Occupancy fraction response ---\n");

        auto scene_tight = test_util::scene_cubic_lattice(3, 3.0);
        auto scene_loose = test_util::scene_cubic_lattice(3, 6.0);

        auto proxy_tight = run_and_proxy(scene_tight, params);
        auto proxy_loose = run_and_proxy(scene_loose, params);

        check(proxy_tight.frac_high_eta >= proxy_loose.frac_high_eta - 0.01,
              "tight packing: more high-eta beads");
    }

    // ---- 5B.7: Spatial autocorrelation > 0 for ordered lattice ----
    {
        std::printf("\n--- 5B.7: Spatial coherence ---\n");

        auto scene = test_util::scene_cubic_lattice(4, 4.0);
        auto proxy = run_and_proxy(scene, params);

        // In an ordered lattice, nearby beads should have similar eta
        check(proxy.eta_spatial_autocorr >= -0.1,
              "lattice: eta not anti-correlated");
        check(proxy.rho_spatial_autocorr >= -0.1,
              "lattice: rho not anti-correlated");
    }
}

// ============================================================================
// Phase 5C — Large-N Proxy Stability
// ============================================================================

static void phase_5c_large_n() {
    std::printf("\n=== Phase 5C: Large-N Proxy Stability ===\n");

    auto params = default_params();
    double dt = 10.0;
    int n_steps = 400;

    struct LargeNConfig {
        const char* label;
        int n_side;
        int expected_n;
    };

    LargeNConfig configs[] = {
        {"N=64",  4,  64},
        {"N=125", 5, 125},
        {"N=216", 6, 216}
    };

    for (const auto& cfg : configs) {
        std::printf("\n--- %s ---\n", cfg.label);

        // Perfect lattice
        {
            auto scene = test_util::scene_cubic_lattice(cfg.n_side, 4.0);
            auto states = test_util::run_all_beads(scene, params, dt, n_steps);
            auto positions = extract_positions(scene);
            auto proxy = coarse_grain::compute_ensemble_proxy(
                states, positions, -1.0, params.r_cutoff);

            char buf[128];
            std::snprintf(buf, sizeof(buf), "%s lattice: all finite", cfg.label);
            check(proxy.all_finite, buf);
            std::snprintf(buf, sizeof(buf), "%s lattice: all bounded", cfg.label);
            check(proxy.all_bounded, buf);

            std::snprintf(buf, sizeof(buf), "%s lattice: cohesion in [0,1]", cfg.label);
            check(proxy.cohesion_proxy >= 0.0 && proxy.cohesion_proxy <= 1.0, buf);
            std::snprintf(buf, sizeof(buf), "%s lattice: uniformity in [0,1]", cfg.label);
            check(proxy.uniformity_proxy >= 0.0 && proxy.uniformity_proxy <= 1.0, buf);
            std::snprintf(buf, sizeof(buf), "%s lattice: texture in [0,1]", cfg.label);
            check(proxy.texture_proxy >= 0.0 && proxy.texture_proxy <= 1.0, buf);
            std::snprintf(buf, sizeof(buf), "%s lattice: stabilization in [0,1]", cfg.label);
            check(proxy.stabilization_proxy >= 0.0 && proxy.stabilization_proxy <= 1.0, buf);
            std::snprintf(buf, sizeof(buf), "%s lattice: surface_sensitivity in [0,1]", cfg.label);
            check(proxy.surface_sensitivity_proxy >= 0.0 && proxy.surface_sensitivity_proxy <= 1.0, buf);
        }

        // Random cloud
        {
            auto scene = test_util::scene_separated_cloud(
                cfg.expected_n, 15.0, 1.5, 42);
            auto states = test_util::run_all_beads(scene, params, dt, n_steps);
            auto positions = extract_positions(scene);
            auto proxy = coarse_grain::compute_ensemble_proxy(
                states, positions, -1.0, params.r_cutoff);

            char buf[128];
            std::snprintf(buf, sizeof(buf), "%s cloud: all finite", cfg.label);
            check(proxy.all_finite, buf);
            std::snprintf(buf, sizeof(buf), "%s cloud: all bounded", cfg.label);
            check(proxy.all_bounded, buf);
            std::snprintf(buf, sizeof(buf), "%s cloud: proxies bounded", cfg.label);
            bool bounded = proxy.cohesion_proxy >= 0 && proxy.cohesion_proxy <= 1
                        && proxy.uniformity_proxy >= 0 && proxy.uniformity_proxy <= 1
                        && proxy.texture_proxy >= 0 && proxy.texture_proxy <= 1
                        && proxy.stabilization_proxy >= 0 && proxy.stabilization_proxy <= 1
                        && proxy.surface_sensitivity_proxy >= 0 && proxy.surface_sensitivity_proxy <= 1;
            check(bounded, buf);
        }
    }

    // ---- Cross-N: surface sensitivity decreases with N ----
    {
        std::printf("\n--- Cross-N surface sensitivity ---\n");

        std::vector<double> surf_sensitivities;
        for (const auto& cfg : configs) {
            auto scene = test_util::scene_cubic_lattice(cfg.n_side, 4.0);
            auto proxy = run_and_proxy(scene, params, dt, n_steps);
            surf_sensitivities.push_back(proxy.surface_sensitivity_proxy);
        }

        // Larger systems have proportionally fewer surface beads
        // Surface sensitivity should not increase dramatically
        bool non_increasing = true;
        for (size_t i = 1; i < surf_sensitivities.size(); ++i) {
            if (surf_sensitivities[i] > surf_sensitivities[i-1] + 0.15) {
                non_increasing = false;
            }
        }
        check(non_increasing,
              "surface sensitivity does not increase dramatically with N");
    }

    // ---- Cross-N: bulk cohesion consistent ----
    {
        std::printf("\n--- Cross-N cohesion consistency ---\n");

        std::vector<double> cohesions;
        for (const auto& cfg : configs) {
            auto scene = test_util::scene_cubic_lattice(cfg.n_side, 4.0);
            auto proxy = run_and_proxy(scene, params, dt, n_steps);
            cohesions.push_back(proxy.cohesion_proxy);
        }

        double max_diff = 0;
        for (size_t i = 1; i < cohesions.size(); ++i) {
            double d = std::abs(cohesions[i] - cohesions[0]);
            if (d > max_diff) max_diff = d;
        }
        check(max_diff < 0.15,
              "cohesion proxy consistent across N (diff < 0.15)");
    }
}

// ============================================================================
// Phase 5D — Invariance
// ============================================================================

static void phase_5d_invariance() {
    std::printf("\n=== Phase 5D: Invariance ===\n");

    auto params = default_params();
    double dt = 10.0;
    int n_steps = 300;

    // ---- 5D.1: Translation invariance ----
    {
        std::printf("\n--- 5D.1: Translation invariance ---\n");

        auto scene_orig = test_util::scene_cubic_lattice(3, 4.0);
        auto scene_shifted = test_util::translate_scene(
            scene_orig, {100.0, -200.0, 50.0});

        auto proxy_orig    = run_and_proxy(scene_orig,    params, dt, n_steps);
        auto proxy_shifted = run_and_proxy(scene_shifted, params, dt, n_steps);

        check(std::abs(proxy_orig.cohesion_proxy - proxy_shifted.cohesion_proxy) < 1e-10,
              "translation: cohesion invariant");
        check(std::abs(proxy_orig.uniformity_proxy - proxy_shifted.uniformity_proxy) < 1e-10,
              "translation: uniformity invariant");
        check(std::abs(proxy_orig.texture_proxy - proxy_shifted.texture_proxy) < 1e-10,
              "translation: texture invariant");
        check(std::abs(proxy_orig.stabilization_proxy - proxy_shifted.stabilization_proxy) < 1e-10,
              "translation: stabilization invariant");
        check(std::abs(proxy_orig.surface_sensitivity_proxy - proxy_shifted.surface_sensitivity_proxy) < 1e-10,
              "translation: surface_sensitivity invariant");
    }

    // ---- 5D.2: Rotation invariance ----
    {
        std::printf("\n--- 5D.2: Rotation invariance ---\n");

        auto scene_orig = test_util::scene_cubic_lattice(3, 4.0);
        auto scene_rotated = test_util::rotate_scene(
            scene_orig, {1.0, 1.0, 1.0}, 1.2);

        auto proxy_orig    = run_and_proxy(scene_orig,    params, dt, n_steps);
        auto proxy_rotated = run_and_proxy(scene_rotated, params, dt, n_steps);

        check(std::abs(proxy_orig.cohesion_proxy - proxy_rotated.cohesion_proxy) < 1e-8,
              "rotation: cohesion invariant");
        check(std::abs(proxy_orig.uniformity_proxy - proxy_rotated.uniformity_proxy) < 1e-8,
              "rotation: uniformity invariant");
        check(std::abs(proxy_orig.texture_proxy - proxy_rotated.texture_proxy) < 1e-8,
              "rotation: texture invariant");
        check(std::abs(proxy_orig.stabilization_proxy - proxy_rotated.stabilization_proxy) < 1e-8,
              "rotation: stabilization invariant");
        check(std::abs(proxy_orig.mean_rho - proxy_rotated.mean_rho) < 1e-8,
              "rotation: mean_rho invariant");
    }

    // ---- 5D.3: Permutation invariance ----
    {
        std::printf("\n--- 5D.3: Permutation invariance ---\n");

        auto scene_orig = test_util::scene_cubic_lattice(3, 4.0);
        auto scene_perm = test_util::permute_scene(scene_orig, 12345);

        auto proxy_orig = run_and_proxy(scene_orig, params, dt, n_steps);
        auto proxy_perm = run_and_proxy(scene_perm, params, dt, n_steps);

        check(std::abs(proxy_orig.mean_eta - proxy_perm.mean_eta) < 1e-10,
              "permutation: mean_eta invariant");
        check(std::abs(proxy_orig.mean_rho - proxy_perm.mean_rho) < 1e-10,
              "permutation: mean_rho invariant");
        check(std::abs(proxy_orig.cohesion_proxy - proxy_perm.cohesion_proxy) < 1e-10,
              "permutation: cohesion invariant");
        check(std::abs(proxy_orig.uniformity_proxy - proxy_perm.uniformity_proxy) < 1e-10,
              "permutation: uniformity invariant");
    }

    // ---- 5D.4: Seeded reproducibility ----
    {
        std::printf("\n--- 5D.4: Seeded reproducibility ---\n");

        auto scene_a = test_util::scene_random_cluster(30, 12.0, 777);
        auto scene_b = test_util::scene_random_cluster(30, 12.0, 777);

        auto proxy_a = run_and_proxy(scene_a, params, dt, n_steps);
        auto proxy_b = run_and_proxy(scene_b, params, dt, n_steps);

        check(std::abs(proxy_a.cohesion_proxy - proxy_b.cohesion_proxy) < 1e-15,
              "seed: identical cohesion");
        check(std::abs(proxy_a.mean_eta - proxy_b.mean_eta) < 1e-15,
              "seed: identical mean_eta");
    }
}

// ============================================================================
// Phase 5E — Proxy Sensitivity
// ============================================================================

static void phase_5e_sensitivity() {
    std::printf("\n=== Phase 5E: Proxy Sensitivity ===\n");

    auto params = default_params();

    // ---- 5E.1: Spacing sweep affects cohesion ----
    {
        std::printf("\n--- 5E.1: Spacing sweep → cohesion ---\n");

        double spacings[] = {2.5, 4.0, 6.0, 8.0, 10.0};
        int n_sp = 5;
        std::vector<double> cohesions;

        bool all_bounded = true;
        for (int i = 0; i < n_sp; ++i) {
            auto scene = test_util::scene_cubic_lattice(3, spacings[i]);
            auto proxy = run_and_proxy(scene, params);
            cohesions.push_back(proxy.cohesion_proxy);
            if (proxy.cohesion_proxy < 0 || proxy.cohesion_proxy > 1)
                all_bounded = false;
        }

        check(all_bounded, "spacing sweep: cohesion always in [0,1]");
        // Tighter spacing should produce higher cohesion
        check(cohesions.front() > cohesions.back(),
              "tight spacing higher cohesion than loose");
        // Should actually respond (not all the same)
        bool has_variation = false;
        for (size_t i = 1; i < cohesions.size(); ++i) {
            if (std::abs(cohesions[i] - cohesions[0]) > 0.01) {
                has_variation = true; break;
            }
        }
        check(has_variation, "cohesion responds to spacing");
    }

    // ---- 5E.2: Alignment sweep affects texture ----
    {
        std::printf("\n--- 5E.2: Alignment sweep → texture ---\n");

        double biases[] = {0.0, 0.25, 0.5, 0.75, 1.0};
        int n_biases = 5;
        std::vector<double> textures;

        for (int i = 0; i < n_biases; ++i) {
            auto scene = test_util::scene_biased_stack_cloud(
                20, 3.5, biases[i], 0.5, 42);
            auto proxy = run_and_proxy(scene, params);
            textures.push_back(proxy.texture_proxy);
        }

        // Higher alignment bias should produce higher texture proxy
        check(textures.back() > textures.front(),
              "full alignment higher texture than random");

        bool responds = std::abs(textures.back() - textures.front()) > 0.01;
        check(responds, "texture responds to alignment bias");
    }

    // ---- 5E.3: Alpha/beta sweep affects proxy structure ----
    {
        std::printf("\n--- 5E.3: Alpha/beta sweep ---\n");

        auto scene = test_util::scene_cubic_lattice(3, 4.0);
        double alphas[] = {0.0, 0.5, 1.0};
        int n_alphas = 3;
        bool all_finite = true;
        bool responds = false;
        double first_cohesion = -1;

        for (int i = 0; i < n_alphas; ++i) {
            auto p = params;
            p.alpha = alphas[i];
            p.beta  = 1.0 - alphas[i];

            auto proxy = run_and_proxy(scene, p);
            if (!proxy.all_finite) all_finite = false;

            if (i == 0) first_cohesion = proxy.cohesion_proxy;
            if (std::abs(proxy.cohesion_proxy - first_cohesion) > 0.01)
                responds = true;
        }

        check(all_finite, "alpha/beta sweep: all finite");
        check(responds,   "alpha/beta sweep: proxies respond");
    }

    // ---- 5E.4: Tau sweep does not break proxies ----
    {
        std::printf("\n--- 5E.4: Tau sweep stability ---\n");

        auto scene = test_util::scene_cubic_lattice(3, 4.0);
        double taus[] = {10.0, 50.0, 100.0, 500.0, 2000.0};
        int n_taus = 5;
        bool all_valid = true;

        for (int i = 0; i < n_taus; ++i) {
            auto p = params;
            p.tau = taus[i];

            auto proxy = run_and_proxy(scene, p);
            if (!proxy.all_finite || !proxy.all_bounded) all_valid = false;
            if (proxy.cohesion_proxy < 0 || proxy.cohesion_proxy > 1) all_valid = false;
        }

        check(all_valid, "tau sweep: all proxies valid across tau range");
    }

    // ---- 5E.5: Scene family comparison ----
    {
        std::printf("\n--- 5E.5: Scene family comparison ---\n");

        auto scene_lattice = test_util::scene_cubic_lattice(3, 4.0);
        auto scene_slab    = test_util::scene_layered_slab(3, 3, 4.0, 4.0);
        auto scene_bundle  = test_util::scene_line_bundle(27, 4.0);
        auto scene_cloud   = test_util::scene_separated_cloud(27, 12.0, 1.0, 42);

        auto proxy_lattice = run_and_proxy(scene_lattice, params);
        auto proxy_slab    = run_and_proxy(scene_slab,    params);
        auto proxy_bundle  = run_and_proxy(scene_bundle,  params);
        auto proxy_cloud   = run_and_proxy(scene_cloud,   params);

        // All should produce valid proxies
        check(proxy_lattice.all_finite && proxy_slab.all_finite &&
              proxy_bundle.all_finite && proxy_cloud.all_finite,
              "all scene families: proxies finite");

        // Different families should produce distinguishable signatures
        bool distinguishable =
            std::abs(proxy_lattice.cohesion_proxy - proxy_bundle.cohesion_proxy) > 0.001 ||
            std::abs(proxy_lattice.texture_proxy - proxy_bundle.texture_proxy) > 0.001 ||
            std::abs(proxy_lattice.uniformity_proxy - proxy_cloud.uniformity_proxy) > 0.001;
        check(distinguishable, "scene families produce distinguishable proxies");
    }
}

// ============================================================================
// Phase 5F — Directed Proxy Validation (Blocking)
//
// One perturbation test per proxy. Each test specifies:
//   - the perturbation applied to states/scene
//   - the expected direction of change
//   - the pass criterion
//
// If any test fails, the proxy formula is wrong — fix it before use.
// ============================================================================

static void phase_5f_directed_validation() {
    std::printf("\n=== Phase 5F: Directed Proxy Validation ===\n");

    auto params = default_params();
    double dt   = 10.0;
    int n_steps = 500;

    // ---- 5F.1: Degrade edge bead states → cohesion decreases ----
    // Perturbation: identify the N/2 lowest-C beads (edge class) and set
    //   their rho_hat = 0, eta = 0, target_f = 0.9 (forces high mismatch).
    // Expected: cohesion = (mean_rho_hat + mean_eta + (1 - mean_mismatch)) / 3
    //   decreases because rho_hat and eta terms fall and mismatch rises.
    // Pass criterion: cohesion_base > cohesion_degraded.
    {
        std::printf("\n--- 5F.1: Edge states degraded → cohesion decreases ---\n");

        auto scene    = test_util::scene_cubic_lattice(3, 4.0);
        auto states   = test_util::run_all_beads(scene, params, dt, n_steps);
        auto positions = extract_positions(scene);
        auto proxy_base = coarse_grain::compute_ensemble_proxy(
            states, positions, -1.0, params.r_cutoff);

        // Degrade edge beads: zero their density/state and induce mismatch
        auto states_degraded = states;
        for (auto& st : states_degraded) {
            if (st.C < proxy_base.c_thresh_used) {
                st.rho_hat  = 0.0;
                st.eta      = 0.0;
                st.target_f = 0.9;
            }
        }
        auto proxy_degraded = coarse_grain::compute_ensemble_proxy(
            states_degraded, positions, -1.0, params.r_cutoff);

        check(proxy_base.cohesion_proxy > proxy_degraded.cohesion_proxy,
              "5F.1: degrading edge states decreases cohesion_proxy");
        check(proxy_base.mean_rho_hat > proxy_degraded.mean_rho_hat,
              "5F.1: mean_rho_hat decreases after edge degradation");
    }

    // ---- 5F.2: Randomize P2_hat → texture decreases ----
    // Perturbation: start with an aligned scene (P2_hat > 0.5), then set
    //   all P2_hat to 1/3 (isotropic random average).
    // Expected: texture_proxy = mean_P2_hat decreases toward 0.33.
    // Pass criterion: texture_aligned > texture_random.
    {
        std::printf("\n--- 5F.2: P2_hat randomized → texture decreases ---\n");

        auto scene_aligned = test_util::scene_biased_stack_cloud(
            20, 3.5, 1.0, 0.0, 42);
        auto states_aligned = test_util::run_all_beads(
            scene_aligned, params, dt, n_steps);
        auto positions_aligned = extract_positions(scene_aligned);
        auto proxy_aligned = coarse_grain::compute_ensemble_proxy(
            states_aligned, positions_aligned, -1.0, params.r_cutoff);

        // Replace all P2_hat with isotropic value
        auto states_randomized = states_aligned;
        for (auto& st : states_randomized) st.P2_hat = 1.0 / 3.0;
        auto proxy_randomized = coarse_grain::compute_ensemble_proxy(
            states_randomized, positions_aligned, -1.0, params.r_cutoff);

        check(proxy_aligned.texture_proxy > proxy_randomized.texture_proxy,
              "5F.2: aligned P2_hat gives higher texture than isotropic");
        check(std::abs(proxy_randomized.texture_proxy - 1.0 / 3.0) < 1e-12,
              "5F.2: randomized texture_proxy == 1/3 (mean_P2_hat)");
    }

    // ---- 5F.3: Reduce variance → uniformity increases ----
    // Perturbation: take heterogeneous converged states; then create a
    //   second version where all beads have the ensemble mean for eta and
    //   rho_hat (zero variance).
    // Expected: uniformity = 1 - normalized_spread → 1.0 when spread = 0.
    // Pass criterion: uniformity_zero_var > uniformity_base.
    {
        std::printf("\n--- 5F.3: Zero variance → uniformity increases ---\n");

        auto scene  = test_util::scene_cubic_lattice(3, 4.0);
        auto states = test_util::run_all_beads(scene, params, dt, n_steps);
        auto positions = extract_positions(scene);
        auto proxy_base = coarse_grain::compute_ensemble_proxy(
            states, positions, -1.0, params.r_cutoff);

        // Set all beads to their ensemble mean (zero variance)
        auto states_uniform = states;
        for (auto& st : states_uniform) {
            st.eta     = proxy_base.mean_eta;
            st.rho_hat = proxy_base.mean_rho_hat;
        }
        auto proxy_uniform = coarse_grain::compute_ensemble_proxy(
            states_uniform, positions, -1.0, params.r_cutoff);

        check(proxy_uniform.uniformity_proxy > proxy_base.uniformity_proxy,
              "5F.3: zero-variance states have higher uniformity");
        check(proxy_uniform.uniformity_proxy > 0.99,
              "5F.3: zero-variance uniformity approaches 1.0");
        check(proxy_uniform.var_eta < 1e-28,
              "5F.3: var_eta is zero after equalization");
    }

    // ---- 5F.4: Freeze eta at random values → stabilization decreases ----
    // Perturbation: take converged states (high eta, low mismatch); replace
    //   eta with LCG pseudo-random values in [0, 1] and set target_f = 0.9.
    // Expected: stabilization = mean_eta * (1 - mean_mismatch) decreases
    //   because mean_eta ≈ 0.5 and mismatch ≈ mean|random - 0.9| ≈ 0.4.
    // Pass criterion: stabilization_converged > stabilization_frozen.
    {
        std::printf("\n--- 5F.4: Frozen random eta → stabilization decreases ---\n");

        auto scene  = test_util::scene_cubic_lattice(3, 4.0);
        auto states = test_util::run_all_beads(scene, params, dt, n_steps);
        auto positions = extract_positions(scene);
        auto proxy_converged = coarse_grain::compute_ensemble_proxy(
            states, positions, -1.0, params.r_cutoff);

        // Freeze eta at pseudo-random values with deterministic seed
        auto states_frozen = states;
        uint32_t seed = 0xDEADBEEFu;
        for (auto& st : states_frozen) {
            seed = seed * 1664525u + 1013904223u;
            st.eta      = static_cast<double>(seed >> 16) / 65535.0;
            st.target_f = 0.9;
        }
        auto proxy_frozen = coarse_grain::compute_ensemble_proxy(
            states_frozen, positions, -1.0, params.r_cutoff);

        check(proxy_converged.stabilization_proxy > proxy_frozen.stabilization_proxy,
              "5F.4: converged state more stabilized than frozen-random eta");
        check(proxy_frozen.mean_state_mismatch > proxy_converged.mean_state_mismatch,
              "5F.4: frozen random eta produces higher mismatch");
    }

    // ---- 5F.5: Identical bead states → surface sensitivity near zero ----
    // Perturbation: set all beads to identical rho, C, eta, target_f.
    //   When bulk mean == edge mean for all fields, all gaps are zero.
    // Expected: surface_sensitivity_proxy ≈ 0.
    // Pass criterion: surface_sensitivity < 0.02.
    {
        std::printf("\n--- 5F.5: Identical states → surface sensitivity near zero ---\n");

        auto scene  = test_util::scene_cubic_lattice(3, 4.0);
        auto states = test_util::run_all_beads(scene, params, dt, n_steps);
        auto positions = extract_positions(scene);

        // Force all beads to identical values — no bulk/edge contrast
        auto states_ident = states;
        for (auto& st : states_ident) {
            st.rho      = 1.0;
            st.rho_hat  = 0.5;
            st.C        = 6.0;
            st.P2       = 0.3;
            st.P2_hat   = 0.53;
            st.eta      = 0.7;
            st.target_f = 0.7;
        }
        auto proxy_ident = coarse_grain::compute_ensemble_proxy(
            states_ident, positions, -1.0, params.r_cutoff);

        check(proxy_ident.surface_sensitivity_proxy < 0.02,
              "5F.5: identical states produce near-zero surface sensitivity");
    }
}

// ============================================================================
// Phase 5G — Time Delta, Validity, and Reference Distributions
// ============================================================================

static void phase_5g_delta_validity_reference() {
    std::printf("\n=== Phase 5G: Delta, Validity, and Reference ===\n");

    auto params = default_params();

    // ---- 5G.1: Converged system has near-zero deltas ----
    {
        std::printf("\n--- 5G.1: Converged delta ---\n");

        auto scene = test_util::scene_cubic_lattice(3, 4.0);

        auto states_a = test_util::run_all_beads(scene, params, 10.0, 500);
        auto states_b = test_util::run_all_beads(scene, params, 10.0, 600);
        auto positions = extract_positions(scene);

        auto proxy_a = coarse_grain::compute_ensemble_proxy(
            states_a, positions, -1.0, params.r_cutoff);
        auto proxy_b = coarse_grain::compute_ensemble_proxy(
            states_b, positions, -1.0, params.r_cutoff);
        auto proxy_b_with_delta = coarse_grain::compute_proxy_delta(
            proxy_b, proxy_a);

        check(std::abs(proxy_b_with_delta.delta_mean_eta) < 0.05,
              "converged: delta_mean_eta small");
        check(std::abs(proxy_b_with_delta.delta_mean_mismatch) < 0.05,
              "converged: delta_mean_mismatch small");
        check(proxy_b_with_delta.converged,
              "converged: converged flag true");
    }

    // ---- 5G.2: Fresh system has non-zero delta ----
    {
        std::printf("\n--- 5G.2: Fresh system delta ---\n");

        auto scene = test_util::scene_cubic_lattice(3, 4.0);
        auto positions = extract_positions(scene);

        // Zero steps → all eta = 0
        auto states_t0 = test_util::run_all_beads(scene, params, 10.0, 0);
        // Few steps → eta has moved
        auto states_t1 = test_util::run_all_beads(scene, params, 10.0, 30);

        auto proxy_t0 = coarse_grain::compute_ensemble_proxy(
            states_t0, positions, -1.0, params.r_cutoff);
        auto proxy_t1 = coarse_grain::compute_ensemble_proxy(
            states_t1, positions, -1.0, params.r_cutoff);
        auto proxy_t1_delta = coarse_grain::compute_proxy_delta(
            proxy_t1, proxy_t0);

        check(proxy_t1_delta.delta_mean_eta > 0.0,
              "fresh: mean_eta increased (eta rising from zero)");
        check(!proxy_t1_delta.converged,
              "fresh: not yet converged");
    }

    // ---- 5G.3: Validity flag — small N flagged ----
    {
        std::printf("\n--- 5G.3: Validity flag ---\n");

        auto params_local = default_params();

        // N=2: below PROXY_MIN_BEADS (8)
        {
            auto scene = test_util::scene_pair(4.0);
            auto proxy = run_and_proxy(scene, params_local);
            check(!proxy.valid, "N=2: valid = false");
        }
        // N=5: below PROXY_MIN_BEADS
        {
            auto scene = test_util::scene_linear_stack(5, 3.5);
            auto proxy = run_and_proxy(scene, params_local);
            check(!proxy.valid, "N=5: valid = false");
        }
        // N=27: above threshold
        {
            auto scene = test_util::scene_cubic_lattice(3, 4.0);
            auto proxy = run_and_proxy(scene, params_local);
            check(proxy.valid,  "N=27: valid = true");
        }
    }

    // ---- 5G.4: c_thresh_used is populated and reasonable ----
    {
        std::printf("\n--- 5G.4: c_thresh_used traceability ---\n");

        auto scene  = test_util::scene_cubic_lattice(3, 4.0);
        auto proxy  = run_and_proxy(scene, params);

        check(proxy.c_thresh_used > 0.0,
              "c_thresh_used > 0 for non-trivial lattice");

        // Explicit threshold respected
        auto states = test_util::run_all_beads(scene, params, 10.0, 200);
        auto positions = extract_positions(scene);
        auto proxy_pin = coarse_grain::compute_ensemble_proxy(
            states, positions, 4.0, params.r_cutoff);
        check(std::abs(proxy_pin.c_thresh_used - 4.0) < 1e-12,
              "explicit c_thresh_used = 4.0 as pinned");
    }

    // ---- 5G.5: Reference bounds — apply_reference fills rel_* ----
    {
        std::printf("\n--- 5G.5: Reference normalisation ---\n");

        // Lo reference: separated (low density, low cohesion)
        auto scene_lo = test_util::scene_separated_cloud(27, 15.0, 2.0, 42);
        auto proxy_lo = run_and_proxy(scene_lo, params);

        // Hi reference: tight lattice (high density, high cohesion)
        auto scene_hi = test_util::scene_cubic_lattice(3, 3.0);
        auto proxy_hi = run_and_proxy(scene_hi, params);

        // Target: medium lattice
        auto scene_mid = test_util::scene_cubic_lattice(3, 4.5);
        auto proxy_mid = run_and_proxy(scene_mid, params);

        coarse_grain::apply_reference(proxy_mid, proxy_lo, proxy_hi);

        check(proxy_mid.rel_cohesion >= 0.0 && proxy_mid.rel_cohesion <= 1.0,
              "reference: rel_cohesion in [0,1]");
        check(proxy_mid.rel_uniformity >= 0.0 && proxy_mid.rel_uniformity <= 1.0,
              "reference: rel_uniformity in [0,1]");
        check(proxy_mid.rel_texture >= 0.0 && proxy_mid.rel_texture <= 1.0,
              "reference: rel_texture in [0,1]");
        check(proxy_mid.rel_stabilization >= 0.0 && proxy_mid.rel_stabilization <= 1.0,
              "reference: rel_stabilization in [0,1]");
        check(proxy_mid.rel_surface_sensitivity >= 0.0 &&
              proxy_mid.rel_surface_sensitivity <= 1.0,
              "reference: rel_surface_sensitivity in [0,1]");

        // Hi ref should have higher cohesion than lo ref
        check(proxy_hi.cohesion_proxy > proxy_lo.cohesion_proxy,
              "reference: hi cohesion > lo cohesion");
    }

    // ---- 5G.6: Correlation lengths finite for ordered lattice ----
    {
        std::printf("\n--- 5G.6: Correlation lengths ---\n");

        auto scene  = test_util::scene_cubic_lattice(4, 4.0);
        auto proxy  = run_and_proxy(scene, params, 10.0, 400);

        // For an ordered lattice with variance > 0, we expect a finite ξ
        // (NaN is acceptable if the field is uniform — but that's checked
        // by the all_finite flag on separate proxies).
        bool eta_ok = (proxy.var_eta < 1e-20) ||
                      (!std::isnan(proxy.eta_corr_length) && proxy.eta_corr_length > 0.0);
        bool rho_ok = (proxy.var_rho < 1e-20) ||
                      (!std::isnan(proxy.rho_corr_length) && proxy.rho_corr_length > 0.0);
        check(eta_ok, "correlation: eta_corr_length finite or field uniform");
        check(rho_ok, "correlation: rho_corr_length finite or field uniform");

        // Correlation length should be physically plausible (< 1000 Å)
        if (!std::isnan(proxy.eta_corr_length)) {
            check(proxy.eta_corr_length < 1000.0,
                  "correlation: eta_corr_length < 1000 A");
        }
    }
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::printf("Suite #5: Emergent Effective Medium Mapping\n");
    std::printf("          Ensemble-Level Macroscopic Response Proxies\n");
    std::printf("================================================================\n");

    phase_5a_computation();
    phase_5b_physical_sense();
    phase_5c_large_n();
    phase_5d_invariance();
    phase_5e_sensitivity();
    phase_5f_directed_validation();
    phase_5g_delta_validity_reference();

    std::printf("\n================================================================\n");
    std::printf("Suite #5 Results: %d passed, %d failed, %d total\n",
                g_pass, g_fail, g_pass + g_fail);

    return g_fail > 0 ? 1 : 0;
}
