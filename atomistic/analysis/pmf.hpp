#pragma once
#include <vector>
#include <string>

namespace atomistic {

/**
 * Pair Type Selector
 * 
 * Generic pair specification for PMF calculation.
 * Format: "Element1:Element2" (e.g., "Mg:F", "Ar:Ar")
 */
struct PairType {
    int type1;  // Atomic number (Z)
    int type2;  // Atomic number (Z)

    // Parse from string (e.g., "Mg:F" → {12, 9})
    static PairType from_string(const std::string& spec);

    // Format for filenames (e.g., "Mg_F")
    std::string to_string() const;

    // Check if ordered pair matches (A-B or B-A)
    bool matches(int z1, int z2) const;
};

/**
 * PMF Calculation Result
 * 
 * Contains PMF curve, RDF, and extracted features.
 */
struct PMFResult {
    // Raw data
    std::vector<double> r;          // Distance bins (Å)
    std::vector<double> g_r;        // Radial distribution function g(r)
    std::vector<double> pmf;        // Potential of mean force (kcal/mol, SHIFTED to tail=0)

    // Extracted features
    double basin_depth;             // Basin depth RELATIVE to tail (kcal/mol, positive)
    double basin_position;          // r at PMF minimum (Å)
    double barrier_height;          // Barrier height relative to basin (kcal/mol, NaN if none)
    int basin_index;                // Index of basin minimum
    int barrier_index;              // Index of barrier maximum (-1 if none)
    bool has_barrier;               // True if barrier detected

    // Reference shift
    double pmf_shift;               // Shift applied to make tail = 0
    double tail_mean;               // Mean PMF in tail region (before shift)
    int tail_start_index;           // Index where tail region starts

    // Quality metrics
    double g_min_floor;             // Floor applied to g(r) to avoid ln(0)
    int floored_bins;               // Number of bins where g(r) was floored

    // Metadata
    PairType pair;
    double temperature;             // K
    double k_B;                     // kcal/mol/K
    int n_samples;                  // Number of distance samples used
    double r_max;                   // Maximum distance (Å)
    double bin_width;               // Bin width (Å)
};

/**
 * PMF Calculator
 * 
 * Computes potential of mean force from radial distribution function.
 * 
 * PMF(r) = -k_B T ln(g(r))
 * 
 * Features:
 * - Generic pair selection (works for any atom types)
 * - Handles g(r) = 0 gracefully (sets PMF = +inf)
 * - Extracts basin/barrier features
 * - Outputs CSV + JSON metadata
 */
class PMFCalculator {
public:
    /**
     * Compute PMF from RDF
     * 
     * Input: RDF data from trajectory analysis
     * Output: PMF curve with extracted features
     * 
     * PMF is shifted so that tail → 0 for meaningful basin depth comparison.
     * g(r) = 0 bins are floored to g_min to avoid ln(0).
     */
    PMFResult compute_from_rdf(
        const std::vector<double>& r_bins,
        const std::vector<double>& g_r,
        PairType pair,
        double temperature,
        double g_min = 1e-10,      // Floor for g(r) to avoid ln(0)
        double tail_fraction = 0.2  // Fraction of tail to average for shift
    );

    /**
     * Save PMF to CSV (primary output)
     * 
     * Format:
     *   # PMF for Mg-F at 300 K
     *   # Units: r (Angstrom), g(r) (unitless), PMF (kcal/mol)
     *   r,g(r),PMF(r)
     *   0.5,0.000,inf
     *   1.0,0.002,3.45
     *   ...
     */
    void save_csv(const PMFResult& pmf, const std::string& filename);

    /**
     * Save metadata to JSON (sidecar)
     * 
     * Format:
     *   {
     *     "pair": "Mg:F",
     *     "temperature": 300.0,
     *     "k_B": 0.001987,
     *     "n_samples": 15000,
     *     "r_max": 10.0,
     *     "bin_width": 0.1,
     *     "basin_depth": -2.34,
     *     "basin_position": 2.1,
     *     "barrier_height": 0.5
     *   }
     */
    void save_metadata_json(const PMFResult& pmf, const std::string& filename);

private:
    // Feature extraction
    double find_basin_depth(const std::vector<double>& pmf, int& basin_idx);
    double find_barrier_height(const std::vector<double>& pmf, int basin_idx, int& barrier_idx);
};

} // namespace atomistic
