#pragma once
/**
 * neighbor_list.hpp - Neighbor List for Nonbonded Interactions
 * 
 * Implements Verlet neighbor list for efficient nonbonded force calculation.
 * 
 * NEIGHBOR LIST HAS NOT YET BEEN IMPLIMENTED THIS IS A PLACEHOLDER
 * 
 */

#include <vector>
#include <cmath>

namespace vsepr {

class NeighborList {
public:
    NeighborList(double cutoff, double skin = 0.5)
        : cutoff_(cutoff), skin_(skin), r_list_(cutoff + skin) {}
    
    // Build neighbor list
    void build(const std::vector<double>& coords, const double* box = nullptr);
    
    // Check if rebuild needed (based on max displacement)
    bool needs_rebuild(const std::vector<double>& coords) const;
    
    // Get neighbor pairs
    const std::vector<std::pair<int, int>>& pairs() const { return pairs_; }
    
    // Statistics
    int num_pairs() const { return pairs_.size(); }
    int num_rebuilds() const { return rebuild_count_; }
    
private:
    double cutoff_;         // Interaction cutoff
    double skin_;           // Skin distance
    double r_list_;         // List cutoff = cutoff + skin
    
    std::vector<std::pair<int, int>> pairs_;
    std::vector<double> coords_ref_;  // Reference coords for rebuild check
    int rebuild_count_ = 0;
};

// Placeholder implementation
inline void NeighborList::build(const std::vector<double>& coords, const double* box) {
    (void)box;  // TODO: Use box for PBC
    pairs_.clear();
    coords_ref_ = coords;
    
    const size_t N = coords.size() / 3;
    for (size_t i = 0; i < N; ++i) {
        for (size_t j = i + 1; j < N; ++j) {
            double dx = coords[3*j] - coords[3*i];
            double dy = coords[3*j+1] - coords[3*i+1];
            double dz = coords[3*j+2] - coords[3*i+2];
            double r2 = dx*dx + dy*dy + dz*dz;
            
            if (r2 < r_list_ * r_list_) {
                pairs_.push_back({static_cast<int>(i), static_cast<int>(j)});
            }
        }
    }
    rebuild_count_++;
}

inline bool NeighborList::needs_rebuild(const std::vector<double>& coords) const {
    if (coords_ref_.empty()) return true;
    
    double max_disp2 = 0.0;
    const size_t N = coords.size() / 3;
    for (size_t i = 0; i < N; ++i) {
        double dx = coords[3*i] - coords_ref_[3*i];
        double dy = coords[3*i+1] - coords_ref_[3*i+1];
        double dz = coords[3*i+2] - coords_ref_[3*i+2];
        double d2 = dx*dx + dy*dy + dz*dz;
        max_disp2 = std::max(max_disp2, d2);
    }
    
    // Rebuild if any atom moved more than skin/2
    return max_disp2 > (skin_ * 0.5) * (skin_ * 0.5);
}

} // namespace vsepr
