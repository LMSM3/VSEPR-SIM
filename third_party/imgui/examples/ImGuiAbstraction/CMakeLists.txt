cmake_minimum_required(VERSION 3.10)
project(ImGuiAbstraction)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ImGui core files
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
)

# Platform-specific backends
if(WIN32)
    set(BACKEND_SOURCES
        ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
        ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp
    )
    set(PLATFORM_LIBS d3d11 dxgi d3dcompiler)
elseif(UNIX AND NOT APPLE)
    # Linux with X11 + OpenGL3
    find_package(OpenGL REQUIRED)
    find_package(X11 REQUIRED)
    
    set(BACKEND_SOURCES
        ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
        ${IMGUI_DIR}/backends/imgui_impl_x11.cpp
    )
    set(PLATFORM_LIBS ${OPENGL_LIBRARIES} ${X11_LIBRARIES} GL dl)
    add_definitions(-DIMGUI_IMPL_OPENGL_LOADER_GL3W)
endif()

# Abstraction layer
set(ABSTRACTION_SOURCES
    ImGuiRenderer.cpp
    example_main.cpp
)

# Create executable
add_executable(imgui_abstraction_example
    ${IMGUI_SOURCES}
    ${BACKEND_SOURCES}
    ${ABSTRACTION_SOURCES}
)

# Include directories
target_include_directories(imgui_abstraction_example PRIVATE
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link libraries
target_link_libraries(imgui_abstraction_example PRIVATE
    ${PLATFORM_LIBS}
)

# Platform-specific settings
if(WIN32)
    target_compile_definitions(imgui_abstraction_example PRIVATE
        UNICODE
        _UNICODE
    )
    set_target_properties(imgui_abstraction_example PROPERTIES
        WIN32_EXECUTABLE OFF  # Keep console window for debug output
    )
endif()

# Output directory
set_target_properties(imgui_abstraction_example PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

message(STATUS "Building ImGui Abstraction Example")
message(STATUS "  Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "  Compiler: ${CMAKE_CXX_COMPILER_ID}")
