/**
 * test_property_pipeline_suite7.cpp — Suite #7: Property Learning Pipeline
 *
 * Validates the supervised learning pipeline that maps proxy distributions
 * and precursor channels to property-scale outputs.
 *
 * Phase 7A: Dataset construction
 *   Row validity, target handling, provenance, source type preservation.
 *
 * Phase 7B: Feature vectorization
 *   Determinism, column ordering, config ablation, missing policy.
 *
 * Phase 7C: Model architecture (Tier 1 linear baseline)
 *   Fitting, coefficient inspection, prediction, abstention, attribution.
 *
 * Phase 7D: Evaluation and split discipline
 *   Metrics computation, grouped splits, leak prevention.
 *
 * Phase 7E: Synthetic supervision
 *   Monotone sweep, ordinal labels, end-to-end pipeline.
 *
 * Three-layer separation verified:
 *   Descriptor (upstream): no learned params, tested in Suites 5–6
 *   Calibration: model fitting from exported data only
 *   Inference: prediction from fitted model only
 *
 * Reference: Property Learning Pipeline specification
 */

#include "coarse_grain/analysis/property_pipeline.hpp"
#include "coarse_grain/analysis/macro_precursor.hpp"
#include "coarse_grain/analysis/ensemble_proxy.hpp"
#include "tests/scene_factory.hpp"
#include "tests/test_viz.hpp"
#include "coarse_grain/core/environment_state.hpp"
#include "atomistic/core/state.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
#include <algorithm>
#include <limits>
#include <string>

static int g_pass = 0;
static int g_fail = 0;

static void check(bool cond, const char* name) {
    if (cond) {
        std::printf("  [PASS] %s\n", name);
        ++g_pass;
    } else {
        std::printf("  [FAIL] %s\n", name);
        ++g_fail;
    }
}

// ============================================================================
// Helpers
// ============================================================================

static coarse_grain::EnvironmentParams default_params() {
    coarse_grain::EnvironmentParams p;
    p.alpha = 0.5;
    p.beta  = 0.5;
    p.tau   = 100.0;
    p.gamma_steric = 0.2;
    p.gamma_elec   = -0.1;
    p.gamma_disp   = 0.5;
    p.sigma_rho = 3.0;
    p.r_cutoff  = 8.0;
    p.delta_sw  = 1.0;
    p.rho_max   = 10.0;
    return p;
}

static std::vector<atomistic::Vec3> extract_positions(
    const std::vector<test_util::SceneBead>& scene)
{
    std::vector<atomistic::Vec3> pos;
    pos.reserve(scene.size());
    for (const auto& b : scene) pos.push_back(b.position);
    return pos;
}

/**
 * Run scene to convergence, compute proxy, compute precursors.
 */
static coarse_grain::EnsembleProxySummary run_and_proxy(
    const std::vector<test_util::SceneBead>& scene,
    const coarse_grain::EnvironmentParams& params,
    double dt = 10.0, int n_steps = 500)
{
    auto states = test_util::run_all_beads(scene, params, dt, n_steps);
    auto positions = extract_positions(scene);
    return coarse_grain::compute_ensemble_proxy(
        states, positions, -1.0, params.r_cutoff);
}

/**
 * Build a valid PropertyDatasetRow from a scene.
 */
static coarse_grain::pipeline::PropertyDatasetRow make_row(
    const std::vector<test_util::SceneBead>& scene,
    const coarse_grain::EnvironmentParams& params,
    const std::string& system_id = "test_system",
    const std::string& target_name = "rigidity",
    double target_value = 0.5)
{
    using namespace coarse_grain::pipeline;

    auto proxy = run_and_proxy(scene, params);
    auto prec  = coarse_grain::compute_macro_precursors(proxy);

    PropertyDatasetRow row;
    row.provenance.system_id = system_id;
    row.provenance.fragment_family = "test_family";
    row.provenance.generator_name = "suite7_test";
    row.provenance.sim_version = "1.0";
    row.provenance.precursor_version = "1.0";
    row.provenance.seed = 42;
    row.provenance.scenario_label = "baseline";
    row.proxy_summary = proxy;
    row.precursor_state = prec;

    PropertyTarget tgt;
    tgt.value = target_value;
    tgt.available = true;
    tgt.confidence_weight = 1.0;
    tgt.source = PropertySourceType::Synthetic;
    row.targets.push_back({target_name, tgt});

    row.metadata.split_group = system_id;
    row.metadata.sample_weight = 1.0;
    row.metadata.family_label = "test_family";
    row.metadata.target_origin = PropertySourceType::Synthetic;

    return row;
}

/**
 * Build a synthetic proxy summary for controlled testing
 * (no simulation needed — pure data construction).
 */
static coarse_grain::EnsembleProxySummary make_synthetic_proxy(
    double cohesion = 0.5,
    double texture = 0.33,
    double uniformity = 0.5,
    double stabilization = 0.5,
    double surface_sensitivity = 0.1,
    int bead_count = 64)
{
    coarse_grain::EnsembleProxySummary ps{};
    ps.bead_count = bead_count;
    ps.mean_rho_hat = cohesion;
    ps.mean_eta = cohesion;
    ps.mean_state_mismatch = 1.0 - cohesion;
    ps.mean_P2_hat = texture;
    ps.var_eta = (1.0 - uniformity) * 0.25;
    ps.var_rho = (1.0 - uniformity) * 0.25;
    ps.cohesion_proxy = cohesion;
    ps.texture_proxy = texture;
    ps.uniformity_proxy = uniformity;
    ps.stabilization_proxy = stabilization;
    ps.surface_sensitivity_proxy = surface_sensitivity;
    ps.n_bulk = bead_count * 3 / 4;
    ps.n_edge = bead_count - ps.n_bulk;
    ps.bulk_edge_rho_gap = surface_sensitivity;
    ps.bulk_edge_C_gap = surface_sensitivity;
    ps.bulk_edge_P2_gap = surface_sensitivity * 0.5;
    ps.bulk_edge_eta_gap = surface_sensitivity;
    ps.valid = (bead_count >= coarse_grain::PROXY_MIN_BEADS);
    ps.all_finite = true;
    ps.all_bounded = true;
    ps.mean_rho = 1.0;
    ps.mean_C = 4.0;
    ps.mean_P2 = texture;
    ps.mean_target_f = 0.9;
    ps.eta_corr_length = 5.0;
    ps.rho_corr_length = 5.0;
    ps.converged = true;
    ps.rel_cohesion = -1;
    ps.rel_texture = -1;
    ps.rel_uniformity = -1;
    ps.rel_stabilization = -1;
    ps.rel_surface_sensitivity = -1;
    return ps;
}

/**
 * Build a dataset row directly from a synthetic proxy.
 */
static coarse_grain::pipeline::PropertyDatasetRow make_synthetic_row(
    const coarse_grain::EnsembleProxySummary& proxy,
    const std::string& system_id = "synth",
    const std::string& target_name = "rigidity",
    double target_value = 0.5)
{
    using namespace coarse_grain::pipeline;

    auto prec = coarse_grain::compute_macro_precursors(proxy);

    PropertyDatasetRow row;
    row.provenance.system_id = system_id;
    row.provenance.fragment_family = "synthetic";
    row.provenance.generator_name = "suite7_synthetic";
    row.proxy_summary = proxy;
    row.precursor_state = prec;

    PropertyTarget tgt;
    tgt.value = target_value;
    tgt.available = true;
    tgt.source = PropertySourceType::Synthetic;
    row.targets.push_back({target_name, tgt});

    row.metadata.split_group = system_id;
    row.metadata.family_label = "synthetic";

    return row;
}

// ============================================================================
// Phase 7A — Dataset Construction
// ============================================================================

static void phase_7a_dataset() {
    std::printf("\n=== Phase 7A: Dataset Construction ===\n");

    using namespace coarse_grain::pipeline;

    // ---- 7A.1: Valid row construction ----
    {
        std::printf("\n--- 7A.1: Valid row construction ---\n");

        auto params = default_params();
        auto scene = test_util::scene_cubic_lattice(3, 4.0);
        auto row = make_row(scene, params);

        check(row.is_valid(), "7A.1a: row is valid");
        check(!row.provenance.system_id.empty(), "7A.1b: system_id populated");
        check(row.precursor_state.source_bead_count > 0, "7A.1c: bead count > 0");
        check(row.count_available_targets() == 1, "7A.1d: one target available");
        check(row.find_target("rigidity") != nullptr, "7A.1e: target findable");
        check(row.find_target("nonexistent") == nullptr, "7A.1f: missing target returns null");
    }

    // ---- 7A.2: Missing target handling ----
    {
        std::printf("\n--- 7A.2: Missing target handling ---\n");

        PropertyDatasetRow row;
        row.provenance.system_id = "test";
        row.precursor_state.source_bead_count = 64;

        // No targets → invalid
        check(!row.is_valid(), "7A.2a: no targets → invalid");

        // Add unavailable target → still invalid
        PropertyTarget tgt;
        tgt.available = false;
        tgt.value = std::numeric_limits<double>::quiet_NaN();
        row.targets.push_back({"missing_prop", tgt});
        check(!row.is_valid(), "7A.2b: unavailable target → invalid");

        // Add available target → valid
        PropertyTarget tgt2;
        tgt2.available = true;
        tgt2.value = 0.5;
        row.targets.push_back({"real_prop", tgt2});
        check(row.is_valid(), "7A.2c: available target → valid");
        check(row.count_available_targets() == 1, "7A.2d: count = 1");
    }

    // ---- 7A.3: Source type preservation ----
    {
        std::printf("\n--- 7A.3: Source type preservation ---\n");

        PropertyTarget tgt;
        tgt.available = true;
        tgt.value = 1.0;

        tgt.source = PropertySourceType::Experimental;
        check(tgt.source == PropertySourceType::Experimental,
              "7A.3a: experimental source preserved");

        tgt.source = PropertySourceType::Atomistic;
        check(tgt.source == PropertySourceType::Atomistic,
              "7A.3b: atomistic source preserved");

        tgt.source = PropertySourceType::Synthetic;
        check(tgt.source == PropertySourceType::Synthetic,
              "7A.3c: synthetic source preserved");

        tgt.source = PropertySourceType::Ordinal;
        check(tgt.source == PropertySourceType::Ordinal,
              "7A.3d: ordinal source preserved");
    }

    // ---- 7A.4: Provenance completeness ----
    {
        std::printf("\n--- 7A.4: Provenance completeness ---\n");

        auto params = default_params();
        auto scene = test_util::scene_cubic_lattice(3, 4.0);
        auto row = make_row(scene, params, "argon_108", "density", 1.5);

        check(row.provenance.system_id == "argon_108",
              "7A.4a: system_id matches");
        check(row.provenance.fragment_family == "test_family",
              "7A.4b: fragment_family matches");
        check(row.provenance.seed == 42,
              "7A.4c: seed recorded");
        check(row.metadata.split_group == "argon_108",
              "7A.4d: split_group from system_id");
    }

    // ---- 7A.5: Empty system_id → invalid ----
    {
        std::printf("\n--- 7A.5: Empty system_id validation ---\n");

        PropertyDatasetRow row;
        row.provenance.system_id = "";
        row.precursor_state.source_bead_count = 64;
        PropertyTarget tgt;
        tgt.available = true;
        tgt.value = 0.5;
        row.targets.push_back({"prop", tgt});

        check(!row.is_valid(), "7A.5: empty system_id → invalid");
    }

    // ---- 7A.6: Zero bead count → invalid ----
    {
        std::printf("\n--- 7A.6: Zero bead count validation ---\n");

        PropertyDatasetRow row;
        row.provenance.system_id = "test";
        row.precursor_state.source_bead_count = 0;
        PropertyTarget tgt;
        tgt.available = true;
        tgt.value = 0.5;
        row.targets.push_back({"prop", tgt});

        check(!row.is_valid(), "7A.6: zero bead count → invalid");
    }
}

// ============================================================================
// Phase 7B — Feature Vectorization
// ============================================================================

static void phase_7b_vectorization() {
    std::printf("\n=== Phase 7B: Feature Vectorization ===\n");

    using namespace coarse_grain::pipeline;

    auto params = default_params();

    // ---- 7B.1: Fixed-length output ----
    {
        std::printf("\n--- 7B.1: Fixed-length feature vector ---\n");

        auto scene = test_util::scene_cubic_lattice(3, 4.0);
        auto row = make_row(scene, params);

        ModelFeatureConfig config;
        auto fv = vectorize(row, config);

        int expected = config.feature_count();
        check(fv.valid, "7B.1a: feature vector valid");
        check(static_cast<int>(fv.values.size()) == expected,
              "7B.1b: correct length");
        check(static_cast<int>(fv.column_names.size()) == expected,
              "7B.1c: column names match length");
        check(expected == 31, "7B.1d: default config = 31 features");
    }

    // ---- 7B.2: Column name determinism ----
    {
        std::printf("\n--- 7B.2: Column ordering determinism ---\n");

        auto scene = test_util::scene_cubic_lattice(3, 4.0);
        auto row = make_row(scene, params);

        ModelFeatureConfig config;
        auto fv1 = vectorize(row, config);
        auto fv2 = vectorize(row, config);

        bool names_match = (fv1.column_names.size() == fv2.column_names.size());
        for (size_t i = 0; names_match && i < fv1.column_names.size(); ++i)
            names_match = (fv1.column_names[i] == fv2.column_names[i]);
        check(names_match, "7B.2a: column names identical across calls");

        bool values_match = (fv1.values.size() == fv2.values.size());
        for (size_t i = 0; values_match && i < fv1.values.size(); ++i) {
            if (std::isfinite(fv1.values[i]) && std::isfinite(fv2.values[i]))
                values_match = (fv1.values[i] == fv2.values[i]);
        }
        check(values_match, "7B.2b: values identical across calls");
    }

    // ---- 7B.3: Config ablation ----
    {
        std::printf("\n--- 7B.3: Config ablation changes length ---\n");

        auto proxy = make_synthetic_proxy();
        auto row = make_synthetic_row(proxy);

        ModelFeatureConfig full;
        auto fv_full = vectorize(row, full);

        ModelFeatureConfig no_proxy;
        no_proxy.include_proxy_scalars = false;
        auto fv_no_proxy = vectorize(row, no_proxy);

        ModelFeatureConfig no_struct;
        no_struct.include_structural_modifiers = false;
        auto fv_no_struct = vectorize(row, no_struct);

        ModelFeatureConfig no_prec;
        no_prec.include_precursor_channels = false;
        auto fv_no_prec = vectorize(row, no_prec);

        ModelFeatureConfig no_conf;
        no_conf.include_confidence_terms = false;
        auto fv_no_conf = vectorize(row, no_conf);

        check(static_cast<int>(fv_full.values.size()) == 31,
              "7B.3a: full config = 31");
        check(static_cast<int>(fv_no_proxy.values.size()) == 21,
              "7B.3b: no proxy = 21 (31-10)");
        check(static_cast<int>(fv_no_struct.values.size()) == 24,
              "7B.3c: no struct = 24 (31-7)");
        check(static_cast<int>(fv_no_prec.values.size()) == 23,
              "7B.3d: no precursor = 23 (31-8)");
        check(static_cast<int>(fv_no_conf.values.size()) == 25,
              "7B.3e: no confidence = 25 (31-6)");
    }

    // ---- 7B.4: Missing value policy ----
    {
        std::printf("\n--- 7B.4: Missing value policy ---\n");

        auto proxy = make_synthetic_proxy();
        proxy.bulk_edge_rho_gap = std::numeric_limits<double>::quiet_NaN();
        auto row = make_synthetic_row(proxy);

        ModelFeatureConfig config_zero;
        config_zero.missing_policy = MissingPolicy::Zero;
        auto fv_zero = vectorize(row, config_zero);

        ModelFeatureConfig config_nan;
        config_nan.missing_policy = MissingPolicy::NaN;
        auto fv_nan = vectorize(row, config_nan);

        // Find the bulk_edge_rho_gap column
        int gap_idx = -1;
        for (int i = 0; i < static_cast<int>(fv_zero.column_names.size()); ++i) {
            if (fv_zero.column_names[i] == "bulk_edge_rho_gap") {
                gap_idx = i;
                break;
            }
        }
        check(gap_idx >= 0, "7B.4a: gap column found");
        if (gap_idx >= 0) {
            check(fv_zero.values[gap_idx] == 0.0,
                  "7B.4b: Zero policy → 0.0 for NaN");
            check(std::isnan(fv_nan.values[gap_idx]),
                  "7B.4c: NaN policy → NaN preserved");
            check(fv_zero.n_imputed >= 1,
                  "7B.4d: imputation counted (zero)");
            check(fv_nan.n_imputed == 0,
                  "7B.4e: NaN policy does not count as imputation");
        }
    }

    // ---- 7B.5: Feature values from known proxy ----
    {
        std::printf("\n--- 7B.5: Feature values from known proxy ---\n");

        auto proxy = make_synthetic_proxy(0.8, 0.5, 0.9, 0.7, 0.05);
        auto row = make_synthetic_row(proxy);

        ModelFeatureConfig config;
        auto fv = vectorize(row, config);

        // Check Block 1 proxy scalars are at expected values
        check(fv.column_names[0] == "cohesion_proxy",
              "7B.5a: first column is cohesion_proxy");
        check(std::abs(fv.values[0] - 0.8) < 0.01,
              "7B.5b: cohesion value matches proxy");
        check(fv.column_names[1] == "texture_proxy",
              "7B.5c: second column is texture_proxy");
        check(std::abs(fv.values[1] - 0.5) < 0.01,
              "7B.5d: texture value matches proxy");
    }

    // ---- 7B.6: All features finite for valid proxy ----
    {
        std::printf("\n--- 7B.6: All features finite for valid proxy ---\n");

        auto proxy = make_synthetic_proxy();
        auto row = make_synthetic_row(proxy);

        ModelFeatureConfig config;
        auto fv = vectorize(row, config);

        bool all_finite = true;
        for (double v : fv.values) {
            if (!std::isfinite(v)) { all_finite = false; break; }
        }
        check(all_finite, "7B.6: all features finite for valid proxy");
    }
}

// ============================================================================
// Phase 7C — Model Architecture (Tier 1 Linear Baseline)
// ============================================================================

static void phase_7c_model() {
    std::printf("\n=== Phase 7C: Model Architecture (Tier 1) ===\n");

    using namespace coarse_grain::pipeline;

    // ---- 7C.1: Linear model fitting ----
    {
        std::printf("\n--- 7C.1: Linear model fitting ---\n");

        // Generate simple training data: y = 2*x1 + 3*x2 + 1
        // Features must be independent (not collinear) for unique coefficients
        std::vector<FeatureVector> X;
        std::vector<double> y;

        for (int i = 0; i < 50; ++i) {
            FeatureVector fv;
            double x1 = (i % 10) * 0.1;          // varies 0..0.9
            double x2 = (i / 10) * 0.2;           // varies 0..0.8
            fv.values = {x1, x2};
            fv.column_names = {"x1", "x2"};
            fv.valid = true;
            X.push_back(fv);
            y.push_back(2.0 * x1 + 3.0 * x2 + 1.0);
        }

        auto model = fit_linear_model(X, y, 0.001, "test_target");

        check(model.fitted, "7C.1a: model fitted");
        check(static_cast<int>(model.weights.size()) == 2,
              "7C.1b: correct number of weights");
        check(std::abs(model.weights[0] - 2.0) < 0.1,
              "7C.1c: w1 ≈ 2.0");
        check(std::abs(model.weights[1] - 3.0) < 0.1,
              "7C.1d: w2 ≈ 3.0");
        check(std::abs(model.bias - 1.0) < 0.1,
              "7C.1e: bias ≈ 1.0");
        check(model.residual_std < 0.1,
              "7C.1f: low residual std");
        check(model.n_train_samples == 50,
              "7C.1g: n_train correct");
        check(model.target_name == "test_target",
              "7C.1h: target name preserved");
    }

    // ---- 7C.2: Coefficient inspection (named lookup) ----
    {
        std::printf("\n--- 7C.2: Coefficient inspection ---\n");

        std::vector<FeatureVector> X;
        std::vector<double> y;

        for (int i = 0; i < 30; ++i) {
            FeatureVector fv;
            fv.values = {static_cast<double>(i) / 30.0, 0.5};
            fv.column_names = {"rigidity_like", "noise"};
            fv.valid = true;
            X.push_back(fv);
            y.push_back(fv.values[0] * 0.8);  // target correlates with rigidity
        }

        auto model = fit_linear_model(X, y, 0.001);

        double w_rigidity = model.coefficient("rigidity_like");
        double w_noise = model.coefficient("noise");
        double w_missing = model.coefficient("nonexistent");

        check(std::isfinite(w_rigidity), "7C.2a: rigidity coeff found");
        check(w_rigidity > 0.0, "7C.2b: rigidity coeff positive");
        check(std::abs(w_rigidity) > std::abs(w_noise),
              "7C.2c: rigidity coeff > noise coeff");
        check(std::isnan(w_missing), "7C.2d: missing feature → NaN");
    }

    // ---- 7C.3: Prediction output ----
    {
        std::printf("\n--- 7C.3: Prediction output ---\n");

        std::vector<FeatureVector> X;
        std::vector<double> y;

        for (int i = 0; i < 20; ++i) {
            FeatureVector fv;
            fv.values = {static_cast<double>(i) / 20.0};
            fv.column_names = {"feature"};
            fv.valid = true;
            X.push_back(fv);
            y.push_back(fv.values[0] * 2.0 + 1.0);
        }

        auto model = fit_linear_model(X, y, 0.001);

        FeatureVector test_x;
        test_x.values = {0.5};
        test_x.column_names = {"feature"};
        test_x.valid = true;

        auto pred = predict_linear(model, test_x, 0.0);

        check(!pred.withheld, "7C.3a: prediction not withheld");
        check(std::isfinite(pred.value), "7C.3b: prediction finite");
        check(std::abs(pred.value - 2.0) < 0.2,
              "7C.3c: prediction ≈ expected");
        check(std::isfinite(pred.uncertainty), "7C.3d: uncertainty finite");
        check(pred.confidence > 0.0, "7C.3e: confidence > 0");
    }

    // ---- 7C.4: Abstention on low confidence ----
    {
        std::printf("\n--- 7C.4: Abstention on low confidence ---\n");

        std::vector<FeatureVector> X;
        std::vector<double> y;

        for (int i = 0; i < 20; ++i) {
            FeatureVector fv;
            fv.values = {static_cast<double>(i) / 20.0};
            fv.column_names = {"feature"};
            fv.valid = true;
            X.push_back(fv);
            y.push_back(fv.values[0]);
        }

        auto model = fit_linear_model(X, y, 0.001);

        // Low-quality input: invalid feature vector
        FeatureVector bad_x;
        bad_x.values = {0.5};
        bad_x.column_names = {"feature"};
        bad_x.valid = false;
        bad_x.n_imputed = 5;  // many imputed values

        auto pred = predict_linear(model, bad_x, 0.5);  // high threshold

        check(pred.withheld, "7C.4a: withheld on low confidence");
        check(pred.confidence < 0.5, "7C.4b: confidence below threshold");
    }

    // ---- 7C.5: Feature attribution ranking ----
    {
        std::printf("\n--- 7C.5: Feature attribution ---\n");

        std::vector<FeatureVector> X;
        std::vector<double> y;

        for (int i = 0; i < 30; ++i) {
            FeatureVector fv;
            double x1 = static_cast<double>(i) / 30.0;
            double x2 = 0.5;  // constant — no signal
            fv.values = {x1, x2};
            fv.column_names = {"signal", "noise"};
            fv.valid = true;
            X.push_back(fv);
            y.push_back(x1 * 3.0);
        }

        auto model = fit_linear_model(X, y, 0.001);

        FeatureVector test_x;
        test_x.values = {0.8, 0.5};
        test_x.column_names = {"signal", "noise"};
        test_x.valid = true;

        auto pred = predict_linear(model, test_x, 0.0);

        check(!pred.top_features.empty(), "7C.5a: attributions populated");
        if (!pred.top_features.empty()) {
            check(pred.top_features[0].feature_name == "signal",
                  "7C.5b: top feature is signal");
            check(pred.top_features[0].contribution > 0.5,
                  "7C.5c: signal dominates attribution");
        }
    }

    // ---- 7C.6: Unfitted model returns withheld ----
    {
        std::printf("\n--- 7C.6: Unfitted model behavior ---\n");

        LinearModelCoefficients empty_model;

        FeatureVector x;
        x.values = {0.5};
        x.column_names = {"feature"};
        x.valid = true;

        auto pred = predict_linear(empty_model, x);

        check(pred.withheld, "7C.6a: unfitted → withheld");
        check(std::isnan(pred.value), "7C.6b: unfitted → NaN value");
    }

    // ---- 7C.7: Dimension mismatch returns withheld ----
    {
        std::printf("\n--- 7C.7: Dimension mismatch ---\n");

        std::vector<FeatureVector> X;
        std::vector<double> y;
        for (int i = 0; i < 10; ++i) {
            FeatureVector fv;
            fv.values = {static_cast<double>(i) / 10.0, 0.5};
            fv.column_names = {"a", "b"};
            fv.valid = true;
            X.push_back(fv);
            y.push_back(fv.values[0]);
        }
        auto model = fit_linear_model(X, y);

        FeatureVector wrong_dim;
        wrong_dim.values = {0.5};
        wrong_dim.column_names = {"a"};
        wrong_dim.valid = true;

        auto pred = predict_linear(model, wrong_dim);
        check(pred.withheld, "7C.7: dimension mismatch → withheld");
    }
}

// ============================================================================
// Phase 7D — Evaluation and Split Discipline
// ============================================================================

static void phase_7d_evaluation() {
    std::printf("\n=== Phase 7D: Evaluation and Split Discipline ===\n");

    using namespace coarse_grain::pipeline;

    // ---- 7D.1: RMSE and MAE computation ----
    {
        std::printf("\n--- 7D.1: RMSE and MAE ---\n");

        std::vector<double> predicted = {1.0, 2.0, 3.0, 4.0, 5.0};
        std::vector<double> actual    = {1.1, 2.2, 2.8, 4.1, 4.9};

        auto m = evaluate_regression(predicted, actual);

        check(m.n_samples == 5, "7D.1a: n_samples correct");
        check(std::isfinite(m.rmse), "7D.1b: RMSE finite");
        check(m.rmse > 0 && m.rmse < 0.3, "7D.1c: RMSE in expected range");
        check(std::isfinite(m.mae), "7D.1d: MAE finite");
        check(m.mae > 0 && m.mae < 0.2, "7D.1e: MAE in expected range");
    }

    // ---- 7D.2: Perfect prediction → zero error ----
    {
        std::printf("\n--- 7D.2: Perfect prediction ---\n");

        std::vector<double> v = {1.0, 2.0, 3.0, 4.0};
        auto m = evaluate_regression(v, v);

        check(m.rmse < 1e-10, "7D.2a: perfect → RMSE ≈ 0");
        check(m.mae < 1e-10, "7D.2b: perfect → MAE ≈ 0");
    }

    // ---- 7D.3: Spearman on monotone data ----
    {
        std::printf("\n--- 7D.3: Spearman rank correlation ---\n");

        std::vector<double> monotone_p = {1.0, 2.0, 3.0, 4.0, 5.0};
        std::vector<double> monotone_a = {0.1, 0.5, 0.9, 1.2, 1.8};

        auto m = evaluate_regression(monotone_p, monotone_a);
        check(std::abs(m.spearman_rho - 1.0) < 0.01,
              "7D.3a: monotone → Spearman ≈ 1.0");

        // Anti-monotone
        std::vector<double> anti_p = {5.0, 4.0, 3.0, 2.0, 1.0};
        auto m2 = evaluate_regression(anti_p, monotone_a);
        check(std::abs(m2.spearman_rho + 1.0) < 0.01,
              "7D.3b: anti-monotone → Spearman ≈ -1.0");
    }

    // ---- 7D.4: Pairwise accuracy ----
    {
        std::printf("\n--- 7D.4: Pairwise accuracy ---\n");

        std::vector<double> sorted_p = {1.0, 2.0, 3.0, 4.0};
        std::vector<double> sorted_a = {0.5, 1.5, 2.5, 3.5};
        auto m = evaluate_regression(sorted_p, sorted_a);

        check(std::abs(m.pairwise_accuracy - 1.0) < 0.01,
              "7D.4a: perfectly sorted → pairwise acc = 1.0");

        // Reversed
        std::vector<double> rev_p = {4.0, 3.0, 2.0, 1.0};
        auto m2 = evaluate_regression(rev_p, sorted_a);
        check(m2.pairwise_accuracy < 0.1,
              "7D.4b: reversed → pairwise acc ≈ 0");
    }

    // ---- 7D.5: Kendall tau ----
    {
        std::printf("\n--- 7D.5: Kendall tau ---\n");

        std::vector<double> p = {1.0, 2.0, 3.0, 4.0, 5.0};
        std::vector<double> a = {1.0, 2.0, 3.0, 4.0, 5.0};
        auto m = evaluate_regression(p, a);

        check(std::abs(m.kendall_tau - 1.0) < 0.01,
              "7D.5a: concordant → tau = 1.0");

        std::vector<double> rev_p = {5.0, 4.0, 3.0, 2.0, 1.0};
        auto m2 = evaluate_regression(rev_p, a);
        check(std::abs(m2.kendall_tau + 1.0) < 0.01,
              "7D.5b: discordant → tau = -1.0");
    }

    // ---- 7D.6: Grouped split discipline ----
    {
        std::printf("\n--- 7D.6: Grouped split discipline ---\n");

        // Create dataset with 4 groups, 5 rows each
        std::vector<PropertyDatasetRow> dataset;
        for (int g = 0; g < 4; ++g) {
            for (int v = 0; v < 5; ++v) {
                auto proxy = make_synthetic_proxy(0.5 + g * 0.1, 0.33, 0.5 + v * 0.05);
                auto row = make_synthetic_row(proxy,
                    "group_" + std::to_string(g),
                    "rigidity",
                    0.5 + g * 0.1 + v * 0.01);
                row.metadata.split_group = "group_" + std::to_string(g);
                dataset.push_back(std::move(row));
            }
        }

        auto split = grouped_split(dataset, 0.6, 0.2, 42);

        // All indices present
        int total = static_cast<int>(split.train_indices.size()
            + split.val_indices.size()
            + split.test_indices.size());
        check(total == 20, "7D.6a: all 20 rows assigned");

        // Verify discipline: no group crosses partitions
        bool discipline_ok = verify_split_discipline(dataset, split);
        check(discipline_ok, "7D.6b: no group crosses partitions");
    }

    // ---- 7D.7: Split with single group ----
    {
        std::printf("\n--- 7D.7: Single group split ---\n");

        std::vector<PropertyDatasetRow> dataset;
        for (int i = 0; i < 10; ++i) {
            auto proxy = make_synthetic_proxy(0.5 + i * 0.05);
            auto row = make_synthetic_row(proxy, "same_group");
            row.metadata.split_group = "same_group";
            dataset.push_back(std::move(row));
        }

        auto split = grouped_split(dataset, 0.7, 0.15, 42);

        // Single group: all rows in one partition
        bool all_in_one = (static_cast<int>(split.train_indices.size()) == 10)
                       || (static_cast<int>(split.val_indices.size()) == 10)
                       || (static_cast<int>(split.test_indices.size()) == 10);
        check(all_in_one, "7D.7: single group → all in one partition");
    }

    // ---- 7D.8: Insufficient data ----
    {
        std::printf("\n--- 7D.8: Insufficient data ---\n");

        std::vector<double> p = {1.0};
        std::vector<double> a = {1.0};
        auto m = evaluate_regression(p, a);

        check(m.n_samples < 2, "7D.8: < 2 samples → metrics undefined");
    }
}

// ============================================================================
// Phase 7E — Synthetic Supervision
// ============================================================================

static void phase_7e_supervision() {
    std::printf("\n=== Phase 7E: Synthetic Supervision ===\n");

    using namespace coarse_grain::pipeline;

    // ---- 7E.1: Ordinal label assignment ----
    {
        std::printf("\n--- 7E.1: Ordinal labels ---\n");

        check(assign_ordinal_label(0.1) == OrdinalClass::Low,
              "7E.1a: 0.1 → Low");
        check(assign_ordinal_label(0.5) == OrdinalClass::Medium,
              "7E.1b: 0.5 → Medium");
        check(assign_ordinal_label(0.8) == OrdinalClass::High,
              "7E.1c: 0.8 → High");
        check(assign_ordinal_label(0.33) == OrdinalClass::Medium,
              "7E.1d: 0.33 → Medium (boundary)");
        check(assign_ordinal_label(0.67) == OrdinalClass::High,
              "7E.1e: 0.67 → High (boundary)");
        check(assign_ordinal_label(std::numeric_limits<double>::quiet_NaN())
                  == OrdinalClass::Low,
              "7E.1f: NaN → Low (safe default)");
    }

    // ---- 7E.2: Monotone verification ----
    {
        std::printf("\n--- 7E.2: Monotone verification ---\n");

        std::vector<double> mono = {0.1, 0.3, 0.5, 0.7, 0.9};
        check(verify_monotone(mono), "7E.2a: monotone increasing passes");

        std::vector<double> non_mono = {0.1, 0.5, 0.3, 0.7, 0.9};
        check(!verify_monotone(non_mono), "7E.2b: non-monotone fails");

        std::vector<double> flat = {0.5, 0.5, 0.5};
        check(verify_monotone(flat), "7E.2c: flat passes (non-decreasing)");

        std::vector<double> decreasing = {0.9, 0.7, 0.5, 0.3, 0.1};
        check(verify_monotone_decreasing(decreasing),
              "7E.2d: decreasing passes");

        check(!verify_monotone_decreasing(mono),
              "7E.2e: increasing fails decreasing check");

        std::vector<double> single = {0.5};
        check(verify_monotone(single), "7E.2f: single element passes");
    }

    // ---- 7E.3: Synthetic sweep generation ----
    {
        std::printf("\n--- 7E.3: Synthetic sweep generation ---\n");

        auto base_proxy = make_synthetic_proxy(0.5, 0.33, 0.5, 0.5, 0.1);

        auto sweep = build_synthetic_sweep(
            base_proxy,
            "cohesion",
            "rigidity",
            [](coarse_grain::EnsembleProxySummary& ps, double t) {
                ps.cohesion_proxy = 0.2 + 0.6 * t;
                ps.mean_rho_hat = 0.2 + 0.6 * t;
                ps.mean_eta = 0.2 + 0.6 * t;
                ps.mean_state_mismatch = 1.0 - (0.2 + 0.6 * t);
            },
            10);

        check(static_cast<int>(sweep.rows.size()) == 10,
              "7E.3a: 10 sweep rows generated");
        check(sweep.swept_variable == "cohesion",
              "7E.3b: swept variable recorded");

        // All rows valid
        bool all_valid = true;
        for (const auto& row : sweep.rows) {
            if (!row.is_valid()) { all_valid = false; break; }
        }
        check(all_valid, "7E.3c: all sweep rows valid");

        // Targets are monotone [0, 1]
        std::vector<double> target_vals;
        for (const auto& row : sweep.rows) {
            const auto* tgt = row.find_target("rigidity");
            if (tgt && tgt->available) target_vals.push_back(tgt->value);
        }
        check(verify_monotone(target_vals),
              "7E.3d: sweep targets monotone");

        // All share same split_group
        bool same_group = true;
        for (size_t i = 1; i < sweep.rows.size(); ++i) {
            if (sweep.rows[i].metadata.split_group !=
                sweep.rows[0].metadata.split_group) {
                same_group = false;
                break;
            }
        }
        check(same_group, "7E.3e: sweep rows share split group");
    }

    // ---- 7E.4: Sweep → vectorize → fit → predict monotone ----
    {
        std::printf("\n--- 7E.4: End-to-end sweep training ---\n");

        auto base_proxy = make_synthetic_proxy(0.3, 0.33, 0.5, 0.3, 0.1);

        // Build two sweeps with different base states for training diversity
        auto sweep1 = build_synthetic_sweep(
            base_proxy, "cohesion", "rigidity",
            [](coarse_grain::EnsembleProxySummary& ps, double t) {
                ps.cohesion_proxy = 0.1 + 0.8 * t;
                ps.mean_rho_hat = 0.1 + 0.8 * t;
                ps.mean_eta = 0.1 + 0.8 * t;
                ps.mean_state_mismatch = 0.9 - 0.8 * t;
                ps.stabilization_proxy = (0.1 + 0.8 * t) * (0.1 + 0.8 * t);
            }, 20);

        auto base2 = make_synthetic_proxy(0.5, 0.5, 0.7, 0.5, 0.2);
        auto sweep2 = build_synthetic_sweep(
            base2, "cohesion", "rigidity",
            [](coarse_grain::EnsembleProxySummary& ps, double t) {
                ps.cohesion_proxy = 0.2 + 0.7 * t;
                ps.mean_rho_hat = 0.2 + 0.7 * t;
                ps.mean_eta = 0.2 + 0.7 * t;
                ps.mean_state_mismatch = 0.8 - 0.7 * t;
                ps.stabilization_proxy = (0.2 + 0.7 * t) * (0.2 + 0.7 * t);
            }, 20);

        // Combine and update split groups for distinct systems
        std::vector<PropertyDatasetRow> dataset;
        for (auto& row : sweep1.rows) {
            row.metadata.split_group = "sweep1";
            dataset.push_back(std::move(row));
        }
        for (auto& row : sweep2.rows) {
            row.metadata.split_group = "sweep2";
            dataset.push_back(std::move(row));
        }

        // Vectorize all
        ModelFeatureConfig config;
        std::vector<FeatureVector> features;
        std::vector<double> targets;
        for (const auto& row : dataset) {
            auto fv = vectorize(row, config);
            if (!fv.valid) continue;
            const auto* tgt = row.find_target("rigidity");
            if (tgt && tgt->available) {
                features.push_back(std::move(fv));
                targets.push_back(tgt->value);
            }
        }

        check(static_cast<int>(features.size()) == 40,
              "7E.4a: 40 training samples");

        // Fit
        auto model = fit_linear_model(features, targets, 0.01, "rigidity");
        check(model.fitted, "7E.4b: model fitted successfully");

        // Predict on a new sweep and verify monotone
        auto test_proxy = make_synthetic_proxy(0.4, 0.33, 0.6, 0.4, 0.15);
        auto test_sweep = build_synthetic_sweep(
            test_proxy, "cohesion", "rigidity",
            [](coarse_grain::EnsembleProxySummary& ps, double t) {
                ps.cohesion_proxy = 0.15 + 0.7 * t;
                ps.mean_rho_hat = 0.15 + 0.7 * t;
                ps.mean_eta = 0.15 + 0.7 * t;
                ps.mean_state_mismatch = 0.85 - 0.7 * t;
                ps.stabilization_proxy = (0.15 + 0.7 * t) * (0.15 + 0.7 * t);
            }, 10);

        std::vector<double> preds;
        for (const auto& row : test_sweep.rows) {
            auto fv = vectorize(row, config);
            auto p = predict_linear(model, fv, 0.0);
            preds.push_back(p.value);
        }

        check(verify_monotone(preds, 0.01),
              "7E.4c: predictions monotone on unseen sweep");
    }

    // ---- 7E.5: Ordinal labels from precursor channels ----
    {
        std::printf("\n--- 7E.5: Ordinal labels from channels ---\n");

        auto low_proxy = make_synthetic_proxy(0.1, 0.1, 0.1, 0.1, 0.5);
        auto prec_low = coarse_grain::compute_macro_precursors(low_proxy);

        auto high_proxy = make_synthetic_proxy(0.9, 0.9, 0.9, 0.9, 0.01);
        auto prec_high = coarse_grain::compute_macro_precursors(high_proxy);

        OrdinalClass rig_low = assign_ordinal_label(prec_low.rigidity_like.value);
        OrdinalClass rig_high = assign_ordinal_label(prec_high.rigidity_like.value);

        check(static_cast<int>(rig_high) >= static_cast<int>(rig_low),
              "7E.5a: high-quality → higher ordinal");

        // When rigidity_like is clearly high, label should be Medium or High
        if (prec_high.rigidity_like.valid) {
            check(rig_high != OrdinalClass::Low,
                  "7E.5b: strong proxy → not Low ordinal");
        }
    }

    // ---- 7E.6: Build synthetic sweep with end-to-end pipeline ----
    {
        std::printf("\n--- 7E.6: End-to-end pipeline convenience ---\n");

        // Build training dataset from multiple sweeps
        std::vector<PropertyDatasetRow> dataset;

        for (int sweep_id = 0; sweep_id < 5; ++sweep_id) {
            double base_coh = 0.2 + sweep_id * 0.1;
            auto proxy = make_synthetic_proxy(base_coh, 0.33, 0.5, base_coh, 0.1);

            auto sweep = build_synthetic_sweep(
                proxy, "cohesion", "rigidity",
                [](coarse_grain::EnsembleProxySummary& ps, double t) {
                    ps.cohesion_proxy = 0.1 + 0.8 * t;
                    ps.mean_rho_hat = 0.1 + 0.8 * t;
                    ps.mean_eta = 0.1 + 0.8 * t;
                    ps.mean_state_mismatch = 0.9 - 0.8 * t;
                    ps.stabilization_proxy = (0.1 + 0.8 * t) * (0.1 + 0.8 * t);
                }, 15);

            for (auto& row : sweep.rows) {
                row.metadata.split_group = "sweep_" + std::to_string(sweep_id);
                dataset.push_back(std::move(row));
            }
        }

        auto result = end_to_end_pipeline(dataset, "rigidity");

        check(result.success, "7E.6a: pipeline completed");
        check(result.model.fitted, "7E.6b: model fitted");
        check(std::isfinite(result.train_metrics.rmse),
              "7E.6c: train RMSE finite");
        check(result.train_metrics.spearman_rho > 0.5,
              "7E.6d: train Spearman > 0.5");
    }
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::printf("Suite #7: Property Learning Pipeline\n");
    std::printf("          Precursors → Dataset → Features → Model → Predictions\n");
    std::printf("================================================================\n");

    phase_7a_dataset();
    phase_7b_vectorization();
    phase_7c_model();
    phase_7d_evaluation();
    phase_7e_supervision();

    std::printf("\n================================================================\n");
    std::printf("Suite #7 Results: %d passed, %d failed, %d total\n",
                g_pass, g_fail, g_pass + g_fail);

    return g_fail > 0 ? 1 : 0;
}
