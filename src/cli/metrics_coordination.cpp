#include "cli/metrics_coordination.hpp"
#include <cmath>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <set>

namespace vsepr {
namespace cli {

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

// Get element symbol from atomic number (Z)
std::string Z_to_symbol_coord(uint32_t Z) {
    static const std::map<uint32_t, std::string> table = {
        {1, "H"}, {6, "C"}, {7, "N"}, {8, "O"}, {9, "F"},
        {11, "Na"}, {12, "Mg"}, {17, "Cl"}, {20, "Ca"},
        {22, "Ti"}, {58, "Ce"}, {57, "La"}, {59, "Pr"}, {60, "Nd"}
    };
    
    auto it = table.find(Z);
    if (it != table.end()) return it->second;
    return "X" + std::to_string(Z);
}

// Generate pair label (sorted alphabetically)
std::string make_pair_label_coord(const std::string& a, const std::string& b) {
    if (a <= b) {
        return a + "-" + b;
    } else {
        return b + "-" + a;
    }
}

// Compute distance with PBC minimum image
double distance_pbc_coord(
    const atomistic::Vec3& r1,
    const atomistic::Vec3& r2,
    const atomistic::BoxPBC& box
) {
    double dx = r1.x - r2.x;
    double dy = r1.y - r2.y;
    double dz = r1.z - r2.z;
    
    if (box.enabled) {
        dx -= box.L.x * std::round(dx * box.invL.x);
        dy -= box.L.y * std::round(dy * box.invL.y);
        dz -= box.L.z * std::round(dz * box.invL.z);
    }
    
    return std::sqrt(dx*dx + dy*dy + dz*dz);
}

// Extract cutoff from RDF first minimum (pair-specific)
double get_cutoff_from_rdf(const std::string& pair_label, const RDFResult& rdf) {
    // Try to find pair-specific RDF peaks
    // Since we only have global peaks, use a heuristic based on element types

    // For now, use first peak's r_min, but increase it slightly for safety
    if (!rdf.peaks.empty()) {
        // Use first peak's r_min, but not less than 2.5 Å
        double r_cut = rdf.peaks[0].r_min;

        // For Ce-F, F-F pairs, use a larger cutoff (Ce is larger)
        if (pair_label.find("Ce") != std::string::npos) {
            r_cut = std::max(r_cut, 3.0);  // At least 3 Å for Ce-containing pairs
        }

        // For other pairs, ensure at least 2.5 Å
        r_cut = std::max(r_cut, 2.5);

        return r_cut;
    }

    // Fallback: use 3.5 Å (reasonable for most ionic systems)
    return 3.5;
}

// ============================================================================
// COORDINATION COMPUTATION
// ============================================================================

CoordinationResult compute_coordination(
    const atomistic::State& state,
    const RDFResult& rdf
) {
    CoordinationResult result;
    
    // Get unique species in system
    std::set<std::string> species;
    for (uint32_t i = 0; i < state.N; ++i) {
        species.insert(Z_to_symbol_coord(state.type[i]));
    }
    
    // Count atoms per species
    for (const auto& sp : species) {
        result.n_atoms_by_species[sp] = 0;
    }
    for (uint32_t i = 0; i < state.N; ++i) {
        std::string sym = Z_to_symbol_coord(state.type[i]);
        result.n_atoms_by_species[sym]++;
    }
    
    // Determine all pair types and cutoffs
    for (const auto& sp_A : species) {
        for (const auto& sp_B : species) {
            std::string pair = make_pair_label_coord(sp_A, sp_B);
            
            // Get cutoff from RDF minimum
            result.cutoffs[pair] = get_cutoff_from_rdf(pair, rdf);
            
            // Initialize histogram (0 to 20 neighbors should be enough)
            result.histograms[pair].resize(21, 0);
        }
    }
    
    // For each atom, count neighbors within cutoff
    std::map<std::string, std::vector<int>> CN_per_atom;
    
    for (uint32_t i = 0; i < state.N; ++i) {
        std::string sym_i = Z_to_symbol_coord(state.type[i]);
        
        // Count neighbors of each type
        std::map<std::string, int> CN_by_type;
        for (const auto& sp : species) {
            CN_by_type[sp] = 0;
        }
        
        for (uint32_t j = 0; j < state.N; ++j) {
            if (i == j) continue;  // Don't count self
            
            std::string sym_j = Z_to_symbol_coord(state.type[j]);
            std::string pair = make_pair_label_coord(sym_i, sym_j);
            
            double r = distance_pbc_coord(state.X[i], state.X[j], state.box);
            
            if (r < result.cutoffs[pair]) {
                CN_by_type[sym_j]++;
            }
        }
        
        // Store CN for this atom and update histogram
        for (const auto& [sp_j, CN] : CN_by_type) {
            std::string pair = make_pair_label_coord(sym_i, sp_j);
            
            if (CN < static_cast<int>(result.histograms[pair].size())) {
                result.histograms[pair][CN]++;
            }
            
            CN_per_atom[pair].push_back(CN);
        }
    }
    
    // Compute statistics for each pair
    for (const auto& [pair, histogram] : result.histograms) {
        // Mean CN
        double sum = 0.0;
        int total_count = 0;
        for (size_t CN = 0; CN < histogram.size(); ++CN) {
            sum += CN * histogram[CN];
            total_count += histogram[CN];
        }
        
        result.mean_CN[pair] = (total_count > 0) ? sum / total_count : 0.0;
        
        // Std deviation
        double var_sum = 0.0;
        for (size_t CN = 0; CN < histogram.size(); ++CN) {
            double diff = CN - result.mean_CN[pair];
            var_sum += diff * diff * histogram[CN];
        }
        result.std_CN[pair] = (total_count > 0) ? std::sqrt(var_sum / total_count) : 0.0;
        
        // Modal CN (most common)
        int max_count = 0;
        int modal = 0;
        for (size_t CN = 0; CN < histogram.size(); ++CN) {
            if (histogram[CN] > max_count) {
                max_count = histogram[CN];
                modal = static_cast<int>(CN);
            }
        }
        result.modal_CN[pair] = modal;
        
        // Coordination crystallinity: fraction at modal CN
        result.crystallinity_fraction[pair] = (total_count > 0) 
            ? static_cast<double>(max_count) / total_count 
            : 0.0;
    }
    
    return result;
}

// ============================================================================
// OUTPUT
// ============================================================================

void write_coordination_csv(
    const std::string& path,
    const CoordinationResult& coord,
    int step,
    double temperature
) {
    std::ofstream file(path);
    if (!file) {
        std::cerr << "WARNING: Failed to write coordination to " << path << "\n";
        return;
    }
    
    // Header
    file << "# Coordination Number Analysis\n";
    file << "# Step: " << step << "\n";
    file << "# Temperature: " << temperature << " K\n";
    file << "#\n";
    file << "# Summary Statistics\n";
    file << "Pair,Mean_CN,Std_CN,Modal_CN,Crystallinity,Cutoff(Angstrom)\n";
    
    // Summary table
    for (const auto& [pair, mean] : coord.mean_CN) {
        file << pair << ","
             << std::fixed << std::setprecision(3) << mean << ","
             << std::setprecision(3) << coord.std_CN.at(pair) << ","
             << coord.modal_CN.at(pair) << ","
             << std::setprecision(3) << coord.crystallinity_fraction.at(pair) << ","
             << std::setprecision(3) << coord.cutoffs.at(pair) << "\n";
    }
    
    file << "#\n";
    
    // Histograms for each pair
    for (const auto& [pair, histogram] : coord.histograms) {
        file << "# Histogram for " << pair << "\n";
        file << "CN,Count\n";
        
        for (size_t CN = 0; CN < histogram.size(); ++CN) {
            if (histogram[CN] > 0) {  // Only print non-zero bins
                file << CN << "," << histogram[CN] << "\n";
            }
        }
        
        file << "#\n";
    }
}

} // namespace cli
} // namespace vsepr
