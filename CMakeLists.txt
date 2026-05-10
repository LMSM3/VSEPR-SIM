cmake_minimum_required(VERSION 3.15...4.2)
project(vsepr-sim VERSION 5.0.0 LANGUAGES C CXX)

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -static-libgcc -static-libstdc++")

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
# Branch: 4.0.4.03
# Lineage: kernel v0.1 â†’ v0.3 â†’ user 2.7 â†’ 2.9 â†’ 3.0 â†’ 4.0-LB
# Purpose: Multi-scale property search with mature deterministic kernel
#
# Previous audit checkpoint: 4.0.0
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
option(BUILD_ADVANCED_RENDER "Build advanced 3D molecular renderer + data window (requires BUILD_VIS=ON)" OFF)
option(BUILD_DEMOS "Build demo targets" OFF)

# ── VSEPR Feature flags ───────────────────────────────────────────────────────
option(VSEPR_ENABLE_ATOMISTIC     "Enable atomistic molecular simulation layer" ON)
option(VSEPR_ENABLE_COARSE_GRAIN  "Enable coarse-grain reduced representation layer" ON)
option(VSEPR_ENABLE_XYZ_IO        "Enable XYZ/XYZFull parsing and validation" ON)
option(VSEPR_ENABLE_FIRE          "Enable FIRE minimization" ON)
option(VSEPR_ENABLE_THERMO        "Enable thermodynamics module" ON)
option(VSEPR_ENABLE_REACTION      "Enable reaction discovery module" ON)
option(VSEPR_ENABLE_CRYSTAL_LIB   "Enable crystal benchmark library" ON)
option(VSEPR_ENABLE_REGISTRY_CORE "Enable VSIM registry/default resolver" ON)
option(VSEPR_ENABLE_INSTALL_TOOLS "Enable install/doctor/path tools" ON)

# ── Generate vsepr_build_config.hpp ──────────────────────────────────────────
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(VSEPRBuildInfo)
vsepr_generate_build_config(OUTPUT_DIR "${CMAKE_BINARY_DIR}/generated")

# ============================================================================
# Compiler Settings
# ============================================================================
# C++23 (N4950) â€” upgraded from C++20 for std::expected, monadic optional,
# std::unreachable, std::to_underlying, consteval improvements.
# GCC 15.2.0 (UCRT64) supports the full C++23 feature set.
# C++26 (N5014) features tracked for future adoption â€” see cpp_corpus/.

set(CMAKE_CXX_STANDARD 23)
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

# ── Feature compile definitions (propagate to all targets via vsepr_core) ────
target_compile_definitions(vsepr_core INTERFACE
    VSEPR_ENABLE_ATOMISTIC=$<BOOL:${VSEPR_ENABLE_ATOMISTIC}>
    VSEPR_ENABLE_COARSE_GRAIN=$<BOOL:${VSEPR_ENABLE_COARSE_GRAIN}>
    VSEPR_ENABLE_XYZ_IO=$<BOOL:${VSEPR_ENABLE_XYZ_IO}>
    VSEPR_ENABLE_FIRE=$<BOOL:${VSEPR_ENABLE_FIRE}>
    VSEPR_ENABLE_THERMO=$<BOOL:${VSEPR_ENABLE_THERMO}>
    VSEPR_ENABLE_REACTION=$<BOOL:${VSEPR_ENABLE_REACTION}>
    VSEPR_ENABLE_CRYSTAL_LIB=$<BOOL:${VSEPR_ENABLE_CRYSTAL_LIB}>
    VSEPR_ENABLE_REGISTRY_CORE=$<BOOL:${VSEPR_ENABLE_REGISTRY_CORE}>
    VSEPR_ENABLE_INSTALL_TOOLS=$<BOOL:${VSEPR_ENABLE_INSTALL_TOOLS}>
)

# ============================================================================
# Part 2: Visualization and GUI Build
#         OpenGL / GLFW / GLEW / ImGui / Qt6
#         Dependencies auto-fetched via FetchContent when not on system.
# ============================================================================

include(cmake/VisBuild.cmake)

# ============================================================================
# Part 3: Advanced Renderer (optional — requires BUILD_VIS=ON)
#         3D molecular window + ImGui data panel
# ============================================================================

include(cmake/AdvancedRender.cmake)

# ============================================================================
# Tests (after both parts so VIS targets are visible to test helpers)
# ============================================================================

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ============================================================================
# Bridge Beta (scaffold for next-gen architecture prerequisites)
# ============================================================================
add_subdirectory(bridge_beta)

# ============================================================================
# Summary
# ============================================================================

message(STATUS "")
message(STATUS "VSEPR-SIM v${PROJECT_VERSION} -- Build Configuration")
message(STATUS "  Compiler:    ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Standard:    C++${CMAKE_CXX_STANDARD}")
message(STATUS "  Build type:  ${CMAKE_BUILD_TYPE}")
message(STATUS "  CUDA:        ${VSEPR_HAS_CUDA}")
message(STATUS "")
message(STATUS "  Build targets:")
message(STATUS "    tests:          ${BUILD_TESTS}")
message(STATUS "    apps:           ${BUILD_APPS}")
message(STATUS "    vis:            ${BUILD_VIS}")
message(STATUS "    demos:          ${BUILD_DEMOS}")
message(STATUS "")
message(STATUS "  Runtime modules:")
message(STATUS "    atomistic:      ${VSEPR_ENABLE_ATOMISTIC}")
message(STATUS "    coarse_grain:   ${VSEPR_ENABLE_COARSE_GRAIN}")
message(STATUS "    xyz_io:         ${VSEPR_ENABLE_XYZ_IO}")
message(STATUS "    fire:           ${VSEPR_ENABLE_FIRE}")
message(STATUS "    thermodynamics: ${VSEPR_ENABLE_THERMO}")
message(STATUS "    reaction:       ${VSEPR_ENABLE_REACTION}")
message(STATUS "    crystal_lib:    ${VSEPR_ENABLE_CRYSTAL_LIB}")
message(STATUS "    registry_core:  ${VSEPR_ENABLE_REGISTRY_CORE}")
message(STATUS "    install_tools:  ${VSEPR_ENABLE_INSTALL_TOOLS}")
message(STATUS "")
if(VSEPR_VIS_READY)
    message(STATUS "  VIS/GUI: active")
else()
    message(STATUS "  VIS/GUI: off (deps not available)")
endif()
message(STATUS "")
message(STATUS "  vsepr-sim --version    -> VSEPR-SIM ${PROJECT_VERSION}")
message(STATUS "  vsepr-sim --build-info -> full compiler/git/module report")
message(STATUS "")
