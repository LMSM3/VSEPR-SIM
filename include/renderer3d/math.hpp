#pragma once

/**
 * renderer3d/math.hpp
 * -------------------
 * Homegrown float 3D math for the VSEPR native renderer.
 *
 * Built from vsepr::Vec3 semantics but stored as floats for GPU-friendly
 * layout and software-rasterizer throughput.  No external dependencies.
 */

#include "core/math_vec3.hpp"

#include <cmath>
#include <cstdint>

namespace vsepr {
namespace renderer3d {

struct Vec3f {
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;

	Vec3f() = default;
	Vec3f(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
	explicit Vec3f(const vsepr::Vec3& v) : x(static_cast<float>(v.x)), y(static_cast<float>(v.y)), z(static_cast<float>(v.z)) {}

	float operator[](int i) const { return (i == 0) ? x : (i == 1) ? y : z; }
	float& operator[](int i) { return (i == 0) ? x : (i == 1) ? y : z; }

	Vec3f operator+(const Vec3f& v) const { return {x + v.x, y + v.y, z + v.z}; }
	Vec3f operator-(const Vec3f& v) const { return {x - v.x, y - v.y, z - v.z}; }
	Vec3f operator*(float s) const { return {x * s, y * s, z * s}; }
	Vec3f operator/(float s) const { return {x / s, y / s, z / s}; }
	Vec3f operator-() const { return {-x, -y, -z}; }

	Vec3f& operator+=(const Vec3f& v) { x += v.x; y += v.y; z += v.z; return *this; }
	Vec3f& operator-=(const Vec3f& v) { x -= v.x; y -= v.y; z -= v.z; return *this; }
	Vec3f& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }
	Vec3f& operator/=(float s) { x /= s; y /= s; z /= s; return *this; }

	[[nodiscard]] float dot(const Vec3f& v) const { return x * v.x + y * v.y + z * v.z; }
	[[nodiscard]] Vec3f cross(const Vec3f& v) const {
		return {y * v.z - z * v.y, z * v.x - x * v.z, x * v.y - y * v.x};
	}
	[[nodiscard]] float norm2() const { return x * x + y * y + z * z; }
	[[nodiscard]] float norm() const { return std::sqrt(norm2()); }
	[[nodiscard]] Vec3f normalized(float eps = 1e-6f) const {
		const float n = norm();
		return n > eps ? (*this / n) : Vec3f{0.0f, 0.0f, 0.0f};
	}
	[[nodiscard]] bool is_zero(float eps = 1e-6f) const { return norm2() < eps * eps; }
};

inline Vec3f operator*(float s, const Vec3f& v) { return v * s; }
inline float dot(const Vec3f& a, const Vec3f& b) { return a.dot(b); }
inline Vec3f cross(const Vec3f& a, const Vec3f& b) { return a.cross(b); }

struct Vec4f {
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
	float w = 1.0f;

	Vec4f() = default;
	Vec4f(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
	explicit Vec4f(const Vec3f& v, float w_ = 1.0f) : x(v.x), y(v.y), z(v.z), w(w_) {}

	float operator[](int i) const { return (&x)[i]; }
	float& operator[](int i) { return (&x)[i]; }

	[[nodiscard]] Vec3f xyz() const { return {x, y, z}; }
	[[nodiscard]] Vec4f operator*(float s) const { return {x * s, y * s, z * s, w * s}; }
};

struct Mat4f {
	// Row-major: m[row * 4 + col]
	float m[16];

	Mat4f();
	explicit Mat4f(float diagonal);

	void identity();

	float operator()(int row, int col) const { return m[row * 4 + col]; }
	float& operator()(int row, int col) { return m[row * 4 + col]; }

	[[nodiscard]] Mat4f operator*(const Mat4f& other) const;
	[[nodiscard]] Vec4f operator*(const Vec4f& v) const;
	[[nodiscard]] Vec3f operator*(const Vec3f& v) const; // treats v as point (w=1)

	[[nodiscard]] Mat4f transposed() const;
	[[nodiscard]] Mat4f inverted() const;

	[[nodiscard]] static Mat4f translate(const Vec3f& t);
	[[nodiscard]] static Mat4f scale(const Vec3f& s);
	[[nodiscard]] static Mat4f rotate_axis(const Vec3f& axis, float radians);
	[[nodiscard]] static Mat4f perspective(float fov_y, float aspect, float near, float far);
	[[nodiscard]] static Mat4f orthographic(float left, float right, float bottom, float top, float near, float far);
	[[nodiscard]] static Mat4f look_at(const Vec3f& eye, const Vec3f& target, const Vec3f& up);
};

struct Color {
	uint8_t r = 0;
	uint8_t g = 0;
	uint8_t b = 0;
	uint8_t a = 255;

	Color() = default;
	Color(uint8_t r_, uint8_t g_, uint8_t b_, uint8_t a_ = 255) : r(r_), g(g_), b(b_), a(a_) {}

	[[nodiscard]] uint32_t bgra() const { return (static_cast<uint32_t>(a) << 24) | (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | b; }
	[[nodiscard]] uint32_t rgba() const { return (static_cast<uint32_t>(a) << 24) | (static_cast<uint32_t>(b) << 16) | (static_cast<uint32_t>(g) << 8) | r; }

	[[nodiscard]] static Color from_float(float r_, float g_, float b_, float a_ = 1.0f);
	[[nodiscard]] static Color lerp(const Color& a, const Color& b, float t);
};

} // namespace renderer3d
} // namespace vsepr
