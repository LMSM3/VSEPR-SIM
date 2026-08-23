#pragma once
/**
 * vsepr_export.hpp  --  Day 84 / WO-84E
 * ============================================================================
 * JSON-lite export for VSEPRReport.
 *
 * "JSON-lite" = deterministic, dependency-free, hand-emitted JSON.  No external
 * JSON library, no ordering ambiguity.  Suitable for reports, fixtures, and
 * downstream tooling.
 *
 * Exported fields (per the WO-84E report contract):
 *   central_atom     -- Z of the representative central site
 *   bonding_domains  -- bonded-domain count
 *   lone_pairs       -- lone-pair domain count
 *   geometry         -- molecular shape string
 *   provider_source  -- "provider" | "element" | "geometry" | "none"
 *   fallback_used    -- true when element/geometry fallback produced the result
 *   confidence       -- geometry confidence [0,1]
 *
 * The representative "central" site is the atom with the most electron domains
 * (ties broken by lowest index), i.e. the molecular centre.  All sites are also
 * emitted in a "sites" array so nothing is lost.
 */

#include "atomistic/classify/vsepr.hpp"

#include <cstddef>
#include <string>

namespace atomistic {
namespace classify {

// Per-site lone-pair provenance label for a single VSEPRSite.
const char* vsepr_lone_pair_source(const VSEPRSite& site);

// Index of the representative central site (most electron domains, else 0).
// Returns 0 for an empty report.
std::size_t vsepr_central_site_index(const VSEPRReport& report);

// Full JSON-lite document for a VSEPRReport.
std::string vsepr_to_json(const VSEPRReport& report);

} // namespace classify
} // namespace atomistic
