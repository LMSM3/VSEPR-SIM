/**
 * Scalable Rendering Demo
 * Demonstrates LOD + culling for infinite molecule generation
 * 
 * Features:
 * - Continuous generation in background
 * - Only render molecules near camera (local sampling)
 * - Dynamic LOD based on distance
 * - Debug visualization of culling/LOD
 * 
 * Controls:
 * - WASD: Move camera
 * - Mouse: Look around
 * - Space: Toggle generation
 * - Tab: Toggle debug view
 */

#include "render/scalable_renderer.hpp"
#include "gui/continuous_generation_manager.hpp"
#include "dynamic/real_molecule_generator.hpp"
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <random>

using namespace vsepr;

// ============================================================================
// Camera Controller
// ============================================================================

class Camera {
public:
    glm::vec3 position{0.0f, 0.0f, 50.0f};
    glm::vec3 forward{0.0f, 0.0f, -1.0f};
    glm::vec3 up{0.0f, 1.0f, 0.0f};
    float yaw = -90.0f;
    float pitch = 0.0f;
    float speed = 10.0f;
    
    glm::mat4 get_view_matrix() const {
        return glm::lookAt(position, position + forward, up);
    }
    
    void update(float dt, GLFWwindow* window) {
        // Movement
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            position += forward * speed * dt;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            position -= forward * speed * dt;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            position -= glm::normalize(glm::cross(forward, up)) * speed * dt;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            position += glm::normalize(glm::cross(forward, up)) * speed * dt;
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
            position += up * speed * dt;
        if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
            position -= up * speed * dt;
    }
    
    void update_rotation(float dx, float dy) {
        yaw += dx * 0.1f;
        pitch += dy * 0.1f;
        
        pitch = glm::clamp(pitch, -89.0f, 89.0f);
        
        forward.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        forward.y = sin(glm::radians(pitch));
        forward.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        forward = glm::normalize(forward);
    }
};

// ============================================================================
// Spatial Distribution Strategies
// ============================================================================

class MoleculeDistributor {
public:
    enum class DistributionMode {
        Random3D,      // Random positions in 3D space
        Grid,          // Regular grid
        Spiral,        // Spiral pattern
        Sphere,        // Distribute on sphere surface
        Wave           // Sine wave pattern
    };
    
    glm::vec3 get_next_position(DistributionMode mode, size_t index) {
        switch (mode) {
            case DistributionMode::Random3D:
                return random_3d(index);
            case DistributionMode::Grid:
                return grid(index);
            case DistributionMode::Spiral:
                return spiral(index);
            case DistributionMode::Sphere:
                return sphere(index);
            case DistributionMode::Wave:
                return wave(index);
            default:
                return glm::vec3(0.0f);
        }
    }
    
private:
    std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<float> dist{-100.0f, 100.0f};
    
    glm::vec3 random_3d(size_t) {
        return glm::vec3(dist(rng), dist(rng), dist(rng));
    }
    
    glm::vec3 grid(size_t index) {
        int grid_size = 20;
        int x = index % grid_size;
        int y = (index / grid_size) % grid_size;
        int z = index / (grid_size * grid_size);
        
        float spacing = 10.0f;
        return glm::vec3(
            (x - grid_size / 2) * spacing,
            (y - grid_size / 2) * spacing,
            (z - grid_size / 2) * spacing
        );
    }
    
    glm::vec3 spiral(size_t index) {
        float t = index * 0.1f;
        float radius = 5.0f + t * 0.5f;
        return glm::vec3(
            radius * cos(t),
            t * 2.0f,
            radius * sin(t)
        );
    }
    
    glm::vec3 sphere(size_t index) {
        float phi = acos(1.0f - 2.0f * (index % 1000) / 1000.0f);
        float theta = 2.0f * M_PI * (index % 137) / 137.0f;
        
        float radius = 50.0f;
        return glm::vec3(
            radius * sin(phi) * cos(theta),
            radius * sin(phi) * sin(theta),
            radius * cos(phi)
        );
    }
    
    glm::vec3 wave(size_t index) {
        float x = (index % 100) * 2.0f;
        float z = (index / 100) * 2.0f;
        float y = 10.0f * sin(x * 0.2f) * cos(z * 0.2f);
        return glm::vec3(x - 100.0f, y, z - 100.0f);
    }
};

// ============================================================================
// Main Application
// ============================================================================

int main() {
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }
    
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
    
    GLFWwindow* window = glfwCreateWindow(1920, 1080, "Scalable Molecular Visualization", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }
    
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);  // VSync
    
    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    ImGui::StyleColorsDark();
    
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    
    // Initialize systems
    render::ScalableMoleculeRenderer renderer;
    renderer.set_lod_distances(15.0f, 50.0f, 150.0f);
    renderer.set_max_render_count(10000);
    renderer.set_frustum_culling(true);
    
    render::StreamingMoleculeManager molecule_manager;
    MoleculeDistributor distributor;
    
    // Continuous generation setup
    ContinuousGenerationManager cont_gen;
    RealMoleculeGenerator::GenerationConfig config;
    config.category = MoleculeCategory::Mixed;
    config.min_atoms = 3;
    config.max_atoms = 20;
    
    Camera camera;
    
    // State
    bool generation_running = false;
    bool debug_view = false;
    float last_frame_time = 0.0f;
    size_t total_generated = 0;
    MoleculeDistributor::DistributionMode distribution_mode = MoleculeDistributor::DistributionMode::Random3D;
    float local_sample_radius = 100.0f;
    
    // Mouse input
    double last_mouse_x, last_mouse_y;
    glfwGetCursorPos(window, &last_mouse_x, &last_mouse_y);
    bool first_mouse = true;
    
    // Main loop
    while (!glfwWindowShouldClose(window)) {
        float current_time = glfwGetTime();
        float dt = current_time - last_frame_time;
        last_frame_time = current_time;
        
        glfwPollEvents();
        
        // Mouse input
        double mouse_x, mouse_y;
        glfwGetCursorPos(window, &mouse_x, &mouse_y);
        
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
            if (!first_mouse) {
                float dx = mouse_x - last_mouse_x;
                float dy = last_mouse_y - mouse_y;
                camera.update_rotation(dx, dy);
            }
            first_mouse = false;
        } else {
            first_mouse = true;
        }
        
        last_mouse_x = mouse_x;
        last_mouse_y = mouse_y;
        
        // Camera update
        camera.update(dt, window);
        
        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        // ====================================================================
        // GUI Controls
        // ====================================================================
        
        ImGui::Begin("Scalable Rendering Control", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        
        ImGui::Text("Press Tab for debug view");
        ImGui::Text("Right-click + drag to rotate camera");
        ImGui::Text("WASD to move");
        ImGui::Separator();
        
        // Generation controls
        ImGui::Text("Continuous Generation");
        if (ImGui::Button(generation_running ? "Stop Generation" : "Start Generation")) {
            generation_running = !generation_running;
            if (generation_running) {
                cont_gen.start(config);
            } else {
                cont_gen.stop();
            }
        }
        
        ImGui::SameLine();
        ImGui::Text("Total: %zu molecules", total_generated);
        
        // Distribution mode
        const char* dist_modes[] = {"Random 3D", "Grid", "Spiral", "Sphere Surface", "Wave"};
        int current_mode = static_cast<int>(distribution_mode);
        if (ImGui::Combo("Distribution", &current_mode, dist_modes, 5)) {
            distribution_mode = static_cast<MoleculeDistributor::DistributionMode>(current_mode);
        }
        
        ImGui::Separator();
        
        // LOD settings
        ImGui::Text("Level of Detail Settings");
        float lod_distances[3] = {15.0f, 50.0f, 150.0f};
        if (ImGui::SliderFloat("Full Detail Range", &lod_distances[0], 5.0f, 50.0f)) {
            renderer.set_lod_distances(lod_distances[0], lod_distances[1], lod_distances[2]);
        }
        if (ImGui::SliderFloat("Simplified Range", &lod_distances[1], 20.0f, 100.0f)) {
            renderer.set_lod_distances(lod_distances[0], lod_distances[1], lod_distances[2]);
        }
        if (ImGui::SliderFloat("Impostor Range", &lod_distances[2], 50.0f, 300.0f)) {
            renderer.set_lod_distances(lod_distances[0], lod_distances[1], lod_distances[2]);
        }
        
        ImGui::Separator();
        
        // Local sampling
        ImGui::Text("Local Sampling");
        ImGui::SliderFloat("Sample Radius", &local_sample_radius, 50.0f, 500.0f);
        ImGui::Text("Only molecules within %.0f units of camera are kept", local_sample_radius);
        
        ImGui::Separator();
        
        // Statistics
        auto stats = renderer.get_stats();
        ImGui::Text("Rendering Statistics");
        ImGui::Text("Total in Scene: %zu", stats.total_molecules);
        ImGui::Text("Rendered: %zu (%.1f%%)", stats.rendered_molecules,
                   stats.total_molecules > 0 ? 100.0f * stats.rendered_molecules / stats.total_molecules : 0.0f);
        ImGui::Text("  Full Detail: %zu", stats.full_detail_count);
        ImGui::Text("  Simplified: %zu", stats.simplified_count);
        ImGui::Text("  Impostors: %zu", stats.impostor_count);
        ImGui::Text("  Culled: %zu", stats.culled_count);
        ImGui::Text("Render Time: %.2f ms (%.1f FPS)", stats.render_time_ms, 1000.0f / stats.render_time_ms);
        ImGui::Text("Culling Time: %.2f ms", stats.culling_time_ms);
        
        ImGui::Separator();
        
        // Camera info
        ImGui::Text("Camera Position: (%.1f, %.1f, %.1f)", 
                   camera.position.x, camera.position.y, camera.position.z);
        ImGui::SliderFloat("Camera Speed", &camera.speed, 1.0f, 100.0f);
        
        ImGui::Checkbox("Debug View", &debug_view);
        
        ImGui::End();
        
        // ====================================================================
        // Update Logic
        // ====================================================================
        
        // Poll continuous generation
        if (generation_running) {
            auto recent = cont_gen.get_recent_molecules(100);
            for (const auto& mol : recent) {
                // Check if we already added this one
                if (mol.num_atoms() > 0 && total_generated < cont_gen.get_total_generated()) {
                    glm::vec3 position = distributor.get_next_position(distribution_mode, total_generated);
                    molecule_manager.add_molecule(mol, position);
                    total_generated++;
                }
            }
        }
        
        // Remove distant molecules (local sampling)
        molecule_manager.remove_distant_molecules(camera.position, local_sample_radius);
        
        // Get local instances and build octree
        auto local_instances = molecule_manager.get_local_instances(camera.position, local_sample_radius);
        if (!local_instances.empty()) {
            renderer.build_octree(local_instances, 6);
        }
        
        // ====================================================================
        // Rendering
        // ====================================================================
        
        glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        
        // Setup matrices
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        
        glm::mat4 projection = glm::perspective(glm::radians(60.0f), 
                                               (float)width / (float)height, 
                                               0.1f, 1000.0f);
        glm::mat4 view = camera.get_view_matrix();
        
        // Render molecules with LOD
        renderer.render(view, projection, camera.position);
        
        // Debug visualization
        if (debug_view) {
            renderer.render_debug(projection * view);
        }
        
        // Render ImGui
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        
        glfwSwapBuffers(window);
        
        // Toggle debug with Tab
        if (glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS) {
            static bool tab_was_pressed = false;
            if (!tab_was_pressed) {
                debug_view = !debug_view;
                tab_was_pressed = true;
            }
        } else {
            static bool tab_was_pressed = false;
            tab_was_pressed = false;
        }
    }
    
    // Cleanup
    cont_gen.stop();
    
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    
    glfwDestroyWindow(window);
    glfwTerminate();
    
    return 0;
}
