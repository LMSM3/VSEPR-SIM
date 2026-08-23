#include "renderer3d/math.hpp"

#include <algorithm>
#include <cstring>

namespace vsepr {
namespace renderer3d {

Mat4f::Mat4f() { identity(); }

Mat4f::Mat4f(float diagonal) {
	identity();
	for (int i = 0; i < 4; ++i) m[i * 4 + i] = diagonal;
}

void Mat4f::identity() {
	for (int i = 0; i < 16; ++i) m[i] = 0.0f;
	for (int i = 0; i < 4; ++i) m[i * 4 + i] = 1.0f;
}

Mat4f Mat4f::operator*(const Mat4f& other) const {
	Mat4f result;
	for (int r = 0; r < 4; ++r) {
		for (int c = 0; c < 4; ++c) {
			float sum = 0.0f;
			for (int k = 0; k < 4; ++k) sum += m[r * 4 + k] * other.m[k * 4 + c];
			result.m[r * 4 + c] = sum;
		}
	}
	return result;
}

Vec4f Mat4f::operator*(const Vec4f& v) const {
	return {
		m[0] * v.x + m[1] * v.y + m[2] * v.z + m[3] * v.w,
		m[4] * v.x + m[5] * v.y + m[6] * v.z + m[7] * v.w,
		m[8] * v.x + m[9] * v.y + m[10] * v.z + m[11] * v.w,
		m[12] * v.x + m[13] * v.y + m[14] * v.z + m[15] * v.w,
	};
}

Vec3f Mat4f::operator*(const Vec3f& v) const {
	const float x = m[0] * v.x + m[1] * v.y + m[2] * v.z + m[3];
	const float y = m[4] * v.x + m[5] * v.y + m[6] * v.z + m[7];
	const float z = m[8] * v.x + m[9] * v.y + m[10] * v.z + m[11];
	const float w = m[12] * v.x + m[13] * v.y + m[14] * v.z + m[15];
	const float inv_w = w != 0.0f ? 1.0f / w : 1.0f;
	return {x * inv_w, y * inv_w, z * inv_w};
}

Mat4f Mat4f::transposed() const {
	Mat4f result;
	for (int r = 0; r < 4; ++r) {
		for (int c = 0; c < 4; ++c) {
			result.m[c * 4 + r] = m[r * 4 + c];
		}
	}
	return result;
}

Mat4f Mat4f::inverted() const {
	// Gauss-Jordan elimination on an augmented 4x8 matrix.
	float a[4][8];
	for (int r = 0; r < 4; ++r) {
		for (int c = 0; c < 4; ++c) a[r][c] = m[r * 4 + c];
		for (int c = 0; c < 4; ++c) a[r][4 + c] = (r == c) ? 1.0f : 0.0f;
	}

	for (int col = 0; col < 4; ++col) {
		int pivot = col;
		for (int row = col + 1; row < 4; ++row) {
			if (std::abs(a[row][col]) > std::abs(a[pivot][col])) pivot = row;
		}
		if (std::abs(a[pivot][col]) < 1e-12f) {
			Mat4f identity;
			return identity;
		}
		if (pivot != col) {
			for (int k = 0; k < 8; ++k) std::swap(a[col][k], a[pivot][k]);
		}
		const float div = a[col][col];
		for (int k = 0; k < 8; ++k) a[col][k] /= div;
		for (int row = 0; row < 4; ++row) {
			if (row == col) continue;
			const float factor = a[row][col];
			for (int k = 0; k < 8; ++k) a[row][k] -= factor * a[col][k];
		}
	}

	Mat4f result;
	for (int r = 0; r < 4; ++r) {
		for (int c = 0; c < 4; ++c) result.m[r * 4 + c] = a[r][4 + c];
	}
	return result;
}

Mat4f Mat4f::translate(const Vec3f& t) {
	Mat4f m;
	m.m[3] = t.x;
	m.m[7] = t.y;
	m.m[11] = t.z;
	return m;
}

Mat4f Mat4f::scale(const Vec3f& s) {
	Mat4f m;
	m.m[0] = s.x;
	m.m[5] = s.y;
	m.m[10] = s.z;
	m.m[15] = 1.0f;
	return m;
}

Mat4f Mat4f::rotate_axis(const Vec3f& axis, float radians) {
	const auto u = axis.normalized();
	const float c = std::cos(radians);
	const float s = std::sin(radians);
	const float t = 1.0f - c;
	Mat4f m;
	m.m[0] = t * u.x * u.x + c;
	m.m[1] = t * u.x * u.y - s * u.z;
	m.m[2] = t * u.x * u.z + s * u.y;
	m.m[3] = 0.0f;
	m.m[4] = t * u.x * u.y + s * u.z;
	m.m[5] = t * u.y * u.y + c;
	m.m[6] = t * u.y * u.z - s * u.x;
	m.m[7] = 0.0f;
	m.m[8] = t * u.x * u.z - s * u.y;
	m.m[9] = t * u.y * u.z + s * u.x;
	m.m[10] = t * u.z * u.z + c;
	m.m[11] = 0.0f;
	m.m[12] = 0.0f;
	m.m[13] = 0.0f;
	m.m[14] = 0.0f;
	m.m[15] = 1.0f;
	return m;
}

Mat4f Mat4f::perspective(float fov_y, float aspect, float near, float far) {
	Mat4f result;
	const float f = 1.0f / std::tan(fov_y * 0.5f);
	const float nf = 1.0f / (near - far);
	result.m[0] = f / aspect;
	result.m[5] = f;
	result.m[10] = (far + near) * nf;
	result.m[11] = 2.0f * far * near * nf;
	result.m[14] = -1.0f;
	result.m[15] = 0.0f;
	return result;
}

Mat4f Mat4f::orthographic(float left, float right, float bottom, float top, float near, float far) {
	Mat4f result;
	result.m[0] = 2.0f / (right - left);
	result.m[3] = -(right + left) / (right - left);
	result.m[5] = 2.0f / (top - bottom);
	result.m[7] = -(top + bottom) / (top - bottom);
	result.m[10] = 2.0f / (near - far);
	result.m[11] = -(near + far) / (near - far);
	result.m[15] = 1.0f;
	return result;
}

Mat4f Mat4f::look_at(const Vec3f& eye, const Vec3f& target, const Vec3f& up) {
	const auto f = (target - eye).normalized();
	const auto s = f.cross(up).normalized();
	const auto u = s.cross(f);
	Mat4f result;
	result.m[0] = s.x;  result.m[1] = s.y;  result.m[2] = s.z;  result.m[3] = -s.dot(eye);
	result.m[4] = u.x;  result.m[5] = u.y;  result.m[6] = u.z;  result.m[7] = -u.dot(eye);
	result.m[8] = -f.x; result.m[9] = -f.y; result.m[10] = -f.z; result.m[11] = f.dot(eye);
	result.m[12] = 0.0f; result.m[13] = 0.0f; result.m[14] = 0.0f; result.m[15] = 1.0f;
	return result;
}

Color Color::from_float(float r_, float g_, float b_, float a_) {
	auto clamp = [](float v) -> uint8_t {
		if (v < 0.0f) return 0;
		if (v > 1.0f) return 255;
		return static_cast<uint8_t>(v * 255.0f + 0.5f);
	};
	return {clamp(r_), clamp(g_), clamp(b_), clamp(a_)};
}

Color Color::lerp(const Color& a, const Color& b, float t) {
	auto l = [](uint8_t u, uint8_t v, float t_) {
		return static_cast<uint8_t>(std::clamp((1.0f - t_) * u + t_ * v + 0.5f, 0.0f, 255.0f));
	};
	return {l(a.r, b.r, t), l(a.g, b.g, t), l(a.b, b.b, t), l(a.a, b.a, t)};
}

} // namespace renderer3d
} // namespace vsepr
				