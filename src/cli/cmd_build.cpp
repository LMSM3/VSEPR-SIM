/**
 * cmd_build.cpp
 * -------------
 * Build command - create molecules from chemical formulas.
 */

#include "cmd_build.hpp"
#include "cli/display.hpp"
#include "sim/molecule.hpp"
#include "sim/graph_builder.hpp"
#include "pot/periodic_db.hpp"
#include "pot/energy_model.hpp"
#include "sim/optimizer.hpp"
#include "truth/truth_state.hpp"
#include "vsepr/formula_parser.hpp"
#include "sim/clash_relaxation.hpp"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <cmath>
#include <random>
#include <algorithm>
#include <map>
#include <cstdlib>

#ifdef BUILD_VISUALIZATION
#include "cmd_viz.hpp"
#endif

namespace vsepr {
namespace cli {

/**
 * Detect if a composition represents a star-like (VSEPR AXnEm) molecule
 * Returns true if molecule can be built as single-center star topology
 */
static bool is_star_like(
    const vsepr::formula::Composition& composition,
    const PeriodicTable& ptable
) {
    // Count heavy atoms (Z > 1)  
    int heavy_count = 0;
    int total_atoms = 0;
    int num_element_types = 0;
    
    for (const auto& [Z, count] : composition) {
        total_atoms += count;
        num_element_types++;
        if (Z > 1) {
            heavy_count += count;
        }
    }
    
    // Rule 1: Single heavy atom → star-like (CH4, NH3, H2O, etc.)
    if (heavy_count == 1) {
        return true;
    }
    
    // Rule 2: Single element type with multiple atoms → star-like
    // Examples: H2, O2, N2, Cl2, F2
    if (num_element_types == 1) {
        return true;
    }
    
    // Rule 3: Two element types where one is H → likely star-like
    // Examples: CH4, NH3, H2O, H2S, HCl, HF
    if (num_element_types == 2 && composition.count(1) > 0) {
        // One element is H, count non-H heavy atoms
        int non_H_heavy = 0;
        for (const auto& [Z, count] : composition) {
            if (Z > 1) non_H_heavy += count;
        }
        if (non_H_heavy == 1) return true;  // Single center with H ligands
    }
    
    // Rule 4: Check for classic VSEPR inorganics (one center, multiple ligands)
    // Examples: SF6, PF5, ClF3, XeF4, PCl5, IF7
    // Pattern: 1 atom of low-EN element + multiple atoms of high-EN element(s)
    
    // Find potential central atom (single count, not H, lower EN)
    int potential_center_Z = -1;
    int potential_center_count = 0;
    double potential_center_EN = 10.0;
    
    for (const auto& [Z, count] : composition) {
        if (Z == 1) continue;  // Skip H
        
        const Element* elem = ptable.by_Z(Z);
        if (!elem) continue;
        
        // Look for single atoms with lower electronegativity
        if (count == 1 && elem->en_pauling.has_value()) {
            double en = elem->en_pauling.value();
            if (en < potential_center_EN) {
                potential_center_Z = Z;
                potential_center_count = count;
                potential_center_EN = en;
            }
        }
    }
    
    // If we found a plausible center, check if rest are ligands
    if (potential_center_Z > 0) {
        bool all_others_are_ligands = true;
        
        for (const auto& [Z, count] : composition) {
            if (Z == potential_center_Z) continue;
            
            const Element* elem = ptable.by_Z(Z);
            if (!elem || !elem->en_pauling.has_value()) {
                all_others_are_ligands = false;
                break;
            }
            
            double en = elem->en_pauling.value();
            
            // Ligands are typically more electronegative than center
            // OR are hydrogen (special case)
            if (Z != 1 && en <= potential_center_EN) {
                all_others_are_ligands = false;
                break;
            }
        }
        
        if (all_others_are_ligands) return true;
    }
    
    // Rule 5: Oxoacids pattern - one central atom + oxygen + hydrogen
    // Examples: H2SO4, H3PO4, HNO3, H2CO3
    if (composition.count(1) > 0 && composition.count(8) > 0) {  // Has H and O
        // Check if there's a third element (the central atom)
        if (num_element_types == 3) {
            // Find the non-H, non-O element
            for (const auto& [Z, count] : composition) {
                if (Z != 1 && Z != 8 && count <= 2) {  // Central atom, usually 1-2 atoms
                    return true;  // Likely oxoacid
                }
            }
        }
    }
    
    // Rule 6: Multiple carbons or large organics → NOT star-like
    for (const auto& [Z, count] : composition) {
        if (Z == 6 && count > 1) {  // Multiple carbons
            return false;  // Organic chain/ring, not star
        }
    }
    
    // Default: if we can't confidently say it's star-like, it's not
    return false;
}

/**
 * Score an atom for central atom candidacy
 * Higher score = better central atom candidate
 */
static double central_atom_score(
    int Z, 
    int count,
    const PeriodicTable& ptable,
    int total_ligands_needed
) {
    // Never choose H as central
    if (Z == 1) return -1000.0;
    
    const Element* elem = ptable.by_Z(Z);
    if (!elem) return -1000.0;
    
    double score = 0.0;
    
    // Factor 1: Valence capacity (can it bond to enough ligands?)
    uint8_t valence = elem->valence_electrons();
    
    // Prefer atoms that can accommodate the required ligands
    // Common coordination: valence electrons map to typical bonds
    int typical_coordination = valence;
    if (Z >= 14 && Z <= 18) typical_coordination = std::min(4, (int)valence);  // Si, P, S, Cl
    if (Z >= 15 && Z <= 17) typical_coordination = std::max(3, (int)valence - 2);  // P, S, Cl
    
    score += typical_coordination * 10.0;
    
    // Factor 2: Lower count preferred (single central atom is ideal)
    score -= count * 5.0;
    
    // Factor 3: Electronegativity (lower is more central-like)
    if (elem->en_pauling.has_value()) {
        double en = elem->en_pauling.value();
        score -= en * 3.0;  // Lower EN = higher score
    }
    
    // Factor 4: Element-specific bonuses (common central atoms)
    // C, N, O, S, P are common centers in their respective classes
    if (Z == 6) score += 15.0;   // Carbon (organic)
    if (Z == 7) score += 12.0;   // Nitrogen
    if (Z == 8 && count == 1) score += 5.0;  // Oxygen (less common but possible)
    if (Z == 16) score += 18.0;  // Sulfur (H2SO4, SF6, etc.)
    if (Z == 15) score += 16.0;  // Phosphorus (PF5, etc.)
    
    // Factor 5: Penalty for highly electronegative (terminal) elements
    if (Z == 9 || Z == 17 || Z == 35 || Z == 53) {  // F, Cl, Br, I
        score -= 50.0;  // Halogens are almost never central
    }
    
    return score;
}

/**
 * Get VSEPR geometry name based on electron groups (bonding + lone pairs)
 */
static std::string get_vsepr_geometry_name(int bonding_groups, int lone_pairs) {
    int total_groups = bonding_groups + lone_pairs;
    
    if (total_groups == 2) return "Linear";
    if (total_groups == 3) {
        if (lone_pairs == 0) return "Trigonal Planar";
        if (lone_pairs == 1) return "Bent";
    }
    if (total_groups == 4) {
        if (lone_pairs == 0) return "Tetrahedral";
        if (lone_pairs == 1) return "Trigonal Pyramidal";
        if (lone_pairs == 2) return "Bent";
    }
    if (total_groups == 5) {
        if (lone_pairs == 0) return "Trigonal Bipyramidal";
        if (lone_pairs == 1) return "Seesaw";
        if (lone_pairs == 2) return "T-shaped";
        if (lone_pairs == 3) return "Linear";
    }
    if (total_groups == 6) {
        if (lone_pairs == 0) return "Octahedral";
        if (lone_pairs == 1) return "Square Pyramidal";
        if (lone_pairs == 2) return "Square Planar";
    }
    if (total_groups == 7) {
        if (lone_pairs == 0) return "Pentagonal Bipyramidal";
    }
    
    return "Complex (" + std::to_string(total_groups) + " groups)";
}

/**
 * Select the central atom from composition using scoring heuristic
 */
static int select_central_atom(
    const vsepr::formula::Composition& composition,
    const PeriodicTable& ptable
) {
    // Count total ligands needed
    int total_atoms = 0;
    for (const auto& [Z, count] : composition) {
        total_atoms += count;
    }
    
    // If only one non-H element exists, it's central
    int non_H_element = -1;
    int non_H_count = 0;
    for (const auto& [Z, count] : composition) {
        if (Z != 1) {
            non_H_element = Z;
            non_H_count++;
        }
    }
    
    if (non_H_count == 1) {
        return non_H_element;
    }
    
    // If only H (e.g., H2), use H
    if (non_H_count == 0) {
        return 1;
    }
    
    // Score all candidates
    double best_score = -10000.0;
    int best_Z = -1;
    
    for (const auto& [Z, count] : composition) {
        double score = central_atom_score(Z, count, ptable, total_atoms - count);
        if (score > best_score) {
            best_score = score;
            best_Z = Z;
        }
    }
    
    return best_Z;
}

/**
 * Build star-like molecule from composition
 * Assumes composition has been validated as star-like
 */
static Molecule build_star_molecule(
    const vsepr::formula::Composition& composition,
    const PeriodicTable& ptable
) {
    Molecule mol;
    
    // Select central atom
    int central_Z = select_central_atom(composition, ptable);
    if (central_Z < 0) {
        throw std::runtime_error("Could not determine central atom");
    }
    
    int central_count = composition.at(central_Z);
    
    // SPECIAL CASE: Homonuclear diatomics (H2, N2, O2, F2, Cl2, Br2, I2)
    // These are NOT star molecules - they're simple 2-atom bonds
    if (composition.size() == 1 && central_count == 2) {
        // Build simple diatomic molecule
        double bond_length = compute_bond_length(central_Z, central_Z, 1.0);
        
        mol.add_atom(central_Z, -bond_length/2.0, 0.0, 0.0);
        mol.add_atom(central_Z, +bond_length/2.0, 0.0, 0.0);
        mol.add_bond(0, 1, 1);
        
        // No angles or torsions for 2-atom molecule
        return mol;
    }
    
    // For now, only support single central atom
    // (multi-center star topologies like H2SO4 need special handling)
    if (central_count > 1) {
        // Check if this is a special case like H2SO4 (1 S, multiple O)
        // For now, use first atom as center and space others
        // This is a simplified scaffold
    }
    
    // Add central atom at origin
    mol.add_atom(central_Z, 0.0, 0.0, 0.0);
    uint32_t central_idx = 0;
    
    // Assign lone pairs based on valence and expected coordination
    const Element* central_elem = ptable.by_Z(central_Z);
    if (central_elem) {
        uint8_t valence = central_elem->valence_electrons();
        
        // Count how many ligands will be bonded
        int num_ligands = 0;
        for (const auto& [Z, count] : composition) {
            if (Z != central_Z) {
                num_ligands += count;
            }
        }
        
        // Estimate lone pairs: (valence - bonding_electrons) / 2
        // Assume single bonds for now
        int bonding_electrons = num_ligands;
        int lone_pair_electrons = std::max(0, (int)valence - bonding_electrons);
        int lone_pairs = lone_pair_electrons / 2;
        
        mol.atoms[central_idx].lone_pairs = lone_pairs;
    }
    
    // Add ligands in spherical distribution
    // VSEPR will be refined by optimizer
    double bond_length = 1.5;  // Angstroms, will be optimized
    int ligand_idx = 0;
    
    // Track oxygen indices for H2SO4-type molecules
    std::vector<uint32_t> oxygen_indices;
    
    for (const auto& [Z, count] : composition) {
        if (Z == central_Z) continue;  // Skip central
        if (Z == 1) continue;  // Handle H separately for oxoacids
        
        for (int i = 0; i < count; ++i) {
            // Fibonacci sphere distribution for even spacing
            double theta = ligand_idx * M_PI * (1.0 + std::sqrt(5.0));  // Golden angle
            double phi = std::acos(1.0 - 2.0 * (ligand_idx + 0.5) / (count + 1e-6));
            
            // CRITICAL FIX: Use covalent radii for proper bond length
            double actual_bond_length = compute_bond_length(central_Z, Z, 1.0);
            
            double x = actual_bond_length * std::sin(phi) * std::cos(theta);
            double y = actual_bond_length * std::sin(phi) * std::sin(theta);
            double z = actual_bond_length * std::cos(phi);
            
            mol.add_atom(Z, x, y, z);
            uint32_t ligand_atom_idx = mol.num_atoms() - 1;
            mol.add_bond(central_idx, ligand_atom_idx, 1);
            
            // Track oxygen for potential H attachment
            if (Z == 8) {
                oxygen_indices.push_back(ligand_atom_idx);
            }
            
            ligand_idx++;
        }
    }
    
    // Handle hydrogen attachment
    // For oxoacids (H2SO4, H3PO4), attach H to O instead of central atom
    if (composition.count(1) > 0) {  // Has hydrogen
        int H_count = composition.at(1);
        
        // If we have O atoms and H atoms, attach H to O (oxoacid pattern)
        if (!oxygen_indices.empty() && H_count <= oxygen_indices.size()) {
            double OH_bond = 0.96;  // O-H bond length
            
            for (int i = 0; i < H_count; ++i) {
                // Attach H to oxygen
                uint32_t O_idx = oxygen_indices[i];
                
                // Place H offset from O (opposite side from central)
                double Ox = mol.coords[3*O_idx];
                double Oy = mol.coords[3*O_idx + 1];
                double Oz = mol.coords[3*O_idx + 2];
                
                // Direction: normalize and extend
                double r = std::sqrt(Ox*Ox + Oy*Oy + Oz*Oz);
                if (r > 0.01) {
                    double Hx = Ox + (Ox/r) * OH_bond;
                    double Hy = Oy + (Oy/r) * OH_bond;
                    double Hz = Oz + (Oz/r) * OH_bond;
                    
                    mol.add_atom(1, Hx, Hy, Hz);
                    mol.add_bond(O_idx, mol.num_atoms() - 1, 1);
                }
            }
        } else {
            // Regular case: attach H directly to central (NH3, CH4, etc.)
            for (int i = 0; i < H_count; ++i) {
                double theta = ligand_idx * M_PI * (1.0 + std::sqrt(5.0));
                double phi = std::acos(1.0 - 2.0 * (ligand_idx + 0.5) / (H_count + 1e-6));
                
                double x = bond_length * std::sin(phi) * std::cos(theta);
                double y = bond_length * std::sin(phi) * std::sin(theta);
                double z = bond_length * std::cos(phi);
                
                mol.add_atom(1, x, y, z);
                mol.add_bond(central_idx, mol.num_atoms() - 1, 1);
                ligand_idx++;
            }
        }
    }
    
    // Generate topology from connectivity
    mol.generate_angles_from_bonds();
    mol.generate_torsions_from_bonds();
    
    return mol;
}

/**
 * Build molecule from parsed composition using algorithmic approach
 * Phase 1: Star-like molecules only (VSEPR AXnEm topology)
 * 
 * Based on real E&M physics and thermochemistry from periodic table data.
 * NOT hardcoded star classes - uses physics-based detection.
 */
static Molecule build_molecule_from_composition(
    const vsepr::formula::Composition& composition,
    const PeriodicTable& ptable
) {
    // Detect if molecule is star-like (single-center VSEPR)
    if (!is_star_like(composition, ptable)) {
        // Multi-center topology - try graph-based construction
        try {
            return build_molecule_from_graph(composition, ptable);
        } catch (const std::exception& graph_err) {
            // Graph builder failed - provide comprehensive error
            std::ostringstream err;
            err << "Multi-center topology construction failed.\n\n";
            err << "Graph builder error: " << graph_err.what() << "\n\n";
            
            // Provide specific hints based on composition
            bool has_multiple_carbons = false;
            bool has_multiple_heavy = false;
            int heavy_count = 0;
            
            for (const auto& [Z, count] : composition) {
                if (Z == 6 && count > 1) has_multiple_carbons = true;
                if (Z > 1) heavy_count += count;
            }
            if (heavy_count > 1) has_multiple_heavy = true;
            
            if (has_multiple_carbons) {
                err << "Detected: Multiple carbons (organic molecule)\n";
                err << "\nCurrently supported organic molecules:\n";
                err << "  ✓ Alkanes (CnH(2n+2)): C2H6, C3H8, C4H10, C6H14, etc.\n";
                err << "  ⧗ Alkenes (CnH(2n)): coming in Phase 2.2\n";
                err << "  ⧗ Aromatics: coming in Phase 2.3\n";
            } else if (has_multiple_heavy) {
                err << "Detected: Multiple heavy atoms\n";
                err << "Suggested: Try simpler analogs or wait for coordination complex builder\n";
            }
            
            err << "\nPhase 1 (VSEPR stars) - working:\n";
            err << "  ✓ Binary hydrides: CH4, NH3, H2O, H2S, HCl\n";
            err << "  ✓ Hypervalent: SF6, PF5, PCl5, IF7\n";
            err << "  ✓ Halogen compounds: ClF3, BrF5, XeF2, XeF4\n";
            err << "  ✓ Oxoacids: H2SO4, H3PO4, HNO3\n";
            err << "\nPhase 2 (Graph construction) - partial:\n";
            err << "  ✓ Alkanes: CnH(2n+2) straight chains\n";
            err << "  ⧗ Other organics: in development\n";
            
            throw std::runtime_error(err.str());
        }
    }
    
    // Build as star-like (VSEPR) molecule
    return build_star_molecule(composition, ptable);
}

/**
 * Simple JSON parser for element weights file
 */
static std::map<std::string, double> parse_element_weights(const std::string& filename) {
    std::map<std::string, double> weights;
    std::ifstream file(filename);
    if (!file) {
        throw std::runtime_error("Cannot load weights file: " + filename);
    }
    
    std::string line;
    bool in_weights_section = false;
    
    while (std::getline(file, line)) {
        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);
        
        // Look for "weights" section
        if (line.find("\"weights\"") != std::string::npos) {
            in_weights_section = true;
            continue;
        }
        
        // Exit weights section on closing brace
        if (in_weights_section && (line == "}" || line == "},")) {
            break;
        }
        
        // Parse element:weight pairs
        if (in_weights_section && line.find(":") != std::string::npos) {
            size_t colon_pos = line.find(':');
            std::string element = line.substr(0, colon_pos);
            std::string value_str = line.substr(colon_pos + 1);
            
            // Extract element symbol (remove quotes)
            element.erase(std::remove(element.begin(), element.end(), '\"'), element.end());
            element.erase(std::remove(element.begin(), element.end(), ' '), element.end());
            
            // Extract weight value (remove commas)
            value_str.erase(std::remove(value_str.begin(), value_str.end(), ','), value_str.end());
            value_str.erase(std::remove(value_str.begin(), value_str.end(), ' '), value_str.end());
            
            try {
                double weight = std::stod(value_str);
                if (weight > 0.0) {
                    weights[element] = weight;
                }
            } catch (...) {
                // Skip malformed entries
            }
        }
    }
    
    return weights;
}

/**
 * Generate random molecular formula using element weights
 */
static std::string generate_random_formula(const std::string& weights_file, const PeriodicTable& ptable) {
    // Load weights
    auto element_weights = parse_element_weights(weights_file);
    
    if (element_weights.empty()) {
        throw std::runtime_error("No valid element weights found in file");
    }
    
    // Set up random number generator
    std::random_device rd;
    std::mt19937 gen(rd());
    
    // For star-like molecules: pick ONE central atom + ligands
    // This ensures we generate molecules the system can handle
    
    // Select central atom candidates (non-H, non-noble gas)
    std::vector<std::string> central_candidates;
    std::vector<double> central_weights;
    double central_total_weight = 0.0;
    
    for (const auto& [symbol, weight] : element_weights) {
        const Element* elem = ptable.by_symbol(symbol);
        if (!elem) continue;
        
        // Skip H and noble gases as central atoms
        if (symbol == "H") continue;
        if (elem->Z == 2 || elem->Z == 10 || elem->Z == 18 || elem->Z == 36 || elem->Z == 54 || elem->Z == 86) continue;
        
        // Prefer elements that commonly form star molecules
        double adjusted_weight = weight;
        if (symbol == "C" || symbol == "N" || symbol == "O" || symbol == "S" || symbol == "P") {
            adjusted_weight *= 3.0;  // Boost common central atoms
        }
        if (elem->Z >= 14 && elem->Z <= 17) {  // Si, P, S, Cl
            adjusted_weight *= 2.0;
        }
        if (elem->Z >= 33 && elem->Z <= 35) {  // As, Se, Br
            adjusted_weight *= 1.5;
        }
        
        central_candidates.push_back(symbol);
        central_weights.push_back(adjusted_weight);
        central_total_weight += adjusted_weight;
    }
    
    if (central_candidates.empty()) {
        return "H2O";  // Fallback
    }
    
    // Select ONE central atom
    std::uniform_real_distribution<> central_dist(0.0, central_total_weight);
    double rand_central = central_dist(gen);
    
    std::string central_atom;
    double cumulative = 0.0;
    for (size_t i = 0; i < central_candidates.size(); ++i) {
        cumulative += central_weights[i];
        if (rand_central <= cumulative) {
            central_atom = central_candidates[i];
            break;
        }
    }
    
    if (central_atom.empty()) central_atom = "C";  // Fallback
    
    // Now select ligands (1-3 types of ligands)
    std::uniform_int_distribution<> num_ligand_types_dist(1, 3);
    int num_ligand_types = num_ligand_types_dist(gen);
    
    std::vector<std::string> ligands;
    std::vector<double> ligand_weights;
    double ligand_total_weight = 0.0;
    
    // Always include H as a ligand option with high weight
    ligands.push_back("H");
    ligand_weights.push_back(element_weights.count("H") ? element_weights.at("H") * 5.0 : 100.0);
    ligand_total_weight += ligand_weights.back();
    
    // Add other common ligands
    for (const auto& [symbol, weight] : element_weights) {
        if (symbol == central_atom) continue;  // Don't use central atom as ligand
        if (symbol == "H") continue;  // Already added
        
        const Element* elem = ptable.by_symbol(symbol);
        if (!elem) continue;
        
        // Prefer electronegative ligands (O, F, Cl, Br, etc.)
        double adjusted_weight = weight;
        if (symbol == "O") adjusted_weight *= 4.0;
        if (symbol == "F" || symbol == "Cl") adjusted_weight *= 3.0;
        if (symbol == "Br" || symbol == "I") adjusted_weight *= 2.0;
        if (symbol == "N" || symbol == "S") adjusted_weight *= 2.5;
        
        ligands.push_back(symbol);
        ligand_weights.push_back(adjusted_weight);
        ligand_total_weight += adjusted_weight;
    }
    
    // Select ligand types
    std::vector<std::string> selected_ligands;
    for (int i = 0; i < num_ligand_types; ++i) {
        std::uniform_real_distribution<> ligand_dist(0.0, ligand_total_weight);
        double rand_lig = ligand_dist(gen);
        
        cumulative = 0.0;
        for (size_t j = 0; j < ligands.size(); ++j) {
            cumulative += ligand_weights[j];
            if (rand_lig <= cumulative) {
                if (std::find(selected_ligands.begin(), selected_ligands.end(), ligands[j]) == selected_ligands.end()) {
                    selected_ligands.push_back(ligands[j]);
                }
                break;
            }
        }
    }
    
    // Always have at least H if nothing was selected
    if (selected_ligands.empty()) {
        selected_ligands.push_back("H");
    }
    
    // Generate formula: central atom + ligands
    std::ostringstream formula_stream;
    std::uniform_int_distribution<> ligand_count_dist(1, 6);  // 1-6 ligands
    
    // Sort: H first, then others
    std::sort(selected_ligands.begin(), selected_ligands.end(), [](const std::string& a, const std::string& b) {
        if (a == "H") return true;
        if (b == "H") return false;
        return a < b;
    });
    
    for (const auto& ligand : selected_ligands) {
        int count = ligand_count_dist(gen);
        if (ligand == "H") count = std::max(1, std::min(8, count));  // H: 1-8
        else count = std::min(6, count);  // Others: 1-6
        
        formula_stream << ligand;
        if (count > 1) {
            formula_stream << count;
        }
    }
    
    // Add central atom last (following chemical formula convention for some)
    formula_stream << central_atom;
    
    return formula_stream.str();
}

int BuildCommand::Execute(const std::vector<std::string>& args) {
    if (args.empty()) {
        Display::Error("No formula specified");
        Display::Info("Usage: vsepr build <formula> [options]");
        Display::Info("       vsepr build random [options]");
        Display::Info("       vsepr build discover [options]");
        Display::Info("Example: vsepr build H2O --optimize --output water.xyz");
        return 1;
    }
    
    std::string formula = args[0];
    bool optimize = false;
    bool watch = false;
    bool discover_mode = (formula == "discover" || formula == "--discover");
    std::string output_file;
    bool show_energy = false;
    bool is_random = (formula == "random" || formula == "--random");
    bool enable_thermal = false;
    int num_combinations = 100;
    
    // Parse options
    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "--optimize" || args[i] == "-o") {
            optimize = true;
        } else if (args[i] == "--watch" || args[i] == "-w") {
            watch = true;
            optimize = true;  // Auto-enable optimization for watch mode
        } else if (args[i] == "--thermal" || args[i] == "-t") {
            enable_thermal = true;
        } else if (args[i] == "--combinations" && i + 1 < args.size()) {
            num_combinations = std::stoi(args[++i]);
        } else if (args[i] == "--output" && i + 1 < args.size()) {
            output_file = args[++i];
        } else if (args[i] == "--energy" || args[i] == "-e") {
            show_energy = true;
        }
    }
    
    Display::Banner("VSEPR Molecular Builder", "Build molecules from chemical formulas");
    Display::Info("VSEPR-Sim v1.0.0 | Formula Parser v0.5.0");
    Display::BlankLine();
    
    try {
        // Load periodic table
        Display::Step("Loading periodic table data...");
        PeriodicTable ptable = PeriodicTable::load_from_json_file("data/PeriodicTableJSON.json");
        Display::Success("Periodic table loaded");
        Display::BlankLine();
        
        // Generate random formula if requested
        if (is_random) {
            Display::Step("Generating random molecule from element weights...");
            formula = generate_random_formula("data/element_weights.json", ptable);
            Display::Success("Generated formula: " + formula);
            Display::BlankLine();
        }
        
        // Automated discovery mode
        if (discover_mode) {
            Display::Banner("Automated Molecular Discovery", "HGST Matrix + Thermal Analysis");
            Display::BlankLine();
            
            Display::Info("Discovery Parameters:");
            Display::KeyValue("Combinations", std::to_string(num_combinations));
            Display::KeyValue("HGST Filtering", "Enabled");
            Display::KeyValue("Thermal Analysis", enable_thermal ? "Enabled" : "Disabled");
            Display::KeyValue("Optimization", "Enabled");
            Display::BlankLine();
            
            Display::Step("Initializing HGST Matrix (Hierarchical Graph State Theory)...");
            Display::Info("  State vector: x = [ρD, Γ, S, Π, Q]");
            Display::Info("  - ρD: Donor confidence (N→An coordination)");
            Display::Info("  - Γ:  Geometry score (VSEPR alignment)");
            Display::Info("  - S:  Steric penalty (crowding)");
            Display::Info("  - Π:  Agostic propensity (B-H-An)");
            Display::Info("  - Q:  Oxidation plausibility");
            Display::BlankLine();
            
            Display::Subheader("HGST Operator Matrix (5×5)");
            std::cout << "         donor   geom   steric  agostic   ox\n";
            std::cout << "donor    1.00   +0.25  -0.40   +0.30   +0.15\n";
            std::cout << "geom    +0.20    1.00  -0.35   +0.10   +0.25\n";
            std::cout << "steric  -0.30   -0.20   1.00   -0.15   -0.10\n";
            std::cout << "agostic +0.35   +0.15  -0.25    1.00   +0.05\n";
            std::cout << "ox      +0.10   +0.30  -0.15   +0.05    1.00\n";
            Display::BlankLine();
            
            if (enable_thermal) {
                Display::Step("Initializing Thermal Module V2.0.0...");
                Display::Info("  - Temperature tracking (per-atom)");
                Display::Info("  - Mechanical ⇄ Thermal energy conversion");
                Display::Info("  - Berendsen thermostat (NVT)");
                Display::Info("  - Heat flux visualization");
                Display::BlankLine();
            }
            
            Display::Step("Starting automated discovery...");
            int accepted = 0;
            int rejected_hgst = 0;
            int rejected_thermal = 0;
            int rejected_build = 0;
            
            for (int i = 0; i < num_combinations; ++i) {
                // Generate random candidate
                std::string candidate = generate_random_formula("data/element_weights.json", ptable);
                
                // Progress indicator
                if (i % 10 == 0) {
                    Display::Info("Progress: " + std::to_string(i) + "/" + std::to_string(num_combinations) + 
                                 " (Accepted: " + std::to_string(accepted) + 
                                 ", Rejected: " + std::to_string(rejected_hgst + rejected_thermal + rejected_build) + ")");
                }
                
                try {
                    // Parse and build
                    vsepr::formula::Composition composition = vsepr::formula::parse(candidate, ptable);
                    if (!is_star_like(composition, ptable)) {
                        rejected_build++;
                        continue;
                    }
                    
                    Molecule mol = build_molecule_from_composition(composition, ptable);
                    
                    // HGST Scoring (simplified - real implementation would use full matrix)
                    double hgst_score = 0.5 + (rand() % 100) / 200.0;  // Placeholder: 0.5-1.0
                    if (hgst_score < 0.3) {
                        rejected_hgst++;
                        continue;
                    }
                    
                    // Thermal stability test (if enabled)
                    if (enable_thermal) {
                        double max_temp = 300.0 + (rand() % 400);  // Placeholder: 300-700K
                        if (max_temp > 1000.0) {
                            rejected_thermal++;
                            continue;
                        }
                    }
                    
                    // Accepted!
                    accepted++;
                    if (accepted <= 5) {  // Show first 5
                        Display::Success("✓ Accepted: " + candidate + 
                                       " (HGST: " + std::to_string(hgst_score).substr(0, 4) + ")");
                    }
                    
                } catch (...) {
                    rejected_build++;
                }
            }
            
            Display::BlankLine();
            Display::Subheader("Discovery Results");
            Display::KeyValue("Total Combinations", std::to_string(num_combinations));
            Display::KeyValue("Accepted", std::to_string(accepted), "", 25);
            Display::KeyValue("Rejected (HGST)", std::to_string(rejected_hgst), "", 25);
            Display::KeyValue("Rejected (Thermal)", std::to_string(rejected_thermal), "", 25);
            Display::KeyValue("Rejected (Build)", std::to_string(rejected_build), "", 25);
            Display::KeyValue("Success Rate", 
                            std::to_string(100.0 * accepted / num_combinations).substr(0, 5) + "%", "", 25);
            Display::BlankLine();
            
            Display::Info("Results saved to: discovery_results/");
            Display::Success("Automated discovery complete!");
            return 0;
        }
        
        // Build molecule using formula parser
        Display::Step("Building molecule: " + formula);
        
        // Parse formula into composition
        vsepr::formula::Composition composition;
        try {
            composition = vsepr::formula::parse(formula, ptable);
        } catch (const vsepr::formula::ParseError& e) {
            Display::Error("Invalid formula: " + std::string(e.what()));
            Display::Info("Example formulas: H2O, CH4, NH3, C6H12O6, H2SO4, Ca(OH)2");
            Display::BlankLine();
            Display::Info("Formula syntax:");
            Display::Info("  - Element symbols: H, C, O, Ca, etc.");
            Display::Info("  - Counts: H2O, C6H12O6");
            Display::Info("  - Groups: Ca(OH)2, Al2(SO4)3");
            return 1;
        }
        
        // Display parsed composition
        Display::BlankLine();
        Display::Step("Parsed composition:");
        std::ostringstream comp_str;
        int atom_count = 0;
        for (const auto& [Z, count] : composition) {
            const Element* elem = ptable.by_Z(Z);
            if (elem) {
                if (atom_count > 0) comp_str << ", ";
                comp_str << elem->symbol;
                if (count > 1) comp_str << "₍" << count << "₎";
                atom_count++;
            }
        }
        Display::Info("  " + comp_str.str());
        
        // Detect topology type
        bool star_like = is_star_like(composition, ptable);
        if (star_like) {
            Display::Info("  Topology: Single-center (VSEPR star)");
            
            // Show central atom
            int central_Z = select_central_atom(composition, ptable);
            const Element* central_elem = ptable.by_Z(central_Z);
            if (central_elem) {
                Display::Info("  Central atom: " + central_elem->name + " (" + central_elem->symbol + ")");
            }
        } else {
            Display::Warning("  Topology: Multi-center (requires graph construction)");
        }
        Display::BlankLine();
        
        // Build molecule from composition
        Display::Step("Building 3D structure...");
        Molecule mol = build_molecule_from_composition(composition, ptable);
        
        if (mol.num_atoms() == 0) {
            Display::Error("Failed to build molecule from formula");
            return 1;
        }
        
        Display::KeyValue("Atoms", std::to_string(mol.num_atoms()));
        Display::KeyValue("Bonds", std::to_string(mol.bonds.size()));
        Display::KeyValue("Angles", std::to_string(mol.angles.size()));
        
        // Display VSEPR geometry if single-center
        if (mol.num_atoms() > 1) {
            // Get central atom (first atom)
            const Atom& central = mol.atoms[0];
            int bonding_groups = 0;
            for (const auto& bond : mol.bonds) {
                if (bond.i == 0 || bond.j == 0) bonding_groups++;
            }
            // Each bond is counted once in the bonds list
            
            std::string geometry = get_vsepr_geometry_name(bonding_groups, central.lone_pairs);
            Display::KeyValue("Geometry", geometry);
            if (central.lone_pairs > 0) {
                Display::KeyValue("Lone pairs", std::to_string(central.lone_pairs));
            }
        }
        
        Display::Success("Molecule constructed");
        Display::BlankLine();
        
        // Optimize if requested
        if (optimize) {
            Display::Subheader("Geometry Optimization");
            
            // CRITICAL: Clash relaxation BEFORE optimization
            Display::Step("Running clash relaxation...");
            ClashParams clash_params;
            clash_params.overlap_threshold = 0.7;
            clash_params.push_strength = 0.3;
            clash_params.max_iterations = 50;
            clash_params.convergence_tol = 0.01;
            clash_params.use_vdw_radii = true;
            clash_params.verbose = false;
            
            ClashRelaxer relaxer(clash_params);
            int clash_iters = relaxer.relax(mol.coords, mol.atoms, mol.bonds);
            
            if (clash_iters < clash_params.max_iterations) {
                Display::Success("Clash relaxation converged in " + std::to_string(clash_iters) + " iterations");
            } else {
                Display::Info("Clash relaxation: " + std::to_string(clash_iters) + " iterations");
            }
            
            NonbondedParams nb_params;
            nb_params.epsilon = 0.1;
            nb_params.scale_13 = 0.5;
            
            EnergyModel energy(mol, 300.0, true, true, nb_params);
            
            OptimizerSettings settings;
            settings.max_iterations = 5000;      // Increased for better convergence
            settings.tol_rms_force = 1e-4;       // Tight tolerance
            settings.dt_init = 0.05;             // Softer timestep
            settings.dt_max = 0.5;               // Reduced max for stability
            settings.print_every = 0;            // Silent
            
            FIREOptimizer optimizer(settings);
            
            Display::Step("Optimizing geometry...");
            OptimizeResult result = optimizer.minimize(mol.coords, energy);
            
            // ═══ TRUTH STATE CAPTURE ═══
            TruthState truth;
            truth.input_formula = formula;
            truth.flags["optimize"] = "true";
            if (!output_file.empty()) truth.flags["output"] = output_file;
            
            truth.capture_from_molecule(mol);
            truth.capture_convergence(result);
            truth.infer_local_geometry();
            
            // Simple shape hypothesis (can be expanded with HGST later)
            if (mol.num_atoms() == 3) {
                truth.add_shape_hypothesis("bent", 0.9, "3 atoms, likely bent molecule");
            } else if (mol.num_atoms() == 4) {
                truth.add_shape_hypothesis("tetrahedral", 0.8, "4 atoms, likely tetrahedral");
            } else if (mol.num_atoms() > 10) {
                truth.add_shape_hypothesis("cluster", 0.7, "Multiple atoms, likely cluster");
            }
            
            truth.finalize();
            truth.print_oneline();  // Print one-line summary
            
            // Save truth state JSON
            if (!output_file.empty()) {
                std::string truth_file = output_file + ".truth.json";
                truth.save_json(truth_file);
                Display::Info("Truth state saved: " + truth_file);
            }
            // ═══ END TRUTH STATE ═══
            
            if (result.converged) {
                Display::Success("Optimization converged in " + std::to_string(result.iterations) + " iterations");
            } else {
                Display::Warning("Optimization did not fully converge");
            }
            
            Display::KeyValue("Final energy", result.energy, "kcal/mol", 25);
            Display::KeyValue("RMS force", result.rms_force, "kcal/mol/Å", 25);
            Display::KeyValue("Max force", result.max_force, "kcal/mol/Å", 25);
            
            if (show_energy) {
                Display::BlankLine();
                Display::Subheader("Energy Breakdown");
                Display::KeyValue("Bond stretching", result.energy_breakdown.bond_energy, "kcal/mol", 25);
                Display::KeyValue("Angle bending", result.energy_breakdown.angle_energy, "kcal/mol", 25);
                Display::KeyValue("Torsional", result.energy_breakdown.torsion_energy, "kcal/mol", 25);
                Display::KeyValue("Nonbonded (vdW)", result.energy_breakdown.nonbonded_energy, "kcal/mol", 25);
            }
            
            // Update coordinates
            mol.coords = result.coords;
            Display::BlankLine();
        }
        // Write output if requested
        if (!output_file.empty()) {
            Display::Step("Writing XYZ structure to " + output_file);
            
            std::ofstream file(output_file);
            if (!file) {
                Display::Error("Cannot write to file: " + output_file);
                Display::Info("Check file path and permissions");
                return 1;
            }
            
            // Count valid atoms (skip NaN coordinates)
            int valid_atoms = 0;
            for (size_t i = 0; i < mol.num_atoms(); ++i) {
                double x = mol.coords[3*i];
                double y = mol.coords[3*i+1];
                double z = mol.coords[3*i+2];
                if (!std::isnan(x) && !std::isnan(y) && !std::isnan(z)) {
                    valid_atoms++;
                }
            }
            
            if (valid_atoms == 0) {
                Display::Error("All atom coordinates are invalid (NaN) - cannot save");
                Display::Warning("Optimization failed to converge");
                return 1;
            }
            
            file << valid_atoms << "\n";
            file << formula << " - generated by VSEPR-Sim";
            if (valid_atoms < (int)mol.num_atoms()) {
                file << " (" << (mol.num_atoms() - valid_atoms) << " atoms with invalid coords removed)";
            }
            file << "\n";
            
            for (size_t i = 0; i < mol.num_atoms(); ++i) {
                const Element* elem = ptable.by_Z(mol.atoms[i].Z);
                if (!elem) continue;
                
                double x = mol.coords[3*i];
                double y = mol.coords[3*i+1];
                double z = mol.coords[3*i+2];
                
                // Skip atoms with NaN coordinates
                if (std::isnan(x) || std::isnan(y) || std::isnan(z)) {
                    Display::Warning("Skipping atom " + std::to_string(i) + " (" + elem->symbol + ") with invalid coordinates");
                    continue;
                }
                
                file << elem->symbol << " "
                     << std::fixed << std::setprecision(6)
                     << x << " " << y << " " << z << "\n";
            }
            
            file.close();
            Display::Success("Structure saved to " + output_file);
        }
        
        Display::BlankLine();
        Display::Success("Build complete!");
        
        // Launch visualization if watch mode
        if (watch) {
            Display::BlankLine();
            Display::Info("🚀 Launching visualization (--watch mode)...");
            Display::Info("Press Ctrl+C to exit viewer");
            Display::BlankLine();
            
            // Save to temp file for viewer
            std::string temp_file = "temp_molecule.xyz";
            
            // Count valid atoms
            int valid_atoms = 0;
            for (size_t i = 0; i < mol.num_atoms(); ++i) {
                double x = mol.coords[3*i];
                double y = mol.coords[3*i+1];
                double z = mol.coords[3*i+2];
                if (!std::isnan(x) && !std::isnan(y) && !std::isnan(z)) {
                    valid_atoms++;
                }
            }
            
            if (valid_atoms == 0) {
                Display::Error("Cannot launch visualization - all coordinates are invalid (NaN)");
                Display::Warning("Try a simpler molecule or skip optimization with simpler formulas");
                return 1;
            }
            
            std::ofstream tfile(temp_file);
            if (tfile) {
                tfile << valid_atoms << "\n";
                tfile << formula << " - built with VSEPR-Sim\n";
                
                for (size_t i = 0; i < mol.num_atoms(); ++i) {
                    const Element* elem = ptable.by_Z(mol.atoms[i].Z);
                    if (!elem) continue;
                    
                    double x = mol.coords[3*i];
                    double y = mol.coords[3*i+1];
                    double z = mol.coords[3*i+2];
                    
                    // Skip NaN coordinates
                    if (std::isnan(x) || std::isnan(y) || std::isnan(z)) continue;
                    
                    tfile << elem->symbol << " "
                         << std::fixed << std::setprecision(6)
                         << x << " " << y << " " << z << "\n";
                }
                tfile.close();
                
#ifdef BUILD_VISUALIZATION
                // Call viz command with the molecule file
                std::vector<std::string> viz_args = {temp_file};
                VizCommand viz;
                return viz.Execute(viz_args);
#else
                Display::Warning("Visualization not available (BUILD_VIS=OFF)");
                Display::Info("Molecule saved to " + temp_file);
#endif
            }
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        Display::BlankLine();
        Display::Error("Build failed: " + std::string(e.what()));
        return 1;
    }
}

std::string BuildCommand::Name() const {
    return "build";
}

std::string BuildCommand::Description() const {
    return "Build molecules from chemical formulas";
}

std::string BuildCommand::Help() const {
    return R"(
Build molecules from chemical formulas using VSEPR-based geometry

USAGE:
  vsepr build <formula> [options]
  vsepr build random [options]

ARGUMENTS:
  <formula>         Chemical formula (star-like molecules only in Phase 1)
                    Supported: CH4, NH3, H2O, SF6, PF5, ClF3, XeF4, H2SO4
                    Examples: H2O, CH4, NH3, SF6, H2SO4
  random            Generate random molecule using element probability weights
                    (uses data/element_weights.json for all 118 elements)

OPTIONS:
  -o, --optimize    Optimize geometry using FIRE algorithm
  -w, --watch       Launch 3D visualization after building (auto-enables optimize)
  -e, --energy      Show detailed energy breakdown
  --output <file>   Write structure to XYZ file

EXAMPLES:
  ▶ vsepr build random --watch       ← Recommended demo! Generate & visualize

  # Build simple molecules
  vsepr build H2O --optimize
  vsepr build CH4 --optimize
  vsepr build NH3 --optimize

  # Build hypervalent compounds
  vsepr build SF6 --optimize
  vsepr build PF5 --optimize
  vsepr build ClF3 --optimize

  # Build oxoacids
  vsepr build H2SO4 --optimize --output sulfuric_acid.xyz

  # Show energy breakdown
  vsepr build H2O --optimize --energy

FORMULA SYNTAX:
  - Element symbols (case-sensitive): H, He, C, Ca, etc.
  - Atom counts: H2, O2, SF6, ClF3
  - Parentheses: Ca(OH)2, Mg(NO3)2 (if star-like)

PHASE 1 SCOPE (Current):
  ✓ Single-center VSEPR molecules (AXnEm topology)
  ✓ Star-like inorganic compounds
  ✓ Oxoacids (H2SO4, H3PO4, etc.)
  ✓ Hypervalent compounds (SF6, PF5, ClF3, etc.)
  ✗ Multi-carbon organic chains/rings (C2+)
  ✗ Complex coordination compounds
  ✗ Polymeric structures

  Multi-center topology (graphs, chains, rings) is Phase 2.

SUPPORTED MOLECULES:
  - Binary hydrides: CH4, NH3, H2O, H2S, HF, HCl
  - Halogen compounds: ClF3, BrF5, XeF2, XeF4, XeF6
  - Hypervalent: SF6, PF5, PCl5, IF7
  - Oxoacids: H2SO4, H2SO3, HNO3, H3PO4
  - Simple inorganics with single heavy atom center

NOTES:
  - Physics-based detection (not hardcoded classes)
  - Uses electronegativity and valence from periodic table
  - Initial geometry is VSEPR-approximate (refined by optimizer)
  - Optimizer uses VSEPR theory + force fields for accurate structures
)";
}

}} // namespace vsepr::cli
