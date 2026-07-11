#pragma once
/**
 [VSEPR-SIM 4.0.4.04]
 * ════════════════════════════════════════════════
 * Quantifies meaningful configurational exploration during formation.
 * Does NOT reward chaos — rewards landscape coverage.
 *
 * From section_v4_transition.tex Eq. (2.1):
 *
 *   γ = w₁·S_bead + w₂·S_scale + w₃·S_var + w₄·S_intensity + w₅·S_orbital
 *
 * Default weights: {0.20, 0.15, 0.25, 0.25, 0.15}
 *
 * All sub-scores ∈ [0,1].  Final γ ∈ [0,1].
 *
 * C++26 features:
 *   - Contract emulation (V4_CONTRACT_PRE/POST)
 *   - Erroneous-behaviour pattern init (-ftrivial-auto-var-init=pattern)
 *   - Structured bindings for weight unpacking
 *   - Trailing return types
 *
 * Anti-black-box: every sub-score exposed, every weight inspectable.
 */

#include "formation_record.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace v4 {

// ============================================================================
// Gamma Sub-score Components
// ============================================================================

struct GammaComponents {
    double S_bead{0.0};       // bead resolution response
    double S_scale{0.0};      // multi-scale consistency
    double S_var{0.0};        // configurational spread
    double S_intensity{0.0};  // informational density
    double S_orbital{0.0};    // orbital augmentation richness
};

// ============================================================================
// Gamma Weights
// ============================================================================

struct GammaWeights {
    double w1{0.20};  // bead
    double w2{0.15};  // scale
    double w3{0.25};  // var
    double w4{0.25};  // intensity
    double w5{0.15};  // orbital

    /// Verify weights sum to ~1.0
    auto valid() const -> bool {
        double s = w1 + w2 + w3 + w4 + w5;
        return std::abs(s - 1.0) < 1e-6;
    }
};

inline constexpr GammaWeights DEFAULT_GAMMA_WEIGHTS{};

// ============================================================================
// Sub-score Computations
// ============================================================================

/**
 * S_bead: Bead Resolution Response
 *
 *   S_bead = 1 - exp(-|η(Nb) - η(Nb')| / σ_η)
 *
 * If no reference run is available (eta_ref = NaN), returns 0
 * (no resolution sensitivity information).
 *
 * @param eta_current   avg_eta from current run
 * @param eta_ref       avg_eta from reference bead-count run
 * @param sigma_eta     std deviation of eta across history
 */
inline auto compute_S_bead(double eta_current, double eta_ref,
                           double sigma_eta) -> double
{
    if (!std::isfinite(eta_current) || !std::isfinite(eta_ref))
        return 0.0;
    if (sigma_eta < 1e-15) return 0.0;
    double diff = std::abs(eta_current - eta_ref);
    return 1.0 - std::exp(-diff / sigma_eta);
}

/**
 * S_scale: Multi-Scale Consistency
 *
 *   S_scale = 1 - |ρ_local - ρ_macro|
 *
 * Clamped to [0,1].  Rewards agreement between local bead density
 * and macro-inferred density.
 *
 * @param rho_local   avg_rho from bead-level formation
 * @param rho_macro   macro precursor density (or rigidity proxy)
 */
inline auto compute_S_scale(double rho_local, double rho_macro) -> double
{
    if (!std::isfinite(rho_local) || !std::isfinite(rho_macro))
        return 0.0;
    double diff = std::abs(rho_local - rho_macro);
    return std::clamp(1.0 - diff, 0.0, 1.0);
}

/**
 * S_var: Configurational Spread
 *
 *   S_var = min(1, σ_η / (η̄ + ε)),  ε = 1e-12
 *
 * High S_var = structurally diverse.  Low S_var = homogeneous crystal.
 *
 * @param mean_eta   mean eta across all beads at final step
 * @param std_eta    standard deviation of eta at final step
 */
inline auto compute_S_var(double mean_eta, double std_eta) -> double
{
    constexpr double eps = 1e-12;
    if (!std::isfinite(mean_eta) || !std::isfinite(std_eta))
        return 0.0;
    double denom = std::abs(mean_eta) + eps;
    return std::min(1.0, std_eta / denom);
}

/**
 * S_intensity: Informational Density
 *
 *   S_intensity = (populated fields) / (total fields)
 *
 * A run that populates all 22 columns scores 1.0.
 */
inline auto compute_S_intensity(const FormationRecord& rec) -> double
{
    return static_cast<double>(rec.populated_fields())
         / static_cast<double>(FormationRecord::FIELD_COUNT);
}

/**
 * S_orbital: Orbital Augmentation Richness
 *
 *   S_orbital = ℓ_max_active / ℓ_max_capable
 *
 * @param l_max_active   highest active SH order across channels
 * @param l_max_capable  system's maximum representable SH order
 */
inline auto compute_S_orbital(int l_max_active, int l_max_capable) -> double
{
    if (l_max_capable <= 0) return 0.0;
    return std::clamp(
        static_cast<double>(l_max_active) / static_cast<double>(l_max_capable),
        0.0, 1.0);
}

// ============================================================================
// Full Gamma Score
// ============================================================================

/**
 * GammaResult — full breakdown of gamma computation.
 *
 * Exposed for traceability: every sub-score is available for
 * inspection, ranking, and reporting.
 */
struct GammaResult {
    GammaComponents components;
    GammaWeights    weights;
    double          gamma{0.0};
};

/**
 * Compute the full gamma score for a formation record.
 *
 * @param rec          completed formation record
 * @param eta_ref      avg_eta from a reference bead-count run (NaN if absent)
 * @param sigma_eta    std deviation of eta across formation history
 * @param rho_macro    macro-level density proxy
 * @param l_max_active highest active SH order
 * @param l_max_cap    system maximum SH order
 * @param w            weight set (defaults provided)
 */
inline auto compute_gamma(
    const FormationRecord& rec,
    double eta_ref,
    double sigma_eta,
    double rho_macro,
    int    l_max_active,
    int    l_max_cap,
    const GammaWeights& w = DEFAULT_GAMMA_WEIGHTS) -> GammaResult
{
    V4_CONTRACT_PRE(w.valid());

    GammaResult result;
    result.weights = w;

    auto& c = result.components;
    c.S_bead      = compute_S_bead(rec.avg_eta, eta_ref, sigma_eta);
    c.S_scale     = compute_S_scale(rec.avg_rho, rho_macro);
    c.S_var       = compute_S_var(rec.avg_eta, sigma_eta);
    c.S_intensity = compute_S_intensity(rec);
    c.S_orbital   = compute_S_orbital(l_max_active, l_max_cap);

    result.gamma = w.w1 * c.S_bead
                 + w.w2 * c.S_scale
                 + w.w3 * c.S_var
                 + w.w4 * c.S_intensity
                 + w.w5 * c.S_orbital;

    // Clamp to [0,1] — contract post-condition
    result.gamma = std::clamp(result.gamma, 0.0, 1.0);

    V4_CONTRACT_POST(result.gamma >= 0.0 && result.gamma <= 1.0);
    return result;
}

/**
 * Convenience: compute gamma and write it directly into a record.
 */
inline auto score_gamma(
    FormationRecord& rec,
    double eta_ref      = NaN,
    double sigma_eta    = 0.0,
    double rho_macro    = 0.0,
    int    l_max_active = 0,
    int    l_max_cap    = 4,
    const GammaWeights& w = DEFAULT_GAMMA_WEIGHTS) -> GammaResult
{
    auto result = compute_gamma(rec, eta_ref, sigma_eta, rho_macro,
                                l_max_active, l_max_cap, w);
    rec.gamma = result.gamma;
    return result;
}

} // namespace v4
