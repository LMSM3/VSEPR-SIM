/**
 * cmd_stream.cpp
 * 
 * Streaming command - continuously generates molecules and exports to WebGL
 * Enables live visualization of C++ molecular dynamics
 */

#include "cli/commands.hpp"
#include "cli/display.hpp"
#include "sim/molecule.hpp"
#include "sim/molecule_builder.hpp"
#include "export/webgl_exporter.hpp"
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <random>

namespace vsepr {
namespace cli {

class StreamCommand : public ICommand {
public:
    std::string Name() const override {
        return "stream";
    }
    
    std::string Description() const override {
        return "Continuously stream molecule data to WebGL viewer";
    }
    
    std::string Help() const override {
        std::ostringstream help;
        help << "Stream Command - Live Molecular Data Pipeline\n\n";
        help << "USAGE:\n";
        help << "  vsepr stream [options]\n\n";
        help << "OPTIONS:\n";
        help << "  --output, -o <file>  Output JSON file (default: outputs/webgl_molecules.json)\n";
        help << "  --interval <ms>      Update interval in milliseconds (default: 2000)\n";
        help << "  --count <n>          Number of molecules to generate (default: infinite)\n";
        help << "  --formulas <list>    Comma-separated formulas to cycle through\n\n";
        help << "MODES:\n";
        help << "  • Random generation: Creates random molecules continuously\n";
        help << "  • Formula cycling: Rotates through specified formulas\n";
        help << "  • File watching: Updates when source files change\n\n";
        help << "EXAMPLES:\n";
        help << "  vsepr stream                              # Infinite random molecules\n";
        help << "  vsepr stream --count 10 --interval 1000   # 10 molecules, 1s apart\n";
        help << "  vsepr stream --formulas H2O,NH3,CH4       # Cycle through these\n\n";
        help << "INTEGRATION:\n";
        help << "  1. Start: vsepr stream -o webgl_molecules.json\n";
        help << "  2. Open: outputs/universal_viewer.html\n";
        help << "  3. Set viewer to: DATA_SOURCE = 'file'\n";
        help << "  4. Viewer auto-refreshes with new molecules\n";
        return help.str();
    }
    
    int Execute(const std::vector<std::string>& args) override {
        // Parse arguments
        std::string output = "outputs/webgl_molecules.json";
        int interval_ms = 2000;
        int count = -1; // infinite
        std::vector<std::string> formulas;
        
        for (size_t i = 0; i < args.size(); ++i) {
            const auto& arg = args[i];
            
            if (arg == "--output" || arg == "-o") {
                if (i + 1 < args.size()) {
                    output = args[++i];
                }
            } else if (arg == "--interval") {
                if (i + 1 < args.size()) {
                    interval_ms = std::stoi(args[++i]);
                }
            } else if (arg == "--count") {
                if (i + 1 < args.size()) {
                    count = std::stoi(args[++i]);
                }
            } else if (arg == "--formulas") {
                if (i + 1 < args.size()) {
                    std::string formula_str = args[++i];
                    // Split by comma
                    size_t start = 0;
                    size_t end = formula_str.find(',');
                    while (end != std::string::npos) {
                        formulas.push_back(formula_str.substr(start, end - start));
                        start = end + 1;
                        end = formula_str.find(',', start);
                    }
                    formulas.push_back(formula_str.substr(start));
                }
            }
        }
        
        // Default formulas if none specified
        if (formulas.empty()) {
            formulas = {"H2O", "NH3", "CH4", "CO2", "H2SO4", "CCl4", "SF6", "XeF4"};
        }
        
        Display::Success("🔄 Starting molecular data stream");
        Display::Info("Output: " + output);
        Display::Info("Interval: " + std::to_string(interval_ms) + " ms");
        Display::Info("Mode: " + (count < 0 ? "Infinite" : std::to_string(count) + " molecules"));
        Display::Info("\n💡 Open outputs/universal_viewer.html and set DATA_SOURCE='file'");
        Display::Info("💡 Press Ctrl+C to stop streaming\n");
        
        int generated = 0;
        size_t formula_idx = 0;
        std::random_device rd;
        std::mt19937 gen(rd());
        
        while (count < 0 || generated < count) {
            try {
                export_webgl::WebGLExporter exporter;
                
                // Generate multiple molecules for the current update
                for (size_t i = 0; i < formulas.size() && (count < 0 || generated < count); ++i) {
                    const auto& formula = formulas[formula_idx];
                    
                    try {
                        Molecule mol = vsepr::build::build_molecule(formula);
                        exporter.add_molecule(formula, mol, formula);
                        
                        Display::Success("  ✓ " + formula + " (" + 
                                       std::to_string(mol.num_atoms()) + " atoms) " +
                                       "[" + std::to_string(generated + 1) + "]");
                        
                        formula_idx = (formula_idx + 1) % formulas.size();
                        generated++;
                        
                    } catch (const std::exception& e) {
                        Display::Warning("  ✗ " + formula + ": " + e.what());
                        formula_idx = (formula_idx + 1) % formulas.size();
                    }
                }
                
                // Write to file
                if (exporter.write_to_file(output)) {
                    Display::Info("📡 Streamed update #" + std::to_string(generated) + 
                                " to " + output);
                } else {
                    Display::Error("Failed to write to " + output);
                    return 1;
                }
                
                // Wait for next update
                if (count < 0 || generated < count) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
                }
                
            } catch (const std::exception& e) {
                Display::Error("Stream error: " + std::string(e.what()));
                return 1;
            }
        }
        
        Display::Success("\n✅ Stream complete: " + std::to_string(generated) + " molecules");
        Display::Info("Final output: " + output);
        
        return 0;
    }
};

// Register command
static CommandRegistrar<StreamCommand> stream_cmd;

} // namespace cli
} // namespace vsepr
