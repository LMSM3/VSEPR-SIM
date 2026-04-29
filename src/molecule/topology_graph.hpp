#pragma once
// ============================================================================
// topology_graph.hpp — Glass Module: Topology Graph Builder
// ============================================================================
// Converts existing vsepr::Molecule data into the lightweight GlassMolecule
// topology used by the prerender layout engine.
// ============================================================================

#include "molecule_types.hpp"
#include <cstdint>
#include <vector>
#include <utility>

namespace vsepr {
namespace glass {

// -----------------------------------------------------------------------
// Default covalent radii (Å) for common elements when caller doesn't supply.
// Covers Z=1..54 (H through Xe). Returns 1.5 for unknown/heavy.
// -----------------------------------------------------------------------
float default_covalent_radius(uint16_t Z);

// -----------------------------------------------------------------------
// Build a GlassMolecule from flat arrays (the format used by FrameSnapshot
// and AtomicGeometry).
//
//   atomic_numbers  — Z values, one per atom
//   bond_pairs      — (i,j) atom index pairs
//   bond_orders     — optional per-bond order (nullptr → all Single)
//   covalent_radii  — optional per-atom radius (nullptr → use defaults)
// -----------------------------------------------------------------------
GlassMolecule build_topology(
    const std::vector<int>&                          atomic_numbers,
    const std::vector<std::pair<uint32_t,uint32_t>>& bond_pairs,
    const uint8_t*  bond_orders    = nullptr,
    const float*    covalent_radii = nullptr
);

} // namespace glass
} // namespace vsepr
