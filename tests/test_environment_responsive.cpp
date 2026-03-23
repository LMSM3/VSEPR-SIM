/**
 * test_environment_responsive.cpp — Tests for Environment-Responsive Bead State
 *
 * Validates the implementation of the environment-responsive extension
 * as specified in section_environment_responsive_beads.tex:
 *
 *   Group A: Environment Parameters
 *   Group B: Switching and Weighting Functions
 *   Group C: Fast Observables (rho, C, P2)
 *   Group D: Slow State Integration (eta)
 *   Group E: Target Function and Normalisation
 *   Group F: Kernel Modulation (Option A)
 *   Group G: Full Update Cycle
 *   Group H: Invariants and Edge Cases
 *
 * Reference: "Environment-Responsive Coarse-Grained Bead Dynamics"
 */

#include "coarse_grain/core/environment_state.hpp"
#include "coarse_grain/models/environment_coupling.hpp"
#include "coarse_grain/core/channel_kernels.hpp"
#include "atomistic/core/state.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>

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

static constexpr double TOL = 1e-10;
static constexpr double RTOL = 1e-6;

// ============================================================================
// Group A: Environment Parameters
// ============================================================================

static void test_default_params() {
    coarse_grain::EnvironmentParams p;
    check(std::abs(p.alpha + p.beta - 1.0) < TOL,
          "A1: alpha + beta = 1 by default");
    check(p.tau > 0.0, "A2: tau > 0 by default");
    check(p.r_cutoff > 0.0, "A3: r_cutoff > 0 by default");
    check(p.sigma_rho > 0.0, "A4: sigma_rho > 0 by default");
    check(p.rho_max > 0.0, "A5: rho_max > 0 by default");
}

// ============================================================================
// Group B: Switching and Weighting Functions
// ============================================================================

static void test_switching_function() {
    using coarse_grain::switching_function;
    double r_c = 8.0, delta = 1.0;

    check(std::abs(switching_function(0.0, r_c, delta) - 1.0) < TOL,
          "B1: sw(0) = 1.0");
    check(std::abs(switching_function(6.0, r_c, delta) - 1.0) < TOL,
          "B2: sw(r < r_c - delta) = 1.0");
    check(std::abs(switching_function(r_c, r_c, delta)) < TOL,
          "B3: sw(r_c) = 0.0");
    check(std::abs(switching_function(10.0, r_c, delta)) < TOL,
          "B4: sw(r > r_c) = 0.0");

    // Midpoint of switching region
    double mid = r_c - delta / 2.0;
    double sw_mid = switching_function(mid, r_c, delta);
    check(sw_mid > 0.0 && sw_mid < 1.0,
          "B5: sw(midpoint) in (0, 1)");
    check(std::abs(sw_mid - 0.5) < 0.01,
          "B6: sw(midpoint) ~ 0.5");
}

static void test_density_weight() {
    using coarse_grain::density_weight;
    double sigma = 3.0, r_c = 8.0, delta = 1.0;

    check(std::abs(density_weight(0.0, sigma, r_c, delta) - 1.0) < TOL,
          "B7: w(0) = 1.0 (Gaussian peak, full switch)");
    check(std::abs(density_weight(r_c, sigma, r_c, delta)) < TOL,
          "B8: w(r_c) = 0.0");
    check(std::abs(density_weight(r_c + 1.0, sigma, r_c, delta)) < TOL,
          "B9: w(r > r_c) = 0.0");

    // Monotonically decreasing
    double w1 = density_weight(1.0, sigma, r_c, delta);
    double w2 = density_weight(3.0, sigma, r_c, delta);
    double w3 = density_weight(6.0, sigma, r_c, delta);
    check(w1 > w2 && w2 > w3,
          "B10: w(r) monotonically decreasing");
}

// ============================================================================
// Group C: Fast Observables
// ============================================================================

static void test_rho_empty() {
    atomistic::Vec3 n_hat{0, 0, 1};
    std::vector<coarse_grain::NeighbourInfo> empty;
    coarse_grain::EnvironmentParams params;

    auto state = coarse_grain::compute_fast_observables(n_hat, true, empty, params);
    check(std::abs(state.rho) < TOL, "C1: rho = 0 with no neighbours");
    check(std::abs(state.C) < TOL, "C2: C = 0 with no neighbours");
    check(std::abs(state.P2) < TOL, "C3: P2 = 0 with no neighbours");
    check(state.neighbour_count == 0, "C4: neighbour_count = 0");
}

static void test_rho_single_close() {
    atomistic::Vec3 n_hat{0, 0, 1};
    coarse_grain::NeighbourInfo nb;
    nb.distance = 2.0;
    nb.n_hat = {0, 0, 1};
    nb.has_orientation = true;

    coarse_grain::EnvironmentParams params;
    params.sigma_rho = 3.0;
    params.r_cutoff = 8.0;
    params.delta_sw = 1.0;

    auto state = coarse_grain::compute_fast_observables(
        n_hat, true, {nb}, params);

    check(state.rho > 0.0, "C5: rho > 0 with one close neighbour");
    check(std::abs(state.C - 1.0) < TOL, "C6: C = 1 with one neighbour");
    check(state.neighbour_count == 1, "C7: neighbour_count = 1");
}

static void test_rho_scales_with_neighbours() {
    atomistic::Vec3 n_hat{0, 0, 1};
    coarse_grain::EnvironmentParams params;
    params.sigma_rho = 3.0;
    params.r_cutoff = 8.0;

    // 4 neighbours at distance 3.0
    std::vector<coarse_grain::NeighbourInfo> nbs(4);
    for (auto& nb : nbs) {
        nb.distance = 3.0;
        nb.n_hat = {0, 0, 1};
        nb.has_orientation = true;
    }

    auto state = coarse_grain::compute_fast_observables(n_hat, true, nbs, params);
    check(std::abs(state.C - 4.0) < TOL, "C8: C = 4 with four neighbours");

    // Compare with single neighbour
    auto state1 = coarse_grain::compute_fast_observables(
        n_hat, true, {nbs[0]}, params);
    check(std::abs(state.rho - 4.0 * state1.rho) < TOL * 100,
          "C9: rho scales linearly with equal-distance neighbours");
}

static void test_P2_parallel_alignment() {
    atomistic::Vec3 n_hat{0, 0, 1};
    coarse_grain::NeighbourInfo nb;
    nb.distance = 4.0;
    nb.n_hat = {0, 0, 1}; // Perfectly parallel
    nb.has_orientation = true;

    coarse_grain::EnvironmentParams params;
    auto state = coarse_grain::compute_fast_observables(
        n_hat, true, {nb}, params);

    // P2 = 0.5 * (3 * 1^2 - 1) = 1.0
    check(std::abs(state.P2 - 1.0) < TOL,
          "C10: P2 = 1.0 for perfect parallel alignment");
}

static void test_P2_perpendicular() {
    atomistic::Vec3 n_hat{0, 0, 1};
    coarse_grain::NeighbourInfo nb;
    nb.distance = 4.0;
    nb.n_hat = {1, 0, 0}; // Perpendicular
    nb.has_orientation = true;

    coarse_grain::EnvironmentParams params;
    auto state = coarse_grain::compute_fast_observables(
        n_hat, true, {nb}, params);

    // P2 = 0.5 * (3 * 0^2 - 1) = -0.5
    check(std::abs(state.P2 + 0.5) < TOL,
          "C11: P2 = -0.5 for perpendicular alignment");
}

static void test_P2_antiparallel() {
    atomistic::Vec3 n_hat{0, 0, 1};
    coarse_grain::NeighbourInfo nb;
    nb.distance = 4.0;
    nb.n_hat = {0, 0, -1}; // Anti-parallel (should still give P2 = 1)
    nb.has_orientation = true;

    coarse_grain::EnvironmentParams params;
    auto state = coarse_grain::compute_fast_observables(
        n_hat, true, {nb}, params);

    // cos^2(pi) = 1, P2 = 0.5 * (3 * 1 - 1) = 1.0
    check(std::abs(state.P2 - 1.0) < TOL,
          "C12: P2 = 1.0 for anti-parallel (cos^2 = 1)");
}

static void test_P2_random_isotropic() {
    // Magic angle: cos(theta) = 1/sqrt(3), P2 = 0
    atomistic::Vec3 n_hat{0, 0, 1};
    double c = 1.0 / std::sqrt(3.0);
    double s = std::sqrt(1.0 - c * c);
    coarse_grain::NeighbourInfo nb;
    nb.distance = 4.0;
    nb.n_hat = {s, 0, c};
    nb.has_orientation = true;

    coarse_grain::EnvironmentParams params;
    auto state = coarse_grain::compute_fast_observables(
        n_hat, true, {nb}, params);

    check(std::abs(state.P2) < TOL,
          "C13: P2 ~ 0 at magic angle (cos = 1/sqrt(3))");
}

static void test_observables_outside_cutoff() {
    atomistic::Vec3 n_hat{0, 0, 1};
    coarse_grain::NeighbourInfo nb;
    nb.distance = 20.0; // Far beyond cutoff
    nb.n_hat = {0, 0, 1};
    nb.has_orientation = true;

    coarse_grain::EnvironmentParams params;
    params.r_cutoff = 8.0;
    auto state = coarse_grain::compute_fast_observables(
        n_hat, true, {nb}, params);

    check(std::abs(state.rho) < TOL, "C14: rho = 0 for distant neighbour");
    check(std::abs(state.C) < TOL, "C15: C = 0 for distant neighbour");
    check(state.neighbour_count == 0, "C16: neighbour_count = 0 for distant");
}

// ============================================================================
// Group D: Slow State Integration
// ============================================================================

static void test_eta_euler_at_target() {
    double eta = 0.7;
    double target = 0.7;
    double result = coarse_grain::integrate_eta_euler(eta, target, 1.0, 100.0);
    check(std::abs(result - 0.7) < TOL,
          "D1: euler: eta stays at target");
}

static void test_eta_euler_approaches_target() {
    double eta = 0.0;
    double target = 1.0;
    double dt = 10.0, tau = 100.0;
    double result = coarse_grain::integrate_eta_euler(eta, target, dt, tau);
    // eta_new = 0 + (10/100)*(1 - 0) = 0.1
    check(std::abs(result - 0.1) < TOL,
          "D2: euler: eta moves toward target");
}

static void test_eta_exact_at_target() {
    double eta = 0.5;
    double target = 0.5;
    double result = coarse_grain::integrate_eta_exact(eta, target, 1.0, 100.0);
    check(std::abs(result - 0.5) < TOL,
          "D3: exact: eta stays at target");
}

static void test_eta_exact_approaches_target() {
    double eta = 0.0;
    double target = 1.0;
    double dt = 100.0, tau = 100.0;
    double result = coarse_grain::integrate_eta_exact(eta, target, dt, tau);
    // eta_new = 1 + (0 - 1) * exp(-1) = 1 - e^(-1) ~ 0.6321
    double expected = 1.0 - std::exp(-1.0);
    check(std::abs(result - expected) < TOL,
          "D4: exact: eta approaches target (dt = tau)");
}

static void test_eta_exact_long_time() {
    double eta = 0.0;
    double target = 0.8;
    double dt = 10000.0, tau = 100.0;
    double result = coarse_grain::integrate_eta_exact(eta, target, dt, tau);
    check(std::abs(result - target) < 1e-6,
          "D5: exact: eta converges to target for dt >> tau");
}

static void test_eta_clamp_invariant() {
    // Test that eta stays in [0, 1]
    double r1 = coarse_grain::integrate_eta_exact(1.5, 0.5, 1.0, 100.0);
    double r2 = coarse_grain::integrate_eta_exact(-0.5, 0.5, 1.0, 100.0);
    check(r1 >= 0.0 && r1 <= 1.0, "D6: exact: result clamped to [0,1] (high)");
    check(r2 >= 0.0 && r2 <= 1.0, "D7: exact: result clamped to [0,1] (low)");
}

static void test_eta_tau_zero() {
    double result = coarse_grain::integrate_eta_exact(0.3, 0.8, 1.0, 0.0);
    check(std::abs(result - 0.8) < TOL,
          "D8: exact: tau=0 snaps to target");
}

static void test_eta_euler_vs_exact_small_dt() {
    double eta = 0.2, target = 0.9;
    double dt = 0.01, tau = 100.0;
    double euler = coarse_grain::integrate_eta_euler(eta, target, dt, tau);
    double exact = coarse_grain::integrate_eta_exact(eta, target, dt, tau);
    check(std::abs(euler - exact) < 1e-6,
          "D9: euler and exact agree for small dt/tau");
}

// ============================================================================
// Group E: Target Function and Normalisation
// ============================================================================

static void test_normalisation_rho_hat() {
    atomistic::Vec3 n_hat{0, 0, 1};
    coarse_grain::EnvironmentParams params;
    params.rho_max = 5.0;
    params.r_cutoff = 20.0;
    params.sigma_rho = 100.0; // Very wide -> w ~ 1

    // 10 neighbours very close -> rho ~ 10, rho_hat = clamp(10/5, 0, 1) = 1.0
    std::vector<coarse_grain::NeighbourInfo> nbs(10);
    for (auto& nb : nbs) {
        nb.distance = 1.0;
        nb.n_hat = {0, 0, 1};
        nb.has_orientation = true;
    }

    auto state = coarse_grain::compute_fast_observables(n_hat, true, nbs, params);
    check(std::abs(state.rho_hat - 1.0) < TOL,
          "E1: rho_hat clamped to 1.0 when rho > rho_max");
}

static void test_P2_hat_range() {
    // Parallel -> P2 = 1.0 -> P2_hat = (1.0 + 0.5) / 1.5 = 1.0
    {
        atomistic::Vec3 n_hat{0, 0, 1};
        coarse_grain::NeighbourInfo nb{4.0, {0, 0, 1}, true};
        coarse_grain::EnvironmentParams params;
        auto state = coarse_grain::compute_fast_observables(n_hat, true, {nb}, params);
        check(std::abs(state.P2_hat - 1.0) < TOL,
              "E2: P2_hat = 1.0 for P2 = 1.0");
    }
    // Perpendicular -> P2 = -0.5 -> P2_hat = 0.0
    {
        atomistic::Vec3 n_hat{0, 0, 1};
        coarse_grain::NeighbourInfo nb{4.0, {1, 0, 0}, true};
        coarse_grain::EnvironmentParams params;
        auto state = coarse_grain::compute_fast_observables(n_hat, true, {nb}, params);
        check(std::abs(state.P2_hat) < TOL,
              "E3: P2_hat = 0.0 for P2 = -0.5");
    }
}

static void test_target_function_weights() {
    atomistic::Vec3 n_hat{0, 0, 1};
    coarse_grain::NeighbourInfo nb{3.0, {0, 0, 1}, true};
    coarse_grain::EnvironmentParams params;
    params.alpha = 1.0;
    params.beta = 0.0;

    auto state = coarse_grain::compute_fast_observables(n_hat, true, {nb}, params);
    check(std::abs(state.target_f - state.rho_hat) < TOL,
          "E4: target_f = rho_hat when alpha=1, beta=0");

    params.alpha = 0.0;
    params.beta = 1.0;
    state = coarse_grain::compute_fast_observables(n_hat, true, {nb}, params);
    check(std::abs(state.target_f - state.P2_hat) < TOL,
          "E5: target_f = P2_hat when alpha=0, beta=1");
}

// ============================================================================
// Group F: Kernel Modulation
// ============================================================================

static void test_modulation_zero_eta() {
    coarse_grain::EnvironmentParams params;
    double g = coarse_grain::kernel_modulation_factor(
        coarse_grain::Channel::Steric, 0.0, 0.0, params);
    check(std::abs(g - 1.0) < TOL,
          "F1: modulation = 1.0 when eta_A = eta_B = 0");
}

static void test_modulation_dispersion_enhancement() {
    coarse_grain::EnvironmentParams params;
    params.gamma_disp = 0.5;
    double g = coarse_grain::kernel_modulation_factor(
        coarse_grain::Channel::Dispersion, 1.0, 1.0, params);
    // eta_bar = 1.0, g = 1 + 0.5 * 1.0 = 1.5
    check(std::abs(g - 1.5) < TOL,
          "F2: dispersion enhancement: g = 1.5 at full eta");
}

static void test_modulation_electrostatic_screening() {
    coarse_grain::EnvironmentParams params;
    params.gamma_elec = -0.1;
    double g = coarse_grain::kernel_modulation_factor(
        coarse_grain::Channel::Electrostatic, 1.0, 1.0, params);
    // eta_bar = 1.0, g = 1 + (-0.1) * 1.0 = 0.9
    check(std::abs(g - 0.9) < TOL,
          "F3: electrostatic screening: g = 0.9 at full eta");
}

static void test_modulation_steric_hardening() {
    coarse_grain::EnvironmentParams params;
    params.gamma_steric = 0.2;
    double g = coarse_grain::kernel_modulation_factor(
        coarse_grain::Channel::Steric, 0.8, 0.6, params);
    // eta_bar = 0.7, g = 1 + 0.2 * 0.7 = 1.14
    double expected = 1.0 + 0.2 * 0.7;
    check(std::abs(g - expected) < TOL,
          "F4: steric hardening: correct modulation factor");
}

static void test_modulation_sign_invariant() {
    // Ensure kernel cannot change sign
    coarse_grain::EnvironmentParams params;
    params.gamma_elec = -2.0; // Extreme negative
    double g = coarse_grain::kernel_modulation_factor(
        coarse_grain::Channel::Electrostatic, 1.0, 1.0, params);
    check(g > 0.0,
          "F5: modulation factor clamped > 0 (sign invariant)");
}

static void test_modulated_kernel_value() {
    coarse_grain::ChannelKernelParams kp{1.0, 1.0};
    coarse_grain::EnvironmentParams ep;
    ep.gamma_disp = 0.5;

    double K_base = coarse_grain::channel_kernel(
        coarse_grain::Channel::Dispersion, 0, 2.0, kp);
    double K_mod = coarse_grain::modulated_channel_kernel(
        coarse_grain::Channel::Dispersion, 0, 2.0, 1.0, 1.0, kp, ep);

    double expected = K_base * 1.5;
    check(std::abs(K_mod - expected) < std::abs(expected) * RTOL,
          "F6: modulated kernel = base * modulation factor");
}

static void test_modulation_report() {
    coarse_grain::EnvironmentParams params;
    params.gamma_steric = 0.2;
    params.gamma_elec = -0.1;
    params.gamma_disp = 0.5;

    auto report = coarse_grain::compute_modulation_report(0.6, 0.8, params);

    check(std::abs(report.eta_bar - 0.7) < TOL,
          "F7: report eta_bar correct");
    check(std::abs(report.g_steric - (1.0 + 0.2 * 0.7)) < TOL,
          "F8: report g_steric correct");
    check(std::abs(report.g_electrostatic - (1.0 - 0.1 * 0.7)) < TOL,
          "F9: report g_electrostatic correct");
    check(std::abs(report.g_dispersion - (1.0 + 0.5 * 0.7)) < TOL,
          "F10: report g_dispersion correct");
}

// ============================================================================
// Group G: Full Update Cycle
// ============================================================================

static void test_full_update_from_zero() {
    atomistic::Vec3 n_hat{0, 0, 1};
    coarse_grain::NeighbourInfo nb{3.0, {0, 0, 1}, true};
    coarse_grain::EnvironmentParams params;
    params.tau = 100.0;

    auto state = coarse_grain::update_environment_state(
        0.0, n_hat, true, {nb}, params, 10.0);

    check(state.eta > 0.0 && state.eta < 1.0,
          "G1: eta increases from 0 toward target");
    check(state.rho > 0.0, "G2: rho computed during update");
    check(state.C > 0.0, "G3: C computed during update");
    check(state.target_f > 0.0, "G4: target_f computed during update");
}

static void test_full_update_convergence() {
    atomistic::Vec3 n_hat{0, 0, 1};
    coarse_grain::NeighbourInfo nb{3.0, {0, 0, 1}, true};
    coarse_grain::EnvironmentParams params;
    params.tau = 10.0;

    double eta = 0.0;
    for (int step = 0; step < 1000; ++step) {
        auto state = coarse_grain::update_environment_state(
            eta, n_hat, true, {nb}, params, 1.0);
        eta = state.eta;
    }

    // After many steps, eta should converge to target_f
    auto final_state = coarse_grain::compute_fast_observables(
        n_hat, true, {nb}, params);
    check(std::abs(eta - final_state.target_f) < 0.01,
          "G5: eta converges to target_f after many steps");
}

static void test_full_update_no_neighbours() {
    atomistic::Vec3 n_hat{0, 0, 1};
    coarse_grain::EnvironmentParams params;
    params.tau = 100.0;

    auto state = coarse_grain::update_environment_state(
        0.5, n_hat, true, {}, params, 10.0);

    check(state.target_f < TOL,
          "G6: target_f = 0 with no neighbours");
    check(state.eta < 0.5,
          "G7: eta decreases toward 0 with no neighbours");
}

// ============================================================================
// Group H: Invariants and Edge Cases
// ============================================================================

static void test_eta_bounds_invariant() {
    // Multiple extreme updates should keep eta in [0, 1]
    double eta = 0.5;
    coarse_grain::EnvironmentParams params;
    params.tau = 0.001; // Very fast relaxation

    atomistic::Vec3 n_hat{0, 0, 1};
    std::vector<coarse_grain::NeighbourInfo> nbs(20);
    for (auto& nb : nbs) {
        nb.distance = 1.0;
        nb.n_hat = {0, 0, 1};
        nb.has_orientation = true;
    }

    for (int i = 0; i < 100; ++i) {
        auto state = coarse_grain::update_environment_state(
            eta, n_hat, true, nbs, params, 100.0);
        eta = state.eta;
        if (eta < 0.0 || eta > 1.0) break;
    }

    check(eta >= 0.0 && eta <= 1.0,
          "H1: eta in [0, 1] after extreme updates");
}

static void test_zero_distance_neighbour() {
    atomistic::Vec3 n_hat{0, 0, 1};
    coarse_grain::NeighbourInfo nb{0.0, {0, 0, 1}, true};
    coarse_grain::EnvironmentParams params;

    auto state = coarse_grain::compute_fast_observables(
        n_hat, true, {nb}, params);

    // Zero-distance neighbour should be skipped
    check(state.neighbour_count == 0,
          "H2: zero-distance neighbour skipped");
}

static void test_no_orientation_P2() {
    atomistic::Vec3 n_hat{0, 0, 1};
    coarse_grain::NeighbourInfo nb{4.0, {0, 0, 1}, false}; // No orientation
    coarse_grain::EnvironmentParams params;

    auto state = coarse_grain::compute_fast_observables(
        n_hat, true, {nb}, params);

    check(std::abs(state.P2) < TOL,
          "H3: P2 = 0 when neighbour has no orientation");
    check(state.C > 0.0,
          "H4: C still counted without orientation");
}

static void test_bead_no_orientation_P2() {
    atomistic::Vec3 n_hat{0, 0, 1};
    coarse_grain::NeighbourInfo nb{4.0, {0, 0, 1}, true};
    coarse_grain::EnvironmentParams params;

    auto state = coarse_grain::compute_fast_observables(
        n_hat, false, {nb}, params); // Bead has no orientation

    check(std::abs(state.P2) < TOL,
          "H5: P2 = 0 when bead itself has no orientation");
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::printf("\n");
    std::printf("================================================================\n");
    std::printf("  Environment-Responsive Bead State Test Suite\n");
    std::printf("================================================================\n\n");

    std::printf("--- Group A: Environment Parameters ---\n");
    test_default_params();

    std::printf("\n--- Group B: Switching and Weighting Functions ---\n");
    test_switching_function();
    test_density_weight();

    std::printf("\n--- Group C: Fast Observables ---\n");
    test_rho_empty();
    test_rho_single_close();
    test_rho_scales_with_neighbours();
    test_P2_parallel_alignment();
    test_P2_perpendicular();
    test_P2_antiparallel();
    test_P2_random_isotropic();
    test_observables_outside_cutoff();

    std::printf("\n--- Group D: Slow State Integration ---\n");
    test_eta_euler_at_target();
    test_eta_euler_approaches_target();
    test_eta_exact_at_target();
    test_eta_exact_approaches_target();
    test_eta_exact_long_time();
    test_eta_clamp_invariant();
    test_eta_tau_zero();
    test_eta_euler_vs_exact_small_dt();

    std::printf("\n--- Group E: Target Function and Normalisation ---\n");
    test_normalisation_rho_hat();
    test_P2_hat_range();
    test_target_function_weights();

    std::printf("\n--- Group F: Kernel Modulation ---\n");
    test_modulation_zero_eta();
    test_modulation_dispersion_enhancement();
    test_modulation_electrostatic_screening();
    test_modulation_steric_hardening();
    test_modulation_sign_invariant();
    test_modulated_kernel_value();
    test_modulation_report();

    std::printf("\n--- Group G: Full Update Cycle ---\n");
    test_full_update_from_zero();
    test_full_update_convergence();
    test_full_update_no_neighbours();

    std::printf("\n--- Group H: Invariants and Edge Cases ---\n");
    test_eta_bounds_invariant();
    test_zero_distance_neighbour();
    test_no_orientation_P2();
    test_bead_no_orientation_P2();

    std::printf("\n================================================================\n");
    std::printf("  Results: %d passed, %d failed (of %d)\n",
                g_pass, g_fail, g_pass + g_fail);
    std::printf("================================================================\n\n");

    return g_fail > 0 ? 1 : 0;
}
