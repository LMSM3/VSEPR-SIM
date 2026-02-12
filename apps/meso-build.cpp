/**
 * meso-build: Interactive Molecular Builder CLI
 * 
 * Interactive command-line interface for building and saving molecules.
 * 
 * Usage:
 *   meso-build              # Enter interactive mode
 *   meso-build script.txt   # Run commands from file
 * 
 * Commands:
 *   build <molecule>        # Build molecule (e.g., cisplatin, water, methane)
 *   load <file.xyz>         # Load molecule from XYZ file
 *   save <file.xyz>         # Save current molecule to XYZ file
 *   list                    # List available pre-built molecules
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
// MOLECULAR BUILDER - PREDEFINED STRUCTURES
// ============================================================================

struct MoleculeData {
    State state;
    std::vector<std::string> element_symbols;
};

struct MoleculeBuilder {
    /**
     * Build cisplatin: cis-[Pt(NH3)2Cl2]
     * 
     * Geometry: Square planar Pt(II) complex
     * Clinical anticancer drug
     */
    static MoleculeData build_cisplatin() {
        MoleculeData mol;
        mol.state.N = 11;  // 1 Pt, 2 N, 2 Cl, 6 H

        mol.state.X.resize(mol.state.N);
        mol.state.V.resize(mol.state.N, {0.0, 0.0, 0.0});
        mol.state.Q.resize(mol.state.N, 0.0);
        mol.state.M.resize(mol.state.N);
        mol.state.type.resize(mol.state.N);
        mol.state.F.resize(mol.state.N, {0.0, 0.0, 0.0});

        mol.element_symbols = {"Pt", "N", "N", "Cl", "Cl", "H", "H", "H", "H", "H", "H"};

        // Atom 0: Pt (center)
        mol.state.type[0] = 78;
        mol.state.X[0] = {0.0, 0.0, 0.0};
        mol.state.M[0] = 195.084;

        // Atoms 1-2: N (NH3 ligands, cis configuration)
        mol.state.type[1] = 7;
        mol.state.X[1] = {2.0, 0.0, 0.0};  // +x
        mol.state.M[1] = 14.007;

        mol.state.type[2] = 7;
        mol.state.X[2] = {0.0, 2.0, 0.0};  // +y (cis to N1)
        mol.state.M[2] = 14.007;

        // Atoms 3-4: Cl (chloride ligands, cis configuration)
        mol.state.type[3] = 17;
        mol.state.X[3] = {-2.0, 0.0, 0.0};  // -x
        mol.state.M[3] = 35.45;

        mol.state.type[4] = 17;
        mol.state.X[4] = {0.0, -2.0, 0.0};  // -y (cis to Cl1)
        mol.state.M[4] = 35.45;

        // Atoms 5-7: H on N1 (NH3)
        mol.state.type[5] = 1;
        mol.state.X[5] = {2.8, 0.5, 0.5};
        mol.state.M[5] = 1.008;

        mol.state.type[6] = 1;
        mol.state.X[6] = {2.8, -0.5, -0.5};
        mol.state.M[6] = 1.008;

        mol.state.type[7] = 1;
        mol.state.X[7] = {2.8, 0.5, -0.5};
        mol.state.M[7] = 1.008;

        // Atoms 8-10: H on N2 (NH3)
        mol.state.type[8] = 1;
        mol.state.X[8] = {0.5, 2.8, 0.5};
        mol.state.M[8] = 1.008;

        mol.state.type[9] = 1;
        mol.state.X[9] = {-0.5, 2.8, -0.5};
        mol.state.M[9] = 1.008;

        mol.state.type[10] = 1;
        mol.state.X[10] = {-0.5, 2.8, 0.5};
        mol.state.M[10] = 1.008;

        return mol;
    }

    /**
     * Build water molecule: H2O
     */
    static MoleculeData build_water() {
        MoleculeData mol;
        mol.state.N = 3;

        mol.state.type = {8, 1, 1};  // O, H, H
        mol.state.M = {15.999, 1.008, 1.008};
        mol.state.Q.resize(3, 0.0);
        mol.element_symbols = {"O", "H", "H"};

        // Bent geometry, ~104.5° angle
        mol.state.X.resize(3);
        mol.state.X[0] = {0.0, 0.0, 0.0};  // O
        mol.state.X[1] = {0.96, 0.0, 0.0};  // H1
        mol.state.X[2] = {-0.24, 0.93, 0.0};  // H2 (bent)

        mol.state.V.resize(3, {0.0, 0.0, 0.0});
        mol.state.F.resize(3, {0.0, 0.0, 0.0});

        return mol;
    }

    /**
     * Build methane: CH4
     */
    static MoleculeData build_methane() {
        MoleculeData mol;
        mol.state.N = 5;

        mol.state.type = {6, 1, 1, 1, 1};  // C, H, H, H, H
        mol.state.M = {12.011, 1.008, 1.008, 1.008, 1.008};
        mol.state.Q.resize(5, 0.0);
        mol.element_symbols = {"C", "H", "H", "H", "H"};

        // Tetrahedral geometry
        mol.state.X.resize(5);
        mol.state.X[0] = {0.0, 0.0, 0.0};  // C
        mol.state.X[1] = {1.09, 0.0, 0.0};
        mol.state.X[2] = {-0.36, 1.03, 0.0};
        mol.state.X[3] = {-0.36, -0.51, 0.89};
        mol.state.X[4] = {-0.36, -0.51, -0.89};

        mol.state.V.resize(5, {0.0, 0.0, 0.0});
        mol.state.F.resize(5, {0.0, 0.0, 0.0});

        return mol;
    }

    /**
     * Build ammonia: NH3
     */
    static MoleculeData build_ammonia() {
        MoleculeData mol;
        mol.state.N = 4;

        mol.state.type = {7, 1, 1, 1};  // N, H, H, H
        mol.state.M = {14.007, 1.008, 1.008, 1.008};
        mol.state.Q.resize(4, 0.0);
        mol.element_symbols = {"N", "H", "H", "H"};

        // Trigonal pyramidal
        mol.state.X.resize(4);
        mol.state.X[0] = {0.0, 0.0, 0.0};  // N
        mol.state.X[1] = {1.01, 0.0, 0.0};
        mol.state.X[2] = {-0.34, 0.95, 0.0};
        mol.state.X[3] = {-0.34, -0.47, 0.82};

        mol.state.V.resize(4, {0.0, 0.0, 0.0});
        mol.state.F.resize(4, {0.0, 0.0, 0.0});

        return mol;
    }

    /**
     * Build ethylene: C2H4
     */
    static MoleculeData build_ethylene() {
        MoleculeData mol;
        mol.state.N = 6;

        mol.state.type = {6, 6, 1, 1, 1, 1};  // C, C, H, H, H, H
        mol.state.M = {12.011, 12.011, 1.008, 1.008, 1.008, 1.008};
        mol.state.Q.resize(6, 0.0);
        mol.element_symbols = {"C", "C", "H", "H", "H", "H"};

        // Planar double bond
        mol.state.X.resize(6);
        mol.state.X[0] = {0.0, 0.0, 0.0};    // C1
        mol.state.X[1] = {1.34, 0.0, 0.0};    // C2 (C=C bond)
        mol.state.X[2] = {-0.59, 0.93, 0.0};  // H on C1
        mol.state.X[3] = {-0.59, -0.93, 0.0}; // H on C1
        mol.state.X[4] = {1.93, 0.93, 0.0};   // H on C2
        mol.state.X[5] = {1.93, -0.93, 0.0};  // H on C2

        mol.state.V.resize(6, {0.0, 0.0, 0.0});
        mol.state.F.resize(6, {0.0, 0.0, 0.0});

        return mol;
    }
};

// ============================================================================
// MOLECULE DATABASE
// ============================================================================

std::map<std::string, std::function<MoleculeData()>> g_molecule_library = {
    {"cisplatin", MoleculeBuilder::build_cisplatin},
    {"water", MoleculeBuilder::build_water},
    {"h2o", MoleculeBuilder::build_water},
    {"methane", MoleculeBuilder::build_methane},
    {"ch4", MoleculeBuilder::build_methane},
    {"ammonia", MoleculeBuilder::build_ammonia},
    {"nh3", MoleculeBuilder::build_ammonia},
    {"ethylene", MoleculeBuilder::build_ethylene},
    {"c2h4", MoleculeBuilder::build_ethylene}
};

// ============================================================================
// CLI COMMAND PROCESSOR
// ============================================================================

struct CLIState {
    State current_molecule;
    std::vector<std::string> element_symbols;
    bool has_molecule = false;
    std::string last_molecule_name = "";
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
  build <molecule>    Build predefined molecule (e.g., cisplatin, water, methane)
  load <file.xyz>     Load molecule from XYZ file
  save <file.xyz>     Save current molecule to XYZ file
  list                List all available predefined molecules
  info                Show information about current molecule
  clear               Clear current molecule
  help                Show this help message
  exit                Exit interactive mode

Examples:
  > build cisplatin
  > info
  > save my_molecule.xyz
  > load test.xyz
)" << std::endl;
}

void cmd_list() {
    std::cout << "\nAvailable Molecules:\n";
    std::cout << "  - cisplatin      : cis-[Pt(NH3)2Cl2] anticancer drug (11 atoms)\n";
    std::cout << "  - water / h2o    : H2O (3 atoms)\n";
    std::cout << "  - methane / ch4  : CH4 (5 atoms)\n";
    std::cout << "  - ammonia / nh3  : NH3 (4 atoms)\n";
    std::cout << "  - ethylene / c2h4: C2H4 double bond (6 atoms)\n";
    std::cout << std::endl;
}

void cmd_build(const std::string& molecule_name) {
    std::string name = to_lower(molecule_name);

    auto it = g_molecule_library.find(name);
    if (it == g_molecule_library.end()) {
        std::cerr << "Unknown molecule: " << molecule_name << "\n";
        std::cerr << "Use 'list' to see available molecules.\n";
        return;
    }

    std::cout << "Building " << molecule_name << "...\n";
    MoleculeData mol_data = it->second();
    g_cli.current_molecule = mol_data.state;
    g_cli.element_symbols = mol_data.element_symbols;
    g_cli.has_molecule = true;
    g_cli.last_molecule_name = molecule_name;

    std::cout << "Built " << molecule_name << " (" << g_cli.current_molecule.N << " atoms)\n";
    std::cout << "Use 'info' for details or 'save <file>' to export.\n";
}

void cmd_info() {
    if (!g_cli.has_molecule) {
        std::cerr << "No molecule loaded. Use 'build' or 'load' first.\n";
        return;
    }

    const auto& mol = g_cli.current_molecule;

    std::cout << "\n=== Molecule Information ===\n";
    if (!g_cli.last_molecule_name.empty()) {
        std::cout << "Name: " << g_cli.last_molecule_name << "\n";
    }
    std::cout << "Atoms: " << mol.N << "\n\n";

    std::cout << "Atomic Composition:\n";
    std::map<uint32_t, int> element_counts;
    for (uint32_t i = 0; i < mol.N; ++i) {
        element_counts[mol.type[i]]++;
    }

    for (const auto& [type, count] : element_counts) {
        std::cout << "  Type=" << type << ": " << count << " atom(s)\n";
    }

    std::cout << "\nCoordinates:\n";
    std::cout << "  Atom  Type        X         Y         Z\n";
    std::cout << "  ----  ----   -------   -------   -------\n";
    for (uint32_t i = 0; i < mol.N; ++i) {
        std::cout << "  " << std::setw(4) << i << "  ";
        std::cout << std::setw(4) << mol.type[i] << "   ";
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

    // Build element symbols vector
    g_cli.element_symbols.clear();
    for (const auto& atom : xyz_mol.atoms) {
        g_cli.element_symbols.push_back(atom.element);
    }

    g_cli.has_molecule = true;
    g_cli.last_molecule_name = filename;

    std::cout << "Loaded " << filename << " (" << g_cli.current_molecule.N << " atoms)\n";
}

void cmd_save(const std::string& filename) {
    if (!g_cli.has_molecule) {
        std::cerr << "No molecule loaded. Use 'build' or 'load' first.\n";
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
    g_cli.last_molecule_name = "";
    g_cli.element_symbols.clear();
    std::cout << "Molecule cleared.\n";
}

bool process_command(const std::string& line) {
    std::istringstream iss(line);
    std::string cmd;
    iss >> cmd;

    cmd = to_lower(cmd);

    if (cmd.empty() || cmd[0] == '#') {
        return true;  // Comment or empty line
    }

    if (cmd == "exit" || cmd == "quit") {
        return false;
    } else if (cmd == "help") {
        cmd_help();
    } else if (cmd == "list") {
        cmd_list();
    } else if (cmd == "build") {
        std::string molecule_name;
        iss >> molecule_name;
        if (molecule_name.empty()) {
            std::cerr << "Usage: build <molecule>\n";
        } else {
            cmd_build(molecule_name);
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
    std::cout << "═══════════════════════════════════════════════════\n";
    std::cout << "  MESO-BUILD: Interactive Molecular Builder\n";
    std::cout << "═══════════════════════════════════════════════════\n\n";

    if (argc > 1) {
        // Script mode: read commands from file
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

    // Interactive mode
    std::cout << "Interactive mode. Type 'help' for commands, 'exit' to quit.\n";
    std::cout << "Suggested: build cisplatin → info → save cisplatin.xyz\n\n";

    std::string line;
    while (true) {
        std::cout << "⚛ ";  // Atomic symbol for active session
        std::cout.flush();

        if (!std::getline(std::cin, line)) {
            break;  // EOF
        }

        if (!process_command(line)) {
            break;  // Exit command
        }
    }

    std::cout << "\nGoodbye!\n";

    return 0;
}
