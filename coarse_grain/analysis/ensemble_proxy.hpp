#pragma once
/**
 * ensemble_proxy.hpp — Ensemble-Level Macroscopic Response Proxies
 *
 * Emergent Effective Medium Mapping layer.
 *
 * Computes macroscopic response proxies from high-N bead-state
 * distributions. This is the bridge between per-bead environment state
 * (rho, C, P2, eta, target_f) and interpretable bulk characterisation.
 *
 * Architecture position:
 *   Environment-state evaluation
 *       ↓
 *   Ensemble statistics / spatial field summaries   ← this module
 *       ↓
 *   Macroscopic response proxies                    ← this module
 *       ↓
 *   Later constitutive or transport models
 *
 * What this module does NOT claim:
 *   - It does not compute elastic modulus, conductivity, or fracture
 *     toughness from first principles
 *   - It does not predict continuum-scale constitutive properties
 *
 * What it does provide:
 *   - First and second moment statistics of all bead-state variables
 *   - Edge vs bulk structural contrasts
 *   - Occupancy fractions (high-coordination, high-eta, high-alignment)
 *   - Spatial autocorrelation of state fields
 *   - Five interpretable response proxies:
 *       cohesion, uniformity, texture, stabilization, surface sensitivity
 *
 * Anti-black-box: every proxy is defined as an explicit, inspectable
 * function of bead-state distributions. No hidden weights. No learned
 * parameters. All intermediate statistics are exposed.
 *
 * Reference: Emergent Effective Medium Mapping specification
 */

#include "coarse_grain/core/environment_state.hpp"
#include "atomistic/core/state.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace coarse_grain {

// ============================================================================
// Model Parameters — documented, inspectable, not buried in formulas
// ============================================================================

/**
 * PROXY_MIN_BEADS
 *   Minimum ensemble size before any proxy is considered reliable.
 *   Below this, all proxy fields are computed but EnsembleProxySummary::valid
 *   is set to false. Rationale: variance and autocorrelation estimates are
 *   unreliable for N < 8; bulk/edge classification degenerates at small N.
 */
constexpr int PROXY_MIN_BEADS = 8;

/**
 * PROXY_CONVERGENCE_THRESH
 *   Maximum absolute change in mean_eta and mean_state_mismatch between
 *   two successive snapshots for the system to be declared converged.
 *   Used by compute_proxy_delta(). Units: dimensionless (eta in [0, 1]).
 */
constexpr double PROXY_CONVERGENCE_THRESH = 1e-4;

/**
 * PROXY_VARIANCE_FLOOR
 *   Lower bound applied to variance values in proxy denominators to
 *   prevent division by zero on degenerate (all-identical) inputs.
 *   Does not affect the stored var_* fields — only internal proxy computation.
 */
constexpr double PROXY_VARIANCE_FLOOR = 1e-12;

// ============================================================================
// Ensemble Proxy Summary
// ============================================================================

/**
 * EnsembleProxySummary — stable output object for high-N ensemble analysis.
 *
 * Sections:
 *   A. Global first moments
 *   B. Global second moments (variance)
 *   C. Edge vs bulk contrasts
 *   D. Occupancy fractions
 *   E. Spatial autocorrelation (neighbor-pair)
 *   F. Response proxies (the main deliverable)
 *   G. Diagnostics
 */
struct EnsembleProxySummary {
    int bead_count{};

    // --- A. Global first moments ---
    double mean_rho{};
    double mean_rho_hat{};
    double mean_C{};
    double mean_P2{};
    double mean_P2_hat{};
    double mean_eta{};
    double mean_target_f{};
    double mean_state_mismatch{};   // mean |eta_i - target_f_i|

    // --- B. Global second moments ---
    double var_rho{};
    double var_C{};
    double var_P2{};
    double var_eta{};
    double var_target_f{};

    // --- C. Edge vs bulk contrasts ---
    int n_bulk{};
    int n_edge{};
    double bulk_edge_rho_gap{};     // bulk_mean_rho - edge_mean_rho
    double bulk_edge_C_gap{};       // bulk_mean_C - edge_mean_C
    double bulk_edge_P2_gap{};      // bulk_mean_P2 - edge_mean_P2
    double bulk_edge_eta_gap{};     // bulk_mean_eta - edge_mean_eta

    // --- D. Occupancy fractions ---
    double frac_high_coord{};       // fraction with C >= mean_C
    double frac_high_eta{};         // fraction with eta >= 0.5
    double frac_high_alignment{};   // fraction with P2_hat >= 0.5

    // --- E. Spatial autocorrelation (neighbor-pair) ---
    double eta_spatial_autocorr{};  // [-1, 1]; positive = coherent
    double rho_spatial_autocorr{};
    double P2_spatial_autocorr{};

    // --- F. Response proxies ---
    /**
     * Cohesion proxy:
     *   (mean_rho_hat + mean_eta + (1 - mean_mismatch)) / 3
     * Combines density response, internal stabilization, and
     * convergence quality. All terms in [0,1]. Higher = more cohesive.
     * Does NOT measure cohesive energy.
     */
    double cohesion_proxy{};

    /**
     * Uniformity proxy:
     *   1 - clamp(2*std_eta + 2*std_rho_hat, 0, 1)
     * Low variance in eta and rho_hat across the ensemble.
     * In [0,1]. Higher = more homogeneous packing.
     */
    double uniformity_proxy{};

    /**
     * Texture proxy:
     *   mean_P2_hat
     * Orientational organisation. In [0,1].
     * 0 = perpendicular/no orientation, ~0.33 = random, 1 = aligned.
     */
    double texture_proxy{};

    /**
     * Stabilization proxy:
     *   mean_eta * (1 - mean_mismatch)
     * Adaptation quality. In [0,1]. Higher = well-adapted to environment.
     */
    double stabilization_proxy{};

    /**
     * Surface sensitivity proxy:
     *   mean of fractional |bulk - edge| gaps in rho, C, eta.
     * In [0,1]. Higher = more surface-dominated structure.
     */
    double surface_sensitivity_proxy{};

    // --- G. Diagnostics ---
    bool all_finite{true};
    bool all_bounded{true};         // all eta in [0, 1]

    // --- H. Validity and traceability ---
    /**
     * valid: false if bead_count < PROXY_MIN_BEADS.
     * Proxy fields are still populated but should not be used for
     * quantitative interpretation at small N.
     */
    bool   valid{true};
    double c_thresh_used{};         // actual coordination threshold applied

    // --- I. Spatial correlation lengths ---
    /**
     * Characteristic decay length ξ from an exponential fit to the
     * binned spatial autocorrelation C(r) ∝ exp(-r/ξ).
     * NaN if the fit fails (too few pairs, non-monotone decay,
     * or uniform field). Units: Angstrom.
     *
     * Fit details: log-linear regression on C(r) vs r using 1 Å bins
     * and a minimum of 3 pairs per bin. Edge-inclusive (no bias exclusion).
     */
    double eta_corr_length{};
    double rho_corr_length{};

    // --- J. Time evolution (populated by compute_proxy_delta) ---
    /**
     * Change in slow-state statistics since the previous snapshot.
     * Populated by compute_proxy_delta(current, previous).
     * Zero-initialized; not meaningful for a single isolated snapshot.
     */
    double delta_mean_eta{};
    double delta_mean_mismatch{};
    /**
     * converged: true if |delta_mean_eta| < PROXY_CONVERGENCE_THRESH AND
     * |delta_mean_mismatch| < PROXY_CONVERGENCE_THRESH.
     * Meaningful only after compute_proxy_delta() has been called.
     */
    bool   converged{false};

    // --- K. Reference-normalised proxies ---
    /**
     * Relative proxy values in [0, 1] expressed as position between
     * a known lower bound (random) and upper bound (ideal lattice).
     * Populated by apply_reference(summary, lo, hi).
     * -1 indicates no reference has been set.
     */
    double rel_cohesion{-1};
    double rel_uniformity{-1};
    double rel_texture{-1};
    double rel_stabilization{-1};
    double rel_surface_sensitivity{-1};
};

// ============================================================================
// Bulk/Edge Classifier
// ============================================================================

/**
 * classify_bulk_edge — explicit, per-bead bulk/edge classifier.
 *
 * A bead is classified as EDGE (surface) if it fails EITHER criterion:
 *
 *   1. Coordination criterion (always active):
 *      C_i < c_thresh  →  edge.
 *      c_thresh: if < 0 the median C of the ensemble is used automatically.
 *      This is a MODEL PARAMETER — it sets the definition of "surface".
 *      The auto-median mode is a reasonable default for homogeneous systems
 *      but should be pinned to an explicit value for comparative studies.
 *
 *   2. Geometric depth criterion (active when surface_depth > 0):
 *      Bead is within surface_depth Å of the radial boundary (maximum
 *      centroid distance) of the ensemble  →  edge.
 *      Precedence: OR logic — failing EITHER criterion marks as edge.
 *      This is conservative: it classifies more beads as surface rather
 *      than fewer, reducing false bulk classification near boundaries.
 *
 * Returns: vector<bool> is_bulk[i], same ordering as states.
 * Writes the resolved c_thresh into *c_thresh_out if non-null.
 */
inline std::vector<bool> classify_bulk_edge(
    const std::vector<EnvironmentState>& states,
    const std::vector<atomistic::Vec3>& positions,
    double c_thresh = -1.0,
    double surface_depth = 0.0,
    double* c_thresh_out = nullptr)
{
    int n = static_cast<int>(states.size());
    std::vector<bool> is_bulk(n, true);
    if (n == 0) return is_bulk;

    // --- Step 1: Resolve coordination threshold ---
    double ct = c_thresh;
    if (ct < 0.0) {
        std::vector<double> cs(n);
        for (int i = 0; i < n; ++i) cs[i] = states[i].C;
        std::sort(cs.begin(), cs.end());
        ct = cs[n / 2];
    }
    if (c_thresh_out) *c_thresh_out = ct;

    // --- Step 2: Coordination criterion ---
    for (int i = 0; i < n; ++i) {
        if (states[i].C < ct) is_bulk[i] = false;
    }

    // --- Step 3: Geometric depth criterion (optional) ---
    if (surface_depth > 0.0 && static_cast<int>(positions.size()) >= n) {
        double cx = 0, cy = 0, cz = 0;
        for (int i = 0; i < n; ++i) {
            cx += positions[i].x;
            cy += positions[i].y;
            cz += positions[i].z;
        }
        cx /= n; cy /= n; cz /= n;

        double max_dist = 0;
        for (int i = 0; i < n; ++i) {
            double dx = positions[i].x - cx;
            double dy = positions[i].y - cy;
            double dz = positions[i].z - cz;
            double d = std::sqrt(dx*dx + dy*dy + dz*dz);
            if (d > max_dist) max_dist = d;
        }

        double inner_radius = max_dist - surface_depth;
        for (int i = 0; i < n; ++i) {
            if (!is_bulk[i]) continue;  // already edge
            double dx = positions[i].x - cx;
            double dy = positions[i].y - cy;
            double dz = positions[i].z - cz;
            double d = std::sqrt(dx*dx + dy*dy + dz*dz);
            if (d >= inner_radius) is_bulk[i] = false;
        }
    }

    return is_bulk;
}

// ============================================================================
// Spatial Autocorrelation Helper
// ============================================================================

/**
 * Compute neighbor-pair spatial autocorrelation for a scalar field.
 *
 * For each pair (i,j) with |r_i - r_j| < cutoff:
 *   accumulate (x_i - mu)(x_j - mu)
 * Divide by (pair_count * var_x).
 *
 * Result in [-1, 1]:
 *   +1  = nearby beads are similar (coherent domains)
 *    0  = uncorrelated
 *   -1  = anti-correlated (alternating)
 *
 * Returns 1.0 for zero-variance fields (trivially coherent).
 * Returns 0.0 if no neighbor pairs exist.
 */
inline double spatial_autocorrelation(
    const std::vector<double>& values,
    double mean,
    double var,
    const std::vector<atomistic::Vec3>& positions,
    double cutoff)
{
    if (var < 1e-30) return 1.0;
    int n = static_cast<int>(values.size());
    if (n < 2 || static_cast<int>(positions.size()) < n) return 0.0;

    double cutoff2 = cutoff * cutoff;
    double sum = 0.0;
    int pair_count = 0;

    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            atomistic::Vec3 dr = positions[j] - positions[i];
            double d2 = atomistic::dot(dr, dr);
            if (d2 < cutoff2 && d2 > 1e-20) {
                sum += (values[i] - mean) * (values[j] - mean);
                ++pair_count;
            }
        }
    }

    if (pair_count == 0) return 0.0;
    return std::clamp((sum / pair_count) / var, -1.0, 1.0);
}

// ============================================================================
// Spatial Correlation Length
// ============================================================================

/**
 * Estimate the spatial correlation length ξ by fitting C(r) ∝ exp(-r/ξ).
 *
 * Method:
 *   1. Bin all pairs by separation r into bins of width bin_width.
 *   2. For each bin with >= min_pairs_per_bin, compute
 *      C(r_k) = mean[(x_i - μ)(x_j - μ)] / var.
 *   3. Fit log(C(r)) = a + b*r via ordinary linear regression.
 *      The slope b = -1/ξ gives ξ = -1/b.
 *
 * Failure modes (all return NaN):
 *   - var < 1e-30 (uniform field — ξ is undefined)
 *   - Fewer than 2 bins have enough pairs
 *   - All positive-correlation bins are exhausted (non-monotone decay)
 *   - Regression slope >= 0 (correlation not decaying)
 *
 * Edge-inclusive: all beads participate regardless of bulk/edge classification.
 * Excluding edge beads changes ξ systematically and is left to the caller
 * via subsetting the input vectors.
 *
 * @param r_max        Maximum separation considered (Å). Use 2× neighbor_cutoff.
 * @param bin_width    Bin width (Å). Default 1.0 Å.
 * @param min_pairs    Minimum pairs per bin to trust the correlation value.
 */
inline double spatial_correlation_length(
    const std::vector<double>& values,
    double mean,
    double var,
    const std::vector<atomistic::Vec3>& positions,
    double r_max,
    double bin_width = 1.0,
    int min_pairs = 3)
{
    const double nan_val = std::numeric_limits<double>::quiet_NaN();
    if (var < 1e-30) return nan_val;
    int n = static_cast<int>(values.size());
    if (n < 4 || static_cast<int>(positions.size()) < n) return nan_val;

    int n_bins = static_cast<int>(std::ceil(r_max / bin_width));
    if (n_bins < 2) return nan_val;

    std::vector<double> bin_sum(n_bins, 0.0);
    std::vector<int>    bin_count(n_bins, 0);

    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            atomistic::Vec3 dr = positions[j] - positions[i];
            double r = std::sqrt(atomistic::dot(dr, dr));
            if (r < 1e-20 || r >= r_max) continue;
            int k = static_cast<int>(r / bin_width);
            if (k >= n_bins) continue;
            bin_sum[k] += (values[i] - mean) * (values[j] - mean);
            ++bin_count[k];
        }
    }

    // Build log-linear fit: log(C(r)) = a + b*r → b = -1/ξ
    std::vector<double> r_pts, log_c_pts;
    for (int k = 0; k < n_bins; ++k) {
        if (bin_count[k] < min_pairs) continue;
        double c_k = (bin_sum[k] / bin_count[k]) / var;
        if (c_k <= 0.0) continue;  // can't take log of non-positive
        r_pts.push_back((k + 0.5) * bin_width);
        log_c_pts.push_back(std::log(c_k));
    }

    if (static_cast<int>(r_pts.size()) < 2) return nan_val;

    double sr = 0, slc = 0, srr = 0, srlc = 0;
    int m = static_cast<int>(r_pts.size());
    for (int k = 0; k < m; ++k) {
        sr   += r_pts[k];
        slc  += log_c_pts[k];
        srr  += r_pts[k] * r_pts[k];
        srlc += r_pts[k] * log_c_pts[k];
    }
    double denom = m * srr - sr * sr;
    if (std::abs(denom) < 1e-20) return nan_val;
    double slope = (m * srlc - sr * slc) / denom;
    if (slope >= 0.0) return nan_val;  // not decaying
    return -1.0 / slope;
}

// ============================================================================
// Ensemble Proxy Computation
// ============================================================================

/**
 * Compute ensemble-level response proxies from bead-state distributions.
 *
 * @param states           Per-bead environment state (from run_all_beads or
 *                         run_formation_history)
 * @param positions        Per-bead position vectors (same ordering as states)
 * @param coord_threshold  Coordination threshold for bulk/edge split.
 *                         If < 0, uses median C (automatic).
 *                         This IS a model parameter — pin it explicitly for
 *                         comparative studies. See PROXY_MIN_BEADS note.
 * @param neighbor_cutoff  Distance cutoff for spatial autocorrelation (Å).
 *                         Default matches EnvironmentParams::r_cutoff.
 * @param surface_depth    Geometric depth criterion (Å). Beads within this
 *                         distance of the radial boundary are additionally
 *                         classified as edge. 0 = disabled (coordination only).
 * @return Fully populated EnsembleProxySummary. Check valid field first.
 */
inline EnsembleProxySummary compute_ensemble_proxy(
    const std::vector<EnvironmentState>& states,
    const std::vector<atomistic::Vec3>& positions,
    double coord_threshold = -1.0,
    double neighbor_cutoff = 8.0,
    double surface_depth = 0.0)
{
    EnsembleProxySummary s;
    int n = static_cast<int>(states.size());
    s.bead_count = n;

    // --- Validity check ---
    s.valid = (n >= PROXY_MIN_BEADS);

    if (n == 0) return s;

    // ==== A. Global first moments ====
    double sum_rho = 0, sum_rho_hat = 0, sum_C = 0;
    double sum_P2 = 0, sum_P2_hat = 0;
    double sum_eta = 0, sum_tf = 0, sum_mismatch = 0;

    for (const auto& st : states) {
        sum_rho     += st.rho;
        sum_rho_hat += st.rho_hat;
        sum_C       += st.C;
        sum_P2      += st.P2;
        sum_P2_hat  += st.P2_hat;
        sum_eta     += st.eta;
        sum_tf      += st.target_f;
        sum_mismatch += std::abs(st.eta - st.target_f);

        if (std::isnan(st.eta) || std::isinf(st.eta) ||
            std::isnan(st.rho) || std::isinf(st.rho)) {
            s.all_finite = false;
        }
        if (st.eta < -1e-15 || st.eta > 1.0 + 1e-15) {
            s.all_bounded = false;
        }
    }

    s.mean_rho     = sum_rho / n;
    s.mean_rho_hat = sum_rho_hat / n;
    s.mean_C       = sum_C / n;
    s.mean_P2      = sum_P2 / n;
    s.mean_P2_hat  = sum_P2_hat / n;
    s.mean_eta     = sum_eta / n;
    s.mean_target_f = sum_tf / n;
    s.mean_state_mismatch = sum_mismatch / n;

    // ==== B. Global second moments ====
    double var_rho = 0, var_C = 0, var_P2 = 0, var_eta = 0, var_tf = 0;
    for (const auto& st : states) {
        double dr  = st.rho - s.mean_rho;     var_rho += dr * dr;
        double dc  = st.C - s.mean_C;         var_C   += dc * dc;
        double dp2 = st.P2 - s.mean_P2;       var_P2  += dp2 * dp2;
        double de  = st.eta - s.mean_eta;      var_eta += de * de;
        double dtf = st.target_f - s.mean_target_f; var_tf += dtf * dtf;
    }
    s.var_rho      = var_rho / n;
    s.var_C        = var_C / n;
    s.var_P2       = var_P2 / n;
    s.var_eta      = var_eta / n;
    s.var_target_f = var_tf / n;

    // ==== C. Edge vs bulk contrasts ====
    // Uses classify_bulk_edge() — see that function for precedence rules.
    {
        double ct_out = 0.0;
        std::vector<bool> is_bulk = classify_bulk_edge(
            states, positions, coord_threshold, surface_depth, &ct_out);
        s.c_thresh_used = ct_out;

        double b_rho = 0, b_C = 0, b_P2 = 0, b_eta = 0;
        double e_rho = 0, e_C = 0, e_P2 = 0, e_eta = 0;
        for (int i = 0; i < n; ++i) {
            const auto& st = states[i];
            if (is_bulk[i]) {
                ++s.n_bulk;
                b_rho += st.rho; b_C += st.C; b_P2 += st.P2; b_eta += st.eta;
            } else {
                ++s.n_edge;
                e_rho += st.rho; e_C += st.C; e_P2 += st.P2; e_eta += st.eta;
            }
        }

        // Guard: NaN when one class is empty — gap is undefined, not zero.
        if (s.n_bulk > 0 && s.n_edge > 0) {
            double b_mean_rho = b_rho / s.n_bulk;
            double b_mean_C   = b_C   / s.n_bulk;
            double b_mean_P2  = b_P2  / s.n_bulk;
            double b_mean_eta = b_eta / s.n_bulk;
            double e_mean_rho = e_rho / s.n_edge;
            double e_mean_C   = e_C   / s.n_edge;
            double e_mean_P2  = e_P2  / s.n_edge;
            double e_mean_eta = e_eta / s.n_edge;

            s.bulk_edge_rho_gap = b_mean_rho - e_mean_rho;
            s.bulk_edge_C_gap   = b_mean_C   - e_mean_C;
            s.bulk_edge_P2_gap  = b_mean_P2  - e_mean_P2;
            s.bulk_edge_eta_gap = b_mean_eta - e_mean_eta;
        } else {
            // All-bulk or all-edge: gap is undefined, not zero.
            const double nan_val = std::numeric_limits<double>::quiet_NaN();
            s.bulk_edge_rho_gap = nan_val;
            s.bulk_edge_C_gap   = nan_val;
            s.bulk_edge_P2_gap  = nan_val;
            s.bulk_edge_eta_gap = nan_val;
        }
    }

    // ==== D. Occupancy fractions ====
    int n_high_C = 0, n_high_eta = 0, n_high_P2 = 0;
    for (const auto& st : states) {
        if (st.C >= s.mean_C) ++n_high_C;
        if (st.eta >= 0.5)    ++n_high_eta;
        if (st.P2_hat >= 0.5) ++n_high_P2;
    }
    s.frac_high_coord     = static_cast<double>(n_high_C) / n;
    s.frac_high_eta       = static_cast<double>(n_high_eta) / n;
    s.frac_high_alignment = static_cast<double>(n_high_P2) / n;

    // ==== E. Spatial autocorrelation + correlation lengths ====
    if (static_cast<int>(positions.size()) >= n) {
        std::vector<double> v_eta(n), v_rho(n), v_P2(n);
        for (int i = 0; i < n; ++i) {
            v_eta[i] = states[i].eta;
            v_rho[i] = states[i].rho;
            v_P2[i]  = states[i].P2;
        }
        s.eta_spatial_autocorr = spatial_autocorrelation(
            v_eta, s.mean_eta, s.var_eta, positions, neighbor_cutoff);
        s.rho_spatial_autocorr = spatial_autocorrelation(
            v_rho, s.mean_rho, s.var_rho, positions, neighbor_cutoff);
        s.P2_spatial_autocorr = spatial_autocorrelation(
            v_P2, s.mean_P2, s.var_P2, positions, neighbor_cutoff);

        // Correlation lengths: fit C(r) ∝ exp(-r/ξ) up to 2× cutoff.
        double r_max = 2.0 * neighbor_cutoff;
        s.eta_corr_length = spatial_correlation_length(
            v_eta, s.mean_eta, s.var_eta, positions, r_max);
        s.rho_corr_length = spatial_correlation_length(
            v_rho, s.mean_rho, s.var_rho, positions, r_max);
    }

    // ==== F. Response proxies ====

    // F.1 Cohesion proxy: density + stabilization + convergence
    //   (mean_rho_hat + mean_eta + (1 - mean_mismatch)) / 3
    s.cohesion_proxy = (s.mean_rho_hat + s.mean_eta
                       + (1.0 - s.mean_state_mismatch)) / 3.0;
    s.cohesion_proxy = std::clamp(s.cohesion_proxy, 0.0, 1.0);

    // F.2 Uniformity proxy: low spread in eta and rho_hat
    //   For [0,1]-bounded variables, max std ≈ 0.5.
    //   Normalize: 2*std maps [0, 0.5] → [0, 1].
    double var_rho_hat = 0;
    for (const auto& st : states) {
        double d = st.rho_hat - s.mean_rho_hat;
        var_rho_hat += d * d;
    }
    var_rho_hat /= n;
    double d_eta = std::clamp(2.0 * std::sqrt(s.var_eta), 0.0, 1.0);
    double d_rho = std::clamp(2.0 * std::sqrt(var_rho_hat), 0.0, 1.0);
    s.uniformity_proxy = std::clamp(1.0 - (d_eta + d_rho) / 2.0, 0.0, 1.0);

    // F.3 Texture proxy: orientational organisation
    //   P2_hat: 0 = perpendicular, ~0.33 = random, 1 = aligned
    s.texture_proxy = std::clamp(s.mean_P2_hat, 0.0, 1.0);

    // F.4 Stabilization proxy: adaptation quality
    //   mean_eta * (1 - mean_mismatch)
    s.stabilization_proxy = std::clamp(
        s.mean_eta * (1.0 - s.mean_state_mismatch), 0.0, 1.0);

    // F.5 Surface sensitivity proxy: fractional |bulk - edge| gaps
    //   Normalised by respective means.
    //   Guard: if gap fields are NaN (no edge or no bulk), sensitivity = 0.
    if (!std::isnan(s.bulk_edge_rho_gap)) {
        constexpr double eps = 1e-10;
        double r_rho = std::abs(s.bulk_edge_rho_gap)
                     / std::max(s.mean_rho, eps);
        double r_C   = std::abs(s.bulk_edge_C_gap)
                     / std::max(s.mean_C, eps);
        double r_eta = std::abs(s.bulk_edge_eta_gap)
                     / std::max(std::max(s.mean_eta, eps), PROXY_VARIANCE_FLOOR);
        s.surface_sensitivity_proxy = std::clamp(
            (r_rho + r_C + r_eta) / 3.0, 0.0, 1.0);
    } else {
        s.surface_sensitivity_proxy = 0.0;
    }

    return s;
}

// ============================================================================
// Time Evolution — compute_proxy_delta
// ============================================================================

/**
 * Compute time-evolution fields from two successive proxy snapshots.
 *
 * Fills the delta_mean_eta, delta_mean_mismatch, and converged fields
 * of the current snapshot. Returns the updated summary by value.
 *
 * A system is converged when BOTH:
 *   |delta_mean_eta|       < PROXY_CONVERGENCE_THRESH
 *   |delta_mean_mismatch|  < PROXY_CONVERGENCE_THRESH
 *
 * Without this, high mismatch could mean "still converging" (delta > 0,
 * improving) or "actively diverging" (delta < 0, worsening) — those are
 * opposite situations requiring opposite responses.
 *
 * @param curr  Current snapshot (returned modified with delta fields set)
 * @param prev  Previous snapshot
 * @param convergence_thresh  Threshold for converged flag. Default: PROXY_CONVERGENCE_THRESH.
 */
inline EnsembleProxySummary compute_proxy_delta(
    EnsembleProxySummary curr,
    const EnsembleProxySummary& prev,
    double convergence_thresh = PROXY_CONVERGENCE_THRESH)
{
    curr.delta_mean_eta      = curr.mean_eta - prev.mean_eta;
    curr.delta_mean_mismatch = curr.mean_state_mismatch - prev.mean_state_mismatch;
    curr.converged = (std::abs(curr.delta_mean_eta)      < convergence_thresh &&
                      std::abs(curr.delta_mean_mismatch) < convergence_thresh);
    return curr;
}

// ============================================================================
// Reference Distributions — EnsembleProxyReference + apply_reference
// ============================================================================

/**
 * EnsembleProxyReference — stores reference proxy summaries for
 * contextualising absolute proxy values.
 *
 * Usage:
 *   1. Run three reference configurations (ideal lattice, random cloud,
 *      disordered orientations) using compute_ensemble_proxy().
 *   2. Populate an EnsembleProxyReference with those summaries.
 *   3. Call apply_reference(target_summary, ref.random, ref.ideal) to
 *      fill the rel_* fields of the target with [0, 1]-normalised values.
 *
 * Rationale: a raw proxy value of 0.73 is meaningless without a baseline.
 * The reference distributions provide the scale:
 *   rel = 0 means "as low as random reference"
 *   rel = 1 means "as high as ideal lattice reference"
 *
 * Reference configurations should match the target N and density.
 */
struct EnsembleProxyReference {
    EnsembleProxySummary ideal;      // ideal lattice — structural upper bound
    EnsembleProxySummary random;     // randomised positions — lower bound
    EnsembleProxySummary disordered; // structured positions, random orientations
    bool populated{false};
};

/**
 * Fill the rel_* fields of a summary by normalising proxy values between
 * a lower-bound reference (lo) and an upper-bound reference (hi).
 *
 * rel = clamp((val - lo) / (hi - lo), 0, 1)
 *
 * If hi ≈ lo (degenerate reference), rel is set to 0.5 (indeterminate).
 * This function modifies the summary in place.
 */
inline void apply_reference(
    EnsembleProxySummary& s,
    const EnsembleProxySummary& lo,
    const EnsembleProxySummary& hi)
{
    auto rel = [](double v, double lo_v, double hi_v) -> double {
        if (hi_v <= lo_v + 1e-10) return 0.5;
        return std::clamp((v - lo_v) / (hi_v - lo_v), 0.0, 1.0);
    };
    s.rel_cohesion            = rel(s.cohesion_proxy,
                                    lo.cohesion_proxy,    hi.cohesion_proxy);
    s.rel_uniformity          = rel(s.uniformity_proxy,
                                    lo.uniformity_proxy,  hi.uniformity_proxy);
    s.rel_texture             = rel(s.texture_proxy,
                                    lo.texture_proxy,     hi.texture_proxy);
    s.rel_stabilization       = rel(s.stabilization_proxy,
                                    lo.stabilization_proxy, hi.stabilization_proxy);
    s.rel_surface_sensitivity = rel(s.surface_sensitivity_proxy,
                                    lo.surface_sensitivity_proxy,
                                    hi.surface_sensitivity_proxy);
}

} // namespace coarse_grain
