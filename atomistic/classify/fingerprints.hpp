#pragma once
/**
 * fingerprints.hpp
 * ----------------
 * Fingerprint construction for structure classification.
 * 
 * Two types:
 *   1. ProtoFingerprint - topology/geometry (chemistry-agnostic)
 *   2. DefectFingerprint - occupancy/disorder (chemistry-aware)
 * 
 * All components computed deterministically from canonicalized coordinates.
 */

#include "atomistic/core/state.hpp"
#include "atomistic/crystal/lattice.hpp"
#include <vector>
#include <map>
#include <cstdint>
#include <array>

namespace atomistic {
namespace classify {

// ============================================================================
// Neighbor Graph (fixed cutoff, deterministic ordering)
// ============================================================================

struct NeighborGraph {
    int N;                                    // Number of atoms
    std::vector<std::vector<int>> adj;        // Adjacency lists
    std::vector<std::vector<double>> dist;    // Bond distances
    std::vector<int> CN;                      // Coordination numbers
    
    NeighborGraph(int n) : N(n), adj(n), dist(n), CN(n, 0) {}
};

// Build neighbor graph from State with fixed cutoff
NeighborGraph build_neighbor_graph(const State& s, double cutoff = 3.5);

// ============================================================================
// Prototype Fingerprint (topology + geometry, chemistry-agnostic)
// ============================================================================

struct ProtoFingerprint {
    uint64_t topology_hash;              // Weisfeiler-Lehman graph hash
    std::vector<double> CN_histogram;    // Coordination number distribution
    std::vector<double> RDF_histogram;   // Radial distribution function
    double volume_per_atom;              // Normalized volume
    std::array<double, 3> lattice_ratios; // a:b:c ratios (for crystals)
    
    ProtoFingerprint();
    
    // Compute distance between two proto fingerprints
    double distance(const ProtoFingerprint& other) const;
};

// Compute prototype fingerprint from State
ProtoFingerprint compute_proto_fingerprint(const State& s, 
                                           const NeighborGraph& g);

// ============================================================================
// Defect Fingerprint (occupancy + disorder, chemistry-aware)
// ============================================================================

struct DefectFingerprint {
    std::vector<double> occupancy;           // o: Site-class occupancy fractions
    std::vector<double> vacancy;             // v: Vacancy fractions per site class
    std::map<int, std::vector<double>> substitution;  // m: Z -> histogram per site
    std::vector<double> CN_deviation;        // d: CN histogram deviations
    std::vector<double> bond_deviation;      // d: Bond length histogram deviations
    
    DefectFingerprint();
    
    // Compute distance with custom weights
    double distance(const DefectFingerprint& other,
                   const std::array<double, 4>& weights = {1.0, 1.5, 2.0, 0.5}) const;
};

// Compute defect fingerprint from State (within a parent prototype)
DefectFingerprint compute_defect_fingerprint(const State& s,
                                             const NeighborGraph& g,
                                             const State& parent_ideal);

// ============================================================================
// Weisfeiler-Lehman Graph Hash (canonical topology signature)
// ============================================================================

// Compute WL hash for graph isomorphism (deterministic)
uint64_t weisfeiler_lehman_hash(const NeighborGraph& g, int iterations = 3);

// ============================================================================
// Utility: Canonicalization
// ============================================================================

// Canonicalize State: wrap coords to [0,1), sort atoms deterministically
State canonicalize(const State& s);

// Extract reduced formula (e.g., MgO from Mg8O8)
std::string reduced_formula(const State& s);

} // namespace classify
} // namespace atomistic
