#pragma once
/**
 * graph_builder.hpp
 * -----------------
 * Graph-based molecular topology construction (Phase 2)
 * 
 * Builds bond networks from chemical composition without assuming
 * a single central atom. Handles organic chains, coordination complexes,
 * and extended structures.
 * 
 * Key features:
 * - Valence-constrained bond assignment
 * - Organic chain/ring detection
 * - Graph-based geometry placement
 * - Derived angles/torsions from adjacency
 */

#include "molecule.hpp"
#include "pot/periodic_db.hpp"
#include <vector>
#include <map>
#include <algorithm>
#include <queue>
#include <cmath>

namespace vsepr {

/**
 * Valence rules for an element
 */
struct ValenceRules {
    int typical_valence;      // Most common valence
    int max_valence;          // Maximum coordination
    std::vector<int> allowed; // All allowed valences (e.g., S: 2, 4, 6)
    double covalent_radius;   // For bond length estimation (Angstroms)
    
    ValenceRules() 
        : typical_valence(0), max_valence(0), covalent_radius(1.0) {}
};

/**
 * Get valence rules for an element from periodic table
 * Hardcoded chemistry knowledge (organic elements focus)
 */
inline ValenceRules get_valence_rules(int Z, const PeriodicTable& ptable) {
    ValenceRules rules;
    
    const Element* elem = ptable.by_Z(Z);
    if (!elem) return rules;
    
    uint8_t valence_e = elem->valence_electrons();
    
    // Element-specific rules (common chemistry)
    switch (Z) {
        case 1:  // H
            rules.typical_valence = 1;
            rules.max_valence = 1;
            rules.allowed = {1};
            rules.covalent_radius = 0.31;
            break;
        case 6:  // C
            rules.typical_valence = 4;
            rules.max_valence = 4;
            rules.allowed = {4};  // sp3, sp2, sp all use 4 bonds
            rules.covalent_radius = 0.76;
            break;
        case 7:  // N
            rules.typical_valence = 3;
            rules.max_valence = 4;
            rules.allowed = {3, 4};  // NH3 (3), NH4+ (4)
            rules.covalent_radius = 0.71;
            break;
        case 8:  // O
            rules.typical_valence = 2;
            rules.max_valence = 2;
            rules.allowed = {2};
            rules.covalent_radius = 0.66;
            break;
        case 9:  // F
            rules.typical_valence = 1;
            rules.max_valence = 1;
            rules.allowed = {1};
            rules.covalent_radius = 0.57;
            break;
        case 15: // P
            rules.typical_valence = 3;
            rules.max_valence = 5;
            rules.allowed = {3, 5};  // PH3, PF5
            rules.covalent_radius = 1.07;
            break;
        case 16: // S
            rules.typical_valence = 2;
            rules.max_valence = 6;
            rules.allowed = {2, 4, 6};  // H2S, SF4, SF6
            rules.covalent_radius = 1.05;
            break;
        case 17: // Cl
            rules.typical_valence = 1;
            rules.max_valence = 7;
            rules.allowed = {1, 3, 5, 7};  // HCl, ClF3, ClF5
            rules.covalent_radius = 1.02;
            break;
        default:
            // Generic fallback: use valence electrons
            rules.typical_valence = std::min(4, (int)valence_e);
            rules.max_valence = std::max(4, (int)valence_e);
            rules.allowed = {rules.typical_valence};
            rules.covalent_radius = 1.5;  // Generic estimate
            break;
    }
    
    return rules;
}

/**
 * Build straight-chain alkane (C_nH_(2n+2))
 * Canonical n-alkane: CH3-(CH2)_(n-2)-CH3
 */
inline Molecule build_alkane_chain(int num_carbons, const PeriodicTable& ptable) {
    if (num_carbons < 1) {
        throw std::runtime_error("Invalid alkane: need at least 1 carbon");
    }
    
    Molecule mol;
    
    // Add carbons in a chain along x-axis
    double C_C_bond = 1.54;  // C-C single bond (Angstroms)
    for (int i = 0; i < num_carbons; ++i) {
        mol.add_atom(6, i * C_C_bond, 0.0, 0.0);
    }
    
    // Bond carbons together
    for (int i = 0; i < num_carbons - 1; ++i) {
        mol.add_bond(i, i + 1, 1);  // C-C single bond
    }
    
    // Add hydrogens (each C needs 4 bonds total)
    double C_H_bond = 1.09;  // C-H bond length
    
    for (int i = 0; i < num_carbons; ++i) {
        double x_c = i * C_C_bond;
        
        // Count existing C-C bonds for this carbon
        int num_cc_bonds = 0;
        if (i > 0) num_cc_bonds++;  // Bond to previous C
        if (i < num_carbons - 1) num_cc_bonds++;  // Bond to next C
        
        int num_h_needed = 4 - num_cc_bonds;
        
        // Place H atoms around carbon in tetrahedral-ish positions
        for (int j = 0; j < num_h_needed; ++j) {
            double angle = j * (2.0 * M_PI / num_h_needed);
            double y = C_H_bond * std::cos(angle);
            double z = C_H_bond * std::sin(angle);
            
            // Offset from chain direction
            if (i == 0) {
                // First carbon: place H towards -x
                mol.add_atom(1, x_c - C_H_bond * 0.5, y, z);
            } else if (i == num_carbons - 1) {
                // Last carbon: place H towards +x
                mol.add_atom(1, x_c + C_H_bond * 0.5, y, z);
            } else {
                // Middle carbons: place H perpendicular to chain
                mol.add_atom(1, x_c, y, z);
            }
            
            mol.add_bond(i, mol.num_atoms() - 1, 1);  // C-H bond
        }
    }
    
    // Generate angles and torsions from connectivity
    mol.generate_angles_from_bonds();
    mol.generate_torsions_from_bonds();
    
    return mol;
}

/**
 * Detect if formula is a simple alkane (C_nH_(2n+2))
 */
inline bool is_alkane_formula(const std::map<int, int>& composition) {
    if (composition.size() != 2) return false;
    if (composition.count(6) == 0 || composition.count(1) == 0) return false;
    
    int num_C = composition.at(6);
    int num_H = composition.at(1);
    
    // Check alkane formula: H = 2*C + 2
    return (num_H == 2 * num_C + 2);
}

/**
 * Detect if formula is an alkene (C_nH_(2n))
 */
inline bool is_alkene_formula(const std::map<int, int>& composition) {
    if (composition.size() != 2) return false;
    if (composition.count(6) == 0 || composition.count(1) == 0) return false;
    
    int num_C = composition.at(6);
    int num_H = composition.at(1);
    
    return (num_H == 2 * num_C);
}

/**
 * Build molecule from composition using graph construction
 * Handles multi-center topologies
 */
inline Molecule build_molecule_from_graph(
    const std::map<int, int>& composition,
    const PeriodicTable& ptable
) {
    // Alkane detection (C_nH_(2n+2))
    if (is_alkane_formula(composition)) {
        int num_carbons = composition.at(6);
        return build_alkane_chain(num_carbons, ptable);
    }
    
    // Alkene detection (C_nH_(2n)) - future
    if (is_alkene_formula(composition)) {
        throw std::runtime_error(
            "Alkenes detected but not yet implemented.\n"
            "Try an alkane (CnH(2n+2)) like C2H6, C3H8, C4H10, etc."
        );
    }
    
    // Generic graph builder (future)
    throw std::runtime_error(
        "Generic graph-based topology not yet implemented.\n"
        "Phase 2.1: Currently supports alkanes only (CnH(2n+2))"
    );
}

} // namespace vsepr
