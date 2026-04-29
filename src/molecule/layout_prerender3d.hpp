#pragma once
// ============================================================================
// layout_prerender3d.hpp — Glass Module: Fast Topology-Driven 3D Layout
// ============================================================================
// Produces approximate 3D atom positions from topology alone.
//
// Design goals:
//   - deterministic (seed-controlled)
//   - slightly organic-looking (torsional jitter, chain curvature)
//   - cheap (graph walk, not force-field optimisation)
//   - ring-aware (small cycles pinned to planar polygons)
//   - good enough for prerender visualization, not physically exact
// ============================================================================

#include "molecule_types.hpp"
#include "ring_detect.hpp"
#include <vector>
#include <cstdint>

namespace vsepr {
namespace glass {

// -----------------------------------------------------------------------
// Vec3f — lightweight float-precision 3D vector for layout positions.
// Exists separately from the double-precision vsepr::Vec3 in math_vec3.hpp
// because the prerender path is entirely float and we avoid coupling.
// -----------------------------------------------------------------------
struct Vec3f {
    float x{}, y{}, z{};

    Vec3f() = default;
    Vec3f(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    Vec3f operator+(const Vec3f& b) const { return {x + b.x, y + b.y, z + b.z}; }
    Vec3f operator-(const Vec3f& b) const { return {x - b.x, y - b.y, z - b.z}; }
    Vec3f operator*(float s)       const { return {x * s, y * s, z * s}; }
    Vec3f operator/(float s)       const { return {x / s, y / s, z / s}; }
    Vec3f& operator+=(const Vec3f& b) { x += b.x; y += b.y; z += b.z; return *this; }
};

float  dot3f(const Vec3f& a, const Vec3f& b);
Vec3f  cross3f(const Vec3f& a, const Vec3f& b);
float  norm3f(const Vec3f& v);
Vec3f  normalize3f(const Vec3f& v);

// -----------------------------------------------------------------------
// LayoutSettings — external knobs, kept lightweight.
// -----------------------------------------------------------------------
struct LayoutSettings {
    float    default_bond_length   = 1.45f;
    float    chain_curve_strength  = 0.18f;
    float    torsion_jitter        = 0.22f;
    float    branch_spread         = 0.95f;
    int      smoothing_passes      = 2;
    uint32_t random_seed           = 116u;
    float    ring_bond_length      = 1.40f;   // for planar ring placement
    float    chain_arc_amplitude   = 0.35f;   // long-chain sinusoidal arc
    uint32_t chain_arc_min_length  = 6;       // minimum chain atoms for arcing
};

// -----------------------------------------------------------------------
// LayoutResult — output of the layout engine.
// -----------------------------------------------------------------------
struct LayoutResult {
    std::vector<Vec3f> atom_positions;
};

// -----------------------------------------------------------------------
// TopologyPrerender3D — the fast prerender layout engine.
//
// Usage:
//   TopologyPrerender3D engine(settings);
//   LayoutResult layout = engine.build_layout(mol);
// -----------------------------------------------------------------------
class TopologyPrerender3D {
public:
    explicit TopologyPrerender3D(LayoutSettings s = {});

    LayoutResult build_layout(const GlassMolecule& mol);

private:
    LayoutSettings settings_;

    // Root selection: highest-degree atom (tie-break: heaviest Z)
    uint32_t choose_root(const GlassMolecule& mol) const;

    // Ring pre-placement: pin ring atoms to planar polygons
    void place_rings(
        const GlassMolecule& mol,
        const std::vector<Ring>& rings,
        std::vector<Vec3f>& pos,
        std::vector<bool>& placed
    );

    // Cheap chain-only smoothing pass (preserves ring atoms)
    void smooth_chains(
        const GlassMolecule& mol,
        std::vector<Vec3f>& pos,
        const std::vector<bool>& is_ring_atom
    );

    // Long-chain sinusoidal arc injection
    void apply_chain_arcs(
        const GlassMolecule& mol,
        std::vector<Vec3f>& pos,
        const std::vector<bool>& is_ring_atom
    );

    // Simple xorshift RNG for deterministic jitter
    static float rand_unit(uint32_t& state);

    // Orthogonal hint vector
    static Vec3f orthogonal_hint(const Vec3f& dir);

    // Bond length from covalent radii (with fallback)
    float bond_length(const GlassMolecule& mol, uint32_t a, uint32_t b) const;
};

} // namespace glass
} // namespace vsepr
