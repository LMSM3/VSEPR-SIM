#pragma once
/**
 * markdown_report.hpp  --  Day 84 / WO-84E
 * ============================================================================
 * Markdown report wiring for VSEPR + OrganicCandidate intelligence.
 *
 * Produces a single deterministic Markdown document with the required WO-84E
 * sections:
 *
 *   [VSEPR]            central_atom, bonding_domains, lone_pairs, geometry,
 *                      provider_source, fallback_used, confidence
 *   [OrganicCandidate] organic_score, rings, aromatic, bond_orders,
 *                      rotatable_bonds, functional_groups, provider_source,
 *                      fallback_used
 *   [SceneHints]       host_object, adsorbates, surface_sites,
 *                      residue_candidates, unsupported_features
 *
 * The report also carries a "Known limitations" block so the engine's gaps are
 * visible rather than hidden.
 */

#include "atomistic/classify/vsepr.hpp"
#include "atomistic/classify/organic_candidate.hpp"
#include "atomistic/classify/bond_order_provider.hpp"
#include "atomistic/export/organic_candidate_export.hpp"

#include <string>

namespace atomistic {
namespace classify {

// Build the full Markdown report.  `title` is used as the top-level heading
// (e.g. the molecule formula or script name).
std::string build_markdown_report(const std::string& title,
								   const VSEPRReport& vsepr,
								   const OrganicCandidate& candidate,
								   const BondOrderTable& bond_orders,
								   const SceneHints& hints);

} // namespace classify
} // namespace atomistic
