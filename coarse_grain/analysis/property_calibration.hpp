#pragma once
/**
 * property_calibration.hpp — Property Calibration and Target Legitimacy
 *
 * Disciplined framework for mapping precursor channels to a small,
 * defensible set of property-scale outputs, with explicit calibration
 * logic, confidence decomposition, rejection behavior, and a formal
 * promotion policy governing when a proxy quantity may be named as a
 * material property.
 *
 * Architecture position:
 *   Environment-state evaluation
 *       ↓
 *   Ensemble statistics / spatial field summaries   (ensemble_proxy.hpp)
 *       ↓
 *   Macroscopic response proxies → precursors        (macro_precursor.hpp)
 *       ↓
 *   Property learning pipeline                       (property_pipeline.hpp)
 *       ↓
 *   Property calibration and target legitimacy        ← this module
 *
 * THREE-LAYER SEPARATION (strict):
 *   1. Descriptor layer  — computes proxy/precursor. NO learned params.
 *   2. Calibration layer — fits model, profiles, curricula. NO simulation.
 *   3. Inference layer   — applies calibrated model. NO fitting/simulation.
 *
 * Implementation phases (strict dependency order):
 *   8A: Property family selection
 *   8B: Target-definition contracts
 *   8C: Calibration profiles
 *   8D: Supervision regime taxonomy
 *   8E: Confidence-gated prediction with decomposed uncertainty
 *   8F: Out-of-distribution detection and extrapolation handling
 *   8G: Property-specific synthetic curricula
 *   8H: Target promotion and legitimacy management
 *
 * Anti-black-box: every mapping decision, every metric, every
 * intermediate result is explicitly inspectable and traceable.
 *
 * Reference: Property Calibration and Target Legitimacy specification
 */

#include "coarse_grain/analysis/property_pipeline.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace coarse_grain::calibration {

// ============================================================================
// Phase 8A: Property Family Selection
// ============================================================================

/**
 * PropertyFamily — first-wave target identifiers.
 *
 * The -like suffix is retained throughout proxy-only and
 * calibrated-relative states.  It may only be dropped when a target
 * reaches externally-anchored legitimacy (Phase 8H).
 */
enum class PropertyFamily {
    RigidityLike,
    DuctilityLike,
    BrittlenessLike,
    ThermalTransportLike,
    ElectricalTransportLike
};

constexpr int PROPERTY_FAMILY_COUNT = 5;

inline const char* property_family_name(PropertyFamily f) {
    switch (f) {
        case PropertyFamily::RigidityLike:            return "rigidity_like";
        case PropertyFamily::DuctilityLike:            return "ductility_like";
        case PropertyFamily::BrittlenessLike:          return "brittleness_like";
        case PropertyFamily::ThermalTransportLike:     return "thermal_transport_like";
        case PropertyFamily::ElectricalTransportLike:  return "electrical_transport_like";
    }
    return "unknown";
}

inline PropertyFamily property_family_from_index(int i) {
    switch (i) {
        case 0: return PropertyFamily::RigidityLike;
        case 1: return PropertyFamily::DuctilityLike;
        case 2: return PropertyFamily::BrittlenessLike;
        case 3: return PropertyFamily::ThermalTransportLike;
        case 4: return PropertyFamily::ElectricalTransportLike;
    }
    return PropertyFamily::RigidityLike;
}

// ============================================================================
// Phase 8B: Target-Definition Contracts
// ============================================================================

/**
 * SignExpectation — expected direction of a feature's influence
 * on a property target.
 */
enum class SignExpectation {
    Positive,   // increasing feature → increasing target
    Negative,   // increasing feature → decreasing target
    Neutral     // no expected directional relationship
};

/**
 * FeatureSignSpec — one feature's expected sign relationship.
 */
struct FeatureSignSpec {
    std::string feature_name;
    SignExpectation expected_sign = SignExpectation::Neutral;
};

/**
 * TargetContract — formal specification for a property target.
 *
 * Records what the property means, which precursor channels drive it
 * in a positive or negative direction, what constitutes a valid output
 * range, under what conditions predictions may be accepted or must
 * be withheld, and what failure modes the contract anticipates.
 */
struct TargetContract {
    std::string property_name;
    PropertyFamily family = PropertyFamily::RigidityLike;

    // Sign expectations: features expected to increase/decrease target
    std::vector<FeatureSignSpec> sign_expectations;

    // Domain constraints
    std::vector<std::string> admissible_material_classes;
    std::vector<std::string> excluded_regimes;
    std::string scale_assumption;

    // Output range
    double output_min = 0.0;
    double output_max = 1.0;

    // Confidence requirements
    double min_precursor_confidence = 0.2;
    int    min_bead_count           = 8;
    int    max_imputed_features     = 5;

    // Dominant features (for OOD detection and attribution checks)
    std::vector<std::string> dominant_features;

    /**
     * Check whether a given sign on a named feature is consistent
     * with this contract.  Returns true if consistent or if the
     * feature is not listed.
     */
    bool check_sign(const std::string& feature, double coefficient) const {
        for (const auto& spec : sign_expectations) {
            if (spec.feature_name == feature) {
                if (spec.expected_sign == SignExpectation::Positive)
                    return coefficient >= 0.0;
                if (spec.expected_sign == SignExpectation::Negative)
                    return coefficient <= 0.0;
                return true;  // Neutral
            }
        }
        return true;  // not listed → no constraint
    }

    /**
     * Count how many sign expectations are violated by a model.
     */
    int count_sign_violations(const pipeline::LinearModelCoefficients& model) const {
        int violations = 0;
        for (const auto& spec : sign_expectations) {
            if (spec.expected_sign == SignExpectation::Neutral) continue;
            double c = model.coefficient(spec.feature_name);
            if (std::isnan(c)) continue;  // feature not in model
            if (spec.expected_sign == SignExpectation::Positive && c < 0.0)
                ++violations;
            if (spec.expected_sign == SignExpectation::Negative && c > 0.0)
                ++violations;
        }
        return violations;
    }

    /**
     * Check whether a predicted value is within the valid output range.
     */
    bool in_valid_range(double predicted) const {
        return std::isfinite(predicted)
            && predicted >= output_min
            && predicted <= output_max;
    }
};

// ---- Contract factories for each first-wave property ----

inline TargetContract make_rigidity_contract() {
    TargetContract c;
    c.property_name = "rigidity_like";
    c.family = PropertyFamily::RigidityLike;
    c.sign_expectations = {
        {"cohesion_proxy",             SignExpectation::Positive},
        {"uniformity_proxy",           SignExpectation::Positive},
        {"stabilization_proxy",        SignExpectation::Positive},
        {"rigidity_like",              SignExpectation::Positive},
        {"interface_penalty",          SignExpectation::Negative},
        {"surface_sensitivity_proxy",  SignExpectation::Negative}
    };
    c.dominant_features = {"cohesion_proxy", "uniformity_proxy",
                           "rigidity_like", "stabilization_proxy"};
    c.admissible_material_classes = {"dense_amorphous", "crystalline", "composite"};
    c.excluded_regimes = {"fracture_initiation", "porous_media"};
    c.scale_assumption = "atomistic_coarse_grained";
    return c;
}

inline TargetContract make_ductility_contract() {
    TargetContract c;
    c.property_name = "ductility_like";
    c.family = PropertyFamily::DuctilityLike;
    c.sign_expectations = {
        {"uniformity_proxy",           SignExpectation::Positive},
        {"ductility_like",             SignExpectation::Positive},
        {"stabilization_proxy",        SignExpectation::Positive},
        {"brittleness_like",           SignExpectation::Negative},
        {"interface_penalty",          SignExpectation::Negative}
    };
    c.dominant_features = {"ductility_like", "uniformity_proxy",
                           "stabilization_proxy"};
    c.admissible_material_classes = {"dense_amorphous", "crystalline", "composite"};
    c.excluded_regimes = {"fracture_initiation"};
    c.scale_assumption = "atomistic_coarse_grained";
    return c;
}

inline TargetContract make_brittleness_contract() {
    TargetContract c;
    c.property_name = "brittleness_like";
    c.family = PropertyFamily::BrittlenessLike;
    c.sign_expectations = {
        {"brittleness_like",           SignExpectation::Positive},
        {"interface_penalty",          SignExpectation::Positive},
        {"surface_sensitivity_proxy",  SignExpectation::Positive},
        {"ductility_like",             SignExpectation::Negative},
        {"uniformity_proxy",           SignExpectation::Negative}
    };
    c.dominant_features = {"brittleness_like", "interface_penalty",
                           "surface_sensitivity_proxy"};
    c.admissible_material_classes = {"dense_amorphous", "crystalline"};
    c.excluded_regimes = {"plastic_flow"};
    c.scale_assumption = "atomistic_coarse_grained";
    return c;
}

inline TargetContract make_thermal_transport_contract() {
    TargetContract c;
    c.property_name = "thermal_transport_like";
    c.family = PropertyFamily::ThermalTransportLike;
    c.sign_expectations = {
        {"cohesion_proxy",             SignExpectation::Positive},
        {"texture_proxy",              SignExpectation::Positive},
        {"thermal_transport_like",     SignExpectation::Positive},
        {"interface_penalty",          SignExpectation::Negative}
    };
    c.dominant_features = {"thermal_transport_like", "cohesion_proxy",
                           "texture_proxy"};
    c.admissible_material_classes = {"dense_amorphous", "crystalline", "composite"};
    c.excluded_regimes = {"porous_media"};
    c.scale_assumption = "atomistic_coarse_grained";
    return c;
}

inline TargetContract make_electrical_transport_contract() {
    TargetContract c;
    c.property_name = "electrical_transport_like";
    c.family = PropertyFamily::ElectricalTransportLike;
    c.sign_expectations = {
        {"texture_proxy",                 SignExpectation::Positive},
        {"cohesion_proxy",                SignExpectation::Positive},
        {"electrical_transport_like",     SignExpectation::Positive},
        {"interface_penalty",             SignExpectation::Negative}
    };
    c.dominant_features = {"electrical_transport_like", "texture_proxy",
                           "cohesion_proxy"};
    c.admissible_material_classes = {"crystalline", "composite", "conductive_polymer"};
    c.excluded_regimes = {"insulator_regime"};
    c.scale_assumption = "atomistic_coarse_grained";
    return c;
}

inline TargetContract make_contract(PropertyFamily f) {
    switch (f) {
        case PropertyFamily::RigidityLike:           return make_rigidity_contract();
        case PropertyFamily::DuctilityLike:          return make_ductility_contract();
        case PropertyFamily::BrittlenessLike:        return make_brittleness_contract();
        case PropertyFamily::ThermalTransportLike:   return make_thermal_transport_contract();
        case PropertyFamily::ElectricalTransportLike:return make_electrical_transport_contract();
    }
    return make_rigidity_contract();
}

// ============================================================================
// Phase 8C: Calibration Profiles
// ============================================================================

/**
 * CalibrationProfile — per-target calibration layer.
 *
 * Provides a calibrated interpretation for each target without
 * modifying the descriptor or precursor computation code.
 */
struct CalibrationProfile {
    std::string property_name;
    double scale       = 1.0;
    double offset      = 0.0;
    double min_confidence_threshold = 0.3;
    double abstention_threshold     = 0.15;
    bool   monotone_expected        = true;
    std::vector<std::string> dominant_features;

    /**
     * Apply calibration transform: out = scale * raw + offset,
     * clamped to [0, 1].
     */
    double apply(double raw) const {
        return std::clamp(scale * raw + offset, 0.0, 1.0);
    }
};

inline CalibrationProfile default_profile(PropertyFamily f) {
    CalibrationProfile p;
    p.property_name = property_family_name(f);
    auto contract = make_contract(f);
    p.dominant_features = contract.dominant_features;

    switch (f) {
        case PropertyFamily::RigidityLike:
            p.scale = 1.0; p.offset = 0.0;
            p.monotone_expected = true;
            break;
        case PropertyFamily::DuctilityLike:
            p.scale = 1.0; p.offset = 0.0;
            p.monotone_expected = false;  // ductility has optimum
            break;
        case PropertyFamily::BrittlenessLike:
            p.scale = 1.0; p.offset = 0.0;
            p.monotone_expected = true;
            break;
        case PropertyFamily::ThermalTransportLike:
            p.scale = 1.0; p.offset = 0.0;
            p.monotone_expected = true;
            break;
        case PropertyFamily::ElectricalTransportLike:
            p.scale = 1.0; p.offset = 0.0;
            p.monotone_expected = true;
            break;
    }
    return p;
}

// ============================================================================
// Phase 8D: Supervision Regime Taxonomy
// ============================================================================

/**
 * SupervisionRegime — the supervision under which a prediction was produced.
 *
 * Must be recorded in prediction provenance and reported alongside
 * the output.
 */
enum class SupervisionRegime {
    ContractSupervised,     // No external data; sign/monotone from contract
    SyntheticCalibrated,    // Controlled perturbation sweeps
    ExternallyCalibrated    // Measured/atomistically validated reference
};

inline const char* supervision_regime_name(SupervisionRegime r) {
    switch (r) {
        case SupervisionRegime::ContractSupervised:  return "contract_supervised";
        case SupervisionRegime::SyntheticCalibrated: return "synthetic_calibrated";
        case SupervisionRegime::ExternallyCalibrated:return "externally_calibrated";
    }
    return "unknown";
}

// ============================================================================
// Phase 8E: Confidence-Gated Prediction with Decomposed Uncertainty
// ============================================================================

/**
 * PredictionStatus — output state of a calibrated prediction.
 */
enum class PredictionStatus {
    Accepted,       // Total and all component confidences above threshold
    LowConfidence,  // Total above abstention but at least one component low
    Withheld        // Total below abstention or hard constraint violated
};

inline const char* prediction_status_name(PredictionStatus s) {
    switch (s) {
        case PredictionStatus::Accepted:      return "accepted";
        case PredictionStatus::LowConfidence: return "low_confidence";
        case PredictionStatus::Withheld:      return "withheld";
    }
    return "unknown";
}

/**
 * ConfidenceDecomposition — five named confidence components.
 *
 * Total confidence = product of all five.
 * Individual floors are enforced independently.
 */
struct ConfidenceDecomposition {
    double input_confidence          = 1.0;  // feature validity flags
    double precursor_confidence      = 1.0;  // convergence/stability
    double coverage_confidence       = 1.0;  // OOD check
    double attribution_stability     = 1.0;  // top features stable
    double calibration_confidence    = 1.0;  // prediction in valid range

    double total() const {
        return input_confidence
             * precursor_confidence
             * coverage_confidence
             * attribution_stability
             * calibration_confidence;
    }

    /**
     * Check individual component floors.
     * Returns false if any component is below the floor.
     */
    bool passes_individual_floors(double floor = 0.1) const {
        return input_confidence       >= floor
            && precursor_confidence   >= floor
            && coverage_confidence    >= floor
            && attribution_stability  >= floor
            && calibration_confidence >= floor;
    }
};

/**
 * CalibratedPrediction — full output with provenance.
 */
struct CalibratedPrediction {
    double value = std::numeric_limits<double>::quiet_NaN();
    double raw_value = std::numeric_limits<double>::quiet_NaN();
    double uncertainty = std::numeric_limits<double>::quiet_NaN();

    PredictionStatus status = PredictionStatus::Withheld;
    ConfidenceDecomposition confidence;

    SupervisionRegime regime = SupervisionRegime::ContractSupervised;
    std::string property_name;

    // Attribution
    std::vector<pipeline::FeatureAttribution> top_contributing;
    std::vector<pipeline::FeatureAttribution> top_suppressing;

    // Provenance
    bool imputed           = false;
    bool extrapolating     = false;
    bool passes_all_checks = false;
};

/**
 * Compute input confidence from a feature vector.
 */
inline double compute_input_confidence(const pipeline::FeatureVector& fv) {
    double conf = 1.0;
    if (!fv.valid) conf *= 0.3;
    if (fv.n_imputed > 0)
        conf *= std::max(0.2, 1.0 - 0.1 * fv.n_imputed);
    // Check for NaN values in feature vector
    int nan_count = 0;
    for (double v : fv.values) {
        if (!std::isfinite(v)) ++nan_count;
    }
    if (nan_count > 0)
        conf *= std::max(0.1, 1.0 - 0.15 * nan_count);
    return std::clamp(conf, 0.0, 1.0);
}

/**
 * Compute precursor confidence from the precursor state and proxy summary.
 */
inline double compute_precursor_confidence(
    const pipeline::PropertyDatasetRow& row)
{
    double conf = row.precursor_state.convergence_confidence;
    if (!row.precursor_state.valid) conf *= 0.3;
    if (!row.proxy_summary.valid)   conf *= 0.3;
    if (!row.proxy_summary.converged) conf *= 0.8;
    return std::clamp(conf, 0.0, 1.0);
}

/**
 * Compute attribution stability by checking whether top features
 * are consistent between two predictions from slightly different models.
 *
 * Returns 1.0 if top features match, degrades for mismatches.
 */
inline double compute_attribution_stability(
    const std::vector<pipeline::FeatureAttribution>& attribs_a,
    const std::vector<pipeline::FeatureAttribution>& attribs_b,
    int top_k = 3)
{
    if (attribs_a.empty() || attribs_b.empty()) return 0.5;

    int k_a = std::min(top_k, static_cast<int>(attribs_a.size()));
    int k_b = std::min(top_k, static_cast<int>(attribs_b.size()));

    int matches = 0;
    for (int i = 0; i < k_a; ++i) {
        for (int j = 0; j < k_b; ++j) {
            if (attribs_a[i].feature_name == attribs_b[j].feature_name) {
                ++matches;
                break;
            }
        }
    }
    return static_cast<double>(matches) / std::max(k_a, 1);
}

/**
 * Compute calibration confidence: does the prediction fall in valid range?
 */
inline double compute_calibration_confidence(
    double predicted,
    const TargetContract& contract,
    const CalibrationProfile& profile)
{
    if (!std::isfinite(predicted)) return 0.0;
    double calibrated = profile.apply(predicted);
    if (!contract.in_valid_range(calibrated)) return 0.0;
    // Higher confidence when closer to center of range
    double range = contract.output_max - contract.output_min;
    if (range <= 0) return 1.0;
    double center = (contract.output_min + contract.output_max) / 2.0;
    double dist = std::abs(calibrated - center) / (range / 2.0);
    // Confidence degrades only near boundaries
    if (dist > 0.95) return 0.5;
    return 1.0;
}

/**
 * Determine prediction status from confidence decomposition and profile.
 */
inline PredictionStatus determine_status(
    const ConfidenceDecomposition& conf,
    const CalibrationProfile& profile)
{
    double total = conf.total();
    if (total < profile.abstention_threshold) return PredictionStatus::Withheld;
    if (!conf.passes_individual_floors(0.1))  return PredictionStatus::Withheld;
    if (total >= profile.min_confidence_threshold
        && conf.passes_individual_floors(0.2))
        return PredictionStatus::Accepted;
    return PredictionStatus::LowConfidence;
}

/**
 * Split feature attributions into contributing (positive) and
 * suppressing (negative) relative to the model prediction.
 */
inline void split_attributions(
    const pipeline::LinearModelCoefficients& model,
    const pipeline::FeatureVector& fv,
    std::vector<pipeline::FeatureAttribution>& contributing,
    std::vector<pipeline::FeatureAttribution>& suppressing,
    int top_k = 3)
{
    contributing.clear();
    suppressing.clear();
    int p = static_cast<int>(model.weights.size());
    if (p <= 0 || static_cast<int>(fv.values.size()) != p) return;

    std::vector<pipeline::FeatureAttribution> pos_attribs, neg_attribs;
    for (int j = 0; j < p; ++j) {
        double product = model.weights[j] * fv.values[j];
        std::string name = (j < static_cast<int>(fv.column_names.size()))
            ? fv.column_names[j] : ("feature_" + std::to_string(j));
        if (product > 0)
            pos_attribs.push_back({name, product});
        else if (product < 0)
            neg_attribs.push_back({name, std::abs(product)});
    }

    auto by_contribution = [](const pipeline::FeatureAttribution& a,
                              const pipeline::FeatureAttribution& b) {
        return a.contribution > b.contribution;
    };
    std::sort(pos_attribs.begin(), pos_attribs.end(), by_contribution);
    std::sort(neg_attribs.begin(), neg_attribs.end(), by_contribution);

    int k_pos = std::min(top_k, static_cast<int>(pos_attribs.size()));
    int k_neg = std::min(top_k, static_cast<int>(neg_attribs.size()));
    contributing.assign(pos_attribs.begin(), pos_attribs.begin() + k_pos);
    suppressing.assign(neg_attribs.begin(), neg_attribs.begin() + k_neg);
}

/**
 * calibrated_predict — full calibrated prediction with decomposed confidence.
 */
inline CalibratedPrediction calibrated_predict(
    const pipeline::LinearModelCoefficients& model,
    const pipeline::FeatureVector& fv,
    const pipeline::PropertyDatasetRow& row,
    const TargetContract& contract,
    const CalibrationProfile& profile,
    SupervisionRegime regime = SupervisionRegime::ContractSupervised)
{
    CalibratedPrediction out;
    out.property_name = contract.property_name;
    out.regime = regime;
    out.imputed = (fv.n_imputed > 0);

    // 1. Run base prediction
    auto base_pred = pipeline::predict_linear(model, fv, 0.0);
    out.raw_value = base_pred.value;
    out.value = profile.apply(base_pred.value);
    out.uncertainty = base_pred.uncertainty;

    // 2. Compute confidence decomposition
    out.confidence.input_confidence = compute_input_confidence(fv);
    out.confidence.precursor_confidence = compute_precursor_confidence(row);
    // coverage_confidence will be set by check_ood (default 1.0)
    out.confidence.attribution_stability = 1.0;  // requires second model
    out.confidence.calibration_confidence =
        compute_calibration_confidence(base_pred.value, contract, profile);

    // 3. Attribution
    split_attributions(model, fv, out.top_contributing, out.top_suppressing);

    // 4. Status
    out.status = determine_status(out.confidence, profile);
    out.passes_all_checks = (out.status == PredictionStatus::Accepted);

    return out;
}

// ============================================================================
// Phase 8F: Out-of-Distribution Detection
// ============================================================================

/**
 * FeatureEnvelope — z-score envelope from training data.
 *
 * For each feature dimension, stores mean and std from the
 * training partition.
 */
struct FeatureEnvelope {
    std::vector<double> means;
    std::vector<double> stds;
    std::vector<std::string> feature_names;
    double z_threshold = 3.0;  // z-score threshold for extrapolation
    bool   populated   = false;
};

/**
 * OODResult — outcome of an out-of-distribution check.
 */
struct OODResult {
    bool extrapolating         = false;
    int  n_features_beyond     = 0;
    double max_z_score         = 0.0;
    std::string worst_feature;
    double coverage_confidence = 1.0;
};

/**
 * Compute feature envelope from training feature vectors.
 */
inline FeatureEnvelope compute_feature_envelope(
    const std::vector<pipeline::FeatureVector>& train_X,
    double z_threshold = 3.0)
{
    FeatureEnvelope env;
    env.z_threshold = z_threshold;
    if (train_X.empty()) return env;

    int p = static_cast<int>(train_X[0].values.size());
    int n = static_cast<int>(train_X.size());
    if (p <= 0) return env;

    env.means.resize(p, 0.0);
    env.stds.resize(p, 0.0);
    env.feature_names = train_X[0].column_names;

    // Compute means
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < p; ++j)
            env.means[j] += train_X[i].values[j];
    for (int j = 0; j < p; ++j)
        env.means[j] /= n;

    // Compute stds
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < p; ++j) {
            double d = train_X[i].values[j] - env.means[j];
            env.stds[j] += d * d;
        }
    }
    for (int j = 0; j < p; ++j) {
        env.stds[j] = (n > 1) ? std::sqrt(env.stds[j] / (n - 1)) : 0.0;
    }

    env.populated = true;
    return env;
}

/**
 * Check whether an input is out of distribution.
 *
 * Only dominant features (as specified by contract) trigger
 * extrapolation tagging.
 */
inline OODResult check_ood(
    const pipeline::FeatureVector& fv,
    const FeatureEnvelope& envelope,
    const std::vector<std::string>& dominant_features)
{
    OODResult result;
    if (!envelope.populated) return result;  // no envelope → pass

    int p = static_cast<int>(envelope.means.size());
    if (static_cast<int>(fv.values.size()) != p) {
        result.extrapolating = true;
        result.coverage_confidence = 0.0;
        return result;
    }

    for (int j = 0; j < p; ++j) {
        double std_j = envelope.stds[j];
        if (std_j < 1e-15) continue;  // constant feature

        double z = std::abs(fv.values[j] - envelope.means[j]) / std_j;

        // Check if this is a dominant feature
        bool is_dominant = false;
        std::string fname = (j < static_cast<int>(envelope.feature_names.size()))
            ? envelope.feature_names[j] : "";
        for (const auto& d : dominant_features) {
            if (d == fname) { is_dominant = true; break; }
        }

        if (is_dominant && z > envelope.z_threshold) {
            ++result.n_features_beyond;
            if (z > result.max_z_score) {
                result.max_z_score = z;
                result.worst_feature = fname;
            }
        }
    }

    result.extrapolating = (result.n_features_beyond > 0);
    result.coverage_confidence = result.extrapolating ? 0.0 : 1.0;
    return result;
}

/**
 * Apply OOD result to a calibrated prediction.
 */
inline void apply_ood(CalibratedPrediction& pred, const OODResult& ood) {
    pred.extrapolating = ood.extrapolating;
    pred.confidence.coverage_confidence = ood.coverage_confidence;
    // Recompute status after coverage change
    // (coverage_confidence = 0 forces total below any threshold)
    if (ood.extrapolating) {
        pred.status = PredictionStatus::Withheld;
        pred.passes_all_checks = false;
    }
}

// ============================================================================
// Phase 8G: Property-Specific Synthetic Curricula
// ============================================================================

/**
 * CurriculumSweep — a controlled perturbation sweep for a specific
 * property family, with regime labeling.
 */
struct CurriculumSweep {
    PropertyFamily family;
    std::string swept_variable;
    std::vector<pipeline::PropertyDatasetRow> rows;
    std::vector<double> expected_targets;
    bool monotone_expected = true;
    SupervisionRegime regime = SupervisionRegime::SyntheticCalibrated;
};

/**
 * Build a rigidity-like curriculum sweep.
 * Varies cohesion from 0 to 1, expects monotone increase in rigidity.
 */
inline CurriculumSweep build_rigidity_curriculum(int n_steps = 10) {
    CurriculumSweep sweep;
    sweep.family = PropertyFamily::RigidityLike;
    sweep.swept_variable = "cohesion_proxy";
    sweep.monotone_expected = true;

    for (int i = 0; i < n_steps; ++i) {
        double t = static_cast<double>(i) / std::max(n_steps - 1, 1);

        EnsembleProxySummary ps{};
        ps.bead_count = 64;
        ps.cohesion_proxy = t;
        ps.uniformity_proxy = 0.5;
        ps.texture_proxy = 0.33;
        ps.stabilization_proxy = 0.5;
        ps.surface_sensitivity_proxy = 0.1;
        ps.mean_rho_hat = t;
        ps.mean_eta = t;
        ps.mean_state_mismatch = 1.0 - t;
        ps.mean_P2_hat = 0.33;
        ps.mean_rho = 1.0;
        ps.mean_C = 4.0;
        ps.mean_P2 = 0.33;
        ps.mean_target_f = 0.9;
        ps.n_bulk = 48;
        ps.n_edge = 16;
        ps.valid = true;
        ps.all_finite = true;
        ps.all_bounded = true;
        ps.converged = true;
        ps.eta_corr_length = 5.0;
        ps.rho_corr_length = 5.0;

        auto prec = compute_macro_precursors(ps);

        pipeline::PropertyDatasetRow row;
        row.provenance.system_id = "curriculum_rigidity_" + std::to_string(i);
        row.provenance.generator_name = "rigidity_curriculum";
        row.provenance.scenario_label = "step_" + std::to_string(i);
        row.proxy_summary = ps;
        row.precursor_state = prec;
        row.metadata.split_group = "curriculum_rigidity";
        row.metadata.target_origin = pipeline::PropertySourceType::Synthetic;

        pipeline::PropertyTarget tgt;
        tgt.value = t;
        tgt.available = true;
        tgt.source = pipeline::PropertySourceType::Synthetic;
        row.targets.push_back({"rigidity_like", tgt});

        sweep.rows.push_back(std::move(row));
        sweep.expected_targets.push_back(t);
    }
    return sweep;
}

/**
 * Build a transport-like curriculum sweep.
 * Varies texture_proxy from 0 to 1, expects monotone increase.
 */
inline CurriculumSweep build_transport_curriculum(
    PropertyFamily family = PropertyFamily::ThermalTransportLike,
    int n_steps = 10)
{
    CurriculumSweep sweep;
    sweep.family = family;
    sweep.swept_variable = "texture_proxy";
    sweep.monotone_expected = true;

    std::string target_name = property_family_name(family);

    for (int i = 0; i < n_steps; ++i) {
        double t = static_cast<double>(i) / std::max(n_steps - 1, 1);

        EnsembleProxySummary ps{};
        ps.bead_count = 64;
        ps.cohesion_proxy = 0.5;
        ps.uniformity_proxy = 0.5;
        ps.texture_proxy = t;
        ps.stabilization_proxy = 0.5;
        ps.surface_sensitivity_proxy = 0.1;
        ps.mean_rho_hat = 0.5;
        ps.mean_eta = 0.5;
        ps.mean_state_mismatch = 0.5;
        ps.mean_P2_hat = t;
        ps.mean_P2 = t;
        ps.mean_rho = 1.0;
        ps.mean_C = 4.0;
        ps.mean_target_f = 0.9;
        ps.n_bulk = 48;
        ps.n_edge = 16;
        ps.valid = true;
        ps.all_finite = true;
        ps.all_bounded = true;
        ps.converged = true;
        ps.eta_corr_length = 5.0;
        ps.rho_corr_length = 5.0;

        auto prec = compute_macro_precursors(ps);

        pipeline::PropertyDatasetRow row;
        row.provenance.system_id = "curriculum_transport_" + std::to_string(i);
        row.provenance.generator_name = "transport_curriculum";
        row.provenance.scenario_label = "step_" + std::to_string(i);
        row.proxy_summary = ps;
        row.precursor_state = prec;
        row.metadata.split_group = "curriculum_transport";
        row.metadata.target_origin = pipeline::PropertySourceType::Synthetic;

        pipeline::PropertyTarget tgt;
        tgt.value = t;
        tgt.available = true;
        tgt.source = pipeline::PropertySourceType::Synthetic;
        row.targets.push_back({target_name, tgt});

        sweep.rows.push_back(std::move(row));
        sweep.expected_targets.push_back(t);
    }
    return sweep;
}

/**
 * Build a brittleness-like curriculum sweep.
 * Varies interface_penalty from 0 to 1 (via surface_sensitivity),
 * expects monotone increase in brittleness.
 */
inline CurriculumSweep build_brittleness_curriculum(int n_steps = 10) {
    CurriculumSweep sweep;
    sweep.family = PropertyFamily::BrittlenessLike;
    sweep.swept_variable = "surface_sensitivity_proxy";
    sweep.monotone_expected = true;

    for (int i = 0; i < n_steps; ++i) {
        double t = static_cast<double>(i) / std::max(n_steps - 1, 1);

        EnsembleProxySummary ps{};
        ps.bead_count = 64;
        ps.cohesion_proxy = 0.5;
        ps.uniformity_proxy = 0.5;
        ps.texture_proxy = 0.33;
        ps.stabilization_proxy = 0.5 * (1.0 - t);  // lower stab → more brittle
        ps.surface_sensitivity_proxy = t;
        ps.mean_rho_hat = 0.5;
        ps.mean_eta = 0.5 * (1.0 - t);
        ps.mean_state_mismatch = 0.5;
        ps.mean_P2_hat = 0.33;
        ps.mean_rho = 1.0;
        ps.mean_C = 4.0;
        ps.mean_P2 = 0.33;
        ps.mean_target_f = 0.9;
        ps.n_bulk = 48;
        ps.n_edge = 16;
        ps.bulk_edge_rho_gap = t * 0.3;
        ps.bulk_edge_eta_gap = t * 0.3;
        ps.valid = true;
        ps.all_finite = true;
        ps.all_bounded = true;
        ps.converged = true;
        ps.eta_corr_length = 5.0;
        ps.rho_corr_length = 5.0;

        auto prec = compute_macro_precursors(ps);

        pipeline::PropertyDatasetRow row;
        row.provenance.system_id = "curriculum_brittleness_" + std::to_string(i);
        row.provenance.generator_name = "brittleness_curriculum";
        row.provenance.scenario_label = "step_" + std::to_string(i);
        row.proxy_summary = ps;
        row.precursor_state = prec;
        row.metadata.split_group = "curriculum_brittleness";
        row.metadata.target_origin = pipeline::PropertySourceType::Synthetic;

        pipeline::PropertyTarget tgt;
        tgt.value = t;
        tgt.available = true;
        tgt.source = pipeline::PropertySourceType::Synthetic;
        row.targets.push_back({"brittleness_like", tgt});

        sweep.rows.push_back(std::move(row));
        sweep.expected_targets.push_back(t);
    }
    return sweep;
}

// ============================================================================
// Phase 8H: Target Promotion and Legitimacy Management
// ============================================================================

/**
 * LegitimacyState — legitimacy state for a property target.
 *
 * ProxyOnly:           -like suffix, contract/synthetic only
 * CalibratedRelative:  -like suffix, monotonicity+signs+domain
 * ExternallyAnchored:  suffix dropped, external data+bounded error
 */
enum class LegitimacyState {
    ProxyOnly,
    CalibratedRelative,
    ExternallyAnchored
};

inline const char* legitimacy_state_name(LegitimacyState s) {
    switch (s) {
        case LegitimacyState::ProxyOnly:           return "proxy_only";
        case LegitimacyState::CalibratedRelative:  return "calibrated_relative";
        case LegitimacyState::ExternallyAnchored:  return "externally_anchored";
    }
    return "unknown";
}

/**
 * PromotionEvidence — evidence gathered during promotion evaluation.
 */
struct PromotionEvidence {
    bool monotonicity_preserved  = false;
    bool signs_consistent        = false;
    bool domain_documented       = false;
    int  sign_violations         = 0;
    bool has_external_data       = false;
    bool bounded_residual_error  = false;
    bool validated_rejection     = false;
    double coverage_floor        = 0.0;  // min coverage confidence in test set
};

/**
 * DemotionCheck — checks performed to detect demotion conditions.
 */
struct DemotionCheck {
    bool calibration_drift_detected    = false;
    bool contract_violation_detected   = false;
    bool coverage_degraded             = false;
    bool attribution_inconsistent      = false;

    bool should_demote() const {
        return calibration_drift_detected
            || contract_violation_detected
            || coverage_degraded
            || attribution_inconsistent;
    }
};

/**
 * TargetLegitimacy — current legitimacy state for a target with
 * promotion/demotion provenance.
 */
struct TargetLegitimacy {
    std::string property_name;
    LegitimacyState state = LegitimacyState::ProxyOnly;
    PromotionEvidence evidence;
    std::vector<std::string> demotion_history;

    /**
     * The display name: includes -like suffix unless ExternallyAnchored.
     */
    std::string display_name() const {
        if (state == LegitimacyState::ExternallyAnchored) {
            // Remove _like suffix if present
            std::string name = property_name;
            const std::string suffix = "_like";
            if (name.size() > suffix.size() &&
                name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0) {
                return name.substr(0, name.size() - suffix.size());
            }
            return name;
        }
        return property_name;
    }
};

/**
 * Check whether a target can be promoted from ProxyOnly to CalibratedRelative.
 *
 * Requires:
 *   - Preserved monotonicity on relevant sweeps
 *   - Consistent sign behavior with contract
 *   - Documented domain of validity
 */
inline bool can_promote_to_calibrated(
    const PromotionEvidence& evidence)
{
    return evidence.monotonicity_preserved
        && evidence.signs_consistent
        && evidence.domain_documented;
}

/**
 * Check whether a target can be promoted from CalibratedRelative
 * to ExternallyAnchored.
 *
 * Requires all CalibratedRelative criteria plus:
 *   - External data coverage across support region
 *   - Bounded and reported residual error
 *   - Validated rejection behavior on out-of-domain inputs
 */
inline bool can_promote_to_anchored(
    const PromotionEvidence& evidence)
{
    return can_promote_to_calibrated(evidence)
        && evidence.has_external_data
        && evidence.bounded_residual_error
        && evidence.validated_rejection;
}

/**
 * Evaluate legitimacy state based on evidence.
 */
inline LegitimacyState evaluate_legitimacy(
    const PromotionEvidence& evidence)
{
    if (can_promote_to_anchored(evidence))
        return LegitimacyState::ExternallyAnchored;
    if (can_promote_to_calibrated(evidence))
        return LegitimacyState::CalibratedRelative;
    return LegitimacyState::ProxyOnly;
}

/**
 * Check for demotion conditions.
 */
inline DemotionCheck check_demotion(
    const pipeline::LinearModelCoefficients& model,
    const TargetContract& contract,
    const FeatureEnvelope& envelope,
    double /*coverage_floor*/ = 0.3)
{
    DemotionCheck result;

    // Contract violation: sign violations
    int violations = contract.count_sign_violations(model);
    if (violations > 0) result.contract_violation_detected = true;

    // Coverage degradation: if envelope has very narrow stds
    if (envelope.populated) {
        int narrow_count = 0;
        for (double s : envelope.stds) {
            if (s < 1e-10) ++narrow_count;
        }
        double frac_narrow = static_cast<double>(narrow_count) / 
            std::max(1, static_cast<int>(envelope.stds.size()));
        if (frac_narrow > 0.5) result.coverage_degraded = true;
    }

    return result;
}

/**
 * Multi-dimensional grouped split.
 *
 * Groups rows jointly on material family, morphology class,
 * synthetic sweep family, precursor template identity, and source
 * provenance block. No two partitions may share rows generated from
 * the same base configuration.
 */
inline pipeline::SplitAssignment multi_grouped_split(
    const std::vector<pipeline::PropertyDatasetRow>& dataset,
    double train_frac = 0.7,
    double val_frac   = 0.15,
    uint64_t seed     = 42)
{
    // Composite group key: family + generator + split_group + scenario
    auto composite_key = [](const pipeline::PropertyDatasetRow& row) -> std::string {
        return row.metadata.family_label
             + "|" + row.provenance.generator_name
             + "|" + row.metadata.split_group
             + "|" + row.provenance.fragment_family;
    };

    // Build temporary rows with composite keys for grouped_split
    std::vector<pipeline::PropertyDatasetRow> keyed = dataset;
    for (auto& row : keyed)
        row.metadata.split_group = composite_key(row);

    return pipeline::grouped_split(keyed, train_frac, val_frac, seed);
}

} // namespace coarse_grain::calibration
