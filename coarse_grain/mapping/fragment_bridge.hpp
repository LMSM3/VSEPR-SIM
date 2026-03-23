#pragma once
/**
 * fragment_bridge.hpp — Atomistic→CG Scale Boundary Bridge
 *
 * Provides the conversion functions from atomistic::FragmentView
 * to coarse_grain:: types (Bead, UnifiedDescriptor). This is the
 * one-directional data flow point:
 *
 *   atomistic::FragmentView → coarse_grain::Bead
 *   atomistic::FragmentView → coarse_grain::UnifiedDescriptor
 *
 * The atomistic namespace never imports coarse_grain types.
 * The coarse_grain namespace depends on atomistic bridge types.
 *
 * Anti-black-box: every field in the output bead is traceable
 * back to the FragmentView that produced it.
 *
 * Reference: "Atomistic Preparation Layer" section of
 *            section_anisotropic_beads.tex
 */

#include "atomistic/core/fragment_view.hpp"
#include "coarse_grain/core/bead.hpp"
#include "coarse_grain/core/unified_descriptor.hpp"
#include "coarse_grain/core/inertia_frame.hpp"
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace coarse_grain {

// ============================================================================
// Bridge Result
// ============================================================================

/**
 * BridgeResult — outcome of a scale boundary crossing.
 *
 * Records whether the conversion succeeded, and if not, why.
 */
struct BridgeResult {
    bool ok{false};
    std::string error;
    atomistic::FragmentStatus fragment_status{atomistic::FragmentStatus::EmptyFragment};
};

// ============================================================================
// Fragment → Bead
// ============================================================================

/**
 * Build a coarse-grained Bead from an atomistic FragmentView.
 *
 * This is the primary conversion point. The bead inherits:
 *   - position (center of mass from fragment)
 *   - mass and charge (aggregates from fragment)
 *   - parent atom indices (from fragment atom records)
 *   - orientation (from fragment local frame)
 *
 * @param frag     Validated fragment view
 * @param type_id  Bead type id (assigned by mapping scheme)
 * @return Bead with full provenance
 */
inline Bead build_bead(const atomistic::FragmentView& frag,
                        uint32_t type_id = 0)
{
    Bead bead;

    if (!frag.is_valid()) return bead;

    // Position: center of mass
    bead.position = frag.center_of_mass();
    bead.com_position = bead.position;
    bead.cog_position = frag.center_of_geometry();

    // Aggregate properties
    bead.mass = frag.total_mass;
    bead.charge = frag.total_charge;
    bead.type_id = type_id;

    // Provenance: parent atom indices
    bead.parent_atom_indices.reserve(frag.num_atoms());
    for (const auto& a : frag.atoms) {
        bead.parent_atom_indices.push_back(a.original_index);
    }

    // Mapping residual
    atomistic::Vec3 d = bead.com_position - bead.cog_position;
    bead.mapping_residual = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);

    // Orientation from fragment frame
    if (frag.frame.well_defined) {
        bead.orientation.normal = frag.frame.axis3;  // Plane normal
        bead.has_orientation = true;
    }

    return bead;
}

// ============================================================================
// Fragment → InertiaFrame (bridge helper)
// ============================================================================

/**
 * Convert an atomistic::LocalFrame to a coarse_grain::InertiaFrame.
 */
inline InertiaFrame bridge_frame(const atomistic::LocalFrame& lf) {
    InertiaFrame frame;
    frame.axis1 = lf.axis1;
    frame.axis2 = lf.axis2;
    frame.axis3 = lf.axis3;
    frame.valid = lf.well_defined;
    return frame;
}

// ============================================================================
// Fragment → UnifiedDescriptor (structural initialization)
// ============================================================================

/**
 * Build a UnifiedDescriptor from a FragmentView with initial resolution.
 *
 * This creates the descriptor structure with appropriate initial ℓ_max
 * based on fragment complexity. It does NOT compute SH coefficients
 * (that requires probe sampling via SurfaceMapper or similar).
 *
 * The initial resolution is determined by fragment properties:
 *   - 1 atom: l_max = 0 (isotropic)
 *   - 2-6 atoms, no metal: l_max = 2 (axial)
 *   - 7-17 atoms, no metal: l_max = 4 (moderate)
 *   - 18+ atoms or metal center: l_max = 4, all channels (start moderate, promote by residual)
 *
 * @param frag           Validated fragment view
 * @param active_channels Number of channels to activate (1-3, default: 1)
 * @return UnifiedDescriptor initialized at appropriate resolution
 */
inline UnifiedDescriptor build_descriptor_structure(
    const atomistic::FragmentView& frag,
    int active_channels = 1)
{
    UnifiedDescriptor ud;

    if (!frag.is_valid()) return ud;

    // Determine initial l_max from fragment complexity
    int l_max;
    uint32_t n = frag.num_atoms();

    if (n <= 1) {
        l_max = 0;
    } else if (n <= 6 && !frag.has_metal_center()) {
        l_max = 2;
    } else if (n <= 17 && !frag.has_metal_center()) {
        l_max = 4;
    } else {
        l_max = 4;  // Start moderate; residual analysis will promote if needed
        active_channels = std::max(active_channels, 2);  // Activate more channels for complex systems
    }

    // Initialize channels based on requested activation count
    if (active_channels >= 3) {
        ud.init(l_max);
    } else if (active_channels == 2) {
        ud.init_single_channel(DescriptorChannel::STERIC, l_max);
        ud.activate_channel(DescriptorChannel::ELECTROSTATIC, l_max);
    } else {
        ud.init_single_channel(DescriptorChannel::STERIC, l_max);
    }

    // Transfer frame
    ud.frame = bridge_frame(frag.frame);

    return ud;
}

// ============================================================================
// Full Bridge: Fragment → Bead with UnifiedDescriptor
// ============================================================================

/**
 * BridgedBead — result of a complete atomistic→CG bridge operation.
 */
struct BridgedBead {
    BridgeResult result;
    Bead bead;
};

/**
 * Build a Bead with initialized UnifiedDescriptor from a FragmentView.
 *
 * This is the complete bridge entry point. It:
 *   1. Validates the fragment
 *   2. Builds the bead (position, mass, charge, provenance)
 *   3. Initializes the unified descriptor structure
 *   4. Attaches the descriptor to the bead
 *
 * Note: SH coefficients are NOT computed here. They require probe
 * sampling which is a separate step (SurfaceMapper / MultiChannelMapper).
 *
 * @param frag           Validated fragment view
 * @param type_id        Bead type id
 * @param active_channels Number of descriptor channels to activate
 * @return BridgedBead with bead, descriptor, and status
 */
inline BridgedBead bridge_fragment_to_bead(
    const atomistic::FragmentView& frag,
    uint32_t type_id = 0,
    int active_channels = 1)
{
    BridgedBead bb;
    bb.result.fragment_status = frag.status;

    if (!frag.is_valid()) {
        bb.result.ok = false;
        bb.result.error = atomistic::fragment_status_name(frag.status);
        return bb;
    }

    // Build bead
    bb.bead = build_bead(frag, type_id);

    // Build and attach descriptor structure
    bb.bead.unified = build_descriptor_structure(frag, active_channels);

    bb.result.ok = true;
    return bb;
}

} // namespace coarse_grain
