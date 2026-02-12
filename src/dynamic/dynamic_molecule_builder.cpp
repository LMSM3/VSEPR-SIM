/**
 * dynamic_molecule_builder.cpp
 * =============================
 * Implementation of dynamic molecule generator with live .xyz export
 */

#include "dynamic/dynamic_molecule_builder.hpp"
#include <cmath>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <iostream>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace vsepr {
namespace dynamic {

// ============================================================================
// Element Mapper
// ============================================================================

ElementMapper::ElementMapper() {
    // Common elements mapped by first letter
    letter_map_['H'] = {1, "H", 1, 2.20};   // Hydrogen
    letter_map_['C'] = {6, "C", 4, 2.55};   // Carbon
    letter_map_['N'] = {7, "N", 3, 3.04};   // Nitrogen
    letter_map_['O'] = {8, "O", 2, 3.44};   // Oxygen
    letter_map_['F'] = {9, "F", 1, 3.98};   // Fluorine
    letter_map_['P'] = {15, "P", 5, 2.19};  // Phosphorus
    letter_map_['S'] = {16, "S", 6, 2.58};  // Sulfur
    letter_map_['K'] = {19, "K", 1, 0.82};  // Potassium
    letter_map_['V'] = {23, "V", 5, 1.63};  // Vanadium
    letter_map_['I'] = {53, "I", 1, 2.66};  // Iodine
    letter_map_['W'] = {74, "W", 6, 2.36};  // Tungsten
    letter_map_['U'] = {92, "U", 6, 1.38};  // Uranium
    
    // Rare-earth elements for crystal structures
    letter_map_['E'] = {58, "Ce", 3, 1.12}; // Cerium (E for cErium, Monazite)
    
    // Alkali and halogen elements for ionic crystals
    letter_map_['A'] = {11, "Na", 1, 0.93}; // Sodium (A for Alkali, Rock Salt)
    letter_map_['L'] = {17, "Cl", 1, 3.16}; // Chlorine (L for haLogen)
}

ElementSpec ElementMapper::get_by_letter(char letter) const {
    auto it = letter_map_.find(std::toupper(letter));
    if (it != letter_map_.end()) {
        return it->second;
    }
    // Default to carbon
    return {6, "C", 4, 2.55};
}

ElementSpec ElementMapper::get_random_common() const {
    static const std::vector<char> common = {'H', 'C', 'N', 'O', 'S'};
    char c = common[rand() % common.size()];
    return get_by_letter(c);
}

ElementSpec ElementMapper::from_fea_id(int fea_element_id) const {
    // Map FEA element ID modulo alphabet size
    char letter = 'A' + (fea_element_id % 26);
    return get_by_letter(letter);
}

// ============================================================================
// Dynamic Molecule Generator
// ============================================================================

DynamicMoleculeGenerator::DynamicMoleculeGenerator() 
    : rng_(std::random_device{}()),
      elem_db_(elements()) {
}

// Generate alkane: CₙH₂ₙ₊₂
Molecule DynamicMoleculeGenerator::generate_alkane(int n_carbons) {
    Molecule mol;
    
    if (n_carbons < 1) n_carbons = 1;
    if (n_carbons > 30) n_carbons = 30;  // Limit to C30
    
    // Build carbon chain
    for (int i = 0; i < n_carbons; ++i) {
        double x = i * 1.54;  // C-C bond length
        mol.add_atom(6, x, 0.0, 0.0);  // Carbon
        
        if (i > 0) {
            mol.add_bond(i - 1, i, 1);  // C-C single bond
        }
    }
    
    // Add hydrogens
    for (int i = 0; i < n_carbons; ++i) {
        int h_count = (i == 0 || i == n_carbons - 1) ? 3 : 2;  // CH3 or CH2
        
        for (int h = 0; h < h_count; ++h) {
            double angle = (h * 120.0 * M_PI / 180.0);
            double x = i * 1.54 + 1.09 * std::cos(angle);
            double y = 1.09 * std::sin(angle);
            double z = (h == 2) ? 1.09 : 0.0;
            
            mol.add_atom(1, x, y, z);  // Hydrogen
            size_t h_idx = mol.num_atoms() - 1;
            mol.add_bond(i, h_idx, 1);  // C-H bond
        }
    }
    
    mol.generate_angles_from_bonds();
    
    if (live_export_enabled_) {
        std::string formula = "C" + std::to_string(n_carbons) + 
                             "H" + std::to_string(2 * n_carbons + 2);
        export_current(mol, "Alkane: " + formula);
    }
    
    return mol;
}

// Generate alkene: CₙH₂ₙ (with C=C double bond)
Molecule DynamicMoleculeGenerator::generate_alkene(int n_carbons) {
    Molecule mol;
    
    if (n_carbons < 2) n_carbons = 2;
    if (n_carbons > 30) n_carbons = 30;
    
    // First two carbons with double bond
    mol.add_atom(6, 0.0, 0.0, 0.0);
    mol.add_atom(6, 1.34, 0.0, 0.0);  // C=C bond length (shorter)
    mol.add_bond(0, 1, 2);  // Double bond
    
    // Rest of chain
    for (int i = 2; i < n_carbons; ++i) {
        double x = 1.34 + (i - 1) * 1.54;
        mol.add_atom(6, x, 0.0, 0.0);
        mol.add_bond(i - 1, i, 1);
    }
    
    // Add hydrogens (simplified)
    int total_H = 2 * n_carbons;
    for (int h = 0; h < total_H; ++h) {
        int carbon = h / 2;
        if (carbon >= n_carbons) break;
        
        double x = (carbon == 0) ? 0.0 : (carbon == 1 ? 1.34 : 1.34 + (carbon - 1) * 1.54);
        double y = (h % 2 == 0) ? 1.09 : -1.09;
        
        mol.add_atom(1, x, y, 0.0);
        size_t h_idx = mol.num_atoms() - 1;
        mol.add_bond(carbon, h_idx, 1);
    }
    
    mol.generate_angles_from_bonds();
    
    if (live_export_enabled_) {
        std::string formula = "C" + std::to_string(n_carbons) + 
                             "H" + std::to_string(2 * n_carbons);
        export_current(mol, "Alkene: " + formula);
    }
    
    return mol;
}

// Generate alkyne: CₙH₂ₙ₋₂ (with C≡C triple bond)
Molecule DynamicMoleculeGenerator::generate_alkyne(int n_carbons) {
    Molecule mol;
    
    if (n_carbons < 2) n_carbons = 2;
    if (n_carbons > 30) n_carbons = 30;
    
    // First two carbons with triple bond
    mol.add_atom(6, 0.0, 0.0, 0.0);
    mol.add_atom(6, 1.20, 0.0, 0.0);  // C≡C bond length (even shorter)
    mol.add_bond(0, 1, 3);  // TRIPLE BOND!
    
    // Rest of chain
    for (int i = 2; i < n_carbons; ++i) {
        double x = 1.20 + (i - 1) * 1.54;
        mol.add_atom(6, x, 0.0, 0.0);
        mol.add_bond(i - 1, i, 1);
    }
    
    // Add hydrogens
    int total_H = 2 * n_carbons - 2;
    for (int h = 0; h < total_H; ++h) {
        int carbon = (h / 2) + 1;  // Start from C1
        if (carbon >= n_carbons) break;
        
        double x = (carbon == 1) ? 1.20 : 1.20 + (carbon - 1) * 1.54;
        double y = (h % 2 == 0) ? 1.09 : -1.09;
        
        mol.add_atom(1, x, y, 0.0);
        size_t h_idx = mol.num_atoms() - 1;
        mol.add_bond(carbon, h_idx, 1);
    }
    
    // Terminal hydrogens
    mol.add_atom(1, -1.09, 0.0, 0.0);
    mol.add_bond(0, mol.num_atoms() - 1, 1);
    
    mol.generate_angles_from_bonds();
    
    if (live_export_enabled_) {
        std::string formula = "C" + std::to_string(n_carbons) + 
                             "H" + std::to_string(2 * n_carbons - 2);
        export_current(mol, "Alkyne: " + formula + " (TRIPLE BOND)");
    }
    
    return mol;
}

// Generate alcohol: R-OH
Molecule DynamicMoleculeGenerator::generate_alcohol(int n_carbons) {
    Molecule mol = generate_alkane(n_carbons);
    
    // Replace one H with OH
    // Find a carbon with at least one H
    for (size_t i = 0; i < mol.num_bonds(); ++i) {
        const auto& bond = mol.bonds[i];
        if (mol.atoms[bond.j].Z == 1) {  // Found C-H
            // Remove this H (mark for removal)
            // Add OH group instead
            double x, y, z;
            mol.get_position(bond.i, x, y, z);
            
            mol.add_atom(8, x + 1.43, y, z);  // Oxygen
            size_t o_idx = mol.num_atoms() - 1;
            mol.add_atom(1, x + 2.39, y, z);  // Hydrogen on O
            size_t h_idx = mol.num_atoms() - 1;
            
            mol.add_bond(bond.i, o_idx, 1);  // C-O
            mol.add_bond(o_idx, h_idx, 1);   // O-H
            
            break;  // Only one OH for now
        }
    }
    
    mol.generate_angles_from_bonds();
    
    if (live_export_enabled_) {
        std::string formula = "C" + std::to_string(n_carbons) + 
                             "H" + std::to_string(2 * n_carbons + 1) + "OH";
        export_current(mol, "Alcohol: " + formula);
    }
    
    return mol;
}

// Generate from element letters
Molecule DynamicMoleculeGenerator::generate_from_letters(const std::string& element_letters) {
    Molecule mol;
    
    int atom_limit = std::min(101, (int)element_letters.length());
    
    for (int i = 0; i < atom_limit; ++i) {
        ElementSpec elem = mapper_.get_by_letter(element_letters[i]);
        
        // Place atoms in a spiral pattern
        double r = 1.5 * std::sqrt(i);
        double theta = i * M_PI * 0.618;  // Golden angle
        double x = r * std::cos(theta);
        double y = r * std::sin(theta);
        double z = i * 0.1;
        
        mol.add_atom(elem.Z, x, y, z);
        
        // Connect to previous atoms within bonding distance
        if (i > 0) {
            for (int j = std::max(0, i - 3); j < i; ++j) {
                double x1, y1, z1, x2, y2, z2;
                mol.get_position(j, x1, y1, z1);
                mol.get_position(i, x2, y2, z2);
                
                double dist = std::sqrt(std::pow(x2-x1, 2) + 
                                       std::pow(y2-y1, 2) + 
                                       std::pow(z2-z1, 2));
                
                if (dist < 2.0) {  // Bonding distance
                    mol.add_bond(j, i, 1);
                }
            }
        }
    }
    
    mol.generate_angles_from_bonds();
    
    if (live_export_enabled_) {
        export_current(mol, "Custom molecule from letters: " + element_letters);
    }
    
    return mol;
}

// Analyze molecule
DynamicMoleculeGenerator::AtomAnalysis 
DynamicMoleculeGenerator::analyze_molecule(const Molecule& mol) const {
    AtomAnalysis analysis;
    analysis.total_atoms = mol.num_atoms();
    analysis.total_bonds = mol.num_bonds();
    
    // Count atoms by type
    for (const auto& atom : mol.atoms) {
        analysis.atom_counts[atom.Z]++;
        if (analysis.atom_symbols.find(atom.Z) == analysis.atom_symbols.end()) {
            analysis.atom_symbols[atom.Z] = elem_db_.get_symbol(atom.Z);
        }
    }
    
    // Calculate average bond length
    double total_length = 0.0;
    for (const auto& bond : mol.bonds) {
        double x1, y1, z1, x2, y2, z2;
        mol.get_position(bond.i, x1, y1, z1);
        mol.get_position(bond.j, x2, y2, z2);
        
        double dist = std::sqrt(std::pow(x2-x1, 2) + 
                               std::pow(y2-y1, 2) + 
                               std::pow(z2-z1, 2));
        total_length += dist;
    }
    analysis.avg_bond_length = (mol.num_bonds() > 0) ? 
        total_length / mol.num_bonds() : 0.0;
    
    // Generate molecular formula
    std::ostringstream formula;
    for (const auto& [Z, count] : analysis.atom_counts) {
        formula << analysis.atom_symbols[Z];
        if (count > 1) formula << count;
    }
    analysis.molecular_formula = formula.str();
    
    return analysis;
}

void DynamicMoleculeGenerator::print_analysis(const AtomAnalysis& analysis) const {
    std::cout << "══════ Molecule Analysis ══════\n";
    std::cout << "Formula: " << analysis.molecular_formula << "\n";
    std::cout << "Total atoms: " << analysis.total_atoms << "\n";
    std::cout << "Total bonds: " << analysis.total_bonds << "\n";
    std::cout << "Avg bond length: " << std::fixed << std::setprecision(3) 
              << analysis.avg_bond_length << " Å\n";
    std::cout << "\nAtom breakdown:\n";
    for (const auto& [Z, count] : analysis.atom_counts) {
        std::cout << "  " << analysis.atom_symbols.at(Z) << ": " << count << "\n";
    }
    std::cout << "═══════════════════════════════\n";
}

// Live export
void DynamicMoleculeGenerator::enable_live_export(const std::string& xyz_path) {
    current_xyz_path_ = xyz_path;
    live_export_enabled_ = true;
}

void DynamicMoleculeGenerator::disable_live_export() {
    live_export_enabled_ = false;
    if (xyz_stream_.is_open()) {
        xyz_stream_.close();
    }
}

void DynamicMoleculeGenerator::export_current(const Molecule& mol, const std::string& comment) {
    if (!live_export_enabled_) return;
    
    std::ofstream out(current_xyz_path_);
    if (!out.is_open()) {
        std::cerr << "Failed to open " << current_xyz_path_ << " for writing\n";
        return;
    }
    
    // .xyz format:
    // Line 1: Number of atoms
    // Line 2: Comment
    // Lines 3+: Element X Y Z
    
    out << mol.num_atoms() << "\n";
    out << comment << "\n";
    
    for (size_t i = 0; i < mol.num_atoms(); ++i) {
        const auto& atom = mol.atoms[i];
        double x, y, z;
        mol.get_position(i, x, y, z);
        
        std::string symbol = elem_db_.get_symbol(atom.Z);
        out << std::setw(3) << symbol << " "
            << std::fixed << std::setprecision(6)
            << std::setw(12) << x << " "
            << std::setw(12) << y << " "
            << std::setw(12) << z << "\n";
    }
    
    out.close();
    
    std::cout << "[EXPORT] Wrote " << mol.num_atoms() << " atoms to " 
              << current_xyz_path_ << "\n";
}

// Generate simple molecule
Molecule DynamicMoleculeGenerator::generate_simple() {
    int type = rand() % 3;
    switch (type) {
        case 0: return generate_alkane(3 + rand() % 5);  // C3-C7
        case 1: return generate_alkene(3 + rand() % 5);
        case 2: return generate_alcohol(2 + rand() % 4);
        default: return generate_alkane(5);
    }
}

Molecule DynamicMoleculeGenerator::generate_medium() {
    int type = rand() % 3;
    switch (type) {
        case 0: return generate_alkane(8 + rand() % 8);   // C8-C15
        case 1: return generate_alkyne(6 + rand() % 6);
        case 2: return generate_alcohol(7 + rand() % 6);
        default: return generate_alkane(10);
    }
}

Molecule DynamicMoleculeGenerator::generate_complex() {
    int type = rand() % 2;
    switch (type) {
        case 0: return generate_alkane(16 + rand() % 15);  // C16-C30
        case 1: return generate_alkene(16 + rand() % 15);
        default: return generate_alkane(20);
    }
}

Molecule DynamicMoleculeGenerator::generate_very_complex() {
    // Generate large molecule with mixed elements
    std::string letters = "CCCCCHHHHHHNNNOOOSSPPP";  // Common elements
    for (int i = 0; i < 80; ++i) {
        letters += "C";  // Carbon backbone
    }
    return generate_from_letters(letters);
}

// ============================================================================
// Crystal Structure Generators
// ============================================================================

/**
 * Generate Monazite-Ce primitive unit cell (CePO₄)
 * Monoclinic system (P2₁/n), 6 atoms per unit cell
 */
Molecule DynamicMoleculeGenerator::generate_monazite_unit_cell() {
    Molecule mol;
    
    // Unit cell parameters for Monazite-Ce
    const double a = 6.788;  // Å
    const double b = 7.015;  // Å
    const double c = 6.471;  // Å
    const double beta = 103.67 * M_PI / 180.0;  // Monoclinic angle (radians)
    
    const double cos_beta = std::cos(beta);  // -0.2298
    const double sin_beta = std::sin(beta);  //  0.9732
    
    // Fractional coordinates (from crystallographic data)
    // Source: Ni et al. (1995), American Mineralogist 80, 21-26
    
    // Cerium (Ce³⁺) - 9-fold coordination
    double ce_frac[3] = {0.282, 0.159, 0.099};
    
    // Phosphorus (P⁵⁺) - tetrahedral center
    double p_frac[3] = {0.305, 0.165, 0.616};
    
    // Oxygen atoms (4 sites forming PO₄ tetrahedron)
    double o1_frac[3] = {0.250, 0.003, 0.445};  // Bridging
    double o2_frac[3] = {0.383, 0.332, 0.493};  // Tetrahedral vertex
    double o3_frac[3] = {0.474, 0.105, 0.812};  // Tetrahedral vertex
    double o4_frac[3] = {0.118, 0.220, 0.715};  // Tetrahedral vertex
    
    // Convert fractional to Cartesian coordinates
    auto frac_to_cart = [&](double xf, double yf, double zf) -> std::array<double, 3> {
        double x = xf * a + zf * c * cos_beta;
        double y = yf * b;
        double z = zf * c * sin_beta;
        return {x, y, z};
    };
    
    // Add Ce atom
    auto ce_pos = frac_to_cart(ce_frac[0], ce_frac[1], ce_frac[2]);
    mol.add_atom(58, ce_pos[0], ce_pos[1], ce_pos[2]);  // Ce (Z=58)
    
    // Add P atom
    auto p_pos = frac_to_cart(p_frac[0], p_frac[1], p_frac[2]);
    mol.add_atom(15, p_pos[0], p_pos[1], p_pos[2]);  // P (Z=15)
    
    // Add O atoms
    auto o1_pos = frac_to_cart(o1_frac[0], o1_frac[1], o1_frac[2]);
    mol.add_atom(8, o1_pos[0], o1_pos[1], o1_pos[2]);  // O1
    
    auto o2_pos = frac_to_cart(o2_frac[0], o2_frac[1], o2_frac[2]);
    mol.add_atom(8, o2_pos[0], o2_pos[1], o2_pos[2]);  // O2
    
    auto o3_pos = frac_to_cart(o3_frac[0], o3_frac[1], o3_frac[2]);
    mol.add_atom(8, o3_pos[0], o3_pos[1], o3_pos[2]);  // O3
    
    auto o4_pos = frac_to_cart(o4_frac[0], o4_frac[1], o4_frac[2]);
    mol.add_atom(8, o4_pos[0], o4_pos[1], o4_pos[2]);  // O4
    
    // Add bonds (P-O tetrahedral bonds)
    for (size_t i = 2; i < 6; ++i) {  // O atoms are indices 2-5
        mol.add_bond(1, i, 1);  // P (index 1) to each O
    }
    
    mol.generate_angles_from_bonds();
    
    std::cout << "[MONAZITE] Generated primitive unit cell: CePO₄ (6 atoms)\n";
    std::cout << "           Cell: a=" << a << " Å, b=" << b 
              << " Å, c=" << c << " Å, β=" << (beta * 180.0 / M_PI) << "°\n";
    
    return mol;
}

/**
 * Generate Monazite-Ce supercell (2×2×4 = 96 atoms)
 * Formula: Ce₁₆P₁₆O₆₄
 */
Molecule DynamicMoleculeGenerator::generate_monazite_supercell(int nx, int ny, int nz) {
    Molecule mol;
    
    // Unit cell parameters
    const double a = 6.788;  // Å
    const double b = 7.015;  // Å
    const double c = 6.471;  // Å
    const double beta = 103.67 * M_PI / 180.0;
    
    const double cos_beta = std::cos(beta);
    const double sin_beta = std::sin(beta);
    
    // Fractional coordinates
    double ce_frac[3] = {0.282, 0.159, 0.099};
    double p_frac[3] = {0.305, 0.165, 0.616};
    double o1_frac[3] = {0.250, 0.003, 0.445};
    double o2_frac[3] = {0.383, 0.332, 0.493};
    double o3_frac[3] = {0.474, 0.105, 0.812};
    double o4_frac[3] = {0.118, 0.220, 0.715};
    
    // Convert fractional to Cartesian
    auto frac_to_cart = [&](double xf, double yf, double zf, double dx, double dy, double dz) 
        -> std::array<double, 3> {
        double x = (xf + dx) * a + (zf + dz) * c * cos_beta;
        double y = (yf + dy) * b;
        double z = (zf + dz) * c * sin_beta;
        return {x, y, z};
    };
    
    std::cout << "[MONAZITE] Generating " << nx << "×" << ny << "×" << nz << " supercell...\n";
    
    // Replicate unit cell
    for (int ix = 0; ix < nx; ++ix) {
        for (int iy = 0; iy < ny; ++iy) {
            for (int iz = 0; iz < nz; ++iz) {
                // Translation for this cell
                double dx = static_cast<double>(ix);
                double dy = static_cast<double>(iy);
                double dz = static_cast<double>(iz);
                
                // Add Ce
                auto ce = frac_to_cart(ce_frac[0], ce_frac[1], ce_frac[2], dx, dy, dz);
                mol.add_atom(58, ce[0], ce[1], ce[2]);
                
                // Add P
                auto p = frac_to_cart(p_frac[0], p_frac[1], p_frac[2], dx, dy, dz);
                mol.add_atom(15, p[0], p[1], p[2]);
                
                // Add O atoms
                auto o1 = frac_to_cart(o1_frac[0], o1_frac[1], o1_frac[2], dx, dy, dz);
                mol.add_atom(8, o1[0], o1[1], o1[2]);
                
                auto o2 = frac_to_cart(o2_frac[0], o2_frac[1], o2_frac[2], dx, dy, dz);
                mol.add_atom(8, o2[0], o2[1], o2[2]);
                
                auto o3 = frac_to_cart(o3_frac[0], o3_frac[1], o3_frac[2], dx, dy, dz);
                mol.add_atom(8, o3[0], o3[1], o3[2]);
                
                auto o4 = frac_to_cart(o4_frac[0], o4_frac[1], o4_frac[2], dx, dy, dz);
                mol.add_atom(8, o4[0], o4[1], o4[2]);
            }
        }
    }
    
    int total_cells = nx * ny * nz;
    int expected_atoms = total_cells * 6;
    
    std::cout << "           Generated " << mol.num_atoms() << " atoms";
    std::cout << " (expected " << expected_atoms << ")\n";
    
    // Detect bonds
    std::cout << "           Detecting P-O and Ce-O bonds...\n";
    
    int p_o_bonds = 0;
    int ce_o_bonds = 0;
    
    // Add bonds based on distance criteria
    for (size_t i = 0; i < mol.num_atoms(); ++i) {
        for (size_t j = i + 1; j < mol.num_atoms(); ++j) {
            double xi, yi, zi, xj, yj, zj;
            mol.get_position(i, xi, yi, zi);
            mol.get_position(j, xj, yj, zj);
            
            double dx = xj - xi;
            double dy = yj - yi;
            double dz = zj - zi;
            double dist = std::sqrt(dx*dx + dy*dy + dz*dz);
            
            uint8_t Zi = mol.atoms[i].Z;
            uint8_t Zj = mol.atoms[j].Z;
            
            // P-O tetrahedral bonds (1.50-1.58 Å)
            if ((Zi == 15 && Zj == 8) || (Zi == 8 && Zj == 15)) {
                if (dist >= 1.50 && dist <= 1.58) {
                    mol.add_bond(i, j, 1);
                    p_o_bonds++;
                }
            }
            
            // Ce-O coordination bonds (2.35-2.65 Å)
            if ((Zi == 58 && Zj == 8) || (Zi == 8 && Zj == 58)) {
                if (dist >= 2.35 && dist <= 2.65) {
                    mol.add_bond(i, j, 1);
                    ce_o_bonds++;
                }
            }
        }
    }
    
    mol.generate_angles_from_bonds();
    
    std::cout << "           Detected " << p_o_bonds << " P-O bonds";
    std::cout << " and " << ce_o_bonds << " Ce-O bonds\n";
    std::cout << "           Total bonds: " << mol.num_bonds() << "\n";
    
    // Supercell dimensions
    double sx = nx * a;
    double sy = ny * b;
    double sz = nz * c * sin_beta;
    
    std::cout << "           Supercell dimensions: " 
              << std::fixed << std::setprecision(2)
              << sx << " × " << sy << " × " << sz << " Å³\n";
    
    if (live_export_enabled_) {
        std::stringstream comment;
        comment << "Monazite-Ce supercell (" << nx << "x" << ny << "x" << nz 
                << "): Ce" << (total_cells) << "P" << (total_cells) 
                << "O" << (total_cells * 4);
        export_current(mol, comment.str());
    }
    
    return mol;
}

// ============================================================================
// Rock Salt (NaCl) Crystal Structure Generator
// ============================================================================

/**
 * Generate Rock Salt (NaCl) unit cell
 * Cubic system (Fm3̄m), face-centered cubic
 * 8 atoms per conventional cubic cell (4 Na + 4 Cl)
 */
Molecule DynamicMoleculeGenerator::generate_rocksalt_unit_cell() {
    Molecule mol;
    
    // Unit cell parameter for NaCl (rock salt)
    const double a = 5.640;  // Å (lattice constant)
    
    std::cout << "[ROCKSALT] Generating primitive unit cell: NaCl\n";
    std::cout << "            Cell: a=" << a << " Å (cubic)\n";
    
    // FCC lattice: Place Na at (0,0,0) and face centers
    // Place Cl at edge centers to form interpenetrating FCC lattices
    
    // Na atoms (FCC positions)
    mol.add_atom(11, 0.0, 0.0, 0.0);                    // Corner
    mol.add_atom(11, a/2, a/2, 0.0);                    // Face center (z=0)
    mol.add_atom(11, a/2, 0.0, a/2);                    // Face center (y=0)
    mol.add_atom(11, 0.0, a/2, a/2);                    // Face center (x=0)
    
    // Cl atoms (displaced FCC positions)
    mol.add_atom(17, a/2, 0.0, 0.0);                    // Edge center
    mol.add_atom(17, 0.0, a/2, 0.0);                    // Edge center
    mol.add_atom(17, 0.0, 0.0, a/2);                    // Edge center
    mol.add_atom(17, a/2, a/2, a/2);                    // Body center
    
    // Add ionic bonds (Na-Cl, 6-fold coordination)
    // Distance: a/2 = 2.82 Å
    double bond_dist = a / 2.0;
    double tolerance = 0.1;  // Allow slight variation
    
    for (size_t i = 0; i < mol.num_atoms(); ++i) {
        for (size_t j = i + 1; j < mol.num_atoms(); ++j) {
            double xi, yi, zi, xj, yj, zj;
            mol.get_position(i, xi, yi, zi);
            mol.get_position(j, xj, yj, zj);
            
            double dx = xj - xi;
            double dy = yj - yi;
            double dz = zj - zi;
            double dist = std::sqrt(dx*dx + dy*dy + dz*dz);
            
            uint8_t Zi = mol.atoms[i].Z;
            uint8_t Zj = mol.atoms[j].Z;
            
            // Only bond Na-Cl (not Na-Na or Cl-Cl)
            if ((Zi == 11 && Zj == 17) || (Zi == 17 && Zj == 11)) {
                if (std::abs(dist - bond_dist) < tolerance) {
                    mol.add_bond(i, j, 1);  // Ionic bond
                }
            }
        }
    }
    
    mol.generate_angles_from_bonds();
    
    std::cout << "[ROCKSALT] Generated unit cell with " << mol.num_atoms() 
              << " atoms, " << mol.num_bonds() << " bonds\n";
    
    return mol;
}

/**
 * Generate Rock Salt (NaCl) supercell
 * Default: 5×5×4 = 100 unit cells → 100 atoms (50 Na + 50 Cl)
 * Formula: Na₅₀Cl₅₀
 */
Molecule DynamicMoleculeGenerator::generate_rocksalt_supercell(int nx, int ny, int nz) {
    Molecule mol;
    
    // Unit cell parameter
    const double a = 5.640;  // Å
    
    std::cout << "[ROCKSALT] Generating " << nx << "×" << ny << "×" << nz << " supercell...\n";
    std::cout << "            Lattice constant: a=" << a << " Å\n";
    
    // Generate simple cubic lattice with alternating Na/Cl
    // Rock salt = two interpenetrating FCC lattices
    
    // For simplicity, use simple cubic with alternating atoms
    // Total atoms: nx × ny × nz × 2 (one Na, one Cl per cell)
    
    int atom_count = 0;
    int na_count = 0;
    int cl_count = 0;
    
    // Generate atoms at lattice sites
    for (int ix = 0; ix < nx; ++ix) {
        for (int iy = 0; iy < ny; ++iy) {
            for (int iz = 0; iz < nz; ++iz) {
                double x = ix * a;
                double y = iy * a;
                double z = iz * a;
                
                // Alternate Na and Cl based on parity
                if ((ix + iy + iz) % 2 == 0) {
                    mol.add_atom(11, x, y, z);  // Na
                    na_count++;
                } else {
                    mol.add_atom(17, x, y, z);  // Cl
                    cl_count++;
                }
                atom_count++;
            }
        }
    }
    
    std::cout << "            Generated " << atom_count << " atoms ";
    std::cout << "(" << na_count << " Na + " << cl_count << " Cl)\n";
    
    // Detect ionic bonds (Na-Cl nearest neighbors)
    std::cout << "            Detecting Na-Cl ionic bonds...\n";
    
    double bond_dist = a;  // Nearest neighbor distance in simple cubic
    double tolerance = 0.2;
    int bond_count = 0;
    
    for (size_t i = 0; i < mol.num_atoms(); ++i) {
        for (size_t j = i + 1; j < mol.num_atoms(); ++j) {
            double xi, yi, zi, xj, yj, zj;
            mol.get_position(i, xi, yi, zi);
            mol.get_position(j, xj, yj, zj);
            
            double dx = xj - xi;
            double dy = yj - yi;
            double dz = zj - zi;
            double dist = std::sqrt(dx*dx + dy*dy + dz*dz);
            
            uint8_t Zi = mol.atoms[i].Z;
            uint8_t Zj = mol.atoms[j].Z;
            
            // Only bond Na-Cl (ionic)
            if ((Zi == 11 && Zj == 17) || (Zi == 17 && Zj == 11)) {
                if (std::abs(dist - bond_dist) < tolerance) {
                    mol.add_bond(i, j, 1);
                    bond_count++;
                }
            }
        }
    }
    
    mol.generate_angles_from_bonds();
    
    std::cout << "            Detected " << bond_count << " Na-Cl bonds\n";
    std::cout << "            Total bonds: " << mol.num_bonds() << "\n";
    
    // Supercell dimensions
    double sx = nx * a;
    double sy = ny * a;
    double sz = nz * a;
    
    std::cout << "            Supercell dimensions: " 
              << std::fixed << std::setprecision(2)
              << sx << " × " << sy << " × " << sz << " Å³\n";
    
    if (live_export_enabled_) {
        std::stringstream comment;
        comment << "Rock Salt (NaCl) supercell (" << nx << "x" << ny << "x" << nz 
                << "): Na" << na_count << "Cl" << cl_count;
        export_current(mol, comment.str());
    }
    
    return mol;
}

} // namespace dynamic
} // namespace vsepr
