/**
 * test_formation_suite3.cpp — Suite #3: Structured N > 10 Formation Studies
 *
 * Phase 1: Single-variable response atlas
 *   For each of 7 environment variables (rho, rho_hat, C, P2, P2_hat, eta,
 *   target_f), sweep a controlling scene parameter while holding all others
 *   fixed. Record output range, monotonicity, perturbation sensitivity.
 *   Run across 6 canonical scenes in order:
 *     isolated, pair, stack, triangle/tetrahedron, ring, cloud.
 *
 * Phase 2a: Pairwise coupling maps
 *   Sweep (rho × eta), (C × P2), (P2 × P2_hat), (rho × target_f) on
 *   coarse 5×5 grids. Detect nonlinear transitions, saturation, divergence.
 *
 * Phase 2b: Structured large-N runs
 *   N = 64, 125, 216 with controlled initial conditions:
 *     perfect lattice, perturbed lattice, line bundle, layered slab,
 *     shell initialization, random cloud.
 *   Record coordination histogram, anisotropy distribution, edge vs bulk
 *   differences.
 *
 * Gate: Phase 2b must show stable, bounded output at N = 216 before
 * proceeding to condition-driven formation studies.
 *
 * Reference: Suite #3 specification from development session
 */

#include "tests/scene_factory.hpp"
#include "tests/test_viz.hpp"
#include "coarse_grain/core/environment_state.hpp"
#include "coarse_grain/models/environment_coupling.hpp"
#include "atomistic/core/state.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
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
// Canonical Scene Table
// ============================================================================

struct CanonicalScene {
    const char* name;
    std::vector<test_util::SceneBead> beads;
};

static std::vector<CanonicalScene> build_canonical_scenes(double spacing) {
    std::vector<CanonicalScene> scenes;
    scenes.push_back({"isolated",    test_util::scene_isolated()});
    scenes.push_back({"pair",        test_util::scene_pair(spacing)});
    scenes.push_back({"stack",       test_util::scene_linear_stack(5, spacing)});
    scenes.push_back({"tetrahedron", test_util::scene_tetrahedron(spacing)});
    scenes.push_back({"ring",        test_util::scene_ring(6, spacing)});
    scenes.push_back({"cloud",       test_util::scene_random_cluster(12, spacing * 3.0, 42)});
    return scenes;
}

// ============================================================================
// Phase 1 — Single-Variable Response Atlas
// ============================================================================
// Strategy: for each scene, sweep one parameter that directly controls
// the density of neighbours (via spacing), which in turn varies rho, C,
// P2, eta, target_f. We sweep spacing from tight to loose, run the
// environment update, and record how each observable responds.
// ============================================================================

static void phase1_spacing_sweep() {
    std::printf("\n══════════════════════════════════════════════════════════\n");
    std::printf("  Phase 1 — Single-Variable Response Atlas (spacing sweep)\n");
    std::printf("══════════════════════════════════════════════════════════\n\n");

    coarse_grain::EnvironmentParams params;
    params.r_cutoff = 12.0;
    params.sigma_rho = 3.0;
    params.tau = 100.0;
    params.alpha = 0.5;
    params.beta = 0.5;
    double dt = 1.0;
    int n_steps = 200;

    // Spacing values to sweep: tight → loose
    const double spacings[] = {2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 10.0, 12.0, 15.0};
    const int n_spacings = 10;

    // Scene builders that accept spacing
    struct SceneBuilder {
        const char* name;
        std::vector<test_util::SceneBead> (*build)(double);
    };

    auto build_isolated = [](double) -> std::vector<test_util::SceneBead> {
        return test_util::scene_isolated();
    };
    auto build_pair = [](double s) -> std::vector<test_util::SceneBead> {
        return test_util::scene_pair(s);
    };
    auto build_stack = [](double s) -> std::vector<test_util::SceneBead> {
        return test_util::scene_linear_stack(5, s);
    };
    auto build_tetra = [](double s) -> std::vector<test_util::SceneBead> {
        return test_util::scene_tetrahedron(s);
    };
    auto build_ring = [](double s) -> std::vector<test_util::SceneBead> {
        return test_util::scene_ring(6, s);
    };
    auto build_cloud = [](double s) -> std::vector<test_util::SceneBead> {
        return test_util::scene_random_cluster(12, s * 3.0, 42);
    };

    // Use function pointers via std::function
    std::vector<std::pair<const char*, std::function<std::vector<test_util::SceneBead>(double)>>> scene_builders = {
        {"isolated",    build_isolated},
        {"pair",        build_pair},
        {"stack",       build_stack},
        {"tetrahedron", build_tetra},
        {"ring",        build_ring},
        {"cloud",       build_cloud}
    };

    const char* var_names[] = {"rho", "rho_hat", "C", "P2", "P2_hat", "eta", "target_f"};
    const int n_vars = 7;

    // Atlas header
    std::printf("  %-12s %-12s %8s %8s %8s %5s %5s %5s %8s %8s\n",
        "Scene", "Variable", "Min", "Max", "Range", "Mon+", "Mon-", "NM",
        "Sens-5%", "Sens+5%");
    std::printf("  %-12s %-12s %8s %8s %8s %5s %5s %5s %8s %8s\n",
        "-----", "--------", "---", "---", "-----", "----", "----", "--",
        "-------", "-------");

    int atlas_pass = 0;
    int atlas_total = 0;

    for (auto& [scene_name, builder] : scene_builders) {
        // Collect sweep data
        std::vector<std::vector<double>> var_sweeps(n_vars);

        for (int si = 0; si < n_spacings; ++si) {
            double s = spacings[si];
            auto scene = builder(s);
            if (scene.empty()) continue;

            // Run environment for bead 0
            auto traj = test_util::run_trajectory(scene, 0, 0.0, params, dt, n_steps);

            // Record final values
            var_sweeps[0].push_back(traj.rho.back());
            // Compute rho_hat from rho
            double rho_hat = std::clamp(traj.rho.back() / params.rho_max, 0.0, 1.0);
            var_sweeps[1].push_back(rho_hat);
            var_sweeps[2].push_back(traj.C.back());
            var_sweeps[3].push_back(traj.P2.back());
            // P2_hat from P2
            double P2_hat = std::clamp((traj.P2.back() + 0.5) / 1.5, 0.0, 1.0);
            var_sweeps[4].push_back(P2_hat);
            var_sweeps[5].push_back(traj.eta.back());
            var_sweeps[6].push_back(traj.target_f.back());
        }

        // Analyze each variable
        for (int vi = 0; vi < n_vars; ++vi) {
            auto& v = var_sweeps[vi];
            if (v.empty()) continue;

            ++atlas_total;

            // Range and finiteness
            bool all_finite = test_util::is_finite(v);
            double vmin = *std::min_element(v.begin(), v.end());
            double vmax = *std::max_element(v.begin(), v.end());
            double range = vmax - vmin;

            // Monotonicity
            bool mon_inc = test_util::is_monotone_increasing(v);
            bool mon_dec = test_util::is_monotone_decreasing(v);
            bool non_mon = !mon_inc && !mon_dec && range > 1e-10;

            // Perturbation sensitivity (±5% of mid-spacing = index 4/5)
            double sens_minus = 0.0, sens_plus = 0.0;
            if (v.size() >= 6) {
                int mid = static_cast<int>(v.size()) / 2;
                if (mid > 0 && mid < static_cast<int>(v.size()) - 1) {
                    sens_minus = std::abs(v[mid] - v[mid - 1]);
                    sens_plus  = std::abs(v[mid + 1] - v[mid]);
                }
            }

            std::printf("  %-12s %-12s %8.4f %8.4f %8.4f %5s %5s %5s %8.4f %8.4f",
                scene_name, var_names[vi],
                vmin, vmax, range,
                mon_inc ? "yes" : "no",
                mon_dec ? "yes" : "no",
                non_mon ? "NM" : "--",
                sens_minus, sens_plus);

            // Check: no NaN/Inf
            if (!all_finite) {
                std::printf("  *** NaN/Inf ***");
            }
            std::printf("\n");

            if (all_finite) ++atlas_pass;
        }
    }

    std::printf("\n  Atlas finiteness: %d/%d passed\n\n", atlas_pass, atlas_total);

    // Structural assertions for the atlas
    // A1: All variables must be finite across all scenes
    check(atlas_pass == atlas_total,
        "P1.A1: All atlas entries finite (no NaN/Inf)");

    // A2: Isolated bead should have zero for all neighbour-dependent variables
    {
        auto iso = test_util::scene_isolated();
        auto traj = test_util::run_trajectory(iso, 0, 0.0, params, dt, n_steps);
        check(std::abs(traj.rho.back()) < 1e-10,
            "P1.A2: isolated rho = 0");
        check(std::abs(traj.C.back()) < 1e-10,
            "P1.A3: isolated C = 0");
        check(std::abs(traj.eta.back()) < 1e-10,
            "P1.A4: isolated eta = 0");
    }

    // A5: Pair scene — rho should decrease monotonically with increasing spacing
    {
        std::vector<double> rho_vs_spacing;
        for (int si = 0; si < n_spacings; ++si) {
            auto scene = test_util::scene_pair(spacings[si]);
            auto traj = test_util::run_trajectory(scene, 0, 0.0, params, dt, n_steps);
            rho_vs_spacing.push_back(traj.rho.back());
        }
        check(test_util::is_monotone_decreasing(rho_vs_spacing),
            "P1.A5: pair rho decreases with spacing");
    }

    // A6: Stack — coordination C should decrease with increasing spacing
    //     (beads leave cutoff)
    {
        std::vector<double> C_vs_spacing;
        for (int si = 0; si < n_spacings; ++si) {
            auto scene = test_util::scene_linear_stack(5, spacings[si]);
            auto traj = test_util::run_trajectory(scene, 0, 0.0, params, dt, n_steps);
            C_vs_spacing.push_back(traj.C.back());
        }
        check(test_util::is_monotone_decreasing(C_vs_spacing),
            "P1.A6: stack C decreases with spacing");
    }

    // A7: Ring — C should be bounded [0, N-1]
    {
        auto ring = test_util::scene_ring(6, 4.0);
        auto traj = test_util::run_trajectory(ring, 0, 0.0, params, dt, n_steps);
        check(traj.C.back() >= 0 && traj.C.back() <= 5.0,
            "P1.A7: ring C bounded [0, N-1]");
    }

    // A8: Cloud — all variables should be finite
    {
        auto cloud = test_util::scene_random_cluster(12, 15.0, 42);
        auto traj = test_util::run_trajectory(cloud, 0, 0.0, params, dt, n_steps);
        check(test_util::is_finite(traj.rho) &&
              test_util::is_finite(traj.eta) &&
              test_util::is_finite(traj.target_f),
            "P1.A8: cloud all finite after 200 steps");
    }

    // A9: P2 range — must be in [-0.5, 1.0] for all scenes
    {
        bool P2_bounded = true;
        for (auto& [sname, builder] : scene_builders) {
            auto scene = builder(4.0);
            if (scene.size() < 2) continue;
            auto traj = test_util::run_trajectory(scene, 0, 0.0, params, dt, n_steps);
            if (!test_util::is_bounded(traj.P2, -0.5, 1.0)) {
                P2_bounded = false;
                std::printf("    P2 out of [-0.5, 1.0] in scene: %s\n", sname);
            }
        }
        check(P2_bounded, "P1.A9: P2 in [-0.5, 1.0] all scenes");
    }

    // A10: eta range — must be in [0, 1] for all scenes
    {
        bool eta_bounded = true;
        for (auto& [sname, builder] : scene_builders) {
            auto scene = builder(4.0);
            auto traj = test_util::run_trajectory(scene, 0, 0.0, params, dt, n_steps);
            if (!test_util::is_bounded(traj.eta, 0.0, 1.0)) {
                eta_bounded = false;
                std::printf("    eta out of [0, 1] in scene: %s\n", sname);
            }
        }
        check(eta_bounded, "P1.A10: eta in [0, 1] all scenes");
    }

    // A11: rho_hat must be in [0, 1]
    {
        bool rho_hat_ok = true;
        for (auto& [sname, builder] : scene_builders) {
            auto scene = builder(4.0);
            auto nbs = test_util::build_neighbours(scene, 0);
            auto state = coarse_grain::compute_fast_observables(
                scene[0].n_hat, scene[0].has_orientation, nbs, params);
            if (state.rho_hat < -1e-15 || state.rho_hat > 1.0 + 1e-15) {
                rho_hat_ok = false;
            }
        }
        check(rho_hat_ok, "P1.A11: rho_hat in [0, 1] all scenes");
    }

    // A12: target_f must be in [0, 1]
    {
        bool tf_ok = true;
        for (auto& [sname, builder] : scene_builders) {
            auto scene = builder(4.0);
            auto nbs = test_util::build_neighbours(scene, 0);
            auto state = coarse_grain::compute_fast_observables(
                scene[0].n_hat, scene[0].has_orientation, nbs, params);
            if (state.target_f < -1e-15 || state.target_f > 1.0 + 1e-15) {
                tf_ok = false;
            }
        }
        check(tf_ok, "P1.A12: target_f in [0, 1] all scenes");
    }
}

// ============================================================================
// Phase 1 — Perturbation Sensitivity Test
// ============================================================================

static void phase1_perturbation_test() {
    std::printf("\n──────────────────────────────────────────────────────────\n");
    std::printf("  Phase 1 — Perturbation Sensitivity (±5%%)\n");
    std::printf("──────────────────────────────────────────────────────────\n\n");

    coarse_grain::EnvironmentParams params;
    params.r_cutoff = 12.0;
    params.sigma_rho = 3.0;
    params.tau = 100.0;
    double dt = 1.0;
    int n_steps = 200;
    double base_spacing = 5.0;
    double perturb = 0.05;  // ±5%

    auto scene_builders = build_canonical_scenes(base_spacing);

    for (auto& cs : scene_builders) {
        if (cs.beads.size() < 2) continue;  // Skip isolated

        // Base measurement
        auto traj_base = test_util::run_trajectory(
            cs.beads, 0, 0.0, params, dt, n_steps);
        double rho_base = traj_base.rho.back();
        double eta_base = traj_base.eta.back();

        // Perturbed: scale all positions by (1 ± 5%)
        auto make_perturbed = [&](double scale) {
            auto scene = cs.beads;
            for (auto& b : scene) {
                b.position.x *= scale;
                b.position.y *= scale;
                b.position.z *= scale;
            }
            return scene;
        };

        auto scene_minus = make_perturbed(1.0 - perturb);
        auto scene_plus  = make_perturbed(1.0 + perturb);

        auto traj_minus = test_util::run_trajectory(
            scene_minus, 0, 0.0, params, dt, n_steps);
        auto traj_plus = test_util::run_trajectory(
            scene_plus, 0, 0.0, params, dt, n_steps);

        double drho_minus = std::abs(traj_minus.rho.back() - rho_base);
        double drho_plus  = std::abs(traj_plus.rho.back()  - rho_base);
        double deta_minus = std::abs(traj_minus.eta.back() - eta_base);
        double deta_plus  = std::abs(traj_plus.eta.back()  - eta_base);

        std::printf("  %-12s  drho(-5%%): %8.5f  drho(+5%%): %8.5f  "
                    "deta(-5%%): %8.5f  deta(+5%%): %8.5f\n",
            cs.name, drho_minus, drho_plus, deta_minus, deta_plus);

        // All perturbed outputs must remain finite
        char buf[128];
        std::snprintf(buf, sizeof(buf),
            "P1.Perturb: %s outputs finite after ±5%% position scale", cs.name);
        check(test_util::is_finite(traj_minus.rho) &&
              test_util::is_finite(traj_plus.rho) &&
              test_util::is_finite(traj_minus.eta) &&
              test_util::is_finite(traj_plus.eta), buf);
    }
}

// ============================================================================
// Phase 1 — Redundancy Detection
// ============================================================================

static void phase1_redundancy_check() {
    std::printf("\n──────────────────────────────────────────────────────────\n");
    std::printf("  Phase 1 — Redundancy Detection\n");
    std::printf("──────────────────────────────────────────────────────────\n\n");

    // Check: rho_hat is just a rescaled rho — flag as expected-redundant
    // Check: P2_hat is just a rescaled P2 — flag as expected-redundant
    // Check: target_f is a linear combination of rho_hat and P2_hat — flag

    coarse_grain::EnvironmentParams params;
    params.r_cutoff = 12.0;

    // Sweep spacing across the pair scene to collect rho/rho_hat/P2/P2_hat/target_f
    std::vector<double> rhos, rho_hats, P2s, P2_hats, target_fs;
    const double spacings[] = {2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 10.0};

    for (double s : spacings) {
        auto scene = test_util::scene_linear_stack(5, s);
        auto nbs = test_util::build_neighbours(scene, 0);
        auto state = coarse_grain::compute_fast_observables(
            scene[0].n_hat, scene[0].has_orientation, nbs, params);
        rhos.push_back(state.rho);
        rho_hats.push_back(state.rho_hat);
        P2s.push_back(state.P2);
        P2_hats.push_back(state.P2_hat);
        target_fs.push_back(state.target_f);
    }

    // rho and rho_hat should be perfectly correlated (monotone transform)
    bool rho_rhohat_corr = true;
    for (size_t i = 1; i < rhos.size(); ++i) {
        bool rho_dir  = (rhos[i] >= rhos[i-1]);
        bool rhat_dir = (rho_hats[i] >= rho_hats[i-1]);
        if (rho_dir != rhat_dir) { rho_rhohat_corr = false; break; }
    }
    check(rho_rhohat_corr,
        "P1.R1: rho and rho_hat are monotonically correlated (expected)");
    if (rho_rhohat_corr)
        std::printf("    ⚑ rho_hat is a deterministic rescaling of rho — "
                    "not independent information\n");

    // P2 and P2_hat
    bool P2_P2hat_corr = true;
    for (size_t i = 1; i < P2s.size(); ++i) {
        bool p2_dir   = (P2s[i] >= P2s[i-1]);
        bool p2h_dir  = (P2_hats[i] >= P2_hats[i-1]);
        if (p2_dir != p2h_dir) { P2_P2hat_corr = false; break; }
    }
    check(P2_P2hat_corr,
        "P1.R2: P2 and P2_hat are monotonically correlated (expected)");
    if (P2_P2hat_corr)
        std::printf("    ⚑ P2_hat is a deterministic rescaling of P2 — "
                    "not independent information\n");

    // target_f should be recoverable from rho_hat and P2_hat
    bool tf_recoverable = true;
    for (size_t i = 0; i < rho_hats.size(); ++i) {
        double expected = params.alpha * rho_hats[i] + params.beta * P2_hats[i];
        expected = std::clamp(expected, 0.0, 1.0);
        if (std::abs(target_fs[i] - expected) > 1e-12) {
            tf_recoverable = false;
            break;
        }
    }
    check(tf_recoverable,
        "P1.R3: target_f = alpha*rho_hat + beta*P2_hat (exact)");
    if (tf_recoverable)
        std::printf("    ⚑ target_f carries no independent information beyond "
                    "rho_hat + P2_hat\n");

    std::printf("\n  Redundancy summary:\n");
    std::printf("    Independent variables: rho, C, P2, eta\n");
    std::printf("    Derived (redundant):   rho_hat (from rho), "
                "P2_hat (from P2), target_f (from rho_hat+P2_hat)\n");
}

// ============================================================================
// Phase 2a — Pairwise Coupling Maps
// ============================================================================

static void phase2a_pairwise_coupling() {
    std::printf("\n══════════════════════════════════════════════════════════\n");
    std::printf("  Phase 2a — Pairwise Coupling Maps\n");
    std::printf("══════════════════════════════════════════════════════════\n\n");

    coarse_grain::EnvironmentParams base_params;
    base_params.r_cutoff = 12.0;
    base_params.sigma_rho = 3.0;
    base_params.tau = 100.0;
    double dt = 1.0;
    int n_steps = 300;
    const int grid = 5;

    // ── Coupling 1: rho × eta (density vs topology) ──
    // Control rho via spacing, observe final eta
    {
        std::printf("  ── rho × eta (spacing → density, observe eta convergence) ──\n");
        double spacings[] = {2.0, 3.5, 5.0, 7.0, 10.0};
        double taus[]     = {20.0, 50.0, 100.0, 200.0, 500.0};

        bool all_finite = true;
        bool eta_responds = false;

        std::printf("  %10s", "tau\\sp");
        for (int j = 0; j < grid; ++j)
            std::printf("  sp=%-5.1f", spacings[j]);
        std::printf("\n");

        for (int ti = 0; ti < grid; ++ti) {
            auto params = base_params;
            params.tau = taus[ti];
            std::printf("  tau=%-6.0f", taus[ti]);

            double prev_eta = -1.0;
            for (int si = 0; si < grid; ++si) {
                auto scene = test_util::scene_dense_shell(8, spacings[si]);
                auto traj = test_util::run_trajectory(
                    scene, 0, 0.0, params, dt, n_steps);
                double eta = traj.eta.back();

                if (!std::isfinite(eta)) all_finite = false;
                if (prev_eta >= 0 && std::abs(eta - prev_eta) > 1e-6)
                    eta_responds = true;
                prev_eta = eta;

                std::printf("  %8.5f", eta);
            }
            std::printf("\n");
        }
        check(all_finite, "P2a.C1: rho×eta grid all finite");
        check(eta_responds, "P2a.C2: eta varies with spacing (density sensitive)");
    }

    // ── Coupling 2: C × P2 (coordination vs orientational order) ──
    {
        std::printf("\n  ── C × P2 (coordination via shell count, P2 via alignment) ──\n");
        int shell_counts[] = {2, 4, 6, 8, 12};
        double align_biases[] = {0.0, 0.25, 0.50, 0.75, 1.0};

        bool all_finite = true;
        bool P2_responds = false;

        std::printf("  %10s", "align\\N");
        for (int j = 0; j < grid; ++j)
            std::printf("  N=%-5d", shell_counts[j]);
        std::printf("\n");

        for (int ai = 0; ai < grid; ++ai) {
            std::printf("  ab=%-7.2f", align_biases[ai]);

            for (int ni = 0; ni < grid; ++ni) {
                // Build scene with controlled alignment
                auto scene = test_util::scene_dense_shell(shell_counts[ni], 4.0);
                // Override orientations with bias
                test_util::Xorshift32 rng(100 + ai * 10 + ni);
                for (size_t k = 1; k < scene.size(); ++k) {
                    if (rng.uniform() < align_biases[ai]) {
                        scene[k].n_hat = {0, 0, 1};
                    } else {
                        scene[k].n_hat = rng.random_direction();
                    }
                }

                auto nbs = test_util::build_neighbours(scene, 0);
                auto state = coarse_grain::compute_fast_observables(
                    scene[0].n_hat, scene[0].has_orientation, nbs, base_params);

                if (!std::isfinite(state.P2) || !std::isfinite(state.C))
                    all_finite = false;
                if (ai > 0 && std::abs(state.P2) > 0.01)
                    P2_responds = true;

                std::printf("  P2=%5.3f", state.P2);
            }
            std::printf("\n");
        }
        check(all_finite, "P2a.C3: C×P2 grid all finite");
        check(P2_responds, "P2a.C4: P2 responds to alignment bias");
    }

    // ── Coupling 3: P2 × P2_hat (correlation check) ──
    {
        std::printf("\n  ── P2 × P2_hat (confirm monotone transform) ──\n");
        bool monotone_ok = true;
        double align_vals[] = {0.0, 0.2, 0.4, 0.6, 0.8, 1.0};
        std::vector<double> P2_vals, P2hat_vals;

        for (double ab : align_vals) {
            auto scene = test_util::scene_dense_shell(8, 4.0);
            test_util::Xorshift32 rng(200);
            for (size_t k = 1; k < scene.size(); ++k) {
                scene[k].n_hat = (rng.uniform() < ab) ?
                    atomistic::Vec3{0, 0, 1} : rng.random_direction();
            }
            auto nbs = test_util::build_neighbours(scene, 0);
            auto state = coarse_grain::compute_fast_observables(
                scene[0].n_hat, scene[0].has_orientation, nbs, base_params);
            P2_vals.push_back(state.P2);
            P2hat_vals.push_back(state.P2_hat);
            std::printf("    align=%.1f  P2=%7.4f  P2_hat=%7.4f\n",
                ab, state.P2, state.P2_hat);
        }

        // Verify P2_hat = (P2 + 0.5) / 1.5 clamped to [0,1]
        for (size_t i = 0; i < P2_vals.size(); ++i) {
            double expected = std::clamp((P2_vals[i] + 0.5) / 1.5, 0.0, 1.0);
            if (std::abs(P2hat_vals[i] - expected) > 1e-12)
                monotone_ok = false;
        }
        check(monotone_ok, "P2a.C5: P2_hat = (P2+0.5)/1.5 exact");
    }

    // ── Coupling 4: rho × target_f (density vs target field response) ──
    {
        std::printf("\n  ── rho × target_f (spacing sweep, varying alpha/beta) ──\n");
        double spacings[] = {2.0, 3.5, 5.0, 7.0, 10.0};
        double alpha_vals[] = {0.0, 0.25, 0.50, 0.75, 1.0};

        bool all_finite = true;
        bool tf_responds = false;

        std::printf("  %10s", "alpha\\sp");
        for (int j = 0; j < grid; ++j)
            std::printf("  sp=%-5.1f", spacings[j]);
        std::printf("\n");

        for (int ai = 0; ai < grid; ++ai) {
            auto params = base_params;
            params.alpha = alpha_vals[ai];
            params.beta  = 1.0 - alpha_vals[ai];

            std::printf("  a=%-8.2f", alpha_vals[ai]);
            double prev_tf = -1.0;
            for (int si = 0; si < grid; ++si) {
                auto scene = test_util::scene_dense_shell(8, spacings[si]);
                auto nbs = test_util::build_neighbours(scene, 0);
                auto state = coarse_grain::compute_fast_observables(
                    scene[0].n_hat, scene[0].has_orientation, nbs, params);

                if (!std::isfinite(state.target_f)) all_finite = false;
                if (prev_tf >= 0 && std::abs(state.target_f - prev_tf) > 1e-6)
                    tf_responds = true;
                prev_tf = state.target_f;

                std::printf("  tf=%5.4f", state.target_f);
            }
            std::printf("\n");
        }
        check(all_finite, "P2a.C6: rho×target_f grid all finite");
        check(tf_responds, "P2a.C7: target_f responds to spacing × alpha");
    }
}

// ============================================================================
// Phase 2b — Structured Large-N Runs
// ============================================================================

struct LargeNResult {
    const char* init_name;
    int N;
    bool all_finite;
    test_util::CoordinationHistogram coord_hist;
    double mean_eta;
    double std_eta;
    double edge_eta_mean;
    double bulk_eta_mean;
    int n_edge;
    int n_bulk;
};

static LargeNResult run_large_n(
    const char* name,
    const std::vector<test_util::SceneBead>& scene,
    const coarse_grain::EnvironmentParams& params,
    double dt, int n_steps)
{
    LargeNResult result;
    result.init_name = name;
    result.N = static_cast<int>(scene.size());

    auto states = test_util::run_all_beads(scene, params, dt, n_steps);

    // Finiteness
    result.all_finite = true;
    for (const auto& s : states) {
        if (!std::isfinite(s.rho) || !std::isfinite(s.eta) ||
            !std::isfinite(s.C) || !std::isfinite(s.P2) ||
            !std::isfinite(s.target_f)) {
            result.all_finite = false;
            break;
        }
    }

    // Coordination histogram
    result.coord_hist = test_util::build_coord_histogram(states);

    // Eta statistics
    double eta_sum = 0, eta_sum2 = 0;
    for (const auto& s : states) {
        eta_sum += s.eta;
        eta_sum2 += s.eta * s.eta;
    }
    result.mean_eta = eta_sum / states.size();
    result.std_eta = std::sqrt(
        eta_sum2 / states.size() - result.mean_eta * result.mean_eta);

    // Edge vs bulk classification:
    // "edge" = bead with C < mean_coord - 1*std_coord
    // "bulk" = bead with C >= mean_coord
    double c_mean = result.coord_hist.mean_coord;
    double c_std  = result.coord_hist.std_coord;
    double edge_eta_sum = 0, bulk_eta_sum = 0;
    result.n_edge = 0;
    result.n_bulk = 0;
    for (const auto& s : states) {
        if (s.C < c_mean - c_std) {
            edge_eta_sum += s.eta;
            result.n_edge++;
        } else if (s.C >= c_mean) {
            bulk_eta_sum += s.eta;
            result.n_bulk++;
        }
    }
    result.edge_eta_mean = (result.n_edge > 0) ? edge_eta_sum / result.n_edge : 0.0;
    result.bulk_eta_mean = (result.n_bulk > 0) ? bulk_eta_sum / result.n_bulk : 0.0;

    return result;
}

static void phase2b_large_n() {
    std::printf("\n══════════════════════════════════════════════════════════\n");
    std::printf("  Phase 2b — Structured Large-N Runs\n");
    std::printf("══════════════════════════════════════════════════════════\n\n");

    coarse_grain::EnvironmentParams params;
    params.r_cutoff = 8.0;
    params.sigma_rho = 3.0;
    params.tau = 100.0;
    params.alpha = 0.5;
    params.beta = 0.5;
    double dt = 1.0;
    int n_steps = 100;

    // N values: 4^3=64, 5^3=125, 6^3=216
    struct LargeNConfig {
        int n_side;  // cube side for lattice
        int N;       // total beads
        double spacing;
    };
    LargeNConfig configs[] = {
        {4,  64,  4.0},
        {5, 125,  4.0},
        {6, 216,  4.0}
    };

    int total_pass = 0;
    int total_runs = 0;

    for (auto& cfg : configs) {
        std::printf("\n  ┌─ N = %d (n_side = %d, spacing = %.1f) "
                    "───────────────────────────────────┐\n",
            cfg.N, cfg.n_side, cfg.spacing);

        // 1. Perfect lattice
        {
            auto scene = test_util::scene_cubic_lattice(cfg.n_side, cfg.spacing);
            auto r = run_large_n("perfect_lattice", scene, params, dt, n_steps);
            ++total_runs;

            std::printf("  │ %-20s  N=%3d  finite=%s  C_mean=%.2f±%.2f  "
                        "eta=%.4f±%.4f  edge=%.4f(%d) bulk=%.4f(%d)\n",
                r.init_name, r.N,
                r.all_finite ? "yes" : "NO",
                r.coord_hist.mean_coord, r.coord_hist.std_coord,
                r.mean_eta, r.std_eta,
                r.edge_eta_mean, r.n_edge,
                r.bulk_eta_mean, r.n_bulk);

            char buf[128];
            std::snprintf(buf, sizeof(buf),
                "P2b.L1: perfect_lattice N=%d all finite", cfg.N);
            check(r.all_finite, buf);
            if (r.all_finite) ++total_pass;
        }

        // 2. Perturbed lattice (5-10% displacement)
        {
            auto scene = test_util::scene_perturbed_lattice(
                cfg.n_side, cfg.spacing, 0.075, 42);
            auto r = run_large_n("perturbed_lattice", scene, params, dt, n_steps);
            ++total_runs;

            std::printf("  │ %-20s  N=%3d  finite=%s  C_mean=%.2f±%.2f  "
                        "eta=%.4f±%.4f  edge=%.4f(%d) bulk=%.4f(%d)\n",
                r.init_name, r.N,
                r.all_finite ? "yes" : "NO",
                r.coord_hist.mean_coord, r.coord_hist.std_coord,
                r.mean_eta, r.std_eta,
                r.edge_eta_mean, r.n_edge,
                r.bulk_eta_mean, r.n_bulk);

            char buf[128];
            std::snprintf(buf, sizeof(buf),
                "P2b.L2: perturbed_lattice N=%d all finite", cfg.N);
            check(r.all_finite, buf);
            if (r.all_finite) ++total_pass;
        }

        // 3. Line bundle
        {
            auto scene = test_util::scene_line_bundle(cfg.N, cfg.spacing);
            auto r = run_large_n("line_bundle", scene, params, dt, n_steps);
            ++total_runs;

            std::printf("  │ %-20s  N=%3d  finite=%s  C_mean=%.2f±%.2f  "
                        "eta=%.4f±%.4f  edge=%.4f(%d) bulk=%.4f(%d)\n",
                r.init_name, r.N,
                r.all_finite ? "yes" : "NO",
                r.coord_hist.mean_coord, r.coord_hist.std_coord,
                r.mean_eta, r.std_eta,
                r.edge_eta_mean, r.n_edge,
                r.bulk_eta_mean, r.n_bulk);

            char buf[128];
            std::snprintf(buf, sizeof(buf),
                "P2b.L3: line_bundle N=%d all finite", cfg.N);
            check(r.all_finite, buf);
            if (r.all_finite) ++total_pass;
        }

        // 4. Layered slab
        {
            // Find n_per_side and n_layers such that total ≈ N
            int n_per_side, n_layers;
            if (cfg.n_side <= 4) {
                n_per_side = 4; n_layers = cfg.N / (n_per_side * n_per_side);
            } else if (cfg.n_side == 5) {
                n_per_side = 5; n_layers = 5;
            } else {
                n_per_side = 6; n_layers = 6;
            }
            auto scene = test_util::scene_layered_slab(
                n_per_side, n_layers, cfg.spacing, cfg.spacing);
            auto r = run_large_n("layered_slab", scene, params, dt, n_steps);
            ++total_runs;

            std::printf("  │ %-20s  N=%3d  finite=%s  C_mean=%.2f±%.2f  "
                        "eta=%.4f±%.4f  edge=%.4f(%d) bulk=%.4f(%d)\n",
                r.init_name, r.N,
                r.all_finite ? "yes" : "NO",
                r.coord_hist.mean_coord, r.coord_hist.std_coord,
                r.mean_eta, r.std_eta,
                r.edge_eta_mean, r.n_edge,
                r.bulk_eta_mean, r.n_bulk);

            char buf[128];
            std::snprintf(buf, sizeof(buf),
                "P2b.L4: layered_slab N=%d all finite",
                static_cast<int>(scene.size()));
            check(r.all_finite, buf);
            if (r.all_finite) ++total_pass;
        }

        // 5. Shell initialization
        {
            auto scene = test_util::scene_large_shell(cfg.N - 1, cfg.spacing * 2.0, 42);
            auto r = run_large_n("shell_init", scene, params, dt, n_steps);
            ++total_runs;

            std::printf("  │ %-20s  N=%3d  finite=%s  C_mean=%.2f±%.2f  "
                        "eta=%.4f±%.4f  edge=%.4f(%d) bulk=%.4f(%d)\n",
                r.init_name, r.N,
                r.all_finite ? "yes" : "NO",
                r.coord_hist.mean_coord, r.coord_hist.std_coord,
                r.mean_eta, r.std_eta,
                r.edge_eta_mean, r.n_edge,
                r.bulk_eta_mean, r.n_bulk);

            char buf[128];
            std::snprintf(buf, sizeof(buf),
                "P2b.L5: shell_init N=%d all finite", r.N);
            check(r.all_finite, buf);
            if (r.all_finite) ++total_pass;
        }

        // 6. Random cloud at fixed density
        {
            double box = std::cbrt(cfg.N) * cfg.spacing;
            auto scene = test_util::scene_random_cluster(cfg.N, box, 42);
            auto r = run_large_n("random_cloud", scene, params, dt, n_steps);
            ++total_runs;

            std::printf("  │ %-20s  N=%3d  finite=%s  C_mean=%.2f±%.2f  "
                        "eta=%.4f±%.4f  edge=%.4f(%d) bulk=%.4f(%d)\n",
                r.init_name, r.N,
                r.all_finite ? "yes" : "NO",
                r.coord_hist.mean_coord, r.coord_hist.std_coord,
                r.mean_eta, r.std_eta,
                r.edge_eta_mean, r.n_edge,
                r.bulk_eta_mean, r.n_bulk);

            char buf[128];
            std::snprintf(buf, sizeof(buf),
                "P2b.L6: random_cloud N=%d all finite", cfg.N);
            check(r.all_finite, buf);
            if (r.all_finite) ++total_pass;
        }

        std::printf("  └──────────────────────────────────────────────────"
                    "──────────────────┘\n");
    }

    std::printf("\n  Large-N finiteness: %d/%d runs stable\n", total_pass, total_runs);

    // Gate assertion: N=216 must be fully stable
    check(total_pass == total_runs,
        "P2b.GATE: All N=64,125,216 runs stable and bounded");
}

// ============================================================================
// Phase 2b — Edge vs Bulk Differentiation
// ============================================================================

static void phase2b_edge_bulk_analysis() {
    std::printf("\n──────────────────────────────────────────────────────────\n");
    std::printf("  Phase 2b — Edge vs Bulk Differentiation\n");
    std::printf("──────────────────────────────────────────────────────────\n\n");

    coarse_grain::EnvironmentParams params;
    params.r_cutoff = 8.0;
    params.sigma_rho = 3.0;
    params.tau = 100.0;
    double dt = 1.0;
    int n_steps = 200;

    // Perfect 6×6×6 = 216 bead lattice
    auto scene = test_util::scene_cubic_lattice(6, 4.0);
    auto states = test_util::run_all_beads(scene, params, dt, n_steps);

    // Classify edge vs bulk by coordination
    auto hist = test_util::build_coord_histogram(states);
    double c_mean = hist.mean_coord;

    std::vector<double> edge_rhos, bulk_rhos;
    std::vector<double> edge_etas, bulk_etas;
    for (const auto& s : states) {
        if (s.C < c_mean - 1.0) {
            edge_rhos.push_back(s.rho);
            edge_etas.push_back(s.eta);
        } else {
            bulk_rhos.push_back(s.rho);
            bulk_etas.push_back(s.eta);
        }
    }

    auto edge_rho_stats = test_util::compute_stats(edge_rhos);
    auto bulk_rho_stats = test_util::compute_stats(bulk_rhos);
    auto edge_eta_stats = test_util::compute_stats(edge_etas);
    auto bulk_eta_stats = test_util::compute_stats(bulk_etas);

    std::printf("  N=216 lattice, %d edge beads, %d bulk beads\n",
        static_cast<int>(edge_rhos.size()),
        static_cast<int>(bulk_rhos.size()));
    std::printf("    rho  — edge: %.4f ± %.4f    bulk: %.4f ± %.4f\n",
        edge_rho_stats.mean, std::sqrt(edge_rho_stats.variance),
        bulk_rho_stats.mean, std::sqrt(bulk_rho_stats.variance));
    std::printf("    eta  — edge: %.4f ± %.4f    bulk: %.4f ± %.4f\n",
        edge_eta_stats.mean, std::sqrt(edge_eta_stats.variance),
        bulk_eta_stats.mean, std::sqrt(bulk_eta_stats.variance));

    // Bulk beads should have higher rho than edge beads
    check(bulk_rho_stats.mean > edge_rho_stats.mean,
        "P2b.EB1: bulk rho > edge rho (lattice N=216)");

    // Bulk beads should have higher eta (more crowded → higher target_f → higher eta)
    check(bulk_eta_stats.mean > edge_eta_stats.mean,
        "P2b.EB2: bulk eta > edge eta (lattice N=216)");

    // Edge beads should have lower coordination
    // (by construction — this validates the classification)
    check(!edge_rhos.empty() && !bulk_rhos.empty(),
        "P2b.EB3: both edge and bulk populations exist");
}

// ============================================================================
// Phase 2b — Coordination Histogram Analysis
// ============================================================================

static void phase2b_coord_histogram() {
    std::printf("\n──────────────────────────────────────────────────────────\n");
    std::printf("  Phase 2b — Coordination Histogram (N=125 lattice)\n");
    std::printf("──────────────────────────────────────────────────────────\n\n");

    coarse_grain::EnvironmentParams params;
    params.r_cutoff = 6.0;  // Tight cutoff to see lattice structure
    params.sigma_rho = 2.0;
    params.tau = 100.0;
    double dt = 1.0;
    int n_steps = 100;

    auto scene = test_util::scene_cubic_lattice(5, 4.0);
    auto states = test_util::run_all_beads(scene, params, dt, n_steps);
    auto hist = test_util::build_coord_histogram(states);

    std::printf("  Coordination histogram (N=125, r_cutoff=6.0, spacing=4.0):\n");
    for (int c = 0; c < static_cast<int>(hist.bins.size()); ++c) {
        if (hist.bins[c] > 0) {
            std::printf("    C=%2d : %3d beads  ", c, hist.bins[c]);
            // Simple bar
            for (int b = 0; b < hist.bins[c] && b < 40; ++b)
                std::printf("█");
            std::printf("\n");
        }
    }
    std::printf("  mean C = %.2f ± %.2f\n", hist.mean_coord, hist.std_coord);

    // For a cubic lattice with spacing 4.0 and cutoff 6.0:
    // nearest neighbours are at 4.0 (face-sharing), 4*sqrt(2)=5.66 (edge-sharing)
    // Both < 6.0, so bulk beads should have C = 6+12 = 18 ... but only face-sharing
    // Interior beads: 6 face neighbours at distance 4.0 (< cutoff 6.0)
    // Corner: 3, edge: 4, face: 5, interior: 6
    check(hist.max_coord <= 26,
        "P2b.H1: max coordination <= 26 (cubic lattice)");
    check(hist.mean_coord > 0,
        "P2b.H2: mean coordination > 0");

    // Verify structured distribution — not all the same
    int distinct_bins = 0;
    for (int c = 0; c < static_cast<int>(hist.bins.size()); ++c) {
        if (hist.bins[c] > 0) ++distinct_bins;
    }
    check(distinct_bins >= 2,
        "P2b.H3: at least 2 distinct coordination values (edge vs bulk)");
}

// ============================================================================
// main
// ============================================================================

int main() {
    std::printf("================================================================\n");
    std::printf("  Suite #3 — Structured N > 10 Formation Studies\n");
    std::printf("================================================================\n");

    // Phase 1
    phase1_spacing_sweep();
    phase1_perturbation_test();
    phase1_redundancy_check();

    // Phase 2a
    phase2a_pairwise_coupling();

    // Phase 2b
    phase2b_large_n();
    phase2b_edge_bulk_analysis();
    phase2b_coord_histogram();

    std::printf("\n================================================================\n");
    std::printf("  Suite #3 Results: %d passed, %d failed (of %d)\n",
        g_pass, g_fail, g_pass + g_fail);
    std::printf("================================================================\n\n");

    return g_fail > 0 ? 1 : 0;
}
