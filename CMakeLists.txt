                  cmake_minimum_required(VERSION 3.15)
project(vsepr-sim VERSION 2.0.0 LANGUAGES CXX)

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

# Dear ImGui (for visualization)
set(IMGUI_DIR ${PROJECT_SOURCE_DIR}/third_party/imgui)
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)
include_directories(${IMGUI_DIR})
include_directories(${IMGUI_DIR}/backends)

find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(GLEW REQUIRED)

# ============================================================================
# Build Options
# ============================================================================

option(BUILD_TESTS "Build test suite" ON)
option(BUILD_APPS "Build applications" ON)
option(BUILD_VIS "Build visualization tools" ON)
option(BUILD_DEMOS "Build demo targets" OFF)

# ============================================================================
# Compiler Settings
# ============================================================================

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -pedantic)
elseif(MSVC)
    add_compile_options(/W4)
endif()

# ============================================================================

# Internal Libraries
# ============================================================================

# --- Core Library ---
add_library(vsepr_core INTERFACE)
target_include_directories(vsepr_core INTERFACE src/core)

# --- Periodic Table Library (Z=1-102 with isotopes) ---
add_library(vsepr_periodic STATIC
    src/core/periodic_table_complete.cpp
    src/core/periodic_table_data_102.cpp
    src/core/decay_chains.cpp
)
target_include_directories(vsepr_periodic PUBLIC 
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(vsepr_periodic PUBLIC vsepr_core)

# --- Simulation Library ---
# Exclude sim_thread.cpp which depends on command_router
file(GLOB SIM_SOURCES src/sim/*.cpp)
list(FILTER SIM_SOURCES EXCLUDE REGEX ".*sim_thread\\.cpp$")
add_library(vsepr_sim STATIC ${SIM_SOURCES})
target_link_libraries(vsepr_sim PUBLIC vsepr_core)
target_include_directories(vsepr_sim PUBLIC src/sim)

# --- Potentials Library ---
add_library(vsepr_pot INTERFACE)
target_include_directories(vsepr_pot INTERFACE src/pot)
target_link_libraries(vsepr_pot INTERFACE vsepr_core)

# --- Box/PBC Library ---
add_library(vsepr_box INTERFACE)
target_include_directories(vsepr_box INTERFACE src/box)
target_link_libraries(vsepr_box INTERFACE vsepr_core)

# --- Neighbor List Library ---
add_library(vsepr_nl INTERFACE)
target_include_directories(vsepr_nl INTERFACE src/nl)
target_link_libraries(vsepr_nl INTERFACE vsepr_core)

# --- Integrators Library ---
add_library(vsepr_int INTERFACE)
target_include_directories(vsepr_int INTERFACE src/int)
target_link_libraries(vsepr_int INTERFACE vsepr_core)

# --- Build System Library (Formula → Molecule) ---
add_library(vsepr_build INTERFACE)
target_include_directories(vsepr_build INTERFACE src/build)
target_link_libraries(vsepr_build INTERFACE vsepr_core vsepr_sim)

# --- Dynamic Generation Library (Real molecule generation, continuous gen) ---
add_library(vsepr_dynamic STATIC
    src/dynamic/real_molecule_generator.cpp
)
target_include_directories(vsepr_dynamic PUBLIC include ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(vsepr_dynamic PUBLIC vsepr_core vsepr_sim)

# --- Primary GUI Utilities Library ---
add_library(vsepr_gui_utils STATIC
    src/gui/batch_worker.cpp
    src/gui/continuous_generation_manager.cpp
)
target_include_directories(vsepr_gui_utils PUBLIC include ${PROJECT_SOURCE_DIR}/src)

# --- GPU Backend Library !!! ---
if(VSEPR_HAS_CUDA)
    # Check if CUDA kernel source exists before trying to compile it
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/gpu/kernels/energy_nonbonded.cu")
        # CUDA kernels library
        add_library(vsepr_cuda_kernels STATIC
            src/gpu/kernels/energy_nonbonded.cu
        )

        set_target_properties(vsepr_cuda_kernels PROPERTIES
            CUDA_SEPARABLE_COMPILATION ON
            CUDA_STANDARD 17
        )

        # Debug vs Release flags for CUDA
        target_compile_options(vsepr_cuda_kernels PRIVATE
            $<$<CONFIG:Debug>:-G -g -DENABLE_DEBUG_CHECKS=1>
            $<$<CONFIG:Release>:-O3 -lineinfo -DENABLE_DEBUG_CHECKS=0>
        )

        target_include_directories(vsepr_cuda_kernels PUBLIC include)

        # GPU backend (C++ wrapper)
        add_library(vsepr_gpu STATIC
            src/gpu/gpu_backend.cpp
        )
        target_include_directories(vsepr_gpu PUBLIC include)
        target_link_libraries(vsepr_gpu PUBLIC vsepr_core vsepr_cuda_kernels)
        target_compile_definitions(vsepr_gpu PUBLIC VSEPR_HAS_CUDA)

        message(STATUS "GPU acceleration: CUDA enabled with kernels")
    else()
        # CUDA available but kernels not implemented yet
        add_library(vsepr_gpu STATIC
            src/gpu/gpu_backend.cpp
        )
        target_include_directories(vsepr_gpu PUBLIC include)
        target_link_libraries(vsepr_gpu PUBLIC vsepr_core)
        target_compile_definitions(vsepr_gpu PUBLIC VSEPR_HAS_CUDA)

        message(STATUS "GPU acceleration: CUDA detected but kernels not built (skeleton only)")
    endif()
else()
    # CPU-only fallback
    add_library(vsepr_gpu STATIC
        src/gpu/gpu_backend.cpp
    )
    target_include_directories(vsepr_gpu PUBLIC include)
    target_link_libraries(vsepr_gpu PUBLIC vsepr_core)

    message(STATUS "GPU acceleration: CPU fallback only")
endif()

# --- Thermal Animation Library (Real-time MD simulation) ---
add_library(vsepr_thermal STATIC
    src/thermal/thermal_runner.cpp
    src/thermal/xyzc_format.cpp
)
target_include_directories(vsepr_thermal PUBLIC include ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(vsepr_thermal PUBLIC vsepr_core vsepr_sim vsepr_io)

# --- I/O Library (XYZ, XYZA formats) ---
add_library(vsepr_io STATIC
    src/io/xyz_format.cpp
)
target_include_directories(vsepr_io PUBLIC include)
target_link_libraries(vsepr_io PUBLIC vsepr_core)

# --- Scalable Rendering Library (LOD, culling, GPU instancing) ---
add_library(vsepr_render STATIC
    src/render/scalable_renderer.cpp
)
target_include_directories(vsepr_render PUBLIC include ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(vsepr_render PUBLIC vsepr_core vsepr_sim)
if(BUILD_VIS)
    target_link_libraries(vsepr_render PUBLIC OpenGL::GL GLEW::GLEW)
endif()

# --- API Façade Library (Production app-facing layer) ---
add_library(vsepr_api STATIC
    src/api/io_api.cpp
)
target_include_directories(vsepr_api PUBLIC include)
target_link_libraries(vsepr_api PUBLIC vsepr_core vsepr_io vsepr_thermal vsepr_pot)

# --- Spec Parser Library (DSL & JSON) ---
add_library(spec_parser STATIC src/spec_parser.cpp)
target_include_directories(spec_parser PUBLIC include)
target_link_libraries(spec_parser PUBLIC vsepr_core)

# --- Demo Launcher Library (Cross-platform terminal spawning) ---
add_library(vsepr_demo STATIC
    src/demo/platform_terminal.cpp
)
target_include_directories(vsepr_demo PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(vsepr_demo PUBLIC vsepr_core)

# --- Atomistic Simulation Library (Canonical CoreState, parsers, compilers) ---
add_subdirectory(atomistic)

# --- Visualization Library ---
if(BUILD_VIS)
    file(GLOB VIS_SOURCES src/vis/*.cpp)
    # Exclude broken/old versions
    list(REMOVE_ITEM VIS_SOURCES 
        "${CMAKE_SOURCE_DIR}/src/vis/command_parser.cpp.broken"
        "${CMAKE_SOURCE_DIR}/src/vis/command_parser.cpp.old"
    )
    
    # Add geometry utilities
    set(VIS_GEOMETRY_SOURCES
        src/vis/geometry/sphere.cpp
        src/vis/geometry/cylinder.cpp
    )
    
    # Add modern renderers
    set(VIS_RENDERER_SOURCES
        src/vis/renderer_base.cpp
        src/vis/renderer_classic.cpp
    )
    
    # Add visualization utilities
    set(VIS_UTIL_SOURCES
        src/vis/animation.cpp
        src/vis/pbc_visualizer.cpp
        src/vis/crystal_grid.cpp
    )
    
    # Add interactive UI utilities
    set(VIS_UI_SOURCES
        src/vis/ui_theme.cpp
        src/vis/picking.cpp
        src/vis/analysis_panel.cpp
    )
    
    # Add sim_thread for visualization support
    add_library(vsepr_vis STATIC 
        ${VIS_SOURCES} 
        ${VIS_GEOMETRY_SOURCES}
        ${VIS_RENDERER_SOURCES}
        ${VIS_UTIL_SOURCES}
        ${VIS_UI_SOURCES}
        ${IMGUI_SOURCES}
        src/sim/sim_thread.cpp
        src/command_router.cpp
    )
    target_link_libraries(vsepr_vis 
        PUBLIC vsepr_core vsepr_sim
        PUBLIC OpenGL::GL glfw GLEW::GLEW
    )
    target_include_directories(vsepr_vis PUBLIC src/vis ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/include)
endif()

# ============================================================================

# Applications
# ============================================================================

if(BUILD_APPS)
    # CLI Tool
    add_executable(vsepr-cli apps/vsepr-cli/main.cpp)
    target_link_libraries(vsepr-cli vsepr_sim vsepr_pot vsepr_box vsepr_nl)
    target_include_directories(vsepr-cli PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/include)
    install(TARGETS vsepr-cli DESTINATION bin)
    
    # Viewer (with visualization)
    if(BUILD_VIS)
        add_executable(vsepr-view 
            apps/vsepr-view/main.cpp
        )
        target_link_libraries(vsepr-view 
            vsepr_sim vsepr_pot vsepr_box vsepr_nl vsepr_int vsepr_vis
        )
        target_include_directories(vsepr-view PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/include)
        install(TARGETS vsepr-view DESTINATION bin)
    endif()
    
    # NEW Unified CLI - domain-aware grammar (Phase 1: Plumbing)
    # Replaces old command registry approach
    add_library(vsepr_cli STATIC
        src/cli/parse.cpp
        src/cli/run_context.cpp
        src/cli/actions_emit.cpp
        src/cli/actions_relax.cpp
        src/cli/actions_form.cpp
        src/cli/actions_test.cpp
        src/cli/emit_output.cpp
        src/cli/emit_crystal.cpp
        src/cli/emit_gas.cpp
        src/cli/metrics_rdf.cpp
        src/cli/metrics_coordination.cpp
        src/cli/rdf_accumulator.cpp
        src/cli/viewer_launcher.cpp
    )
    target_include_directories(vsepr_cli PUBLIC include/ ${PROJECT_SOURCE_DIR})
    target_link_libraries(vsepr_cli PUBLIC vsepr_core atomistic vsepr_io)

    add_executable(vsepr apps/vsepr.cpp)
    target_link_libraries(vsepr vsepr_cli vsepr_sim vsepr_pot vsepr_io vsepr_thermal atomistic)
    target_include_directories(vsepr PRIVATE src/ include/)

    # Equipartition test (Langevin thermostat validation)
    add_executable(test_equipartition apps/test_equipartition.cpp)
    target_link_libraries(test_equipartition atomistic vsepr_core)
    target_include_directories(test_equipartition PRIVATE ${PROJECT_SOURCE_DIR})

    # ========================================================================
    # FUNDAMENTAL VALIDATION: Problem 1 & 2 (Two-Body & Three-Body)
    # ========================================================================
    # These are THE most important tests - if these fail, everything fails!

    add_executable(problem1_two_body_lj tests/problem1_two_body_lj.cpp)
    target_link_libraries(problem1_two_body_lj atomistic)
    target_include_directories(problem1_two_body_lj PRIVATE ${PROJECT_SOURCE_DIR})

    add_executable(problem2_three_body_cluster tests/problem2_three_body_cluster.cpp)
    target_link_libraries(problem2_three_body_cluster atomistic)
    target_include_directories(problem2_three_body_cluster PRIVATE ${PROJECT_SOURCE_DIR})

    # ========================================================================

    # Simple MB test
    add_executable(test_mb_simple apps/test_mb_simple.cpp)
    target_link_libraries(test_mb_simple atomistic vsepr_core)
    target_include_directories(test_mb_simple PRIVATE ${PROJECT_SOURCE_DIR})

    # Surgical Langevin debug test
    add_executable(test_langevin_debug apps/test_langevin_debug.cpp)
    target_link_libraries(test_langevin_debug atomistic vsepr_core)
    target_include_directories(test_langevin_debug PRIVATE ${PROJECT_SOURCE_DIR})

    # BAOAB multi-atom test
    add_executable(test_baoab_multi apps/test_baoab_multi.cpp)
    target_link_libraries(test_baoab_multi atomistic vsepr_core)
    target_include_directories(test_baoab_multi PRIVATE ${PROJECT_SOURCE_DIR})

    # APPLICATION TEST: Thermal formation vs quench-only
    add_executable(test_thermal_vs_quench apps/test_thermal_vs_quench.cpp)
    target_link_libraries(test_thermal_vs_quench atomistic vsepr_core)
    target_include_directories(test_thermal_vs_quench PRIVATE ${PROJECT_SOURCE_DIR})

    # APPLICATION TEST: Ar13 cluster (simplified)
    add_executable(test_thermal_ar13 apps/test_thermal_ar13.cpp)
    target_link_libraries(test_thermal_ar13 atomistic vsepr_core)
    target_include_directories(test_thermal_ar13 PRIVATE ${PROJECT_SOURCE_DIR})

    # PMF Calculator test
    add_executable(test_pmf_calculator apps/test_pmf_calculator.cpp)
    target_link_libraries(test_pmf_calculator atomistic vsepr_core)
    target_include_directories(test_pmf_calculator PRIVATE ${PROJECT_SOURCE_DIR})

    # RDF Accumulator test
    add_executable(test_rdf_accumulator apps/test_rdf_accumulator.cpp)
    target_link_libraries(test_rdf_accumulator vsepr_cli atomistic vsepr_core)
    target_include_directories(test_rdf_accumulator PRIVATE ${PROJECT_SOURCE_DIR})

    # APPLICATION VALIDATION SUITE: Comprehensive E1-E3 tests
    add_executable(test_application_validation apps/test_application_validation.cpp)
    target_link_libraries(test_application_validation atomistic vsepr_core)
    target_include_directories(test_application_validation PRIVATE ${PROJECT_SOURCE_DIR})

    # SAFETY RAILS TEST: B3 safety checks
    add_executable(test_safety_rails apps/test_safety_rails.cpp)
    target_link_libraries(test_safety_rails atomistic vsepr_core)
    target_include_directories(test_safety_rails PRIVATE ${PROJECT_SOURCE_DIR})

    # Link visualization if available
    if(BUILD_VIS)
        target_link_libraries(vsepr vsepr_vis)
        target_compile_definitions(vsepr PRIVATE BUILD_VISUALIZATION)
    endif()

    # Old CLI (quarantined, will be removed)
    # set(VSEPR_OLD_SOURCES
    #     apps/cli.cpp
    #     src/cli/cmd_help.cpp
    #     src/cli/cmd_version.cpp
    #     src/cli/cmd_build.cpp
    #     src/cli/cmd_viz.cpp
    #     src/cli/cmd_therm.cpp
    # )
    # add_executable(vsepr-old ${VSEPR_OLD_SOURCES})
    # target_link_libraries(vsepr-old vsepr_sim vsepr_pot vsepr_core vsepr_io vsepr_thermal)
    
    # Batch runner with DSL/JSON specifications
    add_executable(vsepr_batch apps/vsepr_batch.cpp)
    target_link_libraries(vsepr_batch spec_parser vsepr_core)
    
    # XYZ Suite Tester - Production API validation
    add_executable(xyz_suite_test apps/xyz_suite_test.cpp)
    target_link_libraries(xyz_suite_test vsepr_api vsepr_io vsepr_core)
    target_include_directories(xyz_suite_test PRIVATE include/)
    if(WIN32)
        target_link_libraries(xyz_suite_test psapi)
    endif()
    
    # Meso Relax - Mesoscale FIRE minimization demo
    add_executable(meso-relax apps/meso-relax.cpp)
    target_link_libraries(meso-relax atomistic vsepr_io vsepr_core)
    target_include_directories(meso-relax PRIVATE ${PROJECT_SOURCE_DIR})

    # Atomistic-Sim - Unified simulation & prediction tool (REPLACES vsepr_batch)
    add_executable(meso-sim apps/meso-sim.cpp apps/meso-sim-modes.cpp)
    target_link_libraries(meso-sim atomistic vsepr_io vsepr_core vsepr_pot)
    target_include_directories(meso-sim PRIVATE ${PROJECT_SOURCE_DIR})

    # Atomistic-Discover - Deterministic reaction discovery system
    add_executable(meso-discover apps/meso-discover.cpp)
    target_link_libraries(meso-discover atomistic vsepr_io vsepr_core vsepr_pot)
    target_include_directories(meso-discover PRIVATE ${PROJECT_SOURCE_DIR})

    # Atomistic-Align - Kabsch alignment with camera tracking
    add_executable(meso-align apps/meso-align.cpp)
    target_link_libraries(meso-align atomistic vsepr_io vsepr_core)
    target_include_directories(meso-align PRIVATE ${PROJECT_SOURCE_DIR})

    # Atomistic-Build - Interactive molecular builder CLI
    add_executable(meso-build apps/meso-build.cpp)
    target_link_libraries(meso-build atomistic vsepr_io vsepr_core)
    target_include_directories(meso-build PRIVATE ${PROJECT_SOURCE_DIR})

    # Demo Launcher - Cross-platform terminal spawner (like xclock)
    add_executable(demo apps/demo.cpp)
    target_link_libraries(demo vsepr_demo vsepr_core)
    target_include_directories(demo PRIVATE ${PROJECT_SOURCE_DIR}/src)
    install(TARGETS demo DESTINATION bin)

    # QA Golden Tests - Pre-Batching Quality & Reproducibility Milestone
    add_executable(qa_golden_tests apps/qa_golden_tests.cpp)
    target_link_libraries(qa_golden_tests atomistic vsepr_io vsepr_core)
    target_include_directories(qa_golden_tests PRIVATE ${PROJECT_SOURCE_DIR})
    install(TARGETS qa_golden_tests DESTINATION bin)

    # Simple NaCl Test - Direct PBC verification (no test framework)
    add_executable(simple_nacl_test apps/simple_nacl_test.cpp)
    target_link_libraries(simple_nacl_test atomistic vsepr_core)
    target_include_directories(simple_nacl_test PRIVATE ${PROJECT_SOURCE_DIR})
    install(TARGETS simple_nacl_test DESTINATION bin)

    # Simple Molecular Viewer - Basic ball-and-stick with file watching
    if(BUILD_VIS)
        add_executable(simple-viewer apps/simple-viewer.cpp)
        target_link_libraries(simple-viewer vsepr_vis vsepr_core)
        target_include_directories(simple-viewer PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/include)
        install(TARGETS simple-viewer DESTINATION bin)
    endif()

    # Crystal Grid Viewer - Crystallographic visualization with coordination polyhedra
    if(BUILD_VIS)
        add_executable(crystal-viewer apps/crystal-viewer.cpp)
        target_link_libraries(crystal-viewer vsepr_vis vsepr_core)
        target_include_directories(crystal-viewer PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/include)
        install(TARGETS crystal-viewer DESTINATION bin)
    endif()

    # Card Catalog Viewer - ImGui browser for simulation run cards
    if(BUILD_VIS)
        add_executable(card-viewer apps/card_viewer.cpp ${IMGUI_SOURCES})
        target_link_libraries(card-viewer vsepr_core vsepr_io OpenGL::GL glfw GLEW::GLEW)
        target_include_directories(card-viewer PRIVATE
            ${PROJECT_SOURCE_DIR}/src
            ${PROJECT_SOURCE_DIR}/include
            ${IMGUI_DIR}
            ${IMGUI_DIR}/backends
        )
        install(TARGETS card-viewer DESTINATION bin)
    endif()

    # Compute Forces - CLI tool for force field computation
    add_executable(compute_forces apps/compute_forces.cpp)
    target_link_libraries(compute_forces vsepr_core vsepr_io atomistic)
    target_include_directories(compute_forces PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/include)
    install(TARGETS compute_forces DESTINATION bin)

    # Professional GUI (requires BUILD_VIS=ON)
    if(BUILD_VIS)
        # Note: vsepr-gui/main.cpp is a prototype - needs implementation before building
        # add_executable(vsepr-gui apps/vsepr-gui/main.cpp ${IMGUI_SOURCES})
        # target_link_libraries(vsepr-gui vsepr_vis vsepr_core vsepr_io atomistic OpenGL::GL glfw GLEW::GLEW)
        # target_include_directories(vsepr-gui PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/include ${IMGUI_DIR} ${IMGUI_DIR}/backends)
        # install(TARGETS vsepr-gui DESTINATION bin)
        message(STATUS "vsepr-gui: Prototype exists, implementation needed (commented out)")
    endif()
endif()

# ============================================================================


# Tests
# ============================================================================

if(BUILD_TESTS)
    enable_testing()
    
    # Geometry operations
    add_executable(geom_ops_tests tests/geom_ops_tests.cpp)
    target_link_libraries(geom_ops_tests vsepr_core vsepr_sim)
    add_test(NAME GeometryOpsTest COMMAND geom_ops_tests)
    
    # Energy model
    add_executable(energy_tests tests/energy_tests.cpp)
    target_link_libraries(energy_tests vsepr_sim vsepr_pot)
    add_test(NAME EnergyTest COMMAND energy_tests)
    
    # Optimizer
    add_executable(optimizer_tests tests/optimizer_tests.cpp)
    target_link_libraries(optimizer_tests vsepr_sim vsepr_pot)
    add_test(NAME OptimizerTest COMMAND optimizer_tests)
    
    # Angles
    add_executable(angle_tests tests/angle_tests.cpp)
    target_link_libraries(angle_tests vsepr_sim vsepr_pot)
    add_test(NAME AngleTest COMMAND angle_tests)
    
    # VSEPR
    add_executable(vsepr_tests tests/vsepr_tests.cpp)
    target_link_libraries(vsepr_tests vsepr_sim vsepr_pot)
    add_test(NAME VSEPRTest COMMAND vsepr_tests)
    
    # Torsions
    add_executable(torsion_tests tests/torsion_tests.cpp)
    target_link_libraries(torsion_tests vsepr_sim vsepr_pot)
    add_test(NAME TorsionTest COMMAND torsion_tests)
    
    add_executable(torsion_validation_tests tests/torsion_validation_tests.cpp)
    target_link_libraries(torsion_validation_tests vsepr_sim vsepr_pot)
    add_test(NAME TorsionValidationTest COMMAND torsion_validation_tests)
    
    add_executable(alkane_torsion_tests tests/alkane_torsion_tests.cpp)
    target_link_libraries(alkane_torsion_tests vsepr_sim vsepr_pot)
    add_test(NAME AlkaneTorsionTest COMMAND alkane_torsion_tests)
    
    # Scan tests
    add_executable(butane_scan tests/butane_scan.cpp)
    target_link_libraries(butane_scan vsepr_sim vsepr_pot)
    add_test(NAME ButaneScanTest COMMAND butane_scan)
    
    # Domain tests
    add_executable(vsepr_domain_test tests/vsepr_domain_test.cpp)
    target_link_libraries(vsepr_domain_test vsepr_sim vsepr_pot)
    
    # Basic validation tests (post-refactoring)
    add_executable(basic_molecule_validation tests/basic_molecule_validation.cpp)
    target_link_libraries(basic_molecule_validation vsepr_sim vsepr_pot vsepr_core)
    add_test(NAME BasicMoleculeValidation COMMAND basic_molecule_validation)
    
    add_executable(basic_isomer_validation tests/basic_isomer_validation.cpp)
    target_link_libraries(basic_isomer_validation vsepr_sim vsepr_pot vsepr_core)
    add_test(NAME BasicIsomerValidation COMMAND basic_isomer_validation)
    
    # Ethane torsion barrier test (critical for torsion validation)
    add_executable(test_ethane_torsion tests/test_ethane_torsion.cpp)
    target_link_libraries(test_ethane_torsion vsepr_sim vsepr_pot vsepr_core)
    add_test(NAME EthaneTorsionBarrier COMMAND test_ethane_torsion)
    
    add_test(NAME VSEPRDomainTest COMMAND vsepr_domain_test)
    
    # Standalone optimization
    add_executable(vsepr_standalone_opt tests/vsepr_standalone_opt.cpp)
    target_link_libraries(vsepr_standalone_opt vsepr_sim vsepr_pot)
    
    # Energy model v03
    add_executable(energy_model_v03_test tests/energy_model_v03_test.cpp)
    target_link_libraries(energy_model_v03_test vsepr_sim vsepr_pot)
    
    # PBC tests
    add_executable(pbc_test tests/pbc_test.cpp)
    target_link_libraries(pbc_test vsepr_core vsepr_box)
    add_test(NAME PBCTest COMMAND pbc_test)
    
    # PBC verification (Phase 1 technical requirements)
    add_executable(pbc_verification tests/pbc_verification.cpp)
    target_link_libraries(pbc_verification vsepr_core vsepr_box)
    add_test(NAME PBCVerification COMMAND pbc_verification)
    
    # PBC Phase 2 (physics parity tests)
    add_executable(pbc_phase2_physics tests/pbc_phase2_physics.cpp)
    target_link_libraries(pbc_phase2_physics vsepr_core vsepr_box)
    add_test(NAME PBCPhase2Physics COMMAND pbc_phase2_physics)
    
    # Phase 4: Performance baselines
    add_executable(pbc_phase4_perf tests/pbc_phase4_perf.cpp)
    target_link_libraries(pbc_phase4_perf vsepr_core vsepr_box)
    
    # Phase 5: Golden regression tests
    if(EXISTS ${CMAKE_SOURCE_DIR}/tests/pbc_phase5_regression.cpp)
        add_executable(pbc_phase5_regression tests/pbc_phase5_regression.cpp)
        target_link_libraries(pbc_phase5_regression vsepr_core vsepr_box)
        add_test(NAME PBCPhase5Golden COMMAND pbc_phase5_regression)
    else()
        message(WARNING "Skipping pbc_phase5_regression: source not found")
    endif()
    
    # Periodic table tests (Z=1-102 with isotope support)
    add_executable(test_periodic_table_102 tests/test_periodic_table_102.cpp)
    target_link_libraries(test_periodic_table_102 vsepr_periodic)
    add_test(NAME PeriodicTable102Test COMMAND test_periodic_table_102)

    # Decay chains tests (four natural series)
    add_executable(test_decay_chains tests/test_decay_chains.cpp)
    target_link_libraries(test_decay_chains vsepr_periodic)
    add_test(NAME DecayChainsTest COMMAND test_decay_chains)
    
    # Formula builder tests (production core API)
# TEMPORARILY DISABLED - API MISMATCH:     add_executable(formula_builder_tests tests/formula_builder_tests.cpp)
# TEMPORARILY DISABLED - API MISMATCH:     target_link_libraries(formula_builder_tests vsepr_core vsepr_sim vsepr_build vsepr_pot)
# TEMPORARILY DISABLED - API MISMATCH:     add_test(NAME FormulaBuilderTests COMMAND formula_builder_tests)
# TEMPORARILY DISABLED - API MISMATCH:     
# TEMPORARILY DISABLED - API MISMATCH:     # Conformer finder test
# TEMPORARILY DISABLED - API MISMATCH:     add_executable(conformer_test tests/conformer_test.cpp)
# TEMPORARILY DISABLED - API MISMATCH:     target_link_libraries(conformer_test vsepr_core vsepr_sim vsepr_build vsepr_pot)
# TEMPORARILY DISABLED - API MISMATCH:     add_test(NAME ConformerTest COMMAND conformer_test)
# TEMPORARILY DISABLED - API MISMATCH:     
# TEMPORARILY DISABLED - API MISMATCH:     # Isomer enumeration and identification test
# TEMPORARILY DISABLED - API MISMATCH:     add_executable(isomer_test tests/isomer_test.cpp)
# TEMPORARILY DISABLED - API MISMATCH:     target_link_libraries(isomer_test vsepr_core vsepr_sim vsepr_build vsepr_pot)
# TEMPORARILY DISABLED - API MISMATCH:     add_test(NAME IsomerTest COMMAND isomer_test)
    
# TEMPORARILY DISABLED - API MISMATCH:     # Phase 3: Isomerism validation (cis/trans)
# TEMPORARILY DISABLED - API MISMATCH:     add_executable(phase3_isomer_validation tests/phase3_isomer_validation.cpp)
# TEMPORARILY DISABLED - API MISMATCH:     target_link_libraries(phase3_isomer_validation vsepr_core vsepr_pot vsepr_int)
# TEMPORARILY DISABLED - API MISMATCH:     add_test(NAME Phase3IsomerValidation COMMAND phase3_isomer_validation)
# TEMPORARILY DISABLED - API MISMATCH:     
# TEMPORARILY DISABLED - API MISMATCH:     # Phase 4: Coordination geometry variants
# TEMPORARILY DISABLED - API MISMATCH:     add_executable(phase4_coordination_variants tests/phase4_coordination_variants.cpp)
# TEMPORARILY DISABLED - API MISMATCH:     target_link_libraries(phase4_coordination_variants vsepr_core vsepr_pot vsepr_int)
# TEMPORARILY DISABLED - API MISMATCH:     add_test(NAME Phase4CoordinationVariants COMMAND phase4_coordination_variants)
# TEMPORARILY DISABLED - API MISMATCH:     
# TEMPORARILY DISABLED - API MISMATCH:     # Phase 5: Ionic manifold testing
# TEMPORARILY DISABLED - API MISMATCH:     add_executable(phase5_ionic_manifold tests/phase5_ionic_manifold.cpp)
# TEMPORARILY DISABLED - API MISMATCH:     target_link_libraries(phase5_ionic_manifold vsepr_core vsepr_pot vsepr_int)
# TEMPORARILY DISABLED - API MISMATCH:     add_test(NAME Phase5IonicManifold COMMAND phase5_ionic_manifold)
    
    # Chemistry basic test (typing & thermodynamics)
    add_executable(chemistry_basic_test tests/chemistry_basic_test.cpp)
    target_link_libraries(chemistry_basic_test vsepr_core)
    add_test(NAME ChemistryBasic COMMAND chemistry_basic_test)
    
    # Chemistry universal test (organic + coordination)
# TEMPORARILY DISABLED - API MISMATCH:     add_executable(chemistry_universal_test tests/chemistry_universal_test.cpp)
# TEMPORARILY DISABLED - API MISMATCH:     target_link_libraries(chemistry_universal_test vsepr_core)
# TEMPORARILY DISABLED - API MISMATCH:     add_test(NAME ChemistryUniversal COMMAND chemistry_universal_test)
    
    # Improved nonbonded parameters test (element-specific LJ)
    add_executable(test_improved_nonbonded tests/test_improved_nonbonded.cpp)
    target_link_libraries(test_improved_nonbonded vsepr_core vsepr_sim vsepr_pot)
    add_test(NAME ImprovedNonbondedTest COMMAND test_improved_nonbonded)
    
    # Spec parser test (DSL & JSON)
    add_executable(test_spec_parser tests/test_spec_parser.cpp)
    target_link_libraries(test_spec_parser spec_parser)
    add_test(NAME SpecParserTest COMMAND test_spec_parser)
    
    # Formula parser test (namespaced, comprehensive)
    add_executable(test_formula_parser tests/test_formula_parser.cpp)
    target_link_libraries(test_formula_parser vsepr_core)
    add_test(NAME FormulaParserTest COMMAND test_formula_parser)
    
    # Formula fuzz tester (intensive automated testing)
    add_executable(formula_fuzz_tester tests/formula_fuzz_tester.cpp)
    target_link_libraries(formula_fuzz_tester vsepr_core)
    
    # Phase 1: Element DB + Manifold Sanity (brutal validation)
    add_executable(test_element_db_phase1_simple tests/test_element_db_phase1_simple.cpp)
    target_link_libraries(test_element_db_phase1_simple vsepr_core)
    add_test(NAME ElementDBPhase1Simple COMMAND test_element_db_phase1_simple)
    
    # Phase 2: Complex Molecules (user workflow simulation)
    add_executable(phase2_complex_molecules tests/phase2_complex_molecules.cpp)
    target_link_libraries(phase2_complex_molecules vsepr_core)
    add_test(NAME Phase2ComplexMolecules COMMAND phase2_complex_molecules)
    
    # PBC example/demo
    add_executable(pbc_example docs/pbc_example.cpp)
    target_link_libraries(pbc_example vsepr_core vsepr_box)
    
    # Phase 1: Batch Processing
    add_executable(test_batch_processing tests/test_batch_processing.cpp)
    target_link_libraries(test_batch_processing vsepr_dynamic vsepr_gui_utils vsepr_io vsepr_core vsepr_sim vsepr_gui)
    add_test(NAME BatchProcessingTest COMMAND test_batch_processing)
    
    # Phase 2: Thermal Animation
    add_executable(test_thermal_animation tests/test_thermal_animation.cpp)
    target_link_libraries(test_thermal_animation vsepr_thermal vsepr_io vsepr_core vsepr_sim vsepr_dynamic)
    add_test(NAME ThermalAnimationTest COMMAND test_thermal_animation)
    
    # Phase 3: Continuous Generation
    add_executable(test_continuous_generation tests/test_continuous_generation.cpp)
    target_link_libraries(test_continuous_generation vsepr_gui_utils vsepr_dynamic vsepr_io vsepr_core vsepr_sim vsepr_gui)
    add_test(NAME ContinuousGenerationTest COMMAND test_continuous_generation)

    # Ensemble Consistency & Perturbation Invariance Test
    # TODO: Fix ensemble_consistency_test (needs CoreState from qa_golden_tests.cpp)
    # add_executable(ensemble_consistency_test tests/ensemble_consistency_test.cpp)
    # target_link_libraries(ensemble_consistency_test meso vsepr_core vsepr_io)
    # target_include_directories(ensemble_consistency_test PRIVATE ${PROJECT_SOURCE_DIR})
    # add_test(NAME EnsembleConsistencyTest COMMAND ensemble_consistency_test)
endif()

# ============================================================================

# GUI Applications (New!)
# ============================================================================

option(BUILD_GUI "Build GUI applications" ON)

if(BUILD_GUI AND BUILD_VIS)
    message(STATUS "Building GUI applications...")
    
    # GUI Library (context menus + data pipes + ImGui integration)
    add_library(vsepr_gui STATIC
        src/gui/context_menu.cpp
        src/gui/data_pipe.cpp
        src/gui/imgui_integration.cpp
        src/gui/pokedex_gui.cpp
        src/gui/unified_launcher.cpp
        ${IMGUI_SOURCES}
    )
    
    target_include_directories(vsepr_gui PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
    )
    
    target_link_libraries(vsepr_gui PUBLIC
        vsepr_core
        OpenGL::GL
        glfw
        GLEW::GLEW
    )
    
    # 1. Simple console demo (no ImGui)
    add_executable(vsepr_gui_demo
        examples/gui_demo.cpp
        src/gui/context_menu.cpp
        src/gui/data_pipe.cpp
    )
    
    target_include_directories(vsepr_gui_demo PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    
    # 2. Elevated GUI application
    add_executable(vsepr_gui_elevated
        examples/elevated_gui_app.cpp
    )
    
    target_link_libraries(vsepr_gui_elevated PRIVATE vsepr_gui)
    
    # 3. Pokedex browser
    add_executable(pokedex_app
        examples/pokedex_app.cpp
    )
    
    target_link_libraries(pokedex_app PRIVATE vsepr_gui)
    
    # 4. Unified launcher (main entry point)
    add_executable(vsepr_gui_main
        src/vsepr_gui_main.cpp
    )
    
    target_link_libraries(vsepr_gui_main PRIVATE vsepr_gui)
    
    # 5. Live VSEPR integration (GUI connected to real simulation engine)
    add_executable(vsepr_gui_live
        examples/vsepr_gui_live.cpp
        src/dynamic/dynamic_molecule_builder.cpp
        src/render/molecular_renderer.cpp
    )
    
    target_link_libraries(vsepr_gui_live PRIVATE 
        vsepr_gui 
        vsepr_sim 
        vsepr_core
    )
    
    message(STATUS "GUI targets configured:")
    message(STATUS "  - vsepr_gui (library)")
    message(STATUS "  - vsepr_gui_demo (console demo)")
    message(STATUS "  - vsepr_gui_elevated (ImGui app)")
    message(STATUS "  - pokedex_app (Pokedex browser)")
    message(STATUS "  - vsepr_gui_main (unified launcher)")
    message(STATUS "  - vsepr_gui_live (live VSEPR engine integration)")
elseif(BUILD_GUI)
    message(WARNING "BUILD_GUI=ON but BUILD_VIS=OFF. GUI requires visualization support.")
endif()

# ============================================================================
# Summary
# ============================================================================

message(STATUS "")
message(STATUS "=== VSEPR Simulator Configuration ===")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "Build tests: ${BUILD_TESTS}")
message(STATUS "Build apps: ${BUILD_APPS}")
message(STATUS "Build visualization: ${BUILD_VIS}")
message(STATUS "Build GUI: ${BUILD_GUI}")
message(STATUS "")
message(STATUS "Internal Libraries:")
message(STATUS "  - vsepr_core (header-only)")
message(STATUS "  - vsepr_sim (static)")
message(STATUS "  - vsepr_pot (header-only)")
message(STATUS "  - vsepr_box (header-only)")
message(STATUS "  - vsepr_nl (header-only)")
message(STATUS "  - vsepr_int (header-only)")
message(STATUS "  - vsepr_build (header-only - formula→molecule)")
message(STATUS "  - vsepr_dynamic (static - real molecule generation)")
message(STATUS "  - vsepr_gui_utils (static - GUI utilities)")
message(STATUS "  - vsepr_demo (static - cross-platform terminal launcher)")
message(STATUS "  - vsepr_io (static - .xyz/.xyzA file I/O)")
message(STATUS "  - vsepr_thermal (static - .xyzC thermal pathways)")
message(STATUS "  - vsepr_render (static - scalable rendering)")
message(STATUS "  - vsepr_api (static - production app-facing façade)")
if(BUILD_VIS)
    message(STATUS "  - vsepr_vis (static)")
endif()
if(BUILD_GUI AND BUILD_VIS)
    message(STATUS "  - vsepr_gui (static - context menus + data pipes + ImGui)")
endif()
message(STATUS "")
