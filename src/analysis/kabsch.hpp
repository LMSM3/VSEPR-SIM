#pragma once
/*
kabsch.hpp
----------
Kabsch alignment and RMSD computation for VSEPR-SIM.

Public API accepts vsepr::Vec3 — the project-native state vector.
All linear algebra runs internally through Eigen (SVD, covariance,
rotation matrix).  No Eigen type leaks across the function boundaries.

WO-056-EIGEN-VEC3-BRIDGE: This is the canonical demonstration that
the Eigen bridge works as intended.  External call sites see only
vsepr::Vec3.  Eigen is an implementation detail.

Usage:
	double rmsd = vsepr::analysis::compute_rmsd(reference, current);

	auto result = vsepr::analysis::kabsch_align(reference, mobile);
	// result.rmsd          — post-alignment RMSD
	// result.rotation      — 3×3 rotation matrix (as vsepr::Vec3[3] rows)
	// result.aligned       — rotated+translated mobile frame (Vec3 vector)

Reference:
	Kabsch, W. (1976). Acta Crystallographica, A32, 922–923.
	Kabsch, W. (1978). Acta Crystallographica, A34, 827–828.
*/

#include "../core/math/eigen_bridge.hpp"   // vsepr::eigen_bridge helpers
#include "../core/math_vec3.hpp"           // vsepr::Vec3
#include <Eigen/Dense>
#include <Eigen/SVD>
#include <vector>
#include <cmath>
#include <stdexcept>
#include <array>

namespace vsepr {
namespace analysis {

// ---------------------------------------------------------------------------
// RMSD — root-mean-square deviation (no alignment)
// ---------------------------------------------------------------------------

/// Compute the RMSD between two point sets without any prior alignment.
/// Throws std::invalid_argument if sizes differ or the set is empty.
inline double compute_rmsd(
	const std::vector<vsepr::Vec3>& reference,
	const std::vector<vsepr::Vec3>& current)
{
	if (reference.size() != current.size()) {
		throw std::invalid_argument(
			"compute_rmsd: reference and current must have the same size");
	}
	if (reference.empty()) {
		throw std::invalid_argument(
			"compute_rmsd: point sets must not be empty");
	}

	double sum_sq = 0.0;
	for (std::size_t i = 0; i < reference.size(); ++i) {
		const vsepr::Vec3 d = reference[i] - current[i];
		sum_sq += d.norm2();
	}
	return std::sqrt(sum_sq / static_cast<double>(reference.size()));
}

// ---------------------------------------------------------------------------
// KabschResult — output of a full alignment
// ---------------------------------------------------------------------------

struct KabschResult {
	double rmsd{0.0};                   ///< post-alignment RMSD
	Eigen::Matrix3d rotation;           ///< optimal rotation (mobile → reference frame)
	Eigen::Vector3d translation;        ///< translation applied before rotation
	std::vector<vsepr::Vec3> aligned;  ///< mobile frame after alignment
};

// ---------------------------------------------------------------------------
// Kabsch alignment
// ---------------------------------------------------------------------------

/// Compute the optimal rotation (Kabsch algorithm) that minimises the RMSD
/// between reference and mobile after centroid superposition.
///
/// Returns a KabschResult with the rotation matrix, translation vector,
/// aligned mobile frame, and the resulting RMSD — all in vsepr types
/// except for the Eigen matrix which is documented as an internal detail.
inline KabschResult kabsch_align(
	const std::vector<vsepr::Vec3>& reference,
	const std::vector<vsepr::Vec3>& mobile)
{
	if (reference.size() != mobile.size()) {
		throw std::invalid_argument(
			"kabsch_align: reference and mobile must have the same size");
	}
	if (reference.size() < 2) {
		throw std::invalid_argument(
			"kabsch_align: at least two points are required");
	}

	using namespace vsepr::eigen_bridge;

	// Step 1: compute centroids and center both frames
	const Eigen::Vector3d ref_c  = centroid(reference);
	const Eigen::Vector3d mob_c  = centroid(mobile);

	const Eigen::Index n = static_cast<Eigen::Index>(reference.size());
	Eigen::Matrix3Xd P(3, n);   // centered reference
	Eigen::Matrix3Xd Q(3, n);   // centered mobile

	for (Eigen::Index i = 0; i < n; ++i) {
		P.col(i) = to_eigen(reference[static_cast<std::size_t>(i)]) - ref_c;
		Q.col(i) = to_eigen(mobile[static_cast<std::size_t>(i)])    - mob_c;
	}

	// Step 2: covariance matrix H = Q * P^T
	const Eigen::Matrix3d H = Q * P.transpose();

	// Step 3: SVD of H
	Eigen::JacobiSVD<Eigen::Matrix3d> svd(H, Eigen::ComputeFullU | Eigen::ComputeFullV);
	const Eigen::Matrix3d& U = svd.matrixU();
	const Eigen::Matrix3d& V = svd.matrixV();

	// Step 4: correct for reflection (det check)
	Eigen::Matrix3d D = Eigen::Matrix3d::Identity();
	if ((V * U.transpose()).determinant() < 0.0) {
		D(2, 2) = -1.0;
	}

	// Step 5: optimal rotation R = V * D * U^T
	const Eigen::Matrix3d R = V * D * U.transpose();

	// Step 6: apply rotation and translation to mobile frame
	KabschResult result;
	result.rotation    = R;
	result.translation = mob_c;   // the translation that centred mobile
	result.aligned.reserve(static_cast<std::size_t>(n));

	for (Eigen::Index i = 0; i < n; ++i) {
		// rotate centred mobile point, then shift to reference centroid
		const Eigen::Vector3d p_aligned = R * Q.col(i) + ref_c;
		result.aligned.push_back(from_eigen(p_aligned));
	}

	// Step 7: RMSD on the aligned frame
	result.rmsd = compute_rmsd(reference, result.aligned);

	return result;
}

// ---------------------------------------------------------------------------
// Convenience: RMSD after optimal alignment (single call)
// ---------------------------------------------------------------------------

/// Compute the minimum achievable RMSD between two point sets by first
/// finding the optimal Kabsch rotation and then measuring the deviation.
inline double kabsch_rmsd(
	const std::vector<vsepr::Vec3>& reference,
	const std::vector<vsepr::Vec3>& mobile)
{
	return kabsch_align(reference, mobile).rmsd;
}

} // namespace analysis
} // namespace vsepr
