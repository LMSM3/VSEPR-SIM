#pragma once
/**
 * scale_bridge.hpp — Multi-Scale Property Search Bridge
 * =====================================================
 *
 * Provides the upward/downward scale transitions that allow property
 * searches to cascade from atomistic through grain to macroscopic.
 *
 * The key insight: the mature kernel (v0.1→v0.3→2.7→2.9) now produces
 * deterministic, hash-auditable atomistic structures. This means we
 * can trust the base layer enough to build upward.
 *
 * Scale transitions:
 *   1→2  Atomistic → CG:     Fragment mapping (existing fragment_bridge.hpp)
 *   2→3  CG → Grain:         Bead ensemble → effective medium
 *   3→4  Grain → Component:  Grain assembly → FEA mesh (planned)
 *   4→5  Component → Macro:  Part assembly → system twin (planned)
 *
 * Each transition has:
 *   - A forward (upward) map: fine → coarse
 *   - A backward (downward) map: coarse → fine (refinement)
 *   - A property transfer function: what properties survive the crossing
 *   - A fidelity metric: how much information is lost
 */

#include "version/version_manifest.hpp"
#include <vector>
#include <string>
#include <functional>
#include <cmath>
#include <map>

namespace vsepr {
namespace multiscale {

using version::SimulationScale;
using version::PropertyDomain;

// ============================================================================
// Scale Transition Record
// ============================================================================

struct ScaleTransition {
    SimulationScale from;
    SimulationScale to;
    std::string     name;
    std::string     method;
    double          fidelity;       // 0.0 = total information loss, 1.0 = lossless

    // Properties that survive this crossing
    std::vector<PropertyDomain> preserved_domains;
};

// ============================================================================
// Property Search Result
// ============================================================================

struct PropertySearchResult {
    std::string     property_name;
    std::string     unit;
    double          value;
    double          uncertainty;
    SimulationScale source_scale;
    std::string     scraper_used;
    uint64_t        provenance_hash;    // ties back to structure identity
    bool            converged;
};

// ============================================================================
// Multi-Scale Property Search Engine
// ============================================================================

/**
 * PropertySearchEngine — cascading multi-scale property extraction
 *
 * Given a molecular identity (formula + topology hash), searches across
 * scales 1→5 to extract every computable property. Each extraction is
 * tied to the provenance chain (same hash → same property, always).
 *
 * The "hugely random" aspect: the search explores exotic material
 * candidates using the discovery engine's weighted random sampling,
 * then cascades each candidate through the scale hierarchy to see
 * which properties emerge. This is why the formation physics modules
 * must be robust — they're being hammered with random compositions.
 */
class PropertySearchEngine {
public:
    PropertySearchEngine() = default;

    // ========================================================================
    // Scale transition registry
    // ========================================================================

    void register_transition(const ScaleTransition& t) {
        transitions_.push_back(t);
    }

    const std::vector<ScaleTransition>& transitions() const {
        return transitions_;
    }

    // ========================================================================
    // Default transitions (built from kernel capabilities)
    // ========================================================================

    void register_default_transitions() {
        // Scale 1 → 2: Atomistic → Coarse-Grained
        transitions_.push_back({
            SimulationScale::Atomistic,
            SimulationScale::CoarseGrained,
            "Atomistic → CG",
            "Fragment mapping (Morgan canonical + inertia frame + SH descriptors)",
            0.85,
            {PropertyDomain::Structural, PropertyDomain::Mechanical,
             PropertyDomain::Thermal, PropertyDomain::Stability}
        });

        // Scale 2 → 3: CG → Grain
        transitions_.push_back({
            SimulationScale::CoarseGrained,
            SimulationScale::Grain,
            "CG → Grain",
            "Bead ensemble → effective medium (Voigt-Reuss-Hill bounds + Boltzmann averaging)",
            0.70,
            {PropertyDomain::Mechanical, PropertyDomain::Thermal,
             PropertyDomain::Nuclear}
        });

        // Scale 3 → 4: Grain → Component
        transitions_.push_back({
            SimulationScale::Grain,
            SimulationScale::Component,
            "Grain → Component",
            "Grain assembly → FEA mesh (constitutive law extraction + homogenization)",
            0.60,
            {PropertyDomain::Mechanical, PropertyDomain::Thermal}
        });

        // Scale 4 → 5: Component → Macroscopic
        transitions_.push_back({
            SimulationScale::Component,
            SimulationScale::Macroscopic,
            "Component → System",
            "Part assembly → digital twin (contact + boundary conditions + load paths)",
            0.50,
            {PropertyDomain::Mechanical}
        });
    }

    // ========================================================================
    // Fidelity chain — cumulative information preservation
    // ========================================================================

    /**
     * Compute cumulative fidelity from atomistic up to target scale.
     * Product of all transition fidelities along the chain.
     */
    double cumulative_fidelity(SimulationScale target) const {
        double f = 1.0;
        uint8_t current = 1;
        uint8_t target_val = static_cast<uint8_t>(target);

        while (current < target_val) {
            for (const auto& t : transitions_) {
                if (static_cast<uint8_t>(t.from) == current &&
                    static_cast<uint8_t>(t.to) == current + 1) {
                    f *= t.fidelity;
                    break;
                }
            }
            ++current;
        }
        return f;
    }

    /**
     * Check if a property domain survives to a given scale.
     * Domain must be preserved at every transition in the chain.
     */
    bool domain_survives_to(PropertyDomain domain, SimulationScale target) const {
        uint8_t current = 1;
        uint8_t target_val = static_cast<uint8_t>(target);

        while (current < target_val) {
            bool found = false;
            for (const auto& t : transitions_) {
                if (static_cast<uint8_t>(t.from) == current &&
                    static_cast<uint8_t>(t.to) == current + 1) {
                    // Check if domain is in preserved list
                    for (auto d : t.preserved_domains) {
                        if (d == domain) { found = true; break; }
                    }
                    break;
                }
            }
            if (!found) return false;
            ++current;
        }
        return true;
    }

    // ========================================================================
    // Scale label utilities
    // ========================================================================

    static const char* scale_name(SimulationScale s) {
        switch (s) {
            case SimulationScale::Atomistic:     return "Atomistic";
            case SimulationScale::CoarseGrained: return "Coarse-Grained";
            case SimulationScale::Grain:         return "Grain";
            case SimulationScale::Component:     return "Component";
            case SimulationScale::Macroscopic:   return "Macroscopic";
            default:                             return "Unknown";
        }
    }

    static const char* domain_name(PropertyDomain d) {
        switch (d) {
            case PropertyDomain::Structural: return "Structural";
            case PropertyDomain::Mechanical: return "Mechanical";
            case PropertyDomain::Thermal:    return "Thermal";
            case PropertyDomain::Electronic: return "Electronic";
            case PropertyDomain::Transport:  return "Transport";
            case PropertyDomain::Stability:  return "Stability";
            case PropertyDomain::Nuclear:    return "Nuclear";
            default:                         return "Unknown";
        }
    }

private:
    std::vector<ScaleTransition> transitions_;
};

} // namespace multiscale
} // namespace vsepr
