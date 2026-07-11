#pragma once
/**
 * material_record.hpp — Material Database Record M_k
 *
 * Implements the formal definition:
 *
 *   M_k = (I_k, C_k, ρ_k, φ_k, Π_k, Γ_k, H_k)
 *
 * where:
 *   I_k  — material identity
 *   C_k  — composition (stoichiometric or mass-fraction)
 *   ρ_k  — density / bulk density
 *   φ_k  — phase state
 *   Π_k  — engineering property vector
 *   Γ_k  — hazard / environment descriptor
 *   H_k  — provenance hash block
 *
 * A material is not an element.
 * Materials are stored as typed overlays rather than collapsed into element records.
 *
 * Part of the Day 40C Database Architecture Demonstration Packet.
 */

#include "coarse_grain/database/seed_hash.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <iomanip>

namespace vsepr {
namespace database {

// ============================================================================
// Phase State
// ============================================================================

enum class PhaseState : uint8_t {
    SOLID       = 0,
    LIQUID      = 1,
    GAS         = 2,
    PLASMA      = 3,
    MOLECULAR   = 4,
    AMORPHOUS   = 5,
    MIXED       = 6
};

inline const char* phase_state_name(PhaseState p) {
    switch (p) {
        case PhaseState::SOLID:     return "solid";
        case PhaseState::LIQUID:    return "liquid";
        case PhaseState::GAS:       return "gas";
        case PhaseState::PLASMA:    return "plasma";
        case PhaseState::MOLECULAR: return "molecular";
        case PhaseState::AMORPHOUS: return "amorphous";
        case PhaseState::MIXED:     return "mixed";
    }
    return "unknown";
}

// ============================================================================
// Material Class
// ============================================================================

enum class MaterialClass : uint8_t {
    ELEMENTAL_MATERIAL       = 0,
    COMPOUND_MATERIAL        = 1,
    ALLOY                    = 2,
    ORGANIC_PRECURSOR        = 3,
    ORGANOMETALLIC_PRECURSOR = 4,
    PROCESS_MATERIAL         = 5,
    MIXED_RESIDUE            = 6,
    CERAMIC                  = 7,
    POLYMER                  = 8
};

inline const char* material_class_name(MaterialClass c) {
    switch (c) {
        case MaterialClass::ELEMENTAL_MATERIAL:       return "elemental_material";
        case MaterialClass::COMPOUND_MATERIAL:        return "compound_material";
        case MaterialClass::ALLOY:                    return "alloy";
        case MaterialClass::ORGANIC_PRECURSOR:        return "organic_precursor";
        case MaterialClass::ORGANOMETALLIC_PRECURSOR: return "organometallic_precursor";
        case MaterialClass::PROCESS_MATERIAL:         return "process_material";
        case MaterialClass::MIXED_RESIDUE:            return "mixed_residue";
        case MaterialClass::CERAMIC:                  return "ceramic";
        case MaterialClass::POLYMER:                  return "polymer";
    }
    return "unknown";
}

// ============================================================================
// Composition Block  C_k — elemental mass or mole fractions
// ============================================================================

struct CompositionBlock {
    std::map<std::string, double> fractions;  ///< element symbol → fraction
    bool is_mole_fraction = true;             ///< true = mole, false = mass

    double total() const {
        double s = 0.0;
        for (auto& [k, v] : fractions) s += v;
        return s;
    }

    std::string summary() const {
        std::ostringstream os;
        os << "{";
        bool first = true;
        for (auto& [k, v] : fractions) {
            if (!first) os << ", ";
            os << k << ":" << std::fixed << std::setprecision(3) << v;
            first = false;
        }
        os << "}";
        return os.str();
    }
};

// ============================================================================
// Engineering Property Vector  Π_k
// ============================================================================

struct EngineeringProperties {
    double elastic_modulus    = 0.0;   ///< E (GPa)
    double yield_strength     = 0.0;   ///< σ_y (MPa)
    double poisson_ratio      = 0.0;   ///< ν
    double thermal_conductivity = 0.0; ///< k (W/m·K)
    double specific_heat      = 0.0;   ///< C_p (J/kg·K)
    double melting_point      = 0.0;   ///< T_m (K)
    double boiling_point      = 0.0;   ///< T_b (K)
    double hardness_vickers   = 0.0;   ///< HV
    double tensile_strength   = 0.0;   ///< UTS (MPa)
    double elongation_pct     = 0.0;   ///< % elongation at break

    std::string summary() const {
        std::ostringstream os;
        if (elastic_modulus > 0) os << "E=" << elastic_modulus << "GPa ";
        if (yield_strength > 0) os << "σy=" << yield_strength << "MPa ";
        if (melting_point > 0)  os << "Tm=" << melting_point << "K ";
        return os.str();
    }
};

// ============================================================================
// Hazard / Environment Descriptor  Γ_k
// ============================================================================

struct HazardDescriptor {
    bool radioactive     = false;
    bool pyrophoric      = false;
    bool corrosive       = false;
    bool toxic           = false;
    bool oxidizer        = false;
    std::string nfpa_health;          ///< NFPA 704 health rating
    std::string nfpa_fire;            ///< NFPA 704 fire rating
    std::string nfpa_reactivity;      ///< NFPA 704 reactivity rating
    std::vector<std::string> notes;   ///< Free-form hazard notes

    std::string summary() const {
        std::ostringstream os;
        if (radioactive) os << "RADIOACTIVE ";
        if (pyrophoric)  os << "PYROPHORIC ";
        if (corrosive)   os << "CORROSIVE ";
        if (toxic)       os << "TOXIC ";
        if (oxidizer)    os << "OXIDIZER ";
        return os.str();
    }
};

// ============================================================================
// §3.2  Material Database Record  M_k = (I_k, C_k, ρ_k, φ_k, Π_k, Γ_k, H_k)
// ============================================================================

struct MaterialDatabaseRecord {
    // I_k: Identity
    std::string   material_id;
    std::string   name;
    MaterialClass material_class = MaterialClass::ELEMENTAL_MATERIAL;

    // C_k: Composition
    CompositionBlock composition;

    // ρ_k: Density
    double density_kg_m3 = 0.0;

    // φ_k: Phase
    PhaseState phase = PhaseState::SOLID;

    // Π_k: Engineering properties
    EngineeringProperties properties;

    // Γ_k: Hazard
    HazardDescriptor hazard;

    // H_k: Hash provenance
    HashBlock hash;

    void compute_hash() {
        std::string blob = material_id + ":" + name + ":"
                         + std::to_string(density_kg_m3) + ":"
                         + composition.summary();
        hash.tag_32    = compute_tag(material_id);
        hash.content   = compute_content_hash(blob.data(), blob.size());
        hash.provenance = compute_provenance_hash(blob.data(), blob.size());
    }

    std::string summary() const {
        std::ostringstream os;
        os << "[material] " << name
           << " (" << material_class_name(material_class) << ")"
           << " phase=" << phase_state_name(phase)
           << " ρ=" << std::fixed << std::setprecision(1) << density_kg_m3 << " kg/m³"
           << "  " << composition.summary();
        return os.str();
    }
};

} // namespace database
} // namespace vsepr
