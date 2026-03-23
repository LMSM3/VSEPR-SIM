/**
 * cg-viewer.cpp — Split-Screen Atomistic ↔ Coarse-Grained Viewer
 *
 * Left pane:  Atomistic ball-and-stick (ClassicRenderer)
 * Right pane: Coarse-grained bead spheres (ClassicRenderer, large spheres)
 *
 * Interactive cross-highlighting:
 *   click bead  → highlight parent atoms (left), show indices/labels
 *   click atom  → highlight owning bead (right), show bead type/rule
 *   toggle COM ↔ COG projection
 *   mapping residual metrics in side panel
 *
 * Usage:
 *   cg-viewer                (demo: ethanol 3-bead mapping)
 *   cg-viewer molecule.xyz   (load XYZ + auto-partition)
 *
 * Controls:
 *   Left-click    — select atom/bead (depends on pane)
 *   Right-drag    — orbit camera
 *   Scroll        — zoom
 *   P             — toggle COM/COG
 *   T             — toggle topology overlay
 *   A             — toggle centroid arrows
 *   ESC           — quit
 *
 * Philosophy: Anti-black-box. Every mapping is visible and traceable.
 */

#include "coarse_grain/vis/cg_split_view.hpp"
#include "coarse_grain/mapping/atom_to_bead_mapper.hpp"
#include "coarse_grain/report/mapping_report.hpp"
#include "vis/renderer_classic.hpp"
#include "vis/gl_camera.hpp"
#include "core/math_vec3.hpp"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <iostream>
#include <string>
#include <vector>

using namespace vsepr;
using namespace vsepr::render;

// ============================================================================
// Demo Data: Ethanol C2H6O → 3 beads (CH3 / CH2 / OH)
// ============================================================================

static atomistic::State make_demo_state() {
    atomistic::State s;
    s.N = 9;
    s.X.resize(s.N); s.V.resize(s.N); s.Q.resize(s.N, 0.0);
    s.M.resize(s.N); s.type.resize(s.N); s.F.resize(s.N);

    s.X[0] = {-1.5, 0.0, 0.0}; s.M[0] = 12.011; s.type[0] = 6;  s.Q[0] = -0.18;
    s.X[1] = {-2.1, 0.9, 0.0}; s.M[1] =  1.008; s.type[1] = 1;  s.Q[1] =  0.06;
    s.X[2] = {-2.1,-0.5, 0.8}; s.M[2] =  1.008; s.type[2] = 1;  s.Q[2] =  0.06;
    s.X[3] = {-2.1,-0.5,-0.8}; s.M[3] =  1.008; s.type[3] = 1;  s.Q[3] =  0.06;
    s.X[4] = { 0.0, 0.0, 0.0}; s.M[4] = 12.011; s.type[4] = 6;  s.Q[4] =  0.145;
    s.X[5] = { 0.5, 0.9, 0.0}; s.M[5] =  1.008; s.type[5] = 1;  s.Q[5] =  0.06;
    s.X[6] = { 0.5,-0.9, 0.0}; s.M[6] =  1.008; s.type[6] = 1;  s.Q[6] =  0.06;
    s.X[7] = { 1.2, 0.0, 0.0}; s.M[7] = 15.999; s.type[7] = 8;  s.Q[7] = -0.683;
    s.X[8] = { 1.8, 0.8, 0.0}; s.M[8] =  1.008; s.type[8] = 1;  s.Q[8] =  0.418;

    for (auto& v : s.V) v = {0.0, 0.0, 0.0};
    s.B = {{0,1},{0,2},{0,3},{0,4},{4,5},{4,6},{4,7},{7,8}};
    return s;
}

static coarse_grain::MappingScheme make_demo_scheme() {
    using namespace coarse_grain;
    MappingScheme scheme;
    scheme.name = "Ethanol 3-bead (CH3/CH2/OH)";

    MappingRule r0; r0.rule_id = 0; r0.label = "CH3"; r0.bead_type_id = 0;
    r0.selector.mode = SelectorMode::BY_INDICES; r0.selector.indices = {0,1,2,3};
    scheme.rules.push_back(r0);

    MappingRule r1; r1.rule_id = 1; r1.label = "CH2"; r1.bead_type_id = 1;
    r1.selector.mode = SelectorMode::BY_INDICES; r1.selector.indices = {4,5,6};
    scheme.rules.push_back(r1);

    MappingRule r2; r2.rule_id = 2; r2.label = "OH"; r2.bead_type_id = 2;
    r2.selector.mode = SelectorMode::BY_INDICES; r2.selector.indices = {7,8};
    scheme.rules.push_back(r2);

    return scheme;
}

static AtomicGeometry state_to_geom(const atomistic::State& s) {
    AtomicGeometry g;
    g.atomic_numbers.resize(s.N);
    g.positions.resize(s.N);
    for (uint32_t i = 0; i < s.N; ++i) {
        g.atomic_numbers[i] = static_cast<int>(s.type[i]);
        g.positions[i] = {s.X[i].x, s.X[i].y, s.X[i].z};
    }
    for (const auto& e : s.B) {
        g.bonds.emplace_back(static_cast<int>(e.i), static_cast<int>(e.j));
    }
    return g;
}

// ============================================================================
// GLFW Callbacks
// ============================================================================

struct AppState {
    vsepr::vis::Camera camera;
    vsepr::vis::CameraController* controller = nullptr;
    coarse_grain::vis::CGSplitView split_view;
    bool clicked_this_frame = false;
    float mouse_x = 0, mouse_y = 0;
};

static AppState* g_app = nullptr;

static void glfw_error_cb(int error, const char* desc) {
    std::cerr << "GLFW error " << error << ": " << desc << "\n";
}

static void mouse_button_cb(GLFWwindow* /*win*/, int button, int action, int /*mods*/) {
    if (!g_app) return;
    if (ImGui::GetIO().WantCaptureMouse) return;

    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        g_app->clicked_this_frame = true;
    }
    if (g_app->controller)
        g_app->controller->on_mouse_button(button, action == GLFW_PRESS);
}

static void cursor_pos_cb(GLFWwindow* /*win*/, double x, double y) {
    if (!g_app) return;
    g_app->mouse_x = static_cast<float>(x);
    g_app->mouse_y = static_cast<float>(y);
    if (!ImGui::GetIO().WantCaptureMouse && g_app->controller)
        g_app->controller->on_mouse_move(static_cast<float>(x), static_cast<float>(y));
}

static void scroll_cb(GLFWwindow* /*win*/, double /*dx*/, double dy) {
    if (!g_app) return;
    if (ImGui::GetIO().WantCaptureMouse) return;
    if (g_app->controller)
        g_app->controller->on_mouse_wheel(static_cast<float>(dy));
}

static void key_cb(GLFWwindow* win, int key, int /*scan*/, int action, int /*mods*/) {
    if (action != GLFW_PRESS) return;
    if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(win, 1);
    if (!g_app) return;
    if (key == GLFW_KEY_P) {
        auto mode = g_app->split_view.projection_mode();
        g_app->split_view.set_projection_mode(
            mode == coarse_grain::ProjectionMode::CENTER_OF_MASS
                ? coarse_grain::ProjectionMode::CENTER_OF_GEOMETRY
                : coarse_grain::ProjectionMode::CENTER_OF_MASS);
    }
    if (key == GLFW_KEY_T) g_app->split_view.set_show_topology(!g_app->split_view.show_topology());
    if (key == GLFW_KEY_A) g_app->split_view.set_show_arrows(!g_app->split_view.show_arrows());
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    (void)argc; (void)argv;

    std::cout << "╔══════════════════════════════════════════════════╗\n";
    std::cout << "║   CG Split-Screen Viewer                       ║\n";
    std::cout << "║   Atomistic ↔ Coarse-Grained Mapping Inspector ║\n";
    std::cout << "╚══════════════════════════════════════════════════╝\n\n";

    // --- Build mapping ---
    auto state = make_demo_state();
    auto scheme = make_demo_scheme();

    coarse_grain::AtomToBeadMapper mapper;
    auto result = mapper.map(state, scheme, coarse_grain::ProjectionMode::CENTER_OF_MASS);

    if (!result.ok) {
        std::cerr << "Mapping failed: " << result.error << "\n";
        return 1;
    }

    // Add bead types for display
    result.system.bead_types = {
        {"CH3", 0, 3.75, 0.5},
        {"CH2", 1, 3.50, 0.4},
        {"OH",  2, 3.00, 0.6}
    };

    std::cout << "  Mapping: " << state.N << " atoms → " << result.system.num_beads() << " beads\n";
    std::cout << "  Mass conserved: " << (result.conservation.mass_conserved ? "YES" : "NO") << "\n";
    std::cout << "  Charge conserved: " << (result.conservation.charge_conserved ? "YES" : "NO") << "\n";

    // Write report
    coarse_grain::write_mapping_report("cg_mapping_report.md", state, scheme, result.system, result.conservation);
    std::cout << "  Report: cg_mapping_report.md\n\n";

    // Convert atomistic state to geometry
    auto atomistic_geom = state_to_geom(state);

    // --- GLFW / OpenGL Init ---
    glfwSetErrorCallback(glfw_error_cb);
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1600, 900,
        "CG Split-Screen Viewer — Atomistic | Coarse-Grained", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create window\n";
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW\n";
        return 1;
    }

    // --- ImGui Init ---
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    ImGui::StyleColorsDark();

    // --- Renderers ---
    ClassicRenderer left_renderer;
    ClassicRenderer right_renderer;
    left_renderer.initialize();
    right_renderer.initialize();
    right_renderer.set_atom_scale(2.5f);     // Beads are visually larger
    right_renderer.set_auto_bond(false);     // Use explicit bead bonds

    // --- Camera ---
    AppState app;
    app.camera.set_perspective(45.0f, 1.0f, 0.1f, 100.0f);
    app.camera.set_position(glm::vec3(0, 0, 8));
    app.camera.set_target(glm::vec3(0, 0, 0));

    vsepr::vis::CameraController cam_ctrl(app.camera);
    app.controller = &cam_ctrl;

    // --- Split View ---
    app.split_view.set_data(atomistic_geom, result.system, scheme, result.conservation);
    g_app = &app;

    glfwSetMouseButtonCallback(window, mouse_button_cb);
    glfwSetCursorPosCallback(window, cursor_pos_cb);
    glfwSetScrollCallback(window, scroll_cb);
    glfwSetKeyCallback(window, key_cb);

    // --- Render Loop ---
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        int fb_w, fb_h;
        glfwGetFramebufferSize(window, &fb_w, &fb_h);
        if (fb_w <= 0 || fb_h <= 0) continue;

        int half_w = fb_w / 2;

        // Process selection
        app.split_view.update(app.mouse_x, app.mouse_y,
                              app.clicked_this_frame, fb_w, fb_h);
        app.clicked_this_frame = false;

        // Update camera aspect
        app.camera.set_aspect_ratio(static_cast<float>(half_w) / fb_h);

        // --- ImGui Frame ---
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // --- Clear ---
        glClearColor(0.12f, 0.12f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);

        // --- Left Pane: Atomistic ---
        glViewport(0, 0, half_w, fb_h);
        auto left_geom = app.split_view.get_atomistic_geom_with_highlights();
        left_renderer.render(left_geom, app.camera, half_w, fb_h);

        // --- Divider Line ---
        glViewport(half_w - 1, 0, 2, fb_h);
        glScissor(half_w - 1, 0, 2, fb_h);
        glEnable(GL_SCISSOR_TEST);
        glClearColor(0.4f, 0.4f, 0.5f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glDisable(GL_SCISSOR_TEST);

        // --- Right Pane: Coarse-Grained ---
        glViewport(half_w, 0, fb_w - half_w, fb_h);
        auto bead_geom = app.split_view.get_bead_geom();
        right_renderer.render(bead_geom, app.camera, fb_w - half_w, fb_h);

        // --- UI Panels ---
        glViewport(0, 0, fb_w, fb_h);
        app.split_view.render_ui(fb_w, fb_h);

        // --- Render ImGui ---
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // --- Cleanup ---
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
