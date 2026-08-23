#include "cli/metrics_rdf.hpp"
#include <cmath>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <map>
#include <iostream>

namespace vsepr {
namespace cli {

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

// Get element symbol from atomic number (Z)
std::string Z_to_symbol(uint32_t Z) {
    static const std::map<uint32_t, std::string> table = {
        {1, "H"}, {6, "C"}, {7, "N"}, {8, "O"}, {9, "F"},
        {11, "Na"}, {12, "Mg"}, {17, "Cl"}, {20, "Ca"},
        {22, "Ti"}, {58, "Ce"}, {57, "La"}, {59, "Pr"}, {60, "Nd"}
    };
    
    auto it = table.find(Z);
    if (it != table.end()) return it->second;
    return "X" + std::to_string(Z);  // Unknown element
}

// Generate pair label (sorted alphabetically for consistency)
std::string make_pair_label(const std::string& a, const std::string& b) {
    if (a <= b) {
        return a + "-" + b;
    } else {
        return b + "-" + a;
    }
}

// Compute distance with PBC minimum image convention
double distance_pbc(
    const atomistic::Vec3& r1,
    const atomistic::Vec3& r2,
    const atomistic::BoxPBC& box
) {
    double dx = r1.x - r2.x;
    double dy = r1.y - r2.y;
    double dz = r1.z - r2.z;
    
    if (box.enabled) {
        // Apply minimum image convention
        dx -= box.L.x * std::round(dx * box.invL.x);
        dy -= box.L.y * std::round(dy * box.invL.y);
        dz -= box.L.z * std::round(dz * box.invL.z);
    }
    
    return std::sqrt(dx*dx + dy*dy + dz*dz);
}

// ============================================================================
// RDF COMPUTATION
// ============================================================================

RDFResult compute_rdf(const atomistic::State& state, const RDFParams& params) {
    RDFResult result;
    
    // Initialize bins
    result.rmax = params.rmax;
    result.bin_width = params.bin_width;
    result.n_bins = static_cast<int>(params.rmax / params.bin_width);
    result.n_atoms = state.N;
    result.n_pairs = 0;
    
    // Allocate histograms
    result.r.resize(result.n_bins);
    result.g_total.resize(result.n_bins, 0.0);
    
    // Set bin centers
    for (int i = 0; i < result.n_bins; ++i) {
        result.r[i] = (i + 0.5) * params.bin_width;
    }
    
    // Initialize pair-specific histograms
    std::map<std::string, std::vector<int>> pair_counts;
    std::map<std::string, int> pair_total_count;  // For normalization
    
    if (params.compute_pairs) {
        // Pre-allocate for all possible pairs
        for (uint32_t i = 0; i < state.N; ++i) {
            std::string sym_i = Z_to_symbol(state.type[i]);
            for (uint32_t j = i; j < state.N; ++j) {
                std::string sym_j = Z_to_symbol(state.type[j]);
                std::string label = make_pair_label(sym_i, sym_j);
                
                if (pair_counts.find(label) == pair_counts.end()) {
                    pair_counts[label].resize(result.n_bins, 0);
                    pair_total_count[label] = 0;
                }
            }
        }
    }
    
    // Histogram all pairwise distances
    std::vector<int> total_counts(result.n_bins, 0);
    
    for (uint32_t i = 0; i < state.N; ++i) {
        for (uint32_t j = i + 1; j < state.N; ++j) {
            double r = distance_pbc(state.X[i], state.X[j], state.box);
            
            if (r < params.rmax) {
                int bin = static_cast<int>(r / params.bin_width);
                if (bin >= 0 && bin < result.n_bins) {
                    total_counts[bin]++;
                    result.n_pairs++;
                    
                    // Pair-specific histogram
                    if (params.compute_pairs) {
                        std::string sym_i = Z_to_symbol(state.type[i]);
                        std::string sym_j = Z_to_symbol(state.type[j]);
                        std::string label = make_pair_label(sym_i, sym_j);
                        
                        pair_counts[label][bin]++;
                        pair_total_count[label]++;
                    }
                }
            }
        }
    }
    
    // Normalize to get g(r)
    // g(r) = (histogram / bin_volume) / (ideal_gas_density)
    //
    // Where:
    // - bin_volume = 4π r² Δr
    // - ideal_gas_density = N_pairs / V_sphere
    //
    // For PBC: V_system = Lx * Ly * Lz
    // For vacuum: Use actual particle volume (approximate)
    
    double V_system;
    if (state.box.enabled) {
        V_system = state.box.L.x * state.box.L.y * state.box.L.z;
    } else {
        // Approximate as bounding sphere
        V_system = (4.0/3.0) * M_PI * std::pow(params.rmax, 3);
    }
    
    double rho = static_cast<double>(state.N) / V_system;  // Number density
    
    for (int i = 0; i < result.n_bins; ++i) {
        double r_bin = result.r[i];
        double bin_volume = 4.0 * M_PI * r_bin * r_bin * params.bin_width;
        
        // Ideal number of pairs in this shell
        double n_ideal = rho * bin_volume * state.N;
        
        if (n_ideal > 0) {
            result.g_total[i] = static_cast<double>(total_counts[i]) / n_ideal;
        }
    }
    
    // Normalize pair-specific RDFs
    if (params.compute_pairs) {
        for (const auto& [label, counts] : pair_counts) {
            result.g_pair[label].resize(result.n_bins, 0.0);
            
            for (int i = 0; i < result.n_bins; ++i) {
                double r_bin = result.r[i];
                double bin_volume = 4.0 * M_PI * r_bin * r_bin * params.bin_width;
                double n_ideal = rho * bin_volume * state.N;
                
                if (n_ideal > 0) {
                    result.g_pair[label][i] = static_cast<double>(counts[i]) / n_ideal;
                }
            }
        }
    }
    
    // Find peaks (optional)
    if (params.find_peaks) {
        // Simple peak detection: local maxima with g > 1.5
        for (int i = 1; i < result.n_bins - 1; ++i) {
            if (result.g_total[i] > 1.5 &&
                result.g_total[i] > result.g_total[i-1] &&
                result.g_total[i] > result.g_total[i+1]) {
                
                RDFResult::Peak peak;
                peak.r_peak = result.r[i];
                peak.g_peak = result.g_total[i];
                
                // Find first minimum after peak
                peak.r_min = params.rmax;
                for (int j = i + 1; j < result.n_bins; ++j) {
                    if (result.g_total[j] < result.g_total[j-1]) {
                        peak.r_min = result.r[j];
                        break;
                    }
                }
                
                result.peaks.push_back(peak);
            }
        }
    }
    
    return result;
}

// ============================================================================
// CRYSTALLINITY SCORE
// ============================================================================

double compute_crystallinity(const RDFResult& rdf) {
    if (rdf.g_total.empty()) return 0.0;
    
    // Find first major peak (g > 1.5, within first 5 Å)
    double max_g = 0.0;
    int peak_idx = -1;
    
    for (size_t i = 0; i < rdf.g_total.size(); ++i) {
        if (rdf.r[i] > 5.0) break;  // Only look at first coordination shell
        
        if (rdf.g_total[i] > max_g && rdf.g_total[i] > 1.5) {
            max_g = rdf.g_total[i];
            peak_idx = static_cast<int>(i);
        }
    }
    
    if (peak_idx < 0) return 0.0;  // No peak found
    
    // Compute peak width (FWHM - Full Width at Half Maximum)
    double half_max = max_g / 2.0;
    int left_idx = peak_idx;
    int right_idx = peak_idx;
    
    // Find left edge
    for (int i = peak_idx; i >= 0; --i) {
        if (rdf.g_total[i] < half_max) {
            left_idx = i;
            break;
        }
    }
    
    // Find right edge
    for (size_t i = peak_idx; i < rdf.g_total.size(); ++i) {
        if (rdf.g_total[i] < half_max) {
            right_idx = static_cast<int>(i);
            break;
        }
    }
    
    double peak_width = (right_idx - left_idx) * rdf.bin_width;
    
    if (peak_width < 1e-6) peak_width = rdf.bin_width;  // Avoid division by zero
    
    // Crystallinity = peak_height / peak_width
    // Normalize to [0, 1] range (heuristic)
    double crystallinity = max_g / (peak_width * 10.0);  // 10.0 is empirical scaling
    
    return std::min(1.0, std::max(0.0, crystallinity));
}

// ============================================================================
// OUTPUT
// ============================================================================

void write_rdf_csv(
    const std::string& path,
    const RDFResult& rdf,
    int step,
    double temperature
) {
    std::ofstream file(path);
    if (!file) {
        std::cerr << "WARNING: Failed to write RDF to " << path << "\n";
        return;
    }
    
    // Header
    file << "# RDF (Radial Distribution Function)\n";
    file << "# Step: " << step << "\n";
    file << "# Temperature: " << temperature << " K\n";
    file << "# Atoms: " << rdf.n_atoms << "\n";
    file << "# Pairs counted: " << rdf.n_pairs << "\n";
    file << "# rmax: " << rdf.rmax << " Angstrom\n";
    file << "# bin_width: " << rdf.bin_width << " Angstrom\n";
    file << "#\n";
    
    // Column headers
    file << "r(Angstrom),g_total";
    for (const auto& [label, g] : rdf.g_pair) {
        file << ",g_" << label;
    }
    file << "\n";
    
    // Data rows
    for (int i = 0; i < rdf.n_bins; ++i) {
        file << std::fixed << std::setprecision(6) << rdf.r[i] << ","
             << std::setprecision(6) << rdf.g_total[i];
        
        for (const auto& [label, g] : rdf.g_pair) {
            file << "," << std::setprecision(6) << g[i];
        }
        file << "\n";
    }
    
    // Peak summary (if computed)
    if (!rdf.peaks.empty()) {
        file << "#\n# Detected peaks:\n";
        for (const auto& peak : rdf.peaks) {
            file << "# r=" << std::fixed << std::setprecision(3) << peak.r_peak 
                 << " A, g=" << std::setprecision(3) << peak.g_peak
                 << ", r_min=" << std::setprecision(3) << peak.r_min << " A\n";
        }
    }
}

} // namespace cli
} // namespace vsepr
