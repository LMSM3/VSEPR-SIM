#pragma once
/**
 * organic_candidate_export.hpp  --  Day 84 / WO-84E
 * ============================================================================
 * JSON-lite export for OrganicCandidate, plus a forward-looking SceneHints
 * block for the long-term Chemical Scene Compiler / Process Consequence Engine.
 *
 * "JSON-lite" = deterministic, dependency-free, hand-emitted JSON.
 *
 * OrganicCandidate fields exported (per the WO-84E report contract):
 *   organic_score    -- final_score composite
 *   rings            -- ring_count
 *   aromatic         -- aromatic_ring_count > 0
 *   bond_orders      -- inferred bond-order histogram (single/double/triple)
 *   rotatable_bonds  -- rotatable_bond_count
 *   functional_groups
 *   provider_source  -- "provider" | "fallback" | "none"
 *   fallback_used
 *
 * SceneHints (provisional; some fields intentionally empty until resolved-object
 * chemistry lands):
 *   host_object          -- inorganic host guess (e.g. "SiO2") or ""
 *   adsorbates           -- classified adsorbate labels
 *   surface_sites        -- detected surface-site labels
 *   residue_candidates   -- residue/fouling candidate labels
 *   unsupported_features -- explicit list of things not yet handled
 */

#include "atomistic/classify/organic_candidate.hpp"
#include "atomistic/classify/bond_order_provider.hpp"
#include "atomistic/core/state.hpp"

#include <string>
#include <vector>

namespace atomistic {
namespace classify {

// Forward-looking scene hints.  Populated conservatively; unresolved fields are
// listed explicitly under `unsupported_features` so nothing fails silently.
struct SceneHints {
	std::string              host_object;           // e.g. "SiO2" or ""
	std::vector<std::string> adsorbates;            // polar/aromatic/carbonyl tags
	std::vector<std::string> surface_sites;         // e.g. "silanol", "bridging_O"
	std::vector<std::string> residue_candidates;    // fouling/degradation candidates
	std::vector<std::string> unsupported_features;  // explicit known-gaps list
};

// Derive conservative scene hints from a candidate + state.  This is a hint
// generator, not a resolved-object detector; it always records the current
// limitations under `unsupported_features`.
SceneHints derive_scene_hints(const State& state, const OrganicCandidate& cand);

// JSON-lite for a single OrganicCandidate (no scene hints).
std::string organic_candidate_to_json(const OrganicCandidate& cand,
									   const BondOrderTable& bond_orders);

// JSON-lite for a single OrganicCandidate including a SceneHints block.
std::string organic_candidate_to_json(const OrganicCandidate& cand,
									   const BondOrderTable& bond_orders,
									   const SceneHints& hints);

} // namespace classify
} // namespace atomistic
