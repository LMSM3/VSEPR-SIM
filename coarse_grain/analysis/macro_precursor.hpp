#pragma once
/**
 * macro_precursor.hpp — Macro Property Precursor Channels
 *
 * Converts ensemble-level macroscopic response proxies into property
 * precursor bundles: intermediate latent descriptors that are later
 * calibrated against experiment, atomistic benchmarks, or material
 * family reference sets.
 *
 * Architecture position:
 *   Environment-state evaluation
 *       ↓
 *   Ensemble statistics / spatial field summaries
 *       ↓
 *   Macroscopic response proxies     (ensemble_proxy.hpp)
 *       ↓
 *   Macro precursor channels          ← this module
 *       ↓
 *   Later calibrated property estimator
 *
 * What this module does NOT claim:
 *   - It does NOT compute tensile strength, hardness, or conductivity
 *   - It does NOT predict real engineering material properties
 *   - It does NOT substitute for constitutive models or failure envelopes
 *
 * What it does provide:
 *   - Candidate property precursor channels with -like suffixes:
 *       rigidity_like, ductility_like, brittleness_like,
 *       cohesion_integrity_like, thermal_transport_like,
 *       electrical_transport_like, surface_reactivity_like,
 *       fracture_susceptibility_like
 *   - Explicit weighted formulas — no hidden weights, no learned parameters
 *   - Per-channel confidence and validity flags
 *   - Intermediate penalty/helper terms with full provenance
 *   - An anisotropy index and interface penalty
 *
 * Anti-black-box: every precursor is a documented, inspectable, monotone
 * function of proxy inputs. Weights are named constants. Provenance
 * is preserved. Calibration status is explicit.
 *
 * Reference: Macro Property Precursor Layer specification
 */

#include "coarse_grain/analysis/ensemble_proxy.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace coarse_grain {

// ============================================================================
// Model Parameters — documented, inspectable
// ============================================================================

/**
 * PRECURSOR_XI_REF
 *   Reference correlation length for normalisation (Å).
 *   ξ_norm = clamp(ξ / ξ_ref, 0, 1).
 *   Default 20 Å: represents a characteristic domain size.
 */
constexpr double PRECURSOR_XI_REF = 20.0;

/**
 * PRECURSOR_MIN_CONFIDENCE
 *   Floor below which a channel is marked invalid.
 */
constexpr double PRECURSOR_MIN_CONFIDENCE = 0.1;

// ============================================================================
// Weight Constants — explicit, traceable, not buried in formulas
// ============================================================================

// Rigidity-like weights
// R* = w1·C + w2·U + w3·S + w4·ξ̃  − w5·Σ_surface − w6·P_interface
namespace weights {
    constexpr double rig_w1_cohesion     = 0.25;
    constexpr double rig_w2_uniformity   = 0.20;
    constexpr double rig_w3_stabilize    = 0.20;
    constexpr double rig_w4_xi_norm      = 0.15;
    constexpr double rig_w5_surface      = 0.10;
    constexpr double rig_w6_interface    = 0.10;

    // Brittleness-like weights
    // B* = a1·R* + a2·P_interface + a3·Σ_surface − a4·D_adapt
    constexpr double brit_a1_rigidity    = 0.30;
    constexpr double brit_a2_interface   = 0.25;
    constexpr double brit_a3_surface     = 0.20;
    constexpr double brit_a4_adapt       = 0.25;

    // Ductility-like weights
    // D* = b1·U + b2·A_relax + b3·(1 − B*) − b4·T_lock
    constexpr double duct_b1_uniformity  = 0.30;
    constexpr double duct_b2_relax       = 0.25;
    constexpr double duct_b3_no_brittle  = 0.25;
    constexpr double duct_b4_texture     = 0.20;

    // Transport-like weights (shared form for thermal/electrical)
    // K* = c1·ξ̃  + c2·T + c3·C − c4·P_interface
    constexpr double trans_c1_xi_norm    = 0.30;
    constexpr double trans_c2_texture    = 0.25;
    constexpr double trans_c3_cohesion   = 0.25;
    constexpr double trans_c4_interface  = 0.20;

    // Electrical transport: heavier on alignment continuity
    constexpr double etrans_c1_xi_norm   = 0.25;
    constexpr double etrans_c2_texture   = 0.35;
    constexpr double etrans_c3_cohesion  = 0.20;
    constexpr double etrans_c4_interface = 0.20;

    // Surface reactivity-like weights
    // Sr* = d1·Σ_surface + d2·P_interface + d3·(1 − S) + d4·(1 − ξ̃ )
    constexpr double react_d1_surface    = 0.30;
    constexpr double react_d2_interface  = 0.25;
    constexpr double react_d3_no_stabil  = 0.25;
    constexpr double react_d4_no_xi      = 0.20;

    // Fracture susceptibility-like weights
    // F* = e1·B* + e2·Σ_surface + e3·P_interface − e4·D*
    constexpr double frac_e1_brittle     = 0.30;
    constexpr double frac_e2_surface     = 0.25;
    constexpr double frac_e3_interface   = 0.25;
    constexpr double frac_e4_ductility   = 0.20;

    // Interface penalty weights
    // P_interface = ip_a·|Δη| + ip_b·|Δρ| + ip_c·|Δ_mismatch|
    constexpr double ip_a_eta    = 0.40;
    constexpr double ip_b_rho    = 0.35;
    constexpr double ip_c_mismatch = 0.25;

    // Cohesion integrity-like: directly from cohesion + convergence
    constexpr double coh_f1_cohesion    = 0.40;
    constexpr double coh_f2_stabilize   = 0.30;
    constexpr double coh_f3_uniformity  = 0.30;
} // namespace weights

// ============================================================================
// Structs
// ============================================================================

/**
 * MacroPrecursorChannel — one property precursor.
 *
 * value:      the precursor estimate in [0, 1].
 * confidence: how trustworthy this channel is given input quality.
 *             Degrades with NaN inputs, small N, unconverged state.
 * valid:      false when confidence < PRECURSOR_MIN_CONFIDENCE or
 *             when upstream proxy state is invalid.
 */
struct MacroPrecursorChannel {
    double value      = std::numeric_limits<double>::quiet_NaN();
    double confidence = 0.0;
    bool   valid      = false;
};

/**
 * MacroPrecursorState — full precursor output.
 *
 * Eight property precursor channels plus global diagnostics.
 * All channel values are in [0, 1] when valid.
 *
 * Naming convention: -like suffixes indicate these are NOT real
 * engineering properties. They are evidence variables — monotone
 * functions of ensemble proxies that will later be calibrated
 * against experiment or higher-fidelity simulation.
 */
struct MacroPrecursorState {
    bool valid = false;

    // Property precursor channels
    MacroPrecursorChannel rigidity_like;
    MacroPrecursorChannel ductility_like;
    MacroPrecursorChannel brittleness_like;
    MacroPrecursorChannel cohesion_integrity_like;
    MacroPrecursorChannel thermal_transport_like;
    MacroPrecursorChannel electrical_transport_like;
    MacroPrecursorChannel surface_reactivity_like;
    MacroPrecursorChannel fracture_susceptibility_like;

    // Global diagnostics
    double anisotropy_index       = std::numeric_limits<double>::quiet_NaN();
    double interface_penalty      = std::numeric_limits<double>::quiet_NaN();
    double convergence_confidence = 0.0;

    // Intermediate terms — exposed for traceability
    double xi_norm          = 0.0;   // normalised correlation length
    double adapt_capacity   = 0.0;   // adaptation/relaxation signal
    double texture_lock     = 0.0;   // orientational rigidity measure

    // Provenance
    int    source_bead_count = 0;
    bool   source_valid      = false;
    bool   source_converged  = false;
};

// ============================================================================
// Helper Computations — all exposed, all inspectable
// ============================================================================

/**
 * Normalise a correlation length against a reference scale.
 * Returns clamp(ξ / ξ_ref, 0, 1). Returns 0 if ξ is NaN.
 */
inline double normalize_correlation_length(double xi,
                                           double xi_ref = PRECURSOR_XI_REF)
{
    if (std::isnan(xi) || xi <= 0.0) return 0.0;
    if (xi_ref <= 0.0) return 0.0;
    return std::clamp(xi / xi_ref, 0.0, 1.0);
}

/**
 * Compute the interface penalty from bulk-edge gap fields.
 *
 * P_interface = a·|Δη| + b·|Δρ_gap_normalized| + c·Δ_mismatch_proxy
 *
 * Gap fields set to NaN (degenerate classifier) produce penalty = 0
 * (no classifiable interface).  Clamped to [0, 1].
 */
inline double compute_interface_penalty(const EnsembleProxySummary& s)
{
    if (std::isnan(s.bulk_edge_eta_gap)) return 0.0;

    double abs_eta_gap = std::abs(s.bulk_edge_eta_gap);
    double abs_rho_gap = std::abs(s.bulk_edge_rho_gap);
    constexpr double eps = 1e-10;

    // Normalise gaps to [0, 1] range using ensemble means as scale
    double norm_eta = abs_eta_gap / std::max(std::max(s.mean_eta, eps),
                                             PROXY_VARIANCE_FLOOR);
    double norm_rho = abs_rho_gap / std::max(s.mean_rho, eps);

    // Mismatch contribution: use surface_sensitivity_proxy directly
    // as it already captures the fractional gap structure
    double mismatch_contrib = s.surface_sensitivity_proxy;

    double penalty = weights::ip_a_eta * std::min(norm_eta, 1.0)
                   + weights::ip_b_rho * std::min(norm_rho, 1.0)
                   + weights::ip_c_mismatch * mismatch_contrib;

    return std::clamp(penalty, 0.0, 1.0);
}

/**
 * Compute adaptation capacity from time-delta information.
 *
 * High when:
 *   - the system has converged (small deltas)
 *   - mismatch is low
 *   - eta is high
 *
 * When no delta information is available (single snapshot),
 * falls back to stabilization_proxy as the best available estimate.
 */
inline double compute_adapt_capacity(const EnsembleProxySummary& s)
{
    // If delta information is populated (either converged or deltas are set)
    if (s.converged) {
        return std::clamp(s.stabilization_proxy, 0.0, 1.0);
    }

    // Estimate from stabilization and mismatch
    double adapt = s.mean_eta * (1.0 - s.mean_state_mismatch);
    return std::clamp(adapt, 0.0, 1.0);
}

/**
 * Compute texture lock — how much orientational order resists deformation.
 *
 * High texture proxy + low variance = orientationally locked structure.
 * This inhibits ductile deformation modes.
 */
inline double compute_texture_lock(const EnsembleProxySummary& s)
{
    // texture_proxy is mean_P2_hat, already in [0, 1].
    // Variance of P2 reduces the locking effect (diverse orientations
    // even with high mean suggests some orientational freedom).
    double var_penalty = std::clamp(2.0 * std::sqrt(s.var_P2), 0.0, 1.0);
    return std::clamp(s.texture_proxy * (1.0 - var_penalty), 0.0, 1.0);
}

/**
 * Compute anisotropy index from texture and correlation structure.
 *
 * Currently isotropic-only (single ξ). Architecture placeholder for
 * directional splitting (ξ_x, ξ_y, ξ_z) when available.
 *
 * For now: high texture + any measurable correlation → anisotropic.
 * Range [0, 1]: 0 = isotropic, 1 = strongly anisotropic.
 */
inline double compute_anisotropy_index(const EnsembleProxySummary& s,
                                       double xi_norm)
{
    // Texture alone is the primary anisotropy signal.
    // A system with high texture AND long correlation has sustained
    // directional order over large distances.
    double texture_signal = s.texture_proxy;

    // Random orientation baseline: P2_hat ≈ 1/3.
    // Excess above 1/3 is the orientational anisotropy contribution.
    double excess_texture = std::clamp(
        (texture_signal - 1.0/3.0) / (1.0 - 1.0/3.0), 0.0, 1.0);

    // Scale by correlation persistence
    double corr_factor = 0.5 + 0.5 * xi_norm;  // [0.5, 1.0]

    return std::clamp(excess_texture * corr_factor, 0.0, 1.0);
}

// ============================================================================
// Per-Channel Precursor Computations
// ============================================================================

/**
 * Rigidity-like precursor.
 *
 * R* = w1·C + w2·U + w3·S + w4·ξ̃  − w5·Σ_surface − w6·P_interface
 *
 * Increases with: cohesion, uniformity, stabilisation, correlation length.
 * Decreases with: surface sensitivity, interface disorder.
 */
inline double compute_rigidity_like(const EnsembleProxySummary& s,
                                    double xi_norm,
                                    double interface_penalty)
{
    double val = weights::rig_w1_cohesion   * s.cohesion_proxy
               + weights::rig_w2_uniformity * s.uniformity_proxy
               + weights::rig_w3_stabilize  * s.stabilization_proxy
               + weights::rig_w4_xi_norm    * xi_norm
               - weights::rig_w5_surface    * s.surface_sensitivity_proxy
               - weights::rig_w6_interface  * interface_penalty;
    return std::clamp(val, 0.0, 1.0);
}

/**
 * Brittleness-like precursor.
 *
 * B* = a1·R* + a2·P_interface + a3·Σ_surface − a4·D_adapt
 *
 * High when: rigid but with interface penalties, weak surface adaptation.
 * Low when: good adaptive capacity and ductile relaxation.
 */
inline double compute_brittleness_like(double rigidity_like,
                                       double interface_penalty,
                                       double surface_sens,
                                       double adapt_capacity)
{
    double val = weights::brit_a1_rigidity  * rigidity_like
               + weights::brit_a2_interface * interface_penalty
               + weights::brit_a3_surface   * surface_sens
               - weights::brit_a4_adapt     * adapt_capacity;
    return std::clamp(val, 0.0, 1.0);
}

/**
 * Ductility-like precursor.
 *
 * D* = b1·U + b2·A_relax + b3·(1 − B*) − b4·T_lock
 *
 * Increases with: uniformity, adaptive relaxation, non-brittleness.
 * Decreases with: orientational locking.
 */
inline double compute_ductility_like(double uniformity,
                                     double adapt_capacity,
                                     double brittleness_like,
                                     double texture_lock)
{
    double val = weights::duct_b1_uniformity * uniformity
               + weights::duct_b2_relax      * adapt_capacity
               + weights::duct_b3_no_brittle * (1.0 - brittleness_like)
               - weights::duct_b4_texture    * texture_lock;
    return std::clamp(val, 0.0, 1.0);
}

/**
 * Thermal transport-like precursor.
 *
 * K* = c1·ξ̃  + c2·T + c3·C − c4·P_interface
 *
 * Increases with: long correlation, alignment, cohesion.
 * Decreases with: interface penalty.
 */
inline double compute_thermal_transport_like(const EnsembleProxySummary& s,
                                             double xi_norm,
                                             double interface_penalty)
{
    double val = weights::trans_c1_xi_norm   * xi_norm
               + weights::trans_c2_texture   * s.texture_proxy
               + weights::trans_c3_cohesion  * s.cohesion_proxy
               - weights::trans_c4_interface * interface_penalty;
    return std::clamp(val, 0.0, 1.0);
}

/**
 * Electrical transport-like precursor.
 *
 * Heavier weighting on alignment continuity (texture) and stronger
 * penalties for discontinuity (interface penalty) than thermal.
 */
inline double compute_electrical_transport_like(const EnsembleProxySummary& s,
                                                double xi_norm,
                                                double interface_penalty)
{
    double val = weights::etrans_c1_xi_norm   * xi_norm
               + weights::etrans_c2_texture   * s.texture_proxy
               + weights::etrans_c3_cohesion  * s.cohesion_proxy
               - weights::etrans_c4_interface * interface_penalty;
    return std::clamp(val, 0.0, 1.0);
}

/**
 * Surface reactivity-like precursor.
 *
 * Sr* = d1·Σ_surface + d2·P_interface + d3·(1 − S) + d4·(1 − ξ̃ )
 *
 * Increases with: surface sensitivity, interface mismatch,
 *                 weak stabilisation, short correlation depth.
 */
inline double compute_surface_reactivity_like(const EnsembleProxySummary& s,
                                              double xi_norm,
                                              double interface_penalty)
{
    double val = weights::react_d1_surface   * s.surface_sensitivity_proxy
               + weights::react_d2_interface * interface_penalty
               + weights::react_d3_no_stabil * (1.0 - s.stabilization_proxy)
               + weights::react_d4_no_xi     * (1.0 - xi_norm);
    return std::clamp(val, 0.0, 1.0);
}

/**
 * Fracture susceptibility-like precursor.
 *
 * F* = e1·B* + e2·Σ_surface + e3·P_interface − e4·D*
 *
 * Increases with: brittleness, surface sensitivity, interface penalty.
 * Decreases with: ductility.
 */
inline double compute_fracture_susceptibility_like(double brittleness_like,
                                                   double surface_sens,
                                                   double interface_penalty,
                                                   double ductility_like)
{
    double val = weights::frac_e1_brittle   * brittleness_like
               + weights::frac_e2_surface   * surface_sens
               + weights::frac_e3_interface * interface_penalty
               - weights::frac_e4_ductility * ductility_like;
    return std::clamp(val, 0.0, 1.0);
}

/**
 * Cohesion integrity-like precursor.
 *
 * CI* = f1·C + f2·S + f3·U
 *
 * Directly from cohesion, stabilisation, and uniformity.
 * A simple structural integrity indicator.
 */
inline double compute_cohesion_integrity_like(const EnsembleProxySummary& s)
{
    double val = weights::coh_f1_cohesion   * s.cohesion_proxy
               + weights::coh_f2_stabilize  * s.stabilization_proxy
               + weights::coh_f3_uniformity * s.uniformity_proxy;
    return std::clamp(val, 0.0, 1.0);
}

// ============================================================================
// Confidence Computation
// ============================================================================

/**
 * Compute per-channel confidence based on input quality.
 *
 * Confidence starts at 1.0 and is degraded by:
 *   - Small bead count (< 2× PROXY_MIN_BEADS): −0.3
 *   - Invalid upstream proxy: sets to 0.0
 *   - NaN correlation length: −0.2
 *   - Unconverged time-delta: −0.15
 *   - NaN gap fields (degenerate classifier): −0.1
 *
 * Returns base confidence applicable to all channels.
 * Channels may further adjust their own confidence.
 */
inline double compute_base_confidence(const EnsembleProxySummary& s)
{
    if (!s.valid) return 0.0;

    double conf = 1.0;

    // Small sample penalty
    if (s.bead_count < 2 * PROXY_MIN_BEADS) conf -= 0.3;

    // Finite checks
    if (!s.all_finite) conf -= 0.3;

    // Correlation length availability
    if (std::isnan(s.eta_corr_length) && std::isnan(s.rho_corr_length))
        conf -= 0.2;

    // Convergence status
    // (converged is only meaningful after compute_proxy_delta, but
    //  if it's false we apply a smaller penalty since single snapshots
    //  also have converged=false by default)
    // Only penalise if delta info suggests active divergence
    if (s.delta_mean_mismatch > 0.01) conf -= 0.15;

    // Degenerate bulk/edge classifier
    if (std::isnan(s.bulk_edge_eta_gap)) conf -= 0.1;

    return std::clamp(conf, 0.0, 1.0);
}

// ============================================================================
// Main Pipeline
// ============================================================================

/**
 * Compute all macro property precursor channels from an ensemble proxy
 * summary.
 *
 * This is the main entry point. It:
 *   1. Extracts intermediate terms (xi_norm, interface_penalty, etc.)
 *   2. Computes each precursor channel with its explicit formula
 *   3. Assigns per-channel confidence from input quality
 *   4. Sets validity flags
 *
 * @param s    Ensemble proxy summary (from compute_ensemble_proxy)
 * @param ref  Optional reference distributions. If provided and populated,
 *             uses reference-normalised proxy values where available.
 *             Currently reserved for future use.
 */
inline MacroPrecursorState compute_macro_precursors(
    const EnsembleProxySummary& s,
    const EnsembleProxyReference* ref = nullptr)
{
    MacroPrecursorState out;
    out.source_bead_count = s.bead_count;
    out.source_valid      = s.valid;
    out.source_converged  = s.converged;

    // --- Validity gate ---
    if (s.bead_count == 0) return out;

    // --- Intermediate terms ---
    // Use average of eta and rho correlation lengths
    double xi_raw = 0.0;
    int xi_count = 0;
    if (!std::isnan(s.eta_corr_length) && s.eta_corr_length > 0) {
        xi_raw += s.eta_corr_length;
        ++xi_count;
    }
    if (!std::isnan(s.rho_corr_length) && s.rho_corr_length > 0) {
        xi_raw += s.rho_corr_length;
        ++xi_count;
    }
    if (xi_count > 0) xi_raw /= xi_count;

    double xi_ref = PRECURSOR_XI_REF;
    // If reference provided, future versions may extract ξ_ref from it
    (void)ref;

    out.xi_norm        = normalize_correlation_length(xi_raw, xi_ref);
    out.interface_penalty = compute_interface_penalty(s);
    out.adapt_capacity = compute_adapt_capacity(s);
    out.texture_lock   = compute_texture_lock(s);

    // --- Anisotropy index ---
    out.anisotropy_index = compute_anisotropy_index(s, out.xi_norm);

    // --- Base confidence ---
    double base_conf = compute_base_confidence(s);
    out.convergence_confidence = base_conf;
    out.valid = s.valid && base_conf >= PRECURSOR_MIN_CONFIDENCE;

    // --- Channel computations ---

    // 1. Rigidity-like
    {
        double val = compute_rigidity_like(s, out.xi_norm,
                                           out.interface_penalty);
        double conf = base_conf;
        out.rigidity_like = {val, conf, out.valid && conf >= PRECURSOR_MIN_CONFIDENCE};
    }

    // 2. Brittleness-like (depends on rigidity)
    {
        double val = compute_brittleness_like(
            out.rigidity_like.value,
            out.interface_penalty,
            s.surface_sensitivity_proxy,
            out.adapt_capacity);
        double conf = base_conf;
        // Lower confidence when adaptation info is absent
        if (!s.converged && s.delta_mean_eta == 0.0) conf *= 0.85;
        out.brittleness_like = {val, conf, out.valid && conf >= PRECURSOR_MIN_CONFIDENCE};
    }

    // 3. Ductility-like (depends on brittleness)
    {
        double val = compute_ductility_like(
            s.uniformity_proxy,
            out.adapt_capacity,
            out.brittleness_like.value,
            out.texture_lock);
        double conf = base_conf;
        if (!s.converged && s.delta_mean_eta == 0.0) conf *= 0.85;
        out.ductility_like = {val, conf, out.valid && conf >= PRECURSOR_MIN_CONFIDENCE};
    }

    // 4. Cohesion integrity-like
    {
        double val = compute_cohesion_integrity_like(s);
        double conf = base_conf;
        out.cohesion_integrity_like = {val, conf, out.valid && conf >= PRECURSOR_MIN_CONFIDENCE};
    }

    // 5. Thermal transport-like
    {
        double val = compute_thermal_transport_like(s, out.xi_norm,
                                                    out.interface_penalty);
        double conf = base_conf;
        // Transport confidence degrades without correlation length
        if (std::isnan(s.eta_corr_length) && std::isnan(s.rho_corr_length))
            conf *= 0.7;
        out.thermal_transport_like = {val, conf, out.valid && conf >= PRECURSOR_MIN_CONFIDENCE};
    }

    // 6. Electrical transport-like
    {
        double val = compute_electrical_transport_like(s, out.xi_norm,
                                                       out.interface_penalty);
        double conf = base_conf;
        if (std::isnan(s.eta_corr_length) && std::isnan(s.rho_corr_length))
            conf *= 0.7;
        // Electrical transport confidence additionally degrades without
        // texture information
        if (s.texture_proxy < 0.01) conf *= 0.8;
        out.electrical_transport_like = {val, conf, out.valid && conf >= PRECURSOR_MIN_CONFIDENCE};
    }

    // 7. Surface reactivity-like
    {
        double val = compute_surface_reactivity_like(s, out.xi_norm,
                                                     out.interface_penalty);
        double conf = base_conf;
        // Reactivity confidence degrades when no edge beads exist
        if (s.n_edge == 0) conf *= 0.6;
        out.surface_reactivity_like = {val, conf, out.valid && conf >= PRECURSOR_MIN_CONFIDENCE};
    }

    // 8. Fracture susceptibility-like (depends on brittleness + ductility)
    {
        double val = compute_fracture_susceptibility_like(
            out.brittleness_like.value,
            s.surface_sensitivity_proxy,
            out.interface_penalty,
            out.ductility_like.value);
        double conf = base_conf;
        if (!s.converged && s.delta_mean_eta == 0.0) conf *= 0.85;
        out.fracture_susceptibility_like = {val, conf, out.valid && conf >= PRECURSOR_MIN_CONFIDENCE};
    }

    return out;
}

} // namespace coarse_grain
