#pragma once
/**
 * cg_viz_viewer.hpp — Lightweight CG Bead Visualization Viewer
 *
 * Provides a minimal GLFW/ImGui window for immediate visual inspection
 * of coarse-grained bead systems from the CLI.
 *
 * This is the lightweight --viz viewer, NOT the full desktop application.
 * It focuses on:
 *   - Fast startup
 *   - Scene-first layout
 *   - Bead center rendering with orientation axes
 *   - Scalar overlay selection (rho, C, P2, eta)
 *   - Simple selection / hover inspection
 *   - Neighbour shell highlight for selected beads
 *
 * Usage from CLI:
 *   vsepr cg viz --preset shell --beads 12 --spacing 5.0
 *   vsepr cg viz --preset pair --spacing 4.0 --overlay eta
 *
 * Controls:
 *   Left-click     Select bead
 *   Right-drag     Orbit camera
 *   Middle-drag    Pan camera
 *   Scroll         Zoom
 *   O              Cycle overlay (none → rho → C → P2 → eta)
 *   N              Toggle neighbour shell edges
 *   A              Toggle orientation axes
 *   ESC            Quit
 *
 * Architecture:
 *   CLI → cmd_viz() → CGVizViewer::run(state) → GLFW/ImGui window
 *
 * Anti-black-box: all bead state (position, orientation, environment)
 * is directly inspectable through the inspector panel.
 *
 * Reference: Instructional §6 (CLI --viz Specification)
 */

#include "cli/system_state.hpp"
#include <string>

#ifdef BUILD_VISUALIZATION

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

#include "coarse_grain/vis/bead_inspector_view.hpp"

namespace coarse_grain {
namespace vis {

// ============================================================================
// Scalar Overlay Mode
// ============================================================================

enum class OverlayMode {
    None,
    Density,         // rho
    Coordination,    // C
    OrientOrder,     // P2
    Memory,          // eta
    TargetF,         // target_f
    COUNT
};

inline const char* overlay_name(OverlayMode m) {
    switch (m) {
        case OverlayMode::None:         return "none";
        case OverlayMode::Density:      return "rho";
        case OverlayMode::Coordination: return "C";
        case OverlayMode::OrientOrder:  return "P2";
        case OverlayMode::Memory:       return "eta";
        case OverlayMode::TargetF:      return "target_f";
        default:                        return "unknown";
    }
}

inline OverlayMode parse_overlay_mode(const std::string& name) {
    if (name == "rho" || name == "density")    return OverlayMode::Density;
    if (name == "C" || name == "coord")        return OverlayMode::Coordination;
    if (name == "P2" || name == "p2")          return OverlayMode::OrientOrder;
    if (name == "eta" || name == "memory")     return OverlayMode::Memory;
    if (name == "target_f" || name == "f")     return OverlayMode::TargetF;
    return OverlayMode::None;
}

// ============================================================================
// Automation Command
// ============================================================================

/**
 * VizCommand — A single scripted operation for the viewer.
 *
 * Commands are processed sequentially during the render loop.
 * Non-wait commands execute immediately (batched until a Wait is hit).
 * Wait commands pause command processing for the specified duration
 * while the viewer continues rendering interactively.
 *
 * Usage:
 *   std::vector<VizCommand> cmds = {
 *       VizCommand::set_overlay(OverlayMode::Density),
 *       VizCommand::wait(2.0f),
 *       VizCommand::set_overlay(OverlayMode::Memory),
 *       VizCommand::orbit(0.5f, 0.0f),
 *       VizCommand::wait(2.0f),
 *       VizCommand::close()
 *   };
 */
struct VizCommand {
    enum class Type {
        SetOverlay,       // arg_int = OverlayMode value
        SetAxes,          // arg_bool
        SetNeighbours,    // arg_bool
        SelectBead,       // arg_int = bead index (-1 to deselect)
        SetDetailMode,    // arg_bool (requires a bead selected)
        SetViewMode,      // arg_int = ViewMode value
        OrbitCamera,      // arg_f0 = dtheta, arg_f1 = dphi
        ZoomCamera,       // arg_f0 = delta
        PanCamera,        // arg_f0 = dx, arg_f1 = dy
        ResetCamera,      // re-center on bead centroid
        Wait,             // arg_f0 = seconds to wait
        Close             // close the window
    };

    Type  type;
    int   arg_int  = 0;
    bool  arg_bool = false;
    float arg_f0   = 0.0f;
    float arg_f1   = 0.0f;

    // ---- Factory methods for clean API ----
    static VizCommand set_overlay(OverlayMode m) {
        VizCommand c; c.type = Type::SetOverlay;
        c.arg_int = static_cast<int>(m); return c;
    }
    static VizCommand set_axes(bool v) {
        VizCommand c; c.type = Type::SetAxes; c.arg_bool = v; return c;
    }
    static VizCommand set_neighbours(bool v) {
        VizCommand c; c.type = Type::SetNeighbours; c.arg_bool = v; return c;
    }
    static VizCommand select_bead(int id) {
        VizCommand c; c.type = Type::SelectBead; c.arg_int = id; return c;
    }
    static VizCommand set_detail(bool v) {
        VizCommand c; c.type = Type::SetDetailMode; c.arg_bool = v; return c;
    }
    static VizCommand set_view_mode(int m) {
        VizCommand c; c.type = Type::SetViewMode; c.arg_int = m; return c;
    }
    static VizCommand orbit(float dtheta, float dphi) {
        VizCommand c; c.type = Type::OrbitCamera;
        c.arg_f0 = dtheta; c.arg_f1 = dphi; return c;
    }
    static VizCommand zoom(float delta) {
        VizCommand c; c.type = Type::ZoomCamera; c.arg_f0 = delta; return c;
    }
    static VizCommand pan(float dx, float dy) {
        VizCommand c; c.type = Type::PanCamera;
        c.arg_f0 = dx; c.arg_f1 = dy; return c;
    }
    static VizCommand reset_camera() {
        VizCommand c; c.type = Type::ResetCamera; return c;
    }
    static VizCommand wait(float seconds) {
        VizCommand c; c.type = Type::Wait; c.arg_f0 = seconds; return c;
    }
    static VizCommand close() {
        VizCommand c; c.type = Type::Close; return c;
    }
};

// ============================================================================
// Viewer Configuration
// ============================================================================

struct VizConfig {
    OverlayMode overlay = OverlayMode::None;
    bool show_axes      = true;
    bool show_neighbours = false;
    bool show_pairs     = false;
    int  window_width   = 1280;
    int  window_height  = 800;

    // ---- Automation ----

    /**
     * If > 0, the window auto-closes after this many seconds.
     * 0 = no timeout (manual close only). Default: 0.
     */
    double timeout_seconds = 0.0;

    /**
     * Command queue executed sequentially during display.
     * Non-Wait commands execute immediately (batched).
     * Wait commands pause command processing for the specified duration.
     * The viewer continues rendering and accepting manual input throughout.
     * After all commands are consumed, the viewer remains open until
     * timeout or manual close.
     */
    std::vector<VizCommand> commands;
};

// ============================================================================
// CGVizViewer — Lightweight GLFW/ImGui Bead Viewer
// ============================================================================

class CGVizViewer {
public:
    /**
     * Launch the viewer window with the given system state.
     * Blocks until the user closes the window (ESC or window close).
     *
     * @param state   The CG system state to visualize
     * @param config  Viewer configuration (overlay, window size, etc.)
     * @return 0 on success, non-zero on error
     */
    static int run(const vsepr::cli::CGSystemState& state, const VizConfig& config);

private:
    // ---- Camera state ----
    struct Camera {
        float theta  = 0.4f;    // azimuthal angle (radians)
        float phi    = 0.3f;    // polar angle
        float dist   = 20.0f;   // distance from target
        float target_x = 0.0f;
        float target_y = 0.0f;
        float target_z = 0.0f;

        void orbit(float dtheta, float dphi) {
            theta += dtheta;
            phi   += dphi;
            constexpr float pi = 3.14159265f;
            if (phi < 0.05f) phi = 0.05f;
            if (phi > pi - 0.05f) phi = pi - 0.05f;
        }

        void pan(float dx, float dy) {
            float ct = std::cos(theta), st = std::sin(theta);
            target_x += -ct * dx;
            target_y +=  dy;
            target_z +=  st * dx;
        }

        void zoom(float delta) {
            dist *= (1.0f - delta * 0.1f);
            if (dist < 1.0f) dist = 1.0f;
            if (dist > 200.0f) dist = 200.0f;
        }

        void eye(float& ex, float& ey, float& ez) const {
            ex = target_x + dist * std::sin(phi) * std::cos(theta);
            ey = target_y + dist * std::cos(phi);
            ez = target_z + dist * std::sin(phi) * std::sin(theta);
        }
    };

    // ---- Color mapping ----
    struct Color3 {
        float r, g, b;
    };

    static Color3 scalar_to_color(double value, double vmin, double vmax) {
        if (vmax <= vmin) return {0.5f, 0.5f, 0.5f};
        float t = static_cast<float>((value - vmin) / (vmax - vmin));
        t = std::max(0.0f, std::min(1.0f, t));
        // Cool (blue) → warm (red) colormap
        float r = std::min(1.0f, 2.0f * t);
        float g = 1.0f - 2.0f * std::abs(t - 0.5f);
        float b = std::min(1.0f, 2.0f * (1.0f - t));
        return {r, g, b};
    }

    static Color3 default_bead_color() {
        return {0.3f, 0.6f, 0.9f};  // Blue
    }

    static Color3 selected_bead_color() {
        return {1.0f, 0.85f, 0.2f};  // Gold
    }

    static Color3 neighbour_bead_color() {
        return {0.2f, 0.9f, 0.4f};  // Green
    }

    // ---- GL helpers ----
    static void draw_sphere_immediate(float cx, float cy, float cz,
                                       float radius, const Color3& col,
                                       int segments = 16);
    static void draw_line_immediate(float x0, float y0, float z0,
                                     float x1, float y1, float z1,
                                     const Color3& col, float width = 2.0f);
    static void setup_projection(int width, int height, const Camera& cam);
};

// ============================================================================
// Implementation
// ============================================================================

inline void CGVizViewer::draw_sphere_immediate(float cx, float cy, float cz,
                                                float radius, const Color3& col,
                                                int segments) {
    constexpr float pi = 3.14159265f;
    glColor3f(col.r, col.g, col.b);

    for (int i = 0; i < segments; ++i) {
        float lat0 = pi * (-0.5f + static_cast<float>(i) / segments);
        float lat1 = pi * (-0.5f + static_cast<float>(i + 1) / segments);
        float y0 = std::sin(lat0), yr0 = std::cos(lat0);
        float y1 = std::sin(lat1), yr1 = std::cos(lat1);

        glBegin(GL_QUAD_STRIP);
        for (int j = 0; j <= segments; ++j) {
            float lng = 2.0f * pi * static_cast<float>(j) / segments;
            float xl = std::cos(lng), zl = std::sin(lng);

            glNormal3f(xl * yr0, y0, zl * yr0);
            glVertex3f(cx + radius * xl * yr0,
                       cy + radius * y0,
                       cz + radius * zl * yr0);

            glNormal3f(xl * yr1, y1, zl * yr1);
            glVertex3f(cx + radius * xl * yr1,
                       cy + radius * y1,
                       cz + radius * zl * yr1);
        }
        glEnd();
    }
}

inline void CGVizViewer::draw_line_immediate(float x0, float y0, float z0,
                                              float x1, float y1, float z1,
                                              const Color3& col, float width) {
    glLineWidth(width);
    glBegin(GL_LINES);
    glColor3f(col.r, col.g, col.b);
    glVertex3f(x0, y0, z0);
    glVertex3f(x1, y1, z1);
    glEnd();
}

inline void CGVizViewer::setup_projection(int width, int height,
                                           const Camera& cam) {
    float aspect = static_cast<float>(width) / std::max(1, height);
    float fov_rad = 45.0f * 3.14159265f / 180.0f;
    float f = 1.0f / std::tan(fov_rad / 2.0f);
    float near_p = 0.1f, far_p = 500.0f;

    // Projection matrix (column-major)
    float proj[16] = {};
    proj[0]  = f / aspect;
    proj[5]  = f;
    proj[10] = (far_p + near_p) / (near_p - far_p);
    proj[11] = -1.0f;
    proj[14] = 2.0f * far_p * near_p / (near_p - far_p);

    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(proj);

    // View matrix via gluLookAt equivalent
    float ex, ey, ez;
    cam.eye(ex, ey, ez);

    float fx = cam.target_x - ex, fy = cam.target_y - ey, fz = cam.target_z - ez;
    float flen = std::sqrt(fx*fx + fy*fy + fz*fz);
    fx /= flen; fy /= flen; fz /= flen;

    float ux = 0, uy = 1, uz = 0;  // world up
    // s = f × u
    float sx = fy*uz - fz*uy, sy = fz*ux - fx*uz, sz = fx*uy - fy*ux;
    float slen = std::sqrt(sx*sx + sy*sy + sz*sz);
    sx /= slen; sy /= slen; sz /= slen;
    // u' = s × f
    float upx = sy*fz - sz*fy, upy = sz*fx - sx*fz, upz = sx*fy - sy*fx;

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    // Manual lookAt (column-major for OpenGL fixed-function)
    float m[16];
    m[0] = sx;   m[4] = sy;   m[8]  = sz;   m[12] = -(sx*ex + sy*ey + sz*ez);
    m[1] = upx;  m[5] = upy;  m[9]  = upz;  m[13] = -(upx*ex + upy*ey + upz*ez);
    m[2] = -fx;  m[6] = -fy;  m[10] = -fz;  m[14] = (fx*ex + fy*ey + fz*ez);
    m[3] = 0;    m[7] = 0;    m[11] = 0;     m[15] = 1;
    glLoadMatrixf(m);
}

inline int CGVizViewer::run(const vsepr::cli::CGSystemState& state,
                             const VizConfig& config) {
    if (state.is_empty()) {
        std::cerr << "CGVizViewer: No beads to visualize.\n";
        return 1;
    }

    // ---- GLFW init ----
    if (!glfwInit()) {
        std::cerr << "CGVizViewer: Failed to initialize GLFW\n";
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);

    GLFWwindow* window = glfwCreateWindow(
        config.window_width, config.window_height,
        "VSEPR CG Viewer — Lightweight Bead Inspector",
        nullptr, nullptr);
    if (!window) {
        std::cerr << "CGVizViewer: Failed to create window\n";
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    glewExperimental = GL_TRUE;
    glewInit(); // return value is a known false-positive on Mesa 4.x+; verify manually
    while (glGetError() != GL_NO_ERROR) {} // flush any GL errors raised by GLEW init
    if (!glGetString(GL_VERSION)) {
        std::cerr << "CGVizViewer: No valid OpenGL context\n";
        glfwTerminate();
        return 1;
    }

    // ---- ImGui init ----
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 120");

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 4.0f;
    style.FrameRounding = 2.0f;
    ImGui::StyleColorsDark();

    // ---- Viewer state ----
    Camera cam;
    OverlayMode overlay = config.overlay;
    bool show_axes = config.show_axes;
    bool show_neighbours = config.show_neighbours;
    int selected_bead = -1;
    bool detail_mode = false;           // D key: deep inspection of selected bead
    ViewMode detail_view_mode = ViewMode::Scaffold;
    float detail_clip_alpha = 0.0f;
    BeadVisualRecord active_visual_record;  // Built when entering detail mode

    // Scroll callback: use window user pointer for camera access
    glfwSetWindowUserPointer(window, &cam);
    glfwSetScrollCallback(window, [](GLFWwindow* w, double /*dx*/, double dy) {
        if (ImGui::GetIO().WantCaptureMouse) return;
        auto* c = static_cast<Camera*>(glfwGetWindowUserPointer(w));
        if (c) c->zoom(static_cast<float>(dy));
    });

    // Auto-center camera on bead centroid
    {
        double cx = 0, cy = 0, cz = 0;
        for (int i = 0; i < state.num_beads(); ++i) {
            cx += state.beads[i].position.x;
            cy += state.beads[i].position.y;
            cz += state.beads[i].position.z;
        }
        int n = state.num_beads();
        cam.target_x = static_cast<float>(cx / n);
        cam.target_y = static_cast<float>(cy / n);
        cam.target_z = static_cast<float>(cz / n);

        // Auto-distance: 2.5x bounding radius
        double max_r = 0;
        for (int i = 0; i < n; ++i) {
            double dx = state.beads[i].position.x - cx / n;
            double dy = state.beads[i].position.y - cy / n;
            double dz = state.beads[i].position.z - cz / n;
            double r = std::sqrt(dx*dx + dy*dy + dz*dz);
            if (r > max_r) max_r = r;
        }
        cam.dist = std::max(8.0f, static_cast<float>(max_r * 2.5 + 5.0));
    }

    // Mouse state
    bool dragging = false;
    double last_mx = 0, last_my = 0;
    int drag_button = -1;

    // ---- Compute overlay range ----
    auto compute_overlay_range = [&](OverlayMode mode, double& vmin, double& vmax) {
        vmin = 1e30; vmax = -1e30;
        for (int i = 0; i < state.num_beads(); ++i) {
            double val = 0;
            const auto& e = state.env_states[i];
            switch (mode) {
                case OverlayMode::Density:      val = e.rho; break;
                case OverlayMode::Coordination: val = e.C; break;
                case OverlayMode::OrientOrder:  val = e.P2; break;
                case OverlayMode::Memory:       val = e.eta; break;
                case OverlayMode::TargetF:      val = e.target_f; break;
                default: break;
            }
            if (val < vmin) vmin = val;
            if (val > vmax) vmax = val;
        }
        if (vmax <= vmin) { vmin = 0; vmax = 1; }
    };

    // ---- Automation state ----
    double start_time = glfwGetTime();
    std::vector<VizCommand> cmd_queue = config.commands;
    size_t cmd_index = 0;
    double wait_start = -1.0;      // <0 means no active wait
    bool auto_timeout = config.timeout_seconds > 0.0;

    // Lambda: apply a single non-wait command to viewer state
    auto apply_command = [&](const VizCommand& cmd) {
        switch (cmd.type) {
            case VizCommand::Type::SetOverlay:
                overlay = static_cast<OverlayMode>(cmd.arg_int);
                break;
            case VizCommand::Type::SetAxes:
                show_axes = cmd.arg_bool;
                break;
            case VizCommand::Type::SetNeighbours:
                show_neighbours = cmd.arg_bool;
                break;
            case VizCommand::Type::SelectBead:
                selected_bead = cmd.arg_int;
                detail_mode = false;
                break;
            case VizCommand::Type::SetDetailMode:
                if (selected_bead >= 0 && selected_bead < state.num_beads()) {
                    detail_mode = cmd.arg_bool;
                    if (detail_mode) {
                        const coarse_grain::EnvironmentState* env_ptr = nullptr;
                        if (selected_bead < static_cast<int>(state.env_states.size()))
                            env_ptr = &state.env_states[selected_bead];
                        active_visual_record = build_visual_record(
                            state.beads[selected_bead], selected_bead,
                            nullptr, env_ptr);
                        cam.target_x = static_cast<float>(
                            state.beads[selected_bead].position.x);
                        cam.target_y = static_cast<float>(
                            state.beads[selected_bead].position.y);
                        cam.target_z = static_cast<float>(
                            state.beads[selected_bead].position.z);
                        cam.dist = std::max(4.0f,
                            static_cast<float>(
                                active_visual_record.effective_radius * 6.0));
                    }
                }
                break;
            case VizCommand::Type::SetViewMode:
                detail_view_mode = static_cast<ViewMode>(cmd.arg_int);
                break;
            case VizCommand::Type::OrbitCamera:
                cam.orbit(cmd.arg_f0, cmd.arg_f1);
                break;
            case VizCommand::Type::ZoomCamera:
                cam.zoom(cmd.arg_f0);
                break;
            case VizCommand::Type::PanCamera:
                cam.pan(cmd.arg_f0, cmd.arg_f1);
                break;
            case VizCommand::Type::ResetCamera: {
                double rcx = 0, rcy = 0, rcz = 0;
                for (int ri = 0; ri < state.num_beads(); ++ri) {
                    rcx += state.beads[ri].position.x;
                    rcy += state.beads[ri].position.y;
                    rcz += state.beads[ri].position.z;
                }
                int rn = state.num_beads();
                cam.target_x = static_cast<float>(rcx / rn);
                cam.target_y = static_cast<float>(rcy / rn);
                cam.target_z = static_cast<float>(rcz / rn);
                double rmax = 0;
                for (int ri = 0; ri < rn; ++ri) {
                    double rdx = state.beads[ri].position.x - rcx / rn;
                    double rdy = state.beads[ri].position.y - rcy / rn;
                    double rdz = state.beads[ri].position.z - rcz / rn;
                    double rr = std::sqrt(rdx*rdx + rdy*rdy + rdz*rdz);
                    if (rr > rmax) rmax = rr;
                }
                cam.dist = std::max(8.0f,
                    static_cast<float>(rmax * 2.5 + 5.0));
                break;
            }
            case VizCommand::Type::Close:
                glfwSetWindowShouldClose(window, GLFW_TRUE);
                break;
            default:
                break;
        }
    };

    // ---- Main loop ----
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // --- Automation: timeout check ---
        if (auto_timeout) {
            double elapsed = glfwGetTime() - start_time;
            if (elapsed >= config.timeout_seconds) {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
                continue;
            }
        }

        // --- Automation: command queue processing ---
        double now = glfwGetTime();
        while (cmd_index < cmd_queue.size()) {
            const auto& cmd = cmd_queue[cmd_index];
            if (cmd.type == VizCommand::Type::Wait) {
                if (wait_start < 0.0) {
                    wait_start = now;  // start waiting
                }
                if (now - wait_start >= static_cast<double>(cmd.arg_f0)) {
                    wait_start = -1.0;
                    ++cmd_index;       // wait elapsed, advance
                    continue;
                }
                break;  // still waiting — stop processing
            }
            // Non-wait command: apply immediately
            apply_command(cmd);
            ++cmd_index;
        }

        // --- Input handling ---
        double mx, my;
        glfwGetCursorPos(window, &mx, &my);

        // Keyboard
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }

        // Overlay cycle (O key, with debounce)
        static bool o_was_pressed = false;
        if (glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS && !o_was_pressed) {
            int next = (static_cast<int>(overlay) + 1) % static_cast<int>(OverlayMode::COUNT);
            overlay = static_cast<OverlayMode>(next);
            o_was_pressed = true;
        }
        if (glfwGetKey(window, GLFW_KEY_O) == GLFW_RELEASE) o_was_pressed = false;

        // Toggle axes (A)
        static bool a_was_pressed = false;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS && !a_was_pressed) {
            show_axes = !show_axes;
            a_was_pressed = true;
        }
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_RELEASE) a_was_pressed = false;

        // Toggle neighbours (N)
        static bool n_was_pressed = false;
        if (glfwGetKey(window, GLFW_KEY_N) == GLFW_PRESS && !n_was_pressed) {
            show_neighbours = !show_neighbours;
            n_was_pressed = true;
        }
        if (glfwGetKey(window, GLFW_KEY_N) == GLFW_RELEASE) n_was_pressed = false;

        // Toggle detail mode (D)
        static bool d_was_pressed = false;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS && !d_was_pressed) {
            if (selected_bead >= 0 && selected_bead < state.num_beads()) {
                detail_mode = !detail_mode;
                if (detail_mode) {
                    // Build visual record from solved bead data
                    const coarse_grain::EnvironmentState* env_ptr = nullptr;
                    if (selected_bead < static_cast<int>(state.env_states.size())) {
                        env_ptr = &state.env_states[selected_bead];
                    }
                    active_visual_record = build_visual_record(
                        state.beads[selected_bead], selected_bead,
                        nullptr, env_ptr);
                    // Focus camera on selected bead
                    cam.target_x = static_cast<float>(state.beads[selected_bead].position.x);
                    cam.target_y = static_cast<float>(state.beads[selected_bead].position.y);
                    cam.target_z = static_cast<float>(state.beads[selected_bead].position.z);
                    cam.dist = static_cast<float>(active_visual_record.effective_radius * 6.0);
                    if (cam.dist < 4.0f) cam.dist = 4.0f;
                }
            }
            d_was_pressed = true;
        }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_RELEASE) d_was_pressed = false;

        // Scale level navigation (1-5 keys) when in detail mode
        if (detail_mode) {
            static bool key1_was = false, key2_was = false, key3_was = false;
            static bool key4_was = false, key5_was = false;
            if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS && !key1_was) {
                detail_view_mode = ViewMode::Shell;     key1_was = true; }
            if (glfwGetKey(window, GLFW_KEY_1) == GLFW_RELEASE) key1_was = false;
            if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS && !key2_was) {
                detail_view_mode = ViewMode::Scaffold;  key2_was = true; }
            if (glfwGetKey(window, GLFW_KEY_2) == GLFW_RELEASE) key2_was = false;
            if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS && !key3_was) {
                detail_view_mode = ViewMode::Cutaway;   key3_was = true; }
            if (glfwGetKey(window, GLFW_KEY_3) == GLFW_RELEASE) key3_was = false;
            if (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS && !key4_was) {
                detail_view_mode = ViewMode::Residual;  key4_was = true; }
            if (glfwGetKey(window, GLFW_KEY_4) == GLFW_RELEASE) key4_was = false;
            if (glfwGetKey(window, GLFW_KEY_5) == GLFW_PRESS && !key5_was) {
                detail_view_mode = ViewMode::Comparison; key5_was = true; }
            if (glfwGetKey(window, GLFW_KEY_5) == GLFW_RELEASE) key5_was = false;
        }

        // Mouse: orbit (right button), pan (middle button)
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
            if (!dragging || drag_button != GLFW_MOUSE_BUTTON_RIGHT) {
                dragging = true;
                drag_button = GLFW_MOUSE_BUTTON_RIGHT;
                last_mx = mx; last_my = my;
            } else {
                float dx = static_cast<float>(mx - last_mx) * 0.005f;
                float dy = static_cast<float>(my - last_my) * 0.005f;
                cam.orbit(dx, dy);
                last_mx = mx; last_my = my;
            }
        } else if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS) {
            if (!dragging || drag_button != GLFW_MOUSE_BUTTON_MIDDLE) {
                dragging = true;
                drag_button = GLFW_MOUSE_BUTTON_MIDDLE;
                last_mx = mx; last_my = my;
            } else {
                float dx = static_cast<float>(mx - last_mx) * 0.02f;
                float dy = static_cast<float>(my - last_my) * 0.02f;
                cam.pan(dx, dy);
                last_mx = mx; last_my = my;
            }
        } else {
            dragging = false;
            drag_button = -1;
        }

        // Scroll zoom (via GLFW scroll callback workaround — poll method)
        // Note: For proper scroll, we'd set a callback.  For simplicity,
        // use +/- keys as zoom fallback.
        if (glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS) cam.zoom(0.5f);
        if (glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS) cam.zoom(-0.5f);

        // Left click: select nearest bead (simple screen-space picking)
        static bool lmb_was_pressed = false;
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS
            && !lmb_was_pressed && !ImGui::GetIO().WantCaptureMouse) {
            lmb_was_pressed = true;
            // Simple nearest-to-center picking in screen space
            // Project each bead position and find the closest to mouse
            int fb_w, fb_h;
            glfwGetFramebufferSize(window, &fb_w, &fb_h);
            float best_dist = 1e30f;
            int best_id = -1;

            float ex, ey, ez;
            cam.eye(ex, ey, ez);

            // Approximate: pick bead closest in 3D to the camera-to-mouse ray
            // For simplicity, use angular distance from camera→bead vs camera→mouse
            for (int i = 0; i < state.num_beads(); ++i) {
                float bx = static_cast<float>(state.beads[i].position.x);
                float by = static_cast<float>(state.beads[i].position.y);
                float bz = static_cast<float>(state.beads[i].position.z);
                float dx = bx - ex, dy = by - ey, dz = bz - ez;
                float d = std::sqrt(dx*dx + dy*dy + dz*dz);
                if (d < best_dist) {
                    best_dist = d;
                    best_id = i;
                }
            }
            selected_bead = best_id;
        }
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_RELEASE) {
            lmb_was_pressed = false;
        }

        // --- ImGui frame ---
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // --- Render scene ---
        int fb_w, fb_h;
        glfwGetFramebufferSize(window, &fb_w, &fb_h);
        glViewport(0, 0, fb_w, fb_h);
        glClearColor(0.10f, 0.10f, 0.14f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_LIGHTING);
        glEnable(GL_LIGHT0);

        float light_pos[] = {10.0f, 20.0f, 15.0f, 0.0f};
        float light_amb[] = {0.2f, 0.2f, 0.2f, 1.0f};
        float light_dif[] = {0.8f, 0.8f, 0.8f, 1.0f};
        glLightfv(GL_LIGHT0, GL_POSITION, light_pos);
        glLightfv(GL_LIGHT0, GL_AMBIENT, light_amb);
        glLightfv(GL_LIGHT0, GL_DIFFUSE, light_dif);
        glEnable(GL_COLOR_MATERIAL);
        glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

        setup_projection(fb_w, fb_h, cam);

        // Overlay range
        double ov_min = 0, ov_max = 1;
        if (overlay != OverlayMode::None) {
            compute_overlay_range(overlay, ov_min, ov_max);
        }

        // Find neighbours of selected bead
        std::vector<int> neighbour_ids;
        if (selected_bead >= 0 && show_neighbours) {
            auto nbs = state.build_neighbours(selected_bead);
            for (int i = 0, j = 0; i < state.num_beads(); ++i) {
                if (i == selected_bead) continue;
                if (j < static_cast<int>(nbs.size()) &&
                    nbs[j].distance < state.env_params.r_cutoff) {
                    neighbour_ids.push_back(i);
                }
                ++j;
            }
        }

        auto is_neighbour = [&](int id) {
            return std::find(neighbour_ids.begin(), neighbour_ids.end(), id)
                   != neighbour_ids.end();
        };

        // Draw beads
        for (int i = 0; i < state.num_beads(); ++i) {
            float bx = static_cast<float>(state.beads[i].position.x);
            float by = static_cast<float>(state.beads[i].position.y);
            float bz = static_cast<float>(state.beads[i].position.z);
            float radius = 0.5f;

            Color3 col;
            if (i == selected_bead) {
                col = selected_bead_color();
                radius = 0.6f;
            } else if (is_neighbour(i)) {
                col = neighbour_bead_color();
            } else if (overlay != OverlayMode::None) {
                double val = 0;
                const auto& e = state.env_states[i];
                switch (overlay) {
                    case OverlayMode::Density:      val = e.rho; break;
                    case OverlayMode::Coordination: val = e.C; break;
                    case OverlayMode::OrientOrder:  val = e.P2; break;
                    case OverlayMode::Memory:       val = e.eta; break;
                    case OverlayMode::TargetF:      val = e.target_f; break;
                    default: break;
                }
                col = scalar_to_color(val, ov_min, ov_max);
            } else {
                col = default_bead_color();
            }

            draw_sphere_immediate(bx, by, bz, radius, col);

            // Orientation axis
            if (show_axes && i < static_cast<int>(state.orientations.size())) {
                const auto& n = state.orientations[i];
                float ax_len = 1.2f;
                Color3 axis_col = {0.9f, 0.9f, 0.2f};
                glDisable(GL_LIGHTING);
                draw_line_immediate(bx, by, bz,
                    bx + static_cast<float>(n.x) * ax_len,
                    by + static_cast<float>(n.y) * ax_len,
                    bz + static_cast<float>(n.z) * ax_len,
                    axis_col, 2.0f);
                glEnable(GL_LIGHTING);
            }
        }

        // Draw neighbour edges
        if (selected_bead >= 0 && show_neighbours) {
            glDisable(GL_LIGHTING);
            float sx = static_cast<float>(state.beads[selected_bead].position.x);
            float sy = static_cast<float>(state.beads[selected_bead].position.y);
            float sz = static_cast<float>(state.beads[selected_bead].position.z);
            Color3 edge_col = {0.2f, 0.8f, 0.3f};
            for (int nb_id : neighbour_ids) {
                float nx = static_cast<float>(state.beads[nb_id].position.x);
                float ny = static_cast<float>(state.beads[nb_id].position.y);
                float nz = static_cast<float>(state.beads[nb_id].position.z);
                draw_line_immediate(sx, sy, sz, nx, ny, nz, edge_col, 1.5f);
            }
            glEnable(GL_LIGHTING);
        }

        // --- Detail mode: render selected bead internal structure ---
        if (detail_mode && selected_bead >= 0) {
            BeadInspectorView::render_bead(
                active_visual_record, detail_view_mode, detail_clip_alpha);
        }

        glDisable(GL_LIGHTING);

        // --- ImGui Panels ---

        // Controls panel (top-left, compact)
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(220, 180), ImGuiCond_FirstUseEver);
        ImGui::Begin("Controls", nullptr, ImGuiWindowFlags_NoCollapse);
        {
            ImGui::Text("Scene: %s", state.scene_name.c_str());
            ImGui::Text("Beads: %d", state.num_beads());
            ImGui::Separator();

            const char* overlay_names[] = {"None", "rho", "C", "P2", "eta", "target_f"};
            int ov_idx = static_cast<int>(overlay);
            if (ImGui::Combo("Overlay", &ov_idx, overlay_names, 6)) {
                overlay = static_cast<OverlayMode>(ov_idx);
            }

            ImGui::Checkbox("Axes", &show_axes);
            ImGui::SameLine();
            ImGui::Checkbox("Neighbours", &show_neighbours);

            if (selected_bead >= 0) {
                if (ImGui::Button("Deselect")) {
                    selected_bead = -1;
                    detail_mode = false;
                }
                if (detail_mode) {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.2f, 1.0f), "[DETAIL]");
                }
            }

            // Automation status
            if (auto_timeout || !cmd_queue.empty()) {
                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Automation");
                if (auto_timeout) {
                    double remaining = config.timeout_seconds -
                                       (glfwGetTime() - start_time);
                    if (remaining < 0) remaining = 0;
                    ImGui::Text("Timeout: %.1fs / %.1fs",
                                glfwGetTime() - start_time,
                                config.timeout_seconds);
                    float frac = static_cast<float>(
                        (glfwGetTime() - start_time) / config.timeout_seconds);
                    if (frac > 1.0f) frac = 1.0f;
                    ImGui::ProgressBar(frac, ImVec2(-1, 0), "");
                }
                if (!cmd_queue.empty()) {
                    ImGui::Text("Commands: %zu / %zu",
                                cmd_index, cmd_queue.size());
                    if (cmd_index < cmd_queue.size() &&
                        cmd_queue[cmd_index].type == VizCommand::Type::Wait &&
                        wait_start >= 0.0) {
                        double w_elapsed = glfwGetTime() - wait_start;
                        double w_total = static_cast<double>(
                            cmd_queue[cmd_index].arg_f0);
                        ImGui::Text("Waiting: %.1fs / %.1fs",
                                    w_elapsed, w_total);
                    }
                }
            }
        }
        ImGui::End();

        // Overlay legend (bottom-left)
        if (overlay != OverlayMode::None) {
            ImGui::SetNextWindowPos(
                ImVec2(10, static_cast<float>(fb_h) - 80), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(220, 60), ImGuiCond_Always);
            ImGui::Begin("Legend", nullptr,
                         ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
                       | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);
            {
                ImGui::Text("Overlay: %s", overlay_name(overlay));
                ImGui::Text("Range: [%.4f, %.4f]", ov_min, ov_max);
                // Color bar
                ImDrawList* draw = ImGui::GetWindowDrawList();
                ImVec2 p = ImGui::GetCursorScreenPos();
                float bar_w = 200.0f;
                for (int s = 0; s < static_cast<int>(bar_w); ++s) {
                    float t = static_cast<float>(s) / bar_w;
                    Color3 c = scalar_to_color(ov_min + t * (ov_max - ov_min),
                                                ov_min, ov_max);
                    draw->AddRectFilled(
                        ImVec2(p.x + s, p.y),
                        ImVec2(p.x + s + 1, p.y + 10),
                        IM_COL32(static_cast<int>(c.r * 255),
                                 static_cast<int>(c.g * 255),
                                 static_cast<int>(c.b * 255), 255));
                }
            }
            ImGui::End();
        }

        // Bead inspector (right side)
        if (selected_bead >= 0 && selected_bead < state.num_beads()) {
            ImGui::SetNextWindowPos(
                ImVec2(static_cast<float>(fb_w) - 300, 10), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(280, 420), ImGuiCond_FirstUseEver);
            ImGui::Begin("Bead Inspector", nullptr, ImGuiWindowFlags_NoCollapse);
            {
                const auto& b = state.beads[selected_bead];
                const auto& e = state.env_states[selected_bead];

                ImGui::Text("Bead ID: %d", selected_bead);
                ImGui::Separator();

                ImGui::Text("Position (A)");
                ImGui::Text("  x: %.4f", b.position.x);
                ImGui::Text("  y: %.4f", b.position.y);
                ImGui::Text("  z: %.4f", b.position.z);
                ImGui::Text("Mass: %.3f amu", b.mass);

                ImGui::Separator();
                ImGui::Text("Orientation");
                if (selected_bead < static_cast<int>(state.orientations.size())) {
                    const auto& n = state.orientations[selected_bead];
                    ImGui::Text("  n_hat: (%.4f, %.4f, %.4f)", n.x, n.y, n.z);
                }

                ImGui::Separator();
                ImGui::Text("Environment State (X_B)");
                ImGui::Text("  rho:      %.6f", e.rho);
                ImGui::Text("  rho_hat:  %.6f", e.rho_hat);
                ImGui::Text("  C:        %.6f", e.C);
                ImGui::Text("  P2:       %.6f", e.P2);
                ImGui::Text("  P2_hat:   %.6f", e.P2_hat);
                ImGui::Text("  eta:      %.6f", e.eta);
                ImGui::Text("  target_f: %.6f", e.target_f);

                // Neighbour count
                auto nbs = state.build_neighbours(selected_bead);
                int n_within = 0;
                for (const auto& nb : nbs) {
                    if (nb.distance < state.env_params.r_cutoff) ++n_within;
                }
                ImGui::Separator();
                ImGui::Text("Neighbours: %d (r_cut=%.1f A)",
                            n_within, state.env_params.r_cutoff);

                // Unified descriptor info
                if (b.has_unified_data()) {
                    ImGui::Separator();
                    ImGui::Text("Unified Descriptor");
                    const auto& ud = *b.unified;
                    ImGui::Text("  l_max: %d", ud.max_l_max());
                    ImGui::Text("  Steric: %s", ud.steric.active ? "active" : "off");
                    ImGui::Text("  Electrostatic: %s",
                                ud.electrostatic.active ? "active" : "off");
                    ImGui::Text("  Dispersion: %s",
                                ud.dispersion.active ? "active" : "off");
                }
            }
            ImGui::End();

            // Detail inspector panel (replaces basic inspector when in detail mode)
            if (detail_mode) {
                ImGui::SetNextWindowPos(
                    ImVec2(static_cast<float>(fb_w) - 340, 10), ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowSize(ImVec2(320, 600), ImGuiCond_FirstUseEver);
                BeadInspectorView::draw_inspector_panel(
                    active_visual_record, detail_view_mode, detail_clip_alpha);
            }
        }

        // Help overlay (bottom-right, unobtrusive)
        {
            ImGui::SetNextWindowPos(
                ImVec2(static_cast<float>(fb_w) - 250,
                       static_cast<float>(fb_h) - 130), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(240, 120), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowBgAlpha(0.5f);
            ImGui::Begin("##help", nullptr,
                         ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
                       | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Controls:");
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                "Right-drag  Orbit");
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                "Mid-drag    Pan");
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                "+/-         Zoom");
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                "O           Cycle overlay");
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                "A/N         Axes/Neighbours");
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                "D           Detail view");
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                "1-5         View modes (detail)");
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                "ESC         Quit");
            if (detail_mode) {
                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.2f, 1.0f),
                    "DETAIL MODE: %s", view_mode_name(detail_view_mode));
            }
            ImGui::End();
        }

        // --- Finish frame ---
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // ---- Cleanup ----
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

} // namespace vis
} // namespace coarse_grain

#endif // BUILD_VISUALIZATION
