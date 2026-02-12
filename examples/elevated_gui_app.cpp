/**
 * VSEPR-Sim Elevated GUI Application
 * Full ImGui integration with context menus and data piping
 */

#include "gui/imgui_integration.hpp"
#include "gui/context_menu.hpp"
#include "gui/data_pipe.hpp"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <cmath>

using namespace vsepr::gui;

int main(int argc, char** argv) {
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return 1;
    }
    
    // GL 3.3 + GLSL 330
    const char* glsl_version = "#version 330";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);  // FIXED: Was CORE_PROFILE - causes black screen!
    
    // Create window
    GLFWwindow* window = glfwCreateWindow(
        1280, 720,
        "VSEPR-Sim v2.3.1 - Elevated GUI",
        nullptr, nullptr
    );
    
    if (window == nullptr) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return 1;
    }
    
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync
    
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    // Note: Docking not available in this ImGui version
    
    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);
    
    // Create application
    ImGuiVSEPRWindow app;
    
    // Setup data pipes
    auto status_pipe = std::make_shared<DataPipe<std::string>>("status");
    auto energy_pipe = std::make_shared<DataPipe<double>>("energy");
    
    app.connectPipes(status_pipe, energy_pipe);
    
    // Register pipes
    PipeNetwork::instance().registerPipe("status", status_pipe);
    PipeNetwork::instance().registerPipe("energy", energy_pipe);
    
    std::cout << "╔════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  VSEPR-Sim v2.3.1 - Elevated GUI Application                  ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════════╝\n\n";
    std::cout << "Window opened at 1280x720\n";
    std::cout << "ImGui initialized\n";
    std::cout << "Data pipes connected\n\n";
    std::cout << "Features:\n";
    std::cout << "  • Right-click 3D viewer for context menu\n";
    std::cout << "  • Live energy plotting\n";
    std::cout << "  • Theme switching (View menu)\n";
    std::cout << "  • Reactive data pipes\n\n";
    
    // Simulate some data
    status_pipe->push("Application started");
    
    // Simulation thread (optional)
    bool running = true;
    std::thread sim_thread([&]() {
        int step = 0;
        while (running && step < 50) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            // Push fake energy data
            double energy = -57.8 + (std::sin(step * 0.2) * 5.0);
            energy_pipe->push(energy);
            
            if (step % 10 == 0) {
                status_pipe->push("Computing step " + std::to_string(step));
            }
            
            step++;
        }
        status_pipe->push("Simulation complete");
    });
    
    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        
        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        // Render application
        app.render();
        
        // Demo window (optional)
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D)) {
            ImGui::ShowDemoWindow();
        }
        
        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        
        glfwSwapBuffers(window);
    }
    
    // Cleanup
    running = false;
    if (sim_thread.joinable()) {
        sim_thread.join();
    }
    
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    
    glfwDestroyWindow(window);
    glfwTerminate();
    
    std::cout << "\nApplication closed\n";
    
    return 0;
}
