#pragma once
/**
 * orientation.hpp — Bead Orientation and Pairwise Invariants
 *
 * Defines the orientation representation for anisotropic CG beads
 * and the scalar invariants that characterize their relative alignment.
 *
 * Each bead carries:
 *   - A primary normal vector n̂ (largest-moment principal axis)
 *   - The full InertiaFrame for higher-fidelity models
 *
 * For a pair of beads (A, B), the rotationally invariant descriptors are:
 *   cos θ_A = n̂_A · r̂        (alignment of A's normal with inter-bead axis)
 *   cos θ_B = n̂_B · r̂        (alignment of B's normal with inter-bead axis)
 *   cos φ   = n̂_A · n̂_B      (mutual alignment of normals)
 *
 * Anti-black-box: all invariants are explicitly computed and inspectable.
 * Deterministic: same geometry always yields same invariants.
 *
 * Reference: Section "Orientation-Coupled Interaction Framework" of
 *            section_anisotropic_beads.tex, Equations (10)-(11)
 */

#include "atomistic/core/state.hpp"
#include "coarse_grain/core/inertia_frame.hpp"
#include <cmath>

namespace coarse_grain {

/**
 * BeadOrientation — orientation state of a single bead.
 *
 * The normal vector n̂ is the principal axis corresponding to the
 * largest moment of inertia (axis3 of InertiaFrame). For planar
 * molecules like benzene, this is the out-of-plane direction.
 */
struct BeadOrientation {
    atomistic::Vec3 normal{0.0, 0.0, 1.0};   // Primary axis (unit vector)

    /**
     * Construct from an InertiaFrame: use axis3 (largest-moment direction).
     * For a planar molecule this is the normal to the molecular plane.
     */
    static BeadOrientation from_frame(const InertiaFrame& frame) {
        BeadOrientation o;
        if (frame.valid) {
            o.normal = frame.axis3;
            // Ensure unit length
            double len = std::sqrt(o.normal.x * o.normal.x +
                                   o.normal.y * o.normal.y +
                                   o.normal.z * o.normal.z);
            if (len > 1e-30) {
                o.normal.x /= len;
                o.normal.y /= len;
                o.normal.z /= len;
            }
        }
        return o;
    }
};

/**
 * OrientationInvariants — the three scalar invariants characterizing
 * the relative alignment of two anisotropic beads.
 *
 * These are rotationally invariant: they depend only on the
 * geometry of the two beads, not on the global coordinate frame.
 */
struct OrientationInvariants {
    double cos_theta_A{};     // n̂_A · r̂   — alignment of A normal with separation axis
    double cos_theta_B{};     // n̂_B · r̂   — alignment of B normal with separation axis
    double cos_phi{};         // n̂_A · n̂_B — mutual alignment of normals

    double r{};               // |R_B - R_A| separation distance (Å)

    /**
     * Classification of the dimer configuration.
     *
     * Returns a human-readable label based on the invariants:
     *   "face-to-face"  — normals parallel, both aligned with axis (stacking)
     *   "edge-to-face"  — one normal perpendicular to axis (T-shaped)
     *   "edge-to-edge"  — both normals perpendicular to axis
     *   "offset"        — intermediate angles
     */
    const char* classify() const {
        double abs_cos_phi = std::abs(cos_phi);
        double abs_cos_tA  = std::abs(cos_theta_A);
        double abs_cos_tB  = std::abs(cos_theta_B);

        // Face-to-face: normals parallel, both roughly aligned with axis
        if (abs_cos_phi > 0.85 && abs_cos_tA > 0.7 && abs_cos_tB > 0.7)
            return "face-to-face";

        // Edge-to-face (T-shaped): one normal along axis, other perpendicular
        if ((abs_cos_tA > 0.7 && abs_cos_tB < 0.3) ||
            (abs_cos_tB > 0.7 && abs_cos_tA < 0.3))
            return "edge-to-face";

        // Edge-to-edge: both normals perpendicular to axis
        if (abs_cos_tA < 0.3 && abs_cos_tB < 0.3)
            return "edge-to-edge";

        return "offset";
    }
};

/**
 * Compute the orientation invariants for a pair of beads.
 *
 * @param pos_A   Position of bead A (Å)
 * @param pos_B   Position of bead B (Å)
 * @param ori_A   Orientation of bead A
 * @param ori_B   Orientation of bead B
 * @return OrientationInvariants with all three scalar invariants and distance
 */
inline OrientationInvariants compute_invariants(
    const atomistic::Vec3& pos_A,
    const atomistic::Vec3& pos_B,
    const BeadOrientation& ori_A,
    const BeadOrientation& ori_B)
{
    OrientationInvariants inv;

    // Separation vector: r = R_B - R_A
    double rx = pos_B.x - pos_A.x;
    double ry = pos_B.y - pos_A.y;
    double rz = pos_B.z - pos_A.z;

    inv.r = std::sqrt(rx * rx + ry * ry + rz * rz);

    if (inv.r < 1e-30) {
        // Degenerate: beads at same position
        inv.cos_theta_A = 0.0;
        inv.cos_theta_B = 0.0;
        inv.cos_phi = ori_A.normal.x * ori_B.normal.x +
                      ori_A.normal.y * ori_B.normal.y +
                      ori_A.normal.z * ori_B.normal.z;
        return inv;
    }

    // Unit separation vector
    double inv_r = 1.0 / inv.r;
    double rhat_x = rx * inv_r;
    double rhat_y = ry * inv_r;
    double rhat_z = rz * inv_r;

    // Scalar invariants
    inv.cos_theta_A = ori_A.normal.x * rhat_x +
                      ori_A.normal.y * rhat_y +
                      ori_A.normal.z * rhat_z;

    inv.cos_theta_B = ori_B.normal.x * rhat_x +
                      ori_B.normal.y * rhat_y +
                      ori_B.normal.z * rhat_z;

    inv.cos_phi = ori_A.normal.x * ori_B.normal.x +
                  ori_A.normal.y * ori_B.normal.y +
                  ori_A.normal.z * ori_B.normal.z;

    return inv;
}

/**
 * Compute the normal vector for a planar atom group.
 *
 * Uses the cross product of two in-plane vectors relative to COM
 * as a robust alternative to the inertia-tensor method for small groups.
 *
 * @param positions  Atom positions
 * @param indices    Atom indices (must have at least 3)
 * @param com        Center of mass
 * @return Unit normal vector, or {0,0,1} if degenerate
 */
inline atomistic::Vec3 compute_plane_normal(
    const std::vector<atomistic::Vec3>& positions,
    const std::vector<uint32_t>& indices,
    const atomistic::Vec3& com)
{
    if (indices.size() < 3)
        return {0.0, 0.0, 1.0};

    // Pick two distinct displacement vectors from COM
    atomistic::Vec3 v1 = {
        positions[indices[0]].x - com.x,
        positions[indices[0]].y - com.y,
        positions[indices[0]].z - com.z
    };
    atomistic::Vec3 v2 = {
        positions[indices[1]].x - com.x,
        positions[indices[1]].y - com.y,
        positions[indices[1]].z - com.z
    };

    // Cross product
    double nx = v1.y * v2.z - v1.z * v2.y;
    double ny = v1.z * v2.x - v1.x * v2.z;
    double nz = v1.x * v2.y - v1.y * v2.x;

    double len = std::sqrt(nx * nx + ny * ny + nz * nz);
    if (len < 1e-30)
        return {0.0, 0.0, 1.0};

    return {nx / len, ny / len, nz / len};
}

} // namespace coarse_grain
