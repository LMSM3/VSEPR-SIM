#pragma once
/**
 * input_record.hpp — Generic Scientific Record X_i and Dimension Block D_i
 *
 * Implements the formal definition:
 *
 *   X_i = (I_i, T_i, D_i, P_i, S_i, H_i)
 *
 * where:
 *   I_i  — identity block (name, symbol, canonical ID)
 *   T_i  — type block (element, compound, material, precursor, ...)
 *   D_i  — dimension block: (n_state, n_geom, n_chem, n_nuc, n_aux)
 *   P_i  — payload block (physical / chemical / nuclear data)
 *   S_i  — seed / deterministic initialization block
 *   H_i  — hash / provenance block
 *
 * Also defines:
 *   - SemanticType enum  (§2.3)
 *   - StorageType enum   (§2.4)
 *   - FieldDescriptor    F_ij = (name, semantic_type, storage_type, units, validity)
 *
 * Part of the Day 40C Database Architecture Demonstration Packet.
 */

#include "coarse_grain/database/seed_hash.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <variant>
#include <sstream>

namespace vsepr {
namespace database {

// ============================================================================
// §2.3  Semantic Type Classification
// ============================================================================

enum class SemanticType : uint8_t {
    IDENTITY    = 0,    ///< Immutable object identifier (Z=94, Pu)
    CATEGORICAL = 1,    ///< Discrete class label (actinide, ligand, precursor)
    SCALAR      = 2,    ///< Single real/integer value (mass, radius, charge)
    VECTOR      = 3,    ///< Ordered numeric list (position, velocity)
    TENSOR      = 4,    ///< Matrix-like or directional data (inertia, polarizability)
    SET         = 5,    ///< Unordered discrete collection (oxidation states)
    GRAPH       = 6,    ///< Connectivity relation (bonding map)
    HASH        = 7,    ///< Content signature (SHA-256 / custom 562)
    PATHREF     = 8     ///< Path to canonical shard (/elements/094_Pu/core.json)
};

inline const char* semantic_type_name(SemanticType t) {
    switch (t) {
        case SemanticType::IDENTITY:    return "identity";
        case SemanticType::CATEGORICAL: return "categorical";
        case SemanticType::SCALAR:      return "scalar";
        case SemanticType::VECTOR:      return "vector";
        case SemanticType::TENSOR:      return "tensor";
        case SemanticType::SET:         return "set";
        case SemanticType::GRAPH:       return "graph";
        case SemanticType::HASH:        return "hash";
        case SemanticType::PATHREF:     return "pathref";
    }
    return "unknown";
}

// ============================================================================
// §2.4  Storage Type Classification
// ============================================================================

enum class StorageType : uint8_t {
    U32       = 0,      ///< Compact indices, element IDs
    U64       = 1,      ///< Extended object IDs
    F32       = 2,      ///< Fast approximate simulation values
    F64       = 3,      ///< Scientific numeric values
    STR       = 4,      ///< Names, symbols, tags
    BOOL      = 5,      ///< Flags, response switches
    VEC_I32   = 6,      ///< vec<i32> — ordered integer arrays
    VEC_F64   = 7,      ///< vec<f64> — ordered real arrays
    SET_I32   = 8,      ///< set<i32> — non-duplicate integer collections
    SET_STR   = 9,      ///< set<str> — non-duplicate string collections
    MAP_S_S   = 10,     ///< map<str,str> — keyed string lookup
    MAP_S_F   = 11,     ///< map<str,f64> — keyed numeric lookup
    HASH256   = 12,     ///< 256-bit record integrity
    HASH562   = 13      ///< 562-bit deep provenance digest
};

inline const char* storage_type_name(StorageType t) {
    switch (t) {
        case StorageType::U32:     return "u32";
        case StorageType::U64:     return "u64";
        case StorageType::F32:     return "f32";
        case StorageType::F64:     return "f64";
        case StorageType::STR:     return "str";
        case StorageType::BOOL:    return "bool";
        case StorageType::VEC_I32: return "vec<i32>";
        case StorageType::VEC_F64: return "vec<f64>";
        case StorageType::SET_I32: return "set<i32>";
        case StorageType::SET_STR: return "set<str>";
        case StorageType::MAP_S_S: return "map<str,str>";
        case StorageType::MAP_S_F: return "map<str,f64>";
        case StorageType::HASH256: return "hash256";
        case StorageType::HASH562: return "hash562";
    }
    return "unknown";
}

// ============================================================================
// §2.2  Field Descriptor  F_ij = (name, semantic_type, storage_type, units, validity)
// ============================================================================

struct FieldDescriptor {
    std::string  name;
    SemanticType semantic  = SemanticType::SCALAR;
    StorageType  storage   = StorageType::F64;
    std::string  units;
    std::string  validity;

    std::string summary() const {
        std::ostringstream os;
        os << name << " [" << semantic_type_name(semantic)
           << "/" << storage_type_name(storage) << "]";
        if (!units.empty()) os << " (" << units << ")";
        if (!validity.empty()) os << " {" << validity << "}";
        return os.str();
    }
};

// ============================================================================
// §1.2  Record Type Classification  T_i
// ============================================================================

enum class RecordType : uint8_t {
    ELEMENT     = 0,
    ISOTOPE     = 1,
    ATOMISTIC   = 2,
    BEAD        = 3,
    COMPOUND    = 4,
    MATERIAL    = 5,
    PRECURSOR   = 6,
    HYBRID      = 7
};

inline const char* record_type_name(RecordType t) {
    switch (t) {
        case RecordType::ELEMENT:   return "element";
        case RecordType::ISOTOPE:   return "isotope";
        case RecordType::ATOMISTIC: return "atomistic";
        case RecordType::BEAD:      return "bead";
        case RecordType::COMPOUND:  return "compound";
        case RecordType::MATERIAL:  return "material";
        case RecordType::PRECURSOR: return "precursor";
        case RecordType::HYBRID:    return "hybrid";
    }
    return "unknown";
}

// ============================================================================
// §1.3  Dimension Block  D_i = (n_state, n_geom, n_chem, n_nuc, n_aux)
// ============================================================================

struct DimensionBlock {
    uint32_t n_state = 0;   ///< Number of runtime state variables
    uint32_t n_geom  = 0;   ///< Spatial / geometric dimensions
    uint32_t n_chem  = 0;   ///< Chemical descriptor count
    uint32_t n_nuc   = 0;   ///< Nuclear descriptor count
    uint32_t n_aux   = 0;   ///< Metadata / diagnostics / tags

    uint32_t total() const {
        return n_state + n_geom + n_chem + n_nuc + n_aux;
    }

    std::string summary() const {
        std::ostringstream os;
        os << "D=(" << n_state << "," << n_geom << ","
           << n_chem << "," << n_nuc << "," << n_aux
           << ") total=" << total();
        return os.str();
    }
};

// ============================================================================
// §1.2  Identity Block  I_i
// ============================================================================

struct IdentityBlock {
    std::string canonical_id;       ///< Registry primary key (e.g. "elem_094_Pu")
    std::string name;               ///< Human name (e.g. "Plutonium")
    std::string symbol;             ///< Chemical symbol or short code
    uint32_t    atomic_number = 0;  ///< Z (0 if not an element)
    std::string formula;            ///< Molecular formula if applicable
    std::string cas_number;         ///< CAS registry number if known

    std::string summary() const {
        std::ostringstream os;
        os << canonical_id;
        if (!name.empty()) os << " (" << name << ")";
        if (atomic_number > 0) os << " Z=" << atomic_number;
        return os.str();
    }
};

// ============================================================================
// Payload — variant-based scientific data container
// ============================================================================

using PayloadValue = std::variant<
    uint32_t,
    uint64_t,
    float,
    double,
    bool,
    std::string,
    std::vector<int32_t>,
    std::vector<double>,
    std::map<std::string, double>,
    std::map<std::string, std::string>
>;

struct PayloadBlock {
    std::map<std::string, PayloadValue> fields;

    void set_f64(const std::string& key, double v) {
        fields[key] = v;
    }
    void set_u32(const std::string& key, uint32_t v) {
        fields[key] = v;
    }
    void set_str(const std::string& key, const std::string& v) {
        fields[key] = v;
    }
    void set_bool(const std::string& key, bool v) {
        fields[key] = v;
    }
    void set_vec_f64(const std::string& key, const std::vector<double>& v) {
        fields[key] = v;
    }
    void set_vec_i32(const std::string& key, const std::vector<int32_t>& v) {
        fields[key] = v;
    }

    bool has(const std::string& key) const {
        return fields.count(key) > 0;
    }

    size_t field_count() const { return fields.size(); }
};

// ============================================================================
// §1.2  Generic Scientific Record  X_i = (I_i, T_i, D_i, P_i, S_i, H_i)
// ============================================================================

struct InputRecord {
    IdentityBlock  identity;        ///< I_i
    RecordType     type = RecordType::ELEMENT;  ///< T_i
    DimensionBlock dimension;       ///< D_i
    PayloadBlock   payload;         ///< P_i
    SeedBlock      seed;            ///< S_i
    HashBlock      hash;            ///< H_i

    /// Schema: list of FieldDescriptor for this record's payload fields
    std::vector<FieldDescriptor> schema;

    std::string summary() const {
        std::ostringstream os;
        os << "[" << record_type_name(type) << "] "
           << identity.summary() << "  "
           << dimension.summary();
        if (seed.is_set()) os << "  " << seed.summary();
        if (hash.has_content_hash()) os << "  " << hash.summary();
        return os.str();
    }

    /// Compute and fill the hash block from the payload
    void compute_hashes() {
        // Tag from canonical_id
        hash.tag_32 = compute_tag(identity.canonical_id);

        // Content hash from a serialized summary of the payload
        std::string blob = identity.canonical_id + ":"
                         + std::to_string(static_cast<int>(type)) + ":"
                         + std::to_string(dimension.total()) + ":"
                         + std::to_string(payload.field_count());
        hash.content = compute_content_hash(blob.data(), blob.size());

        // Provenance hash includes seed info
        std::string pblob = blob + ":" + seed.summary();
        hash.provenance = compute_provenance_hash(pblob.data(), pblob.size());
    }
};

// ============================================================================
// Standard dimension presets for each record type
// ============================================================================

/// Element core: no geometry, 4-12 chemical, 0-4 nuclear
inline DimensionBlock dim_element_core() {
    return {0, 0, 8, 2, 4};
}

/// Isotope record: no geometry, 2-8 chemical, 4-12 nuclear
inline DimensionBlock dim_isotope_record() {
    return {0, 0, 4, 8, 4};
}

/// Atomistic object: 3D geometry, rich chemical and nuclear
inline DimensionBlock dim_atomistic_object() {
    return {6, 3, 12, 6, 4};
}

/// Bead object: 3D reduced state
inline DimensionBlock dim_bead_object() {
    return {6, 3, 8, 2, 4};
}

/// Compound record: variable geometry, rich chemical
inline DimensionBlock dim_compound_record() {
    return {0, 0, 16, 4, 8};
}

/// Material preset: variable geometry, rich chemical + engineering
inline DimensionBlock dim_material_preset() {
    return {0, 3, 24, 4, 8};
}

/// Precursor: staged, variable geometry
inline DimensionBlock dim_precursor_object() {
    return {4, 3, 16, 4, 6};
}

/// Hybrid mixed-resolution: the Pu+Cp+Hexene demo dimensions
inline DimensionBlock dim_hybrid_demo() {
    return {41, 3, 19, 5, 12};
}

} // namespace database
} // namespace vsepr
