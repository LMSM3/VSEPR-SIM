#pragma once
/**
 * kernel_v4.hpp  —  IKK Bilinear Projection Kernel V4
 *
 * Implements the generalised bilinear density projection:
 *
 *   rho(r, t) = R_grid^T * M * e(t)
 *
 * where:
 *   R_grid  in R^{N x n}   — precomputed spatial radial channel matrix
 *   M       in R^{n x m+1} — channel mixing matrix (precomputed)
 *   e(t)    in R^{m+1}     — temporal state vector (updated each step)
 *
 * For the two-channel case (n=2, m=1):
 *   R = [R1^2(r), R2^2(r)]^T
 *   e = [1, ez(t)]
 *   M = (1/2) [[1, 1], [1, -1]]
 *   rho(r,t) = B(r) + ez(t) * D(r)
 *     where B_i = (R1_i^2 + R2_i^2) / 2
 *           D_i = (R1_i^2 - R2_i^2) / 2
 *
 * Runtime cost after presolve:
 *   Full grid:    O(2N)   (BLAS DGEMV)
 *   Single point: O(1)    (scalar dot product)
 *
 * Derived from IKK Doc III — Kernel V4 Bilinear Projection Form
 * (VSEPR-SIM Theoretical Division, Development Day 64)
 *
 * Design rules:
 *   — No virtual dispatch; header-only, trivially copyable payload.
 *   — Two-channel KernelV4 is the production path.
 *   — Multi-channel KernelV4Multi is the generalised engine for n>2, m>1.
 *   — Both live in the ikk:: namespace (separate from vsepr::identity).
 *
 * v5.2.0  |  WO-SM-IDENTITY  |  v5.0.0-main
 */

#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace ikk {

// ============================================================================
// KernelV4  —  Two-channel bilinear projection  (n=2, m=1)
//
// rho(r, t) = B(r) + ez(t) * D(r)
//
// where:
//   B_i = 0.5 * (r1sq[i] + r2sq[i])   — base density (time-independent)
//   D_i = 0.5 * (r1sq[i] - r2sq[i])   — difference density (modulated)
//
// After construction (offline presolve), each timestep costs O(2N).
// ============================================================================

class KernelV4 {
public:
	/// Presolve constructor.
	/// r1sq : R1^2 values on the spatial grid, length N.
	/// r2sq : R2^2 values, same length.
	KernelV4(std::span<const double> r1sq,
			 std::span<const double> r2sq)
	{
		const std::size_t N = r1sq.size();
		rm_.resize(N);
		for (std::size_t i = 0; i < N; ++i) {
			rm_[i][0] = 0.5 * (r1sq[i] + r2sq[i]);  // B_i
			rm_[i][1] = 0.5 * (r1sq[i] - r2sq[i]);  // D_i
		}
	}

	/// Number of grid points N.
	[[nodiscard]] std::size_t grid_size() const noexcept { return rm_.size(); }

	/// Full-grid density evaluation.
	/// rho_out must be pre-allocated with length N.  Cost: O(2N).
	void density(double ez, std::span<double> rho_out) const noexcept {
		for (std::size_t i = 0; i < rm_.size(); ++i)
			rho_out[i] = rm_[i][0] + ez * rm_[i][1];
	}

	/// Single-point lookup.  Cost: O(1).
	[[nodiscard]]
	double density_at(std::size_t i, double ez) const noexcept {
		return rm_[i][0] + ez * rm_[i][1];
	}

	/// Base density at point i (time-independent component).
	[[nodiscard]]
	double base_at(std::size_t i) const noexcept { return rm_[i][0]; }

	/// Difference density at point i (ez-modulated component).
	[[nodiscard]]
	double diff_at(std::size_t i) const noexcept { return rm_[i][1]; }

private:
	/// Cached presolve: rm_[i] = {B_i, D_i}
	std::vector<std::array<double, 2>> rm_;
};

// ============================================================================
// KernelV4Multi  —  Generalised bilinear projection  (n channels, m+1 states)
//
// rho(r, t) = R_grid^T * M * e(t)
//
// After presolve, RM = R_grid * M is cached (N x (m+1) matrix).
// Runtime per point: O(m+1) dot product.
// Runtime full grid: O(N*(m+1)).
//
// Template parameters:
//   N_CHAN   : number of radial channels n
//   N_STATE  : number of temporal state channels m+1
// ============================================================================

template<std::size_t N_CHAN, std::size_t N_STATE>
class KernelV4Multi {
public:
	using MixMatrix    = std::array<std::array<double, N_STATE>, N_CHAN>;
	using StateVector  = std::array<double, N_STATE>;
	using RM_Row       = std::array<double, N_STATE>;

	/// Presolve constructor.
	/// r_channels : N_CHAN vectors of length N (each is R_k^2 on the grid).
	/// M          : channel mixing matrix [N_CHAN][N_STATE]
	KernelV4Multi(const std::vector<std::vector<double>>& r_channels,
				  const MixMatrix& M)
	{
		const std::size_t N = r_channels[0].size();
		rm_.resize(N);
		for (std::size_t i = 0; i < N; ++i) {
			rm_[i].fill(0.0);
			for (std::size_t k = 0; k < N_CHAN; ++k)
				for (std::size_t s = 0; s < N_STATE; ++s)
					rm_[i][s] += r_channels[k][i] * M[k][s];
		}
	}

	[[nodiscard]] std::size_t grid_size() const noexcept { return rm_.size(); }

	/// Full-grid evaluation.  rho_out must have length N.
	void density(const StateVector& e, std::span<double> rho_out) const noexcept {
		for (std::size_t i = 0; i < rm_.size(); ++i) {
			double val = 0.0;
			for (std::size_t s = 0; s < N_STATE; ++s)
				val += rm_[i][s] * e[s];
			rho_out[i] = val;
		}
	}

	/// Single-point lookup.
	[[nodiscard]]
	double density_at(std::size_t i, const StateVector& e) const noexcept {
		double val = 0.0;
		for (std::size_t s = 0; s < N_STATE; ++s)
			val += rm_[i][s] * e[s];
		return val;
	}

private:
	std::vector<RM_Row> rm_;   // rm_[i] = precomputed row i of (R_grid * M)
};

// ============================================================================
// Convenience: standard two-channel mixing matrix M_2
//
//   M_2 = (1/2) * [[1,  1],
//                  [1, -1]]
//
// Maps [1, ez] -> [B_i, D_i] weights as per IKK Doc III eq.(M2).
// ============================================================================

inline KernelV4Multi<2, 2>::MixMatrix mixing_matrix_2ch() noexcept {
	return {{{0.5, 0.5},   // row 0: R1^2 weights for state [1, ez]
			 {0.5,-0.5}}}; // row 1: R2^2 weights
}

} // namespace ikk
