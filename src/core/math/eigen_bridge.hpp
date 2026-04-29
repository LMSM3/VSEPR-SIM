#pragma once
/*
eigen_bridge.hpp
----------------
Eigen conversion helpers for vsepr::Vec3.

Design contract (WO-056-EIGEN-VEC3-BRIDGE):
  - vsepr::Vec3 is the authoritative project state vector.
  - Eigen::Vector3d is used internally by analysis routines (Kabsch, SVD,
	covariance, rotation fitting, etc.) that need matrix operations.
  - These helpers are the ONLY permitted crossing point between the two
	representations.  They must not be included by state, simulation,
	integrator, or I/O headers.

Include this header ONLY in:
  - src/analysis/         (Kabsch, RMSD, covariance, alignment tools)
  - src/core/stats/       (when a stat helper needs matrix math)
  - Tests that verify the bridge contract

Never include this header in:
  - atomistic/
  - src/io/
  - sim/
  - chem/
  - src/v4/
  - include/sensor/
*/

#include "math_vec3.hpp"          // vsepr::Vec3  (no external deps)
#include <Eigen/Core>             // Eigen::Vector3d, Eigen::Matrix3d
#include <vector>
#include <stdexcept>

namespace vsepr {
namespace eigen_bridge {

// ---------------------------------------------------------------------------
// Scalar conversions
// ---------------------------------------------------------------------------

/// Convert a project-native Vec3 to an Eigen column vector.
inline Eigen::Vector3d to_eigen(const vsepr::Vec3& v) {
	return Eigen::Vector3d{v.x, v.y, v.z};
}

/// Convert an Eigen column vector back to the project-native Vec3.
inline vsepr::Vec3 from_eigen(const Eigen::Vector3d& v) {
	return vsepr::Vec3{v.x(), v.y(), v.z()};
}

// ---------------------------------------------------------------------------
// Batch conversions  (trajectory frames, point clouds)
// ---------------------------------------------------------------------------

/// Convert a frame of Vec3 positions to a vector of Eigen vectors.
inline std::vector<Eigen::Vector3d>
to_eigen_points(const std::vector<vsepr::Vec3>& points) {
	std::vector<Eigen::Vector3d> out;
	out.reserve(points.size());
	for (const auto& p : points) {
		out.push_back(to_eigen(p));
	}
	return out;
}

/// Convert a vector of Eigen vectors back to Vec3 positions.
inline std::vector<vsepr::Vec3>
from_eigen_points(const std::vector<Eigen::Vector3d>& points) {
	std::vector<vsepr::Vec3> out;
	out.reserve(points.size());
	for (const auto& p : points) {
		out.push_back(from_eigen(p));
	}
	return out;
}

/// Pack a Vec3 frame into an Eigen 3×N matrix (column-major, one atom per column).
/// Useful for covariance and SVD routines.
inline Eigen::Matrix3Xd to_eigen_matrix(const std::vector<vsepr::Vec3>& points) {
	const Eigen::Index n = static_cast<Eigen::Index>(points.size());
	Eigen::Matrix3Xd mat(3, n);
	for (Eigen::Index i = 0; i < n; ++i) {
		mat(0, i) = points[static_cast<std::size_t>(i)].x;
		mat(1, i) = points[static_cast<std::size_t>(i)].y;
		mat(2, i) = points[static_cast<std::size_t>(i)].z;
	}
	return mat;
}

/// Unpack an Eigen 3×N matrix back to a Vec3 frame.
inline std::vector<vsepr::Vec3>
from_eigen_matrix(const Eigen::Matrix3Xd& mat) {
	std::vector<vsepr::Vec3> out;
	out.reserve(static_cast<std::size_t>(mat.cols()));
	for (Eigen::Index i = 0; i < mat.cols(); ++i) {
		out.push_back(vsepr::Vec3{mat(0, i), mat(1, i), mat(2, i)});
	}
	return out;
}

// ---------------------------------------------------------------------------
// Centroid helpers  (shared by RMSD and Kabsch)
// ---------------------------------------------------------------------------

/// Compute the centroid of a point set as an Eigen vector.
inline Eigen::Vector3d centroid(const std::vector<vsepr::Vec3>& points) {
	if (points.empty()) {
		return Eigen::Vector3d::Zero();
	}
	Eigen::Vector3d sum = Eigen::Vector3d::Zero();
	for (const auto& p : points) {
		sum += to_eigen(p);
	}
	return sum / static_cast<double>(points.size());
}

} // namespace eigen_bridge
} // namespace vsepr
