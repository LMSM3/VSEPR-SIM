/**
 * report_generator.cpp
 * --------------------
 * VSEPR-SIM Autonomous Report Generator
 * Work Order: WO-TMS-CRG-001
 *
 * Continuously generates thermal-materials digital experiment reports
 * with escalating complexity from simple single-element cases to
 * exotic multi-factor coupled systems.
 *
 * Usage:
 *   report_generator [OPTIONS]
 *
 * Options:
 *   --count N       Number of reports to generate (default: 1000)
 *   --seed N        Base RNG seed (default: 42)
 *   --out DIR       Output directory (default: reports)
 *   --no-files      Skip writing individual .md files
 *   --no-csv        Skip writing summary CSV
 *   --quiet         Suppress progress output
 *   --interval N    Progress print interval (default: 50)
 *   --l2 N          L2 escalation threshold (default: 50)
 *   --l3 N          L3 escalation threshold (default: 150)
 *   --l4 N          L4 escalation threshold (default: 400)
 *   --l5 N          L5 escalation threshold (default: 700)
 *   --help          Show this help
 */

#include "core/report_engine.hpp"
#include <iostream>
#include <cstring>
#include <string>

int main(int argc, char** argv) {
    vsepr::report::EngineConfig config;

    // Parse command-line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            std::cout << R"(
VSEPR-SIM Autonomous Report Generator
Work Order: WO-TMS-CRG-001

Usage: report_generator [OPTIONS]

Options:
  --count N       Number of reports to generate (default: 1000)
  --seed N        Base RNG seed (default: 42)
  --out DIR       Output directory (default: reports)
  --no-files      Skip writing individual .md files
  --no-csv        Skip writing summary CSV
  --quiet         Suppress progress output
  --interval N    Progress print interval (default: 50)
  --l2 N          L2 escalation threshold (default: 50)
  --l3 N          L3 escalation threshold (default: 150)
  --l4 N          L4 escalation threshold (default: 400)
  --l5 N          L5 escalation threshold (default: 700)

Complexity Levels:
  L1: Simple uniform single-element materials
  L2: Binary/ternary alloys and compounds
  L3: Anisotropic, graded, or layered materials
  L4: Transient thermal loading with material variation
  L5: Rare/unstable synthetic configurations

Output:
  reports/TMS-NNNNNN.md   Individual Markdown reports
  reports/summary.csv     Cumulative CSV log
)";
            return 0;
        }
        else if (arg == "--count" && i + 1 < argc) {
            config.target_reports = std::stoi(argv[++i]);
        }
        else if (arg == "--seed" && i + 1 < argc) {
            config.base_seed = std::stoull(argv[++i]);
        }
        else if (arg == "--out" && i + 1 < argc) {
            config.output_dir = argv[++i];
        }
        else if (arg == "--no-files") {
            config.write_individual = false;
        }
        else if (arg == "--no-csv") {
            config.write_csv_log = false;
        }
        else if (arg == "--quiet") {
            config.print_progress = false;
        }
        else if (arg == "--interval" && i + 1 < argc) {
            config.progress_interval = std::stoi(argv[++i]);
        }
        else if (arg == "--l2" && i + 1 < argc) {
            config.threshold_l2 = std::stoi(argv[++i]);
        }
        else if (arg == "--l3" && i + 1 < argc) {
            config.threshold_l3 = std::stoi(argv[++i]);
        }
        else if (arg == "--l4" && i + 1 < argc) {
            config.threshold_l4 = std::stoi(argv[++i]);
        }
        else if (arg == "--l5" && i + 1 < argc) {
            config.threshold_l5 = std::stoi(argv[++i]);
        }
        else {
            std::cerr << "Unknown option: " << arg << "\n";
            std::cerr << "Run with --help for usage.\n";
            return 1;
        }
    }

    vsepr::report::AutonomousEngine engine(config);
    return engine.run();
}
