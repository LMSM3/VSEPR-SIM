#pragma once
/**
 * alpha_stability.hpp
 * ===================
 * Ties the alpha model into the nuclear stability module.
 *
 * Provides alpha_predict_checked() — a drop-in for alpha_predict() that
 * silently runs a nuclear stability check and returns an annotated result.
 *
 * The stability check:
 *   1. Computes StabilityInfo from Z (no external data).
 *   2. Tags the prediction with a confidence score [0,1].
 *   3. For superheavy/very-short-lived elements, flags the prediction
 *      as "theoretical" — not experimentally verifiable.
 *   4. Detects if Z is at or near a magic proton number (extra stability)
 *      and reports it as a nuclear-shell bonus.
 *
 * This is the "undocumented stability check" — it runs silently inside
 * the prediction pipeline and doesn't alter the predicted value, only
 * annotates it.  The caller can choose to use the annotations or ignore them.
 *
 * For the AlphaCalibrator: stability confidence gates whether a new
 * measurement should be accepted.  Measurements of superheavy elements
 * are inherently uncertain, so the calibrator is more lenient (wider
 * threshold) for low-confidence predictions.
 *
 * Usage:
 *   auto result = alpha_predict_checked(80);   // Hg
 *   result.alpha;         // 7.127 (same as alpha_predict)
 *   result.confidence;    // 1.0 (Hg is stable)
 *   result.cls;           // Stable
 *   result.is_theoretical; // false
 *   result.note;          // ""
 *
 *   auto result2 = alpha_predict_checked(118);  // Og
 *   result2.confidence;   // 0.05
 *   result2.is_theoretical; // true
 *   result2.note;         // "Superheavy (Z=118), t½ < 1 ms"
 */

#include "alpha_model.hpp"
#include "nuclear_stability.hpp"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>

namespace atomistic {
namespace polarization {

// ============================================================================
// Checked prediction result
// ============================================================================

struct AlphaPredictionResult {
    uint32_t Z;
    double   alpha;              ///< Predicted polarizability (Å³)
    double   confidence;         ///< [0,1] overall confidence
    nuclear::StabilityClass cls; ///< Nuclear stability class
    nuclear::DecayType dominant_decay;
    uint32_t A_most_stable;      ///< Predicted most stable mass number
    double   binding_per_nucleon;///< B/A in MeV
    double   fissility;          ///< Z²/A
    bool     is_theoretical;     ///< True if element is too short-lived to measure
    bool     is_magic_Z;         ///< True if Z is a magic proton number
    char     note[128];          ///< Human-readable annotation
};

// ============================================================================
// Core function: alpha_predict with silent stability check
// ============================================================================

/**
 * Predict polarizability and annotate with nuclear stability info.
 *
 * This function:
 *   1. Calls alpha_predict(Z, params) for the polarizability value.
 *   2. Calls nuclear::stability_of(Z) for the stability annotation.
 *   3. Combines both into an AlphaPredictionResult.
 *
 * The predicted alpha is NEVER modified by the stability check.
 * The check only adds metadata (confidence, theoretical flag, note).
 */
inline AlphaPredictionResult alpha_predict_checked(
        uint32_t Z,
        const AlphaModelParams& params = {}) noexcept {
    AlphaPredictionResult r{};
    r.Z     = Z;
    r.alpha = alpha_predict(Z, params);
    r.note[0] = '\0';

    auto si = nuclear::stability_of(Z);
    r.confidence         = si.alpha_confidence;
    r.cls                = si.cls;
    r.dominant_decay     = si.dominant_decay;
    r.A_most_stable      = si.A_most_stable;
    r.binding_per_nucleon = si.binding_energy_per_nucleon;
    r.fissility          = si.fissility;
    r.is_magic_Z         = si.is_magic_Z;

    // Determine if element is "theoretical" (not experimentally measurable)
    switch (si.cls) {
        case nuclear::StabilityClass::Stable:
            r.is_theoretical = false;
            break;
        case nuclear::StabilityClass::PrimordialLong:
            r.is_theoretical = false;
            std::snprintf(r.note, sizeof(r.note),
                "Primordial (t½ > 4.5 Gy), decay=%s",
                nuclear::decay_name(si.dominant_decay));
            break;
        case nuclear::StabilityClass::Radioactive:
            r.is_theoretical = false;
            std::snprintf(r.note, sizeof(r.note),
                "Radioactive (log₁₀ t½ ≈ %.0f s), decay=%s",
                si.log10_halflife_s,
                nuclear::decay_name(si.dominant_decay));
            break;
        case nuclear::StabilityClass::VeryShortLived:
            r.is_theoretical = true;
            std::snprintf(r.note, sizeof(r.note),
                "Very short-lived (t½ < 1 s), decay=%s, confidence=%.0f%%",
                nuclear::decay_name(si.dominant_decay),
                si.alpha_confidence * 100.0);
            break;
        case nuclear::StabilityClass::Superheavy:
            r.is_theoretical = true;
            std::snprintf(r.note, sizeof(r.note),
                "Superheavy (Z=%u), t½ < 1 ms, prediction is theoretical",
                Z);
            break;
    }

    return r;
}

// ============================================================================
// Stability-aware calibration threshold
//
// For the AlphaCalibrator: widen the acceptance threshold for elements
// where the prediction confidence is low (unstable / theoretical).
//
// Base threshold (1%) is used for stable elements.
// Unstable elements get threshold scaled by 1/confidence, up to 20%.
// This allows the calibrator to accept noisier measurements for
// superheavy elements while remaining strict for well-known elements.
// ============================================================================

inline double calibration_threshold_for(uint32_t Z,
                                        double base_threshold = 0.01) noexcept {
    double conf = nuclear::alpha_prediction_confidence(Z);
    if (conf <= 0.0) return 0.20;  // maximum 20% threshold
    double threshold = base_threshold / conf;
    return std::min(threshold, 0.20);
}

// ============================================================================
// Batch diagnostic: print stability info for a range of elements
// ============================================================================

inline void print_stability_table(uint32_t Z_start = 1, uint32_t Z_end = 118) {
    std::printf("%-4s %-4s %-16s %-10s %8s %6s %6s %5s %6s\n",
                "Z", "A", "Class", "Decay", "log₁₀t½", "B/A", "fiss",
                "magic", "conf");
    std::printf("---- ---- ---------------- ---------- -------- "
                "------ ------ ----- ------\n");
    for (uint32_t Z = Z_start; Z <= Z_end; ++Z)
        nuclear::print_stability(Z);
}

} // namespace polarization
} // namespace atomistic
