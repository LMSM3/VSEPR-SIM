# ============================================================================
# ELECTROHYDRODYNAMIC SIMULATION LIBRARY BUILD
# ============================================================================
# Continuum-scale multiphysics library — coupled fluid flow, electrostatics,
# and ionic transport in structured channel geometries.
#
# Namespace: vsepr::ehd
# Dependencies: vsepr_core
#
# Architecture (five stages):
#   [1] CAD     — Parametric geometry generation, STEP-style body export
#   [2] Domain  — Fluid/solid domain extraction, named regions, boundary tags
#   [3] Physics — Flow model, electrostatic model, ion transport, coupled solver
#   [4] Mesh    — Mesh controls, inflation layers, adaptive refinement
#   [5] Post    — Contour export, streamline export, section probes, tables
# ============================================================================

message(STATUS "")
message(STATUS "╔════════════════════════════════════════════════════════════════════════════════╗")
message(STATUS "║                                                                                ║")
message(STATUS "║       EHD: Electrohydrodynamic Multiphysics Simulation                        ║")
message(STATUS "║                                                                                ║")
message(STATUS "╚════════════════════════════════════════════════════════════════════════════════╝")
message(STATUS "")
message(STATUS "  Stages:")
message(STATUS "    [1] CAD:     HelixGenerator, TubeBodyGenerator, ElectrodeLayout, StepExport")
message(STATUS "    [2] Domain:  DomainExtract, NamedRegions, BoundaryTags")
message(STATUS "    [3] Physics: FlowModel, ElectrostaticModel, IonTransportModel, CoupledSolver")
message(STATUS "    [4] Mesh:    MeshControls, InflationLayers, AdaptiveRefinement")
message(STATUS "    [5] Post:    ContourExport, StreamlineExport, SectionProbe, ComparisonTables")
message(STATUS "")

# Header-only at this stage — all definitions are in .hpp files.
# Promotes INTERFACE until .cpp compilation units are added.
add_library(vsepr_ehd INTERFACE)

target_include_directories(vsepr_ehd INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

target_link_libraries(vsepr_ehd INTERFACE
    vsepr_core
)

target_compile_features(vsepr_ehd INTERFACE cxx_std_20)

message(STATUS "  ✓ vsepr_ehd library configured (header-only / INTERFACE)")
message(STATUS "")
