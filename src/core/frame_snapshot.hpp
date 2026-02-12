#pragma once

#include "core/math_vec3.hpp"
#include <vector>
#include <utility>
#include <string>
#include <unordered_map>

namespace vsepr {

/**
 * Immutable snapshot of simulation state for rendering.
 * The simulation owns the data; the renderer consumes snapshots.
 * This is the stable data contract between simulation and visualization.
 */
struct FrameSnapshot {
    // Required data
    std::vector<Vec3> positions;           // Atom positions in Cartesian coordinates
    std::vector<int> atomic_numbers;       // Atomic numbers for coloring/sizing
    
    // Optional topology data
    std::vector<std::pair<int, int>> bonds; // Bond index pairs for line rendering
    
    // Optional diagnostic data
    double energy = 0.0;                   // Total system energy
    int iteration = 0;                     // Optimization iteration count
    double rms_force = 0.0;                // RMS force magnitude
    double max_force = 0.0;                // Maximum force component
    
    // Extended statistics (for command bus data queries)
    std::unordered_map<std::string, double> stats;
    
    // Metadata
    std::string status_message;            // Optional status string
    
    // Clear all data
    void clear() {
        positions.clear();
        atomic_numbers.clear();
        bonds.clear();
        energy = 0.0;
        iteration = 0;
        rms_force = 0.0;
        max_force = 0.0;
        stats.clear();
        status_message.clear();
    }
    
    // Check if snapshot has valid data
    bool is_valid() const {
        return !positions.empty() && positions.size() == atomic_numbers.size();
    }
};

} // namespace vsepr
