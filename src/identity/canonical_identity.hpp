#pragma once
/**
 * canonical_identity.hpp
 * ----------------------
 * Canonical molecular identity and fingerprinting.
 *
 * Prevents database inflation by ensuring each distinct molecule
 * has exactly one canonical representation.
 *
 * Identity = formula + graph topology + charge + spin
 * Fingerprint = graph hash (topology) + geometry hash (rotation-invariant)
 *
 * "Same molecule" means: same formula + same bond graph = same entry.
 * Conformers (same graph, different geometry) are tracked as sub-entries.
 */

#include "sim/molecule.hpp"
#include "core/types.hpp"
#include <vector>
#include <string>
#include <map>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <functional>

namespace vsepr {
namespace identity {

// ============================================================================
// Canonical Formula String
// ============================================================================

/**
 * Generate canonical formula string from atom list.
 * Hill system: C first, H second, then alphabetical.
 * Example: {8:1, 1:2} -> "H2O", {6:2, 1:6} -> "C2H6"
 */
inline std::string canonical_formula(const std::vector<Atom>& atoms,
                                     const std::function<std::string(uint8_t)>& Z_to_symbol) {
    std::map<uint8_t, int> composition;
    for (const auto& a : atoms) {
        composition[a.Z]++;
    }

    // Hill system ordering
    std::vector<std::pair<std::string, int>> sorted;
    for (const auto& [Z, count] : composition) {
        sorted.push_back({Z_to_symbol(Z), count});
    }

    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) {
        // C first, H second, rest alphabetical
        auto rank = [](const std::string& s) -> int {
            if (s == "C") return 0;
            if (s == "H") return 1;
            return 2;
        };
        int ra = rank(a.first), rb = rank(b.first);
        if (ra != rb) return ra < rb;
        return a.first < b.first;
    });

    std::ostringstream oss;
    for (const auto& [sym, count] : sorted) {
        oss << sym;
        if (count > 1) oss << count;
    }
    return oss.str();
}

// ============================================================================
// Graph Canonical Form (Morgan Algorithm)
// ============================================================================

/**
 * Compute Morgan extended connectivity indices.
 * Produces a canonical atom ordering independent of input order.
 *
 * Algorithm:
 *   1. Initial invariant = atomic number
 *   2. Iterate: new_inv[i] = sum(inv[neighbors])
 *   3. Converge when partition count stabilizes
 *   4. Break ties by Z, then by neighbor invariants
 */
struct MorganResult {
    std::vector<uint32_t> canonical_order;  // Permutation: canonical[i] = original index
    std::vector<uint64_t> invariants;       // Final Morgan invariants per atom
};

inline MorganResult morgan_canonicalize(const std::vector<Atom>& atoms,
                                        const std::vector<Bond>& bonds) {
    const size_t N = atoms.size();
    MorganResult result;
    result.invariants.resize(N);
    result.canonical_order.resize(N);

    if (N == 0) return result;

    // Build adjacency list
    std::vector<std::vector<uint32_t>> adj(N);
    for (const auto& b : bonds) {
        if (b.i < N && b.j < N) {
            adj[b.i].push_back(b.j);
            adj[b.j].push_back(b.i);
        }
    }

    // Initial invariants = atomic number
    std::vector<uint64_t> inv(N);
    for (size_t i = 0; i < N; ++i) {
        inv[i] = atoms[i].Z;
    }

    // Iterate until partition stabilizes
    int prev_partitions = 0;
    for (int iter = 0; iter < 100; ++iter) {
        std::vector<uint64_t> new_inv(N);
        for (size_t i = 0; i < N; ++i) {
            uint64_t sum = inv[i] * 1000;  // Self contribution
            for (uint32_t j : adj[i]) {
                sum += inv[j];
            }
            new_inv[i] = sum;
        }

        // Count distinct partitions
        std::vector<uint64_t> sorted_inv = new_inv;
        std::sort(sorted_inv.begin(), sorted_inv.end());
        int partitions = static_cast<int>(
            std::unique(sorted_inv.begin(), sorted_inv.end()) - sorted_inv.begin());

        inv = new_inv;
        if (partitions == prev_partitions) break;
        prev_partitions = partitions;
    }

    result.invariants = inv;

    // Generate canonical ordering: sort by invariant, break ties by Z
    std::iota(result.canonical_order.begin(), result.canonical_order.end(), 0u);
    std::sort(result.canonical_order.begin(), result.canonical_order.end(),
              [&](uint32_t a, uint32_t b) {
                  if (inv[a] != inv[b]) return inv[a] < inv[b];
                  return atoms[a].Z < atoms[b].Z;
              });

    return result;
}

// ============================================================================
// Graph Hash (Topology Fingerprint)
// ============================================================================

/**
 * Compute a deterministic hash of the molecular graph.
 * Invariant to atom ordering. Includes Z and bond orders.
 *
 * Two molecules with the same graph_hash have identical topology.
 */
inline uint64_t graph_hash(const std::vector<Atom>& atoms,
                           const std::vector<Bond>& bonds) {
    auto morgan = morgan_canonicalize(atoms, bonds);
    const size_t N = atoms.size();

    // Build canonical edge list
    std::vector<uint32_t> inv_order(N);
    for (size_t i = 0; i < N; ++i) {
        inv_order[morgan.canonical_order[i]] = static_cast<uint32_t>(i);
    }

    // Hash: sequence of (Z values in canonical order) + (canonical edges)
    uint64_t h = 14695981039346656037ULL;  // FNV-1a offset basis
    auto fnv_mix = [&h](uint64_t val) {
        h ^= val;
        h *= 1099511628211ULL;  // FNV-1a prime
    };

    // Hash atoms in canonical order
    for (size_t i = 0; i < N; ++i) {
        uint32_t orig = morgan.canonical_order[i];
        fnv_mix(atoms[orig].Z);
    }

    // Hash bonds in canonical order
    struct CanonEdge {
        uint32_t ci, cj;
        uint8_t order;
        bool operator<(const CanonEdge& o) const {
            if (ci != o.ci) return ci < o.ci;
            if (cj != o.cj) return cj < o.cj;
            return order < o.order;
        }
    };

    std::vector<CanonEdge> cedges;
    for (const auto& b : bonds) {
        uint32_t ci = inv_order[b.i];
        uint32_t cj = inv_order[b.j];
        if (ci > cj) std::swap(ci, cj);
        cedges.push_back({ci, cj, b.order});
    }
    std::sort(cedges.begin(), cedges.end());

    for (const auto& e : cedges) {
        fnv_mix(e.ci);
        fnv_mix(e.cj);
        fnv_mix(e.order);
    }

    return h;
}

// ============================================================================
// Geometry Hash (Rotation/Translation Invariant)
// ============================================================================

/**
 * Compute a geometry fingerprint from pairwise distance matrix.
 * Invariant to rotation, translation, and atom ordering.
 * Tolerant to small noise (distances rounded to tolerance).
 *
 * Two molecules with same geometry_hash are geometrically equivalent
 * within the given tolerance.
 */
inline uint64_t geometry_hash(const std::vector<double>& coords,
                              size_t N,
                              double tolerance = 0.01) {
    // Collect all pairwise distances, sort them
    std::vector<uint32_t> rounded_dists;
    for (size_t i = 0; i < N; ++i) {
        for (size_t j = i + 1; j < N; ++j) {
            double dx = coords[3*i]   - coords[3*j];
            double dy = coords[3*i+1] - coords[3*j+1];
            double dz = coords[3*i+2] - coords[3*j+2];
            double r = std::sqrt(dx*dx + dy*dy + dz*dz);
            // Round to tolerance grid
            rounded_dists.push_back(static_cast<uint32_t>(r / tolerance + 0.5));
        }
    }
    std::sort(rounded_dists.begin(), rounded_dists.end());

    uint64_t h = 14695981039346656037ULL;
    for (uint32_t d : rounded_dists) {
        h ^= d;
        h *= 1099511628211ULL;
    }
    return h;
}

// ============================================================================
// Molecular Identity
// ============================================================================

struct MolecularIdentity {
    std::string formula;         // Hill system canonical formula
    uint64_t    topology_hash;   // Graph hash (bond topology)
    uint64_t    geometry_hash;   // Distance matrix hash (rotation-invariant)
    int         charge;          // Net formal charge
    int         multiplicity;    // Spin multiplicity (2S+1)

    // Two molecules are "the same entry" if formula + topology match
    bool same_molecule(const MolecularIdentity& other) const {
        return formula == other.formula &&
               topology_hash == other.topology_hash &&
               charge == other.charge;
    }

    // Two molecules are "the same conformer" if geometry also matches
    bool same_conformer(const MolecularIdentity& other) const {
        return same_molecule(other) &&
               geometry_hash == other.geometry_hash;
    }

    // String key for database indexing
    std::string db_key() const {
        std::ostringstream oss;
        oss << formula << "_q" << charge << "_t" << std::hex << topology_hash;
        return oss.str();
    }

    // Human-readable summary
    std::string summary() const {
        std::ostringstream oss;
        oss << formula;
        if (charge > 0) oss << "(" << charge << "+)";
        else if (charge < 0) oss << "(" << -charge << "-)";
        oss << " [topo:" << std::hex << (topology_hash & 0xFFFF)
            << " geom:" << (geometry_hash & 0xFFFF) << "]";
        return oss.str();
    }
};

/**
 * Compute full molecular identity from a Molecule.
 */
inline MolecularIdentity compute_identity(
    const Molecule& mol,
    const std::function<std::string(uint8_t)>& Z_to_symbol,
    int charge = 0,
    int multiplicity = 1
) {
    MolecularIdentity id;
    id.formula = canonical_formula(mol.atoms, Z_to_symbol);
    id.topology_hash = graph_hash(mol.atoms, mol.bonds);
    id.geometry_hash = geometry_hash(mol.coords, mol.num_atoms());
    id.charge = charge;
    id.multiplicity = multiplicity;
    return id;
}

} // namespace identity
} // namespace vsepr
