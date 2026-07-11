#pragma once

#include <string>
#include <memory>

// Simple abstraction layer for ImGui rendering
// Supports multiple backends: DirectX11, OpenGL, etc.

namespace ImGuiAbstraction {

enum class RendererType {
    DirectX11,
    OpenGL3,
    Vulkan,
    Auto  // Automatically select best available
};

enum class PlatformType {
    Win32,
    SDL2,
    GLFW,
    Auto  // Automatically select best available
};

struct RendererConfig {
    RendererType rendererType = RendererType::Auto;
    PlatformType platformType = PlatformType::Auto;
    int windowWidth = 1280;
    int windowHeight = 720;
    std::string windowTitle = "ImGui Application";
    bool vsync = true;
};

class IRenderer {
public:
    virtual ~IRenderer() = default;
    
    virtual bool Init(const RendererConfig& config) = 0;
    virtual void Shutdown() = 0;
    
    virtual void NewFrame() = 0;
    virtual void Render() = 0;
    
    virtual bool ShouldClose() const = 0;
    virtual void PollEvents() = 0;
    
    virtual void* GetNativeWindow() const = 0;
    virtual RendererType GetRendererType() const = 0;
    virtual PlatformType GetPlatformType() const = 0;
};

// Factory function to create appropriate renderer
std::unique_ptr<IRenderer> CreateRenderer(const RendererConfig& config = RendererConfig());

// Helper to get available renderer types on current platform
bool IsRendererAvailable(RendererType type);
bool IsPlatformAvailable(PlatformType type);

} // namespace ImGuiAbstraction
