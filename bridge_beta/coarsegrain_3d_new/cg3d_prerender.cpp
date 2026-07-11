// ============================================================================
// cg3d_prerender.cpp - Bridge Beta: CG Bead -> Glass 3D Pipeline
// ============================================================================

#include "cg3d_prerender.hpp"

namespace vsepr {
namespace bridge_beta {

// -----------------------------------------------------------------------
// Structural role -> pseudo-Z for CPK color mapping
// -----------------------------------------------------------------------
uint16_t CG3DSettings::role_to_Z(uint8_t role) const {
    switch (role) {
        case 0: return 18;  // Inert        -> Ar (teal-grey)
        case 1: return 11;  // IonicDominant -> Na (purple)
        case 2: return 6;   // Covalent     -> C  (dark grey)
        case 3: return 29;  // Metallic     -> Cu (copper)
        case 4: return 16;  // Mixed        -> S  (yellow)
        default: return 10; // Unknown      -> Ne (pale blue)
    }
}

// -----------------------------------------------------------------------
// Constructor
// -----------------------------------------------------------------------
CG3DPrerender::CG3DPrerender(CG3DSettings s)
    : settings_(std::move(s)) {}

// -----------------------------------------------------------------------
// to_topology: BeadSystem -> GlassMolecule
// -----------------------------------------------------------------------
glass::GlassMolecule CG3DPrerender::to_topology(
    const coarse_grain::BeadSystem& system) const
{
    glass::GlassMolecule mol;
    mol.atoms.reserve(system.beads.size());

    // Map each bead to a GlassAtom
    for (uint32_t i = 0; i < static_cast<uint32_t>(system.beads.size()); ++i) {
        const auto& bead = system.beads[i];
        glass::GlassAtom atom;
        atom.index = i;
        // Map structural role to pseudo atomic number for coloring
        atom.atomic_number = settings_.role_to_Z(
            static_cast<uint8_t>(bead.structural_role));
        // Bead radius from mass (rough: r ~ mass^(1/3) scaled)
        atom.covalent_radius = settings_.bead_radius_scale *
            std::cbrt(static_cast<float>(bead.mass) / 12.0f);
        mol.atoms.push_back(atom);
    }

    // Map bead-bead bonds
    mol.bonds.reserve(system.bonds.size());
    for (uint32_t i = 0; i < static_cast<uint32_t>(system.bonds.size()); ++i) {
        const auto& [a, b] = system.bonds[i];
        glass::GlassBond bond;
        bond.index = i;
        bond.a = a;
        bond.b = b;
        bond.order = glass::BondOrder::Single;
        mol.bonds.push_back(bond);

        // Wire adjacency
        mol.atoms[a].bond_ids.push_back(i);
        mol.atoms[b].bond_ids.push_back(i);
    }

    return mol;
}

// -----------------------------------------------------------------------
// prerender: full pipeline BeadSystem -> topology -> layout -> buffers
// -----------------------------------------------------------------------
CG3DResult CG3DPrerender::prerender(
    const coarse_grain::BeadSystem& system) const
{
    CG3DResult result;

    // Step 1: Convert to topology
    result.topology = to_topology(system);
    if (result.topology.atoms.empty()) return result;

    // Step 2: Detect rings in the bead connectivity
    auto rings = glass::detect_rings(result.topology, 8);

    // Step 3: 3D layout
    glass::TopologyPrerender3D layout_engine(settings_.layout);
    result.layout = layout_engine.build_layout(result.topology);

    // Step 4: Build instance buffers
    glass::PrerenderSettings ps;
    ps.atom_radius_scale = settings_.bead_radius_scale;
    ps.bond_radius = settings_.bond_radius;
    glass::MoleculePrerenderBuilder builder(ps);
    result.buffers = builder.build(result.topology, result.layout, rings);

    // Step 5: Optionally inject orientation markers as extra bond instances
    if (settings_.show_orientation) {
        for (uint32_t i = 0; i < static_cast<uint32_t>(system.beads.size()); ++i) {
            const auto& bead = system.beads[i];
            if (!bead.has_orientation) continue;

            const auto& pos = result.layout.atom_positions[i];
            const auto& axis = bead.orientation.normal;
            float len = settings_.orientation_length;

            glass::BondInstance marker;
            marker.endpoint_a = pos;
            marker.endpoint_b = glass::Vec3f{
                pos.x + static_cast<float>(axis.x) * len,
                pos.y + static_cast<float>(axis.y) * len,
                pos.z + static_cast<float>(axis.z) * len
            };
            marker.radius = settings_.bond_radius * 0.6f;
            marker.bond_index = static_cast<uint32_t>(result.buffers.bond_instances.size());
            marker.bond_order = 1;
            marker.style_flags = glass::StyleFlags::Highlighted;
            result.buffers.bond_instances.push_back(marker);
        }
    }

    return result;
}

// -----------------------------------------------------------------------
// CG3DResult::write_svg
// -----------------------------------------------------------------------
bool CG3DResult::write_svg(const std::string& path,
                           const glass::ReportSettings& rs) const {
    glass::ReportRenderer renderer(rs);
    return renderer.write_svg(path, buffers);
}

} // namespace bridge_beta
} // namespace vsepr



