/**
 * test_cg_cli.cpp — CLI Integration Tests for the CG Console
 *
 * Validates the universal interpretation layer (CGSystemState)
 * and all five CG CLI operations:
 *
 *   1. Scene construction (all presets)
 *   2. State inspection (positions, env, descriptors)
 *   3. Environment update pipeline (eta relaxation)
 *   4. Interaction evaluation (pairwise energy decomposition)
 *   5. Viz stub (placeholder acknowledgement)
 *
 * These tests exercise the CGSystemState API directly, verifying
 * that the interpretation layer correctly connects CLI to kernel.
 *
 * Architecture coverage:
 *   CLI → command layer → [CGSystemState] → kernel/environment engine
 *                          ^^^^^^^^^^^^^ tested here
 *
 * Reference: Layer B1 (System Services), Layer C1 (CLI Frontend)
 */

#include "cli/system_state.hpp"
#include "coarse_grain/core/environment_state.hpp"
#include "coarse_grain/core/unified_descriptor.hpp"
#include "coarse_grain/models/interaction_engine.hpp"
#include "coarse_grain/models/environment_coupling.hpp"
#include "atomistic/core/state.hpp"
#include "tests/test_viz.hpp"
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

// ============================================================================
// Minimal test harness
// ============================================================================

static int g_pass = 0, g_fail = 0;

#define CHECK(cond, msg) do { \
    if (cond) { ++g_pass; std::printf("  [PASS] %s\n", msg); } \
    else      { ++g_fail; std::printf("  [FAIL] %s\n", msg); } \
} while (0)

#define CHECK_NEAR(a, b, tol, msg) do { \
    if (std::abs((a) - (b)) < (tol)) { ++g_pass; std::printf("  [PASS] %s\n", msg); } \
    else { ++g_fail; std::printf("  [FAIL] %s (got %g, expected %g)\n", msg, \
           static_cast<double>(a), static_cast<double>(b)); } \
} while (0)

#define SECTION(name) std::printf("\n--- %s ---\n", name)

// ============================================================================
// 1. Scene Construction Tests
// ============================================================================

void test_scene_presets() {
    SECTION("Scene Construction — All Presets");
    using namespace vsepr::cli;

    // Isolated
    {
        CGSystemState s;
        s.build_preset(ScenePreset::Isolated, 1, 4.0, 42);
        CHECK(s.num_beads() == 1, "Isolated: 1 bead");
        CHECK(s.scene_name.find("isolated") != std::string::npos, "Isolated: name");
        CHECK(!s.is_empty(), "Isolated: not empty");
        VIZ_CHECKPOINT("Isolated bead", s);
    }

    // Pair
    {
        CGSystemState s;
        s.build_preset(ScenePreset::Pair, 2, 5.0, 42);
        CHECK(s.num_beads() == 2, "Pair: 2 beads");
        double dx = s.beads[1].position.x - s.beads[0].position.x;
        CHECK_NEAR(dx, 5.0, 1e-10, "Pair: separation = 5.0 A");
        VIZ_CHECKPOINT("Pair (5.0 A)", s);
    }

    // Linear Stack
    {
        CGSystemState s;
        s.build_preset(ScenePreset::LinearStack, 6, 3.5, 42);
        CHECK(s.num_beads() == 6, "Stack: 6 beads");
        double z4 = s.beads[4].position.z;
        CHECK_NEAR(z4, 14.0, 1e-10, "Stack: bead 4 at z=14.0 A");
        // All beads should be along z-axis (x=0, y=0)
        bool on_axis = true;
        for (int i = 0; i < s.num_beads(); ++i) {
            if (std::abs(s.beads[i].position.x) > 1e-12 ||
                std::abs(s.beads[i].position.y) > 1e-12) {
                on_axis = false;
            }
        }
        CHECK(on_axis, "Stack: all beads on z-axis");
        VIZ_CHECKPOINT("Linear Stack (6 beads, 3.5 A)", s);
    }

    // T-Shape
    {
        CGSystemState s;
        s.build_preset(ScenePreset::TShape, 3, 4.0, 42);
        CHECK(s.num_beads() == 3, "TShape: 3 beads");
        // Third bead oriented along x
        CHECK_NEAR(s.orientations[2].x, 1.0, 1e-10, "TShape: bead 2 n_hat.x = 1");
        VIZ_CHECKPOINT("T-Shape (4.0 A)", s);
    }

    // Square
    {
        CGSystemState s;
        s.build_preset(ScenePreset::Square, 4, 6.0, 42);
        CHECK(s.num_beads() == 4, "Square: 4 beads");
        // All in xy-plane (z=0)
        bool in_plane = true;
        for (int i = 0; i < s.num_beads(); ++i) {
            if (std::abs(s.beads[i].position.z) > 1e-12) in_plane = false;
        }
        CHECK(in_plane, "Square: all beads in xy-plane");
        VIZ_CHECKPOINT("Square (6.0 A)", s);
    }

    // Dense Shell
    {
        CGSystemState s;
        s.build_preset(ScenePreset::DenseShell, 10, 5.0, 42);
        CHECK(s.num_beads() == 11, "Shell: 1 central + 10 = 11 beads");
        // Central bead at origin
        CHECK_NEAR(s.beads[0].position.x, 0.0, 1e-12, "Shell: central at origin");
        // All shell beads at distance 5.0 from origin
        bool on_shell = true;
        for (int i = 1; i < s.num_beads(); ++i) {
            double r = atomistic::norm(s.beads[i].position);
            if (std::abs(r - 5.0) > 1e-8) on_shell = false;
        }
        CHECK(on_shell, "Shell: all shell beads at r=5.0 A");
        VIZ_CHECKPOINT("Dense Shell (10+1 beads, 5.0 A)", s);
    }

    // Random Cluster
    {
        CGSystemState s;
        s.build_preset(ScenePreset::RandomCluster, 15, 12.0, 42);
        CHECK(s.num_beads() == 15, "Cloud: 15 beads");
        // All beads within box [-6, 6]
        bool in_box = true;
        for (int i = 0; i < s.num_beads(); ++i) {
            const auto& p = s.beads[i].position;
            if (std::abs(p.x) > 6.0 + 1e-8 ||
                std::abs(p.y) > 6.0 + 1e-8 ||
                std::abs(p.z) > 6.0 + 1e-8) {
                in_box = false;
            }
        }
        CHECK(in_box, "Cloud: all beads within box");
        VIZ_CHECKPOINT("Random Cloud (15 beads, 12.0 A box)", s);
    }

    // Determinism: same seed
    {
        CGSystemState s1, s2;
        s1.build_preset(ScenePreset::RandomCluster, 8, 10.0, 77);
        s2.build_preset(ScenePreset::RandomCluster, 8, 10.0, 77);
        bool same = true;
        for (int i = 0; i < s1.num_beads(); ++i) {
            if (std::abs(s1.beads[i].position.x - s2.beads[i].position.x) > 1e-15 ||
                std::abs(s1.beads[i].position.y - s2.beads[i].position.y) > 1e-15 ||
                std::abs(s1.beads[i].position.z - s2.beads[i].position.z) > 1e-15) {
                same = false;
            }
        }
        CHECK(same, "Cloud: deterministic (same seed → same scene)");
    }

    // Different seeds → different scenes
    {
        CGSystemState s1, s2;
        s1.build_preset(ScenePreset::RandomCluster, 8, 10.0, 42);
        s2.build_preset(ScenePreset::RandomCluster, 8, 10.0, 99);
        bool different = false;
        for (int i = 0; i < s1.num_beads(); ++i) {
            if (std::abs(s1.beads[i].position.x - s2.beads[i].position.x) > 1e-10) {
                different = true;
                break;
            }
        }
        CHECK(different, "Cloud: different seeds → different scenes");
    }

    // Preset name parsing
    {
        CHECK(parse_scene_preset("pair") == ScenePreset::Pair, "Parse: pair");
        CHECK(parse_scene_preset("stack") == ScenePreset::LinearStack, "Parse: stack");
        CHECK(parse_scene_preset("shell") == ScenePreset::DenseShell, "Parse: shell");
        CHECK(parse_scene_preset("cloud") == ScenePreset::RandomCluster, "Parse: cloud");
        CHECK(parse_scene_preset("tshape") == ScenePreset::TShape, "Parse: tshape");
        CHECK(parse_scene_preset("square") == ScenePreset::Square, "Parse: square");
        CHECK(parse_scene_preset("isolated") == ScenePreset::Isolated, "Parse: isolated");
        CHECK(parse_scene_preset("unknown") == ScenePreset::Isolated, "Parse: unknown → isolated");
    }
}

// ============================================================================
// 2. State Inspection Tests
// ============================================================================

void test_state_inspection() {
    SECTION("State Inspection — Neighbour Lists and Environment Init");
    using namespace vsepr::cli;

    // Pair scene: each bead should see 1 neighbour
    {
        CGSystemState s;
        s.build_preset(ScenePreset::Pair, 2, 4.0, 42);
        auto nbs0 = s.build_neighbours(0);
        auto nbs1 = s.build_neighbours(1);
        CHECK(nbs0.size() == 1, "Pair: bead 0 has 1 neighbour");
        CHECK(nbs1.size() == 1, "Pair: bead 1 has 1 neighbour");
        CHECK_NEAR(nbs0[0].distance, 4.0, 1e-10, "Pair: distance = 4.0 A");
    }

    // Stack scene: interior beads have more neighbours
    {
        CGSystemState s;
        s.build_preset(ScenePreset::LinearStack, 5, 3.5, 42);
        auto nbs0 = s.build_neighbours(0);
        auto nbs2 = s.build_neighbours(2);
        // Bead 0: neighbours at 3.5, 7.0 (within cutoff 8.0)
        int n0_within = 0;
        for (const auto& nb : nbs0) {
            if (nb.distance < s.env_params.r_cutoff) ++n0_within;
        }
        // Bead 2 (center): neighbours at -7, -3.5, +3.5, +7
        int n2_within = 0;
        for (const auto& nb : nbs2) {
            if (nb.distance < s.env_params.r_cutoff) ++n2_within;
        }
        CHECK(n0_within == 2, "Stack: bead 0 has 2 neighbours within cutoff");
        CHECK(n2_within == 4, "Stack: bead 2 has 4 neighbours within cutoff");
    }

    // Environment states initialized to zero
    {
        CGSystemState s;
        s.build_preset(ScenePreset::Pair, 2, 4.0, 42);
        CHECK(s.has_env_state(), "Pair: env states initialized");
        CHECK_NEAR(s.env_states[0].eta, 0.0, 1e-15, "Pair: eta[0] = 0 initially");
        CHECK_NEAR(s.env_states[1].eta, 0.0, 1e-15, "Pair: eta[1] = 0 initially");
    }

    // Orientation data
    {
        CGSystemState s;
        s.build_preset(ScenePreset::TShape, 3, 4.0, 42);
        CHECK(s.orientation_valid[0], "TShape: bead 0 has valid orientation");
        CHECK_NEAR(s.orientations[0].z, 1.0, 1e-10, "TShape: bead 0 along z");
        CHECK_NEAR(s.orientations[2].x, 1.0, 1e-10, "TShape: bead 2 along x");
    }

    // Clear resets everything
    {
        CGSystemState s;
        s.build_preset(ScenePreset::Pair, 2, 4.0, 42);
        s.clear();
        CHECK(s.is_empty(), "Clear: empties beads");
        CHECK(s.env_states.empty(), "Clear: empties env states");
        CHECK(s.step_count == 0, "Clear: resets step count");
    }
}

// ============================================================================
// 3. Environment Update Pipeline Tests
// ============================================================================

void test_environment_update() {
    SECTION("Environment Update Pipeline — eta Relaxation");
    using namespace vsepr::cli;

    // Pair: eta should increase from 0 toward target
    {
        CGSystemState s;
        s.dt = 1.0;
        s.env_params.tau = 100.0;
        s.build_preset(ScenePreset::Pair, 2, 4.0, 42);

        s.update_environment(100);

        CHECK(s.env_states[0].eta > 0.0, "Pair: eta[0] > 0 after 100 steps");
        CHECK(s.env_states[0].eta < 1.0, "Pair: eta[0] < 1 (bounded)");
        CHECK(s.step_count == 100, "Pair: step_count = 100");
        VIZ_CHECKPOINT_OVERLAY("Pair after 100 env steps", s, test_viz::overlay::memory);
    }

    // Symmetry: identical beads should have identical eta
    {
        CGSystemState s;
        s.dt = 1.0;
        s.env_params.tau = 100.0;
        s.build_preset(ScenePreset::Pair, 2, 4.0, 42);

        s.update_environment(200);

        CHECK_NEAR(s.env_states[0].eta, s.env_states[1].eta, 1e-12,
                   "Pair: symmetric beads have identical eta");
    }

    // Stack: boundary-interior gradient
    {
        CGSystemState s;
        s.dt = 1.0;
        s.env_params.tau = 100.0;
        s.build_preset(ScenePreset::LinearStack, 5, 3.5, 42);

        s.update_environment(500);

        double eta_edge = s.env_states[0].eta;
        double eta_center = s.env_states[2].eta;
        CHECK(eta_center > eta_edge,
              "Stack: center bead has higher eta than edge bead");
        // Mirror symmetry: bead 0 == bead 4, bead 1 == bead 3
        CHECK_NEAR(s.env_states[0].eta, s.env_states[4].eta, 1e-12,
                   "Stack: mirror symmetry (bead 0 == bead 4)");
        CHECK_NEAR(s.env_states[1].eta, s.env_states[3].eta, 1e-12,
                   "Stack: mirror symmetry (bead 1 == bead 3)");
        VIZ_CHECKPOINT_OVERLAY("Stack eta gradient (500 steps)", s, test_viz::overlay::memory);
    }

    // Isolated: no neighbours
    {
        CGSystemState s;
        s.dt = 1.0;
        s.env_params.tau = 100.0;
        s.env_params.r_cutoff = 5.0;  // ensure isolated
        s.build_preset(ScenePreset::Isolated, 1, 4.0, 42);

        s.update_environment(200);

        CHECK_NEAR(s.env_states[0].eta, 0.0, 1e-12,
                   "Isolated: eta = 0 (no neighbours, no driving force)");
    }

    // Convergence: after many steps, eta should approach target_f
    {
        CGSystemState s;
        s.dt = 1.0;
        s.env_params.tau = 50.0;  // faster relaxation
        s.build_preset(ScenePreset::Pair, 2, 4.0, 42);

        s.update_environment(2000);

        double eta = s.env_states[0].eta;
        double f = s.env_states[0].target_f;
        CHECK_NEAR(eta, f, 0.01,
                   "Pair: eta converges to target_f after 2000 steps");
        VIZ_CHECKPOINT_OVERLAY("Pair converged (2000 steps)", s, test_viz::overlay::target_f);
    }

    // Monotonicity: eta should monotonically approach target from 0
    {
        CGSystemState s;
        s.dt = 1.0;
        s.env_params.tau = 100.0;
        s.build_preset(ScenePreset::Pair, 2, 4.0, 42);

        double prev_eta = 0.0;
        bool monotone = true;
        for (int step = 0; step < 200; ++step) {
            s.update_environment(1);
            double curr = s.env_states[0].eta;
            if (curr < prev_eta - 1e-15) {
                monotone = false;
                break;
            }
            prev_eta = curr;
        }
        CHECK(monotone, "Pair: eta monotonically increases from 0");
    }
}

// ============================================================================
// 4. Interaction Evaluation Tests
// ============================================================================

void test_interaction_evaluation() {
    SECTION("Interaction Evaluation — Pairwise Energy");
    using namespace vsepr::cli;

    // Create a pair with unified descriptors
    {
        CGSystemState s;
        s.build_preset(ScenePreset::Pair, 2, 4.0, 42);

        // Assign descriptors
        for (int i = 0; i < s.num_beads(); ++i) {
            coarse_grain::UnifiedDescriptor desc;
            desc.init(2);  // l_max = 2
            desc.steric.coeffs[0] = 1.0;
            desc.dispersion.coeffs[0] = 1.0;
            s.beads[i].unified = desc;
        }

        atomistic::Vec3 r_vec = s.beads[1].position - s.beads[0].position;
        auto result = coarse_grain::interaction_energy(
            *s.beads[0].unified, *s.beads[1].unified, r_vec, s.interaction_params);

        CHECK(std::isfinite(result.E_total), "Pair interaction: finite energy");
        CHECK(result.separation > 0, "Pair interaction: positive separation");
        CHECK_NEAR(result.separation, 4.0, 1e-10, "Pair interaction: r = 4.0 A");
    }

    // Steric energy decreases with distance (repulsive)
    {
        CGSystemState s1, s2;
        s1.build_preset(ScenePreset::Pair, 2, 3.0, 42);
        s2.build_preset(ScenePreset::Pair, 2, 6.0, 42);

        for (auto* s : {&s1, &s2}) {
            for (int i = 0; i < s->num_beads(); ++i) {
                coarse_grain::UnifiedDescriptor desc;
                desc.init(2);
                desc.steric.coeffs[0] = 1.0;
                s->beads[i].unified = desc;
            }
        }

        atomistic::Vec3 r1 = s1.beads[1].position - s1.beads[0].position;
        atomistic::Vec3 r2 = s2.beads[1].position - s2.beads[0].position;
        auto res1 = coarse_grain::interaction_energy(
            *s1.beads[0].unified, *s1.beads[1].unified, r1);
        auto res2 = coarse_grain::interaction_energy(
            *s2.beads[0].unified, *s2.beads[1].unified, r2);

        CHECK(res1.steric.energy > res2.steric.energy,
              "Steric: closer beads have higher energy");
    }

    // Dispersion energy is negative (attractive)
    {
        CGSystemState s;
        s.build_preset(ScenePreset::Pair, 2, 4.0, 42);

        for (int i = 0; i < s.num_beads(); ++i) {
            coarse_grain::UnifiedDescriptor desc;
            desc.init(2);
            desc.dispersion.coeffs[0] = 1.0;
            s.beads[i].unified = desc;
        }

        atomistic::Vec3 r_vec = s.beads[1].position - s.beads[0].position;
        auto result = coarse_grain::interaction_energy(
            *s.beads[0].unified, *s.beads[1].unified, r_vec);

        CHECK(result.dispersion.energy < 0,
              "Dispersion: negative energy (attractive)");
    }

    // Environment modulation changes kernel strength
    {
        CGSystemState s;
        s.build_preset(ScenePreset::Pair, 2, 4.0, 42);

        double g_zero = coarse_grain::kernel_modulation_factor(
            coarse_grain::Channel::Steric, 0.0, 0.0, s.env_params);
        CHECK_NEAR(g_zero, 1.0, 1e-10,
                   "Modulation: g_steric(0,0) = 1.0 (no modulation at eta=0)");

        double g_half = coarse_grain::kernel_modulation_factor(
            coarse_grain::Channel::Steric, 0.5, 0.5, s.env_params);
        CHECK(g_half > 1.0,
              "Modulation: g_steric(0.5,0.5) > 1.0 (steric hardening)");

        double g_elec = coarse_grain::kernel_modulation_factor(
            coarse_grain::Channel::Electrostatic, 0.5, 0.5, s.env_params);
        CHECK(g_elec < 1.0,
              "Modulation: g_elec(0.5,0.5) < 1.0 (screening)");

        double g_disp = coarse_grain::kernel_modulation_factor(
            coarse_grain::Channel::Dispersion, 0.5, 0.5, s.env_params);
        CHECK(g_disp > 1.0,
              "Modulation: g_disp(0.5,0.5) > 1.0 (dispersion enhancement)");
    }

    // Per-l decomposition sums to channel total
    {
        CGSystemState s;
        s.build_preset(ScenePreset::Pair, 2, 4.0, 42);

        for (int i = 0; i < s.num_beads(); ++i) {
            coarse_grain::UnifiedDescriptor desc;
            desc.init(4);  // l_max = 4 for richer decomposition
            desc.steric.coeffs[0] = 1.0;
            if (desc.steric.coeffs.size() > 4) desc.steric.coeffs[4] = 0.3;
            s.beads[i].unified = desc;
        }

        atomistic::Vec3 r_vec = s.beads[1].position - s.beads[0].position;
        auto result = coarse_grain::interaction_energy(
            *s.beads[0].unified, *s.beads[1].unified, r_vec);

        double sum_per_l = 0;
        for (double e : result.steric.per_l_energy) sum_per_l += e;
        CHECK_NEAR(sum_per_l, result.steric.energy, 1e-12,
                   "Per-l decomposition sums to channel total");
    }
}

// ============================================================================
// 5. System State Integration Tests
// ============================================================================

void test_system_state_integration() {
    SECTION("System State Integration — Full Pipeline");
    using namespace vsepr::cli;

    // Full workflow: build → env update → inspect → interact
    {
        CGSystemState s;
        s.dt = 1.0;
        s.env_params.tau = 100.0;
        s.build_preset(ScenePreset::LinearStack, 4, 3.5, 42);

        // 1. Verify initial state
        CHECK(s.num_beads() == 4, "Pipeline: 4 beads built");
        CHECK(s.step_count == 0, "Pipeline: 0 steps initially");

        // 2. Run environment update
        s.update_environment(100);
        CHECK(s.step_count == 100, "Pipeline: 100 steps after env update");

        // 3. All etas should be positive (dense neighbourhood)
        bool all_positive = true;
        for (int i = 0; i < s.num_beads(); ++i) {
            if (s.env_states[i].eta <= 0) all_positive = false;
        }
        CHECK(all_positive, "Pipeline: all etas positive after 100 steps");

        // 4. Assign descriptors and evaluate interactions
        for (int i = 0; i < s.num_beads(); ++i) {
            coarse_grain::UnifiedDescriptor desc;
            desc.init(2);
            desc.steric.coeffs[0] = 1.0;
            desc.dispersion.coeffs[0] = 1.0;
            s.beads[i].unified = desc;
        }

        atomistic::Vec3 r_01 = s.beads[1].position - s.beads[0].position;
        auto result = coarse_grain::interaction_energy(
            *s.beads[0].unified, *s.beads[1].unified, r_01, s.interaction_params);

        CHECK(std::isfinite(result.E_total), "Pipeline: finite interaction energy");
        CHECK_NEAR(result.separation, 3.5, 1e-10, "Pipeline: correct separation");

        // 5. Modulation reflects non-zero eta
        double g = coarse_grain::kernel_modulation_factor(
            coarse_grain::Channel::Steric,
            s.env_states[0].eta, s.env_states[1].eta,
            s.env_params);
        CHECK(g > 1.0, "Pipeline: steric modulation > 1 (non-zero eta)");
        VIZ_CHECKPOINT_OVERLAY("Full pipeline: stack after env+interact", s, test_viz::overlay::memory);
    }

    // Rebuild resets state
    {
        CGSystemState s;
        s.build_preset(ScenePreset::Pair, 2, 4.0, 42);
        s.update_environment(100);
        CHECK(s.step_count == 100, "Rebuild: steps accumulated");

        s.build_preset(ScenePreset::LinearStack, 3, 3.0, 42);
        CHECK(s.step_count == 0, "Rebuild: steps reset after rebuild");
        CHECK(s.num_beads() == 3, "Rebuild: new bead count");
        CHECK_NEAR(s.env_states[0].eta, 0.0, 1e-15, "Rebuild: eta reset to 0");
    }
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::printf("================================================================\n");
    std::printf("  CG CLI Integration Tests\n");
    std::printf("  Architecture: CLI -> command layer -> CGSystemState -> kernel\n");
    std::printf("================================================================\n");

    test_scene_presets();
    test_state_inspection();
    test_environment_update();
    test_interaction_evaluation();
    test_system_state_integration();

    std::printf("\n================================================================\n");
    std::printf("  Results: %d passed, %d failed (of %d)\n",
                g_pass, g_fail, g_pass + g_fail);
    std::printf("================================================================\n");

    return g_fail > 0 ? 1 : 0;
}
