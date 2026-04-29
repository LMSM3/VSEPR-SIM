/**
 * example_bead_fire_reporting.cpp — Demonstration of BeadFIRE Outcome Reporting
 *
 * This example shows how to:
 *   1. Set up a simple bead system
 *   2. Run FIRE minimization
 *   3. Generate and export automated outcome reports
 *   4. Use console summaries for quick diagnostics
 *
 * Usage:
 *   ./example_bead_fire_reporting [output_directory]
 *
 * Outputs:
 *   - Console summary
 *   - Detailed Markdown report file
 */

#include "coarse_grain/report/bead_fire_report.hpp"
#include "coarse_grain/models/bead_fire.hpp"
#include "coarse_grain/core/bead.hpp"
#include "coarse_grain/core/unified_descriptor.hpp"
#include <iostream>
#include <string>
#include <vector>

using namespace coarse_grain;

// Helper: Create a simple 2-bead system for demonstration
std::vector<Bead> create_demo_beads() {
    std::vector<Bead> beads;

    // Bead 1: positioned at origin
    Bead b1;
    b1.position = {0.0, 0.0, 0.0};
    b1.unified = std::make_shared<UnifiedDescriptor>();
    b1.unified->sigma_eff = 3.5;  // Effective radius (Å)

    // Initialize SH coefficients (simple example)
    b1.unified->sh_coeff_electrostatic.resize(9, 0.0);
    b1.unified->sh_coeff_dispersion.resize(9, 0.0);
    b1.unified->sh_coeff_steric.resize(9, 0.0);

    // Set some non-zero values for electrostatic channel
    b1.unified->sh_coeff_electrostatic[0] = 1.0;  // l=0, m=0 (monopole)

    // Bead 2: positioned nearby (will be optimized)
    Bead b2;
    b2.position = {4.0, 0.0, 0.0};  // Start at 4.0 Å separation
    b2.unified = std::make_shared<UnifiedDescriptor>();
    b2.unified->sigma_eff = 3.5;

    b2.unified->sh_coeff_electrostatic.resize(9, 0.0);
    b2.unified->sh_coeff_dispersion.resize(9, 0.0);
    b2.unified->sh_coeff_steric.resize(9, 0.0);

    b2.unified->sh_coeff_electrostatic[0] = -1.0;  // Opposite charge

    beads.push_back(b1);
    beads.push_back(b2);

    return beads;
}

int main(int argc, char* argv[]) {
    std::cout << "=== BeadFIRE Reporting Example ===\n\n";

    // Output directory (default to current directory)
    std::string output_dir = (argc > 1) ? argv[1] : ".";

    // ========================================================================
    // Step 1: Set up the system
    // ========================================================================
    std::cout << "Setting up 2-bead demo system...\n";
    auto beads = create_demo_beads();
    std::vector<EnvironmentState> env_states;  // Empty for this demo

    // ========================================================================
    // Step 2: Configure FIRE parameters
    // ========================================================================
    BeadFIREParams fire_params;
    fire_params.max_steps = 500;
    fire_params.epsF = 1e-4;        // Force convergence threshold
    fire_params.epsU = 1e-8;        // Energy convergence threshold
    fire_params.dt = 1.0;
    fire_params.dt_max = 10.0;

    // ========================================================================
    // Step 3: Configure interaction parameters
    // ========================================================================
    InteractionParams int_params;
    int_params.epsilon_elec = 1.0;  // Electrostatic strength
    int_params.epsilon_disp = 0.5;  // Dispersion strength
    int_params.epsilon_steric = 2.0; // Steric repulsion strength

    // ========================================================================
    // Step 4: Run minimization
    // ========================================================================
    std::cout << "Running FIRE minimization...\n";
    auto start_time = std::chrono::steady_clock::now();

    BeadFIREResult result = BeadFIRE::minimize(
        beads,
        env_states,
        int_params,
        fire_params,
        {}  // Empty environment params
    );

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    std::cout << "Minimization completed in " << duration.count() << " ms\n\n";

    // ========================================================================
    // Step 5: Console Summary
    // ========================================================================
    std::cout << "--- Quick Summary ---\n";
    std::cout << bead_fire_summary(result) << "\n";
    std::cout << "Execution time: " << duration.count() << " ms\n";

    if (result.converged) {
        std::cout << "Final bead separation: " 
                  << std::sqrt(
                      atomistic::dot(
                          beads[1].position - beads[0].position,
                          beads[1].position - beads[0].position
                      )
                  ) << " Å\n";
    }
    std::cout << "\n";

    // ========================================================================
    // Step 6: Generate Detailed Report
    // ========================================================================
    std::string report_path = output_dir + "/bead_fire_demo_report.md";
    std::cout << "Generating detailed report...\n";

    bool success = write_bead_fire_report(
        report_path,
        result,
        static_cast<int>(beads.size()),
        fire_params
    );

    if (success) {
        std::cout << "✓ Report written to: " << report_path << "\n";
    } else {
        std::cerr << "✗ Failed to write report to: " << report_path << "\n";
        return 1;
    }

    // ========================================================================
    // Step 7: Diagnostic Output
    // ========================================================================
    std::cout << "\n--- Diagnostic Details ---\n";
    std::cout << "History recorded: " << (result.record_history ? "Yes" : "No") << "\n";
    if (result.record_history) {
        std::cout << "Trajectory steps: " << result.history.size() << "\n";

        if (!result.history.empty()) {
            std::cout << "Initial energy: " << result.history[0].U_total << " kcal/mol\n";
            std::cout << "Final energy: " << result.history.back().U_total << " kcal/mol\n";
            double dU = result.history.back().U_total - result.history[0].U_total;
            std::cout << "Energy change: " << dU << " kcal/mol\n";
        }
    }

    std::cout << "\n=== Example Complete ===\n";
    std::cout << "Review the generated report for full details.\n";

    return 0;
}
