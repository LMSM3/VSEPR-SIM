#pragma once
/**
 * mapping_rule.hpp — Declarative Atom-to-Bead Mapping Rules
 *
 * A MappingRule specifies HOW a group of atoms should collapse into a single bead.
 * Rules are data, not code — they can be serialized, inspected, and reported.
 *
 * This is the anti-black-box layer: every mapping decision is recorded as
 * a named, numbered rule with explicit atom-index lists.
 */

#include <cstdint>
#include <string>
#include <vector>

namespace coarse_grain {

/**
 * AtomSelector — which atoms from the source State belong to this bead.
 *
 * Three modes:
 *   BY_INDICES  — explicit list of atom indices (most common)
 *   BY_TYPE     — all atoms of a given type id
 *   BY_RANGE    — contiguous index range [first, first+count)
 */
enum class SelectorMode {
    BY_INDICES,
    BY_TYPE,
    BY_RANGE
};

struct AtomSelector {
    SelectorMode mode = SelectorMode::BY_INDICES;

    // BY_INDICES
    std::vector<uint32_t> indices;

    // BY_TYPE
    uint32_t type_id{};

    // BY_RANGE
    uint32_t range_first{};
    uint32_t range_count{};
};

/**
 * MappingRule — one rule that produces one bead.
 *
 * Fields:
 *   rule_id      — unique rule identifier (for traceability)
 *   label        — human-readable label (e.g. "backbone", "sidechain_1")
 *   bead_type_id — index into the BeadType table
 *   selector     — which atoms this rule consumes
 */
struct MappingRule {
    uint32_t     rule_id{};
    std::string  label;
    uint32_t     bead_type_id{};
    AtomSelector selector;
};

/**
 * MappingScheme — a complete set of rules for one molecule/system.
 *
 * Invariant: the union of all selectors must cover every atom exactly once.
 * This is enforced by AtomToBeadMapper::validate_scheme().
 */
struct MappingScheme {
    std::string name;                   // Scheme name (e.g. "MARTINI-like", "4:1 water")
    std::vector<MappingRule> rules;

    uint32_t num_rules() const { return static_cast<uint32_t>(rules.size()); }
};

} // namespace coarse_grain
