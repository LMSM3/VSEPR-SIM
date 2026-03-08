#include "lattice.hpp"
#include <cmath>
#include <stdexcept>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace atomistic {
namespace crystal {

// ============================================================================
// Mat3
// ============================================================================

Mat3 Mat3::inverse() const {
    double d = det();
    if (std::abs(d) < 1e-15)
        throw std::runtime_error("Lattice matrix is singular (det ≈ 0)");

    Mat3 inv;
    double inv_d = 1.0 / d;

    inv.m[0][0] =  (m[1][1]*m[2][2] - m[1][2]*m[2][1]) * inv_d;
    inv.m[0][1] = -(m[0][1]*m[2][2] - m[0][2]*m[2][1]) * inv_d;
    inv.m[0][2] =  (m[0][1]*m[1][2] - m[0][2]*m[1][1]) * inv_d;
    inv.m[1][0] = -(m[1][0]*m[2][2] - m[1][2]*m[2][0]) * inv_d;
    inv.m[1][1] =  (m[0][0]*m[2][2] - m[0][2]*m[2][0]) * inv_d;
    inv.m[1][2] = -(m[0][0]*m[1][2] - m[0][2]*m[1][0]) * inv_d;
    inv.m[2][0] =  (m[1][0]*m[2][1] - m[1][1]*m[2][0]) * inv_d;
    inv.m[2][1] = -(m[0][0]*m[2][1] - m[0][1]*m[2][0]) * inv_d;
    inv.m[2][2] =  (m[0][0]*m[1][1] - m[0][1]*m[1][0]) * inv_d;

    return inv;
}

// ============================================================================
// Lattice
// ============================================================================

Lattice::Lattice(const Vec3& a, const Vec3& b, const Vec3& c) {
    A.set_col(0, a);
    A.set_col(1, b);
    A.set_col(2, c);

    V = std::abs(A.det());
    if (V < 1e-15)
        throw std::runtime_error("Degenerate lattice (volume ≈ 0)");

    A_inv = A.inverse();
    G = A.transpose() * A;  // Metric tensor
}

Lattice Lattice::from_parameters(double a, double b, double c,
                                 double alpha, double beta, double gamma) {
    // Convert degrees to radians
    constexpr double deg2rad = M_PI / 180.0;
    double al = alpha * deg2rad;
    double be = beta  * deg2rad;
    double ga = gamma * deg2rad;

    // Standard crystallographic convention:
    // a along x, b in xy-plane, c general
    double cos_al = std::cos(al);
    double cos_be = std::cos(be);
    double cos_ga = std::cos(ga);
    double sin_ga = std::sin(ga);

    Vec3 va = {a, 0, 0};
    Vec3 vb = {b * cos_ga, b * sin_ga, 0};

    double cx = c * cos_be;
    double cy = c * (cos_al - cos_be * cos_ga) / sin_ga;
    double cz = std::sqrt(c*c - cx*cx - cy*cy);
    Vec3 vc = {cx, cy, cz};

    return Lattice(va, vb, vc);
}

Lattice Lattice::cubic(double a) {
    return Lattice({a, 0, 0}, {0, a, 0}, {0, 0, a});
}

Lattice Lattice::tetragonal(double a, double c) {
    return Lattice({a, 0, 0}, {0, a, 0}, {0, 0, c});
}

Lattice Lattice::orthorhombic(double a, double b, double c) {
    return Lattice({a, 0, 0}, {0, b, 0}, {0, 0, c});
}

Lattice Lattice::hexagonal(double a, double c) {
    // a along x, b at 120° from a in xy-plane
    return from_parameters(a, a, c, 90.0, 90.0, 120.0);
}

double Lattice::alpha_deg() const {
    Vec3 bv = A.col(1), cv = A.col(2);
    double bl = norm(bv), cl = norm(cv);
    return std::acos(dot(bv, cv) / (bl * cl)) * 180.0 / M_PI;
}

double Lattice::beta_deg() const {
    Vec3 av = A.col(0), cv = A.col(2);
    double al = norm(av), cl = norm(cv);
    return std::acos(dot(av, cv) / (al * cl)) * 180.0 / M_PI;
}

double Lattice::gamma_deg() const {
    Vec3 av = A.col(0), bv = A.col(1);
    double al = norm(av), bl = norm(bv);
    return std::acos(dot(av, bv) / (al * bl)) * 180.0 / M_PI;
}

BoxPBC Lattice::to_box_pbc() const {
    // Only valid for orthogonal cells
    Vec3 av = A.col(0), bv = A.col(1), cv = A.col(2);
    return BoxPBC(norm(av), norm(bv), norm(cv));
}

} // namespace crystal
} // namespace atomistic
