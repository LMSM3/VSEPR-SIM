#pragma once
/**
 * tube_body_generator.hpp
 *
 * Electrohydrodynamic Simulation — Stage 1: CAD / Geometry
 *
 * Parametric tube body generator for straight and modulated (bulged) channels.
 * Generates axial profile points that define the tube inner wall.
 *
 * Two geometry modes:
 *   - Straight:   constant radius cylinder
 *   - Modulated:  sinusoidal bulge along the axis (for field-concentration studies)
 */

#include "sim/ehd/ehd_types.hpp"
#include <vector>
#include <cmath>

namespace vsepr {
namespace ehd {
namespace cad {

// ============================================================================
// Tube Profile Types
// ============================================================================

enum class TubeProfileType {
    STRAIGHT,
    MODULATED     // sinusoidal bulge
};

struct TubeParams {
    double radius         = 4.0e-3;   // base inner radius (m)
    double length         = 60.0e-3;  // total tube length (m)
    double wall_thickness = 0.5e-3;   // tube wall thickness (m)
    int    axial_divisions = 200;     // discretisation along axis

    // Modulation parameters (only used for MODULATED type)
    double modulation_amplitude = 1.0e-3;   // bulge half-amplitude (m)
    double modulation_wavelength = 12.0e-3; // spatial period (m)

    TubeProfileType profile_type = TubeProfileType::STRAIGHT;
};

inline TubeParams from_ehd_straight(const EHDParameters& p) {
    TubeParams t;
    t.radius = p.tube_radius_m;
    t.length = p.tube_length_m + p.inlet_length_m + p.outlet_length_m;
    t.profile_type = TubeProfileType::STRAIGHT;
    return t;
}

inline TubeParams from_ehd_modulated(const EHDParameters& p,
                                      double amplitude, double wavelength) {
    TubeParams t = from_ehd_straight(p);
    t.modulation_amplitude  = amplitude;
    t.modulation_wavelength = wavelength;
    t.profile_type = TubeProfileType::MODULATED;
    return t;
}

// ============================================================================
// Axial Profile Point
// ============================================================================

struct ProfilePoint {
    double z;               // axial position (m)
    double inner_radius;    // inner wall radius at this z (m)
    double outer_radius;    // outer wall radius at this z (m)
};

/**
 * Generate axial profile describing the tube inner/outer wall radii.
 */
inline std::vector<ProfilePoint> generate_tube_profile(const TubeParams& tp) {
    std::vector<ProfilePoint> profile;
    profile.reserve(static_cast<size_t>(tp.axial_divisions + 1));

    const double dz = tp.length / tp.axial_divisions;

    for (int i = 0; i <= tp.axial_divisions; ++i) {
        double z = i * dz;
        double r_inner = tp.radius;

        if (tp.profile_type == TubeProfileType::MODULATED) {
            double phase = 2.0 * constants::PI * z / tp.modulation_wavelength;
            r_inner += tp.modulation_amplitude * std::sin(phase);
        }

        double r_outer = r_inner + tp.wall_thickness;
        profile.push_back({z, r_inner, r_outer});
    }
    return profile;
}

/**
 * Generate a ring of points at a given axial position and radius.
 * Used for constructing annular cross-sections for surface tessellation.
 */
inline std::vector<Vec3> generate_ring(double z, double radius, int num_pts = 36) {
    std::vector<Vec3> ring;
    ring.reserve(static_cast<size_t>(num_pts));
    for (int i = 0; i < num_pts; ++i) {
        double theta = 2.0 * constants::PI * i / num_pts;
        ring.emplace_back(radius * std::cos(theta),
                          radius * std::sin(theta),
                          z);
    }
    return ring;
}

} // namespace cad
} // namespace ehd
} // namespace vsepr
