#pragma once
/**
 * graph_topology.hpp — Bead Interaction Graph G = (V, E)
 *
 * Implements the formal graph-theoretic structure for the bead system:
 *
 *   G = (V, E),  |V| = N
 *
 *   A_ij = { 1  if (i,j) ∈ E
 *           { 0  if (i,j) ∉ E
 *
 *   deg(i) = Σ_{j=1}^{N}  A_ij
 *
 * Edges are determined by the spatial cutoff r_cutoff:
 *   (i,j) ∈ E  ⟺  |r_i − r_j| < r_cutoff
 *
 * The graph provides:
 *   - Adjacency structure for pairwise interaction enumeration
 *   - Degree distribution → maps to coordination number C_B
 *   - Laplacian spectrum → structural connectivity analysis
 *   - Neighbour lists for efficient O(N) force evaluation
 *
 * This is the combinatorial substrate for:
 *   - §3.3 Pairwise mixed interactions (iterate over E)
 *   - §3.1 Energy decomposition (sum over edges)
 *   - Level 3 domain clustering (connected components / spatial proximity)
 *
 * Anti-black-box: the adjacency matrix and degree sequence are
 * explicitly constructible and inspectable at any step.
 *
 * Reference: Graph theory layer for VSEPR-SIM theory §3
 */

#include "atomistic/core/state.hpp"
#include "coarse_grain/core/bead.hpp"
#include "coarse_grain/core/bead_system.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <vector>

namespace coarse_grain {
namespace theory {

// ============================================================================
// Edge
// ============================================================================

/** Directed edge in the interaction graph. */
struct Edge {
    uint32_t i{};       ///< Source vertex (bead index)
    uint32_t j{};       ///< Target vertex (bead index)
    double   r_ij{};    ///< Separation distance (Å)
    double   weight{1.0}; ///< Edge weight (1.0 for unweighted, or switching fn)
};

// ============================================================================
// InteractionGraph  G = (V, E)
// ============================================================================

/**
 * InteractionGraph — the combinatorial backbone of the bead system.
 *
 * Vertices = beads (indexed 0..N-1).
 * Edges = pairs within r_cutoff, undirected but stored as (i < j) canonical.
 *
 * Storage: edge list (compact for sparse graphs).
 * Adjacency access: via adjacency() for O(N) lookup or via edge list traversal.
 */
struct InteractionGraph {
    uint32_t N{};                   ///< |V| = number of beads
    std::vector<Edge> edges;        ///< E = undirected edges (i < j)
    std::vector<uint32_t> degree;   ///< deg(i) = Σ_j A_ij

    /** Number of edges |E|. */
    uint32_t num_edges() const { return static_cast<uint32_t>(edges.size()); }

    /** Mean degree <k> = 2|E| / N. */
    double mean_degree() const {
        return (N > 0) ? 2.0 * edges.size() / N : 0.0;
    }

    /** Max degree. */
    uint32_t max_degree() const {
        if (degree.empty()) return 0;
        return *std::max_element(degree.begin(), degree.end());
    }

    /** Degree variance. */
    double degree_variance() const {
        if (N < 2) return 0.0;
        double mu = mean_degree();
        double var = 0.0;
        for (uint32_t d : degree) {
            double diff = static_cast<double>(d) - mu;
            var += diff * diff;
        }
        return var / (N - 1);
    }

    /**
     * Build adjacency list representation (for algorithms that need it).
     * Returns vector of vectors: adj[i] = list of neighbour indices j.
     */
    std::vector<std::vector<uint32_t>> adjacency_list() const {
        std::vector<std::vector<uint32_t>> adj(N);
        for (const auto& e : edges) {
            adj[e.i].push_back(e.j);
            adj[e.j].push_back(e.i);
        }
        return adj;
    }

    /**
     * Build dense adjacency matrix A_ij.
     * Only practical for small N (N < ~1000).
     * Returns row-major N×N vector.
     */
    std::vector<double> adjacency_matrix() const {
        std::vector<double> A(static_cast<size_t>(N) * N, 0.0);
        for (const auto& e : edges) {
            A[static_cast<size_t>(e.i) * N + e.j] = e.weight;
            A[static_cast<size_t>(e.j) * N + e.i] = e.weight;
        }
        return A;
    }

    /**
     * Build degree matrix D_ii = deg(i) (diagonal).
     * Returns N-length vector of diagonal entries.
     */
    std::vector<double> degree_matrix() const {
        std::vector<double> D(N);
        for (uint32_t k = 0; k < N; ++k)
            D[k] = static_cast<double>(degree[k]);
        return D;
    }

    /**
     * Build graph Laplacian L = D − A  (dense, row-major N×N).
     *
     * L_ij = { deg(i)    if i == j
     *        { −A_ij     if i ≠ j
     *
     * The Laplacian eigenvalues characterise connectivity:
     *   λ₁ = 0 always (connected component),
     *   λ₂ = algebraic connectivity (Fiedler value),
     *   Σλ = 2|E| (trace of L).
     */
    std::vector<double> laplacian_matrix() const {
        std::vector<double> L(static_cast<size_t>(N) * N, 0.0);
        for (uint32_t k = 0; k < N; ++k)
            L[static_cast<size_t>(k) * N + k] = static_cast<double>(degree[k]);
        for (const auto& e : edges) {
            L[static_cast<size_t>(e.i) * N + e.j] -= e.weight;
            L[static_cast<size_t>(e.j) * N + e.i] -= e.weight;
        }
        return L;
    }
};

// ============================================================================
// Graph Construction
// ============================================================================

/**
 * Build the interaction graph from bead positions and a cutoff radius.
 *
 * Edges: (i,j) with i < j and |r_i − r_j| < r_cutoff.
 * Weight: switching_function(r, r_cutoff, delta_sw) if use_switching = true,
 *         1.0 otherwise.
 *
 * Complexity: O(N²) brute-force. For large N, use cell lists.
 */
inline InteractionGraph build_graph(
    const BeadSystem& sys,
    double r_cutoff,
    double delta_sw = 1.0,
    bool use_switching = false)
{
    InteractionGraph G;
    G.N = static_cast<uint32_t>(sys.beads.size());
    G.degree.assign(G.N, 0);

    for (uint32_t i = 0; i < G.N; ++i) {
        const auto& ri = sys.beads[i].position;
        for (uint32_t j = i + 1; j < G.N; ++j) {
            const auto& rj = sys.beads[j].position;
            double dx = ri.x - rj.x;
            double dy = ri.y - rj.y;
            double dz = ri.z - rj.z;
            double r2 = dx * dx + dy * dy + dz * dz;
            double rc2 = r_cutoff * r_cutoff;
            if (r2 < rc2) {
                double r = std::sqrt(r2);
                Edge e;
                e.i = i;
                e.j = j;
                e.r_ij = r;
                if (use_switching) {
                    // Smooth switching weight
                    if (r <= r_cutoff - delta_sw) {
                        e.weight = 1.0;
                    } else {
                        constexpr double pi = 3.14159265358979323846;
                        double x = (r - r_cutoff + delta_sw) / delta_sw;
                        e.weight = 0.5 * (1.0 + std::cos(pi * x));
                    }
                } else {
                    e.weight = 1.0;
                }
                G.edges.push_back(e);
                G.degree[i]++;
                G.degree[j]++;
            }
        }
    }

    return G;
}

// ============================================================================
// Graph Diagnostics
// ============================================================================

/**
 * GraphDiagnostics — summary statistics for the interaction graph.
 */
struct GraphDiagnostics {
    uint32_t N{};                   ///< |V|
    uint32_t num_edges{};           ///< |E|
    double   mean_degree{};         ///< <k>
    uint32_t max_degree{};          ///< max deg(i)
    double   degree_variance{};     ///< Var(deg)
    double   mean_separation{};     ///< <r_ij> over E (Å)
    double   min_separation{};      ///< min r_ij (Å)
    double   max_separation{};      ///< max r_ij (Å)
    double   density_edges_per_v{}; ///< |E| / |V|
};

inline GraphDiagnostics diagnose_graph(const InteractionGraph& G) {
    GraphDiagnostics d;
    d.N = G.N;
    d.num_edges = G.num_edges();
    d.mean_degree = G.mean_degree();
    d.max_degree = G.max_degree();
    d.degree_variance = G.degree_variance();
    d.density_edges_per_v = (G.N > 0)
        ? static_cast<double>(G.num_edges()) / G.N : 0.0;

    d.min_separation = 1e30;
    d.max_separation = 0.0;
    double sum_r = 0.0;
    for (const auto& e : G.edges) {
        sum_r += e.r_ij;
        if (e.r_ij < d.min_separation) d.min_separation = e.r_ij;
        if (e.r_ij > d.max_separation) d.max_separation = e.r_ij;
    }
    d.mean_separation = (d.num_edges > 0) ? sum_r / d.num_edges : 0.0;
    if (d.num_edges == 0) { d.min_separation = 0.0; d.max_separation = 0.0; }

    return d;
}

} // namespace theory
} // namespace coarse_grain
