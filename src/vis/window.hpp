#pragma once

#include "vis/renderer.hpp"
#include "vis/viz_router.hpp"
#include "core/frame_buffer.hpp"
#include "vis/ui_panels.hpp"
#include <string>
#include <functional>
#include <chrono>

struct GLFWwindow;

namespace vsepr {

// Forward declare
class SimulationThread;
class CommandRouter;

/**
 * Window manager for molecular visualization.
 * Handles GLFW window creation, event processing, and render loop.
 */
class Window {
public:
    Window(int width = 1280, int height = 720, const std::string& title = "VSEPR Simulator");
    ~Window();
    
    // Initialize window and OpenGL context
    bool initialize();
    
    // Shutdown ImGui and cleanup
    void shutdown();
    
    // Main render loop (blocks until window closes)
    void run(FrameBuffer& frame_buffer);
    
    // Main render loop with UI (old - without CommandRouter)
    void run_with_ui(SimulationThread& sim_thread);
    
    // Main render loop with UI and CommandRouter (new)
    void run_with_ui(SimulationThread& sim_thread, CommandRouter& command_router);
    
    // Request window to close
    void close();
    
    // Check if window should close
    bool should_close() const;
    
    // Single frame update (for manual control)
    void update(const FrameSnapshot& frame);
    
    // Get window size
    void get_size(int& width, int& height) const;
    
    // Camera access
    Camera& camera();
    
    // Visualization router access
    VizRouter& viz_router() { return viz_router_; }
    const VizRouter& viz_router() const { return viz_router_; }
    
    // Callbacks for external control
    using KeyCallback = std::function<void(int key, int action, int mods)>;
    void set_key_callback(KeyCallback callback) { key_callback_ = callback; }

private:
    // GLFW callbacks
    static void framebuffer_size_callback(GLFWwindow* window, int width, int height);
    static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
    static void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos);
    static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
    static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
    
    GLFWwindow* window_;
    Renderer renderer_;
    VizRouter viz_router_;
    UIManager ui_manager_;
    
    int width_;
    int height_;
    std::string title_;
    
    // Timing for fixed timestep + interpolation
    using Clock = std::chrono::steady_clock;
    using TimePoint = std::chrono::time_point<Clock>;
    TimePoint last_frame_time_;
    
    // Mouse state
    bool mouse_left_down_;
    bool mouse_right_down_;
    double last_mouse_x_;
    double last_mouse_y_;
    
    KeyCallback key_callback_;
};

} // namespace vsepr
