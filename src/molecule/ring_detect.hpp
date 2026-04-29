#pragma once
// ============================================================================
// ring_detect.hpp — Glass Module: Small Cycle Detection
// ============================================================================
// Detects small rings (3–8 membered) in the topology graph so the layout
// engine can pin ring atoms to planar polygons before placing substituents.
// ============================================================================

#include "molecule_types.hpp"
#include <vector>
#include <cstdint>

namespace vsepr {
namespace glass {

// -----------------------------------------------------------------------
// A detected ring: ordered list of atom indices forming a simple cycle.
// -----------------------------------------------------------------------
struct Ring {
    std::vector<uint32_t> atom_indices;
    uint32_t size() const { return static_cast<uint32_t>(atom_indices.size()); }
};

// -----------------------------------------------------------------------
// Detect all simple rings up to `max_size` members (default 8).
// Returns rings sorted by size (smallest first).
// Uses a bounded DFS approach — not SSSR, but sufficient for layout.
// -----------------------------------------------------------------------
std::vector<Ring> detect_rings(const GlassMolecule& mol, uint32_t max_size = 8);

} // namespace glass
} // namespace vsepr
