#pragma once
/**
 * lattice.hpp
 * -----------
 * Triclinic lattice representation for crystallographic simulation.
 *
 * Core objects:
 *   Mat3       — 3×3 matrix (column-major: columns = lattice vectors)
 *   Lattice    — Full lattice with cached inverse, metric tensor, volume
 *
 * Key operations:
 *   to_cartesian(f)  — r = A · f
 *   to_fractional(r) — f = A⁻¹ · r
 *   mic_delta(fi, fj) — Δf_MIC = Δf - round(Δf), then Δr = A · Δf_MIC
 *   distance(fi, fj)  — ||Δr_MIC||₂  (or via metric tensor G = Aᵀ A)
 *
 * Follows docs/FROM_MOLECULES_TO_MATTER.ipynb formulation exactly.
 */

#include "../core/state.hpp"
#include <cmath>
#include <stdexcept>
#include <array>

namespace atomistic {
namespace crystal {

// ============================================================================
// 3×3 Matrix (column-major: columns are lattice vectors a, b, c)
// ============================================================================

struct Mat3 {
    double m[3][3]{};  // m[row][col]

    // Column access: column 0 = a, 1 = b, 2 = c
    Vec3 col(int j) const { return {m[0][j], m[1][j], m[2][j]}; }
    void set_col(int j, const Vec3& v) { m[0][j] = v.x; m[1][j] = v.y; m[2][j] = v.z; }

    // Row access
    Vec3 row(int i) const { return {m[i][0], m[i][1], m[i][2]}; }

    // Matrix-vector product: r = M · v
    Vec3 mul(const Vec3& v) const {
        return {
            m[0][0]*v.x + m[0][1]*v.y + m[0][2]*v.z,
            m[1][0]*v.x + m[1][1]*v.y + m[1][2]*v.z,
            m[2][0]*v.x + m[2][1]*v.y + m[2][2]*v.z
        };
    }

    // Determinant
    double det() const {
        return m[0][0]*(m[1][1]*m[2][2] - m[1][2]*m[2][1])
             - m[0][1]*(m[1][0]*m[2][2] - m[1][2]*m[2][0])
             + m[0][2]*(m[1][0]*m[2][1] - m[1][1]*m[2][0]);
    }

    // Inverse (throws if singular)
    Mat3 inverse() const;

    // Transpose
    Mat3 transpose() const {
        Mat3 t;
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                t.m[i][j] = m[j][i];
        return t;
    }

    // Matrix-matrix product: C = A * B
    Mat3 operator*(const Mat3& B) const {
        Mat3 C;
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) {
                C.m[i][j] = 0;
                for (int k = 0; k < 3; ++k)
                    C.m[i][j] += m[i][k] * B.m[k][j];
            }
        return C;
    }

    // Identity
    static Mat3 identity() {
        Mat3 I;
        I.m[0][0] = I.m[1][1] = I.m[2][2] = 1.0;
        return I;
    }
};

// ============================================================================
// Lattice: A = [a b c], with cached A⁻¹, G = AᵀA, volume
// ============================================================================

struct Lattice {
    Mat3 A;       // Lattice matrix [a b c] (columns are lattice vectors)
    Mat3 A_inv;   // Cached inverse
    Mat3 G;       // Metric tensor G = Aᵀ A
    double V{};   // Cell volume |det(A)|

    // Default: 1×1×1 cubic (placeholder, will be overwritten)
    Lattice() : Lattice({1,0,0}, {0,1,0}, {0,0,1}) {}

    // Construct from three lattice vectors
    Lattice(const Vec3& a, const Vec3& b, const Vec3& c);

    // Construct from lattice parameters (a, b, c, alpha, beta, gamma in degrees)
    static Lattice from_parameters(double a, double b, double c,
                                   double alpha, double beta, double gamma);

    // Convenience constructors for common crystal systems
    static Lattice cubic(double a);
    static Lattice tetragonal(double a, double c);
    static Lattice orthorhombic(double a, double b, double c);
    static Lattice hexagonal(double a, double c);

    // ========================================================================
    // Coordinate transforms
    // ========================================================================

    // Fractional → Cartesian: r = A · f
    Vec3 to_cartesian(const Vec3& frac) const { return A.mul(frac); }

    // Cartesian → Fractional: f = A⁻¹ · r
    Vec3 to_fractional(const Vec3& cart) const { return A_inv.mul(cart); }

    // ========================================================================
    // Minimum image convention (MIC) for triclinic cells
    // ========================================================================

    // Fractional displacement wrapped to (-0.5, 0.5]
    Vec3 mic_frac_delta(const Vec3& fi, const Vec3& fj) const {
        Vec3 df = fj - fi;
        df.x -= std::round(df.x);
        df.y -= std::round(df.y);
        df.z -= std::round(df.z);
        return df;
    }

    // Cartesian displacement under MIC
    Vec3 mic_delta(const Vec3& fi, const Vec3& fj) const {
        return A.mul(mic_frac_delta(fi, fj));
    }

    // Distance under MIC using metric tensor (no Cartesian intermediate)
    // r² = Δf_MIC^T · G · Δf_MIC
    double distance2_metric(const Vec3& fi, const Vec3& fj) const {
        Vec3 df = mic_frac_delta(fi, fj);
        Vec3 Gdf = G.mul(df);
        return dot(df, Gdf);
    }

    double distance_metric(const Vec3& fi, const Vec3& fj) const {
        return std::sqrt(distance2_metric(fi, fj));
    }

    // Distance via Cartesian (equivalent, but uses A not G)
    double distance(const Vec3& fi, const Vec3& fj) const {
        Vec3 dr = mic_delta(fi, fj);
        return norm(dr);
    }

    // ========================================================================
    // Lattice parameters
    // ========================================================================

    double a_len() const { return norm(A.col(0)); }
    double b_len() const { return norm(A.col(1)); }
    double c_len() const { return norm(A.col(2)); }

    // Angles in degrees
    double alpha_deg() const; // angle between b and c
    double beta_deg()  const; // angle between a and c
    double gamma_deg() const; // angle between a and b

    // Wrap fractional coordinate to [0, 1)
    static Vec3 wrap_frac(const Vec3& f) {
        return {f.x - std::floor(f.x),
                f.y - std::floor(f.y),
                f.z - std::floor(f.z)};
    }

    // Convert BoxPBC for orthogonal compatibility with atomistic::State
    BoxPBC to_box_pbc() const;
};

} // namespace crystal
} // namespace atomistic
