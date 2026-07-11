#pragma once
/**
 * metal_sim_builder.hpp — BeadSystem Constructor for Metal Research
 *
 * Builds a BeadSystem from a MetalRecord with realistic initial geometry.
 *
 * Three geometry modes:
 *   CLUSTER_SC     — simple cubic arrangement (legacy, fast)
 *   CLUSTER_FCC    — FCC cluster shell construction
 *   CLUSTER_BCC    — BCC cluster shell construction
 *
 * The appropriate mode is selected automatically from CrystalStructure.
 * HCP is mapped to FCC (proxy model, c/a ≈ √(8/3) accounted by β parameter).
 *
 * Bead parameters (σ, ε, mass) come directly from MetalRecord LJ proxy.
 * Structural role is set to Metallic for all beads.
 *
 * Anti-black-box: initial positions, mass, type_id are all
 * readable from the returned BeadSystem.
 */

#include "coarse_grain/metals/metal_registry.hpp"
#include "coarse_grain/core/bead.hpp"
#include "coarse_grain/core/bead_system.hpp"
#include <cmath>
#include <vector>

namespace coarse_grain {
namespace metals {

// ============================================================================
// Geometry mode
// ============================================================================

enum class ClusterGeometry { AUTO, SC, FCC, BCC };

// ============================================================================
// FCC shell builder
// ============================================================================

/**
 * Build FCC cluster positions centred at origin.
 * Fills up to n_target beads; actual count may vary slightly.
 */
inline std::vector<atomistic::Vec3>
build_fcc_positions(double a0, uint32_t n_target)
{
    // FCC basis vectors (Cartesian)
    std::vector<atomistic::Vec3> basis = {
        {0.0,     0.0,    0.0},
        {a0/2.0,  a0/2.0, 0.0},
        {a0/2.0,  0.0,    a0/2.0},
        {0.0,     a0/2.0, a0/2.0}
    };

    std::vector<atomistic::Vec3> positions;
    int n_side = static_cast<int>(std::ceil(std::cbrt(n_target / 4.0))) + 2;
    double offset = -0.5 * a0 * (n_side - 1);

    for (int ix = 0; ix < n_side && positions.size() < n_target * 2; ++ix)
    for (int iy = 0; iy < n_side && positions.size() < n_target * 2; ++iy)
    for (int iz = 0; iz < n_side && positions.size() < n_target * 2; ++iz)
    for (const auto& bv : basis) {
        positions.push_back({
            offset + a0 * ix + bv.x,
            offset + a0 * iy + bv.y,
            offset + a0 * iz + bv.z
        });
        if (positions.size() >= static_cast<size_t>(n_target)) goto done_fcc;
    }
    done_fcc:
    positions.resize(std::min((size_t)n_target, positions.size()));
    return positions;
}

/**
 * Build BCC cluster positions centred at origin.
 */
inline std::vector<atomistic::Vec3>
build_bcc_positions(double a0, uint32_t n_target)
{
    std::vector<atomistic::Vec3> basis = {
        {0.0,    0.0,    0.0},
        {a0/2.0, a0/2.0, a0/2.0}
    };

    std::vector<atomistic::Vec3> positions;
    int n_side = static_cast<int>(std::ceil(std::cbrt(n_target / 2.0))) + 2;
    double offset = -0.5 * a0 * (n_side - 1);

    for (int ix = 0; ix < n_side && positions.size() < n_target * 2; ++ix)
    for (int iy = 0; iy < n_side && positions.size() < n_target * 2; ++iy)
    for (int iz = 0; iz < n_side && positions.size() < n_target * 2; ++iz)
    for (const auto& bv : basis) {
        positions.push_back({
            offset + a0 * ix + bv.x,
            offset + a0 * iy + bv.y,
            offset + a0 * iz + bv.z
        });
        if (positions.size() >= static_cast<size_t>(n_target)) goto done_bcc;
    }
    done_bcc:
    positions.resize(std::min((size_t)n_target, positions.size()));
    return positions;
}

/**
 * Build SC cluster positions (fallback).
 */
inline std::vector<atomistic::Vec3>
build_sc_positions(double a0, uint32_t n_target)
{
    std::vector<atomistic::Vec3> positions;
    int n_side = static_cast<int>(std::ceil(std::cbrt(static_cast<double>(n_target))));
    double offset = -0.5 * a0 * (n_side - 1);

    for (int ix = 0; ix < n_side; ++ix)
    for (int iy = 0; iy < n_side; ++iy)
    for (int iz = 0; iz < n_side; ++iz) {
        positions.push_back({
            offset + a0 * ix,
            offset + a0 * iy,
            offset + a0 * iz
        });
        if (positions.size() >= static_cast<size_t>(n_target)) goto done_sc;
    }
    done_sc:
    positions.resize(std::min((size_t)n_target, positions.size()));
    return positions;
}

// ============================================================================
// Main builder
// ============================================================================

/**
 * Build a BeadSystem for a single pure metal.
 *
 * @param metal     MetalRecord from registry
 * @param n_beads   Target bead count (cluster size)
 * @param geom      Geometry override (AUTO selects from crystal structure)
 */
inline BeadSystem build_metal_bead_system(
    const MetalRecord&  metal,
    uint32_t            n_beads = 64,
    ClusterGeometry     geom    = ClusterGeometry::AUTO)
{
    BeadSystem sys;

    // Bead type from LJ proxy parameters
    BeadType bt;
    bt.name    = metal.symbol;
    bt.id      = 0;
    bt.sigma   = metal.lj_sigma_ang;
    bt.epsilon = metal.lj_epsilon_kcal;
    sys.bead_types.push_back(bt);
    sys.source_atom_count = n_beads;

    // Select geometry
    ClusterGeometry actual_geom = geom;
    if (actual_geom == ClusterGeometry::AUTO) {
        switch (metal.structure) {
            case CrystalStructure::FCC: actual_geom = ClusterGeometry::FCC; break;
            case CrystalStructure::BCC: actual_geom = ClusterGeometry::BCC; break;
            case CrystalStructure::HCP: actual_geom = ClusterGeometry::FCC; break;
        }
    }

    double a0 = metal.lattice_constant_ang > 0.0 ? metal.lattice_constant_ang : 4.0;
    std::vector<atomistic::Vec3> positions;

    switch (actual_geom) {
        case ClusterGeometry::FCC: positions = build_fcc_positions(a0, n_beads); break;
        case ClusterGeometry::BCC: positions = build_bcc_positions(a0, n_beads); break;
        default:                   positions = build_sc_positions(a0, n_beads);  break;
    }

    sys.beads.reserve(positions.size());
    for (const auto& pos : positions) {
        Bead b;
        b.position        = pos;
        b.mass            = metal.atomic_mass_amu;
        b.charge          = 0.0;
        b.type_id         = 0;
        b.structural_role = StructuralRole::Metallic;
        sys.beads.push_back(b);
    }

    return sys;
}

/**
 * Build a binary alloy BeadSystem.
 * Beads alternate A-B-A-B in position order.
 * Composition x_B controls the B fraction.
 */
inline BeadSystem build_alloy_bead_system(
    const MetalRecord& A,
    const MetalRecord& B,
    uint32_t           n_beads = 64,
    double             x_B     = 0.5,
    ClusterGeometry    geom    = ClusterGeometry::AUTO)
{
    BeadSystem sys;

    // Bead type A
    BeadType btA;
    btA.name    = A.symbol;
    btA.id      = 0;
    btA.sigma   = A.lj_sigma_ang;
    btA.epsilon = A.lj_epsilon_kcal;
    sys.bead_types.push_back(btA);

    // Bead type B
    BeadType btB;
    btB.name    = B.symbol;
    btB.id      = 1;
    btB.sigma   = B.lj_sigma_ang;
    btB.epsilon = B.lj_epsilon_kcal;
    sys.bead_types.push_back(btB);

    sys.source_atom_count = n_beads;

    // Use A lattice constant for geometry (majority component if x_B < 0.5)
    double a_mix = (1.0 - x_B) * A.lattice_constant_ang + x_B * B.lattice_constant_ang;
    CrystalStructure dominant_struct = (x_B < 0.5) ? A.structure : B.structure;

    ClusterGeometry actual_geom = geom;
    if (actual_geom == ClusterGeometry::AUTO) {
        switch (dominant_struct) {
            case CrystalStructure::FCC: actual_geom = ClusterGeometry::FCC; break;
            case CrystalStructure::BCC: actual_geom = ClusterGeometry::BCC; break;
            case CrystalStructure::HCP: actual_geom = ClusterGeometry::FCC; break;
        }
    }

    std::vector<atomistic::Vec3> positions;
    switch (actual_geom) {
        case ClusterGeometry::FCC: positions = build_fcc_positions(a_mix, n_beads); break;
        case ClusterGeometry::BCC: positions = build_bcc_positions(a_mix, n_beads); break;
        default:                   positions = build_sc_positions(a_mix, n_beads);  break;
    }

    uint32_t n_B_target = static_cast<uint32_t>(x_B * positions.size());
    sys.beads.reserve(positions.size());
    uint32_t n_B_placed = 0;

    for (uint32_t i = 0; i < positions.size(); ++i) {
        bool is_B = (n_B_placed < n_B_target) &&
                    ((i % 2 == 1) || (n_B_placed * positions.size() < n_B_target * (i + 1)));

        Bead b;
        b.position        = positions[i];
        b.structural_role = StructuralRole::Metallic;
        if (is_B) {
            b.mass    = B.atomic_mass_amu;
            b.charge  = 0.0;
            b.type_id = 1;
            ++n_B_placed;
        } else {
            b.mass    = A.atomic_mass_amu;
            b.charge  = 0.0;
            b.type_id = 0;
        }
        sys.beads.push_back(b);
    }

    return sys;
}

} // namespace metals
} // namespace coarse_grain
