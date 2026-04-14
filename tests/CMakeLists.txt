# ============================================================================
# VSEPR-SIM Modular Test Infrastructure - V3.0.X                             #
# ============================================================================     
# +-----------------------------------------------------------------------------+
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
    add_test(NAME ${T_NAME} COMMAND ${T_TARGET})
    if(T_LABELS)
        set_tests_properties(${T_NAME} PROPERTIES LABELS "${T_LABELS}")
    endif()
endfunction()

# Helper: VIZ support for CG test targets
function(add_test_viz_support target)
    if(VSEPR_VIS_READY)
        target_compile_definitions(${target} PRIVATE BUILD_VISUALIZATION)
        target_link_libraries(${target} vsepr_vis)
    endif()
endfunction()

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
vsepr_add_test(NAME BasicIsomerValidation TARGET basic_isomer_validation LABELS core)

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

# ============================================================================
# Group 3: Periodic Table / Nuclear
# ============================================================================

add_executable(test_periodic_table_102 test_periodic_table_102.cpp)
target_link_libraries(test_periodic_table_102 vsepr_periodic)
vsepr_add_test(NAME PeriodicTable102Test TARGET test_periodic_table_102 LABELS core periodic quick)

# DISABLED: unicode char literal errors under Clang/UCRT64
# add_executable(test_decay_chains test_decay_chains.cpp)
# target_link_libraries(test_decay_chains vsepr_periodic)
# vsepr_add_test(NAME DecayChainsTest TARGET test_decay_chains LABELS core periodic)

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

# DISABLED: unicode char literal errors under Clang/UCRT64
# add_executable(phase2_complex_molecules phase2_complex_molecules.cpp)
# target_link_libraries(phase2_complex_molecules vsepr_core)
# vsepr_add_test(NAME Phase2ComplexMolecules TARGET phase2_complex_molecules LABELS core chemistry)

# PBC example/demo
if(EXISTS ${PROJECT_SOURCE_DIR}/docs/pbc_example.cpp)
    add_executable(pbc_example ${PROJECT_SOURCE_DIR}/docs/pbc_example.cpp)
    target_link_libraries(pbc_example vsepr_core vsepr_box)
endif()

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

# ============================================================================
# Group 7: Coarse-Grained Mapping & Descriptors
# ============================================================================

add_executable(test_cg_mapping_suite test_cg_mapping.cpp)
target_link_libraries(test_cg_mapping_suite coarse_grain atomistic vsepr_core)
target_include_directories(test_cg_mapping_suite PRIVATE ${PROJECT_SOURCE_DIR})
vsepr_add_test(NAME CGMappingTest TARGET test_cg_mapping_suite LABELS coarse_grain)

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
# formula_builder_tests, conformer_test, isomer_test,
# phase3_isomer_validation, phase4_coordination_variants,
# phase5_ionic_manifold, chemistry_universal_test,
# ensemble_consistency_test

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
# Group 16: ISO 286 Tolerance & Fit (pure C)
# ============================================================================

add_executable(test_iso_fit test_iso_fit.c)
target_link_libraries(test_iso_fit vsepr_iso_fit)
target_include_directories(test_iso_fit PRIVATE ${PROJECT_SOURCE_DIR}/include)
set_source_files_properties(test_iso_fit.c PROPERTIES LANGUAGE C)
vsepr_add_test(NAME IsoFitTest TARGET test_iso_fit LABELS core engineering quick)

# Group 17: Fatigue analysis
# ============================================================================

add_executable(test_fatigue test_fatigue.c)
target_link_libraries(test_fatigue vsepr_fatigue)
target_include_directories(test_fatigue PRIVATE ${PROJECT_SOURCE_DIR}/include)
set_source_files_properties(test_fatigue.c PROPERTIES LANGUAGE C)
vsepr_add_test(NAME FatigueTest TARGET test_fatigue LABELS core engineering quick)
