#pragma once

#include "atomistic/core/state.hpp"
#include "cli/metrics_rdf.hpp"
#include <vector>
#include <map>
#include <string>

namespace vsepr {
namespace cli {

// ============================================================================
// COORDINATION NUMBER ANALYSIS
// ============================================================================

/**
 * Coordination Number (CN) Analysis
 * 
 * Counts neighbors within first coordination shell (defined by RDF minimum).
 * 
 * For each atom i of species A:
 *   CN_A→B(i) = #{j ∈ B | r_ij < r_cut_A,B}
 * 
 * Where r_cut_A,B = first RDF minimum for pair (A,B)
 */
struct CoordinationResult {
    // Pair-specific coordination histograms
    // Key: "Mg-F", Value: histogram[CN] = count
    std::map<std::string, std::vector<int>> histograms;
    
    // Mean coordination and std deviation for each pair
    std::map<std::string, double> mean_CN;
    std::map<std::string, double> std_CN;
    
    // Modal (most common) CN for each pair
    std::map<std::string, int> modal_CN;
    
    // Coordination crystallinity: fraction of atoms with modal CN
    std::map<std::string, double> crystallinity_fraction;
    
    // Cutoff distances used (from RDF minima)
    std::map<std::string, double> cutoffs;
    
    // Total atoms analyzed per species
    std::map<std::string, int> n_atoms_by_species;
};

/**
 * Compute coordination numbers from State
 * 
 * Uses RDF first minimum as cutoff distance (scientifically standard).
 * 
 * Algorithm:
 * 1. Extract r_cut from RDF first minimum for each pair
 * 2. For each atom, count neighbors within r_cut
 * 3. Build histogram of coordination numbers
 * 4. Compute mean, std, modal CN
 * 5. Compute "coordination crystallinity" (fraction at modal CN)
 * 
 * @param state Atomistic state with positions and types
 * @param rdf RDF result (used to extract cutoffs from first minimum)
 * @return Coordination analysis results
 */
CoordinationResult compute_coordination(
    const atomistic::State& state,
    const RDFResult& rdf
);

/**
 * Write coordination analysis to CSV
 * 
 * Format:
 * # Coordination Number Analysis
 * # Pair, Mean_CN, Std_CN, Modal_CN, Crystallinity, Cutoff(A)
 * Mg-F, 5.8, 0.4, 6, 0.83, 2.45
 * F-F, 11.2, 1.1, 12, 0.67, 3.50
 * 
 * # Histogram for Mg-F
 * CN, Count
 * 4, 1
 * 5, 2
 * 6, 5
 * 7, 0
 */
void write_coordination_csv(
    const std::string& path,
    const CoordinationResult& coord,
    int step = 0,
    double temperature = 0.0
);

} // namespace cli
} // namespace vsepr
