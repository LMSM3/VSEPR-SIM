#pragma once
/**
 * scene_builders.hpp — Synthetic Bead Scene Construction and Validation
 *
 * Provides:
 *   1. SceneBead: lightweight bead for scene construction
 *   2. Xorshift32: deterministic PRNG for reproducible random scenes
 *   3. Deterministic scene builders: geometric configurations
 *   4. Randomized scene generators: MC-style with controlled distributions
 *   5. Scene transforms: translate, rotate (rigid, preserves n_hat)
 *   6. Scene validation: duplicate detection, min distance, unit norms,
 *      centroid, bounding box
 *   7. Neighbour list builder
 *
 * Philosophy: state-first, not chemistry-first. Manufacture neighborhood
 * structure directly without dragging atomistic chemistry into every test.
 *
 * Note on SceneBead::eta:
 *   Stores the initial internal state for this bead. Scene builders
 *   initialize it to 0.0 by default. Use set_initial_eta() or assign
 *   directly to configure non-uniform initial eta distributions for
 *   hysteresis or memory tests.
 *
 * Note on neighbour cutoff:
 *   build_neighbours() returns ALL other beads in the scene as neighbours
 *   regardless of distance. The cutoff filtering (r_cutoff, sigma_rho)
 *   happens inside compute_fast_observables() in the environment state
 *   kernel. Scenes should be sized so that the intended neighbour count
 *   falls naturally from the spatial configuration and kernel parameters.
 *
 * Reference: Suite #2/#3 specification from development sessions
 */

#include "coarse_grain/core/environment_state.hpp"
#include "atomistic/core/state.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

namespace test_util {

// ============================================================================
// PRNG — Xorshift32
// ============================================================================

/**
 * Simple xorshift32 PRNG for reproducible test generation.
 * All randomized scene builders use this exclusively.
 */
struct Xorshift32 {
    uint32_t state;
    explicit Xorshift32(uint32_t seed) : state(seed ? seed : 1u) {}
    uint32_t next() {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }
    double uniform() {
        return (next() & 0x7FFFFFFF) / static_cast<double>(0x7FFFFFFF);
    }
    double uniform(double lo, double hi) {
        return lo + uniform() * (hi - lo);
    }
    atomistic::Vec3 random_direction() {
        constexpr double pi = 3.14159265358979323846;
        double theta = std::acos(2.0 * uniform() - 1.0);
        double phi = 2.0 * pi * uniform();
        return {std::sin(theta) * std::cos(phi),
                std::sin(theta) * std::sin(phi),
                std::cos(theta)};
    }
};

// ============================================================================
// Scene Bead — lightweight bead for scene construction
// ============================================================================

struct SceneBead {
    atomistic::Vec3 position{};
    atomistic::Vec3 n_hat{0, 0, 1};   // Primary orientation axis
    bool has_orientation{true};
    double eta{};                       // Initial internal state (see header note)
};

// ============================================================================
// Deterministic Scene Builders
// ============================================================================

/**
 * Isolated bead: single bead at origin, no neighbours.
 */
inline std::vector<SceneBead> scene_isolated() {
    return {SceneBead{{0, 0, 0}, {0, 0, 1}, true, 0.0}};
}

/**
 * Pair: two beads along x-axis at given separation.
 * Both oriented along z.
 */
inline std::vector<SceneBead> scene_pair(double separation) {
    return {
        SceneBead{{0, 0, 0},          {0, 0, 1}, true, 0.0},
        SceneBead{{separation, 0, 0}, {0, 0, 1}, true, 0.0}
    };
}

/**
 * Linear aligned stack: N beads along z-axis, all oriented along z.
 * Spacing is uniform.
 */
inline std::vector<SceneBead> scene_linear_stack(int n, double spacing) {
    std::vector<SceneBead> beads(n);
    for (int i = 0; i < n; ++i) {
        beads[i].position = {0, 0, i * spacing};
        beads[i].n_hat = {0, 0, 1};
        beads[i].has_orientation = true;
    }
    return beads;
}

/**
 * T-shape: 3 beads. Central bead at origin, one above along z,
 * one to the right along x. Central bead oriented along z,
 * side bead oriented along x (perpendicular).
 */
inline std::vector<SceneBead> scene_t_shape(double spacing) {
    return {
        SceneBead{{0, 0, 0},       {0, 0, 1}, true, 0.0},       // center
        SceneBead{{0, 0, spacing},  {0, 0, 1}, true, 0.0},       // top (aligned)
        SceneBead{{spacing, 0, 0},  {1, 0, 0}, true, 0.0}        // right (perp)
    };
}

/**
 * Square: 4 beads in xy-plane, all oriented along z.
 */
inline std::vector<SceneBead> scene_square(double side) {
    double h = side / 2.0;
    return {
        SceneBead{{-h, -h, 0}, {0, 0, 1}, true, 0.0},
        SceneBead{{ h, -h, 0}, {0, 0, 1}, true, 0.0},
        SceneBead{{ h,  h, 0}, {0, 0, 1}, true, 0.0},
        SceneBead{{-h,  h, 0}, {0, 0, 1}, true, 0.0}
    };
}

/**
 * Triangle cluster: 3 beads in xy-plane forming equilateral triangle.
 * All oriented along z.
 */
inline std::vector<SceneBead> scene_triangle(double side) {
    double h = side * std::sqrt(3.0) / 2.0;
    return {
        SceneBead{{0,          0, 0}, {0, 0, 1}, true, 0.0},
        SceneBead{{side,       0, 0}, {0, 0, 1}, true, 0.0},
        SceneBead{{side / 2.0, h, 0}, {0, 0, 1}, true, 0.0}
    };
}

/**
 * Tetrahedral cluster: 4 beads at regular tetrahedron vertices.
 * All oriented along z.
 */
inline std::vector<SceneBead> scene_tetrahedron(double edge) {
    double a = edge / std::sqrt(2.0);
    return {
        SceneBead{{ a,  a,  a}, {0, 0, 1}, true, 0.0},
        SceneBead{{ a, -a, -a}, {0, 0, 1}, true, 0.0},
        SceneBead{{-a,  a, -a}, {0, 0, 1}, true, 0.0},
        SceneBead{{-a, -a,  a}, {0, 0, 1}, true, 0.0}
    };
}

/**
 * Dense shell: N beads evenly distributed on a sphere of given radius
 * around a central bead. All oriented along z.
 * Uses Fibonacci spiral for quasi-uniform distribution.
 */
inline std::vector<SceneBead> scene_dense_shell(int n_shell, double radius) {
    std::vector<SceneBead> beads;
    // Central bead
    beads.push_back(SceneBead{{0, 0, 0}, {0, 0, 1}, true, 0.0});

    constexpr double pi = 3.14159265358979323846;
    constexpr double golden = 1.6180339887498949;

    for (int i = 0; i < n_shell; ++i) {
        double theta = std::acos(1.0 - 2.0 * (i + 0.5) / n_shell);
        double phi = 2.0 * pi * i / golden;
        double x = radius * std::sin(theta) * std::cos(phi);
        double y = radius * std::sin(theta) * std::sin(phi);
        double z = radius * std::cos(theta);
        beads.push_back(SceneBead{{x, y, z}, {0, 0, 1}, true, 0.0});
    }
    return beads;
}

/**
 * Planar ring: N beads uniformly distributed on a circle in the xy-plane.
 * All oriented along z (perpendicular to ring plane).
 */
inline std::vector<SceneBead> scene_ring(int n, double radius) {
    constexpr double pi = 3.14159265358979323846;
    std::vector<SceneBead> beads(n);
    for (int i = 0; i < n; ++i) {
        double theta = 2.0 * pi * i / n;
        beads[i].position = {radius * std::cos(theta), radius * std::sin(theta), 0};
        beads[i].n_hat = {0, 0, 1};
        beads[i].has_orientation = true;
    }
    return beads;
}

/**
 * Perfect cubic lattice: n_side^3 beads on a regular grid.
 * All oriented along z.
 */
inline std::vector<SceneBead> scene_cubic_lattice(int n_side, double spacing) {
    std::vector<SceneBead> beads;
    beads.reserve(n_side * n_side * n_side);
    double offset = -(n_side - 1) * spacing / 2.0;
    for (int ix = 0; ix < n_side; ++ix) {
        for (int iy = 0; iy < n_side; ++iy) {
            for (int iz = 0; iz < n_side; ++iz) {
                SceneBead b;
                b.position = {offset + ix * spacing,
                              offset + iy * spacing,
                              offset + iz * spacing};
                b.n_hat = {0, 0, 1};
                b.has_orientation = true;
                beads.push_back(b);
            }
        }
    }
    return beads;
}

/**
 * Line bundle: N beads along the z-axis with controlled inter-bead spacing.
 * Orientations all along z (perfectly aligned).
 */
inline std::vector<SceneBead> scene_line_bundle(int n, double spacing) {
    std::vector<SceneBead> beads(n);
    double offset = -(n - 1) * spacing / 2.0;
    for (int i = 0; i < n; ++i) {
        beads[i].position = {0, 0, offset + i * spacing};
        beads[i].n_hat = {0, 0, 1};
        beads[i].has_orientation = true;
    }
    return beads;
}

/**
 * Layered slab: n_layers planes in xy, each with n_per_side^2 beads
 * on a square grid. Layer normal along z.
 */
inline std::vector<SceneBead> scene_layered_slab(
    int n_per_side, int n_layers, double in_plane_spacing, double layer_spacing)
{
    std::vector<SceneBead> beads;
    beads.reserve(n_per_side * n_per_side * n_layers);
    double xy_off = -(n_per_side - 1) * in_plane_spacing / 2.0;
    double z_off  = -(n_layers - 1) * layer_spacing / 2.0;
    for (int iz = 0; iz < n_layers; ++iz) {
        for (int ix = 0; ix < n_per_side; ++ix) {
            for (int iy = 0; iy < n_per_side; ++iy) {
                SceneBead b;
                b.position = {xy_off + ix * in_plane_spacing,
                              xy_off + iy * in_plane_spacing,
                              z_off  + iz * layer_spacing};
                b.n_hat = {0, 0, 1};
                b.has_orientation = true;
                beads.push_back(b);
            }
        }
    }
    return beads;
}

// ============================================================================
// Randomized Scene Generators (all use Xorshift32)
// ============================================================================

/**
 * Random cluster: N beads within a box of given size.
 * All have random orientations. No minimum-separation guarantee.
 */
inline std::vector<SceneBead> scene_random_cluster(
    int n, double box_size, uint32_t seed = 42)
{
    Xorshift32 rng(seed);
    std::vector<SceneBead> beads(n);
    for (int i = 0; i < n; ++i) {
        beads[i].position = {
            rng.uniform(-box_size / 2.0, box_size / 2.0),
            rng.uniform(-box_size / 2.0, box_size / 2.0),
            rng.uniform(-box_size / 2.0, box_size / 2.0)
        };
        beads[i].n_hat = rng.random_direction();
        beads[i].has_orientation = true;
    }
    return beads;
}

/**
 * Random cloud: N beads with random positions in a box and random orientations.
 * Center bead at origin; N-1 neighbours scattered in a box.
 */
inline std::vector<SceneBead> scene_random_cloud(
    int n, double box_size, uint32_t seed = 42)
{
    Xorshift32 rng(seed);
    std::vector<SceneBead> beads;
    // Center bead at origin
    SceneBead center;
    center.position = {0, 0, 0};
    center.n_hat = rng.random_direction();
    center.has_orientation = true;
    beads.push_back(center);
    // N-1 neighbours
    for (int i = 1; i < n; ++i) {
        SceneBead b;
        b.position = {rng.uniform(-box_size/2, box_size/2),
                      rng.uniform(-box_size/2, box_size/2),
                      rng.uniform(-box_size/2, box_size/2)};
        b.n_hat = rng.random_direction();
        b.has_orientation = true;
        beads.push_back(b);
    }
    return beads;
}

/**
 * Random shell: center bead + N neighbours on a spherical shell at
 * given radius, with random orientations.
 */
inline std::vector<SceneBead> scene_random_shell(
    int n_shell, double radius, uint32_t seed = 42)
{
    Xorshift32 rng(seed);
    std::vector<SceneBead> beads;
    // Center bead at origin
    SceneBead center;
    center.position = {0, 0, 0};
    center.n_hat = {0, 0, 1};
    center.has_orientation = true;
    beads.push_back(center);
    // Shell neighbours
    for (int i = 0; i < n_shell; ++i) {
        atomistic::Vec3 dir = rng.random_direction();
        SceneBead b;
        b.position = dir * radius;
        b.n_hat = rng.random_direction();
        b.has_orientation = true;
        beads.push_back(b);
    }
    return beads;
}

/**
 * Biased stack cloud: N beads in a column along z with controlled jitter.
 * align_bias in [0,1]: 0 = random orientations, 1 = perfectly aligned.
 * position_jitter: max lateral displacement from stack axis.
 */
inline std::vector<SceneBead> scene_biased_stack_cloud(
    int n, double spacing, double align_bias, double position_jitter,
    uint32_t seed = 42)
{
    Xorshift32 rng(seed);
    std::vector<SceneBead> beads;
    for (int i = 0; i < n; ++i) {
        SceneBead b;
        double dx = rng.uniform(-position_jitter, position_jitter);
        double dy = rng.uniform(-position_jitter, position_jitter);
        b.position = {dx, dy, i * spacing};

        // Orientation: blend between z-aligned and random
        if (rng.uniform() < align_bias) {
            b.n_hat = {0, 0, 1};
        } else {
            b.n_hat = rng.random_direction();
        }
        b.has_orientation = true;
        beads.push_back(b);
    }
    return beads;
}

/**
 * Perturbed cubic lattice: perfect lattice with random displacement
 * up to jitter_fraction * spacing from each lattice site.
 */
inline std::vector<SceneBead> scene_perturbed_lattice(
    int n_side, double spacing, double jitter_fraction, uint32_t seed = 42)
{
    auto beads = scene_cubic_lattice(n_side, spacing);
    Xorshift32 rng(seed);
    double jitter = jitter_fraction * spacing;
    for (auto& b : beads) {
        b.position.x += rng.uniform(-jitter, jitter);
        b.position.y += rng.uniform(-jitter, jitter);
        b.position.z += rng.uniform(-jitter, jitter);
        b.n_hat = rng.random_direction();
    }
    return beads;
}

/**
 * Large shell initialization: center bead + N_shell beads on a sphere
 * with random orientations (for large-N studies).
 */
inline std::vector<SceneBead> scene_large_shell(
    int n_shell, double radius, uint32_t seed = 42)
{
    return scene_random_shell(n_shell, radius, seed);
}

/**
 * Separated random cloud: N beads with random positions in a box,
 * guaranteeing that no two beads are closer than min_sep.
 *
 * Uses rejection sampling: positions that violate the minimum separation
 * are re-drawn (up to max_attempts per bead). If placement fails, the
 * bead is placed at the last attempted position with a warning flag.
 *
 * Returns the scene. Use scene_min_distance() to verify post-hoc.
 */
inline std::vector<SceneBead> scene_separated_cloud(
    int n, double box_size, double min_sep, uint32_t seed = 42,
    int max_attempts = 1000)
{
    Xorshift32 rng(seed);
    double min_sep2 = min_sep * min_sep;
    std::vector<SceneBead> beads;
    beads.reserve(n);

    for (int i = 0; i < n; ++i) {
        SceneBead b;
        b.n_hat = rng.random_direction();
        b.has_orientation = true;

        bool placed = false;
        for (int attempt = 0; attempt < max_attempts; ++attempt) {
            b.position = {rng.uniform(-box_size/2, box_size/2),
                          rng.uniform(-box_size/2, box_size/2),
                          rng.uniform(-box_size/2, box_size/2)};

            bool ok = true;
            for (const auto& existing : beads) {
                atomistic::Vec3 dr = b.position - existing.position;
                double d2 = atomistic::dot(dr, dr);
                if (d2 < min_sep2) { ok = false; break; }
            }
            if (ok) { placed = true; break; }
        }
        (void)placed;  // best-effort; use scene_min_distance() to verify
        beads.push_back(b);
    }
    return beads;
}

// ============================================================================
// Monte Carlo Scene Configuration
// ============================================================================

/**
 * MCSceneConfig — controlled distribution parameters for random scene
 * generation.
 *
 * Inputs:
 *   n_min, n_max:      neighbour count range (uniform draw)
 *   r_min, r_max:      radial distance range for neighbours
 *   align_bias:        0 = random orientations, 1 = fully aligned to z
 *   seed:              PRNG seed for reproducibility
 */
struct MCSceneConfig {
    int n_min{3};
    int n_max{12};
    double r_min{2.0};
    double r_max{7.0};
    double align_bias{0.0};   // 0 = random, 1 = fully aligned
    uint32_t seed{42};
};

/**
 * Generate a single scene from MC configuration.
 * Returns center bead at origin + N neighbours in [r_min, r_max] shell.
 */
inline std::vector<SceneBead> generate_mc_scene(const MCSceneConfig& cfg) {
    Xorshift32 rng(cfg.seed);
    int n = cfg.n_min + static_cast<int>(rng.uniform() * (cfg.n_max - cfg.n_min + 1));
    if (n < cfg.n_min) n = cfg.n_min;
    if (n > cfg.n_max) n = cfg.n_max;

    std::vector<SceneBead> beads;
    // Center bead at origin, z-aligned
    SceneBead center;
    center.position = {0, 0, 0};
    center.n_hat = {0, 0, 1};
    center.has_orientation = true;
    beads.push_back(center);

    for (int i = 0; i < n; ++i) {
        SceneBead b;
        atomistic::Vec3 dir = rng.random_direction();
        double r = cfg.r_min + rng.uniform() * (cfg.r_max - cfg.r_min);
        b.position = dir * r;

        if (rng.uniform() < cfg.align_bias) {
            b.n_hat = {0, 0, 1};
        } else {
            b.n_hat = rng.random_direction();
        }
        b.has_orientation = true;
        beads.push_back(b);
    }
    return beads;
}

// ============================================================================
// Neighbour List Builder
// ============================================================================

/**
 * Build NeighbourInfo list for a specific bead from a scene.
 *
 * Returns ALL other beads as neighbours. The environment state kernel
 * (compute_fast_observables) applies its own distance-based weighting
 * via r_cutoff and sigma_rho, so no pre-filtering is needed here.
 */
inline std::vector<coarse_grain::NeighbourInfo> build_neighbours(
    const std::vector<SceneBead>& scene,
    int bead_index)
{
    std::vector<coarse_grain::NeighbourInfo> nbs;
    const auto& center = scene[bead_index];
    for (int i = 0; i < static_cast<int>(scene.size()); ++i) {
        if (i == bead_index) continue;
        atomistic::Vec3 dr = scene[i].position - center.position;
        double dist = atomistic::norm(dr);
        nbs.push_back({dist, scene[i].n_hat, scene[i].has_orientation});
    }
    return nbs;
}

// ============================================================================
// Scene Transforms
// ============================================================================

/**
 * Translate all beads in a scene by a given offset.
 * Orientations are unaffected (translation invariance).
 */
inline std::vector<SceneBead> translate_scene(
    const std::vector<SceneBead>& scene,
    const atomistic::Vec3& offset)
{
    auto result = scene;
    for (auto& b : result) {
        b.position = b.position + offset;
    }
    return result;
}

/**
 * Rotate all beads in a scene about an axis through the origin.
 *
 * Applies Rodrigues' rotation formula to both positions and orientation
 * axes (n_hat). This is a rigid rotation: inter-bead distances and
 * relative orientations are preserved.
 *
 * axis:  rotation axis (will be normalized internally)
 * angle: rotation angle in radians
 */
inline std::vector<SceneBead> rotate_scene(
    const std::vector<SceneBead>& scene,
    const atomistic::Vec3& axis,
    double angle)
{
    // Normalize axis
    double axis_len = atomistic::norm(axis);
    if (axis_len < 1e-15) return scene;  // degenerate axis, no rotation
    atomistic::Vec3 k = {axis.x / axis_len, axis.y / axis_len, axis.z / axis_len};

    double c = std::cos(angle);
    double s = std::sin(angle);
    double t = 1.0 - c;

    // Rodrigues: v' = v*cos(a) + (k x v)*sin(a) + k*(k.v)*(1-cos(a))
    auto rotate_vec = [&](const atomistic::Vec3& v) -> atomistic::Vec3 {
        // k x v
        atomistic::Vec3 kxv = {
            k.y * v.z - k.z * v.y,
            k.z * v.x - k.x * v.z,
            k.x * v.y - k.y * v.x
        };
        double kdv = atomistic::dot(k, v);
        return {
            v.x * c + kxv.x * s + k.x * kdv * t,
            v.y * c + kxv.y * s + k.y * kdv * t,
            v.z * c + kxv.z * s + k.z * kdv * t
        };
    };

    auto result = scene;
    for (auto& b : result) {
        b.position = rotate_vec(b.position);
        if (b.has_orientation) {
            b.n_hat = rotate_vec(b.n_hat);
        }
    }
    return result;
}

// ============================================================================
// Scene Validation Helpers
// ============================================================================

/**
 * SceneInfo — summary diagnostics for a scene.
 */
struct SceneInfo {
    int n_beads{};
    double min_pair_distance{};
    bool has_duplicate_positions{};
    bool all_orientations_unit{};
    atomistic::Vec3 centroid{};
    atomistic::Vec3 bbox_min{};
    atomistic::Vec3 bbox_max{};
};

/**
 * Compute the minimum pairwise distance in a scene.
 * Returns infinity for scenes with < 2 beads.
 */
inline double scene_min_distance(const std::vector<SceneBead>& scene) {
    if (scene.size() < 2) return std::numeric_limits<double>::infinity();
    double min_d2 = std::numeric_limits<double>::max();
    for (size_t i = 0; i < scene.size(); ++i) {
        for (size_t j = i + 1; j < scene.size(); ++j) {
            atomistic::Vec3 dr = scene[j].position - scene[i].position;
            double d2 = atomistic::dot(dr, dr);
            if (d2 < min_d2) min_d2 = d2;
        }
    }
    return std::sqrt(min_d2);
}

/**
 * Check if any two beads share the same position (within tolerance).
 */
inline bool scene_has_duplicates(const std::vector<SceneBead>& scene,
                                  double tol = 1e-12) {
    double tol2 = tol * tol;
    for (size_t i = 0; i < scene.size(); ++i) {
        for (size_t j = i + 1; j < scene.size(); ++j) {
            atomistic::Vec3 dr = scene[j].position - scene[i].position;
            if (atomistic::dot(dr, dr) < tol2) return true;
        }
    }
    return false;
}

/**
 * Check that all orientation vectors are unit length (within tolerance).
 */
inline bool scene_all_unit_normals(const std::vector<SceneBead>& scene,
                                    double tol = 1e-10) {
    for (const auto& b : scene) {
        if (!b.has_orientation) continue;
        double len = atomistic::norm(b.n_hat);
        if (std::abs(len - 1.0) > tol) return false;
    }
    return true;
}

/**
 * Compute the centroid (center of mass, equal weights) of a scene.
 */
inline atomistic::Vec3 scene_centroid(const std::vector<SceneBead>& scene) {
    if (scene.empty()) return {0, 0, 0};
    double sx = 0, sy = 0, sz = 0;
    for (const auto& b : scene) {
        sx += b.position.x;
        sy += b.position.y;
        sz += b.position.z;
    }
    double n = static_cast<double>(scene.size());
    return {sx / n, sy / n, sz / n};
}

/**
 * Compute axis-aligned bounding box of a scene.
 * Returns {min_corner, max_corner}.
 */
inline std::pair<atomistic::Vec3, atomistic::Vec3> scene_bounding_box(
    const std::vector<SceneBead>& scene)
{
    if (scene.empty()) return {{0,0,0}, {0,0,0}};
    double xlo = scene[0].position.x, xhi = xlo;
    double ylo = scene[0].position.y, yhi = ylo;
    double zlo = scene[0].position.z, zhi = zlo;
    for (const auto& b : scene) {
        if (b.position.x < xlo) xlo = b.position.x;
        if (b.position.x > xhi) xhi = b.position.x;
        if (b.position.y < ylo) ylo = b.position.y;
        if (b.position.y > yhi) yhi = b.position.y;
        if (b.position.z < zlo) zlo = b.position.z;
        if (b.position.z > zhi) zhi = b.position.z;
    }
    return {{xlo, ylo, zlo}, {xhi, yhi, zhi}};
}

/**
 * Full scene validation: compute all diagnostics in one pass.
 */
inline SceneInfo validate_scene(const std::vector<SceneBead>& scene) {
    SceneInfo info;
    info.n_beads = static_cast<int>(scene.size());
    info.min_pair_distance = scene_min_distance(scene);
    info.has_duplicate_positions = scene_has_duplicates(scene);
    info.all_orientations_unit = scene_all_unit_normals(scene);
    info.centroid = scene_centroid(scene);
    auto [bmin, bmax] = scene_bounding_box(scene);
    info.bbox_min = bmin;
    info.bbox_max = bmax;
    return info;
}

// ============================================================================
// Initial Eta Configuration
// ============================================================================

/**
 * Set uniform initial eta for all beads in a scene.
 */
inline void set_initial_eta(std::vector<SceneBead>& scene, double eta_val) {
    for (auto& b : scene) b.eta = eta_val;
}

/**
 * Set per-bead initial eta from a vector. Sizes must match.
 */
inline void set_initial_eta(std::vector<SceneBead>& scene,
                             const std::vector<double>& eta_vals) {
    int n = static_cast<int>(std::min(scene.size(), eta_vals.size()));
    for (int i = 0; i < n; ++i) scene[i].eta = eta_vals[i];
}

// ============================================================================
// Bead-Order Permutation
// ============================================================================

/**
 * Permute bead order in a scene using a deterministic shuffle (Fisher-Yates).
 * Useful for proving bead-order invariance: aggregate statistics should
 * be identical regardless of index ordering.
 */
inline std::vector<SceneBead> permute_scene(
    const std::vector<SceneBead>& scene,
    uint32_t seed)
{
    auto result = scene;
    Xorshift32 rng(seed);
    int n = static_cast<int>(result.size());
    for (int i = n - 1; i > 0; --i) {
        int j = static_cast<int>(rng.uniform() * (i + 1));
        if (j > i) j = i;
        std::swap(result[i], result[j]);
    }
    return result;
}

} // namespace test_util
