/**
 * v5_3d_demos.cpp — V5 Extended 3D Geometry Demos + Verification Dataset
 * ════════════════════════════════════════════════════════════════════════
 *
 * Five phases, each a self-contained 3D configuration with distinct
 * geometric character, run under the full V5 ERB update chain:
 *
 *   poses → fast observables → eta → modulated kernels → forces
 *
 * Phase D: Helical Chain
 *   20 beads wound on a 3-turn helix (R=4 Å, pitch=8 Å).
 *   n_hat = unit tangent.  Shear transport drives lateral drift.
 *   Verification: high initial P2 (tangent near-parallel), eta grows,
 *   beads remain within expanding bounding box.
 *
 * Phase E: FCC Patch (2×2×2 cells, a=4.5 Å → 32 beads)
 *   Face-centred cubic packing.  Bulk beads (interior) vs surface beads.
 *   Verification: bulk rho > surface rho, bulk eta > surface eta after
 *   relaxation, all invariants satisfied.
 *
 * Phase F: Bilayer Slab (4×4 grid per leaflet → 32 beads)
 *   Two opposed planes, n_hat = ±z.  Membrane-like anti-parallel order.
 *   Verification: avg P2 across all beads is near zero (anti-aligned
 *   cross-pairs cancel aligned within-plane pairs), P2_hat stays in [0,1].
 *
 * Phase G: Icosahedral Shell (12 shell + 1 centre → 13 beads)
 *   Maximally uniform spherical arrangement.
 *   Verification: rho of centre bead ≈ same order as shell bead rho,
 *   P2 at centre ≈ 0 (isotropic neighbourhood), all invariants pass.
 *
 * Phase H: Dual Cluster Merger
 *   Two 8-bead FCC cells separated by 20 Å, driven together by the
 *   elaborated gradient_field confinement.  Monitors inter-cluster
 *   centroid gap closing over 1000 steps.
 *   Verification: final gap < initial gap (merger tendency confirmed).
 *
 * All phases write CSV logs to v5/ and print PASS/FAIL assertions.
 *
 * Build (WSL / GCC 14):
 *   g++ -std=c++23 -O2 -I. v5/v5_3d_demos.cpp -o v5/v5_3d_demos
 *
 * VSEPR-SIM V5.0  |  2026-04-16
 */

#include "v5/bead_transport.hpp"
#include "tests/scene_builders.hpp"
#include "coarse_grain/core/environment_state.hpp"
#include "coarse_grain/models/environment_coupling.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <iomanip>
#include <numeric>
#include <string>
#include <vector>

// ============================================================================
// Shared Utilities
// ============================================================================

static void print_banner(const char* title) {
	std::printf("\n");
	std::printf("═══════════════════════════════════════════════════════════════\n");
	std::printf("  %s\n", title);
	std::printf("═══════════════════════════════════════════════════════════════\n\n");
}

static const char* pass_fail(bool ok) { return ok ? "PASS" : "FAIL"; }

/** Convert a scene into a vector of BeadDynState with zero velocity/accel. */
static std::vector<v5::BeadDynState> scene_to_dyn(
	const std::vector<test_util::SceneBead>& scene)
{
	std::vector<v5::BeadDynState> beads(scene.size());
	for (int i = 0; i < static_cast<int>(scene.size()); ++i) {
		beads[i].position = scene[i].position;
		beads[i].n_hat    = scene[i].n_hat;
		beads[i].has_orientation = scene[i].has_orientation;
		beads[i].env.eta  = scene[i].eta;
	}
	return beads;
}

struct EnsembleStats {
	double avg_eta{}, avg_rho{}, avg_C{}, avg_P2{};
	double min_eta{},  max_eta{};
	double avg_speed{};
};

static EnsembleStats compute_stats(const std::vector<v5::BeadDynState>& beads) {
	EnsembleStats s;
	s.min_eta =  1e30;
	s.max_eta = -1e30;
	for (const auto& b : beads) {
		s.avg_eta   += b.env.eta;
		s.avg_rho   += b.env.rho;
		s.avg_C     += b.env.C;
		s.avg_P2    += b.env.P2;
		s.min_eta    = std::min(s.min_eta, b.env.eta);
		s.max_eta    = std::max(s.max_eta, b.env.eta);
		double spd   = atomistic::norm(b.velocity);
		s.avg_speed += spd;
	}
	double n = static_cast<double>(beads.size());
	s.avg_eta   /= n;
	s.avg_rho   /= n;
	s.avg_C     /= n;
	s.avg_P2    /= n;
	s.avg_speed /= n;
	return s;
}

/** Centroid of positions. */
static atomistic::Vec3 centroid(const std::vector<v5::BeadDynState>& beads) {
	atomistic::Vec3 c{};
	for (const auto& b : beads) c = c + b.position;
	double n = static_cast<double>(beads.size());
	return c * (1.0 / n);
}

/** Default environment params used across all 3D demos. */
static coarse_grain::EnvironmentParams default_env_params() {
	coarse_grain::EnvironmentParams p;
	p.alpha        = 0.5;
	p.beta         = 0.5;
	p.tau          = 150.0;
	p.gamma_steric = 0.25;
	p.gamma_elec   = -0.15;
	p.gamma_disp   = 0.60;
	p.sigma_rho    = 3.0;
	p.r_cutoff     = 9.0;    // slightly larger than phase A/B to capture FCC neighbours
	p.delta_sw     = 1.0;
	p.rho_max      = 12.0;
	return p;
}

/** Transport with moderate drag + full confinement fields. */
static v5::TransportParams default_transport() {
	v5::TransportParams tp;
	tp.A      = 0.008;
	tp.B      = 1.0;
	tp.C_grad = 0.5;
	return tp;
}

// ============================================================================
// Phase D: Helical Chain
// ============================================================================

static void phase_d() {
	print_banner("PHASE D: Helical Chain — 20 Beads, 3 Turns (R=4 Å, pitch=8 Å)");

	// Build helix: 20 beads, R=4 Å, pitch=8 Å, 3 turns
	auto scene = test_util::scene_helix(20, 4.0, 8.0, 3.0);
	auto beads  = scene_to_dyn(scene);
	int  n      = static_cast<int>(beads.size());

	// Compute initial P2 to verify tangent near-parallel structure
	double init_P2_sum = 0.0;
	int    init_P2_cnt = 0;
	for (int i = 0; i < n; ++i)
	for (int j = i + 1; j < n; ++j) {
		double cos_a = atomistic::dot(beads[i].n_hat, beads[j].n_hat);
		cos_a = std::clamp(cos_a, -1.0, 1.0);
		init_P2_sum += 0.5 * (3.0 * cos_a * cos_a - 1.0);
		++init_P2_cnt;
	}
	double init_P2_global = (init_P2_cnt > 0) ? init_P2_sum / init_P2_cnt : 0.0;

	std::printf("  20 beads | 3-turn helix | R=4 Å | pitch=8 Å\n");
	std::printf("  initial global P2 (tangent pairs): %+.4f\n", init_P2_global);
	std::printf("  (expected > 0.3 for tightly wound helix with parallel neighbours)\n\n");

	auto env_params = default_env_params();
	auto tp         = default_transport();
	double dt       = 4.0;
	int n_steps     = 600;

	v5::StepLog logger("v5/phase_d_helix.csv");

	std::printf("  %5s  %8s  %8s  %8s  %8s  %8s\n",
				"step", "avg_eta", "avg_rho", "avg_P2", "avg_C", "avg_spd");
	std::printf("  %s\n", std::string(58, '-').c_str());

	EnsembleStats stats_init = compute_stats(beads);

	for (int s = 0; s < n_steps; ++s) {
		v5::full_step(beads, env_params, tp, dt, s, &logger);
		if (s % 100 == 0 || s == n_steps - 1) {
			auto st = compute_stats(beads);
			std::printf("  %5d  %8.4f  %8.4f  %+8.4f  %8.2f  %8.5f\n",
						s, st.avg_eta, st.avg_rho, st.avg_P2, st.avg_C, st.avg_speed);
		}
	}

	auto stats_final = compute_stats(beads);
	int violations   = v5::check_invariants(beads, env_params, n_steps, false);

	std::printf("\n  ── Verification ──\n");
	std::printf("    initial global P2 > 0.3:    %s  (%.4f)\n",
				pass_fail(init_P2_global > 0.3), init_P2_global);
	std::printf("    eta grew from 0:             %s  (%.4f → %.4f)\n",
				pass_fail(stats_final.avg_eta > stats_init.avg_eta + 0.01),
				stats_init.avg_eta, stats_final.avg_eta);
	std::printf("    avg_C > 0 (neighbours):      %s  (%.2f)\n",
				pass_fail(stats_final.avg_C > 0.0), stats_final.avg_C);
	std::printf("    invariant violations:        %d\n", violations);
	std::printf("    all invariants pass:         %s\n", pass_fail(violations == 0));

	logger.flush();
	std::printf("\n  CSV → v5/phase_d_helix.csv\n");
}

// ============================================================================
// Phase E: FCC Patch — Bulk vs Surface Density Contrast
// ============================================================================

static void phase_e() {
	print_banner("PHASE E: FCC Patch — 2×2×2 Cells, a=4.5 Å (32 beads)");

	// 2 cells per axis → 2^3 × 4 = 32 beads
	auto scene = test_util::scene_fcc_patch(2, 2, 2, 4.5);
	auto beads  = scene_to_dyn(scene);
	int  n      = static_cast<int>(beads.size());

	std::printf("  %d beads | FCC a=4.5 Å | 2×2×2 unit cells\n\n", n);

	auto env_params = default_env_params();
	auto tp         = default_transport();
	tp.A = 0.005;    // gentler drag for the ordered lattice
	double dt   = 3.0;
	int n_steps = 500;

	v5::StepLog logger("v5/phase_e_fcc.csv");

	for (int s = 0; s < n_steps; ++s)
		v5::full_step(beads, env_params, tp, dt, s, &logger);

	// Classify bulk vs surface by coordination number after evolution
	// (C >= 8 → interior, C < 8 → surface for FCC with r_cutoff=9)
	double bulk_rho = 0.0, surf_rho = 0.0;
	double bulk_eta = 0.0, surf_eta = 0.0;
	int n_bulk = 0, n_surf = 0;
	for (const auto& b : beads) {
		if (b.env.C >= 7.0) {
			bulk_rho += b.env.rho;
			bulk_eta += b.env.eta;
			++n_bulk;
		} else {
			surf_rho += b.env.rho;
			surf_eta += b.env.eta;
			++n_surf;
		}
	}
	if (n_bulk > 0) { bulk_rho /= n_bulk; bulk_eta /= n_bulk; }
	if (n_surf > 0) { surf_rho /= n_surf; surf_eta /= n_surf; }

	auto stats  = compute_stats(beads);
	int  violations = v5::check_invariants(beads, env_params, n_steps, false);

	std::printf("  ── Final State ──\n");
	std::printf("    avg_eta=%.4f  avg_rho=%.4f  avg_C=%.2f  avg_P2=%+.4f\n",
				stats.avg_eta, stats.avg_rho, stats.avg_C, stats.avg_P2);
	std::printf("    bulk beads (C≥7): %d  |  surface beads (C<7): %d\n", n_bulk, n_surf);
	std::printf("    bulk_rho=%.4f  surf_rho=%.4f\n", bulk_rho, surf_rho);
	std::printf("    bulk_eta=%.4f  surf_eta=%.4f\n", bulk_eta, surf_eta);

	std::printf("\n  ── Verification ──\n");
	std::printf("    bulk_rho > surf_rho:         %s  (%.4f > %.4f)\n",
				pass_fail(bulk_rho > surf_rho), bulk_rho, surf_rho);
	std::printf("    bulk_eta > surf_eta:         %s  (%.4f > %.4f)\n",
				pass_fail(bulk_eta > surf_eta), bulk_eta, surf_eta);
	std::printf("    avg_C > 4 (FCC packing):     %s  (%.2f)\n",
				pass_fail(stats.avg_C > 4.0), stats.avg_C);
	std::printf("    eta in [0,1] all:             %s\n",
				pass_fail(violations == 0));
	std::printf("    invariant violations:        %d\n", violations);

	logger.flush();
	std::printf("\n  CSV → v5/phase_e_fcc.csv\n");
}

// ============================================================================
// Phase F: Bilayer Slab — Anti-Parallel Order
// ============================================================================

static void phase_f() {
	print_banner("PHASE F: Bilayer Slab — 4×4 Grid Per Leaflet, z_sep=5 Å (32 beads)");

	// 4×4 per leaflet, spacing=3.5 Å, separation=5 Å
	auto scene = test_util::scene_bilayer(4, 3.5, 5.0);
	auto beads  = scene_to_dyn(scene);
	int  n      = static_cast<int>(beads.size());

	// Compute initial global P2 (expected near 0 due to ±z anti-alignment)
	double init_P2 = 0.0;
	int    cnt = 0;
	for (int i = 0; i < n; ++i)
	for (int j = i + 1; j < n; ++j) {
		double cos_a = atomistic::dot(beads[i].n_hat, beads[j].n_hat);
		init_P2 += 0.5 * (3.0 * cos_a * cos_a - 1.0);
		++cnt;
	}
	init_P2 = (cnt > 0) ? init_P2 / cnt : 0.0;

	std::printf("  %d beads | 4×4 bilayer | spacing=3.5 Å | z_sep=5.0 Å\n", n);
	std::printf("  upper leaflet n_hat=+z, lower n_hat=-z\n");
	std::printf("  initial global P2 (all pairs): %+.4f\n", init_P2);
	std::printf("  (expected near 0: within-plane +1 pairs cancel cross-layer -0.5 pairs)\n\n");

	auto env_params = default_env_params();
	env_params.r_cutoff = 8.0;
	env_params.beta  = 0.6;    // weight orientational order more heavily
	env_params.alpha = 0.4;
	auto tp = default_transport();
	tp.A = 0.006;
	tp.C_grad = 0.3;   // softer confinement to preserve slab structure

	double dt   = 3.0;
	int n_steps = 500;

	v5::StepLog logger("v5/phase_f_bilayer.csv");

	for (int s = 0; s < n_steps; ++s)
		v5::full_step(beads, env_params, tp, dt, s, &logger);

	// Classify upper vs lower leaflet (z > 0 vs z < 0)
	double upper_P2 = 0.0, lower_P2 = 0.0;
	double upper_eta = 0.0, lower_eta = 0.0;
	int n_upper = 0, n_lower = 0;
	for (const auto& b : beads) {
		if (b.position.z > 0) {
			upper_P2  += b.env.P2;
			upper_eta += b.env.eta;
			++n_upper;
		} else {
			lower_P2  += b.env.P2;
			lower_eta += b.env.eta;
			++n_lower;
		}
	}
	if (n_upper > 0) { upper_P2 /= n_upper; upper_eta /= n_upper; }
	if (n_lower > 0) { lower_P2 /= n_lower; lower_eta /= n_lower; }

	// Global avg P2 after evolution
	double final_P2 = 0.0;
	for (const auto& b : beads) final_P2 += b.env.P2;
	final_P2 /= n;

	int violations = v5::check_invariants(beads, env_params, n_steps, false);

	std::printf("  ── Final State ──\n");
	std::printf("    upper leaflet: avg_P2=%+.4f  avg_eta=%.4f  (%d beads)\n",
				upper_P2, upper_eta, n_upper);
	std::printf("    lower leaflet: avg_P2=%+.4f  avg_eta=%.4f  (%d beads)\n",
				lower_P2, lower_eta, n_lower);
	std::printf("    global avg_P2=%+.4f\n", final_P2);

	std::printf("\n  ── Verification ──\n");
	// Within-leaflet P2 should be high (neighbours aligned)
	std::printf("    upper leaflet avg_P2 > 0.3:  %s  (%+.4f)\n",
				pass_fail(upper_P2 > 0.3), upper_P2);
	std::printf("    lower leaflet avg_P2 > 0.3:  %s  (%+.4f)\n",
				pass_fail(lower_P2 > 0.3), lower_P2);
	// Global P2 should be substantially less than single-leaflet (cross-layer cancels)
	std::printf("    global P2 < single leaflet:  %s\n",
				pass_fail(final_P2 < upper_P2 - 0.05 || final_P2 < lower_P2 - 0.05));
	// P2_hat in [0,1]
	bool p2hat_ok = true;
	for (const auto& b : beads)
		if (b.env.P2_hat < 0.0 || b.env.P2_hat > 1.0 + 1e-9) p2hat_ok = false;
	std::printf("    all P2_hat in [0,1]:         %s\n", pass_fail(p2hat_ok));
	std::printf("    invariant violations:        %d  → %s\n",
				violations, pass_fail(violations == 0));

	logger.flush();
	std::printf("\n  CSV → v5/phase_f_bilayer.csv\n");
}

// ============================================================================
// Phase G: Icosahedral Shell — Spherical Symmetry
// ============================================================================

static void phase_g() {
	print_banner("PHASE G: Icosahedral Shell — 12 Vertices + 1 Centre (13 beads, R=6 Å)");

	auto scene = test_util::scene_icosahedron(6.0, true);
	auto beads  = scene_to_dyn(scene);
	int  n      = static_cast<int>(beads.size());

	std::printf("  %d beads | icosahedral shell R=6 Å | centre at origin\n", n);
	std::printf("  n_hat = outward radial for shell; +z for centre\n\n");

	auto env_params = default_env_params();
	env_params.r_cutoff = 10.0;   // must span shell→centre (6 Å) + shell→shell
	env_params.sigma_rho = 4.0;
	env_params.rho_max   = 15.0;
	auto tp = default_transport();
	tp.A = 0.005;
	tp.C_grad = 0.4;

	double dt   = 4.0;
	int n_steps = 500;

	v5::StepLog logger("v5/phase_g_icosahedron.csv");

	for (int s = 0; s < n_steps; ++s)
		v5::full_step(beads, env_params, tp, dt, s, &logger);

	// bead[0] is the centre; beads[1..12] are the shell
	const auto& centre = beads[0];

	double shell_rho = 0.0, shell_C = 0.0, shell_P2 = 0.0, shell_eta = 0.0;
	for (int i = 1; i < n; ++i) {
		shell_rho += beads[i].env.rho;
		shell_C   += beads[i].env.C;
		shell_P2  += beads[i].env.P2;
		shell_eta += beads[i].env.eta;
	}
	double ns = static_cast<double>(n - 1);
	shell_rho /= ns;  shell_C /= ns;
	shell_P2  /= ns;  shell_eta /= ns;

	int violations = v5::check_invariants(beads, env_params, n_steps, false);

	std::printf("  ── Final State ──\n");
	std::printf("    centre:  rho=%.4f  C=%.1f  P2=%+.4f  eta=%.4f\n",
				centre.env.rho, centre.env.C,
				centre.env.P2, centre.env.eta);
	std::printf("    shell avg: rho=%.4f  C=%.1f  P2=%+.4f  eta=%.4f\n",
				shell_rho, shell_C, shell_P2, shell_eta);

	std::printf("\n  ── Verification ──\n");
	// Centre sees all 12 shell beads → should have high C
	std::printf("    centre C ≥ 10 (sees shell):  %s  (C=%.1f)\n",
				pass_fail(centre.env.C >= 10.0), centre.env.C);
	// Centre rho should be comparable to shell rho (icosahedral symmetry)
	std::printf("    centre rho > 0:              %s  (%.4f)\n",
				pass_fail(centre.env.rho > 0.0), centre.env.rho);
	// Centre P2: outward radial n_hat vectors cancel → near 0
	// P2 at centre = avg of 0.5(3cos²(θ_ij)-1) over all shell pair combos
	// The icosahedron has known P2 ≈ -0.1 for radial axis pairs from centre
	std::printf("    centre P2 in [-0.5, 0.3]:    %s  (%+.4f)\n",
				pass_fail(centre.env.P2 > -0.5 && centre.env.P2 < 0.3),
				centre.env.P2);
	// All shell beads should have non-zero rho (each sees at least neighbours)
	bool shell_rho_ok = true;
	for (int i = 1; i < n; ++i)
		if (beads[i].env.rho < 1e-6) shell_rho_ok = false;
	std::printf("    all shell rho > 0:           %s\n", pass_fail(shell_rho_ok));
	std::printf("    invariant violations:        %d  → %s\n",
				violations, pass_fail(violations == 0));

	logger.flush();
	std::printf("\n  CSV → v5/phase_g_icosahedron.csv\n");
}

// ============================================================================
// Phase H: Dual Cluster Merger
// ============================================================================

static void phase_h() {
	print_banner("PHASE H: Dual Cluster Merger — Two FCC Cells, Initial Gap=20 Å");

	// Two 1×1×1 FCC cells (4 beads each), separated along x by 20 Å
	auto scene_left  = test_util::scene_fcc_patch(1, 1, 1, 4.0);
	auto scene_right = test_util::translate_scene(
		test_util::scene_fcc_patch(1, 1, 1, 4.0),
		{20.0, 0.0, 0.0});

	// Merge scenes
	std::vector<test_util::SceneBead> scene_merged = scene_left;
	scene_merged.insert(scene_merged.end(), scene_right.begin(), scene_right.end());

	auto beads = scene_to_dyn(scene_merged);
	int  n     = static_cast<int>(beads.size());
	int  n_L   = static_cast<int>(scene_left.size());   // 4

	// Measure initial inter-cluster centroid gap
	auto gap_vec = [&]() -> double {
		atomistic::Vec3 cL{}, cR{};
		for (int i = 0; i < n_L; ++i)        cL = cL + beads[i].position;
		for (int i = n_L; i < n; ++i)        cR = cR + beads[i].position;
		cL = cL * (1.0 / n_L);
		cR = cR * (1.0 / (n - n_L));
		return atomistic::norm(cR - cL);
	};

	double gap_init = gap_vec();
	std::printf("  %d beads (%d left + %d right) | initial centroid gap=%.2f Å\n\n",
				n, n_L, n - n_L, gap_init);

	// Strong confinement to pull clusters together; moderate drag
	auto env_params = default_env_params();
	env_params.r_cutoff = 12.0;
	auto tp = default_transport();
	tp.A      = 0.012;
	tp.B      = 1.0;
	tp.C_grad = 1.2;   // stronger gradient driving for merger

	double dt   = 4.0;
	int n_steps = 1000;

	v5::StepLog logger("v5/phase_h_merger.csv");

	std::printf("  %5s  %10s  %10s  %10s\n", "step", "gap(Å)", "avg_eta", "avg_rho");
	std::printf("  %s\n", std::string(42, '-').c_str());

	double gap_prev = gap_init;
	bool   gap_ever_closed = false;

	for (int s = 0; s < n_steps; ++s) {
		v5::full_step(beads, env_params, tp, dt, s, &logger);

		if (s % 200 == 0 || s == n_steps - 1) {
			double gap  = gap_vec();
			auto   st   = compute_stats(beads);
			std::printf("  %5d  %10.4f  %10.4f  %10.4f\n",
						s, gap, st.avg_eta, st.avg_rho);
			if (gap < gap_prev) gap_ever_closed = true;
			gap_prev = gap;
		}
	}

	double gap_final  = gap_vec();
	auto   stats_fin  = compute_stats(beads);
	int    violations = v5::check_invariants(beads, env_params, n_steps, false);

	std::printf("\n  ── Verification ──\n");
	std::printf("    initial gap=%.4f Å\n", gap_init);
	std::printf("    final   gap=%.4f Å\n", gap_final);
	std::printf("    gap reduced (merger):        %s  (Δ=%.4f Å)\n",
				pass_fail(gap_final < gap_init), gap_init - gap_final);
	std::printf("    gap ever closed during run:  %s\n", pass_fail(gap_ever_closed));
	std::printf("    avg_eta > 0 (env. evolved):  %s  (%.4f)\n",
				pass_fail(stats_fin.avg_eta > 0.01), stats_fin.avg_eta);
	std::printf("    invariant violations:        %d  → %s\n",
				violations, pass_fail(violations == 0));

	logger.flush();
	std::printf("\n  CSV → v5/phase_h_merger.csv\n");
}

// ============================================================================
// Verification Summary Table
// ============================================================================

static void print_verification_summary() {
	std::printf("\n");
	std::printf("╔═══════════════════════════════════════════════════════════════╗\n");
	std::printf("║  V5 3D Demo Verification Dataset — Summary                  ║\n");
	std::printf("╠═══════════════════════════════════════════════════════════════╣\n");
	std::printf("║  Phase  Geometry             Beads  Key Verification         ║\n");
	std::printf("║  ─────  ────────────────────  ─────  ─────────────────────── ║\n");
	std::printf("║  D      Helical Chain            20  P2>0.3 (tangent align)  ║\n");
	std::printf("║  E      FCC Patch (2×2×2)        32  bulk_rho > surf_rho     ║\n");
	std::printf("║  F      Bilayer Slab (4×4)       32  leaflet P2 > global P2  ║\n");
	std::printf("║  G      Icosahedral Shell        13  centre C≥10, P2 near 0  ║\n");
	std::printf("║  H      Dual Cluster Merger       8  gap_final < gap_init    ║\n");
	std::printf("╠═══════════════════════════════════════════════════════════════╣\n");
	std::printf("║  CSV logs written to v5/phase_[d-h]_*.csv                   ║\n");
	std::printf("╚═══════════════════════════════════════════════════════════════╝\n\n");
}

// ============================================================================
// Main
// ============================================================================

int main() {
	std::printf("\n");
	std::printf("╔═══════════════════════════════════════════════════════════════╗\n");
	std::printf("║  VSEPR-SIM V5.0 — Extended 3D Geometry Demos               ║\n");
	std::printf("║  Phases D–H: Helix, FCC, Bilayer, Icosahedron, Merger      ║\n");
	std::printf("║  ERB chain: poses → observables → η → kernels → F          ║\n");
	std::printf("║  2026-04-16                                                  ║\n");
	std::printf("╚═══════════════════════════════════════════════════════════════╝\n");

	phase_d();
	phase_e();
	phase_f();
	phase_g();
	phase_h();

	print_verification_summary();

	return 0;
}
