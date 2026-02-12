#pragma once
#include "state.hpp"
#include <cmath>
#include <algorithm>
#include <array>

namespace atomistic {
namespace linalg {

/**
 * 3x3 Matrix utilities for molecular alignment and transformations
 * 
 * Physics foundation:
 * - Kabsch algorithm: Optimal rotation R minimizing RMSD = sqrt(Σ|R·xi - yi|²/N)
 * - Uses SVD: H = U Σ V^T where H = Σ(xi ⊗ yi) is covariance matrix
 * - Optimal rotation: R = V·U^T (with det correction for chirality)
 * 
 * References:
 * - Kabsch, W. (1976). Acta Cryst. A32, 922-923
 * - Kabsch, W. (1978). Acta Cryst. A34, 827-828
 */

// 3x3 matrix stored in row-major order
struct Mat3 {
    double m[9];
    
    Mat3() { for (int i = 0; i < 9; ++i) m[i] = 0; }
    
    static Mat3 identity() {
        Mat3 I;
        I.m[0] = I.m[4] = I.m[8] = 1.0;
        return I;
    }
    
    static Mat3 zero() { return Mat3(); }
    
    double& operator()(int i, int j) { return m[i*3 + j]; }
    const double& operator()(int i, int j) const { return m[i*3 + j]; }
    
    // Get column as Vec3
    Vec3 col(int j) const {
        return {m[0*3+j], m[1*3+j], m[2*3+j]};
    }
    
    // Get row as Vec3
    Vec3 row(int i) const {
        return {m[i*3+0], m[i*3+1], m[i*3+2]};
    }
    
    // Matrix-vector product
    Vec3 operator*(const Vec3& v) const {
        return {
            m[0]*v.x + m[1]*v.y + m[2]*v.z,
            m[3]*v.x + m[4]*v.y + m[5]*v.z,
            m[6]*v.x + m[7]*v.y + m[8]*v.z
        };
    }
    
    // Matrix-matrix product
    Mat3 operator*(const Mat3& B) const {
        Mat3 C;
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                C(i,j) = 0;
                for (int k = 0; k < 3; ++k) {
                    C(i,j) += (*this)(i,k) * B(k,j);
                }
            }
        }
        return C;
    }
    
    // Transpose
    Mat3 transpose() const {
        Mat3 T;
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                T(i,j) = (*this)(j,i);
        return T;
    }
    
    // Determinant
    double det() const {
        return m[0]*(m[4]*m[8] - m[5]*m[7])
             - m[1]*(m[3]*m[8] - m[5]*m[6])
             + m[2]*(m[3]*m[7] - m[4]*m[6]);
    }
    
    // Frobenius norm
    double fnorm() const {
        double sum = 0;
        for (int i = 0; i < 9; ++i) sum += m[i]*m[i];
        return std::sqrt(sum);
    }
};

/**
 * SVD decomposition for 3x3 matrix: A = U Σ V^T
 * Uses Jacobi iteration for symmetric eigenvalue problem
 * 
 * Algorithm:
 * 1. Form A^T A (symmetric 3x3)
 * 2. Diagonalize via Jacobi rotations → V and Σ²
 * 3. Compute U = A V Σ^{-1}
 * 
 * Accuracy: ~1e-12 for well-conditioned matrices
 */
struct SVD3 {
    Mat3 U;      // Left singular vectors (3x3 orthogonal)
    Vec3 sigma;  // Singular values (σ₁ ≥ σ₂ ≥ σ₃ ≥ 0)
    Mat3 V;      // Right singular vectors (3x3 orthogonal)
    
    SVD3() = default;
    
    // Compute SVD of 3x3 matrix A
    explicit SVD3(const Mat3& A);
    
private:
    // Jacobi rotation to zero out A(p,q)
    static void jacobi_rotate(Mat3& A, Mat3& V, int p, int q);
    
    // Symmetric eigendecomposition via Jacobi
    static void eig_jacobi(const Mat3& A_sym, Mat3& V, Vec3& lambda);
};

/**
 * Polar decomposition: A = R S where R is rotation, S is symmetric
 * Uses SVD: R = U V^T
 * Returns rotation matrix (proper or improper depending on det(A))
 */
Mat3 polar_rotation(const Mat3& A);

/**
 * Extract Euler angles (ZYX convention) from rotation matrix
 * Returns {α, β, γ} in radians
 */
Vec3 rotation_to_euler(const Mat3& R);

/**
 * Construct rotation matrix from Euler angles (ZYX convention)
 */
Mat3 euler_to_rotation(double alpha, double beta, double gamma);

/**
 * Rodrigues' rotation formula: rotate vector v by angle θ around axis
 * R = I + sin(θ)K + (1-cos(θ))K² where K is skew-symmetric matrix of axis
 */
Mat3 axis_angle_to_rotation(const Vec3& axis, double theta);

} // namespace linalg
} // namespace atomistic
