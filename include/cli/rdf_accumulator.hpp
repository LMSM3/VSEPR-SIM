#pragma once
#include "atomistic/core/state.hpp"
#include <vector>
#include <string>

namespace vsepr {
namespace cli {

/**
 * Lightweight RDF Accumulator
 * 
 * Accumulates radial distribution function from MD snapshots.
 * 
 * Design:
 * - Accumulate at checkpoints (not every step)
 * - PBC minimum-image distances
 * - Proper normalization (shell volume + density)
 * - Total RDF only (no partials for now)
 */
class RDFAccumulator {
public:
    /**
     * Constructor
     * 
     * @param r_max Maximum distance (Angstroms, should be min(Lx,Ly,Lz)/2)
     * @param dr Bin width (Angstroms, typically 0.02-0.05)
     */
    RDFAccumulator(double r_max, double dr);
    
    /**
     * Accumulate RDF from current state
     * 
     * Computes pairwise distances with PBC minimum-image.
     * Updates histogram counts.
     * 
     * @param state Current atomistic state
     * @param box_lengths [Lx, Ly, Lz] for PBC
     */
    void accumulate(const atomistic::State& state, const std::vector<double>& box_lengths);
    
    /**
     * Compute normalized g(r)
     * 
     * Normalizes histogram by shell volume and number density.
     * Returns (r_bins, g_r) suitable for PMF calculation.
     * 
     * @param N_total Total number of atoms
     * @param V_box Box volume (Angstroms^3)
     */
    void compute_gr(int N_total, double V_box);
    
    /**
     * Get results
     */
    const std::vector<double>& get_r_bins() const { return r_bins_; }
    const std::vector<double>& get_gr() const { return g_r_; }
    
    /**
     * Get number of samples accumulated
     */
    int get_n_samples() const { return n_samples_; }
    
    /**
     * Check if minimum samples reached
     */
    bool has_sufficient_samples(int min_samples = 10) const {
        return n_samples_ >= min_samples;
    }
    
private:
    double r_max_;              // Maximum distance
    double dr_;                 // Bin width
    int n_bins_;                // Number of bins
    
    std::vector<double> r_bins_;     // Bin centers
    std::vector<int> histogram_;     // Raw counts
    std::vector<double> g_r_;        // Normalized g(r)
    
    int n_samples_;             // Number of snapshots accumulated
    
    // PBC minimum-image distance
    double min_image_distance(double dx, double dy, double dz, 
                             double Lx, double Ly, double Lz);
};

}} // namespace vsepr::cli
