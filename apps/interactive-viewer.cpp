/**
 * Interactive Molecular Viewer - Windows 11 Style UI
 * 
 * Demonstrates the complete interactive visualization system with:
 * - Windows 11 light theme
 * - Mouse picking (hover over atoms/bonds)
 * - Rich tooltips with element data
 * - Animations and visual effects
 * - PBC visualization for crystals
 * 
 * Features:
 * - **Hover over atoms**: Shows element name, symbol, mass, electronegativity,
 *   position, radii, coordination number, bonded atoms with distances
 * - **Hover over bonds**: Shows bond length (single number)
 * - Modern Windows 11 light UI theme
 * - Animation controls
 * - Quality settings
 * - Visual effects
 * 
 * Usage:
 *   interactive-viewer molecule.xyz [options]
 * 
 * Controls:
 *   Mouse Hover - Show atom/bond tooltips
 *   SPACE       - Play/pause animation
 *   1-6         - Change animation type
 *   Q/W         - Decrease/increase quality
 *   P           - Toggle PBC visualization
 *   T           - Toggle tooltips
 *   F           - Toggle depth cueing (fog)
 *   G           - Toggle glow effect
 *   ESC         - Quit
 */

#include "vis/renderer_classic.hpp"
#include "vis/animation.hpp"
#include "vis/pbc_visualizer.hpp"
#include "vis/ui_theme.hpp"
#include "vis/picking.hpp"
#include "vis/analysis_panel.hpp"
#include "core/math_vec3.hpp"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>

using namespace vsepr;
using namespace vsepr::render;

// ============================================================================
// XYZ File Parser
// ============================================================================

struct XYZData {
    std::vector<int> atomic_numbers;
    std::vector<Vec3> positions;
    std::string comment;
};

int element_symbol_to_Z(const std::string& symbol) {
    // Comprehensive element lookup
    static const std::unordered_map<std::string, int> elements = {
        {"H", 1}, {"He", 2}, {"Li", 3}, {"Be", 4}, {"B", 5}, {"C", 6},
        {"N", 7}, {"O", 8}, {"F", 9}, {"Ne", 10}, {"Na", 11}, {"Mg", 12},
        {"Al", 13}, {"Si", 14}, {"P", 15}, {"S", 16}, {"Cl", 17}, {"Ar", 18},
        {"K", 19}, {"Ca", 20}, {"Sc", 21}, {"Ti", 22}, {"V", 23}, {"Cr", 24},
        {"Mn", 25}, {"Fe", 26}, {"Co", 27}, {"Ni", 28}, {"Cu", 29}, {"Zn", 30},
        {"Ga", 31}, {"Ge", 32}, {"As", 33}, {"Se", 34}, {"Br", 35}, {"Kr", 36},
        {"Rb", 37}, {"Sr", 38}, {"Y", 39}, {"Zr", 40}, {"Nb", 41}, {"Mo", 42},
        {"Tc", 43}, {"Ru", 44}, {"Rh", 45}, {"Pd", 46}, {"Ag", 47}, {"Cd", 48},
        {"In", 49}, {"Sn", 50}, {"Sb", 51}, {"Te", 52}, {"I", 53}, {"Xe", 54}
    };
    
    auto it = elements.find(symbol);
    return (it != elements.end()) ? it->second : 0;
}

XYZData load_xyz(const std::string& filename) {
    XYZData data;
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        std::cerr << "Failed to open: " << filename << std::endl;
        return data;
    }
    
    int n_atoms;
    file >> n_atoms;
    file.ignore(1000, '\n');
    
    std::getline(file, data.comment);
    
    for (int i = 0; i < n_atoms; ++i) {
        std::string symbol;
        Vec3 pos;
        file >> symbol >> pos.x >> pos.y >> pos.z;
        
        data.atomic_numbers.push_back(element_symbol_to_Z(symbol));
        data.positions.push_back(pos);
    }
    
    std::cout << "Loaded " << n_atoms << " atoms from " << filename << std::endl;
    std::cout << "Comment: " << data.comment << std::endl;
    
    return data;
}

// ============================================================================
// Simple Camera (placeholder - uses fixed matrices for now)
// ============================================================================

struct SimpleCamera {
    Vec3 position = Vec3(0, 0, 15);
    Vec3 target = Vec3(0, 0, 0);
    Vec3 up = Vec3(0, 1, 0);
    
    float fov = 45.0f;
    float aspect = 16.0f / 9.0f;
    float near_plane = 0.1f;
    float far_plane = 100.0f;
    
    // Simplified view/projection matrices (4x4 identity for now)
    // In production, use proper GLM or meso::linalg matrices
    std::array<float, 16> get_view_matrix() const {
        // Simple lookAt approximation (forward = -Z)
        return {
            1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 1, 0,
            -position.x, -position.y, -position.z, 1
        };
    }
    
    std::array<float, 16> get_projection_matrix() const {
        // Simple perspective approximation
        float f = 1.0f / std::tan(fov * 0.5f * 3.14159f / 180.0f);
        float range = far_plane - near_plane;
        
        return {
            f / aspect, 0, 0, 0,
            0, f, 0, 0,
            0, 0, -(far_plane + near_plane) / range, -1,
            0, 0, -(2.0f * far_plane * near_plane) / range, 0
        };
    }
};

// ============================================================================
// Application State
// ============================================================================

struct AppState {
    std::unique_ptr<ClassicRenderer> renderer;
    AnimationController animator;
    PBCVisualizer pbc_vis;
    Windows11Theme ui_theme;
    MoleculePicker picker;
    AnalysisPanel analysis_panel;
    
    AtomicGeometry geometry;
    AtomicGeometry original_geometry;
    
    SimpleCamera camera;
    
    bool show_pbc = false;
    bool show_tooltips = true;
    bool show_depth_cue = true;
    bool show_glow = false;
    
    AnimationType current_animation = AnimationType::ROTATE_Y;
    RenderQuality current_quality = RenderQuality::MEDIUM;
    
    int window_width = 1280;
    int window_height = 720;
    
    double mouse_x = 0.0;
    double mouse_y = 0.0;
    
    float last_frame_time = 0.0f;
    
    // UI state
    bool show_info_panel = true;
    bool show_controls_panel = true;
};

AppState g_app;

// ============================================================================
// GLFW Callbacks
// ============================================================================

void cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
    g_app.mouse_x = xpos;
    g_app.mouse_y = ypos;
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action != GLFW_PRESS) return;
    
    switch (key) {
        case GLFW_KEY_ESCAPE:
            glfwSetWindowShouldClose(window, GLFW_TRUE);
            break;
        
        case GLFW_KEY_SPACE:
            g_app.animator.toggle_pause();
            std::cout << (g_app.animator.is_paused() ? "Paused" : "Playing") << std::endl;
            break;
        
        case GLFW_KEY_T:
            g_app.show_tooltips = !g_app.show_tooltips;
            std::cout << "Tooltips: " << (g_app.show_tooltips ? "ON" : "OFF") << std::endl;
            break;
        
        case GLFW_KEY_F:
            g_app.show_depth_cue = !g_app.show_depth_cue;
            std::cout << "Depth cueing: " << (g_app.show_depth_cue ? "ON" : "OFF") << std::endl;
            break;
        
        case GLFW_KEY_G:
            g_app.show_glow = !g_app.show_glow;
            std::cout << "Glow: " << (g_app.show_glow ? "ON" : "OFF") << std::endl;
            break;
        
        case GLFW_KEY_1:
            g_app.current_animation = AnimationType::NONE;
            g_app.animator.set_animation(AnimationType::NONE);
            std::cout << "Animation: NONE" << std::endl;
            break;
        
        case GLFW_KEY_2:
            g_app.current_animation = AnimationType::ROTATE_Y;
            g_app.animator.set_animation(AnimationType::ROTATE_Y);
            std::cout << "Animation: ROTATE_Y" << std::endl;
            break;
        
        case GLFW_KEY_3:
            g_app.current_animation = AnimationType::ROTATE_XYZ;
            g_app.animator.set_animation(AnimationType::ROTATE_XYZ);
            std::cout << "Animation: ROTATE_XYZ" << std::endl;
            break;
        
        case GLFW_KEY_4:
            g_app.current_animation = AnimationType::OSCILLATE;
            g_app.animator.set_animation(AnimationType::OSCILLATE);
            std::cout << "Animation: OSCILLATE" << std::endl;
            break;
        
        case GLFW_KEY_5:
            g_app.current_animation = AnimationType::ZOOM_PULSE;
            g_app.animator.set_animation(AnimationType::ZOOM_PULSE);
            std::cout << "Animation: ZOOM_PULSE" << std::endl;
            break;
        
        case GLFW_KEY_6:
            g_app.current_animation = AnimationType::ORBIT_CAMERA;
            g_app.animator.set_animation(AnimationType::ORBIT_CAMERA);
            std::cout << "Animation: ORBIT_CAMERA" << std::endl;
            break;
        
        case GLFW_KEY_Q:
            if (g_app.current_quality > RenderQuality::LOW) {
                g_app.current_quality = static_cast<RenderQuality>(
                    static_cast<int>(g_app.current_quality) - 1);
                g_app.renderer->set_quality(g_app.current_quality);
                std::cout << "Quality: " << static_cast<int>(g_app.current_quality) << std::endl;
            }
            break;
        
        case GLFW_KEY_W:
            if (g_app.current_quality < RenderQuality::ULTRA) {
                g_app.current_quality = static_cast<RenderQuality>(
                    static_cast<int>(g_app.current_quality) + 1);
                g_app.renderer->set_quality(g_app.current_quality);
                std::cout << "Quality: " << static_cast<int>(g_app.current_quality) << std::endl;
            }
            break;
        
        case GLFW_KEY_P:
            g_app.show_pbc = !g_app.show_pbc;
            std::cout << "PBC: " << (g_app.show_pbc ? "ON" : "OFF") << std::endl;
            break;
    }
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    g_app.window_width = width;
    g_app.window_height = height;
    g_app.camera.aspect = static_cast<float>(width) / static_cast<float>(height);
    glViewport(0, 0, width, height);
}

// ============================================================================
// ImGui UI Panels
// ============================================================================

void render_info_panel() {
    if (!g_app.show_info_panel) return;
    
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 200), ImGuiCond_FirstUseEver);
    
    if (ImGui::Begin("Molecule Info", &g_app.show_info_panel)) {
        g_app.ui_theme.section_header("Molecule");
        ImGui::Text("Atoms: %zu", g_app.geometry.positions.size());
        ImGui::Text("Bonds: %zu", g_app.geometry.bonds.size());
        
        g_app.ui_theme.separator();
        
        g_app.ui_theme.section_header("Rendering");
        ImGui::Text("Quality: %s", 
            g_app.current_quality == RenderQuality::LOW ? "Low" :
            g_app.current_quality == RenderQuality::MEDIUM ? "Medium" :
            g_app.current_quality == RenderQuality::HIGH ? "High" : "Ultra");
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        
        g_app.ui_theme.separator();
        
        g_app.ui_theme.section_header("Camera");
        ImGui::Text("Position: (%.1f, %.1f, %.1f)", 
            g_app.camera.position.x, 
            g_app.camera.position.y, 
            g_app.camera.position.z);
    }
    ImGui::End();
}

void render_controls_panel() {
    if (!g_app.show_controls_panel) return;
    
    ImGui::SetNextWindowPos(ImVec2(10, 230), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 350), ImGuiCond_FirstUseEver);
    
    if (ImGui::Begin("Controls", &g_app.show_controls_panel)) {
        g_app.ui_theme.section_header("Animation");
        
        const char* anim_names[] = {
            "None", "Rotate Y", "Rotate XYZ", "Oscillate", 
            "Trajectory", "Zoom Pulse", "Orbit Camera"
        };
        int current_anim = static_cast<int>(g_app.current_animation);
        if (ImGui::Combo("Type", &current_anim, anim_names, 7)) {
            g_app.current_animation = static_cast<AnimationType>(current_anim);
            g_app.animator.set_animation(g_app.current_animation);
        }
        
        bool is_paused = g_app.animator.is_paused();
        if (ImGui::Checkbox("Paused", &is_paused)) {
            if (is_paused) g_app.animator.pause();
            else g_app.animator.play();
        }
        
        g_app.ui_theme.separator();
        
        g_app.ui_theme.section_header("Visual Effects");
        ImGui::Checkbox("Depth Cueing (Fog)", &g_app.show_depth_cue);
        ImGui::Checkbox("Glow", &g_app.show_glow);
        ImGui::Checkbox("PBC Visualization", &g_app.show_pbc);
        ImGui::Checkbox("Tooltips", &g_app.show_tooltips);
        
        g_app.ui_theme.separator();
        
        g_app.ui_theme.section_header("Quality");
        const char* quality_names[] = {"Low", "Medium", "High", "Ultra"};
        int quality_idx = static_cast<int>(g_app.current_quality);
        if (ImGui::Combo("Render Quality", &quality_idx, quality_names, 4)) {
            g_app.current_quality = static_cast<RenderQuality>(quality_idx);
            g_app.renderer->set_quality(g_app.current_quality);
        }
        
        g_app.ui_theme.separator();
        
        g_app.ui_theme.section_header("Keyboard Shortcuts");
        ImGui::BulletText("SPACE - Play/Pause");
        ImGui::BulletText("1-6 - Animation type");
        ImGui::BulletText("Q/W - Quality down/up");
        ImGui::BulletText("T - Toggle tooltips");
        ImGui::BulletText("F - Toggle fog");
        ImGui::BulletText("G - Toggle glow");
        ImGui::BulletText("P - Toggle PBC");
    }
    ImGui::End();
}

// ============================================================================
// Main Application
// ============================================================================

int main(int argc, char* argv[]) {
    // Parse command line
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <molecule.xyz>" << std::endl;
        return 1;
    }
    
    std::string xyz_file = argv[1];
    
    // Load molecule
    auto xyz_data = load_xyz(xyz_file);
    if (xyz_data.atomic_numbers.empty()) {
        std::cerr << "Failed to load molecule" << std::endl;
        return 1;
    }
    
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return 1;
    }
    
    // Configure OpenGL 3.3 Core
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);  // 4x MSAA
    
    // Create window
    GLFWwindow* window = glfwCreateWindow(
        g_app.window_width, g_app.window_height,
        "Interactive Molecular Viewer - Windows 11 Style",
        nullptr, nullptr
    );
    
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return 1;
    }
    
    glfwMakeContextCurrent(window);
    glfwSetCursorPosCallback(window, cursor_position_callback);
    glfwSetKeyCallback(window, key_callback);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSwapInterval(1);  // VSync
    
    // Initialize GLEW
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW" << std::endl;
        return 1;
    }
    
    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    
    // Apply Windows 11 theme
    g_app.ui_theme.apply();
    
    // Initialize ImGui backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    
    // Setup OpenGL state
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.98f, 0.98f, 0.98f, 1.0f);  // Light gray background (Windows 11 style)
    
    // Create geometry
    g_app.geometry.positions = xyz_data.positions;
    g_app.geometry.atomic_numbers = xyz_data.atomic_numbers;
    g_app.original_geometry = g_app.geometry;
    
    // Create renderer
    g_app.renderer = std::make_unique<ClassicRenderer>();
    g_app.renderer->set_quality(g_app.current_quality);
    
    // Start animation
    g_app.animator.set_animation(g_app.current_animation);
    
    std::cout << "\n=== Interactive Molecular Viewer ===\n";
    std::cout << "Hover over atoms to see detailed element data\n";
    std::cout << "Hover over bonds to see bond lengths\n";
    std::cout << "Press T to toggle tooltips\n";
    std::cout << "Press SPACE to pause/play animation\n\n";
    
    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        
        // Update time
        float current_time = static_cast<float>(glfwGetTime());
        float dt = current_time - g_app.last_frame_time;
        g_app.last_frame_time = current_time;
        
        // Update animation
        g_app.geometry = g_app.original_geometry;
        g_app.animator.update(g_app.geometry, dt);
        
        // Update PBC visualization
        if (g_app.show_pbc) {
            g_app.geometry = g_app.pbc_vis.generate_replicas(g_app.original_geometry);
        }
        
        // Update mouse picking (if tooltips enabled)
        if (g_app.show_tooltips) {
            g_app.analysis_panel.update(
                g_app.geometry,
                static_cast<float>(g_app.mouse_x),
                static_cast<float>(g_app.mouse_y),
                g_app.window_width,
                g_app.window_height,
                g_app.camera.get_view_matrix().data(),
                g_app.camera.get_projection_matrix().data()
            );
        }
        
        // Render
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        // Render molecule (placeholder - actual rendering would go here)
        // g_app.renderer->render(g_app.geometry);
        
        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        // Render UI panels
        render_info_panel();
        render_controls_panel();
        
        // Render tooltips (if enabled and hovering)
        if (g_app.show_tooltips) {
            g_app.analysis_panel.render();
        }
        
        // Render ImGui
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        
        glfwSwapBuffers(window);
    }
    
    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    
    glfwDestroyWindow(window);
    glfwTerminate();
    
    return 0;
}
