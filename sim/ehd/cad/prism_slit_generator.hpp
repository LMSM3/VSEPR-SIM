#pragma once
/**
 * prism_slit_generator.hpp
 *
 * Electrohydrodynamic Simulation — Stage 1: CAD / Geometry
 *
 * Configuration (d): Triangular prism slit-electrode for cross-flow.
 * An array of triangular prism electrodes separated by narrow slits
 * creates strong non-uniform electric fields that drive cross-flow
 * patterns through dielectrophoretic and Coulomb forces.
 *
 * Geometry:
 *   - N_prism triangular prism electrodes, each with base b and height h
 *   - Slit gaps of width w_slit between adjacent prisms
 *   - Flow passes through the slits in the transverse direction
 *   - Prisms extend over depth d_slit in the primary flow direction
 *
 * The geometry produces highly non-uniform |E| fields at the prism
 * tips, making it suitable for DEP and Coulomb pumping.
 */

#include "sim/ehd/ehd_types.hpp"
#include <vector>
#include <cmath>

namespace vsepr {
namespace ehd {
namespace cad {

// ============================================================================
// Prism Slit Parameters
// ============================================================================

struct PrismSlitParams {
    double slit_width   = 1.5e-3;   // w_slit (m) — gap between prisms
    double slit_depth   = 5.0e-3;   // d_slit (m) — flow-through depth
    double prism_base   = 3.0e-3;   // b (m) — triangular base width
    double prism_height = 4.0e-3;   // h (m) — triangular height (apex to base)
    int    prism_count  = 4;        // number of prisms in array
    int    pts_per_edge = 20;       // discretisation per triangle edge
};

inline PrismSlitParams from_ehd_prism_slit(const EHDParameters& p) {
    PrismSlitParams ps;
    ps.slit_width   = p.slit_width_m;
    ps.slit_depth   = p.slit_depth_m;
    ps.prism_base   = p.prism_base_m;
    ps.prism_height = p.prism_height_m;
    ps.prism_count  = p.prism_count;
    return ps;
}

// ============================================================================
// Prism Cross-Section (2D triangle in the x-y plane)
// ============================================================================

struct PrismTriangle {
    Vec3 apex;       // top vertex (tip where field is strongest)
    Vec3 base_left;  // bottom-left vertex
    Vec3 base_right; // bottom-right vertex
};

/**
 * Generate the array of prism cross-sections at z = 0.
 * Prisms are arranged along the x-axis with slit gaps between them.
 * Each prism is an isosceles triangle with apex pointing upward (+y).
 */
inline std::vector<PrismTriangle> generate_prism_array(
    const PrismSlitParams& ps)
{
    std::vector<PrismTriangle> prisms;
    prisms.reserve(static_cast<size_t>(ps.prism_count));

    double pitch = ps.prism_base + ps.slit_width;

    for (int i = 0; i < ps.prism_count; ++i) {
        double x_center = i * pitch;
        PrismTriangle tri;
        tri.apex       = {x_center, ps.prism_height, 0.0};
        tri.base_left  = {x_center - ps.prism_base * 0.5, 0.0, 0.0};
        tri.base_right = {x_center + ps.prism_base * 0.5, 0.0, 0.0};
        prisms.push_back(tri);
    }
    return prisms;
}

/**
 * Generate the outline vertices for a single prism cross-section.
 * Returns the triangle vertices plus interpolated edge points.
 */
inline std::vector<Vec3> generate_prism_outline(
    const PrismTriangle& tri,
    int pts_per_edge = 20)
{
    std::vector<Vec3> outline;
    outline.reserve(static_cast<size_t>(3 * pts_per_edge));

    auto lerp = [](const Vec3& a, const Vec3& b, double t) -> Vec3 {
        return {a.x + t * (b.x - a.x),
                a.y + t * (b.y - a.y),
                a.z + t * (b.z - a.z)};
    };

    // Edge: base_left → apex
    for (int i = 0; i < pts_per_edge; ++i) {
        double t = static_cast<double>(i) / pts_per_edge;
        outline.push_back(lerp(tri.base_left, tri.apex, t));
    }
    // Edge: apex → base_right
    for (int i = 0; i < pts_per_edge; ++i) {
        double t = static_cast<double>(i) / pts_per_edge;
        outline.push_back(lerp(tri.apex, tri.base_right, t));
    }
    // Edge: base_right → base_left
    for (int i = 0; i < pts_per_edge; ++i) {
        double t = static_cast<double>(i) / pts_per_edge;
        outline.push_back(lerp(tri.base_right, tri.base_left, t));
    }

    return outline;
}

/**
 * Total array width (x-direction extent).
 */
inline double array_total_width(const PrismSlitParams& ps) {
    if (ps.prism_count <= 0) return 0.0;
    return ps.prism_count * ps.prism_base
         + (ps.prism_count - 1) * ps.slit_width;
}

/**
 * Approximate tip field enhancement for a triangular prism electrode.
 * The field at the apex is enhanced relative to a parallel-plate field:
 *   E_tip ≈ (ΔV / w_slit) · β
 * where β is a geometric enhancement factor depending on apex angle.
 * For an isosceles triangle with half-angle α:
 *   β ≈ 1 / sin(α)   (lightning-rod effect)
 */
inline double prism_tip_field(double delta_V,
                               const PrismSlitParams& ps)
{
    if (ps.slit_width <= 0.0) return 0.0;
    double E_uniform = delta_V / ps.slit_width;

    // Half-angle of the isosceles triangle
    double half_base = ps.prism_base * 0.5;
    double alpha = std::atan2(half_base, ps.prism_height);
    double beta = 1.0 / std::sin(alpha);

    return E_uniform * beta;
}

} // namespace cad
} // namespace ehd
} // namespace vsepr
