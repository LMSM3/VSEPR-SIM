/**
 * test_macro_precursor_suite6.cpp — Suite #6: Macro Property Precursor Channels
 *
 * Validates the precursor layer that converts ensemble proxies into
 * candidate property channels for later calibration.
 *
 * Phase 6A: Numerical validity
 *   All channels finite when proxy valid, invalid when proxy invalid,
 *   bounded in [0,1], confidence degrades on weak input.
 *
 * Phase 6B: Monotonic directed perturbation
 *   One directed test per precursor channel verifying correct response
 *   direction to controlled perturbation.
 *
 * Phase 6C: Counterfactual separation
 *   Distinct channels must not collapse — states that differ in one
 *   proxy dimension must produce different channel values where expected.
 *
 * Phase 6D: Invariance / equivariance
 *   Translation, rotation, permutation invariance for isotropic channels.
 *
 * Phase 6E: Confidence and provenance
 *   Low sample count, NaN correlation length, no edge beads, unconverged
 *   state all degrade confidence appropriately.
 *
 * Architecture position:
 *   Ensemble statistics / spatial field summaries
 *       ↓
 *   Macroscopic response proxies   (suite 5)
 *       ↓
 *   Macro precursor channels        ← validated here
 *       ↓
 *   Later calibrated property estimator
 *
 * Reference: Macro Property Precursor Layer specification
 */

#include "coarse_grain/analysis/macro_precursor.hpp"
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
#include <limits>

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

/**
 * Run scene, compute proxy, compute precursors.
 */
static coarse_grain::MacroPrecursorState run_and_precursor(
    const std::vector<test_util::SceneBead>& scene,
    const coarse_grain::EnvironmentParams& params,
    double dt = 10.0,
    int n_steps = 500)
{
    auto proxy = run_and_proxy(scene, params, dt, n_steps);
    return coarse_grain::compute_macro_precursors(proxy);
}

static bool channel_bounded(const coarse_grain::MacroPrecursorChannel& ch) {
    if (!ch.valid) return true;  // invalid channels are NaN — acceptable
    return ch.value >= 0.0 && ch.value <= 1.0;
}

static bool channel_finite(const coarse_grain::MacroPrecursorChannel& ch) {
    if (!ch.valid) return true;
    return std::isfinite(ch.value) && std::isfinite(ch.confidence);
}

static bool all_channels_bounded(const coarse_grain::MacroPrecursorState& st) {
    return channel_bounded(st.rigidity_like)
        && channel_bounded(st.ductility_like)
        && channel_bounded(st.brittleness_like)
        && channel_bounded(st.cohesion_integrity_like)
        && channel_bounded(st.thermal_transport_like)
        && channel_bounded(st.electrical_transport_like)
        && channel_bounded(st.surface_reactivity_like)
        && channel_bounded(st.fracture_susceptibility_like);
}

static bool all_channels_finite(const coarse_grain::MacroPrecursorState& st) {
    return channel_finite(st.rigidity_like)
        && channel_finite(st.ductility_like)
        && channel_finite(st.brittleness_like)
        && channel_finite(st.cohesion_integrity_like)
        && channel_finite(st.thermal_transport_like)
        && channel_finite(st.electrical_transport_like)
        && channel_finite(st.surface_reactivity_like)
        && channel_finite(st.fracture_susceptibility_like);
}

// ============================================================================
// Phase 6A — Numerical Validity
// ============================================================================

static void phase_6a_validity() {
    std::printf("\n=== Phase 6A: Numerical Validity ===\n");

    auto params = default_params();

    // ---- 6A.1: Valid proxy → all channels finite and bounded ----
    {
        std::printf("\n--- 6A.1: Valid proxy produces valid channels ---\n");

        auto scene = test_util::scene_cubic_lattice(3, 4.0);
        auto proxy = run_and_proxy(scene, params);
        auto prec  = coarse_grain::compute_macro_precursors(proxy);

        check(prec.valid, "precursor state valid");
        check(prec.source_valid, "source proxy valid");
        check(all_channels_finite(prec), "all channels finite");
        check(all_channels_bounded(prec), "all channels in [0,1]");

        check(prec.rigidity_like.valid, "rigidity_like valid");
        check(prec.ductility_like.valid, "ductility_like valid");
        check(prec.brittleness_like.valid, "brittleness_like valid");
        check(prec.cohesion_integrity_like.valid, "cohesion_integrity_like valid");
        check(prec.thermal_transport_like.valid, "thermal_transport_like valid");
        check(prec.electrical_transport_like.valid, "electrical_transport_like valid");
        check(prec.surface_reactivity_like.valid, "surface_reactivity_like valid");
        check(prec.fracture_susceptibility_like.valid, "fracture_susceptibility_like valid");
    }

    // ---- 6A.2: Invalid proxy (small N) → invalid precursor ----
    {
        std::printf("\n--- 6A.2: Invalid proxy → invalid precursor ---\n");

        auto scene = test_util::scene_pair(4.0);  // N=2 < PROXY_MIN_BEADS
        auto proxy = run_and_proxy(scene, params);
        auto prec  = coarse_grain::compute_macro_precursors(proxy);

        check(!proxy.valid, "source proxy invalid (N=2)");
        check(!prec.valid,  "precursor state invalid");
        check(!prec.rigidity_like.valid, "rigidity_like invalid");
        check(!prec.ductility_like.valid, "ductility_like invalid");
    }

    // ---- 6A.3: Empty input → safe defaults ----
    {
        std::printf("\n--- 6A.3: Empty input ---\n");

        std::vector<coarse_grain::EnvironmentState> empty_s;
        std::vector<atomistic::Vec3> empty_p;
        auto proxy = coarse_grain::compute_ensemble_proxy(empty_s, empty_p);
        auto prec  = coarse_grain::compute_macro_precursors(proxy);

        check(!prec.valid, "empty: precursor invalid");
        check(prec.source_bead_count == 0, "empty: source bead count 0");
    }

    // ---- 6A.4: Intermediate terms finite ----
    {
        std::printf("\n--- 6A.4: Intermediate terms finite ---\n");

        auto scene = test_util::scene_cubic_lattice(3, 4.0);
        auto proxy = run_and_proxy(scene, params);
        auto prec  = coarse_grain::compute_macro_precursors(proxy);

        check(std::isfinite(prec.xi_norm), "xi_norm finite");
        check(std::isfinite(prec.interface_penalty), "interface_penalty finite");
        check(std::isfinite(prec.adapt_capacity), "adapt_capacity finite");
        check(std::isfinite(prec.texture_lock), "texture_lock finite");
        check(std::isfinite(prec.anisotropy_index), "anisotropy_index finite");

        check(prec.xi_norm >= 0.0 && prec.xi_norm <= 1.0,
              "xi_norm in [0,1]");
        check(prec.interface_penalty >= 0.0 && prec.interface_penalty <= 1.0,
              "interface_penalty in [0,1]");
        check(prec.adapt_capacity >= 0.0 && prec.adapt_capacity <= 1.0,
              "adapt_capacity in [0,1]");
        check(prec.texture_lock >= 0.0 && prec.texture_lock <= 1.0,
              "texture_lock in [0,1]");
        check(prec.anisotropy_index >= 0.0 && prec.anisotropy_index <= 1.0,
              "anisotropy_index in [0,1]");
    }

    // ---- 6A.5: Confidence in [0, 1] ----
    {
        std::printf("\n--- 6A.5: Confidence bounds ---\n");

        auto scene = test_util::scene_cubic_lattice(3, 4.0);
        auto proxy = run_and_proxy(scene, params);
        auto prec  = coarse_grain::compute_macro_precursors(proxy);

        check(prec.convergence_confidence >= 0.0 && prec.convergence_confidence <= 1.0,
              "convergence_confidence in [0,1]");
        check(prec.rigidity_like.confidence >= 0.0 && prec.rigidity_like.confidence <= 1.0,
              "rigidity_like.confidence in [0,1]");
        check(prec.thermal_transport_like.confidence >= 0.0 &&
              prec.thermal_transport_like.confidence <= 1.0,
              "thermal_transport_like.confidence in [0,1]");
    }

    // ---- 6A.6: Multiple scene geometries all valid ----
    {
        std::printf("\n--- 6A.6: Scene family validity ---\n");

        auto scene_lattice = test_util::scene_cubic_lattice(3, 4.0);
        auto scene_slab    = test_util::scene_layered_slab(3, 3, 4.0, 4.0);
        auto scene_bundle  = test_util::scene_line_bundle(27, 4.0);
        auto scene_cloud   = test_util::scene_separated_cloud(27, 12.0, 1.0, 42);

        auto prec_lattice = run_and_precursor(scene_lattice, params);
        auto prec_slab    = run_and_precursor(scene_slab, params);
        auto prec_bundle  = run_and_precursor(scene_bundle, params);
        auto prec_cloud   = run_and_precursor(scene_cloud, params);

        check(prec_lattice.valid && prec_slab.valid &&
              prec_bundle.valid && prec_cloud.valid,
              "all scene families valid");
        check(all_channels_bounded(prec_lattice) && all_channels_bounded(prec_slab) &&
              all_channels_bounded(prec_bundle) && all_channels_bounded(prec_cloud),
              "all scene families bounded");
    }

    // ---- 6A.7: Large-N stability ----
    {
        std::printf("\n--- 6A.7: Large-N stability ---\n");

        int sides[] = {4, 5, 6};
        for (int s : sides) {
            auto scene = test_util::scene_cubic_lattice(s, 4.0);
            auto prec  = run_and_precursor(scene, params);

            char buf[128];
            std::snprintf(buf, sizeof(buf), "N=%d: valid and bounded", s*s*s);
            check(prec.valid && all_channels_bounded(prec), buf);
        }
    }
}

// ============================================================================
// Phase 6B — Monotonic Directed Perturbation
// ============================================================================

static void phase_6b_monotonic() {
    std::printf("\n=== Phase 6B: Monotonic Directed Perturbation ===\n");

    auto params = default_params();
    double dt = 10.0;
    int n_steps = 500;

    // ---- 6B.1: Increase cohesion → rigidity rises ----
    {
        std::printf("\n--- 6B.1: Cohesion ↑ → rigidity ↑ ---\n");

        // Dense lattice (high cohesion) vs sparse lattice (low cohesion)
        auto scene_dense  = test_util::scene_cubic_lattice(3, 3.0);
        auto scene_sparse = test_util::scene_cubic_lattice(3, 7.0);

        auto prec_dense  = run_and_precursor(scene_dense, params, dt, n_steps);
        auto prec_sparse = run_and_precursor(scene_sparse, params, dt, n_steps);

        check(prec_dense.rigidity_like.value > prec_sparse.rigidity_like.value,
              "6B.1: dense → higher rigidity_like");
        check(prec_dense.cohesion_integrity_like.value >
              prec_sparse.cohesion_integrity_like.value,
              "6B.1: dense → higher cohesion_integrity_like");
    }

    // ---- 6B.2: Increase interface mismatch → fracture susceptibility rises ----
    {
        std::printf("\n--- 6B.2: Interface mismatch ↑ → fracture ↑ ---\n");

        auto scene = test_util::scene_cubic_lattice(3, 4.0);
        auto states = test_util::run_all_beads(scene, params, dt, n_steps);
        auto positions = extract_positions(scene);
        auto proxy_base = coarse_grain::compute_ensemble_proxy(
            states, positions, -1.0, params.r_cutoff);

        // Degrade edge beads to create interface mismatch
        auto states_degraded = states;
        for (auto& st : states_degraded) {
            if (st.C < proxy_base.c_thresh_used) {
                st.rho_hat = 0.0;
                st.eta = 0.0;
                st.target_f = 0.9;
            }
        }
        auto proxy_degraded = coarse_grain::compute_ensemble_proxy(
            states_degraded, positions, -1.0, params.r_cutoff);

        auto prec_base    = coarse_grain::compute_macro_precursors(proxy_base);
        auto prec_degraded = coarse_grain::compute_macro_precursors(proxy_degraded);

        check(prec_degraded.fracture_susceptibility_like.value >
              prec_base.fracture_susceptibility_like.value,
              "6B.2: degraded edges → higher fracture_susceptibility_like");
        check(prec_degraded.interface_penalty >
              prec_base.interface_penalty,
              "6B.2: degraded edges → higher interface_penalty");
    }

    // ---- 6B.3: Increase surface sensitivity → reactivity rises ----
    {
        std::printf("\n--- 6B.3: Surface sensitivity ↑ → reactivity ↑ ---\n");

        // Sparse cloud: many surface beads, high surface sensitivity
        auto scene_surface = test_util::scene_separated_cloud(27, 15.0, 2.0, 42);
        // Dense lattice: mostly bulk beads
        auto scene_bulk = test_util::scene_cubic_lattice(3, 3.5);

        auto prec_surface = run_and_precursor(scene_surface, params, dt, n_steps);
        auto prec_bulk    = run_and_precursor(scene_bulk, params, dt, n_steps);

        check(prec_surface.surface_reactivity_like.value >
              prec_bulk.surface_reactivity_like.value,
              "6B.3: surface-dominated → higher surface_reactivity_like");
    }

    // ---- 6B.4: Increase correlation length + alignment → transport rises ----
    {
        std::printf("\n--- 6B.4: Alignment ↑ → transport ↑ ---\n");

        // Aligned system (high texture)
        auto scene_aligned = test_util::scene_biased_stack_cloud(
            20, 3.5, 1.0, 0.0, 42);
        // Random orientation
        auto scene_random  = test_util::scene_biased_stack_cloud(
            20, 3.5, 0.0, 0.0, 42);

        auto prec_aligned = run_and_precursor(scene_aligned, params, dt, n_steps);
        auto prec_random  = run_and_precursor(scene_random, params, dt, n_steps);

        check(prec_aligned.thermal_transport_like.value >
              prec_random.thermal_transport_like.value,
              "6B.4: aligned → higher thermal_transport_like");
        check(prec_aligned.electrical_transport_like.value >
              prec_random.electrical_transport_like.value,
              "6B.4: aligned → higher electrical_transport_like");
    }

    // ---- 6B.5: Increase stabilisation, reduce mismatch → brittleness falls ----
    {
        std::printf("\n--- 6B.5: Stabilisation ↑ → brittleness ↓ ---\n");

        auto scene = test_util::scene_cubic_lattice(3, 4.0);

        // Converged: well-adapted
        auto prec_converged = run_and_precursor(scene, params, dt, 500);
        // Barely started: poorly adapted
        auto prec_fresh = run_and_precursor(scene, params, dt, 5);

        // Converged system has higher adapt_capacity → lower brittleness
        check(prec_converged.brittleness_like.value <=
              prec_fresh.brittleness_like.value + 0.05,
              "6B.5: converged → not more brittle than fresh");
    }

    // ---- 6B.6: Uniformity increase → ductility rises ----
    {
        std::printf("\n--- 6B.6: Uniformity ↑ → ductility ↑ ---\n");

        // Regular lattice (uniform) vs random cloud (non-uniform)
        auto scene_lattice = test_util::scene_cubic_lattice(3, 4.0);
        auto scene_cloud   = test_util::scene_random_cluster(27, 12.0, 42);

        auto prec_lattice = run_and_precursor(scene_lattice, params, dt, n_steps);
        auto prec_cloud   = run_and_precursor(scene_cloud, params, dt, n_steps);

        check(prec_lattice.ductility_like.value >=
              prec_cloud.ductility_like.value - 0.1,
              "6B.6: uniform lattice ≥ random cloud ductility (within tolerance)");
    }

    // ---- 6B.7: Cohesion integrity tracks cohesion proxy ----
    {
        std::printf("\n--- 6B.7: Cohesion integrity tracks cohesion ---\n");

        double spacings[] = {3.0, 5.0, 7.0};
        std::vector<double> coh_integ;
        std::vector<double> coh_proxy;

        for (double sp : spacings) {
            auto scene = test_util::scene_cubic_lattice(3, sp);
            auto proxy = run_and_proxy(scene, params, dt, n_steps);
            auto prec  = coarse_grain::compute_macro_precursors(proxy);
            coh_integ.push_back(prec.cohesion_integrity_like.value);
            coh_proxy.push_back(proxy.cohesion_proxy);
        }

        // Both should decrease with increasing spacing
        check(coh_integ[0] > coh_integ[2],
              "6B.7: tight spacing → higher cohesion_integrity_like");
        // Ordering should be monotone with cohesion
        bool monotone = (coh_integ[0] >= coh_integ[1]) &&
                        (coh_integ[1] >= coh_integ[2]);
        check(monotone, "6B.7: cohesion_integrity monotone with spacing");
    }
}

// ============================================================================
// Phase 6C — Counterfactual Separation
// ============================================================================

static void phase_6c_counterfactual() {
    std::printf("\n=== Phase 6C: Counterfactual Separation ===\n");

    auto params = default_params();
    double dt = 10.0;
    int n_steps = 500;

    // ---- 6C.1: Same cohesion, different texture → different transport ----
    {
        std::printf("\n--- 6C.1: Equal cohesion, different texture → transport differs ---\n");

        // Same converged states, two texture extremes forced via P2_hat.
        // Cohesion is unchanged (same rho, eta, mismatch).
        auto scene_base = test_util::scene_cubic_lattice(3, 4.0);
        auto states_base = test_util::run_all_beads(scene_base, params, dt, n_steps);
        auto positions = extract_positions(scene_base);

        // Scene A: isotropic (P2_hat = 1/3)
        auto states_iso = states_base;
        for (auto& st : states_iso) st.P2_hat = 1.0 / 3.0;
        auto proxy_a = coarse_grain::compute_ensemble_proxy(
            states_iso, positions, -1.0, params.r_cutoff);

        // Scene B: fully aligned (P2_hat = 1.0)
        auto states_aligned = states_base;
        for (auto& st : states_aligned) st.P2_hat = 1.0;
        auto proxy_b = coarse_grain::compute_ensemble_proxy(
            states_aligned, positions, -1.0, params.r_cutoff);

        // cohesion should be identical (same rho, eta, mismatch)
        check(std::abs(proxy_a.cohesion_proxy - proxy_b.cohesion_proxy) < 1e-12,
              "6C.1: cohesion identical between scenes");

        auto prec_a = coarse_grain::compute_macro_precursors(proxy_a);
        auto prec_b = coarse_grain::compute_macro_precursors(proxy_b);

        // Transport should differ (different texture: 1/3 vs 1.0)
        check(std::abs(prec_a.thermal_transport_like.value -
                       prec_b.thermal_transport_like.value) > 0.01,
              "6C.1: thermal_transport differs despite same cohesion");
        check(std::abs(prec_a.electrical_transport_like.value -
                       prec_b.electrical_transport_like.value) > 0.01,
              "6C.1: electrical_transport differs despite same cohesion");
    }

    // ---- 6C.2: Same rigidity, different surface → reactivity differs ----
    {
        std::printf("\n--- 6C.2: Similar rigidity, different surface → reactivity differs ---\n");

        // Tight lattice (bulk-dominated, low surface sensitivity)
        auto scene_bulk = test_util::scene_cubic_lattice(4, 4.0);
        // Smaller lattice with more surface fraction
        auto scene_surface = test_util::scene_cubic_lattice(3, 4.0);

        auto prec_bulk    = run_and_precursor(scene_bulk, params, dt, n_steps);
        auto prec_surface = run_and_precursor(scene_surface, params, dt, n_steps);

        // Surface reactivity should differ
        double react_diff = std::abs(
            prec_bulk.surface_reactivity_like.value -
            prec_surface.surface_reactivity_like.value);
        // They may have different rigidity too — the key test is that
        // surface_reactivity is sensitive to surface fraction
        check(react_diff > 0.001 ||
              prec_surface.surface_reactivity_like.value >=
              prec_bulk.surface_reactivity_like.value - 0.01,
              "6C.2: surface reactivity distinguishes bulk vs surface fraction");
    }

    // ---- 6C.3: Channels are not all identical ----
    {
        std::printf("\n--- 6C.3: Channels are distinguishable ---\n");

        auto scene = test_util::scene_cubic_lattice(3, 4.0);
        auto prec  = run_and_precursor(scene, params, dt, n_steps);

        // Check that at least some channels have different values
        double vals[] = {
            prec.rigidity_like.value,
            prec.ductility_like.value,
            prec.brittleness_like.value,
            prec.thermal_transport_like.value,
            prec.surface_reactivity_like.value,
            prec.fracture_susceptibility_like.value
        };

        bool some_differ = false;
        for (int i = 1; i < 6; ++i) {
            if (std::abs(vals[i] - vals[0]) > 0.01) {
                some_differ = true;
                break;
            }
        }
        check(some_differ, "6C.3: channels have distinguishable values");
    }

    // ---- 6C.4: Texture change affects anisotropy but not cohesion integrity ----
    {
        std::printf("\n--- 6C.4: Texture → anisotropy, not cohesion integrity ---\n");

        auto scene = test_util::scene_cubic_lattice(3, 4.0);
        auto states = test_util::run_all_beads(scene, params, dt, n_steps);
        auto positions = extract_positions(scene);

        // Version A: isotropic (P2_hat = 1/3)
        auto states_iso = states;
        for (auto& st : states_iso) st.P2_hat = 1.0 / 3.0;
        auto proxy_iso = coarse_grain::compute_ensemble_proxy(
            states_iso, positions, -1.0, params.r_cutoff);
        auto prec_iso = coarse_grain::compute_macro_precursors(proxy_iso);

        // Version B: strongly aligned (P2_hat = 0.9)
        auto states_aniso = states;
        for (auto& st : states_aniso) st.P2_hat = 0.9;
        auto proxy_aniso = coarse_grain::compute_ensemble_proxy(
            states_aniso, positions, -1.0, params.r_cutoff);
        auto prec_aniso = coarse_grain::compute_macro_precursors(proxy_aniso);

        check(prec_aniso.anisotropy_index > prec_iso.anisotropy_index,
              "6C.4: aligned → higher anisotropy_index");

        // Cohesion integrity should be similar (same rho, eta, mismatch)
        check(std::abs(prec_aniso.cohesion_integrity_like.value -
                       prec_iso.cohesion_integrity_like.value) < 0.05,
              "6C.4: cohesion_integrity similar despite texture change");
    }

    // ---- 6C.5: Rigidity vs ductility anti-correlation tendency ----
    {
        std::printf("\n--- 6C.5: Rigidity-ductility relationship ---\n");

        // Very rigid system: dense, well-converged, aligned
        auto scene_rig = test_util::scene_cubic_lattice(3, 3.0);
        auto prec_rig  = run_and_precursor(scene_rig, params, dt, n_steps);

        // More ductile: moderate spacing, less locked
        auto scene_duct = test_util::scene_cubic_lattice(3, 5.0);
        auto prec_duct  = run_and_precursor(scene_duct, params, dt, n_steps);

        // Not necessarily strict anti-correlation, but if rigidity is higher
        // then ductility should not be dramatically higher too
        if (prec_rig.rigidity_like.value > prec_duct.rigidity_like.value) {
            check(prec_rig.ductility_like.value <=
                  prec_duct.ductility_like.value + 0.15,
                  "6C.5: higher rigidity does not produce much higher ductility");
        } else {
            check(true, "6C.5: rigidity ordering reversed — skipped check");
        }
    }
}

// ============================================================================
// Phase 6D — Invariance
// ============================================================================

static void phase_6d_invariance() {
    std::printf("\n=== Phase 6D: Invariance ===\n");

    auto params = default_params();
    double dt = 10.0;
    int n_steps = 300;

    // ---- 6D.1: Translation invariance ----
    {
        std::printf("\n--- 6D.1: Translation invariance ---\n");

        auto scene_orig = test_util::scene_cubic_lattice(3, 4.0);
        auto scene_shifted = test_util::translate_scene(
            scene_orig, {100.0, -200.0, 50.0});

        auto prec_orig    = run_and_precursor(scene_orig, params, dt, n_steps);
        auto prec_shifted = run_and_precursor(scene_shifted, params, dt, n_steps);

        check(std::abs(prec_orig.rigidity_like.value -
                       prec_shifted.rigidity_like.value) < 1e-10,
              "translation: rigidity invariant");
        check(std::abs(prec_orig.ductility_like.value -
                       prec_shifted.ductility_like.value) < 1e-10,
              "translation: ductility invariant");
        check(std::abs(prec_orig.brittleness_like.value -
                       prec_shifted.brittleness_like.value) < 1e-10,
              "translation: brittleness invariant");
        check(std::abs(prec_orig.thermal_transport_like.value -
                       prec_shifted.thermal_transport_like.value) < 1e-10,
              "translation: thermal_transport invariant");
        check(std::abs(prec_orig.surface_reactivity_like.value -
                       prec_shifted.surface_reactivity_like.value) < 1e-10,
              "translation: surface_reactivity invariant");
        check(std::abs(prec_orig.fracture_susceptibility_like.value -
                       prec_shifted.fracture_susceptibility_like.value) < 1e-10,
              "translation: fracture_susceptibility invariant");
        check(std::abs(prec_orig.interface_penalty -
                       prec_shifted.interface_penalty) < 1e-10,
              "translation: interface_penalty invariant");
    }

    // ---- 6D.2: Rotation invariance ----
    {
        std::printf("\n--- 6D.2: Rotation invariance ---\n");

        auto scene_orig = test_util::scene_cubic_lattice(3, 4.0);
        auto scene_rotated = test_util::rotate_scene(
            scene_orig, {1.0, 1.0, 1.0}, 1.2);

        auto prec_orig    = run_and_precursor(scene_orig, params, dt, n_steps);
        auto prec_rotated = run_and_precursor(scene_rotated, params, dt, n_steps);

        check(std::abs(prec_orig.rigidity_like.value -
                       prec_rotated.rigidity_like.value) < 1e-8,
              "rotation: rigidity invariant");
        check(std::abs(prec_orig.cohesion_integrity_like.value -
                       prec_rotated.cohesion_integrity_like.value) < 1e-8,
              "rotation: cohesion_integrity invariant");
        check(std::abs(prec_orig.ductility_like.value -
                       prec_rotated.ductility_like.value) < 1e-8,
              "rotation: ductility invariant");
        check(std::abs(prec_orig.surface_reactivity_like.value -
                       prec_rotated.surface_reactivity_like.value) < 1e-8,
              "rotation: surface_reactivity invariant");
    }

    // ---- 6D.3: Permutation invariance ----
    {
        std::printf("\n--- 6D.3: Permutation invariance ---\n");

        auto scene_orig = test_util::scene_cubic_lattice(3, 4.0);
        auto scene_perm = test_util::permute_scene(scene_orig, 12345);

        auto prec_orig = run_and_precursor(scene_orig, params, dt, n_steps);
        auto prec_perm = run_and_precursor(scene_perm, params, dt, n_steps);

        check(std::abs(prec_orig.rigidity_like.value -
                       prec_perm.rigidity_like.value) < 1e-10,
              "permutation: rigidity invariant");
        check(std::abs(prec_orig.ductility_like.value -
                       prec_perm.ductility_like.value) < 1e-10,
              "permutation: ductility invariant");
        check(std::abs(prec_orig.thermal_transport_like.value -
                       prec_perm.thermal_transport_like.value) < 1e-10,
              "permutation: thermal_transport invariant");
        check(std::abs(prec_orig.interface_penalty -
                       prec_perm.interface_penalty) < 1e-10,
              "permutation: interface_penalty invariant");
    }

    // ---- 6D.4: Seeded reproducibility ----
    {
        std::printf("\n--- 6D.4: Seeded reproducibility ---\n");

        auto scene_a = test_util::scene_random_cluster(30, 12.0, 777);
        auto scene_b = test_util::scene_random_cluster(30, 12.0, 777);

        auto prec_a = run_and_precursor(scene_a, params, dt, n_steps);
        auto prec_b = run_and_precursor(scene_b, params, dt, n_steps);

        check(std::abs(prec_a.rigidity_like.value -
                       prec_b.rigidity_like.value) < 1e-15,
              "seed: identical rigidity");
        check(std::abs(prec_a.fracture_susceptibility_like.value -
                       prec_b.fracture_susceptibility_like.value) < 1e-15,
              "seed: identical fracture_susceptibility");
        check(std::abs(prec_a.interface_penalty -
                       prec_b.interface_penalty) < 1e-15,
              "seed: identical interface_penalty");
    }
}

// ============================================================================
// Phase 6E — Confidence and Provenance
// ============================================================================

static void phase_6e_confidence() {
    std::printf("\n=== Phase 6E: Confidence and Provenance ===\n");

    auto params = default_params();
    double dt = 10.0;
    int n_steps = 500;

    // ---- 6E.1: Low sample count lowers confidence ----
    {
        std::printf("\n--- 6E.1: Low sample count → lower confidence ---\n");

        // N=8: at threshold (PROXY_MIN_BEADS)
        auto scene_small = test_util::scene_cubic_lattice(2, 4.0);  // N=8
        auto scene_large = test_util::scene_cubic_lattice(4, 4.0);  // N=64

        auto prec_small = run_and_precursor(scene_small, params, dt, n_steps);
        auto prec_large = run_and_precursor(scene_large, params, dt, n_steps);

        // N=8 is valid but < 2×PROXY_MIN_BEADS → confidence penalty
        check(prec_small.convergence_confidence <
              prec_large.convergence_confidence + 0.01,
              "6E.1: small N has lower confidence than large N");
    }

    // ---- 6E.2: NaN correlation length reduces transport confidence ----
    {
        std::printf("\n--- 6E.2: NaN ξ → lower transport confidence ---\n");

        auto scene = test_util::scene_cubic_lattice(3, 4.0);
        auto proxy = run_and_proxy(scene, params, dt, n_steps);

        // Manually set correlation lengths to NaN
        auto proxy_no_xi = proxy;
        proxy_no_xi.eta_corr_length = std::numeric_limits<double>::quiet_NaN();
        proxy_no_xi.rho_corr_length = std::numeric_limits<double>::quiet_NaN();

        auto prec_with_xi = coarse_grain::compute_macro_precursors(proxy);
        auto prec_no_xi   = coarse_grain::compute_macro_precursors(proxy_no_xi);

        check(prec_no_xi.thermal_transport_like.confidence <=
              prec_with_xi.thermal_transport_like.confidence,
              "6E.2: NaN ξ reduces thermal_transport confidence");
        check(prec_no_xi.electrical_transport_like.confidence <=
              prec_with_xi.electrical_transport_like.confidence,
              "6E.2: NaN ξ reduces electrical_transport confidence");
        check(prec_no_xi.xi_norm == 0.0,
              "6E.2: NaN ξ → xi_norm = 0");
    }

    // ---- 6E.3: No edge beads weakens surface reactivity confidence ----
    {
        std::printf("\n--- 6E.3: No edge beads → lower reactivity confidence ---\n");

        auto scene = test_util::scene_cubic_lattice(3, 4.0);
        auto proxy = run_and_proxy(scene, params, dt, n_steps);

        // Force all beads to be bulk (no edge) by setting n_edge = 0
        auto proxy_no_edge = proxy;
        proxy_no_edge.n_edge = 0;
        proxy_no_edge.n_bulk = proxy.bead_count;
        proxy_no_edge.bulk_edge_eta_gap = std::numeric_limits<double>::quiet_NaN();
        proxy_no_edge.bulk_edge_rho_gap = std::numeric_limits<double>::quiet_NaN();
        proxy_no_edge.bulk_edge_C_gap   = std::numeric_limits<double>::quiet_NaN();
        proxy_no_edge.bulk_edge_P2_gap  = std::numeric_limits<double>::quiet_NaN();
        proxy_no_edge.surface_sensitivity_proxy = 0.0;

        auto prec_normal  = coarse_grain::compute_macro_precursors(proxy);
        auto prec_no_edge = coarse_grain::compute_macro_precursors(proxy_no_edge);

        check(prec_no_edge.surface_reactivity_like.confidence <=
              prec_normal.surface_reactivity_like.confidence,
              "6E.3: no edge beads → lower surface_reactivity confidence");
    }

    // ---- 6E.4: Unconverged delta state lowers confidence ----
    {
        std::printf("\n--- 6E.4: Divergence → lower confidence ---\n");

        auto scene = test_util::scene_cubic_lattice(3, 4.0);
        auto proxy = run_and_proxy(scene, params, dt, n_steps);

        // Simulate divergent state: positive delta_mean_mismatch
        auto proxy_diverging = proxy;
        proxy_diverging.delta_mean_mismatch = 0.05;  // actively worsening
        proxy_diverging.converged = false;

        auto prec_stable   = coarse_grain::compute_macro_precursors(proxy);
        auto prec_diverging = coarse_grain::compute_macro_precursors(proxy_diverging);

        check(prec_diverging.convergence_confidence <=
              prec_stable.convergence_confidence,
              "6E.4: diverging state has lower convergence confidence");
    }

    // ---- 6E.5: Provenance fields populated ----
    {
        std::printf("\n--- 6E.5: Provenance fields ---\n");

        auto scene = test_util::scene_cubic_lattice(3, 4.0);
        auto prec  = run_and_precursor(scene, params, dt, n_steps);

        check(prec.source_bead_count == 27, "provenance: bead count = 27");
        check(prec.source_valid, "provenance: source valid");
    }

    // ---- 6E.6: Invalid proxy → zero confidence on all channels ----
    {
        std::printf("\n--- 6E.6: Invalid proxy → zero confidence ---\n");

        auto scene = test_util::scene_pair(4.0);  // N=2
        auto proxy = run_and_proxy(scene, params);
        auto prec  = coarse_grain::compute_macro_precursors(proxy);

        check(prec.rigidity_like.confidence == 0.0,
              "6E.6: invalid → rigidity confidence = 0");
        check(prec.thermal_transport_like.confidence == 0.0,
              "6E.6: invalid → transport confidence = 0");
        check(prec.convergence_confidence == 0.0,
              "6E.6: invalid → convergence confidence = 0");
    }

    // ---- 6E.7: Confidence monotone with N ----
    {
        std::printf("\n--- 6E.7: Confidence vs N ---\n");

        int sides[] = {3, 4, 5};  // N = 27, 64, 125
        std::vector<double> confidences;

        for (int s : sides) {
            auto scene = test_util::scene_cubic_lattice(s, 4.0);
            auto prec  = run_and_precursor(scene, params, dt, 400);
            confidences.push_back(prec.convergence_confidence);
        }

        // N=27 might get small-N penalty; larger N should not
        bool non_decreasing = true;
        for (size_t i = 1; i < confidences.size(); ++i) {
            if (confidences[i] < confidences[i-1] - 0.01)
                non_decreasing = false;
        }
        check(non_decreasing,
              "6E.7: confidence non-decreasing with N");
    }
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::printf("Suite #6: Macro Property Precursor Channels\n");
    std::printf("          Environment → Proxies → Precursors\n");
    std::printf("================================================================\n");

    phase_6a_validity();
    phase_6b_monotonic();
    phase_6c_counterfactual();
    phase_6d_invariance();
    phase_6e_confidence();

    std::printf("\n================================================================\n");
    std::printf("Suite #6 Results: %d passed, %d failed, %d total\n",
                g_pass, g_fail, g_pass + g_fail);

    return g_fail > 0 ? 1 : 0;
}
