/**
 * bead_fire_report_test.cpp — Test and demonstrate BeadFIRE outcome reporting
 *
 * Verifies that:
 *   1. Report generation produces valid Markdown
 *   2. All critical metrics are included
 *   3. History data is properly formatted
 *   4. File export works correctly
 */

#include "coarse_grain/report/bead_fire_report.hpp"
#include "coarse_grain/models/bead_fire.hpp"
#include <cassert>
#include <iostream>
#include <sstream>

using namespace coarse_grain;

void test_basic_report_generation() {
    std::cout << "Test: Basic report generation... ";

    // Create a mock BeadFIREResult with synthetic data
    BeadFIREResult result;
    result.steps_taken = 150;
    result.U_final = -245.678;
    result.Frms_final = 0.00005;
    result.Fmax_final = 0.00012;
    result.alpha_final = 0.025;
    result.dt_final = 8.5;
    result.converged = true;
    result.U_steric = -100.2;
    result.U_electrostatic = -120.5;
    result.U_dispersion = -24.978;
    result.record_history = true;

    // Add some history steps
    for (int i = 0; i < 200; i += 10) {
        BeadFIREStep step;
        step.step = i;
        step.U_total = -200.0 - (i * 0.2);
        step.U_steric = -80.0 - (i * 0.1);
        step.U_electrostatic = -100.0 - (i * 0.08);
        step.U_dispersion = -20.0 - (i * 0.02);
        step.Frms = 1.0 / (1.0 + i * 0.1);
        step.Fmax = 2.0 / (1.0 + i * 0.1);
        step.alpha = 0.1 * std::exp(-i * 0.01);
        step.dt = 1.0 + i * 0.03;
        step.dU_per_bead = (i > 0) ? 0.01 / (1.0 + i * 0.1) : 0.0;
        result.history.push_back(step);
    }

    BeadFIREParams params;
    int n_beads = 12;

    // Generate report
    std::string report = bead_fire_report_md(result, n_beads, params);

    // Verify critical sections exist
    assert(report.find("# Bead FIRE Minimization Report") != std::string::npos);
    assert(report.find("## Convergence Outcome") != std::string::npos);
    assert(report.find("## Energy Decomposition") != std::string::npos);
    assert(report.find("## FIRE Algorithm Parameters") != std::string::npos);
    assert(report.find("## Convergence History") != std::string::npos);
    assert(report.find("## Mathematical Model") != std::string::npos);

    // Verify convergence status
    assert(report.find("✓ Converged") != std::string::npos);

    // Verify metrics are present
    assert(report.find("150") != std::string::npos); // steps
    assert(report.find("-245.678") != std::string::npos); // U_final

    std::cout << "PASS\n";
}

void test_summary_generation() {
    std::cout << "Test: Summary generation... ";

    BeadFIREResult result;
    result.converged = true;
    result.steps_taken = 75;
    result.U_final = -123.456;
    result.Frms_final = 0.0001;

    std::string summary = bead_fire_summary(result);

    assert(summary.find("Converged") != std::string::npos);
    assert(summary.find("75") != std::string::npos);
    assert(summary.find("-123.456") != std::string::npos);

    std::cout << "PASS\n";
    std::cout << "  Summary: " << summary << "\n";
}

void test_non_converged_report() {
    std::cout << "Test: Non-converged report... ";

    BeadFIREResult result;
    result.steps_taken = 2000;
    result.U_final = -50.0;
    result.Frms_final = 0.5;
    result.Fmax_final = 1.2;
    result.alpha_final = 0.1;
    result.dt_final = 1.0;
    result.converged = false;
    result.U_steric = -20.0;
    result.U_electrostatic = -25.0;
    result.U_dispersion = -5.0;

    BeadFIREParams params;
    int n_beads = 8;

    std::string report = bead_fire_report_md(result, n_beads, params);

    assert(report.find("✗ Did not converge") != std::string::npos);
    assert(report.find("2000") != std::string::npos);

    std::cout << "PASS\n";
}

void test_file_export() {
    std::cout << "Test: File export... ";

    BeadFIREResult result;
    result.steps_taken = 100;
    result.U_final = -100.0;
    result.Frms_final = 0.0001;
    result.Fmax_final = 0.0005;
    result.alpha_final = 0.05;
    result.dt_final = 5.0;
    result.converged = true;
    result.U_steric = -40.0;
    result.U_electrostatic = -50.0;
    result.U_dispersion = -10.0;

    BeadFIREParams params;
    int n_beads = 6;

    std::string test_path = "test_bead_fire_report.md";
    bool success = write_bead_fire_report(test_path, result, n_beads, params);

    assert(success);

    std::cout << "PASS\n";
    std::cout << "  Report written to: " << test_path << "\n";
}

void test_no_history_report() {
    std::cout << "Test: Report without history... ";

    BeadFIREResult result;
    result.steps_taken = 50;
    result.U_final = -80.0;
    result.Frms_final = 0.00008;
    result.Fmax_final = 0.0002;
    result.alpha_final = 0.03;
    result.dt_final = 6.0;
    result.converged = true;
    result.U_steric = -30.0;
    result.U_electrostatic = -40.0;
    result.U_dispersion = -10.0;
    result.record_history = false;
    // history vector is empty

    BeadFIREParams params;
    int n_beads = 10;

    std::string report = bead_fire_report_md(result, n_beads, params);

    assert(report.find("*History recording was disabled") != std::string::npos);

    std::cout << "PASS\n";
}

int main() {
    std::cout << "=== BeadFIRE Report Generation Tests ===\n\n";

    test_basic_report_generation();
    test_summary_generation();
    test_non_converged_report();
    test_file_export();
    test_no_history_report();

    std::cout << "\n=== All tests passed ===\n";

    return 0;
}
