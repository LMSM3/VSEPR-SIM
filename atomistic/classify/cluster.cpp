#include "atomistic/classify/cluster.hpp"
#include <algorithm>
#include <numeric>
#include <unordered_map>
#include <iostream>

namespace atomistic {
namespace classify {

// ============================================================================
// Core Clustering Algorithm (Deterministic Threshold Assignment)
// ============================================================================

ClusterResult cluster_by_proto(const std::vector<State>& structures, double epsilon) {
    ClusterResult result;
    result.cluster_ids.resize(structures.size(), 0);
    
    // Compute all fingerprints
    std::vector<ProtoFingerprint> fps;
    std::vector<NeighborGraph> graphs;
    
    fps.reserve(structures.size());
    graphs.reserve(structures.size());
    
    for (const auto& s : structures) {
        State s_canon = canonicalize(s);
        NeighborGraph g = build_neighbor_graph(s_canon, 3.5);
        ProtoFingerprint fp = compute_proto_fingerprint(s_canon, g);
        
        fps.push_back(fp);
        graphs.push_back(g);
    }
    
    // Create processing order (sorted by topology hash for determinism)
    std::vector<size_t> order(structures.size());
    std::iota(order.begin(), order.end(), 0);
    
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        return fps[a].topology_hash < fps[b].topology_hash;
    });
    
    // Clustering: assign to first cluster within epsilon or create new
    for (size_t idx : order) {
        bool assigned = false;
        
        // Try existing clusters
        for (const auto& [cluster_id, centroid] : result.proto_centroids) {
            double dist = fps[idx].distance(centroid);
            
            if (dist < epsilon) {
                result.cluster_ids[idx] = cluster_id;
                result.cluster_sizes[cluster_id]++;
                
                // Update centroid (running average)
                // (Simplified: just use first member as centroid)
                assigned = true;
                break;
            }
        }
        
        if (!assigned) {
            // Create new cluster
            uint64_t new_id = fps[idx].topology_hash;
            result.cluster_ids[idx] = new_id;
            result.cluster_sizes[new_id] = 1;
            result.proto_centroids[new_id] = fps[idx];
        }
    }
    
    return result;
}

ClusterResult cluster_by_defect(const std::vector<State>& structures,
                                const std::vector<uint64_t>& parent_ids,
                                const std::map<uint64_t, State>& parent_ideals,
                                double epsilon,
                                const std::array<double, 4>& weights) {
    ClusterResult result;
    result.cluster_ids.resize(structures.size(), 0);
    
    // Group structures by parent ID
    std::map<uint64_t, std::vector<size_t>> parent_groups;
    for (size_t i = 0; i < structures.size(); ++i) {
        parent_groups[parent_ids[i]].push_back(i);
    }
    
    // Cluster within each parent
    for (const auto& [parent_id, indices] : parent_groups) {
        // Find parent ideal structure
        if (!parent_ideals.count(parent_id)) {
            std::cerr << "Warning: No ideal structure for parent " << parent_id << "\n";
            continue;
        }
        
        const State& parent = parent_ideals.at(parent_id);
        
        // Compute defect fingerprints
        std::vector<DefectFingerprint> fps;
        fps.reserve(indices.size());
        
        for (size_t idx : indices) {
            State s_canon = canonicalize(structures[idx]);
            NeighborGraph g = build_neighbor_graph(s_canon, 3.5);
            DefectFingerprint fp = compute_defect_fingerprint(s_canon, g, parent);
            fps.push_back(fp);
        }
        
        // Clustering within this parent
        std::map<uint64_t, DefectFingerprint> local_centroids;
        
        for (size_t i = 0; i < indices.size(); ++i) {
            size_t global_idx = indices[i];
            bool assigned = false;
            
            // Try existing clusters
            for (const auto& [cluster_id, centroid] : local_centroids) {
                double dist = fps[i].distance(centroid, weights);
                
                if (dist < epsilon) {
                    result.cluster_ids[global_idx] = cluster_id;
                    result.cluster_sizes[cluster_id]++;
                    assigned = true;
                    break;
                }
            }
            
            if (!assigned) {
                // Create new cluster (hash based on parent + local index)
                uint64_t new_id = parent_id * 1000 + local_centroids.size();
                result.cluster_ids[global_idx] = new_id;
                result.cluster_sizes[new_id] = 1;
                local_centroids[new_id] = fps[i];
                result.defect_centroids[new_id] = fps[i];
            }
        }
    }
    
    return result;
}

// ============================================================================
// Module A: Polymorph Classification
// ============================================================================

std::map<std::string, PolymorphResult>
classify_polymorphs(const std::vector<State>& structures,
                   const std::vector<double>& energies,
                   double epsilon) {
    std::map<std::string, PolymorphResult> results;
    
    // Group by formula
    std::map<std::string, std::vector<size_t>> formula_groups;
    for (size_t i = 0; i < structures.size(); ++i) {
        std::string formula = reduced_formula(structures[i]);
        formula_groups[formula].push_back(i);
    }
    
    // Cluster each formula group by prototype
    for (const auto& [formula, indices] : formula_groups) {
        std::vector<State> group_structs;
        group_structs.reserve(indices.size());
        
        for (size_t idx : indices) {
            group_structs.push_back(structures[idx]);
        }
        
        ClusterResult cluster_result = cluster_by_proto(group_structs, epsilon);
        
        // Build PolymorphResult
        PolymorphResult pr;
        pr.formula = formula;
        
        // Map local indices to global
        for (size_t local_idx = 0; local_idx < group_structs.size(); ++local_idx) {
            size_t global_idx = indices[local_idx];
            uint64_t cluster_id = cluster_result.cluster_ids[local_idx];
            
            pr.phase_ids.push_back(cluster_id);
            pr.phase_sizes[cluster_id]++;
            
            // Accumulate energies
            if (!energies.empty() && global_idx < energies.size()) {
                pr.mean_energies[cluster_id] += energies[global_idx];
            }
        }
        
        // Compute mean energies
        for (auto& [id, energy] : pr.mean_energies) {
            energy /= pr.phase_sizes[id];
        }
        
        results[formula] = pr;
    }
    
    return results;
}

// ============================================================================
// Module B: Isomorph Classification
// ============================================================================

std::vector<IsomorphResult>
classify_isomorphs(const std::vector<State>& structures,
                  double proto_epsilon,
                  double /*deco_epsilon*/) {
    // Phase 1: Assign prototypes
    ClusterResult proto_result = cluster_by_proto(structures, proto_epsilon);
    
    // Phase 2: Within each prototype, cluster by chemistry
    std::map<uint64_t, std::vector<size_t>> proto_groups;
    for (size_t i = 0; i < structures.size(); ++i) {
        proto_groups[proto_result.cluster_ids[i]].push_back(i);
    }
    
    std::vector<IsomorphResult> results;
    
    for (const auto& [proto_id, indices] : proto_groups) {
        IsomorphResult ir;
        ir.prototype_id = proto_id;
        
        // Group by formula within this prototype
        std::map<std::string, std::vector<size_t>> formula_groups;
        for (size_t idx : indices) {
            std::string formula = reduced_formula(structures[idx]);
            formula_groups[formula].push_back(idx);
        }
        
        // Each unique formula is a variant
        uint64_t variant_counter = 0;
        for (const auto& [formula, variant_indices] : formula_groups) {
            uint64_t variant_id = proto_id * 1000 + variant_counter++;
            
            ir.variant_ids.push_back(variant_id);
            ir.variant_sizes[variant_id] = variant_indices.size();
            ir.variant_formulas[variant_id] = formula;
        }
        
        results.push_back(ir);
    }
    
    return results;
}

// ============================================================================
// Module C: Defect Microstate Classification
// ============================================================================

std::vector<DefectResult>
classify_defects(const std::vector<State>& structures,
                const std::vector<double>& energies,
                double proto_epsilon,
                double defect_epsilon,
                const std::array<double, 4>& weights) {
    // Phase 1: Assign parent prototypes
    ClusterResult proto_result = cluster_by_proto(structures, proto_epsilon);
    
    // Build ideal parent structures (use first member of each cluster)
    std::map<uint64_t, State> parent_ideals;
    for (size_t i = 0; i < structures.size(); ++i) {
        uint64_t proto_id = proto_result.cluster_ids[i];
        if (!parent_ideals.count(proto_id)) {
            parent_ideals[proto_id] = structures[i];  // Use first as ideal
        }
    }
    
    // Phase 2: Cluster by defect fingerprints within each parent
    ClusterResult defect_result = cluster_by_defect(structures,
                                                     proto_result.cluster_ids,
                                                     parent_ideals,
                                                     defect_epsilon,
                                                     weights);
    
    // Build DefectResults
    std::map<uint64_t, std::vector<size_t>> parent_groups;
    for (size_t i = 0; i < structures.size(); ++i) {
        parent_groups[proto_result.cluster_ids[i]].push_back(i);
    }
    
    std::vector<DefectResult> results;
    
    for (const auto& [parent_id, indices] : parent_groups) {
        DefectResult dr;
        dr.parent_id = parent_id;
        dr.parent_formula = reduced_formula(parent_ideals[parent_id]);
        
        // Collect defect class stats
        for (size_t idx : indices) {
            uint64_t defect_class = defect_result.cluster_ids[idx];
            
            dr.defect_class_ids.push_back(defect_class);
            dr.class_sizes[defect_class]++;
            
            // Accumulate energies
            if (!energies.empty() && idx < energies.size()) {
                dr.mean_energies[defect_class] += energies[idx];
            }
            
            // Accumulate vacancy info (from defect fingerprint)
            if (defect_result.defect_centroids.count(defect_class)) {
                const auto& fp = defect_result.defect_centroids[defect_class];
                dr.mean_vacancies[defect_class] = fp.vacancy;
            }
        }
        
        // Compute mean energies
        for (auto& [id, energy] : dr.mean_energies) {
            energy /= dr.class_sizes[id];
        }
        
        results.push_back(dr);
    }
    
    return results;
}

} // namespace classify
} // namespace atomistic
