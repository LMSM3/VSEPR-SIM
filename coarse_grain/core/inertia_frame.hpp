#pragma once
/**
 * inertia_frame.hpp — Local Coordinate Frame from Inertia Tensor
 *
 * Constructs a principal-axis frame for a group of atoms, enabling
 * orientation tracking of anisotropic coarse-grained beads.
 *
 * Implementation:
 *   - 3×3 symmetric inertia tensor from atom positions relative to COM.
 *   - Jacobi eigenvalue iteration (deterministic, no external deps).
 *   - Asphericity metric κ quantifies shape anisotropy.
 *   - Right-handed frame guaranteed by det(R) correction.
 *
 * Anti-black-box: all intermediate values (tensor, eigenvalues, axes)
 * are stored and inspectable.
 *
 * Reference: Section 2 and 3 of section_anisotropic_beads.tex
 */

#include "atomistic/core/state.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace coarse_grain {

/**
 * 3×3 symmetric matrix stored as flat array (row-major).
 */
struct Mat3 {
    double m[9]{};

    double& operator()(int r, int c)       { return m[r * 3 + c]; }
    double  operator()(int r, int c) const { return m[r * 3 + c]; }
};

/**
 * InertiaFrame — principal-axis decomposition of an atom group.
 *
 * Fields are all public for anti-black-box inspection.
 */
struct InertiaFrame {
    Mat3   tensor{};                      // Raw inertia tensor
    double eigenvalues[3]{};              // Principal moments (sorted: I1 ≤ I2 ≤ I3)

    // Principal axes (columns of rotation matrix, orthonormal)
    atomistic::Vec3 axis1{};              // Smallest moment direction
    atomistic::Vec3 axis2{};              // Middle moment direction
    atomistic::Vec3 axis3{};              // Largest moment direction

    double asphericity{};                 // κ = 1 - 3(I1·I2 + I2·I3 + I3·I1) / (I1+I2+I3)²
    bool   valid{false};                  // True if eigendecomposition succeeded

    /**
     * Direction (θ, φ) of vector v in the local principal-axis frame.
     * Returns {theta, phi}.
     */
    std::array<double, 2> local_angles(const atomistic::Vec3& v) const {
        // Project v into local frame
        double lx = v.x * axis1.x + v.y * axis1.y + v.z * axis1.z;
        double ly = v.x * axis2.x + v.y * axis2.y + v.z * axis2.z;
        double lz = v.x * axis3.x + v.y * axis3.y + v.z * axis3.z;

        double r = std::sqrt(lx * lx + ly * ly + lz * lz);
        if (r < 1e-30) return {0.0, 0.0};

        double theta = std::acos(std::clamp(lz / r, -1.0, 1.0));
        double phi   = std::atan2(ly, lx);
        if (phi < 0.0) phi += 2.0 * 3.14159265358979323846;

        return {theta, phi};
    }

    /**
     * Transform a world-space direction into the local frame.
     */
    atomistic::Vec3 to_local(const atomistic::Vec3& v) const {
        return {
            v.x * axis1.x + v.y * axis1.y + v.z * axis1.z,
            v.x * axis2.x + v.y * axis2.y + v.z * axis2.z,
            v.x * axis3.x + v.y * axis3.y + v.z * axis3.z
        };
    }

    /**
     * Transform a local-frame direction into world space.
     */
    atomistic::Vec3 to_world(const atomistic::Vec3& v) const {
        return {
            v.x * axis1.x + v.y * axis2.x + v.z * axis3.x,
            v.x * axis1.y + v.y * axis2.y + v.z * axis3.y,
            v.x * axis1.z + v.y * axis2.z + v.z * axis3.z
        };
    }
};

// ============================================================================
// Jacobi Eigenvalue Iteration for 3×3 Symmetric Matrices
// ============================================================================

namespace detail {

/**
 * Apply a Givens rotation to zero element (p, q) of symmetric matrix A.
 * Updates A in-place and accumulates rotation into V.
 */
inline void jacobi_rotate(Mat3& A, Mat3& V, int p, int q) {
    double app = A(p, p);
    double aqq = A(q, q);
    double apq = A(p, q);

    if (std::abs(apq) < 1e-15) return;

    double tau = (aqq - app) / (2.0 * apq);
    double t;
    if (tau >= 0.0)
        t =  1.0 / ( tau + std::sqrt(1.0 + tau * tau));
    else
        t = -1.0 / (-tau + std::sqrt(1.0 + tau * tau));

    double c = 1.0 / std::sqrt(1.0 + t * t);
    double s = t * c;

    // Update diagonal
    A(p, p) = app - t * apq;
    A(q, q) = aqq + t * apq;
    A(p, q) = 0.0;
    A(q, p) = 0.0;

    // Update off-diagonal rows/cols
    for (int r = 0; r < 3; ++r) {
        if (r == p || r == q) continue;
        double arp = A(r, p);
        double arq = A(r, q);
        A(r, p) = c * arp - s * arq;
        A(p, r) = A(r, p);
        A(r, q) = s * arp + c * arq;
        A(q, r) = A(r, q);
    }

    // Accumulate eigenvectors
    for (int r = 0; r < 3; ++r) {
        double vrp = V(r, p);
        double vrq = V(r, q);
        V(r, p) = c * vrp - s * vrq;
        V(r, q) = s * vrp + c * vrq;
    }
}

} // namespace detail

/**
 * Compute InertiaFrame from a set of atoms relative to a center.
 *
 * @param positions  Atom positions
 * @param masses     Atom masses
 * @param indices    Which atoms to include
 * @param center     Reference point (usually COM)
 */
inline InertiaFrame compute_inertia_frame(
    const std::vector<atomistic::Vec3>& positions,
    const std::vector<double>& masses,
    const std::vector<uint32_t>& indices,
    const atomistic::Vec3& center)
{
    InertiaFrame frame;

    if (indices.empty()) return frame;

    // Build inertia tensor: I_ab = Σ m_i (|r_i|² δ_ab - r_ia · r_ib)
    Mat3& I = frame.tensor;
    for (uint32_t idx : indices) {
        double m = masses[idx];
        double dx = positions[idx].x - center.x;
        double dy = positions[idx].y - center.y;
        double dz = positions[idx].z - center.z;
        double r2 = dx * dx + dy * dy + dz * dz;

        I(0, 0) += m * (r2 - dx * dx);
        I(1, 1) += m * (r2 - dy * dy);
        I(2, 2) += m * (r2 - dz * dz);
        I(0, 1) -= m * dx * dy;
        I(0, 2) -= m * dx * dz;
        I(1, 2) -= m * dy * dz;
    }
    I(1, 0) = I(0, 1);
    I(2, 0) = I(0, 2);
    I(2, 1) = I(1, 2);

    // Jacobi eigenvalue iteration
    Mat3 A = I;
    Mat3 V{};  // Identity
    V(0, 0) = 1.0; V(1, 1) = 1.0; V(2, 2) = 1.0;

    constexpr int MAX_ITER = 50;
    for (int iter = 0; iter < MAX_ITER; ++iter) {
        // Find largest off-diagonal element
        double max_off = 0.0;
        int p = 0, q = 1;
        for (int i = 0; i < 3; ++i) {
            for (int j = i + 1; j < 3; ++j) {
                if (std::abs(A(i, j)) > max_off) {
                    max_off = std::abs(A(i, j));
                    p = i; q = j;
                }
            }
        }
        if (max_off < 1e-14) break;
        detail::jacobi_rotate(A, V, p, q);
    }

    // Extract eigenvalues (diagonal of A)
    double evals[3] = { A(0, 0), A(1, 1), A(2, 2) };

    // Sort eigenvalues ascending and reorder eigenvectors
    int order[3] = {0, 1, 2};
    if (evals[order[0]] > evals[order[1]]) std::swap(order[0], order[1]);
    if (evals[order[1]] > evals[order[2]]) std::swap(order[1], order[2]);
    if (evals[order[0]] > evals[order[1]]) std::swap(order[0], order[1]);

    frame.eigenvalues[0] = evals[order[0]];
    frame.eigenvalues[1] = evals[order[1]];
    frame.eigenvalues[2] = evals[order[2]];

    frame.axis1 = { V(0, order[0]), V(1, order[0]), V(2, order[0]) };
    frame.axis2 = { V(0, order[1]), V(1, order[1]), V(2, order[1]) };
    frame.axis3 = { V(0, order[2]), V(1, order[2]), V(2, order[2]) };

    // Ensure right-handed frame: if det(R) < 0, flip axis3
    double det = frame.axis1.x * (frame.axis2.y * frame.axis3.z - frame.axis2.z * frame.axis3.y)
               - frame.axis1.y * (frame.axis2.x * frame.axis3.z - frame.axis2.z * frame.axis3.x)
               + frame.axis1.z * (frame.axis2.x * frame.axis3.y - frame.axis2.y * frame.axis3.x);
    if (det < 0.0) {
        frame.axis3.x = -frame.axis3.x;
        frame.axis3.y = -frame.axis3.y;
        frame.axis3.z = -frame.axis3.z;
    }

    // Asphericity: κ = 1 - 3(I1·I2 + I2·I3 + I3·I1) / (I1+I2+I3)²
    double I1 = frame.eigenvalues[0];
    double I2 = frame.eigenvalues[1];
    double I3 = frame.eigenvalues[2];
    double trace = I1 + I2 + I3;

    if (trace > 1e-30) {
        double cross = I1 * I2 + I2 * I3 + I3 * I1;
        frame.asphericity = 1.0 - 3.0 * cross / (trace * trace);
    } else {
        frame.asphericity = 0.0;
    }

    frame.valid = true;
    return frame;
}

} // namespace coarse_grain
