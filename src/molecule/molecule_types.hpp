#pragma once
// ============================================================================
// molecule_types.hpp — Glass Module: Topology Layer
// ============================================================================
// Internal types for the prerender molecule system. These are lightweight
// topology-only structures optimised for fast graph walk layout. They bridge
// to the existing vsepr::Molecule / vsepr::FrameSnapshot types but carry no
// simulation state.
//
// Naming: "Glass" refers to the prerender pipeline — transparent pass-through
// of topology into renderable instance buffers.
// ============================================================================

#include <cstdint>
#include <vector>

namespace vsepr {
namespace glass {

// -----------------------------------------------------------------------
// Bond order (strongly typed, supports future bond-order styling)
// -----------------------------------------------------------------------
enum class BondOrder : uint8_t {
    Single   = 1,
    Double   = 2,
    Triple   = 3,
    Aromatic = 4
};

// -----------------------------------------------------------------------
// GlassAtom — one atom in the topology graph
// -----------------------------------------------------------------------
struct GlassAtom {
    uint32_t index{};
    uint16_t atomic_number{};
    float    covalent_radius{};          // Å (from periodic table or caller)
    std::vector<uint32_t> bond_ids;      // indices into GlassMolecule::bonds
};

// -----------------------------------------------------------------------
// GlassBond — one bond in the topology graph
// -----------------------------------------------------------------------
struct GlassBond {
    uint32_t index{};
    uint32_t a{};                        // atom index
    uint32_t b{};                        // atom index
    BondOrder order{BondOrder::Single};
};

// -----------------------------------------------------------------------
// GlassMolecule — adjacency-list topology container
// -----------------------------------------------------------------------
struct GlassMolecule {
    std::vector<GlassAtom> atoms;
    std::vector<GlassBond> bonds;

    // Return the neighbour atom across a given bond from `atom_idx`
    uint32_t other(uint32_t bond_id, uint32_t atom_idx) const {
        const auto& b = bonds[bond_id];
        return (b.a == atom_idx) ? b.b : b.a;
    }
};

} // namespace glass
} // namespace vsepr
