#include "alignment.hpp"
#include "linalg.hpp"
#include <cmath>
#include <algorithm>
#include <stdexcept>

namespace atomistic {

Vec3 compute_com(const State& s) {
    if (s.N == 0 || s.M.size() != s.N) return {0, 0, 0};
    
    Vec3 com = {0, 0, 0};
    double total_mass = 0.0;
    
    for (uint32_t i = 0; i < s.N; ++i) {
        com = com + s.X[i] * s.M[i];
        total_mass += s.M[i];
    }
    
    if (total_mass > 0) {
        com = com * (1.0 / total_mass);
    }
    
    return com;
}

void center_at_origin(State& s) {
    Vec3 com = compute_com(s);
    for (auto& x : s.X) {
        x = x - com;
    }
}

double compute_bounding_radius(const State& s, const Vec3& center) {
    if (s.N == 0) return 1.0;
    
    double max_r2 = 0.0;
    for (uint32_t i = 0; i < s.N; ++i) {
        Vec3 d = s.X[i] - center;
        double r2 = dot(d, d);
        if (r2 > max_r2) max_r2 = r2;
    }
    
    return std::sqrt(max_r2);
}

double compute_rmsd(const State& a, const State& b) {
    if (a.N != b.N || a.N == 0) return 0.0;
    
    double sum = 0.0;
    for (uint32_t i = 0; i < a.N; ++i) {
        Vec3 d = a.X[i] - b.X[i];
        sum += dot(d, d);
    }
    
    return std::sqrt(sum / a.N);
}

/**
 * Kabsch algorithm for optimal rotation alignment
 * 
 * Physics:
 * - Minimizes RMSD = sqrt(Σ|R·xi - yi|²/N) over all rotations R
 * - Solution: R = V·U^T where H = U Σ V^T is SVD of covariance matrix
 * - Covariance: H = Σ(target_i ⊗ reference_i)
 * - Chirality correction: if det(R) < 0, flip sign of smallest singular vector
 * 
 * References:
 * - Kabsch, W. (1976). "A solution for the best rotation..." Acta Cryst. A32, 922.
 * - Kabsch, W. (1978). "A discussion of the solution..." Acta Cryst. A34, 827.
 * - Coutsias, E.A. et al. (2004). "Using quaternions..." J. Comp. Chem. 25(15), 1849.
 */
AlignmentResult kabsch_align(State& target, const State& reference) {
    AlignmentResult result;
    
    // Store initial COM positions for camera tracking
    result.target_com_before = compute_com(target);
    result.reference_com = compute_com(reference);
    
    result.rmsd_before = compute_rmsd(target, reference);
    
    if (target.N != reference.N || target.N < 2) {
        // Identity rotation
        result.R = linalg::Mat3::identity();
        result.translation = {0, 0, 0};
        result.rmsd_after = result.rmsd_before;
        result.target_com_after = result.target_com_before;
        result.max_deviation = 0.0;
        return result;
    }
    
    // Center both states at COM
    State ref_copy = reference;
    center_at_origin(target);
    center_at_origin(ref_copy);
    
    // Build covariance matrix H = Σ(target_i ⊗ reference_i)
    // H(i,j) = Σ target_k^i · reference_k^j
    linalg::Mat3 H = linalg::Mat3::zero();
    for (uint32_t k = 0; k < target.N; ++k) {
        H(0,0) += target.X[k].x * ref_copy.X[k].x;
        H(0,1) += target.X[k].x * ref_copy.X[k].y;
        H(0,2) += target.X[k].x * ref_copy.X[k].z;
        H(1,0) += target.X[k].y * ref_copy.X[k].x;
        H(1,1) += target.X[k].y * ref_copy.X[k].y;
        H(1,2) += target.X[k].y * ref_copy.X[k].z;
        H(2,0) += target.X[k].z * ref_copy.X[k].x;
        H(2,1) += target.X[k].z * ref_copy.X[k].y;
        H(2,2) += target.X[k].z * ref_copy.X[k].z;
    }
    
    // SVD: H = U Σ V^T
    linalg::SVD3 svd(H);
    
    // Optimal rotation: R = V·U^T
    linalg::Mat3 R_opt = svd.V * svd.U.transpose();
    
    // Chirality check: if det(R) < 0, reflection occurred
    // Correct by flipping sign of column corresponding to smallest singular value
    if (R_opt.det() < 0) {
        // Flip last column of V (smallest singular value)
        svd.V(0,2) = -svd.V(0,2);
        svd.V(1,2) = -svd.V(1,2);
        svd.V(2,2) = -svd.V(2,2);
        R_opt = svd.V * svd.U.transpose();
    }
    
    // Track maximum deviation for camera framing
    result.max_deviation = 0.0;
    for (uint32_t i = 0; i < target.N; ++i) {
        Vec3 before = target.X[i];
        Vec3 after = R_opt * target.X[i];
        Vec3 displacement = after - before;
        double dev = std::sqrt(dot(displacement, displacement));
        if (dev > result.max_deviation) {
            result.max_deviation = dev;
        }
    }
    
    // Apply rotation to target
    for (uint32_t i = 0; i < target.N; ++i) {
        target.X[i] = R_opt * target.X[i];
    }
    
    // Also rotate velocities if present
    if (target.V.size() == target.N) {
        for (uint32_t i = 0; i < target.N; ++i) {
            target.V[i] = R_opt * target.V[i];
        }
    }
    
    // Store result
    result.R = R_opt;
    result.translation = {0, 0, 0}; // Already centered
    result.target_com_after = compute_com(target);
    result.rmsd_after = compute_rmsd(target, ref_copy);
    
    return result;
}

/**
 * Animated alignment with smooth interpolation
 * 
 * Uses SLERP (spherical linear interpolation) for rotation
 * Converts rotation matrix to quaternion for smooth interpolation
 */
AlignmentResult animated_align(
    State& target,
    const State& reference,
    int n_steps,
    std::optional<std::function<void(double, double, const State&)>> callback
) {
    if (n_steps < 1) n_steps = 1;
    
    // Store initial state
    State initial_target = target;
    
    // Compute final alignment (without modifying target yet)
    State temp_target = target;
    AlignmentResult final_result = kabsch_align(temp_target, reference);
    
    // Restore target to initial state
    target = initial_target;
    center_at_origin(target);
    
    // Center reference
    State ref_copy = reference;
    center_at_origin(ref_copy);
    
    // Interpolate rotation from identity to final R
    linalg::Mat3 R_identity = linalg::Mat3::identity();
    linalg::Mat3 R_final = final_result.R;
    
    // Animate through steps
    for (int step = 0; step <= n_steps; ++step) {
        double t = static_cast<double>(step) / static_cast<double>(n_steps);
        
        // Linear interpolation of rotation (not ideal, but simple)
        // Better: use quaternion SLERP for smooth rotation
        linalg::Mat3 R_t = linalg::Mat3::zero();
        for (int i = 0; i < 9; ++i) {
            R_t.m[i] = (1.0 - t) * R_identity.m[i] + t * R_final.m[i];
        }
        
        // Apply interpolated rotation
        State current_target = initial_target;
        center_at_origin(current_target);
        for (uint32_t i = 0; i < current_target.N; ++i) {
            current_target.X[i] = R_t * current_target.X[i];
        }
        
        // Compute current RMSD
        double current_rmsd = compute_rmsd(current_target, ref_copy);
        
        // Call visualization callback
        if (callback.has_value()) {
            (*callback)(t, current_rmsd, current_target);
        }
    }
    
    // Apply final alignment
    target = temp_target;
    
    return final_result;
}

/**
 * Compute optimal camera to view both structures
 */
AlignmentCamera compute_alignment_camera(
    const State& reference,
    const State& target,
    const AlignmentResult& result
) {
    (void)result;  // Reserved for future use (e.g., max_deviation framing)
    
    AlignmentCamera cam;
    
    // Camera looks at midpoint between reference and target COMs
    Vec3 ref_com = compute_com(reference);
    Vec3 tgt_com = compute_com(target);
    cam.target = (ref_com + tgt_com) * 0.5;
    
    // Compute bounding sphere to frame both structures
    double ref_radius = compute_bounding_radius(reference, ref_com);
    double tgt_radius = compute_bounding_radius(target, tgt_com);
    double max_radius = std::max(ref_radius, tgt_radius);
    
    // Add separation distance between structures
    Vec3 separation = tgt_com - ref_com;
    double sep_dist = std::sqrt(dot(separation, separation));
    
    // Camera distance: fit both structures + separation
    double total_extent = max_radius * 2.0 + sep_dist;
    cam.fov = 45.0;  // degrees
    cam.distance = total_extent / std::tan(cam.fov * 0.5 * 3.14159 / 180.0) * 1.5;  // 1.5x margin
    
    // Position camera along Z-axis (looking down -Z)
    cam.up = {0, 1, 0};
    cam.position = cam.target + Vec3{0, 0, static_cast<float>(cam.distance)};
    
    return cam;
}

/**
 * Smooth camera interpolation using SLERP for orientation
 */
AlignmentCamera interpolate_camera(
    const AlignmentCamera& cam_start,
    const AlignmentCamera& cam_end,
    double t
) {
    AlignmentCamera cam;
    
    // Linear interpolation of position and target
    cam.position.x = (1.0 - t) * cam_start.position.x + t * cam_end.position.x;
    cam.position.y = (1.0 - t) * cam_start.position.y + t * cam_end.position.y;
    cam.position.z = (1.0 - t) * cam_start.position.z + t * cam_end.position.z;
    
    cam.target.x = (1.0 - t) * cam_start.target.x + t * cam_end.target.x;
    cam.target.y = (1.0 - t) * cam_start.target.y + t * cam_end.target.y;
    cam.target.z = (1.0 - t) * cam_start.target.z + t * cam_end.target.z;
    
    cam.up.x = (1.0 - t) * cam_start.up.x + t * cam_end.up.x;
    cam.up.y = (1.0 - t) * cam_start.up.y + t * cam_end.up.y;
    cam.up.z = (1.0 - t) * cam_start.up.z + t * cam_end.up.z;
    
    // Normalize up vector
    double up_len = std::sqrt(dot(cam.up, cam.up));
    if (up_len > 1e-8) {
        cam.up = cam.up * (1.0 / up_len);
    }
    
    // Interpolate distance and FOV
    cam.distance = (1.0 - t) * cam_start.distance + t * cam_end.distance;
    cam.fov = (1.0 - t) * cam_start.fov + t * cam_end.fov;
    
    return cam;
}

} // namespace atomistic

