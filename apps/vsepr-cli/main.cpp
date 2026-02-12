/*
vsepr_cli.cpp
-------------
CLI wrapper for VSEPR simulation engine.
Thin layer - delegates to core solver.

Commands:
  vsepr make --formula H2O --out h2o.json
  vsepr run input.json
  vsepr optimize input.json --out result.json
*/

#include "sim/molecule.hpp"
#include "pot/energy_model.hpp"
#include "sim/optimizer.hpp"
#include "core/json_schema.hpp"
#include "core/geom_ops.hpp"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <map>
#include <cstring>

using namespace vsepr;

// Simple formula parser (H2O, CH4, etc.)
struct FormulaParser {
    std::map<int, int> atoms;  // Z -> count
    
    void parse(const std::string& formula) {
        size_t i = 0;
        while (i < formula.size()) {
            // Read element symbol
            if (!std::isupper(formula[i])) {
                throw std::runtime_error("Invalid formula at position " + std::to_string(i));
            }
            
            std::string symbol;
            symbol += formula[i++];
            
            // Optional lowercase letter
            if (i < formula.size() && std::islower(formula[i])) {
                symbol += formula[i++];
            }
            
            // Read count
            int count = 0;
            while (i < formula.size() && std::isdigit(formula[i])) {
                count = count * 10 + (formula[i++] - '0');
            }
            if (count == 0) count = 1;
            
            // Map symbol to atomic number
            int Z = symbol_to_Z(symbol);
            atoms[Z] += count;
        }
    }
    
    int symbol_to_Z(const std::string& sym) {
        static std::map<std::string, int> table = {
            {"H", 1}, {"He", 2}, {"Li", 3}, {"Be", 4}, {"B", 5}, {"C", 6},
            {"N", 7}, {"O", 8}, {"F", 9}, {"Ne", 10}, {"Na", 11}, {"Mg", 12},
            {"Al", 13}, {"Si", 14}, {"P", 15}, {"S", 16}, {"Cl", 17}, {"Ar", 18},
            {"K", 19}, {"Ca", 20}, {"Br", 35}, {"I", 53}
        };
        
        auto it = table.find(sym);
        if (it == table.end()) {
            throw std::runtime_error("Unknown element: " + sym);
        }
        return it->second;
    }
};

// Build simple molecule from formula (rough geometry)
Molecule build_from_formula(const std::string& formula) {
    FormulaParser parser;
    parser.parse(formula);
    
    Molecule mol;
    
    // Find central atom (highest valence, lowest count)
    int central_Z = -1;
    int min_count = 1000;
    
    for (const auto& [Z, count] : parser.atoms) {
        if (Z != 1 && count < min_count) {  // Not hydrogen
            central_Z = Z;
            min_count = count;
        }
    }
    
    // If only hydrogens, just make H2
    if (central_Z == -1) {
        central_Z = 1;
    }
    
    // Add central atom(s)
    std::vector<uint32_t> central_indices;
    for (int i = 0; i < parser.atoms[central_Z]; ++i) {
        mol.add_atom(central_Z, i * 1.5, 0.0, 0.0);
        central_indices.push_back(mol.num_atoms() - 1);
    }
    
    // Assign lone pairs based on typical valence
    auto assign_lone_pairs = [](int Z) -> int {
        if (Z == 8) return 2;       // O
        if (Z == 7) return 1;       // N
        if (Z == 16) return 2;      // S
        if (Z == 9 || Z == 17) return 3;  // F, Cl
        return 0;
    };
    
    mol.atoms[central_indices[0]].lone_pairs = assign_lone_pairs(central_Z);
    
    // Add ligands around first central atom
    double bond_length = 1.0;  // Will be optimized
    int ligand_idx = 0;
    
    for (const auto& [Z, count] : parser.atoms) {
        if (Z == central_Z) continue;  // Skip central
        
        for (int i = 0; i < count; ++i) {
            // Place in rough geometry
            double angle = ligand_idx * (2.0 * M_PI / (count));
            double x = central_indices[0] * 1.5 + bond_length * std::cos(angle);
            double y = bond_length * std::sin(angle);
            double z = (ligand_idx % 2 == 0) ? 0.3 : -0.3;
            
            mol.add_atom(Z, x, y, z);
            mol.add_bond(central_indices[0], mol.num_atoms() - 1, 1);
            ligand_idx++;
        }
    }
    
    return mol;
}

void cmd_make(int argc, char** argv) {
    std::string formula;
    std::string output_file;
    
    // Parse args
    for (int i = 2; i < argc; ++i) {
        if (std::strcmp(argv[i], "--formula") == 0 && i + 1 < argc) {
            formula = argv[++i];
        } else if (std::strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
            output_file = argv[++i];
        }
    }
    
    if (formula.empty()) {
        std::cerr << "Usage: vsepr make --formula H2O --out h2o.json\n";
        exit(1);
    }
    
    if (output_file.empty()) {
        output_file = "molecule.json";
    }
    
    // Build molecule
    Molecule mol = build_from_formula(formula);
    
    // Generate topology
    mol.generate_angles_from_bonds();
    mol.generate_torsions_from_bonds();
    
    // Write JSON
    std::string json = write_simulation_json(mol);
    
    std::ofstream out(output_file);
    if (!out) {
        std::cerr << "Error: Cannot write to " << output_file << "\n";
        exit(1);
    }
    out << json;
    
    std::cout << "Created " << output_file << " from formula " << formula << "\n";
    std::cout << "  Atoms: " << mol.num_atoms() << "\n";
    std::cout << "  Bonds: " << mol.bonds.size() << "\n";
}

void cmd_run(const std::string& input_file, const std::string& output_file = "") {
    // Load molecule
    Molecule mol = read_molecule_json(input_file);
    
    // Auto-generate topology if needed
    if (mol.angles.empty()) {
        mol.generate_angles_from_bonds();
    }
    if (mol.torsions.empty()) {
        mol.generate_torsions_from_bonds();
    }
    
    // Setup energy model (default params)
    NonbondedParams nb_params;
    nb_params.epsilon = 0.1;
    nb_params.scale_13 = 0.5;
    
    EnergyModel energy(mol, 300.0, true, true, nb_params, true);
    
    // Setup optimizer
    OptimizerSettings settings;
    settings.max_iterations = 500;
    settings.tol_rms_force = 1e-4;
    settings.print_every = 50;
    
    FIREOptimizer optimizer(settings);
    
    std::cout << "Optimizing " << input_file << "...\n";
    std::cout << "  Atoms: " << mol.num_atoms() << "\n";
    std::cout << "  Bonds: " << mol.bonds.size() << "\n";
    std::cout << "  Angles: " << mol.angles.size() << "\n";
    std::cout << "  Torsions: " << mol.torsions.size() << "\n\n";
    
    auto result = optimizer.minimize(mol.coords, energy);
    
    std::cout << "\nOptimization complete:\n";
    std::cout << "  Converged: " << (result.converged ? "YES" : "NO") << "\n";
    std::cout << "  Iterations: " << result.iterations << "\n";
    std::cout << "  Final energy: " << result.energy << " kcal/mol\n";
    std::cout << "  RMS force: " << result.rms_force << "\n";
    std::cout << "  Max force: " << result.max_force << "\n";
    std::cout << "\nEnergy breakdown:\n";
    std::cout << "  Bond:      " << result.energy_breakdown.bond_energy << " kcal/mol\n";
    std::cout << "  Angle:     " << result.energy_breakdown.angle_energy << " kcal/mol\n";
    std::cout << "  Nonbonded: " << result.energy_breakdown.nonbonded_energy << " kcal/mol\n";
    std::cout << "  Torsion:   " << result.energy_breakdown.torsion_energy << " kcal/mol\n";
    
    // Update coordinates
    mol.coords = result.coords;
    
    // Write result if requested
    if (!output_file.empty()) {
        std::string json = write_simulation_json(mol);
        std::ofstream out(output_file);
        out << json;
        std::cout << "\nWrote result to " << output_file << "\n";
    }
}

void cmd_optimize(int argc, char** argv) {
    std::string input_file;
    std::string output_file;
    
    for (int i = 2; i < argc; ++i) {
        if (argv[i][0] != '-') {
            input_file = argv[i];
        } else if (std::strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
            output_file = argv[++i];
        }
    }
    
    if (input_file.empty()) {
        std::cerr << "Usage: vsepr optimize input.json [--out result.json]\n";
        exit(1);
    }
    
    cmd_run(input_file, output_file);
}

void cmd_analyze(const std::string& input_file) {
    // Load molecule
    Molecule mol = read_molecule_json(input_file);
    
    // Auto-generate topology if needed
    if (mol.angles.empty()) {
        mol.generate_angles_from_bonds();
    }
    if (mol.torsions.empty()) {
        mol.generate_torsions_from_bonds();
    }
    
    std::cout << "Analyzing " << input_file << "\n";
    std::cout << "==========================================\n\n";
    
    // Basic topology
    std::cout << "Topology:\n";
    std::cout << "  Atoms:    " << mol.num_atoms() << "\n";
    std::cout << "  Bonds:    " << mol.bonds.size() << "\n";
    std::cout << "  Angles:   " << mol.angles.size() << "\n";
    std::cout << "  Torsions: " << mol.torsions.size() << "\n\n";
    
    // Atom list
    std::cout << "Atoms:\n";
    for (size_t i = 0; i < mol.atoms.size(); ++i) {
        int Z = mol.atoms[i].Z;
        std::string symbol = "?";
        if (Z == 1) symbol = "H";
        else if (Z == 6) symbol = "C";
        else if (Z == 7) symbol = "N";
        else if (Z == 8) symbol = "O";
        else if (Z == 9) symbol = "F";
        else if (Z == 15) symbol = "P";
        else if (Z == 16) symbol = "S";
        else if (Z == 17) symbol = "Cl";
        
        std::cout << "  [" << i << "] " << symbol << " (Z=" << Z << ")";
        if (mol.atoms[i].lone_pairs > 0) {
            std::cout << " LP=" << static_cast<int>(mol.atoms[i].lone_pairs);
        }
        std::cout << " at (" << std::fixed << std::setprecision(3)
                  << mol.coords[3*i] << ", " 
                  << mol.coords[3*i+1] << ", "
                  << mol.coords[3*i+2] << ")\n";
    }
    std::cout << "\n";
    
    // Bond list with lengths
    std::cout << "Bonds:\n";
    for (const auto& bond : mol.bonds) {
        double r = distance(mol.coords, bond.i, bond.j);
        std::cout << "  " << bond.i << "-" << bond.j 
                  << " order=" << static_cast<int>(bond.order)
                  << " length=" << std::fixed << std::setprecision(3) << r << " Å\n";
    }
    std::cout << "\n";
    
    // Angle list with values
    if (mol.angles.size() > 0 && mol.angles.size() <= 20) {
        std::cout << "Angles:\n";
        for (const auto& ang : mol.angles) {
            double theta = angle(mol.coords, ang.i, ang.j, ang.k) * 180.0 / M_PI;
            std::cout << "  " << ang.i << "-" << ang.j << "-" << ang.k
                      << " = " << std::fixed << std::setprecision(1) << theta << "°\n";
        }
        std::cout << "\n";
    }
    
    // Count torsion types
    if (mol.torsions.size() > 0) {
        int heavy_torsions = 0;
        int h_torsions = 0;
        for (const auto& t : mol.torsions) {
            bool has_h = (mol.atoms[t.i].Z == 1 || mol.atoms[t.j].Z == 1 ||
                          mol.atoms[t.k].Z == 1 || mol.atoms[t.l].Z == 1);
            if (has_h) h_torsions++;
            else heavy_torsions++;
        }
        
        std::cout << "Torsions:\n";
        std::cout << "  Heavy-atom: " << heavy_torsions << " (with barriers)\n";
        std::cout << "  H-involving: " << h_torsions << " (V=0)\n";
        
        if (heavy_torsions > 0 && heavy_torsions <= 10) {
            std::cout << "  Heavy-atom torsions:\n";
            for (const auto& t : mol.torsions) {
                bool has_h = (mol.atoms[t.i].Z == 1 || mol.atoms[t.j].Z == 1 ||
                              mol.atoms[t.k].Z == 1 || mol.atoms[t.l].Z == 1);
                if (!has_h) {
                    double phi = torsion(mol.coords, t.i, t.j, t.k, t.l) * 180.0 / M_PI;
                    std::cout << "    " << t.i << "-" << t.j << "-" << t.k << "-" << t.l
                              << " = " << std::fixed << std::setprecision(1) << phi << "°\n";
                }
            }
        }
        std::cout << "\n";
    }
    
    // Energy evaluation (no optimization)
    NonbondedParams nb_params;
    nb_params.epsilon = 0.1;
    nb_params.scale_13 = 0.5;
    
    EnergyModel energy(mol, 300.0, true, true, nb_params, true);
    auto breakdown = energy.evaluate_detailed(mol.coords);
    
    std::cout << "Energy (current geometry):\n";
    std::cout << "  Total:     " << std::fixed << std::setprecision(4) << breakdown.total_energy << " kcal/mol\n";
    std::cout << "  Bond:      " << breakdown.bond_energy << " kcal/mol\n";
    std::cout << "  Angle:     " << breakdown.angle_energy << " kcal/mol\n";
    std::cout << "  Nonbonded: " << breakdown.nonbonded_energy << " kcal/mol\n";
    std::cout << "  Torsion:   " << breakdown.torsion_energy << " kcal/mol\n";
}

void print_usage() {
    std::cout << "VSEPR Molecular Simulation Tool\n\n";
    std::cout << "Usage:\n";
    std::cout << "  vsepr make --formula H2O --out h2o.json\n";
    std::cout << "    Generate molecule structure from formula\n\n";
    std::cout << "  vsepr analyze input.json\n";
    std::cout << "    Analyze geometry and energy without optimization\n\n";
    std::cout << "  vsepr run input.json\n";
    std::cout << "    Run optimization from JSON file\n\n";
    std::cout << "  vsepr optimize input.json --out result.json\n";
    std::cout << "    Optimize and save result\n\n";
}

int main(int argc, char** argv) {
    if (argc < 2) {
        print_usage();
        return 1;
    }
    
    std::string command = argv[1];
    
    try {
        if (command == "make") {
            cmd_make(argc, argv);
        } else if (command == "analyze") {
            if (argc < 3) {
                std::cerr << "Usage: vsepr analyze input.json\n";
                return 1;
            }
            cmd_analyze(argv[2]);
        } else if (command == "run") {
            if (argc < 3) {
                std::cerr << "Usage: vsepr run input.json\n";
                return 1;
            }
            cmd_run(argv[2]);
        } else if (command == "optimize") {
            cmd_optimize(argc, argv);
        } else {
            std::cerr << "Unknown command: " << command << "\n\n";
            print_usage();
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
