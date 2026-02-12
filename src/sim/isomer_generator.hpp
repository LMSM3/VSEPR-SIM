#pragma once
/**
 * isomer_generator.hpp - Systematic Isomer Enumeration
 * 
 * Generates all symmetry-distinct isomers for a given molecular formula:
 * 1. Geometric isomers: cis/trans, fac/mer in coordination complexes
 * 2. Conformational isomers: torsional rotamers
 * 3. Constitutional isomers: different bonding patterns (future)
 * 
 * Key features:
 * - Early rejection of symmetry-redundant variants
 * - Coordination-aware geometry templates
 * - Donor atom detection for ligands
 */

#include "molecule.hpp"
#include "isomer_signature.hpp"
#include "../core/geom_ops.hpp"
#include "../core/element_data_integrated.hpp"
#include <vector>
#include <algorithm>
#include <cmath>
#include <set>
#include <iostream>

namespace vsepr {

//=============================================================================
// Variant Type Classification
//=============================================================================

enum class VariantType {
    CONFORMER,        // Torsional rotation (same connectivity, different angles)
    GEOMETRIC_ISOMER, // Coordination geometry (cis/trans, fac/mer)
    CONSTITUTIONAL    // Different bonding pattern (future)
};

//=============================================================================
// Coordination Geometry Templates
//=============================================================================

/**
 * Standard coordination geometries with canonical ligand positions.
 * Each position is a unit vector from metal center.
 */
struct CoordinationGeometry {
    std::string name;
    uint32_t coordination_number;
    std::vector<Vec3> positions; // Unit vectors
};

// Octahedral (CN=6): vertices of regular octahedron
inline CoordinationGeometry octahedral_geometry() {
    return {
        "octahedral", 6,
        {
            Vec3(1, 0, 0), Vec3(-1, 0, 0),  // trans pair along x
            Vec3(0, 1, 0), Vec3(0, -1, 0),  // trans pair along y
            Vec3(0, 0, 1), Vec3(0, 0, -1)   // trans pair along z
        }
    };
}

// Square planar (CN=4): coplanar square
inline CoordinationGeometry square_planar_geometry() {
    return {
        "square_planar", 4,
        {
            Vec3(1, 0, 0), Vec3(-1, 0, 0),
            Vec3(0, 1, 0), Vec3(0, -1, 0)
        }
    };
}

// Tetrahedral (CN=4): vertices of regular tetrahedron
inline CoordinationGeometry tetrahedral_geometry() {
    const double a = 1.0 / std::sqrt(3.0);
    return {
        "tetrahedral", 4,
        {
            Vec3(a, a, a),
            Vec3(a, -a, -a),
            Vec3(-a, a, -a),
            Vec3(-a, -a, a)
        }
    };
}

// Trigonal bipyramidal (CN=5)
inline CoordinationGeometry trigonal_bipyramidal_geometry() {
    const double cos120 = -0.5;
    const double sin120 = std::sqrt(3.0) / 2.0;
    return {
        "trigonal_bipyramidal", 5,
        {
            Vec3(0, 0, 1),           // axial
            Vec3(0, 0, -1),          // axial
            Vec3(1, 0, 0),           // equatorial
            Vec3(cos120, sin120, 0), // equatorial
            Vec3(cos120, -sin120, 0) // equatorial
        }
    };
}

// Square pyramidal (CN=5)
inline CoordinationGeometry square_pyramidal_geometry() {
    return {
        "square_pyramidal", 5,
        {
            Vec3(0, 0, 1),      // apical
            Vec3(1, 0, 0),      // basal
            Vec3(-1, 0, 0),     // basal
            Vec3(0, 1, 0),      // basal
            Vec3(0, -1, 0)      // basal
        }
    };
}

//=============================================================================
// Ligand Assignment Enumeration
//=============================================================================

// Forward declaration
inline std::string generate_descriptor(
    const std::vector<uint32_t>& assignment,
    const CoordinationGeometry& geom);

/**
 * Given ligand types and counts, enumerate all symmetry-distinct assignments
 * to coordination positions.
 * 
 * Example: [MA4B2] octahedral → cis and trans isomers
 * - cis: B ligands adjacent (90° angle)
 * - trans: B ligands opposite (180° angle)
 */
struct LigandAssignment {
    std::vector<uint32_t> position_types; // ligand_type_idx for each position
    std::string descriptor; // "cis", "trans", "fac", "mer", etc.
};

/**
 * Check if two assignments are equivalent under point group symmetry.
 * Uses angular pattern comparison from isomer signature.
 */
inline bool are_symmetry_equivalent(
    const std::vector<uint32_t>& assign1,
    const std::vector<uint32_t>& assign2,
    const CoordinationGeometry& geom)
{
    if (assign1.size() != assign2.size()) return false;
    
    const uint32_t n = assign1.size();
    
    // Compute angular patterns for both
    auto compute_pattern = [&](const std::vector<uint32_t>& assign) {
        std::vector<std::tuple<uint32_t, uint32_t, int>> pattern;
        
        for (uint32_t i = 0; i < n; ++i) {
            for (uint32_t j = i + 1; j < n; ++j) {
                // Angle between positions i and j
                double dot = geom.positions[i].dot(geom.positions[j]);
                dot = std::max(-1.0, std::min(1.0, dot));
                double angle_deg = std::acos(dot) * 180.0 / M_PI;
                int angle_bin = static_cast<int>(std::round(angle_deg / 30.0));
                
                uint32_t t1 = assign[i];
                uint32_t t2 = assign[j];
                if (t1 > t2) std::swap(t1, t2);
                
                pattern.push_back({t1, t2, angle_bin});
            }
        }
        
        std::sort(pattern.begin(), pattern.end());
        return pattern;
    };
    
    return compute_pattern(assign1) == compute_pattern(assign2);
}

/**
 * Enumerate all unique ligand assignments for given geometry and ligand types.
 * 
 * ligand_counts[i] = number of ligands of type i
 * Returns list of symmetry-distinct assignments.
 */
inline std::vector<LigandAssignment> enumerate_ligand_assignments(
    const CoordinationGeometry& geom,
    const std::vector<uint32_t>& ligand_counts)
{
    const uint32_t n_pos = geom.coordination_number;
    const uint32_t n_types = ligand_counts.size();
    
    // Build flat list of ligand type indices
    std::vector<uint32_t> ligands;
    for (uint32_t type = 0; type < n_types; ++type) {
        for (uint32_t count = 0; count < ligand_counts[type]; ++count) {
            ligands.push_back(type);
        }
    }
    
    if (ligands.size() != n_pos) {
        std::cerr << "Warning: ligand count mismatch in enumerate_ligand_assignments\n";
        return {};
    }
    
    // Generate all permutations, filter for symmetry uniqueness
    std::vector<LigandAssignment> unique_assignments;
    std::set<std::vector<uint32_t>> seen; // Track canonical patterns
    
    std::sort(ligands.begin(), ligands.end());
    
    do {
        // Check if this assignment is symmetry-equivalent to any previous one
        bool is_new = true;
        for (const auto& prev : unique_assignments) {
            if (are_symmetry_equivalent(ligands, prev.position_types, geom)) {
                is_new = false;
                break;
            }
        }
        
        if (is_new) {
            LigandAssignment assignment;
            assignment.position_types = ligands;
            assignment.descriptor = generate_descriptor(ligands, geom);
            unique_assignments.push_back(assignment);
        }
        
    } while (std::next_permutation(ligands.begin(), ligands.end()));
    
    return unique_assignments;
}

/**
 * Generate human-readable descriptor (cis, trans, fac, mer, etc.)
 * based on ligand positions and geometry.
 */
inline std::string generate_descriptor(
    const std::vector<uint32_t>& assignment,
    const CoordinationGeometry& geom)
{
    // Count ligand types
    std::map<uint32_t, uint32_t> type_counts;
    for (uint32_t t : assignment) {
        type_counts[t]++;
    }
    
    // Special cases for common isomerism patterns
    if (geom.name == "octahedral" && type_counts.size() == 2) {
        // MA4B2 or MA3B3
        auto it = type_counts.begin();
        uint32_t type_A = it->first;
        uint32_t count_A = it->second;
        ++it;
        uint32_t type_B = it->first;
        uint32_t count_B = it->second;
        
        if (count_A == 4 && count_B == 2) {
            // MA4B2: check if B ligands are trans or cis
            std::vector<uint32_t> b_positions;
            for (uint32_t i = 0; i < assignment.size(); ++i) {
                if (assignment[i] == type_B) {
                    b_positions.push_back(i);
                }
            }
            
            if (b_positions.size() == 2) {
                double dot = geom.positions[b_positions[0]].dot(geom.positions[b_positions[1]]);
                if (dot < -0.9) return "trans"; // ~180°
                else return "cis"; // ~90°
            }
        }
        else if (count_A == 3 && count_B == 3) {
            // MA3B3: check if fac or mer
            std::vector<uint32_t> a_positions;
            for (uint32_t i = 0; i < assignment.size(); ++i) {
                if (assignment[i] == type_A) {
                    a_positions.push_back(i);
                }
            }
            
            // fac: all three on same face (mutual 90° angles)
            // mer: meridional (one trans pair among the three)
            if (a_positions.size() == 3) {
                bool has_trans_pair = false;
                for (uint32_t i = 0; i < 3; ++i) {
                    for (uint32_t j = i + 1; j < 3; ++j) {
                        double dot = geom.positions[a_positions[i]].dot(geom.positions[a_positions[j]]);
                        if (dot < -0.9) {
                            has_trans_pair = true;
                            break;
                        }
                    }
                }
                return has_trans_pair ? "mer" : "fac";
            }
        }
    }
    
    // Default: just count pattern
    std::ostringstream oss;
    bool first = true;
    for (auto [type, count] : type_counts) {
        if (!first) oss << "-";
        oss << "T" << type << "x" << count;
        first = false;
    }
    return oss.str();
}

//=============================================================================
// Isomer Variant Generator
//=============================================================================

/**
 * Generate molecular structures for all isomers of a coordination complex.
 * 
 * Input:
 * - metal_Z: central metal atomic number
 * - ligand_Z_counts: map of ligand Z → count
 * - coordination_number: desired CN (must match sum of counts)
 * 
 * Output:
 * - List of Molecule objects with different ligand arrangements
 */
struct IsomerVariant {
    Molecule structure;
    VariantType type;
    std::string descriptor;
    IsomerSignature signature;
};

class IsomerGenerator {
public:
    /**
     * Generate geometric isomers for a coordination complex.
     * Example: generate_coordination_isomers(27, {{7,4},{17,2}}, 6)
     *          → [Co(NH3)4Cl2]+ with cis and trans isomers
     */
    static std::vector<IsomerVariant> generate_coordination_isomers(
        uint32_t metal_Z,
        const std::map<uint32_t, uint32_t>& ligand_Z_counts,
        uint32_t coordination_number);
    
    /**
     * Generate conformational variants by rotating torsions.
     * (Delegates to existing conformer finder logic)
     */
    static std::vector<IsomerVariant> generate_conformers(
        const Molecule& base_structure,
        int num_samples,
        int seed);
    
private:
    // Select appropriate coordination geometry template
    static CoordinationGeometry select_geometry(uint32_t coordination_number);
    
    // Build molecule from metal + ligand assignment
    static Molecule build_coordination_complex(
        uint32_t metal_Z,
        const std::vector<uint32_t>& ligand_Zs,
        const LigandAssignment& assignment,
        const CoordinationGeometry& geom);
};

// Implementation of select_geometry
inline CoordinationGeometry IsomerGenerator::select_geometry(uint32_t cn) {
    switch (cn) {
        case 4:
            // Default to square planar for d8 metals, tetrahedral otherwise
            // For now, use square planar (more common for isomerism)
            return square_planar_geometry();
        case 5:
            return trigonal_bipyramidal_geometry();
        case 6:
            return octahedral_geometry();
        default:
            std::cerr << "Warning: no template for CN=" << cn << ", using generic\n";
            return octahedral_geometry(); // Fallback
    }
}

// Implementation of build_coordination_complex
inline Molecule IsomerGenerator::build_coordination_complex(
    uint32_t metal_Z,
    const std::vector<uint32_t>& ligand_Zs,
    const LigandAssignment& assignment,
    const CoordinationGeometry& geom)
{
    Molecule mol;
    
    // Add metal at origin
    mol.add_atom(metal_Z, 0.0, 0.0, 0.0);
    const uint32_t metal_idx = 0;
    
    // Estimate metal-ligand bond length (simple covalent radii sum)
    // For production: use element_data_integrated covalent radii
    auto estimate_bond_length = [](uint32_t z1, uint32_t z2) -> double {
        // Rough estimates (Angstroms)
        std::map<uint32_t, double> radii = {
            {1, 0.31}, {6, 0.76}, {7, 0.71}, {8, 0.66},
            {9, 0.57}, {15, 1.07}, {16, 1.05}, {17, 1.02},
            {26, 1.32}, {27, 1.26}, {28, 1.24}, {29, 1.32}
        };
        double r1 = radii.count(z1) ? radii.at(z1) : 1.2;
        double r2 = radii.count(z2) ? radii.at(z2) : 1.2;
        return r1 + r2;
    };
    
    // Add ligands at assigned positions
    for (uint32_t i = 0; i < assignment.position_types.size(); ++i) {
        uint32_t ligand_type = assignment.position_types[i];
        uint32_t ligand_Z = ligand_Zs[ligand_type];
        
        Vec3 direction = geom.positions[i];
        double bond_length = estimate_bond_length(metal_Z, ligand_Z);
        
        double x = direction.x * bond_length;
        double y = direction.y * bond_length;
        double z = direction.z * bond_length;
        
        mol.add_atom(ligand_Z, x, y, z);
        mol.add_bond(metal_idx, i + 1, 1); // Single bond to metal
    }
    
    return mol;
}

// Implementation of generate_coordination_isomers
inline std::vector<IsomerVariant> IsomerGenerator::generate_coordination_isomers(
    uint32_t metal_Z,
    const std::map<uint32_t, uint32_t>& ligand_Z_counts,
    uint32_t coordination_number)
{
    std::vector<IsomerVariant> variants;
    
    // Select geometry template
    CoordinationGeometry geom = select_geometry(coordination_number);
    
    if (geom.coordination_number != coordination_number) {
        std::cerr << "Error: geometry mismatch in isomer generation\n";
        return variants;
    }
    
    // Build ligand type vectors
    std::vector<uint32_t> ligand_Zs;
    std::vector<uint32_t> ligand_counts;
    
    for (const auto& [Z, count] : ligand_Z_counts) {
        ligand_Zs.push_back(Z);
        ligand_counts.push_back(count);
    }
    
    // Enumerate symmetry-distinct ligand assignments
    auto assignments = enumerate_ligand_assignments(geom, ligand_counts);
    
    std::cout << "Generated " << assignments.size() << " symmetry-distinct isomers for CN="
              << coordination_number << "\n";
    
    // Build molecule for each assignment
    for (const auto& assignment : assignments) {
        Molecule mol = build_coordination_complex(metal_Z, ligand_Zs, assignment, geom);
        
        IsomerVariant variant;
        variant.structure = mol;
        variant.type = VariantType::GEOMETRIC_ISOMER;
        variant.descriptor = assignment.descriptor;
        variant.signature = compute_isomer_signature(mol);
        
        variants.push_back(variant);
    }
    
    return variants;
}

} // namespace vsepr
