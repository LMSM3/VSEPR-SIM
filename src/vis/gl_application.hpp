#pragma once
/**
 * gl_application.hpp
 * Main application framework integrating all OpenGL systems
 */

#include "gl_context.hpp"
#include "gl_renderer.hpp"
#include "gl_camera.hpp"
#include "gl_shader.hpp"
#include "gl_material.hpp"
#include <memory>
#include <string>
#include <functional>

namespace vsepr {
namespace vis {

// ============================================================================
// Application Configuration
// ============================================================================

struct ApplicationConfig {
    int window_width = 1280;
    int window_height = 720;
    std::string window_title = "VSEPR-Sim OpenGL Viewer";
    bool enable_vsync = true;
    bool enable_msaa = true;
    int msaa_samples = 4;
    bool enable_hdri = false;
    float target_fps = 60.0f;
};

// ============================================================================
// Application Base
// ============================================================================

class Application {
public:
    Application(const ApplicationConfig& config = {});
    virtual ~Application();
    
    /**
     * Initialize and run application
     */
    int run();
    
    /**
     * Request application exit
     */
    void exit(int code = 0);
    
    /**
     * Get configuration
     */
    const ApplicationConfig& get_config() const { return config_; }
    
    /**
     * Get subsystems
     */
    GLContext* get_context() { return context_.get(); }
    Renderer* get_renderer() { return &renderer_; }
    Camera* get_camera() { return &camera_; }
    CameraController* get_camera_controller() { return &camera_controller_; }
    Scene* get_scene() { return &scene_; }
    
    /**
     * Window size callback
     */
    void on_window_resized(int width, int height);
    
    /**
     * Mouse callbacks
     */
    void on_mouse_move(float x, float y);
    void on_mouse_button(int button, bool pressed);
    void on_mouse_wheel(float delta);
    
    /**
     * Keyboard callback
     */
    void on_key(int key, bool pressed);
    
protected:
    /**
     * Virtual methods to override in subclasses
     */
    
    /**
     * Called once on startup
     */
    virtual void on_initialize() {}
    
    /**
     * Called each frame for update logic
     */
    virtual void on_update(float delta_time) {}
    
    /**
     * Called for rendering (after engine render)
     */
    virtual void on_render() {}
    
    /**
     * Called on shutdown
     */
    virtual void on_shutdown() {}
    
    /**
     * Setup default scene content
     */
    virtual void setup_scene();
    
private:
    ApplicationConfig config_;
    std::unique_ptr<GLContext> context_;
    
    Renderer renderer_;
    Camera camera_;
    CameraController camera_controller_;
    Scene scene_;
    
    bool running_ = false;
    int exit_code_ = 0;
    
    double last_frame_time_ = 0;
    double frame_time_accumulator_ = 0;
    int frame_count_ = 0;
    
    /**
     * Main loop
     */
    void main_loop();
    
    /**
     * Update frame statistics
     */
    void update_stats();
};

// ============================================================================
// Molecule Viewer Application
// ============================================================================

class MoleculeViewerApp : public Application {
public:
    MoleculeViewerApp(const ApplicationConfig& config = {});
    
    /**
     * Load molecule from XYZ file
     */
    bool load_molecule(const std::string& filepath);
    
    /**
     * Load molecule from structure
     */
    void set_molecule(const std::vector<glm::vec3>& positions,
                     const std::vector<uint8_t>& atomic_numbers,
                     const std::vector<std::pair<int, int>>& bonds = {});
    
protected:
    void on_initialize() override;
    void on_update(float delta_time) override;
    void on_render() override;
    
private:
    std::shared_ptr<Entity> molecule_entity_;
    
    void create_molecule_mesh(const std::vector<glm::vec3>& positions,
                            const std::vector<uint8_t>& atomic_numbers,
                            const std::vector<std::pair<int, int>>& bonds);
};

// ============================================================================
// FEA Visualization Application
// ============================================================================

class FEAViewerApp : public Application {
public:
    FEAViewerApp(const ApplicationConfig& config = {});
    
    /**
     * Load FEA mesh
     */
    bool load_mesh(const std::string& filepath);
    
    /**
     * Visualize stress/strain results
     */
    void set_result_field(const std::vector<float>& values, 
                         const std::string& field_name);
    
protected:
    void on_initialize() override;
    void on_update(float delta_time) override;
    void on_render() override;
    
private:
    std::shared_ptr<Entity> mesh_entity_;
    std::shared_ptr<Mesh> fea_mesh_;
};

} // namespace vis
} // namespace vsepr
