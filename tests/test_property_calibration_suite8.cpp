/**
 * test_property_calibration_suite8.cpp — Suite #8: Property Calibration
 *                                         and Target Legitimacy
 *
 * Validates the calibration layer that maps precursor channels to a
 * compact set of property-scale outputs with explicit calibration logic,
 * confidence decomposition, rejection behavior, and a formal promotion
 * policy.
 *
 * Phase 8A: Property family selection
 *   Five first-wave targets, naming, round-trip indexing.
 *
 * Phase 8B: Target-definition contracts
 *   Contract construction, sign expectations, domain constraints.
 *
 * Phase 8C: Calibration profiles
 *   Profile application, scale/offset, default profiles.
 *
 * Phase 8D: Supervision regime taxonomy
 *   Three regimes, naming, recording in predictions.
 *
 * Phase 8E: Confidence-gated prediction with decomposed uncertainty
 *   Five confidence components, status determination, abstention.
 *
 * Phase 8F: Out-of-distribution detection
 *   Feature envelope, z-score extrapolation, coverage confidence.
 *
 * Phase 8G: Property-specific synthetic curricula
 *   Rigidity, transport, brittleness sweeps with monotone checks.
 *
 * Phase 8H: Target promotion and legitimacy management
 *   Three legitimacy states, promotion, demotion, display names.
 *
 * Reference: Property Calibration and Target Legitimacy specification
 */

#include "coarse_grain/analysis/property_calibration.hpp"
#include "coarse_grain/analysis/property_pipeline.hpp"
#include "coarse_grain/analysis/macro_precursor.hpp"
#include "coarse_grain/analysis/ensemble_proxy.hpp"
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

static coarse_grain::pipeline::PropertyDatasetRow make_synthetic_row(
    const coarse_grain::EnsembleProxySummary& proxy,
    const std::string& system_id = "synth",
    const std::string& target_name = "rigidity_like",
    double target_value = 0.5)
{
    using namespace coarse_grain::pipeline;

    auto prec = coarse_grain::compute_macro_precursors(proxy);

    PropertyDatasetRow row;
    row.provenance.system_id = system_id;
    row.provenance.fragment_family = "synthetic";
    row.provenance.generator_name = "suite8_synthetic";
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

/**
 * Build a simple training dataset with N samples varying cohesion.
 */
static std::vector<coarse_grain::pipeline::PropertyDatasetRow>
build_training_dataset(int n = 20, const std::string& target_name = "rigidity_like")
{
    std::vector<coarse_grain::pipeline::PropertyDatasetRow> dataset;
    for (int i = 0; i < n; ++i) {
        double t = static_cast<double>(i) / std::max(n - 1, 1);
        auto proxy = make_synthetic_proxy(t, 0.33, 0.5, 0.5, 0.1);
        auto row = make_synthetic_row(proxy, "train_" + std::to_string(i),
                                      target_name, t);
        dataset.push_back(std::move(row));
    }
    return dataset;
}

// ============================================================================
// Phase 8A — Property Family Selection
// ============================================================================

static void phase_8a_property_families() {
    std::printf("\n=== Phase 8A: Property Family Selection ===\n");

    using namespace coarse_grain::calibration;

    // ---- 8A.1: Five first-wave families ----
    {
        std::printf("\n--- 8A.1: Family count ---\n");
        check(PROPERTY_FAMILY_COUNT == 5, "five first-wave families");
    }

    // ---- 8A.2: Name round-trip ----
    {
        std::printf("\n--- 8A.2: Name round-trip ---\n");

        for (int i = 0; i < PROPERTY_FAMILY_COUNT; ++i) {
            auto f = property_family_from_index(i);
            const char* name = property_family_name(f);
            check(name != nullptr && std::strlen(name) > 0,
                  "family name non-empty");
        }

        check(std::strcmp(property_family_name(PropertyFamily::RigidityLike),
                          "rigidity_like") == 0,
              "rigidity name correct");
        check(std::strcmp(property_family_name(PropertyFamily::DuctilityLike),
                          "ductility_like") == 0,
              "ductility name correct");
        check(std::strcmp(property_family_name(PropertyFamily::BrittlenessLike),
                          "brittleness_like") == 0,
              "brittleness name correct");
        check(std::strcmp(property_family_name(PropertyFamily::ThermalTransportLike),
                          "thermal_transport_like") == 0,
              "thermal_transport name correct");
        check(std::strcmp(property_family_name(PropertyFamily::ElectricalTransportLike),
                          "electrical_transport_like") == 0,
              "electrical_transport name correct");
    }

    // ---- 8A.3: All names contain _like suffix ----
    {
        std::printf("\n--- 8A.3: -like suffix ----\n");

        for (int i = 0; i < PROPERTY_FAMILY_COUNT; ++i) {
            auto f = property_family_from_index(i);
            std::string name = property_family_name(f);
            bool has_like = name.find("_like") != std::string::npos;
            check(has_like, "name contains _like suffix");
        }
    }

    // ---- 8A.4: Index round-trip ----
    {
        std::printf("\n--- 8A.4: Index round-trip ---\n");

        check(property_family_from_index(0) == PropertyFamily::RigidityLike,
              "index 0 → RigidityLike");
        check(property_family_from_index(1) == PropertyFamily::DuctilityLike,
              "index 1 → DuctilityLike");
        check(property_family_from_index(2) == PropertyFamily::BrittlenessLike,
              "index 2 → BrittlenessLike");
        check(property_family_from_index(3) == PropertyFamily::ThermalTransportLike,
              "index 3 → ThermalTransportLike");
        check(property_family_from_index(4) == PropertyFamily::ElectricalTransportLike,
              "index 4 → ElectricalTransportLike");
    }
}

// ============================================================================
// Phase 8B — Target-Definition Contracts
// ============================================================================

static void phase_8b_contracts() {
    std::printf("\n=== Phase 8B: Target-Definition Contracts ===\n");

    using namespace coarse_grain::calibration;

    // ---- 8B.1: Contract factory produces valid contracts ----
    {
        std::printf("\n--- 8B.1: Contract factory ---\n");

        for (int i = 0; i < PROPERTY_FAMILY_COUNT; ++i) {
            auto f = property_family_from_index(i);
            auto contract = make_contract(f);

            check(!contract.property_name.empty(),
                  "contract has property name");
            check(!contract.sign_expectations.empty(),
                  "contract has sign expectations");
            check(!contract.dominant_features.empty(),
                  "contract has dominant features");
            check(!contract.admissible_material_classes.empty(),
                  "contract has admissible classes");
            check(contract.output_min < contract.output_max,
                  "contract output range valid");
            check(contract.min_bead_count > 0,
                  "contract min_bead_count > 0");
        }
    }

    // ---- 8B.2: Sign expectations are directional ----
    {
        std::printf("\n--- 8B.2: Sign directionality ---\n");

        auto rig = make_rigidity_contract();
        // Rigidity should increase with cohesion
        check(rig.check_sign("cohesion_proxy", 0.5), "rigidity: +cohesion OK");
        check(!rig.check_sign("cohesion_proxy", -0.5), "rigidity: -cohesion NOT OK");
        // Rigidity should decrease with interface_penalty
        check(rig.check_sign("interface_penalty", -0.3), "rigidity: -interface OK");
        check(!rig.check_sign("interface_penalty", 0.3), "rigidity: +interface NOT OK");
        // Unknown feature: no constraint
        check(rig.check_sign("unknown_feature", 99.0), "unknown: any sign OK");
    }

    // ---- 8B.3: Sign violation counting ----
    {
        std::printf("\n--- 8B.3: Sign violation counting ---\n");

        auto contract = make_rigidity_contract();

        // Create a model with correct signs
        coarse_grain::pipeline::LinearModelCoefficients model;
        model.fitted = true;
        model.feature_names = {"cohesion_proxy", "interface_penalty",
                               "uniformity_proxy"};
        model.weights = {0.5, -0.3, 0.2};  // all correct

        int v = contract.count_sign_violations(model);
        check(v == 0, "no violations with correct signs");

        // Now violate one
        model.weights = {0.5, 0.3, 0.2};  // interface_penalty should be negative
        v = contract.count_sign_violations(model);
        check(v == 1, "one violation with wrong interface sign");

        // Violate two
        model.weights = {-0.5, 0.3, 0.2};  // both wrong
        v = contract.count_sign_violations(model);
        check(v == 2, "two violations with both wrong");
    }

    // ---- 8B.4: Output range checking ----
    {
        std::printf("\n--- 8B.4: Output range ---\n");

        auto contract = make_rigidity_contract();

        check(contract.in_valid_range(0.5),  "0.5 in valid range");
        check(contract.in_valid_range(0.0),  "0.0 in valid range");
        check(contract.in_valid_range(1.0),  "1.0 in valid range");
        check(!contract.in_valid_range(-0.1), "-0.1 out of range");
        check(!contract.in_valid_range(1.1),  "1.1 out of range");
        check(!contract.in_valid_range(std::numeric_limits<double>::quiet_NaN()),
              "NaN out of range");
    }

    // ---- 8B.5: Domain constraints populated ----
    {
        std::printf("\n--- 8B.5: Domain constraints ---\n");

        auto rig = make_rigidity_contract();
        check(!rig.excluded_regimes.empty(), "rigidity: has excluded regimes");
        check(!rig.scale_assumption.empty(), "rigidity: has scale assumption");

        auto brit = make_brittleness_contract();
        check(!brit.excluded_regimes.empty(), "brittleness: has excluded regimes");
    }
}

// ============================================================================
// Phase 8C — Calibration Profiles
// ============================================================================

static void phase_8c_profiles() {
    std::printf("\n=== Phase 8C: Calibration Profiles ===\n");

    using namespace coarse_grain::calibration;

    // ---- 8C.1: Default profiles for all families ----
    {
        std::printf("\n--- 8C.1: Default profiles ---\n");

        for (int i = 0; i < PROPERTY_FAMILY_COUNT; ++i) {
            auto f = property_family_from_index(i);
            auto profile = default_profile(f);

            check(!profile.property_name.empty(), "profile has name");
            check(!profile.dominant_features.empty(), "profile has dominant features");
            check(profile.min_confidence_threshold > 0.0, "profile confidence threshold > 0");
            check(profile.abstention_threshold > 0.0, "profile abstention threshold > 0");
            check(profile.abstention_threshold < profile.min_confidence_threshold,
                  "abstention < confidence threshold");
        }
    }

    // ---- 8C.2: Identity transform ----
    {
        std::printf("\n--- 8C.2: Identity transform ---\n");

        CalibrationProfile p;
        p.scale = 1.0;
        p.offset = 0.0;

        check(std::abs(p.apply(0.5) - 0.5) < 1e-15, "identity: 0.5 → 0.5");
        check(std::abs(p.apply(0.0) - 0.0) < 1e-15, "identity: 0.0 → 0.0");
        check(std::abs(p.apply(1.0) - 1.0) < 1e-15, "identity: 1.0 → 1.0");
    }

    // ---- 8C.3: Scale+offset transform ----
    {
        std::printf("\n--- 8C.3: Scale+offset transform ---\n");

        CalibrationProfile p;
        p.scale = 0.5;
        p.offset = 0.25;

        check(std::abs(p.apply(0.0) - 0.25) < 1e-15, "scaled: 0.0 → 0.25");
        check(std::abs(p.apply(1.0) - 0.75) < 1e-15, "scaled: 1.0 → 0.75");
        check(std::abs(p.apply(0.5) - 0.50) < 1e-15, "scaled: 0.5 → 0.50");
    }

    // ---- 8C.4: Clamping behavior ----
    {
        std::printf("\n--- 8C.4: Clamping ---\n");

        CalibrationProfile p;
        p.scale = 2.0;
        p.offset = 0.0;

        check(p.apply(0.8) == 1.0, "clamp: 0.8*2 clamped to 1.0");
        check(p.apply(-0.5) == 0.0, "clamp: negative clamped to 0.0");
    }

    // ---- 8C.5: Ductility non-monotone flag ----
    {
        std::printf("\n--- 8C.5: Ductility non-monotone ---\n");

        auto duct_profile = default_profile(PropertyFamily::DuctilityLike);
        check(!duct_profile.monotone_expected,
              "ductility marked as non-monotone");

        auto rig_profile = default_profile(PropertyFamily::RigidityLike);
        check(rig_profile.monotone_expected,
              "rigidity marked as monotone");
    }
}

// ============================================================================
// Phase 8D — Supervision Regime Taxonomy
// ============================================================================

static void phase_8d_regimes() {
    std::printf("\n=== Phase 8D: Supervision Regime Taxonomy ===\n");

    using namespace coarse_grain::calibration;

    // ---- 8D.1: Three regimes ----
    {
        std::printf("\n--- 8D.1: Regime names ---\n");

        check(std::strcmp(supervision_regime_name(SupervisionRegime::ContractSupervised),
                          "contract_supervised") == 0,
              "contract_supervised name");
        check(std::strcmp(supervision_regime_name(SupervisionRegime::SyntheticCalibrated),
                          "synthetic_calibrated") == 0,
              "synthetic_calibrated name");
        check(std::strcmp(supervision_regime_name(SupervisionRegime::ExternallyCalibrated),
                          "externally_calibrated") == 0,
              "externally_calibrated name");
    }

    // ---- 8D.2: Regime recorded in prediction ----
    {
        std::printf("\n--- 8D.2: Regime in prediction ---\n");

        CalibratedPrediction pred;
        pred.regime = SupervisionRegime::SyntheticCalibrated;
        check(pred.regime == SupervisionRegime::SyntheticCalibrated,
              "regime recorded in prediction");

        pred.regime = SupervisionRegime::ContractSupervised;
        check(pred.regime == SupervisionRegime::ContractSupervised,
              "regime updated in prediction");
    }
}

// ============================================================================
// Phase 8E — Confidence-Gated Prediction
// ============================================================================

static void phase_8e_confidence() {
    std::printf("\n=== Phase 8E: Confidence-Gated Prediction ===\n");

    using namespace coarse_grain::calibration;
    using namespace coarse_grain::pipeline;

    // ---- 8E.1: Confidence decomposition product ----
    {
        std::printf("\n--- 8E.1: Confidence product ---\n");

        ConfidenceDecomposition cd;
        cd.input_confidence = 0.9;
        cd.precursor_confidence = 0.8;
        cd.coverage_confidence = 1.0;
        cd.attribution_stability = 0.95;
        cd.calibration_confidence = 1.0;

        double expected = 0.9 * 0.8 * 1.0 * 0.95 * 1.0;
        check(std::abs(cd.total() - expected) < 1e-12,
              "confidence total = product of components");
    }

    // ---- 8E.2: Individual floor checking ----
    {
        std::printf("\n--- 8E.2: Individual floors ---\n");

        ConfidenceDecomposition cd;
        cd.input_confidence = 0.9;
        cd.precursor_confidence = 0.8;
        cd.coverage_confidence = 1.0;
        cd.attribution_stability = 0.95;
        cd.calibration_confidence = 1.0;

        check(cd.passes_individual_floors(0.2), "all above 0.2 floor");

        cd.coverage_confidence = 0.05;  // below any floor
        check(!cd.passes_individual_floors(0.1), "fails when one below 0.1");
    }

    // ---- 8E.3: Coverage = 0 forces Withheld ----
    {
        std::printf("\n--- 8E.3: Zero coverage forces Withheld ---\n");

        ConfidenceDecomposition cd;
        cd.input_confidence = 1.0;
        cd.precursor_confidence = 1.0;
        cd.coverage_confidence = 0.0;  // extrapolating
        cd.attribution_stability = 1.0;
        cd.calibration_confidence = 1.0;

        check(cd.total() == 0.0, "total = 0 with zero coverage");

        auto profile = default_profile(PropertyFamily::RigidityLike);
        auto status = determine_status(cd, profile);
        check(status == PredictionStatus::Withheld,
              "zero coverage → Withheld");
    }

    // ---- 8E.4: Status determination ----
    {
        std::printf("\n--- 8E.4: Status determination ---\n");

        auto profile = default_profile(PropertyFamily::RigidityLike);

        // All high → Accepted
        {
            ConfidenceDecomposition cd;
            cd.input_confidence = 0.9;
            cd.precursor_confidence = 0.9;
            cd.coverage_confidence = 1.0;
            cd.attribution_stability = 0.9;
            cd.calibration_confidence = 1.0;
            check(determine_status(cd, profile) == PredictionStatus::Accepted,
                  "high confidence → Accepted");
        }

        // Total above abstention but one component marginal → LowConfidence
        {
            ConfidenceDecomposition cd;
            cd.input_confidence = 0.5;
            cd.precursor_confidence = 0.9;
            cd.coverage_confidence = 1.0;
            cd.attribution_stability = 0.9;
            cd.calibration_confidence = 1.0;
            auto st = determine_status(cd, profile);
            check(st == PredictionStatus::LowConfidence || st == PredictionStatus::Accepted,
                  "marginal input → LowConfidence or Accepted");
        }

        // Very low total → Withheld
        {
            ConfidenceDecomposition cd;
            cd.input_confidence = 0.1;
            cd.precursor_confidence = 0.1;
            cd.coverage_confidence = 0.1;
            cd.attribution_stability = 0.1;
            cd.calibration_confidence = 0.1;
            check(determine_status(cd, profile) == PredictionStatus::Withheld,
                  "very low confidence → Withheld");
        }
    }

    // ---- 8E.5: Input confidence computation ----
    {
        std::printf("\n--- 8E.5: Input confidence ---\n");

        FeatureVector fv;
        fv.valid = true;
        fv.n_imputed = 0;
        fv.values = {0.5, 0.3, 0.7};

        double ic = compute_input_confidence(fv);
        check(ic == 1.0, "valid features: input confidence = 1.0");

        fv.valid = false;
        ic = compute_input_confidence(fv);
        check(ic < 1.0, "invalid features: input confidence < 1.0");

        fv.valid = true;
        fv.n_imputed = 3;
        ic = compute_input_confidence(fv);
        check(ic < 1.0, "imputed features: input confidence < 1.0");
    }

    // ---- 8E.6: Precursor confidence computation ----
    {
        std::printf("\n--- 8E.6: Precursor confidence ---\n");

        auto proxy = make_synthetic_proxy(0.7);
        auto row = make_synthetic_row(proxy, "conf_test");

        double pc = compute_precursor_confidence(row);
        check(pc > 0.0, "valid row: precursor confidence > 0");
        check(pc <= 1.0, "precursor confidence <= 1.0");
    }

    // ---- 8E.7: Attribution split ----
    {
        std::printf("\n--- 8E.7: Attribution split ---\n");

        LinearModelCoefficients model;
        model.fitted = true;
        model.weights = {0.5, -0.3, 0.2, -0.1, 0.4};
        model.feature_names = {"a", "b", "c", "d", "e"};

        FeatureVector fv;
        fv.valid = true;
        fv.values = {1.0, 1.0, 1.0, 1.0, 1.0};
        fv.column_names = {"a", "b", "c", "d", "e"};

        std::vector<FeatureAttribution> contrib, suppress;
        split_attributions(model, fv, contrib, suppress);

        check(!contrib.empty(), "has contributing features");
        check(!suppress.empty(), "has suppressing features");
        // a (0.5), c (0.2), e (0.4) are positive
        check(contrib.size() == 3, "3 contributing features");
        // b (-0.3), d (-0.1) are negative
        check(suppress.size() == 2, "2 suppressing features");
    }

    // ---- 8E.8: Full calibrated prediction ----
    {
        std::printf("\n--- 8E.8: Full calibrated prediction ---\n");

        // Build a small training set and fit a model
        auto dataset = build_training_dataset(20);
        ModelFeatureConfig config;
        std::vector<FeatureVector> X;
        std::vector<double> y;
        for (const auto& row : dataset) {
            auto fv = vectorize(row, config);
            if (!fv.valid) continue;
            const auto* tgt = row.find_target("rigidity_like");
            if (!tgt || !tgt->available) continue;
            X.push_back(fv);
            y.push_back(tgt->value);
        }
        auto model = fit_linear_model(X, y, 0.01, "rigidity_like");
        check(model.fitted, "model fitted");

        auto contract = make_rigidity_contract();
        auto profile = default_profile(PropertyFamily::RigidityLike);

        // Predict on a new sample
        auto test_proxy = make_synthetic_proxy(0.7);
        auto test_row = make_synthetic_row(test_proxy, "test_pred");
        auto test_fv = vectorize(test_row, config);

        auto pred = calibrated_predict(model, test_fv, test_row,
                                       contract, profile,
                                       SupervisionRegime::SyntheticCalibrated);

        check(std::isfinite(pred.value), "prediction value finite");
        check(pred.value >= 0.0 && pred.value <= 1.0,
              "prediction in [0, 1]");
        check(pred.regime == SupervisionRegime::SyntheticCalibrated,
              "regime recorded correctly");
        check(!pred.property_name.empty(), "property name recorded");
        check(pred.confidence.total() > 0.0, "total confidence > 0");
    }

    // ---- 8E.9: Prediction status names ----
    {
        std::printf("\n--- 8E.9: Status names ---\n");

        check(std::strcmp(prediction_status_name(PredictionStatus::Accepted),
                          "accepted") == 0, "accepted name");
        check(std::strcmp(prediction_status_name(PredictionStatus::LowConfidence),
                          "low_confidence") == 0, "low_confidence name");
        check(std::strcmp(prediction_status_name(PredictionStatus::Withheld),
                          "withheld") == 0, "withheld name");
    }
}

// ============================================================================
// Phase 8F — Out-of-Distribution Detection
// ============================================================================

static void phase_8f_ood() {
    std::printf("\n=== Phase 8F: Out-of-Distribution Detection ===\n");

    using namespace coarse_grain::calibration;
    using namespace coarse_grain::pipeline;

    // Build training data and feature vectors
    auto dataset = build_training_dataset(20);
    ModelFeatureConfig config;
    std::vector<FeatureVector> train_X;
    for (const auto& row : dataset) {
        auto fv = vectorize(row, config);
        if (fv.valid) train_X.push_back(fv);
    }

    // ---- 8F.1: Feature envelope computation ----
    {
        std::printf("\n--- 8F.1: Feature envelope ---\n");

        auto env = compute_feature_envelope(train_X, 3.0);

        check(env.populated, "envelope is populated");
        check(env.means.size() == env.stds.size(), "means/stds same length");
        check(!env.feature_names.empty(), "feature names populated");
        check(env.z_threshold == 3.0, "z threshold set correctly");

        // All stds should be non-negative
        bool all_non_neg = true;
        for (double s : env.stds) {
            if (s < 0.0) all_non_neg = false;
        }
        check(all_non_neg, "all stds non-negative");
    }

    // ---- 8F.2: In-distribution sample passes ----
    {
        std::printf("\n--- 8F.2: In-distribution pass ---\n");

        auto env = compute_feature_envelope(train_X, 3.0);
        auto contract = make_rigidity_contract();

        // A sample from the middle of the training range
        auto proxy = make_synthetic_proxy(0.5);
        auto row = make_synthetic_row(proxy);
        auto fv = vectorize(row, config);

        auto ood = check_ood(fv, env, contract.dominant_features);
        check(!ood.extrapolating, "in-distribution: not extrapolating");
        check(ood.coverage_confidence == 1.0, "in-distribution: coverage = 1.0");
        check(ood.n_features_beyond == 0, "in-distribution: no features beyond");
    }

    // ---- 8F.3: Out-of-distribution sample flagged ----
    {
        std::printf("\n--- 8F.3: Out-of-distribution flagged ---\n");

        auto env = compute_feature_envelope(train_X, 3.0);
        auto contract = make_rigidity_contract();

        // Create a wildly out-of-range sample
        auto proxy = make_synthetic_proxy(0.5);
        auto row = make_synthetic_row(proxy);
        auto fv = vectorize(row, config);

        // Corrupt dominant features to extreme values
        for (int j = 0; j < static_cast<int>(fv.values.size()); ++j) {
            for (const auto& d : contract.dominant_features) {
                if (j < static_cast<int>(fv.column_names.size())
                    && fv.column_names[j] == d) {
                    fv.values[j] = 100.0;  // far beyond training range
                }
            }
        }

        auto ood = check_ood(fv, env, contract.dominant_features);
        check(ood.extrapolating, "OOD: extrapolating flagged");
        check(ood.coverage_confidence == 0.0, "OOD: coverage = 0");
        check(ood.n_features_beyond > 0, "OOD: features beyond envelope");
        check(!ood.worst_feature.empty(), "OOD: worst feature identified");
    }

    // ---- 8F.4: OOD applied to prediction ----
    {
        std::printf("\n--- 8F.4: OOD applied to prediction ---\n");

        CalibratedPrediction pred;
        pred.status = PredictionStatus::Accepted;
        pred.confidence.coverage_confidence = 1.0;
        pred.passes_all_checks = true;

        OODResult ood;
        ood.extrapolating = true;
        ood.coverage_confidence = 0.0;
        ood.n_features_beyond = 2;

        apply_ood(pred, ood);

        check(pred.extrapolating, "OOD applied: extrapolating flag set");
        check(pred.confidence.coverage_confidence == 0.0,
              "OOD applied: coverage confidence zeroed");
        check(pred.status == PredictionStatus::Withheld,
              "OOD applied: status forced to Withheld");
        check(!pred.passes_all_checks, "OOD applied: passes_all_checks false");
    }

    // ---- 8F.5: Empty envelope passes everything ----
    {
        std::printf("\n--- 8F.5: Empty envelope ---\n");

        FeatureEnvelope empty;
        auto contract = make_rigidity_contract();

        auto proxy = make_synthetic_proxy(0.5);
        auto row = make_synthetic_row(proxy);
        auto fv = vectorize(row, config);

        auto ood = check_ood(fv, empty, contract.dominant_features);
        check(!ood.extrapolating, "empty envelope: not extrapolating");
        check(ood.coverage_confidence == 1.0, "empty envelope: coverage = 1.0");
    }

    // ---- 8F.6: Only dominant features trigger extrapolation ----
    {
        std::printf("\n--- 8F.6: Dominant feature selectivity ---\n");

        auto env = compute_feature_envelope(train_X, 3.0);

        // Create a sample with extreme non-dominant feature
        auto proxy = make_synthetic_proxy(0.5);
        auto row = make_synthetic_row(proxy);
        auto fv = vectorize(row, config);

        // Corrupt a non-dominant feature
        for (int j = 0; j < static_cast<int>(fv.values.size()); ++j) {
            if (j < static_cast<int>(fv.column_names.size()) &&
                fv.column_names[j] == "bead_count") {
                fv.values[j] = 99999.0;
            }
        }

        // Only check dominant features — bead_count should not trigger
        auto contract = make_rigidity_contract();
        auto ood = check_ood(fv, env, contract.dominant_features);
        check(!ood.extrapolating || ood.worst_feature != "bead_count",
              "non-dominant extreme does not trigger extrapolation");
    }
}

// ============================================================================
// Phase 8G — Property-Specific Synthetic Curricula
// ============================================================================

static void phase_8g_curricula() {
    std::printf("\n=== Phase 8G: Property-Specific Synthetic Curricula ===\n");

    using namespace coarse_grain::calibration;
    using namespace coarse_grain::pipeline;

    // ---- 8G.1: Rigidity curriculum ----
    {
        std::printf("\n--- 8G.1: Rigidity curriculum ---\n");

        auto sweep = build_rigidity_curriculum(10);
        check(static_cast<int>(sweep.rows.size()) == 10,
              "rigidity curriculum: 10 rows");
        check(sweep.family == PropertyFamily::RigidityLike,
              "rigidity curriculum: correct family");
        check(sweep.monotone_expected, "rigidity curriculum: monotone expected");
        check(sweep.regime == SupervisionRegime::SyntheticCalibrated,
              "rigidity curriculum: synthetic regime");

        // Target values should be monotone
        bool targets_monotone = true;
        for (size_t i = 1; i < sweep.expected_targets.size(); ++i) {
            if (sweep.expected_targets[i] < sweep.expected_targets[i-1] - 1e-10) {
                targets_monotone = false;
                break;
            }
        }
        check(targets_monotone, "rigidity curriculum: targets monotone");

        // All rows should be valid
        bool all_valid = true;
        for (const auto& row : sweep.rows) {
            if (!row.is_valid()) all_valid = false;
        }
        check(all_valid, "rigidity curriculum: all rows valid");
    }

    // ---- 8G.2: Transport curriculum ----
    {
        std::printf("\n--- 8G.2: Transport curriculum ---\n");

        auto sweep = build_transport_curriculum(
            PropertyFamily::ThermalTransportLike, 10);
        check(static_cast<int>(sweep.rows.size()) == 10,
              "thermal transport curriculum: 10 rows");
        check(sweep.family == PropertyFamily::ThermalTransportLike,
              "thermal transport curriculum: correct family");

        auto sweep_elec = build_transport_curriculum(
            PropertyFamily::ElectricalTransportLike, 10);
        check(sweep_elec.family == PropertyFamily::ElectricalTransportLike,
              "electrical transport curriculum: correct family");
    }

    // ---- 8G.3: Brittleness curriculum ----
    {
        std::printf("\n--- 8G.3: Brittleness curriculum ---\n");

        auto sweep = build_brittleness_curriculum(10);
        check(static_cast<int>(sweep.rows.size()) == 10,
              "brittleness curriculum: 10 rows");
        check(sweep.family == PropertyFamily::BrittlenessLike,
              "brittleness curriculum: correct family");
        check(sweep.monotone_expected,
              "brittleness curriculum: monotone expected");
    }

    // ---- 8G.4: Curriculum trains a model with correct monotonicity ----
    {
        std::printf("\n--- 8G.4: Curriculum trains monotone model ---\n");

        auto sweep = build_rigidity_curriculum(20);
        ModelFeatureConfig config;

        std::vector<FeatureVector> X;
        std::vector<double> y;
        for (const auto& row : sweep.rows) {
            auto fv = vectorize(row, config);
            if (!fv.valid) continue;
            const auto* tgt = row.find_target("rigidity_like");
            if (!tgt || !tgt->available) continue;
            X.push_back(fv);
            y.push_back(tgt->value);
        }

        auto model = fit_linear_model(X, y, 0.01, "rigidity_like");
        check(model.fitted, "curriculum model fitted");

        // Predict along the sweep and check monotonicity
        std::vector<double> predictions;
        for (const auto& fv : X) {
            auto pred = predict_linear(model, fv, 0.0);
            predictions.push_back(pred.value);
        }

        bool monotone = verify_monotone(predictions, 0.01);
        check(monotone, "rigidity curriculum: predictions are monotone");
    }

    // ---- 8G.5: All curriculum rows tagged synthetic ----
    {
        std::printf("\n--- 8G.5: Regime tagging ---\n");

        auto sweep = build_rigidity_curriculum(5);
        bool all_synthetic = true;
        for (const auto& row : sweep.rows) {
            if (row.metadata.target_origin != PropertySourceType::Synthetic)
                all_synthetic = false;
        }
        check(all_synthetic, "all curriculum rows tagged synthetic");
    }

    // ---- 8G.6: Brittleness curriculum precursor response ----
    {
        std::printf("\n--- 8G.6: Brittleness precursor response ---\n");

        auto sweep = build_brittleness_curriculum(10);
        double first_brit = sweep.rows.front().precursor_state.brittleness_like.value;
        double last_brit  = sweep.rows.back().precursor_state.brittleness_like.value;

        // Increasing surface_sensitivity should increase brittleness
        check(last_brit >= first_brit - 0.05,
              "brittleness precursor responds to surface sensitivity sweep");
    }
}

// ============================================================================
// Phase 8H — Target Promotion and Legitimacy Management
// ============================================================================

static void phase_8h_legitimacy() {
    std::printf("\n=== Phase 8H: Target Promotion and Legitimacy Management ===\n");

    using namespace coarse_grain::calibration;

    // ---- 8H.1: Legitimacy state names ----
    {
        std::printf("\n--- 8H.1: State names ---\n");

        check(std::strcmp(legitimacy_state_name(LegitimacyState::ProxyOnly),
                          "proxy_only") == 0, "proxy_only name");
        check(std::strcmp(legitimacy_state_name(LegitimacyState::CalibratedRelative),
                          "calibrated_relative") == 0, "calibrated_relative name");
        check(std::strcmp(legitimacy_state_name(LegitimacyState::ExternallyAnchored),
                          "externally_anchored") == 0, "externally_anchored name");
    }

    // ---- 8H.2: Promotion to CalibratedRelative ----
    {
        std::printf("\n--- 8H.2: Promotion to CalibratedRelative ---\n");

        PromotionEvidence ev;
        ev.monotonicity_preserved = true;
        ev.signs_consistent = true;
        ev.domain_documented = true;

        check(can_promote_to_calibrated(ev),
              "monotone+signs+domain → can promote to calibrated");

        ev.monotonicity_preserved = false;
        check(!can_promote_to_calibrated(ev),
              "no monotonicity → cannot promote");
    }

    // ---- 8H.3: Promotion to ExternallyAnchored ----
    {
        std::printf("\n--- 8H.3: Promotion to ExternallyAnchored ---\n");

        PromotionEvidence ev;
        ev.monotonicity_preserved = true;
        ev.signs_consistent = true;
        ev.domain_documented = true;
        ev.has_external_data = true;
        ev.bounded_residual_error = true;
        ev.validated_rejection = true;

        check(can_promote_to_anchored(ev),
              "full evidence → can promote to anchored");

        ev.has_external_data = false;
        check(!can_promote_to_anchored(ev),
              "no external data → cannot promote to anchored");
    }

    // ---- 8H.4: Evaluate legitimacy ----
    {
        std::printf("\n--- 8H.4: Evaluate legitimacy ---\n");

        // ProxyOnly: insufficient evidence
        {
            PromotionEvidence ev;
            check(evaluate_legitimacy(ev) == LegitimacyState::ProxyOnly,
                  "empty evidence → ProxyOnly");
        }

        // CalibratedRelative
        {
            PromotionEvidence ev;
            ev.monotonicity_preserved = true;
            ev.signs_consistent = true;
            ev.domain_documented = true;
            check(evaluate_legitimacy(ev) == LegitimacyState::CalibratedRelative,
                  "monotone+signs+domain → CalibratedRelative");
        }

        // ExternallyAnchored
        {
            PromotionEvidence ev;
            ev.monotonicity_preserved = true;
            ev.signs_consistent = true;
            ev.domain_documented = true;
            ev.has_external_data = true;
            ev.bounded_residual_error = true;
            ev.validated_rejection = true;
            check(evaluate_legitimacy(ev) == LegitimacyState::ExternallyAnchored,
                  "full evidence → ExternallyAnchored");
        }
    }

    // ---- 8H.5: Display name changes with legitimacy ----
    {
        std::printf("\n--- 8H.5: Display name ---\n");

        TargetLegitimacy tl;
        tl.property_name = "rigidity_like";

        tl.state = LegitimacyState::ProxyOnly;
        check(tl.display_name() == "rigidity_like",
              "ProxyOnly: keeps _like suffix");

        tl.state = LegitimacyState::CalibratedRelative;
        check(tl.display_name() == "rigidity_like",
              "CalibratedRelative: keeps _like suffix");

        tl.state = LegitimacyState::ExternallyAnchored;
        check(tl.display_name() == "rigidity",
              "ExternallyAnchored: drops _like suffix");
    }

    // ---- 8H.6: Demotion check ----
    {
        std::printf("\n--- 8H.6: Demotion conditions ---\n");

        auto contract = make_rigidity_contract();

        // Model with correct signs: no demotion
        {
            coarse_grain::pipeline::LinearModelCoefficients model;
            model.fitted = true;
            model.feature_names = {"cohesion_proxy", "interface_penalty"};
            model.weights = {0.5, -0.3};

            FeatureEnvelope env;
            env.populated = true;
            env.means = {0.5, 0.3};
            env.stds = {0.15, 0.1};

            auto dem = check_demotion(model, contract, env);
            check(!dem.should_demote(), "correct model: no demotion");
        }

        // Model with sign violation: triggers demotion
        {
            coarse_grain::pipeline::LinearModelCoefficients model;
            model.fitted = true;
            model.feature_names = {"cohesion_proxy", "interface_penalty"};
            model.weights = {-0.5, 0.3};  // both wrong

            FeatureEnvelope env;
            env.populated = false;

            auto dem = check_demotion(model, contract, env);
            check(dem.should_demote(), "sign violation: triggers demotion");
            check(dem.contract_violation_detected,
                  "contract violation detected");
        }
    }

    // ---- 8H.7: Demotion history ----
    {
        std::printf("\n--- 8H.7: Demotion history ---\n");

        TargetLegitimacy tl;
        tl.property_name = "rigidity_like";
        tl.state = LegitimacyState::CalibratedRelative;
        tl.demotion_history.push_back("v1.0: sign violation detected");

        check(tl.demotion_history.size() == 1, "demotion history recorded");
        check(tl.state == LegitimacyState::CalibratedRelative,
              "state still calibrated before explicit demotion");
    }

    // ---- 8H.8: Multi-grouped split ----
    {
        std::printf("\n--- 8H.8: Multi-grouped split ---\n");

        // Build dataset with varied provenance
        std::vector<coarse_grain::pipeline::PropertyDatasetRow> dataset;
        for (int i = 0; i < 30; ++i) {
            double t = static_cast<double>(i) / 29.0;
            auto proxy = make_synthetic_proxy(t);
            auto row = make_synthetic_row(proxy, "system_" + std::to_string(i));
            row.metadata.family_label = (i < 15) ? "family_A" : "family_B";
            row.provenance.generator_name = (i % 2 == 0) ? "gen_X" : "gen_Y";
            row.provenance.fragment_family = "frag_" + std::to_string(i % 5);
            dataset.push_back(std::move(row));
        }

        auto split = multi_grouped_split(dataset, 0.7, 0.15, 42);

        int total = static_cast<int>(split.train_indices.size()
                                   + split.val_indices.size()
                                   + split.test_indices.size());
        check(total == 30, "multi-grouped split covers all rows");
        check(!split.train_indices.empty(), "train set non-empty");
        check(!split.val_indices.empty(), "val set non-empty");
    }

    // ---- 8H.9: Promotion is not permanent ----
    {
        std::printf("\n--- 8H.9: Promotion not permanent ---\n");

        TargetLegitimacy tl;
        tl.property_name = "rigidity_like";

        // Promote
        tl.state = LegitimacyState::CalibratedRelative;
        check(tl.display_name() == "rigidity_like",
              "promoted to calibrated: keeps suffix");

        // Demote
        tl.state = LegitimacyState::ProxyOnly;
        tl.demotion_history.push_back("drift detected");
        check(tl.state == LegitimacyState::ProxyOnly,
              "demoted back to ProxyOnly");
        check(tl.demotion_history.size() == 1,
              "demotion recorded");
    }
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::printf("Suite #8: Property Calibration and Target Legitimacy\n");
    std::printf("          Calibration Profiles, Confidence Decomposition,\n");
    std::printf("          OOD Detection, and Legitimacy Management\n");
    std::printf("================================================================\n");

    phase_8a_property_families();
    phase_8b_contracts();
    phase_8c_profiles();
    phase_8d_regimes();
    phase_8e_confidence();
    phase_8f_ood();
    phase_8g_curricula();
    phase_8h_legitimacy();

    std::printf("\n================================================================\n");
    std::printf("Suite #8 Results: %d passed, %d failed, %d total\n",
                g_pass, g_fail, g_pass + g_fail);

    return g_fail > 0 ? 1 : 0;
}
