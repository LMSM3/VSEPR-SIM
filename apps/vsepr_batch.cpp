/**
 * @file vsepr_batch.cpp
 * @brief Batch simulation runner using DSL/JSON specifications
 * 
 * This tool processes simulation specifications (DSL or JSON) and runs
 * molecule_builder for each component in the specification.
 * 
 * Usage:
 *   ./vsepr_batch "<spec>" --out <output_dir> [--total N]
 *   ./vsepr_batch --file <spec.json> --out <output_dir> [--total N]
 * 
 * Examples:
 *   ./vsepr_batch "CH12CaO9" --out runs/test1/
 *   ./vsepr_batch "H2O, CO2 -per{50,50}" --out runs/mixture1/ --total 200
 *   ./vsepr_batch --file specs/ikaite_mixture.json --out runs/ikaite/
 */

#include "vsepr/spec_parser.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <ctime>
#include <iomanip>

namespace fs = std::filesystem;

struct BatchConfig {
    std::string spec_string;
    std::string spec_file;
    std::string output_dir;
    int total_molecules = 100;
    bool verbose = false;
    bool dry_run = false;
};

void print_usage(const char* prog_name) {
    std::cout << "VSEPR Batch Runner\n";
    std::cout << "==================\n\n";
    std::cout << "Usage:\n";
    std::cout << "  " << prog_name << " \"<spec>\" --out <dir> [options]\n";
    std::cout << "  " << prog_name << " --file <spec.json> --out <dir> [options]\n\n";
    std::cout << "Arguments:\n";
    std::cout << "  <spec>              DSL specification string\n";
    std::cout << "  --file <path>       Read specification from JSON file\n";
    std::cout << "  --out <dir>         Output directory for results\n\n";
    std::cout << "Options:\n";
    std::cout << "  --total N           Total molecules for mixture (default: 100)\n";
    std::cout << "  --verbose, -v       Verbose output\n";
    std::cout << "  --dry-run           Show plan without executing\n";
    std::cout << "  --help, -h          Show this help\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << prog_name << " \"CH12CaO9\" --out runs/ikaite/\n";
    std::cout << "  " << prog_name << " \"H2O, CO2 -per{80,20}\" --out runs/mix1/ --total 500\n";
    std::cout << "  " << prog_name << " \"H2O --T=273, H2O --T=300\" --out runs/temp_study/\n";
    std::cout << "  " << prog_name << " --file specs/complex.json --out runs/batch1/\n\n";
    std::cout << "DSL Syntax:\n";
    std::cout << "  Formula: H2O, CO2, CH4, etc.\n";
    std::cout << "  Temperature: --T=<Kelvin>\n";
    std::cout << "  Count: -n=<integer>\n";
    std::cout << "  Position: -pos{random|fixed:x,y,z|seeded:seed:bx,by,bz}\n";
    std::cout << "  Percentages: -per{p1,p2,...} (at end)\n";
    std::cout << "  Separator: , (comma)\n\n";
}

BatchConfig parse_arguments(int argc, char* argv[]) {
    BatchConfig config;
    
    if (argc < 2) {
        print_usage(argv[0]);
        exit(1);
    }
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            exit(0);
        } else if (arg == "--verbose" || arg == "-v") {
            config.verbose = true;
        } else if (arg == "--dry-run") {
            config.dry_run = true;
        } else if (arg == "--file") {
            if (i + 1 < argc) {
                config.spec_file = argv[++i];
            } else {
                std::cerr << "Error: --file requires an argument\n";
                exit(1);
            }
        } else if (arg == "--out") {
            if (i + 1 < argc) {
                config.output_dir = argv[++i];
            } else {
                std::cerr << "Error: --out requires an argument\n";
                exit(1);
            }
        } else if (arg == "--total") {
            if (i + 1 < argc) {
                config.total_molecules = std::stoi(argv[++i]);
            } else {
                std::cerr << "Error: --total requires an argument\n";
                exit(1);
            }
        } else if (arg[0] != '-') {
            // Positional argument - treat as spec string
            if (config.spec_string.empty()) {
                config.spec_string = arg;
            }
        }
    }
    
    return config;
}

std::string read_file(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("Failed to open file: " + path);
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::string get_timestamp() {
    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d_%H-%M-%S");
    return oss.str();
}

int run_molecule_builder(
    const std::string& formula,
    const std::string& output_path,
    const vsepr::MoleculeSpec& spec,
    bool verbose
) {
    std::ostringstream cmd;
    
    #ifdef _WIN32
    cmd << ".\\build\\bin\\molecule_builder.exe ";
    #else
    cmd << "./build/bin/molecule_builder ";
    #endif
    
    cmd << "\"" << formula << "\" ";
    cmd << "--xyz \"" << output_path << "\"";
    
    // Add temperature if specified
    if (spec.temperature.has_value()) {
        // Note: molecule_builder doesn't support --T yet, but we prepare for it
        // For now, we'll note it in the filename or metadata
    }
    
    if (verbose) {
        std::cout << "  Running: " << cmd.str() << "\n";
    }
    
    int result = std::system(cmd.str().c_str());
    return result;
}

void run_batch(const BatchConfig& config) {
    // Parse specification
    vsepr::SimulationSpec spec;
    
    if (!config.spec_file.empty()) {
        std::string content = read_file(config.spec_file);
        // Try JSON first, then DSL
        try {
            spec = vsepr::from_json(content);
        } catch (...) {
            // JSON parsing not implemented yet, try DSL
            spec = vsepr::parse_dsl(content);
        }
    } else if (!config.spec_string.empty()) {
        spec = vsepr::parse_dsl(config.spec_string);
    } else {
        throw std::runtime_error("No specification provided");
    }
    
    // Print parsed specification
    if (config.verbose) {
        std::cout << "\n" << vsepr::to_string(spec) << "\n";
    }
    
    // Expand to run plan
    auto run_plan = vsepr::expand_to_run_plan(spec, config.total_molecules);
    
    std::cout << "\nRun Plan:\n";
    std::cout << "=========\n";
    int total_runs = 0;
    for (size_t i = 0; i < run_plan.size(); ++i) {
        const auto& item = run_plan[i];
        std::cout << "  [" << i << "] " << item.formula 
                  << " × " << item.count;
        
        if (item.temperature.has_value()) {
            std::cout << " (T=" << item.temperature.value() << "K)";
        }
        
        if (item.position.has_value()) {
            std::cout << " (positioned)";
        }
        
        std::cout << "\n";
        total_runs += item.count;
    }
    std::cout << "  Total: " << total_runs << " molecules\n\n";
    
    if (config.dry_run) {
        std::cout << "Dry run - no execution.\n";
        
        // Print JSON output
        std::cout << "\nJSON Specification:\n";
        std::cout << vsepr::to_json(spec) << "\n";
        return;
    }
    
    // Create output directory
    if (!config.output_dir.empty()) {
        fs::create_directories(config.output_dir);
    }
    
    // Save specification
    std::string spec_path = config.output_dir + "/specification.json";
    {
        std::ofstream spec_file(spec_path);
        spec_file << vsepr::to_json(spec) << "\n";
    }
    std::cout << "Saved specification to: " << spec_path << "\n\n";
    
    // Execute each component
    std::cout << "Executing batch...\n";
    std::cout << "==================\n\n";
    
    int success_count = 0;
    int failure_count = 0;
    
    for (size_t i = 0; i < run_plan.size(); ++i) {
        const auto& item = run_plan[i];
        
        std::cout << "[" << (i + 1) << "/" << run_plan.size() << "] "
                  << item.formula << " (×" << item.count << ")...\n";
        
        for (int copy = 0; copy < item.count; ++copy) {
            // Generate output filename
            std::ostringstream output_name;
            output_name << config.output_dir << "/" 
                       << item.formula;
            
            if (item.temperature.has_value()) {
                output_name << "_T" << static_cast<int>(item.temperature.value());
            }
            
            if (item.count > 1) {
                output_name << "_" << std::setfill('0') << std::setw(4) << copy;
            }
            
            output_name << ".xyz";
            
            // Find the corresponding MoleculeSpec for full info
            const vsepr::MoleculeSpec* mol_spec = nullptr;
            for (const auto& comp : spec.mixture.components) {
                if (comp.formula == item.formula) {
                    mol_spec = &comp;
                    break;
                }
            }
            
            // Run molecule builder
            int result = run_molecule_builder(
                item.formula,
                output_name.str(),
                mol_spec ? *mol_spec : vsepr::MoleculeSpec(item.formula),
                config.verbose
            );
            
            if (result == 0) {
                success_count++;
                if (!config.verbose) {
                    std::cout << "  ✓ " << output_name.str() << "\n";
                }
            } else {
                failure_count++;
                std::cout << "  ✗ Failed: " << output_name.str() << "\n";
            }
        }
        
        std::cout << "\n";
    }
    
    // Summary
    std::cout << "Batch Complete\n";
    std::cout << "==============\n";
    std::cout << "  Success: " << success_count << "\n";
    std::cout << "  Failure: " << failure_count << "\n";
    std::cout << "  Output: " << fs::absolute(config.output_dir).string() << "\n";
}

int main(int argc, char* argv[]) {
    try {
        BatchConfig config = parse_arguments(argc, argv);
        
        std::cout << "VSEPR Batch Runner\n";
        std::cout << "==================\n\n";
        
        run_batch(config);
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
