/**
 * seed_bead_demo.cpp — Live 60fps Seed-and-Bead Model Demo
 *
 * Demonstrates the unified 6+9 steady-state step function with real-time
 * visualization. Builds a small nanoparticle cluster and runs the stepper
 * with environment-responsive dynamics visible at 60fps.
 *
 * Usage:
 *   seed-bead-demo [material]
 *
 *   material:  Au | Ag | Cu | Ar | Pt | Fe   (default: Au)
 *
 * Features exercised:
 *   - SeedBeadStepper (6+9 step function)
 *   - SeedBeadViewer (60fps GLFW/ImGui visualization)
 *   - SnapshotGraphCollector (automatic data capture)
 *   - Material study reporting (Markdown + CSV)
 */

#include "coarse_grain/models/seed_bead_stepper.hpp"
#include "coarse_grain/report/snapshot_graph.hpp"
#include "coarse_grain/report/metal_gas_study.hpp"

#ifdef BUILD_VISUALIZATION
#include "coarse_grain/vis/seed_bead_viewer.hpp"
#endif

#include <cstring>
#include <iostream>
#include <string>

using namespace coarse_grain;
using namespace atomistic::validation::reference;

int main(int argc, char* argv[]) {
    std::cout << "=== VSEPR-SIM: Seed & Bead Model Demo ===\n";
    std::cout << "6+9 Steady-State Step Function\n\n";

    // Parse material argument
    std::string material = "Au";
    if (argc > 1) material = argv[1];

    // Select reference data
    atomistic::validation::ExperimentalRecord ref;
    if (material == "Au" || material == "au" || material == "gold") {
        ref = gold_5nm_fcc();
    } else if (material == "Ag" || material == "ag" || material == "silver") {
        ref = silver_5nm_fcc();
    } else if (material == "Cu" || material == "cu" || material == "copper") {
        ref = copper_4nm_fcc();
    } else if (material == "Ar" || material == "ar" || material == "argon") {
        ref = argon_5nm_cluster();
    } else if (material == "Pt" || material == "pt" || material == "platinum") {
        ref = platinum_5nm_fcc();
    } else if (material == "Fe" || material == "fe" || material == "iron") {
        ref = iron_5nm_bcc();
    } else {
        std::cerr << "Unknown material: " << material << "\n";
        std::cerr << "Options: Au, Ag, Cu, Ar, Pt, Fe\n";
        return 1;
    }

    std::cout << "Material: " << ref.label << "\n";
    std::cout << "Source: " << ref.source << "\n";
    std::cout << "Z=" << ref.Z << ", a=" << ref.lattice_constant << " A\n\n";

    // Build study configuration (32 beads for responsive demo)
    uint32_t n_beads = 32;
    auto cfg = build_study_config(ref, n_beads);

    std::cout << "Built " << cfg.system.beads.size() << " beads\n";
    std::cout << "Material class: " << material_class_name(cfg.material_class) << "\n";
    std::cout << "Parameters:\n";
    std::cout << "  tau = " << cfg.params.env_params.tau << " fs\n";
    std::cout << "  gamma_steric = " << cfg.params.env_params.gamma_steric << "\n";
    std::cout << "  gamma_elec = " << cfg.params.env_params.gamma_elec << "\n";
    std::cout << "  gamma_disp = " << cfg.params.env_params.gamma_disp << "\n\n";

#ifdef BUILD_VISUALIZATION
    // Launch live 60fps viewer
    std::string title = "VSEPR-SIM: Seed & Bead — " + std::string(ref.label);
    SeedBeadViewer::run(cfg.system, cfg.params, title);
#else
    // Headless mode: run study and generate report
    std::cout << "No visualization (BUILD_VIS=OFF). Running headless study...\n\n";

    auto result = run_material_study(cfg);

    std::cout << "Converged: " << (result.stepper_result.converged ? "YES" : "NO") << "\n";
    std::cout << "Steps: " << result.stepper_result.steps_taken << "\n";
    std::cout << "Final energy: " << result.stepper_result.final_energy << " kcal/mol\n";
    std::cout << "Final RMS force: " << result.stepper_result.final_rms_force << "\n";
    std::cout << "Final mean eta: " << result.stepper_result.final_avg_eta << "\n\n";

    // Write reports
    std::string report = generate_material_report(result, cfg);
    {
        std::string path = ref.label + "_report.md";
        std::ofstream f(path);
        if (f.is_open()) {
            f << report;
            std::cout << "Report written: " << path << "\n";
        }
    }

    result.collector.export_timeseries_csv(ref.label + "_timeseries.csv");
    std::cout << "CSV written: " << ref.label + "_timeseries.csv" << "\n";

    result.collector.export_snapshots_csv(ref.label + "_snapshots.csv");
    std::cout << "CSV written: " << ref.label + "_snapshots.csv" << "\n";

    // Comparison summary
    std::cout << "\n--- Experimental Comparison ---\n";
    for (auto& c : result.comparisons) {
        std::cout << "  " << c.name << ": "
                  << c.simulated << " vs " << c.experimental
                  << " (tol=" << c.tolerance << ") "
                  << (c.passed ? "PASS" : "FAIL") << "\n";
    }
    std::cout << "\nOverall: " << (result.overall_pass ? "PASS" : "FAIL") << "\n";
#endif

    return 0;
}
