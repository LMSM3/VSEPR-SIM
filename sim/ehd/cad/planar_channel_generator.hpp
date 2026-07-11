#pragma once
/**
 * planar_channel_generator.hpp
 *
 * Electrohydrodynamic Simulation — Stage 1: CAD / Geometry
 *
 * Configuration (a): Planar electrode channel.
 * Two parallel planar electrodes separated by a gap H with DC voltage
 * driving Coulomb / electroosmotic flow through the channel.
 *
 * Geometry:
 *   - Channel of height H, width W, length L_ch
 *   - Bottom electrode at y = 0  (positive)
 *   - Top electrode at y = H     (negative / grounded)
 *   - Soft base (flexible substrate) below bottom electrode
 *
 * The channel cross-section is rectangular; the primary flow direction
 * is along +z, with the electric field predominantly in +y.
 */

#include "sim/ehd/ehd_types.hpp"
#include <vector>
#include <cmath>

namespace vsepr {
namespace ehd {
namespace cad {

// ============================================================================
// Planar Channel Parameters
// ============================================================================

struct PlanarChannelParams {
    double height    = 2.0e-3;    // H — electrode gap (m)
    double width     = 10.0e-3;   // W — electrode span (m)
    double length    = 40.0e-3;   // L_ch — electrode length (m)
    int    nx_divs   = 100;       // discretisation along length
    int    ny_divs   = 40;        // discretisation across gap
};

inline PlanarChannelParams from_ehd_planar(const EHDParameters& p) {
    PlanarChannelParams pc;
    pc.height = p.channel_height_m;
    pc.width  = p.channel_width_m;
    pc.length = p.channel_length_m;
    return pc;
}

// ============================================================================
// Channel Profile Point (2D cross-section)
// ============================================================================

struct ChannelProfilePoint {
    double z;      // streamwise position (m)
    double y_bot;  // bottom electrode y (m) — always 0 for planar
    double y_top;  // top electrode y (m)    — always H for planar
};

/**
 * Generate the planar channel profile along the streamwise direction.
 */
inline std::vector<ChannelProfilePoint> generate_planar_profile(
    const PlanarChannelParams& pc)
{
    std::vector<ChannelProfilePoint> profile;
    profile.reserve(static_cast<size_t>(pc.nx_divs + 1));

    double dz = pc.length / pc.nx_divs;
    for (int i = 0; i <= pc.nx_divs; ++i) {
        double z = i * dz;
        profile.push_back({z, 0.0, pc.height});
    }
    return profile;
}

/**
 * Analytical parallel-plate electric field:
 *   E_y = ΔV / H   (uniform between infinite plates)
 *
 * Returns the y-component of the field magnitude.
 */
inline double parallel_plate_field(double delta_V, double H) {
    if (H <= 0.0) return 0.0;
    return delta_V / H;
}

/**
 * Analytical parallel-plate potential at height y:
 *   φ(y) = ΔV · (1 - y/H)
 */
inline double parallel_plate_potential(double y, double delta_V, double H) {
    if (H <= 0.0) return 0.0;
    return delta_V * (1.0 - y / H);
}

/**
 * Plane-Poiseuille velocity profile between parallel plates:
 *   u_z(y) = (3/2)·U_avg · (1 - (2y/H - 1)²)
 *          = (6·U_avg / H²) · y · (H - y)
 */
inline double plane_poiseuille_velocity(double y, double U_avg, double H) {
    if (H <= 0.0) return 0.0;
    return 6.0 * U_avg * y * (H - y) / (H * H);
}

/**
 * Plane-Poiseuille pressure drop:
 *   ΔP = 12·μ·U_avg·L / H²
 */
inline double plane_poiseuille_pressure_drop(
    double mu_f, double U_avg, double L, double H)
{
    if (H <= 0.0) return 0.0;
    return 12.0 * mu_f * U_avg * L / (H * H);
}

/**
 * Generate corner vertices of the planar electrode pair for export.
 * Returns 8 corners (bottom plate 4 + top plate 4) of the bounding box.
 */
inline std::vector<Vec3> generate_planar_electrode_corners(
    const PlanarChannelParams& pc)
{
    std::vector<Vec3> corners;
    corners.reserve(8);

    double hw = pc.width * 0.5;

    // Bottom electrode (y = 0 plane)
    corners.emplace_back(-hw, 0.0, 0.0);
    corners.emplace_back( hw, 0.0, 0.0);
    corners.emplace_back( hw, 0.0, pc.length);
    corners.emplace_back(-hw, 0.0, pc.length);

    // Top electrode (y = H plane)
    corners.emplace_back(-hw, pc.height, 0.0);
    corners.emplace_back( hw, pc.height, 0.0);
    corners.emplace_back( hw, pc.height, pc.length);
    corners.emplace_back(-hw, pc.height, pc.length);

    return corners;
}

} // namespace cad
} // namespace ehd
} // namespace vsepr
