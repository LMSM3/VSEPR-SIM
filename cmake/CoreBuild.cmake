# ============================================================================
# VSEPR-SIM  â€”  Part 1: Core Build (Headless)
# ============================================================================
#
# All libraries, applications, and infrastructure that compile WITHOUT
# external graphics dependencies (no OpenGL, GLFW, GLEW, ImGui, Qt).
#
# This file is included from the root CMakeLists.txt AFTER project(),
# compiler settings, and build options have been established.
# ============================================================================

# ============================================================================
# 1. Internal Librariesn
# ============================================================================

# --- Core Library (header-only, everything depends on this) ---
add_library(vsepr_core INTERFACE)
target_include_directories(vsepr_core INTERFACE src/core)

# --- Element Index Tracker Library (persistent particle identity + random picker + alias resolver) ---
add_library(vsepr_tracker STATIC
    src/core/element_index_tracker.cpp
    src/core/random_element_picker.cpp
    src/core/molecule_alias.cpp
)
target_include_directories(vsepr_tracker PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(vsepr_tracker PUBLIC vsepr_core)

# --- Code Trail Wind v0.1 (deterministic operation audit trail, CSV output) ---
add_library(vsepr_trail STATIC
    src/core/code_trail.cpp
)
target_include_directories(vsepr_trail PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(vsepr_trail PUBLIC vsepr_core)

# --- Energy Units + Reported Quantity (canonical Hartree unit system, physical co-reports) ---
add_library(vsepr_units STATIC
    src/core/energy_units.cpp
    src/core/reported_quantity.cpp
)
target_include_directories(vsepr_units PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(vsepr_units PUBLIC vsepr_core)

# --- Gas Module Library (ideal/real gas, kinetic theory, Maxwell-Boltzmann) ---
add_library(vsepr_gas STATIC
    src/core/gas_module.cpp
)
target_include_directories(vsepr_gas PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(vsepr_gas PUBLIC vsepr_core)

# --- Gas2 Module Library (advanced heat, EOS comparison, kinetic theory, transport) ---
add_library(vsepr_gas2 STATIC
    src/gas2/gas2_engine.cpp
)
target_include_directories(vsepr_gas2 PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(vsepr_gas2 PUBLIC vsepr_core)

# --- Gas3 Module Library (quality pipeline, fitting, export, reporting) ---
add_library(vsepr_gas3 STATIC
    src/gas3/gas3_engine.cpp
)
target_include_directories(vsepr_gas3 PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(vsepr_gas3 PUBLIC vsepr_gas2 vsepr_core)

# --- Live Server Library (zero-input HTTP analysis stream on port 99998) ---
add_library(vsepr_live_lib STATIC
    src/core/live_server.cpp
)
target_include_directories(vsepr_live_lib PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(vsepr_live_lib PUBLIC vsepr_core vsepr_gas)
if(WIN32)
    target_link_libraries(vsepr_live_lib PUBLIC ws2_32)
endif()

# --- Dual-Port Viz Stream Server (port 9999 atomic + port 10001 analysis) ---
add_library(vsepr_viz_lib STATIC
    src/core/viz_server.cpp
)
target_include_directories(vsepr_viz_lib PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(vsepr_viz_lib PUBLIC vsepr_gas2 vsepr_core)
if(WIN32)
    target_link_libraries(vsepr_viz_lib PUBLIC ws2_32)
endif()

# --- Report Engine Library (autonomous thermal-materials experiment + report generation) ---
add_library(vsepr_report STATIC
    src/core/report_engine.cpp
)
target_include_directories(vsepr_report PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(vsepr_report PUBLIC vsepr_core)

# --- Infrastructure Library (bootstrap probe, MOTD, NVIDIA TUI) ---
add_library(vsepr_infra STATIC
    src/infra/bootstrap_probe.cpp
    src/infra/motd.cpp
    src/infra/nvidia_tui.cpp
)
target_include_directories(vsepr_infra PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(vsepr_infra PUBLIC vsepr_core)

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

# --- ISO 286 Tolerance & Fit Library (pure C) ---
add_library(vsepr_iso_fit STATIC src/core/iso_fit.c)
target_include_directories(vsepr_iso_fit PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)
set_source_files_properties(src/core/iso_fit.c PROPERTIES LANGUAGE C)
if(NOT MSVC)
    target_link_libraries(vsepr_iso_fit PUBLIC m)
endif()

# --- Fatigue Analysis Library (pure C) ---
add_library(vsepr_fatigue STATIC src/core/fatigue.c)
target_include_directories(vsepr_fatigue PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)
set_source_files_properties(src/core/fatigue.c PROPERTIES LANGUAGE C)
if(NOT MSVC)
    target_link_libraries(vsepr_fatigue PUBLIC m)
endif()

# --- Legacy C API (species codes + decay event — portable C99) ---
add_library(vsepr_legacy_c STATIC
    legacy/c_api/particle_ids.c
    legacy/c_api/decay_event.c
)
target_include_directories(vsepr_legacy_c PUBLIC
    ${PROJECT_SOURCE_DIR}/legacy/c_api
)
set_source_files_properties(
    legacy/c_api/particle_ids.c
    legacy/c_api/decay_event.c
    PROPERTIES LANGUAGE C
)

# --- Simulation Library ---
file(GLOB SIM_SOURCES src/sim/*.cpp)
list(FILTER SIM_SOURCES EXCLUDE REGEX ".*sim_thread\\.cpp$")
add_library(vsepr_sim STATIC ${SIM_SOURCES})
target_link_libraries(vsepr_sim PUBLIC vsepr_core)
target_include_directories(vsepr_sim PUBLIC src/sim)

# --- Potentials Library (header-only) ---
add_library(vsepr_pot INTERFACE)
target_include_directories(vsepr_pot INTERFACE src/pot)
target_link_libraries(vsepr_pot INTERFACE vsepr_core)

# --- Box/PBC Library (header-only) ---
add_library(vsepr_box INTERFACE)
target_include_directories(vsepr_box INTERFACE src/box)
target_link_libraries(vsepr_box INTERFACE vsepr_core)

# --- Neighbor List Library (header-only) ---
add_library(vsepr_nl INTERFACE)
target_include_directories(vsepr_nl INTERFACE src/nl)
target_link_libraries(vsepr_nl INTERFACE vsepr_core)

# --- Integrators Library (header-only) ---
add_library(vsepr_int INTERFACE)
target_include_directories(vsepr_int INTERFACE src/int)
target_link_libraries(vsepr_int INTERFACE vsepr_core)

# --- Build System Library (Formula -> Molecule, header-only) ---
add_library(vsepr_build INTERFACE)
target_include_directories(vsepr_build INTERFACE src/build)
target_link_libraries(vsepr_build INTERFACE vsepr_core vsepr_sim)

# --- Dynamic Generation Library (real molecule generation) ---
add_library(vsepr_dynamic STATIC
    src/dynamic/real_molecule_generator.cpp
)
target_include_directories(vsepr_dynamic PUBLIC include ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(vsepr_dynamic PUBLIC vsepr_core vsepr_sim)

# --- GPU Backend Library ---
if(VSEPR_HAS_CUDA)
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/gpu/kernels/energy_nonbonded.cu")
        add_library(vsepr_cuda_kernels STATIC
            src/gpu/kernels/energy_nonbonded.cu
        )
        set_target_properties(vsepr_cuda_kernels PROPERTIES
            CUDA_SEPARABLE_COMPILATION ON
            CUDA_STANDARD 17
        )
        target_compile_options(vsepr_cuda_kernels PRIVATE
            $<$<CONFIG:Debug>:-G -g -DENABLE_DEBUG_CHECKS=1>
            $<$<CONFIG:Release>:-O3 -lineinfo -DENABLE_DEBUG_CHECKS=0>
        )
        target_include_directories(vsepr_cuda_kernels PUBLIC include)

        add_library(vsepr_gpu STATIC src/gpu/gpu_backend.cpp)
        target_include_directories(vsepr_gpu PUBLIC include)
        target_link_libraries(vsepr_gpu PUBLIC vsepr_core vsepr_cuda_kernels)
        target_compile_definitions(vsepr_gpu PUBLIC VSEPR_HAS_CUDA)
        message(STATUS "GPU acceleration: CUDA enabled with kernels")
    else()
        add_library(vsepr_gpu STATIC src/gpu/gpu_backend.cpp)
        target_include_directories(vsepr_gpu PUBLIC include)
        target_link_libraries(vsepr_gpu PUBLIC vsepr_core)
        target_compile_definitions(vsepr_gpu PUBLIC VSEPR_HAS_CUDA)
        message(STATUS "GPU acceleration: CUDA detected but kernels not built (skeleton only)")
    endif()
else()
    add_library(vsepr_gpu STATIC src/gpu/gpu_backend.cpp)
    target_include_directories(vsepr_gpu PUBLIC include)
    target_link_libraries(vsepr_gpu PUBLIC vsepr_core)
    message(STATUS "GPU acceleration: CPU fallback only")
endif()

# --- I/O Library (XYZ, XYZA formats + unified molecular_io API) ---
add_library(vsepr_io STATIC
    src/io/xyz_format.cpp
    src/io/molecular_io.cpp
)
target_include_directories(vsepr_io PUBLIC include)
target_link_libraries(vsepr_io PUBLIC vsepr_core)

# --- Thermal Animation Library (real-time MD simulation) ---
add_library(vsepr_thermal STATIC
    src/thermal/thermal_runner.cpp
    src/thermal/xyzc_format.cpp
)
target_include_directories(vsepr_thermal PUBLIC include ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(vsepr_thermal PUBLIC vsepr_core vsepr_sim vsepr_io)

# --- API Facade Library (production app-facing layer) ---
add_library(vsepr_api STATIC
    src/api/io_api.cpp
)
target_include_directories(vsepr_api PUBLIC include)
target_link_libraries(vsepr_api PUBLIC vsepr_core vsepr_io vsepr_thermal vsepr_pot)

# --- Spec Parser Library (DSL & JSON) ---
add_library(spec_parser STATIC src/spec_parser.cpp)
target_include_directories(spec_parser PUBLIC include)
target_link_libraries(spec_parser PUBLIC vsepr_core)

# --- Demo Launcher Library (cross-platform terminal spawning) ---
add_library(vsepr_demo STATIC
    src/demo/platform_terminal.cpp
)
target_include_directories(vsepr_demo PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(vsepr_demo PUBLIC vsepr_core)

# --- Atomistic Simulation Library (canonical CoreState, parsers, compilers, TUI) ---
add_subdirectory(atomistic)

# --- Coarse-Grained Modeling Library (bead mapping, reduced representation) ---
add_subdirectory(coarse_grain)

# --- Electrohydrodynamic Simulation Library (coupled flow + electrostatics + ion transport) ---
add_subdirectory(sim/ehd)

# ============================================================================
# 2. Headless Applications (BUILD_APPS)
# ============================================================================

if(BUILD_APPS)
    # CLI Tool (legacy)
    add_executable(vsepr-cli apps/vsepr-cli/main.cpp)
    target_link_libraries(vsepr-cli vsepr_sim vsepr_pot vsepr_box vsepr_nl)
    target_include_directories(vsepr-cli PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/include)
    install(TARGETS vsepr-cli DESTINATION bin)

    # Unified CLI library (domain-aware grammar)
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
        src/cli/cmd_therm.cpp
    )
    target_include_directories(vsepr_cli PUBLIC include/ ${PROJECT_SOURCE_DIR})
    target_link_libraries(vsepr_cli PUBLIC vsepr_core vsepr_tracker atomistic vsepr_io coarse_grain)

    # Unified CLI executable
    add_executable(vsepr apps/vsepr.cpp)
    target_link_libraries(vsepr vsepr_cli vsepr_sim vsepr_pot vsepr_io vsepr_thermal atomistic vsepr_gas vsepr_gas2 vsepr_gas3 vsepr_live_lib vsepr_viz_lib)
    target_include_directories(vsepr PRIVATE src/ include/ ${PROJECT_SOURCE_DIR})

    # Autonomous Report Generator (WO-TMS-CRG-001)
    add_executable(report_generator apps/report_generator.cpp)
    target_link_libraries(report_generator vsepr_report vsepr_core)
    target_include_directories(report_generator PRIVATE ${PROJECT_SOURCE_DIR}/include)

    # Code Trail Wind v0.1 — semi-visual demo
    add_executable(demo_code_trail apps/demo_code_trail.cpp)
    target_link_libraries(demo_code_trail vsepr_trail vsepr_core)
    target_include_directories(demo_code_trail PRIVATE ${PROJECT_SOURCE_DIR}/include)

    # Equipartition test (Langevin thermostat validation)
    add_executable(test_equipartition apps/test_equipartition.cpp)
    target_link_libraries(test_equipartition atomistic vsepr_core)
    target_include_directories(test_equipartition PRIVATE ${PROJECT_SOURCE_DIR})

    # Polarizability model fitter
    add_executable(fit_alpha_model tools/fit_alpha_model.cpp)
    target_link_libraries(fit_alpha_model atomistic)
    target_include_directories(fit_alpha_model PRIVATE ${PROJECT_SOURCE_DIR})

    # Auto-fit: self-improving infinite training loop
    add_executable(auto_fit_alpha tools/auto_fit_alpha.cpp)
    target_link_libraries(auto_fit_alpha atomistic)
    target_include_directories(auto_fit_alpha PRIVATE ${PROJECT_SOURCE_DIR})

    # Phase executables
    add_executable(phase1_kernel_audit apps/phase1_kernel_audit.cpp)
    target_link_libraries(phase1_kernel_audit atomistic)
    target_include_directories(phase1_kernel_audit PRIVATE ${PROJECT_SOURCE_DIR})

    add_executable(phase2_structural_energy apps/phase2_structural_energy.cpp)
    target_link_libraries(phase2_structural_energy atomistic vsepr_io vsepr_core)
    target_include_directories(phase2_structural_energy PRIVATE ${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src)

    add_executable(phase3_relaxation apps/phase3_relaxation.cpp)
    target_link_libraries(phase3_relaxation atomistic vsepr_io vsepr_core)
    target_include_directories(phase3_relaxation PRIVATE ${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src)

    add_executable(phase4_formation_priors apps/phase4_formation_priors.cpp)
    target_link_libraries(phase4_formation_priors atomistic)
    target_include_directories(phase4_formation_priors PRIVATE ${PROJECT_SOURCE_DIR})

    add_executable(phase5_crystal_validation apps/phase5_crystal_validation.cpp)
    target_link_libraries(phase5_crystal_validation atomistic)
    target_include_directories(phase5_crystal_validation PRIVATE ${PROJECT_SOURCE_DIR})

    add_executable(phase6_7_verification_sessions apps/phase6_7_verification_sessions.cpp)
    target_link_libraries(phase6_7_verification_sessions atomistic vsepr_io vsepr_core)
    target_include_directories(phase6_7_verification_sessions PRIVATE ${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src)

    add_executable(phase8_9_10_environment apps/phase8_9_10_environment.cpp)
    target_link_libraries(phase8_9_10_environment atomistic vsepr_io vsepr_core)
    target_include_directories(phase8_9_10_environment PRIVATE ${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src)

    add_executable(phase11_12_13_final apps/phase11_12_13_final.cpp)
    target_link_libraries(phase11_12_13_final atomistic vsepr_io vsepr_core)
    target_include_directories(phase11_12_13_final PRIVATE ${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src)

    add_executable(phase14_milestones apps/phase14_milestones.cpp)
    target_link_libraries(phase14_milestones atomistic vsepr_io vsepr_core)
    target_include_directories(phase14_milestones PRIVATE ${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src)

    # Deep Verification
    add_executable(deep_verification apps/deep_verification.cpp)
    target_link_libraries(deep_verification atomistic vsepr_io vsepr_core)
    target_include_directories(deep_verification PRIVATE
        ${PROJECT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/verification)

    # Test apps (headless)
    add_executable(test_mb_simple apps/test_mb_simple.cpp)
    target_link_libraries(test_mb_simple atomistic vsepr_core)
    target_include_directories(test_mb_simple PRIVATE ${PROJECT_SOURCE_DIR})

    add_executable(test_langevin_debug apps/test_langevin_debug.cpp)
    target_link_libraries(test_langevin_debug atomistic vsepr_core)
    target_include_directories(test_langevin_debug PRIVATE ${PROJECT_SOURCE_DIR})

    add_executable(test_baoab_multi apps/test_baoab_multi.cpp)
    target_link_libraries(test_baoab_multi atomistic vsepr_core)
    target_include_directories(test_baoab_multi PRIVATE ${PROJECT_SOURCE_DIR})

    add_executable(test_thermal_vs_quench apps/test_thermal_vs_quench.cpp)
    target_link_libraries(test_thermal_vs_quench atomistic vsepr_core)
    target_include_directories(test_thermal_vs_quench PRIVATE ${PROJECT_SOURCE_DIR})

    add_executable(test_thermal_ar13 apps/test_thermal_ar13.cpp)
    target_link_libraries(test_thermal_ar13 atomistic vsepr_core)
    target_include_directories(test_thermal_ar13 PRIVATE ${PROJECT_SOURCE_DIR})

    add_executable(test_pmf_calculator apps/test_pmf_calculator.cpp)
    target_link_libraries(test_pmf_calculator atomistic vsepr_core)
    target_include_directories(test_pmf_calculator PRIVATE ${PROJECT_SOURCE_DIR})

    add_executable(test_rdf_accumulator apps/test_rdf_accumulator.cpp)
    target_link_libraries(test_rdf_accumulator vsepr_cli atomistic vsepr_core)
    target_include_directories(test_rdf_accumulator PRIVATE ${PROJECT_SOURCE_DIR})

    if(EXISTS ${PROJECT_SOURCE_DIR}/atomistic/analysis/rdf.hpp)
        add_executable(test_application_validation apps/test_application_validation.cpp)
        target_link_libraries(test_application_validation atomistic vsepr_core)
        target_include_directories(test_application_validation PRIVATE ${PROJECT_SOURCE_DIR})
    endif()

    # Batch runner
    add_executable(vsepr_batch apps/vsepr_batch.cpp)
    target_link_libraries(vsepr_batch spec_parser vsepr_core)

    # Atomistic tools
    add_executable(atomistic-relax apps/atomistic-relax.cpp)
    target_link_libraries(atomistic-relax atomistic vsepr_io vsepr_core)
    target_include_directories(atomistic-relax PRIVATE ${PROJECT_SOURCE_DIR})

    add_executable(atomistic-sim apps/atomistic-sim.cpp apps/atomistic-sim-modes.cpp)
    target_link_libraries(atomistic-sim atomistic vsepr_io vsepr_core vsepr_pot)
    target_include_directories(atomistic-sim PRIVATE ${PROJECT_SOURCE_DIR})

    add_executable(atomistic-discover apps/atomistic-discover.cpp)
    target_link_libraries(atomistic-discover atomistic vsepr_io vsepr_core vsepr_pot)
    target_include_directories(atomistic-discover PRIVATE ${PROJECT_SOURCE_DIR})

    add_executable(continual_runner apps/continual_runner.cpp)
    target_link_libraries(continual_runner atomistic vsepr_io vsepr_core vsepr_pot)
    target_include_directories(continual_runner PRIVATE ${PROJECT_SOURCE_DIR})

    add_executable(atomistic-align apps/atomistic-align.cpp)
    target_link_libraries(atomistic-align atomistic vsepr_io vsepr_core)
    target_include_directories(atomistic-align PRIVATE ${PROJECT_SOURCE_DIR})

    if(EXISTS ${PROJECT_SOURCE_DIR}/apps/build/builder_core.hpp)
        add_executable(atomistic-build apps/atomistic-build.cpp)
        target_link_libraries(atomistic-build atomistic vsepr_io vsepr_core vsepr_pot)
        target_include_directories(atomistic-build PRIVATE ${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/src)
    endif()

    # Demo Launcher
    add_executable(demo apps/demo.cpp)
    target_link_libraries(demo vsepr_demo vsepr_core)
    target_include_directories(demo PRIVATE ${PROJECT_SOURCE_DIR}/src)
    install(TARGETS demo DESTINATION bin)

    # Provenance & Deterministic Hash Demo — shell-based ASCII visualization
    add_executable(demo-provenance-shell apps/demo_provenance_shell.cpp)
    target_link_libraries(demo-provenance-shell vsepr_sim vsepr_core)
    target_include_directories(demo-provenance-shell PRIVATE
        ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR})
    install(TARGETS demo-provenance-shell DESTINATION bin)

    # Version Lineage & Multi-Scale Registry Demo — 4.0-legacy-beta birth certificate
    add_executable(demo-version-lineage apps/demo_version_lineage.cpp)
    target_link_libraries(demo-version-lineage vsepr_core)
    target_include_directories(demo-version-lineage PRIVATE
        ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR})
    install(TARGETS demo-version-lineage DESTINATION bin)

    # Hash Invariance & 3-Tier Provenance Audit Test Suite
    add_executable(test-hash-invariance tests/test_hash_invariance.cpp)
    target_link_libraries(test-hash-invariance vsepr_sim vsepr_core)
    target_include_directories(test-hash-invariance PRIVATE
        ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR})

    # Nuclear Core Runner — Z=94 Pu-239 autonomous report generation (LaTeX + Excel XML)
    add_executable(nuclear-core-runner apps/nuclear_core_runner.cpp)
    target_link_libraries(nuclear-core-runner vsepr_report vsepr_sim vsepr_core)
    target_include_directories(nuclear-core-runner PRIVATE
        ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR})
    install(TARGETS nuclear-core-runner DESTINATION bin)

    # QA suites
    add_executable(qa_golden_tests apps/qa_golden_tests.cpp)
    target_link_libraries(qa_golden_tests atomistic vsepr_io vsepr_core)
    target_include_directories(qa_golden_tests PRIVATE ${PROJECT_SOURCE_DIR})
    install(TARGETS qa_golden_tests DESTINATION bin)

    add_executable(qa_random_tests apps/qa_random_tests.cpp)
    target_link_libraries(qa_random_tests atomistic vsepr_io vsepr_core)
    target_include_directories(qa_random_tests PRIVATE ${PROJECT_SOURCE_DIR})
    install(TARGETS qa_random_tests DESTINATION bin)

    # Compute forces
    add_executable(compute_forces apps/compute_forces.cpp)
    target_link_libraries(compute_forces vsepr_core vsepr_io atomistic)
    target_include_directories(compute_forces PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/include)
    install(TARGETS compute_forces DESTINATION bin)

    # Headless seed-bead-demo (no-VIS fallback is always available)
    if(NOT BUILD_VIS)
        add_executable(seed-bead-demo apps/seed_bead_demo.cpp)
        target_link_libraries(seed-bead-demo coarse_grain atomistic vsepr_core)
        target_include_directories(seed-bead-demo PRIVATE
            ${PROJECT_SOURCE_DIR}/src
            ${PROJECT_SOURCE_DIR}/include
            ${PROJECT_SOURCE_DIR}
        )
        install(TARGETS seed-bead-demo DESTINATION bin)
    endif()

    # Molecular Census â€” Deep analysis (112+ data points, conformer search, classification)
    add_executable(molecular-census apps/molecular_census.cpp)
    target_link_libraries(molecular-census vsepr_sim vsepr_pot vsepr_core)
    target_include_directories(molecular-census PRIVATE
        ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR})
    install(TARGETS molecular-census DESTINATION bin)

    # Examples (all headless)
    add_executable(organic-inorganic-synthesis examples/example_organic_inorganic_synthesis.cpp)
    target_link_libraries(organic-inorganic-synthesis coarse_grain atomistic vsepr_core)
    target_include_directories(organic-inorganic-synthesis PRIVATE
        ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR})
    install(TARGETS organic-inorganic-synthesis DESTINATION bin)

    add_executable(catalytic-dissociation examples/example_catalytic_dissociation.cpp)
    target_link_libraries(catalytic-dissociation coarse_grain atomistic vsepr_core)
    target_include_directories(catalytic-dissociation PRIVATE
        ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR})
    install(TARGETS catalytic-dissociation DESTINATION bin)

    add_executable(universal-reaction-engine examples/example_universal_reaction_engine.cpp)
    target_link_libraries(universal-reaction-engine coarse_grain atomistic vsepr_core)
    target_include_directories(universal-reaction-engine PRIVATE
        ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR})
    install(TARGETS universal-reaction-engine DESTINATION bin)

    add_executable(example-32bit-hourglass-lookglass examples/example_32bit_hourglass_lookglass.cpp)
    target_link_libraries(example-32bit-hourglass-lookglass coarse_grain atomistic vsepr_core)
    target_include_directories(example-32bit-hourglass-lookglass PRIVATE
        ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR})
    install(TARGETS example-32bit-hourglass-lookglass DESTINATION bin)

    # Report Automation Runner
    add_executable(run-report-automation apps/run_report_automation.cpp)
    target_link_libraries(run-report-automation coarse_grain atomistic vsepr_core)
    target_include_directories(run-report-automation PRIVATE
        ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR})
    install(TARGETS run-report-automation DESTINATION bin)

    # Batch simulation runners
    add_executable(sim-101x50 apps/sim_101x50.cpp)
    target_link_libraries(sim-101x50 coarse_grain atomistic vsepr_core)
    target_include_directories(sim-101x50 PRIVATE
        ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR})
    install(TARGETS sim-101x50 DESTINATION bin)

    add_executable(sim-1000 apps/sim_1000.cpp)
    target_link_libraries(sim-1000 coarse_grain atomistic vsepr_core)
    target_include_directories(sim-1000 PRIVATE
        ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR})
    install(TARGETS sim-1000 DESTINATION bin)

    add_executable(fire-smooth apps/fire_smooth_task.cpp)
    target_link_libraries(fire-smooth coarse_grain atomistic vsepr_core)
    target_include_directories(fire-smooth PRIVATE
        ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR})
    install(TARGETS fire-smooth DESTINATION bin)

    add_executable(metal-sim apps/metal_sim.cpp)
    target_link_libraries(metal-sim coarse_grain atomistic vsepr_core)
    target_include_directories(metal-sim PRIVATE
        ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR})
    install(TARGETS metal-sim DESTINATION bin)

    # Live Analysis Server — zero-input HTTP stream on port 99998
    add_executable(vsepr_live apps/vsepr_live.cpp)
    target_link_libraries(vsepr_live vsepr_live_lib vsepr_gas vsepr_core)
    target_include_directories(vsepr_live PRIVATE ${PROJECT_SOURCE_DIR}/include)
    install(TARGETS vsepr_live DESTINATION bin)

endif() # BUILD_APPS

# --- Standalone C Tools ---
add_executable(ti84_ascii tools/ti84_ascii.c)
set_source_files_properties(tools/ti84_ascii.c PROPERTIES LANGUAGE C)
target_include_directories(ti84_ascii PRIVATE ${PROJECT_SOURCE_DIR}/include)
install(TARGETS ti84_ascii DESTINATION bin)

add_executable(fatigue_calc tools/fatigue_calc.c)
set_source_files_properties(tools/fatigue_calc.c PROPERTIES LANGUAGE C)
target_link_libraries(fatigue_calc vsepr_fatigue)
install(TARGETS fatigue_calc DESTINATION bin)

# --- Module Registry + Gas Module + Live Server Tests ---
add_executable(test_modules tests/test_modules.cpp)
target_include_directories(test_modules PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_modules vsepr_gas vsepr_live_lib vsepr_core)
if(WIN32)
    target_link_libraries(test_modules ws2_32)
endif()

# --- Gas2 Module Tests (advanced heat + gas, 30 tests) ---
add_executable(test_gas2 tests/test_gas2.cpp)
target_include_directories(test_gas2 PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_gas2 vsepr_gas2 vsepr_core)

# --- Gas3 Module Tests (quality pipeline, fitting, export, 25 tests) ---
add_executable(test_gas3 tests/test_gas3.cpp)
target_include_directories(test_gas3 PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_gas3 vsepr_gas3 vsepr_gas2 vsepr_core)

# --- Species Family Classification Tests (taxonomy, entity model, 40 tests) ---
add_executable(test_species_family tests/test_species_family.cpp)
target_include_directories(test_species_family PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_species_family vsepr_core)

# --- Sensor System + XYZL Format Tests (wind/material/energy sensors, bead dynamics, 50 tests) ---
add_executable(test_sensor tests/test_sensor.cpp)
target_include_directories(test_sensor PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_sensor vsepr_core)

