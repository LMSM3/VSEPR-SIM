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
option(BUILD_VIS "Build visualization tools (vsepr-view, vsepr_vis)" ON)
option(BUILD_DEMOS "Build demo targets" OFF)

find_package(nlohmann_json QUIET)

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

# Optional GLM (header-only math library used by vis/render code)
find_package(glm QUIET)
if(glm_FOUND)
    message(STATUS "GLM found — enabling VSEPR_HAS_GLM")
    target_compile_definitions(vsepr_io PUBLIC VSEPR_HAS_GLM)
else()
    message(STATUS "GLM not found — GLM helpers disabled in vsepr_io")
endif()

# --- Scalable Rendering Library (LOD, culling, GPU instancing) ---
# Requires GLM and OpenGL — only built when visualization is enabled
if(BUILD_VIS)
    add_library(vsepr_render STATIC
        src/render/scalable_renderer.cpp
    )
    target_include_directories(vsepr_render PUBLIC include ${PROJECT_SOURCE_DIR}/src)
    target_link_libraries(vsepr_render PUBLIC vsepr_core vsepr_sim OpenGL::GL GLEW::GLEW)
    # GLM GTX (experimental) extensions used in scalable_renderer — must be enabled explicitly
    target_compile_definitions(vsepr_render PUBLIC GLM_ENABLE_EXPERIMENTAL)
    if(glm_FOUND)
        target_compile_definitions(vsepr_render PUBLIC VSEPR_HAS_GLM)
    endif()
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

# --- Coarse-Grained Modeling Library (Bead mapping, reduced representation) ---
add_subdirectory(coarse_grain)

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
        src/cli/cg_commands.cpp
    )

    # Add crystal visualizer bridge if visualization enabled
    if(BUILD_VIS)
        target_sources(vsepr_cli PRIVATE src/cli/crystal_visualizer.cpp)
        target_link_libraries(vsepr_cli PUBLIC vsepr_vis)
        target_compile_definitions(vsepr_cli PRIVATE ENABLE_CRYSTAL_VIZ)
        target_compile_definitions(vsepr_cli PUBLIC BUILD_VISUALIZATION)
    endif()

    target_include_directories(vsepr_cli PUBLIC include/ ${PROJECT_SOURCE_DIR})
    target_link_libraries(vsepr_cli PUBLIC vsepr_core atomistic vsepr_io coarse_grain)

    add_executable(vsepr apps/vsepr.cpp)
    target_link_libraries(vsepr vsepr_cli vsepr_sim vsepr_pot vsepr_io vsepr_thermal atomistic)
    target_include_directories(vsepr PRIVATE src/ include/ ${PROJECT_SOURCE_DIR})

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

    # SCF Polarization Phase 1 — Ar dimer + water trimer (50-seed determinism)
    add_executable(test_scf_polarization tests/test_scf_polarization.cpp)
    target_link_libraries(test_scf_polarization atomistic)
    target_include_directories(test_scf_polarization PRIVATE ${PROJECT_SOURCE_DIR})

    # Polarizability model fitter (offline tool, reads data/polarizability_ref.csv)
    add_executable(fit_alpha_model tools/fit_alpha_model.cpp)
    target_link_libraries(fit_alpha_model atomistic)
    target_include_directories(fit_alpha_model PRIVATE ${PROJECT_SOURCE_DIR})

    # Auto-fit: self-improving infinite training loop
    add_executable(auto_fit_alpha tools/auto_fit_alpha.cpp)
    target_link_libraries(auto_fit_alpha atomistic)
    target_include_directories(auto_fit_alpha PRIVATE ${PROJECT_SOURCE_DIR})

    # Alpha model descriptor & prediction tests (T1/T2/T3/T4)
    add_executable(test_alpha_model tests/test_alpha_model.cpp)
    target_link_libraries(test_alpha_model atomistic)
    target_include_directories(test_alpha_model PRIVATE ${PROJECT_SOURCE_DIR})

    # Polarization Phases 2-4 validation (Thole damping, energy/forces, campaign)
    add_executable(test_polarization_phases tests/test_polarization_phases.cpp)
    target_link_libraries(test_polarization_phases atomistic)
    target_include_directories(test_polarization_phases PRIVATE ${PROJECT_SOURCE_DIR})

    # Phase 6: Nuclear Chemical Module (Z=79-120, ext module, actinide regression)
    add_executable(test_nuclear_chemical tests/test_nuclear_chemical.cpp)
    target_link_libraries(test_nuclear_chemical atomistic)
    target_include_directories(test_nuclear_chemical PRIVATE ${PROJECT_SOURCE_DIR})

    # v2.6.1: AlphaCalibrator — 1% auto-override gate (runtime self-calibration)
    add_executable(test_alpha_calibrator tests/test_alpha_calibrator.cpp)
    target_link_libraries(test_alpha_calibrator atomistic)
    target_include_directories(test_alpha_calibrator PRIVATE ${PROJECT_SOURCE_DIR})

    # v2.6.3: Nuclear Stability — ab-initio stability predictor from Z
    add_executable(test_nuclear_stability tests/test_nuclear_stability.cpp)
    target_link_libraries(test_nuclear_stability atomistic)
    target_include_directories(test_nuclear_stability PRIVATE ${PROJECT_SOURCE_DIR})

    # Experimental data matching — near-atomistic nanoparticle regime (4-6 nm)
    add_executable(test_experiment_match tests/test_experiment_match.cpp)
    target_link_libraries(test_experiment_match atomistic)
    target_include_directories(test_experiment_match PRIVATE ${PROJECT_SOURCE_DIR})

    # ========================================================================

    # Phase 1 Kernel Audit — deterministic revalidation of core energy/forces
    add_executable(phase1_kernel_audit apps/phase1_kernel_audit.cpp)
    target_link_libraries(phase1_kernel_audit atomistic)
    target_include_directories(phase1_kernel_audit PRIVATE ${PROJECT_SOURCE_DIR})

    # Phase 2 Structural Energy — reference molecules through end-user pipeline
    add_executable(phase2_structural_energy apps/phase2_structural_energy.cpp)
    target_link_libraries(phase2_structural_energy atomistic vsepr_io vsepr_core)
    target_include_directories(phase2_structural_energy PRIVATE ${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src)

    # Phase 3 Relaxation — FIRE minimization on distorted molecules
    add_executable(phase3_relaxation apps/phase3_relaxation.cpp)
    target_link_libraries(phase3_relaxation atomistic vsepr_io vsepr_core)
    target_include_directories(phase3_relaxation PRIVATE ${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src)

    # Phase 4 Formation Priors — crystal presets and supercell verification
    add_executable(phase4_formation_priors apps/phase4_formation_priors.cpp)
    target_link_libraries(phase4_formation_priors atomistic)
    target_include_directories(phase4_formation_priors PRIVATE ${PROJECT_SOURCE_DIR})

    # Phase 5 Crystal Validation — nn distances, coordination, relaxation under PBC
    add_executable(phase5_crystal_validation apps/phase5_crystal_validation.cpp)
    target_link_libraries(phase5_crystal_validation atomistic)
    target_include_directories(phase5_crystal_validation PRIVATE ${PROJECT_SOURCE_DIR})

    # Phase 6/7 Verification Sessions — named session ladder with artifact output
    add_executable(phase6_7_verification_sessions apps/phase6_7_verification_sessions.cpp)
    target_link_libraries(phase6_7_verification_sessions atomistic vsepr_io vsepr_core)
    target_include_directories(phase6_7_verification_sessions PRIVATE ${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src)

    # Phase 8/9/10 Environment — physics registry, layer manifest, EnvironmentContext
    add_executable(phase8_9_10_environment apps/phase8_9_10_environment.cpp)
    target_link_libraries(phase8_9_10_environment atomistic vsepr_io vsepr_core)
    target_include_directories(phase8_9_10_environment PRIVATE ${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src)

    # Phase 11/12/13 Final — composite model, bridge integration, regression
    add_executable(phase11_12_13_final apps/phase11_12_13_final.cpp)
    target_link_libraries(phase11_12_13_final atomistic vsepr_io vsepr_core)
    target_include_directories(phase11_12_13_final PRIVATE ${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src)

    # Phase 14 Milestones — Coulomb, matched FF, angle fix, covalent crystal, screening
    add_executable(phase14_milestones apps/phase14_milestones.cpp)
    target_link_libraries(phase14_milestones atomistic vsepr_io vsepr_core)
    target_include_directories(phase14_milestones PRIVATE ${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src)

    # Deep Verification — automated empirical sweep
    add_executable(deep_verification apps/deep_verification.cpp)
    target_link_libraries(deep_verification atomistic vsepr_io vsepr_core)
    target_include_directories(deep_verification PRIVATE
        ${PROJECT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/verification)

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
    # TODO: Enable when atomistic/analysis/rdf.hpp is implemented
    if(EXISTS ${PROJECT_SOURCE_DIR}/atomistic/analysis/rdf.hpp)
        add_executable(test_application_validation apps/test_application_validation.cpp)
        target_link_libraries(test_application_validation atomistic vsepr_core)
        target_include_directories(test_application_validation PRIVATE ${PROJECT_SOURCE_DIR})
    endif()

    # SAFETY RAILS TEST: B3 safety checks
    # TODO: Fix test_safety_rails — IModel::compute_energy_and_forces API mismatch
    # add_executable(test_safety_rails apps/test_safety_rails.cpp)
    # target_link_libraries(test_safety_rails atomistic vsepr_core)
    # target_include_directories(test_safety_rails PRIVATE ${PROJECT_SOURCE_DIR})

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
    
    # Atomistic Relax - FIRE minimization
    add_executable(atomistic-relax apps/atomistic-relax.cpp)
    target_link_libraries(atomistic-relax atomistic vsepr_io vsepr_core)
    target_include_directories(atomistic-relax PRIVATE ${PROJECT_SOURCE_DIR})

    # Atomistic-Sim - Unified simulation & prediction tool
    add_executable(atomistic-sim apps/atomistic-sim.cpp apps/atomistic-sim-modes.cpp)
    target_link_libraries(atomistic-sim atomistic vsepr_io vsepr_core vsepr_pot)
    target_include_directories(atomistic-sim PRIVATE ${PROJECT_SOURCE_DIR})

    # Atomistic-Discover - Deterministic reaction discovery system
    add_executable(atomistic-discover apps/atomistic-discover.cpp)
    target_link_libraries(atomistic-discover atomistic vsepr_io vsepr_core vsepr_pot)
    target_include_directories(atomistic-discover PRIVATE ${PROJECT_SOURCE_DIR})

    # Atomistic-Align - Kabsch alignment with camera tracking
    add_executable(atomistic-align apps/atomistic-align.cpp)
    target_link_libraries(atomistic-align atomistic vsepr_io vsepr_core)
    target_include_directories(atomistic-align PRIVATE ${PROJECT_SOURCE_DIR})

    # Atomistic-Build - Interactive molecular builder CLI (formula pipeline)
    # TODO: Enable when apps/build/builder_core.hpp is implemented
    if(EXISTS ${PROJECT_SOURCE_DIR}/apps/build/builder_core.hpp)
        add_executable(atomistic-build apps/atomistic-build.cpp)
        target_link_libraries(atomistic-build atomistic vsepr_io vsepr_core vsepr_pot)
        target_include_directories(atomistic-build PRIVATE ${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/src)
    endif()

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
    # TODO: Fix simple_nacl_test — uses stale vsepr:: namespace (BoxPBC, FIREParams, Vec3); port to atomistic::
    # add_executable(simple_nacl_test apps/simple_nacl_test.cpp)
    # target_link_libraries(simple_nacl_test atomistic vsepr_core)
    # target_include_directories(simple_nacl_test PRIVATE ${PROJECT_SOURCE_DIR})
    # install(TARGETS simple_nacl_test DESTINATION bin)

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
    if(BUILD_VIS AND nlohmann_json_FOUND)
        add_executable(card-viewer apps/card_viewer.cpp ${IMGUI_SOURCES})
        target_link_libraries(card-viewer vsepr_core vsepr_io OpenGL::GL glfw GLEW::GLEW nlohmann_json::nlohmann_json)
        target_include_directories(card-viewer PRIVATE
            ${PROJECT_SOURCE_DIR}/src
            ${PROJECT_SOURCE_DIR}/include
            ${IMGUI_DIR}
            ${IMGUI_DIR}/backends
        )
        install(TARGETS card-viewer DESTINATION bin)
    elseif(BUILD_VIS)
        message(STATUS "card-viewer skipped — nlohmann_json not found (install nlohmann-json-devel or nlohmann/json.hpp)")
    endif()

    # Compute Forces - CLI tool for force field computation
    add_executable(compute_forces apps/compute_forces.cpp)
    target_link_libraries(compute_forces vsepr_core vsepr_io atomistic)
    target_include_directories(compute_forces PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/include)
    install(TARGETS compute_forces DESTINATION bin)

    # CG Mapping Test — Coarse-grained mapping pipeline validation
    add_executable(test_cg_mapping tests/test_cg_mapping.cpp)
    target_link_libraries(test_cg_mapping coarse_grain atomistic vsepr_core)
    target_include_directories(test_cg_mapping PRIVATE ${PROJECT_SOURCE_DIR})

    # CG Split-Screen Viewer — Atomistic ↔ Coarse-Grained inspection tool
    if(BUILD_VIS)
        add_executable(cg-viewer apps/cg-viewer.cpp)
        target_link_libraries(cg-viewer coarse_grain atomistic vsepr_vis vsepr_core OpenGL::GL glfw GLEW::GLEW)
        target_include_directories(cg-viewer PRIVATE
            ${PROJECT_SOURCE_DIR}/src
            ${PROJECT_SOURCE_DIR}/include
            ${PROJECT_SOURCE_DIR}
            ${IMGUI_DIR}
            ${IMGUI_DIR}/backends
        )
        install(TARGETS cg-viewer DESTINATION bin)
    endif()

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
    if(EXISTS ${PROJECT_SOURCE_DIR}/docs/pbc_example.cpp)
        add_executable(pbc_example docs/pbc_example.cpp)
        target_link_libraries(pbc_example vsepr_core vsepr_box)
    endif()
    
    # Phase 1: Batch Processing (requires vsepr_gui — only available with BUILD_VIS=ON)
    if(BUILD_VIS)
        add_executable(test_batch_processing tests/test_batch_processing.cpp)
        target_link_libraries(test_batch_processing vsepr_dynamic vsepr_gui_utils vsepr_io vsepr_core vsepr_sim vsepr_gui)
        add_test(NAME BatchProcessingTest COMMAND test_batch_processing)
    endif()
    
    # Phase 2: Thermal Animation
    add_executable(test_thermal_animation tests/test_thermal_animation.cpp)
    target_link_libraries(test_thermal_animation vsepr_thermal vsepr_io vsepr_core vsepr_sim vsepr_dynamic)
    add_test(NAME ThermalAnimationTest COMMAND test_thermal_animation)
    
    # Phase 3: Continuous Generation (requires vsepr_gui — only available with BUILD_VIS=ON)
    if(BUILD_VIS)
        add_executable(test_continuous_generation tests/test_continuous_generation.cpp)
        target_link_libraries(test_continuous_generation vsepr_gui_utils vsepr_dynamic vsepr_io vsepr_core vsepr_sim vsepr_gui)
        add_test(NAME ContinuousGenerationTest COMMAND test_continuous_generation)
    endif()

    # Ensemble Consistency & Perturbation Invariance Test
    # TODO: Fix ensemble_consistency_test (needs CoreState from qa_golden_tests.cpp)
    # add_executable(ensemble_consistency_test tests/ensemble_consistency_test.cpp)
    # target_link_libraries(ensemble_consistency_test atomistic vsepr_core vsepr_io)
    # target_include_directories(ensemble_consistency_test PRIVATE ${PROJECT_SOURCE_DIR})
    # add_test(NAME EnsembleConsistencyTest COMMAND ensemble_consistency_test)

    # Heat-Gated Reaction Control Tests (SS8b)
    add_executable(test_heat_gate tests/test_heat_gate.cpp)
    target_link_libraries(test_heat_gate atomistic vsepr_core)
    target_include_directories(test_heat_gate PRIVATE ${PROJECT_SOURCE_DIR})
    add_test(NAME HeatGateTest COMMAND test_heat_gate)

    # Crystal Pipeline Tests (SS10 - Supercell, Lattice, Coordination)
    add_executable(test_crystal_pipeline tests/test_crystal_pipeline.cpp)
    target_link_libraries(test_crystal_pipeline atomistic vsepr_core)
    target_include_directories(test_crystal_pipeline PRIVATE ${PROJECT_SOURCE_DIR})
    add_test(NAME CrystalPipelineTest COMMAND test_crystal_pipeline)

    # Crystal Metrics & Verification Scorecard Tests (SS10b)
    add_executable(test_crystal_metrics tests/test_crystal_metrics.cpp)
    target_link_libraries(test_crystal_metrics atomistic vsepr_core)
    target_include_directories(test_crystal_metrics PRIVATE ${PROJECT_SOURCE_DIR})
    add_test(NAME CrystalMetricsTest COMMAND test_crystal_metrics)

    # Coarse-Grained Mapping Pipeline Test (Dev Day #32)
    add_executable(test_cg_mapping_suite tests/test_cg_mapping.cpp)
    target_link_libraries(test_cg_mapping_suite coarse_grain atomistic vsepr_core)
    target_include_directories(test_cg_mapping_suite PRIVATE ${PROJECT_SOURCE_DIR})
    add_test(NAME CGMappingTest COMMAND test_cg_mapping_suite)

    # Anisotropic Surface-Mapped Bead Tests (benzene canonical test case)
    add_executable(test_anisotropic_beads tests/test_anisotropic_beads.cpp)
    target_link_libraries(test_anisotropic_beads coarse_grain atomistic vsepr_core)
    target_include_directories(test_anisotropic_beads PRIVATE ${PROJECT_SOURCE_DIR})
    add_test(NAME AnisotropicBeadsTest COMMAND test_anisotropic_beads)

    # Orientation-Coupled Interaction Tests (benzene dimer configurations)
    add_executable(test_orientation_coupling tests/test_orientation_coupling.cpp)
    target_link_libraries(test_orientation_coupling coarse_grain atomistic vsepr_core)
    target_include_directories(test_orientation_coupling PRIVATE ${PROJECT_SOURCE_DIR})
    add_test(NAME OrientationCouplingTest COMMAND test_orientation_coupling)

    # Descriptor Enrichment Tests (multi-channel, higher-order SH, adaptive complexity)
    add_executable(test_descriptor_enrichment tests/test_descriptor_enrichment.cpp)
    target_link_libraries(test_descriptor_enrichment coarse_grain atomistic vsepr_core)
    target_include_directories(test_descriptor_enrichment PRIVATE ${PROJECT_SOURCE_DIR})
    add_test(NAME DescriptorEnrichmentTest COMMAND test_descriptor_enrichment)

    # Model Selection Tests (two-tier hierarchical selection and tiered interaction routing)
    add_executable(test_model_selection tests/test_model_selection.cpp)
    target_link_libraries(test_model_selection coarse_grain atomistic vsepr_core)
    target_include_directories(test_model_selection PRIVATE ${PROJECT_SOURCE_DIR})
    add_test(NAME ModelSelectionTest COMMAND test_model_selection)

    # Unified Descriptor Strategy Tests (adaptive truncation, residual-driven promotion)
    add_executable(test_unified_descriptor tests/test_unified_descriptor.cpp)
    target_link_libraries(test_unified_descriptor coarse_grain atomistic vsepr_core)
    target_include_directories(test_unified_descriptor PRIVATE ${PROJECT_SOURCE_DIR})
    add_test(NAME UnifiedDescriptorTest COMMAND test_unified_descriptor)

    # Fragment Bridge Tests (atomistic preparation layer, scale boundary)
    add_executable(test_fragment_bridge tests/test_fragment_bridge.cpp)
    target_link_libraries(test_fragment_bridge coarse_grain atomistic vsepr_core)
    target_include_directories(test_fragment_bridge PRIVATE ${PROJECT_SOURCE_DIR})
    add_test(NAME FragmentBridgeTest COMMAND test_fragment_bridge)

    # Anisotropic Bead Model Tests (channel kernels, SH rotation, interaction engine, adaptive refinement)
    add_executable(test_anisotropic_model tests/test_anisotropic_model.cpp)
    target_link_libraries(test_anisotropic_model coarse_grain atomistic vsepr_core)
    target_include_directories(test_anisotropic_model PRIVATE ${PROJECT_SOURCE_DIR})
    add_test(NAME AnisotropicModelTest COMMAND test_anisotropic_model)

    # Environment-Responsive Bead State Tests (observables, eta integration, kernel modulation)
    add_executable(test_environment_responsive tests/test_environment_responsive.cpp)
    target_link_libraries(test_environment_responsive coarse_grain atomistic vsepr_core)
    target_include_directories(test_environment_responsive PRIVATE ${PROJECT_SOURCE_DIR})
    add_test(NAME EnvironmentResponsiveTest COMMAND test_environment_responsive)

    # Suite #2: Environment-Responsive Pipeline Tests (scene factory, trajectories, behavioral assertions)
    add_executable(test_environment_suite2 tests/test_environment_suite2.cpp)
    target_link_libraries(test_environment_suite2 coarse_grain atomistic vsepr_core)
    target_include_directories(test_environment_suite2 PRIVATE ${PROJECT_SOURCE_DIR})
    add_test(NAME EnvironmentSuite2Test COMMAND test_environment_suite2)

    # Track 2 Phase 1: Randomized Robustness Harness (ensemble statistical validation)
    add_executable(test_track2_robustness tests/test_track2_robustness.cpp)
    target_link_libraries(test_track2_robustness coarse_grain atomistic vsepr_core)
    target_include_directories(test_track2_robustness PRIVATE ${PROJECT_SOURCE_DIR})
    add_test(NAME Track2RobustnessTest COMMAND test_track2_robustness)

    # CG CLI Integration Tests (universal interpretation layer validation)
    add_executable(test_cg_cli tests/test_cg_cli.cpp)
    target_link_libraries(test_cg_cli coarse_grain atomistic vsepr_core)
    target_include_directories(test_cg_cli PRIVATE ${PROJECT_SOURCE_DIR})
    add_test(NAME CGCLITest COMMAND test_cg_cli)

    # Suite #3 — Structured N > 10 Formation Studies
    add_executable(test_formation_suite3 tests/test_formation_suite3.cpp)
    target_link_libraries(test_formation_suite3 coarse_grain atomistic vsepr_core)
    target_include_directories(test_formation_suite3 PRIVATE ${PROJECT_SOURCE_DIR})
    add_test(NAME FormationSuite3Test COMMAND test_formation_suite3)

    # Suite #4 — Dynamic Formation Regimes, Invariance, Condition-to-Structure
    add_executable(test_formation_regimes_suite4 tests/test_formation_regimes_suite4.cpp)
    target_link_libraries(test_formation_regimes_suite4 coarse_grain atomistic vsepr_core)
    target_include_directories(test_formation_regimes_suite4 PRIVATE ${PROJECT_SOURCE_DIR})
    add_test(NAME FormationRegimesSuite4Test COMMAND test_formation_regimes_suite4)

    # Suite #5 — Emergent Effective Medium Mapping
    add_executable(test_ensemble_proxy_suite5 tests/test_ensemble_proxy_suite5.cpp)
    target_link_libraries(test_ensemble_proxy_suite5 coarse_grain atomistic vsepr_core)
    target_include_directories(test_ensemble_proxy_suite5 PRIVATE ${PROJECT_SOURCE_DIR})
    add_test(NAME EnsembleProxySuite5Test COMMAND test_ensemble_proxy_suite5)

    # Suite #6 — Macro Property Precursor Channels
    add_executable(test_macro_precursor_suite6 tests/test_macro_precursor_suite6.cpp)
    target_link_libraries(test_macro_precursor_suite6 coarse_grain atomistic vsepr_core)
    target_include_directories(test_macro_precursor_suite6 PRIVATE ${PROJECT_SOURCE_DIR})
    add_test(NAME MacroPrecursorSuite6Test COMMAND test_macro_precursor_suite6)

    # Suite #7 — Property Learning Pipeline
    add_executable(test_property_pipeline_suite7 tests/test_property_pipeline_suite7.cpp)
    target_link_libraries(test_property_pipeline_suite7 coarse_grain atomistic vsepr_core)
    target_include_directories(test_property_pipeline_suite7 PRIVATE ${PROJECT_SOURCE_DIR})
    add_test(NAME PropertyPipelineSuite7Test COMMAND test_property_pipeline_suite7)

    # Suite #8 — Property Calibration and Target Legitimacy
    add_executable(test_property_calibration_suite8 tests/test_property_calibration_suite8.cpp)
    target_link_libraries(test_property_calibration_suite8 coarse_grain atomistic vsepr_core)
    target_include_directories(test_property_calibration_suite8 PRIVATE ${PROJECT_SOURCE_DIR})
    add_test(NAME PropertyCalibrationSuite8Test COMMAND test_property_calibration_suite8)

    # Multi-Layer Bead Visualization Model (Layer 2 + Layer 3)
    add_executable(test_bead_layers tests/test_bead_layers.cpp)
    target_link_libraries(test_bead_layers coarse_grain atomistic vsepr_core)
    target_include_directories(test_bead_layers PRIVATE ${PROJECT_SOURCE_DIR})
    add_test(NAME BeadLayersTest COMMAND test_bead_layers)

    # ---- Test Visualization Support ----
    # When BUILD_VIS=ON, CG tests can open an interactive GLFW/ImGui viewer
    # at each VIZ_CHECKPOINT(). Activated at runtime by: VSEPR_TEST_VIZ=1
    function(add_test_viz_support target)
        if(BUILD_VIS)
            target_compile_definitions(${target} PRIVATE BUILD_VISUALIZATION)
            target_link_libraries(${target} vsepr_vis)
        endif()
    endfunction()

    add_test_viz_support(test_cg_mapping_suite)
    add_test_viz_support(test_environment_suite2)
    add_test_viz_support(test_cg_cli)
    add_test_viz_support(test_environment_responsive)
    add_test_viz_support(test_anisotropic_model)
    add_test_viz_support(test_track2_robustness)
    add_test_viz_support(test_formation_suite3)
    add_test_viz_support(test_formation_regimes_suite4)
    add_test_viz_support(test_ensemble_proxy_suite5)
    add_test_viz_support(test_macro_precursor_suite6)
    add_test_viz_support(test_property_pipeline_suite7)
    add_test_viz_support(test_property_calibration_suite8)
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
# Qt Desktop Application (primary workstation UI)
# ============================================================================

option(BUILD_DESKTOP "Build Qt-based desktop application" ON)

if(BUILD_DESKTOP)
    find_package(Qt6 COMPONENTS Widgets OpenGLWidgets QUIET)
    if(Qt6_FOUND)
        set(CMAKE_AUTOMOC ON)

        add_executable(vsepr-desktop
            apps/desktop/main.cpp
            apps/desktop/MainWindow.cpp
            apps/desktop/ViewportWidget.cpp
            apps/desktop/ObjectTree.cpp
            apps/desktop/PropertiesPanel.cpp
            apps/desktop/ConsolePanel.cpp
            apps/desktop/bridge/EngineAdapter.cpp
        )

        target_link_libraries(vsepr-desktop PRIVATE
            Qt6::Widgets
            Qt6::OpenGLWidgets
            vsepr_sim
            vsepr_io
            vsepr_core
        )

        target_include_directories(vsepr-desktop PRIVATE
            ${PROJECT_SOURCE_DIR}/apps/desktop
            ${PROJECT_SOURCE_DIR}/include
            ${PROJECT_SOURCE_DIR}/src
            ${PROJECT_SOURCE_DIR}
        )

        install(TARGETS vsepr-desktop DESTINATION bin)

        message(STATUS "Qt Desktop: vsepr-desktop (Qt ${Qt6_VERSION})")
    else()
        message(STATUS "Qt Desktop: SKIPPED (Qt6 not found — install qt6-base-dev)")
    endif()
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
if(BUILD_VIS)
    message(STATUS "  - vsepr_render (static - scalable rendering)")
endif()
message(STATUS "  - vsepr_api (static - production app-facing façade)")
if(BUILD_VIS)
    message(STATUS "  - vsepr_vis (static)")
endif()
if(BUILD_GUI AND BUILD_VIS)
    message(STATUS "  - vsepr_gui (static - context menus + data pipes + ImGui)")
endif()
message(STATUS "")
