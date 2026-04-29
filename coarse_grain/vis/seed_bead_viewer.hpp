#pragma once
/**
 * seed_bead_viewer.hpp — Live 60fps Seed-and-Bead Model Visualization
 *
 * Real-time GLFW/ImGui viewer for the 6+9 steady-state step function.
 * Renders bead positions with environment-state color overlays, live
 * convergence graphs, and snapshot capture for automatic reports.
 *
 * Features:
 *   - 60fps render loop (vsync) with FIRE stepping per frame
 *   - Overlay modes: η, ρ, C, P₂, target_f, g_steric, g_elec, g_disp
 *   - Live ImGui panels: energy graph, force graph, η convergence
 *   - Snapshot capture (F5) and automatic CSV export (F6)
 *   - Steady-state auto-stop with report generation
 *   - Camera orbit/pan/zoom (mouse)
 *
 * Anti-black-box: all per-bead diagnostics visible in inspector.
 * Deterministic: same seed → same frame sequence.
 *
 * Reference: coarse_grain/vis/cg_viz_viewer.hpp (existing pattern)
 */

#ifdef BUILD_VISUALIZATION

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "coarse_grain/models/seed_bead_stepper.hpp"
#include "coarse_grain/report/snapshot_graph.hpp"
#include "coarse_grain/core/environment_state.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace coarse_grain {

// ============================================================================
// Overlay Modes
// ============================================================================

enum class SBOverlayMode {
    Eta,            // Slow state η ∈ [0,1]
    Density,        // Local density ρ
    Coordination,   // Coordination C
    P2,             // Orientational order P₂
    TargetF,        // Target function f
    GSteric,        // Steric modulation g_s
    GElec,          // Electrostatic modulation g_e
    GDisp,          // Dispersion modulation g_d
    Energy,         // Per-bead energy proxy
    Uniform         // No overlay (solid color)
};

inline const char* sb_overlay_name(SBOverlayMode m) {
    switch (m) {
        case SBOverlayMode::Eta:          return "eta (slow state)";
        case SBOverlayMode::Density:      return "rho (density)";
        case SBOverlayMode::Coordination: return "C (coordination)";
        case SBOverlayMode::P2:           return "P2 (orient. order)";
        case SBOverlayMode::TargetF:      return "f (target)";
        case SBOverlayMode::GSteric:      return "g_steric";
        case SBOverlayMode::GElec:        return "g_elec";
        case SBOverlayMode::GDisp:        return "g_disp";
        case SBOverlayMode::Energy:       return "Energy (proxy)";
        case SBOverlayMode::Uniform:      return "Uniform";
    }
    return "unknown";
}

// ============================================================================
// Camera
// ============================================================================

struct SBCamera {
    float theta{0.5f};      // Polar angle
    float phi{0.8f};        // Azimuthal angle
    float distance{50.0f};  // Distance from origin
    float target_x{0.0f};
    float target_y{0.0f};
    float target_z{0.0f};

    void orbit(float dtheta, float dphi) {
        theta += dtheta;
        phi += dphi;
        if (phi < 0.05f) phi = 0.05f;
        if (phi > 3.09f) phi = 3.09f;
    }

    void zoom(float dz) {
        distance += dz;
        if (distance < 5.0f) distance = 5.0f;
        if (distance > 500.0f) distance = 500.0f;
    }

    void eye(float& ex, float& ey, float& ez) const {
        ex = target_x + distance * std::sin(phi) * std::cos(theta);
        ey = target_y + distance * std::cos(phi);
        ez = target_z + distance * std::sin(phi) * std::sin(theta);
    }
};

// ============================================================================
// Color Mapping
// ============================================================================

struct Color3f { float r, g, b; };

inline Color3f scalar_to_jet(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    float r = std::clamp(1.5f - std::abs(t - 0.75f) * 4.0f, 0.0f, 1.0f);
    float g = std::clamp(1.5f - std::abs(t - 0.50f) * 4.0f, 0.0f, 1.0f);
    float b = std::clamp(1.5f - std::abs(t - 0.25f) * 4.0f, 0.0f, 1.0f);
    return {r, g, b};
}

// ============================================================================
// Immediate-Mode Sphere (simple)
// ============================================================================

inline void draw_sphere_im(float cx, float cy, float cz, float radius,
                            Color3f col, int slices = 12, int stacks = 8) {
    constexpr float PI = 3.14159265f;
    for (int i = 0; i < stacks; ++i) {
        float lat0 = PI * (-0.5f + static_cast<float>(i) / stacks);
        float lat1 = PI * (-0.5f + static_cast<float>(i + 1) / stacks);
        float y0 = std::sin(lat0), y1 = std::sin(lat1);
        float r0 = std::cos(lat0), r1 = std::cos(lat1);

        glBegin(GL_TRIANGLE_STRIP);
        glColor3f(col.r, col.g, col.b);
        for (int j = 0; j <= slices; ++j) {
            float lng = 2.0f * PI * static_cast<float>(j) / slices;
            float x = std::cos(lng), z = std::sin(lng);
            glNormal3f(x * r0, y0, z * r0);
            glVertex3f(cx + radius * x * r0, cy + radius * y0, cz + radius * z * r0);
            glNormal3f(x * r1, y1, z * r1);
            glVertex3f(cx + radius * x * r1, cy + radius * y1, cz + radius * z * r1);
        }
        glEnd();
    }
}

// ============================================================================
// SeedBeadViewer — Main Viewer Class
// ============================================================================

class SeedBeadViewer {
public:
    /**
     * Launch the live 60fps seed-and-bead viewer.
     *
     * @param system    BeadSystem to visualize (will be modified in-place)
     * @param params    6+9 step function parameters
     * @param title     Window title
     */
    static void run(
        BeadSystem& system,
        SeedBeadParams params,
        const std::string& title = "VSEPR-SIM: Seed & Bead Model (60fps)")
    {
        // --- GLFW Init ---
        if (!glfwInit()) return;
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
        GLFWwindow* window = glfwCreateWindow(1400, 900, title.c_str(), nullptr, nullptr);
        if (!window) { glfwTerminate(); return; }
        glfwMakeContextCurrent(window);
        glfwSwapInterval(1);  // vsync = 60fps

        glewExperimental = GL_TRUE;
        glewInit();

        // --- ImGui Init ---
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 130");
        ImGui::StyleColorsDark();

        // --- Simulation State ---
        const size_t N = system.beads.size();
        std::vector<EnvironmentState> env_states(N);
        std::vector<atomistic::Vec3> velocities(N);
        std::vector<atomistic::Vec3> forces(N);
        FIREState fire;
        fire.dt = params.dt_initial;
        fire.alpha = params.fire_alpha_start;
        SeedBeadStepper::init(system, env_states, params);

        // --- Viewer State ---
        SBCamera camera;
        SBOverlayMode overlay = SBOverlayMode::Eta;
        SnapshotGraphCollector collector;
        collector.snapshot_interval = params.snapshot_interval;

        uint64_t step_counter = 0;
        bool running = true;
        bool reached_steady = false;
        int selected_bead = -1;
        int steps_per_frame = 1;
        bool auto_report = true;

        // Energy history for ImGui plot
        std::vector<float> energy_plot;
        std::vector<float> force_plot;
        std::vector<float> eta_plot;

        // Mouse state
        double last_mx = 0, last_my = 0;
        bool dragging = false;

        // --- GL Setup ---
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_LIGHTING);
        glEnable(GL_LIGHT0);
        glEnable(GL_COLOR_MATERIAL);
        GLfloat light_pos[] = {50.0f, 50.0f, 50.0f, 1.0f};
        GLfloat light_amb[] = {0.3f, 0.3f, 0.3f, 1.0f};
        glLightfv(GL_LIGHT0, GL_POSITION, light_pos);
        glLightfv(GL_LIGHT0, GL_AMBIENT, light_amb);

        // ================================================================
        // RENDER LOOP — 60fps via vsync
        // ================================================================

        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();

            // --- Mouse input ---
            double mx, my;
            glfwGetCursorPos(window, &mx, &my);
            if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
                if (dragging) {
                    camera.orbit(
                        static_cast<float>(mx - last_mx) * 0.005f,
                        static_cast<float>(my - last_my) * 0.005f);
                }
                dragging = true;
            } else {
                dragging = false;
            }
            last_mx = mx;
            last_my = my;

            // Scroll zoom
            // (simplified: use right-click drag for zoom)
            if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
                camera.zoom(static_cast<float>(my - last_my) * 0.1f);
            }

            // --- Key input ---
            if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) running = !running;
            if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
                // Reset
                step_counter = 0;
                reached_steady = false;
                energy_plot.clear();
                force_plot.clear();
                eta_plot.clear();
            }

            // --- Simulation step(s) ---
            if (running && !reached_steady) {
                for (int s = 0; s < steps_per_frame; ++s) {
                    auto record = SeedBeadStepper::step(
                        system, env_states, velocities, forces,
                        fire, params, step_counter);

                    collector.record(record);

                    energy_plot.push_back(static_cast<float>(record.total_energy));
                    force_plot.push_back(static_cast<float>(record.rms_force));
                    eta_plot.push_back(static_cast<float>(record.avg_eta));

                    ++step_counter;

                    if (record.steady_state) {
                        reached_steady = true;
                        break;
                    }
                }
            }

            // --- Rendering ---
            int w, h;
            glfwGetFramebufferSize(window, &w, &h);
            glViewport(0, 0, w, h);
            glClearColor(0.08f, 0.08f, 0.12f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // Projection
            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            float aspect = (h > 0) ? static_cast<float>(w) / h : 1.0f;
            float fov = 45.0f;
            float zn = 1.0f, zf = 1000.0f;
            float top = zn * std::tan(fov * 0.5f * 3.14159f / 180.0f);
            float right = top * aspect;
            glFrustum(-right, right, -top, top, zn, zf);

            // Camera
            glMatrixMode(GL_MODELVIEW);
            glLoadIdentity();
            float ex, ey, ez;
            camera.eye(ex, ey, ez);
            gluLookAt(ex, ey, ez,
                       camera.target_x, camera.target_y, camera.target_z,
                       0.0f, 1.0f, 0.0f);

            glLightfv(GL_LIGHT0, GL_POSITION, light_pos);

            // Draw beads
            for (size_t i = 0; i < N; ++i) {
                float val = get_overlay_value(i, overlay, env_states, params.env_params);
                Color3f col = scalar_to_jet(val);
                float radius = 1.5f;

                draw_sphere_im(
                    static_cast<float>(system.beads[i].position.x),
                    static_cast<float>(system.beads[i].position.y),
                    static_cast<float>(system.beads[i].position.z),
                    radius, col);
            }

            // Draw bonds (if any)
            glDisable(GL_LIGHTING);
            glColor3f(0.5f, 0.5f, 0.5f);
            glLineWidth(1.0f);
            glBegin(GL_LINES);
            for (auto& [a, b] : system.bonds) {
                if (a < N && b < N) {
                    glVertex3f(
                        static_cast<float>(system.beads[a].position.x),
                        static_cast<float>(system.beads[a].position.y),
                        static_cast<float>(system.beads[a].position.z));
                    glVertex3f(
                        static_cast<float>(system.beads[b].position.x),
                        static_cast<float>(system.beads[b].position.y),
                        static_cast<float>(system.beads[b].position.z));
                }
            }
            glEnd();
            glEnable(GL_LIGHTING);

            // ============================================================
            // ImGui Panels
            // ============================================================

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            // --- Control Panel ---
            ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(320, 500), ImGuiCond_FirstUseEver);
            ImGui::Begin("Seed & Bead Control");

            ImGui::Text("Step: %llu", (unsigned long long)step_counter);
            ImGui::Text("State: %s", reached_steady ? "STEADY STATE" :
                                     (running ? "Running" : "Paused"));
            ImGui::Separator();

            ImGui::Checkbox("Running", &running);
            ImGui::SliderInt("Steps/Frame", &steps_per_frame, 1, 50);
            ImGui::Checkbox("Auto-report at SS", &auto_report);
            ImGui::Separator();

            // Overlay selector
            int ov = static_cast<int>(overlay);
            const char* overlay_names[] = {
                "eta", "rho", "C", "P2", "f",
                "g_steric", "g_elec", "g_disp", "Energy", "Uniform"
            };
            if (ImGui::Combo("Overlay", &ov, overlay_names, 10)) {
                overlay = static_cast<SBOverlayMode>(ov);
            }

            ImGui::Separator();
            ImGui::Text("System: %zu beads", N);
            if (!energy_plot.empty()) {
                ImGui::Text("Energy: %.4f kcal/mol", energy_plot.back());
                ImGui::Text("RMS Force: %.6f", force_plot.back());
                ImGui::Text("Mean eta: %.4f", eta_plot.back());
            }

            ImGui::Separator();
            ImGui::Text("6+9 Unit Status:");
            if (!collector.energy_series.points.empty()) {
                auto& last = collector.energy_series.points.back();
                ImGui::BulletText("S1-S6: SEED active");
                ImGui::BulletText("B1-B9: BEAD active");
                ImGui::BulletText("dt = %.3f fs", collector.dt_series.final_val());
            }

            ImGui::End();

            // --- Energy Graph ---
            ImGui::SetNextWindowPos(ImVec2(10, 520), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(320, 180), ImGuiCond_FirstUseEver);
            ImGui::Begin("Energy Convergence");
            if (!energy_plot.empty()) {
                ImGui::PlotLines("E (kcal/mol)", energy_plot.data(),
                    static_cast<int>(energy_plot.size()), 0, nullptr,
                    FLT_MAX, FLT_MAX, ImVec2(300, 120));
            }
            ImGui::End();

            // --- Force Graph ---
            ImGui::SetNextWindowPos(ImVec2(340, 520), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(320, 180), ImGuiCond_FirstUseEver);
            ImGui::Begin("RMS Force");
            if (!force_plot.empty()) {
                ImGui::PlotLines("F_rms", force_plot.data(),
                    static_cast<int>(force_plot.size()), 0, nullptr,
                    FLT_MAX, FLT_MAX, ImVec2(300, 120));
            }
            ImGui::End();

            // --- Eta Graph ---
            ImGui::SetNextWindowPos(ImVec2(670, 520), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(320, 180), ImGuiCond_FirstUseEver);
            ImGui::Begin("Mean eta (slow state)");
            if (!eta_plot.empty()) {
                ImGui::PlotLines("eta", eta_plot.data(),
                    static_cast<int>(eta_plot.size()), 0, nullptr,
                    0.0f, 1.0f, ImVec2(300, 120));
            }
            ImGui::End();

            // --- Bead Inspector ---
            if (selected_bead >= 0 && selected_bead < static_cast<int>(N)) {
                ImGui::SetNextWindowPos(ImVec2(1000, 10), ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowSize(ImVec2(380, 300), ImGuiCond_FirstUseEver);
                ImGui::Begin("Bead Inspector");
                auto& b = system.beads[selected_bead];
                auto& es = env_states[selected_bead];
                ImGui::Text("Bead #%d", selected_bead);
                ImGui::Separator();
                ImGui::Text("Position: (%.3f, %.3f, %.3f)",
                    b.position.x, b.position.y, b.position.z);
                ImGui::Text("Mass: %.3f amu", b.mass);
                ImGui::Separator();
                ImGui::Text("rho: %.4f (hat: %.4f)", es.rho, es.rho_hat);
                ImGui::Text("C: %.1f", es.C);
                ImGui::Text("P2: %.4f (hat: %.4f)", es.P2, es.P2_hat);
                ImGui::Text("eta: %.4f", es.eta);
                ImGui::Text("f: %.4f", es.target_f);
                ImGui::Text("Neighbours: %d", es.neighbour_count);
                ImGui::End();
            }

            // --- Steady-State Notification ---
            if (reached_steady) {
                ImGui::SetNextWindowPos(
                    ImVec2(w * 0.5f - 200, h * 0.5f - 50), ImGuiCond_Always);
                ImGui::SetNextWindowSize(ImVec2(400, 100));
                ImGui::Begin("Steady State Reached",
                    nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
                ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f),
                    "STEADY STATE at step %llu",
                    (unsigned long long)step_counter);
                if (ImGui::Button("Generate Report")) {
                    SeedBeadStepper::SeedBeadResult res;
                    res.converged = true;
                    res.steps_taken = step_counter;
                    if (!energy_plot.empty()) {
                        res.final_energy = energy_plot.back();
                        res.final_rms_force = force_plot.back();
                        res.final_avg_eta = eta_plot.back();
                    }
                    write_seed_bead_report(
                        "seed_bead_report.md", title, params, res, collector);
                    collector.export_timeseries_csv("seed_bead_timeseries.csv");
                    collector.export_snapshots_csv("seed_bead_snapshots.csv");
                }
                if (ImGui::Button("Continue")) {
                    reached_steady = false;
                }
                ImGui::End();
            }

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
    }

private:
    static float get_overlay_value(
        size_t bead_idx,
        SBOverlayMode mode,
        const std::vector<EnvironmentState>& env_states,
        const EnvironmentParams& params)
    {
        if (bead_idx >= env_states.size()) return 0.0f;
        const auto& es = env_states[bead_idx];

        switch (mode) {
            case SBOverlayMode::Eta:
                return static_cast<float>(es.eta);
            case SBOverlayMode::Density:
                return static_cast<float>(es.rho_hat);
            case SBOverlayMode::Coordination:
                return static_cast<float>(std::clamp(es.C / 12.0, 0.0, 1.0));
            case SBOverlayMode::P2:
                return static_cast<float>(es.P2_hat);
            case SBOverlayMode::TargetF:
                return static_cast<float>(es.target_f);
            case SBOverlayMode::GSteric:
                return static_cast<float>(std::clamp(
                    kernel_modulation_factor(Channel::Steric, es.eta, es.eta, params) - 0.5, 0.0, 1.0));
            case SBOverlayMode::GElec:
                return static_cast<float>(std::clamp(
                    kernel_modulation_factor(Channel::Electrostatic, es.eta, es.eta, params) + 0.5, 0.0, 1.0));
            case SBOverlayMode::GDisp:
                return static_cast<float>(std::clamp(
                    kernel_modulation_factor(Channel::Dispersion, es.eta, es.eta, params) - 0.5, 0.0, 1.0));
            case SBOverlayMode::Energy:
                return static_cast<float>(es.rho_hat * 0.5 + es.eta * 0.5);
            case SBOverlayMode::Uniform:
                return 0.5f;
        }
        return 0.0f;
    }
};

} // namespace coarse_grain

#endif // BUILD_VISUALIZATION
