// ============================================================================
// topology_graph.cpp — Glass Module: Topology Graph Builder
// ============================================================================

#include "topology_graph.hpp"
#include <algorithm>

namespace vsepr {
namespace glass {

// -----------------------------------------------------------------------
// Default covalent radii (Å) — Cordero 2008 compilation, truncated to
// the first 54 elements.  Unknown / heavy atoms fall back to 1.50.
// -----------------------------------------------------------------------
static constexpr float kCovalentRadii[] = {
    0.00f,  // Z=0 placeholder
    0.31f,  // H
    0.28f,  // He
    1.28f,  // Li
    0.96f,  // Be
    0.84f,  // B
    0.76f,  // C
    0.71f,  // N
    0.66f,  // O
    0.57f,  // F
    0.58f,  // Ne
    1.66f,  // Na
    1.41f,  // Mg
    1.21f,  // Al
    1.11f,  // Si
    1.07f,  // P
    1.05f,  // S
    1.02f,  // Cl
    1.06f,  // Ar
    2.03f,  // K
    1.76f,  // Ca
    1.70f,  // Sc
    1.60f,  // Ti
    1.53f,  // V
    1.39f,  // Cr
    1.39f,  // Mn (low-spin)
    1.32f,  // Fe (low-spin)
    1.26f,  // Co (low-spin)
    1.24f,  // Ni
    1.32f,  // Cu
    1.22f,  // Zn
    1.22f,  // Ga
    1.20f,  // Ge
    1.19f,  // As
    1.20f,  // Se
    1.20f,  // Br
    1.16f,  // Kr
    2.20f,  // Rb
    1.95f,  // Sr
    1.90f,  // Y
    1.75f,  // Zr
    1.64f,  // Nb
    1.54f,  // Mo
    1.47f,  // Tc
    1.46f,  // Ru
    1.42f,  // Rh
    1.39f,  // Pd
    1.45f,  // Ag
    1.44f,  // Cd
    1.42f,  // In
    1.39f,  // Sn
    1.39f,  // Sb
    1.38f,  // Te
    1.39f,  // I
    1.40f   // Xe
};
static constexpr int kNumRadii = static_cast<int>(sizeof(kCovalentRadii) / sizeof(float));

float default_covalent_radius(uint16_t Z) {
    if (Z > 0 && Z < kNumRadii) return kCovalentRadii[Z];
    return 1.50f;
}

// -----------------------------------------------------------------------
// build_topology — from flat arrays
// -----------------------------------------------------------------------
GlassMolecule build_topology(
    const std::vector<int>&                          atomic_numbers,
    const std::vector<std::pair<uint32_t,uint32_t>>& bond_pairs,
    const uint8_t*  bond_orders,
    const float*    covalent_radii
) {
    GlassMolecule mol;
    mol.atoms.resize(atomic_numbers.size());
    for (size_t i = 0; i < atomic_numbers.size(); ++i) {
        auto& a     = mol.atoms[i];
        a.index     = static_cast<uint32_t>(i);
        a.atomic_number = static_cast<uint16_t>(atomic_numbers[i]);
        a.covalent_radius = covalent_radii
            ? covalent_radii[i]
            : default_covalent_radius(a.atomic_number);
    }

    mol.bonds.resize(bond_pairs.size());
    for (size_t i = 0; i < bond_pairs.size(); ++i) {
        auto& b = mol.bonds[i];
        b.index = static_cast<uint32_t>(i);
        b.a     = bond_pairs[i].first;
        b.b     = bond_pairs[i].second;
        b.order = bond_orders
            ? static_cast<BondOrder>(bond_orders[i])
            : BondOrder::Single;

        mol.atoms[b.a].bond_ids.push_back(b.index);
        mol.atoms[b.b].bond_ids.push_back(b.index);
    }

    return mol;
}

} // namespace glass
} // namespace vsepr
