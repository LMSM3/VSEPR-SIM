#include "cli/rdf_accumulator.hpp"
#include <cmath>
#include <algorithm>
#include <stdexcept>

namespace vsepr {
namespace cli {

RDFAccumulator::RDFAccumulator(double r_max, double dr)
    : r_max_(r_max), dr_(dr), n_samples_(0)
{
    if (r_max <= 0.0 || dr <= 0.0) {
        throw std::runtime_error("RDFAccumulator: r_max and dr must be positive");
    }
    
    // Compute number of bins
    n_bins_ = static_cast<int>(std::floor(r_max_ / dr_));
    
    if (n_bins_ < 10) {
        throw std::runtime_error("RDFAccumulator: Too few bins (increase r_max or decrease dr)");
    }
    
    // Initialize bins
    r_bins_.resize(n_bins_);
    histogram_.resize(n_bins_, 0);
    g_r_.resize(n_bins_, 0.0);
    
    // Set bin centers
    for (int i = 0; i < n_bins_; ++i) {
        r_bins_[i] = (i + 0.5) * dr_;
    }
}

double RDFAccumulator::min_image_distance(
    double dx, double dy, double dz,
    double Lx, double Ly, double Lz
) {
    // PBC minimum-image convention
    dx -= Lx * std::round(dx / Lx);
    dy -= Ly * std::round(dy / Ly);
    dz -= Lz * std::round(dz / Lz);
    
    return std::sqrt(dx*dx + dy*dy + dz*dz);
}

void RDFAccumulator::accumulate(
    const atomistic::State& state,
    const std::vector<double>& box_lengths
) {
    if (box_lengths.size() != 3) {
        throw std::runtime_error("RDFAccumulator: box_lengths must have 3 elements");
    }
    
    if (state.N < 2) {
        // Need at least 2 atoms for RDF
        return;
    }
    
    double Lx = box_lengths[0];
    double Ly = box_lengths[1];
    double Lz = box_lengths[2];
    
    // Accumulate pairwise distances (count each pair once: i < j)
    for (uint32_t i = 0; i < state.N; ++i) {
        for (uint32_t j = i + 1; j < state.N; ++j) {
            // Compute distance vector
            double dx = state.X[j].x - state.X[i].x;
            double dy = state.X[j].y - state.X[i].y;
            double dz = state.X[j].z - state.X[i].z;
            
            // Apply PBC minimum-image
            double r = min_image_distance(dx, dy, dz, Lx, Ly, Lz);
            
            // Skip if outside r_max
            if (r >= r_max_) continue;
            
            // Find bin
            int bin = static_cast<int>(std::floor(r / dr_));
            
            if (bin >= 0 && bin < n_bins_) {
                histogram_[bin]++;
            }
        }
    }
    
    n_samples_++;
}

void RDFAccumulator::compute_gr(int N_total, double V_box) {
    if (n_samples_ == 0) {
        throw std::runtime_error("RDFAccumulator: No samples accumulated");
    }

    if (N_total < 2) {
        throw std::runtime_error("RDFAccumulator: Need at least 2 atoms for RDF");
    }

    if (V_box <= 0.0) {
        throw std::runtime_error("RDFAccumulator: Box volume must be positive");
    }

    // Number density (atoms per Angstrom^3)
    double rho = static_cast<double>(N_total) / V_box;

    // Normalize each bin
    for (int i = 0; i < n_bins_; ++i) {
        double r = r_bins_[i];

        // Shell volume: 4/3 * pi * [(r + dr/2)^3 - (r - dr/2)^3]
        double r_inner = r - dr_ / 2.0;
        double r_outer = r + dr_ / 2.0;

        // Clamp r_inner to 0 (first bin)
        if (r_inner < 0.0) r_inner = 0.0;

        double shell_volume = (4.0 / 3.0) * M_PI * 
            (std::pow(r_outer, 3) - std::pow(r_inner, 3));

        // Expected pairs per snapshot:
        // We loop over N atoms, and for each we look at (N-1) others
        // For ideal gas, expected count in shell = N * rho * shell_volume
        // But since we count i < j (each pair once), we get N*(N-1)/2 pairs total
        // So expected per shell = N * (N-1)/2 * (shell_volume / V_box) * 2
        // Simplified: expected = N * (N-1) * (shell_volume / V_box)

        // Actually, let's think of it differently:
        // For each atom i, how many atoms j do we expect in the shell?
        // Answer: (N-1) * (shell_volume / V_box)  [uniform distribution]
        // We sum over N atoms, but count each pair once (i < j)
        // So total expected = N * (N-1) * (shell_volume / V_box) / 2

        double prob_in_shell = shell_volume / V_box;
        double expected_per_snapshot = N_total * (N_total - 1) * prob_in_shell / 2.0;

        // Total expected over all snapshots
        double expected_total = expected_per_snapshot * n_samples_;

        // Normalize: g(r) = observed / expected
        if (expected_total > 0.0) {
            double observed = static_cast<double>(histogram_[i]);
            g_r_[i] = observed / expected_total;
        } else {
            g_r_[i] = 0.0;
        }
    }
}

}} // namespace vsepr::cli
