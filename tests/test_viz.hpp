#pragma once
/**
 * test_viz.hpp — Test Visualization Infrastructure
 *
 * Provides opt-in visual checkpoints for the test harness.
 * Any test can call VIZ_CHECKPOINT() to open a GLFW/ImGui window
 * showing the current CG system state, letting the user inspect
 * exactly what the test is doing.
 *
 * Activation:
 *   1. Build with BUILD_VIS=ON  (cmake -DBUILD_VIS=ON)
 *   2. Set env var:             VSEPR_TEST_VIZ=1
 *   3. Run test:                DISPLAY=:0 ./test_my_test
 *
 * When VSEPR_TEST_VIZ is not set (or =0), all VIZ_ macros are
 * lightweight no-ops that print "[VIZ] title (skipped)" so normal
 * CI/headless test runs are unaffected.
 *
 * When BUILD_VISUALIZATION is not defined at compile time, the
 * macros compile to simple print stubs with zero GL dependency.
 *
 * Automation API:
 *   The VizSequence builder allows scripted viewer sessions:
 *
 *     test_viz::VizSequence seq;
 *     seq.overlay(test_viz::overlay::density)
 *        .wait(2.0f)
 *        .overlay(test_viz::overlay::memory)
 *        .orbit(0.5f, 0.0f)
 *        .wait(2.0f)
 *        .close();
 *     VIZ_AUTOMATED("Overlay sweep", state, seq);
 *
 *   Timed display:
 *     VIZ_TIMED("Quick look", state, 3.0);
 *
 *   Scene with sequence:
 *     VIZ_SCENE_AUTOMATED("Bead sweep", scene, params, seq);
 *
 * Usage (manual):
 *   #include "tests/test_viz.hpp"
 *
 *   void test_pair_scene() {
 *       CGSystemState state;
 *       state.build_preset(ScenePreset::Pair, 2, 4.0, 42);
 *       state.update_environment(10);
 *       CHECK(state.num_beads() == 2, "pair has 2 beads");
 *
 *       VIZ_CHECKPOINT("Pair after 10 env steps", state);
 *       // ^^ opens a window if VSEPR_TEST_VIZ=1, otherwise prints (skipped)
 *   }
 *
 * Scene factory integration:
 *   auto scene = test_util::scene_dense_shell(12, 5.0);
 *   VIZ_SCENE("Dense shell (12 beads, 5 A)", scene);
 *
 * Architecture:
 *   tests → test_viz.hpp → CGVizViewer::run() → GLFW/ImGui window
 *
 * Anti-black-box: the viewer shows every bead position, orientation,
 * environment overlay, and neighbour shell — nothing hidden.
 */

#include "cli/system_state.hpp"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

// scene_factory.hpp forward — only needed for VIZ_SCENE
#ifdef __has_include
#  if __has_include("tests/scene_factory.hpp")
#    include "tests/scene_factory.hpp"
#    define TESTVIZ_HAS_SCENE_FACTORY 1
#  endif
#endif

#ifdef BUILD_VISUALIZATION
#include "coarse_grain/vis/cg_viz_viewer.hpp"
#endif

namespace test_viz {

// ============================================================================
// Runtime opt-in check
// ============================================================================

/**
 * Returns true if VSEPR_TEST_VIZ=1 is set in the environment.
 * Result is cached on first call.
 */
inline bool enabled() {
    static int cached = -1;
    if (cached < 0) {
        const char* env = std::getenv("VSEPR_TEST_VIZ");
        cached = (env && std::string(env) == "1") ? 1 : 0;
        if (cached) {
            std::printf("[VIZ] Visual test mode ENABLED (VSEPR_TEST_VIZ=1)\n");
            std::printf("[VIZ] Each checkpoint opens a window — close to continue.\n\n");
        }
    }
    return cached == 1;
}

// ============================================================================
// Overlay mode (integer-based to avoid compile-time vis dependency)
// ============================================================================

/**
 * Overlay modes matching coarse_grain::vis::OverlayMode.
 * Using int constants so the header compiles without BUILD_VISUALIZATION.
 */
namespace overlay {
    constexpr int none         = 0;
    constexpr int density      = 1;  // rho
    constexpr int coordination = 2;  // C
    constexpr int orient_order = 3;  // P2
    constexpr int memory       = 4;  // eta
    constexpr int target_f     = 5;  // target_f
}

// ============================================================================
// Show a CGSystemState in a viewer window
// ============================================================================

/**
 * Open a viewer window displaying the given state.
 * Blocks until the user closes the window.
 *
 * @param title         Checkpoint label (shown in console output)
 * @param state         CG system state to visualize
 * @param overlay_mode  Scalar overlay (use test_viz::overlay:: constants)
 */
inline void show(const std::string& title,
                 const vsepr::cli::CGSystemState& state,
                 int overlay_mode = overlay::none) {
    if (!enabled()) {
        std::printf("  [VIZ] %s (skipped — set VSEPR_TEST_VIZ=1)\n",
                    title.c_str());
        return;
    }

#ifdef BUILD_VISUALIZATION
    std::printf("  [VIZ] %s — opening viewer... (close window to continue)\n",
                title.c_str());
    coarse_grain::vis::VizConfig config;
    config.overlay = static_cast<coarse_grain::vis::OverlayMode>(overlay_mode);
    config.show_axes = true;
    coarse_grain::vis::CGVizViewer::run(state, config);
    std::printf("  [VIZ] %s — viewer closed.\n", title.c_str());
#else
    (void)state; (void)overlay_mode;
    std::printf("  [VIZ] %s (BUILD_VIS=OFF — rebuild with -Vis)\n",
                title.c_str());
#endif
}

// ============================================================================
// Convert SceneBead vector → CGSystemState → show
// ============================================================================

#ifdef TESTVIZ_HAS_SCENE_FACTORY
/**
 * Convert a vector of SceneBeads (from scene_factory.hpp) into a
 * CGSystemState and display it.
 */
inline void show_scene(
        const std::string& title,
        const std::vector<test_util::SceneBead>& scene,
        const coarse_grain::EnvironmentParams& params =
            coarse_grain::EnvironmentParams{},
        int overlay_mode = overlay::none) {

    vsepr::cli::CGSystemState state;
    state.scene_name = title;
    state.env_params = params;

    for (const auto& sb : scene) {
        coarse_grain::Bead b;
        b.position = sb.position;
        b.mass = 1.0;
        state.beads.push_back(b);
        state.orientations.push_back(sb.n_hat);
        state.orientation_valid.push_back(sb.has_orientation);

        coarse_grain::EnvironmentState es;
        es.eta = sb.eta;
        state.env_states.push_back(es);
    }

    show(title, state, overlay_mode);
}
#endif // TESTVIZ_HAS_SCENE_FACTORY

// ============================================================================
// VizSequence — Fluent builder for scripted viewer sessions
// ============================================================================

#ifdef BUILD_VISUALIZATION

/**
 * VizSequence builds a command queue for automated viewer sessions.
 *
 * Usage:
 *   test_viz::VizSequence seq;
 *   seq.overlay(test_viz::overlay::density)
 *      .wait(2.0f)
 *      .orbit(0.5f, 0.0f)
 *      .overlay(test_viz::overlay::memory)
 *      .select(3)
 *      .detail(true)
 *      .wait(1.5f)
 *      .close();
 *
 *   test_viz::show_automated("My test", state, seq);
 *
 * Commands execute sequentially during the render loop:
 * - Non-wait commands apply immediately (batched)
 * - wait(N) pauses for N seconds while the viewer keeps rendering
 * - close() ends the session programmatically
 *
 * Manual interaction (orbit, zoom, select) remains active
 * throughout the automated sequence.
 */
class VizSequence {
    std::vector<coarse_grain::vis::VizCommand> commands_;
public:
    VizSequence& overlay(int mode) {
        commands_.push_back(coarse_grain::vis::VizCommand::set_overlay(
            static_cast<coarse_grain::vis::OverlayMode>(mode)));
        return *this;
    }
    VizSequence& axes(bool v) {
        commands_.push_back(coarse_grain::vis::VizCommand::set_axes(v));
        return *this;
    }
    VizSequence& neighbours(bool v) {
        commands_.push_back(coarse_grain::vis::VizCommand::set_neighbours(v));
        return *this;
    }
    VizSequence& select(int bead_id) {
        commands_.push_back(coarse_grain::vis::VizCommand::select_bead(bead_id));
        return *this;
    }
    VizSequence& deselect() {
        commands_.push_back(coarse_grain::vis::VizCommand::select_bead(-1));
        return *this;
    }
    VizSequence& detail(bool v) {
        commands_.push_back(coarse_grain::vis::VizCommand::set_detail(v));
        return *this;
    }
    VizSequence& view_mode(int m) {
        commands_.push_back(coarse_grain::vis::VizCommand::set_view_mode(m));
        return *this;
    }
    VizSequence& orbit(float dtheta, float dphi) {
        commands_.push_back(coarse_grain::vis::VizCommand::orbit(dtheta, dphi));
        return *this;
    }
    VizSequence& zoom(float delta) {
        commands_.push_back(coarse_grain::vis::VizCommand::zoom(delta));
        return *this;
    }
    VizSequence& pan(float dx, float dy) {
        commands_.push_back(coarse_grain::vis::VizCommand::pan(dx, dy));
        return *this;
    }
    VizSequence& reset_camera() {
        commands_.push_back(coarse_grain::vis::VizCommand::reset_camera());
        return *this;
    }
    VizSequence& wait(float seconds) {
        commands_.push_back(coarse_grain::vis::VizCommand::wait(seconds));
        return *this;
    }
    VizSequence& close() {
        commands_.push_back(coarse_grain::vis::VizCommand::close());
        return *this;
    }

    const std::vector<coarse_grain::vis::VizCommand>& commands() const {
        return commands_;
    }
    bool empty() const { return commands_.empty(); }
    size_t size() const { return commands_.size(); }
};

#else // !BUILD_VISUALIZATION

// Stub when visualization is not compiled — preserves API surface
class VizSequence {
public:
    VizSequence& overlay(int)           { return *this; }
    VizSequence& axes(bool)             { return *this; }
    VizSequence& neighbours(bool)       { return *this; }
    VizSequence& select(int)            { return *this; }
    VizSequence& deselect()             { return *this; }
    VizSequence& detail(bool)           { return *this; }
    VizSequence& view_mode(int)         { return *this; }
    VizSequence& orbit(float, float)    { return *this; }
    VizSequence& zoom(float)            { return *this; }
    VizSequence& pan(float, float)      { return *this; }
    VizSequence& reset_camera()         { return *this; }
    VizSequence& wait(float)            { return *this; }
    VizSequence& close()                { return *this; }
    bool empty() const { return true; }
    size_t size() const { return 0; }
};

#endif // BUILD_VISUALIZATION

// ============================================================================
// Automation show functions
// ============================================================================

/**
 * Open a viewer window that auto-closes after the given duration.
 * Blocks for at most `seconds` seconds. Manual interaction (orbit,
 * zoom, overlay cycling) remains active during the timed display.
 */
inline void show_timed(const std::string& title,
                       const vsepr::cli::CGSystemState& state,
                       double seconds,
                       int overlay_mode = overlay::none) {
    if (!enabled()) {
        std::printf("  [VIZ] %s (skipped — set VSEPR_TEST_VIZ=1)\n",
                    title.c_str());
        return;
    }

#ifdef BUILD_VISUALIZATION
    std::printf("  [VIZ] %s — timed %.1fs — opening viewer...\n",
                title.c_str(), seconds);
    coarse_grain::vis::VizConfig config;
    config.overlay = static_cast<coarse_grain::vis::OverlayMode>(overlay_mode);
    config.show_axes = true;
    config.timeout_seconds = seconds;
    coarse_grain::vis::CGVizViewer::run(state, config);
    std::printf("  [VIZ] %s — viewer closed.\n", title.c_str());
#else
    (void)state; (void)seconds; (void)overlay_mode;
    std::printf("  [VIZ] %s (BUILD_VIS=OFF — rebuild with -Vis)\n",
                title.c_str());
#endif
}

/**
 * Open a viewer window and execute a scripted command sequence.
 * Commands run sequentially during the render loop; the viewer
 * stays open until the sequence issues close(), timeout expires,
 * or the user closes the window manually.
 *
 * @param title      Checkpoint label
 * @param state      CG system state to visualize
 * @param seq        VizSequence of commands to execute
 * @param timeout    Auto-close timeout in seconds (0 = no timeout)
 */
inline void show_automated(const std::string& title,
                           const vsepr::cli::CGSystemState& state,
                           const VizSequence& seq,
                           double timeout = 0.0) {
    if (!enabled()) {
        std::printf("  [VIZ] %s (skipped — set VSEPR_TEST_VIZ=1)\n",
                    title.c_str());
        return;
    }

#ifdef BUILD_VISUALIZATION
    std::printf("  [VIZ] %s — automated (%zu commands",
                title.c_str(), seq.size());
    if (timeout > 0) std::printf(", timeout %.1fs", timeout);
    std::printf(") — opening viewer...\n");

    coarse_grain::vis::VizConfig config;
    config.show_axes = true;
    config.timeout_seconds = timeout;
    config.commands = seq.commands();
    coarse_grain::vis::CGVizViewer::run(state, config);
    std::printf("  [VIZ] %s — viewer closed.\n", title.c_str());
#else
    (void)state; (void)seq; (void)timeout;
    std::printf("  [VIZ] %s (BUILD_VIS=OFF — rebuild with -Vis)\n",
                title.c_str());
#endif
}

// ============================================================================
// Scene → automated show (with SceneBead conversion)
// ============================================================================

#ifdef TESTVIZ_HAS_SCENE_FACTORY
/**
 * Convert SceneBeads to CGSystemState and run an automated sequence.
 */
inline void show_scene_automated(
        const std::string& title,
        const std::vector<test_util::SceneBead>& scene,
        const coarse_grain::EnvironmentParams& params,
        const VizSequence& seq,
        double timeout = 0.0) {

    vsepr::cli::CGSystemState state;
    state.scene_name = title;
    state.env_params = params;

    for (const auto& sb : scene) {
        coarse_grain::Bead b;
        b.position = sb.position;
        b.mass = 1.0;
        state.beads.push_back(b);
        state.orientations.push_back(sb.n_hat);
        state.orientation_valid.push_back(sb.has_orientation);

        coarse_grain::EnvironmentState es;
        es.eta = sb.eta;
        state.env_states.push_back(es);
    }

    show_automated(title, state, seq, timeout);
}

/**
 * Convert SceneBeads and show with timed auto-close.
 */
inline void show_scene_timed(
        const std::string& title,
        const std::vector<test_util::SceneBead>& scene,
        const coarse_grain::EnvironmentParams& params,
        double seconds,
        int overlay_mode = overlay::none) {

    vsepr::cli::CGSystemState state;
    state.scene_name = title;
    state.env_params = params;

    for (const auto& sb : scene) {
        coarse_grain::Bead b;
        b.position = sb.position;
        b.mass = 1.0;
        state.beads.push_back(b);
        state.orientations.push_back(sb.n_hat);
        state.orientation_valid.push_back(sb.has_orientation);

        coarse_grain::EnvironmentState es;
        es.eta = sb.eta;
        state.env_states.push_back(es);
    }

    show_timed(title, state, seconds, overlay_mode);
}
#endif // TESTVIZ_HAS_SCENE_FACTORY

} // namespace test_viz

// ============================================================================
// Convenience Macros
// ============================================================================

// ---- Manual (existing) ----

/**
 * VIZ_CHECKPOINT(title, state)
 *   Show a CGSystemState at a test checkpoint.
 */
#define VIZ_CHECKPOINT(title, state) \
    test_viz::show((title), (state))

/**
 * VIZ_CHECKPOINT_OVERLAY(title, state, overlay)
 *   Show a CGSystemState with a scalar overlay.
 *   overlay: test_viz::overlay::density, etc.
 */
#define VIZ_CHECKPOINT_OVERLAY(title, state, overlay) \
    test_viz::show((title), (state), (overlay))

/**
 * VIZ_SCENE(title, scene_beads)
 *   Show a vector<SceneBead> (from scene_factory.hpp).
 */
#ifdef TESTVIZ_HAS_SCENE_FACTORY
#define VIZ_SCENE(title, scene_beads) \
    test_viz::show_scene((title), (scene_beads))
#else
#define VIZ_SCENE(title, scene_beads) \
    std::printf("  [VIZ] %s (scene_factory not available)\n", (title))
#endif

/**
 * VIZ_SCENE_OVERLAY(title, scene_beads, params, overlay)
 *   Show a SceneBead vector with custom params and overlay.
 */
#ifdef TESTVIZ_HAS_SCENE_FACTORY
#define VIZ_SCENE_OVERLAY(title, scene_beads, params, overlay) \
    test_viz::show_scene((title), (scene_beads), (params), (overlay))
#else
#define VIZ_SCENE_OVERLAY(title, scene_beads, params, overlay) \
    std::printf("  [VIZ] %s (scene_factory not available)\n", (title))
#endif

// ---- Automation ----

/**
 * VIZ_TIMED(title, state, seconds)
 *   Show a CGSystemState for a fixed duration, then auto-close.
 */
#define VIZ_TIMED(title, state, seconds) \
    test_viz::show_timed((title), (state), (seconds))

/**
 * VIZ_TIMED_OVERLAY(title, state, seconds, overlay)
 *   Show a CGSystemState with overlay for a fixed duration.
 */
#define VIZ_TIMED_OVERLAY(title, state, seconds, overlay) \
    test_viz::show_timed((title), (state), (seconds), (overlay))

/**
 * VIZ_AUTOMATED(title, state, sequence)
 *   Show a CGSystemState with a scripted VizSequence.
 *   The viewer stays open until close() command, timeout, or manual close.
 */
#define VIZ_AUTOMATED(title, state, sequence) \
    test_viz::show_automated((title), (state), (sequence))

/**
 * VIZ_AUTOMATED_TIMEOUT(title, state, sequence, timeout)
 *   Show with a VizSequence and a safety timeout.
 */
#define VIZ_AUTOMATED_TIMEOUT(title, state, sequence, timeout) \
    test_viz::show_automated((title), (state), (sequence), (timeout))

/**
 * VIZ_SCENE_TIMED(title, scene_beads, params, seconds)
 *   Show a SceneBead vector for a fixed duration.
 */
#ifdef TESTVIZ_HAS_SCENE_FACTORY
#define VIZ_SCENE_TIMED(title, scene_beads, params, seconds) \
    test_viz::show_scene_timed((title), (scene_beads), (params), (seconds))
#else
#define VIZ_SCENE_TIMED(title, scene_beads, params, seconds) \
    std::printf("  [VIZ] %s (scene_factory not available)\n", (title))
#endif

/**
 * VIZ_SCENE_AUTOMATED(title, scene_beads, params, sequence)
 *   Show a SceneBead vector with a scripted VizSequence.
 */
#ifdef TESTVIZ_HAS_SCENE_FACTORY
#define VIZ_SCENE_AUTOMATED(title, scene_beads, params, sequence) \
    test_viz::show_scene_automated((title), (scene_beads), (params), (sequence))
#else
#define VIZ_SCENE_AUTOMATED(title, scene_beads, params, sequence) \
    std::printf("  [VIZ] %s (scene_factory not available)\n", (title))
#endif
