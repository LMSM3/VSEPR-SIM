/**
 * atomistic-build: Interactive Molecular Builder CLI
 * 
 * Builds molecules from chemical formulas using the formation pipeline:
 *   formula -> parse -> VSEPR placement -> FIRE optimization -> validate
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
 *   validate                # Run validation gates on current molecule
 *   identity                # Show canonical identity and fingerprints
 *   clear                   # Clear current molecule
 *   help                    # Show help message
 *   exit                    # Exit interactive mode
 */

#include "atomistic/core/state.hpp"
#include "atomistic/parsers/xyz_parser.hpp"
#include "atomistic/compilers/xyz_compiler.hpp"
#include "io/xyz_format.hpp"
#include "build/builder_core.hpp"
#include "identity/canonical_identity.hpp"
#include "validation/validation_gates.hpp"

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
#include <functional>

using namespace atomistic;

// ============================================================================
// GLOBALS
// ============================================================================

static vsepr::PeriodicTable g_ptable;
static bool g_ptable_loaded = false;

bool ensure_periodic_table() {
    if (g_ptable_loaded) return true;
    try {
        g_ptable.load_separated("data/elements.physics.json", "", "");
        g_ptable_loaded = true;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to load periodic table: " << e.what() << "\n";
        std::cerr << "Ensure data/elements.physics.json exists.\n";
        return false;
    }
}

std::string Z_to_symbol(uint8_t Z) {
    if (!g_ptable_loaded) return "?";
    const auto* elem = g_ptable.by_Z(Z);
    return elem ? elem->symbol : "?";
}

// ============================================================================
// Molecule ? State conversion
// ============================================================================

State molecule_to_state(const vsepr::Molecule& mol) {
    State s;
    s.N = static_cast<uint32_t>(mol.num_atoms());
    s.X.resize(s.N);
    s.V.resize(s.N, {0, 0, 0});
    s.F.resize(s.N, {0, 0, 0});
    s.Q.resize(s.N, 0.0);
    s.M.resize(s.N);
    s.type.resize(s.N);

    for (uint32_t i = 0; i < s.N; ++i) {
        s.X[i] = {mol.coords[3*i], mol.coords[3*i+1], mol.coords[3*i+2]};
        s.type[i] = mol.atoms[i].Z;
        const auto* elem = g_ptable.by_Z(mol.atoms[i].Z);
        s.M[i] = elem ? elem->mass : 1.0;
    }

    // Convert bonds
    for (const auto& b : mol.bonds) {
        s.B.push_back({b.i, b.j});
    }

    return s;
}

std::vector<std::string> molecule_symbols(const vsepr::Molecule& mol) {
    std::vector<std::string> syms;
    for (const auto& a : mol.atoms) {
        syms.push_back(Z_to_symbol(a.Z));
    }
    return syms;
}

// ============================================================================
// CLI STATE
// ============================================================================

struct CLIState {
    State current_molecule;
    std::vector<std::string> element_symbols;
    bool has_molecule = false;
    std::string last_formula = "";

    // Keep vsepr::Molecule for identity/validation
    vsepr::Molecule vsepr_mol;
    bool has_vsepr_mol = false;
};

CLIState g_cli;

std::string to_lower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

// ============================================================================
// COMMANDS
// ============================================================================

void cmd_help() {
    std::cout << R"(
Available Commands:
  build <formula>     Build molecule from chemical formula
                      Examples: H2O, CH4, NH3, C2H6, C6H6, SF6
                      Pipeline: parse -> VSEPR -> FIRE -> validate
  load <file.xyz>     Load molecule from XYZ file
  save <file.xyz>     Save current molecule to XYZ file
  info                Show information about current molecule
  validate            Run validation gates on current molecule
  identity            Show canonical identity and fingerprints
  clear               Clear current molecule
  help                Show this help message
  exit                Exit interactive mode

Examples:
  >> build H2O
  >> info
  >> validate
  >> identity
  >> save H2O.xyz
)" << std::endl;
}

void cmd_build(const std::string& formula) {
    if (!ensure_periodic_table()) return;

    std::cout << "Building " << formula << "...\n";
    std::cout << "  Pipeline: parse -> VSEPR placement -> FIRE optimization\n";

    try {
        vsepr::MoleculeBuildSettings settings = vsepr::MoleculeBuildSettings::production();
        settings.physics_json_path = "data/elements.physics.json";

        vsepr::Molecule mol = vsepr::build_and_optimize_from_formula(formula, settings);

        // Validate
        auto report = vsepr::validation::validate_molecule(mol);

        // Convert to atomistic::State
        g_cli.current_molecule = molecule_to_state(mol);
        g_cli.element_symbols = molecule_symbols(mol);
        g_cli.has_molecule = true;
        g_cli.last_formula = formula;
        g_cli.vsepr_mol = mol;
        g_cli.has_vsepr_mol = true;

        std::cout << "  Atoms: " << mol.num_atoms() << "\n";
        std::cout << "  Bonds: " << mol.num_bonds() << "\n";
        std::cout << "  Validation: " << report.summary() << "\n";

        // Show identity
        auto id = vsepr::identity::compute_identity(mol, Z_to_symbol);
        std::cout << "  Identity: " << id.summary() << "\n";

        std::cout << "Built " << formula << ". Use 'info' for details, 'save <file>' to export.\n";

    } catch (const std::exception& e) {
        std::cerr << "Build failed: " << e.what() << "\n";
    }
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
    std::cout << "Atoms: " << mol.N << "\n";
    std::cout << "Bonds: " << mol.B.size() << "\n\n";

    std::cout << "Atomic Composition:\n";
    std::map<uint32_t, int> element_counts;
    for (uint32_t i = 0; i < mol.N; ++i) {
        element_counts[mol.type[i]]++;
    }
    for (const auto& [Z, count] : element_counts) {
        std::cout << "  " << Z_to_symbol(static_cast<uint8_t>(Z))
                  << " (Z=" << Z << "): " << count << "\n";
    }

    std::cout << "\nCoordinates (Angstrom):\n";
    std::cout << "  Atom  Elem       X         Y         Z\n";
    std::cout << "  ----  ----  --------  --------  --------\n";
    for (uint32_t i = 0; i < mol.N; ++i) {
        std::cout << "  " << std::setw(4) << i << "  ";
        std::cout << std::setw(4) << Z_to_symbol(static_cast<uint8_t>(mol.type[i])) << "  ";
        std::cout << std::fixed << std::setprecision(4);
        std::cout << std::setw(8) << mol.X[i].x << "  ";
        std::cout << std::setw(8) << mol.X[i].y << "  ";
        std::cout << std::setw(8) << mol.X[i].z << "\n";
    }
    std::cout << std::endl;
}

void cmd_validate() {
    if (!g_cli.has_vsepr_mol) {
        std::cerr << "No molecule with topology. Use 'build <formula>' first.\n";
        return;
    }

    auto report = vsepr::validation::validate_molecule(g_cli.vsepr_mol);
    std::cout << "\n=== Validation Report ===\n";
    std::cout << report.summary() << "\n\n";
}

void cmd_identity() {
    if (!g_cli.has_vsepr_mol) {
        std::cerr << "No molecule with topology. Use 'build <formula>' first.\n";
        return;
    }

    auto id = vsepr::identity::compute_identity(g_cli.vsepr_mol, Z_to_symbol);
    std::cout << "\n=== Molecular Identity ===\n";
    std::cout << "  Formula:       " << id.formula << "\n";
    std::cout << "  Charge:        " << id.charge << "\n";
    std::cout << "  Topology hash: " << std::hex << id.topology_hash << std::dec << "\n";
    std::cout << "  Geometry hash: " << std::hex << id.geometry_hash << std::dec << "\n";
    std::cout << "  DB key:        " << id.db_key() << "\n";
    std::cout << "  Summary:       " << id.summary() << "\n\n";
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
    g_cli.has_vsepr_mol = false;

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
    g_cli.has_vsepr_mol = false;
    g_cli.last_formula = "";
    g_cli.element_symbols.clear();
    std::cout << "Molecule cleared.\n";
}

// ============================================================================
// COMMAND DISPATCH
// ============================================================================

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
    } else if (cmd == "validate") {
        cmd_validate();
    } else if (cmd == "identity") {
        cmd_identity();
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

    // Pre-load periodic table
    if (!ensure_periodic_table()) {
        std::cerr << "Warning: Periodic table not loaded. Build command unavailable.\n\n";
    }

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
            std::cout << ">> " << line << "\n";
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
