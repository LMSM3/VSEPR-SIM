#pragma once
/**
 * isomer_signature.hpp - Canonical Isomer Identification
 * 
 * Provides index-invariant signatures for different isomer types:
 * - Constitutional isomers: Graph topology (connectivity)
 * - Geometric isomers: Ligand arrangement around metal centers
 * - Stereoisomers: Chirality and spatial configuration
 * 
 * Key principle: Same isomer → Same signature regardless of atom ordering
 */

#include "molecule.hpp"
#include "../core/element_data_integrated.hpp"
#include <vector>
#include <algorithm>
#include <cmath>
#include <string>
#include <sstream>
#include <map>

namespace vsepr {

//=============================================================================
// Graph-Based Constitutional Signature
//=============================================================================

/**
 * Compute canonical graph hash (connectivity-based).
 * Identifies constitutional isomers (same formula, different bonding).
 * 
 * Uses Morgan algorithm / Extended Connectivity Fingerprint:
 * 1. Initial atom labels (Z + degree)
 * 2. Iterative refinement based on neighbor labels
 * 3. Canonicalization via sorting
 */
struct ConstitutionalSignature {
    std::vector<uint64_t> atom_hashes;  // Sorted Morgan labels
    std::vector<std::pair<uint64_t, uint64_t>> bond_hashes; // Sorted bond fingerprints
    
    bool operator==(const ConstitutionalSignature& other) const {
        return atom_hashes == other.atom_hashes && bond_hashes == other.bond_hashes;
    }
    
    std::string to_string() const {
        std::ostringstream oss;
        oss << "CONST[";
        for (size_t i = 0; i < std::min(atom_hashes.size(), size_t(5)); ++i) {
            if (i > 0) oss << ",";
            oss << atom_hashes[i];
        }
        oss << "]";
        return oss.str();
    }
};

inline ConstitutionalSignature compute_constitutional_signature(const Molecule& mol) {
    const uint32_t n = mol.num_atoms();
    
    // Build adjacency list with bond orders
    std::vector<std::vector<std::pair<uint32_t, int>>> adj(n);
    for (const auto& bond : mol.bonds) {
        adj[bond.i].push_back({bond.j, bond.order});
        adj[bond.j].push_back({bond.i, bond.order});
    }
    
    // Morgan algorithm: iterative label refinement
    std::vector<uint64_t> labels(n);
    std::vector<uint64_t> new_labels(n);
    
    // Initialize: hash(Z, degree, bond_order_sum)
    for (uint32_t i = 0; i < n; ++i) {
        uint64_t degree = adj[i].size();
        uint64_t bond_sum = 0;
        for (auto [j, order] : adj[i]) {
            bond_sum += order;
        }
        labels[i] = (uint64_t(mol.atoms[i].Z) << 32) | (degree << 16) | bond_sum;
    }
    
    // Refine labels for 5 iterations (sufficient for most molecules)
    for (int iter = 0; iter < 5; ++iter) {
        for (uint32_t i = 0; i < n; ++i) {
            std::vector<uint64_t> neighbor_labels;
            for (auto [j, order] : adj[i]) {
                neighbor_labels.push_back(labels[j] * order); // Weight by bond order
            }
            std::sort(neighbor_labels.begin(), neighbor_labels.end());
            
            // Hash current label + sorted neighbor labels
            uint64_t hash = labels[i];
            for (auto nl : neighbor_labels) {
                hash = hash * 31 + nl; // Simple polynomial hash
            }
            new_labels[i] = hash;
        }
        labels = new_labels;
    }
    
    ConstitutionalSignature sig;
    sig.atom_hashes = labels;
    std::sort(sig.atom_hashes.begin(), sig.atom_hashes.end());
    
    // Bond fingerprints: sorted pairs of endpoint labels
    for (const auto& bond : mol.bonds) {
        uint64_t h1 = labels[bond.i];
        uint64_t h2 = labels[bond.j];
        if (h1 > h2) std::swap(h1, h2);
        sig.bond_hashes.push_back({h1, h2});
    }
    std::sort(sig.bond_hashes.begin(), sig.bond_hashes.end());
    
    return sig;
}

//=============================================================================
// Coordination Geometry Signature
//=============================================================================

/**
 * Identifies geometric isomers in coordination complexes.
 * Example: cis/trans [MA4B2], fac/mer [MA3B3]
 * 
 * Strategy:
 * - Find metal center
 * - Extract ligand types and relative positions
 * - Compute pairwise angular relationships
 * - Canonicalize via sorting
 */
struct CoordinationSignature {
    uint32_t metal_Z;               // Metal atomic number
    uint32_t coordination_number;   // Number of ligands
    std::vector<uint32_t> ligand_types; // Sorted ligand Z values
    std::vector<int> angular_pattern;   // Canonical arrangement descriptor
    
    bool operator==(const CoordinationSignature& other) const {
        return metal_Z == other.metal_Z &&
               coordination_number == other.coordination_number &&
               ligand_types == other.ligand_types &&
               angular_pattern == other.angular_pattern;
    }
    
    std::string to_string() const {
        std::ostringstream oss;
        oss << "COORD[M=" << metal_Z << ",CN=" << coordination_number << ",pat=";
        for (size_t i = 0; i < std::min(angular_pattern.size(), size_t(6)); ++i) {
            oss << angular_pattern[i];
        }
        oss << "]";
        return oss.str();
    }
};

/**
 * Compute angular pattern: which ligand types are trans/cis/adjacent?
 * Returns canonical descriptor independent of ligand ordering.
 */
inline std::vector<int> compute_angular_pattern(
    const Molecule& mol,
    uint32_t metal_idx,
    const std::vector<uint32_t>& ligand_indices,
    const std::vector<uint32_t>& ligand_types)
{
    const uint32_t n_lig = ligand_indices.size();
    
    // Compute all pairwise angles from metal center
    std::vector<std::tuple<uint32_t, uint32_t, double>> angles; // (type1, type2, angle)
    
    for (uint32_t i = 0; i < n_lig; ++i) {
        for (uint32_t j = i + 1; j < n_lig; ++j) {
            uint32_t li = ligand_indices[i];
            uint32_t lj = ligand_indices[j];
            
            // Vector from metal to ligands
            double dx1 = mol.coords[3*li] - mol.coords[3*metal_idx];
            double dy1 = mol.coords[3*li+1] - mol.coords[3*metal_idx+1];
            double dz1 = mol.coords[3*li+2] - mol.coords[3*metal_idx+2];
            
            double dx2 = mol.coords[3*lj] - mol.coords[3*metal_idx];
            double dy2 = mol.coords[3*lj+1] - mol.coords[3*metal_idx+1];
            double dz2 = mol.coords[3*lj+2] - mol.coords[3*metal_idx+2];
            
            // Normalize
            double len1 = std::sqrt(dx1*dx1 + dy1*dy1 + dz1*dz1);
            double len2 = std::sqrt(dx2*dx2 + dy2*dy2 + dz2*dz2);
            
            if (len1 < 1e-6 || len2 < 1e-6) continue;
            
            dx1 /= len1; dy1 /= len1; dz1 /= len1;
            dx2 /= len2; dy2 /= len2; dz2 /= len2;
            
            // Angle
            double dot = dx1*dx2 + dy1*dy2 + dz1*dz2;
            dot = std::max(-1.0, std::min(1.0, dot)); // Clamp
            double angle = std::acos(dot) * 180.0 / M_PI;
            
            // Classify: trans (~180°), cis (~90°), etc.
            uint32_t t1 = ligand_types[i];
            uint32_t t2 = ligand_types[j];
            if (t1 > t2) std::swap(t1, t2); // Canonical ordering
            
            angles.push_back({t1, t2, angle});
        }
    }
    
    // Sort angles canonically: by (type1, type2, angle)
    std::sort(angles.begin(), angles.end());
    
    // Discretize angles into pattern codes
    std::vector<int> pattern;
    for (auto [t1, t2, angle] : angles) {
        // Encode: type_pair + angle_bin
        int angle_bin = static_cast<int>(std::round(angle / 30.0)); // 30° bins
        int code = (t1 << 16) | (t2 << 8) | angle_bin;
        pattern.push_back(code);
    }
    
    return pattern;
}

inline CoordinationSignature compute_coordination_signature(
    const Molecule& mol)
{
    CoordinationSignature sig;
    sig.metal_Z = 0;
    sig.coordination_number = 0;
    
    // Find first metal center (Z >= 21 and <= 30, or other transition metals)
    // Simple heuristic: atoms with Z in transition metal range
    auto is_metal = [](uint32_t Z) -> bool {
        return (Z >= 21 && Z <= 30) || // 3d metals
               (Z >= 39 && Z <= 48) || // 4d metals
               (Z >= 72 && Z <= 80);   // 5d metals
    };
    
    uint32_t metal_idx = UINT32_MAX;
    for (uint32_t i = 0; i < mol.num_atoms(); ++i) {
        if (is_metal(mol.atoms[i].Z)) {
            metal_idx = i;
            sig.metal_Z = mol.atoms[i].Z;
            break;
        }
    }
    
    if (metal_idx == UINT32_MAX) {
        // No metal center → not a coordination complex
        return sig;
    }
    
    // Find ligands (atoms bonded to metal)
    std::vector<uint32_t> ligand_indices;
    std::vector<uint32_t> ligand_types;
    
    for (const auto& bond : mol.bonds) {
        if (bond.i == metal_idx) {
            ligand_indices.push_back(bond.j);
            ligand_types.push_back(mol.atoms[bond.j].Z);
        } else if (bond.j == metal_idx) {
            ligand_indices.push_back(bond.i);
            ligand_types.push_back(mol.atoms[bond.i].Z);
        }
    }
    
    sig.coordination_number = ligand_indices.size();
    sig.ligand_types = ligand_types;
    std::sort(sig.ligand_types.begin(), sig.ligand_types.end());
    
    // Compute angular pattern
    sig.angular_pattern = compute_angular_pattern(mol, metal_idx, ligand_indices, ligand_types);
    
    return sig;
}

//=============================================================================
// Stereochemistry Signature (Chirality)
//=============================================================================

/**
 * Detect chirality centers and assign R/S configuration.
 * For future expansion - currently placeholder.
 */
struct ChiralSignature {
    std::vector<int> chiral_centers; // Atom indices with chirality
    std::vector<char> configurations; // 'R' or 'S' for each center
    
    bool operator==(const ChiralSignature& other) const {
        return chiral_centers == other.chiral_centers &&
               configurations == other.configurations;
    }
    
    std::string to_string() const {
        std::ostringstream oss;
        oss << "CHIRAL[";
        for (size_t i = 0; i < std::min(chiral_centers.size(), size_t(3)); ++i) {
            if (i > 0) oss << ",";
            oss << chiral_centers[i] << ":" << configurations[i];
        }
        oss << "]";
        return oss.str();
    }
};

inline ChiralSignature compute_chiral_signature(const Molecule& mol) {
    // TODO: Implement Cahn-Ingold-Prelog priority rules
    // For now, return empty (no chirality detected)
    return ChiralSignature();
}

//=============================================================================
// Combined Isomer Signature
//=============================================================================

/**
 * Complete isomer signature combining all levels:
 * 1. Constitutional (graph topology)
 * 2. Coordination geometry (cis/trans, fac/mer)
 * 3. Stereochemistry (R/S chirality)
 * 
 * Two structures are the same isomer if all three signatures match.
 */
struct IsomerSignature {
    ConstitutionalSignature constitutional;
    CoordinationSignature coordination;
    ChiralSignature chiral;
    
    bool operator==(const IsomerSignature& other) const {
        return constitutional == other.constitutional &&
               coordination == other.coordination &&
               chiral == other.chiral;
    }
    
    bool operator!=(const IsomerSignature& other) const {
        return !(*this == other);
    }
    
    std::string to_string() const {
        return constitutional.to_string() + " " +
               coordination.to_string() + " " +
               chiral.to_string();
    }
};

inline IsomerSignature compute_isomer_signature(const Molecule& mol)
{
    IsomerSignature sig;
    sig.constitutional = compute_constitutional_signature(mol);
    sig.coordination = compute_coordination_signature(mol);
    sig.chiral = compute_chiral_signature(mol);
    return sig;
}

} // namespace vsepr
