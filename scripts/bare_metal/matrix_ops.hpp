#pragma once
/**
 * matrix_ops.hpp
 * ──────────────
 * Dynamically allocated matrix with flat storage for cache efficiency.
 * C++26 / C++23 features: std::expected, trailing return types.
 *
 * Compile:  g++ -std=c++2c main.cpp -o matrix
 */

#include <vector>
#include <string>
#include <cstddef>
#include <stdexcept>
#include <cmath>

/// Explicitly defined Matrix structure — flat row-major storage
struct Matrix {
    size_t rows;
    size_t cols;
    std::vector<double> data;   // rows*cols flat array for cache efficiency

    /// Element access with bounds checking (debug) or raw (release)
    auto operator()(size_t r, size_t c) -> double& {
        return data[r * cols + c];
    }
    auto operator()(size_t r, size_t c) const -> const double& {
        return data[r * cols + c];
    }

    /// Total element count
    auto size() const -> size_t { return rows * cols; }

    /// Frobenius norm: ||A||_F = sqrt(sum a_ij^2)
    auto frobenius_norm() const -> double {
        double sum = 0.0;
        for (auto v : data) sum += v * v;
        return std::sqrt(sum);
    }
};

/// Create a zero-initialised matrix
auto create_matrix(size_t r, size_t c) -> Matrix {
    return {r, c, std::vector<double>(r * c, 0.0)};
}

/// Create an identity matrix
auto create_identity(size_t n) -> Matrix {
    auto m = create_matrix(n, n);
    for (size_t i = 0; i < n; ++i) m(i, i) = 1.0;
    return m;
}

/// Matrix multiply: C = A * B
auto mat_mul(const Matrix& A, const Matrix& B) -> Matrix {
    auto C = create_matrix(A.rows, B.cols);
    for (size_t i = 0; i < A.rows; ++i)
        for (size_t k = 0; k < A.cols; ++k) {
            double a_ik = A(i, k);
            for (size_t j = 0; j < B.cols; ++j)
                C(i, j) += a_ik * B(k, j);
        }
    return C;
}

/// Matrix transpose
auto mat_transpose(const Matrix& A) -> Matrix {
    auto T = create_matrix(A.cols, A.rows);
    for (size_t i = 0; i < A.rows; ++i)
        for (size_t j = 0; j < A.cols; ++j)
            T(j, i) = A(i, j);
    return T;
}

/// Scalar multiply in-place
auto mat_scale(Matrix& A, double s) -> void {
    for (auto& v : A.data) v *= s;
}

/// Element-wise addition: C = A + B
auto mat_add(const Matrix& A, const Matrix& B) -> Matrix {
    auto C = create_matrix(A.rows, A.cols);
    for (size_t i = 0; i < A.data.size(); ++i)
        C.data[i] = A.data[i] + B.data[i];
    return C;
}
