#pragma once
/**
 [VSEPR-SIM 4.0.4.04]
 * ═══════════════════════════════════════════════════
 * Computes the pairwise Pearson correlation matrix R_ij for the
 * V4 variable set.
 *
 * From section_v4_transition.tex Section 5:
 *
 *   R_ij = corr(x_i, x_j)
 *
 * Clean variable set (11 variables):
 *   {avg_eta, avg_rho, avg_C, rms_force, final_energy,
 *    macro_rigi, macro_duc, macro_col, γ, Q_data, C_compact}
 *
 * C++26 features:
 *   - Contract emulation
 *   - Flat matrix storage (cache-efficient, no mdspan yet in GCC 14)
 *   - Erroneous-behaviour pattern init
 *   - Trailing return types
 *   - Structured bindings
 *
 * Anti-black-box: full matrix exposed, ranked pairs available.
 */

#include "formation_record.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

namespace v4 {

// ============================================================================
// Variable Index — the 11-variable canonical set
// ============================================================================

enum class CorrVar : int {
    AVG_ETA        = 0,
    AVG_RHO        = 1,
    AVG_C          = 2,
    RMS_FORCE      = 3,
    FINAL_ENERGY   = 4,
    MACRO_RIGI     = 5,
    MACRO_DUC      = 6,
    MACRO_COL      = 7,
    GAMMA          = 8,
    Q_DATA         = 9,
    C_COMPACT      = 10,
    COUNT          = 11
};

static constexpr int CORR_VAR_COUNT = static_cast<int>(CorrVar::COUNT);

inline auto corr_var_name(CorrVar v) -> const char* {
    switch (v) {
        case CorrVar::AVG_ETA:       return "avg_eta";
        case CorrVar::AVG_RHO:       return "avg_rho";
        case CorrVar::AVG_C:         return "avg_C";
        case CorrVar::RMS_FORCE:     return "rms_force";
        case CorrVar::FINAL_ENERGY:  return "final_energy";
        case CorrVar::MACRO_RIGI:    return "macro_rigi";
        case CorrVar::MACRO_DUC:     return "macro_duc";
        case CorrVar::MACRO_COL:     return "macro_col";
        case CorrVar::GAMMA:         return "gamma";
        case CorrVar::Q_DATA:        return "Q_data";
        case CorrVar::C_COMPACT:     return "C_compact";
        default:                     return "?";
    }
}

// ============================================================================
// Extract variable vector from a dataset
// ============================================================================

/**
 * Extract the value of one CorrVar from a FormationRecord.
 * Returns NaN for unset/invalid values.
 */
inline auto extract_var(const FormationRecord& rec, CorrVar v) -> double
{
    switch (v) {
        case CorrVar::AVG_ETA:       return rec.avg_eta;
        case CorrVar::AVG_RHO:       return rec.avg_rho;
        case CorrVar::AVG_C:         return rec.avg_C;
        case CorrVar::RMS_FORCE:     return rec.rms_force;
        case CorrVar::FINAL_ENERGY:  return rec.final_energy;
        case CorrVar::MACRO_RIGI:    return rec.macro_rigidity;
        case CorrVar::MACRO_DUC:     return rec.macro_ductility;
        case CorrVar::MACRO_COL:     return rec.macro_color;
        case CorrVar::GAMMA:         return rec.gamma;
        case CorrVar::Q_DATA:        return rec.data_quality;
        case CorrVar::C_COMPACT:     return rec.compactness;
        default:                     return NaN;
    }
}

// ============================================================================
// Correlation Matrix — flat row-major storage
// ============================================================================

/**
 * CorrMatrix — 11×11 Pearson correlation matrix.
 *
 * Flat storage for cache efficiency.
 * R(i,j) = Pearson correlation between variable i and variable j.
 */
struct CorrMatrix {
    static constexpr int N = CORR_VAR_COUNT;
    std::array<double, N * N> data{};
    int sample_count{0};

    auto operator()(int i, int j) const -> double { return data[i * N + j]; }
    auto operator()(int i, int j) -> double&      { return data[i * N + j]; }

    auto operator()(CorrVar i, CorrVar j) const -> double {
        return data[static_cast<int>(i) * N + static_cast<int>(j)];
    }
    auto operator()(CorrVar i, CorrVar j) -> double& {
        return data[static_cast<int>(i) * N + static_cast<int>(j)];
    }
};

// ============================================================================
// Pearson Correlation Computation
// ============================================================================

/**
 * Compute Pearson r between two vectors of equal length.
 * Skips NaN pairs.  Returns NaN if fewer than 3 valid pairs.
 */
inline auto pearson_r(const std::vector<double>& x,
                      const std::vector<double>& y) -> double
{
    V4_CONTRACT_PRE(x.size() == y.size());

    int n = 0;
    double sx = 0, sy = 0, sxx = 0, syy = 0, sxy = 0;

    for (size_t k = 0; k < x.size(); ++k) {
        if (!std::isfinite(x[k]) || !std::isfinite(y[k])) continue;
        ++n;
        sx  += x[k];
        sy  += y[k];
        sxx += x[k] * x[k];
        syy += y[k] * y[k];
        sxy += x[k] * y[k];
    }

    if (n < 3) return NaN;

    double dn = static_cast<double>(n);
    double num   = dn * sxy - sx * sy;
    double denom = std::sqrt((dn * sxx - sx * sx) * (dn * syy - sy * sy));
    if (denom < 1e-30) return 0.0;
    return std::clamp(num / denom, -1.0, 1.0);
}

/**
 * Compute the full 11×11 Pearson correlation matrix from a dataset.
 *
 * @tparam Container — anything with .size() and operator[] returning
 *                     FormationRecord (vector, array, span)
 */
template<typename Container>
auto compute_correlation_matrix(const Container& records) -> CorrMatrix
{
    CorrMatrix mat;
    mat.sample_count = static_cast<int>(records.size());

    if (records.size() < 3) {
        mat.data.fill(NaN);
        return mat;
    }

    // Extract column vectors
    std::array<std::vector<double>, CORR_VAR_COUNT> columns;
    for (auto& col : columns) col.resize(records.size());

    for (size_t k = 0; k < records.size(); ++k) {
        for (int v = 0; v < CORR_VAR_COUNT; ++v) {
            columns[v][k] = extract_var(records[k], static_cast<CorrVar>(v));
        }
    }

    // Compute pairwise correlations
    for (int i = 0; i < CORR_VAR_COUNT; ++i) {
        mat(i, i) = 1.0;  // self-correlation
        for (int j = i + 1; j < CORR_VAR_COUNT; ++j) {
            double r = pearson_r(columns[i], columns[j]);
            mat(i, j) = r;
            mat(j, i) = r;  // symmetric
        }
    }

    return mat;
}

// ============================================================================
// Ranked Correlation Pairs
// ============================================================================

struct CorrPair {
    CorrVar var_i;
    CorrVar var_j;
    double  r;
};

/**
 * Extract all unique pairs from the correlation matrix, sorted by |r|
 * descending.  Excludes diagonal (self-correlation).
 */
inline auto ranked_correlations(const CorrMatrix& mat) -> std::vector<CorrPair>
{
    std::vector<CorrPair> pairs;
    pairs.reserve(CORR_VAR_COUNT * (CORR_VAR_COUNT - 1) / 2);

    for (int i = 0; i < CORR_VAR_COUNT; ++i) {
        for (int j = i + 1; j < CORR_VAR_COUNT; ++j) {
            double r = mat(i, j);
            if (std::isfinite(r)) {
                pairs.push_back({static_cast<CorrVar>(i),
                                 static_cast<CorrVar>(j), r});
            }
        }
    }

    std::sort(pairs.begin(), pairs.end(),
              [](const CorrPair& a, const CorrPair& b) {
                  return std::abs(a.r) > std::abs(b.r);
              });

    return pairs;
}

/**
 * Find which variables most strongly predict a given target variable.
 *
 * Returns pairs sorted by |r| descending for all correlations involving
 * the target variable.
 */
inline auto predictors_of(const CorrMatrix& mat,
                           CorrVar target) -> std::vector<CorrPair>
{
    int t = static_cast<int>(target);
    std::vector<CorrPair> preds;

    for (int i = 0; i < CORR_VAR_COUNT; ++i) {
        if (i == t) continue;
        double r = mat(t, i);
        if (std::isfinite(r)) {
            preds.push_back({target, static_cast<CorrVar>(i), r});
        }
    }

    std::sort(preds.begin(), preds.end(),
              [](const CorrPair& a, const CorrPair& b) {
                  return std::abs(a.r) > std::abs(b.r);
              });

    return preds;
}

} // namespace v4
