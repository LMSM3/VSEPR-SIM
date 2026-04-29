#pragma once
/**
 [VSEPR-SIM 4.0.4.04]
 * ═════════════════════════════════════════════════════════
 * Normalized descriptor of how tightly organized or spatially
 * efficient the final structure is.
 *
 * From section_v4_transition.tex Eq. (4.1):
 *
 *   C_compact = b₁·ρ* + b₂·η* + b₃·R*_coord + b₄·D*_macro − b₅·Φ*_void
 *
 * Default weights: {0.25, 0.25, 0.20, 0.15, 0.15}
 *
 * All starred terms are normalized ∈ [0,1].  Final C_compact ∈ [0,1].
 *
 * C++26 features:
 *   - Contract emulation
 *   - Erroneous-behaviour pattern init
 *   - Trailing return types
 *
 * Anti-black-box: all intermediate terms exposed.
 */

#include "formation_record.hpp"

#include <algorithm>
#include <cmath>

namespace v4 {

// ============================================================================
// Compactness Components
// ============================================================================

struct CompactnessComponents {
    double rho_star{0.0};       // effective density / occupancy
    double eta_star{0.0};       // packing efficiency
    double R_coord_star{0.0};   // coordination regularity
    double D_macro_star{0.0};   // macro deformation / stiffness consistency
    double Phi_void_star{0.0};  // void fraction / looseness penalty
};

// ============================================================================
// Compactness Weights
// ============================================================================

struct CompactnessWeights {
    double b1{0.25};  // density
    double b2{0.25};  // packing
    double b3{0.20};  // coordination
    double b4{0.15};  // macro deformation
    double b5{0.15};  // void penalty

    auto valid() const -> bool {
        double s = b1 + b2 + b3 + b4;
        return s > b5 && (s + b5) > 0.0;
    }
};

inline constexpr CompactnessWeights DEFAULT_COMPACT_WEIGHTS{};

// ============================================================================
// Normalization Reference Scales
// ============================================================================

/**
 * CompactnessNormalization — reference values for normalizing raw quantities
 * into [0,1] starred terms.
 *
 * Defaults calibrated from the Day #47 12-element reference dataset:
 *   rho_max  ≈ 11.8   (Fe avg_rho)
 *   eta_max  ≈ 0.494  (W avg_eta)
 *   C_max    ≈ 45.34  (Fe avg_C)
 *   macro_max = 1.0    (precursor channels are already [0,1])
 */
struct CompactnessNormalization {
    double rho_max{12.0};    // max avg_rho across reference set
    double eta_max{0.50};    // max avg_eta across reference set
    double C_max{46.0};      // max avg_C (coordination) across reference set
    double macro_max{1.0};   // macro precursors already normalized
};

inline constexpr CompactnessNormalization DEFAULT_COMPACT_NORM{};

// ============================================================================
// Component Computations
// ============================================================================

/**
 * ρ* — Normalized effective density.
 *
 *   ρ* = min(1, avg_rho / rho_max)
 */
inline auto compute_rho_star(double avg_rho,
                             double rho_max = DEFAULT_COMPACT_NORM.rho_max) -> double
{
    if (!std::isfinite(avg_rho) || avg_rho <= 0.0) return 0.0;
    return std::clamp(avg_rho / rho_max, 0.0, 1.0);
}

/**
 * η* — Normalized packing efficiency.
 *
 *   η* = min(1, avg_eta / eta_max)
 */
inline auto compute_eta_star(double avg_eta,
                             double eta_max = DEFAULT_COMPACT_NORM.eta_max) -> double
{
    if (!std::isfinite(avg_eta) || avg_eta <= 0.0) return 0.0;
    return std::clamp(avg_eta / eta_max, 0.0, 1.0);
}

/**
 * R*_coord — Coordination regularity.
 *
 *   R*_coord = min(1, avg_C / C_max)
 *
 * High coordination = tightly packed.  Low = sparse or surface-dominated.
 */
inline auto compute_R_coord_star(double avg_C,
                                 double C_max = DEFAULT_COMPACT_NORM.C_max) -> double
{
    if (!std::isfinite(avg_C) || avg_C <= 0.0) return 0.0;
    return std::clamp(avg_C / C_max, 0.0, 1.0);
}

/**
 * D*_macro — Macro deformation / stiffness consistency.
 *
 *   D*_macro = (rigidity_like + cohesion_like) / 2
 *
 * Uses the macro precursor channels directly.
 * Falls back to (rigidity + color) / 2 when cohesion not available.
 *
 * @param macro_rigidity   rigidity_like precursor
 * @param macro_cohesion   cohesion_integrity_like or macro_color fallback
 */
inline auto compute_D_macro_star(double macro_rigidity,
                                 double macro_cohesion) -> double
{
    double r = std::isfinite(macro_rigidity) ? macro_rigidity : 0.0;
    double c = std::isfinite(macro_cohesion) ? macro_cohesion : 0.0;
    return std::clamp((r + c) / 2.0, 0.0, 1.0);
}

/**
 * Φ*_void — Void fraction / looseness penalty.
 *
 *   Φ*_void = 1 - η*
 *
 * Simple complement of packing.  High void fraction = loose structure.
 * More sophisticated models can use actual void-detection later.
 */
inline auto compute_Phi_void_star(double eta_star) -> double
{
    return std::clamp(1.0 - eta_star, 0.0, 1.0);
}

// ============================================================================
// Full Compactness Score
// ============================================================================

struct CompactnessResult {
    CompactnessComponents  components;
    CompactnessWeights     weights;
    CompactnessNormalization norm;
    double                 C_compact{0.0};
};

/**
 * Compute the full compactness score for a formation record.
 */
inline auto compute_compactness(
    const FormationRecord& rec,
    const CompactnessWeights& w = DEFAULT_COMPACT_WEIGHTS,
    const CompactnessNormalization& norm = DEFAULT_COMPACT_NORM) -> CompactnessResult
{
    V4_CONTRACT_PRE(w.valid());

    CompactnessResult result;
    result.weights = w;
    result.norm    = norm;

    auto& c = result.components;
    c.rho_star      = compute_rho_star(rec.avg_rho, norm.rho_max);
    c.eta_star      = compute_eta_star(rec.avg_eta, norm.eta_max);
    c.R_coord_star  = compute_R_coord_star(rec.avg_C, norm.C_max);
    c.D_macro_star  = compute_D_macro_star(rec.macro_rigidity, rec.macro_color);
    c.Phi_void_star = compute_Phi_void_star(c.eta_star);

    result.C_compact = w.b1 * c.rho_star
                     + w.b2 * c.eta_star
                     + w.b3 * c.R_coord_star
                     + w.b4 * c.D_macro_star
                     - w.b5 * c.Phi_void_star;

    result.C_compact = std::clamp(result.C_compact, 0.0, 1.0);

    V4_CONTRACT_POST(result.C_compact >= 0.0 && result.C_compact <= 1.0);
    return result;
}

/**
 * Convenience: compute compactness and write into record.
 */
inline auto score_compactness(
    FormationRecord& rec,
    const CompactnessWeights& w = DEFAULT_COMPACT_WEIGHTS,
    const CompactnessNormalization& norm = DEFAULT_COMPACT_NORM) -> CompactnessResult
{
    auto result = compute_compactness(rec, w, norm);
    rec.compactness = result.C_compact;
    return result;
}

} // namespace v4
