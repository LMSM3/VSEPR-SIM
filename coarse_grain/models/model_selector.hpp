#pragma once
/**
 * model_selector.hpp — Two-Tier Model Selection for CG Bead Interactions
 *
 * NOTE: SUPERSEDED by the Unified Descriptor Strategy.
 *
 * This file is preserved as a historical record of the two-tier model
 * selection approach. The discrete tier-based architecture (hard if-statement
 * branching on atom count, metal center, anisotropy threshold) has been
 * superseded by the unified descriptor formalism in:
 *
 *   coarse_grain/core/unified_descriptor.hpp    — single adaptive descriptor
 *   coarse_grain/models/unified_potential.hpp    — single energy evaluator
 *   coarse_grain/models/descriptor_residual.hpp  — residual-driven promotion
 *
 * The unified approach uses the same descriptor object and energy machinery
 * for all systems, with model complexity adjusted through adaptive truncation
 * (ℓ_max and channel activation) rather than discrete model switching.
 *
 * See: "Unified Descriptor Strategy" section of section_anisotropic_beads.tex
 *
 * --- Original documentation (preserved for history) ---
 *
 * Implements the hierarchical model selection strategy described in the
 * "Model Selection Guidelines" section of section_anisotropic_beads.tex.
 *
 * Two tiers:
 *
 *   Tier 1 (Reduced):
 *     - Orientation-coupled potential U_eff(r, θ_A, θ_B, φ)
 *     - For rigid, compact, smoothly anisotropic systems (e.g. benzene)
 *     - Low-order SH expansion is sufficient
 *
 *   Tier 2 (Enriched):
 *     - Multi-channel descriptor model {S^(steric), S^(elec), S^(disp)}
 *     - For sterically complex, chemically heterogeneous, or metal-centered systems
 *     - Higher-order SH expansion and multiple interaction channels
 *
 * Selection criteria:
 *   - Atom count ≥ 18 → promotes to Tier 2
 *   - Metal center present → promotes to Tier 2
 *   - High angular complexity (anisotropy ratio above threshold) → promotes to Tier 2
 *   - Definitive criterion is angular complexity of the surface descriptor
 *
 * Anti-black-box: the selection decision, all input criteria, and the
 * resulting tier are explicitly stored and inspectable.
 *
 * Reference: Section "Model Selection Guidelines" of
 *            section_anisotropic_beads.tex
 */

#include "coarse_grain/core/bead.hpp"
#include "coarse_grain/core/surface_descriptor.hpp"
#include "coarse_grain/core/multi_channel_descriptor.hpp"
#include "coarse_grain/core/orientation.hpp"
#include "coarse_grain/models/orientation_potential.hpp"
#include "coarse_grain/models/multi_channel_potential.hpp"
#include "coarse_grain/models/anisotropic_potential.hpp"
#include <cmath>
#include <cstdint>

namespace coarse_grain {

// ============================================================================
// Model Tier
// ============================================================================

/**
 * ModelTier — which interaction model to apply.
 */
enum class ModelTier {
    REDUCED  = 1,   // Tier 1: orientation-coupled reduced model
    ENRICHED = 2    // Tier 2: multi-channel enriched descriptor model
};

/**
 * Return human-readable name for a model tier.
 */
inline const char* tier_name(ModelTier tier) {
    switch (tier) {
        case ModelTier::REDUCED:  return "Tier 1: Reduced Anisotropic";
        case ModelTier::ENRICHED: return "Tier 2: Enriched Descriptor";
        default:                  return "unknown";
    }
}

// ============================================================================
// Selection Configuration
// ============================================================================

/**
 * ModelSelectionConfig — thresholds and flags for tier selection.
 *
 * All parameters are explicit and inspectable.
 */
struct ModelSelectionConfig {
    /// Atom-count heuristic threshold (N ≥ threshold → Tier 2 candidate)
    uint32_t atom_count_threshold = 18;

    /// If true, presence of a metal center forces Tier 2
    bool metal_center_promotes = true;

    /// Anisotropy ratio threshold: above this → Tier 2 required
    /// This is the definitive criterion per the theory
    double anisotropy_threshold = 0.45;

    /// If true, always prefer Tier 2 when multi-channel data is available
    bool prefer_enriched_when_available = false;
};

// ============================================================================
// Selection Result
// ============================================================================

/**
 * ModelSelectionResult — records the decision and all input criteria.
 *
 * Anti-black-box: every factor contributing to the selection is stored.
 */
struct ModelSelectionResult {
    ModelTier tier{ModelTier::REDUCED};

    // Input criteria used
    uint32_t atom_count{};
    bool     has_metal_center{false};
    double   anisotropy_ratio{};
    bool     has_surface_data{false};
    bool     has_multi_channel_data{false};

    // Which criterion triggered promotion (if any)
    bool promoted_by_atom_count{false};
    bool promoted_by_metal_center{false};
    bool promoted_by_anisotropy{false};
    bool promoted_by_availability{false};

    /**
     * Human-readable explanation of the selection decision.
     */
    const char* reason() const {
        if (promoted_by_anisotropy)    return "angular complexity exceeds threshold";
        if (promoted_by_metal_center)  return "metal center detected";
        if (promoted_by_atom_count)    return "atom count exceeds heuristic threshold";
        if (promoted_by_availability)  return "enriched descriptor data available";
        return "default: reduced model sufficient";
    }
};

// ============================================================================
// Selection Logic
// ============================================================================

/**
 * Determine the appropriate model tier for a bead.
 *
 * The selection follows the hierarchy from the theory:
 *   1. Angular complexity (definitive criterion — overrides everything)
 *   2. Metal center presence (structural criterion)
 *   3. Atom count heuristic (guideline threshold)
 *   4. Data availability (optional override)
 *
 * @param bead    The CG bead to evaluate
 * @param config  Selection thresholds
 * @return ModelSelectionResult with tier and full decision record
 */
inline ModelSelectionResult select_model_tier(
    const Bead& bead,
    const ModelSelectionConfig& config = {})
{
    ModelSelectionResult result;
    result.atom_count          = static_cast<uint32_t>(bead.parent_atom_indices.size());
    result.has_surface_data    = bead.has_surface_data();
    result.has_multi_channel_data = bead.has_multi_channel_data();

    // Compute anisotropy ratio from whichever descriptor is available
    if (bead.has_multi_channel_data()) {
        result.anisotropy_ratio = bead.multi_channel->max_anisotropy();
    } else if (bead.has_surface_data()) {
        result.anisotropy_ratio = bead.surface->anisotropy_ratio();
    }

    // Default to Tier 1
    result.tier = ModelTier::REDUCED;

    // --- Definitive criterion: angular complexity ---
    if (result.anisotropy_ratio > config.anisotropy_threshold) {
        result.tier = ModelTier::ENRICHED;
        result.promoted_by_anisotropy = true;
        return result;
    }

    // --- Metal center ---
    if (config.metal_center_promotes && result.has_metal_center) {
        result.tier = ModelTier::ENRICHED;
        result.promoted_by_metal_center = true;
        return result;
    }

    // --- Atom count heuristic ---
    if (result.atom_count >= config.atom_count_threshold) {
        result.tier = ModelTier::ENRICHED;
        result.promoted_by_atom_count = true;
        return result;
    }

    // --- Availability preference ---
    if (config.prefer_enriched_when_available && result.has_multi_channel_data) {
        result.tier = ModelTier::ENRICHED;
        result.promoted_by_availability = true;
        return result;
    }

    return result;
}

/**
 * Convenience: select tier using atom count and anisotropy ratio directly.
 *
 * Useful when you don't have a full Bead struct (e.g. during mapping).
 */
inline ModelSelectionResult select_model_tier_from_metrics(
    uint32_t atom_count,
    double anisotropy_ratio,
    bool has_metal_center = false,
    const ModelSelectionConfig& config = {})
{
    ModelSelectionResult result;
    result.atom_count       = atom_count;
    result.anisotropy_ratio = anisotropy_ratio;
    result.has_metal_center = has_metal_center;
    result.tier             = ModelTier::REDUCED;

    // Definitive: angular complexity
    if (anisotropy_ratio > config.anisotropy_threshold) {
        result.tier = ModelTier::ENRICHED;
        result.promoted_by_anisotropy = true;
        return result;
    }

    // Metal center
    if (config.metal_center_promotes && has_metal_center) {
        result.tier = ModelTier::ENRICHED;
        result.promoted_by_metal_center = true;
        return result;
    }

    // Atom count heuristic
    if (atom_count >= config.atom_count_threshold) {
        result.tier = ModelTier::ENRICHED;
        result.promoted_by_atom_count = true;
        return result;
    }

    return result;
}

// ============================================================================
// Unified Interaction Result
// ============================================================================

/**
 * UnifiedInteractionResult — energy from either tier, with provenance.
 *
 * Records which tier was used and the decomposed energy so that the
 * interaction pathway is always inspectable.
 */
struct UnifiedInteractionResult {
    ModelTier tier{ModelTier::REDUCED};

    double E_isotropic{};         // LJ baseline
    double E_anisotropic{};       // Orientation / channel correction
    double E_total{};             // Total energy

    // Tier 2 decomposition (zero if Tier 1 used)
    double E_steric{};
    double E_electrostatic{};
    double E_dispersion{};
};

/**
 * Evaluate bead-bead interaction using the appropriate tier.
 *
 * Routes to orientation_potential (Tier 1) or multi_channel_potential (Tier 2)
 * based on the selected model tier.
 *
 * @param bead_A, bead_B     The two interacting beads
 * @param tier               Which model tier to use (from select_model_tier)
 * @param reduced_params     Parameters for Tier 1 (orientation model)
 * @param enriched_params    Parameters for Tier 2 (multi-channel model)
 * @return UnifiedInteractionResult with energy and tier provenance
 */
inline UnifiedInteractionResult evaluate_tiered_interaction(
    const Bead& bead_A,
    const Bead& bead_B,
    ModelTier tier,
    const OrientationPotentialParams& reduced_params,
    const MultiChannelPotentialParams& enriched_params)
{
    UnifiedInteractionResult result;
    result.tier = tier;

    if (tier == ModelTier::ENRICHED &&
        bead_A.has_multi_channel_data() && bead_B.has_multi_channel_data())
    {
        // Tier 2: multi-channel enriched model
        atomistic::Vec3 r_vec = {
            bead_B.position.x - bead_A.position.x,
            bead_B.position.y - bead_A.position.y,
            bead_B.position.z - bead_A.position.z
        };

        auto mc_result = multi_channel_potential(r_vec,
                                                  *bead_A.multi_channel,
                                                  *bead_B.multi_channel,
                                                  enriched_params);

        result.E_isotropic     = mc_result.E_isotropic;
        result.E_anisotropic   = mc_result.E_anisotropic_total;
        result.E_total         = mc_result.E_total;
        result.E_steric        = mc_result.E_steric;
        result.E_electrostatic = mc_result.E_electrostatic;
        result.E_dispersion    = mc_result.E_dispersion;
    }
    else
    {
        // Tier 1: reduced orientation-coupled model
        auto ori_result = evaluate_orientation_potential(
            bead_A.position, bead_B.position,
            bead_A.orientation, bead_B.orientation,
            reduced_params);

        result.E_isotropic   = ori_result.E_isotropic;
        result.E_anisotropic = ori_result.E_alignment + ori_result.E_axis_A + ori_result.E_axis_B;
        result.E_total       = ori_result.E_total;

        // Fall back to Tier 1 if Tier 2 was selected but data unavailable
        if (tier == ModelTier::ENRICHED)
            result.tier = ModelTier::REDUCED;
    }

    return result;
}

} // namespace coarse_grain
