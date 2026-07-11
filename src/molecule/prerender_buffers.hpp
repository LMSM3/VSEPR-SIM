#pragma once
// ============================================================================
// prerender_buffers.hpp — Glass Module: Instance Buffer Builder
// ============================================================================
// Converts layout positions + topology into flat instance buffers ready for
// GPU upload. One shared sphere mesh + one shared cylinder mesh; instances
// carry transform + type + style flags.
// ============================================================================

#include "molecule_types.hpp"
#include "layout_prerender3d.hpp"
#include "render_style_keys.hpp"
#include <vector>
#include <cstdint>

namespace vsepr {
namespace glass {

// -----------------------------------------------------------------------
// AtomInstance — per-atom data for instanced sphere rendering.
// -----------------------------------------------------------------------
struct AtomInstance {
    Vec3f    position;
    float    radius;
    uint32_t atom_index;
    uint32_t atom_type;       // Z
    uint32_t style_flags;
};

// -----------------------------------------------------------------------
// BondInstance — per-bond data for instanced cylinder rendering.
// Stores endpoints; the renderer computes midpoint/orientation/length.
// -----------------------------------------------------------------------
struct BondInstance {
    Vec3f    endpoint_a;
    Vec3f    endpoint_b;
    float    radius;
    uint32_t bond_index;
    uint32_t bond_order;      // 1,2,3,4
    uint32_t style_flags;
};

// -----------------------------------------------------------------------
// PrerenderBuffers — output of the instance buffer builder.
// -----------------------------------------------------------------------
struct PrerenderBuffers {
    std::vector<AtomInstance> atom_instances;
    std::vector<BondInstance> bond_instances;

    bool empty() const { return atom_instances.empty() && bond_instances.empty(); }
    size_t atom_count() const { return atom_instances.size(); }
    size_t bond_count() const { return bond_instances.size(); }
};

// -----------------------------------------------------------------------
// PrerenderSettings — controls instance generation.
// -----------------------------------------------------------------------
struct PrerenderSettings {
    float atom_radius_scale  = 0.65f;    // scale covalent_radius -> display radius
    float bond_radius        = 0.11f;    // cylinder radius for single bonds
    float double_bond_offset = 0.08f;    // lateral offset for double bond pairs
    float triple_bond_offset = 0.10f;    // lateral offset for triple bond triples
};

// -----------------------------------------------------------------------
// MoleculePrerenderBuilder — builds instance buffers from topology + layout.
//
// Usage:
//   MoleculePrerenderBuilder builder(prerender_settings);
//   PrerenderBuffers buffers = builder.build(mol, layout, rings);
// -----------------------------------------------------------------------
class MoleculePrerenderBuilder {
public:
    explicit MoleculePrerenderBuilder(PrerenderSettings s = {});

    PrerenderBuffers build(
        const GlassMolecule& mol,
        const LayoutResult& layout,
        const std::vector<Ring>& rings = {}
    ) const;

private:
    PrerenderSettings settings_;

    uint32_t compute_atom_flags(
        const GlassMolecule& mol,
        uint32_t atom_idx,
        const std::vector<bool>& is_ring_atom
    ) const;

    uint32_t compute_bond_flags(
        const GlassMolecule& mol,
        uint32_t bond_idx,
        const std::vector<bool>& is_ring_atom
    ) const;
};

} // namespace glass
} // namespace vsepr
