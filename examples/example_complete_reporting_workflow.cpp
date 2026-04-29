/**
 * example_complete_reporting_workflow.cpp
 * 
 * Complete demonstration of BeadFIRE outcome reporting with visualization:
 *   1. Run FIRE minimization with full history recording
 *   2. Generate Markdown report
 *   3. Export CSV trajectory data
 *   4. Show Python visualization command
 *
 * Usage:
 *   ./example_complete_reporting_workflow [output_directory]
 *
 * Outputs:
 *   - minimization_report.md   (Human-readable documentation)
 *   - trajectory.csv           (Machine-readable data)
 *   - (Python script generates PNG visualizations)
 *
 * Anti-black-box: Every step, every position, every energy value is exported.
 */

#include "coarse_grain/report/bead_fire_report.hpp"
#include "coarse_grain/report/bead_fire_csv_export.hpp"
#include "coarse_grain/models/bead_fire.hpp"
#include "coarse_grain/core/bead.hpp"
#include "coarse_grain/core/unified_descriptor.hpp"
#include <iostream>
#include <chrono>
#include <cmath>

using namespace coarse_grain;

// ============================================================================
// Demo System Setup
// ============================================================================

std::vector<Bead> create_hexagonal_cluster(int n_beads = 6, double radius = 5.0) {
    """
    Create a ring of beads in a hexagonal arrangement.
    This will be used as an initial guess that needs optimization.
    """
    std::vector<Bead> beads;

    for (int i = 0; i < n_beads; ++i) {
        Bead b;

        // Position in a circle with some perturbation
        double angle = 2.0 * M_PI * i / n_beads;
        double r = radius + 0.5 * std::sin(3 * angle);  // Radial variation

        b.position.x = r * std::cos(angle);
        b.position.y = r * std::sin(angle);
        b.position.z = 0.5 * std::cos(2 * angle);  // Z-variation

        // Add random noise to make it non-optimal
        b.position.x += (std::rand() % 100 - 50) * 0.01;
        b.position.y += (std::rand() % 100 - 50) * 0.01;
        b.position.z += (std::rand() % 100 - 50) * 0.01;

        // Create unified descriptor
        b.unified = std::make_shared<UnifiedDescriptor>();
        b.unified->sigma_eff = 3.5;  // Effective radius (Å)

        // Initialize SH coefficients
        b.unified->sh_coeff_electrostatic.resize(9, 0.0);
        b.unified->sh_coeff_dispersion.resize(9, 0.0);
        b.unified->sh_coeff_steric.resize(9, 0.0);

        // Set monopole charges (alternating +/-)
        b.unified->sh_coeff_electrostatic[0] = (i % 2 == 0) ? 1.0 : -1.0;

        // Set isotropic dispersion
        b.unified->sh_coeff_dispersion[0] = -0.5;

        // Set steric repulsion
        b.unified->sh_coeff_steric[0] = 2.0;

        beads.push_back(b);
    }

    return beads;
}


void print_bead_positions(const std::vector<Bead>& beads, const std::string& label) {
    std::cout << "\n" << label << ":\n";
    for (size_t i = 0; i < beads.size(); ++i) {
        const auto& pos = beads[i].position;
        double r = std::sqrt(pos.x*pos.x + pos.y*pos.y + pos.z*pos.z);
        std::cout << "  Bead " << i << ": (" 
                  << std::fixed << std::setprecision(3)
                  << pos.x << ", " << pos.y << ", " << pos.z 
                  << ")  r=" << r << " Å\n";
    }
}


// ============================================================================
// Main Workflow
// ============================================================================

int main(int argc, char* argv[]) {
    std::cout << "==========================================================\n";
    std::cout << "   BeadFIRE Complete Reporting Workflow Demonstration\n";
    std::cout << "==========================================================\n\n";

    // Output directory
    std::string output_dir = (argc > 1) ? argv[1] : ".";
    std::cout << "Output directory: " << output_dir << "\n\n";

    // ========================================================================
    // Step 1: Create initial bead system
    // ========================================================================
    std::cout << "=== Step 1: System Setup ===\n";
    std::srand(12345);  // Reproducible random seed

    int n_beads = 8;
    std::vector<Bead> beads = create_hexagonal_cluster(n_beads, 5.0);
    std::vector<EnvironmentState> env_states;  // Empty for this demo

    print_bead_positions(beads, "Initial Configuration");

    // ========================================================================
    // Step 2: Configure FIRE parameters
    // ========================================================================
    std::cout << "\n=== Step 2: FIRE Configuration ===\n";

    BeadFIREParams fire_params;
    fire_params.max_steps = 300;
    fire_params.epsF = 1e-4;        // Force convergence
    fire_params.epsU = 1e-8;        // Energy convergence
    fire_params.dt = 1.0;
    fire_params.dt_max = 10.0;
    fire_params.fd_delta = 1e-4;

    std::cout << "Max steps:       " << fire_params.max_steps << "\n";
    std::cout << "Force threshold: " << fire_params.epsF << " kcal/mol·Å\n";
    std::cout << "Energy threshold: " << fire_params.epsU << " kcal/mol\n";

    // ========================================================================
    // Step 3: Configure interaction parameters
    // ========================================================================
    std::cout << "\n=== Step 3: Interaction Model ===\n";

    InteractionParams int_params;
    int_params.epsilon_elec = 1.0;    // Electrostatic strength
    int_params.epsilon_disp = 0.5;    // Dispersion strength
    int_params.epsilon_steric = 2.0;  // Steric repulsion strength

    std::cout << "Electrostatic: " << int_params.epsilon_elec << "\n";
    std::cout << "Dispersion:    " << int_params.epsilon_disp << "\n";
    std::cout << "Steric:        " << int_params.epsilon_steric << "\n";

    // ========================================================================
    // Step 4: Run minimization
    // ========================================================================
    std::cout << "\n=== Step 4: Running FIRE Minimization ===\n";
    std::cout << "Starting minimization...\n";

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

    std::cout << "\n✓ Minimization complete!\n";
    std::cout << "Execution time: " << duration.count() << " ms\n";

    // ========================================================================
    // Step 5: Console summary
    // ========================================================================
    std::cout << "\n=== Step 5: Quick Summary ===\n";
    std::cout << bead_fire_summary(result) << "\n";

    print_bead_positions(beads, "Final Configuration");

    // Compute configuration metrics
    double total_displacement = 0.0;
    for (size_t i = 0; i < beads.size() && i < result.history.size(); ++i) {
        if (!result.history.empty() && result.history[0].positions.size() > i) {
            auto& pos_init = result.history[0].positions[i];
            auto& pos_final = beads[i].position;
            double dx = pos_final.x - pos_init.x;
            double dy = pos_final.y - pos_init.y;
            double dz = pos_final.z - pos_init.z;
            total_displacement += std::sqrt(dx*dx + dy*dy + dz*dz);
        }
    }
    std::cout << "\nAverage displacement: " 
              << std::fixed << std::setprecision(3)
              << total_displacement / n_beads << " Å\n";

    // ========================================================================
    // Step 6: Generate Markdown report
    // ========================================================================
    std::cout << "\n=== Step 6: Generating Markdown Report ===\n";

    std::string md_path = output_dir + "/minimization_report.md";
    bool md_ok = write_bead_fire_report(md_path, result, n_beads, fire_params);

    if (md_ok) {
        std::cout << "✓ Markdown report: " << md_path << "\n";
    } else {
        std::cerr << "✗ Failed to write Markdown report\n";
    }

    // ========================================================================
    // Step 7: Export CSV trajectory
    // ========================================================================
    std::cout << "\n=== Step 7: Exporting CSV Trajectory ===\n";

    std::string csv_path = output_dir + "/trajectory.csv";
    bool csv_ok = write_bead_fire_csv(csv_path, result, beads);

    if (csv_ok) {
        std::cout << "✓ CSV trajectory: " << csv_path << "\n";
        std::cout << "  Steps recorded: " << result.history.size() << "\n";
        std::cout << "  Beads: " << n_beads << "\n";
    } else {
        std::cerr << "✗ Failed to write CSV trajectory\n";
    }

    // ========================================================================
    // Step 8: Visualization instructions
    // ========================================================================
    std::cout << "\n=== Step 8: Visualization ===\n";
    std::cout << "\nTo generate trajectory visualizations, run:\n\n";
    std::cout << "  python scripts/visualize_bead_fire_trajectory.py " << csv_path << "\n\n";
    std::cout << "This will create:\n";
    std::cout << "  - trajectory_xy.png      (XY plane projection)\n";
    std::cout << "  - trajectory_xz.png      (XZ plane projection)\n";
    std::cout << "  - energy_convergence.png (Energy vs. step)\n";
    std::cout << "  - force_evolution.png    (Forces vs. step)\n";
    std::cout << "  - fire_parameters.png    (α and Δt evolution)\n";
    std::cout << "  - combined_report.png    (All panels together)\n";

    // ========================================================================
    // Step 9: Diagnostic summary
    // ========================================================================
    std::cout << "\n=== Step 9: Diagnostic Summary ===\n";
    std::cout << "History recorded:  " << (result.record_history ? "Yes" : "No") << "\n";
    std::cout << "Trajectory steps:  " << result.history.size() << "\n";
    std::cout << "Converged:         " << (result.converged ? "Yes" : "No") << "\n";

    if (!result.history.empty()) {
        double U_init = result.history[0].U_total;
        double U_final = result.history.back().U_total;
        double dU = U_final - U_init;

        std::cout << "\nEnergy trajectory:\n";
        std::cout << "  Initial: " << std::fixed << std::setprecision(4) << U_init << " kcal/mol\n";
        std::cout << "  Final:   " << U_final << " kcal/mol\n";
        std::cout << "  Change:  " << dU << " kcal/mol\n";
        std::cout << "  Per bead: " << dU / n_beads << " kcal/mol\n";
    }

    // ========================================================================
    // Complete!
    // ========================================================================
    std::cout << "\n==========================================================\n";
    std::cout << "✓ Workflow Complete!\n";
    std::cout << "==========================================================\n\n";
    std::cout << "Review outputs:\n";
    std::cout << "  1. " << md_path << "  (documentation)\n";
    std::cout << "  2. " << csv_path << "  (trajectory data)\n";
    std::cout << "  3. Run Python script for visualizations\n\n";

    return 0;
}
