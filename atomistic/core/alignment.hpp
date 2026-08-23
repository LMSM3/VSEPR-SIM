#pragma once
#include "state.hpp"
#include "linalg.hpp"
#include <vector>
#include <functional>
#include <optional>

namespace atomistic {

/**
 * Structural alignment utilities for molecular superposition
 * 
 * Key algorithms:
 * - RMSD: Root-mean-square deviation between structures
 * - Kabsch: Optimal rotation minimizing RMSD
 * - COM centering: Translate to center-of-mass frame
 */

// Compute RMSD between two states (assumes same N, aligned)
double compute_rmsd(const State& a, const State& b);

/**
 * Result of Kabsch alignment
 */
struct AlignmentResult {
    double rmsd_before;          // RMSD before alignment
    double rmsd_after;           // RMSD after optimal rotation
    Vec3 translation;            // COM translation applied
    linalg::Mat3 R;              // Optimal rotation matrix (3x3)
    
    // Visualization data (for camera tracking)
    Vec3 reference_com;          // Reference center of mass
    Vec3 target_com_before;      // Target COM before alignment
    Vec3 target_com_after;       // Target COM after alignment
    double max_deviation;        // Maximum atom displacement during alignment
};

/**
 * Kabsch algorithm: Find optimal rotation to minimize RMSD
 * 
 * Aligns target onto reference by:
 * 1. Centering both at COM
 * 2. Computing covariance H = Σ(target ⊗ reference)
 * 3. SVD: H = U Σ V^T
 * 4. Optimal rotation: R = V·U^T (with chirality correction)
 * 
 * Modifies target.X (and target.V if present) in-place
 * Returns RMSD before/after and rotation matrix
 */
AlignmentResult kabsch_align(State& target, const State& reference);

/**
 * Animated alignment with visualization callbacks
 * 
 * Smoothly rotates target onto reference over N steps
 * Calls callback each step for camera tracking/rendering
 * 
 * @param target State to align (modified in-place)
 * @param reference Reference state
 * @param n_steps Number of interpolation steps (default 60)
 * @param callback Called each step with (progress, current_rmsd, target_state)
 * @return Final alignment result
 */
AlignmentResult animated_align(
    State& target,
    const State& reference,
    int n_steps = 60,
    std::optional<std::function<void(double, double, const State&)>> callback = std::nullopt
);

/**
 * Camera tracking parameters for alignment visualization
 */
struct AlignmentCamera {
    Vec3 position;      // Camera position
    Vec3 target;        // Look-at target (usually COM)
    Vec3 up;            // Up vector
    double distance;    // Distance from target
    double fov;         // Field of view (degrees)
};

/**
 * Compute optimal camera position to view both states during alignment
 * 
 * @param reference Reference state
 * @param target Target state (before alignment)
 * @param result Alignment result (for COM positions)
 * @return Camera parameters to view both structures
 */
AlignmentCamera compute_alignment_camera(
    const State& reference,
    const State& target,
    const AlignmentResult& result
);

/**
 * Interpolate camera between two positions (for smooth tracking)
 * 
 * @param cam_start Starting camera
 * @param cam_end Ending camera
 * @param t Interpolation parameter [0, 1]
 * @return Interpolated camera
 */
AlignmentCamera interpolate_camera(
    const AlignmentCamera& cam_start,
    const AlignmentCamera& cam_end,
    double t
);

// Center a state at origin (center of mass)
void center_at_origin(State& s);

/**
 * Compute center of mass of a state
 */
Vec3 compute_com(const State& s);

/**
 * Compute bounding sphere radius (for camera framing)
 */
double compute_bounding_radius(const State& s, const Vec3& center);

} // namespace atomistic

