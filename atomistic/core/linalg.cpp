#include "linalg.hpp"
#include <cmath>
#include <algorithm>

namespace atomistic {
namespace linalg {

// Jacobi rotation to eliminate A(p,q) in symmetric matrix
void SVD3::jacobi_rotate(Mat3& A, Mat3& V, int p, int q) {
    if (std::abs(A(p,q)) < 1e-15) return;
    
    double tau = (A(q,q) - A(p,p)) / (2.0 * A(p,q));
    double t = (tau >= 0) ? 1.0/(tau + std::sqrt(1 + tau*tau))
                          : -1.0/(-tau + std::sqrt(1 + tau*tau));
    double c = 1.0 / std::sqrt(1 + t*t);
    double s = t * c;
    
    // Rotate A
    double App = A(p,p), Aqq = A(q,q), Apq = A(p,q);
    A(p,p) = c*c*App - 2*s*c*Apq + s*s*Aqq;
    A(q,q) = s*s*App + 2*s*c*Apq + c*c*Aqq;
    A(p,q) = A(q,p) = 0;
    
    for (int i = 0; i < 3; ++i) {
        if (i != p && i != q) {
            double Aip = A(i,p), Aiq = A(i,q);
            A(i,p) = A(p,i) = c*Aip - s*Aiq;
            A(i,q) = A(q,i) = s*Aip + c*Aiq;
        }
    }
    
    // Accumulate V
    for (int i = 0; i < 3; ++i) {
        double Vip = V(i,p), Viq = V(i,q);
        V(i,p) = c*Vip - s*Viq;
        V(i,q) = s*Vip + c*Viq;
    }
}

// Symmetric eigendecomposition: A_sym = V Λ V^T
void SVD3::eig_jacobi(const Mat3& A_sym, Mat3& V, Vec3& lambda) {
    Mat3 A = A_sym;
    V = Mat3::identity();
    
    // Jacobi iteration (max 50 sweeps)
    for (int sweep = 0; sweep < 50; ++sweep) {
        double off_diag = std::abs(A(0,1)) + std::abs(A(0,2)) + std::abs(A(1,2));
        if (off_diag < 1e-15) break;
        
        jacobi_rotate(A, V, 0, 1);
        jacobi_rotate(A, V, 0, 2);
        jacobi_rotate(A, V, 1, 2);
    }
    
    lambda = {A(0,0), A(1,1), A(2,2)};
}

SVD3::SVD3(const Mat3& A) {
    // Compute A^T A (symmetric positive semi-definite)
    Mat3 AT = A.transpose();
    Mat3 ATA = AT * A;
    
    // Eigendecomposition: A^T A = V Σ² V^T
    Vec3 sigma2;
    eig_jacobi(ATA, V, sigma2);
    
    // Sort singular values descending
    struct IdxVal { int idx; double val; };
    IdxVal sv[3] = {{0, sigma2.x}, {1, sigma2.y}, {2, sigma2.z}};
    std::sort(sv, sv+3, [](const IdxVal& a, const IdxVal& b) { return a.val > b.val; });
    
    // Reorder V columns and compute σ
    Mat3 V_sorted;
    for (int i = 0; i < 3; ++i) {
        V_sorted.m[0*3+i] = V.m[0*3 + sv[i].idx];
        V_sorted.m[1*3+i] = V.m[1*3 + sv[i].idx];
        V_sorted.m[2*3+i] = V.m[2*3 + sv[i].idx];
    }
    V = V_sorted;
    sigma = {std::sqrt(std::max(0.0, sv[0].val)),
             std::sqrt(std::max(0.0, sv[1].val)),
             std::sqrt(std::max(0.0, sv[2].val))};
    
    // Compute U = A V Σ^{-1}
    U = Mat3::identity();
    for (int i = 0; i < 3; ++i) {
        Vec3 v_col = V.col(i);
        Vec3 u_col;
        if (sigma.x > 1e-12 && i == 0) u_col = (A * v_col) * (1.0 / sigma.x);
        else if (sigma.y > 1e-12 && i == 1) u_col = (A * v_col) * (1.0 / sigma.y);
        else if (sigma.z > 1e-12 && i == 2) u_col = (A * v_col) * (1.0 / sigma.z);
        else u_col = {0, 0, 0}; // Null space
        
        U(0,i) = u_col.x;
        U(1,i) = u_col.y;
        U(2,i) = u_col.z;
    }
    
    // Handle rank-deficient case: ensure U is orthogonal
    // For rank-2, compute 3rd column as cross product
    if (sigma.z < 1e-12) {
        Vec3 u0 = U.col(0);
        Vec3 u1 = U.col(1);
        Vec3 u2 = {u0.y*u1.z - u0.z*u1.y,
                   u0.z*u1.x - u0.x*u1.z,
                   u0.x*u1.y - u0.y*u1.x};
        double n = norm(u2);
        if (n > 1e-12) {
            u2 = u2 * (1.0 / n);
            U(0,2) = u2.x;
            U(1,2) = u2.y;
            U(2,2) = u2.z;
        }
    }
}

Mat3 polar_rotation(const Mat3& A) {
    SVD3 svd(A);
    return svd.U * svd.V.transpose();
}

Vec3 rotation_to_euler(const Mat3& R) {
    // ZYX Euler angles (yaw-pitch-roll)
    // R = Rz(α) Ry(β) Rx(γ)
    double beta = std::asin(-R(2,0));
    double alpha, gamma;
    
    if (std::abs(std::cos(beta)) > 1e-6) {
        alpha = std::atan2(R(1,0), R(0,0));
        gamma = std::atan2(R(2,1), R(2,2));
    } else {
        // Gimbal lock
        alpha = std::atan2(-R(0,1), R(1,1));
        gamma = 0;
    }
    
    return {alpha, beta, gamma};
}

Mat3 euler_to_rotation(double alpha, double beta, double gamma) {
    double ca = std::cos(alpha), sa = std::sin(alpha);
    double cb = std::cos(beta),  sb = std::sin(beta);
    double cg = std::cos(gamma), sg = std::sin(gamma);
    
    Mat3 R;
    R(0,0) = ca*cb;
    R(0,1) = ca*sb*sg - sa*cg;
    R(0,2) = ca*sb*cg + sa*sg;
    R(1,0) = sa*cb;
    R(1,1) = sa*sb*sg + ca*cg;
    R(1,2) = sa*sb*cg - ca*sg;
    R(2,0) = -sb;
    R(2,1) = cb*sg;
    R(2,2) = cb*cg;
    
    return R;
}

Mat3 axis_angle_to_rotation(const Vec3& axis, double theta) {
    double n = norm(axis);
    if (n < 1e-12) return Mat3::identity();
    
    Vec3 a = axis * (1.0 / n);
    double c = std::cos(theta);
    double s = std::sin(theta);
    double t = 1.0 - c;
    
    Mat3 R;
    R(0,0) = t*a.x*a.x + c;
    R(0,1) = t*a.x*a.y - s*a.z;
    R(0,2) = t*a.x*a.z + s*a.y;
    R(1,0) = t*a.x*a.y + s*a.z;
    R(1,1) = t*a.y*a.y + c;
    R(1,2) = t*a.y*a.z - s*a.x;
    R(2,0) = t*a.x*a.z - s*a.y;
    R(2,1) = t*a.y*a.z + s*a.x;
    R(2,2) = t*a.z*a.z + c;
    
    return R;
}

} // namespace linalg
} // namespace atomistic
