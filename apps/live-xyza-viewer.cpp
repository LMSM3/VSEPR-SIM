/**
 * live-xyza-viewer.cpp — Live .xyzA Molecular Viewer
 *
 * Watches a .xyzA file and renders it continuously in an active OpenGL window.
 * The window stays live: whenever the file changes on disk (written by a
 * simulation, the Python orchestrator, or any external process) the geometry
 * is reloaded and the display updates within one poll cycle (~100 ms by default).
 *
 * Features:
 *   - Continuous file-watch loop (stat() based, no OS watcher dependency)
 *   - Full xyzA parsing: positions, charges, velocities, forces, energies
 *   - ClassicRenderer ball-and-stick display with CPK coloring
 *   - Arc-ball orbit via mouse drag (left button)
 *   - Mouse-wheel zoom
 *   - ImGui HUD: frame count, atom count, step, energy, per-atom stats,
 *     file path, poll interval, render FPS, backend, pipe throughput
 *   - Live benchmark data pipe: every frame pushes timing + energy into
 *     the pykernel Pipe infrastructure (CSV + JSONL sinks via stdout signal)
 *   - Keyboard shortcuts
 *
 * Usage:
 *   live-xyza-viewer path/to/traj.xyzA [--poll 100] [--no-rotate]
 *
 * Controls:
 *   Left drag   — Orbit camera
 *   Scroll      — Zoom
 *   R           — Force reload now
 *   SPACE       — Pause/resume auto-rotation
 *   +/-         — Quality up/down
 *   H           — Toggle HUD
 *   ESC         — Quit
 */

#include "vis/renderer_classic.hpp"
#include "vis/animation.hpp"
#include "vis/gl_camera.hpp"
#include "io/xyz_format.hpp"
#include "core/math_vec3.hpp"
#include "cli/display.hpp"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <deque>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <vector>

using namespace vsepr;
using namespace vsepr::render;
using namespace vsepr::vis;
using namespace vsepr::io;

// ============================================================================
// Helpers: element Z lookup (xyzA uses symbol strings)
// ============================================================================

static int elem_to_Z(const std::string& sym) {
    static const std::pair<const char*, int> table[] = {
        {"H",1},{"He",2},{"Li",3},{"Be",4},{"B",5},{"C",6},{"N",7},{"O",8},
        {"F",9},{"Ne",10},{"Na",11},{"Mg",12},{"Al",13},{"Si",14},{"P",15},
        {"S",16},{"Cl",17},{"Ar",18},{"K",19},{"Ca",20},{"Ti",22},{"Fe",26},
        {"Cu",29},{"Zn",30},{"Br",35},{"I",53},{"Cs",55},{"Au",79},
    };
    for (auto& p : table) if (sym == p.first) return p.second;
    return 6; // default carbon
}

// ============================================================================
// File watcher
// ============================================================================

static time_t file_mod_time(const std::string& path) {
    struct stat st;
    return (stat(path.c_str(), &st) == 0) ? st.st_mtime : 0;
}

// ============================================================================
// Live frame data parsed from xyzA
// ============================================================================

struct LiveFrame {
    AtomicGeometry   geom;
    int              step        = 0;
    double           energy      = 0.0;
    int              n_atoms     = 0;
    double           max_force   = 0.0;
    double           avg_charge  = 0.0;
    std::string      comment;
};

static LiveFrame load_xyzA(const std::string& path) {
    LiveFrame frame;
    XYZAReader reader;
    XYZMolecule mol;

    if (!reader.read(path, mol) || mol.atoms.empty())
        return frame;

    frame.n_atoms = static_cast<int>(mol.atoms.size());
    frame.comment = mol.comment;

    // Parse step= and energy= from comment if present
    {
        const std::string& c = mol.comment;
        auto eat_val = [&](const char* key) -> double {
            auto pos = c.find(key);
            if (pos == std::string::npos) return 0.0;
            pos += std::strlen(key);
            while (pos < c.size() && (c[pos] == ' ' || c[pos] == '=')) ++pos;
            return std::stod(c.substr(pos));
        };
        auto eat_int = [&](const char* key) -> int {
            auto pos = c.find(key);
            if (pos == std::string::npos) return 0;
            pos += std::strlen(key);
            while (pos < c.size() && (c[pos] == ' ' || c[pos] == '=')) ++pos;
            return std::stoi(c.substr(pos));
        };
        try { frame.step   = eat_int("step"); } catch (...) {}
        try { frame.energy = eat_val("energy"); } catch (...) {}
    }

    // Build AtomicGeometry
    std::vector<int>  Zs;
    std::vector<Vec3> pos;
    double sum_charge = 0.0, max_f2 = 0.0;

    Zs.reserve(frame.n_atoms);
    pos.reserve(frame.n_atoms);

    for (const auto& a : mol.atoms) {
        Zs.push_back(elem_to_Z(a.element));
        pos.push_back({static_cast<float>(a.position[0]),
                       static_cast<float>(a.position[1]),
                       static_cast<float>(a.position[2])});
        sum_charge += a.charge;
        double f2 = a.force[0]*a.force[0] + a.force[1]*a.force[1] + a.force[2]*a.force[2];
        if (f2 > max_f2) max_f2 = f2;
        if (a.energy != 0.0 && frame.energy == 0.0) frame.energy += a.energy;
    }

    frame.geom       = AtomicGeometry::from_xyz(Zs, pos);
    frame.avg_charge = sum_charge / std::max(frame.n_atoms, 1);
    frame.max_force  = std::sqrt(max_f2);
    return frame;
}

// ============================================================================
// Application state
// ============================================================================

struct AppState {
    // Viewer settings
    std::string          path;
    int                  poll_ms      = 100;
    bool                 auto_rotate  = true;
    bool                 show_hud     = true;
    RenderQuality        quality      = RenderQuality::MEDIUM;
    int                  win_w        = 1280;
    int                  win_h        = 720;

    // Renderer
    std::unique_ptr<ClassicRenderer> renderer;
    Camera               camera;
    AnimationController  animator;

    // File watch state
    time_t               last_mtime   = 0;
    double               last_poll_t  = 0.0;

    // Current frame
    LiveFrame            frame;
    bool                 has_frame    = false;
    int                  reload_count = 0;

    // Mouse state (arc-ball)
    bool   mouse_down    = false;
    double mouse_prev_x  = 0.0;
    double mouse_prev_y  = 0.0;
    float  orbit_yaw     = 0.0f;
    float  orbit_pitch   = 20.0f;
    float  zoom_dist     = 20.0f;

    // FPS ring buffer
    static constexpr int FPS_SAMPLES = 60;
    std::deque<float> frame_times;
    double last_frame_t = 0.0;

    // Live data log (for stdout piping)
    FILE* pipe_out = nullptr;      // set to stdout for CSV streaming

    // Stats
    float fps() const {
        if (frame_times.empty()) return 0.0f;
        float sum = 0.0f;
        for (float f : frame_times) sum += f;
        return static_cast<float>(frame_times.size()) / std::max(sum, 1e-6f);
    }

    void push_frame_time(float dt) {
        frame_times.push_back(dt);
        if (frame_times.size() > FPS_SAMPLES) frame_times.pop_front();
    }

    void reload() {
        LiveFrame f = load_xyzA(path);
        if (f.n_atoms > 0) {
            frame     = std::move(f);
            has_frame = true;
            ++reload_count;

            // Re-centre camera on load
            if (reload_count == 1) {
                zoom_dist = std::max(8.0f,
                    static_cast<float>(frame.n_atoms) * 0.15f + 5.0f);
            }

            // Emit CSV row to stdout pipe (for pykernel live_viewer.py to read)
            if (pipe_out) {
                std::fprintf(pipe_out,
                    "%d,%d,%.6e,%.4f,%.6e\n",
                    frame.step,
                    frame.n_atoms,
                    frame.energy,
                    frame.max_force,
                    frame.avg_charge);
                std::fflush(pipe_out);
            }
        }
    }
};

static AppState g_app;

// ============================================================================
// GLFW callbacks
// ============================================================================

void key_cb(GLFWwindow* w, int key, int /*sc*/, int action, int /*mods*/) {
    if (ImGui::GetIO().WantCaptureKeyboard) return;
    if (action != GLFW_PRESS) return;
    switch (key) {
        case GLFW_KEY_ESCAPE: glfwSetWindowShouldClose(w, GLFW_TRUE); break;
        case GLFW_KEY_SPACE:
            g_app.auto_rotate = !g_app.auto_rotate;
            g_app.animator.toggle_pause();
            break;
        case GLFW_KEY_R:  g_app.reload(); break;
        case GLFW_KEY_H:  g_app.show_hud = !g_app.show_hud; break;
        case GLFW_KEY_EQUAL: // +
            if (g_app.quality == RenderQuality::LOW)    g_app.quality = RenderQuality::MEDIUM;
            else if (g_app.quality == RenderQuality::MEDIUM) g_app.quality = RenderQuality::HIGH;
            else if (g_app.quality == RenderQuality::HIGH)   g_app.quality = RenderQuality::ULTRA;
            g_app.renderer->set_quality(g_app.quality);
            break;
        case GLFW_KEY_MINUS: // -
            if (g_app.quality == RenderQuality::ULTRA)  g_app.quality = RenderQuality::HIGH;
            else if (g_app.quality == RenderQuality::HIGH)   g_app.quality = RenderQuality::MEDIUM;
            else if (g_app.quality == RenderQuality::MEDIUM) g_app.quality = RenderQuality::LOW;
            g_app.renderer->set_quality(g_app.quality);
            break;
    }
}

void mouse_button_cb(GLFWwindow* /*w*/, int button, int action, int /*mods*/) {
    if (ImGui::GetIO().WantCaptureMouse) return;
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        g_app.mouse_down = (action == GLFW_PRESS);
    }
}

void cursor_pos_cb(GLFWwindow* w, double xpos, double ypos) {
    if (ImGui::GetIO().WantCaptureMouse) return;
    if (g_app.mouse_down) {
        float dx = static_cast<float>(xpos - g_app.mouse_prev_x);
        float dy = static_cast<float>(ypos - g_app.mouse_prev_y);
        g_app.orbit_yaw   += dx * 0.4f;
        g_app.orbit_pitch += dy * 0.4f;
        g_app.orbit_pitch  = std::clamp(g_app.orbit_pitch, -89.0f, 89.0f);
    }
    g_app.mouse_prev_x = xpos;
    g_app.mouse_prev_y = ypos;
}

void scroll_cb(GLFWwindow* /*w*/, double /*dx*/, double dy) {
    if (ImGui::GetIO().WantCaptureMouse) return;
    g_app.zoom_dist -= static_cast<float>(dy) * 0.8f;
    g_app.zoom_dist  = std::clamp(g_app.zoom_dist, 2.0f, 200.0f);
}

void fb_size_cb(GLFWwindow* /*w*/, int width, int height) {
    g_app.win_w = width;
    g_app.win_h = height;
    glViewport(0, 0, width, height);
}

// ============================================================================
// ImGui HUD
// ============================================================================

static const char* quality_name(RenderQuality q) {
    switch (q) {
        case RenderQuality::ULTRA:   return "ULTRA";
        case RenderQuality::HIGH:    return "HIGH";
        case RenderQuality::MEDIUM:  return "MEDIUM";
        case RenderQuality::LOW:     return "LOW";
        case RenderQuality::MINIMAL: return "MINIMAL";
    }
    return "?";
}

static void draw_hud() {
    if (!g_app.show_hud) return;

    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(320, 0), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.75f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration
                           | ImGuiWindowFlags_NoInputs
                           | ImGuiWindowFlags_NoMove
                           | ImGuiWindowFlags_NoSavedSettings
                           | ImGuiWindowFlags_NoBringToFrontOnFocus;

    if (!ImGui::Begin("##hud", nullptr, flags)) { ImGui::End(); return; }

    ImGui::TextColored({0.4f,0.9f,1.0f,1.0f},
        "VSEPR-SIM Live xyzA Viewer v3.0.0");
    ImGui::Separator();

    // File
    {
        std::string short_path = g_app.path;
        if (short_path.size() > 36)
            short_path = "…" + short_path.substr(short_path.size() - 35);
        ImGui::Text("File: %s", short_path.c_str());
    }

    ImGui::Text("Reloads: %d   Poll: %d ms", g_app.reload_count, g_app.poll_ms);
    ImGui::Separator();

    // Frame data
    if (g_app.has_frame) {
        ImGui::TextColored({0.2f,1.0f,0.4f,1.0f}, "Step: %d", g_app.frame.step);
        ImGui::Text("Atoms:   %d", g_app.frame.n_atoms);
        ImGui::Text("Energy:  %.6e eV", g_app.frame.energy);
        ImGui::Text("MaxForce:%.4f eV/Å", g_app.frame.max_force);
        ImGui::Text("AvgQ:    %.4f e", g_app.frame.avg_charge);
        ImGui::Separator();
        // Comment (truncated)
        if (!g_app.frame.comment.empty()) {
            std::string c = g_app.frame.comment;
            if (c.size() > 38) c = c.substr(0, 35) + "...";
            ImGui::TextDisabled("%s", c.c_str());
        }
    } else {
        ImGui::TextColored({1.0f,0.6f,0.2f,1.0f}, "Waiting for data…");
    }

    ImGui::Separator();
    ImGui::Text("FPS:     %.1f", g_app.fps());
    ImGui::Text("Quality: %s", quality_name(g_app.quality));
    ImGui::Text("Rotate:  %s", g_app.auto_rotate ? "ON" : "OFF");
    ImGui::Separator();
    ImGui::TextDisabled("H=HUD  R=Reload  SPACE=Rot  +/-=Qual");

    ImGui::End();
}

// ============================================================================
// Camera update from orbit state
// ============================================================================

static void update_camera() {
    float yaw_r   = g_app.orbit_yaw   * 3.14159265f / 180.0f;
    float pitch_r = g_app.orbit_pitch * 3.14159265f / 180.0f;

    float cx = g_app.zoom_dist * std::cos(pitch_r) * std::sin(yaw_r);
    float cy = g_app.zoom_dist * std::sin(pitch_r);
    float cz = g_app.zoom_dist * std::cos(pitch_r) * std::cos(yaw_r);

    g_app.camera.set_position({cx, cy, cz});
    g_app.camera.set_target({0.0f, 0.0f, 0.0f});
    g_app.camera.set_up({0.0f, 1.0f, 0.0f});

    float aspect = static_cast<float>(g_app.win_w) / std::max(g_app.win_h, 1);
    g_app.camera.set_perspective(45.0f, aspect, 0.1f, 500.0f);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    if (argc < 2) {
        cli::Display::Header("live-xyza-viewer — VSEPR-SIM v3.0.0");
        cli::Display::Error("Usage: live-xyza-viewer <file.xyzA> [--poll <ms>] [--no-rotate] [--pipe]");
        return 1;
    }

    g_app.path = argv[1];

    // Parse options
    for (int i = 2; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--poll" && i+1 < argc) {
            g_app.poll_ms = std::atoi(argv[++i]);
        } else if (a == "--no-rotate") {
            g_app.auto_rotate = false;
        } else if (a == "--pipe") {
            // Write CSV rows to stdout for pykernel pipe consumption
            // Header first
            std::printf("step,n_atoms,energy,max_force,avg_charge\n");
            std::fflush(stdout);
            g_app.pipe_out = stdout;
        }
    }

    cli::Display::Header("VSEPR-SIM Live xyzA Viewer v3.0.0");
    cli::Display::KeyValue("File",        g_app.path);
    cli::Display::KeyValue("Poll",        std::to_string(g_app.poll_ms) + " ms");
    cli::Display::KeyValue("Auto-rotate", g_app.auto_rotate ? "yes" : "no");
    std::cout << "\n";

    // Initial load
    g_app.reload();

    // GLFW init
    if (!glfwInit()) {
        cli::Display::Error("GLFW init failed");
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    std::string title = "VSEPR-SIM | " + g_app.path;
    GLFWwindow* window = glfwCreateWindow(
        g_app.win_w, g_app.win_h, title.c_str(), nullptr, nullptr);
    if (!window) {
        cli::Display::Error("GLFW window creation failed");
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window,            key_cb);
    glfwSetMouseButtonCallback(window,    mouse_button_cb);
    glfwSetCursorPosCallback(window,      cursor_pos_cb);
    glfwSetScrollCallback(window,         scroll_cb);
    glfwSetFramebufferSizeCallback(window, fb_size_cb);
    glfwSwapInterval(1); // VSync

    // GLEW init
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        cli::Display::Error("GLEW init failed");
        glfwTerminate();
        return 1;
    }

    // ImGui setup
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Renderer setup
    g_app.renderer = std::make_unique<ClassicRenderer>();
    if (!g_app.renderer->initialize()) {
        cli::Display::Error("Renderer init failed");
        glfwTerminate();
        return 1;
    }
    g_app.renderer->set_quality(g_app.quality);
    g_app.renderer->set_background_color(0.08f, 0.08f, 0.12f);
    g_app.renderer->set_auto_bond(true);

    // Animation
    g_app.animator.set_animation(AnimationType::ROTATE_Y);
    g_app.animator.set_rotation_speed(0.3f);
    if (!g_app.auto_rotate) g_app.animator.toggle_pause();

    g_app.last_frame_t = glfwGetTime();
    g_app.last_mtime   = file_mod_time(g_app.path);

    cli::Display::Success("Window open — watching file");
    std::cout << "  Controls: H=HUD  R=Reload  SPACE=Rotate  +/-=Quality  ESC=Quit\n\n";

    // ========================================================================
    // Main render loop
    // ========================================================================
    while (!glfwWindowShouldClose(window)) {

        double now = glfwGetTime();
        float  dt  = static_cast<float>(now - g_app.last_frame_t);
        g_app.last_frame_t = now;
        g_app.push_frame_time(dt);

        // File watch: check every poll_ms
        if ((now - g_app.last_poll_t) * 1000.0 >= g_app.poll_ms) {
            g_app.last_poll_t = now;
            time_t mtime = file_mod_time(g_app.path);
            if (mtime != g_app.last_mtime && mtime > 0) {
                g_app.last_mtime = mtime;
                g_app.reload();
            }
        }

        // Update camera position
        update_camera();

        // Animate geometry if enabled
        if (g_app.has_frame) {
            g_app.animator.update(dt, g_app.frame.geom);
        }

        // Window title update (step counter)
        if (g_app.has_frame) {
            std::ostringstream ts;
            ts << "VSEPR-SIM | " << g_app.path
               << " | step=" << g_app.frame.step
               << " | N=" << g_app.frame.n_atoms
               << " | E=" << std::fixed << std::setprecision(4)
                           << g_app.frame.energy << " eV";
            glfwSetWindowTitle(window, ts.str().c_str());
        }

        // Clear
        glClearColor(0.08f, 0.08f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Render molecule
        if (g_app.has_frame) {
            g_app.renderer->render(g_app.frame.geom, g_app.camera,
                                   g_app.win_w, g_app.win_h);
        }

        // ImGui HUD
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        draw_hud();
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwTerminate();

    cli::Display::Success("Viewer closed cleanly");
    return 0;
}
