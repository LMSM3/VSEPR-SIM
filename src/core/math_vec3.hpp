#pragma once
/*
math_vec3.hpp
-------------
Minimal 3D vector math for VSEPR sim.

Design goals:
- deterministic, compiler-agnostic
- no external dependencies
- efficient for small molecules (no SIMD overkill)
*/

#include <cmath>
#include <algorithm>

namespace vsepr {

struct Vec3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    Vec3() = default;
    Vec3(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}

    // Element access
    double& operator[](int i) {
        return (i == 0) ? x : (i == 1) ? y : z;
    }
    double operator[](int i) const {
        return (i == 0) ? x : (i == 1) ? y : z;
    }

    // Addition
    Vec3 operator+(const Vec3& v) const {
        return {x + v.x, y + v.y, z + v.z};
    }
    Vec3& operator+=(const Vec3& v) {
        x += v.x; y += v.y; z += v.z;
        return *this;
    }

    // Subtraction
    Vec3 operator-(const Vec3& v) const {
        return {x - v.x, y - v.y, z - v.z};
    }
    Vec3& operator-=(const Vec3& v) {
        x -= v.x; y -= v.y; z -= v.z;
        return *this;
    }

    // Scalar multiplication
    Vec3 operator*(double s) const {
        return {x * s, y * s, z * s};
    }
    Vec3& operator*=(double s) {
        x *= s; y *= s; z *= s;
        return *this;
    }

    // Scalar division
    Vec3 operator/(double s) const {
        return {x / s, y / s, z / s};
    }
    Vec3& operator/=(double s) {
        x /= s; y /= s; z /= s;
        return *this;
    }

    // Unary minus
    Vec3 operator-() const {
        return {-x, -y, -z};
    }

    // Dot product
    double dot(const Vec3& v) const {
        return x * v.x + y * v.y + z * v.z;
    }

    // Cross product
    Vec3 cross(const Vec3& v) const {
        return {
            y * v.z - z * v.y,
            z * v.x - x * v.z,
            x * v.y - y * v.x
        };
    }

    // Squared norm (avoids sqrt when only comparing)
    double norm2() const {
        return x*x + y*y + z*z;
    }

    // Euclidean norm
    double norm() const {
        return std::sqrt(norm2());
    }

    // Normalize (returns zero vector if norm too small)
    Vec3 normalized(double eps = 1e-12) const {
        double n = norm();
        return (n > eps) ? (*this / n) : Vec3{0, 0, 0};
    }

    // In-place normalize
    Vec3& normalize(double eps = 1e-12) {
        double n = norm();
        if (n > eps) {
            *this /= n;
        } else {
            x = y = z = 0.0;
        }
        return *this;
    }

    // Zero check
    bool is_zero(double eps = 1e-12) const {
        return norm2() < eps * eps;
    }
};

// Scalar * Vec3
inline Vec3 operator*(double s, const Vec3& v) {
    return v * s;
}

// Standalone dot product
inline double dot(const Vec3& a, const Vec3& b) {
    return a.dot(b);
}

// Standalone cross product
inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return a.cross(b);
}

// Standalone norm
inline double norm(const Vec3& v) {
    return v.norm();
}

// Standalone norm squared
inline double norm2(const Vec3& v) {
    return v.norm2();
}

} // namespace vsepr
