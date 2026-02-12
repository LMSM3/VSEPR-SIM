#include "vis/window.hpp"
#include "sim/sim_thread.hpp"
#include "command_router.hpp"
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <iostream>

namespace vsepr {

// GLFW error callback
static void glfw_error_callback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << "\n";
}

Window::Window(int width, int height, const std::string& title)
    : window_(nullptr)
    , width_(width)
    , height_(height)
    , title_(title)
    , mouse_left_down_(false)
    , mouse_right_down_(false)
    , last_mouse_x_(0.0)
    , last_mouse_y_(0.0)
{}

Window::~Window() {
    shutdown();
    if (window_) {
        glfwDestroyWindow(window_);
    }
    glfwTerminate();
}

void Window::shutdown() {
    // Cleanup ImGui
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

bool Window::initialize() {
    // Set error callback before init
    glfwSetErrorCallback(glfw_error_callback);
    
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW (check error messages above)\n";
        return false;
    }
    
    std::cout << "GLFW Version: " << glfwGetVersionString() << "\n";
    
    // Set OpenGL version (3.3 core profile)
    // For better WSLg compatibility, try 3.2 or even 2.1
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    
    // Create window
    window_ = glfwCreateWindow(width_, height_, title_.c_str(), nullptr, nullptr);
    if (!window_) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return false;
    }
    
    glfwMakeContextCurrent(window_);
    glfwSetWindowUserPointer(window_, this);
    
    // Set callbacks
    glfwSetFramebufferSizeCallback(window_, framebuffer_size_callback);
    glfwSetMouseButtonCallback(window_, mouse_button_callback);
    glfwSetCursorPosCallback(window_, cursor_pos_callback);
    glfwSetScrollCallback(window_, scroll_callback);
    glfwSetKeyCallback(window_, key_callback);
    
    // Initialize renderer
    if (!renderer_.initialize()) {
        std::cerr << "Failed to initialize renderer\n";
        return false;
    }
    
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    
    // Setup ImGui style
    ImGui::StyleColorsDark();
    
    // Setup Platform/Renderer backends
    const char* glsl_version = "#version 150";  // OpenGL 3.2+
    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init(glsl_version);
    
    return true;
}

void Window::run(FrameBuffer& frame_buffer) {
    while (!glfwWindowShouldClose(window_)) {
        // Poll events
        glfwPollEvents();
        
        // Get latest frame from simulation
        FrameSnapshot frame = frame_buffer.read();
        
        // Render
        int width, height;
        glfwGetFramebufferSize(window_, &width, &height);
        renderer_.render(frame, width, height);
        
        // Swap buffers
        glfwSwapBuffers(window_);
    }
}

void Window::run_with_ui(SimulationThread& sim_thread) {
    while (!glfwWindowShouldClose(window_)) {
        // Poll and handle events
        glfwPollEvents();
        
        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        // Render UI panels
        ui_manager_.render(sim_thread);
        
        // Get latest frame from simulation
        FrameSnapshot frame = sim_thread.get_latest_frame();
        
        // Render 3D scene
        int width, height;
        glfwGetFramebufferSize(window_, &width, &height);
        renderer_.render(frame, width, height);
        
        // Render ImGui on top
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        
        // Swap buffers
        glfwSwapBuffers(window_);
    }
}

void Window::run_with_ui(SimulationThread& sim_thread, CommandRouter& command_router) {
    while (!glfwWindowShouldClose(window_)) {
        // Poll and handle events
        glfwPollEvents();
        
        // Process pending results from SimThread
        command_router.process_results();
        
        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        // Render UI panels with command router
        ui_manager_.render(sim_thread, command_router);
        
        // Get latest frame from simulation
        FrameSnapshot frame = sim_thread.get_latest_frame();
        
        // Render 3D scene
        int width, height;
        glfwGetFramebufferSize(window_, &width, &height);
        renderer_.render(frame, width, height);
        
        // Render ImGui on top
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        
        // Swap buffers
        glfwSwapBuffers(window_);
    }
}

void Window::close() {
    if (window_) {
        glfwSetWindowShouldClose(window_, GLFW_TRUE);
    }
}

bool Window::should_close() const {
    return window_ && glfwWindowShouldClose(window_);
}

void Window::update(const FrameSnapshot& frame) {
    glfwPollEvents();
    
    int width, height;
    glfwGetFramebufferSize(window_, &width, &height);
    renderer_.render(frame, width, height);
    
    glfwSwapBuffers(window_);
}

void Window::get_size(int& width, int& height) const {
    glfwGetFramebufferSize(window_, &width, &height);
}

Camera& Window::camera() {
    return renderer_.camera();
}

// ============================================================================
// GLFW Callbacks
// ============================================================================

void Window::framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    (void)window;
    (void)width;
    (void)height;
    // Viewport will be set in render() call
}

void Window::mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    (void)mods;
    Window* win = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (!win) return;
    
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        win->mouse_left_down_ = (action == GLFW_PRESS);
    } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        win->mouse_right_down_ = (action == GLFW_PRESS);
    }
}

void Window::cursor_pos_callback(GLFWwindow* window, double xpos, double ypos) {
    Window* win = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (!win) return;
    
    double dx = xpos - win->last_mouse_x_;
    double dy = ypos - win->last_mouse_y_;
    
    if (win->mouse_left_down_) {
        win->renderer_.camera().orbit(dx, dy);
    } else if (win->mouse_right_down_) {
        win->renderer_.camera().pan(dx, dy);
    }
    
    win->last_mouse_x_ = xpos;
    win->last_mouse_y_ = ypos;
}

void Window::scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    (void)xoffset;
    Window* win = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (!win) return;
    
    win->renderer_.camera().zoom(yoffset);
}

void Window::key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    (void)scancode;
    Window* win = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (!win) return;
    
    // Handle built-in keys
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
    
    if (key == GLFW_KEY_R && action == GLFW_PRESS) {
        win->renderer_.camera().reset();
    }
    
    // Call user callback if set
    if (win->key_callback_) {
        win->key_callback_(key, action, mods);
    }
}

} // namespace vsepr
