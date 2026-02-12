#pragma once
/**
 * gl_context.hpp
 * Modern OpenGL 3.3+ context management
 * GLFW3 window, GLEW extensions, core context
 */

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <iostream>

namespace vsepr {
namespace vis {

// ============================================================================
// OpenGL Core Context
// ============================================================================

class GLContext {
public:
    /**
     * Initialize OpenGL context with GLFW window
     * @param width - Window width in pixels
     * @param height - Window height in pixels
     * @param title - Window title
     */
    bool initialize(int width, int height, const std::string& title);
    
    /**
     * Shutdown and clean up context
     */
    void shutdown();
    
    /**
     * Check if context is valid and window should stay open
     */
    bool is_active() const;
    
    /**
     * Swap buffers (present frame to display)
     */
    void swap_buffers();
    
    /**
     * Poll input events
     */
    void poll_events();
    
    /**
     * Set window vsync
     */
    void set_vsync(bool enabled);
    
    /**
     * Get window dimensions
     */
    void get_size(int& width, int& height) const;
    
    /**
     * Set viewport
     */
    void set_viewport(int x, int y, int width, int height);
    
    /**
     * Clear color and depth buffers
     */
    void clear(glm::vec4 color = {0.1f, 0.1f, 0.1f, 1.0f});
    
    /**
     * Get raw GLFW window pointer (for advanced use)
     */
    GLFWwindow* get_window() { return window_; }
    
    /**
     * Get OpenGL version info
     */
    static std::string get_version_info();
    
    /**
     * Check for OpenGL errors
     */
    static bool check_errors(const std::string& context = "");
    
private:
    GLFWwindow* window_ = nullptr;
    int width_ = 0;
    int height_ = 0;
    bool initialized_ = false;
    
    static void error_callback_(int error, const char* description);
};

// ============================================================================
// RAII Context Manager
// ============================================================================

class GLContextGuard {
public:
    GLContextGuard(int width, int height, const std::string& title) {
        context_ = std::make_unique<GLContext>();
        if (!context_->initialize(width, height, title)) {
            context_.reset();
        }
    }
    
    ~GLContextGuard() {
        if (context_) {
            context_->shutdown();
        }
    }
    
    GLContext* operator->() { return context_.get(); }
    GLContext* get() { return context_.get(); }
    bool is_valid() const { return context_ != nullptr; }
    
private:
    std::unique_ptr<GLContext> context_;
};

// ============================================================================
// Global Context Access (for callbacks)
// ============================================================================

GLContext* get_current_context();
void set_current_context(GLContext* ctx);

} // namespace vis
} // namespace vsepr
