#pragma once
/*
geom_ops.hpp
------------
Geometric operations on molecular coordinates.

Functions operate on flat coordinate arrays:
  coords[3*i + 0] = x_i
  coords[3*i + 1] = y_i
  coords[3*i + 2] = z_i

All functions are deterministic and numerically stable.
*/

#include "core/math_vec3.hpp"
#include <vector>
#include <cmath>
#include <algorithm>
#include <cassert>
#include <cstdint>

namespace vsepr {

// ============================================================================
// Coordinate Access Helpers
// ============================================================================

// Get position of atom i as Vec3
inline Vec3 get_pos(const std::vector<double>& coords, uint32_t i) {
    size_t idx = 3 * i;
    return {coords[idx], coords[idx + 1], coords[idx + 2]};
}

// Set position of atom i from Vec3
inline void set_pos(std::vector<double>& coords, uint32_t i, const Vec3& v) {
    size_t idx = 3 * i;
    coords[idx]     = v.x;
    coords[idx + 1] = v.y;
    coords[idx + 2] = v.z;
}

// Accumulate gradient for atom i
inline void accumulate_grad(std::vector<double>& grad, uint32_t i, const Vec3& g) {
    size_t idx = 3 * i;
    grad[idx]     += g.x;
    grad[idx + 1] += g.y;
    grad[idx + 2] += g.z;
}

// ============================================================================
// Distance and Direction
// ============================================================================

// Distance between atoms i and j
inline double distance(const std::vector<double>& coords, uint32_t i, uint32_t j) {
    Vec3 ri = get_pos(coords, i);
    Vec3 rj = get_pos(coords, j);
    return (rj - ri).norm();
}

// Displacement vector r_ij = r_j - r_i (points from i to j)
inline Vec3 rij(const std::vector<double>& coords, uint32_t i, uint32_t j) {
    Vec3 ri = get_pos(coords, i);
    Vec3 rj = get_pos(coords, j);
    return rj - ri;
}

// ============================================================================
// Angle (i-j-k, vertex at j)
// ============================================================================

// Compute angle in radians between vectors j->i and j->k
// Uses stable formula with clamping to avoid acos domain errors
inline double angle(const std::vector<double>& coords, uint32_t i, uint32_t j, uint32_t k) {
    Vec3 rj  = get_pos(coords, j);
    Vec3 rji = get_pos(coords, i) - rj;  // j -> i
    Vec3 rjk = get_pos(coords, k) - rj;  // j -> k

    double dji = rji.norm();
    double djk = rjk.norm();

    if (dji < 1e-12 || djk < 1e-12) return 0.0;  // degenerate

    double cos_theta = rji.dot(rjk) / (dji * djk);
    
    // Clamp to valid domain for acos
    cos_theta = std::clamp(cos_theta, -1.0, 1.0);
    
    return std::acos(cos_theta);
}

// ============================================================================
// Torsion / Dihedral (i-j-k-l)
// ============================================================================

// Compute dihedral angle in radians for i-j-k-l
// Uses stable atan2 formulation
// Returns angle in range [-π, π]
inline double torsion(const std::vector<double>& coords, 
                      uint32_t i, uint32_t j, uint32_t k, uint32_t l) {
    Vec3 ri = get_pos(coords, i);
    Vec3 rj = get_pos(coords, j);
    Vec3 rk = get_pos(coords, k);
    Vec3 rl = get_pos(coords, l);

    Vec3 rij = rj - ri;
    Vec3 rjk = rk - rj;
    Vec3 rkl = rl - rk;

    // Normal vectors to the two planes
    Vec3 n1 = rij.cross(rjk);
    Vec3 n2 = rjk.cross(rkl);

    double n1_norm = n1.norm();
    double n2_norm = n2.norm();

    if (n1_norm < 1e-12 || n2_norm < 1e-12) return 0.0;  // degenerate (linear)

    // Normalize
    n1 /= n1_norm;
    n2 /= n2_norm;

    // Stable dihedral using atan2
    double cos_phi = n1.dot(n2);
    double sin_phi = rjk.normalized().dot(n1.cross(n2));

    return std::atan2(sin_phi, cos_phi);
}

// ============================================================================
// Invariance Checks (for testing)
// ============================================================================

// Check if operation preserves translation invariance
// Returns true if f(coords) == f(coords + translation)
template<typename Func>
bool check_translation_invariance(const std::vector<double>& coords, 
                                   Func f, 
                                   const Vec3& translation = {1.0, 2.0, 3.0},
                                   double tol = 1e-10) {
    size_t n = coords.size() / 3;
    std::vector<double> coords_shifted = coords;
    
    // Apply translation
    for (size_t i = 0; i < n; ++i) {
        coords_shifted[3*i + 0] += translation.x;
        coords_shifted[3*i + 1] += translation.y;
        coords_shifted[3*i + 2] += translation.z;
    }

    double val1 = f(coords);
    double val2 = f(coords_shifted);

    return std::abs(val1 - val2) < tol;
}

// Check if operation preserves rotation invariance
// Returns true if f(coords) == f(rotated_coords)
// Uses simple 90° rotation around z-axis for testing
template<typename Func>
bool check_rotation_invariance(const std::vector<double>& coords, 
                                Func f,
                                double tol = 1e-10) {
    size_t n = coords.size() / 3;
    std::vector<double> coords_rotated = coords;
    
    // Apply 90° rotation around z-axis: (x, y, z) -> (-y, x, z)
    for (size_t i = 0; i < n; ++i) {
        double x = coords[3*i + 0];
        double y = coords[3*i + 1];
        coords_rotated[3*i + 0] = -y;
        coords_rotated[3*i + 1] = x;
        // z unchanged
    }

    double val1 = f(coords);
    double val2 = f(coords_rotated);

    return std::abs(val1 - val2) < tol;
}

// ============================================================================
// Center of Mass (utility)
// ============================================================================

// Compute geometric center (unweighted average)
inline Vec3 geometric_center(const std::vector<double>& coords) {
    size_t n = coords.size() / 3;
    if (n == 0) return {0, 0, 0};

    Vec3 center{0, 0, 0};
    for (size_t i = 0; i < n; ++i) {
        center += get_pos(coords, i);
    }
    return center / static_cast<double>(n);
}

// Translate all coordinates so geometric center is at origin
inline void center_coords(std::vector<double>& coords) {
    Vec3 c = geometric_center(coords);
    size_t n = coords.size() / 3;
    for (size_t i = 0; i < n; ++i) {
        Vec3 pos = get_pos(coords, i) - c;
        set_pos(coords, i, pos);
    }
}

} // namespace vsepr
