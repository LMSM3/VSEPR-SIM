/**
 * gap_classifier.cpp  --  WO-74A / WO-74C
 *
 * Single unified gap-classifier translation unit (consolidated from WO-74A).
 *
 * (1) classify_gap() / classify_record_gap()  — legacy 3-label free functions.
 *     Kept for backward compatibility with existing call sites and tests.
 *
 * (2) ThresholdGapClassifier  — IGapClassifier wrapper around classify_gap().
 *     Self-registers as "threshold" in ModuleRegistry<IGapClassifier>.
 *
 * (3) GapClassifier class — 14-label adaptive classifier (lambda + energy).
 *     Declared in gap_classifier.hpp; implemented here after WO-74A merge.
 *
 * (4) AdaptiveGapClassifier  — IGapClassifier wrapper around GapClassifier.
 *     Self-registers as "adaptive" in ModuleRegistry<IGapClassifier>.
 *
 * Labels (per gap_label_registry.csv):
 *   3-label set: good_match / overextended_field / underresolved_scale
 *  14-label set: good_match, insufficient_data, lambda_large_high/low,
 *                both_high/low, lambda_high/low, energy_large_high/low,
 *                lambda_ok_energy_gap
 */

#include "multiscale/continual_run.hpp"
#include "multiscale/gap_classifier.hpp"
#include "vsim/analysis/i_gap_classifier.hpp"
#include "vsim/module_registry.hpp"

#include <cmath>
#include <string>

namespace vsepr {
namespace multiscale {

// ============================================================================
// Legacy free functions (backward-compatible API)
// ============================================================================

std::string classify_gap(double lambda_sim,
                         double lambda_ref,
                         double tolerance_nm)
{
    const double diff = lambda_sim - lambda_ref;
    // Small relative epsilon guards IEEE 754 rounding at exact boundaries
    // (e.g. 1.05 - 1.00 computes as ~0.05000000000000004, not exactly 0.05).
    const double eps  = tolerance_nm * 1e-9;
    if (std::abs(diff) <= tolerance_nm + eps) return "good_match";
    if (diff > tolerance_nm + eps)            return "overextended_field";
    return "underresolved_scale";
}

void classify_record_gap(ContinualRunRecord& rec, double tolerance_nm)
{
    rec.gap_label = classify_gap(rec.Lambda_nm, rec.prior_lambda_nm, tolerance_nm);
}

// ============================================================================
// ThresholdGapClassifier — IGapClassifier strategy "threshold"
// ============================================================================
// Wraps the 3-label classify_gap() free function.
// Uses GapInput::lambda_sim, lambda_emp, and tolerance_nm.
// Energy fields (e_sim, e_emp) are intentionally ignored by this strategy.

class ThresholdGapClassifier final : public IGapClassifier {
public:
    [[nodiscard]] std::string_view strategy_name() const override { return "threshold"; }

    [[nodiscard]] std::string_view description() const override {
        return "3-label threshold classifier (good_match / overextended_field / underresolved_scale)";
    }

    [[nodiscard]] GapClassificationResult classify(const GapInput& in) const override {
        GapClassificationResult r;
        const std::string label = classify_gap(in.lambda_sim, in.lambda_emp, in.tolerance_nm);
        r.gap_label      = label;
        r.epsilon_lambda = in.lambda_sim - in.lambda_emp;
        r.epsilon_energy = 0.0;  // not used by this strategy
        if (label == "good_match")          { r.severity = 0; r.action = "none"; }
        else if (label == "overextended_field") { r.severity = 2; r.action = "flag_review"; }
        else                                { r.severity = 2; r.action = "rerun_finer"; }
        return r;
    }
};

// ============================================================================
// GapClassifier — 14-label adaptive classifier (WO-74A merge from wo74c)
// ============================================================================

GapClassifier::GapClassifier(const GapThresholds& t) : t_(t) {}

GapClassifier::Mag GapClassifier::magnitude(double epsilon, double reference) const
{
    const double ref  = std::abs(reference);
    const double eps  = std::abs(epsilon);
    const double frac = (ref > 1e-15) ? eps / ref : eps;
    if (frac < t_.small_fraction)    return Mag::SMALL;
    if (frac < t_.moderate_fraction) return Mag::MODERATE;
    return Mag::LARGE;
}

GapClassificationResult GapClassifier::classify(double Lambda_sim,
                                                  double Lambda_emp,
                                                  double E_sim,
                                                  double E_emp,
                                                  int    reference_count) const
{
    GapClassificationResult r;
    r.epsilon_lambda = Lambda_sim - Lambda_emp;
    r.epsilon_energy = E_sim - E_emp;

    if (reference_count < 3) {
        r.gap_label = "insufficient_data";
        r.severity  = 0;
        r.action    = "expand_db";
        return r;
    }

    const Mag  lmag = magnitude(r.epsilon_lambda, Lambda_emp);
    const Mag  emag = magnitude(r.epsilon_energy, E_emp);
    const bool lpos = r.epsilon_lambda >= 0.0;
    const bool epos = r.epsilon_energy >= 0.0;

    // Both within tolerance
    if (lmag == Mag::SMALL && emag == Mag::SMALL) {
        r.gap_label = "good_match"; r.severity = 0; r.action = "none";
        return r;
    }

    // Lambda large — dominates
    if (lmag == Mag::LARGE) {
        r.gap_label = lpos ? "lambda_large_high" : "lambda_large_low";
        r.severity  = 3;
        r.action    = lpos ? "rerun_constrain" : "rerun_finer";
        return r;
    }

    // Lambda moderate
    if (lmag == Mag::MODERATE) {
        if (emag == Mag::MODERATE) {
            r.gap_label = (lpos && epos)   ? "both_high"   :
                          (!lpos && !epos)  ? "both_low"    :
                          lpos             ? "lambda_high" : "lambda_low";
            r.severity  = (r.gap_label == "both_low") ? 3 : 2;
            r.action    = (r.gap_label == "lambda_low" || r.gap_label == "both_low")
                          ? "rerun_finer" : "flag_review";
            return r;
        }
        // lambda moderate, energy small or large
        if (emag == Mag::LARGE) {
            r.gap_label = epos ? "energy_large_high" : "energy_large_low";
            r.severity  = 3;
            r.action    = epos ? "rerun_energy_cap" : "rerun_energy_relax";
            return r;
        }
        r.gap_label = lpos ? "lambda_high" : "lambda_low";
        r.severity  = lpos ? 1 : 2;
        r.action    = lpos ? "flag_review" : "rerun_finer";
        return r;
    }

    // Lambda small — energy gap is decisive
    if (emag == Mag::LARGE) {
        r.gap_label = epos ? "energy_large_high" : "energy_large_low";
        r.severity  = 3;
        r.action    = epos ? "rerun_energy_cap" : "rerun_energy_relax";
        return r;
    }
    if (emag == Mag::MODERATE) {
        r.gap_label = "lambda_ok_energy_gap";
        r.severity  = 1;
        r.action    = "flag_review";
        return r;
    }

    // Default
    r.gap_label = "good_match"; r.severity = 0; r.action = "none";
    return r;
}

// ============================================================================
// AdaptiveGapClassifier — IGapClassifier strategy "adaptive"
// ============================================================================

class AdaptiveGapClassifier final : public IGapClassifier {
public:
    [[nodiscard]] std::string_view strategy_name() const override { return "adaptive"; }

    [[nodiscard]] std::string_view description() const override {
        return "14-label adaptive classifier (lambda + energy, reference_count guard)";
    }

    [[nodiscard]] GapClassificationResult classify(const GapInput& in) const override {
        return clf_.classify(in.lambda_sim, in.lambda_emp,
                             in.e_sim, in.e_emp, in.ref_count);
    }

private:
    GapClassifier clf_;  // default GapThresholds
};

} // namespace multiscale
} // namespace vsepr

// Self-register "threshold" strategy at static-init time.
static vsim::AutoRegister<vsepr::multiscale::ThresholdGapClassifier,
                          vsepr::multiscale::IGapClassifier>
    s_threshold_reg("threshold");

// Self-register "adaptive" strategy at static-init time.
static vsim::AutoRegister<vsepr::multiscale::AdaptiveGapClassifier,
                          vsepr::multiscale::IGapClassifier>
    s_adaptive_reg("adaptive");
