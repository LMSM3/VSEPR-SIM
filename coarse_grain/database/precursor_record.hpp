#pragma once
/**
 * precursor_record.hpp — Precursor Database Record P_m
 *
 * Implements the formal definition:
 *
 *   P_m = (I_m, Ψ_m, Ω_m, Λ_m, T_m, H_m)
 *
 * where:
 *   I_m  — precursor identity
 *   Ψ_m  — chemical state
 *   Ω_m  — environment readiness
 *   Λ_m  — transformation / reaction flags
 *   T_m  — target formation class
 *   H_m  — provenance block
 *
 * A precursor is not just a compound.  It is a reaction-positioned entity.
 * Precursors are staged formation objects rather than fully committed
 * final structures.
 *
 * Part of the Day 40C Database Architecture Demonstration Packet.
 */

#include "coarse_grain/database/seed_hash.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <sstream>

namespace vsepr {
namespace database {

// ============================================================================
// Precursor Class
// ============================================================================

enum class PrecursorClass : uint8_t {
    ORGANIC_CHAIN        = 0,
    ORGANOMETALLIC       = 1,
    PRECIPITATION        = 2,
    PROCESS              = 3,
    PROCESS_FIELD        = 4,
    DISSOLVED_COMPLEX    = 5,
    SLURRY               = 6,
    CRYSTALLINE_SOLID    = 7
};

inline const char* precursor_class_name(PrecursorClass c) {
    switch (c) {
        case PrecursorClass::ORGANIC_CHAIN:     return "organic_chain_precursor";
        case PrecursorClass::ORGANOMETALLIC:    return "organometallic_precursor";
        case PrecursorClass::PRECIPITATION:     return "precipitation_precursor";
        case PrecursorClass::PROCESS:           return "process_precursor";
        case PrecursorClass::PROCESS_FIELD:     return "process_field_precursor";
        case PrecursorClass::DISSOLVED_COMPLEX: return "dissolved_complex";
        case PrecursorClass::SLURRY:            return "slurry_precursor";
        case PrecursorClass::CRYSTALLINE_SOLID: return "crystalline_solid_precursor";
    }
    return "unknown";
}

// ============================================================================
// Target Formation Class
// ============================================================================

enum class TargetFormation : uint8_t {
    HYDROCARBON_FAMILY        = 0,
    COORDINATION_SYSTEM       = 1,
    OXIDE_EVOLUTION           = 2,
    SLUDGE_SALT_OXIDE         = 3,
    DEPOSITION_MODEL          = 4,
    RESIDUE_EVOLUTION         = 5,
    BEAD_TESTED_FAMILY        = 6,
    MIXED_BEAD_ATOMISTIC      = 7,
    THERMAL_DECOMPOSITION     = 8
};

inline const char* target_formation_name(TargetFormation t) {
    switch (t) {
        case TargetFormation::HYDROCARBON_FAMILY:    return "bead_tested_hydrocarbon_family";
        case TargetFormation::COORDINATION_SYSTEM:   return "mixed_bead_atomistic_coordination";
        case TargetFormation::OXIDE_EVOLUTION:        return "oxide_or_residue_evolution";
        case TargetFormation::SLUDGE_SALT_OXIDE:     return "sludge_salt_oxide_outcome";
        case TargetFormation::DEPOSITION_MODEL:      return "materials_deposition_model";
        case TargetFormation::RESIDUE_EVOLUTION:      return "residue_evolution";
        case TargetFormation::BEAD_TESTED_FAMILY:    return "bead_tested_family";
        case TargetFormation::MIXED_BEAD_ATOMISTIC:  return "mixed_bead_atomistic";
        case TargetFormation::THERMAL_DECOMPOSITION: return "thermal_decomposition";
    }
    return "unknown";
}

// ============================================================================
// Chemical State Block  Ψ_m
// ============================================================================

struct ChemicalState {
    std::string phase;              ///< "solid", "liquid", "dissolved", "molecular"
    double temperature_K   = 298.15;
    double pressure_Pa     = 101325.0;
    double pH              = 7.0;
    std::string solvent;            ///< Solvent if in solution
    std::vector<int32_t> oxidation_states;

    std::string summary() const {
        std::ostringstream os;
        os << phase << " T=" << temperature_K << "K"
           << " P=" << pressure_Pa << "Pa";
        if (pH != 7.0) os << " pH=" << pH;
        return os.str();
    }
};

// ============================================================================
// Environment Readiness Block  Ω_m
// ============================================================================

struct EnvironmentReadiness {
    bool atmosphere_inert   = false;   ///< Requires inert atmosphere
    bool moisture_sensitive = false;
    bool light_sensitive    = false;
    double max_temperature_K = 1e6;    ///< Max stable temperature
    double min_temperature_K = 0.0;    ///< Min stable temperature
    std::string containment_class;     ///< "open", "glovebox", "hot_cell"
    std::vector<std::string> incompatible_with;

    std::string summary() const {
        std::ostringstream os;
        if (atmosphere_inert)   os << "INERT_ATM ";
        if (moisture_sensitive) os << "MOISTURE_SENS ";
        if (light_sensitive)    os << "LIGHT_SENS ";
        if (!containment_class.empty()) os << "contain=" << containment_class;
        return os.str();
    }
};

// ============================================================================
// Transformation / Reaction Flags  Λ_m
// ============================================================================

struct TransformationFlags {
    bool reactive             = false;
    bool exothermic           = false;
    bool catalytic            = false;
    bool self_initiating      = false;
    double activation_energy  = 0.0;    ///< E_a (kJ/mol)
    std::string reaction_type;          ///< "precipitation", "oxidation", "polymerization"
    std::string mechanism;              ///< "nucleation", "chain_growth", etc.
    std::vector<std::string> products;  ///< Expected product names

    std::string summary() const {
        std::ostringstream os;
        if (reactive) os << "REACTIVE ";
        if (exothermic) os << "EXOTHERMIC ";
        if (catalytic) os << "CATALYTIC ";
        if (!reaction_type.empty()) os << "type=" << reaction_type << " ";
        if (activation_energy > 0) os << "Ea=" << activation_energy << "kJ/mol ";
        return os.str();
    }
};

// ============================================================================
// §5.2  Precursor Record  P_m = (I_m, Ψ_m, Ω_m, Λ_m, T_m, H_m)
// ============================================================================

struct PrecursorRecord {
    // I_m: Identity
    std::string precursor_id;
    std::string name;
    PrecursorClass precursor_class = PrecursorClass::ORGANIC_CHAIN;
    std::string base_compound_id;       ///< Link to CompoundRecord if applicable
    uint32_t    stage = 1;              ///< Staging level (1 = initial, 2+ = intermediate)

    // Ψ_m: Chemical state
    ChemicalState chemical_state;

    // Ω_m: Environment readiness
    EnvironmentReadiness environment;

    // Λ_m: Transformation flags
    TransformationFlags transformation;

    // T_m: Target formation
    TargetFormation target = TargetFormation::HYDROCARBON_FAMILY;

    // H_m: Hash provenance
    HashBlock hash;

    void compute_hash() {
        std::string blob = precursor_id + ":" + name + ":"
                         + std::to_string(static_cast<int>(precursor_class)) + ":"
                         + std::to_string(stage) + ":"
                         + chemical_state.summary();
        hash.tag_32     = compute_tag(precursor_id);
        hash.content    = compute_content_hash(blob.data(), blob.size());
        hash.provenance = compute_provenance_hash(blob.data(), blob.size());
    }

    std::string summary() const {
        std::ostringstream os;
        os << "[precursor] " << name
           << " (" << precursor_class_name(precursor_class) << ")"
           << " stage=" << stage
           << " → " << target_formation_name(target)
           << "  " << chemical_state.summary()
           << "  " << transformation.summary();
        return os.str();
    }
};

} // namespace database
} // namespace vsepr
