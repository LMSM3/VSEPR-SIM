/**
 * bio_report_generator.cpp
 * ------------------------
 * VSEPR-SIM Bio-Organic Report Generator
 * Work Order: WO-BIO-CRG-003
 *
 * Generates reports for floral pigment chemistry, leaf seasonal isolation,
 * volatile release kinetics, UV patterning, and developmental staging.
 *
 * Usage:
 *   bio_report_generator [OPTIONS]
 *
 * Options:
 *   --count N       Number of reports (default: 10)
 *   --seed N        Base RNG seed (default: 42)
 *   --out DIR       Output directory (default: reports)
 *   --domain D      Domain: pigment, volatile, leaf, structure, developmental (default: pigment)
 *   --no-files      Skip individual .md files
 *   --quiet         Suppress progress output
 *   --help          Show usage
 */

#include "core/bio_report_engine.hpp"
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    using namespace vsepr::report::bio;

    BioEngineConfig config;
    config.target_reports = 10;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            std::cout << R"(
VSEPR-SIM Bio-Organic Report Generator
Work Order: WO-BIO-CRG-003

Usage: bio_report_generator [OPTIONS]

Options:
  --count N       Number of reports to generate (default: 10)
  --seed N        Base RNG seed (default: 42)
  --out DIR       Output directory (default: reports)
  --domain D      Domain selector:
                    pigment      Floral pigment palette
                    volatile     Floral volatile emission
                    leaf         Leaf seasonal pigment isolation
                    structure    Leaf microstructure transport
                    developmental Bud-to-senescence staging
  --no-files      Skip writing individual .md files
  --no-csv        Skip writing summary CSV
  --quiet         Suppress progress output
  --interval N    Progress print interval (default: 10)

Domains:
  pigment:       Anthocyanin, chalcone, aurone, flavonoid colour chemistry
  volatile:      Terpenoid, benzenoid, aldehyde scent profiles
  leaf:          Chlorophyll a/b, carotenoid, anthocyanin seasonal transitions
  structure:     Cuticle, mesophyll, trichome transport and extraction
  developmental: Chemical staging from bud through senescence

Output:
  reports/BIO-NNNNNN.md   Individual Markdown reports
  reports/bio_summary.csv Cumulative CSV log
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
        else if (arg == "--domain" && i + 1 < argc) {
            std::string d = argv[++i];
            if (d == "pigment")            config.domain = SystemDomain::FLORAL_PIGMENT;
            else if (d == "volatile")      config.domain = SystemDomain::FLORAL_VOLATILE;
            else if (d == "leaf")          config.domain = SystemDomain::LEAF_SEASONAL;
            else if (d == "structure")     config.domain = SystemDomain::LEAF_STRUCTURE;
            else if (d == "developmental") config.domain = SystemDomain::DEVELOPMENTAL;
            else {
                std::cerr << "Unknown domain: " << d << "\n";
                return 1;
            }
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
        else {
            std::cerr << "Unknown option: " << arg << "\n";
            std::cerr << "Run with --help for usage.\n";
            return 1;
        }
    }

    BioAutonomousEngine engine(config);
    return engine.run();
}
