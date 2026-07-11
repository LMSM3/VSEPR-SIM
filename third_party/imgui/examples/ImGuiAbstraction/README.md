# ImGui Abstraction Layer

A simple, cross-platform abstraction layer for Dear ImGui that automatically selects the best rendering backend and platform layer for your system.

## Features

- **Automatic Backend Selection**: Automatically chooses the best renderer (DirectX11, OpenGL3, Vulkan) and platform (Win32, X11, SDL2, GLFW) for your system
- **Simple API**: Create an ImGui application in just a few lines of code
- **Cross-Platform**: Works on Windows, Linux, and WSL
- **Easy Integration**: Drop-in abstraction that works with existing ImGui code

## Supported Platforms & Renderers

### Windows
- **Renderer**: DirectX 11 (default)
- **Platform**: Win32
- **Requirements**: Visual Studio 2019 or later with C++ Desktop Development workload

### Linux / WSL
- **Renderer**: OpenGL 3.3
- **Platform**: X11
- **Requirements**: 
  - GCC/Clang compiler
  - CMake 3.10+
  - X11 development libraries
  - OpenGL development libraries
  - For WSL: X Server on Windows (VcXsrv, X410, etc.)

## Quick Start

### Windows - Visual Studio

```cpp
#include "ImGuiAbstraction/ImGuiRenderer.h"
#include "imgui.h"

int main() {
    // Create renderer with auto-detection
    auto renderer = ImGuiAbstraction::CreateRenderer();
    if (!renderer) return 1;
    
    // Main loop
    while (!renderer->ShouldClose()) {
        renderer->PollEvents();
        renderer->NewFrame();
        
        // Your ImGui code here
        ImGui::ShowDemoWindow();
        
        renderer->Render();
    }
    
    renderer->Shutdown();
    return 0;
}
```

### Building

#### Windows - Command Line (Batch)
```batch
cd ImGuiAbstraction
build_windows.bat
cd build_cli
imgui_abstraction_example.exe
```

#### Windows - PowerShell
```powershell
cd ImGuiAbstraction
.\build_windows.ps1
cd build_cli
.\imgui_abstraction_example.exe
```

#### Windows - CMake + Visual Studio
```batch
cd ImGuiAbstraction
mkdir build
cd build
cmake ..
cmake --build . --config Release
bin\imgui_abstraction_example.exe
```

#### Linux / WSL
```bash
cd ImGuiAbstraction
chmod +x build_wsl.sh
./build_wsl.sh
cd build_wsl/bin
./imgui_abstraction_example
```

## WSL Graphics Setup

To run graphical applications in WSL, you need an X Server:

1. **Install an X Server on Windows**:
   - VcXsrv (Recommended): https://sourceforge.net/projects/vcxsrv/
   - X410 (Paid): Microsoft Store
   - Xming: https://sourceforge.net/projects/xming/

2. **Launch X Server**:
   - For VcXsrv: Run XLaunch and select "Disable access control"

3. **Set DISPLAY environment variable**:
   ```bash
   # For WSL1
   export DISPLAY=:0
   
   # For WSL2
   export DISPLAY=$(cat /etc/resolv.conf | grep nameserver | awk '{print $2}'):0
   ```

4. **Add to ~/.bashrc** for persistence:
   ```bash
   echo 'export DISPLAY=$(cat /etc/resolv.conf | grep nameserver | awk "{print \$2}"):0' >> ~/.bashrc
   ```

## Configuration

Customize renderer settings:

```cpp
ImGuiAbstraction::RendererConfig config;
config.rendererType = ImGuiAbstraction::RendererType::Auto; // or DirectX11, OpenGL3, Vulkan
config.platformType = ImGuiAbstraction::PlatformType::Auto; // or Win32, X11, SDL2, GLFW
config.windowWidth = 1280;
config.windowHeight = 720;
config.windowTitle = "My ImGui App";
config.vsync = true;

auto renderer = ImGuiAbstraction::CreateRenderer(config);
```

## API Reference

### ImGuiAbstraction::IRenderer

- `bool Init(const RendererConfig& config)` - Initialize renderer
- `void Shutdown()` - Cleanup and destroy renderer
- `void NewFrame()` - Begin new frame (call before ImGui code)
- `void Render()` - Render frame (call after ImGui code)
- `bool ShouldClose()` - Check if window should close
- `void PollEvents()` - Process window events
- `void* GetNativeWindow()` - Get native window handle
- `RendererType GetRendererType()` - Get current renderer type
- `PlatformType GetPlatformType()` - Get current platform type

### Factory Functions

- `std::unique_ptr<IRenderer> CreateRenderer(config)` - Create renderer with auto-detection
- `bool IsRendererAvailable(type)` - Check if renderer is available
- `bool IsPlatformAvailable(type)` - Check if platform is available

## Project Structure

```
ImGuiAbstraction/
├── ImGuiRenderer.h          # Main abstraction interface
├── ImGuiRenderer.cpp        # Implementation
├── example_main.cpp         # Example application
├── CMakeLists.txt          # CMake build configuration
├── build_windows.bat       # Windows batch build script
├── build_windows.ps1       # Windows PowerShell build script
├── build_wsl.sh           # WSL/Linux build script
└── README.md              # This file
```

## Troubleshooting

### Windows

**Error: "cl.exe not found"**
- Run from "x64 Native Tools Command Prompt for VS"
- Or install Visual Studio with C++ Desktop Development workload

**Error: "Cannot find d3d11.lib"**
- Install Windows SDK through Visual Studio Installer

### WSL

**Error: "cannot open display"**
- Make sure X Server is running on Windows
- Set DISPLAY environment variable correctly
- Check Windows Firewall isn't blocking X Server

**Error: "libGL.so not found"**
```bash
sudo apt-get install libgl1-mesa-dev libglu1-mesa-dev
```

## License

This abstraction layer follows the same license as Dear ImGui (MIT License).

## Credits

Built on top of [Dear ImGui](https://github.com/ocornut/imgui) by Omar Cornut.
