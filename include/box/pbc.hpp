#pragma once
/**
 * pbc.hpp — Periodic Boundary Conditions
 * ========================================
 *
 * Public boundary/geometry primitive.  Used by potentials, FIRE, neighbor
 * lists, diffusion, crystal builders, and tests.  Not a potential.
 *
 * Canonical type:  vsepr::BoxOrtho
 *
 * Orthogonal box with:
 *   wrap()       — position into primary cell [0, L)       via floor()
 *   delta()      — minimum-image displacement (-L/2, L/2]  via nearbyint()
 *   dist2/dist() — MIC distance helpers
 *   wrap_coords()— in-place flat-array wrap
 *
 * enabled flag:
 *   Stored as a data member (bool enabled) so State-based code can read and
 *   write it directly.  Set automatically by the (Lx,Ly,Lz) constructor;
 *   can be overridden at runtime (e.g. to temporarily disable PBC).
 *
 * Rounding doctrine:
 *   delta() uses std::nearbyint() — ties to even, IEEE 754 default.
 *   This is the canonical choice.  Do not introduce a second implementation
 *   using std::round() — that is how silent ambiguity breeds.
 *
 * Day #57A: moved from src/box/pbc.hpp → include/box/pbc.hpp (public tree).
 */

#include "core/math_vec3.hpp"
#include <cmath>
#include <stdexcept>
#include <vector>

namespace vsepr {

struct BoxOrtho {
    Vec3 L;       // Box lengths (Lx, Ly, Lz)
    Vec3 invL;    // Cached 1/L for performance
    bool enabled; // PBC on/off — true when all dimensions > 0

    // Disabled box (default)
    BoxOrtho() : L{0, 0, 0}, invL{0, 0, 0}, enabled(false) {}

    explicit BoxOrtho(double Lx, double Ly, double Lz)
        : L{Lx, Ly, Lz}
        , invL{(Lx > 0 ? 1.0/Lx : 0), (Ly > 0 ? 1.0/Ly : 0), (Lz > 0 ? 1.0/Lz : 0)}
        , enabled(Lx > 0 && Ly > 0 && Lz > 0)
    {}

    explicit BoxOrtho(const Vec3& lengths)
        : BoxOrtho(lengths.x, lengths.y, lengths.z)
    {}

    // Volume (unchecked — caller should verify enabled first)
    double volume() const { return L.x * L.y * L.z; }

    // Update box size and re-derive enabled + invL
    void set_dimensions(double Lx, double Ly, double Lz) {
        L = {Lx, Ly, Lz};
        invL.x  = (Lx > 0 ? 1.0/Lx : 0);
        invL.y  = (Ly > 0 ? 1.0/Ly : 0);
        invL.z  = (Lz > 0 ? 1.0/Lz : 0);
        enabled = (Lx > 0 && Ly > 0 && Lz > 0);
    }

    void set_dimensions(const Vec3& lengths) {
        set_dimensions(lengths.x, lengths.y, lengths.z);
    }

    /**
     * Wrap position into primary cell [0, L).
     * floor() handles negative coordinates correctly.
     *   r = -0.5, L = 10  →  floor(-0.05) = -1  →  r' = 9.5
     */
    Vec3 wrap(const Vec3& r) const {
        if (!enabled) return r;
        return {
            r.x - L.x * std::floor(r.x * invL.x),
            r.y - L.y * std::floor(r.y * invL.y),
            r.z - L.z * std::floor(r.z * invL.z)
        };
    }

    /**
     * Minimum-image displacement: dr = rj - ri, wrapped into (-L/2, L/2].
     * nearbyint() — ties to even (IEEE 754 default, most numerically stable).
     */
    Vec3 delta(const Vec3& ri, const Vec3& rj) const {
        Vec3 dr = rj - ri;
        if (!enabled) return dr;
        dr.x -= L.x * std::nearbyint(dr.x * invL.x);
        dr.y -= L.y * std::nearbyint(dr.y * invL.y);
        dr.z -= L.z * std::nearbyint(dr.z * invL.z);
        return dr;
    }

    // Squared MIC distance (avoids sqrt when only comparing)
    double dist2(const Vec3& ri, const Vec3& rj) const {
        Vec3 dr = delta(ri, rj);
        return dr.norm2();
    }

    // MIC distance
    double dist(const Vec3& ri, const Vec3& rj) const {
        return std::sqrt(dist2(ri, rj));
    }

    /**
     * Wrap all coordinates in a flat array [x0,y0,z0, x1,y1,z1, ...] in-place.
     */
    void wrap_coords(std::vector<double>& coords) const {
        if (!enabled) return;
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

// Legacy name aliases — same type, no overhead
using Box    = BoxOrtho;
using BoxPBC = BoxOrtho;   // Day #57A: unification alias — use vsepr::BoxOrtho directly in new code

// ============================================================================
// WO-VSEPR-SIM-57A  Canonical PBC API
// ============================================================================
//
// These types are the beta-8 canonical interface for periodic boundary math.
// BoxOrtho above is the high-performance internal implementation; the types
// below are the public API consumed by FIRE, tests, and future interpreter
// bindings (57C).
//
// Naming canon:
//   PeriodicCell          — orthorhombic periodic cell with per-axis flags
//   BoundaryMode          — open / periodic / reflective / absorbing
//   BoundaryConfig        — per-axis mode + cell
//   ImageCount            — cumulative boundary crossings per particle
//   wrap_scalar           — 1-D wrap into [0, L)
//   wrap_position         — 3-D wrap using PeriodicCell
//   minimum_image_scalar  — 1-D MIC correction
//   minimum_image_delta   — 3-D MIC displacement vector
//   pbc_distance          — MIC distance scalar
//   update_image_count    — advance image counters (call BEFORE wrap)
//   unwrap_position       — reconstruct continuous position from image count
// ============================================================================

enum class BoundaryMode {
    Open,
    Periodic,
    Reflective,   // reserved — throws if used in math path
    Absorbing     // reserved — throws if used in math path
};

struct PeriodicCell {
    Vec3 lengths;
    bool periodic_x = false;
    bool periodic_y = false;
    bool periodic_z = false;

    bool enabled() const {
        return periodic_x || periodic_y || periodic_z;
    }

    void validate() const {
        if (periodic_x && lengths.x <= 0.0)
            throw std::runtime_error("Invalid periodic cell length Lx.");
        if (periodic_y && lengths.y <= 0.0)
            throw std::runtime_error("Invalid periodic cell length Ly.");
        if (periodic_z && lengths.z <= 0.0)
            throw std::runtime_error("Invalid periodic cell length Lz.");
    }
};

struct BoundaryConfig {
    BoundaryMode x = BoundaryMode::Open;
    BoundaryMode y = BoundaryMode::Open;
    BoundaryMode z = BoundaryMode::Open;
    PeriodicCell cell;
};

inline bool is_periodic_axis(BoundaryMode mode) {
    return mode == BoundaryMode::Periodic;
}

struct ImageCount {
    int ix = 0;
    int iy = 0;
    int iz = 0;
};

// ── Core free functions ────────────────────────────────────────────────────

// Wrap scalar coordinate into [0, L)
inline double wrap_scalar(double x, double L) {
    double y = std::fmod(x, L);
    return y < 0.0 ? y + L : y;
}

// Wrap 3-D position into primary cell using per-axis flags
inline Vec3 wrap_position(const Vec3& r, const PeriodicCell& cell) {
    cell.validate();
    Vec3 out = r;
    if (cell.periodic_x) out.x = wrap_scalar(out.x, cell.lengths.x);
    if (cell.periodic_y) out.y = wrap_scalar(out.y, cell.lengths.y);
    if (cell.periodic_z) out.z = wrap_scalar(out.z, cell.lengths.z);
    return out;
}

// Minimum-image correction on a single axis
inline double minimum_image_scalar(double dx, double L) {
    return dx - L * std::round(dx / L);
}

// Minimum-image displacement: dr = rj − ri, each periodic axis corrected
inline Vec3 minimum_image_delta(
    const Vec3& ri,
    const Vec3& rj,
    const PeriodicCell& cell
) {
    cell.validate();
    Vec3 dr = rj - ri;
    if (cell.periodic_x) dr.x = minimum_image_scalar(dr.x, cell.lengths.x);
    if (cell.periodic_y) dr.y = minimum_image_scalar(dr.y, cell.lengths.y);
    if (cell.periodic_z) dr.z = minimum_image_scalar(dr.z, cell.lengths.z);
    return dr;
}

// MIC distance scalar
inline double pbc_distance(
    const Vec3& ri,
    const Vec3& rj,
    const PeriodicCell& cell
) {
    Vec3 dr = minimum_image_delta(ri, rj, cell);
    return std::sqrt(dr.x*dr.x + dr.y*dr.y + dr.z*dr.z);
}

// Reconstruct continuous (unwrapped) position from wrapped position + image count
inline Vec3 unwrap_position(
    const Vec3& wrapped,
    const ImageCount& img,
    const PeriodicCell& cell
) {
    return {
        wrapped.x + img.ix * cell.lengths.x,
        wrapped.y + img.iy * cell.lengths.y,
        wrapped.z + img.iz * cell.lengths.z
    };
}

// Update image counters for boundary crossings.
// MUST be called with pre-wrap positions BEFORE wrap_position is applied.
// A step larger than L/2 on any axis is treated as one crossing; this is
// the correct assumption for simulations with dt small enough that particles
// cannot skip across the full cell in a single step.
inline void update_image_count(
    const Vec3& old_r,
    const Vec3& new_r,
    ImageCount& img,
    const PeriodicCell& cell
) {
    if (cell.periodic_x) {
        double dx = new_r.x - old_r.x;
        if (dx >  cell.lengths.x * 0.5) img.ix -= 1;
        if (dx < -cell.lengths.x * 0.5) img.ix += 1;
    }
    if (cell.periodic_y) {
        double dy = new_r.y - old_r.y;
        if (dy >  cell.lengths.y * 0.5) img.iy -= 1;
        if (dy < -cell.lengths.y * 0.5) img.iy += 1;
    }
    if (cell.periodic_z) {
        double dz = new_r.z - old_r.z;
        if (dz >  cell.lengths.z * 0.5) img.iz -= 1;
        if (dz < -cell.lengths.z * 0.5) img.iz += 1;
    }
}

} // namespace vsepr
