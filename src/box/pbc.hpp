#pragma once
/**
 * pbc.hpp - Periodic Boundary Conditions
 * 
 * Orthogonal periodic box with minimum image convention.
 * Handles:
 * - Coordinate wrapping into primary cell [0, L)
 * - Minimum image displacement (MIC) into (-L/2, L/2]
 * - Distance calculations with PBC
 * - Efficient caching of 1/L for performance
 * 
 * Future: Triclinic boxes require 3x3 matrix approach.
 */

#include "core/math_vec3.hpp"
#include <cmath>
#include <vector>

namespace vsepr {

/**
 * BoxOrtho - Orthogonal (rectangular) periodic box
 * 
 * Stores box dimensions L = (Lx, Ly, Lz) and cached inverse.
 * Box is disabled if any dimension <= 0.
 */
struct BoxOrtho {
    Vec3 L;      // Box lengths (Lx, Ly, Lz)
    Vec3 invL;   // Cached 1/L for performance
    
    // Constructors
    BoxOrtho() : L{0, 0, 0}, invL{0, 0, 0} {}
    
    explicit BoxOrtho(double Lx, double Ly, double Lz)
        : L{Lx, Ly, Lz}
        , invL{(Lx > 0 ? 1.0/Lx : 0), (Ly > 0 ? 1.0/Ly : 0), (Lz > 0 ? 1.0/Lz : 0)}
    {}
    
    explicit BoxOrtho(const Vec3& lengths)
        : BoxOrtho(lengths.x, lengths.y, lengths.z)
    {}
    
    // Check if PBC is enabled (all dimensions > 0)
    bool enabled() const {
        return (L.x > 0 && L.y > 0 && L.z > 0);
    }
    
    // Volume
    double volume() const {
        return L.x * L.y * L.z;
    }
    
    // Update box size (recalculates invL)
    void set_dimensions(double Lx, double Ly, double Lz) {
        L = {Lx, Ly, Lz};
        invL.x = (Lx > 0 ? 1.0/Lx : 0);
        invL.y = (Ly > 0 ? 1.0/Ly : 0);
        invL.z = (Lz > 0 ? 1.0/Lz : 0);
    }
    
    void set_dimensions(const Vec3& lengths) {
        set_dimensions(lengths.x, lengths.y, lengths.z);
    }
    
    /**
     * Wrap position into primary cell [0, L)
     * Uses floor() to handle negative coordinates correctly.
     * 
     * Example: r = -0.5, L = 10 → floor(-0.5/10) = -1 → r' = -0.5 - (-1)*10 = 9.5
     */
    Vec3 wrap(const Vec3& r) const {
        if (!enabled()) return r;
        
        double fx = std::floor(r.x * invL.x);
        double fy = std::floor(r.y * invL.y);
        double fz = std::floor(r.z * invL.z);
        
        return {
            r.x - fx * L.x,
            r.y - fy * L.y,
            r.z - fz * L.z
        };
    }
    
    /**
     * Minimum image displacement: dr = rj - ri
     * Wraps into (-L/2, L/2] using nearest integer rounding.
     * 
     * This is the key function for computing forces with PBC.
     * nearbyint() rounds to nearest integer (ties to even).
     */
    Vec3 delta(const Vec3& ri, const Vec3& rj) const {
        Vec3 dr = rj - ri;
        if (!enabled()) return dr;
        
        dr.x -= L.x * std::nearbyint(dr.x * invL.x);
        dr.y -= L.y * std::nearbyint(dr.y * invL.y);
        dr.z -= L.z * std::nearbyint(dr.z * invL.z);
        
        return dr;
    }
    
    /**
     * Squared distance with minimum image convention
     * Avoids sqrt for performance when only comparing distances.
     */
    double dist2(const Vec3& ri, const Vec3& rj) const {
        Vec3 dr = delta(ri, rj);
        return dr.norm2();
    }
    
    /**
     * Distance with minimum image convention
     */
    double dist(const Vec3& ri, const Vec3& rj) const {
        return std::sqrt(dist2(ri, rj));
    }
    
    /**
     * Wrap all coordinates in a flat array [x0,y0,z0, x1,y1,z1, ...]
     * In-place modification.
     */
    void wrap_coords(std::vector<double>& coords) const {
        if (!enabled()) return;
        
        const size_t N = coords.size() / 3;
        for (size_t i = 0; i < N; ++i) {
            Vec3 r(coords[3*i], coords[3*i+1], coords[3*i+2]);
            r = wrap(r);
            coords[3*i]   = r.x;
            coords[3*i+1] = r.y;
            coords[3*i+2] = r.z;
        }
    }
};

// Legacy interface for compatibility
using Box = BoxOrtho;

} // namespace vsepr
