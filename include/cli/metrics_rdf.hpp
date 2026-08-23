#pragma once

#include "atomistic/core/state.hpp"
#include <vector>
#include <map>
#include <string>

namespace vsepr {
namespace cli {

// ============================================================================
// RDF (Radial Distribution Function) Analysis
// ============================================================================

/**
 * Radial Distribution Function g(r)
 * 
 * Measures probability of finding an atom at distance r relative to
 * uniform random distribution (ideal gas).
 * 
 * g(r) = 1: random distribution
 * g(r) > 1: preferred distance (bonding, lattice spacing)
 * g(r) < 1: excluded volume (repulsion)
 * 
 * For crystals: sharp peaks at lattice spacings
 * For liquids: broad peaks, decay to 1.0
 * For gases: g(r) ≈ 1.0 everywhere
 */
struct RDFResult {
    // Total RDF (all pairs)
    std::vector<double> r;       // Bin centers (Å)
    std::vector<double> g_total; // g(r) for all pairs
    
    // Pair-specific RDFs
    std::map<std::string, std::vector<double>> g_pair;
    // Examples: "Mg-F", "F-F", "Ce-F"
    
    // Metadata
    double rmax;                 // Maximum distance computed (Å)
    double bin_width;            // Bin size (Å)
    int n_bins;                  // Number of bins
    int n_atoms;                 // Total atoms in system
    int n_pairs;                 // Total pairs counted
    
    // Peak analysis (optional)
    struct Peak {
        double r_peak;           // Peak position (Å)
        double g_peak;           // Peak height
        double r_min;            // First minimum after peak (Å)
    };
    std::vector<Peak> peaks;
};

/**
 * RDF computation parameters
 */
struct RDFParams {
    double rmax = 10.0;          // Maximum distance (Å)
    double bin_width = 0.1;      // Bin size (Å)
    bool compute_pairs = true;   // Compute pair-specific RDFs?
    bool find_peaks = false;     // Detect peak positions?
};

/**
 * Compute RDF from atomistic State
 * 
 * Uses PBC minimum image convention if state.box.enabled = true
 * 
 * Algorithm:
 * 1. Histogram all pairwise distances into bins
 * 2. Normalize by:
 *    - Number of atoms
 *    - Bin volume (4π r² Δr)
 *    - System density (N/V)
 * 
 * Time complexity: O(N²) for N atoms
 * Space complexity: O(n_bins * n_pairs)
 */
RDFResult compute_rdf(
    const atomistic::State& state,
    const RDFParams& params = RDFParams{}
);

/**
 * Compute crystallinity score from RDF
 * 
 * Simple metric: peak_height / peak_width
 * 
 * Returns:
 * - 0.0 = completely amorphous
 * - 1.0 = perfect crystal (narrow, tall peaks)
 * 
 * Note: This is a heuristic, not a rigorous order parameter!
 */
double compute_crystallinity(const RDFResult& rdf);

/**
 * Write RDF to CSV file
 * 
 * Format:
 * # RDF for <system> at step=<step> T=<T>K
 * # r(Angstrom), g_total, g_Mg-F, g_F-F, ...
 * 0.05, 0.000, 0.000, 0.000
 * 0.15, 0.012, 0.008, 0.015
 * ...
 */
void write_rdf_csv(
    const std::string& path,
    const RDFResult& rdf,
    int step = 0,
    double temperature = 0.0
);

} // namespace cli
} // namespace vsepr
