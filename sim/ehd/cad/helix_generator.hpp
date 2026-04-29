#pragma once
/**
 * helix_generator.hpp
 *
 * Electrohydrodynamic Simulation — Stage 1: CAD / Geometry
 *
 * Parametric helical electrode geometry generator.
 * Produces discretised helix centerline points for a wire electrode wound
 * inside a cylindrical tube at specified pitch, radius, and turn count.
 *
 * Output is a point cloud (Vec3 sequence) suitable for sweep-path construction
 * or direct tessellation in downstream STEP / STL exporters.
 */

#include "sim/ehd/ehd_types.hpp"
#include <vector>
#include <cmath>

namespace vsepr {
namespace ehd {
namespace cad {

// ============================================================================
// HelixGenerator — parametric helix centerline
// ============================================================================

struct HelixParams {
    double radius        = 3.0e-3;   // helix centerline radius (m)
    double pitch         = 6.0e-3;   // axial advance per revolution (m)
    int    num_turns     = 9;
    int    points_per_turn = 72;     // angular resolution
    double start_z      = 0.0;      // axial offset of first point
};

inline HelixParams from_ehd(const EHDParameters& p) {
    HelixParams h;
    h.radius         = p.helix_radius();
    h.pitch          = p.helix_pitch_m;
    h.num_turns      = p.num_turns;
    h.start_z        = p.inlet_length_m;
    return h;
}

/**
 * Generate helix centerline points.
 * The helix axis is aligned with +Z.
 */
inline std::vector<Vec3> generate_helix_centerline(const HelixParams& h) {
    std::vector<Vec3> pts;
    const int total_pts = h.num_turns * h.points_per_turn + 1;
    pts.reserve(static_cast<size_t>(total_pts));

    const double dtheta = 2.0 * constants::PI / h.points_per_turn;
    const double dz_per_step = h.pitch / h.points_per_turn;

    for (int i = 0; i < total_pts; ++i) {
        double theta = i * dtheta;
        double x = h.radius * std::cos(theta);
        double y = h.radius * std::sin(theta);
        double z = h.start_z + i * dz_per_step;
        pts.emplace_back(x, y, z);
    }
    return pts;
}

/**
 * Generate wire cross-section circle at a given point and tangent direction.
 * Used for sweep-path solid generation.
 */
inline std::vector<Vec3> generate_wire_section(
    const Vec3& center, const Vec3& tangent,
    double wire_radius, int num_pts = 16)
{
    // Build a local frame: tangent is forward, compute two perpendicular axes
    Vec3 t = tangent.normalized();
    Vec3 arbitrary = (std::abs(t.x) < 0.9) ? Vec3{1, 0, 0} : Vec3{0, 1, 0};

    // u = t × arbitrary
    Vec3 u{t.y * arbitrary.z - t.z * arbitrary.y,
            t.z * arbitrary.x - t.x * arbitrary.z,
            t.x * arbitrary.y - t.y * arbitrary.x};
    double un = u.norm();
    if (un > 0.0) { u.x /= un; u.y /= un; u.z /= un; }

    // v = t × u
    Vec3 v{t.y * u.z - t.z * u.y,
            t.z * u.x - t.x * u.z,
            t.x * u.y - t.y * u.x};

    std::vector<Vec3> section;
    section.reserve(static_cast<size_t>(num_pts));
    for (int i = 0; i < num_pts; ++i) {
        double a = 2.0 * constants::PI * i / num_pts;
        Vec3 pt = center + u * (wire_radius * std::cos(a))
                         + v * (wire_radius * std::sin(a));
        section.push_back(pt);
    }
    return section;
}

} // namespace cad
} // namespace ehd
} // namespace vsepr
