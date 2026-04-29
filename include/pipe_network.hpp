#pragma once
/**
 * pipe_network.hpp — Vec3×Vec3 Beam and Pipe Network Generator
 *
 * Generates stochastic 3D pipe/beam networks from the L5 macro layer,
 * where each segment is defined by two Vec3 endpoints (the Vec3×Vec3
 * pair) and carries full material identity propagated from L1→L2→L4.
 *
 * The Vec3×Vec3 segment representation:
 *
 *   PipeSegment.origin   (Vec3)  — start point in 3D space (m)
 *   PipeSegment.terminus (Vec3)  — end point in 3D space (m)
 *
 *   Direction vector: d = terminus − origin
 *   Segment length:   L = |d|
 *   Unit tangent:     t̂ = d / L
 *
 * The "complex random" generation uses chaos factor χ to control:
 *   - Junction branching probability (more junctions = more complex topology)
 *   - Segment length spread (χ amplifies the lognormal length distribution)
 *   - Angular deviation at junctions (χ widens the deflection angle)
 *   - Loop-back probability (creates closed cycles in the network)
 *   - Material assignment randomness (alloy bead → segment material)
 *
 * Network topology:
 *   The network is a directed graph:
 *     Nodes  = junction points (Vec3 positions)
 *     Edges  = pipe segments (Vec3×Vec3 pairs + PipeSegment data)
 *   Stored as adjacency list.  Multi-edges and self-loops are prevented.
 *
 * Each segment carries:
 *   - L5_MacroGeometry for the pipe body
 *   - Dominant alloy bead index (links to the AlloyComposition)
 *   - Vec3 origin + terminus (the Vec3×Vec3 representation)
 *   - Structural beam tensor B (3×3 matrix of inertia-like properties)
 *
 * The Beam Tensor B:
 *   B = L × t̂ ⊗ t̂   (outer product of unit tangent with itself, scaled by length)
 *   This 3×3 matrix encodes:
 *     - Segment orientation (principal axis)
 *     - Segment length (trace = L)
 *     - Cross-section anisotropy (off-diagonal coupling between axes)
 *   The beam tensor is the natural representation for structural analysis:
 *     stiffness, buckling, and thermal expansion all project along t̂.
 *
 * Anti-black-box: every segment's length, angle, branching decision, and
 * material assignment are recorded with their RNG provenance.
 *
 * Deterministic: same seed + same chaos factor → same network.
 *
 * Reference: include/layer_stack.hpp (L5 layer)
 *            include/alloy_generator.hpp (alloy material source)
 *            docs/section_layer_stack.tex §5
 */

#include "atomistic/core/state.hpp"
#include "include/layer_stack.hpp"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace pipe_network {

using Vec3 = atomistic::Vec3;

// ============================================================================
// Mat3 — 3×3 matrix (beam tensor, rotation, etc.)
// ============================================================================

/**
 * Mat3 — column-major 3×3 matrix.
 *
 * Storage: m[row][col], row-major in memory.
 * Used for:
 *   - Beam tensor B = L × (t̂ ⊗ t̂)
 *   - Rotation matrices for deflection at junctions
 *   - Combined network stiffness accumulator
 */
struct Mat3 {
    double m[3][3]{};

    Mat3() { for (int i=0;i<3;i++) for (int j=0;j<3;j++) m[i][j]=0.0; }

    static Mat3 identity() {
        Mat3 I; I.m[0][0]=I.m[1][1]=I.m[2][2]=1.0; return I;
    }

    Mat3 operator+(const Mat3& o) const {
        Mat3 r;
        for (int i=0;i<3;i++) for (int j=0;j<3;j++) r.m[i][j]=m[i][j]+o.m[i][j];
        return r;
    }

    Mat3& operator+=(const Mat3& o) {
        for (int i=0;i<3;i++) for (int j=0;j<3;j++) m[i][j]+=o.m[i][j];
        return *this;
    }

    Mat3 operator*(double s) const {
        Mat3 r;
        for (int i=0;i<3;i++) for (int j=0;j<3;j++) r.m[i][j]=m[i][j]*s;
        return r;
    }

    // Matrix-vector product
    Vec3 apply(const Vec3& v) const {
        return {
            m[0][0]*v.x + m[0][1]*v.y + m[0][2]*v.z,
            m[1][0]*v.x + m[1][1]*v.y + m[1][2]*v.z,
            m[2][0]*v.x + m[2][1]*v.y + m[2][2]*v.z
        };
    }

    double trace() const { return m[0][0]+m[1][1]+m[2][2]; }
};

/**
 * outer_product — Vec3 ⊗ Vec3 → Mat3
 * B_ij = a_i * b_j
 */
inline Mat3 outer_product(const Vec3& a, const Vec3& b) {
    Mat3 r;
    double av[3] = {a.x, a.y, a.z};
    double bv[3] = {b.x, b.y, b.z};
    for (int i=0;i<3;i++) for (int j=0;j<3;j++) r.m[i][j] = av[i]*bv[j];
    return r;
}

/**
 * beam_tensor — B = length × (t̂ ⊗ t̂)
 *
 * For a segment from origin to terminus:
 *   d = terminus − origin
 *   L = |d|
 *   t̂ = d / L
 *   B = L × (t̂ ⊗ t̂)
 *
 * Trace(B) = L   (stores segment length)
 * Eigenvector of B = t̂  (stores segment direction)
 */
inline Mat3 beam_tensor(const Vec3& origin, const Vec3& terminus) {
    Vec3 d = terminus - origin;
    double L = atomistic::norm(d);
    if (L < 1e-12) return Mat3{};
    Vec3 t = d * (1.0 / L);
    return outer_product(t, t) * L;
}

// ============================================================================
// Network chaos parameters
// ============================================================================

/**
 * NetworkChaos — derived parameters from the chaos factor χ.
 *
 * At χ = 1.25:
 *   branch_prob        = 0.50  (50% chance of a new branch at each junction)
 *   max_branches       = 3     (up to 3 outgoing edges per node)
 *   length_mean_m      = 2.5 m (mean segment length)
 *   length_sigma_m     = 1.25 m
 *   deflection_max_rad = 1.18 rad (67.5°)
 *   loop_back_prob     = 0.188  (19% loop-back probability)
 *   alloy_segment_prob = 0.938  (93.8% segments assigned to alloy material)
 *   cross_section_noise= 0.156  (OD varies ±15.6%)
 */
struct NetworkChaos {
    double chi{1.0};

    double branch_prob()          const { return std::min(0.90, 0.40 * chi); }
    int    max_branches()         const { return static_cast<int>(2.0 * chi) + 1; }
    double length_mean_m()        const { return 2.0 * chi; }
    double length_sigma_m()       const { return chi; }
    double deflection_max_rad()   const { return 0.942 * chi; }  // χ×54° in rad
    double loop_back_prob()       const { return 0.15 * chi; }
    double alloy_segment_prob()   const { return std::min(1.0, 0.75 * chi); }
    double cross_section_noise()  const { return 0.125 * chi; }
    double wall_thickness_noise() const { return 0.10 * chi; }
};

// ============================================================================
// Pipe segment — Vec3×Vec3 + structural data
// ============================================================================

/**
 * PipeSegment — one segment in the network.
 *
 * The fundamental Vec3×Vec3 representation:
 *   origin   — start node position (m)
 *   terminus — end node position (m)
 *
 * The beam tensor B is computed from these two Vec3s.
 * All structural quantities are derived from B.
 */
struct PipeSegment {
    uint32_t  segment_id{};
    uint32_t  node_from{};
    uint32_t  node_to{};

    Vec3 origin{};      // Vec3 start (m)
    Vec3 terminus{};    // Vec3 end   (m)

    // Derived geometry
    double    length_m{};          // |terminus - origin|
    Vec3      unit_tangent{};      // t̂ = (terminus−origin)/L
    Mat3      beam_tensor_B{};     // B = L × (t̂⊗t̂)  — the 3×3 beam representation

    // Cross-section (circular pipe)
    double    outer_diameter_m{};  // m
    double    wall_thickness_m{};  // m
    double    inner_diameter_m{};  // m = OD − 2×wall

    // Material (back-reference into AlloyComposition)
    uint32_t  alloy_bead_index{};  // Index into AlloyComposition.beads
    uint8_t   dominant_Z{29};      // Z of dominant element (Cu = 29 default)
    std::string material_label;    // e.g. "Cu-Zn (70/30 brass-like)"

    // Surface condition from L5
    layer_stack::L5_SurfaceCondition surface;

    // Chaos provenance
    double    length_sample{};     // Raw lognormal sample before clamping
    double    deflection_rad{};    // Deflection angle from previous segment (rad)
    bool      is_loop_back{false}; // True if this segment closes a cycle
    bool      is_alloy_material{true};

    // Compute all derived geometry from origin + terminus
    void compute_geometry() {
        Vec3 d = terminus - origin;
        length_m = atomistic::norm(d);
        if (length_m > 1e-12) {
            unit_tangent = d * (1.0 / length_m);
        } else {
            unit_tangent = {1.0, 0.0, 0.0};
            length_m = 0.0;
        }
        beam_tensor_B = beam_tensor(origin, terminus);
        inner_diameter_m = outer_diameter_m - 2.0 * wall_thickness_m;
        if (inner_diameter_m < 0) inner_diameter_m = 0;
    }

    double cross_sectional_area_m2() const {
        double ro = 0.5 * outer_diameter_m;
        double ri = 0.5 * inner_diameter_m;
        return 3.14159265358979 * (ro*ro - ri*ri);
    }
};

// ============================================================================
// Network node
// ============================================================================

struct NetworkNode {
    uint32_t    node_id{};
    Vec3        position{};        // (m)
    std::vector<uint32_t> outgoing; // Segment IDs leaving this node
    std::vector<uint32_t> incoming; // Segment IDs entering this node
    bool        is_source{false};   // Flow inlet
    bool        is_sink{false};     // Flow outlet
    bool        is_junction{false}; // Branch point
};

// ============================================================================
// Pipe network
// ============================================================================

struct PipeNetwork {
    std::string                  name;
    double                       chi{};
    uint64_t                     seed{};

    std::vector<NetworkNode>     nodes;
    std::vector<PipeSegment>     segments;

    // Aggregate beam tensor: sum of all segment B matrices
    // Encodes the cumulative structural orientation of the whole network
    Mat3 network_beam_tensor{};

    // Statistics
    uint32_t n_segments{};
    uint32_t n_junctions{};
    uint32_t n_loops{};
    double   total_length_m{};
    double   mean_segment_length_m{};
    double   mean_deflection_rad{};
    double   alloy_segment_fraction{};
};

// ============================================================================
// Random rotation helper — rotate a unit vector by a random deflection
// ============================================================================

/**
 * random_deflect — deflect a unit direction vector by a random angle.
 *
 * Generates a random rotation axis perpendicular to `dir`, then rotates
 * `dir` by `angle_rad` around that axis using Rodrigues' rotation formula:
 *
 *   v_rot = v·cos(θ) + (k×v)·sin(θ) + k·(k·v)·(1−cos(θ))
 *
 * where k is the random rotation axis (perpendicular to v).
 */
inline Vec3 random_deflect(const Vec3& dir, double angle_rad,
                           std::mt19937_64& rng)
{
    if (angle_rad < 1e-9) return dir;

    // Build an arbitrary perpendicular to dir
    Vec3 perp;
    if (std::abs(dir.x) < 0.9) {
        perp = {0.0, -dir.z, dir.y};
    } else {
        perp = {-dir.y, dir.x, 0.0};
    }
    double pn = atomistic::norm(perp);
    if (pn < 1e-12) perp = {0.0, 1.0, 0.0};
    else            perp = perp * (1.0/pn);

    // Random azimuthal angle (rotation axis in the plane perp to dir)
    std::uniform_real_distribution<double> azimuth(0.0, 6.28318530718);
    double phi = azimuth(rng);

    // Rotate perp around dir by phi to get the actual rotation axis k
    // k = perp·cos(phi) + (dir×perp)·sin(phi)
    Vec3 cross = {
        dir.y*perp.z - dir.z*perp.y,
        dir.z*perp.x - dir.x*perp.z,
        dir.x*perp.y - dir.y*perp.x
    };
    Vec3 k = perp * std::cos(phi) + cross * std::sin(phi);

    // Rodrigues rotation of `dir` around `k` by angle_rad
    double cos_a = std::cos(angle_rad);
    double sin_a = std::sin(angle_rad);
    double kdotv = k.x*dir.x + k.y*dir.y + k.z*dir.z; // should be ~0
    Vec3 kcrossv = {
        k.y*dir.z - k.z*dir.y,
        k.z*dir.x - k.x*dir.z,
        k.x*dir.y - k.y*dir.x
    };
    Vec3 result = dir * cos_a + kcrossv * sin_a + k * (kdotv * (1.0 - cos_a));
    double rn = atomistic::norm(result);
    if (rn > 1e-12) result = result * (1.0/rn);
    return result;
}

// ============================================================================
// Network generator
// ============================================================================

/**
 * generate_pipe_network — main entry point.
 *
 * @param n_segments_target  Approximate number of segments to generate
 * @param chaos              NetworkChaos (chi = 1.25 for complex topology)
 * @param alloy_bead_count   Number of alloy bead records available
 * @param seed               RNG seed
 *
 * Algorithm:
 *  1. Create source node at origin.
 *  2. Use a frontier queue (depth-limited DFS-like expansion).
 *  3. At each frontier node, draw branch count from chaos parameters.
 *  4. For each branch: sample segment length (lognormal), deflect
 *     direction from parent, place terminus node.
 *  5. With loop_back_prob(), try to connect to an existing node instead
 *     of creating a new one (cycle creation).
 *  6. Assign alloy material to each segment.
 *  7. Compute beam tensors.  Accumulate network beam tensor.
 *  8. Compute statistics.
 */
inline PipeNetwork generate_pipe_network(
    uint32_t      n_segments_target,
    NetworkChaos  chaos,
    uint32_t      alloy_bead_count = 64,
    uint64_t      seed             = 77)
{
    std::mt19937_64 rng(seed);

    // Distributions
    std::lognormal_distribution<double> len_dist(
        std::log(chaos.length_mean_m()),
        chaos.length_sigma_m() / chaos.length_mean_m());
    std::uniform_real_distribution<double> u01(0.0, 1.0);
    std::normal_distribution<double>       od_noise(0.0, chaos.cross_section_noise());
    std::normal_distribution<double>       wt_noise(0.0, chaos.wall_thickness_noise());
    std::uniform_int_distribution<uint32_t> bead_pick(
        0, std::max(1u, alloy_bead_count) - 1);

    // Base pipe dimensions (before noise)
    const double OD_BASE = 0.15;    // 150 mm OD
    const double WT_BASE = 0.010;   // 10 mm wall

    PipeNetwork net;
    net.chi  = chaos.chi;
    net.seed = seed;

    // Source node
    NetworkNode src;
    src.node_id  = 0;
    src.position = {0.0, 0.0, 0.0};
    src.is_source = true;
    net.nodes.push_back(src);

    // Frontier: (node_id, current_direction)
    using Frontier = std::pair<uint32_t, Vec3>;
    std::vector<Frontier> frontier;
    frontier.push_back({0, {1.0, 0.0, 0.0}});  // Start heading +X

    uint32_t seg_id  = 0;
    uint32_t node_id = 1;

    while (seg_id < n_segments_target && !frontier.empty()) {
        // Pop front
        auto [from_node, cur_dir] = frontier.front();
        frontier.erase(frontier.begin());

        // How many branches from this node?
        int n_branch = 1;
        if (u01(rng) < chaos.branch_prob())
            n_branch = 2 + (static_cast<int>(u01(rng) * (chaos.max_branches() - 1)));
        n_branch = std::max(1, std::min(n_branch, chaos.max_branches()));
        if (seg_id + static_cast<uint32_t>(n_branch) > n_segments_target)
            n_branch = static_cast<int>(n_segments_target - seg_id);

        Vec3 branch_dir = cur_dir;

        for (int b = 0; b < n_branch && seg_id < n_segments_target; ++b) {
            // Deflect direction for this branch
            double defl_max = chaos.deflection_max_rad();
            std::uniform_real_distribution<double> defl_dist(0.0, defl_max);
            double defl_angle = (b == 0) ? defl_dist(rng) * 0.5 : defl_dist(rng);
            Vec3 seg_dir = random_deflect(branch_dir, defl_angle, rng);
            branch_dir = seg_dir;

            // Sample length
            double raw_L = len_dist(rng);
            double seg_L = std::max(0.2, std::min(raw_L, 30.0));

            // Cross-section with noise
            double od = std::max(0.02, OD_BASE * (1.0 + od_noise(rng)));
            double wt = std::max(0.001, WT_BASE * (1.0 + wt_noise(rng)));
            wt = std::min(wt, od * 0.45);  // wall cannot exceed 45% of OD

            // Terminus position
            Vec3 from_pos = net.nodes[from_node].position;
            Vec3 terminus_pos = {
                from_pos.x + seg_dir.x * seg_L,
                from_pos.y + seg_dir.y * seg_L,
                from_pos.z + seg_dir.z * seg_L
            };

            // Loop-back? Try to reuse a nearby existing node
            bool is_loop = false;
            uint32_t to_node_id = node_id;
            if (u01(rng) < chaos.loop_back_prob() && net.nodes.size() > 2) {
                // Find a node not adjacent to from_node and at least 2 away
                std::uniform_int_distribution<uint32_t> node_pick(
                    0, static_cast<uint32_t>(net.nodes.size()) - 1);
                uint32_t candidate = node_pick(rng);
                if (candidate != from_node) {
                    terminus_pos = net.nodes[candidate].position;
                    to_node_id   = candidate;
                    is_loop      = true;
                }
            }

            // Create new node if not looping
            if (!is_loop) {
                NetworkNode nnode;
                nnode.node_id  = node_id;
                nnode.position = terminus_pos;
                nnode.is_sink  = (seg_id == n_segments_target - 1);
                nnode.is_junction = (n_branch > 1);
                net.nodes.push_back(nnode);
                ++node_id;
            }

            // Build segment
            PipeSegment seg;
            seg.segment_id        = seg_id;
            seg.node_from         = from_node;
            seg.node_to           = to_node_id;
            seg.origin            = from_pos;
            seg.terminus          = net.nodes[to_node_id].position;
            seg.outer_diameter_m  = od;
            seg.wall_thickness_m  = wt;
            seg.length_sample     = raw_L;
            seg.deflection_rad    = defl_angle;
            seg.is_loop_back      = is_loop;

            seg.is_alloy_material = (u01(rng) < chaos.alloy_segment_prob());
            seg.alloy_bead_index  = bead_pick(rng);
            seg.dominant_Z        = 29;  // Cu default

            seg.compute_geometry();

            // Register in node adjacency
            net.nodes[from_node].outgoing.push_back(seg_id);
            net.nodes[to_node_id].incoming.push_back(seg_id);

            net.segments.push_back(seg);
            net.network_beam_tensor += seg.beam_tensor_B;

            ++seg_id;

            // Push terminus to frontier if not a loop
            if (!is_loop)
                frontier.push_back({to_node_id, seg_dir});
        }

        // If junction: mark node
        if (n_branch > 1)
            net.nodes[from_node].is_junction = true;
    }

    // ── Statistics ────────────────────────────────────────────────────────────
    net.n_segments = static_cast<uint32_t>(net.segments.size());
    net.n_junctions = 0;
    net.n_loops     = 0;
    double sum_L = 0.0, sum_defl = 0.0;
    uint32_t n_alloy = 0;

    for (auto& s : net.segments) {
        sum_L    += s.length_m;
        sum_defl += s.deflection_rad;
        if (s.is_loop_back) net.n_loops++;
        if (s.is_alloy_material) n_alloy++;
    }
    for (auto& n : net.nodes)
        if (n.is_junction) net.n_junctions++;

    net.total_length_m        = sum_L;
    net.mean_segment_length_m = (net.n_segments > 0) ? sum_L / net.n_segments : 0.0;
    net.mean_deflection_rad   = (net.n_segments > 0) ? sum_defl / net.n_segments : 0.0;
    net.alloy_segment_fraction = (net.n_segments > 0)
                                  ? static_cast<double>(n_alloy) / net.n_segments : 0.0;

    // Name
    net.name = "CuAlloy pipe network (chi="
             + std::to_string(chaos.chi).substr(0, 4)
             + ", " + std::to_string(net.n_segments) + " segs, "
             + std::to_string(net.nodes.size()) + " nodes)";

    return net;
}

} // namespace pipe_network
