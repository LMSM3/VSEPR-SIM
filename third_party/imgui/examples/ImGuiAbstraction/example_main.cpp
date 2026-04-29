#include "ImGuiRenderer.h"
#include "imgui.h"
#include <stdio.h>

int main(int argc, char** argv) {
    // Configure renderer
    ImGuiAbstraction::RendererConfig config;
    config.windowTitle = "ImGui Abstraction Example";
    config.windowWidth = 1280;
    config.windowHeight = 720;
    config.vsync = true;
    
    // Create renderer (auto-selects best available backend)
    auto renderer = ImGuiAbstraction::CreateRenderer(config);
    if (!renderer) {
        fprintf(stderr, "Failed to create renderer!\n");
        return 1;
    }
    
    printf("Successfully initialized ImGui with:\n");
    printf("  Renderer: DirectX11\n");
    printf("  Platform: Win32\n");
    
    // Application state
    bool show_demo_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    
    // Main loop
    while (!renderer->ShouldClose()) {
        renderer->PollEvents();
        renderer->NewFrame();
        
        // 1. Show the big demo window
        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);
        
        // 2. Show a simple window
        {
            static float f = 0.0f;
            static int counter = 0;
            
            ImGui::Begin("Hello, ImGui Abstraction!");
            
            ImGui::Text("This is a simple abstraction layer for ImGui.");
            ImGui::Text("It automatically selects the best renderer/platform.");
            ImGui::Separator();
            
            ImGui::Checkbox("Demo Window", &show_demo_window);
            ImGui::Checkbox("Another Window", &show_another_window);
            
            ImGui::SliderFloat("float", &f, 0.0f, 1.0f);
            ImGui::ColorEdit3("clear color", (float*)&clear_color);
            
            if (ImGui::Button("Button"))
                counter++;
            ImGui::SameLine();
            ImGui::Text("counter = %d", counter);
            
            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 
                       1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
            ImGui::End();
        }
        
        // 3. Show another simple window
        if (show_another_window) {
            ImGui::Begin("Another Window", &show_another_window);
            ImGui::Text("Hello from another window!");
            if (ImGui::Button("Close Me"))
                show_another_window = false;
            ImGui::End();
        }
        
        // Rendering
        renderer->Render();
    }
    
    renderer->Shutdown();
    return 0;
}
