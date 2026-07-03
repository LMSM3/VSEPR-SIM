# ============================================================================
# VSEPR-SIM Modular Test Infrastructure - V3.1.0                             #
# ============================================================================
# 116 active test targets across Groups 1-52 (Groups 43-52: WO-VSIM-62C,
# WO-XSUITE-02B, WO-VSIM-66A, WO-BRIDGE-65). Dead if(EXISTS) blocks and
# commented-out disabled targets removed in v5.1.13 cleanup pass.
#
# Extracted from root CMakeLists.txt (archive/integrated-testing-1013).
# All test targets are defined here and grouped by domain via CTest labels.
#
# Usage:
#   cmake --build <dir> --target all          # build everything
#   ctest --test-dir <dir>                    # run all tests
#   ctest --test-dir <dir> -L core            # run only core tests
#   ctest --test-dir <dir> -L atomistic       # run only atomistic tests
#   ctest --test-dir <dir> -L coarse_grain    # run only CG tests
#   ctest --test-dir <dir> -L cg_suites       # run CG suites 1-8 + Track2
#
# Or via the unified runner script:
#   ./scripts/run_tests.sh [--all|--core|--atomistic|--cg|--suites|--quick]
#
# Expand user interaction tree here.
#
# ============================================================================

# Helper: register a test with labels for filtered execution
function(vsepr_add_test)
    cmake_parse_arguments(T "" "NAME;TARGET" "LABELS" ${ARGN})
    add_test(NAME ${T_NAME} COMMAND ${T_TARGET}
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    if(T_LABELS)
        set_tests_properties(${T_NAME} PROPERTIES LABELS "${T_LABELS}")
    endif()
endfunction()

# Helper: VIZ support for CG test targets
function(add_test_viz_support target)
    if(VSEPR_VIS_READY AND TARGET ${target})
        target_compile_definitions(${target} PRIVATE BUILD_VISUALIZATION)
        target_link_libraries(${target} vsepr_vis)
    endif()
endfunction()

# Create data/ junction in the binary directory so tests can resolve
# relative paths like "data/element_weights.json" at runtime.
if(EXISTS "${CMAKE_SOURCE_DIR}/data" AND NOT EXISTS "${CMAKE_BINARY_DIR}/data")
    file(CREATE_LINK "${CMAKE_SOURCE_DIR}/data" "${CMAKE_BINARY_DIR}/data"
         RESULT _link_result)
    if(_link_result)
        # Symbolic link failed (no privilege) — fall back to a directory junction on Windows
        if(WIN32)
            execute_process(
                COMMAND cmd /c mklink /J "${CMAKE_BINARY_DIR}/data" "${CMAKE_SOURCE_DIR}/data"
                RESULT_VARIABLE _mklink_result)
            if(_mklink_result)
                message(WARNING "Could not create data junction: ${_mklink_result}. Tests needing runtime data may fail.")
            endif()
        else()
            message(WARNING "Could not create data symlink: ${_link_result}")
        endif()
    endif()
endif()

# ============================================================================
# Group 1: Core / Geometry / VSEPR (foundational)
# ============================================================================

add_executable(test_molecular_io test_molecular_io.cpp)
target_link_libraries(test_molecular_io vsepr_io vsepr_core)
vsepr_add_test(NAME MolecularIOTest TARGET test_molecular_io LABELS core io quick)

add_executable(geom_ops_tests geom_ops_tests.cpp)
target_link_libraries(geom_ops_tests vsepr_core vsepr_sim)
vsepr_add_test(NAME GeometryOpsTest TARGET geom_ops_tests LABELS core quick)

add_executable(energy_tests energy_tests.cpp)
target_link_libraries(energy_tests vsepr_sim vsepr_pot)
vsepr_add_test(NAME EnergyTest TARGET energy_tests LABELS core quick)

add_executable(optimizer_tests optimizer_tests.cpp)
target_link_libraries(optimizer_tests vsepr_sim vsepr_pot)
vsepr_add_test(NAME OptimizerTest TARGET optimizer_tests LABELS core quick)

add_executable(angle_tests angle_tests.cpp)
target_link_libraries(angle_tests vsepr_sim vsepr_pot)
vsepr_add_test(NAME AngleTest TARGET angle_tests LABELS core quick)

add_executable(vsepr_tests vsepr_tests.cpp)
target_link_libraries(vsepr_tests vsepr_sim vsepr_pot)
vsepr_add_test(NAME VSEPRTest TARGET vsepr_tests LABELS core quick)

add_executable(torsion_tests torsion_tests.cpp)
target_link_libraries(torsion_tests vsepr_sim vsepr_pot)
vsepr_add_test(NAME TorsionTest TARGET torsion_tests LABELS core)

add_executable(test_torsion_analysis test_torsion_analysis.cpp)
target_link_libraries(test_torsion_analysis vsepr_sim vsepr_pot)
vsepr_add_test(NAME TorsionAnalysisTest TARGET test_torsion_analysis LABELS core)

add_executable(torsion_validation_tests torsion_validation_tests.cpp)
target_link_libraries(torsion_validation_tests vsepr_sim vsepr_pot)
vsepr_add_test(NAME TorsionValidationTest TARGET torsion_validation_tests LABELS core)

add_executable(alkane_torsion_tests alkane_torsion_tests.cpp)
target_link_libraries(alkane_torsion_tests vsepr_sim vsepr_pot)
vsepr_add_test(NAME AlkaneTorsionTest TARGET alkane_torsion_tests LABELS core)

add_executable(butane_scan butane_scan.cpp)
target_link_libraries(butane_scan vsepr_sim vsepr_pot)
vsepr_add_test(NAME ButaneScanTest TARGET butane_scan LABELS core)

add_executable(vsepr_domain_test vsepr_domain_test.cpp)
target_link_libraries(vsepr_domain_test vsepr_sim vsepr_pot)
vsepr_add_test(NAME VSEPRDomainTest TARGET vsepr_domain_test LABELS core)

add_executable(basic_molecule_validation basic_molecule_validation.cpp)
target_link_libraries(basic_molecule_validation vsepr_sim vsepr_pot vsepr_core)
vsepr_add_test(NAME BasicMoleculeValidation TARGET basic_molecule_validation LABELS core quick)

add_executable(basic_isomer_validation basic_isomer_validation.cpp)
target_link_libraries(basic_isomer_validation vsepr_sim vsepr_pot vsepr_core)
vsepr_add_test(NAME BasicIsomerValidation TARGET basic_isomer_validation LABELS core isomer)

add_executable(test_ethane_torsion test_ethane_torsion.cpp)
target_link_libraries(test_ethane_torsion vsepr_sim vsepr_pot vsepr_core)
vsepr_add_test(NAME EthaneTorsionBarrier TARGET test_ethane_torsion LABELS core)

# Standalone optimization (no ctest registration -- manual runner)
add_executable(vsepr_standalone_opt vsepr_standalone_opt.cpp)
target_link_libraries(vsepr_standalone_opt vsepr_sim vsepr_pot)

# Energy model v03 (no ctest registration -- manual runner)
add_executable(energy_model_v03_test energy_model_v03_test.cpp)
target_link_libraries(energy_model_v03_test vsepr_sim vsepr_pot)

# ============================================================================
# Group 2: PBC (Periodic Boundary Conditions)
# ============================================================================

add_executable(pbc_test pbc_test.cpp)
target_link_libraries(pbc_test vsepr_core vsepr_box)
vsepr_add_test(NAME PBCTest TARGET pbc_test LABELS core pbc quick)

# Beta-8 gate: FIRE+PBC coordinate wrapping + Ewald summation
add_executable(test_pbc_fire test_pbc_fire.cpp)
target_include_directories(test_pbc_fire PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_pbc_fire vsepr_core vsepr_box vsepr_pot)
vsepr_add_test(NAME PBCFireEwald TARGET test_pbc_fire LABELS core pbc ewald beta8)

add_executable(pbc_verification pbc_verification.cpp)
target_link_libraries(pbc_verification vsepr_core vsepr_box)
vsepr_add_test(NAME PBCVerification TARGET pbc_verification LABELS core pbc)

add_executable(pbc_phase2_physics pbc_phase2_physics.cpp)
target_link_libraries(pbc_phase2_physics vsepr_core vsepr_box)
vsepr_add_test(NAME PBCPhase2Physics TARGET pbc_phase2_physics LABELS core pbc)

add_executable(pbc_phase4_perf pbc_phase4_perf.cpp)
target_link_libraries(pbc_phase4_perf vsepr_core vsepr_box)

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/pbc_phase5_regression.cpp)
    add_executable(pbc_phase5_regression pbc_phase5_regression.cpp)
    target_link_libraries(pbc_phase5_regression vsepr_core vsepr_box)
    vsepr_add_test(NAME PBCPhase5Golden TARGET pbc_phase5_regression LABELS core pbc)
else()
    message(STATUS "Skipping pbc_phase5_regression: source not found")
endif()

# Group 25 — PBC Core: WO-VSEPR-SIM-57A canonical API gate
add_executable(test_pbc_core test_pbc_core.cpp)
target_include_directories(test_pbc_core PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_pbc_core vsepr_core vsepr_box)
vsepr_add_test(NAME PBCCoreGate TARGET test_pbc_core LABELS core pbc pbc_core beta8 quick)

# Group 28 — Beta-8 PBC/Ewald Empirical Validation: CaF2 fluorite 4×4×4 supercell
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_pbc_ewald_caf2.cpp)
    add_executable(test_pbc_ewald_caf2 test_pbc_ewald_caf2.cpp)
    target_include_directories(test_pbc_ewald_caf2 PRIVATE ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/include)
    target_link_libraries(test_pbc_ewald_caf2 vsepr_core vsepr_box vsepr_pot)
    vsepr_add_test(NAME PBCEwaldCaF2 TARGET test_pbc_ewald_caf2 LABELS core pbc ewald caf2 beta8)
endif()

# ============================================================================
# Group 3: Periodic Table / Nuclear
# ============================================================================

add_executable(test_periodic_table_102 test_periodic_table_102.cpp)
target_link_libraries(test_periodic_table_102 vsepr_periodic)
vsepr_add_test(NAME PeriodicTable102Test TARGET test_periodic_table_102 LABELS core periodic quick)


# --- Unified Species Code / Decay Event / Kinetic Record (particle_id + decay builders) ---
add_executable(test_particle_id_decay test_particle_id_decay.cpp)
target_include_directories(test_particle_id_decay PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_particle_id_decay vsepr_core)
vsepr_add_test(NAME ParticleIDDecayTest TARGET test_particle_id_decay LABELS core nuclear physics quick)

# --- Element Index Tracker + Random Picker ---
add_executable(tracker_tests tracker_tests.cpp)
target_link_libraries(tracker_tests vsepr_tracker vsepr_core)
vsepr_add_test(NAME TrackerTests TARGET tracker_tests LABELS core tracker quick)

# --- Code Trail Wind v0.1 (deterministic operation audit trail) ---
add_executable(test_code_trail test_code_trail.cpp)
target_link_libraries(test_code_trail vsepr_trail vsepr_core)
vsepr_add_test(NAME CodeTrailTests TARGET test_code_trail LABELS core trail quick)

# --- Energy Units + Reported Quantity ---
add_executable(test_energy_units test_energy_units.cpp)
target_link_libraries(test_energy_units vsepr_units vsepr_core)
target_include_directories(test_energy_units PRIVATE ${PROJECT_SOURCE_DIR}/include)
vsepr_add_test(NAME EnergyUnitsTests TARGET test_energy_units LABELS core units quick)

# --- Molecule Alias Resolver ---
add_executable(alias_tests alias_tests.cpp)
target_link_libraries(alias_tests vsepr_tracker vsepr_core)
target_include_directories(alias_tests PRIVATE ${PROJECT_SOURCE_DIR}/include)
vsepr_add_test(NAME AliasTests TARGET alias_tests LABELS core alias quick)

# --- Report Engine (WO-TMS-CRG-001) ---
add_executable(report_engine_tests report_engine_tests.cpp)
target_link_libraries(report_engine_tests vsepr_report vsepr_core)
target_include_directories(report_engine_tests PRIVATE ${PROJECT_SOURCE_DIR}/include)
vsepr_add_test(NAME ReportEngineTests TARGET report_engine_tests LABELS core report)

# ============================================================================
# Group 4: Chemistry / Parsers
# ============================================================================

add_executable(chemistry_basic_test chemistry_basic_test.cpp)
target_link_libraries(chemistry_basic_test vsepr_core)
vsepr_add_test(NAME ChemistryBasic TARGET chemistry_basic_test LABELS core chemistry quick)

add_executable(test_dissolution test_dissolution.cpp)
target_link_libraries(test_dissolution atomistic vsepr_core)
vsepr_add_test(NAME DissolutionTest TARGET test_dissolution LABELS atomistic chemistry reaction)

add_executable(test_improved_nonbonded test_improved_nonbonded.cpp)
target_link_libraries(test_improved_nonbonded vsepr_core vsepr_sim vsepr_pot)
vsepr_add_test(NAME ImprovedNonbondedTest TARGET test_improved_nonbonded LABELS core chemistry)

add_executable(test_spec_parser test_spec_parser.cpp)
target_link_libraries(test_spec_parser spec_parser)
vsepr_add_test(NAME SpecParserTest TARGET test_spec_parser LABELS core parsers quick)

add_executable(test_formula_parser test_formula_parser.cpp)
target_link_libraries(test_formula_parser vsepr_core)
vsepr_add_test(NAME FormulaParserTest TARGET test_formula_parser LABELS core parsers quick)

add_executable(formula_fuzz_tester formula_fuzz_tester.cpp)
target_link_libraries(formula_fuzz_tester vsepr_core)

add_executable(test_element_db_phase1_simple test_element_db_phase1_simple.cpp)
target_link_libraries(test_element_db_phase1_simple vsepr_core)
vsepr_add_test(NAME ElementDBPhase1Simple TARGET test_element_db_phase1_simple LABELS core chemistry)



# ============================================================================
# Group 5: Thermal / Animation / Batch (some require BUILD_VIS)
# ============================================================================

# VIS-dependent tests (test_batch_processing, test_continuous_generation)
# are now defined in cmake/VisBuild.cmake to ensure VIS targets exist.

add_executable(test_thermal_animation test_thermal_animation.cpp)
target_link_libraries(test_thermal_animation vsepr_thermal vsepr_io vsepr_core vsepr_sim vsepr_dynamic)
vsepr_add_test(NAME ThermalAnimationTest TARGET test_thermal_animation LABELS thermal)

# ============================================================================
# Group 6: Atomistic Simulation
# ============================================================================

add_executable(test_heat_gate test_heat_gate.cpp)
target_link_libraries(test_heat_gate atomistic vsepr_core)
target_include_directories(test_heat_gate PRIVATE ${PROJECT_SOURCE_DIR})
vsepr_add_test(NAME HeatGateTest TARGET test_heat_gate LABELS atomistic)

add_executable(test_crystal_pipeline test_crystal_pipeline.cpp)
target_link_libraries(test_crystal_pipeline atomistic vsepr_core)
target_include_directories(test_crystal_pipeline PRIVATE ${PROJECT_SOURCE_DIR})
vsepr_add_test(NAME CrystalPipelineTest TARGET test_crystal_pipeline LABELS atomistic crystal)

add_executable(test_crystal_metrics test_crystal_metrics.cpp)
target_link_libraries(test_crystal_metrics atomistic vsepr_core)
target_include_directories(test_crystal_metrics PRIVATE ${PROJECT_SOURCE_DIR})
vsepr_add_test(NAME CrystalMetricsTest TARGET test_crystal_metrics LABELS atomistic crystal)

add_executable(problem1_two_body_lj problem1_two_body_lj.cpp)
target_link_libraries(problem1_two_body_lj atomistic)
target_include_directories(problem1_two_body_lj PRIVATE ${PROJECT_SOURCE_DIR})
vsepr_add_test(NAME Problem1TwoBodyLJ TARGET problem1_two_body_lj LABELS atomistic fundamental quick)

add_executable(problem2_three_body_cluster problem2_three_body_cluster.cpp)
target_link_libraries(problem2_three_body_cluster atomistic)
target_include_directories(problem2_three_body_cluster PRIVATE ${PROJECT_SOURCE_DIR})
vsepr_add_test(NAME Problem2ThreeBodyCluster TARGET problem2_three_body_cluster LABELS atomistic fundamental quick)

add_executable(test_scf_polarization test_scf_polarization.cpp)
target_link_libraries(test_scf_polarization atomistic)
target_include_directories(test_scf_polarization PRIVATE ${PROJECT_SOURCE_DIR})
vsepr_add_test(NAME SCFPolarizationTest TARGET test_scf_polarization LABELS atomistic polarization)

add_executable(test_alpha_model test_alpha_model.cpp)
target_link_libraries(test_alpha_model atomistic)
target_include_directories(test_alpha_model PRIVATE ${PROJECT_SOURCE_DIR})
vsepr_add_test(NAME AlphaModelTest TARGET test_alpha_model LABELS atomistic polarization)

add_executable(test_polarization_phases test_polarization_phases.cpp)
target_link_libraries(test_polarization_phases atomistic)
target_include_directories(test_polarization_phases PRIVATE ${PROJECT_SOURCE_DIR})
vsepr_add_test(NAME PolarizationPhasesTest TARGET test_polarization_phases LABELS atomistic polarization)

add_executable(test_nuclear_chemical test_nuclear_chemical.cpp)
target_link_libraries(test_nuclear_chemical atomistic)
target_include_directories(test_nuclear_chemical PRIVATE ${PROJECT_SOURCE_DIR})
vsepr_add_test(NAME NuclearChemicalTest TARGET test_nuclear_chemical LABELS atomistic nuclear)

add_executable(test_alpha_calibrator test_alpha_calibrator.cpp)
target_link_libraries(test_alpha_calibrator atomistic)
target_include_directories(test_alpha_calibrator PRIVATE ${PROJECT_SOURCE_DIR})
vsepr_add_test(NAME AlphaCalibratorTest TARGET test_alpha_calibrator LABELS atomistic polarization)

add_executable(test_nuclear_stability test_nuclear_stability.cpp)
target_link_libraries(test_nuclear_stability atomistic)
target_include_directories(test_nuclear_stability PRIVATE ${PROJECT_SOURCE_DIR})
vsepr_add_test(NAME NuclearStabilityTest TARGET test_nuclear_stability LABELS atomistic nuclear)

add_executable(test_wind_tui test_wind_tui.cpp)
target_link_libraries(test_wind_tui atomistic)
target_include_directories(test_wind_tui PRIVATE ${PROJECT_SOURCE_DIR})
vsepr_add_test(NAME WindTUITest TARGET test_wind_tui LABELS atomistic tui)

add_executable(test_flower_health test_flower_health.cpp)
target_link_libraries(test_flower_health atomistic)
target_include_directories(test_flower_health PRIVATE ${PROJECT_SOURCE_DIR})
vsepr_add_test(NAME FlowerHealthTest TARGET test_flower_health LABELS atomistic tui)

add_executable(test_env_particle test_env_particle.cpp)
target_link_libraries(test_env_particle atomistic)
target_include_directories(test_env_particle PRIVATE ${PROJECT_SOURCE_DIR})
vsepr_add_test(NAME EnvParticleTest TARGET test_env_particle LABELS atomistic environment)

add_executable(test_experiment_match test_experiment_match.cpp)
target_link_libraries(test_experiment_match atomistic)
target_include_directories(test_experiment_match PRIVATE ${PROJECT_SOURCE_DIR})
vsepr_add_test(NAME ExperimentMatchTest TARGET test_experiment_match LABELS atomistic)

add_executable(test_classify_compile_status test_classify_compile_status.cpp)
target_link_libraries(test_classify_compile_status atomistic)
target_include_directories(test_classify_compile_status PRIVATE ${PROJECT_SOURCE_DIR})
vsepr_add_test(NAME ClassifyCompileStatusTest TARGET test_classify_compile_status LABELS atomistic classify quick)

add_executable(test_classify_vsepr test_classify_vsepr.cpp)
target_link_libraries(test_classify_vsepr atomistic)
target_include_directories(test_classify_vsepr PRIVATE ${PROJECT_SOURCE_DIR})
vsepr_add_test(NAME ClassifyVSEPRTest TARGET test_classify_vsepr LABELS atomistic classify quick)

add_executable(test_classify_lone_pair_inference test_classify_lone_pair_inference.cpp)
target_link_libraries(test_classify_lone_pair_inference atomistic)
target_include_directories(test_classify_lone_pair_inference PRIVATE ${PROJECT_SOURCE_DIR})
vsepr_add_test(NAME ClassifyLonePairInferenceTest TARGET test_classify_lone_pair_inference LABELS atomistic classify quick)

add_executable(test_script_expansion test_script_expansion.cpp)
target_link_libraries(test_script_expansion vsepr_cli)
target_include_directories(test_script_expansion PRIVATE ${PROJECT_SOURCE_DIR})
vsepr_add_test(NAME ScriptExpansionPreviewTest TARGET test_script_expansion LABELS cli script quick)

add_executable(test_providers_smoke test_providers_smoke.cpp)
target_link_libraries(test_providers_smoke atomistic)
target_include_directories(test_providers_smoke PRIVATE ${PROJECT_SOURCE_DIR})
vsepr_add_test(NAME ClassifyProvidersTest TARGET test_providers_smoke LABELS atomistic classify quick)

add_executable(test_classify_pipeline test_classify_pipeline.cpp)
target_link_libraries(test_classify_pipeline atomistic)
target_include_directories(test_classify_pipeline PRIVATE ${PROJECT_SOURCE_DIR})
vsepr_add_test(NAME ClassifyPipelineTest TARGET test_classify_pipeline LABELS atomistic classify quick)

add_executable(test_classify_determinism test_classify_determinism.cpp)
target_link_libraries(test_classify_determinism atomistic)
target_include_directories(test_classify_determinism PRIVATE ${PROJECT_SOURCE_DIR})
vsepr_add_test(NAME ClassifyDeterminismTest TARGET test_classify_determinism LABELS atomistic classify quick)

add_executable(test_classify_safe_defaults test_classify_safe_defaults.cpp)
target_link_libraries(test_classify_safe_defaults atomistic)
target_include_directories(test_classify_safe_defaults PRIVATE ${PROJECT_SOURCE_DIR})
vsepr_add_test(NAME ClassifySafeDefaultsTest TARGET test_classify_safe_defaults LABELS atomistic classify quick)

# ============================================================================
# Group 7: Coarse-Grained Mapping & Descriptors
# ============================================================================


add_executable(bead_fire_report_test bead_fire_report_test.cpp)
target_link_libraries(bead_fire_report_test coarse_grain atomistic vsepr_core)
target_include_directories(bead_fire_report_test PRIVATE ${PROJECT_SOURCE_DIR})
vsepr_add_test(NAME BeadFIREReportTest TARGET bead_fire_report_test LABELS coarse_grain reporting quick)

add_executable(test_anisotropic_beads test_anisotropic_beads.cpp)
target_link_libraries(test_anisotropic_beads coarse_grain atomistic vsepr_core)
target_include_directories(test_anisotropic_beads PRIVATE ${PROJECT_SOURCE_DIR})
vsepr_add_test(NAME AnisotropicBeadsTest TARGET test_anisotropic_beads LABELS coarse_grain)

add_executable(test_orientation_coupling test_orientation_coupling.cpp)
target_link_libraries(test_orientation_coupling coarse_grain atomistic vsepr_core)
target_include_directories(test_orientation_coupling PRIVATE ${PROJECT_SOURCE_DIR})
vsepr_add_test(NAME OrientationCouplingTest TARGET test_orientation_coupling LABELS coarse_grain)

add_executable(test_descriptor_enrichment test_descriptor_enrichment.cpp)
target_link_libraries(test_descriptor_enrichment coarse_grain atomistic vsepr_core)
target_include_directories(test_descriptor_enrichment PRIVATE ${PROJECT_SOURCE_DIR})
vsepr_add_test(NAME DescriptorEnrichmentTest TARGET test_descriptor_enrichment LABELS coarse_grain)

add_executable(test_model_selection test_model_selection.cpp)
target_link_libraries(test_model_selection coarse_grain atomistic vsepr_core)
target_include_directories(test_model_selection PRIVATE ${PROJECT_SOURCE_DIR})
vsepr_add_test(NAME ModelSelectionTest TARGET test_model_selection LABELS coarse_grain)

add_executable(test_unified_descriptor test_unified_descriptor.cpp)
target_link_libraries(test_unified_descriptor coarse_grain atomistic vsepr_core)
target_include_directories(test_unified_descriptor PRIVATE ${PROJECT_SOURCE_DIR})
vsepr_add_test(NAME UnifiedDescriptorTest TARGET test_unified_descriptor LABELS coarse_grain)

add_executable(test_fragment_bridge test_fragment_bridge.cpp)
target_link_libraries(test_fragment_bridge coarse_grain atomistic vsepr_core)
target_include_directories(test_fragment_bridge PRIVATE ${PROJECT_SOURCE_DIR})
vsepr_add_test(NAME FragmentBridgeTest TARGET test_fragment_bridge LABELS coarse_grain)

# ============================================================================
# Group 8: CG Validation Suites (#1 - #8, Track 2, CLI, Bead Layers)
# ============================================================================

# Suite #1 -- Anisotropic Model (channel kernels, SH rotation)
add_executable(test_anisotropic_model test_anisotropic_model.cpp)
target_link_libraries(test_anisotropic_model coarse_grain atomistic vsepr_core)
target_include_directories(test_anisotropic_model PRIVATE ${PROJECT_SOURCE_DIR})
vsepr_add_test(NAME AnisotropicModelTest TARGET test_anisotropic_model LABELS coarse_grain cg_suites)

# Environment-Responsive Bead State
add_executable(test_environment_responsive test_environment_responsive.cpp)
target_link_libraries(test_environment_responsive coarse_grain atomistic vsepr_core)
target_include_directories(test_environment_responsive PRIVATE ${PROJECT_SOURCE_DIR})
vsepr_add_test(NAME EnvironmentResponsiveTest TARGET test_environment_responsive LABELS coarse_grain cg_suites)

# Suite #2 -- Environment Pipeline
add_executable(test_environment_suite2 test_environment_suite2.cpp)
target_link_libraries(test_environment_suite2 coarse_grain atomistic vsepr_core)
target_include_directories(test_environment_suite2 PRIVATE ${PROJECT_SOURCE_DIR})
vsepr_add_test(NAME EnvironmentSuite2Test TARGET test_environment_suite2 LABELS coarse_grain cg_suites)

# Track 2 -- Monte Carlo Robustness
add_executable(test_track2_robustness test_track2_robustness.cpp)
target_link_libraries(test_track2_robustness coarse_grain atomistic vsepr_core)
target_include_directories(test_track2_robustness PRIVATE ${PROJECT_SOURCE_DIR})
vsepr_add_test(NAME Track2RobustnessTest TARGET test_track2_robustness LABELS coarse_grain cg_suites)

# CG CLI Integration
add_executable(test_cg_cli test_cg_cli.cpp)
target_link_libraries(test_cg_cli coarse_grain atomistic vsepr_core)
target_include_directories(test_cg_cli PRIVATE ${PROJECT_SOURCE_DIR})
vsepr_add_test(NAME CGCLITest TARGET test_cg_cli LABELS coarse_grain cg_suites)

# Suite #3 -- Formation Studies
add_executable(test_formation_suite3 test_formation_suite3.cpp)
target_link_libraries(test_formation_suite3 coarse_grain atomistic vsepr_core)
target_include_directories(test_formation_suite3 PRIVATE ${PROJECT_SOURCE_DIR})
vsepr_add_test(NAME FormationSuite3Test TARGET test_formation_suite3 LABELS coarse_grain cg_suites)

# Suite #4 -- Dynamic Formation Regimes
add_executable(test_formation_regimes_suite4 test_formation_regimes_suite4.cpp)
target_link_libraries(test_formation_regimes_suite4 coarse_grain atomistic vsepr_core)
target_include_directories(test_formation_regimes_suite4 PRIVATE ${PROJECT_SOURCE_DIR})
vsepr_add_test(NAME FormationRegimesSuite4Test TARGET test_formation_regimes_suite4 LABELS coarse_grain cg_suites)

# Suite #5 -- Ensemble Proxies
add_executable(test_ensemble_proxy_suite5 test_ensemble_proxy_suite5.cpp)
target_link_libraries(test_ensemble_proxy_suite5 coarse_grain atomistic vsepr_core)
target_include_directories(test_ensemble_proxy_suite5 PRIVATE ${PROJECT_SOURCE_DIR})
vsepr_add_test(NAME EnsembleProxySuite5Test TARGET test_ensemble_proxy_suite5 LABELS coarse_grain cg_suites)

# Suite #6 -- Macro Precursor Channels
add_executable(test_macro_precursor_suite6 test_macro_precursor_suite6.cpp)
target_link_libraries(test_macro_precursor_suite6 coarse_grain atomistic vsepr_core)
target_include_directories(test_macro_precursor_suite6 PRIVATE ${PROJECT_SOURCE_DIR})
vsepr_add_test(NAME MacroPrecursorSuite6Test TARGET test_macro_precursor_suite6 LABELS coarse_grain cg_suites)

# Suite #7 -- Property Learning Pipeline
add_executable(test_property_pipeline_suite7 test_property_pipeline_suite7.cpp)
target_link_libraries(test_property_pipeline_suite7 coarse_grain atomistic vsepr_core)
target_include_directories(test_property_pipeline_suite7 PRIVATE ${PROJECT_SOURCE_DIR})
vsepr_add_test(NAME PropertyPipelineSuite7Test TARGET test_property_pipeline_suite7 LABELS coarse_grain cg_suites)

# Suite #8 -- Property Calibration
add_executable(test_property_calibration_suite8 test_property_calibration_suite8.cpp)
target_link_libraries(test_property_calibration_suite8 coarse_grain atomistic vsepr_core)
target_include_directories(test_property_calibration_suite8 PRIVATE ${PROJECT_SOURCE_DIR})
vsepr_add_test(NAME PropertyCalibrationSuite8Test TARGET test_property_calibration_suite8 LABELS coarse_grain cg_suites)

# Bead Layers -- Visualization data model
add_executable(test_bead_layers test_bead_layers.cpp)
target_link_libraries(test_bead_layers coarse_grain atomistic vsepr_core)
target_include_directories(test_bead_layers PRIVATE ${PROJECT_SOURCE_DIR})
vsepr_add_test(NAME BeadLayersTest TARGET test_bead_layers LABELS coarse_grain cg_suites)

# ---- VIZ support for CG test targets ----
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

# ============================================================================
# Group 9: Seed & Bead Stepper (6+9 Steady-State Step Function)
# ============================================================================

add_executable(test_seed_bead_stepper test_seed_bead_stepper.cpp)
target_link_libraries(test_seed_bead_stepper coarse_grain atomistic vsepr_core)
target_include_directories(test_seed_bead_stepper PRIVATE ${PROJECT_SOURCE_DIR})
vsepr_add_test(NAME SeedBeadStepperTest TARGET test_seed_bead_stepper LABELS coarse_grain reporting quick)

# ============================================================================
# Group 10: GPU vs CPU Benchmark
# ============================================================================

add_executable(bench_gpu_cpu bench_gpu_cpu.cpp)
target_link_libraries(bench_gpu_cpu vsepr_gpu vsepr_core vsepr_sim vsepr_pot)
target_include_directories(bench_gpu_cpu PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src)
vsepr_add_test(NAME BenchGpuCpu TARGET bench_gpu_cpu LABELS benchmark gpu)

# ============================================================================
# Disabled tests (API mismatch -- preserved for future re-enablement)
# ============================================================================
# formula_builder_tests, conformer_test,
# phase3_isomer_validation, phase4_coordination_variants,
# phase5_ionic_manifold, ensemble_consistency_test

# ============================================================================
# Re-enabled chemistry tests (API mismatch fixed 2026-06-18)
# ============================================================================

# chemistry_universal_test: element DB + organic + coordination compounds
add_executable(chemistry_universal_test chemistry_universal_test.cpp)
target_include_directories(chemistry_universal_test PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR})
set_target_properties(chemistry_universal_test PROPERTIES
    AUTOMOC OFF AUTOUIC OFF AUTORCC OFF CXX_SCAN_FOR_MODULES OFF)
target_link_libraries(chemistry_universal_test PRIVATE vsepr_chem vsepr_core)
vsepr_add_test(NAME ChemistryUniversalV2 TARGET chemistry_universal_test
    LABELS chemistry organic coordination element universal)

# phase2_complex_molecules: coordination complexes + hypervalent molecules
add_executable(phase2_complex_molecules phase2_complex_molecules.cpp)
target_include_directories(phase2_complex_molecules PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR})
set_target_properties(phase2_complex_molecules PROPERTIES
    AUTOMOC OFF AUTOUIC OFF AUTORCC OFF CXX_SCAN_FOR_MODULES OFF)
target_link_libraries(phase2_complex_molecules PRIVATE vsepr_chem vsepr_core)
vsepr_add_test(NAME Phase2ComplexMolecules TARGET phase2_complex_molecules
    LABELS chemistry coordination complex phase2)

# isomer_test: geometric isomers cis/trans/fac/mer + conformational + canonical hash
add_executable(isomer_test isomer_test.cpp)
target_include_directories(isomer_test PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR})
set_target_properties(isomer_test PROPERTIES
    AUTOMOC OFF AUTOUIC OFF AUTORCC OFF CXX_SCAN_FOR_MODULES OFF)
target_link_libraries(isomer_test PRIVATE vsepr_sim vsepr_pot vsepr_core)
vsepr_add_test(NAME IsomerTest TARGET isomer_test
    LABELS isomer chemistry conformer cis trans fac mer)

# ============================================================================
# Group 11: Electrohydrodynamic Multiphysics (EHD)
# ============================================================================

add_executable(test_ehd test_ehd.cpp)
target_link_libraries(test_ehd vsepr_ehd vsepr_core)
target_include_directories(test_ehd PRIVATE ${PROJECT_SOURCE_DIR})
vsepr_add_test(NAME EHDMultiphysicsTest TARGET test_ehd LABELS ehd multiphysics quick)

# ============================================================================
# Group 12: EHD Performance Benchmark
# ============================================================================

add_executable(bench_ehd bench_ehd.cpp)
target_link_libraries(bench_ehd vsepr_ehd vsepr_core)
target_include_directories(bench_ehd PRIVATE ${PROJECT_SOURCE_DIR})
vsepr_add_test(NAME EHDBenchmark TARGET bench_ehd LABELS ehd benchmark)

# ============================================================================
# Group 13: Dev Day ~42 Revised Notation (StateC)
# ============================================================================

add_executable(test_state_c test_state_c.cpp)
target_link_libraries(test_state_c atomistic vsepr_core)
target_include_directories(test_state_c PRIVATE ${PROJECT_SOURCE_DIR})
vsepr_add_test(NAME StateCRevisedNotationTest TARGET test_state_c LABELS atomistic core quick)

# ============================================================================
# Group 14: Emergence Dataset #1 â€” Anisotropy and Isomer-Fall Transitions
# ============================================================================

add_executable(test_emergence_dataset test_emergence_dataset.cpp)
target_link_libraries(test_emergence_dataset atomistic vsepr_core)
target_include_directories(test_emergence_dataset PRIVATE ${PROJECT_SOURCE_DIR})
vsepr_add_test(NAME EmergenceDatasetTest TARGET test_emergence_dataset LABELS atomistic datasets quick)

# ============================================================================
# Group 15: Â§15 Post-Formation Property Scraper
# ============================================================================

add_executable(test_property_scraper test_property_scraper.cpp)
target_link_libraries(test_property_scraper atomistic vsepr_core)
target_include_directories(test_property_scraper PRIVATE ${PROJECT_SOURCE_DIR})
vsepr_add_test(NAME PropertyScraperTest TARGET test_property_scraper LABELS atomistic analysis quick)


# ============================================================================
# Group 18: Scale Mission
# ============================================================================

add_executable(test_scale_mission test_scale_mission.cpp)
target_include_directories(test_scale_mission PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR})
target_link_libraries(test_scale_mission vsepr_gas2 vsepr_gas3 vsepr_core)
vsepr_add_test(NAME ScaleMissionTest TARGET test_scale_mission
    LABELS mission scale particles clouds lattice pipe quick)

# ============================================================================
# Group 19: Day #56 — Vec3 Type Unification Audit
# ============================================================================

add_executable(vec3_type_audit vec3_type_audit.cpp)
target_include_directories(vec3_type_audit PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR})
target_link_libraries(vec3_type_audit atomistic vsepr_core)
vsepr_add_test(NAME Vec3TypeAudit TARGET vec3_type_audit LABELS core math audit quick)

# ============================================================================
# Group 20: WO-056 — Eigen Bridge + Kabsch/RMSD
# ============================================================================

add_executable(test_eigen_bridge test_eigen_bridge.cpp)
target_include_directories(test_eigen_bridge PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR})
target_link_libraries(test_eigen_bridge vsepr_core vsepr_eigen)
vsepr_add_test(NAME EigenBridgeTest TARGET test_eigen_bridge
    LABELS core math eigen bridge kabsch rmsd quick)

# ============================================================================
# Group 21: Simulation Health Metrics Suite
# ============================================================================

add_executable(test_sim_metrics test_sim_metrics.cpp)
target_include_directories(test_sim_metrics PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR})
target_link_libraries(test_sim_metrics vsepr_core vsepr_eigen)
vsepr_add_test(NAME SimMetricsTest TARGET test_sim_metrics
    LABELS core stats metrics drift rmsd displacement structural quick)

# ============================================================================
# Group 22: Emergence Microtests (Tasks 4–8)
# ============================================================================
# Three-atom cluster, four-atom shape classification, 3x3 parameter sweep,
# reference-free emergence report, five-to-ten atom micro-nucleation.
# Analysis labels are computed in vsepr::cluster::ClusterAnalysis only;
# atomistic::State remains property-free.

add_executable(test_emergence test_emergence.cpp)
target_include_directories(test_emergence PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR})
target_link_libraries(test_emergence atomistic vsepr_core vsepr_eigen)
vsepr_add_test(NAME EmergenceTest TARGET test_emergence
    LABELS atomistic emergence cluster nucleation stats)

# ============================================================================
# Group 23: Crystal Imperfection and Surface Interaction Tests (Tasks 9–10)
# ============================================================================
# Task 9 — Full crystal imperfection emergence test.
#   SC / FCC / BCC Ar 3x3x3 supercells with seven perturbation variants.
#   Defect type inferred from trajectory via CrystalImperfectionAnalyzer.
#   atomistic::State remains property-free (no vacancy_fraction, defect_type, etc.).
#
# Task 10 — Full surface interaction and damage test.
#   Ar 3x3x5 slab with six incoming-velocity cases.
#   Surface behavior inferred from trajectory via SurfaceAnalyzer.
#   No erosion_rate, adsorption_probability, or surface_damage in state.

add_executable(test_crystal_surface test_crystal_surface.cpp)
target_include_directories(test_crystal_surface PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR})
target_link_libraries(test_crystal_surface atomistic vsepr_core vsepr_eigen)
vsepr_add_test(NAME CrystalSurfaceTest TARGET test_crystal_surface
    LABELS atomistic crystal surface imperfection emergence defect)

# ============================================================================
# Group 24: Macro Property Inference Layers (Tasks 12B + 13B)
# ============================================================================
# Task 12B — Diffusion-based macro transport inference.
#   Consumes MSD/jump/residence/tortuosity records from DiffusionTracker.
#   Outputs D_eff, axis diffusivities, anisotropy, mobility, transport class.
#   D_eff is computed from MSD slope only — never stored in State.
#
# Task 13B — Packing-based macro property inference.
#   Consumes contact/coordination/void/wall records from PackingTracker.
#   Outputs bulk density, porosity, compressibility, permeability, sintering.
#   No porosity, bulk_density, compressibility stored in xyzFull.

add_executable(test_macro_inference test_macro_inference.cpp)
target_include_directories(test_macro_inference PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR})
target_link_libraries(test_macro_inference atomistic vsepr_core vsepr_eigen)
vsepr_add_test(NAME MacroInferenceTest TARGET test_macro_inference
    LABELS atomistic diffusion packing macro inference transport porosity)

# ============================================================================
# Group 25: Lattice Type Classifier — Steinhardt Bond-Order Parameters
# ============================================================================
# Detects all common (SC, BCC, FCC, HCP, Diamond, ZincBlende, Wurtzite,
# NaCl, CsCl) and uncommon (Icosahedral, Interstitial, Amorphous, Liquid)
# lattice arrangements from atomic positions alone using Q4, Q6, W6.
# Analysis-only — no lattice_type or Q6 stored in atomistic::State.

add_executable(test_lattice_classifier test_lattice_classifier.cpp)
target_include_directories(test_lattice_classifier PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR})
target_link_libraries(test_lattice_classifier vsepr_core)
vsepr_add_test(NAME LatticeClassifierTest TARGET test_lattice_classifier
    LABELS atomistic crystal lattice steinhardt classification quick)

# ============================================================================
# Group 26: beta-7 Research Pipeline Round-Trip (T1–T8)
# ============================================================================
# Validates the full FormationOutput → FingerprintRecord → ClusterRecord →
# AnalysisRecord → ReportRecord → DashboardRecord pipeline using the 12-element
# v4 reference dataset. Header-only; no link dependency on simulation kernel.
#
# Tests:
#   T1  stage_fingerprint:   feature vector, topology hash, warnings
#   T2  stage_cluster:       cluster assignment, registry growth
#   T3  stage_analysis:      stability, packing, motif class, interpretation
#   T4  stage_report:        CSV row, JSON fragment, summary line
#   T5  stage_dashboard:     Markdown table, JSON array, run summary
#   T6  run_pipeline:        full round-trip, complete flag
#   T7  determinism:         identical cluster IDs across two runs
#   T8  CSV consistency:     header col count == row col count

add_executable(test_beta7_pipeline test_beta7_pipeline.cpp)
target_include_directories(test_beta7_pipeline PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR})
target_link_libraries(test_beta7_pipeline vsepr_core)
vsepr_add_test(NAME Beta7PipelineTest TARGET test_beta7_pipeline
    LABELS pipeline beta7 formation fingerprint cluster analysis report dashboard)

# ============================================================================
# Group 28: WO-56D-AuditChain — FormationEvent FIRE Exit Wiring
# ============================================================================
# Acceptance tests verifying that real FIRE relaxation runs record a
# FormationEvent into KernelEventLog — not just test harnesses.
#
# Tests:
#   1. test_fire_records_formation_event      — event appears after real FIRE run
#   2. test_fire_event_fields_are_populated   — symbolic trace, result_value, unit
#   3. test_fire_event_source_formula_threaded — source_formula threads from settings
#   4. test_fire_nonconverged_event_flagged   — is_valid=false, warning non-empty
#   5. test_fire_multiple_runs_accumulate_events — 2 runs → 2 events, monotonic IDs

add_executable(test_formation_event_wiring test_formation_event_wiring.cpp)
target_include_directories(test_formation_event_wiring PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR})
target_link_libraries(test_formation_event_wiring vsepr_sim vsepr_pot vsepr_core)
vsepr_add_test(NAME FormationEventWiringTest TARGET test_formation_event_wiring
    LABELS kernel audit formation fire event_log wiring beta10 quick)

# ============================================================================
# Group 27: VSIM Runtime Day #2 — O(N) Fit Detection Tests
# ============================================================================
# 1. test_cached_metric_guard        — while guards use cached metric; O(1) eval path
# 2. test_batch_artifact_isolation   — batch runs isolate artifacts per case
# 3. test_visual_decimation          — [visual external] render interval respected
# 4. test_N_evolution_event_counter  — spawn/remove counters drive dN/dt; no full scan
# 5. test_while_loop_safety_cap      — max_iter stops non-converging loops; warning emitted
# 6. test_open_closed_boundary_dispatch — closed/open/ambient boundary routing

add_executable(test_vsim_runtime_day2 test_vsim_runtime_day2.cpp)
target_include_directories(test_vsim_runtime_day2 PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR})
target_link_libraries(test_vsim_runtime_day2 vsepr_core)
vsepr_add_test(NAME VsimRuntimeDay2Test TARGET test_vsim_runtime_day2
    LABELS vsim runtime scripting while batch visual N_evolution boundary beta10 quick)

# ============================================================================
# Group 29: Script-Declared Export Flush
# ============================================================================
# 1. test_no_flags_no_output              — no [export] flags → no files written
# 2. test_jsonl_written                   — write_events_json=true → events.jsonl
# 3. test_markdown_written                — write_report_md=true → events.md
# 4. test_both_flags_both_files           — both flags → both files in one call
# 5. test_multi_run_appends               — multiple flush calls accumulate in one file
# 6. test_empty_log_produces_valid_files  — empty log → valid but empty files (no crash)

add_executable(test_export_flush test_export_flush.cpp)
target_include_directories(test_export_flush PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR})
target_link_libraries(test_export_flush vsepr_core)
vsepr_add_test(NAME ExportFlushTest TARGET test_export_flush
    LABELS vsim export kernel audit beta7 pipeline quick)

# ============================================================================
# Group 30: Phase 1 Gate 1 — Real Simulation Exit → Pipeline
# ============================================================================
# T1 — empty log → graceful empty DashboardRecord, no crash
# T2 — single FormationEvent → PipelineRecord + ClusterRecord produced
# T3 — multiple FormationEvents → AnalysisRecord produced for each
# T4 — symbol and energy round-trip through pipeline
# T5 — non-converged event → pipeline processes without drop
# T6 — run_pipeline_from_log does NOT mutate the log

add_executable(test_pipeline_sim_exit test_pipeline_sim_exit.cpp)
target_include_directories(test_pipeline_sim_exit PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR})
target_link_libraries(test_pipeline_sim_exit vsepr_sim vsepr_pot vsepr_core)
vsepr_add_test(NAME PipelineSimExitTest TARGET test_pipeline_sim_exit
    LABELS pipeline kernel audit formation beta7 phase1 gate1 quick)

# ============================================================================
# Group 31: Phase 2 Gate — Artifact Export Completion
# ============================================================================
# T1 — 2A: report MD and JSON written under reports/
# T2 — 2B: SVG dashboard written under reports/dashboard/ + PNG deferred marker
# T3 — 2C: audit JSONL written with all stage entries, no placeholder status
# T4 — 2D: write_analysis_json → pipeline_records.json present
# T5 — 2E: run_manifest.json written with all artifact paths + deferred list
# T6 — Full Phase 2 gate: all flags → all files present on disk

add_executable(test_artifact_export test_artifact_export.cpp)
target_include_directories(test_artifact_export PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR})
target_link_libraries(test_artifact_export vsepr_sim vsepr_pot vsepr_core)
vsepr_add_test(NAME ArtifactExportTest TARGET test_artifact_export
    LABELS pipeline export report dashboard audit formation beta7 phase2 gate2 quick)

# ============================================================================
# Group 32: WO-VSEPR-SIM-57B/57C — PBC Scripting Surface
# ============================================================================
# test_pbc_vsim_parser — [cell]/[boundary]/[pbc] parser + binding helpers (57B)
# test_pbc_bindings    — pbc.* binding correctness + error semantics (57C)

add_executable(test_pbc_vsim_parser test_pbc_vsim_parser.cpp
    ${PROJECT_SOURCE_DIR}/src/vsim/vsim_parser.cpp
    ${PROJECT_SOURCE_DIR}/src/vsim/bridge/bridge_parse.cpp)  # WO-67N bridge symbol
target_include_directories(test_pbc_vsim_parser PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR})
target_link_libraries(test_pbc_vsim_parser PRIVATE vsepr_chem)
vsepr_add_test(NAME PBCVsimParserTest TARGET test_pbc_vsim_parser
    LABELS pbc vsim parser scripting beta8 quick)

add_executable(test_pbc_bindings test_pbc_bindings.cpp
    ${PROJECT_SOURCE_DIR}/src/vsim/bindings/pbc_bindings.cpp)
target_include_directories(test_pbc_bindings PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR})
vsepr_add_test(NAME PBCBindingsTest TARGET test_pbc_bindings
    LABELS pbc vsim bindings scripting beta8 quick)

# ============================================================================
# Group 33: WO-VSEPR-SIM-57D — XYZVec3 / VsimInterpreter / PBC bridge
# ============================================================================
# test_vsim_value_xyzvec3      — VsimValue XYZVec3 / Int3 type layer (57D)
# test_vsim_interpreter_pbc    — VsimInterpreter pbc.* expression eval (57D)
# test_vsim_xyz_pbc_bridge     — xyz comment-line → runtime → pbc.* bridge (57D)

set(VSIM_INTERP_SRCS
    ${PROJECT_SOURCE_DIR}/src/vsim/vsim_interpreter.cpp
    ${PROJECT_SOURCE_DIR}/src/vsim/register_pbc_builtins.cpp
    ${PROJECT_SOURCE_DIR}/src/vsim/register_particle_builtins.cpp)

add_executable(test_vsim_value_xyzvec3 test_vsim_value_xyzvec3.cpp)
target_include_directories(test_vsim_value_xyzvec3 PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR})
vsepr_add_test(NAME VsimValueXYZVec3Test TARGET test_vsim_value_xyzvec3
    LABELS vsim value xyzvec3 scripting beta8 quick)

add_executable(test_vsim_interpreter_pbc test_vsim_interpreter_pbc.cpp
    ${VSIM_INTERP_SRCS})
target_include_directories(test_vsim_interpreter_pbc PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR})
vsepr_add_test(NAME VsimInterpreterPBCTest TARGET test_vsim_interpreter_pbc
    LABELS vsim interpreter pbc scripting beta8 quick)

add_executable(test_vsim_xyz_pbc_bridge test_vsim_xyz_pbc_bridge.cpp
    ${VSIM_INTERP_SRCS})
target_include_directories(test_vsim_xyz_pbc_bridge PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR})
vsepr_add_test(NAME VsimXYZPBCBridgeTest TARGET test_vsim_xyz_pbc_bridge
    LABELS vsim xyz pbc bridge scripting beta8 quick)

# ============================================================================
# Group 34: WO-VSEPR-SIM-57E — PBC post-step script execution
# ============================================================================
# test_vsim_post_step  — VsimRuntime::run_post_step() wires interpreter into
#                        simulation loop; [post_step] script_block executes
#                        pbc.* and particle.* against live particle state.

add_executable(test_vsim_post_step test_vsim_post_step.cpp
    ${VSIM_INTERP_SRCS})
target_include_directories(test_vsim_post_step PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR})
vsepr_add_test(NAME VsimPostStepTest TARGET test_vsim_post_step
    LABELS vsim pbc post_step scripting beta8 quick)

# ============================================================================
# Group 35: WO-VSEPR-SIM-57D — render_interval cadence field
# ============================================================================
# test_render_interval — VisualSection + VisualExternalSection render_interval
#                        field: default, step gating, zero guard, parser
#                        round-trip, external override, external fallback.

add_executable(test_render_interval test_render_interval.cpp
    ${PROJECT_SOURCE_DIR}/src/vsim/vsim_parser.cpp)
target_include_directories(test_render_interval PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR})
target_link_libraries(test_render_interval PRIVATE vsepr_chem)
vsepr_add_test(NAME RenderIntervalTest TARGET test_render_interval
    LABELS vsim visual render_interval scripting beta8 quick)

# ============================================================================
# Group 36: WO-VSIM-03B — Intent-based structure authoring (Level 0–4)
# ============================================================================
# test_wo_03b — MaterialSection, RunSection, EnvironmentSection, ExciteSection,
#               ObserveSection, ParticleOverrideEntry, RawObjectEntry.
#               Covers: L0A L0B L1A L1B L2A L2B L2C L3A L4A L4B ALIAS

add_executable(test_wo_03b test_wo_03b.cpp
    ${PROJECT_SOURCE_DIR}/src/vsim/vsim_parser.cpp)
target_include_directories(test_wo_03b PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR})
target_link_libraries(test_wo_03b PRIVATE vsepr_chem)
target_compile_options(test_wo_03b PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/FS>)
vsepr_add_test(NAME IntentAuthoringTest TARGET test_wo_03b
    LABELS vsim material run environment excite observe override raw scripting beta8 quick)







# =============================================================================
# WO-B9-001 — Batch Manifest Runner
#
# Targets:
#   batch_runner      — CLI entry point (manifest.json → run tree)
#
# New source files introduced:
#   apps/batch_runner.cpp
#   src/batch/manifest_loader.cpp
#   src/batch/manifest_runner.cpp
#
# Demo manifest:
#   scripts/b9_reactor_channel_001_manifest.json
#   (4 temperatures × 3 pressures × 3 seeds = 36 runs)
# =============================================================================

add_executable(batch_runner
    ${PROJECT_SOURCE_DIR}/apps/batch_runner.cpp
    ${PROJECT_SOURCE_DIR}/src/batch/manifest_loader.cpp
    ${PROJECT_SOURCE_DIR}/src/batch/manifest_runner.cpp)
target_include_directories(batch_runner PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR})
target_compile_options(batch_runner PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/FS>)
add_test(NAME BatchManifestRunner COMMAND batch_runner --help
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
set_tests_properties(BatchManifestRunner PROPERTIES
    LABELS "batch;manifest;wo-b9-001;beta9;quick;BatchRunner")

# =============================================================================
# WO-VSEPR-SIM-62B — Batch Verification Aggregation (Group 40 / Day 62 / beta-12)
#
# Tests: 20 assertions covering VerificationRunRecord aggregation,
#        pass-rate calculation, gate evaluation, failure mode classification,
#        TSV/markdown writers, and BatchRunRecord verify field wiring.
#
# New files introduced:
#   include/batch/failure_modes.hpp         (header — failure mode codes)
#   include/batch/batch_verification.hpp    (header — public API)
#   src/batch/batch_verification_kernel.hpp (header — internal kernel impl)
#   src/batch/batch_verification.cpp        (source — verification engine)
# =============================================================================

add_executable(test_batch_verification
    ${PROJECT_SOURCE_DIR}/tests/test_batch_verification.cpp
    ${PROJECT_SOURCE_DIR}/src/batch/batch_verification.cpp)
target_include_directories(test_batch_verification PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR})
target_compile_options(test_batch_verification PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/FS>)
vsepr_add_test(NAME BatchVerificationAggregation TARGET test_batch_verification
    LABELS batch verify aggregation wo-vsepr-sim-62b beta12 quick BatchVerification)

# =============================================================================
# WO-VSIM-62C — Batch Layer Parser & Static Axis Runtime (Groups 41 & 42)
#
# Group 41 — Batch parser tests (P1–P18)
#   Parser struct population, validate() error contracts,
#   [study], [batch.base/design/axis/case/expand], [seed], [formation.library.*]
#
# Group 42 — Batch expander tests (E1–E10)
#   Cartesian product, replicates, seed_policy split/shift,
#   SeedResolver derivation, cases × axes cross-product,
#   case/axis override precedence, batch_plan.tsv writer
#
# New source files introduced:
#   include/batch/batch_document.hpp
#   include/batch/batch_parser.hpp       src/batch/batch_parser.cpp
#   include/batch/batch_expander.hpp     src/batch/batch_expander.cpp
#   include/batch/seed_resolver.hpp      src/batch/seed_resolver.cpp
#   include/batch/batch_merger.hpp       src/batch/batch_merger.cpp
#   include/batch/batch_checkpoint.hpp   src/batch/batch_checkpoint.cpp
#   include/batch/batch_require_checker.hpp  src/batch/batch_require_checker.cpp
#   include/batch/batch_aggregator.hpp   src/batch/batch_aggregator.cpp
#   include/batch/resolved_writer.hpp    src/batch/resolved_writer.cpp
# =============================================================================

add_executable(test_batch_parser
    ${PROJECT_SOURCE_DIR}/tests/test_batch_parser.cpp
    ${PROJECT_SOURCE_DIR}/src/batch/batch_parser.cpp
    ${PROJECT_SOURCE_DIR}/src/batch/seed_resolver.cpp
    ${PROJECT_SOURCE_DIR}/src/vsim/vsim_parser.cpp)
target_include_directories(test_batch_parser PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR})
target_link_libraries(test_batch_parser PRIVATE vsepr_chem)
target_compile_options(test_batch_parser PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/FS>)
vsepr_add_test(NAME BatchParserGroup41 TARGET test_batch_parser
    LABELS batch parser wo-vsim-62c beta12 quick BatchParser)

add_executable(test_batch_expander
    ${PROJECT_SOURCE_DIR}/tests/test_batch_expander.cpp
    ${PROJECT_SOURCE_DIR}/src/batch/batch_parser.cpp
    ${PROJECT_SOURCE_DIR}/src/batch/batch_expander.cpp
    ${PROJECT_SOURCE_DIR}/src/batch/seed_resolver.cpp
    ${PROJECT_SOURCE_DIR}/src/vsim/vsim_parser.cpp)
target_include_directories(test_batch_expander PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR})
target_link_libraries(test_batch_expander PRIVATE vsepr_chem)
target_compile_options(test_batch_expander PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/FS>)
vsepr_add_test(NAME BatchExpanderGroup42 TARGET test_batch_expander
    LABELS batch expander wo-vsim-62c beta12 quick BatchExpander)

add_executable(test_batch_runner_static
    ${PROJECT_SOURCE_DIR}/tests/test_batch_runner_static.cpp
    ${PROJECT_SOURCE_DIR}/src/batch/batch_parser.cpp
    ${PROJECT_SOURCE_DIR}/src/batch/batch_expander.cpp
    ${PROJECT_SOURCE_DIR}/src/batch/seed_resolver.cpp
    ${PROJECT_SOURCE_DIR}/src/batch/batch_merger.cpp
    ${PROJECT_SOURCE_DIR}/src/batch/batch_checkpoint.cpp
    ${PROJECT_SOURCE_DIR}/src/batch/batch_require_checker.cpp
    ${PROJECT_SOURCE_DIR}/src/batch/batch_aggregator.cpp
    ${PROJECT_SOURCE_DIR}/src/batch/resolved_writer.cpp
    ${PROJECT_SOURCE_DIR}/src/vsim/io/demo_frame_sampler.cpp
    ${PROJECT_SOURCE_DIR}/src/vsim/io/demo_bundle_writer.cpp
    ${PROJECT_SOURCE_DIR}/src/xbundle/xbundle_reader.cpp
    ${PROJECT_SOURCE_DIR}/src/xbundle/xbundle_writer.cpp
    ${PROJECT_SOURCE_DIR}/src/xbundle/xbundle_validator.cpp
    ${PROJECT_SOURCE_DIR}/src/vsim/vsim_parser.cpp)
target_include_directories(test_batch_runner_static PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR})
target_link_libraries(test_batch_runner_static PRIVATE vsepr_chem vsepr_demo_stack vsepr_xbundle)
target_compile_options(test_batch_runner_static PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/FS>)
vsepr_add_test(NAME BatchRunnerStaticGroup43 TARGET test_batch_runner_static
    LABELS batch runner static wo-vsim-62c beta12 BatchRunnerStatic)

# ============================================================================
# WO-XSUITE-02B  —  Presolve layer tests  (EIG-1..10 + basis archive)
# Groups 44-47
# ============================================================================

# Group 44 — EigenMine small tests (EIG-1..6)
add_executable(test_eigenmine_small
    ${PROJECT_SOURCE_DIR}/tests/test_eigenmine_small.cpp
)
target_include_directories(test_eigenmine_small PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)
target_link_libraries(test_eigenmine_small PRIVATE vsepr_presolve)
vsepr_add_test(NAME EigenMineSmallGroup44 TARGET test_eigenmine_small
    LABELS eigenmine presolve wo-xsuite-02b EIG EigenMineSmall)

# Group 45 — CurveFit presolve tests (EIG-7..8)
add_executable(test_curvefit_presolve
    ${PROJECT_SOURCE_DIR}/tests/test_curvefit_presolve.cpp
)
target_include_directories(test_curvefit_presolve PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)
target_link_libraries(test_curvefit_presolve PRIVATE vsepr_presolve)
vsepr_add_test(NAME CurvefitPresolveGroup45 TARGET test_curvefit_presolve
    LABELS curvefit presolve wo-xsuite-02b EIG CurvefitPresolve)

# Group 46 — Release gate tests (EIG-9..10)
add_executable(test_release_gate
    ${PROJECT_SOURCE_DIR}/tests/test_release_gate.cpp
)
target_include_directories(test_release_gate PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)
target_link_libraries(test_release_gate PRIVATE vsepr_presolve)
vsepr_add_test(NAME ReleaseGateGroup46 TARGET test_release_gate
    LABELS release_gate presolve wo-xsuite-02b EIG ReleaseGate)

# Group 47 — Basis archive tests
add_executable(test_basis_archive
    ${PROJECT_SOURCE_DIR}/tests/test_basis_archive.cpp
)
target_include_directories(test_basis_archive PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)
target_link_libraries(test_basis_archive PRIVATE vsepr_presolve)
vsepr_add_test(NAME BasisArchiveGroup47 TARGET test_basis_archive
    LABELS basis_archive presolve wo-xsuite-02b BasisArchive)

# Group 48 — .xyza dynamic state gate (WO-VSIM-66A)
add_executable(test_xyza_dynamic_state
    ${PROJECT_SOURCE_DIR}/tests/test_xyza_dynamic_state.cpp
    ${PROJECT_SOURCE_DIR}/src/io/xyza_dynamic.cpp
)
target_include_directories(test_xyza_dynamic_state PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(test_xyza_dynamic_state PRIVATE vsepr_io vsepr_core)
vsepr_add_test(NAME XyzaDynamicStateGroup48 TARGET test_xyza_dynamic_state
    LABELS xyza dynamic_state io wo-66a XyzaDynamic)

# ============================================================================
# WO-BRIDGE-65 — Classical MD / Python-Empirical Bridge (Groups 49-52)
# ============================================================================

# Group 49 — Atom event records and logger routing
add_executable(test_bridge65_atom_events
    ${PROJECT_SOURCE_DIR}/tests/test_bridge65_atom_events.cpp
    ${PROJECT_SOURCE_DIR}/src/bridge65/atom_event_logger.cpp
    ${PROJECT_SOURCE_DIR}/src/bridge65/event_limiter.cpp
)
target_include_directories(test_bridge65_atom_events PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)
vsepr_add_test(NAME Bridge65AtomEventsGroup49 TARGET test_bridge65_atom_events
    LABELS bridge65 atom_events wo-bridge-65 Bridge65)

# Group 50 — High-rate event limiter/sampler
add_executable(test_bridge65_event_limiter
    ${PROJECT_SOURCE_DIR}/tests/test_bridge65_event_limiter.cpp
    ${PROJECT_SOURCE_DIR}/src/bridge65/event_limiter.cpp
)
target_include_directories(test_bridge65_event_limiter PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)
vsepr_add_test(NAME Bridge65EventLimiterGroup50 TARGET test_bridge65_event_limiter
    LABELS bridge65 event_limiter wo-bridge-65 Bridge65)

# Group 51 — Matrix force agreement validation
add_executable(test_bridge65_matrix_force
    ${PROJECT_SOURCE_DIR}/tests/test_bridge65_matrix_force.cpp
)
target_include_directories(test_bridge65_matrix_force PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)
vsepr_add_test(NAME Bridge65MatrixForceGroup51 TARGET test_bridge65_matrix_force
    LABELS bridge65 matrix_force wo-bridge-65 Bridge65)

# Group 52 — Replay hash independence
add_executable(test_bridge65_replay_hash
    ${PROJECT_SOURCE_DIR}/tests/test_bridge65_replay_hash.cpp
)
target_include_directories(test_bridge65_replay_hash PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)
vsepr_add_test(NAME Bridge65ReplayHashGroup52 TARGET test_bridge65_replay_hash
    LABELS bridge65 replay_hash wo-bridge-65 Bridge65)

# ============================================================================
# Group 53 — WO-67-B: Viewer Runtime Bridge and XYZ Data Contract
# ============================================================================

if(TARGET vsepr_view_lib)
    add_executable(test_view_67b
        ${PROJECT_SOURCE_DIR}/tests/test_view_67b.cpp
    )
    target_include_directories(test_view_67b PRIVATE
        ${PROJECT_SOURCE_DIR}/include
    )
    target_link_libraries(test_view_67b PRIVATE vsepr_view_lib vsepr_io vsepr_core)
    vsepr_add_test(NAME ViewerRuntimeBridge67B TARGET test_view_67b
        LABELS view wo-67-b viewer data_contract quick)
endif()

# ============================================================================
# WO-67N/67O — DEM + FEA Bridge Parse, Validate, Export (Day 67)
# ============================================================================

add_executable(test_bridge_parse
    ${PROJECT_SOURCE_DIR}/tests/test_bridge_parse.cpp
)
set_target_properties(test_bridge_parse PROPERTIES
    AUTOMOC OFF AUTORCC OFF AUTOUIC OFF WIN32_EXECUTABLE OFF)
target_include_directories(test_bridge_parse PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)
target_link_libraries(test_bridge_parse PRIVATE vsepr_chem vsepr_core)
if(MINGW)
    target_link_options(test_bridge_parse PRIVATE -mconsole -Wl,--subsystem,console -Wl,--entry,mainCRTStartup)
endif()
vsepr_add_test(NAME BridgeParseWO67 TARGET test_bridge_parse
    LABELS bridge wo-67n wo-67o dem_bridge fea_bridge parse quick)

# ============================================================================
# Phase 6 — Intent Runtime Bridge  (WO-VSIM-INTENT-BRIDGE-A)
# Group 54 — Intent runtime bridge basic
# ============================================================================

add_executable(test_intent_bridge_a
    ${PROJECT_SOURCE_DIR}/tests/test_intent_bridge_a.cpp
    ${PROJECT_SOURCE_DIR}/src/vsim/vsim_parser.cpp
)
target_include_directories(test_intent_bridge_a PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}
)
target_link_libraries(test_intent_bridge_a PRIVATE vsepr_chem)
vsepr_add_test(NAME IntentBridgeBasicGroup54 TARGET test_intent_bridge_a
    LABELS intent bridge material environment run wo-intent-bridge-a quick)

# ============================================================================
# Phase 7 — Formation FieldRamp  (WO-VSIM-FORMATION-FIELDRAMP)
# Group 55 — Formation FieldRamp
# ============================================================================

add_executable(test_fieldramp
    ${PROJECT_SOURCE_DIR}/tests/test_fieldramp.cpp
)
target_include_directories(test_fieldramp PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}
)
target_link_libraries(test_fieldramp PRIVATE vsepr_chem)
vsepr_add_test(NAME FormationFieldRampGroup55 TARGET test_fieldramp
    LABELS formation field_ramp wo-fieldramp quick)

# ============================================================================
# Phase 8 — Isomer Pipeline Wiring A  (WO-VSIM-ISOMER-WIRE-A)
# Group 56 — Isomer pipeline wiring A
# ============================================================================

add_executable(test_isomer_wire_a
    ${PROJECT_SOURCE_DIR}/tests/test_isomer_wire_a.cpp
)
target_include_directories(test_isomer_wire_a PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}
)
target_link_libraries(test_isomer_wire_a PRIVATE vsepr_chem)
vsepr_add_test(NAME IsomerWireAGroup56 TARGET test_isomer_wire_a
    LABELS isomer wo-isomer-wire-a generator deterministic quick)

# ============================================================================
# Phase 9 — Dynx v1 Session Archive  (WO-VSIM-DYNX-V1-A / WO-VSIM-DYNX-V1-B)
# Group 57 — Dynx v1 session archive
# ============================================================================

add_executable(test_dynx_v1
    ${PROJECT_SOURCE_DIR}/tests/test_dynx_v1.cpp
    ${PROJECT_SOURCE_DIR}/src/vsim/io/dynx_writer.cpp
)
target_include_directories(test_dynx_v1 PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}
)
target_link_libraries(test_dynx_v1 PRIVATE vsepr_core)
vsepr_add_test(NAME DynxV1SessionArchiveGroup57 TARGET test_dynx_v1
    LABELS dynx session_archive wo-dynx-v1 inspect validate quick)

# ============================================================================
# WO-VSIM-CTL  —  Modernized VSIM Control Pipeline
# Groups 58–64 — CTL-01 through CTL-07
# ============================================================================

# Group 58 — CTL-01: Typed command namespace registry
add_executable(test_ctl_01
    ${PROJECT_SOURCE_DIR}/tests/test_ctl_01.cpp
)
target_include_directories(test_ctl_01 PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}
)
target_link_libraries(test_ctl_01 PRIVATE vsepr_core)
vsepr_add_test(NAME CtlNamespaceRegistryGroup58 TARGET test_ctl_01
    LABELS ctl namespace registry wo-vsim-ctl quick)

# Group 59 — CTL-02: Script parser to execution graph
add_executable(test_ctl_02
    ${PROJECT_SOURCE_DIR}/tests/test_ctl_02.cpp
)
target_include_directories(test_ctl_02 PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}
)
target_link_libraries(test_ctl_02 PRIVATE vsepr_core)
vsepr_add_test(NAME CtlParserExecGraphGroup59 TARGET test_ctl_02
    LABELS ctl parser exec_graph wo-vsim-ctl quick)

# Group 60 — CTL-03: Runtime wrapper dispatch + validator
add_executable(test_ctl_03
    ${PROJECT_SOURCE_DIR}/tests/test_ctl_03.cpp
)
target_include_directories(test_ctl_03 PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}
)
target_link_libraries(test_ctl_03 PRIVATE vsepr_core)
vsepr_add_test(NAME CtlDispatchValidatorGroup60 TARGET test_ctl_03
    LABELS ctl dispatcher validator wo-vsim-ctl quick)

# Group 61 — CTL-04: Kernel channel command bindings
add_executable(test_ctl_04
    ${PROJECT_SOURCE_DIR}/tests/test_ctl_04.cpp
)
target_include_directories(test_ctl_04 PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}
)
target_link_libraries(test_ctl_04 PRIVATE vsepr_core)
vsepr_add_test(NAME CtlKernelChannelBindingsGroup61 TARGET test_ctl_04
    LABELS ctl kernel channel wo-vsim-ctl quick)

# Group 62 — CTL-05: Artifact command bindings (XBIT/Dynx)
add_executable(test_ctl_05
    ${PROJECT_SOURCE_DIR}/tests/test_ctl_05.cpp
)
target_include_directories(test_ctl_05 PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}
)
target_link_libraries(test_ctl_05 PRIVATE vsepr_core)
vsepr_add_test(NAME CtlArtifactBindingsGroup62 TARGET test_ctl_05
    LABELS ctl artifact dynx xbit wo-vsim-ctl quick)

# Group 63 — CTL-06: Metrics/gate assertion system
add_executable(test_ctl_06
    ${PROJECT_SOURCE_DIR}/tests/test_ctl_06.cpp
)
target_include_directories(test_ctl_06 PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}
)
target_link_libraries(test_ctl_06 PRIVATE vsepr_core)
vsepr_add_test(NAME CtlMetricsGateSystemGroup63 TARGET test_ctl_06
    LABELS ctl metrics gate assert wo-vsim-ctl quick)

# Group 64 — CTL-07: Deterministic plan hash + artifact manifest
add_executable(test_ctl_07
    ${PROJECT_SOURCE_DIR}/tests/test_ctl_07.cpp
)
target_include_directories(test_ctl_07 PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}
)
target_link_libraries(test_ctl_07 PRIVATE vsepr_core)
vsepr_add_test(NAME CtlPlanHashManifestGroup64 TARGET test_ctl_07
    LABELS ctl plan hash manifest deterministic wo-vsim-ctl quick)

# ============================================================================
# WO-VSIM-CTL-TEST  —  Groups 65–69: CTL Integration Testing
# ============================================================================

# Group 65 — CTL End-to-End Smoke
add_executable(test_ctl_smoke
    ${PROJECT_SOURCE_DIR}/tests/test_ctl_smoke.cpp
)
target_include_directories(test_ctl_smoke PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}
)
target_link_libraries(test_ctl_smoke PRIVATE vsepr_core)
vsepr_add_test(NAME CtlEndToEndSmokeGroup65 TARGET test_ctl_smoke
    LABELS ctl integration smoke end-to-end wo-vsim-ctl-test)

# Group 66 — CTL Runtime Hook Integration
add_executable(test_ctl_hooks
    ${PROJECT_SOURCE_DIR}/tests/test_ctl_hooks.cpp
)
target_include_directories(test_ctl_hooks PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}
)
target_link_libraries(test_ctl_hooks PRIVATE vsepr_core)
vsepr_add_test(NAME CtlRuntimeHookIntegrationGroup66 TARGET test_ctl_hooks
    LABELS ctl integration hooks runtime wo-vsim-ctl-test)

# Group 67 — CTL Negative Validation
add_executable(test_ctl_negative
    ${PROJECT_SOURCE_DIR}/tests/test_ctl_negative.cpp
)
target_include_directories(test_ctl_negative PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}
)
target_link_libraries(test_ctl_negative PRIVATE vsepr_core)
vsepr_add_test(NAME CtlNegativeValidationGroup67 TARGET test_ctl_negative
    LABELS ctl integration negative validation wo-vsim-ctl-test)

# Group 68 — CTL Determinism
add_executable(test_ctl_determinism
    ${PROJECT_SOURCE_DIR}/tests/test_ctl_determinism.cpp
)
target_include_directories(test_ctl_determinism PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}
)
target_link_libraries(test_ctl_determinism PRIVATE vsepr_core)
vsepr_add_test(NAME CtlDeterminismGroup68 TARGET test_ctl_determinism
    LABELS ctl integration determinism hash wo-vsim-ctl-test)

# Group 69 — CTL Existing Workflow Compatibility
add_executable(test_ctl_compat
    ${PROJECT_SOURCE_DIR}/tests/test_ctl_compat.cpp
)
target_include_directories(test_ctl_compat PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}
)
target_link_libraries(test_ctl_compat PRIVATE vsepr_core)
vsepr_add_test(NAME CtlWorkflowCompatGroup69 TARGET test_ctl_compat
    LABELS ctl integration compat legacy wo-vsim-ctl-test)

# ============================================================================
# WO-72A / WO-XYZSUITE-X  —  .X Framework Consolidated Test Inlet
# XFrameworkAudit  —  XBundle (XB-01..10) + XSuite (XP-01..05)
# Installed alias: vsepr-x-audit.exe  (see installer/setup.iss)
# ============================================================================

add_executable(test_x_framework
    ${PROJECT_SOURCE_DIR}/tests/test_x_framework.cpp
)
set_target_properties(test_x_framework PROPERTIES
    AUTOMOC OFF
    AUTOUIC OFF
    AUTORCC OFF
    CXX_SCAN_FOR_MODULES OFF
)
target_include_directories(test_x_framework PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}
)
target_link_libraries(test_x_framework PRIVATE vsepr_xbundle vsepr_core)
target_compile_features(test_x_framework PRIVATE cxx_std_23)
vsepr_add_test(NAME XFrameworkAudit TARGET test_x_framework
    LABELS xbundle xsuite bundle suite format smoke audit wo-72a wo-xyzsuite-x)

# ============================================================================
# WO-72B  —  .dynx Pipeline Emitter
# Group 71  —  DynxEmitter smoke (live cache + archive)
# ============================================================================

add_executable(test_dynx_emitter
    ${PROJECT_SOURCE_DIR}/tests/test_dynx_emitter.cpp
    ${PROJECT_SOURCE_DIR}/src/vsim/io/dynx_writer.cpp
    ${PROJECT_SOURCE_DIR}/src/vsim/io/dynx_emitter.cpp
)
target_include_directories(test_dynx_emitter PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}
)
target_link_libraries(test_dynx_emitter PRIVATE vsepr_core)
vsepr_add_test(NAME DynxEmitterGroup71 TARGET test_dynx_emitter
    LABELS dynx emitter live_cache post_step wo-72b quick)

# ============================================================================
# WO-72D  —  DynxWriter streaming API + DynxReader round-trip
# Group 72  —  Streaming write / read-back / session helpers
# ============================================================================

add_executable(test_dynx_writer_streaming
    ${PROJECT_SOURCE_DIR}/tests/test_dynx_writer_streaming.cpp
    ${PROJECT_SOURCE_DIR}/src/vsim/io/dynx_writer.cpp
    ${PROJECT_SOURCE_DIR}/src/vsim/io/dynx_reader.cpp
)
target_include_directories(test_dynx_writer_streaming PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}
)
target_link_libraries(test_dynx_writer_streaming PRIVATE vsepr_core)
vsepr_add_test(NAME DynxStreamingGroup72 TARGET test_dynx_writer_streaming
    LABELS dynx streaming reader session wo-72d quick)

# ============================================================================
# WO-OUTPUT-P2-A  —  ExportDemoSection parse + defaults
# Group 73
# ============================================================================

add_executable(test_export_demo_section
    ${PROJECT_SOURCE_DIR}/tests/test_export_demo_section.cpp
)
target_include_directories(test_export_demo_section PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}
)
target_link_libraries(test_export_demo_section PRIVATE vsepr_chem)
vsepr_add_test(NAME ExportDemoSectionGroup73 TARGET test_export_demo_section
    LABELS export demo schema wo-output-p2 quick)

# ============================================================================
# WO-OUTPUT-P2-B  —  DemoFrameSampler all strategies
# Group 74
# ============================================================================

add_executable(test_demo_frame_sampler
    ${PROJECT_SOURCE_DIR}/tests/test_demo_frame_sampler.cpp
)
target_include_directories(test_demo_frame_sampler PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}
)
target_link_libraries(test_demo_frame_sampler PRIVATE vsepr_core vsepr_demo_stack)
vsepr_add_test(NAME DemoFrameSamplerGroup74 TARGET test_demo_frame_sampler
    LABELS demo sampler dynx strategies wo-output-p2 quick)

# ============================================================================
# WO-OUTPUT-P2-C  —  DemoBundleWriter round-trip
# Group 75
# ============================================================================

add_executable(test_demo_bundle_writer
    ${PROJECT_SOURCE_DIR}/tests/test_demo_bundle_writer.cpp
)
target_include_directories(test_demo_bundle_writer PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}
)
target_link_libraries(test_demo_bundle_writer PRIVATE vsepr_core vsepr_demo_stack vsepr_xbundle)
vsepr_add_test(NAME DemoBundleWriterGroup75 TARGET test_demo_bundle_writer
    LABELS demo bundle xbundle round_trip wo-output-p2 quick)

# ============================================================================
# WO-OUTPUT-P2  —  Demo Pipeline Integration
# Group 76
# ============================================================================

add_executable(test_demo_pipeline_integration
    ${PROJECT_SOURCE_DIR}/tests/test_demo_pipeline_integration.cpp
)
target_include_directories(test_demo_pipeline_integration PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}
)
target_link_libraries(test_demo_pipeline_integration PRIVATE vsepr_core vsepr_demo_stack vsepr_xbundle)
vsepr_add_test(NAME DemoPipelineIntegrationGroup76 TARGET test_demo_pipeline_integration
    LABELS demo integration session sampler dynx live_cache wo-output-p2 quick)

# ============================================================================
# WO-72E  —  Default Usage Bundle (DUB) Factory Smoke
# Group 77
# ============================================================================

add_executable(test_dub_factory
    ${PROJECT_SOURCE_DIR}/tests/test_dub_factory.cpp
)
target_include_directories(test_dub_factory PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}
)
target_link_libraries(test_dub_factory PRIVATE vsepr_core)
vsepr_add_test(NAME DubFactoryGroup77 TARGET test_dub_factory
    LABELS bundle dub factory wo-72e quick)

# ============================================================================
# WO-73D  —  Length-Scale Fitter
# Group 78
# ============================================================================

add_executable(test_length_scale_fitter
    ${PROJECT_SOURCE_DIR}/tests/test_length_scale_fitter.cpp
)
target_include_directories(test_length_scale_fitter PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}
)
target_link_libraries(test_length_scale_fitter PRIVATE vsepr_length_fitter vsepr_core)
vsepr_add_test(NAME LengthScaleFitterGroup78 TARGET test_length_scale_fitter
    LABELS multiscale empirical fitter wo-73d quick)

# ============================================================================
# WO-73E  —  VSIM Output Filter
# Group 79
# ============================================================================

add_executable(test_vsim_output_filter
    ${PROJECT_SOURCE_DIR}/tests/test_vsim_output_filter.cpp
)
target_include_directories(test_vsim_output_filter PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}
)
target_link_libraries(test_vsim_output_filter PRIVATE
    -Wl,--whole-archive vsepr_vsim_filter -Wl,--no-whole-archive
    vsepr_core)
vsepr_add_test(NAME VsimOutputFilterGroup79 TARGET test_vsim_output_filter
    LABELS multiscale empirical filter wo-73e wo-74b quick)

# ============================================================================
# WO-74C  —  Gap Classifier
# Group 80
# ============================================================================

add_executable(test_gap_classifier
    ${PROJECT_SOURCE_DIR}/tests/test_gap_classifier.cpp
)
target_include_directories(test_gap_classifier PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}
)
# --whole-archive ensures self-registering AutoRegister statics from
# gap_classifier*.cpp.obj are not dead-stripped by the linker (WO-74A).
target_link_libraries(test_gap_classifier PRIVATE
    -Wl,--whole-archive vsepr_continual -Wl,--no-whole-archive
    vsepr_core)
vsepr_add_test(NAME GapClassifierGroup80 TARGET test_gap_classifier
    LABELS multiscale continual gap wo-74c quick)

# ============================================================================
# WO-75A  --  Observe Metrics
# Group 81
# ============================================================================

add_executable(test_observe_metrics
    ${PROJECT_SOURCE_DIR}/tests/test_observe_metrics.cpp
)
target_include_directories(test_observe_metrics PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}
)
target_link_libraries(test_observe_metrics PRIVATE
    vsepr_analysis_helpers
    vsepr_core)
vsepr_add_test(NAME ObserveMetricsGroup81 TARGET test_observe_metrics
    LABELS vsim analysis observe wo-75a quick)

# ============================================================================
# WO-75C  --  Formation Pathway Helper (stoichiometry / route logic)
# Group 82
# ============================================================================

add_executable(test_formation_helper
    ${PROJECT_SOURCE_DIR}/tests/test_formation_helper.cpp
)
target_include_directories(test_formation_helper PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}
)
target_link_libraries(test_formation_helper PRIVATE vsepr_core)
vsepr_add_test(NAME FormationHelperGroup82 TARGET test_formation_helper
    LABELS vsim formation helper wo-75c quick)

# ============================================================================
# WO-75E  --  Biological Object Layer (semantic non-geometry objects)
# Group 83
# ============================================================================

add_executable(test_bio_object
    ${PROJECT_SOURCE_DIR}/tests/test_bio_object.cpp
    ${PROJECT_SOURCE_DIR}/src/vsim/vsim_parser.cpp
)
target_include_directories(test_bio_object PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}
)
target_link_libraries(test_bio_object PRIVATE vsepr_chem)
vsepr_add_test(NAME BioObjectGroup83 TARGET test_bio_object
    LABELS bio object organic vsim wo-75e quick)

# ============================================================================
# WO-NL0C  --  Type-3 Demo Replay  (.dynx / .xyzf input-side + gl_spin)
# Group 84
# ============================================================================

add_executable(test_wo_nl0c
    ${PROJECT_SOURCE_DIR}/tests/test_wo_nl0c.cpp
    ${PROJECT_SOURCE_DIR}/src/vsim/vsim_parser.cpp
)
target_include_directories(test_wo_nl0c PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}
)
target_link_libraries(test_wo_nl0c PRIVATE vsepr_chem)
vsepr_add_test(NAME ReplayGroup84 TARGET test_wo_nl0c
    LABELS replay dynx xyzf gl_spin visual vsim wo-nl0c quick)

# ---------------------------------------------------------------------------
# Group 85  WO-NL0A  Type 1: continuous random-materials discovery
# ---------------------------------------------------------------------------
add_executable(test_wo_nl0a
    ${PROJECT_SOURCE_DIR}/tests/test_wo_nl0a.cpp
    ${PROJECT_SOURCE_DIR}/src/vsim/vsim_parser.cpp
)
target_include_directories(test_wo_nl0a PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}
)
target_link_libraries(test_wo_nl0a PRIVATE vsepr_chem)
vsepr_add_test(NAME DiscoveryGroup85 TARGET test_wo_nl0a
    LABELS discovery newleaf type1 vsim wo-nl0a quick)

# ---------------------------------------------------------------------------
# Group 86  WO-NL0B  Type 2: deterministic MD / crystal scripts
# ---------------------------------------------------------------------------
add_executable(test_wo_nl0b
    ${PROJECT_SOURCE_DIR}/tests/test_wo_nl0b.cpp
    ${PROJECT_SOURCE_DIR}/src/vsim/vsim_parser.cpp
)
target_include_directories(test_wo_nl0b PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}
)
target_link_libraries(test_wo_nl0b PRIVATE vsepr_chem)
vsepr_add_test(NAME CrystalMDGroup86 TARGET test_wo_nl0b
    LABELS md crystal type2 gl_spin vsim wo-nl0b quick)

# ============================================================================
# WO-75A  --  IKK Report End-Tag Enrichment
# Group 87
# ============================================================================

add_executable(test_ikk_end_tag
    ${PROJECT_SOURCE_DIR}/tests/test_ikk_end_tag.cpp
    ${PROJECT_SOURCE_DIR}/src/vsim/vsim_parser.cpp
    ${PROJECT_SOURCE_DIR}/src/vsim/analysis/ikk_end_tag.cpp
)
target_include_directories(test_ikk_end_tag PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}
)
target_link_libraries(test_ikk_end_tag PRIVATE vsepr_chem)
vsepr_add_test(NAME IkkEndTagGroup87 TARGET test_ikk_end_tag
    LABELS ikk identity sidecar report vsim wo-75a quick)

# ============================================================================
# Group 88  --  IKK Identity Vector  (WO-75B Phase 1)
# ============================================================================

add_executable(test_ikk_identity_vector
    ${PROJECT_SOURCE_DIR}/tests/test_ikk_identity_vector.cpp
    ${PROJECT_SOURCE_DIR}/src/vsim/vsim_parser.cpp
    ${PROJECT_SOURCE_DIR}/src/vsim/analysis/ikk_identity_vector.cpp
    ${PROJECT_SOURCE_DIR}/src/vsim/analysis/mcf_cai.cpp
)
target_include_directories(test_ikk_identity_vector PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}
)
target_link_libraries(test_ikk_identity_vector PRIVATE vsepr_chem)
vsepr_add_test(NAME IkkIdentityVectorGroup88 TARGET test_ikk_identity_vector
    LABELS ikk identity ivec sidecar vsim wo-75b quick)

# ============================================================================
# Group 89  --  MCF-CAI State Vector  (WO-76 Steps 3+5)
# ============================================================================

add_executable(test_mcf_cai
    ${PROJECT_SOURCE_DIR}/tests/test_mcf_cai.cpp
    ${PROJECT_SOURCE_DIR}/src/vsim/vsim_parser.cpp
    ${PROJECT_SOURCE_DIR}/src/vsim/analysis/mcf_cai.cpp
    ${PROJECT_SOURCE_DIR}/src/vsim/analysis/ikk_identity_vector.cpp
)
target_include_directories(test_mcf_cai PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}
)
target_link_libraries(test_mcf_cai PRIVATE vsepr_chem)
vsepr_add_test(NAME McfCaiGroup89 TARGET test_mcf_cai
    LABELS mcf cai identity vsim wo-76 quick)

# ============================================================================
# Group 90  --  WO-84S: VSEPR Observe Sink
# ============================================================================

add_executable(test_vsepr_observe_sink
    ${PROJECT_SOURCE_DIR}/tests/test_vsepr_observe_sink.cpp
    ${PROJECT_SOURCE_DIR}/atomistic/classify/vsepr.cpp
    ${PROJECT_SOURCE_DIR}/atomistic/classify/organic_candidate.cpp
)
target_include_directories(test_vsepr_observe_sink PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/atomistic
)
target_link_libraries(test_vsepr_observe_sink PRIVATE
    vsepr_analysis_helpers
    vsepr_core)
vsepr_add_test(NAME VseprObserveSinkGroup90 TARGET test_vsepr_observe_sink
    LABELS vsepr observe sink wo-84s quick)
