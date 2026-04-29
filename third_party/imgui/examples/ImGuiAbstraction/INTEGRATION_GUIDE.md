# Integration Guide

How to integrate the ImGui Abstraction Layer into your own project.

## Option 1: Copy Files (Simplest)

1. **Copy these files to your project**:
   ```
   ImGuiRenderer.h
   ImGuiRenderer.cpp
   ```

2. **Include ImGui source files** (if not already in your project):
   ```
   imgui.h
   imgui.cpp
   imgui_demo.cpp
   imgui_draw.cpp
   imgui_tables.cpp
   imgui_widgets.cpp
   ```

3. **Include backend files for your platform**:
   
   **Windows**:
   ```
   backends/imgui_impl_win32.h
   backends/imgui_impl_win32.cpp
   backends/imgui_impl_dx11.h
   backends/imgui_impl_dx11.cpp
   ```
   
   **Linux**:
   ```
   backends/imgui_impl_x11.h
   backends/imgui_impl_x11.cpp
   backends/imgui_impl_opengl3.h
   backends/imgui_impl_opengl3.cpp
   ```

4. **Update your build system**:

   **Visual Studio Project**:
   - Add all `.cpp` files to your project
   - Add `d3d11.lib` to Linker > Input > Additional Dependencies (Windows)
   
   **CMakeLists.txt**:
   ```cmake
   add_executable(YourApp
       main.cpp
       ImGuiRenderer.cpp
       # ImGui core
       imgui/imgui.cpp
       imgui/imgui_demo.cpp
       imgui/imgui_draw.cpp
       imgui/imgui_tables.cpp
       imgui/imgui_widgets.cpp
       # Backends
       imgui/backends/imgui_impl_win32.cpp
       imgui/backends/imgui_impl_dx11.cpp
   )
   target_link_libraries(YourApp d3d11)
   ```

5. **Use in your code**:
   ```cpp
   #include "ImGuiRenderer.h"
   #include "imgui.h"
   
   int main() {
       auto renderer = ImGuiAbstraction::CreateRenderer();
       while (!renderer->ShouldClose()) {
           renderer->PollEvents();
           renderer->NewFrame();
           
           // Your code
           ImGui::Begin("My Window");
           ImGui::Text("Hello!");
           ImGui::End();
           
           renderer->Render();
       }
       renderer->Shutdown();
   }
   ```

## Option 2: Static Library

Build as a library and link against it.

**Create library**:
```cmake
# CMakeLists.txt
add_library(ImGuiAbstraction STATIC
    ImGuiRenderer.cpp
    # ... imgui sources ...
)
target_include_directories(ImGuiAbstraction PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
```

**Use library**:
```cmake
target_link_libraries(YourApp ImGuiAbstraction)
```

## Option 3: Submodule (Git Projects)

1. **Add as submodule**:
   ```bash
   git submodule add <your-imgui-abstraction-repo> external/imgui_abstraction
   git submodule update --init --recursive
   ```

2. **Update CMakeLists.txt**:
   ```cmake
   add_subdirectory(external/imgui_abstraction)
   target_link_libraries(YourApp ImGuiAbstraction)
   ```

## Minimal Example

Smallest possible ImGui application:

```cpp
#include "ImGuiRenderer.h"
#include "imgui.h"

int main() {
    auto r = ImGuiAbstraction::CreateRenderer();
    while (!r->ShouldClose()) {
        r->PollEvents();
        r->NewFrame();
        ImGui::ShowDemoWindow();
        r->Render();
    }
    r->Shutdown();
}
```

## Customization Examples

### Custom Window Size
```cpp
ImGuiAbstraction::RendererConfig config;
config.windowWidth = 1920;
config.windowHeight = 1080;
config.windowTitle = "My Application";
auto renderer = ImGuiAbstraction::CreateRenderer(config);
```

### Disable VSync
```cpp
ImGuiAbstraction::RendererConfig config;
config.vsync = false;  // Maximum FPS
auto renderer = ImGuiAbstraction::CreateRenderer(config);
```

### Check Available Renderers
```cpp
if (ImGuiAbstraction::IsRendererAvailable(ImGuiAbstraction::RendererType::DirectX11)) {
    std::cout << "DirectX11 is available\n";
}
```

## Build Flags

### Windows
```
/I<imgui_path> /I<backends_path>
d3d11.lib dxgi.lib d3dcompiler.lib
```

### Linux
```
-I<imgui_path> -I<backends_path>
-lGL -lX11 -ldl
```

## Common Issues

### "Cannot find imgui.h"
- Add imgui directory to include paths: `/I<path>` or `-I<path>`

### "Cannot find d3d11.lib"
- Install Windows SDK via Visual Studio Installer
- Or specify SDK version in project settings

### "undefined reference to ImGui::..."
- Make sure all imgui `.cpp` files are compiled and linked

### Runtime "Failed to create renderer"
- On Windows: DirectX 11 driver not available
- On Linux: X Server not running or DISPLAY not set

## Performance Tips

1. **Release builds**: Use `/O2` (Windows) or `-O3` (Linux)
2. **Disable unnecessary ImGui features** in `imconfig.h`
3. **VSync control**: Disable for benchmarks, enable for smooth rendering
4. **Minimal includes**: Only include backends you need

## Platform-Specific Notes

### Windows
- Requires Windows 7+ with DirectX 11
- No administrator rights needed
- Works on Windows 10/11 out of the box

### WSL
- Requires X Server on host Windows
- Set `DISPLAY` environment variable
- VcXsrv recommended (free, works well with WSL2)

### Native Linux
- Requires X11 development libraries: `libx11-dev`
- Requires OpenGL libraries: `libgl1-mesa-dev`
- Wayland support: Not yet implemented (use Xwayland)

## Next Steps

Once integrated:
1. Check out `imgui_demo.cpp` for examples of all ImGui features
2. Read [ImGui documentation](https://github.com/ocornut/imgui)
3. Explore different backends if needed (Vulkan, SDL2, GLFW)
4. Consider adding your own renderer implementations

## Support

For issues specific to this abstraction layer, check:
- `README.md` - Usage guide
- `BUILD_SUMMARY.md` - Build status and known issues
- ImGui main repository for ImGui-specific questions
