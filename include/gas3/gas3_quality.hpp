/**
 * gas3_quality.hpp
 * ----------------
 * Quality Scoring and Tier Classification for Gas3 Module.
 *
 * Implements explicit penalty-based scoring (not vibes):
 *   Start at 100, subtract for:
 *     not converged:          -100
 *     residual > threshold:   -10 to -40  (scaled)
 *     nonphysical property:   -100
 *     near singular Jacobian: -20
 *     oscillatory solve:      -15
 *     outside model region:   -10
 *     critical proximity:     -20
 *
 * Tier classification:
 *     Q4: 90-100   reference-grade
 *     Q3: 75-89    production-grade
 *     Q2: 55-74    usable
 *     Q1: 25-54    weak
 *     Q0: <25      failed
 *
 * Anti-black-box: every penalty is named, every deduction is traceable.
 */

#pragma once

#include "gas3_state_record.hpp"
#include <string>
#include <cmath>
#include <sstream>

namespace vsepr {
namespace gas3 {

// ============================================================================
// Quality penalty catalogue
// ============================================================================

struct QualityPenalties {
    double not_converged      = 0.0;   // -100 if !converged
    double high_residual      = 0.0;   // -10 to -40 scaled
    double nonphysical        = 0.0;   // -100 for negative V, P, T
    double near_singular      = 0.0;   // -20 if iterations >= max-5
    double oscillatory        = 0.0;   // -15 if solver oscillated (high iters, small change)
    double outside_model      = 0.0;   // -10 if Tr < 0.5 or Pr > 10 for cubic EOS
    double critical_proximity = 0.0;   // -20 if 0.8 < Tr < 1.2 and unstable Z

    double total() const {
        return not_converged + high_residual + nonphysical +
               near_singular + oscillatory + outside_model +
               critical_proximity;
    }

    std::string describe() const {
        std::ostringstream ss;
        if (not_converged != 0.0)      ss << "NOT_CONVERGED;";
        if (high_residual != 0.0)      ss << "HIGH_RESIDUAL(" << high_residual << ");";
        if (nonphysical != 0.0)        ss << "NONPHYSICAL;";
        if (near_singular != 0.0)      ss << "NEAR_SINGULAR;";
        if (oscillatory != 0.0)        ss << "OSCILLATORY;";
        if (outside_model != 0.0)      ss << "OUTSIDE_MODEL;";
        if (critical_proximity != 0.0) ss << "CRITICAL_ZONE;";
        return ss.str();
    }
};

// ============================================================================
// Score a single state record
// ============================================================================

inline QualityPenalties compute_penalties(const GasStateRecord& r) {
    QualityPenalties p{};

    // 1. Convergence
    if (!r.converged) {
        p.not_converged = -100.0;
        return p;  // Automatic Q0 — no further analysis meaningful
    }

    // 2. Physical validity
    if (r.V_m3 <= 0.0 || r.T_K <= 0.0 || r.P_Pa <= 0.0 ||
        r.Z <= 0.0 || std::isnan(r.Z) || std::isinf(r.Z)) {
        p.nonphysical = -100.0;
        return p;
    }

    // 3. Residual-based penalty
    //    We define residual as |P_calc - P_input| / P_input
    if (!std::isnan(r.residual)) {
        double abs_r = std::abs(r.residual);
        if (abs_r > 1e-3)       p.high_residual = -40.0;
        else if (abs_r > 1e-5)  p.high_residual = -25.0;
        else if (abs_r > 1e-8)  p.high_residual = -10.0;
    }

    // 4. Near-singular (solver struggled)
    if (r.iterations >= 55) {  // max_iter is typically 60
        p.near_singular = -20.0;
    }

    // 5. Oscillatory convergence (many iterations but converged)
    if (r.iterations >= 40 && r.converged) {
        p.oscillatory = -15.0;
    }

    // 6. Outside recommended model region
    //    VdW/RK unreliable at very low Tr or very high Pr
    if (!std::isnan(r.Tr) && !std::isnan(r.Pr)) {
        if (r.Tr < 0.5 || r.Pr > 10.0) {
            p.outside_model = -10.0;
        }
    }

    // 7. Critical proximity with unstable Z
    if (!std::isnan(r.Tr)) {
        if (r.Tr > 0.8 && r.Tr < 1.2 && r.Z < 0.5) {
            p.critical_proximity = -20.0;
        }
    }

    return p;
}

inline double compute_quality_score(const GasStateRecord& r,
                                     QualityPenalties* out_pen = nullptr) {
    auto pen = compute_penalties(r);
    double score = 100.0 + pen.total();
    if (score < 0.0) score = 0.0;
    if (score > 100.0) score = 100.0;
    if (out_pen) *out_pen = pen;
    return score;
}

inline QualityTier score_to_tier(double score) {
    if (score >= 90.0) return QualityTier::Q4;
    if (score >= 75.0) return QualityTier::Q3;
    if (score >= 55.0) return QualityTier::Q2;
    if (score >= 25.0) return QualityTier::Q1;
    return QualityTier::Q0;
}

// Score and classify a record in-place
inline void score_record(GasStateRecord& r) {
    QualityPenalties pen;
    r.quality_score = compute_quality_score(r, &pen);
    r.quality_tier = score_to_tier(r.quality_score);
    r.warning_flags = pen.describe();
}

} // namespace gas3
} // namespace vsepr
