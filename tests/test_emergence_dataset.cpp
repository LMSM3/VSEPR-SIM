/**
 * test_emergence_dataset.cpp
 *
 * Tests for Emergence Dataset #1: Anisotropy and Isomer-Fall Transitions.
 * Covers schema construction, fall detection, severity scoring,
 * anisotropy computation, CSV export, benchmark exemplars, and queries.
 */

#include "atomistic/datasets/emergence_dataset.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <string>

static constexpr double TOL = 1e-8;

static bool approx(double a, double b, double rel = 1e-4) {
    if (std::abs(b) < 1e-30) return std::abs(a) < 1e-10;
    return std::abs(a - b) / std::abs(b) < rel;
}

using namespace atomistic;
using namespace atomistic::emergence;

// ============================================================================
// Enum name tests
// ============================================================================

static void test_family_names() {
    assert(std::string(family_name(DatasetFamily::ED1A_Molecular))     == "ED1A-Molecular");
    assert(std::string(family_name(DatasetFamily::ED1B_Coordination))  == "ED1B-Coordination");
    assert(std::string(family_name(DatasetFamily::ED1C_BeadStructure)) == "ED1C-Bead/Structure");
    std::cout << "  [PASS] family_names\n";
}

static void test_system_class_names() {
    assert(std::string(system_class_name(SystemClass::SmallMolecule))       == "small_molecule");
    assert(std::string(system_class_name(SystemClass::CoordinationComplex)) == "coordination_complex");
    assert(std::string(system_class_name(SystemClass::BeadAssembly))        == "bead_assembly");
    std::cout << "  [PASS] system_class_names\n";
}

static void test_driver_type_names() {
    assert(std::string(driver_type_name(DriverType::Thermal))       == "thermal");
    assert(std::string(driver_type_name(DriverType::ElectricField)) == "electric_field");
    assert(std::string(driver_type_name(DriverType::Stress))        == "stress");
    assert(std::string(driver_type_name(DriverType::Radiation))     == "radiation");
    std::cout << "  [PASS] driver_type_names\n";
}

static void test_anisotropy_type_names() {
    assert(std::string(anisotropy_type_name(AnisotropyType::Geometric))  == "geometric");
    assert(std::string(anisotropy_type_name(AnisotropyType::Mechanical)) == "mechanical");
    assert(std::string(anisotropy_type_name(AnisotropyType::Transport))  == "transport");
    std::cout << "  [PASS] anisotropy_type_names\n";
}

static void test_fall_mode_names() {
    assert(std::string(fall_mode_name(FallMode::TorsionFall))              == "torsion_fall");
    assert(std::string(fall_mode_name(FallMode::CoordinationFall))         == "coordination_fall");
    assert(std::string(fall_mode_name(FallMode::CollapseFall))             == "collapse_fall");
    assert(std::string(fall_mode_name(FallMode::FieldInducedIsomerization)) == "field_induced_isomerization");
    assert(std::string(fall_mode_name(FallMode::None))                     == "none");
    std::cout << "  [PASS] fall_mode_names\n";
}

// ============================================================================
// AnisotropyDescriptor tests
// ============================================================================

static void test_anisotropy_isotropic() {
    AnisotropyDescriptor d;
    d.ratio = 1.0;
    assert(d.is_isotropic());

    d.ratio = 1.04;
    assert(d.is_isotropic());  // Below default tolerance 1.05

    d.ratio = 1.06;
    assert(!d.is_isotropic());
    std::cout << "  [PASS] anisotropy_isotropic\n";
}

static void test_compute_geometric_anisotropy_sphere() {
    // Points on a unit sphere approximation (6 points: ±x, ±y, ±z)
    std::vector<Vec3> pos = {
        {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1}
    };
    std::vector<double> masses(6, 1.0);

    auto desc = compute_geometric_anisotropy(pos, masses);

    // Symmetric distribution => λ₁ ≈ λ₂ ≈ λ₃ => ratio ≈ 1
    assert(desc.ratio < 1.01);
    assert(desc.asphericity < 0.01);
    std::cout << "  [PASS] geometric_anisotropy_sphere\n";
}

static void test_compute_geometric_anisotropy_rod() {
    // Rod along x-axis: clearly anisotropic
    std::vector<Vec3> pos = {
        {0,0,0}, {2,0,0}, {4,0,0}, {6,0,0}, {8,0,0}, {10,0,0}
    };
    std::vector<double> masses(6, 1.0);

    auto desc = compute_geometric_anisotropy(pos, masses);

    // λ₁ >> λ₂ ≈ λ₃ (both near zero for perfect line)
    assert(desc.ratio > 5.0);
    assert(desc.eigenvalues[0] > desc.eigenvalues[1]);
    assert(desc.eigenvalues[0] > desc.eigenvalues[2]);
    std::cout << "  [PASS] geometric_anisotropy_rod\n";
}

static void test_compute_geometric_anisotropy_disc() {
    // Disc in xy-plane
    std::vector<Vec3> pos = {
        {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0},
        {0.7,0.7,0}, {-0.7,-0.7,0}, {0.7,-0.7,0}, {-0.7,0.7,0}
    };
    std::vector<double> masses(8, 1.0);

    auto desc = compute_geometric_anisotropy(pos, masses);

    // λ₁ ≈ λ₂ >> λ₃ (λ₃ near zero for perfect plane)
    assert(desc.eigenvalues[0] > 0.01);
    assert(desc.eigenvalues[1] > 0.01);
    // λ₃ should be essentially zero (all in xy-plane)
    assert(desc.eigenvalues[2] < 1e-10);
    assert(desc.ratio > 1e6);  // Effectively infinite
    std::cout << "  [PASS] geometric_anisotropy_disc\n";
}

// ============================================================================
// Fall severity
// ============================================================================

static void test_fall_severity_zero() {
    double s = compute_fall_severity(0.0, 0.0, 0.0);
    assert(approx(s, 0.0));
    std::cout << "  [PASS] fall_severity_zero\n";
}

static void test_fall_severity_max() {
    FallSeverityWeights w;
    double s = compute_fall_severity(w.rmsd_max * 2, w.dE_max * 2, w.tau_max * 2, w);
    assert(approx(s, 1.0));  // Clamped to 1.0
    std::cout << "  [PASS] fall_severity_max\n";
}

static void test_fall_severity_partial() {
    FallSeverityWeights w;
    w.w1 = 0.5; w.w2 = 0.3; w.w3 = 0.2;
    w.rmsd_max = 10.0; w.dE_max = 100.0; w.tau_max = 1000.0;

    double s = compute_fall_severity(5.0, 50.0, 500.0, w);
    // 0.5 * (5/10) + 0.3 * (50/100) + 0.2 * (500/1000) = 0.25 + 0.15 + 0.1 = 0.5
    assert(approx(s, 0.5));
    std::cout << "  [PASS] fall_severity_partial\n";
}

// ============================================================================
// Fall detection
// ============================================================================

static void test_fall_detection_rmsd() {
    EmergenceSample s;
    s.rmsd = 1.0;
    s.initial_state.name = "A";
    s.final_state.name = "A";  // Same label
    s.barrier_energy = 100.0;
    s.driver.magnitude = 0.1;  // Barrier >> perturbation

    FallDetectionConfig cfg;
    cfg.rmsd_threshold = 0.5;

    assert(s.detect_fall(cfg));  // RMSD criterion triggers
    std::cout << "  [PASS] fall_detection_rmsd\n";
}

static void test_fall_detection_barrier() {
    EmergenceSample s;
    s.rmsd = 0.1;  // Below threshold
    s.initial_state.name = "A";
    s.final_state.name = "A";  // Same label
    s.barrier_energy = 2.0;
    s.driver.magnitude = 5.0;  // Perturbation > barrier

    FallDetectionConfig cfg;
    cfg.rmsd_threshold = 0.5;

    assert(s.detect_fall(cfg));  // Barrier criterion triggers
    std::cout << "  [PASS] fall_detection_barrier\n";
}

static void test_fall_detection_state_label() {
    EmergenceSample s;
    s.rmsd = 0.1;
    s.barrier_energy = 100.0;
    s.driver.magnitude = 0.1;
    s.initial_state.name = "anti";
    s.final_state.name = "gauche";  // Different label

    FallDetectionConfig cfg;
    cfg.rmsd_threshold = 0.5;

    assert(s.detect_fall(cfg));  // State label criterion triggers
    std::cout << "  [PASS] fall_detection_state_label\n";
}

static void test_fall_detection_no_fall() {
    EmergenceSample s;
    s.rmsd = 0.1;
    s.barrier_energy = 100.0;
    s.driver.magnitude = 0.1;
    s.initial_state.name = "A";
    s.final_state.name = "A";

    FallDetectionConfig cfg;
    cfg.rmsd_threshold = 0.5;

    assert(!s.detect_fall(cfg));  // Nothing triggers
    std::cout << "  [PASS] fall_detection_no_fall\n";
}

// ============================================================================
// StateLabel equality
// ============================================================================

static void test_state_label_eq() {
    StateLabel a{"anti", "linear_backbone", 0, ""};
    StateLabel b{"anti", "linear_backbone", 0, ""};
    StateLabel c{"gauche", "linear_backbone", 0, ""};

    assert(a == b);
    assert(a != c);
    std::cout << "  [PASS] state_label_eq\n";
}

// ============================================================================
// EmergenceDataset container
// ============================================================================

static void test_dataset_add_and_query() {
    EmergenceDataset ds;

    EmergenceSample s1;
    s1.family = DatasetFamily::ED1A_Molecular;
    s1.fall_flag = true;
    s1.fall_mode = FallMode::TorsionFall;
    ds.add(std::move(s1));

    EmergenceSample s2;
    s2.family = DatasetFamily::ED1B_Coordination;
    s2.fall_flag = false;
    ds.add(std::move(s2));

    EmergenceSample s3;
    s3.family = DatasetFamily::ED1A_Molecular;
    s3.fall_flag = true;
    s3.fall_mode = FallMode::ConformerFall;
    s3.recoverable = true;
    ds.add(std::move(s3));

    assert(ds.total() == 3);
    assert(ds.fall_count() == 2);
    assert(ds.recoverable_count() == 1);

    auto mol = ds.by_family(DatasetFamily::ED1A_Molecular);
    assert(mol.size() == 2);

    auto coord = ds.by_family(DatasetFamily::ED1B_Coordination);
    assert(coord.size() == 1);

    auto falls = ds.falls_only();
    assert(falls.size() == 2);

    auto torsions = ds.by_fall_mode(FallMode::TorsionFall);
    assert(torsions.size() == 1);

    std::cout << "  [PASS] dataset_add_and_query\n";
}

static void test_dataset_detect_all_falls() {
    EmergenceDataset ds;

    EmergenceSample s1;
    s1.initial_state.name = "anti";
    s1.final_state.name = "gauche";
    s1.rmsd = 0.8;
    s1.delta_E = 0.9;
    s1.residence_time_final = 30.0;
    ds.add(std::move(s1));

    EmergenceSample s2;
    s2.initial_state.name = "chair";
    s2.final_state.name = "chair";  // No change
    s2.rmsd = 0.1;
    s2.barrier_energy = 100.0;
    s2.driver.magnitude = 0.1;
    ds.add(std::move(s2));

    ds.detect_all_falls();

    assert(ds.samples[0].fall_flag == true);
    assert(ds.samples[0].fall_severity > 0.0);
    assert(ds.samples[1].fall_flag == false);
    std::cout << "  [PASS] dataset_detect_all_falls\n";
}

// ============================================================================
// CSV export
// ============================================================================

static void test_csv_export() {
    EmergenceDataset ds;

    EmergenceSample s;
    s.family = DatasetFamily::ED1A_Molecular;
    s.system_class = SystemClass::SmallMolecule;
    s.composition = "C4H10";
    s.initial_state.name = "anti";
    s.final_state.name = "gauche";
    s.fall_flag = true;
    s.fall_mode = FallMode::TorsionFall;
    s.driver.type = DriverType::Thermal;
    ds.add(std::move(s));

    std::string csv = ds.to_csv();

    // Check header
    assert(csv.find("sample_id,family") != std::string::npos);
    // Check data row
    assert(csv.find("ED1A-Molecular") != std::string::npos);
    assert(csv.find("small_molecule") != std::string::npos);
    assert(csv.find("C4H10") != std::string::npos);
    assert(csv.find("torsion_fall") != std::string::npos);
    assert(csv.find("thermal") != std::string::npos);
    std::cout << "  [PASS] csv_export\n";
}

// ============================================================================
// Summary report
// ============================================================================

static void test_summary_report() {
    EmergenceDataset ds;
    ds.name = "Test Dataset";

    EmergenceSample s;
    s.family = DatasetFamily::ED1A_Molecular;
    s.fall_flag = true;
    s.fall_severity = 0.5;
    ds.add(std::move(s));

    std::string report = ds.summary();
    assert(report.find("Test Dataset") != std::string::npos);
    assert(report.find("Total samples:     1") != std::string::npos);
    assert(report.find("Fall events:       1") != std::string::npos);
    std::cout << "  [PASS] summary_report\n";
}

// ============================================================================
// Benchmark exemplars
// ============================================================================

static void test_benchmark_exemplars() {
    auto ds = build_benchmark_exemplars();

    assert(ds.total() == 5);
    assert(ds.fall_count() == 5);  // All five are fall events

    // Check families
    auto mol = ds.by_family(DatasetFamily::ED1A_Molecular);
    assert(mol.size() == 2);  // Butane + cyclohexane

    auto coord = ds.by_family(DatasetFamily::ED1B_Coordination);
    assert(coord.size() == 1);  // Pt complex

    auto bead = ds.by_family(DatasetFamily::ED1C_BeadStructure);
    assert(bead.size() == 2);  // Crystal platelet + bead assembly

    // Check specific exemplars
    assert(ds.samples[0].composition == "C4H10");
    assert(ds.samples[0].initial_state.name == "anti");
    assert(ds.samples[0].final_state.name == "gauche");
    assert(ds.samples[0].fall_mode == FallMode::TorsionFall);

    assert(ds.samples[1].composition == "C6H12");
    assert(ds.samples[1].fall_mode == FallMode::ConformerFall);
    assert(ds.samples[1].metastable);

    assert(ds.samples[2].composition == "PtCl2(NH3)2");
    assert(ds.samples[2].fall_mode == FallMode::CoordinationFall);
    assert(ds.samples[2].hysteresis);

    assert(ds.samples[3].fall_mode == FallMode::OrientationFall);
    assert(ds.samples[3].anisotropy.ratio > 8.0);

    assert(ds.samples[4].fall_mode == FallMode::CollapseFall);
    assert(!ds.samples[4].recoverable);

    // All should have severity computed
    for (const auto& s : ds.samples)
        assert(s.fall_severity > 0.0);

    std::cout << "  [PASS] benchmark_exemplars\n";
}

static void test_benchmark_csv_roundtrip() {
    auto ds = build_benchmark_exemplars();
    std::string csv = ds.to_csv();

    // Verify all five exemplars appear
    assert(csv.find("C4H10") != std::string::npos);
    assert(csv.find("C6H12") != std::string::npos);
    assert(csv.find("PtCl2(NH3)2") != std::string::npos);
    assert(csv.find("MoS2_fragment") != std::string::npos);
    assert(csv.find("CG_branch_12bead") != std::string::npos);

    // Count data lines (header + 5 rows)
    int newlines = 0;
    for (char c : csv) if (c == '\n') ++newlines;
    assert(newlines == 6);  // 1 header + 5 data rows

    std::cout << "  [PASS] benchmark_csv_roundtrip\n";
}

static void test_benchmark_summary() {
    auto ds = build_benchmark_exemplars();
    std::string report = ds.summary();

    assert(report.find("Total samples:     5") != std::string::npos);
    assert(report.find("Fall events:       5") != std::string::npos);
    assert(report.find("ED1A-Molecular") != std::string::npos);
    assert(report.find("ED1B-Coordination") != std::string::npos);
    assert(report.find("ED1C-Bead/Structure") != std::string::npos);
    std::cout << "  [PASS] benchmark_summary\n";
}

// ============================================================================
// PerturbationDriver
// ============================================================================

static void test_perturbation_driver() {
    PerturbationDriver drv;
    drv.type = DriverType::ElectricField;
    drv.magnitude = 0.05;
    drv.direction = {1, 0, 0};
    drv.duration = 100.0;
    drv.unit = "V/A";

    assert(drv.type == DriverType::ElectricField);
    assert(drv.magnitude > 0.0);
    assert(drv.direction.x == 1.0);
    std::cout << "  [PASS] perturbation_driver\n";
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "=== Emergence Dataset #1: Anisotropy and Isomer-Fall Tests ===\n\n";

    std::cout << "--- Enum Names ---\n";
    test_family_names();
    test_system_class_names();
    test_driver_type_names();
    test_anisotropy_type_names();
    test_fall_mode_names();

    std::cout << "\n--- Anisotropy Descriptors ---\n";
    test_anisotropy_isotropic();
    test_compute_geometric_anisotropy_sphere();
    test_compute_geometric_anisotropy_rod();
    test_compute_geometric_anisotropy_disc();

    std::cout << "\n--- Fall Severity ---\n";
    test_fall_severity_zero();
    test_fall_severity_max();
    test_fall_severity_partial();

    std::cout << "\n--- Fall Detection ---\n";
    test_fall_detection_rmsd();
    test_fall_detection_barrier();
    test_fall_detection_state_label();
    test_fall_detection_no_fall();

    std::cout << "\n--- State Label ---\n";
    test_state_label_eq();

    std::cout << "\n--- Dataset Container ---\n";
    test_dataset_add_and_query();
    test_dataset_detect_all_falls();

    std::cout << "\n--- Export ---\n";
    test_csv_export();
    test_summary_report();

    std::cout << "\n--- Perturbation ---\n";
    test_perturbation_driver();

    std::cout << "\n--- Benchmark Exemplars ---\n";
    test_benchmark_exemplars();
    test_benchmark_csv_roundtrip();
    test_benchmark_summary();

    std::cout << "\n=== ALL 25 TESTS PASSED ===\n";
    return 0;
}
