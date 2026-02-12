/**
 * topology_builder.cpp
 * --------------------
 * Implementation of formula → topology converter.
 */

#include "../include/topology_builder.hpp"
#include <cctype>
#include <cmath>
#include <random>
#include <fstream>
#include <iostream>

namespace vsepr {

// ============================================================================
// Formula Parser
// ============================================================================

std::map<std::string, int> parse_formula(const std::string& formula) {
    std::map<std::string, int> composition;
    
    size_t i = 0;
    while (i < formula.size()) {
        // Skip whitespace
        if (std::isspace(formula[i])) {
            ++i;
            continue;
        }
        
        // Read element symbol (uppercase + optional lowercase)
        std::string element;
        if (std::isupper(formula[i])) {
            element += formula[i++];
            if (i < formula.size() && std::islower(formula[i])) {
                element += formula[i++];
            }
        } else {
            // Invalid formula
            return {};
        }
        
        // Read count (optional, default = 1)
        int count = 0;
        while (i < formula.size() && std::isdigit(formula[i])) {
            count = count * 10 + (formula[i++] - '0');
        }
        if (count == 0) count = 1;
        
        composition[element] += count;
    }
    
    return composition;
}

// ============================================================================
// Topology Builder
// ============================================================================

TopologyBuilder::TopologyBuilder() {
    // Initialize periodic table (atomic numbers)
    atomic_numbers_ = {
        {"H", 1}, {"He", 2},
        {"Li", 3}, {"Be", 4}, {"B", 5}, {"C", 6}, {"N", 7}, {"O", 8}, {"F", 9}, {"Ne", 10},
        {"Na", 11}, {"Mg", 12}, {"Al", 13}, {"Si", 14}, {"P", 15}, {"S", 16}, {"Cl", 17}, {"Ar", 18},
        {"K", 19}, {"Ca", 20},
        {"Br", 35}, {"Kr", 36},
        {"I", 53}, {"Xe", 54}
    };
    
    // Typical bond lengths (Angstroms) for common pairs
    // Format: {Z1, Z2} → length (smaller Z first)
    bond_lengths_ = {
        {{1, 1}, 0.74},   // H-H
        {{1, 6}, 1.09},   // C-H
        {{1, 7}, 1.01},   // N-H
        {{1, 8}, 0.96},   // O-H
        {{6, 6}, 1.54},   // C-C single
        {{6, 7}, 1.47},   // C-N
        {{6, 8}, 1.43},   // C-O
        {{7, 7}, 1.45},   // N-N
        {{7, 8}, 1.40},   // N-O
        {{8, 8}, 1.48},   // O-O
        {{14, 8}, 1.61},  // Si-O
    };
}

std::optional<Molecule> TopologyBuilder::build_from_formula(
    const std::string& formula,
    GeometryGuess guess,
    int charge,
    int seed
) {
    last_error_ = "";
    
    // Parse formula
    auto composition = parse_formula(formula);
    if (composition.empty()) {
        last_error_ = "Failed to parse formula: " + formula;
        return std::nullopt;
    }
    
    // Build based on geometry guess
    Molecule mol;
    switch (guess) {
        case GeometryGuess::VSEPR:
            mol = build_vsepr(composition, seed);
            break;
        case GeometryGuess::CHAIN:
            mol = build_chain(composition, seed);
            break;
        case GeometryGuess::RING:
            mol = build_ring(composition, seed);
            break;
        case GeometryGuess::RANDOM:
            mol = build_random(composition, seed);
            break;
        default:
            mol = build_vsepr(composition, seed);
            break;
    }
    
    return mol;
}

std::optional<Molecule> TopologyBuilder::load_preset(
    const std::string& name,
    const std::string& variant
) {
    last_error_ = "";
    
    // Build path: data/presets/<name>/<variant>.json or data/presets/<name>.json
    std::string filename;
    if (variant.empty()) {
        filename = "data/presets/" + name + ".json";
    } else {
        filename = "data/presets/" + name + "/" + variant + ".json";
    }
    
    // Try to load JSON file
    std::ifstream file(filename);
    if (!file.is_open()) {
        last_error_ = "Preset not found: " + filename;
        return std::nullopt;
    }
    
    // For now, return empty molecule - full JSON parsing will be added
    // TODO: Implement JSON → Molecule deserialization
    Molecule mol;
    last_error_ = "JSON parsing not yet implemented for presets";
    return std::nullopt;
}

std::vector<std::string> TopologyBuilder::list_presets() const {
    // TODO: Scan data/presets/ directory
    return {"h2o", "ch4", "nh3", "co2", "butane", "benzene"};
}

// ============================================================================
// Build Strategies
// ============================================================================

Molecule TopologyBuilder::build_vsepr(const std::map<std::string, int>& composition, int seed) {
    Molecule mol;
    
    // Strategy: Find central atom (lowest count, highest valence)
    // Then attach ligands in VSEPR geometry
    
    // Simple heuristic: prioritize C, N, O, S, P as central atoms
    std::string central_element;
    int central_count = 1000;
    
    for (const auto& [elem, count] : composition) {
        if (elem == "C" || elem == "N" || elem == "O" || elem == "S" || elem == "P") {
            if (count < central_count) {
                central_element = elem;
                central_count = count;
            }
        }
    }
    
    // If no priority element, use first element with count=1
    if (central_element.empty()) {
        for (const auto& [elem, count] : composition) {
            if (count == 1) {
                central_element = elem;
                break;
            }
        }
    }
    
    // Still nothing? Use first element
    if (central_element.empty()) {
        central_element = composition.begin()->first;
    }
    
    int central_Z = get_atomic_number(central_element);
    
    // Add central atom at origin
    mol.add_atom(central_Z, 0.0, 0.0, 0.0);
    
    // Add ligands
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> angle_dist(0.0, 2.0 * M_PI);
    
    int ligand_count = 0;
    for (const auto& [elem, count] : composition) {
        if (elem == central_element) {
            continue;  // Skip central atom
        }
        
        int Z = get_atomic_number(elem);
        double bond_len = get_bond_length(central_Z, Z);
        
        for (int i = 0; i < count; ++i) {
            // Place ligand in VSEPR-appropriate position
            apply_vsepr_geometry(mol, 0);  // 0 = central atom index
            
            // For now, simple tetrahedral-ish placement
            double theta = ligand_count * 109.5 * M_PI / 180.0;
            double phi = angle_dist(rng);
            
            double x = bond_len * sin(theta) * cos(phi);
            double y = bond_len * sin(theta) * sin(phi);
            double z = bond_len * cos(theta);
            
            int atom_idx = mol.add_atom(Z, x, y, z);
            mol.add_bond(0, atom_idx, 1);  // Single bond to central atom
            ligand_count++;
        }
    }
    
    // Generate angles from bonds
    mol.generate_angles_from_bonds();
    
    return mol;
}

Molecule TopologyBuilder::build_chain(const std::map<std::string, int>& composition, int seed) {
    Molecule mol;
    
    // Build linear chain by alternating elements
    std::vector<std::string> elements;
    for (const auto& [elem, count] : composition) {
        for (int i = 0; i < count; ++i) {
            elements.push_back(elem);
        }
    }
    
    double x = 0.0;
    for (size_t i = 0; i < elements.size(); ++i) {
        int Z = get_atomic_number(elements[i]);
        mol.add_atom(Z, x, 0.0, 0.0);
        
        if (i > 0) {
            double bond_len = 1.5;  // Default bond length
            mol.add_bond(i - 1, i, 1);
            x += bond_len;
        }
    }
    
    mol.generate_angles_from_bonds();
    return mol;
}

Molecule TopologyBuilder::build_ring(const std::map<std::string, int>& composition, int seed) {
    Molecule mol;
    
    // Build cyclic ring
    std::vector<std::string> elements;
    for (const auto& [elem, count] : composition) {
        for (int i = 0; i < count; ++i) {
            elements.push_back(elem);
        }
    }
    
    int n = elements.size();
    double radius = 1.5;
    
    for (size_t i = 0; i < elements.size(); ++i) {
        int Z = get_atomic_number(elements[i]);
        double angle = 2.0 * M_PI * i / n;
        double x = radius * cos(angle);
        double y = radius * sin(angle);
        mol.add_atom(Z, x, y, 0.0);
        
        if (i > 0) {
            mol.add_bond(i - 1, i, 1);
        }
    }
    
    // Close ring
    if (n > 2) {
        mol.add_bond(n - 1, 0, 1);
    }
    
    mol.generate_angles_from_bonds();
    return mol;
}

Molecule TopologyBuilder::build_random(const std::map<std::string, int>& composition, int seed) {
    Molecule mol;
    
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> dist(-2.0, 2.0);
    
    for (const auto& [elem, count] : composition) {
        int Z = get_atomic_number(elem);
        for (int i = 0; i < count; ++i) {
            mol.add_atom(Z, dist(rng), dist(rng), dist(rng));
        }
    }
    
    return mol;
}

// ============================================================================
// Helper Functions
// ============================================================================

int TopologyBuilder::get_atomic_number(const std::string& symbol) const {
    auto it = atomic_numbers_.find(symbol);
    if (it != atomic_numbers_.end()) {
        return it->second;
    }
    return 0;  // Unknown element
}

double TopologyBuilder::get_bond_length(int Z1, int Z2) const {
    // Ensure Z1 < Z2 for lookup
    if (Z1 > Z2) std::swap(Z1, Z2);
    
    auto it = bond_lengths_.find({Z1, Z2});
    if (it != bond_lengths_.end()) {
        return it->second;
    }
    
    // Default fallback
    return 1.5;
}

int TopologyBuilder::get_valence(int Z) const {
    // Simple valence rules
    if (Z == 1) return 1;   // H
    if (Z == 6) return 4;   // C
    if (Z == 7) return 3;   // N
    if (Z == 8) return 2;   // O
    if (Z == 9) return 1;   // F
    if (Z == 14) return 4;  // Si
    if (Z == 15) return 3;  // P
    if (Z == 16) return 2;  // S
    if (Z == 17) return 1;  // Cl
    return 1;  // Default
}

void TopologyBuilder::apply_vsepr_geometry(Molecule& mol, int central_atom) {
    // TODO: Implement proper VSEPR geometry based on coordination number
    // For now, this is a placeholder
}

} // namespace vsepr
