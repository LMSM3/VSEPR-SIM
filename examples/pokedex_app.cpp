/**
 * VSEPR-Sim Molecular Pokedex - Complete GUI Application
 * Interactive molecule browser with visual testing
 */

#include "gui/pokedex_gui.hpp"
#include "gui/imgui_integration.hpp"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <iostream>

using namespace vsepr::pokedex;
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
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    // Create window
    GLFWwindow* window = glfwCreateWindow(
        1600, 900,
        "VSEPR-Sim Molecular Pokedex v2.3.1",
        nullptr, nullptr
    );
    
    if (window == nullptr) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return 1;
    }
    
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    
    // Setup Dear ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    // Note: Docking not available in this ImGui version
    
    // Setup backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);
    
    // Apply VSEPR Blue theme
    ImGuiThemeManager::apply(ImGuiThemeManager::Theme::VSEPR_BLUE);
    
    // Create Pokedex browser
    ImGuiPokedexBrowser pokedex;
    
    // Create data pipes
    auto molecule_pipe = std::make_shared<DataPipe<MoleculeEntry>>("molecule");
    auto status_pipe = std::make_shared<DataPipe<std::string>>("status");
    
    pokedex.connectPipes(molecule_pipe, status_pipe);
    
    // Status subscriber
    status_pipe->subscribe([](const std::string& status) {
        std::cout << "[STATUS] " << status << "\n";
    });
    
    // Banner
    std::cout << "╔════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  VSEPR-Sim Molecular Pokedex v2.3.1                           ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════════╝\n\n";
    std::cout << "Window: 1600x900\n";
    std::cout << "Database: " << PokedexDatabase::instance().getTotalCount() << " molecules\n";
    std::cout << "Tested: " << PokedexDatabase::instance().getTestedCount() << " molecules\n";
    std::cout << "Success Rate: " << PokedexDatabase::instance().getSuccessRate() << "%\n\n";
    std::cout << "Features:\n";
    std::cout << "  • Browse molecules by category\n";
    std::cout << "  • Search molecules by name/formula\n";
    std::cout << "  • View detailed information\n";
    std::cout << "  • Test molecules with VSEPR\n";
    std::cout << "  • Track test results\n\n";
    
    status_pipe->push("Pokedex ready");
    
    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        
        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        // Menu bar
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Export Results")) { }
                if (ImGui::MenuItem("Import Database")) { }
                ImGui::Separator();
                if (ImGui::MenuItem("Exit", "Alt+F4")) {
                    glfwSetWindowShouldClose(window, true);
                }
                ImGui::EndMenu();
            }
            
            if (ImGui::BeginMenu("View")) {
                if (ImGui::MenuItem("Light Theme")) {
                    ImGuiThemeManager::apply(ImGuiThemeManager::Theme::LIGHT);
                }
                if (ImGui::MenuItem("Dark Theme")) {
                    ImGuiThemeManager::apply(ImGuiThemeManager::Theme::DARK);
                }
                if (ImGui::MenuItem("VSEPR Blue")) {
                    ImGuiThemeManager::apply(ImGuiThemeManager::Theme::VSEPR_BLUE);
                }
                ImGui::EndMenu();
            }
            
            if (ImGui::BeginMenu("Test")) {
                if (ImGui::MenuItem("Test All Molecules")) {
                    status_pipe->push("Testing all molecules...");
                }
                if (ImGui::MenuItem("Test Phase 1")) {
                    status_pipe->push("Testing Phase 1 molecules...");
                }
                if (ImGui::MenuItem("Test Phase 2")) {
                    status_pipe->push("Testing Phase 2 molecules...");
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Clear Results")) {
                    status_pipe->push("Results cleared");
                }
                ImGui::EndMenu();
            }
            
            if (ImGui::BeginMenu("Help")) {
                if (ImGui::MenuItem("About")) { }
                if (ImGui::MenuItem("Documentation")) { }
                ImGui::EndMenu();
            }
            
            // Stats in menu bar
            ImGui::Spacing();
            ImGui::SameLine(ImGui::GetWindowWidth() - 300);
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), 
                             "✓ %d/%d (%.0f%%)",
                             PokedexDatabase::instance().getSuccessCount(),
                             PokedexDatabase::instance().getTestedCount(),
                             PokedexDatabase::instance().getSuccessRate());
            
            ImGui::EndMainMenuBar();
        }
        
        // Render Pokedex browser
        pokedex.render();
        
        // Status bar
        ImGui::SetNextWindowPos(ImVec2(0, io.DisplaySize.y - 25));
        ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, 25));
        
        if (ImGui::Begin("StatusBar", nullptr,
                        ImGuiWindowFlags_NoTitleBar |
                        ImGuiWindowFlags_NoResize |
                        ImGuiWindowFlags_NoMove)) {
            std::string status;
            if (status_pipe->tryGet(status)) {
                ImGui::Text("%s", status.c_str());
            }
        }
        ImGui::End();
        
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
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    
    glfwDestroyWindow(window);
    glfwTerminate();
    
    std::cout << "\nPokedex closed\n";
    
    return 0;
}
