/**
 * VSEPR-Sim - Main GUI Entry Point
 * Unified launcher integrating all features
 */

#include "gui/unified_launcher.hpp"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <iostream>

using namespace vsepr::launcher;

void print_banner() {
    std::cout << "╔════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                                                                ║\n";
    std::cout << "║  VSEPR-Sim v2.3.1 - Unified GUI Launcher                      ║\n";
    std::cout << "║  Molecular Simulation & Discovery System                      ║\n";
    std::cout << "║                                                                ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════════╝\n\n";
    std::cout << "Features:\n";
    std::cout << "  • 3D Molecular Viewer\n";
    std::cout << "  • Interactive Pokedex (26+ molecules)\n";
    std::cout << "  • Batch Job Manager\n";
    std::cout << "  • Integrated Shell Terminal\n";
    std::cout << "  • Direct script execution\n\n";
}

int main(int argc, char** argv) {
    print_banner();
    
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return 1;
    }
    
    // GL 3.3 + GLSL 330
    const char* glsl_version = "#version 330";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    // Create window
    GLFWwindow* window = glfwCreateWindow(
        1920, 1080,
        "VSEPR-Sim v2.3.1 - Unified Launcher",
        nullptr, nullptr
    );
    
    if (window == nullptr) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return 1;
    }
    
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);  // Enable vsync
    
    // Setup Dear ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    // Note: Docking not available in this ImGui version
    // Note: Multi-viewport not available
    
    // Setup backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);
    
    // Apply theme
    vsepr::gui::ImGuiThemeManager::apply(vsepr::gui::ImGuiThemeManager::Theme::VSEPR_BLUE);
    
    // Create unified launcher
    UnifiedLauncher launcher;
    
    std::cout << "Window: 1920x1080 (Full HD)\n";
    std::cout << "Theme: VSEPR Blue\n";
    std::cout << "Ready!\n\n";
    
    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        
        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        // Render launcher
        launcher.render();
        
        // ImGui demo (Ctrl+D)
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D)) {
            static bool show_demo = false;
            show_demo = !show_demo;
            if (show_demo) ImGui::ShowDemoWindow(&show_demo);
        }
        
        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        
        // Note: Multi-viewport not available in this ImGui version
        
        glfwSwapBuffers(window);
    }
    
    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    
    glfwDestroyWindow(window);
    glfwTerminate();
    
    std::cout << "\nVSEPR-Sim closed\n";
    
    return 0;
}
