#include "vis/window.hpp"
#include "../sim/sim_thread.hpp"
#include "command_router.hpp"
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <iostream>
#include <chrono>

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
    , last_frame_time_(Clock::now())
    , mouse_left_down_(false)
    , mouse_right_down_(false)
    , last_mouse_x_(0.0)
    , last_mouse_y_(0.0)
{
    // Initialize viz router with default cartoon mode
    VizConfig config;
    config.apply_mode_preset(VizMode::CARTOON);
    viz_router_.init(config);
}

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
    
    // Setup ImGui style - Custom Green GPU Theme
    ImGui::StyleColorsDark();
    
    // Apply green GPU-accelerated theme
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;
    
    // Green accent colors for GPU theme
    colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.12f, 0.09f, 0.94f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.05f, 0.08f, 0.06f, 1.00f);
    colors[ImGuiCol_Border] = ImVec4(0.15f, 0.65f, 0.25f, 0.50f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.30f, 0.10f, 0.00f);
    
    // Title bars and headers - bright green
    colors[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.30f, 0.15f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.15f, 0.55f, 0.25f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.08f, 0.20f, 0.12f, 0.75f);
    colors[ImGuiCol_Header] = ImVec4(0.15f, 0.50f, 0.25f, 0.80f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.20f, 0.65f, 0.30f, 0.80f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.25f, 0.75f, 0.35f, 1.00f);
    
    // Buttons - green gradient
    colors[ImGuiCol_Button] = ImVec4(0.15f, 0.45f, 0.20f, 0.40f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.20f, 0.60f, 0.28f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.10f, 0.75f, 0.25f, 1.00f);
    
    // Selection and sliders
    colors[ImGuiCol_FrameBg] = ImVec4(0.10f, 0.25f, 0.15f, 0.54f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.15f, 0.40f, 0.22f, 0.40f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.18f, 0.50f, 0.26f, 0.67f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.25f, 0.80f, 0.35f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.30f, 0.95f, 0.45f, 1.00f);
    
    // Checkboxes and radio buttons
    colors[ImGuiCol_CheckMark] = ImVec4(0.30f, 0.95f, 0.45f, 1.00f);
    
    // Text selection
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.20f, 0.60f, 0.30f, 0.35f);
    
    // Tabs
    colors[ImGuiCol_Tab] = ImVec4(0.12f, 0.35f, 0.18f, 0.86f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.20f, 0.65f, 0.30f, 0.80f);
    colors[ImGuiCol_TabActive] = ImVec4(0.18f, 0.55f, 0.26f, 1.00f);
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.08f, 0.20f, 0.12f, 0.97f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.12f, 0.35f, 0.18f, 1.00f);
    
    // Resize grips
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.20f, 0.60f, 0.28f, 0.25f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.25f, 0.75f, 0.35f, 0.67f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.30f, 0.90f, 0.45f, 0.95f);
    
    // Borders and separators
    colors[ImGuiCol_Separator] = ImVec4(0.15f, 0.50f, 0.22f, 0.50f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.20f, 0.65f, 0.30f, 0.78f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.25f, 0.75f, 0.35f, 1.00f);
    
    // Rounding for modern look
    style.WindowRounding = 5.0f;
    style.FrameRounding = 3.0f;
    style.GrabRounding = 3.0f;
    style.ScrollbarRounding = 3.0f;
    style.TabRounding = 4.0f;
    
    // Setup Platform/Renderer backends
    const char* glsl_version = "#version 150";  // OpenGL 3.2+
    ImGui_ImplGlfw_InitForOpenGL(window_, false);  // false = don't install callbacks, we handle them
    ImGui_ImplOpenGL3_Init(glsl_version);
    
    return true;
}

void Window::run(FrameBuffer& frame_buffer) {
    last_frame_time_ = Clock::now();
    
    while (!glfwWindowShouldClose(window_)) {
        // Calculate frame time
        auto now = Clock::now();
        std::chrono::duration<double> elapsed = now - last_frame_time_;
        double frame_time = elapsed.count();
        last_frame_time_ = now;
        
        // Update router timing (handles interpolation)
        viz_router_.update(frame_time);
        
        // Poll events
        glfwPollEvents();
        
        // Get latest frame from simulation
        FrameSnapshot frame = frame_buffer.read();
        viz_router_.update_physics(frame);
        
        // Render via router
        int width, height;
        glfwGetFramebufferSize(window_, &width, &height);
        viz_router_.render(renderer_, width, height);
        
        // Swap buffers
        glfwSwapBuffers(window_);
    }
}

void Window::run_with_ui(SimulationThread& sim_thread) {
    last_frame_time_ = Clock::now();
    
    while (!glfwWindowShouldClose(window_)) {
        // Calculate frame time
        auto now = Clock::now();
        std::chrono::duration<double> elapsed = now - last_frame_time_;
        double frame_time = elapsed.count();
        last_frame_time_ = now;
        
        // Update router timing
        viz_router_.update(frame_time);
        
        // Poll and handle events
        glfwPollEvents();
        
        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        // Render UI panels (pass renderer for visualization controls)
        ui_manager_.render(sim_thread, &renderer_);
        
        // Get latest frame from simulation
        FrameSnapshot frame = sim_thread.get_latest_frame();
        viz_router_.update_physics(frame);
        
        // Render via router (with interpolation)
        int width, height;
        glfwGetFramebufferSize(window_, &width, &height);
        viz_router_.render(renderer_, width, height);
        
        // Render ImGui on top
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        
        // Swap buffers
        glfwSwapBuffers(window_);
    }
}

void Window::run_with_ui(SimulationThread& sim_thread, CommandRouter& command_router) {
    last_frame_time_ = Clock::now();
    
    while (!glfwWindowShouldClose(window_)) {
        // Calculate frame time
        auto now = Clock::now();
        std::chrono::duration<double> elapsed = now - last_frame_time_;
        double frame_time = elapsed.count();
        last_frame_time_ = now;
        
        // Update router timing
        viz_router_.update(frame_time);
        
        // Poll and handle events
        glfwPollEvents();
        
        // Process pending results from SimThread
        command_router.process_results();
        
        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        // Render UI panels with command router
        ui_manager_.render(sim_thread, command_router, &renderer_);
        
        // Get latest frame from simulation
        FrameSnapshot frame = sim_thread.get_latest_frame();
        viz_router_.update_physics(frame);
        
        // Render via router (with interpolation)
        int width, height;
        glfwGetFramebufferSize(window_, &width, &height);
        viz_router_.render(renderer_, width, height);
        
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
    Window* win = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (!win) return;
    
    // Forward to ImGui first
    ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);
    
    // Check if ImGui wants the mouse (hovering over UI)
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) return;
    
    // Handle camera controls only if not over UI
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        win->mouse_left_down_ = (action == GLFW_PRESS);
    } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        win->mouse_right_down_ = (action == GLFW_PRESS);
    }
}

void Window::cursor_pos_callback(GLFWwindow* window, double xpos, double ypos) {
    Window* win = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (!win) return;
    
    // Forward to ImGui
    ImGui_ImplGlfw_CursorPosCallback(window, xpos, ypos);
    
    double dx = xpos - win->last_mouse_x_;
    double dy = ypos - win->last_mouse_y_;
    
    // Only handle camera if not over UI
    ImGuiIO& io = ImGui::GetIO();
    if (!io.WantCaptureMouse) {
        if (win->mouse_left_down_) {
            win->renderer_.camera().orbit(dx, dy);
        } else if (win->mouse_right_down_) {
            win->renderer_.camera().pan(dx, dy);
        }
    }
    
    win->last_mouse_x_ = xpos;
    win->last_mouse_y_ = ypos;
}

void Window::scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    Window* win = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (!win) return;
    
    // Forward to ImGui first
    ImGui_ImplGlfw_ScrollCallback(window, xoffset, yoffset);
    
    // Handle camera zoom only if ImGui doesn't want mouse
    ImGuiIO& io = ImGui::GetIO();
    if (!io.WantCaptureMouse) {
        win->renderer_.camera().zoom(yoffset);
    }
}

void Window::key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    (void)scancode;
    Window* win = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (!win) return;
    
    // Tilde key (~) always toggles terminal, even if ImGui has focus
    if (key == GLFW_KEY_GRAVE_ACCENT && action == GLFW_PRESS) {
        win->ui_manager_.show_command_console = !win->ui_manager_.show_command_console;
    }
    
    // Check if ImGui wants keyboard input (text input fields, etc.)
    ImGuiIO& io = ImGui::GetIO();
    
    // Handle other built-in keys only if ImGui doesn't want keyboard
    if (!io.WantCaptureKeyboard) {
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
        
        if (key == GLFW_KEY_R && action == GLFW_PRESS) {
            win->renderer_.camera().reset();
        }
    }
    
    // Forward to ImGui for its own processing
    ImGui_ImplGlfw_KeyCallback(window, key, scancode, action, mods);
    
    // Call user callback if set
    if (win->key_callback_) {
        win->key_callback_(key, action, mods);
    }
}

} // namespace vsepr
