cmake_minimum_required(VERSION 3.15)
project(vsepr-sim VERSION 4.0.0 LANGUAGES C CXX)

# ============================================================================
# VSEPR-SIM  Two-Part Build System (Orchestrator)
# ============================================================================
#
# This root file provides:
#   1. Project identity, compiler settings, build options
#   2. include(cmake/CoreBuild.cmake)  Part 1: all headless targets
#   3. include(cmake/VisBuild.cmake)   Part 2: VIS / GUI / Desktop
#   4. Tests (after both parts so all targets are available)
#   5. Summary
#
# Branch: 4.0-legacy-beta
# Lineage: kernel v0.1 → v0.3 → user 2.7 → 2.9 → 3.0 → 4.0-LB
# Purpose: Multi-scale property search with mature deterministic kernel
#
# Previous audit checkpoint: 3.0.1
# Original monolithic file archived at:
#   archive/CMakeLists.txt.monolithic-backup
# ============================================================================

# Global include paths for project headers
include_directories(${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/include)

# ============================================================================
# Optional CUDA Support (graceful fallback to CPU)
# ============================================================================

include(CheckLanguage)
check_language(CUDA)
if(CMAKE_CUDA_COMPILER)
    enable_language(CUDA)
    set(VSEPR_HAS_CUDA ON)
    message(STATUS "CUDA enabled: ${CMAKE_CUDA_COMPILER}")
else()
    set(VSEPR_HAS_CUDA OFF)
    message(STATUS "CUDA not found, using CPU fallback")
endif()

# ============================================================================
# Build Options
# ============================================================================

option(BUILD_TESTS "Build test suite" ON)
option(BUILD_APPS  "Build applications" ON)
option(BUILD_VIS   "Build visualization tools (vsepr-view, vsepr_vis)" ON)
option(BUILD_DEMOS "Build demo targets" OFF)

# ============================================================================
# Compiler Settings
# ============================================================================

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# M_PI and friends for all platforms (Clang/UCRT64 needs this)
add_compile_definitions(_USE_MATH_DEFINES)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -pedantic)
elseif(MSVC)
    add_compile_options(/W4)
endif()

# ============================================================================
# Part 1: Core Build (headless libraries and apps, no graphics deps)
# ============================================================================

include(cmake/CoreBuild.cmake)

# ============================================================================
# Part 2: Visualization and GUI Build
#         OpenGL / GLFW / GLEW / ImGui / Qt6
#         Dependencies auto-fetched via FetchContent when not on system.
# ============================================================================

include(cmake/VisBuild.cmake)

# ============================================================================
# Tests (after both parts so VIS targets are visible to test helpers)
# ============================================================================

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ============================================================================
# Summary
# ============================================================================

message(STATUS "")
message(STATUS "=== VSEPR-SIM v${PROJECT_VERSION} Build Summary ===")
message(STATUS "Added VSPER top-down implementation direct pathway for support of the VSEPR workspace.")
message(STATUS "")
message(STATUS "Build tests:         ${BUILD_TESTS}")
message(STATUS "Build apps:          ${BUILD_APPS}")
message(STATUS "Build visualization: ${BUILD_VIS}")
message(STATUS "Build GUI:           ${BUILD_GUI}")
message(STATUS "Build desktop:       ${BUILD_DESKTOP}")
message(STATUS "CUDA:                ${VSEPR_HAS_CUDA}")
message(STATUS "")
message(STATUS "Core Libraries (Part 1):")
message(STATUS "  vsepr_core        (INTERFACE)")
message(STATUS "  vsepr_tracker     (STATIC — particle identity + random picker)")
message(STATUS "  vsepr_infra       (STATIC — bootstrap probe, MOTD, NVIDIA TUI)")
message(STATUS "  vsepr_periodic    (STATIC Z=1-102)")
message(STATUS "  vsepr_sim         (STATIC)")
message(STATUS "  vsepr_pot         (INTERFACE)")
message(STATUS "  vsepr_box         (INTERFACE)")
message(STATUS "  vsepr_nl          (INTERFACE)")
message(STATUS "  vsepr_int         (INTERFACE)")
message(STATUS "  vsepr_build       (INTERFACE)")
message(STATUS "  vsepr_dynamic     (STATIC)")
message(STATUS "  vsepr_gpu         (STATIC)")
message(STATUS "  vsepr_io          (STATIC)")
message(STATUS "  vsepr_thermal     (STATIC)")
message(STATUS "  vsepr_api         (STATIC)")
message(STATUS "  spec_parser       (STATIC)")
message(STATUS "  vsepr_demo        (STATIC)")
message(STATUS "  atomistic         (STATIC + TUI active)")
message(STATUS "  coarse_grain      (INTERFACE)")
message(STATUS "  vsepr_ehd         (INTERFACE — Electrohydrodynamic Multiphysics)")
if(VSEPR_VIS_READY)
    message(STATUS "")
    message(STATUS "VIS/GUI Libraries (Part 2 active):")
    message(STATUS "  vsepr_gui_utils   (STATIC)")
    message(STATUS "  vsepr_render      (STATIC)")
    message(STATUS "  vsepr_vis         (STATIC)")
    if(BUILD_GUI)
        message(STATUS "  vsepr_gui         (STATIC)")
    endif()
else()
    message(STATUS "")
    message(STATUS "VIS/GUI Libraries: OFF (deps not available)")
endif()
message(STATUS "")
