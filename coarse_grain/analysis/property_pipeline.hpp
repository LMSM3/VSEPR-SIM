#pragma once
/**
 * property_pipeline.hpp — Property Learning Pipeline
 *
 * Supervised learning system that maps bead-resolved proxy distributions
 * and macroscopic precursor channels to property-scale outputs, with
 * explicit uncertainty quantification and full provenance.
 *
 * Architecture position:
 *   Environment-state evaluation
 *       ↓
 *   Ensemble statistics / spatial field summaries   (ensemble_proxy.hpp)
 *       ↓
 *   Macroscopic response proxies → precursors        (macro_precursor.hpp)
 *       ↓
 *   Property learning pipeline                       ← this module
 *       ↓
 *   Calibrated property predictions
 *
 * THREE-LAYER SEPARATION (strict, enforced by architecture):
 *   1. Descriptor layer  — computes proxy/precursor.  NO learned params.
 *   2. Calibration layer — fits model from dataset.   NO simulation logic.
 *   3. Inference layer   — applies fitted model.      NO fitting, NO simulation.
 *
 * Formula fusion PROHIBITED. Layers communicate through defined data
 * structures only. A system in which physics and learning are fused
 * into a single formula cannot be audited, cannot be incrementally
 * improved, and cannot be trusted.
 *
 * Implementation phases (strict dependency order):
 *   7A: Dataset construction
 *   7B: Feature vectorization
 *   7C: Model architecture (Tier 1 linear baseline)
 *   7D: Evaluation and split discipline
 *   7E: Training stages and synthetic supervision
 *
 * Anti-black-box: every coefficient is inspectable, every prediction
 * carries uncertainty and provenance, every intermediate is exposed.
 *
 * Reference: Property Learning Pipeline specification
 */

#include "coarse_grain/analysis/macro_precursor.hpp"
#include "coarse_grain/analysis/ensemble_proxy.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

namespace coarse_grain::pipeline {

// ============================================================================
// Model Parameters — documented, inspectable
// ============================================================================

/**
 * PIPELINE_DEFAULT_REGULARIZATION
 *   Default L2 regularization strength for Tier 1 ridge regression.
 *   Prevents overfitting on small datasets. Inspectable via
 *   LinearModelCoefficients::regularization.
 */
constexpr double PIPELINE_DEFAULT_REGULARIZATION = 0.01;

/**
 * PIPELINE_CONFIDENCE_THRESHOLD
 *   Below this, predictions are withheld rather than returned with
 *   low quality. Prevents overconfident outputs from poorly constrained
 *   inputs.
 */
constexpr double PIPELINE_CONFIDENCE_THRESHOLD = 0.2;

/**
 * PIPELINE_SINGULAR_THRESHOLD
 *   Pivot threshold for Gaussian elimination in the linear solver.
 *   Below this, the system is treated as singular.
 */
constexpr double PIPELINE_SINGULAR_THRESHOLD = 1e-14;

/**
 * PIPELINE_MAX_FEATURES
 *   Upper bound on feature vector dimensionality.
 *   Prevents accidental memory issues from malformed configs.
 */
constexpr int PIPELINE_MAX_FEATURES = 256;

// ============================================================================
// Phase 7A: Dataset Construction
// ============================================================================

/**
 * PropertySourceType — classification of target value origin.
 *
 * Deployment order: synthetic → ordinal → atomistic → experimental.
 * This ordering reflects practical difficulty, not model limitation.
 */
enum class PropertySourceType {
    Experimental,   // Measured property from material database
    Atomistic,      // Computed from atomistic simulation (MD/DFT)
    Synthetic,      // Constructed from controlled perturbation sweep
    Ordinal         // Coarse label {low, medium, high} from domain rules
};

/**
 * PropertyTarget — one target observable for a dataset row.
 *
 * Missing targets are represented with available=false.
 * They are NEVER silently substituted with zero.
 */
struct PropertyTarget {
    double value               = std::numeric_limits<double>::quiet_NaN();
    bool   available           = false;
    double confidence_weight   = 1.0;
    PropertySourceType source  = PropertySourceType::Synthetic;
};

/**
 * DatasetProvenance — Block A: identity and provenance.
 *
 * Records the complete origin of a dataset row for reproducibility.
 */
struct DatasetProvenance {
    std::string system_id;
    std::string fragment_family;
    std::string generator_name;
    std::string sim_version;
    std::string precursor_version;
    uint64_t    seed             = 0;
    std::string scenario_label;
};

/**
 * TrainingMetadata — Block E: training-logic metadata.
 *
 * split_group: key for grouped partitioning.
 *   All perturbed variants of the same base structure share a split_group
 *   to prevent family-fingerprint leakage across partitions.
 * sample_weight: importance weight for loss computation.
 * family_label: material family for family-conditioned models.
 * target_origin: how the target was obtained.
 */
struct TrainingMetadata {
    std::string        split_group;
    double             sample_weight  = 1.0;
    std::string        family_label;
    PropertySourceType target_origin  = PropertySourceType::Synthetic;
};

/**
 * PropertyDatasetRow — fundamental unit of the learning pipeline.
 *
 * Five contiguous blocks:
 *   A. Identity and provenance (DatasetProvenance)
 *   B. Full EnsembleProxySummary at time of export
 *   C. Evaluated MacroPrecursorState
 *   D. Property targets (named, typed)
 *   E. Training metadata
 *
 * Training proceeds from exported rows, not direct simulation state.
 * This separation is essential for reproducibility.
 */
struct PropertyDatasetRow {
    // Block A: Provenance
    DatasetProvenance provenance;

    // Block B: Proxy summary (full upstream state)
    EnsembleProxySummary proxy_summary;

    // Block C: Precursor channels
    MacroPrecursorState precursor_state;

    // Block D: Property targets (name → target)
    std::vector<std::pair<std::string, PropertyTarget>> targets;

    // Block E: Training metadata
    TrainingMetadata metadata;

    /**
     * Validity: a row is valid when:
     *   1. system_id is non-empty
     *   2. At least one target is available
     *   3. Precursor state has been populated (source_bead_count > 0)
     */
    bool is_valid() const {
        if (provenance.system_id.empty()) return false;
        if (precursor_state.source_bead_count <= 0) return false;
        bool has_target = false;
        for (const auto& [name, tgt] : targets) {
            if (tgt.available) { has_target = true; break; }
        }
        return has_target;
    }

    /**
     * Count available targets.
     */
    int count_available_targets() const {
        int n = 0;
        for (const auto& [name, tgt] : targets) {
            if (tgt.available) ++n;
        }
        return n;
    }

    /**
     * Lookup a target by name. Returns nullptr if not found.
     */
    const PropertyTarget* find_target(const std::string& name) const {
        for (const auto& [n, tgt] : targets) {
            if (n == name) return &tgt;
        }
        return nullptr;
    }
};

// ============================================================================
// Phase 7B: Feature Vectorization
// ============================================================================

/**
 * MissingPolicy — how to handle missing/invalid feature values.
 *
 * Never silently dropped or filled without recording the imputation.
 */
enum class MissingPolicy {
    Zero,   // Replace with 0.0 (conservative: no signal)
    NaN,    // Leave as NaN (forces downstream handling)
    Mean    // Replace with training-set mean (requires external mean vector)
};

/**
 * ModelFeatureConfig — specifies which feature blocks to include.
 *
 * Drives ablation studies: turning off blocks changes vector length.
 * Column ordering is stable within a given config.
 */
struct ModelFeatureConfig {
    bool include_proxy_scalars        = true;   // Block 1: 10 features
    bool include_structural_modifiers = true;   // Block 2: 7 features
    bool include_precursor_channels   = true;   // Block 3: 8 features
    bool include_confidence_terms     = true;   // Block 4: 6 features
    bool include_categorical_context  = false;  // Block 5: deferred

    MissingPolicy missing_policy = MissingPolicy::Zero;

    /**
     * Count total features for this config.
     */
    int feature_count() const {
        int n = 0;
        if (include_proxy_scalars)        n += 10;
        if (include_structural_modifiers) n += 7;
        if (include_precursor_channels)   n += 8;
        if (include_confidence_terms)     n += 6;
        // Block 5 deferred
        return n;
    }
};

/**
 * FeatureVector — fixed-length numeric feature vector.
 *
 * Column ordering is deterministic and preserved across runs.
 * Column names are stored for traceability and attribution.
 */
struct FeatureVector {
    std::vector<double>      values;
    std::vector<std::string> column_names;
    bool valid     = false;
    int  n_imputed = 0;     // count of missing values that were filled
};

/**
 * Apply missing value policy to a single feature.
 * Returns the (possibly imputed) value and increments imputed count.
 */
inline double apply_missing_policy(double v, MissingPolicy policy, int& n_imputed) {
    if (std::isfinite(v)) return v;
    switch (policy) {
        case MissingPolicy::Zero: ++n_imputed; return 0.0;
        case MissingPolicy::NaN:  return v;  // leave as NaN, not imputed
        case MissingPolicy::Mean: ++n_imputed; return 0.0; // fallback when no mean available
    }
    return 0.0;
}

/**
 * vectorize — convert a PropertyDatasetRow to a fixed-length FeatureVector.
 *
 * Deterministic column ordering:
 *   Block 1 (proxy scalars): cohesion, texture, uniformity, stabilization,
 *     surface_sensitivity + 5 reference-normalized forms
 *   Block 2 (structural modifiers): interface_penalty, anisotropy_index,
 *     xi_norm, 4 bulk-edge gap fields
 *   Block 3 (precursor channels): 8 channel values in declaration order
 *   Block 4 (confidence/support): convergence_confidence, valid (0/1),
 *     converged (0/1), bead_count, frac_bulk, frac_edge
 *
 * This function is PURE: same input + config → identical output.
 * No learned parameters, no hidden state.
 */
inline FeatureVector vectorize(const PropertyDatasetRow& row,
                               const ModelFeatureConfig& config) {
    FeatureVector fv;
    int expected = config.feature_count();
    if (expected <= 0 || expected > PIPELINE_MAX_FEATURES) {
        fv.valid = false;
        return fv;
    }
    fv.values.reserve(expected);
    fv.column_names.reserve(expected);
    fv.n_imputed = 0;

    auto push = [&](const std::string& name, double v) {
        double val = apply_missing_policy(v, config.missing_policy, fv.n_imputed);
        fv.values.push_back(val);
        fv.column_names.push_back(name);
    };

    const auto& ps = row.proxy_summary;
    const auto& pr = row.precursor_state;

    // Block 1: Proxy scalars (10 features)
    if (config.include_proxy_scalars) {
        push("cohesion_proxy",                ps.cohesion_proxy);
        push("texture_proxy",                 ps.texture_proxy);
        push("uniformity_proxy",              ps.uniformity_proxy);
        push("stabilization_proxy",           ps.stabilization_proxy);
        push("surface_sensitivity_proxy",     ps.surface_sensitivity_proxy);
        push("rel_cohesion",                  ps.rel_cohesion >= 0 ? ps.rel_cohesion : 0.0);
        push("rel_texture",                   ps.rel_texture >= 0 ? ps.rel_texture : 0.0);
        push("rel_uniformity",                ps.rel_uniformity >= 0 ? ps.rel_uniformity : 0.0);
        push("rel_stabilization",             ps.rel_stabilization >= 0 ? ps.rel_stabilization : 0.0);
        push("rel_surface_sensitivity",       ps.rel_surface_sensitivity >= 0 ? ps.rel_surface_sensitivity : 0.0);
    }

    // Block 2: Structural modifiers (7 features)
    if (config.include_structural_modifiers) {
        push("interface_penalty",     pr.interface_penalty);
        push("anisotropy_index",      pr.anisotropy_index);
        push("xi_norm",               pr.xi_norm);
        push("bulk_edge_rho_gap",     ps.bulk_edge_rho_gap);
        push("bulk_edge_C_gap",       ps.bulk_edge_C_gap);
        push("bulk_edge_P2_gap",      ps.bulk_edge_P2_gap);
        push("bulk_edge_eta_gap",     ps.bulk_edge_eta_gap);
    }

    // Block 3: Precursor channels (8 features)
    if (config.include_precursor_channels) {
        push("rigidity_like",               pr.rigidity_like.value);
        push("ductility_like",              pr.ductility_like.value);
        push("brittleness_like",            pr.brittleness_like.value);
        push("cohesion_integrity_like",     pr.cohesion_integrity_like.value);
        push("thermal_transport_like",      pr.thermal_transport_like.value);
        push("electrical_transport_like",   pr.electrical_transport_like.value);
        push("surface_reactivity_like",     pr.surface_reactivity_like.value);
        push("fracture_susceptibility_like", pr.fracture_susceptibility_like.value);
    }

    // Block 4: Confidence and support (6 features)
    if (config.include_confidence_terms) {
        push("convergence_confidence",  pr.convergence_confidence);
        push("proxy_valid",             pr.source_valid ? 1.0 : 0.0);
        push("proxy_converged",         pr.source_converged ? 1.0 : 0.0);
        push("bead_count",              static_cast<double>(pr.source_bead_count));
        int n_total = ps.n_bulk + ps.n_edge;
        push("frac_bulk", n_total > 0 ? static_cast<double>(ps.n_bulk) / n_total : 0.0);
        push("frac_edge", n_total > 0 ? static_cast<double>(ps.n_edge) / n_total : 0.0);
    }

    fv.valid = (static_cast<int>(fv.values.size()) == expected);
    return fv;
}

// ============================================================================
// Phase 7C: Model Architecture
// ============================================================================

/**
 * FeatureAttribution — contribution of one feature to a prediction.
 *
 * Computed as |w_i * x_i| for linear models. Ranked by magnitude.
 */
struct FeatureAttribution {
    std::string feature_name;
    double      contribution  = 0.0;
};

/**
 * PropertyPrediction — output of the inference layer.
 *
 * Every prediction carries four fields:
 *   1. value:        predicted scalar or class label
 *   2. uncertainty:  estimate of prediction error (residual std from training)
 *   3. confidence:   derived from input proxy validity and precursor confidence
 *   4. top_features: ranked contributing features
 *
 * withheld=true when confidence < threshold. The system does not produce
 * confident predictions from poorly constrained inputs.
 */
struct PropertyPrediction {
    double value        = std::numeric_limits<double>::quiet_NaN();
    double uncertainty  = std::numeric_limits<double>::quiet_NaN();
    double confidence   = 0.0;
    std::vector<FeatureAttribution> top_features;
    bool   withheld     = true;
};

/**
 * LinearModelCoefficients — Tier 1 transparent linear baseline.
 *
 * Regularized linear regression: y = X w + bias + ε
 * with L2 penalty λ ||w||^2.
 *
 * Coefficients are inspected directly after fitting. If a precursor
 * channel that should carry physical signal receives a near-zero or
 * sign-reversed coefficient, this is a finding requiring investigation.
 *
 * The linear baseline is not an obstacle to be replaced; it is the
 * first scientific result.
 */
struct LinearModelCoefficients {
    std::vector<double>      weights;
    double                   bias             = 0.0;
    double                   regularization   = PIPELINE_DEFAULT_REGULARIZATION;
    std::vector<std::string> feature_names;
    bool                     fitted           = false;
    double                   residual_std     = std::numeric_limits<double>::quiet_NaN();
    int                      n_train_samples  = 0;
    std::string              target_name;

    /**
     * Lookup the coefficient for a named feature.
     * Returns NaN if not found.
     */
    double coefficient(const std::string& name) const {
        for (size_t i = 0; i < feature_names.size(); ++i) {
            if (feature_names[i] == name) return weights[i];
        }
        return std::numeric_limits<double>::quiet_NaN();
    }
};

// ============================================================================
// Calibration Layer — model fitting (NO simulation logic)
// ============================================================================

namespace detail {

/**
 * solve_linear_system — Gaussian elimination with partial pivoting.
 *
 * Solves Ax = b for dense n×n system.
 * Returns empty vector on singular system.
 * Suitable for n ≤ PIPELINE_MAX_FEATURES.
 */
inline std::vector<double> solve_linear_system(
    std::vector<std::vector<double>>& A,
    std::vector<double>& b)
{
    int n = static_cast<int>(A.size());
    if (n == 0 || n != static_cast<int>(b.size())) return {};

    // Forward elimination with partial pivoting
    for (int col = 0; col < n; ++col) {
        int max_row = col;
        double max_val = std::abs(A[col][col]);
        for (int row = col + 1; row < n; ++row) {
            if (std::abs(A[row][col]) > max_val) {
                max_val = std::abs(A[row][col]);
                max_row = row;
            }
        }
        if (max_val < PIPELINE_SINGULAR_THRESHOLD) return {};  // singular

        std::swap(A[col], A[max_row]);
        std::swap(b[col], b[max_row]);

        for (int row = col + 1; row < n; ++row) {
            double factor = A[row][col] / A[col][col];
            for (int j = col; j < n; ++j) {
                A[row][j] -= factor * A[col][j];
            }
            b[row] -= factor * b[col];
        }
    }

    // Back substitution
    std::vector<double> x(n);
    for (int i = n - 1; i >= 0; --i) {
        x[i] = b[i];
        for (int j = i + 1; j < n; ++j) {
            x[i] -= A[i][j] * x[j];
        }
        x[i] /= A[i][i];
    }
    return x;
}

/**
 * compute_ranks — rank-transform a vector (average ranks for ties).
 */
inline std::vector<double> compute_ranks(const std::vector<double>& v) {
    int n = static_cast<int>(v.size());
    std::vector<int> idx(n);
    std::iota(idx.begin(), idx.end(), 0);
    std::sort(idx.begin(), idx.end(), [&](int a, int b) {
        return v[a] < v[b];
    });

    std::vector<double> ranks(n);
    int i = 0;
    while (i < n) {
        int j = i;
        while (j < n - 1 && v[idx[j + 1]] == v[idx[j]]) ++j;
        double avg_rank = 0.5 * (i + j) + 1.0;  // 1-based average
        for (int k = i; k <= j; ++k) ranks[idx[k]] = avg_rank;
        i = j + 1;
    }
    return ranks;
}

/**
 * pearson_correlation — Pearson product-moment correlation.
 */
inline double pearson_correlation(const std::vector<double>& x,
                                  const std::vector<double>& y) {
    int n = static_cast<int>(x.size());
    if (n < 2 || n != static_cast<int>(y.size()))
        return std::numeric_limits<double>::quiet_NaN();

    double mx = 0, my = 0;
    for (int i = 0; i < n; ++i) { mx += x[i]; my += y[i]; }
    mx /= n; my /= n;

    double sxy = 0, sxx = 0, syy = 0;
    for (int i = 0; i < n; ++i) {
        double dx = x[i] - mx, dy = y[i] - my;
        sxy += dx * dy;
        sxx += dx * dx;
        syy += dy * dy;
    }
    if (sxx < 1e-30 || syy < 1e-30)
        return std::numeric_limits<double>::quiet_NaN();
    return sxy / std::sqrt(sxx * syy);
}

} // namespace detail

/**
 * fit_linear_model — Tier 1 ridge regression.
 *
 * Solves: w = (X^T X + λI)^{-1} X^T y
 *
 * where X is the feature matrix (n_samples × n_features),
 * y is the target vector, and λ is the regularization strength.
 *
 * CALIBRATION LAYER: contains no simulation logic. Operates entirely
 * on exported FeatureVectors and target values.
 *
 * Returns unfitted model if insufficient data or singular system.
 */
inline LinearModelCoefficients fit_linear_model(
    const std::vector<FeatureVector>& X,
    const std::vector<double>& y,
    double regularization = PIPELINE_DEFAULT_REGULARIZATION,
    const std::string& target_name = "")
{
    LinearModelCoefficients model;
    model.regularization = regularization;
    model.target_name = target_name;

    int n = static_cast<int>(X.size());
    if (n < 2 || n != static_cast<int>(y.size())) return model;
    int p = static_cast<int>(X[0].values.size());
    if (p <= 0 || p > PIPELINE_MAX_FEATURES) return model;

    // Verify all feature vectors have same length
    for (const auto& fv : X) {
        if (static_cast<int>(fv.values.size()) != p) return model;
    }

    model.feature_names = X[0].column_names;

    // Center y: compute mean
    double y_mean = 0;
    for (int i = 0; i < n; ++i) y_mean += y[i];
    y_mean /= n;

    // Center X: compute feature means
    std::vector<double> x_mean(p, 0.0);
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < p; ++j)
            x_mean[j] += X[i].values[j];
    for (int j = 0; j < p; ++j)
        x_mean[j] /= n;

    // Build X^T X + λI  (p × p)
    std::vector<std::vector<double>> XtX(p, std::vector<double>(p, 0.0));
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < p; ++j) {
            double xij = X[i].values[j] - x_mean[j];
            for (int k = j; k < p; ++k) {
                double xik = X[i].values[k] - x_mean[k];
                XtX[j][k] += xij * xik;
            }
        }
    }
    // Symmetrize + regularize
    for (int j = 0; j < p; ++j) {
        for (int k = j + 1; k < p; ++k)
            XtX[k][j] = XtX[j][k];
        XtX[j][j] += regularization;
    }

    // Build X^T y_centered  (p × 1)
    std::vector<double> Xty(p, 0.0);
    for (int i = 0; i < n; ++i) {
        double yi = y[i] - y_mean;
        for (int j = 0; j < p; ++j)
            Xty[j] += (X[i].values[j] - x_mean[j]) * yi;
    }

    // Solve
    auto w = detail::solve_linear_system(XtX, Xty);
    if (w.empty()) return model;

    // Compute bias: y_mean - sum(w_j * x_mean_j)
    double bias = y_mean;
    for (int j = 0; j < p; ++j)
        bias -= w[j] * x_mean[j];

    model.weights = std::move(w);
    model.bias = bias;
    model.fitted = true;
    model.n_train_samples = n;

    // Compute residual standard deviation
    double sse = 0;
    for (int i = 0; i < n; ++i) {
        double pred = model.bias;
        for (int j = 0; j < p; ++j)
            pred += model.weights[j] * X[i].values[j];
        double resid = y[i] - pred;
        sse += resid * resid;
    }
    model.residual_std = (n > p + 1)
        ? std::sqrt(sse / (n - p - 1))
        : std::sqrt(sse / n);

    return model;
}

// ============================================================================
// Inference Layer — prediction (NO fitting, NO simulation)
// ============================================================================

/**
 * predict_linear — apply a fitted Tier 1 linear model.
 *
 * INFERENCE LAYER: contains no simulation logic, no fitting.
 * Applies the trained coefficients to a new feature vector.
 *
 * Confidence is derived from:
 *   - Input precursor confidence (Block 4 features)
 *   - Model fit quality (residual_std)
 *   - Feature vector validity
 *
 * When confidence < threshold, prediction is withheld.
 */
inline PropertyPrediction predict_linear(
    const LinearModelCoefficients& model,
    const FeatureVector& x,
    double confidence_threshold = PIPELINE_CONFIDENCE_THRESHOLD)
{
    PropertyPrediction pred;
    int p = static_cast<int>(model.weights.size());

    if (!model.fitted || p <= 0 ||
        static_cast<int>(x.values.size()) != p) {
        return pred;  // withheld
    }

    // Compute prediction
    double y = model.bias;
    for (int j = 0; j < p; ++j)
        y += model.weights[j] * x.values[j];
    pred.value = y;
    pred.uncertainty = model.residual_std;

    // Compute feature attributions: |w_j * x_j|
    std::vector<FeatureAttribution> attribs(p);
    double total_contrib = 0;
    for (int j = 0; j < p; ++j) {
        attribs[j].feature_name = (j < static_cast<int>(x.column_names.size()))
            ? x.column_names[j] : ("feature_" + std::to_string(j));
        attribs[j].contribution = std::abs(model.weights[j] * x.values[j]);
        total_contrib += attribs[j].contribution;
    }
    // Normalize to fractions
    if (total_contrib > 1e-15) {
        for (auto& a : attribs)
            a.contribution /= total_contrib;
    }
    // Sort descending by contribution
    std::sort(attribs.begin(), attribs.end(),
        [](const FeatureAttribution& a, const FeatureAttribution& b) {
            return a.contribution > b.contribution;
        });
    // Keep top-k (at most 5)
    int top_k = std::min(p, 5);
    pred.top_features.assign(attribs.begin(), attribs.begin() + top_k);

    // Compute confidence from input quality
    double conf = 1.0;
    if (!x.valid) conf *= 0.5;
    if (x.n_imputed > 0)
        conf *= std::max(0.3, 1.0 - 0.1 * x.n_imputed);

    // Check for convergence_confidence in Block 4 (if available)
    for (int j = 0; j < static_cast<int>(x.column_names.size()); ++j) {
        if (x.column_names[j] == "convergence_confidence") {
            conf *= std::max(0.3, x.values[j]);
            break;
        }
    }

    pred.confidence = std::clamp(conf, 0.0, 1.0);
    pred.withheld = (pred.confidence < confidence_threshold);

    return pred;
}

// ============================================================================
// Phase 7D: Evaluation and Split Discipline
// ============================================================================

/**
 * EvaluationMetrics — regression and ranking metrics.
 *
 * All metrics are reported together for complete evaluation.
 * Metrics are chosen per task type:
 *   Regression: RMSE, MAE, Spearman ρ
 *   Ranking:    pairwise accuracy, Kendall τ
 */
struct EvaluationMetrics {
    double rmse             = std::numeric_limits<double>::quiet_NaN();
    double mae              = std::numeric_limits<double>::quiet_NaN();
    double spearman_rho     = std::numeric_limits<double>::quiet_NaN();
    double pairwise_accuracy = std::numeric_limits<double>::quiet_NaN();
    double kendall_tau      = std::numeric_limits<double>::quiet_NaN();
    int    n_samples        = 0;
    int    n_withheld       = 0;
};

/**
 * SplitAssignment — train/validation/test partition indices.
 *
 * Constructed by grouped_split() to ensure all perturbed variants
 * of a single base structure remain in the same partition.
 */
struct SplitAssignment {
    std::vector<int> train_indices;
    std::vector<int> val_indices;
    std::vector<int> test_indices;

    bool is_valid() const {
        return !train_indices.empty() && !val_indices.empty();
    }
};

/**
 * evaluate_regression — compute regression and ranking metrics.
 *
 * Inputs must have the same length. Ignores NaN values.
 */
inline EvaluationMetrics evaluate_regression(
    const std::vector<double>& predicted,
    const std::vector<double>& actual)
{
    EvaluationMetrics m;
    int n = static_cast<int>(predicted.size());
    if (n < 2 || n != static_cast<int>(actual.size())) return m;

    // Filter finite pairs
    std::vector<double> p_clean, a_clean;
    p_clean.reserve(n);
    a_clean.reserve(n);
    for (int i = 0; i < n; ++i) {
        if (std::isfinite(predicted[i]) && std::isfinite(actual[i])) {
            p_clean.push_back(predicted[i]);
            a_clean.push_back(actual[i]);
        }
    }
    n = static_cast<int>(p_clean.size());
    m.n_samples = n;
    if (n < 2) return m;

    // RMSE and MAE
    double sse = 0, sae = 0;
    for (int i = 0; i < n; ++i) {
        double e = p_clean[i] - a_clean[i];
        sse += e * e;
        sae += std::abs(e);
    }
    m.rmse = std::sqrt(sse / n);
    m.mae = sae / n;

    // Spearman rank correlation
    auto ranks_p = detail::compute_ranks(p_clean);
    auto ranks_a = detail::compute_ranks(a_clean);
    m.spearman_rho = detail::pearson_correlation(ranks_p, ranks_a);

    // Pairwise accuracy and Kendall tau
    int concordant = 0, discordant = 0, tied = 0;
    int total_pairs = 0;
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            double dp = p_clean[i] - p_clean[j];
            double da = a_clean[i] - a_clean[j];
            if (dp * da > 0) ++concordant;
            else if (dp * da < 0) ++discordant;
            else ++tied;
            ++total_pairs;
        }
    }
    if (total_pairs > 0) {
        m.pairwise_accuracy = static_cast<double>(concordant + tied) / total_pairs;
        int denom = concordant + discordant;
        m.kendall_tau = (denom > 0)
            ? static_cast<double>(concordant - discordant) / denom
            : std::numeric_limits<double>::quiet_NaN();
    }

    return m;
}

/**
 * grouped_split — partition dataset with family-fingerprint leak prevention.
 *
 * Groups rows by TrainingMetadata::split_group. All rows sharing a
 * split_group are assigned to the same partition.
 *
 * This prevents the model from learning family fingerprints rather
 * than generalizable descriptor-to-property mappings.
 *
 * Uses deterministic assignment: groups sorted by key, then sequentially
 * assigned to train/val/test bins by cumulative fraction.
 */
inline SplitAssignment grouped_split(
    const std::vector<PropertyDatasetRow>& dataset,
    double train_frac = 0.7,
    double val_frac = 0.15,
    uint64_t seed = 42)
{
    SplitAssignment split;
    int n = static_cast<int>(dataset.size());
    if (n == 0) return split;

    // Collect unique groups and their member indices
    struct Group {
        std::string key;
        std::vector<int> indices;
    };
    std::vector<Group> groups;

    for (int i = 0; i < n; ++i) {
        const auto& key = dataset[i].metadata.split_group;
        bool found = false;
        for (auto& g : groups) {
            if (g.key == key) {
                g.indices.push_back(i);
                found = true;
                break;
            }
        }
        if (!found) {
            groups.push_back({key, {i}});
        }
    }

    // Sort groups deterministically by key
    std::sort(groups.begin(), groups.end(),
        [](const Group& a, const Group& b) { return a.key < b.key; });

    // Simple deterministic shuffle using seed
    // (Knuth shuffle with xorshift)
    uint64_t rng = seed ? seed : 1;
    auto next_rng = [&]() -> uint64_t {
        rng ^= rng << 13;
        rng ^= rng >> 7;
        rng ^= rng << 17;
        return rng;
    };
    for (int i = static_cast<int>(groups.size()) - 1; i > 0; --i) {
        int j = static_cast<int>(next_rng() % (i + 1));
        std::swap(groups[i], groups[j]);
    }

    // Assign groups to partitions based on cumulative row count
    int total = n;
    int train_target = static_cast<int>(std::round(train_frac * total));
    int val_target = static_cast<int>(std::round(val_frac * total));
    int running = 0;

    for (const auto& g : groups) {
        if (running < train_target) {
            for (int idx : g.indices)
                split.train_indices.push_back(idx);
        } else if (running < train_target + val_target) {
            for (int idx : g.indices)
                split.val_indices.push_back(idx);
        } else {
            for (int idx : g.indices)
                split.test_indices.push_back(idx);
        }
        running += static_cast<int>(g.indices.size());
    }

    return split;
}

/**
 * Verify grouped split discipline: no split_group appears in
 * multiple partitions.
 */
inline bool verify_split_discipline(
    const std::vector<PropertyDatasetRow>& dataset,
    const SplitAssignment& split)
{
    auto group_key = [&](int idx) -> std::string {
        return dataset[idx].metadata.split_group;
    };

    // Collect groups per partition
    auto collect_groups = [&](const std::vector<int>& indices) {
        std::vector<std::string> gs;
        for (int i : indices) {
            auto k = group_key(i);
            if (std::find(gs.begin(), gs.end(), k) == gs.end())
                gs.push_back(k);
        }
        return gs;
    };

    auto train_groups = collect_groups(split.train_indices);
    auto val_groups = collect_groups(split.val_indices);
    auto test_groups = collect_groups(split.test_indices);

    // Check no overlap
    for (const auto& g : train_groups) {
        if (std::find(val_groups.begin(), val_groups.end(), g) != val_groups.end())
            return false;
        if (std::find(test_groups.begin(), test_groups.end(), g) != test_groups.end())
            return false;
    }
    for (const auto& g : val_groups) {
        if (std::find(test_groups.begin(), test_groups.end(), g) != test_groups.end())
            return false;
    }
    return true;
}

// ============================================================================
// Phase 7E: Synthetic Supervision and Training Stages
// ============================================================================

/**
 * SyntheticSweepResult — output of a controlled perturbation sweep.
 *
 * Each row in the sweep was generated by varying one variable
 * monotonically while holding others fixed. The ground-truth ordering
 * is known by construction.
 */
struct SyntheticSweepResult {
    std::vector<PropertyDatasetRow> rows;
    std::string swept_variable;
    bool monotone_expected = true;
};

/**
 * verify_monotone — check that a sequence of predictions is monotonically
 * non-decreasing.
 *
 * Used to validate synthetic supervision: if the model fails to recover
 * a known monotone ordering, either the features or the training is wrong.
 *
 * tolerance: allowable regression per step before declaring non-monotone.
 */
inline bool verify_monotone(const std::vector<double>& predictions,
                            double tolerance = 0.0) {
    if (predictions.size() < 2) return true;
    for (size_t i = 1; i < predictions.size(); ++i) {
        if (predictions[i] < predictions[i - 1] - tolerance)
            return false;
    }
    return true;
}

/**
 * verify_monotone_decreasing — check monotonically non-increasing.
 */
inline bool verify_monotone_decreasing(const std::vector<double>& predictions,
                                       double tolerance = 0.0) {
    if (predictions.size() < 2) return true;
    for (size_t i = 1; i < predictions.size(); ++i) {
        if (predictions[i] > predictions[i - 1] + tolerance)
            return false;
    }
    return true;
}

/**
 * OrdinalClass — coarse ordinal labels for Stage 2 supervision.
 */
enum class OrdinalClass {
    Low    = 0,
    Medium = 1,
    High   = 2
};

/**
 * assign_ordinal_label — derive ordinal class from a precursor channel.
 *
 * Stage 2 supervision: weak labels from domain rules.
 * Thresholds: low < 0.33, medium ∈ [0.33, 0.67), high ≥ 0.67.
 * These are explicit model parameters, not hidden thresholds.
 */
constexpr double ORDINAL_LOW_THRESHOLD  = 0.33;
constexpr double ORDINAL_HIGH_THRESHOLD = 0.67;

inline OrdinalClass assign_ordinal_label(double channel_value) {
    if (!std::isfinite(channel_value)) return OrdinalClass::Low;
    if (channel_value < ORDINAL_LOW_THRESHOLD)  return OrdinalClass::Low;
    if (channel_value < ORDINAL_HIGH_THRESHOLD) return OrdinalClass::Medium;
    return OrdinalClass::High;
}

/**
 * build_synthetic_sweep — generate a controlled perturbation sweep dataset.
 *
 * Takes a base proxy summary + precursor state, varies one proxy field
 * through a monotone range, and produces dataset rows whose relative
 * ordering is known by construction.
 *
 * field_setter: function that modifies the proxy summary for each step.
 * n_steps: number of sweep steps.
 *
 * Returns a SyntheticSweepResult with n_steps rows.
 */
inline SyntheticSweepResult build_synthetic_sweep(
    const EnsembleProxySummary& base_proxy,
    const std::string& swept_variable,
    const std::string& target_name,
    std::function<void(EnsembleProxySummary&, double)> field_setter,
    int n_steps = 10)
{
    SyntheticSweepResult result;
    result.swept_variable = swept_variable;
    result.monotone_expected = true;
    result.rows.reserve(n_steps);

    for (int i = 0; i < n_steps; ++i) {
        double t = static_cast<double>(i) / std::max(n_steps - 1, 1);

        EnsembleProxySummary proxy = base_proxy;
        field_setter(proxy, t);

        MacroPrecursorState prec = compute_macro_precursors(proxy);

        PropertyDatasetRow row;
        row.provenance.system_id = "sweep_" + swept_variable;
        row.provenance.generator_name = "synthetic_sweep";
        row.provenance.scenario_label = "step_" + std::to_string(i);
        row.proxy_summary = proxy;
        row.precursor_state = prec;
        row.metadata.split_group = "sweep_" + swept_variable;
        row.metadata.target_origin = PropertySourceType::Synthetic;

        // Target value = sweep parameter (monotone by construction)
        PropertyTarget tgt;
        tgt.value = t;
        tgt.available = true;
        tgt.source = PropertySourceType::Synthetic;
        row.targets.push_back({target_name, tgt});

        result.rows.push_back(std::move(row));
    }

    return result;
}

/**
 * end_to_end_pipeline — convenience function demonstrating the full pipeline.
 *
 * 1. Vectorize dataset rows
 * 2. Split into train/val
 * 3. Fit Tier 1 linear model on training set
 * 4. Predict on validation set
 * 5. Evaluate metrics
 *
 * This function exists for testing and demonstration.
 * Production use should call each step explicitly for inspection.
 */
struct PipelineResult {
    LinearModelCoefficients model;
    EvaluationMetrics       train_metrics;
    EvaluationMetrics       val_metrics;
    SplitAssignment         split;
    bool                    success = false;
};

inline PipelineResult end_to_end_pipeline(
    const std::vector<PropertyDatasetRow>& dataset,
    const std::string& target_name,
    const ModelFeatureConfig& config = {},
    double regularization = PIPELINE_DEFAULT_REGULARIZATION)
{
    PipelineResult result;

    // Vectorize
    std::vector<FeatureVector> all_features;
    std::vector<double> all_targets;
    for (const auto& row : dataset) {
        auto fv = vectorize(row, config);
        if (!fv.valid) continue;
        const auto* tgt = row.find_target(target_name);
        if (!tgt || !tgt->available) continue;
        all_features.push_back(std::move(fv));
        all_targets.push_back(tgt->value);
    }
    if (all_features.size() < 4) return result;

    // Split
    result.split = grouped_split(dataset);
    if (!result.split.is_valid()) {
        // Fallback: simple 70/30 split if grouping produced empty partitions
        int n = static_cast<int>(all_features.size());
        int n_train = static_cast<int>(0.7 * n);
        result.split.train_indices.clear();
        result.split.val_indices.clear();
        for (int i = 0; i < n_train; ++i)
            result.split.train_indices.push_back(i);
        for (int i = n_train; i < n; ++i)
            result.split.val_indices.push_back(i);
    }

    // Partition data
    std::vector<FeatureVector> train_X, val_X;
    std::vector<double> train_y, val_y;
    for (int i : result.split.train_indices) {
        if (i < static_cast<int>(all_features.size())) {
            train_X.push_back(all_features[i]);
            train_y.push_back(all_targets[i]);
        }
    }
    for (int i : result.split.val_indices) {
        if (i < static_cast<int>(all_features.size())) {
            val_X.push_back(all_features[i]);
            val_y.push_back(all_targets[i]);
        }
    }
    if (train_X.size() < 2) return result;

    // Fit
    result.model = fit_linear_model(train_X, train_y, regularization, target_name);
    if (!result.model.fitted) return result;

    // Predict + evaluate on train
    {
        std::vector<double> preds;
        for (const auto& x : train_X) {
            auto p = predict_linear(result.model, x);
            preds.push_back(p.value);
        }
        result.train_metrics = evaluate_regression(preds, train_y);
    }

    // Predict + evaluate on val
    if (!val_X.empty()) {
        std::vector<double> preds;
        for (const auto& x : val_X) {
            auto p = predict_linear(result.model, x);
            preds.push_back(p.value);
        }
        result.val_metrics = evaluate_regression(preds, val_y);
    }

    result.success = true;
    return result;
}

} // namespace coarse_grain::pipeline
