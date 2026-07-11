#pragma once
// ============================================================================
// cg3d_prerender.hpp - Bridge Beta: CG Bead -> Glass 3D Prerender Pipeline
// ============================================================================
// Converts a coarse_grain::BeadSystem into the glass molecule prerender
// pipeline (GlassMolecule -> Layout -> PrerenderBuffers -> SVG).
//
// This bridges two worlds: the CG bead representation (anisotropic
// surface descriptors, orientation axes, structural roles) and the glass
// visualization pipeline (topology graph, 3D layout, instanced rendering).
//
// Usage:
//   CG3DPrerender bridge;
//   auto result = bridge.prerender(bead_system);
//   result.report_renderer.write_svg("cg_system.svg", result.buffers);
// ============================================================================

#include "molecule/glass.hpp"
#include "coarse_grain/core/bead_system.hpp"
#include <string>

namespace vsepr {
namespace bridge_beta {

// -----------------------------------------------------------------------
// CG3DSettings - controls how beads map to glass visual primitives
// -----------------------------------------------------------------------
struct CG3DSettings {
    // Bead rendering
    float bead_radius_scale   = 1.2f;    // scale factor for bead display radius
    float bond_radius         = 0.15f;   // cylinder radius for bead-bead bonds
    bool  show_orientation     = true;    // render orientation axis as a small bond
    float orientation_length   = 0.6f;    // length of orientation marker

    // Structural role -> pseudo atomic number mapping
    // Maps StructuralRole to a fake Z for CPK coloring:
    //   Inert=18(Ar/grey), Ionic=11(Na/purple), Covalent=6(C/dark),
    //   Metallic=29(Cu/copper), Mixed=16(S/yellow)
    uint16_t role_to_Z(uint8_t role) const;

    // Layout settings override
    glass::LayoutSettings layout;

    // SVG report settings override
    glass::ReportSettings report;
};

// -----------------------------------------------------------------------
// CG3DResult - output of the prerender bridge
// -----------------------------------------------------------------------
struct CG3DResult {
    glass::GlassMolecule    topology;
    glass::LayoutResult     layout;
    glass::PrerenderBuffers buffers;

    // Convenience: write SVG directly
    bool write_svg(const std::string& path,
                   const glass::ReportSettings& rs = {}) const;

    bool empty() const { return buffers.empty(); }
};

// -----------------------------------------------------------------------
// CG3DPrerender - the bridge from BeadSystem to glass visualization
// -----------------------------------------------------------------------
class CG3DPrerender {
public:
    explicit CG3DPrerender(CG3DSettings s = {});

    // Full pipeline: BeadSystem -> topology -> layout -> buffers
    CG3DResult prerender(const coarse_grain::BeadSystem& system) const;

    // Intermediate: convert BeadSystem to GlassMolecule topology
    glass::GlassMolecule to_topology(const coarse_grain::BeadSystem& system) const;

    CG3DSettings&       settings()       { return settings_; }
    const CG3DSettings& settings() const { return settings_; }

private:
    CG3DSettings settings_;
};

} // namespace bridge_beta
} // namespace vsepr
