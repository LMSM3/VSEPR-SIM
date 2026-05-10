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
target_link_libraries(basic_isomer_validation vsepr_sim vsepr_pot vsepr_core vsepr_analysis)
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

# ============================================================================
# Group 18: Scale Mission — V1 Particles / V2 Clouds / V3 Lattice / V4 Pipe Gas 3
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
    ${PROJECT_SOURCE_DIR}/src/vsim/vsim_parser.cpp)
target_include_directories(test_pbc_vsim_parser PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR})
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
target_compile_options(test_wo_03b PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/FS>)
vsepr_add_test(NAME IntentAuthoringTest TARGET test_wo_03b
    LABELS vsim material run environment excite observe override raw scripting beta8 quick)

# ============================================================================
# Group 37: WO-VSIM-03B Stress / Failure-Mapping
# ============================================================================
# test_wo_03b_stress — adversarial parse inputs for all WO-VSIM-03B sections:
#   [material], [run], [environment], [excite.*], [observe],
#   [[override.particle]], [[raw.object]], and general structural errors.
# Verifies that every known bad input produces a soft ParseError or
# ValidationResult warning rather than an uncaught exception or silent corruption.

add_executable(test_wo_03b_stress test_wo_03b_stress.cpp
    ${PROJECT_SOURCE_DIR}/src/vsim/vsim_parser.cpp)
target_include_directories(test_wo_03b_stress PRIVATE
    ${PROJECT_SOURCE_DIR}/include/vsim_override
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR})
target_compile_options(test_wo_03b_stress PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/FS>)
vsepr_add_test(NAME IntentAuthoringStressTest TARGET test_wo_03b_stress
    LABELS vsim material run environment excite observe override raw stress beta9 quick)

# ============================================================================
# Group 38: WO-VSIM-03C — Registry Resolution Engine
# ============================================================================
# test_wo_03c — RegistryBundle + RegistryResolver + VsimRuntime::resolve_material.
#   R1  RegistryBundle default-constructed is empty
#   R2  resolve_proto("B1_NaCl") returns correct fields
#   R3  alias "rocksalt" → B1_NaCl via MaterialSection
#   R4  prototype set directly resolves correctly
#   R5  [REGISTRY] log lines emitted for all fields
#   R6  unknown prototype — no crash, unpopulated bundle
#   R7  ionic family coverage (~11 aliases)
#   R8  elemental metals + graphene (is_periodic=false for graphene)
#   R9  molecular geometry aliases — is_periodic=false for all
#   R10 polymers / organics — populated, is_periodic=false
#   R11 porous / framework — is_periodic=true
#   R12 bead / premacro coverage
#   R13 VsimRuntime::resolve_material() with [material] section
#   R14 VsimRuntime::resolve_material() without [material] section
#   R15 Level 0 end-to-end gate: parse + resolve
#   R16 UO2 fluorite end-to-end + sub-registry fields (B9-15)
#   R17 resolve_export_profile — research_report / minimal / publication (B9-16)
#   R18 apply_registry_defaults — explicit user values not overwritten (B9-12)

add_executable(test_wo_03c test_wo_03c.cpp
    ${PROJECT_SOURCE_DIR}/src/vsim/vsim_parser.cpp)
target_include_directories(test_wo_03c PRIVATE
    ${PROJECT_SOURCE_DIR}/include/vsim_override
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR})
target_compile_options(test_wo_03c PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/FS>)
vsepr_add_test(NAME RegistryResolutionTest TARGET test_wo_03c
    LABELS vsim material registry resolver beta9 quick RegistryCore)

# ============================================================================
# Group 39: Demo Script Suite — file-based parse + validate + resolve
# ============================================================================
# test_demo_scripts — Parses all beta-9/10 demo .vsim scripts from disk.
#   D1  demo_01_nacl_level0     — Level 0 casual alias (rocksalt → B1_NaCl)
#   D2  demo_02_silicon_diamond — Level 1 prototype key (A4_Si → diamond_cubic)
#   D3  demo_03_pbc_nacl        — [cell]+[boundary]+[pbc]+Ewald fields
#   D4  demo_04_md_laser        — [run]=md, [excite.laser], [observe]
#   D5  demo_05_variance_while  — beta-10 inline [variance]/[N_evolution]/[while]
#   D6  demo_06_batch_sweep     — [batch] two-job parameter sweep

add_executable(test_demo_scripts test_demo_scripts.cpp
    ${PROJECT_SOURCE_DIR}/src/vsim/vsim_parser.cpp)
target_include_directories(test_demo_scripts PRIVATE
    ${PROJECT_SOURCE_DIR}/include/vsim_override
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR})
target_compile_options(test_demo_scripts PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/FS>)
target_compile_definitions(test_demo_scripts PRIVATE
    SCRIPTS_DIR="${PROJECT_SOURCE_DIR}/scripts")
vsepr_add_test(NAME DemoScriptSuiteTest TARGET test_demo_scripts
    LABELS vsim demo material registry pbc excite observe variance while batch beta9 beta10 quick RegistryCore)

# ============================================================================
# Group 40: Beta-10 Smoke Tests — install layout, CLI commands, run folders
# ============================================================================
# test_b10_core — Exercises beta-10 operational layer:
#   T1  AppDataPaths resolves config/bin/logs paths
#   T2  create_run_folder creates run_001/run_002 with manifest
#   T3  next_run_number increments past existing folders
#   T4  DoctorCommand instantiates and prints header/status
#   T5  InspectCommand summarises a .vsim file
#   T6  InspectCommand summarises a .xyz file
#   T7  InspectCommand summarises a project folder with run_001
#   T8  AssocCommand compiles and has correct names
#   T9  OpenCommand returns error for missing file
#   T10 RunCommand --safe validates and resolves minimal NaCl script

add_executable(test_b10_core test_b10_core.cpp
    ${PROJECT_SOURCE_DIR}/src/vsim/vsim_parser.cpp
    ${PROJECT_SOURCE_DIR}/src/cli/cmd_doctor.cpp
    ${PROJECT_SOURCE_DIR}/src/cli/cmd_run.cpp
    ${PROJECT_SOURCE_DIR}/src/cli/cmd_open.cpp
    ${PROJECT_SOURCE_DIR}/src/cli/cmd_inspect.cpp
    ${PROJECT_SOURCE_DIR}/src/cli/cmd_assoc.cpp
    ${PROJECT_SOURCE_DIR}/src/cli/cmd_validate.cpp
    ${PROJECT_SOURCE_DIR}/src/cli/cmd_version.cpp)
target_include_directories(test_b10_core PRIVATE
    ${PROJECT_SOURCE_DIR}/include/vsim_override
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR})
target_compile_options(test_b10_core PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/FS>)
vsepr_add_test(NAME Beta10SmokeTest TARGET test_b10_core
    LABELS beta10 install cli run inspect doctor assoc output_layout FinalBetaCore InstallCore)

# ============================================================================
# Group 35b: Ambient Chemistry Integration Tests
# ============================================================================
# test_chemistry_vsim — Parser, bridge, runtime, and observe-metric coverage
#   C1  [chemistry] block parses all keys
#   C2  [system] legacy alias routes to ChemistrySection
#   C3  heat = -1 derives from environment.temperature
#   C4  explicit heat overrides temperature derivation
#   C5  ChemistrySection defaults correct
#   C6  min_score_threshold default = 0.25
#   C7  chemistry = "none" → is_active() == false
#   C8  chemistry = "oxidation" → is_active() / has_chemistry() true
#   C9  ReactionBridge::run_pass non-negative evaluated count
#   C10 single-molecule doc → 0 pairs evaluated
#   C11 VsimRuntime::run_chemistry_pass proxies bridge
#   C12 eval_observe_metrics "reaction_events" counts log entries
#   C13 eval_observe_metrics empty-log → no crash, non-negative value
#   C14 unknown metric produces non-empty warning
#   C15 KernelEventLog count non-decreasing after run_pass

add_executable(test_chemistry_vsim test_chemistry_vsim.cpp
    ${PROJECT_SOURCE_DIR}/src/vsim/vsim_parser.cpp)
target_include_directories(test_chemistry_vsim PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR})
target_compile_options(test_chemistry_vsim PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/FS>)
vsepr_add_test(NAME AmbientChemistryTest TARGET test_chemistry_vsim
    LABELS vsim chemistry reaction heat_gate observe kernel_events ambient beta9 quick ChemistryCore)

# ============================================================================
# Group 38b: WO-VSIM-04A — Isomer Detection Revival
# ============================================================================
# test_isomer_detection — 9 unit tests for the graph-validity pipeline:
#   T1  Formula extraction from MolecularGraph
#   T2  Valence pass  — H-O-O-H → NeutralValid
#   T3  Valence fail  — H with 2 bonds → InvalidValence
#   T4  Disconnected fail — allow_fragments=false → DisconnectedInvalid
#   T5  Disconnected allowed — allow_fragments=true → NeutralValid
#   T6  Duplicate removal — same graph, different atom ordering → same hash
#   T7  Constitutional isomer — same formula, different graph hash
#   T8  Conformer — same graph hash, RMSD > tolerance
#   T9  Ring count via Euler formula

add_executable(test_isomer_detection test_isomer_detection.cpp)
target_link_libraries(test_isomer_detection vsepr_analysis vsepr_core)
target_include_directories(test_isomer_detection PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR})
target_compile_options(test_isomer_detection PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/FS>)
vsepr_add_test(NAME IsomerDetectionTest TARGET test_isomer_detection
    LABELS isomer graph valence canonical_hash wo04a quick IsomerCore)

# test_isomer_vsim_parser — VSIM parser integration for new isomer blocks:
#   P1  [analysis.isomers] parses all keys
#   P2  [generator.isomers] parses all keys
#   P3  [analysis.isomer_tracking] parses all keys
#   P4  Unknown keys fall through to raw_sections

add_executable(test_isomer_vsim_parser test_isomer_vsim_parser.cpp
    ${PROJECT_SOURCE_DIR}/src/vsim/vsim_parser.cpp)
target_include_directories(test_isomer_vsim_parser PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR})
target_compile_options(test_isomer_vsim_parser PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/FS>)
vsepr_add_test(NAME IsomerVsimParserTest TARGET test_isomer_vsim_parser
    LABELS isomer vsim parser wo04a quick IsomerParser)

# stress_isomer_pipeline — thousands of candidates through the full pipeline
#   Sections:
#     A  Validity sweep: N_PERM_REPEAT permutations × all base graphs
#     B  Hash permutation-invariance
#     C  Constitutional isomer family sweep (C2H4O2)
#     D  Conformer RMSD sweep
#     E  Known-invalid gate tests
#     F  Markdown/CSV/JSON report generation

add_executable(stress_isomer_pipeline stress_isomer_pipeline.cpp)
target_link_libraries(stress_isomer_pipeline vsepr_analysis vsepr_core)
target_include_directories(stress_isomer_pipeline PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR})
target_compile_options(stress_isomer_pipeline PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/FS>)
vsepr_add_test(NAME IsomerPipelineStress TARGET stress_isomer_pipeline
    LABELS isomer stress pipeline wo04a perf IsomerStress)

# isomer_demo — narrated end-to-end demo for WO-VSIM-04A:
#   Run 1  C2H6O constitutional isomers (ethanol vs DME)
#   Run 2  C2H4O2 five-member family (4 valid, 1 disconnected)
#   Run 3  Valence traps (H-overbonded, C-pentavalent, O-trivalent)
#   Run 4  Disconnected CO+CO: DisconnectedInvalid vs allow_fragments=true
#   Run 5  Permutation-invariance: ethanol over 20 shuffles (0 failures)
#   Run 6  RMSD conformer sweep: SameStructure vs Conformer boundary
#   Run 7  Full batch: 6 valid + 2 invalid candidates, 3 report files

add_executable(isomer_demo
    ${PROJECT_SOURCE_DIR}/apps/isomer_demo.cpp)
target_link_libraries(isomer_demo vsepr_analysis vsepr_core)
target_include_directories(isomer_demo PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR})
target_compile_options(isomer_demo PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/FS>)
vsepr_add_test(NAME IsomerDemo TARGET isomer_demo
    LABELS isomer demo wo04a quick IsomerDemo)

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
vsepr_add_test(NAME BatchManifestRunner TARGET batch_runner
    LABELS batch manifest wo-b9-001 beta9 quick BatchRunner)

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
    ${PROJECT_SOURCE_DIR}/src/vsim/vsim_parser.cpp)
target_include_directories(test_batch_runner_static PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR})
target_compile_options(test_batch_runner_static PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/FS>)
vsepr_add_test(NAME BatchRunnerStaticGroup43 TARGET test_batch_runner_static
    LABELS batch runner static wo-vsim-62c beta12 BatchRunnerStatic)
