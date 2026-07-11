/**
 * v5_demo.cpp — V5 Environment-Responsive Bead Transport Demo
 * ═══════════════════════════════════════════════════════════════
 *
 * Three-phase document-compatible demo:
 *
 *   Phase A: Single bead, prescribed ambient fields.
 *            Verifies integration sanity + logging.
 *            rho/C/P2 are degenerate (no neighbours). Honest about this.
 *
 *   Phase B: 10-bead ensemble, local observables active.
 *            First case that actually matches the ERB paper.
 *            Verifies: rho_B, C_B, P_{2,B}, eta_B evolution,
 *            three tau regimes, observable correctness.
 *
 *   Phase C: Kernel modulation comparison.
 *            Same geometry, same seed, gamma_k=0 vs gamma_k≠0.
 *            Demonstrates ordering tendency, effective stiffness,
 *            damping/screening, persistence due to eta.
 *
 * Update order per step (ERB §, invariant chain):
 *   poses → fast observables → eta → modulated kernels → forces
 *
 * Build (WSL / GCC 14):
 *   g++ -std=c++23 -O2 -I. v5/v5_demo.cpp -o v5/v5_demo
 *
 * VSEPR-SIM V5.0  |  2026-04-16
 */

#include "v5/bead_transport.hpp"
#include "tests/scene_builders.hpp"
#include "coarse_grain/core/environment_state.hpp"
#include "coarse_grain/models/environment_coupling.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

// ============================================================================
// Helpers
// ============================================================================

static void print_banner(const char* title) {
	std::printf("\n");
	std::printf("═══════════════════════════════════════════════════════════════\n");
	std::printf("  %s\n", title);
	std::printf("═══════════════════════════════════════════════════════════════\n\n");
}

static void print_bead_summary(int id, const v5::BeadDynState& b) {
	std::printf("  bead %2d  pos=(%7.3f,%7.3f,%7.3f)  v=(%7.4f,%7.4f,%7.4f)\n"
				"           rho=%.4f  C=%.1f  P2=%+.4f  eta=%.4f  f=%.4f\n"
				"           g_ster=%.4f  g_elec=%.4f  g_disp=%.4f\n",
				id,
				b.position.x, b.position.y, b.position.z,
				b.velocity.x, b.velocity.y, b.velocity.z,
				b.env.rho, b.env.C, b.env.P2,
				b.env.eta, b.env.target_f,
				b.mod_report.g_steric, b.mod_report.g_electrostatic,
				b.mod_report.g_dispersion);
}

// ============================================================================
// Phase A: Single Bead — Integration Sanity
// ============================================================================

static void phase_a() {
	print_banner("PHASE A: Single Bead — Integration + Transport Sanity");

	std::printf("  NOTE: Single bead has NO neighbours.\n");
	std::printf("  rho, C, P2 are degenerate (all zero).\n");
	std::printf("  This is a numerical harness, not full physics.\n\n");

	// One bead at (5, 0, 2) with initial velocity (0.1, 0, 0)
	v5::BeadDynState bead;
	bead.position = {5.0, 0.0, 2.0};
	bead.velocity = {0.1, 0.0, 0.0};
	bead.n_hat = {0, 0, 1};
	bead.has_orientation = true;

	coarse_grain::EnvironmentParams env_params;
	env_params.alpha = 0.5;
	env_params.beta  = 0.5;
	env_params.tau   = 100.0;

	v5::TransportParams tp;
	tp.A = 0.01;
	tp.B = 1.0;
	tp.C_grad = 0.5;

	double dt = 5.0;
	int n_steps = 200;

	std::vector<v5::BeadDynState> beads = {bead};
	v5::StepLog logger("v5/phase_a_log.csv");

	std::printf("  dt=%.1f fs  |  %d steps  |  transport A=%.3f B=%.1f C=%.2f\n\n",
				dt, n_steps, tp.A, tp.B, tp.C_grad);

	// Header
	std::printf("  %5s  %9s %9s %9s  %9s %9s  %7s %7s\n",
				"step", "x", "y", "z", "vx", "vy", "eta", "f");
	std::printf("  %s\n", std::string(72, '-').c_str());

	for (int s = 0; s < n_steps; ++s) {
		v5::full_step(beads, env_params, tp, dt, s, &logger);

		// Print every 20 steps
		if (s % 20 == 0 || s == n_steps - 1) {
			const auto& b = beads[0];
			std::printf("  %5d  %9.4f %9.4f %9.4f  %9.5f %9.5f  %7.4f %7.4f\n",
						s, b.position.x, b.position.y, b.position.z,
						b.velocity.x, b.velocity.y,
						b.env.eta, b.env.target_f);
		}
	}

	int violations = v5::check_invariants(beads, env_params, n_steps, true);
	std::printf("\n  Invariant violations: %d\n", violations);

	// Verify: isolated bead → rho=0, C=0, P2=0, eta→0
	const auto& final_b = beads[0];
	bool rho_ok = std::abs(final_b.env.rho) < 1e-10;
	bool C_ok   = std::abs(final_b.env.C) < 1e-10;
	bool P2_ok  = std::abs(final_b.env.P2) < 1e-10;
	bool eta_ok = final_b.env.eta >= 0.0 && final_b.env.eta <= 1.0;

	std::printf("  rho=0 (isolated): %s\n", rho_ok ? "PASS" : "FAIL");
	std::printf("  C=0   (isolated): %s\n", C_ok   ? "PASS" : "FAIL");
	std::printf("  P2=0  (isolated): %s\n", P2_ok  ? "PASS" : "FAIL");
	std::printf("  eta in [0,1]:     %s\n", eta_ok ? "PASS" : "FAIL");

	logger.flush();
	std::printf("\n  CSV → v5/phase_a_log.csv\n");
}

// ============================================================================
// Phase B: 10-Bead Ensemble — Real Local Observables
// ============================================================================

static void phase_b() {
	print_banner("PHASE B: 10-Bead Ensemble — Document-Compatible Observables");

	// Build a linear stack of 10 beads along z-axis, spacing=4.0 Å.
	// Orientations drawn from a 45-degree cone around +z (theta_max = pi/4).
	// This gives a non-degenerate directional distribution:
	//   <P2> ≈ 0.5 * cos(theta_max) * (1 + cos(theta_max)) ≈ 0.427
	constexpr double pi = 3.14159265358979323846;
	constexpr double theta_b = pi / 4.0;   // 45 deg cone half-angle
	auto scene = test_util::scene_linear_stack_spread(10, 4.0, theta_b, 42u);

	// Report initial N-directional distribution
	{
		double sum_cos = 0.0;
		double sum_p2  = 0.0;
		for (const auto& sb : scene) {
			sum_cos += sb.n_hat.z;   // cos(theta) relative to +z
			sum_p2  += 0.5 * (3.0 * sb.n_hat.z * sb.n_hat.z - 1.0);
		}
		int ns = static_cast<int>(scene.size());
		std::printf("  Orientation spread: theta_max=%.0f deg  seed=42\n",
					theta_b * 180.0 / pi);
		std::printf("  Initial avg N directional: avg_cos_theta=%.4f  avg_P2_init=%+.4f\n",
					sum_cos / ns, sum_p2 / ns);
		std::printf("  Expected <P2> (cone formula): %.4f\n\n",
					0.5 * std::cos(theta_b) * (1.0 + std::cos(theta_b)));
	}

	// Convert to v5 dynamic state
	std::vector<v5::BeadDynState> beads(scene.size());
	for (int i = 0; i < static_cast<int>(scene.size()); ++i) {
		beads[i].position = scene[i].position;
		beads[i].n_hat = scene[i].n_hat;
		beads[i].has_orientation = scene[i].has_orientation;
		beads[i].env.eta = 0.0;
	}

	coarse_grain::EnvironmentParams env_params;
	env_params.alpha      = 0.5;
	env_params.beta       = 0.5;
	env_params.tau         = 100.0;
	env_params.gamma_steric = 0.2;
	env_params.gamma_elec  = -0.1;
	env_params.gamma_disp  = 0.5;
	env_params.sigma_rho   = 3.0;
	env_params.r_cutoff    = 8.0;
	env_params.delta_sw    = 1.0;
	env_params.rho_max     = 10.0;

	v5::TransportParams tp;
	tp.A = 0.0;        // NO transport drag — Phase B is a pure observable test
	tp.B = 0.0;        // no body force for Phase B
	tp.C_grad = 0.0;   // no gradient for Phase B

	double dt = 5.0;
	int n_steps = 400;

	v5::StepLog logger("v5/phase_b_log.csv");

	std::printf("  10 beads, linear stack, spacing=4.0 Å, all oriented z\n");
	std::printf("  env: alpha=%.1f beta=%.1f tau=%.0f fs\n",
				env_params.alpha, env_params.beta, env_params.tau);
	std::printf("  dt=%.1f fs  |  %d steps\n\n", dt, n_steps);

	// ── Run three tau regimes ──────────────────────────────────────────
	double tau_values[] = {50.0, 500.0, 5000.0};
	const char* tau_labels[] = {"small (50 fs)", "moderate (500 fs)", "large (5000 fs)"};

	for (int ti = 0; ti < 3; ++ti) {
		env_params.tau = tau_values[ti];
		std::printf("  ── τ = %s ──\n", tau_labels[ti]);

		// Reset state AND positions for each tau regime
		for (int i = 0; i < static_cast<int>(scene.size()); ++i) {
			beads[i].position = scene[i].position;
			beads[i].env.eta = 0.0;
			beads[i].velocity = {0, 0, 0};
			beads[i].acceleration = {0, 0, 0};
		}

		for (int s = 0; s < n_steps; ++s) {
			v5::full_step(beads, env_params, tp, dt, s,
						  ti == 0 ? &logger : nullptr);
		}

		// Summary statistics
		double sum_eta = 0, sum_rho = 0, sum_C = 0, sum_P2 = 0;
		double min_eta = 1e30, max_eta = -1e30;
		for (const auto& b : beads) {
			sum_eta += b.env.eta;
			sum_rho += b.env.rho;
			sum_C   += b.env.C;
			sum_P2  += b.env.P2;
			min_eta = std::min(min_eta, b.env.eta);
			max_eta = std::max(max_eta, b.env.eta);
		}
		int n = static_cast<int>(beads.size());
		std::printf("    avg_eta=%.4f  [%.4f, %.4f]  avg_rho=%.4f  avg_C=%.1f  avg_P2=%+.4f\n",
					sum_eta / n, min_eta, max_eta,
					sum_rho / n, sum_C / n, sum_P2 / n);

		// Invariant check
		int viol = v5::check_invariants(beads, env_params, n_steps);
		std::printf("    invariant violations: %d\n\n", viol);
	}

	// ── Observable correctness verification ────────────────────────────
	env_params.tau = 100.0;
	for (int i = 0; i < static_cast<int>(scene.size()); ++i) {
		beads[i].position = scene[i].position;
		beads[i].env.eta = 0.0;
		beads[i].velocity = {0, 0, 0};
		beads[i].acceleration = {0, 0, 0};
	}
	for (int s = 0; s < 200; ++s)
		v5::full_step(beads, env_params, tp, dt, s, nullptr);

	std::printf("  ── Observable Correctness Checks ──\n");

	// rho: nearby beads increase density
	// Edge bead (0) vs centre bead (5) — centre should have higher rho
	bool rho_centre_higher = beads[5].env.rho > beads[0].env.rho;
	std::printf("    rho: centre > edge:  %s  (%.4f > %.4f)\n",
				rho_centre_higher ? "PASS" : "FAIL",
				beads[5].env.rho, beads[0].env.rho);

	// C: coordination count
	bool C_positive = beads[5].env.C > 0;
	std::printf("    C:   centre > 0:     %s  (C=%.1f)\n",
				C_positive ? "PASS" : "FAIL", beads[5].env.C);

	// P2: all aligned along z → P2 should be high positive
	bool P2_aligned = beads[5].env.P2 > 0.5;
	std::printf("    P2:  aligned > 0.5:  %s  (P2=%+.4f)\n",
				P2_aligned ? "PASS" : "FAIL", beads[5].env.P2);

	// eta: after evolution, should be > 0 in ordered environment
	bool eta_nonzero = beads[5].env.eta > 0.1;
	std::printf("    eta: evolved > 0.1:  %s  (eta=%.4f)\n",
				eta_nonzero ? "PASS" : "FAIL", beads[5].env.eta);

	// Average N directional after evolution
	{
		double sum_p2 = 0.0;
		double sum_cos = 0.0;
		for (const auto& b : beads) {
			sum_p2  += b.env.P2;
			// recover mean cos from each bead's env P2 contribution
			sum_cos += b.n_hat.z;
		}
		int nb = static_cast<int>(beads.size());
		std::printf("\n    avg N directional (post-evolution):\n");
		std::printf("      avg_cos_theta = %.4f\n", sum_cos / nb);
		std::printf("      avg_env_P2    = %+.4f\n", sum_p2 / nb);
	}

	std::printf("\n  Final state (all beads):\n");
	for (int i = 0; i < static_cast<int>(beads.size()); ++i)
		print_bead_summary(i, beads[i]);

	logger.flush();
	std::printf("\n  CSV → v5/phase_b_log.csv\n");
}

// ============================================================================
// Phase C: Kernel Modulation Comparison
// ============================================================================

static void phase_c() {
	print_banner("PHASE C: Kernel Modulation — gamma=0 vs gamma≠0");

	// Perturbed 3D lattice (2^3 = 8 beads, slight disorder).
	// Orientations from a 60-degree cone: theta_max = pi/3.
	// This gives a richer directional landscape than fully random
	// while still preserving a measurable ordering bias:
	//   <P2> ≈ 0.5 * cos(pi/3) * (1 + cos(pi/3)) = 0.375
	constexpr double pi = 3.14159265358979323846;
	constexpr double theta_c = pi / 3.0;   // 60 deg cone half-angle
	auto scene = test_util::scene_perturbed_lattice_spread(2, 4.5, 0.08, theta_c, 42u);

	// Report initial N-directional distribution
	{
		double sum_cos = 0.0, sum_p2 = 0.0;
		for (const auto& sb : scene) {
			sum_cos += sb.n_hat.z;
			sum_p2  += 0.5 * (3.0 * sb.n_hat.z * sb.n_hat.z - 1.0);
		}
		int ns = static_cast<int>(scene.size());
		std::printf("  Orientation spread: theta_max=%.0f deg  seed=42\n",
					theta_c * 180.0 / pi);
		std::printf("  Initial avg N directional: avg_cos_theta=%.4f  avg_P2_init=%+.4f\n",
					sum_cos / ns, sum_p2 / ns);
		std::printf("  Expected <P2> (cone formula): %.4f\n\n",
					0.5 * std::cos(theta_c) * (1.0 + std::cos(theta_c)));
	}

	auto make_beads = [&]() -> std::vector<v5::BeadDynState> {
		std::vector<v5::BeadDynState> beads(scene.size());
		for (int i = 0; i < static_cast<int>(scene.size()); ++i) {
			beads[i].position = scene[i].position;
			beads[i].n_hat = scene[i].n_hat;
			beads[i].has_orientation = scene[i].has_orientation;
			beads[i].env.eta = 0.0;
		}
		return beads;
	};

	coarse_grain::EnvironmentParams env_base;
	env_base.alpha      = 0.5;
	env_base.beta       = 0.5;
	env_base.tau         = 200.0;
	env_base.sigma_rho   = 3.0;
	env_base.r_cutoff    = 8.0;
	env_base.delta_sw    = 1.0;
	env_base.rho_max     = 10.0;

	v5::TransportParams tp;
	tp.A = 0.01;       // Moderate drag to stabilise lattice under LJ forces
	tp.B = 0.0;
	tp.C_grad = 0.0;

	double dt = 5.0;
	int n_steps = 500;

	// ── Run A: gamma_k = 0 (no modulation) ────────────────────────────
	coarse_grain::EnvironmentParams env_off = env_base;
	env_off.gamma_steric = 0.0;
	env_off.gamma_elec   = 0.0;
	env_off.gamma_disp   = 0.0;

	auto beads_off = make_beads();
	v5::StepLog log_off("v5/phase_c_gamma_off.csv");

	for (int s = 0; s < n_steps; ++s)
		v5::full_step(beads_off, env_off, tp, dt, s, &log_off);
	log_off.flush();

	// ── Run B: gamma_k ≠ 0 (full modulation) ─────────────────────────
	coarse_grain::EnvironmentParams env_on = env_base;
	env_on.gamma_steric = 0.3;
	env_on.gamma_elec   = -0.2;
	env_on.gamma_disp   = 0.8;

	auto beads_on = make_beads();
	v5::StepLog log_on("v5/phase_c_gamma_on.csv");

	for (int s = 0; s < n_steps; ++s)
		v5::full_step(beads_on, env_on, tp, dt, s, &log_on);
	log_on.flush();

	// ── Comparison ────────────────────────────────────────────────────
	int n = static_cast<int>(beads_off.size());

	auto compute_stats = [](const std::vector<v5::BeadDynState>& bs) {
		double sum_eta = 0, sum_rho = 0, sum_P2 = 0;
		double sum_gs = 0, sum_ge = 0, sum_gd = 0;
		for (const auto& b : bs) {
			sum_eta += b.env.eta;
			sum_rho += b.env.rho;
			sum_P2  += b.env.P2;
			sum_gs  += b.mod_report.g_steric;
			sum_ge  += b.mod_report.g_electrostatic;
			sum_gd  += b.mod_report.g_dispersion;
		}
		int n = static_cast<int>(bs.size());
		return std::array<double, 6>{
			sum_eta/n, sum_rho/n, sum_P2/n, sum_gs/n, sum_ge/n, sum_gd/n
		};
	};

	auto stats_off = compute_stats(beads_off);
	auto stats_on  = compute_stats(beads_on);

	std::printf("  %d beads  |  %d steps  |  dt=%.1f fs  |  tau=%.0f fs\n\n", n, n_steps, dt, env_base.tau);

	std::printf("  %-20s  %10s  %10s\n", "Observable", "gamma=0", "gamma≠0");
	std::printf("  %s\n", std::string(44, '-').c_str());
	const char* labels[] = {"avg_eta", "avg_rho", "avg_P2", "g_steric", "g_elec", "g_disp"};
	for (int k = 0; k < 6; ++k) {
		std::printf("  %-20s  %10.4f  %10.4f\n", labels[k], stats_off[k], stats_on[k]);
	}

	// Kernel modulation should be visible
	std::printf("\n  ── Modulation Verification ──\n");

	// With gamma=0, all g factors should be 1.0
	bool g_off_ok = std::abs(stats_off[3] - 1.0) < 0.01
				 && std::abs(stats_off[4] - 1.0) < 0.01
				 && std::abs(stats_off[5] - 1.0) < 0.01;
	std::printf("    gamma=0 → all g=1.0:  %s\n", g_off_ok ? "PASS" : "FAIL");

	// With gamma≠0, g factors should differ from 1.0
	bool g_on_diff = std::abs(stats_on[3] - 1.0) > 0.001
				  || std::abs(stats_on[4] - 1.0) > 0.001
				  || std::abs(stats_on[5] - 1.0) > 0.001;
	std::printf("    gamma≠0 → g≠1.0:     %s\n", g_on_diff ? "PASS" : "FAIL");

	// g_steric should be > 1 (hardening)
	bool gs_up = stats_on[3] > 1.0;
	std::printf("    g_steric > 1 (hardening):    %s  (%.4f)\n",
				gs_up ? "PASS" : "FAIL", stats_on[3]);

	// g_elec should be < 1 (screening)
	bool ge_down = stats_on[4] < 1.0;
	std::printf("    g_elec < 1 (screening):      %s  (%.4f)\n",
				ge_down ? "PASS" : "FAIL", stats_on[4]);

	// g_disp should be > 1 (enhancement)
	bool gd_up = stats_on[5] > 1.0;
	std::printf("    g_disp > 1 (enhancement):    %s  (%.4f)\n",
				gd_up ? "PASS" : "FAIL", stats_on[5]);

	// Invariants both runs
	int viol_off = v5::check_invariants(beads_off, env_off, n_steps);
	int viol_on  = v5::check_invariants(beads_on,  env_on,  n_steps);
	std::printf("\n    invariant violations (gamma=0): %d\n", viol_off);
	std::printf("    invariant violations (gamma≠0): %d\n", viol_on);

	std::printf("\n  CSV → v5/phase_c_gamma_off.csv, v5/phase_c_gamma_on.csv\n");
}

// ============================================================================
// Main
// ============================================================================

int main() {
	std::printf("\n");
	std::printf("╔═══════════════════════════════════════════════════════════════╗\n");
	std::printf("║  VSEPR-SIM V5.0 — Environment-Responsive Bead Transport     ║\n");
	std::printf("║  Document-Compatible Implementation Demo                     ║\n");
	std::printf("║  ERB Update Order: poses → observables → η → kernels → F    ║\n");
	std::printf("║  2026-04-16                                                  ║\n");
	std::printf("╚═══════════════════════════════════════════════════════════════╝\n");

	phase_a();
	phase_b();
	phase_c();

	std::printf("\n");
	std::printf("═══════════════════════════════════════════════════════════════\n");
	std::printf("  V5 DEMO COMPLETE\n");
	std::printf("  All CSV logs written to v5/\n");
	std::printf("═══════════════════════════════════════════════════════════════\n\n");

	return 0;
}
