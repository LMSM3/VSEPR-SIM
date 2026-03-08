#pragma once
/**
 * alpha_calibrator.hpp
 * ====================
 * Runtime self-calibrating override table for atomic polarizabilities.
 *
 * Architecture:
 *
 *   alpha_predict(Z)         ← offline fitted model (13 params, baked in)
 *         │
 *         ▼
 *   AlphaCalibrator::get(Z)  ← returns override if set, else model value
 *         │
 *         ├── calibrate(Z, new_val):
 *         │     • computes delta = |new_val - current| / current
 *         │     • if delta <= ACCEPT_THRESHOLD (1%): accepts → override[Z] = new_val
 *         │     • if delta > ACCEPT_THRESHOLD: rejects, returns false
 *         │     • subsequent refinements are checked against the override,
 *         │       not the original model — enabling progressive convergence
 *         │
 *         └── alpha_predict_calibrated(Z) ← drop-in for alpha_predict(Z)
 *
 * Design decisions:
 *
 *   1. The 1% threshold is physical: small refinements (better experimental
 *      value, SCF-derived effective alpha) are trusted; large deviations
 *      (wrong element, unit error, outlier) are silently rejected.
 *
 *   2. The calibrator holds one double per Z (Z=1..118), ~1 KB.  Zero-cost
 *      when no overrides are set: get() hot-path is a single branch.
 *
 *   3. update_count[Z] tracks how many accepted calibrations have occurred.
 *      This lets callers detect when a value has been confirmed multiple times.
 *
 *   4. Thread safety: NOT thread-safe by default.  In multi-threaded contexts
 *      wrap calibrate() calls in a mutex.  get() is read-only and safe.
 *
 *   5. Persistence: in-memory only.  The snapshot() / restore() API allows
 *      callers to serialise the override table to a file if needed.
 *
 * Usage:
 *   #include "alpha_calibrator.hpp"
 *   using namespace atomistic::polarization;
 *
 *   // Global calibrator (or per-simulation instance)
 *   AlphaCalibrator cal;
 *
 *   // Feed a new measurement / SCF-derived value
 *   CalibrateResult r = cal.calibrate(8, 0.807);  // O, slightly refined
 *   // r.accepted == true  (0.807 is within 1% of model 0.802)
 *   // r.delta_pct == 0.62%
 *
 *   // Use in SCF solver
 *   double a_O = cal.get(8);  // returns 0.807, not 0.802
 *
 *   // Drop-in for alpha_predict:
 *   double a = alpha_predict_calibrated(8, cal);
 *
 * References:
 *   See alpha_model.hpp for the base model and fitted parameters.
 *   See tests/test_alpha_calibrator.cpp for validation.
 */

#include "alpha_model.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>

namespace atomistic {
namespace polarization {

// ============================================================================
// Result type
// ============================================================================

struct CalibrateResult {
    bool     accepted;      ///< true if override was set/updated
    double   delta_pct;     ///< |new - current| / current * 100 (always >= 0)
    double   old_value;     ///< value before this call
    double   new_value;     ///< value after this call (same as old if rejected)
    uint32_t update_count;  ///< cumulative accepted calibrations for this Z
};

// ============================================================================
// AlphaCalibrator
// ============================================================================

class AlphaCalibrator {
public:
    // 1% acceptance threshold — class-level constant, overridable per instance
    static constexpr double DEFAULT_THRESHOLD = 0.01;

    explicit AlphaCalibrator(double threshold = DEFAULT_THRESHOLD,
                             const AlphaModelParams& params = {}) noexcept
        : threshold_(threshold)
        , params_(params)
    {
        overrides_.fill(0.0);
        counts_.fill(0u);
        active_.fill(false);
    }

    // ── Core API ─────────────────────────────────────────────────────────────

    /**
     * Attempt to calibrate element Z with a new polarizability value.
     *
     * Accepts the new value if:
     *   |new_alpha - current| / current <= threshold_
     *
     * "current" is the override if one exists, else the model prediction.
     * This means progressive refinement converges smoothly: each accepted
     * update narrows the range for the next one.
     *
     * Returns a CalibrateResult describing what happened.
     */
    CalibrateResult calibrate(uint32_t Z, double new_alpha) noexcept {
        if (Z == 0 || Z > 118 || new_alpha <= 0.0) {
            return {false, 0.0, 0.0, new_alpha, 0u};
        }

        const double current   = get(Z);
        const double delta     = std::abs(new_alpha - current) / current;
        const double delta_pct = delta * 100.0;
        const bool   accepted  = (delta <= threshold_);

        if (accepted) {
            overrides_[Z] = new_alpha;
            active_[Z]    = true;
            ++counts_[Z];
        }

        return {accepted, delta_pct, current,
                accepted ? new_alpha : current,
                counts_[Z]};
    }

    /**
     * Get the best known polarizability for element Z.
     * Returns the override if one has been accepted, else alpha_predict(Z).
     */
    double get(uint32_t Z) const noexcept {
        if (Z > 0 && Z <= 118 && active_[Z])
            return overrides_[Z];
        return alpha_predict(Z, params_);
    }

    // ── Override management ───────────────────────────────────────────────

    /// Force-set an override, bypassing the threshold check.
    /// Use when you have a trusted high-quality experimental value.
    void set_override(uint32_t Z, double alpha) noexcept {
        if (Z == 0 || Z > 118 || alpha <= 0.0) return;
        overrides_[Z] = alpha;
        active_[Z]    = true;
        // Note: does NOT increment counts_ — this is a forced set, not a
        // calibration event.
    }

    /// Remove the override for element Z (reverts to model prediction).
    void reset(uint32_t Z) noexcept {
        if (Z == 0 || Z > 118) return;
        overrides_[Z] = 0.0;
        active_[Z]    = false;
        counts_[Z]    = 0u;
    }

    /// Remove all overrides.
    void reset_all() noexcept {
        overrides_.fill(0.0);
        active_.fill(false);
        counts_.fill(0u);
    }

    // ── Introspection ─────────────────────────────────────────────────────

    bool     is_overridden(uint32_t Z)   const noexcept { return Z > 0 && Z <= 118 && active_[Z]; }
    uint32_t update_count(uint32_t Z)    const noexcept { return (Z > 0 && Z <= 118) ? counts_[Z] : 0u; }
    double   threshold()                 const noexcept { return threshold_; }

    /// Count of elements currently carrying an override.
    uint32_t override_count() const noexcept {
        uint32_t n = 0;
        for (uint32_t Z = 1; Z <= 118; ++Z)
            if (active_[Z]) ++n;
        return n;
    }

    // ── Snapshot / restore ────────────────────────────────────────────────

    struct Snapshot {
        std::array<double,   119> overrides;
        std::array<uint32_t, 119> counts;
        std::array<bool,     119> active;
        double threshold;
    };

    Snapshot snapshot() const noexcept {
        return {overrides_, counts_, active_, threshold_};
    }

    void restore(const Snapshot& s) noexcept {
        overrides_  = s.overrides;
        counts_     = s.counts;
        active_     = s.active;
        threshold_  = s.threshold;
    }

    // ── Diagnostics ───────────────────────────────────────────────────────

    /// Print a summary of all active overrides.
    void print_overrides(const char* prefix = "") const {
        uint32_t n = 0;
        for (uint32_t Z = 1; Z <= 118; ++Z) {
            if (!active_[Z]) continue;
            double model = alpha_predict(Z, params_);
            double over  = overrides_[Z];
            double diff  = (over - model) / model * 100.0;
            std::printf("%sZ=%3u  model=%.4f  override=%.4f  delta=%+.2f%%  n=%u\n",
                        prefix, Z, model, over, diff, counts_[Z]);
            ++n;
        }
        if (n == 0) std::printf("%s(no active overrides)\n", prefix);
    }

private:
    std::array<double,   119> overrides_;
    std::array<uint32_t, 119> counts_;
    std::array<bool,     119> active_;
    double           threshold_;
    AlphaModelParams params_;
};

// ============================================================================
// Free-function drop-in for alpha_predict
// ============================================================================

/**
 * Drop-in replacement for alpha_predict(Z).
 * Returns the calibrator's best known value for Z.
 * Falls back to alpha_predict(Z) if no calibrator override is set.
 */
inline double alpha_predict_calibrated(uint32_t Z,
                                       const AlphaCalibrator& cal) noexcept {
    return cal.get(Z);
}

/**
 * Global (process-wide) calibrator instance.
 *
 * Optional convenience: allows code to call
 *   atomistic::polarization::global_calibrator().calibrate(Z, val)
 * without passing a calibrator everywhere.
 *
 * The global calibrator is NOT used by alpha_predict() itself —
 * callers must explicitly pass it or use alpha_predict_calibrated().
 */
inline AlphaCalibrator& global_calibrator() noexcept {
    static AlphaCalibrator instance;
    return instance;
}

} // namespace polarization
} // namespace atomistic
