// =============================================================================
// tests/test_eigen_bridge.cpp — WO-056-EIGEN-VEC3-BRIDGE
// =============================================================================
// Verifies:
//   1. Round-trip: vsepr::Vec3 → Eigen::Vector3d → vsepr::Vec3 preserves
//      x, y, z, norm, and dot-product to double precision.
//   2. Batch round-trip: to_eigen_points / from_eigen_points.
//   3. Matrix round-trip: to_eigen_matrix / from_eigen_matrix.
//   4. Centroid helper produces the correct centroid.
//   5. compute_rmsd returns 0 for identical frames.
//   6. compute_rmsd returns the known value for a shifted frame.
//   7. kabsch_rmsd aligns and produces near-zero RMSD for a rotated frame.
//   8. kabsch_align returns a frame that matches the reference under rotation.
//
// Test vectors cover: zero, unit axes, negatives, large values, tiny values.
// =============================================================================

#include <cassert>
#include <cmath>
#include <vector>
#include <cstdio>

#include "core/math/eigen_bridge.hpp"
#include "analysis/kabsch.hpp"

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

static constexpr double EPS = 1e-10;

static bool near(double a, double b, double tol = EPS) {
	return std::abs(a - b) < tol;
}

static bool vec3_near(const vsepr::Vec3& a, const vsepr::Vec3& b, double tol = EPS) {
	return near(a.x, b.x, tol) && near(a.y, b.y, tol) && near(a.z, b.z, tol);
}

// ---------------------------------------------------------------------------
// Test 1: scalar round-trip correctness
// ---------------------------------------------------------------------------

static void test_scalar_roundtrip() {
	using namespace vsepr::eigen_bridge;

	const std::vector<vsepr::Vec3> cases = {
		{0.0,  0.0,  0.0},                          // zero vector
		{1.0,  0.0,  0.0},                          // unit X
		{0.0,  1.0,  0.0},                          // unit Y
		{0.0,  0.0,  1.0},                          // unit Z
		{-1.0, -1.0, -1.0},                         // negative
		{1e8,  -1e8, 2.5e8},                        // large
		{1e-12, -1e-12, 1e-14},                     // tiny
		{3.14159, -2.71828, 1.41421},               // mixed
	};

	for (const auto& v : cases) {
		const auto ev = to_eigen(v);
		const auto rt = from_eigen(ev);

		assert(vec3_near(v, rt) && "scalar round-trip: x/y/z mismatch");

		// norm preserved
		assert(near(v.norm(), ev.norm()) && "scalar round-trip: norm mismatch");

		// dot with itself preserved
		assert(near(vsepr::dot(v, v), ev.dot(ev)) && "scalar round-trip: dot mismatch");
	}

	// dot between two distinct vectors
	const vsepr::Vec3 a{1.0, 2.0, 3.0};
	const vsepr::Vec3 b{4.0, 5.0, 6.0};
	assert(near(vsepr::dot(a, b), to_eigen(a).dot(to_eigen(b)))
		   && "scalar round-trip: cross-dot mismatch");

	// distance preserved
	const vsepr::Vec3 diff = a - b;
	const double d_native = diff.norm();
	const double d_eigen  = (to_eigen(a) - to_eigen(b)).norm();
	assert(near(d_native, d_eigen) && "scalar round-trip: distance mismatch");

	std::puts("PASS  test_scalar_roundtrip");
}

// ---------------------------------------------------------------------------
// Test 2: batch round-trip
// ---------------------------------------------------------------------------

static void test_batch_roundtrip() {
	using namespace vsepr::eigen_bridge;

	const std::vector<vsepr::Vec3> frame = {
		{1.0, 0.0, 0.0},
		{0.0, 1.0, 0.0},
		{0.0, 0.0, 1.0},
		{-1.5, 2.3, -0.7},
		{0.0, 0.0, 0.0},
		{1e6, -1e6, 1e6},
	};

	const auto ev   = to_eigen_points(frame);
	const auto rt   = from_eigen_points(ev);

	assert(rt.size() == frame.size() && "batch round-trip: size mismatch");
	for (std::size_t i = 0; i < frame.size(); ++i) {
		assert(vec3_near(frame[i], rt[i]) && "batch round-trip: point mismatch");
	}

	std::puts("PASS  test_batch_roundtrip");
}

// ---------------------------------------------------------------------------
// Test 3: matrix round-trip
// ---------------------------------------------------------------------------

static void test_matrix_roundtrip() {
	using namespace vsepr::eigen_bridge;

	const std::vector<vsepr::Vec3> frame = {
		{1.0, 2.0, 3.0},
		{4.0, 5.0, 6.0},
		{-1.0, 0.0, 1.0},
	};

	const auto mat = to_eigen_matrix(frame);
	assert(mat.rows() == 3 && "matrix round-trip: wrong row count");
	assert(mat.cols() == static_cast<Eigen::Index>(frame.size())
		   && "matrix round-trip: wrong column count");

	const auto rt = from_eigen_matrix(mat);
	assert(rt.size() == frame.size() && "matrix round-trip: size mismatch");
	for (std::size_t i = 0; i < frame.size(); ++i) {
		assert(vec3_near(frame[i], rt[i]) && "matrix round-trip: point mismatch");
	}

	std::puts("PASS  test_matrix_roundtrip");
}

// ---------------------------------------------------------------------------
// Test 4: centroid
// ---------------------------------------------------------------------------

static void test_centroid() {
	using namespace vsepr::eigen_bridge;

	// Symmetric tetrahedron-ish: centroid should be ~origin
	const std::vector<vsepr::Vec3> pts = {
		{ 1.0,  1.0,  1.0},
		{-1.0, -1.0,  1.0},
		{-1.0,  1.0, -1.0},
		{ 1.0, -1.0, -1.0},
	};
	const auto c = centroid(pts);
	assert(near(c.x(), 0.0) && near(c.y(), 0.0) && near(c.z(), 0.0)
		   && "centroid: symmetric set should be at origin");

	// Known centroid
	const std::vector<vsepr::Vec3> line = {
		{0.0, 0.0, 0.0},
		{2.0, 4.0, 6.0},
	};
	const auto lc = centroid(line);
	assert(near(lc.x(), 1.0) && near(lc.y(), 2.0) && near(lc.z(), 3.0)
		   && "centroid: known two-point centroid");

	std::puts("PASS  test_centroid");
}

// ---------------------------------------------------------------------------
// Test 5: compute_rmsd — identical frames → 0
// ---------------------------------------------------------------------------

static void test_rmsd_identical() {
	const std::vector<vsepr::Vec3> frame = {
		{1.0, 2.0, 3.0},
		{4.0, 5.0, 6.0},
		{-1.0, 0.0, 1.0},
	};
	const double r = vsepr::analysis::compute_rmsd(frame, frame);
	assert(near(r, 0.0) && "rmsd identical: expected 0");
	std::puts("PASS  test_rmsd_identical");
}

// ---------------------------------------------------------------------------
// Test 6: compute_rmsd — uniform shift → known value
// ---------------------------------------------------------------------------

static void test_rmsd_shifted() {
	// Shift every point by (1,0,0) → RMSD = 1.0
	const std::vector<vsepr::Vec3> ref = {
		{0.0, 0.0, 0.0},
		{1.0, 0.0, 0.0},
		{2.0, 0.0, 0.0},
	};
	std::vector<vsepr::Vec3> shifted = ref;
	for (auto& v : shifted) { v.x += 1.0; }

	const double r = vsepr::analysis::compute_rmsd(ref, shifted);
	assert(near(r, 1.0, 1e-12) && "rmsd shifted: expected 1.0");
	std::puts("PASS  test_rmsd_shifted");
}

// ---------------------------------------------------------------------------
// Test 7: kabsch_rmsd — rotation only → near-zero post-alignment RMSD
// ---------------------------------------------------------------------------

static void test_kabsch_rotation() {
	// Reference: three atoms on the XY plane
	const std::vector<vsepr::Vec3> ref = {
		{ 1.0,  0.0, 0.0},
		{-1.0,  0.0, 0.0},
		{ 0.0,  1.0, 0.0},
	};

	// Mobile: ref rotated 90° around Z axis
	//   (x,y,z) → (-y, x, z)
	std::vector<vsepr::Vec3> mobile;
	for (const auto& v : ref) {
		mobile.push_back({-v.y, v.x, v.z});
	}

	const double r = vsepr::analysis::kabsch_rmsd(ref, mobile);
	// After optimal alignment the RMSD must be near zero
	assert(r < 1e-9 && "kabsch rotation: post-alignment RMSD should be near zero");
	std::puts("PASS  test_kabsch_rotation");
}

// ---------------------------------------------------------------------------
// Test 8: kabsch_align — aligned frame close to reference
// ---------------------------------------------------------------------------

static void test_kabsch_align_frame() {
	const std::vector<vsepr::Vec3> ref = {
		{0.0, 0.0, 0.0},
		{1.0, 0.0, 0.0},
		{0.0, 1.0, 0.0},
		{0.0, 0.0, 1.0},
	};

	// Mobile: ref rotated 45° around Z and translated by (5, -3, 2)
	const double c45 = std::cos(M_PI / 4.0);
	const double s45 = std::sin(M_PI / 4.0);
	std::vector<vsepr::Vec3> mobile;
	for (const auto& v : ref) {
		mobile.push_back({
			c45 * v.x - s45 * v.y + 5.0,
			s45 * v.x + c45 * v.y - 3.0,
			v.z + 2.0
		});
	}

	const auto result = vsepr::analysis::kabsch_align(ref, mobile);
	assert(result.rmsd < 1e-9 && "kabsch align: post-alignment RMSD too large");
	assert(result.aligned.size() == ref.size() && "kabsch align: aligned frame size mismatch");

	for (std::size_t i = 0; i < ref.size(); ++i) {
		assert(vec3_near(result.aligned[i], ref[i], 1e-9)
			   && "kabsch align: aligned point does not match reference");
	}
	std::puts("PASS  test_kabsch_align_frame");
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main() {
	test_scalar_roundtrip();
	test_batch_roundtrip();
	test_matrix_roundtrip();
	test_centroid();
	test_rmsd_identical();
	test_rmsd_shifted();
	test_kabsch_rotation();
	test_kabsch_align_frame();

	std::puts("\nAll Eigen bridge tests passed.");
	return 0;
}
