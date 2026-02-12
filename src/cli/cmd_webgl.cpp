/**
 * cmd_webgl.cpp
 * 
 * CLI command to export molecules for WebGL visualization
 * All dynamics computed by C++ engine - this just exports results
 */

#include "cli/commands.hpp"
#include "cli/display.hpp"
#include "sim/molecule.hpp"
#include "sim/molecule_builder.hpp"
#include "export/webgl_exporter.hpp"
#include "export/xyz_reader.hpp"
#include <iostream>
#include <fstream>

namespace vsepr {
namespace cli {

class WebGLCommand : public ICommand {
public:
    std::string Name() const override {
        return "webgl";
    }
    
    std::string Description() const override {
        return "Export molecule(s) to JSON format for WebGL viewer";
    }
    
    std::string Help() const override {
        std::ostringstream help;
        help << "WebGL Export Command\n\n";
        help << "USAGE:\n";
        help << "  vsepr webgl <input> [options]\n\n";
        help << "INPUT:\n";
        help << "  <input>              Single XYZ file or formula (e.g., H2O, CCl4)\n";
        help << "  --batch <file.txt>   Export multiple molecules from list\n\n";
        help << "OPTIONS:\n";
        help << "  --output, -o <file>  Output JSON file (default: webgl_molecules.json)\n";
        help << "  --name <name>        Human-readable name for molecule\n\n";
        help << "FEATURES:\n";
        help << "  • Exports molecular structure in JSON format\n";
        help << "  • Compatible with outputs/universal_viewer.html\n";
        help << "  • All dynamics computed by C++ engine\n";
        help << "  • WebGL handles rendering only\n\n";
        help << "EXAMPLES:\n";
        help << "  vsepr webgl H2O --output molecules.json --name \"Water\"\n";
        help << "  vsepr webgl water.xyz -o viewer_data.json\n";
        help << "  vsepr webgl --batch molecules.txt -o all_molecules.json\n\n";
        help << "BATCH FILE FORMAT:\n";
        help << "  H2O Water\n";
        help << "  NH3 Ammonia\n";
        help << "  CCl4 Carbon_Tetrachloride\n";
        return help.str();
    }
    
    int Execute(const std::vector<std::string>& args) override {
        if (args.empty()) {
            Display::Error("No input specified. Use --help for usage.");
            return 1;
        }
        
        // Parse arguments
        std::string input;
        std::string output = "outputs/webgl_molecules.json";
        std::string name;
        std::string batch_file;
        bool is_batch = false;
        
        for (size_t i = 0; i < args.size(); ++i) {
            const auto& arg = args[i];
            
            if (arg == "--output" || arg == "-o") {
                if (i + 1 < args.size()) {
                    output = args[++i];
                }
            } else if (arg == "--name") {
                if (i + 1 < args.size()) {
                    name = args[++i];
                }
            } else if (arg == "--batch") {
                if (i + 1 < args.size()) {
                    batch_file = args[++i];
                    is_batch = true;
                }
            } else if (arg[0] != '-') {
                input = arg;
            }
        }
        
        if (is_batch) {
            return export_batch(batch_file, output);
        } else if (!input.empty()) {
            return export_single(input, output, name);
        } else {
            Display::Error("No valid input specified");
            return 1;
        }
    }
    
private:
    int export_single(const std::string& input, 
                     const std::string& output,
                     const std::string& name) {
        Display::Info("Exporting molecule: " + input);
        
        // Try to load from XYZ file first
        Molecule mol;
        bool loaded = false;
        
        if (input.find(".xyz") != std::string::npos) {
            try {
                mol = vsepr::export_xyz::read_xyz(input);
                loaded = true;
                Display::Success("Loaded XYZ file: " + input);
            } catch (const std::exception& e) {
                Display::Warning("Could not load XYZ: " + std::string(e.what()));
            }
        }
        
        // Otherwise try building from formula
        if (!loaded) {
            try {
                mol = vsepr::build::build_molecule(input);
                loaded = true;
                Display::Success("Built molecule from formula: " + input);
            } catch (const std::exception& e) {
                Display::Error("Failed to build molecule: " + std::string(e.what()));
                return 1;
            }
        }
        
        // Export to JSON
        std::string molecule_name = name.empty() ? input : name;
        bool success = vsepr::export_webgl::write_molecule_json(mol, output, molecule_name);
        
        if (success) {
            Display::Success("Exported to: " + output);
            Display::Info("Atoms: " + std::to_string(mol.num_atoms()));
            Display::Info("Open outputs/universal_viewer.html to view");
            return 0;
        } else {
            Display::Error("Failed to write JSON file");
            return 1;
        }
    }
    
    int export_batch(const std::string& batch_file, const std::string& output) {
        Display::Info("Batch export from: " + batch_file);
        
        std::ifstream infile(batch_file);
        if (!infile) {
            Display::Error("Could not open batch file: " + batch_file);
            return 1;
        }
        
        export_webgl::WebGLExporter exporter;
        std::string line;
        int count = 0;
        
        while (std::getline(infile, line)) {
            // Skip empty lines and comments
            if (line.empty() || line[0] == '#') continue;
            
            // Parse: formula name
            std::istringstream iss(line);
            std::string formula, mol_name;
            iss >> formula;
            std::getline(iss, mol_name);
            
            // Trim whitespace from name
            size_t start = mol_name.find_first_not_of(" \t");
            if (start != std::string::npos) {
                mol_name = mol_name.substr(start);
            }
            
            try {
                // Try XYZ file first
                Molecule mol;
                if (formula.find(".xyz") != std::string::npos) {
                    mol = vsepr::export_xyz::read_xyz(formula);
                } else {
                    mol = vsepr::build::build_molecule(formula);
                }
                
                exporter.add_molecule(formula, mol, mol_name.empty() ? formula : mol_name);
                Display::Success("  ✓ " + formula + " (" + std::to_string(mol.num_atoms()) + " atoms)");
                count++;
            } catch (const std::exception& e) {
                Display::Warning("  ✗ " + formula + ": " + e.what());
            }
        }
        
        if (count == 0) {
            Display::Error("No molecules successfully exported");
            return 1;
        }
        
        // Write combined JSON
        if (exporter.write_to_file(output)) {
            Display::Success("\nExported " + std::to_string(count) + " molecules to: " + output);
            Display::Info("Open outputs/universal_viewer.html to view");
            return 0;
        } else {
            Display::Error("Failed to write output file");
            return 1;
        }
    }
};

// Register command
static CommandRegistrar<WebGLCommand> webgl_cmd;

} // namespace cli
} // namespace vsepr
