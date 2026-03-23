cmake_minimum_required(VERSION 3.15)

# ============================================================================
# COARSE-GRAINED LIBRARY BUILD
# ============================================================================
# Second-scale simulation library — reduced representation of atomistic systems.
# Deterministic mapping from atomistic::State to coarse_grain::BeadSystem.
#
# Namespace: coarse_grain
# Dependencies: atomistic (consumes State for mapping)
# ============================================================================

message(STATUS "")
message(STATUS "╔════════════════════════════════════════════════════════════════════════════════╗")
message(STATUS "║                                                                                ║")
message(STATUS "║              COARSE_GRAIN: Reduced-Representation Modeling Layer               ║")
message(STATUS "║                                                                                ║")
message(STATUS "╚════════════════════════════════════════════════════════════════════════════════╝")
message(STATUS "")
message(STATUS "  Modules:")
message(STATUS "    [1] Core:    Bead, BeadSystem, ConservationReport, MappingDiagnostics")
message(STATUS "    [2] Mapping: MappingRule, MappingScheme, AtomToBeadMapper")
message(STATUS "")

# Header-only at this stage — no .cpp files yet.
# Use INTERFACE library until implementation files are added.
add_library(coarse_grain INTERFACE)

target_include_directories(coarse_grain INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(coarse_grain INTERFACE
    atomistic
)

target_compile_features(coarse_grain INTERFACE cxx_std_20)

message(STATUS "  ✓ coarse_grain library configured (header-only / INTERFACE)")
message(STATUS "")
