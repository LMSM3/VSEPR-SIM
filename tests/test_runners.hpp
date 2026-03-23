#pragma once
/**
 * test_runners.hpp — Test Execution, Analysis, and Assertion Infrastructure
 *
 * Provides:
 *   1. EtaTrajectory + run_trajectory: single-bead time evolution
 *   2. run_all_beads: multi-bead environment update pass
 *   3. Behavioral assertion helpers: monotonicity, boundedness, convergence
 *   4. StatSummary + compute_stats: descriptive statistics
 *   5. Statistical correlation tools: mean_increases_with, groups_separated
 *   6. RunRecord + run_robustness_trial: Monte Carlo trial execution
 *   7. ResponseRecord, AtlasRow: single-variable sweep data structures
 *   8. run_single_variable_sweep: direct sweep harness
 *   9. PairwiseCell, CoordinationHistogram: analysis structures
 *
 * Variable hierarchy (for reference):
 *   geometry / neighborhood  →  rho, C, P2       (measured local state)
 *   normalized transforms    →  rho_hat, P2_hat   (rescaled forms)
 *   control / relaxation     →  target_f           (driving field)
 *   dynamical response       →  eta                (stateful memory)
 *
 * C in this context is the coordination number: the soft-weighted count
 * of neighbours within the interaction range. It is NOT a generic "C"
 * variable — it measures how many beads are nearby.
 *
 * Reference: Suite #2/#3 specification from development sessions
 */

#include "tests/scene_builders.hpp"
#include "coarse_grain/core/environment_state.hpp"
#include "coarse_grain/models/environment_coupling.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <limits>
#include <vector>

namespace test_util {

// ============================================================================
// Multi-Step Runner
// ============================================================================

/**
 * EtaTrajectory — records the evolution of eta over time.
 */
struct EtaTrajectory {
    std::vector<double> eta;
    std::vector<double> target_f;
    std::vector<double> rho;
    std::vector<double> P2;
    std::vector<double> C;
    int steps{};
};

/**
 * Run N steps of environment state update for bead at bead_index
 * within a static scene. Collects full trajectory.
 *
 * The scene does NOT evolve (positions are frozen). Only eta changes.
 * Uses bead.eta as initial eta if initial_eta < 0 (sentinel).
 */
inline EtaTrajectory run_trajectory(
    const std::vector<SceneBead>& scene,
    int bead_index,
    double initial_eta,
    const coarse_grain::EnvironmentParams& params,
    double dt,
    int n_steps)
{
    EtaTrajectory traj;
    traj.steps = n_steps;

    auto nbs = build_neighbours(scene, bead_index);
    const auto& bead = scene[bead_index];

    double eta = initial_eta;
    for (int step = 0; step < n_steps; ++step) {
        auto state = coarse_grain::update_environment_state(
            eta, bead.n_hat, bead.has_orientation, nbs, params, dt);

        traj.eta.push_back(state.eta);
        traj.target_f.push_back(state.target_f);
        traj.rho.push_back(state.rho);
        traj.P2.push_back(state.P2);
        traj.C.push_back(state.C);

        eta = state.eta;
    }
    return traj;
}

/**
 * Run a multi-bead environment update pass: compute observables for
 * every bead in the scene, evolve eta, return per-bead states.
 *
 * Respects SceneBead::eta as initial values on step 0.
 */
inline std::vector<coarse_grain::EnvironmentState> run_all_beads(
    const std::vector<SceneBead>& scene,
    const coarse_grain::EnvironmentParams& params,
    double dt,
    int n_steps)
{
    int n = static_cast<int>(scene.size());
    std::vector<double> etas(n);
    for (int i = 0; i < n; ++i) etas[i] = scene[i].eta;
    std::vector<coarse_grain::EnvironmentState> states(n);

    for (int step = 0; step < n_steps; ++step) {
        for (int i = 0; i < n; ++i) {
            auto nbs = build_neighbours(scene, i);
            states[i] = coarse_grain::update_environment_state(
                etas[i], scene[i].n_hat, scene[i].has_orientation,
                nbs, params, dt);
            etas[i] = states[i].eta;
        }
    }
    return states;
}

// ============================================================================
// Behavioral Assertion Helpers
// ============================================================================

/**
 * Check if a sequence is monotonically non-decreasing.
 */
inline bool is_monotone_increasing(const std::vector<double>& v) {
    for (size_t i = 1; i < v.size(); ++i) {
        if (v[i] < v[i - 1] - 1e-15) return false;
    }
    return true;
}

/**
 * Check if a sequence is monotonically non-increasing.
 */
inline bool is_monotone_decreasing(const std::vector<double>& v) {
    for (size_t i = 1; i < v.size(); ++i) {
        if (v[i] > v[i - 1] + 1e-15) return false;
    }
    return true;
}

/**
 * Check if all values in a sequence are within [lo, hi].
 */
inline bool is_bounded(const std::vector<double>& v, double lo, double hi) {
    for (double x : v) {
        if (x < lo - 1e-15 || x > hi + 1e-15) return false;
    }
    return true;
}

/**
 * Check if the final value is within tolerance of a target.
 */
inline bool approaches_value(const std::vector<double>& v,
                              double target, double tol)
{
    if (v.empty()) return false;
    return std::abs(v.back() - target) < tol;
}

/**
 * Check that values are NOT all equal (i.e., the system actually responded).
 */
inline bool has_variation(const std::vector<double>& v, double tol = 1e-12) {
    if (v.size() < 2) return false;
    for (size_t i = 1; i < v.size(); ++i) {
        if (std::abs(v[i] - v[0]) > tol) return true;
    }
    return false;
}

/**
 * Check no NaN or Inf values in a sequence.
 */
inline bool is_finite(const std::vector<double>& v) {
    for (double x : v) {
        if (std::isnan(x) || std::isinf(x)) return false;
    }
    return true;
}

// ============================================================================
// Statistical Tools
// ============================================================================

/**
 * StatSummary — descriptive statistics for a sample.
 */
struct StatSummary {
    double mean{};
    double variance{};
    double min_val{};
    double max_val{};
    int count{};
    int nan_count{};
    int inf_count{};
    double p05{};    // 5th percentile
    double p50{};    // median
    double p95{};    // 95th percentile
};

/**
 * Compute descriptive statistics for a vector of doubles.
 */
inline StatSummary compute_stats(std::vector<double> v) {
    StatSummary s;
    s.count = static_cast<int>(v.size());
    if (v.empty()) return s;

    // Count pathological values
    s.nan_count = 0;
    s.inf_count = 0;
    std::vector<double> clean;
    clean.reserve(v.size());
    for (double x : v) {
        if (std::isnan(x)) { ++s.nan_count; continue; }
        if (std::isinf(x)) { ++s.inf_count; continue; }
        clean.push_back(x);
    }

    if (clean.empty()) {
        s.mean = 0; s.variance = 0;
        s.min_val = 0; s.max_val = 0;
        return s;
    }

    std::sort(clean.begin(), clean.end());
    s.min_val = clean.front();
    s.max_val = clean.back();

    // Percentiles
    auto pctile = [&](double p) -> double {
        double idx = p * (clean.size() - 1);
        int lo = static_cast<int>(idx);
        int hi = lo + 1;
        if (hi >= static_cast<int>(clean.size())) return clean.back();
        double frac = idx - lo;
        return clean[lo] * (1.0 - frac) + clean[hi] * frac;
    };
    s.p05 = pctile(0.05);
    s.p50 = pctile(0.50);
    s.p95 = pctile(0.95);

    // Mean
    double sum = 0;
    for (double x : clean) sum += x;
    s.mean = sum / clean.size();

    // Variance
    double var_sum = 0;
    for (double x : clean) {
        double d = x - s.mean;
        var_sum += d * d;
    }
    s.variance = var_sum / clean.size();

    return s;
}

/**
 * Check whether mean of y increases (Spearman-like) with sorted x bins.
 * Splits data into n_bins, verifies mean(y) is non-decreasing across bins.
 */
inline bool mean_increases_with(
    const std::vector<double>& x,
    const std::vector<double>& y,
    int n_bins = 4)
{
    if (x.size() != y.size() || x.size() < static_cast<size_t>(n_bins * 2))
        return false;

    // Create index-sorted-by-x
    std::vector<size_t> idx(x.size());
    for (size_t i = 0; i < idx.size(); ++i) idx[i] = i;
    std::sort(idx.begin(), idx.end(),
              [&](size_t a, size_t b) { return x[a] < x[b]; });

    size_t per_bin = idx.size() / n_bins;
    std::vector<double> bin_means;
    for (int b = 0; b < n_bins; ++b) {
        double sum = 0;
        size_t start = b * per_bin;
        size_t end = (b == n_bins - 1) ? idx.size() : start + per_bin;
        for (size_t i = start; i < end; ++i) sum += y[idx[i]];
        bin_means.push_back(sum / (end - start));
    }

    for (size_t i = 1; i < bin_means.size(); ++i) {
        if (bin_means[i] < bin_means[i - 1] - 1e-10) return false;
    }
    return true;
}

/**
 * Check that two groups have separated means (group_a_mean < group_b_mean).
 */
inline bool groups_separated(
    const std::vector<double>& group_a,
    const std::vector<double>& group_b,
    double min_gap = 0.0)
{
    if (group_a.empty() || group_b.empty()) return false;
    double mean_a = 0, mean_b = 0;
    for (double x : group_a) mean_a += x;
    for (double x : group_b) mean_b += x;
    mean_a /= group_a.size();
    mean_b /= group_b.size();
    return (mean_b - mean_a) > min_gap;
}

// ============================================================================
// Run Record — Robustness Trial
// ============================================================================

/**
 * RunRecord — captures one complete robustness trial.
 */
struct RunRecord {
    uint32_t seed{};
    int n_neighbours{};
    double final_eta{};
    double final_rho{};
    double final_P2{};
    double final_C{};
    double final_target_f{};
    double g_steric{};
    double g_elec{};
    double g_disp{};
    bool eta_bounded{};
    bool eta_finite{};
};

/**
 * Execute a single robustness trial: build scene, run trajectory,
 * collect record.
 */
inline RunRecord run_robustness_trial(
    const MCSceneConfig& base_cfg,
    uint32_t trial_seed,
    const coarse_grain::EnvironmentParams& params,
    double dt,
    int n_steps)
{
    MCSceneConfig cfg = base_cfg;
    cfg.seed = trial_seed;
    auto scene = generate_mc_scene(cfg);

    auto traj = run_trajectory(scene, 0, 0.0, params, dt, n_steps);

    RunRecord rec;
    rec.seed = trial_seed;
    rec.n_neighbours = static_cast<int>(scene.size()) - 1;
    rec.final_eta = traj.eta.back();
    rec.final_rho = traj.rho.back();
    rec.final_P2 = traj.P2.back();
    rec.final_C = traj.C.back();
    rec.final_target_f = traj.target_f.back();
    rec.eta_bounded = is_bounded(traj.eta, 0.0, 1.0);
    rec.eta_finite = is_finite(traj.eta);

    // Kernel modulation at final state
    auto report = coarse_grain::compute_modulation_report(
        rec.final_eta, rec.final_eta, params);
    rec.g_steric = report.g_steric;
    rec.g_elec = report.g_electrostatic;
    rec.g_disp = report.g_dispersion;

    return rec;
}

// ============================================================================
// Response Atlas Data Structures
// ============================================================================

/**
 * ResponseRecord — one measurement in a single-variable sweep.
 */
struct ResponseRecord {
    double input_value{};
    double rho{};
    double rho_hat{};
    double C{};
    double P2{};
    double P2_hat{};
    double eta{};
    double target_f{};
    bool finite{true};
};

/**
 * AtlasRow — summary of a single-variable sweep across one scene.
 */
struct AtlasRow {
    const char* variable_name{};
    const char* scene_name{};
    int n_samples{};
    double output_min{};
    double output_max{};
    bool all_finite{true};
    bool monotonic_increasing{false};
    bool monotonic_decreasing{false};
    bool non_monotonic{false};
    double sensitivity_minus{};   // delta output for -5% perturbation
    double sensitivity_plus{};    // delta output for +5% perturbation
    bool redundant_flag{false};   // output identical to another variable
    const char* redundant_with{};
};

/**
 * PairwiseCell — one cell in a pairwise coupling grid.
 */
struct PairwiseCell {
    double x_val{};
    double y_val{};
    double output{};
    bool finite{true};
};

// ============================================================================
// Direct Sweep Harness
// ============================================================================

/**
 * SweepConfig — configuration for a single-variable sweep.
 */
struct SweepConfig {
    double lo{};               // sweep range start
    double hi{};               // sweep range end
    int n_samples{10};         // number of sample points
    double dt{10.0};           // timestep (fs)
    int n_steps{500};          // evolution steps per sample
};

/**
 * Run a single-variable sweep over a scene-generating function.
 *
 * scene_fn(double value) should return a scene for the given sweep value.
 * The sweep value is varied linearly from cfg.lo to cfg.hi.
 * For each value, the scene is built, all beads are evolved, and the
 * state of bead 0 is recorded.
 *
 * Returns a vector of ResponseRecords, one per sample point.
 */
inline std::vector<ResponseRecord> run_single_variable_sweep(
    std::function<std::vector<SceneBead>(double)> scene_fn,
    const coarse_grain::EnvironmentParams& params,
    const SweepConfig& cfg)
{
    std::vector<ResponseRecord> records;
    records.reserve(cfg.n_samples);

    for (int i = 0; i < cfg.n_samples; ++i) {
        double val = (cfg.n_samples <= 1)
            ? cfg.lo
            : cfg.lo + i * (cfg.hi - cfg.lo) / (cfg.n_samples - 1);

        auto scene = scene_fn(val);
        if (scene.empty()) continue;

        auto states = run_all_beads(scene, params, cfg.dt, cfg.n_steps);

        ResponseRecord r;
        r.input_value = val;
        r.rho = states[0].rho;
        r.rho_hat = states[0].rho_hat;
        r.C = states[0].C;
        r.P2 = states[0].P2;
        r.P2_hat = states[0].P2_hat;
        r.eta = states[0].eta;
        r.target_f = states[0].target_f;
        r.finite = std::isfinite(states[0].eta) &&
                   std::isfinite(states[0].rho) &&
                   std::isfinite(states[0].P2);
        records.push_back(r);
    }
    return records;
}

/**
 * Extract a single variable from a sweep result set.
 * accessor returns the desired field from each ResponseRecord.
 */
inline std::vector<double> extract_sweep_variable(
    const std::vector<ResponseRecord>& records,
    std::function<double(const ResponseRecord&)> accessor)
{
    std::vector<double> out;
    out.reserve(records.size());
    for (const auto& r : records) out.push_back(accessor(r));
    return out;
}

// ============================================================================
// Coordination Histogram (C = coordination number)
// ============================================================================

/**
 * CoordinationHistogram — binned coordination counts for large-N analysis.
 *
 * C (coordination number) is the soft-weighted count of neighbours
 * within the interaction range. bin[i] counts beads with C in [i, i+1).
 */
struct CoordinationHistogram {
    std::vector<int> bins;   // bin[i] = count of beads with coordination in [i, i+1)
    int max_coord{};
    double mean_coord{};
    double std_coord{};
};

/**
 * Build coordination histogram from a set of environment states.
 */
inline CoordinationHistogram build_coord_histogram(
    const std::vector<coarse_grain::EnvironmentState>& states)
{
    CoordinationHistogram h;
    if (states.empty()) return h;

    double sum = 0, sum2 = 0;
    int max_c = 0;
    for (const auto& s : states) {
        int c = static_cast<int>(s.C + 0.5);
        if (c > max_c) max_c = c;
        sum += s.C;
        sum2 += s.C * s.C;
    }
    h.max_coord = max_c;
    h.mean_coord = sum / states.size();
    h.std_coord = std::sqrt(sum2 / states.size() - h.mean_coord * h.mean_coord);

    h.bins.resize(max_c + 2, 0);
    for (const auto& s : states) {
        int c = static_cast<int>(s.C + 0.5);
        if (c >= 0 && c < static_cast<int>(h.bins.size()))
            h.bins[c]++;
    }
    return h;
}

// ============================================================================
// Stage 4 — Parameter Schedule
// ============================================================================

/**
 * ParameterSchedule — lightweight scheduling for parameter sweeps over time.
 *
 * Controls time-dependent variation of a single parameter (spacing,
 * alignment bias, alpha, beta, tau, etc.) during formation experiments.
 *
 * Modes:
 *   Constant:    value = start_value for all steps
 *   LinearRamp:  linearly interpolate start_value → end_value over
 *                [start_step, end_step]
 *   Step:        start_value before start_step, end_value from start_step
 *   UpDownRamp:  ramp up to end_value at midpoint, then ramp back down
 */
struct ParameterSchedule {
    enum class Mode {
        Constant,
        LinearRamp,
        Step,
        UpDownRamp
    };

    Mode mode{Mode::Constant};
    double start_value{};
    double end_value{};
    int start_step{};
    int end_step{};

    double evaluate(int step) const {
        switch (mode) {
        case Mode::Constant:
            return start_value;
        case Mode::Step:
            return (step >= start_step) ? end_value : start_value;
        case Mode::LinearRamp: {
            if (step <= start_step) return start_value;
            if (step >= end_step) return end_value;
            double t = static_cast<double>(step - start_step)
                     / static_cast<double>(end_step - start_step);
            return start_value + t * (end_value - start_value);
        }
        case Mode::UpDownRamp: {
            if (step <= start_step) return start_value;
            if (step >= end_step) return start_value;
            int mid = (start_step + end_step) / 2;
            if (step <= mid) {
                double t = static_cast<double>(step - start_step)
                         / static_cast<double>(mid - start_step);
                return start_value + t * (end_value - start_value);
            } else {
                double t = static_cast<double>(step - mid)
                         / static_cast<double>(end_step - mid);
                return end_value + t * (start_value - end_value);
            }
        }
        }
        return start_value;
    }
};

// ============================================================================
// Stage 4 — Formation History Runner
// ============================================================================

/**
 * FormationSnapshot — per-step aggregate of all bead states.
 */
struct FormationSnapshot {
    int step{};
    double mean_eta{};
    double mean_rho{};
    double mean_P2{};
    double mean_C{};
};

/**
 * FormationHistory — full time-evolution record for a multi-bead system.
 */
struct FormationHistory {
    std::vector<FormationSnapshot> snapshots;
    std::vector<coarse_grain::EnvironmentState> final_states;
    int bead_count{};
    int total_steps{};
};

/**
 * Run multi-step formation evolution recording per-step snapshots.
 *
 * Evolves all beads simultaneously (positions frozen, eta evolves).
 * Records aggregate mean eta/rho/P2/C at each step plus final per-bead
 * states.
 */
inline FormationHistory run_formation_history(
    const std::vector<SceneBead>& scene,
    const coarse_grain::EnvironmentParams& params,
    double dt,
    int n_steps)
{
    FormationHistory hist;
    int n = static_cast<int>(scene.size());
    hist.bead_count = n;
    hist.total_steps = n_steps;

    std::vector<double> etas(n);
    for (int i = 0; i < n; ++i) etas[i] = scene[i].eta;

    std::vector<coarse_grain::EnvironmentState> states(n);

    for (int step = 0; step < n_steps; ++step) {
        double sum_eta = 0, sum_rho = 0, sum_P2 = 0, sum_C = 0;
        for (int i = 0; i < n; ++i) {
            auto nbs = build_neighbours(scene, i);
            states[i] = coarse_grain::update_environment_state(
                etas[i], scene[i].n_hat, scene[i].has_orientation,
                nbs, params, dt);
            etas[i] = states[i].eta;
            sum_eta += states[i].eta;
            sum_rho += states[i].rho;
            sum_P2  += states[i].P2;
            sum_C   += states[i].C;
        }
        FormationSnapshot snap;
        snap.step = step;
        snap.mean_eta = sum_eta / n;
        snap.mean_rho = sum_rho / n;
        snap.mean_P2  = sum_P2 / n;
        snap.mean_C   = sum_C / n;
        hist.snapshots.push_back(snap);
    }
    hist.final_states = states;
    return hist;
}

// ============================================================================
// Stage 4 — Bulk/Edge Classification
// ============================================================================

/**
 * BulkEdgeStats — aggregate statistics split by bulk vs edge classification.
 *
 * Classification uses coordination number: beads with C >= threshold
 * are "bulk"; beads below are "edge".
 */
struct BulkEdgeStats {
    int n_bulk{};
    int n_edge{};
    double bulk_mean_eta{};
    double edge_mean_eta{};
    double bulk_mean_rho{};
    double edge_mean_rho{};
    double bulk_mean_P2{};
    double edge_mean_P2{};
    double bulk_mean_C{};
    double edge_mean_C{};
};

/**
 * Classify beads into bulk (high coordination) and edge (low coordination),
 * then compute per-group statistics.
 *
 * coord_threshold: beads with C >= threshold are bulk; below are edge.
 * If threshold <= 0, uses median C as the threshold.
 */
inline BulkEdgeStats classify_bulk_edge(
    const std::vector<coarse_grain::EnvironmentState>& states,
    double coord_threshold = -1.0)
{
    BulkEdgeStats be;
    if (states.empty()) return be;

    // Auto-threshold: use median C
    if (coord_threshold < 0.0) {
        std::vector<double> cs;
        cs.reserve(states.size());
        for (const auto& s : states) cs.push_back(s.C);
        std::sort(cs.begin(), cs.end());
        coord_threshold = cs[cs.size() / 2];
    }

    double b_eta = 0, b_rho = 0, b_P2 = 0, b_C = 0;
    double e_eta = 0, e_rho = 0, e_P2 = 0, e_C = 0;

    for (const auto& s : states) {
        if (s.C >= coord_threshold) {
            ++be.n_bulk;
            b_eta += s.eta; b_rho += s.rho; b_P2 += s.P2; b_C += s.C;
        } else {
            ++be.n_edge;
            e_eta += s.eta; e_rho += s.rho; e_P2 += s.P2; e_C += s.C;
        }
    }

    if (be.n_bulk > 0) {
        be.bulk_mean_eta = b_eta / be.n_bulk;
        be.bulk_mean_rho = b_rho / be.n_bulk;
        be.bulk_mean_P2  = b_P2 / be.n_bulk;
        be.bulk_mean_C   = b_C / be.n_bulk;
    }
    if (be.n_edge > 0) {
        be.edge_mean_eta = e_eta / be.n_edge;
        be.edge_mean_rho = e_rho / be.n_edge;
        be.edge_mean_P2  = e_P2 / be.n_edge;
        be.edge_mean_C   = e_C / be.n_edge;
    }
    return be;
}

// ============================================================================
// Stage 4 — Formation Regime Record
// ============================================================================

/**
 * FormationRegimeRecord — summary of one formation experiment.
 *
 * Captures final-state statistics, bulk/edge split, relaxation timing,
 * and a regime label for classification.
 */
struct FormationRegimeRecord {
    const char* scene_name{};
    int bead_count{};

    double mean_eta{};
    double std_eta{};
    double mean_rho{};
    double std_rho{};
    double mean_P2{};
    double mean_C{};

    double bulk_eta{};
    double edge_eta{};
    double bulk_rho{};
    double edge_rho{};

    double t_to_10pct{};
    double t_to_5pct{};
    double t_to_1pct{};

    bool bounded{true};
    bool finite{true};
    bool hysteretic{false};

    const char* regime_label{};
};

/**
 * Compute relaxation time: the step at which mean eta first comes
 * within (fraction * final_value) of the final value. Returns -1 if
 * the trajectory never reaches the threshold.
 */
inline double relaxation_time(
    const std::vector<FormationSnapshot>& snaps,
    double fraction,
    double dt)
{
    if (snaps.empty()) return -1.0;
    double final_eta = snaps.back().mean_eta;
    if (std::abs(final_eta) < 1e-15) return 0.0;
    double threshold = fraction * std::abs(final_eta);

    for (const auto& s : snaps) {
        if (std::abs(s.mean_eta - final_eta) < threshold) {
            return s.step * dt;
        }
    }
    return -1.0;
}

/**
 * Build a FormationRegimeRecord from a completed history.
 */
inline FormationRegimeRecord build_regime_record(
    const char* scene_name,
    const FormationHistory& hist,
    double dt)
{
    FormationRegimeRecord rec;
    rec.scene_name = scene_name;
    rec.bead_count = hist.bead_count;

    // Final-state statistics
    std::vector<double> etas, rhos;
    double sum_P2 = 0, sum_C = 0;
    for (const auto& s : hist.final_states) {
        etas.push_back(s.eta);
        rhos.push_back(s.rho);
        sum_P2 += s.P2;
        sum_C  += s.C;
    }

    auto eta_stats = compute_stats(etas);
    auto rho_stats = compute_stats(rhos);
    rec.mean_eta = eta_stats.mean;
    rec.std_eta  = std::sqrt(eta_stats.variance);
    rec.mean_rho = rho_stats.mean;
    rec.std_rho  = std::sqrt(rho_stats.variance);
    rec.mean_P2  = sum_P2 / hist.bead_count;
    rec.mean_C   = sum_C / hist.bead_count;

    // Bulk/edge split
    auto be = classify_bulk_edge(hist.final_states);
    rec.bulk_eta = be.bulk_mean_eta;
    rec.edge_eta = be.edge_mean_eta;
    rec.bulk_rho = be.bulk_mean_rho;
    rec.edge_rho = be.edge_mean_rho;

    // Relaxation timing
    rec.t_to_10pct = relaxation_time(hist.snapshots, 0.10, dt);
    rec.t_to_5pct  = relaxation_time(hist.snapshots, 0.05, dt);
    rec.t_to_1pct  = relaxation_time(hist.snapshots, 0.01, dt);

    // Boundedness / finiteness
    rec.bounded = is_bounded(etas, 0.0, 1.0);
    rec.finite  = is_finite(etas) && is_finite(rhos);

    return rec;
}

} // namespace test_util
