#pragma once
/**
 * bead.hpp — Coarse-Grained Bead
 *
 * A bead is the fundamental unit in the coarse-grained representation.
 * Each bead maps to a group of atomistic particles and carries aggregate
 * physical properties (mass, charge, position, type).
 *
 * Design:  Plain-old-data with named accessors.
 * Philosophy: Anti-black-box — every field is inspectable, every mapping
 *             is traceable back to the parent atom group.
 */

#include "atomistic/core/state.hpp"
#include "coarse_grain/core/surface_descriptor.hpp"
#include "coarse_grain/core/multi_channel_descriptor.hpp"
#include "coarse_grain/core/unified_descriptor.hpp"
#include "coarse_grain/core/orientation.hpp"
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace coarse_grain {

// ============================================================================
// Structural Role Signature (Σ_i)
// ============================================================================

/**
 * StructuralRole — discrete structural prior encoding dominant bonding
 * topology, as specified in §0 Identity–State Decomposition Framework.
 *
 *   Σ_i ∈ {0, 1, 2, 3, 4}
 *
 * This parameter biases interaction kernel selection and channel weights
 * without imposing explicit bond constraints.
 *
 * Reference: docs/section0_identity_state_decomposition.tex §0.1.4
 */
enum class StructuralRole : uint8_t {
    Inert             = 0,   // Closed-shell / noble-gas-like
    IonicDominant     = 1,   // Electrostatic-dominant bonding
    DirectionalCovalent = 2, // Directional sp/sp2/sp3 covalent
    Metallic          = 3,   // Delocalized / metallic bonding
    Mixed             = 4    // Transitional / multi-character
};

inline const char* structural_role_name(StructuralRole role) {
    switch (role) {
        case StructuralRole::Inert:              return "Inert";
        case StructuralRole::IonicDominant:      return "Ionic-Dominant";
        case StructuralRole::DirectionalCovalent: return "Directional-Covalent";
        case StructuralRole::Metallic:           return "Metallic";
        case StructuralRole::Mixed:              return "Mixed";
        default:                                 return "Unknown";
    }
}

/**
 * StructuralRoleWeights — per-channel interaction weight bias derived
 * from the structural role Σ_i.
 *
 * Each role produces a triplet (w_steric, w_electrostatic, w_dispersion)
 * that scales the per-channel coupling lambdas in the interaction engine.
 * Weights are normalised so that the dominant channel for each role
 * receives the strongest contribution.
 *
 * These are NOT arbitrary: they encode the known physics of each
 * bonding regime:
 *   Inert:      dispersion-only (vdW solids)
 *   Ionic:      electrostatic-dominant (Madelung energy)
 *   Covalent:   steric-dominant (directionality matters)
 *   Metallic:   dispersion + steric (EAM-like, delocalised)
 *   Mixed:      equal weighting (no prior)
 */
struct StructuralRoleWeights {
    double w_steric{1.0};
    double w_electrostatic{1.0};
    double w_dispersion{1.0};
};

/**
 * Derive channel weight biases from a structural role.
 *
 * Returns weights in [0.1, 1.5] range — they multiply the existing
 * lambda_k coupling constants, never zeroing a channel completely.
 */
inline StructuralRoleWeights role_weights(StructuralRole role) {
    switch (role) {
        case StructuralRole::Inert:
            // Noble gases, closed shells: dispersion dominates
            return {0.3, 0.1, 1.5};

        case StructuralRole::IonicDominant:
            // NaCl, MgO: electrostatic dominates
            return {0.5, 1.5, 0.3};

        case StructuralRole::DirectionalCovalent:
            // Diamond, organics: steric/directional dominates
            return {1.5, 0.5, 0.7};

        case StructuralRole::Metallic:
            // Cu, Au, Fe: dispersion + steric, weak electrostatic
            return {1.0, 0.2, 1.3};

        case StructuralRole::Mixed:
        default:
            // No prior — equal weighting
            return {1.0, 1.0, 1.0};
    }
}

/**
 * Compute the combined pairwise role weights for a bead pair (A, B).
 *
 * Uses geometric mean of per-bead weights (analogous to Lorentz-Berthelot
 * for LJ parameters):
 *
 *   w_k^{AB} = sqrt(w_k^A · w_k^B)
 */
inline StructuralRoleWeights combined_role_weights(
    StructuralRole role_A, StructuralRole role_B)
{
    auto wA = role_weights(role_A);
    auto wB = role_weights(role_B);
    return {
        std::sqrt(wA.w_steric * wB.w_steric),
        std::sqrt(wA.w_electrostatic * wB.w_electrostatic),
        std::sqrt(wA.w_dispersion * wB.w_dispersion)
    };
}

// ============================================================================
// Stability Class (Λ_i)
// ============================================================================

/**
 * StabilityClass — statistical persistence under thermal and
 * configurational perturbation.
 *
 *   Λ_i = argmax_k P_i(survival | ΔE, T, t)
 *
 * Reference: docs/section0_identity_state_decomposition.tex §0.1.5
 */
enum class StabilityClass : uint8_t {
    Transient       = 0,   // Unstable under any perturbation
    Metastable      = 1,   // Kinetically trapped
    AmbientStable   = 2,   // Stable at room temperature
    BulkLattice     = 3    // Bulk crystalline candidate
};

inline const char* stability_class_name(StabilityClass sc) {
    switch (sc) {
        case StabilityClass::Transient:     return "Transient";
        case StabilityClass::Metastable:    return "Metastable";
        case StabilityClass::AmbientStable: return "Ambient-Stable";
        case StabilityClass::BulkLattice:   return "Bulk-Lattice";
        default:                            return "Unknown";
    }
}

// ============================================================================
// BeadType
// ============================================================================

/**
 * BeadType — classification tag for a bead.
 *
 * Examples:
 *   "BB"   — backbone bead (polymer)
 *   "SC1"  — side-chain bead 1
 *   "W"    — water bead (4:1 mapping)
 *   "ION"  — ionic bead
 */
struct BeadType {
    std::string name;          // Human-readable label
    uint32_t    id{};          // Numeric id (unique within a mapping scheme)
    double      sigma{};       // Effective LJ sigma (Å)
    double      epsilon{};     // Effective LJ epsilon (kcal/mol)
};

/**
 * Bead — one coarse-grained site.
 *
 * Stores both the CG-level state AND the provenance (parent atom indices).
 * This is the anti-black-box contract: you can always trace a bead back
 * to the atoms that produced it.
 */
struct Bead {
    // --- CG-level state ---
    atomistic::Vec3 position{};     // Center of bead (Å)
    atomistic::Vec3 velocity{};     // Bead velocity (Å/fs)
    double          mass{};         // Sum of parent atom masses (amu)
    double          charge{};       // Sum of parent atom charges (e)
    uint32_t        type_id{};      // Index into BeadType table

    // --- Identity–State Decomposition (§0) ---
    StructuralRole  structural_role{StructuralRole::Mixed};  // Σ_i: bonding topology prior
    StabilityClass  stability_class{StabilityClass::AmbientStable}; // Λ_i: persistence class

    // --- Provenance (mapping traceability) ---
    std::vector<uint32_t> parent_atom_indices;   // Indices into atomistic::State
    uint32_t              mapping_rule_id{};      // Which MappingRule produced this bead

    // --- Diagnostics ---
    atomistic::Vec3 com_position{};     // Center-of-mass position (reference)
    atomistic::Vec3 cog_position{};     // Center-of-geometry position (reference)
    double          mapping_residual{}; // |COM - COG| distance (Å), mapping quality metric

    // --- Anisotropic Surface Data (optional) ---
    std::optional<SurfaceDescriptor> surface;   // Populated by SurfaceMapper
    bool has_surface_data() const { return surface.has_value(); }

    // --- Multi-Channel Descriptor (optional, enriched representation) ---
    std::optional<MultiChannelDescriptor> multi_channel;  // Populated by MultiChannelMapper
    bool has_multi_channel_data() const { return multi_channel.has_value(); }

    // --- Unified Descriptor (adaptive resolution, supersedes tier-based selection) ---
    std::optional<UnifiedDescriptor> unified;  // Single formalism, adaptive truncation
    bool has_unified_data() const { return unified.has_value(); }

    // --- Orientation (for orientation-coupled interactions) ---
    BeadOrientation orientation{};              // Primary axis from inertia frame
    bool            has_orientation{false};     // True if orientation was computed

    // Convenience accessors for surface properties
    double asphericity() const {
        return surface ? surface->asphericity() : 0.0;
    }
    double anisotropy_ratio() const {
        return surface ? surface->anisotropy_ratio() : 0.0;
    }
};

/**
 * Projection mode for computing bead center from parent atoms.
 */
enum class ProjectionMode {
    CENTER_OF_MASS,      // Weighted by atomic masses: r = Σ(m_i * r_i) / Σ(m_i)
    CENTER_OF_GEOMETRY   // Unweighted average:        r = Σ(r_i) / N
};

// ============================================================================
// Structural Role Classifier
// ============================================================================

/**
 * Infer the structural role Σ_i from atomistic data.
 *
 * Classification heuristic based on Z values of the constituent atoms:
 *
 *   - All noble gases (Z ∈ {2,10,18,36,54,86})           → Inert
 *   - Contains alkali/alkaline earth + halogen/chalcogen   → IonicDominant
 *   - All atoms are non-metals (Z ∈ {1,5,6,7,8,9,14,...}) → DirectionalCovalent
 *   - Contains transition metals (21≤Z≤30, 39≤Z≤48, ...)  → Metallic
 *   - Otherwise                                            → Mixed
 *
 * @param atomic_numbers  The Z values of atoms in the bead's parent group
 * @return The inferred structural role
 */
inline StructuralRole classify_structural_role(
    const std::vector<uint8_t>& atomic_numbers)
{
    if (atomic_numbers.empty()) return StructuralRole::Mixed;

    bool has_noble = false;
    bool all_noble = true;
    bool has_alkali_ae = false;    // Alkali / alkaline earth
    bool has_halogen_chalc = false; // Halogen / chalcogen
    bool has_transition = false;
    bool all_nonmetal = true;

    for (uint8_t Z : atomic_numbers) {
        // Noble gases
        bool noble = (Z == 2 || Z == 10 || Z == 18 || Z == 36 || Z == 54 || Z == 86);
        if (noble) has_noble = true;
        else all_noble = false;

        // Alkali metals (1=H excluded) and alkaline earth
        bool alkali = (Z == 3 || Z == 11 || Z == 19 || Z == 37 || Z == 55 || Z == 87);
        bool alk_earth = (Z == 4 || Z == 12 || Z == 20 || Z == 38 || Z == 56 || Z == 88);
        if (alkali || alk_earth) has_alkali_ae = true;

        // Halogens and chalcogens (O, S, Se, Te)
        bool halogen = (Z == 9 || Z == 17 || Z == 35 || Z == 53 || Z == 85);
        bool chalcogen = (Z == 8 || Z == 16 || Z == 34 || Z == 52);
        if (halogen || chalcogen) has_halogen_chalc = true;

        // Transition metals: 3d(21-30), 4d(39-48), 5d(72-80), lanthanides, actinides
        bool transition = (Z >= 21 && Z <= 30) || (Z >= 39 && Z <= 48)
                       || (Z >= 72 && Z <= 80) || (Z >= 57 && Z <= 71)
                       || (Z >= 89 && Z <= 103);
        if (transition) has_transition = true;

        // Non-metals: H, B, C, N, O, F, Si, P, S, Cl, Se, Br, I, At + noble
        bool nonmetal = (Z == 1 || Z == 5 || Z == 6 || Z == 7 || Z == 8 || Z == 9
                      || Z == 14 || Z == 15 || Z == 16 || Z == 17 || Z == 34 || Z == 35
                      || Z == 53 || Z == 85 || noble);
        if (!nonmetal) all_nonmetal = false;
    }

    if (all_noble) return StructuralRole::Inert;
    if (has_alkali_ae && has_halogen_chalc) return StructuralRole::IonicDominant;
    if (has_transition) return StructuralRole::Metallic;
    if (all_nonmetal) return StructuralRole::DirectionalCovalent;
    return StructuralRole::Mixed;
}

} // namespace coarse_grain
