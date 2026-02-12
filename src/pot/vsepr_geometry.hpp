#pragma once
/**
 * vsepr_geometry.hpp
 * ------------------
 * VSEPR Geometry Analysis and Angle Constraints
 * 
 * Provides proper angle constraints for all VSEPR geometries including:
 * - AX5 (Trigonal Bipyramidal): 90°, 120°, 180°
 * - AX6 (Octahedral): 90°, 180°
 * - AX4E2 (Square Planar): 90°, 180°
 */

#include "core/types.hpp"
#include <vector>
#include <cmath>
#include <algorithm>

namespace vsepr {

/**
 * VSEPR Geometry Types with explicit angle patterns
 */
enum class VSEPRGeometry {
    LINEAR,                  // AX2: 180°
    TRIGONAL_PLANAR,         // AX3: 120°
    BENT_3,                  // AX2E: ~118°
    TETRAHEDRAL,             // AX4: 109.5°
    TRIGONAL_PYRAMIDAL,      // AX3E: ~107°
    BENT_4,                  // AX2E2: ~104°
    TRIGONAL_BIPYRAMIDAL,    // AX5: 90° (ax-eq), 120° (eq-eq), 180° (ax-ax)
    SEESAW,                  // AX4E: ~90°, ~120°
    T_SHAPED,                // AX3E2: ~90°
    OCTAHEDRAL,              // AX6: 90°, 180°
    SQUARE_PYRAMIDAL,        // AX5E: ~90°
    SQUARE_PLANAR,           // AX4E2: 90°, 180°
    PENTAGONAL_BIPYRAMIDAL,  // AX7: 72°, 90°, 180°
    UNKNOWN
};

/**
 * Detect VSEPR geometry from atom connectivity
 */
inline VSEPRGeometry detect_vsepr_geometry(
    int bonded_neighbors,
    int lone_pairs
) {
    int total = bonded_neighbors + lone_pairs;
    
    if (total == 2) {
        return lone_pairs == 0 ? VSEPRGeometry::LINEAR : VSEPRGeometry::UNKNOWN;
    }
    if (total == 3) {
        if (lone_pairs == 0) return VSEPRGeometry::TRIGONAL_PLANAR;
        if (lone_pairs == 1) return VSEPRGeometry::BENT_3;
    }
    if (total == 4) {
        if (lone_pairs == 0) return VSEPRGeometry::TETRAHEDRAL;
        if (lone_pairs == 1) return VSEPRGeometry::TRIGONAL_PYRAMIDAL;
        if (lone_pairs == 2) return VSEPRGeometry::BENT_4;
    }
    if (total == 5) {
        if (lone_pairs == 0) return VSEPRGeometry::TRIGONAL_BIPYRAMIDAL;
        if (lone_pairs == 1) return VSEPRGeometry::SEESAW;
        if (lone_pairs == 2) return VSEPRGeometry::T_SHAPED;
    }
    if (total == 6) {
        if (lone_pairs == 0) return VSEPRGeometry::OCTAHEDRAL;
        if (lone_pairs == 1) return VSEPRGeometry::SQUARE_PYRAMIDAL;
        if (lone_pairs == 2) return VSEPRGeometry::SQUARE_PLANAR;
    }
    if (total == 7 && lone_pairs == 0) {
        return VSEPRGeometry::PENTAGONAL_BIPYRAMIDAL;
    }
    
    return VSEPRGeometry::UNKNOWN;
}

/**
 * Get ideal angle for a specific VSEPR geometry and angle type
 * 
 * For AX5 (trigonal bipyramidal), we need to distinguish:
 * - axial-axial: 180°
 * - equatorial-equatorial: 120°
 * - axial-equatorial: 90°
 * 
 * This requires geometric analysis of the actual positions
 */
inline double get_vsepr_ideal_angle(
    VSEPRGeometry geom,
    const std::vector<double>& coords,
    uint32_t i, uint32_t j, uint32_t k
) {
    constexpr double DEG_TO_RAD = M_PI / 180.0;
    
    switch (geom) {
        case VSEPRGeometry::LINEAR:
            return 180.0 * DEG_TO_RAD;
            
        case VSEPRGeometry::TRIGONAL_PLANAR:
            return 120.0 * DEG_TO_RAD;
            
        case VSEPRGeometry::BENT_3:
            return 118.0 * DEG_TO_RAD;
            
        case VSEPRGeometry::TETRAHEDRAL:
            return 109.47 * DEG_TO_RAD;
            
        case VSEPRGeometry::TRIGONAL_PYRAMIDAL:
            return 107.0 * DEG_TO_RAD;
            
        case VSEPRGeometry::BENT_4:
            return 104.5 * DEG_TO_RAD;
            
        case VSEPRGeometry::TRIGONAL_BIPYRAMIDAL: {
            // Need to determine if this is ax-ax, eq-eq, or ax-eq angle
            // Use z-axis alignment to identify axial vs equatorial
            
            // Get vectors from central atom (j) to neighbors
            double xi = coords[3*i], yi = coords[3*i+1], zi = coords[3*i+2];
            double xj = coords[3*j], yj = coords[3*j+1], zj = coords[3*j+2];
            double xk = coords[3*k], yk = coords[3*k+1], zk = coords[3*k+2];
            
            double dxi = xi - xj, dyi = yi - yj, dzi = zi - zj;
            double dxk = xk - xj, dyk = yk - yj, dzk = zk - zj;
            
            double ri = std::sqrt(dxi*dxi + dyi*dyi + dzi*dzi);
            double rk = std::sqrt(dxk*dxk + dyk*dyk + dzk*dzk);
            
            if (ri < 1e-10 || rk < 1e-10) return 109.47 * DEG_TO_RAD;  // Degenerate
            
            // Axial atoms have high |z|/r ratio
            double z_ratio_i = std::abs(dzi) / ri;
            double z_ratio_k = std::abs(dzk) / rk;
            
            bool i_is_axial = z_ratio_i > 0.8;  // Nearly aligned with z-axis
            bool k_is_axial = z_ratio_k > 0.8;
            
            if (i_is_axial && k_is_axial) {
                // Both axial: 180°
                return 180.0 * DEG_TO_RAD;
            } else if (!i_is_axial && !k_is_axial) {
                // Both equatorial: 120°
                return 120.0 * DEG_TO_RAD;
            } else {
                // One axial, one equatorial: 90°
                return 90.0 * DEG_TO_RAD;
            }
        }
            
        case VSEPRGeometry::SEESAW:
            // Mix of 90° and 120° - use average or geometry-specific
            return 101.5 * DEG_TO_RAD;
            
        case VSEPRGeometry::T_SHAPED:
            return 90.0 * DEG_TO_RAD;
            
        case VSEPRGeometry::OCTAHEDRAL: {
            // Determine if opposite (180°) or adjacent (90°)
            double xi = coords[3*i], yi = coords[3*i+1], zi = coords[3*i+2];
            double xj = coords[3*j], yj = coords[3*j+1], zj = coords[3*j+2];
            double xk = coords[3*k], yk = coords[3*k+1], zk = coords[3*k+2];
            
            double dxi = xi - xj, dyi = yi - yj, dzi = zi - zj;
            double dxk = xk - xj, dyk = yk - yj, dzk = zk - zj;
            
            double ri = std::sqrt(dxi*dxi + dyi*dyi + dzi*dzi);
            double rk = std::sqrt(dxk*dxk + dyk*dyk + dzk*dzk);
            
            if (ri < 1e-10 || rk < 1e-10) return 90.0 * DEG_TO_RAD;
            
            double dot = (dxi*dxk + dyi*dyk + dzi*dzk) / (ri * rk);
            
            if (dot < -0.9) {
                // Nearly opposite: 180°
                return 180.0 * DEG_TO_RAD;
            } else {
                // Adjacent: 90°
                return 90.0 * DEG_TO_RAD;
            }
        }
            
        case VSEPRGeometry::SQUARE_PYRAMIDAL:
            return 90.0 * DEG_TO_RAD;
            
        case VSEPRGeometry::SQUARE_PLANAR: {
            // Similar to octahedral: opposite (180°) or adjacent (90°)
            double xi = coords[3*i], yi = coords[3*i+1], zi = coords[3*i+2];
            double xj = coords[3*j], yj = coords[3*j+1], zj = coords[3*j+2];
            double xk = coords[3*k], yk = coords[3*k+1], zk = coords[3*k+2];
            
            double dxi = xi - xj, dyi = yi - yj, dzi = zi - zj;
            double dxk = xk - xj, dyk = yk - yj, dzk = zk - zj;
            
            double ri = std::sqrt(dxi*dxi + dyi*dyi + dzi*dzi);
            double rk = std::sqrt(dxk*dxk + dyk*dyk + dzk*dzk);
            
            if (ri < 1e-10 || rk < 1e-10) return 90.0 * DEG_TO_RAD;
            
            double dot = (dxi*dxk + dyi*dyk + dzi*dzk) / (ri * rk);
            
            if (dot < -0.9) {
                return 180.0 * DEG_TO_RAD;
            } else {
                return 90.0 * DEG_TO_RAD;
            }
        }
            
        case VSEPRGeometry::PENTAGONAL_BIPYRAMIDAL:
            return 72.0 * DEG_TO_RAD;  // Simplified
            
        default:
            return 109.47 * DEG_TO_RAD;  // Fallback to tetrahedral
    }
}

/**
 * Element-aware bond detection using covalent radii
 * 
 * Rule: d(A,B) ≤ scale * (r_cov(A) + r_cov(B))
 * Recommended scale: 1.20-1.30
 * 
 * This prevents spurious bonds (e.g., F-F in PF5)
 */
inline bool should_bond_by_covalent_radii(
    uint8_t Z_a, uint8_t Z_b,
    double distance,
    double scale = 1.25
) {
    // Covalent radii (Angstroms) - from Cordero et al. 2008
    const double covalent_radii[] = {
        0.0,   // None (0)
        0.31,  // H  (1)
        0.28,  // He (2)
        1.28,  // Li (3)
        0.96,  // Be (4)
        0.84,  // B  (5)
        0.76,  // C  (6) - was 0.73
        0.71,  // N  (7)
        0.66,  // O  (8)
        0.57,  // F  (9) - was 0.64
        0.58,  // Ne (10)
        1.66,  // Na (11)
        1.41,  // Mg (12)
        1.21,  // Al (13)
        1.11,  // Si (14)
        1.07,  // P  (15)
        1.05,  // S  (16)
        1.02,  // Cl (17)
        1.06,  // Ar (18)
        2.03,  // K  (19)
        1.76,  // Ca (20)
        // Add more as needed...
        1.70,  // Sc (21)
        1.60,  // Ti (22)
        1.53,  // V  (23)
        1.39,  // Cr (24)
        1.39,  // Mn (25) - was 1.61
        1.32,  // Fe (26)
        1.26,  // Co (27)
        1.24,  // Ni (28)
        1.32,  // Cu (29)
        1.22,  // Zn (30)
        1.22,  // Ga (31)
        1.20,  // Ge (32)
        1.19,  // As (33)
        1.20,  // Se (34)
        1.20,  // Br (35)
        1.16,  // Kr (36)
        2.20,  // Rb (37)
        1.95,  // Sr (38)
        1.90,  // Y  (39)
        1.75,  // Zr (40)
        1.64,  // Nb (41)
        1.54,  // Mo (42)
        1.47,  // Tc (43)
        1.46,  // Ru (44)
        1.42,  // Rh (45)
        1.39,  // Pd (46)
        1.45,  // Ag (47)
        1.44,  // Cd (48)
        1.42,  // In (49)
        1.39,  // Sn (50)
        1.39,  // Sb (51)
        1.38,  // Te (52)
        1.39,  // I  (53)
        1.40,  // Xe (54)
    };
    
    constexpr int MAX_Z = sizeof(covalent_radii) / sizeof(covalent_radii[0]) - 1;
    
    if (Z_a > MAX_Z || Z_b > MAX_Z) {
        return false;  // Unknown element
    }
    
    double r_a = covalent_radii[Z_a];
    double r_b = covalent_radii[Z_b];
    
    double threshold = scale * (r_a + r_b);
    
    return distance <= threshold;
}

} // namespace vsepr
