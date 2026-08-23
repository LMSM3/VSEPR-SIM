#pragma once
/**
 * identity_matrix.hpp  -  Plain-aggregate matrix types for the IDENTITY layer
 *
 * Matrix2x2  — 2×2 identity/state matrix used for particle Z-state
 * Matrix3x3  — 3×3 identity matrix for fundamental particles (3-generation space)
 *
 * Convention:
 *   m[row][col], row-major, zero-initialised by default.
 *   All operations are free functions to keep the structs trivially-copyable.
 *
 * Math surface:
 *   trace()          — sum of diagonal elements
 *   frobenius_norm() — sqrt(sum of squares of all elements)
 *   frobenius_dot()  — element-wise inner product tr(A^T B)
 *   opposition()     — 1 - frobenius_dot(A,B) / (norm(A)*norm(B) + eps)
 *                      Maps to kappa_Z in the interaction channel layer.
 *                      0 = identical, 1 = fully opposite, >0 = partial opposition.
 *   mat_mul()        — standard matrix product (for hidden-layer composition)
 *   identity_2x2()   — multiplicative identity
 *   identity_3x3()
 *
 * v5.2.0  |  WO-SM-IDENTITY  |  v5.0.0-main
 */

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace vsepr {
namespace identity {

// ============================================================================
// Matrix2x2
// ============================================================================

struct Matrix2x2 {
	double m[2][2]{};

	double& operator()(int r, int c)       { return m[r][c]; }
	double  operator()(int r, int c) const { return m[r][c]; }

	static Matrix2x2 zero() {
		Matrix2x2 out;
		for (int r = 0; r < 2; ++r)
			for (int c = 0; c < 2; ++c)
				out.m[r][c] = 0.0;
		return out;
	}
};

inline Matrix2x2 identity_2x2() {
	Matrix2x2 M = Matrix2x2::zero();
	M.m[0][0] = 1.0;
	M.m[1][1] = 1.0;
	return M;
}

inline double trace(const Matrix2x2& A) {
	return A.m[0][0] + A.m[1][1];
}

inline double frobenius_norm_sq(const Matrix2x2& A) {
	double s = 0.0;
	for (int r = 0; r < 2; ++r)
		for (int c = 0; c < 2; ++c)
			s += A.m[r][c] * A.m[r][c];
	return s;
}

inline double frobenius_norm(const Matrix2x2& A) {
	return std::sqrt(frobenius_norm_sq(A));
}

// tr(A^T B) = sum_ij A_ij * B_ij
inline double frobenius_dot(const Matrix2x2& A, const Matrix2x2& B) {
	double s = 0.0;
	for (int r = 0; r < 2; ++r)
		for (int c = 0; c < 2; ++c)
			s += A.m[r][c] * B.m[r][c];
	return s;
}

// kappa_Z analogue: 0 = same state, 1 = fully opposite
inline double opposition(const Matrix2x2& A, const Matrix2x2& B, double eps = 1e-15) {
	double dot  = frobenius_dot(A, B);
	double denom = frobenius_norm(A) * frobenius_norm(B) + eps;
	return 1.0 - dot / denom;
}

inline Matrix2x2 mat_mul(const Matrix2x2& A, const Matrix2x2& B) {
	Matrix2x2 C = Matrix2x2::zero();
	for (int r = 0; r < 2; ++r)
		for (int k = 0; k < 2; ++k)
			for (int c = 0; c < 2; ++c)
				C.m[r][c] += A.m[r][k] * B.m[k][c];
	return C;
}

// tr(Z_i * Z_j) / (|Z_i||Z_j| + eps)  — used directly in kappa_Z formula
inline double matrix_compatibility(const Matrix2x2& Zi, const Matrix2x2& Zj, double eps = 1e-15) {
	Matrix2x2 prod = mat_mul(Zi, Zj);
	double num   = trace(prod);
	double denom = frobenius_norm(Zi) * frobenius_norm(Zj) + eps;
	return num / denom;
}

// ============================================================================
// Matrix3x3  — fundamental-particle identity matrix (3-generation space)
// ============================================================================

struct Matrix3x3 {
	double m[3][3]{};

	double& operator()(int r, int c)       { return m[r][c]; }
	double  operator()(int r, int c) const { return m[r][c]; }

	static Matrix3x3 zero() {
		Matrix3x3 out;
		for (int r = 0; r < 3; ++r)
			for (int c = 0; c < 3; ++c)
				out.m[r][c] = 0.0;
		return out;
	}
};

inline Matrix3x3 identity_3x3() {
	Matrix3x3 M = Matrix3x3::zero();
	M.m[0][0] = 1.0;
	M.m[1][1] = 1.0;
	M.m[2][2] = 1.0;
	return M;
}

inline double trace(const Matrix3x3& A) {
	return A.m[0][0] + A.m[1][1] + A.m[2][2];
}

inline double frobenius_norm_sq(const Matrix3x3& A) {
	double s = 0.0;
	for (int r = 0; r < 3; ++r)
		for (int c = 0; c < 3; ++c)
			s += A.m[r][c] * A.m[r][c];
	return s;
}

inline double frobenius_norm(const Matrix3x3& A) {
	return std::sqrt(frobenius_norm_sq(A));
}

inline double frobenius_dot(const Matrix3x3& A, const Matrix3x3& B) {
	double s = 0.0;
	for (int r = 0; r < 3; ++r)
		for (int c = 0; c < 3; ++c)
			s += A.m[r][c] * B.m[r][c];
	return s;
}

inline double opposition(const Matrix3x3& A, const Matrix3x3& B, double eps = 1e-15) {
	double dot   = frobenius_dot(A, B);
	double denom = frobenius_norm(A) * frobenius_norm(B) + eps;
	return 1.0 - dot / denom;
}

inline Matrix3x3 mat_mul(const Matrix3x3& A, const Matrix3x3& B) {
	Matrix3x3 C = Matrix3x3::zero();
	for (int r = 0; r < 3; ++r)
		for (int k = 0; k < 3; ++k)
			for (int c = 0; c < 3; ++c)
				C.m[r][c] += A.m[r][k] * B.m[k][c];
	return C;
}

inline double matrix_compatibility(const Matrix3x3& Zi, const Matrix3x3& Zj, double eps = 1e-15) {
	Matrix3x3 prod = mat_mul(Zi, Zj);
	double num   = trace(prod);
	double denom = frobenius_norm(Zi) * frobenius_norm(Zj) + eps;
	return num / denom;
}

// ============================================================================
// IKK Scale Ladder  (IKK Doc I §2.1, Doc II §B)
// ============================================================================

// Scale level identifiers — use with scale_mask bitmask (bit n = S_n active).
enum class ScaleLevel : uint8_t {
	S0 = 0,   // existence / Planck
	S1 = 1,   // vector / linear permission
	S2 = 2,   // relational / angular / color (gluon)
	S3 = 3,   // spatial / binding (photon / EM)
	S4 = 4,   // time-ordered / weak (W, Z)
	S5 = 5,   // mass-energy curvature (graviton)
	SD = 6,   // dark-scale permission (X_D unknown)
};

inline constexpr uint8_t scale_bit(ScaleLevel s) noexcept {
	return static_cast<uint8_t>(1u << static_cast<unsigned>(s));
}

inline bool scale_active(uint8_t mask, ScaleLevel s) noexcept {
	return (mask & scale_bit(s)) != 0;
}

// ============================================================================
// Fuzz ratio  Phi_S = r_f / ell_S  (IKK Doc I §2.3)
//
// r_fuzz is a caller-supplied dimensionless spread of the identity anchor.
// ell_S is the characteristic length of scale S in VSIM units.
// Defaults are representative (Angstrom-scale for S3, fm-scale for S2).
// ============================================================================

inline double fuzz_ratio(double r_fuzz, ScaleLevel s,
						 double ell_S3_ang = 1.0,    // 1 Angstrom
						 double ell_S2_fm  = 1.0e-5) // 1 fm in Angstrom
{
	double ell = 1.0;
	switch (s) {
		case ScaleLevel::S2: ell = ell_S2_fm;  break;
		case ScaleLevel::S3: ell = ell_S3_ang; break;
		default:             ell = ell_S3_ang; break;
	}
	return r_fuzz / (ell + 1e-300);
}

// ============================================================================
// Hidden intensity  H_j = c_j^T W c_j  for diagonal W = I  (IKK Doc II §D.3)
//
// c_j is a vector of sub-channel values (e.g. quark charges for charge channel).
// With diagonal W = I this reduces to the sum of squares: H = ||c_j||^2.
// ============================================================================

template<std::size_t N>
inline double hidden_intensity_diagonal(const std::array<double, N>& c_j) noexcept {
	double h = 0.0;
	for (auto v : c_j) h += v * v;
	return h;
}

// Three-quark charge hidden intensity — canonical IKK neutron-like example.
// For an (up, down, down) triplet: H = (2/3)^2 + (1/3)^2 + (1/3)^2 = 2/3
inline double quark_charge_hidden_intensity(double q_u, double q_d1, double q_d2) noexcept {
	return q_u * q_u + q_d1 * q_d1 + q_d2 * q_d2;
}

} // namespace identity
} // namespace vsepr
