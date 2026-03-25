/**
 * cg_anim_demo.cpp — Animated Ensemble Proxy Demo
 *
 * Demonstrates eta relaxation and ensemble proxy evolution across four
 * bead scene families. Reuses the test scene infrastructure directly
 * (scene_factory.hpp / test_runners.hpp) to construct deterministic
 * scenes, run them to convergence, and then visualise the resulting
 * states with an animated overlay cycle in CGVizViewer.
 *
 * Scene families:
 *   1. Cubic lattice (tight, 3 A)   — high cohesion, regular structure
 *   2. Cubic lattice (loose, 6 A)   — sparse, reduced cohesion
 *   3. Aligned stack cloud (bias 1) — high texture proxy (P2 alignment)
 *   4. Random cluster (N=27)        — low uniformity, high surface sensitivity
 *
 * Animation per scene (~14 s auto, then manual):
 *   density    → orbit  → coordination  → orbit
 *   eta/memory → orbit  → orient-order  → orbit
 *   reset camera → close
 *
 * Controls during animation:
 *   Right-drag   orbit camera
 *   Scroll       zoom
 *   O            cycle overlay manually
 *   ESC          skip to next scene
 *
 * Flags:
 *   --headless   print proxy table only, skip viewer
 *   --help       show usage
 *
 * Architecture position:
 *   test_util scene builders + runners  (test infrastructure reuse)
 *         ↓
 *   EnvironmentState (converged, 500 steps)
 *         ↓
 *   EnsembleProxySummary  (printed to terminal)
 *         ↓
 *   CGVizViewer  (animated overlay display — BUILD_VIS required)
 *
 * Reference: Emergent Effective Medium Mapping specification (Suite #5)
 */

#include "tests/scene_factory.hpp"
#include "coarse_grain/analysis/ensemble_proxy.hpp"
#include "cli/system_state.hpp"
#include "coarse_grain/core/bead.hpp"

#ifdef BUILD_VISUALIZATION
#  include "coarse_grain/vis/cg_viz_viewer.hpp"
#endif

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// ============================================================================
// Default environment parameters — mirrors default_params() in Suite #5
// ============================================================================

static coarse_grain::EnvironmentParams make_params() {
    coarse_grain::EnvironmentParams p;
    p.alpha        = 0.5;
    p.beta         = 0.5;
    p.tau          = 100.0;
    p.gamma_steric = 0.2;
    p.gamma_elec   = -0.1;
    p.gamma_disp   = 0.5;
    p.sigma_rho    = 3.0;
    p.r_cutoff     = 8.0;
    p.delta_sw     = 1.0;
    p.rho_max      = 10.0;
    return p;
}

// ============================================================================
// Demo scene descriptor
// ============================================================================

struct DemoScene {
    const char* name;
    std::vector<test_util::SceneBead> beads;
};

static std::vector<DemoScene> build_demo_scenes() {
    std::vector<DemoScene> demos;

    demos.push_back({"Cubic Lattice  (tight, 3 A, N=27)",
                     test_util::scene_cubic_lattice(3, 3.0)});

    demos.push_back({"Cubic Lattice  (loose, 6 A, N=27)",
                     test_util::scene_cubic_lattice(3, 6.0)});

    demos.push_back({"Aligned Stack  (bias=1.0, N=30)",
                     test_util::scene_biased_stack_cloud(30, 3.5, 1.0, 0.0, 42)});

    demos.push_back({"Random Cluster (N=27, box=12 A)",
                     test_util::scene_random_cluster(27, 12.0, 99)});

    return demos;
}

// ============================================================================
// Convert SceneBead + EnvironmentState vectors → CGSystemState
// ============================================================================

static vsepr::cli::CGSystemState make_cg_state(
    const std::string&                              name,
    const std::vector<test_util::SceneBead>&        scene,
    const std::vector<coarse_grain::EnvironmentState>& states,
    const coarse_grain::EnvironmentParams&          params,
    int                                             step_count)
{
    vsepr::cli::CGSystemState cg;
    cg.scene_name  = name;
    cg.env_params  = params;
    cg.step_count  = step_count;

    for (size_t i = 0; i < scene.size(); ++i) {
        coarse_grain::Bead b;
        b.position = scene[i].position;
        b.mass     = 1.0;
        cg.beads.push_back(b);
        cg.orientations.push_back(scene[i].n_hat);
        cg.orientation_valid.push_back(scene[i].has_orientation);
        cg.env_states.push_back(states[i]);
    }
    return cg;
}

// ============================================================================
// Terminal output helpers
// ============================================================================

static void print_ruler() {
    std::printf("  %s\n", std::string(110, '-').c_str());
}

static void print_proxy_header() {
    std::printf("  %-42s %9s %9s %9s %9s %9s %5s %5s\n",
                "Scene", "cohesion", "uniform", "texture",
                "stab", "surf_sens", "N", "valid");
    print_ruler();
}

static void print_proxy_row(const char* name,
                             const coarse_grain::EnsembleProxySummary& p) {
    std::printf("  %-42s %9.4f %9.4f %9.4f %9.4f %9.4f %5d %5s\n",
                name,
                p.cohesion_proxy,
                p.uniformity_proxy,
                p.texture_proxy,
                p.stabilization_proxy,
                p.surface_sensitivity_proxy,
                p.bead_count,
                p.valid ? "yes" : "no");
}

// ============================================================================
// Build animated VizConfig for one scene
// ============================================================================

#ifdef BUILD_VISUALIZATION
static coarse_grain::vis::VizConfig make_anim_config() {
    using CV = coarse_grain::vis::VizCommand;
    using OM = coarse_grain::vis::OverlayMode;

    coarse_grain::vis::VizConfig cfg;
    cfg.show_axes       = true;
    cfg.show_neighbours = true;
    cfg.window_width    = 1280;
    cfg.window_height   = 800;

    // Overlay cycle: density → coordination → eta/memory → orient-order
    // Each overlay held for 2.5 s, camera orbits 0.4 rad between each.
    cfg.commands = {
        CV::set_overlay(OM::Density),
        CV::wait(2.5f),
        CV::orbit(0.4f, 0.0f),
        CV::wait(1.0f),

        CV::set_overlay(OM::Coordination),
        CV::wait(2.5f),
        CV::orbit(0.4f, 0.05f),
        CV::wait(1.0f),

        CV::set_overlay(OM::Memory),
        CV::wait(2.5f),
        CV::orbit(0.4f, 0.0f),
        CV::wait(1.0f),

        CV::set_overlay(OM::OrientOrder),
        CV::wait(2.5f),
        CV::orbit(-0.35f, 0.05f),
        CV::wait(1.0f),

        CV::reset_camera(),
        CV::wait(0.5f),
        CV::close()
    };

    return cfg;
}
#endif // BUILD_VISUALIZATION

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    bool headless = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--headless") == 0) headless = true;
        if (std::strcmp(argv[i], "--help") == 0 ||
            std::strcmp(argv[i], "-h") == 0) {
            std::printf("Usage: cg-anim-demo [--headless] [--help]\n");
            std::printf("  --headless   Print proxy table only; skip viewer.\n");
            return 0;
        }
    }

    std::printf("\n");
    std::printf("╔══════════════════════════════════════════════════════════════╗\n");
    std::printf("║   VSEPR-SIM  Animated Ensemble Proxy Demo                   ║\n");
    std::printf("║   η relaxation + macroscopic proxy evolution                ║\n");
    std::printf("╚══════════════════════════════════════════════════════════════╝\n");
    std::printf("\n");

    constexpr double dt      = 10.0;
    constexpr int    n_steps = 500;

    auto params = make_params();
    auto demos  = build_demo_scenes();

    std::printf("Scenes: %zu    Steps: %d    dt: %.0f fs\n\n",
                demos.size(), n_steps, dt);

    // ---- Run all scenes to convergence ----

    struct RunResult {
        const char*                                    name;
        std::vector<test_util::SceneBead>              scene;
        std::vector<coarse_grain::EnvironmentState>    states;
        coarse_grain::EnsembleProxySummary             proxy;
    };

    std::vector<RunResult> results;
    results.reserve(demos.size());

    for (auto& demo : demos) {
        std::printf("  ▸ Running %-42s (N=%zu)  ...",
                    demo.name, demo.beads.size());
        std::fflush(stdout);

        auto states = test_util::run_all_beads(demo.beads, params, dt, n_steps);

        std::vector<atomistic::Vec3> positions;
        positions.reserve(demo.beads.size());
        for (const auto& b : demo.beads) positions.push_back(b.position);

        auto proxy = coarse_grain::compute_ensemble_proxy(
            states, positions, -1.0, params.r_cutoff);

        std::printf("  done\n");

        results.push_back({demo.name,
                           std::move(demo.beads),
                           std::move(states),
                           proxy});
    }

    // ---- Print proxy comparison table ----

    std::printf("\nProxy Summary (converged, %d steps, dt=%.0f fs):\n\n",
                n_steps, dt);
    print_proxy_header();
    for (const auto& r : results)
        print_proxy_row(r.name, r.proxy);
    print_ruler();
    std::printf("\n");

#ifdef BUILD_VISUALIZATION
    if (headless) {
        std::printf("(--headless: viewer skipped)\n\n");
        return 0;
    }

    std::printf("Opening animated viewer for each scene (~14 s auto-cycle).\n");
    std::printf("Controls: right-drag=orbit  scroll=zoom  O=cycle overlay  ESC=next scene\n\n");

    for (size_t idx = 0; idx < results.size(); ++idx) {
        const auto& r = results[idx];
        std::printf("─── Scene %zu/%zu: %s ───\n",
                    idx + 1, results.size(), r.name);
        print_proxy_row(r.name, r.proxy);
        std::printf("  Opening viewer...\n");
        std::fflush(stdout);

        // Scene name shown in the viewer window title bar
        char title[256];
        std::snprintf(title, sizeof(title),
            "%s  [coh=%.3f  tex=%.3f  stab=%.3f]",
            r.name,
            r.proxy.cohesion_proxy,
            r.proxy.texture_proxy,
            r.proxy.stabilization_proxy);

        auto cg_state = make_cg_state(
            std::string(title), r.scene, r.states, params, n_steps);

        coarse_grain::vis::CGVizViewer::run(cg_state, make_anim_config());

        std::printf("  Viewer closed.\n\n");
    }

    std::printf("╔══════════════════════════════════════════════════════════════╗\n");
    std::printf("║   Demo complete. All %zu scenes shown.                       ║\n",
                results.size());
    std::printf("╚══════════════════════════════════════════════════════════════╝\n\n");

#else
    (void)headless;
    std::printf("(BUILD_VIS=OFF — rebuild with cmake -DBUILD_VIS=ON for viewer)\n\n");
#endif

    return 0;
}
