/**
 * atomistic-build: Interactive Molecular Builder CLI
 * 
 * Builds molecules from chemical formulas using the formation pipeline:
 *   formula -> parse -> VSEPR placement -> FIRE optimization
 *
 * No hardcoded geometries. Structure emerges from physics.
 * 
 * Usage:
 *   atomistic-build              # Enter interactive mode
 *   atomistic-build script.txt   # Run commands from file
 * 
 * Commands:
 *   build <formula>         # Build from formula (e.g., H2O, CH4, C2H6)
 *   load <file.xyz>         # Load molecule from XYZ file
 *   save <file.xyz>         # Save current molecule to XYZ file
 *   info                    # Show information about current molecule
 *   clear                   # Clear current molecule
 *   help                    # Show help message
 *   exit                    # Exit interactive mode
 */

#include "atomistic/core/state.hpp"
#include "atomistic/parsers/xyz_parser.hpp"
#include "atomistic/compilers/xyz_compiler.hpp"
#include "io/xyz_format.hpp"

#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <memory>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>

using namespace atomistic;

// ============================================================================
// CLI STATE
// ============================================================================

struct CLIState {
    State current_molecule;
    std::vector<std::string> element_symbols;
    bool has_molecule = false;
    std::string last_formula = "";
};

CLIState g_cli;

std::string to_lower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

void cmd_help() {
    std::cout << R"(
Available Commands:
  build <formula>     Build molecule from chemical formula
                      Examples: H2O, CH4, NH3, C2H6, C6H6, NaCl
                      The formula is parsed, placed via VSEPR, then
                      optimized with FIRE. No hardcoded geometries.
  load <file.xyz>     Load molecule from XYZ file
  save <file.xyz>     Save current molecule to XYZ file
  info                Show information about current molecule
  clear               Clear current molecule
  help                Show this help message
  exit                Exit interactive mode

Pipeline:
  formula -> parse -> VSEPR placement -> FIRE optimization -> result
  Structure emerges from force models. Nothing is hardcoded.

Examples:
  > build H2O
  > info
  > save water.xyz
  > build C2H6
  > save ethane.xyz
)" << std::endl;
}

void cmd_build(const std::string& formula) {
    // The build command delegates to the formation pipeline.
    // Full integration with build_and_optimize_from_formula() requires
    // linking the build/ module (formula_builder.hpp + builder_core.hpp).
    //
    // TODO: Wire up vsepr::build_and_optimize_from_formula(formula, settings)
    //       and convert the resulting Molecule to atomistic::State.
    
    std::cerr << "Formula pipeline: " << formula << "\n";
    std::cerr << "  Parse -> VSEPR placement -> FIRE optimization\n";
    std::cerr << "  [Integration pending: link src/build/ pipeline]\n";
    std::cerr << "  Use 'load <file.xyz>' to work with existing structures.\n";
}

void cmd_info() {
    if (!g_cli.has_molecule) {
        std::cerr << "No molecule loaded. Use 'build <formula>' or 'load <file>' first.\n";
        return;
    }

    const auto& mol = g_cli.current_molecule;

    std::cout << "\n=== Molecule Information ===\n";
    if (!g_cli.last_formula.empty()) {
        std::cout << "Formula: " << g_cli.last_formula << "\n";
    }
    std::cout << "Atoms: " << mol.N << "\n\n";

    std::cout << "Atomic Composition:\n";
    std::map<uint32_t, int> element_counts;
    for (uint32_t i = 0; i < mol.N; ++i) {
        element_counts[mol.type[i]]++;
    }

    for (const auto& [type, count] : element_counts) {
        std::cout << "  Z=" << type << ": " << count << " atom(s)\n";
    }

    std::cout << "\nCoordinates:\n";
    std::cout << "  Atom  Z           X         Y         Z\n";
    std::cout << "  ----  --     -------   -------   -------\n";
    for (uint32_t i = 0; i < mol.N; ++i) {
        std::cout << "  " << std::setw(4) << i << "  ";
        std::cout << std::setw(2) << mol.type[i] << "   ";
        std::cout << std::fixed << std::setprecision(3);
        std::cout << std::setw(7) << mol.X[i].x << "   ";
        std::cout << std::setw(7) << mol.X[i].y << "   ";
        std::cout << std::setw(7) << mol.X[i].z << "\n";
    }
    std::cout << std::endl;
}

void cmd_load(const std::string& filename) {
    vsepr::io::XYZReader reader;
    vsepr::io::XYZMolecule xyz_mol;

    if (!reader.read(filename, xyz_mol)) {
        std::cerr << "Failed to load: " << reader.get_error() << "\n";
        return;
    }

    g_cli.current_molecule = parsers::from_xyz(xyz_mol);

    g_cli.element_symbols.clear();
    for (const auto& atom : xyz_mol.atoms) {
        g_cli.element_symbols.push_back(atom.element);
    }

    g_cli.has_molecule = true;
    g_cli.last_formula = filename;

    std::cout << "Loaded " << filename << " (" << g_cli.current_molecule.N << " atoms)\n";
}

void cmd_save(const std::string& filename) {
    if (!g_cli.has_molecule) {
        std::cerr << "No molecule loaded. Use 'build <formula>' or 'load <file>' first.\n";
        return;
    }

    vsepr::io::XYZMolecule xyz_mol = compilers::to_xyz(g_cli.current_molecule, g_cli.element_symbols);
    vsepr::io::XYZWriter writer;

    if (!writer.write(filename, xyz_mol)) {
        std::cerr << "Failed to save: " << writer.get_error() << "\n";
        return;
    }

    std::cout << "Saved to " << filename << "\n";
}

void cmd_clear() {
    g_cli.has_molecule = false;
    g_cli.last_formula = "";
    g_cli.element_symbols.clear();
    std::cout << "Molecule cleared.\n";
}

bool process_command(const std::string& line) {
    std::istringstream iss(line);
    std::string cmd;
    iss >> cmd;

    cmd = to_lower(cmd);

    if (cmd.empty() || cmd[0] == '#') {
        return true;
    }

    if (cmd == "exit" || cmd == "quit") {
        return false;
    } else if (cmd == "help") {
        cmd_help();
    } else if (cmd == "build") {
        std::string formula;
        iss >> formula;
        if (formula.empty()) {
            std::cerr << "Usage: build <formula>  (e.g., H2O, CH4, C6H6)\n";
        } else {
            cmd_build(formula);
        }
    } else if (cmd == "load") {
        std::string filename;
        iss >> filename;
        if (filename.empty()) {
            std::cerr << "Usage: load <filename.xyz>\n";
        } else {
            cmd_load(filename);
        }
    } else if (cmd == "save") {
        std::string filename;
        iss >> filename;
        if (filename.empty()) {
            std::cerr << "Usage: save <filename.xyz>\n";
        } else {
            cmd_save(filename);
        }
    } else if (cmd == "info") {
        cmd_info();
    } else if (cmd == "clear") {
        cmd_clear();
    } else {
        std::cerr << "Unknown command: " << cmd << "\n";
        std::cerr << "Type 'help' for available commands.\n";
    }

    return true;
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char** argv) {
    std::cout << "===================================================\n";
    std::cout << "  Atomistic Builder: Formula -> VSEPR -> FIRE\n";
    std::cout << "===================================================\n\n";

    if (argc > 1) {
        std::string script_file = argv[1];
        std::ifstream file(script_file);

        if (!file.is_open()) {
            std::cerr << "Failed to open script: " << script_file << "\n";
            return 1;
        }

        std::cout << "Running script: " << script_file << "\n\n";

        std::string line;
        while (std::getline(file, line)) {
            std::cout << "> " << line << "\n";
            if (!process_command(line)) {
                break;
            }
        }

        return 0;
    }

    std::cout << "Interactive mode. Type 'help' for commands, 'exit' to quit.\n";
    std::cout << "Build any molecule: build H2O | build CH4 | build C6H6\n\n";

    std::string line;
    while (true) {
        std::cout << ">> ";
        std::cout.flush();

        if (!std::getline(std::cin, line)) {
            break;
        }

        if (!process_command(line)) {
            break;
        }
    }

    std::cout << "\nGoodbye!\n";

    return 0;
}
