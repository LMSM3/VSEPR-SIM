#include "atomistic/classify/fingerprints.hpp"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <unordered_map>
#include <functional>

namespace atomistic {
namespace classify {

// ============================================================================
// Neighbor Graph Construction
// ============================================================================

NeighborGraph build_neighbor_graph(const State& s, double cutoff) {
    NeighborGraph g(static_cast<int>(s.N));
    
    double cutoff_sq = cutoff * cutoff;
    
    for (uint32_t i = 0; i < s.N; ++i) {
        for (uint32_t j = i + 1; j < s.N; ++j) {
            Vec3 dr = s.X[j] - s.X[i];
            
            // Apply MIC if PBC enabled
            if (s.box.enabled) {
                dr = s.box.delta(s.X[i], s.X[j]);
            }
            
            double r_sq = dot(dr, dr);
            
            if (r_sq < cutoff_sq) {
                double r = std::sqrt(r_sq);
                g.adj[i].push_back(j);
                g.adj[j].push_back(i);
                g.dist[i].push_back(r);
                g.dist[j].push_back(r);
                g.CN[i]++;
                g.CN[j]++;
            }
        }
    }
    
    return g;
}

// ============================================================================
// Weisfeiler-Lehman Hash
// ============================================================================

uint64_t weisfeiler_lehman_hash(const NeighborGraph& g, int iterations) {
    std::vector<uint64_t> labels(g.N);
    
    // Initialize labels with coordination numbers
    for (int i = 0; i < g.N; ++i) {
        labels[i] = static_cast<uint64_t>(g.CN[i]);
    }
    
    // Iterate label propagation
    for (int iter = 0; iter < iterations; ++iter) {
        std::vector<uint64_t> new_labels(g.N);
        
        for (int i = 0; i < g.N; ++i) {
            // Collect neighbor labels
            std::vector<uint64_t> neighbor_labels;
            neighbor_labels.push_back(labels[i]);  // Include self
            
            for (int j : g.adj[i]) {
                neighbor_labels.push_back(labels[j]);
            }
            
            // Sort for canonical ordering
            std::sort(neighbor_labels.begin(), neighbor_labels.end());
            
            // Hash combination
            uint64_t hash = 0;
            for (uint64_t label : neighbor_labels) {
                hash = hash * 31 + label;  // Simple polynomial hash
            }
            
            new_labels[i] = hash;
        }
        
        labels = new_labels;
    }
    
    // Final hash: sort all labels and combine
    std::sort(labels.begin(), labels.end());
    
    uint64_t final_hash = 0;
    for (uint64_t label : labels) {
        final_hash = final_hash * 31 + label;
    }
    
    return final_hash;
}

// ============================================================================
// Prototype Fingerprint
// ============================================================================

ProtoFingerprint::ProtoFingerprint()
    : topology_hash(0)
    , volume_per_atom(0.0)
    , lattice_ratios({1.0, 1.0, 1.0})
{}

double ProtoFingerprint::distance(const ProtoFingerprint& other) const {
    // Topology hash mismatch → large distance
    if (topology_hash != other.topology_hash) {
        return 1e6;
    }
    
    // Histogram distances (chi-squared)
    double d_CN = 0.0;
    size_t n = std::min(CN_histogram.size(), other.CN_histogram.size());
    for (size_t i = 0; i < n; ++i) {
        double diff = CN_histogram[i] - other.CN_histogram[i];
        double sum = CN_histogram[i] + other.CN_histogram[i] + 1e-10;
        d_CN += diff * diff / sum;
    }
    
    double d_RDF = 0.0;
    n = std::min(RDF_histogram.size(), other.RDF_histogram.size());
    for (size_t i = 0; i < n; ++i) {
        double diff = RDF_histogram[i] - other.RDF_histogram[i];
        d_RDF += diff * diff;
    }
    
    double d_vol = std::abs(volume_per_atom - other.volume_per_atom) / 
                   (volume_per_atom + other.volume_per_atom + 1e-10);
    
    double d_lattice = 0.0;
    for (int i = 0; i < 3; ++i) {
        double diff = lattice_ratios[i] - other.lattice_ratios[i];
        d_lattice += diff * diff;
    }
    
    return d_CN + 0.5 * d_RDF + d_vol + 0.3 * d_lattice;
}

ProtoFingerprint compute_proto_fingerprint(const State& s, const NeighborGraph& g) {
    ProtoFingerprint fp;
    
    // Topology hash
    fp.topology_hash = weisfeiler_lehman_hash(g, 3);
    
    // CN histogram (bins 0-12)
    fp.CN_histogram.resize(13, 0);
    for (int cn : g.CN) {
        if (cn < 13) {
            fp.CN_histogram[cn]++;
        }
    }
    // Normalize
    for (auto& val : fp.CN_histogram) {
        val = static_cast<double>(val) / s.N;
    }
    
    // RDF histogram (20 bins, 0-10 Å)
    fp.RDF_histogram.resize(20, 0.0);
    double bin_width = 10.0 / 20.0;
    
    int pair_count = 0;
    for (uint32_t i = 0; i < s.N; ++i) {
        for (size_t k = 0; k < g.adj[i].size(); ++k) {
            double r = g.dist[i][k];
            int bin = static_cast<int>(r / bin_width);
            if (bin < 20) {
                fp.RDF_histogram[bin] += 1.0;
                pair_count++;
            }
        }
    }
    
    // Normalize RDF
    if (pair_count > 0) {
        for (auto& val : fp.RDF_histogram) {
            val /= pair_count;
        }
    }
    
    // Volume per atom
    if (s.box.enabled) {
        fp.volume_per_atom = s.box.L.x * s.box.L.y * s.box.L.z / s.N;
    } else {
        fp.volume_per_atom = 0.0;  // Non-PBC systems don't have well-defined volume
    }

    // Lattice ratios (for crystals)
    if (s.box.enabled) {
        double a = s.box.L.x;
        double b = s.box.L.y;
        double c = s.box.L.z;
        double min_len = std::min({a, b, c});
        fp.lattice_ratios = {a / min_len, b / min_len, c / min_len};
    }
    
    return fp;
}

// ============================================================================
// Defect Fingerprint
// ============================================================================

DefectFingerprint::DefectFingerprint()
{}

double DefectFingerprint::distance(const DefectFingerprint& other,
                                   const std::array<double, 4>& weights) const {
    double d = 0.0;
    
    // Occupancy vector distance
    double d_o = 0.0;
    size_t n = std::min(occupancy.size(), other.occupancy.size());
    for (size_t i = 0; i < n; ++i) {
        double diff = occupancy[i] - other.occupancy[i];
        d_o += diff * diff;
    }
    d += weights[0] * std::sqrt(d_o);
    
    // Vacancy vector distance
    double d_v = 0.0;
    n = std::min(vacancy.size(), other.vacancy.size());
    for (size_t i = 0; i < n; ++i) {
        double diff = vacancy[i] - other.vacancy[i];
        d_v += diff * diff;
    }
    d += weights[1] * std::sqrt(d_v);
    
    // Substitution distance (discrete histogram diff)
    double d_m = 0.0;
    for (const auto& [Z, hist] : substitution) {
        if (other.substitution.count(Z)) {
            const auto& other_hist = other.substitution.at(Z);
            size_t n_hist = std::min(hist.size(), other_hist.size());
            for (size_t i = 0; i < n_hist; ++i) {
                d_m += std::abs(hist[i] - other_hist[i]);
            }
        } else {
            // Element present in one but not the other
            for (double val : hist) {
                d_m += std::abs(val);
            }
        }
    }
    d += weights[2] * d_m;
    
    // Distortion distance (CN + bond histograms)
    double d_d = 0.0;
    n = std::min(CN_deviation.size(), other.CN_deviation.size());
    for (size_t i = 0; i < n; ++i) {
        double diff = CN_deviation[i] - other.CN_deviation[i];
        d_d += diff * diff;
    }
    
    n = std::min(bond_deviation.size(), other.bond_deviation.size());
    for (size_t i = 0; i < n; ++i) {
        double diff = bond_deviation[i] - other.bond_deviation[i];
        d_d += diff * diff;
    }
    
    d += weights[3] * std::sqrt(d_d);
    
    return d;
}

DefectFingerprint compute_defect_fingerprint(const State& s,
                                             const NeighborGraph& g,
                                             const State& parent_ideal) {
    DefectFingerprint fp;
    
    // For now, compute simple global statistics
    // (Full site-class decomposition would require symmetry analysis)
    
    // Occupancy: fraction of sites occupied per element type
    std::map<uint32_t, int> type_counts;
    for (uint32_t t : s.type) {
        type_counts[t]++;
    }

    // Compare to ideal parent
    std::map<uint32_t, int> parent_type_counts;
    for (uint32_t t : parent_ideal.type) {
        parent_type_counts[t]++;
    }
    
    fp.occupancy.clear();
    fp.vacancy.clear();
    
    for (const auto& [Z, count] : parent_type_counts) {
        double ideal_count = static_cast<double>(count);
        double actual_count = static_cast<double>(type_counts[Z]);
        
        double occ = actual_count / ideal_count;
        double vac = 1.0 - occ;
        
        fp.occupancy.push_back(occ);
        fp.vacancy.push_back(vac);
        
        // Substitution: species histogram (simplified)
        std::vector<double> hist(118, 0.0);  // Z=1 to Z=118
        if (Z < 118) {
            hist[Z] = occ;
        }
        fp.substitution[Z] = hist;
    }
    
    // CN deviation histogram
    fp.CN_deviation.resize(13, 0.0);
    
    // Get ideal CN distribution
    NeighborGraph g_parent = build_neighbor_graph(parent_ideal, 3.5);
    std::vector<int> ideal_CN_hist(13, 0);
    for (int cn : g_parent.CN) {
        if (cn < 13) ideal_CN_hist[cn]++;
    }
    
    std::vector<int> actual_CN_hist(13, 0);
    for (int cn : g.CN) {
        if (cn < 13) actual_CN_hist[cn]++;
    }
    
    for (int i = 0; i < 13; ++i) {
        double ideal_frac = static_cast<double>(ideal_CN_hist[i]) / parent_ideal.N;
        double actual_frac = static_cast<double>(actual_CN_hist[i]) / s.N;
        fp.CN_deviation[i] = actual_frac - ideal_frac;
    }
    
    // Bond deviation histogram (mean bond length per element pair)
    fp.bond_deviation.clear();
    
    // Compute mean bond length from graph
    double mean_bond = 0.0;
    int bond_count = 0;
    for (uint32_t i = 0; i < s.N; ++i) {
        for (double r : g.dist[i]) {
            mean_bond += r;
            bond_count++;
        }
    }
    if (bond_count > 0) {
        mean_bond /= bond_count;
    }
    
    double parent_mean_bond = 0.0;
    int parent_bond_count = 0;
    for (uint32_t i = 0; i < parent_ideal.N; ++i) {
        for (double r : g_parent.dist[i]) {
            parent_mean_bond += r;
            parent_bond_count++;
        }
    }
    if (parent_bond_count > 0) {
        parent_mean_bond /= parent_bond_count;
    }
    
    fp.bond_deviation.push_back(mean_bond - parent_mean_bond);
    
    return fp;
}

// ============================================================================
// Canonicalization
// ============================================================================

State canonicalize(const State& s) {
    State s_canon = s;
    
    // Wrap coordinates to [0, 1) if PBC
    if (s.box.enabled) {
        for (uint32_t i = 0; i < s_canon.N; ++i) {
            s_canon.X[i].x = s_canon.X[i].x - std::floor(s_canon.X[i].x);
            s_canon.X[i].y = s_canon.X[i].y - std::floor(s_canon.X[i].y);
            s_canon.X[i].z = s_canon.X[i].z - std::floor(s_canon.X[i].z);
        }
    }
    
    // Sort atoms: first by type, then by position hash
    std::vector<size_t> indices(s_canon.N);
    std::iota(indices.begin(), indices.end(), size_t(0));

    std::sort(indices.begin(), indices.end(), [&](size_t a, size_t b) {
        if (s_canon.type[a] != s_canon.type[b]) {
            return s_canon.type[a] < s_canon.type[b];
        }
        
        // Tie-break by position hash
        uint64_t hash_a = static_cast<uint64_t>(s_canon.X[a].x * 1e6) * 31 +
                         static_cast<uint64_t>(s_canon.X[a].y * 1e6) * 31 +
                         static_cast<uint64_t>(s_canon.X[a].z * 1e6);
        uint64_t hash_b = static_cast<uint64_t>(s_canon.X[b].x * 1e6) * 31 +
                         static_cast<uint64_t>(s_canon.X[b].y * 1e6) * 31 +
                         static_cast<uint64_t>(s_canon.X[b].z * 1e6);
        return hash_a < hash_b;
    });
    
    // Reorder arrays
    State s_sorted;
    s_sorted.N = s_canon.N;
    s_sorted.box = s_canon.box;
    s_sorted.X.reserve(s_canon.N);
    s_sorted.V.reserve(s_canon.N);
    s_sorted.M.reserve(s_canon.N);
    s_sorted.Q.reserve(s_canon.N);
    s_sorted.type.reserve(s_canon.N);
    
    for (size_t idx : indices) {
        s_sorted.X.push_back(s_canon.X[idx]);
        s_sorted.V.push_back(s_canon.V[idx]);
        s_sorted.M.push_back(s_canon.M[idx]);
        s_sorted.Q.push_back(s_canon.Q[idx]);
        s_sorted.type.push_back(s_canon.type[idx]);
    }
    
    return s_sorted;
}

std::string reduced_formula(const State& s) {
    // Count element types
    std::map<uint32_t, int> counts;
    for (uint32_t t : s.type) {
        counts[t]++;
    }

    // Find GCD of all counts
    int gcd = 0;
    for (const auto& [Z, count] : counts) {
        gcd = (gcd == 0) ? count : std::gcd(gcd, count);
    }

    // Build reduced formula string
    std::string formula;

    // Sort by atomic number
    std::vector<std::pair<uint32_t, int>> sorted_counts(counts.begin(), counts.end());
    std::sort(sorted_counts.begin(), sorted_counts.end());
    
    // Element symbols (simplified table)
    const char* symbols[] = {
        "", "H", "He", "Li", "Be", "B", "C", "N", "O", "F", "Ne",
        "Na", "Mg", "Al", "Si", "P", "S", "Cl", "Ar", "K", "Ca",
        "Ti", "Fe", "Cu", "Zn", "Br", "Ag", "I", "Au"
    };
    
    for (const auto& [Z, count] : sorted_counts) {
        if (Z < 30) {
            formula += symbols[Z];
        } else {
            formula += "X" + std::to_string(Z);
        }
        
        int reduced_count = count / gcd;
        if (reduced_count > 1) {
            formula += std::to_string(reduced_count);
        }
    }
    
    return formula;
}

} // namespace classify
} // namespace atomistic
