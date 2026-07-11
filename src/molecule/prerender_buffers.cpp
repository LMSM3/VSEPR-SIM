// ============================================================================
// prerender_buffers.cpp — Glass Module: Instance Buffer Builder
// ============================================================================

#include "prerender_buffers.hpp"
#include <algorithm>

namespace vsepr {
namespace glass {

MoleculePrerenderBuilder::MoleculePrerenderBuilder(PrerenderSettings s)
    : settings_(std::move(s)) {}

uint32_t MoleculePrerenderBuilder::compute_atom_flags(
    const GlassMolecule& mol,
    uint32_t atom_idx,
    const std::vector<bool>& is_ring_atom
) const {
    uint32_t flags = StyleFlags::None;
    if (is_ring_atom[atom_idx])
        flags |= StyleFlags::Ring;
    if (mol.atoms[atom_idx].bond_ids.size() <= 1)
        flags |= StyleFlags::Terminal;
    return flags;
}

uint32_t MoleculePrerenderBuilder::compute_bond_flags(
    const GlassMolecule& mol,
    uint32_t bond_idx,
    const std::vector<bool>& is_ring_atom
) const {
    uint32_t flags = StyleFlags::None;
    const auto& bond = mol.bonds[bond_idx];
    if (is_ring_atom[bond.a] && is_ring_atom[bond.b])
        flags |= StyleFlags::Ring;
    if (bond.order == BondOrder::Aromatic)
        flags |= StyleFlags::Aromatic;
    return flags;
}

PrerenderBuffers MoleculePrerenderBuilder::build(
    const GlassMolecule& mol,
    const LayoutResult& layout,
    const std::vector<Ring>& rings
) const {
    PrerenderBuffers out;

    // Build ring-atom lookup
    std::vector<bool> is_ring_atom(mol.atoms.size(), false);
    for (const auto& ring : rings) {
        for (uint32_t idx : ring.atom_indices) {
            is_ring_atom[idx] = true;
        }
    }

    // --- Atom instances ---
    out.atom_instances.reserve(mol.atoms.size());
    for (const auto& atom : mol.atoms) {
        AtomInstance inst;
        inst.position    = layout.atom_positions[atom.index];
        inst.radius      = atom.covalent_radius * settings_.atom_radius_scale;
        inst.atom_index  = atom.index;
        inst.atom_type   = atom.atomic_number;
        inst.style_flags = compute_atom_flags(mol, atom.index, is_ring_atom);
        out.atom_instances.push_back(inst);
    }

    // --- Bond instances ---
    out.bond_instances.reserve(mol.bonds.size());
    for (const auto& bond : mol.bonds) {
        BondInstance inst;
        inst.endpoint_a  = layout.atom_positions[bond.a];
        inst.endpoint_b  = layout.atom_positions[bond.b];
        inst.radius      = settings_.bond_radius;
        inst.bond_index  = bond.index;
        inst.bond_order  = static_cast<uint32_t>(bond.order);
        inst.style_flags = compute_bond_flags(mol, bond.index, is_ring_atom);
        out.bond_instances.push_back(inst);
    }

    return out;
}

} // namespace glass
} // namespace vsepr
