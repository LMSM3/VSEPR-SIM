// ============================================================================
// layout_prerender3d.cpp — Glass Module: Fast Topology-Driven 3D Layout
// ============================================================================

#include "layout_prerender3d.hpp"
#include <cmath>
#include <algorithm>
#include <queue>
#include <numeric>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace vsepr {
namespace glass {

// -----------------------------------------------------------------------
// Vec3f free functions
// -----------------------------------------------------------------------
float dot3f(const Vec3f& a, const Vec3f& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3f cross3f(const Vec3f& a, const Vec3f& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

float norm3f(const Vec3f& v) {
    return std::sqrt(dot3f(v, v));
}

Vec3f normalize3f(const Vec3f& v) {
    float n = norm3f(v);
    if (n < 1e-8f) return {1.0f, 0.0f, 0.0f};
    return v / n;
}

// -----------------------------------------------------------------------
// xorshift32 — cheap deterministic RNG, returns [-1, 1]
// -----------------------------------------------------------------------
float TopologyPrerender3D::rand_unit(uint32_t& state) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    float u = static_cast<float>(state & 0x7FFFFFFFu) / static_cast<float>(0x7FFFFFFFu);
    return u * 2.0f - 1.0f;
}

Vec3f TopologyPrerender3D::orthogonal_hint(const Vec3f& dir) {
    Vec3f up = (std::fabs(dir.z) < 0.8f) ? Vec3f{0, 0, 1} : Vec3f{0, 1, 0};
    return normalize3f(cross3f(dir, up));
}

float TopologyPrerender3D::bond_length(const GlassMolecule& mol, uint32_t a, uint32_t b) const {
    float ra = mol.atoms[a].covalent_radius;
    float rb = mol.atoms[b].covalent_radius;
    float L = ra + rb;
    if (L < 0.5f) L = settings_.default_bond_length;
    return L;
}

// -----------------------------------------------------------------------
// Constructor
// -----------------------------------------------------------------------
TopologyPrerender3D::TopologyPrerender3D(LayoutSettings s)
    : settings_(std::move(s)) {}

// -----------------------------------------------------------------------
// Root selection
// -----------------------------------------------------------------------
uint32_t TopologyPrerender3D::choose_root(const GlassMolecule& mol) const {
    uint32_t best = 0;
    size_t best_degree = 0;
    uint16_t best_Z = 0;
    for (uint32_t i = 0; i < static_cast<uint32_t>(mol.atoms.size()); ++i) {
        size_t d = mol.atoms[i].bond_ids.size();
        uint16_t Z = mol.atoms[i].atomic_number;
        if (d > best_degree || (d == best_degree && Z > best_Z)) {
            best_degree = d;
            best_Z = Z;
            best = i;
        }
    }
    return best;
}

// -----------------------------------------------------------------------
// Ring pre-placement
// -----------------------------------------------------------------------
void TopologyPrerender3D::place_rings(
    const GlassMolecule& /* mol */,
    const std::vector<Ring>& rings,
    std::vector<Vec3f>& pos,
    std::vector<bool>& placed
) {
    Vec3f ring_origin{0.0f, 0.0f, 0.0f};
    Vec3f ring_right{1.0f, 0.0f, 0.0f};
    Vec3f ring_up{0.0f, 1.0f, 0.0f};

    for (const auto& ring : rings) {
        uint32_t n = ring.size();
        if (n < 3) continue;

        // Check if any atom in this ring is already placed (fused ring)
        int placed_count = 0;
        Vec3f anchor_center{0, 0, 0};
        for (uint32_t idx : ring.atom_indices) {
            if (placed[idx]) {
                anchor_center += pos[idx];
                ++placed_count;
            }
        }

        if (placed_count >= 2) {
            anchor_center = anchor_center / static_cast<float>(placed_count);
            ring_origin = anchor_center;
        }

        float R = settings_.ring_bond_length / (2.0f * std::sin(static_cast<float>(M_PI) / n));

        for (uint32_t k = 0; k < n; ++k) {
            uint32_t atom_idx = ring.atom_indices[k];
            if (placed[atom_idx]) continue;

            float angle = 2.0f * static_cast<float>(M_PI) * k / n;
            float cx = R * std::cos(angle);
            float cy = R * std::sin(angle);

            pos[atom_idx] = ring_origin + ring_right * cx + ring_up * cy;
            placed[atom_idx] = true;
        }

        // Offset origin for next disconnected ring
        ring_origin += ring_right * (R * 2.5f);
    }
}

// -----------------------------------------------------------------------
// Chain smoothing (degree-2 non-ring atoms only)
// -----------------------------------------------------------------------
void TopologyPrerender3D::smooth_chains(
    const GlassMolecule& mol,
    std::vector<Vec3f>& pos,
    const std::vector<bool>& is_ring_atom
) {
    std::vector<Vec3f> next = pos;

    for (size_t i = 0; i < mol.atoms.size(); ++i) {
        if (is_ring_atom[i]) continue;
        if (mol.atoms[i].bond_ids.size() != 2) continue;

        uint32_t n0 = mol.other(mol.atoms[i].bond_ids[0], static_cast<uint32_t>(i));
        uint32_t n1 = mol.other(mol.atoms[i].bond_ids[1], static_cast<uint32_t>(i));

        Vec3f midpoint = (pos[n0] + pos[n1]) * 0.5f;
        next[i] = pos[i] * 0.65f + midpoint * 0.35f;
    }

    pos.swap(next);
}

// -----------------------------------------------------------------------
// Long-chain sinusoidal arc injection
// -----------------------------------------------------------------------
void TopologyPrerender3D::apply_chain_arcs(
    const GlassMolecule& mol,
    std::vector<Vec3f>& pos,
    const std::vector<bool>& is_ring_atom
) {
    std::vector<bool> visited(mol.atoms.size(), false);

    for (uint32_t start = 0; start < static_cast<uint32_t>(mol.atoms.size()); ++start) {
        if (visited[start]) continue;
        if (is_ring_atom[start]) continue;
        if (mol.atoms[start].bond_ids.size() != 2) continue;

        // Trace the chain in both directions from start
        std::vector<uint32_t> chain;
        chain.push_back(start);
        visited[start] = true;

        // Trace in two directions (dir=0 forward, dir=1 backward)
        for (int dir = 0; dir < 2; ++dir) {
            uint32_t cur = start;
            uint32_t prev = mol.other(
                mol.atoms[start].bond_ids[dir == 0 ? 1 : 0], start);

            while (true) {
                uint32_t next_atom = 0;
                bool found_next = false;
                for (uint32_t bid : mol.atoms[cur].bond_ids) {
                    uint32_t nbr = mol.other(bid, cur);
                    if (nbr == prev) continue;
                    if (visited[nbr]) break;
                    if (is_ring_atom[nbr]) break;
                    if (mol.atoms[nbr].bond_ids.size() != 2) break;
                    next_atom = nbr;
                    found_next = true;
                    break;
                }
                if (!found_next) break;

                visited[next_atom] = true;
                if (dir == 0)
                    chain.push_back(next_atom);
                else
                    chain.insert(chain.begin(), next_atom);

                prev = cur;
                cur = next_atom;
            }
        }

        if (chain.size() < settings_.chain_arc_min_length) continue;

        Vec3f chord = pos[chain.back()] - pos[chain.front()];
        Vec3f arc_axis = orthogonal_hint(normalize3f(chord));

        float amplitude = settings_.chain_arc_amplitude;
        uint32_t len = static_cast<uint32_t>(chain.size());

        for (uint32_t k = 0; k < len; ++k) {
            float phase = static_cast<float>(k) / static_cast<float>(len - 1);
            float offset = amplitude * std::sin(phase * static_cast<float>(M_PI));
            pos[chain[k]] += arc_axis * offset;
        }
    }
}

// -----------------------------------------------------------------------
// build_layout — main entry point
// -----------------------------------------------------------------------
LayoutResult TopologyPrerender3D::build_layout(const GlassMolecule& mol) {
    LayoutResult out;
    out.atom_positions.resize(mol.atoms.size(), {0, 0, 0});

    if (mol.atoms.empty()) return out;
    if (mol.atoms.size() == 1) return out;

    // 1. Detect rings
    auto rings = detect_rings(mol, 8);

    // Mark ring atoms
    std::vector<bool> is_ring_atom(mol.atoms.size(), false);
    for (const auto& ring : rings) {
        for (uint32_t idx : ring.atom_indices) {
            is_ring_atom[idx] = true;
        }
    }

    // 2. Place rings first (planar polygon pinning)
    std::vector<bool> placed(mol.atoms.size(), false);
    place_rings(mol, rings, out.atom_positions, placed);

    // 3. BFS placement from placed atoms outward
    uint32_t root = choose_root(mol);
    if (!placed[root]) {
        placed[root] = true;
        out.atom_positions[root] = {0.0f, 0.0f, 0.0f};
    }

    std::vector<int> parent(mol.atoms.size(), -1);
    uint32_t rng_state = settings_.random_seed;

    std::queue<uint32_t> bfs;
    for (uint32_t i = 0; i < static_cast<uint32_t>(mol.atoms.size()); ++i) {
        if (placed[i]) bfs.push(i);
    }

    while (!bfs.empty()) {
        uint32_t cur = bfs.front();
        bfs.pop();

        for (uint32_t bid : mol.atoms[cur].bond_ids) {
            uint32_t nbr = mol.other(bid, cur);
            if (placed[nbr]) continue;

            placed[nbr] = true;
            parent[nbr] = static_cast<int>(cur);

            float L = bond_length(mol, cur, nbr);
            Vec3f forward{1.0f, 0.0f, 0.0f};
            if (parent[cur] >= 0) {
                forward = normalize3f(
                    out.atom_positions[cur] -
                    out.atom_positions[static_cast<uint32_t>(parent[cur])]
                );
            }
            Vec3f side = orthogonal_hint(forward);
            Vec3f up = normalize3f(cross3f(side, forward));

            // Count unplaced neighbours for branch spread
            int unplaced_count = 0;
            int my_rank = 0;
            for (uint32_t bid2 : mol.atoms[cur].bond_ids) {
                uint32_t n2 = mol.other(bid2, cur);
                if (!placed[n2] || n2 == nbr) {
                    if (n2 == nbr) my_rank = unplaced_count;
                    ++unplaced_count;
                }
            }

            float t = (unplaced_count <= 1)
                ? 0.0f
                : static_cast<float>(my_rank) / std::max(1, unplaced_count - 1);
            float angle = (-0.5f + t) * settings_.branch_spread * 1.8f;

            float jitter = settings_.torsion_jitter * rand_unit(rng_state);
            float curve  = settings_.chain_curve_strength * rand_unit(rng_state);

            Vec3f dir = normalize3f(
                forward * (0.85f + 0.1f * rand_unit(rng_state)) +
                side    * (angle + jitter) +
                up      * curve
            );

            out.atom_positions[nbr] = out.atom_positions[cur] + dir * L;
            bfs.push(nbr);
        }
    }

    // 4. Place any remaining disconnected atoms
    float offset_x = 0.0f;
    for (uint32_t i = 0; i < static_cast<uint32_t>(mol.atoms.size()); ++i) {
        if (!placed[i]) {
            placed[i] = true;
            out.atom_positions[i] = {offset_x, 5.0f, 0.0f};
            offset_x += 2.0f;
        }
    }

    // 5. Long-chain arc injection
    apply_chain_arcs(mol, out.atom_positions, is_ring_atom);

    // 6. Smoothing passes (chain-only)
    for (int pass = 0; pass < settings_.smoothing_passes; ++pass) {
        smooth_chains(mol, out.atom_positions, is_ring_atom);
    }

    return out;
}

} // namespace glass
} // namespace vsepr
