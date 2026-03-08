#pragma once
/**
 * cluster.hpp
 * -----------
 * Deterministic threshold-based clustering for structure classification.
 * 
 * - Processes structures in canonical order (by fingerprint hash)
 * - Assigns to first cluster within epsilon threshold
 * - Cluster IDs are deterministic hashes (not sequential integers)
 */

#include "atomistic/classify/fingerprints.hpp"
#include <vector>
#include <map>
#include <cstdint>

namespace atomistic {
namespace classify {

// ============================================================================
// Cluster Result
// ============================================================================

struct ClusterResult {
    std::vector<uint64_t> cluster_ids;       // Cluster ID per structure
    std::map<uint64_t, int> cluster_sizes;   // ID -> size
    std::map<uint64_t, ProtoFingerprint> proto_centroids;   // For proto clustering
    std::map<uint64_t, DefectFingerprint> defect_centroids; // For defect clustering
    
    int num_clusters() const { return cluster_sizes.size(); }
};

// ============================================================================
// Clustering Functions
// ============================================================================

// Cluster by prototype fingerprints (topology/geometry)
ClusterResult cluster_by_proto(const std::vector<State>& structures,
                               double epsilon = 0.05);

// Cluster by defect fingerprints (occupancy/disorder)
// Requires parent_ids to cluster within each prototype
ClusterResult cluster_by_defect(const std::vector<State>& structures,
                                const std::vector<uint64_t>& parent_ids,
                                const std::map<uint64_t, State>& parent_ideals,
                                double epsilon = 0.05,
                                const std::array<double, 4>& weights = {1.0, 1.5, 2.0, 0.5});

// ============================================================================
// Classification Modules
// ============================================================================

// Module A: Polymorph Detection (same chemistry → structure phases)
struct PolymorphResult {
    std::string formula;
    std::vector<uint64_t> phase_ids;       // α, β, γ, ... (ranked by energy)
    std::map<uint64_t, int> phase_sizes;
    std::map<uint64_t, double> mean_energies;  // If available
};

std::map<std::string, PolymorphResult>
classify_polymorphs(const std::vector<State>& structures,
                   const std::vector<double>& energies = {},
                   double epsilon = 0.05);

// Module B: Isomorph Detection (same structure → chemistry variants)
struct IsomorphResult {
    uint64_t prototype_id;
    std::vector<uint64_t> variant_ids;
    std::map<uint64_t, int> variant_sizes;
    std::map<uint64_t, std::string> variant_formulas;
};

std::vector<IsomorphResult>
classify_isomorphs(const std::vector<State>& structures,
                  double proto_epsilon = 0.05,
                  double deco_epsilon = 0.10);

// Module C: Defect Microstate Detection (same parent → occupancy/disorder)
struct DefectResult {
    uint64_t parent_id;
    std::string parent_formula;
    std::vector<uint64_t> defect_class_ids;   // ω₁, ω₂, ω₃, ...
    std::map<uint64_t, int> class_sizes;
    std::map<uint64_t, double> mean_energies;
    std::map<uint64_t, std::vector<double>> mean_vacancies;
};

std::vector<DefectResult>
classify_defects(const std::vector<State>& structures,
                const std::vector<double>& energies = {},
                double proto_epsilon = 0.05,
                double defect_epsilon = 0.05,
                const std::array<double, 4>& weights = {1.0, 1.5, 2.0, 0.5});

} // namespace classify
} // namespace atomistic
