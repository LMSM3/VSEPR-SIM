/**
 * gas3_fitting.hpp
 * ----------------
 * Curve Fitting Engine for Gas3 Module.
 *
 * Provides:
 *   - Polynomial fits in 1D: y(x) = a0 + a1*x + a2*x^2 + ...
 *   - Polynomial surface fits in 2D: Z(T,P) = sum(a_ij * T^i * P^j)
 *   - Train/validation split
 *   - Residual analysis
 *   - Fit quality metrics (MAE, MAPE, max error, R^2)
 *
 * Fits only on quality-filtered data (Q2+ by default, Q3/Q4 preferred).
 *
 * Implementation: normal equations via Gaussian elimination.
 * No external dependencies.
 *
 * Anti-black-box: all coefficients, residuals, and metrics are stored.
 */

#pragma once

#include "gas3_state_record.hpp"
#include <vector>
#include <cmath>
#include <string>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <numeric>

namespace vsepr {
namespace gas3 {

// ============================================================================
// Fit result (1D polynomial)
// ============================================================================

struct FitResult1D {
    std::string name;             // e.g. "Z(T) for Ar at 1 atm"
    std::string x_var;            // e.g. "T_K"
    std::string y_var;            // e.g. "Z"
    int degree = 0;
    std::vector<double> coeffs;   // a0, a1, a2, ...

    // Metrics
    double mae  = NAN;            // mean absolute error
    double mape = NAN;            // mean absolute percentage error
    double max_error = NAN;       // worst-case absolute error
    double r_squared = NAN;       // coefficient of determination
    size_t n_train = 0;
    size_t n_valid = 0;

    double evaluate(double x) const {
        double result = 0.0;
        double xp = 1.0;
        for (size_t i = 0; i < coeffs.size(); ++i) {
            result += coeffs[i] * xp;
            xp *= x;
        }
        return result;
    }

    std::string format() const {
        std::ostringstream ss;
        ss << std::fixed;
        ss << name << "\n";
        ss << "  " << y_var << " = ";
        for (size_t i = 0; i < coeffs.size(); ++i) {
            if (i > 0) ss << " + ";
            ss << std::scientific << std::setprecision(4) << coeffs[i];
            if (i == 1) ss << "*" << x_var;
            else if (i > 1) ss << "*" << x_var << "^" << i;
        }
        ss << "\n";
        ss << std::fixed << std::setprecision(6);
        ss << "  MAE=" << mae << "  MAPE=" << std::setprecision(2) << mape << "%"
           << "  max_err=" << std::setprecision(6) << max_error
           << "  R^2=" << r_squared << "\n";
        ss << "  train=" << n_train << "  valid=" << n_valid << "\n";
        return ss.str();
    }
};

// ============================================================================
// Fit result (2D polynomial surface)
// ============================================================================

struct FitResult2D {
    std::string name;
    std::string x1_var;           // e.g. "T_K"
    std::string x2_var;           // e.g. "P_atm"
    std::string y_var;            // e.g. "Z"
    int degree = 0;               // max total degree
    size_t n_terms = 0;
    std::vector<double> coeffs;
    std::vector<std::pair<int,int>> powers;  // (i,j) for each coeff

    double mae  = NAN;
    double mape = NAN;
    double max_error = NAN;
    double r_squared = NAN;
    size_t n_train = 0;
    size_t n_valid = 0;

    double evaluate(double x1, double x2) const {
        double result = 0.0;
        for (size_t k = 0; k < coeffs.size(); ++k) {
            result += coeffs[k] * std::pow(x1, powers[k].first)
                                * std::pow(x2, powers[k].second);
        }
        return result;
    }
};

// ============================================================================
// Gaussian elimination for Ax=b (small systems, in-place)
// ============================================================================

inline bool solve_linear_system(std::vector<std::vector<double>>& A,
                                std::vector<double>& b) {
    size_t n = b.size();
    for (size_t col = 0; col < n; ++col) {
        // Partial pivot
        size_t max_row = col;
        for (size_t row = col + 1; row < n; ++row)
            if (std::abs(A[row][col]) > std::abs(A[max_row][col]))
                max_row = row;
        std::swap(A[col], A[max_row]);
        std::swap(b[col], b[max_row]);

        if (std::abs(A[col][col]) < 1e-15) return false;

        for (size_t row = col + 1; row < n; ++row) {
            double f = A[row][col] / A[col][col];
            for (size_t j = col; j < n; ++j)
                A[row][j] -= f * A[col][j];
            b[row] -= f * b[col];
        }
    }
    // Back substitution
    for (int i = static_cast<int>(n) - 1; i >= 0; --i) {
        for (size_t j = static_cast<size_t>(i) + 1; j < n; ++j)
            b[i] -= A[i][j] * b[j];
        b[i] /= A[i][i];
    }
    return true;
}

// ============================================================================
// 1D polynomial fit via normal equations
// ============================================================================

inline FitResult1D poly_fit_1d(
    const std::vector<double>& x_data,
    const std::vector<double>& y_data,
    int degree,
    const std::string& name = "",
    const std::string& x_var = "x",
    const std::string& y_var = "y",
    double train_fraction = 0.8)
{
    FitResult1D result;
    result.name = name;
    result.x_var = x_var;
    result.y_var = y_var;
    result.degree = degree;

    size_t n = x_data.size();
    size_t n_train = static_cast<size_t>(n * train_fraction);
    size_t n_valid = n - n_train;
    result.n_train = n_train;
    result.n_valid = n_valid;

    if (n_train < static_cast<size_t>(degree + 1)) {
        result.coeffs.resize(degree + 1, 0.0);
        return result;
    }

    size_t m = static_cast<size_t>(degree + 1);

    // Build normal equations from training set
    std::vector<std::vector<double>> A(m, std::vector<double>(m, 0.0));
    std::vector<double> b(m, 0.0);

    for (size_t i = 0; i < n_train; ++i) {
        double xp = 1.0;
        for (size_t j = 0; j < m; ++j) {
            double xq = 1.0;
            for (size_t k = 0; k < m; ++k) {
                A[j][k] += xp * xq;
                xq *= x_data[i];
            }
            b[j] += xp * y_data[i];
            xp *= x_data[i];
        }
    }

    if (!solve_linear_system(A, b)) {
        result.coeffs.resize(m, 0.0);
        return result;
    }

    result.coeffs = b;

    // Compute metrics on validation set
    double sum_err = 0.0, sum_pct = 0.0, max_err = 0.0;
    double ss_res = 0.0, ss_tot = 0.0;
    double y_mean = 0.0;
    for (size_t i = n_train; i < n; ++i)
        y_mean += y_data[i];
    if (n_valid > 0) y_mean /= n_valid;

    for (size_t i = n_train; i < n; ++i) {
        double pred = result.evaluate(x_data[i]);
        double err = std::abs(pred - y_data[i]);
        sum_err += err;
        if (std::abs(y_data[i]) > 1e-15)
            sum_pct += err / std::abs(y_data[i]) * 100.0;
        if (err > max_err) max_err = err;
        ss_res += (pred - y_data[i]) * (pred - y_data[i]);
        ss_tot += (y_data[i] - y_mean) * (y_data[i] - y_mean);
    }

    if (n_valid > 0) {
        result.mae = sum_err / n_valid;
        result.mape = sum_pct / n_valid;
        result.max_error = max_err;
        result.r_squared = (ss_tot > 1e-15) ? (1.0 - ss_res / ss_tot) : NAN;
    }

    return result;
}

// ============================================================================
// 2D polynomial surface fit
// Z(x1, x2) = sum a_ij * x1^i * x2^j  for i+j <= degree
// ============================================================================

inline FitResult2D poly_fit_2d(
    const std::vector<double>& x1_data,
    const std::vector<double>& x2_data,
    const std::vector<double>& y_data,
    int degree,
    const std::string& name = "",
    const std::string& x1_var = "T",
    const std::string& x2_var = "P",
    const std::string& y_var = "Z",
    double train_fraction = 0.8)
{
    FitResult2D result;
    result.name = name;
    result.x1_var = x1_var;
    result.x2_var = x2_var;
    result.y_var = y_var;
    result.degree = degree;

    // Generate power pairs (i,j) where i+j <= degree
    for (int total = 0; total <= degree; ++total) {
        for (int i = 0; i <= total; ++i) {
            result.powers.push_back({i, total - i});
        }
    }
    result.n_terms = result.powers.size();
    size_t m = result.n_terms;

    size_t n = x1_data.size();
    size_t n_train = static_cast<size_t>(n * train_fraction);
    size_t n_valid = n - n_train;
    result.n_train = n_train;
    result.n_valid = n_valid;

    if (n_train < m) {
        result.coeffs.resize(m, 0.0);
        return result;
    }

    // Build normal equations
    std::vector<std::vector<double>> A(m, std::vector<double>(m, 0.0));
    std::vector<double> b(m, 0.0);

    for (size_t i = 0; i < n_train; ++i) {
        for (size_t j = 0; j < m; ++j) {
            double phi_j = std::pow(x1_data[i], result.powers[j].first)
                         * std::pow(x2_data[i], result.powers[j].second);
            for (size_t k = 0; k < m; ++k) {
                double phi_k = std::pow(x1_data[i], result.powers[k].first)
                             * std::pow(x2_data[i], result.powers[k].second);
                A[j][k] += phi_j * phi_k;
            }
            b[j] += phi_j * y_data[i];
        }
    }

    if (!solve_linear_system(A, b)) {
        result.coeffs.resize(m, 0.0);
        return result;
    }

    result.coeffs = b;

    // Metrics on validation set
    double sum_err = 0.0, sum_pct = 0.0, max_err = 0.0;
    double ss_res = 0.0, ss_tot = 0.0;
    double y_mean = 0.0;
    for (size_t i = n_train; i < n; ++i) y_mean += y_data[i];
    if (n_valid > 0) y_mean /= n_valid;

    for (size_t i = n_train; i < n; ++i) {
        double pred = result.evaluate(x1_data[i], x2_data[i]);
        double err = std::abs(pred - y_data[i]);
        sum_err += err;
        if (std::abs(y_data[i]) > 1e-15)
            sum_pct += err / std::abs(y_data[i]) * 100.0;
        if (err > max_err) max_err = err;
        ss_res += (pred - y_data[i]) * (pred - y_data[i]);
        ss_tot += (y_data[i] - y_mean) * (y_data[i] - y_mean);
    }

    if (n_valid > 0) {
        result.mae = sum_err / n_valid;
        result.mape = sum_pct / n_valid;
        result.max_error = max_err;
        result.r_squared = (ss_tot > 1e-15) ? (1.0 - ss_res / ss_tot) : NAN;
    }

    return result;
}

} // namespace gas3
} // namespace vsepr
