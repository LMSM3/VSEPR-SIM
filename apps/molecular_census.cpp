/**
 * molecular-census: Deep Molecular Analysis CLI
 *
 * Collects 112+ atomistic data points on a molecule from a single primary
 * FIRE build instance. Discovers alternative minimization geometries.
 * Automatically classifies by group / sub-group / ionic character / purpose.
 *
 * Usage:
 *   molecular-census <formula>                  # Analyze single molecule
 *   molecular-census <formula> --trials 100     # More conformer trials
 *   molecular-census <formula> --seed 123       # Reproducible RNG
 *   molecular-census <formula> --quiet          # Suppress banners
 *
 * Output: Structured text report to stdout (pipe to file for archival).
 */

#include "analysis/molecular_census.hpp"
#include "pot/periodic_db.hpp"

#include <iostream>
#include <string>
#include <cstdlib>
#include <cstring>

static void print_usage() {
    std::cout << R"(
molecular-census — Deep Molecular Analysis (112+ data points)

Usage:
  molecular-census <formula> [options]

Options:
  --trials <N>    Conformer search trials (default: 50)
  --seed <N>      RNG seed (default: 42)
  --quiet         Suppress phase banners
  --fire-steps N  Max FIRE iterations (default: 5000)
  --help          Show this help

Examples:
  molecular-census H2O
  molecular-census CH4 --trials 100 --seed 99
  molecular-census SF6 --quiet
  molecular-census C2H6 --trials 200 > ethane_census.txt

)" << std::endl;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    std::string formula;
    vsepr::analysis::MolecularCensus::Settings settings;
    bool quiet = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            print_usage();
            return 0;
        } else if (arg == "--trials" && i + 1 < argc) {
            settings.conformer_trials = std::atoi(argv[++i]);
        } else if (arg == "--seed" && i + 1 < argc) {
            settings.conformer_seed = std::atoi(argv[++i]);
        } else if (arg == "--fire-steps" && i + 1 < argc) {
            settings.max_fire_steps = std::atoi(argv[++i]);
        } else if (arg == "--quiet") {
            quiet = true;
            settings.verbose = false;
        } else if (formula.empty()) {
            formula = arg;
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            return 1;
        }
    }

    if (formula.empty()) {
        std::cerr << "Error: no formula specified.\n";
        print_usage();
        return 1;
    }

    // Load periodic table
    vsepr::PeriodicTable pt;
    try {
        pt = vsepr::PeriodicTable::load_separated("data/elements.physics.json", "");
    } catch (const std::exception& e) {
        std::cerr << "Failed to load periodic table: " << e.what() << "\n";
        std::cerr << "Ensure data/elements.physics.json exists.\n";
        return 1;
    }

    // Run census
    vsepr::analysis::MolecularCensus census(pt, settings);
    auto result = census.run(formula);

    // Generate and print report
    std::string report = vsepr::analysis::MolecularCensus::generate_report(result);
    std::cout << report << std::endl;

    // Exit code based on data point count
    if (result.total_data_points >= 112) {
        return 0;
    } else {
        std::cerr << "Warning: Only " << result.total_data_points
                  << " data points collected (target: 112+)\n";
        return 0; // Still success, just warn
    }
}
