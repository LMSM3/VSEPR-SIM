#pragma once
/**
 * compound_record.hpp — Compound Database Record C_j
 *
 * Implements the formal definition:
 *
 *   C_j = (S_j, G_j, B_j, P_j, R_j, H_j)
 *
 * where:
 *   S_j  — stoichiometry
 *   G_j  — geometry / graph / arrangement hints
 *   B_j  — bond or interaction classification
 *   P_j  — phase / process metadata
 *   R_j  — resolution projections
 *   H_j  — compound hash block
 *
 * Compounds are distinct database objects with stoichiometric, structural,
 * and resolution-aware metadata.
 *
 * Part of the Day 40C Database Architecture Demonstration Packet.
 */

#include "coarse_grain/database/seed_hash.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <sstream>

namespace vsepr {
namespace database {

// ============================================================================
// Bond / Interaction Classification
// ============================================================================

enum class BondClass : uint8_t {
    COVALENT_NONPOLAR   = 0,
    COVALENT_POLAR      = 1,
    IONIC               = 2,
    METALLIC            = 3,
    IONIC_COVALENT_HYB  = 4,
    VAN_DER_WAALS       = 5,
    HYDROGEN_BOND       = 6,
    LATTICE             = 7,
    COORDINATION        = 8
};

inline const char* bond_class_name(BondClass b) {
    switch (b) {
        case BondClass::COVALENT_NONPOLAR:  return "covalent_nonpolar";
        case BondClass::COVALENT_POLAR:     return "polar_covalent";
        case BondClass::IONIC:              return "ionic";
        case BondClass::METALLIC:           return "metallic";
        case BondClass::IONIC_COVALENT_HYB: return "ionic_covalent_hybrid";
        case BondClass::VAN_DER_WAALS:      return "van_der_waals";
        case BondClass::HYDROGEN_BOND:      return "hydrogen_bond";
        case BondClass::LATTICE:            return "lattice";
        case BondClass::COORDINATION:       return "coordination";
    }
    return "unknown";
}

// ============================================================================
// Structure Family
// ============================================================================

enum class StructureFamily : uint8_t {
    MOLECULAR     = 0,
    CHAIN         = 1,
    RING          = 2,
    BRANCHED      = 3,
    CRYSTAL       = 4,
    COORDINATION  = 5,
    LAYERED       = 6,
    FRAMEWORK     = 7,
    CLUSTER       = 8
};

inline const char* structure_family_name(StructureFamily f) {
    switch (f) {
        case StructureFamily::MOLECULAR:    return "molecular";
        case StructureFamily::CHAIN:        return "chain";
        case StructureFamily::RING:         return "ring";
        case StructureFamily::BRANCHED:     return "branched";
        case StructureFamily::CRYSTAL:      return "crystal";
        case StructureFamily::COORDINATION: return "coordination";
        case StructureFamily::LAYERED:      return "layered";
        case StructureFamily::FRAMEWORK:    return "framework";
        case StructureFamily::CLUSTER:      return "cluster";
    }
    return "unknown";
}

// ============================================================================
// Resolution Projection Mode
// ============================================================================

enum class ProjectionMode : uint8_t {
    ATOMISTIC       = 0,
    BEAD            = 1,
    ATOMISTIC_BEAD  = 2,    ///< Supports both
    FIELD           = 3,
    MACRO           = 4
};

inline const char* projection_mode_name(ProjectionMode m) {
    switch (m) {
        case ProjectionMode::ATOMISTIC:      return "atomistic";
        case ProjectionMode::BEAD:           return "bead";
        case ProjectionMode::ATOMISTIC_BEAD: return "atomistic+bead";
        case ProjectionMode::FIELD:          return "field";
        case ProjectionMode::MACRO:          return "macro";
    }
    return "unknown";
}

// ============================================================================
// Stoichiometry Block  S_j
// ============================================================================

struct StoichiometryBlock {
    std::map<std::string, uint32_t> elements;  ///< symbol → count
    std::string formula;                        ///< Canonical formula string

    std::string summary() const {
        if (!formula.empty()) return formula;
        std::ostringstream os;
        for (auto& [sym, cnt] : elements)
            os << sym << cnt;
        return os.str();
    }

    uint32_t total_atoms() const {
        uint32_t n = 0;
        for (auto& [k, v] : elements) n += v;
        return n;
    }
};

// ============================================================================
// Geometry / Arrangement Hints  G_j
// ============================================================================

struct GeometryHints {
    std::string arrangement;       ///< e.g. "bent", "crystal lattice", "chain"
    StructureFamily family = StructureFamily::MOLECULAR;
    std::string geometry_class;    ///< VSEPR class: "AX2E2", etc.
    double bond_angle_typical = 0.0;  ///< Typical bond angle (degrees)
};

// ============================================================================
// Resolution Projection Block  R_j
// ============================================================================

struct ResolutionProjection {
    std::vector<ProjectionMode> modes;
    std::string default_seed_class;

    bool supports_atomistic() const {
        for (auto m : modes)
            if (m == ProjectionMode::ATOMISTIC || m == ProjectionMode::ATOMISTIC_BEAD)
                return true;
        return false;
    }

    bool supports_bead() const {
        for (auto m : modes)
            if (m == ProjectionMode::BEAD || m == ProjectionMode::ATOMISTIC_BEAD)
                return true;
        return false;
    }
};

// ============================================================================
// §4.2  Compound Record  C_j = (S_j, G_j, B_j, P_j, R_j, H_j)
// ============================================================================

struct CompoundRecord {
    // Identity
    std::string compound_id;
    std::string name;
    std::string compound_class;     ///< e.g. "organic_precursor", "nuclear_fuel"

    // S_j: Stoichiometry
    StoichiometryBlock stoichiometry;

    // G_j: Geometry / arrangement
    GeometryHints geometry;

    // B_j: Bond classification
    BondClass bond_class = BondClass::COVALENT_NONPOLAR;

    // P_j: Phase / process metadata
    std::string phase_description;
    double molecular_weight = 0.0;  ///< g/mol

    // R_j: Resolution projections
    ResolutionProjection resolution;

    // H_j: Hash block
    HashBlock hash;

    void compute_hash() {
        std::string blob = compound_id + ":" + name + ":"
                         + stoichiometry.summary() + ":"
                         + bond_class_name(bond_class);
        hash.tag_32     = compute_tag(compound_id);
        hash.content    = compute_content_hash(blob.data(), blob.size());
        hash.provenance = compute_provenance_hash(blob.data(), blob.size());
    }

    std::string summary() const {
        std::ostringstream os;
        os << "[compound] " << name
           << " (" << stoichiometry.summary() << ")"
           << " bond=" << bond_class_name(bond_class)
           << " " << phase_description;
        if (resolution.supports_atomistic()) os << " [atomistic]";
        if (resolution.supports_bead())      os << " [bead]";
        return os.str();
    }
};

} // namespace database
} // namespace vsepr
