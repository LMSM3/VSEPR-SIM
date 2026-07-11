#pragma once
/**
 * provenance_record.hpp — 3-Tier Hash Audit & Provenance Record
 * ==============================================================
 *
 * Formalizes the identity-and-provenance layer as a structured record:
 *
 *   Tier 1: Identity
 *     - species (formula)
 *     - atom count, bond count
 *     - composition map {Z → count}
 *     - canonical atom ordering
 *
 *   Tier 2: Topology
 *     - adjacency / bond class
 *     - formal coordination numbers
 *     - topology hash (Morgan + FNV-1a)
 *     - bond signature string
 *
 *   Tier 3: Geometry
 *     - pose-normalized flag
 *     - geometry class (linear, bent, tetrahedral, ...)
 *     - symmetry class (C∞v, C2v, Td, Oh, ...)
 *     - geometry hash (pairwise distance matrix, rotation-invariant)
 *     - reference error (if reference geometry available)
 *
 *   Provenance Record = {N_atoms, N_bonds, H_topo, H_geom, M_build, V_det}
 *
 * Every field is deterministic, inspectable, and traceable.
 * hash_version tracks the hashing algorithm revision for forward compat.
 */

#include "identity/canonical_identity.hpp"
#include "sim/molecule.hpp"
#include "core/types.hpp"

#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <functional>

namespace vsepr {
namespace provenance {

// Current hash algorithm version
inline constexpr int HASH_VERSION = 1;

// ============================================================================
// Geometry Classification
// ============================================================================

enum class GeometryClass {
    Unknown,
    Linear,             // 2 atoms, or 180° angle
    Bent,               // 3 atoms, angle < 180°
    TrigonalPlanar,     // 3 ligands, ~120°
    TrigonalPyramidal,  // 3 ligands + lone pair, ~107°
    Tetrahedral,        // 4 ligands, ~109.5°
    SquarePlanar,       // 4 ligands, ~90°
    TrigonalBipyramidal,// 5 ligands
    Octahedral,         // 6 ligands, ~90°
    Other
};

inline const char* geometry_class_name(GeometryClass gc) {
    switch (gc) {
        case GeometryClass::Linear:             return "linear";
        case GeometryClass::Bent:               return "bent";
        case GeometryClass::TrigonalPlanar:     return "trigonal_planar";
        case GeometryClass::TrigonalPyramidal:  return "trigonal_pyramidal";
        case GeometryClass::Tetrahedral:        return "tetrahedral";
        case GeometryClass::SquarePlanar:       return "square_planar";
        case GeometryClass::TrigonalBipyramidal:return "trigonal_bipyramidal";
        case GeometryClass::Octahedral:         return "octahedral";
        case GeometryClass::Other:              return "other";
        default:                                return "unknown";
    }
}

// ============================================================================
// Symmetry Classification (point group approximation)
// ============================================================================

enum class SymmetryClass {
    Unknown,
    Cinfv,   // C∞v — linear (heteronuclear)
    Dinfh,   // D∞h — linear (homonuclear)
    C2v,     // C2v — bent
    C3v,     // C3v — pyramidal
    Td,      // Td  — tetrahedral
    D4h,     // D4h — square planar
    D3h,     // D3h — trigonal planar / bipyramidal
    Oh,      // Oh  — octahedral
    C1,      // C1  — no symmetry
    Other
};

inline const char* symmetry_class_name(SymmetryClass sc) {
    switch (sc) {
        case SymmetryClass::Cinfv: return "C_inf_v";
        case SymmetryClass::Dinfh: return "D_inf_h";
        case SymmetryClass::C2v:   return "C2v";
        case SymmetryClass::C3v:   return "C3v";
        case SymmetryClass::Td:    return "Td";
        case SymmetryClass::D4h:   return "D4h";
        case SymmetryClass::D3h:   return "D3h";
        case SymmetryClass::Oh:    return "Oh";
        case SymmetryClass::C1:    return "C1";
        default:                   return "unknown";
    }
}

// ============================================================================
// Classify geometry from coordination number and angles
// ============================================================================

/**
 * Classify the geometry of a central-atom molecule.
 * Finds the atom with most bonds (central atom), counts ligands,
 * and estimates angles to classify.
 */
inline GeometryClass classify_geometry(const Molecule& mol) {
    if (mol.num_atoms() < 2) return GeometryClass::Unknown;
    if (mol.num_atoms() == 2) return GeometryClass::Linear;

    // Find central atom (most bonds)
    std::vector<int> degree(mol.num_atoms(), 0);
    for (const auto& b : mol.bonds) {
        degree[b.i]++;
        degree[b.j]++;
    }
    uint32_t central = 0;
    for (size_t i = 1; i < mol.num_atoms(); ++i) {
        if (degree[i] > degree[central]) central = static_cast<uint32_t>(i);
    }

    int coordination = degree[central];

    // Get ligand positions relative to central atom
    double cx = mol.coords[3 * central];
    double cy = mol.coords[3 * central + 1];
    double cz = mol.coords[3 * central + 2];

    std::vector<uint32_t> ligands;
    for (const auto& b : mol.bonds) {
        if (b.i == central) ligands.push_back(b.j);
        if (b.j == central) ligands.push_back(b.i);
    }

    if (coordination == 1) return GeometryClass::Linear;

    if (coordination == 2) {
        // Compute angle
        if (ligands.size() >= 2) {
            double ax = mol.coords[3 * ligands[0]] - cx;
            double ay = mol.coords[3 * ligands[0] + 1] - cy;
            double az = mol.coords[3 * ligands[0] + 2] - cz;
            double bx = mol.coords[3 * ligands[1]] - cx;
            double by = mol.coords[3 * ligands[1] + 1] - cy;
            double bz = mol.coords[3 * ligands[1] + 2] - cz;

            double dot = ax * bx + ay * by + az * bz;
            double ma = std::sqrt(ax * ax + ay * ay + az * az);
            double mb = std::sqrt(bx * bx + by * by + bz * bz);
            if (ma > 1e-10 && mb > 1e-10) {
                double cos_angle = dot / (ma * mb);
                cos_angle = std::max(-1.0, std::min(1.0, cos_angle));
                double angle_deg = std::acos(cos_angle) * 180.0 / 3.14159265358979;
                if (angle_deg > 170.0) return GeometryClass::Linear;
                return GeometryClass::Bent;
            }
        }
        return GeometryClass::Bent;
    }

    if (coordination == 3) {
        // Check planarity: compute normal from first two ligand vectors, check third
        if (ligands.size() >= 3) {
            double v1x = mol.coords[3 * ligands[0]] - cx;
            double v1y = mol.coords[3 * ligands[0] + 1] - cy;
            double v1z = mol.coords[3 * ligands[0] + 2] - cz;
            double v2x = mol.coords[3 * ligands[1]] - cx;
            double v2y = mol.coords[3 * ligands[1] + 1] - cy;
            double v2z = mol.coords[3 * ligands[1] + 2] - cz;
            double v3x = mol.coords[3 * ligands[2]] - cx;
            double v3y = mol.coords[3 * ligands[2] + 1] - cy;
            double v3z = mol.coords[3 * ligands[2] + 2] - cz;

            // Normal = v1 × v2
            double nx = v1y * v2z - v1z * v2y;
            double ny = v1z * v2x - v1x * v2z;
            double nz = v1x * v2y - v1y * v2x;
            double nm = std::sqrt(nx * nx + ny * ny + nz * nz);

            if (nm > 1e-10) {
                double dot = (v3x * nx + v3y * ny + v3z * nz) / nm;
                double v3m = std::sqrt(v3x * v3x + v3y * v3y + v3z * v3z);
                if (v3m > 1e-10) {
                    double out_of_plane = std::abs(dot) / v3m;
                    if (out_of_plane < 0.1) return GeometryClass::TrigonalPlanar;
                    return GeometryClass::TrigonalPyramidal;
                }
            }
        }
        return GeometryClass::TrigonalPyramidal;
    }

    if (coordination == 4) {
        // Tetrahedral vs square planar: check if ligands are coplanar
        if (ligands.size() >= 4) {
            double v1x = mol.coords[3 * ligands[0]] - cx;
            double v1y = mol.coords[3 * ligands[0] + 1] - cy;
            double v1z = mol.coords[3 * ligands[0] + 2] - cz;
            double v2x = mol.coords[3 * ligands[1]] - cx;
            double v2y = mol.coords[3 * ligands[1] + 1] - cy;
            double v2z = mol.coords[3 * ligands[1] + 2] - cz;
            double v4x = mol.coords[3 * ligands[3]] - cx;
            double v4y = mol.coords[3 * ligands[3] + 1] - cy;
            double v4z = mol.coords[3 * ligands[3] + 2] - cz;

            double nx = v1y * v2z - v1z * v2y;
            double ny = v1z * v2x - v1x * v2z;
            double nz = v1x * v2y - v1y * v2x;
            double nm = std::sqrt(nx * nx + ny * ny + nz * nz);

            if (nm > 1e-10) {
                double dot = (v4x * nx + v4y * ny + v4z * nz) / nm;
                double v4m = std::sqrt(v4x * v4x + v4y * v4y + v4z * v4z);
                if (v4m > 1e-10) {
                    double out_of_plane = std::abs(dot) / v4m;
                    if (out_of_plane < 0.1) return GeometryClass::SquarePlanar;
                    return GeometryClass::Tetrahedral;
                }
            }
        }
        return GeometryClass::Tetrahedral;
    }

    if (coordination == 5) return GeometryClass::TrigonalBipyramidal;
    if (coordination == 6) return GeometryClass::Octahedral;

    return GeometryClass::Other;
}

/**
 * Approximate symmetry class from geometry class and ligand equivalence.
 */
inline SymmetryClass classify_symmetry(const Molecule& mol, GeometryClass gc) {
    // Count distinct ligand types
    std::vector<int> degree(mol.num_atoms(), 0);
    for (const auto& b : mol.bonds) {
        degree[b.i]++;
        degree[b.j]++;
    }

    uint32_t central = 0;
    for (size_t i = 1; i < mol.num_atoms(); ++i) {
        if (degree[i] > degree[central]) central = static_cast<uint32_t>(i);
    }

    std::map<uint8_t, int> ligand_types;
    for (const auto& b : mol.bonds) {
        uint32_t lig = (b.i == central) ? b.j : (b.j == central ? b.i : UINT32_MAX);
        if (lig != UINT32_MAX && lig < mol.num_atoms()) {
            ligand_types[mol.atoms[lig].Z]++;
        }
    }

    bool all_same = (ligand_types.size() == 1);

    switch (gc) {
        case GeometryClass::Linear:
            return all_same ? SymmetryClass::Dinfh : SymmetryClass::Cinfv;
        case GeometryClass::Bent:
            return SymmetryClass::C2v;
        case GeometryClass::TrigonalPlanar:
            return all_same ? SymmetryClass::D3h : SymmetryClass::C1;
        case GeometryClass::TrigonalPyramidal:
            return all_same ? SymmetryClass::C3v : SymmetryClass::C1;
        case GeometryClass::Tetrahedral:
            return all_same ? SymmetryClass::Td : SymmetryClass::C1;
        case GeometryClass::SquarePlanar:
            return all_same ? SymmetryClass::D4h : SymmetryClass::C1;
        case GeometryClass::TrigonalBipyramidal:
            return all_same ? SymmetryClass::D3h : SymmetryClass::C1;
        case GeometryClass::Octahedral:
            return all_same ? SymmetryClass::Oh : SymmetryClass::C1;
        default:
            return SymmetryClass::Unknown;
    }
}

// ============================================================================
// Bond Signature — canonical bond-type string
// ============================================================================

/**
 * Produce a sorted bond signature: "C-H:4,O-H:2" etc.
 * Element pair is always ordered alphabetically.
 */
inline std::string bond_signature(
    const Molecule& mol,
    const std::function<std::string(uint8_t)>& Z_to_sym)
{
    std::map<std::string, int> counts;
    for (const auto& b : mol.bonds) {
        std::string sa = Z_to_sym(mol.atoms[b.i].Z);
        std::string sb = Z_to_sym(mol.atoms[b.j].Z);
        if (sa > sb) std::swap(sa, sb);
        std::string key = sa + "-" + sb;
        if (b.order > 1) key += "=" + std::to_string(b.order);
        counts[key]++;
    }

    std::ostringstream oss;
    bool first = true;
    for (const auto& [key, count] : counts) {
        if (!first) oss << ",";
        oss << key << ":" << count;
        first = false;
    }
    return oss.str();
}

// ============================================================================
// Canonical Atom Ordering String
// ============================================================================

/**
 * Produce canonical atom ordering string from Morgan canonical order.
 * Example: "O,H,H" for H2O.
 */
inline std::string canonical_order_string(
    const Molecule& mol,
    const std::function<std::string(uint8_t)>& Z_to_sym)
{
    auto morgan = identity::morgan_canonicalize(mol.atoms, mol.bonds);

    std::ostringstream oss;
    for (size_t i = 0; i < morgan.canonical_order.size(); ++i) {
        if (i > 0) oss << ",";
        oss << Z_to_sym(mol.atoms[morgan.canonical_order[i]].Z);
    }
    return oss.str();
}

// ============================================================================
// Coordination Numbers
// ============================================================================

/**
 * Compute formal coordination number for each atom.
 */
inline std::vector<int> formal_coordination(const Molecule& mol) {
    std::vector<int> coord(mol.num_atoms(), 0);
    for (const auto& b : mol.bonds) {
        if (b.i < mol.num_atoms()) coord[b.i]++;
        if (b.j < mol.num_atoms()) coord[b.j]++;
    }
    return coord;
}

// ============================================================================
// The 3-Tier Provenance Record
// ============================================================================

struct ProvenanceRecord {
    // === Tier 1: Identity ===
    std::string          species;             // Hill system formula
    uint32_t             atom_count;
    uint32_t             bond_count;
    std::map<uint8_t, int> composition;       // {Z → count}
    std::string          canonical_order;     // "O,H,H"

    // === Tier 2: Topology ===
    uint64_t             topo_hash;           // Morgan + FNV-1a
    std::string          bond_sig;            // "H-O:2"
    std::vector<int>     coordination;        // per-atom coordination numbers

    // === Tier 3: Geometry ===
    uint64_t             geom_hash;           // pairwise distance matrix hash
    GeometryClass        geom_class;
    SymmetryClass        sym_class;
    bool                 pose_normalized;     // geom hash is rotation/translation invariant

    // === Provenance metadata ===
    std::string          method;              // construction/generation method
    bool                 deterministic;       // V_det: verified by double-build
    int                  hash_version;        // algorithm revision

    // ========================================================================
    // Serialization
    // ========================================================================

    /**
     * Produce the full audit record as a human-readable string.
     */
    std::string to_audit_string() const {
        std::ostringstream oss;
        oss << "species: " << species << "\n";
        oss << "atoms: " << atom_count << "\n";
        oss << "bonds: " << bond_count << "\n";
        oss << "topo_hash: 0x" << std::hex << std::setfill('0')
            << std::setw(16) << topo_hash << std::dec << "\n";
        oss << "geom_hash: 0x" << std::hex << std::setfill('0')
            << std::setw(16) << geom_hash << std::dec << "\n";
        oss << "geometry_class: " << geometry_class_name(geom_class) << "\n";
        oss << "symmetry_class: " << symmetry_class_name(sym_class) << "\n";
        oss << "canonical_order: " << canonical_order << "\n";
        oss << "bond_signature: " << bond_sig << "\n";
        oss << "pose_normalized: " << (pose_normalized ? "yes" : "no") << "\n";
        oss << "hash_version: v" << hash_version << "\n";
        oss << "verified: " << (deterministic ? "deterministic" : "unverified") << "\n";
        oss << "method: " << method << "\n";
        return oss.str();
    }

    /**
     * Tier comparison: two records match at a given tier if all
     * fields up to that tier are equal.
     */
    bool matches_tier1(const ProvenanceRecord& other) const {
        return species == other.species &&
               atom_count == other.atom_count &&
               bond_count == other.bond_count;
    }

    bool matches_tier2(const ProvenanceRecord& other) const {
        return matches_tier1(other) &&
               topo_hash == other.topo_hash;
    }

    bool matches_tier3(const ProvenanceRecord& other) const {
        return matches_tier2(other) &&
               geom_hash == other.geom_hash;
    }
};

// ============================================================================
// Build a ProvenanceRecord from a Molecule
// ============================================================================

inline ProvenanceRecord build_provenance(
    const Molecule& mol,
    const std::function<std::string(uint8_t)>& Z_to_sym,
    const std::string& method = "",
    bool verify_deterministic = false,
    std::function<Molecule()> rebuild_fn = nullptr)
{
    ProvenanceRecord rec;

    // Tier 1: Identity
    rec.species = identity::canonical_formula(mol.atoms, Z_to_sym);
    rec.atom_count = static_cast<uint32_t>(mol.num_atoms());
    rec.bond_count = static_cast<uint32_t>(mol.num_bonds());
    for (const auto& a : mol.atoms) rec.composition[a.Z]++;
    rec.canonical_order = canonical_order_string(mol, Z_to_sym);

    // Tier 2: Topology
    rec.topo_hash = identity::graph_hash(mol.atoms, mol.bonds);
    rec.bond_sig = bond_signature(mol, Z_to_sym);
    rec.coordination = formal_coordination(mol);

    // Tier 3: Geometry
    rec.geom_hash = identity::geometry_hash(mol.coords, mol.num_atoms());
    rec.geom_class = classify_geometry(mol);
    rec.sym_class = classify_symmetry(mol, rec.geom_class);
    rec.pose_normalized = true;  // Our geometry_hash uses pairwise distances

    // Provenance
    rec.method = method;
    rec.hash_version = HASH_VERSION;

    // Deterministic verification: rebuild and compare
    if (verify_deterministic && rebuild_fn) {
        Molecule mol2 = rebuild_fn();
        auto id1 = identity::compute_identity(mol, Z_to_sym);
        auto id2 = identity::compute_identity(mol2, Z_to_sym);
        rec.deterministic = id1.same_conformer(id2);
    } else {
        rec.deterministic = false;
    }

    return rec;
}

} // namespace provenance
} // namespace vsepr
