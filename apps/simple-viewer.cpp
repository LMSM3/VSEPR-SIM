/**
 * Simple Molecular Viewer - Example Application
 * 
 * Demonstrates the Ballstick renderer with animations and PBC visualization.
 * 
 * Features:
 * - Load XYZ files
 * - Rotate molecule
 * - PBC visualization (crystals)
 * - Animation controls
 * - Quality settings
 * - File watching (--watch mode)
 * 
 * Usage:
 *   simple-viewer molecule.xyz [options]
 * 
 * Options:
 *   --watch    Auto-reload file on changes (for live simulations)
 * 
 * Controls:
 *   SPACE - Play/pause animation
 *   1-6   - Change animation type
 *   Q/W   - Decrease/increase quality
 *   P     - Toggle PBC visualization
 *   R     - Reload file manually
 *   ESC   - Quit
 */

#include "vis/renderer_classic.hpp"
#include "vis/animation.hpp"
#include "vis/pbc_visualizer.hpp"
#include "core/math_vec3.hpp"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <sys/stat.h>

using namespace vsepr;
using namespace vsepr::render;

// ============================================================================
// File Watching
// ============================================================================

time_t get_file_mod_time(const std::string& path) {
    struct stat result;
    if (stat(path.c_str(), &result) == 0) {
        return result.st_mtime;
    }
    return 0;
}

// ============================================================================
// XYZ File Parser
// ============================================================================

struct XYZData {
    std::vector<int> atomic_numbers;
    std::vector<Vec3> positions;
    std::string comment;
};

int element_symbol_to_Z(const std::string& symbol) {
    // Simple lookup for common elements
    if (symbol == "H") return 1;
    if (symbol == "C") return 6;
    if (symbol == "N") return 7;
    if (symbol == "O") return 8;
    if (symbol == "F") return 9;
    if (symbol == "P") return 15;
    if (symbol == "S") return 16;
    if (symbol == "Cl") return 17;
    if (symbol == "Fe") return 26;
    if (symbol == "Cu") return 29;
    if (symbol == "Zn") return 30;
    if (symbol == "Br") return 35;
    if (symbol == "I") return 53;
    return 0;  // Unknown
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
// Application State
// ============================================================================

struct AppState {
    std::unique_ptr<ClassicRenderer> renderer;
    AnimationController animator;
    PBCVisualizer pbc_vis;

    AtomicGeometry geometry;
    AtomicGeometry original_geometry;  // For animation reset

    bool show_pbc = false;
    AnimationType current_animation = AnimationType::ROTATE_Y;
    RenderQuality current_quality = RenderQuality::MEDIUM;

    int window_width = 1280;
    int window_height = 720;

    float last_frame_time = 0.0f;

    // Watch mode
    bool watch_mode = false;
    std::string xyz_file;
    time_t last_file_time = 0;
    float last_check_time = 0.0f;
    const float check_interval = 0.1f;  // Check every 100ms

    void reload_file() {
        XYZData xyz = load_xyz(xyz_file);
        if (!xyz.atomic_numbers.empty()) {
            geometry = AtomicGeometry::from_xyz(xyz.atomic_numbers, xyz.positions);
            original_geometry = geometry;
            std::cout << "Reloaded: " << xyz_file << " (" << xyz.atomic_numbers.size() << " atoms)\n";
        }
    }
};

AppState g_app;

// ============================================================================
// GLFW Callbacks
// ============================================================================

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
            std::cout << "Animation: ROTATE_XYZ (tumble)" << std::endl;
            break;
        
        case GLFW_KEY_4:
            g_app.current_animation = AnimationType::OSCILLATE;
            g_app.animator.set_animation(AnimationType::OSCILLATE);
            std::cout << "Animation: OSCILLATE" << std::endl;
            break;
        
        case GLFW_KEY_Q:
            // Decrease quality
            if (g_app.current_quality == RenderQuality::ULTRA)
                g_app.current_quality = RenderQuality::HIGH;
            else if (g_app.current_quality == RenderQuality::HIGH)
                g_app.current_quality = RenderQuality::MEDIUM;
            else if (g_app.current_quality == RenderQuality::MEDIUM)
                g_app.current_quality = RenderQuality::LOW;
            
            g_app.renderer->set_quality(g_app.current_quality);
            std::cout << "Quality decreased" << std::endl;
            break;
        
        case GLFW_KEY_W:
            // Increase quality
            if (g_app.current_quality == RenderQuality::LOW)
                g_app.current_quality = RenderQuality::MEDIUM;
            else if (g_app.current_quality == RenderQuality::MEDIUM)
                g_app.current_quality = RenderQuality::HIGH;
            else if (g_app.current_quality == RenderQuality::HIGH)
                g_app.current_quality = RenderQuality::ULTRA;
            
            g_app.renderer->set_quality(g_app.current_quality);
            std::cout << "Quality increased" << std::endl;
            break;
        
        case GLFW_KEY_P:
            g_app.show_pbc = !g_app.show_pbc;
            g_app.pbc_vis.set_enabled(g_app.show_pbc);
            std::cout << "PBC: " << (g_app.show_pbc ? "ON" : "OFF") << std::endl;
            break;

        case GLFW_KEY_R:
            // Manual reload
            g_app.reload_file();
            break;
    }
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    g_app.window_width = width;
    g_app.window_height = height;
    glViewport(0, 0, width, height);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " molecule.xyz [--watch]" << std::endl;
        std::cerr << "\nOptions:\n";
        std::cerr << "  --watch    Auto-reload file on changes (for live simulations)\n";
        return 1;
    }

    std::string xyz_file = argv[1];
    bool watch_mode = false;

    // Parse options
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--watch") {
            watch_mode = true;
        }
    }

    // Store in global state
    g_app.xyz_file = xyz_file;
    g_app.watch_mode = watch_mode;

    if (watch_mode) {
        std::cout << "Watch mode ENABLED - will auto-reload on file changes\n";
        g_app.last_file_time = get_file_mod_time(xyz_file);
    }

    // Load XYZ file
    XYZData xyz = load_xyz(xyz_file);
    if (xyz.atomic_numbers.empty()) {
        std::cerr << "Failed to load molecule!" << std::endl;
        return 1;
    }
    
    // Create geometry
    g_app.geometry = AtomicGeometry::from_xyz(xyz.atomic_numbers, xyz.positions);
    g_app.original_geometry = g_app.geometry;
    
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return 1;
    }
    
    // Create window
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    GLFWwindow* window = glfwCreateWindow(
        g_app.window_width, g_app.window_height,
        "Simple Molecular Viewer", nullptr, nullptr);
    
    if (!window) {
        std::cerr << "Failed to create window" << std::endl;
        glfwTerminate();
        return 1;
    }
    
    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, key_callback);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSwapInterval(1);  // VSync
    
    // Initialize renderer
    g_app.renderer = std::make_unique<ClassicRenderer>();
    if (!g_app.renderer->initialize()) {
        std::cerr << "Failed to initialize renderer" << std::endl;
        return 1;
    }
    
    g_app.renderer->set_quality(RenderQuality::MEDIUM);
    g_app.renderer->set_background_color(0.1f, 0.1f, 0.15f);  // Dark blue
    
    // Setup animation
    g_app.animator.set_animation(AnimationType::ROTATE_Y);
    g_app.animator.set_rotation_speed(0.5f);  // Slow rotation
    
    // Setup PBC (disabled by default)
    g_app.pbc_vis.set_replication(1, 1, 1);  // 3x3x3 grid
    g_app.pbc_vis.set_ghost_opacity(0.3f);
    
    std::cout << "\n=== Controls ===\n";
    std::cout << "SPACE  - Play/pause\n";
    std::cout << "1      - No animation\n";
    std::cout << "2      - Rotate Y\n";
    std::cout << "3      - Tumble (XYZ)\n";
    std::cout << "4      - Oscillate\n";
    std::cout << "Q/W    - Quality down/up\n";
    std::cout << "P      - Toggle PBC\n";
    std::cout << "R      - Reload file\n";
    std::cout << "ESC    - Quit\n";

    if (watch_mode) {
        std::cout << "\n[WATCH MODE ACTIVE - Auto-reloading every " << g_app.check_interval * 1000 << "ms]\n";
    }
    std::cout << "\n";
    
    // Main loop
    g_app.last_frame_time = static_cast<float>(glfwGetTime());
    
    while (!glfwWindowShouldClose(window)) {
        float current_time = static_cast<float>(glfwGetTime());
        float dt = current_time - g_app.last_frame_time;
        g_app.last_frame_time = current_time;

        // Watch mode: Check for file changes
        if (g_app.watch_mode && (current_time - g_app.last_check_time) > g_app.check_interval) {
            g_app.last_check_time = current_time;

            time_t current_file_time = get_file_mod_time(g_app.xyz_file);
            if (current_file_time != g_app.last_file_time && current_file_time > 0) {
                g_app.last_file_time = current_file_time;
                g_app.reload_file();
            }
        }

        // Update animation
        g_app.animator.update(dt, g_app.geometry);
        
        // Apply PBC if enabled
        if (g_app.show_pbc) {
            AtomicGeometry pbc_geom = g_app.pbc_vis.generate_replicas(g_app.geometry);
            // TODO: Render pbc_geom instead of g_app.geometry
        }
        
        // Render
        // TODO: Need Camera class implementation
        // For now, renderer will use placeholder identity matrix
        // g_app.renderer->render(g_app.geometry, camera, 
        //                       g_app.window_width, g_app.window_height);
        
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    
    glfwTerminate();
    return 0;
}
