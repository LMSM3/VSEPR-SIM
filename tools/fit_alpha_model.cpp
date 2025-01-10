/**
 * fit_alpha_model.cpp
 * ===================
 * Offline deterministic fitter for Alpha Method D (v2.8.X).
 *
 * Algorithm (two-loop iterative reweighting):
 *
 *   OUTER loop (reweighting, N=50):
 *     INNER loop (coordinate descent, N=80):
 *       Minimise:  L(theta) = Huber_loss(log pred - log ref; delta)
 *                           + eta * smoothness_penalty(theta)
 *       using weighted coordinate descent over all 19 parameters.
 *     Evaluate residuals per element.
 *     Update per-element weights (push toward persistent failures).
 *     Log full diagnostic table.
 *
 * Loss components:
 *   Huber(e; delta) = e^2/2              if |e| <= delta
 *                   = delta*(|e| - d/2)  otherwise
 *   Applied in log-space: e = log(alpha_pred) - log(alpha_ref).
 *   delta = 0.30 (about 35% relative error).
 *
 * Smoothness penalty:
 *   For elements in the same group (consecutive periods), the MODEL'S
 *   log-differences should match the REFERENCE'S log-differences:
 *     L_smooth = eta * SUM_{adjacent pairs by group}
 *                Huber(d_pred - d_ref; delta)
 *   where d = log(alpha[Z_{p+1}]) - log(alpha[Z_p]).
 *   This constrains group-wise trend direction, not just pointwise values.
 *
 * Chemistry-aware weights (initial):
 *   Noble gases  (group 18):          x4.0  — shell-closure regime
 *   Halogens     (group 17):          x2.5  — late p-block before closure
 *   Alkalis      (group 1):           x2.5  — s-block trend anchors
 *   Early actinides (Th-Am, Z=90-95): x2.0  — 5f/6d crossover
 *   Heavy-Z amplification:            x(1 + lambda*(Z/100)^p)
 *
 * Auto-weight update rule (per outer iteration):
 *   r_i = |log(alpha_pred) - log(alpha_ref)|   [absolute log error]
 *   w_i <- clip(w_i * (1 + alpha * max(0, r_i/tol - 1)), w_min, w_max)
 *   Persistent failures accumulate weight; well-fitted elements stabilise.
 *
 * Reads:   data/polarizability_ref.csv
 * Writes:  config/alpha_model_params.json
 */

#include "atomistic/models/alpha_model.hpp"
#include "atomistic/models/atomic_descriptors.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using namespace atomistic::polarization;

// ============================================================================
// Chemistry-aware weighting configuration
// ============================================================================

struct WeightConfig {
    double w_noble_gas     = 4.0;   // group 18: shell-closure, must get trend right
    double w_halogen       = 2.5;   // group 17: late p-block before closure
    double w_alkali        = 2.5;   // group 1: s-block trend anchors
    double w_early_actinide = 2.0;  // Z=90-95 (Th-Am): 5f/6d crossover
    double w_hg            = 2.5;   // Hg (Z=80): relativistic inert-pair anchor
    double w_group12       = 1.5;   // Zn/Cd/Cn (group 12): filled-d anomaly
    double heavy_z_lambda  = 1.0;   // heavy-Z amplification coefficient
    double heavy_z_pow     = 2.0;   // exponent for (Z/100)^p
    double eta_smooth      = 0.15;  // smoothness penalty coupling (s/p/d-block)
    double eta_smooth_f    = 0.0;   // smoothness penalty for f-block pairs (0 = disabled)
    double delta_huber     = 0.30;  // Huber threshold (log units, ~35% rel. error)
    double alpha_reweight  = 0.25;  // weight update rate per outer iteration
    double tol_target      = 0.15;  // per-element residual tolerance (log units)
    double w_min           = 0.5;
    double w_max           = 10.0;
};

// ============================================================================
// CSV parser
// ============================================================================

struct RefEntry {
    uint32_t    Z;
    std::string symbol;
    double      alpha_ref;
    double      weight;          // working weight (updated each outer iteration)
    double      weight_base;     // CSV-loaded weight (never modified)
};

static std::vector<RefEntry> load_csv(const char* path) {
    std::ifstream f(path);
    if (!f) {
        const char* fallbacks[] = {"data/polarizability_ref.csv",
                                   "../data/polarizability_ref.csv"};
        for (auto* fb : fallbacks) { f.open(fb); if (f) break; }
    }
    if (!f) { std::fprintf(stderr, "ERROR: cannot open %s\n", path); std::exit(1); }

    std::vector<RefEntry> entries;
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#' || line[0] == 'Z') continue;
        std::istringstream ss(line);
        std::string tok;
        RefEntry e{};
        std::getline(ss, tok, ','); e.Z = static_cast<uint32_t>(std::stoi(tok));
        std::getline(ss, e.symbol, ',');
        std::getline(ss, tok, ','); e.alpha_ref  = std::stod(tok);
        std::getline(ss, tok, ','); e.weight_base = std::stod(tok);
        e.weight = e.weight_base;
        if (e.Z > 0 && e.alpha_ref > 0.0) entries.push_back(e);
    }
    return entries;
}

// ============================================================================
// T3 — Training coverage check
// ============================================================================

static void check_coverage(const std::vector<RefEntry>& data) {
    bool period_covered[8] = {};
    bool block_covered[4]  = {};
    const char* block_names[] = {"s", "p", "d", "f"};

    for (const auto& e : data) {
        if (e.weight_base < 0.2) continue;
        uint32_t p = desc::period(e.Z);
        uint32_t b = desc::block_index(e.Z);
        if (p >= 1 && p <= 7) period_covered[p] = true;
        if (b <= 3)           block_covered[b]  = true;
    }

    int errors = 0;
    for (int p = 1; p <= 7; ++p) {
        if (!period_covered[p]) {
            std::fprintf(stderr, "[T3 FAIL] k_period[%d]: no training data.\n", p);
            ++errors;
        }
    }
    for (int b = 0; b < 4; ++b) {
        if (!block_covered[b]) {
            std::fprintf(stderr, "[T3 FAIL] c_block[%s]: no training data.\n", block_names[b]);
            ++errors;
        }
    }
    if (errors) {
        std::fprintf(stderr,
            "FATAL: %d identifiability failure(s). "
            "Extend data/polarizability_ref.csv before fitting.\n", errors);
        std::exit(1);
    }
    std::printf("[T3] Coverage check passed.\n\n");
}

// ============================================================================
// Compute chemistry-aware initial weights
// ============================================================================

static void compute_initial_weights(std::vector<RefEntry>& data,
                                    const WeightConfig& cfg) {
    for (auto& e : data) {
        double w = e.weight_base;
        uint32_t g = desc::group(e.Z);

        if (g == 18)                             w *= cfg.w_noble_gas;
        if (g == 17)                             w *= cfg.w_halogen;
        if (g == 1)                              w *= cfg.w_alkali;
        if (e.Z >= 90 && e.Z <= 95)              w *= cfg.w_early_actinide;
        if (e.Z == 80)                           w *= cfg.w_hg;       // Hg: relativistic anchor
        if (g == 12 && e.Z != 80)               w *= cfg.w_group12;  // Zn/Cd/Cn: filled-d

        // Heavy-Z amplification: higher Z -> harder to fit -> more weight
        double hz = cfg.heavy_z_lambda * std::pow(e.Z / 100.0, cfg.heavy_z_pow);
        w *= (1.0 + hz);

        e.weight = std::min(w, cfg.w_max);
    }
}

// ============================================================================
// Huber loss (applied in log space)
// ============================================================================

static inline double huber(double e, double delta) noexcept {
    double ae = std::abs(e);
    return (ae <= delta) ? 0.5 * e * e : delta * (ae - 0.5 * delta);
}

// ============================================================================
// Group-pair index for smoothness penalty
// Pairs of (idx_in_data_i, idx_in_data_j) where j is one period above i
// in the same group (consecutive group members that both appear in the dataset).
// ============================================================================

using GroupPairs = std::vector<std::pair<int,int>>;

static GroupPairs build_group_pairs(const std::vector<RefEntry>& data) {
    // Build a map from Z to data index
    std::vector<int> z_to_idx(119, -1);
    for (int i = 0; i < static_cast<int>(data.size()); ++i)
        if (data[i].Z <= 118) z_to_idx[data[i].Z] = i;

    GroupPairs pairs;

    // For each group, find consecutive elements sorted by period
    for (uint32_t g = 1; g <= 18; ++g) {
        std::vector<std::pair<uint32_t,int>> members; // (period, data_idx)
        for (int i = 0; i < static_cast<int>(data.size()); ++i) {
            if (desc::group(data[i].Z) == g) {
                members.push_back({desc::period(data[i].Z), i});
            }
        }
        if (members.size() < 2) continue;
        std::sort(members.begin(), members.end());
        for (size_t k = 0; k + 1 < members.size(); ++k)
            pairs.push_back({members[k].second, members[k+1].second});
    }
    return pairs;
}

// ============================================================================
// Full objective: Huber(log error) + smoothness penalty
// ============================================================================

static double objective_full(const std::vector<RefEntry>& data,
                             const AlphaModelParams& params,
                             const GroupPairs& pairs,
                             const WeightConfig& cfg) {
    double loss = 0.0;

    // Pointwise Huber loss in log-space
    for (const auto& e : data) {
        double pred    = alpha_predict(e.Z, params);
        double log_err = std::log(pred) - std::log(e.alpha_ref);
        loss += e.weight * huber(log_err, cfg.delta_huber);
    }

    // Smoothness penalty: consecutive group-pairs must maintain correct trend direction.
    // F-block pairs use eta_smooth_f (default 0.0) to avoid penalising real anomalies.
    for (const auto& [a, b] : pairs) {
        const bool is_fblock = (desc::block_index(data[a].Z) == 3 ||
                                desc::block_index(data[b].Z) == 3);
        const double eta = is_fblock ? cfg.eta_smooth_f : cfg.eta_smooth;
        if (eta == 0.0) continue;
        double pred_a = std::log(alpha_predict(data[a].Z, params));
        double pred_b = std::log(alpha_predict(data[b].Z, params));
        double ref_a  = std::log(data[a].alpha_ref);
        double ref_b  = std::log(data[b].alpha_ref);
        double d_pred = pred_b - pred_a;
        double d_ref  = ref_b  - ref_a;
        loss += eta * huber(d_pred - d_ref, cfg.delta_huber);
    }

    return loss;
}

// ============================================================================
// Auto-weight update
// ============================================================================

static void update_weights(std::vector<RefEntry>& data,
                           const AlphaModelParams& params,
                           const WeightConfig& cfg) {
    for (auto& e : data) {
        double pred    = alpha_predict(e.Z, params);
        double log_err = std::abs(std::log(pred) - std::log(e.alpha_ref));
        // If residual exceeds tolerance, increase weight proportionally
        double excess = std::max(0.0, log_err / cfg.tol_target - 1.0);
        double w_new  = e.weight * (1.0 + cfg.alpha_reweight * excess);
        e.weight = std::max(cfg.w_min, std::min(cfg.w_max, w_new));
    }
}

// ============================================================================
// Inner coordinate descent — one full sweep over all 19 parameters
// Alpha Method D (v2.8.X)
// ============================================================================

static double cd_sweep(std::vector<RefEntry>& data,
                       AlphaModelParams& best,
                       double best_loss,
                       const GroupPairs& pairs,
                       const WeightConfig& cfg) {
    auto obj = [&](const AlphaModelParams& p) {
        return objective_full(data, p, pairs, cfg);
    };

    auto scan_param = [&](double& param, double lo, double hi) {
        const double lo_floor = lo;
        double p_best = param;
        for (int pass = 0; pass < 4; ++pass) {
            double range = hi - lo;
            double step = range / 100.0;
            for (double v = lo; v <= hi; v += step) {
                param = v;
                double loss = obj(best);
                if (loss < best_loss) { best_loss = loss; p_best = v; }
            }
            double margin = range * 0.04;
            lo = std::max(p_best - margin, lo_floor);
            hi = p_best + margin;
        }
        param = p_best;
        return best_loss;
    };

    // --- Core multiplicative structure ---
    for (int p = 0; p < 7; ++p)
        best_loss = scan_param(best.k_period[p], 0.1, 60.0);
    for (int b = 0; b < 4; ++b)
        best_loss = scan_param(best.c_block[b], 0.05, 10.0);

    // --- Binding stiffness (replaces softness/chi/shell-closure) ---
    // Scan b_bind, c_rel, and beta_f jointly with period/block
    // since they couple through the same r^3 * g_bind product.
    for (int refine = 0; refine < 4; ++refine) {
        best_loss = scan_param(best.b_bind, 0.0, 1.0);
        best_loss = scan_param(best.c_rel, -1e-4, 1e-4);
        best_loss = scan_param(best.beta_f, 0.0, 0.5);
        for (int p = 0; p < 7; ++p)
            best_loss = scan_param(best.k_period[p], 0.1, 60.0);
        for (int b = 0; b < 4; ++b)
            best_loss = scan_param(best.c_block[b], 0.05, 10.0);
    }

    // --- F-block correction (isolated: g_f = 1 for non-f-block) ---
    for (int refine_f = 0; refine_f < 6; ++refine_f) {
        best_loss = scan_param(best.a_f1,     -2.0,  2.0);
        best_loss = scan_param(best.a_f2,     -2.0,  4.0);
        best_loss = scan_param(best.a_f3,     -4.0,  2.0);
        best_loss = scan_param(best.sigma_f1,  0.5, 10.0);
        best_loss = scan_param(best.sigma_f2,  0.5, 10.0);
        // Additive blob: only blob_f_lin active.
        // blob_f_half/full FROZEN until residual pattern shows localized spikes.
        best_loss = scan_param(best.blob_f_lin,  -2.0,  2.0);
        // best_loss = scan_param(best.blob_f_half, -10.0, 10.0);  // FROZEN
        // best_loss = scan_param(best.blob_f_full, -10.0, 10.0);  // FROZEN
        // Re-tune beta_f + c_block[f] + period-6/7 (lanthanide/actinide)
        best_loss = scan_param(best.beta_f, 0.0, 0.5);
        best_loss = scan_param(best.c_block[3],  0.05, 10.0);
        best_loss = scan_param(best.k_period[5], 0.1,  60.0);
        best_loss = scan_param(best.k_period[6], 0.1,  60.0);
    }

    return best_loss;
}

// ============================================================================
// Diagnostic metrics for one outer iteration
// ============================================================================

struct Metrics {
    double rms_global;          // unweighted RMS % error (linear space)
    double rms_log;             // unweighted RMS of |log(pred/ref)| (log space, *100 for display)
    double rms_weighted;        // weighted RMS % error
    double max_pct;
    double rms_block[4];        // s/p/d/f
    double rms_period[8];       // index 1-7
    double rms_noble;
    double rms_halogen;
    double rms_alkali;
    int    monotonicity_breaks; // count of adjacent group pairs with wrong trend sign
    int    sign_flips;          // count of sign changes in consecutive group errors
    int    N;
};

static Metrics compute_metrics(const std::vector<RefEntry>& data,
                                const AlphaModelParams& params,
                                const GroupPairs& pairs) {
    Metrics m{};
    m.N = static_cast<int>(data.size());

    double sum_sq = 0, sum_wt_sq = 0, sum_wt = 0, sum_log_sq = 0;
    double blk_sq[4] = {}, blk_wt[4] = {};
    double per_sq[8] = {}, per_wt[8] = {};
    double ng_sq = 0, ng_n = 0;
    double hal_sq = 0, hal_n = 0;
    double alk_sq = 0, alk_n = 0;

    for (const auto& e : data) {
        double pred    = alpha_predict(e.Z, params);
        double pct     = 100.0 * (pred - e.alpha_ref) / e.alpha_ref;
        double log_err = std::log(pred) - std::log(e.alpha_ref);
        double p2      = pct * pct;
        uint32_t g  = desc::group(e.Z);
        uint32_t p  = desc::period(e.Z);
        uint32_t b  = desc::block_index(e.Z);

        sum_sq      += p2;
        sum_log_sq  += log_err * log_err;
        sum_wt_sq   += e.weight * p2;
        sum_wt    += e.weight;
        blk_sq[b] += p2; blk_wt[b] += 1;
        per_sq[p] += p2; per_wt[p] += 1;
        if (g == 18) { ng_sq  += p2; ++ng_n; }
        if (g == 17) { hal_sq += p2; ++hal_n; }
        if (g == 1)  { alk_sq += p2; ++alk_n; }
        m.max_pct = std::max(m.max_pct, std::abs(pct));
    }
    m.rms_global   = std::sqrt(sum_sq / m.N);
    m.rms_log      = std::sqrt(sum_log_sq / m.N) * 100.0;  // scaled to % for display
    m.rms_weighted = std::sqrt(sum_wt_sq / sum_wt);
    for (int b = 0; b < 4; ++b)
        m.rms_block[b] = blk_wt[b] > 0 ? std::sqrt(blk_sq[b] / blk_wt[b]) : 0;
    for (int p = 1; p <= 7; ++p)
        m.rms_period[p] = per_wt[p] > 0 ? std::sqrt(per_sq[p] / per_wt[p]) : 0;
    m.rms_noble   = ng_n  > 0 ? std::sqrt(ng_sq  / ng_n)  : 0;
    m.rms_halogen = hal_n > 0 ? std::sqrt(hal_sq / hal_n) : 0;
    m.rms_alkali  = alk_n > 0 ? std::sqrt(alk_sq / alk_n) : 0;

    // Monotonicity breaks and sign flips in group-adjacent pairs
    for (const auto& [ai, bi] : pairs) {
        double pred_a = alpha_predict(data[ai].Z, params);
        double pred_b = alpha_predict(data[bi].Z, params);
        double ref_a  = data[ai].alpha_ref;
        double ref_b  = data[bi].alpha_ref;
        bool ref_up   = (ref_b  > ref_a);
        bool pred_up  = (pred_b > pred_a);
        if (ref_up != pred_up) ++m.monotonicity_breaks;

        // Sign flip: error changed sign between adjacent elements in same group
        double err_a = pred_a - ref_a;
        double err_b = pred_b - ref_b;
        if ((err_a > 0) != (err_b > 0)) ++m.sign_flips;
    }

    return m;
}

static void print_metrics(const Metrics& m, int outer) {
    const char* block_names[] = {"s", "p", "d", "f"};
    std::printf("\n  Outer %2d | RMS=%.1f%%  logRMS=%.1f%%  wRMS=%.1f%%  Max=%.1f%%"
                "  mono_breaks=%d  sign_flips=%d\n",
                outer, m.rms_global, m.rms_log, m.rms_weighted, m.max_pct,
                m.monotonicity_breaks, m.sign_flips);
    std::printf("           | Block: ");
    for (int b = 0; b < 4; ++b)
        std::printf("%s=%.1f%% ", block_names[b], m.rms_block[b]);
    std::printf("\n           | Period: ");
    for (int p = 1; p <= 7; ++p)
        std::printf("P%d=%.1f%% ", p, m.rms_period[p]);
    std::printf("\n           | Groups: noble=%.1f%% halogen=%.1f%% alkali=%.1f%%\n",
                m.rms_noble, m.rms_halogen, m.rms_alkali);
}

// ============================================================================
// Two-loop fitter
// ============================================================================

static AlphaModelParams fit(std::vector<RefEntry>& data,
                            const WeightConfig& cfg,
                            const AlphaModelParams& start_params) {
    auto pairs = build_group_pairs(data);
    std::printf("Smoothness pairs: %zu group-adjacent element pairs.\n\n", pairs.size());

    compute_initial_weights(data, cfg);

    AlphaModelParams best = start_params;
    double best_loss = objective_full(data, best, pairs, cfg);
    std::printf("Initial loss: %.6f\n", best_loss);

    // Track the best unweighted RMS checkpoint separately from the weighted loss.
    // The weighted loss will keep rising as weights saturate — we want the params
    // at the lowest *unweighted* RMS, not the final ones.
    AlphaModelParams best_rms_params = best;
    double best_rms = 1e9;
    int best_rms_outer = 0;

    for (int outer = 0; outer < 50; ++outer) {
        std::printf("\n--- Outer iteration %d (reweighting round) ---\n", outer + 1);

        // Inner coordinate descent
        double prev_loss = best_loss;
        int inner_sweeps = 0;
        for (int inner = 0; inner < 80; ++inner) {
            double new_loss = cd_sweep(data, best, best_loss, pairs, cfg);
            ++inner_sweeps;
            if (std::abs(new_loss - best_loss) < 1e-12) break;
            best_loss = new_loss;
        }
        double loss_delta = prev_loss - best_loss;
        std::printf("  Inner CD: %d sweeps  loss=%.6f  delta=%.8f\n",
                    inner_sweeps, best_loss, loss_delta);

        // Compute and print diagnostic metrics
        Metrics m = compute_metrics(data, best, pairs);
        print_metrics(m, outer + 1);

        // Track best unweighted RMS checkpoint
        if (m.rms_global < best_rms) {
            best_rms        = m.rms_global;
            best_rms_params = best;
            best_rms_outer  = outer + 1;
            std::printf("  [*] New best unweighted RMS: %.2f%% at outer %d\n",
                        best_rms, best_rms_outer);
        }

        // Update weights based on residuals
        update_weights(data, best, cfg);

        // Track weight saturation: how many elements are at w_max?
        int n_sat = 0;
        for (const auto& e : data) if (e.weight >= cfg.w_max * 0.99) ++n_sat;

        // Print top-5 highest-weight elements (most persistently failing)
        auto sorted_data = data;
        std::sort(sorted_data.begin(), sorted_data.end(),
                  [](const RefEntry& a, const RefEntry& b) { return a.weight > b.weight; });
        std::printf("  Weights: %d/%d saturated at w_max  Top-5: ",
                    n_sat, (int)data.size());
        for (int i = 0; i < std::min(5, (int)sorted_data.size()); ++i)
            std::printf("%-3s(w=%.1f) ", sorted_data[i].symbol.c_str(), sorted_data[i].weight);
        std::printf("\n");

        // Early stopping: if loss_delta < threshold AND no saturation growth, done
        if (loss_delta < 1e-8 && outer >= 5) {
            std::printf("  [Converged: loss_delta=%.2e < 1e-8 at outer=%d]\n",
                        loss_delta, outer + 1);
            break;
        }
    }

    std::printf("\n[Best checkpoint] outer=%d  unweighted RMS=%.2f%%\n",
                best_rms_outer, best_rms);
    return best_rms_params;
}

// ============================================================================
// Final report
// ============================================================================

static void report(const std::vector<RefEntry>& data,
                   const AlphaModelParams& params,
                   const GroupPairs& pairs) {
    const char* block_names[] = {"s", "p", "d", "f"};

    struct Row { uint32_t Z; std::string sym; double ref, pred, pct;
                 uint32_t per, blk, grp; };
    std::vector<Row> rows;
    rows.reserve(data.size());
    for (const auto& e : data) {
        double pred = alpha_predict(e.Z, params);
        double pct  = 100.0 * (pred - e.alpha_ref) / e.alpha_ref;
        rows.push_back({e.Z, e.symbol, e.alpha_ref, pred, pct,
                        desc::period(e.Z), desc::block_index(e.Z), desc::group(e.Z)});
    }

    // Full table
    std::printf("\n%-4s %-4s %10s %10s %7s\n", "Z", "Sym", "alpha_ref", "alpha_pred", "pct_err");
    std::printf("---- ---- ---------- ---------- -------\n");
    for (const auto& r : rows)
        std::printf("%4u %-4s %10.3f %10.3f %+6.1f%%\n",
                    r.Z, r.sym.c_str(), r.ref, r.pred, r.pct);

    // Overall
    Metrics m = compute_metrics(data, params, pairs);
    std::printf("\nFinal metrics:\n");
    std::printf("  Overall N=%d  RMS=%.1f%%  wRMS=%.1f%%  Max=%.1f%%\n",
                m.N, m.rms_global, m.rms_weighted, m.max_pct);
    std::printf("  Monotonicity breaks: %d / %zu pairs\n",
                m.monotonicity_breaks, pairs.size());
    std::printf("  Sign flips in adjacent group errors: %d\n\n", m.sign_flips);

    std::printf("Per-block RMS:\n");
    for (int b = 0; b < 4; ++b)
        std::printf("  %s-block  RMS=%5.1f%%\n", block_names[b], m.rms_block[b]);

    std::printf("\nPer-period RMS:\n");
    for (int p = 1; p <= 7; ++p)
        std::printf("  period %d  RMS=%5.1f%%\n", p, m.rms_period[p]);

    std::printf("\nGroup RMS:\n");
    std::printf("  noble gases  RMS=%.1f%%  (shell-closure proxy failure regime)\n", m.rms_noble);
    std::printf("  halogens     RMS=%.1f%%\n", m.rms_halogen);
    std::printf("  alkalis      RMS=%.1f%%\n", m.rms_alkali);

    // Top-10 worst offenders
    auto sorted = rows;
    std::sort(sorted.begin(), sorted.end(),
              [](const Row& a, const Row& b) { return std::abs(a.pct) > std::abs(b.pct); });
    std::printf("\nTop-10 worst offenders:\n");
    for (int i = 0; i < std::min(10, (int)sorted.size()); ++i)
        std::printf("  Z=%3u %-4s  ref=%.3f  pred=%.3f  %+6.1f%%\n",
                    sorted[i].Z, sorted[i].sym.c_str(),
                    sorted[i].ref, sorted[i].pred, sorted[i].pct);

    // Stop analysis: which regime still dominates
    std::printf("\nStop analysis (residuals > 20%%):\n");
    int n_above = 0;
    double blk_above[4] = {}, blk_n_above[4] = {};
    for (const auto& r : rows) {
        if (std::abs(r.pct) > 20.0) {
            ++n_above;
            blk_above[r.blk] += r.pct * r.pct;
            blk_n_above[r.blk] += 1;
        }
    }
    std::printf("  %d elements above 20%% error\n", n_above);
    for (int b = 0; b < 4; ++b) {
        if (blk_n_above[b] > 0)
            std::printf("  %s-block: %.0f of them (RMS=%.1f%%)\n",
                        block_names[b], blk_n_above[b],
                        std::sqrt(blk_above[b] / blk_n_above[b]));
    }
}

// ============================================================================
// JSON writer — Alpha Method D (v2.8.X)
// ============================================================================

static void write_json(const char* path, const AlphaModelParams& p) {
    std::ofstream f(path);
    if (!f) { std::fprintf(stderr, "ERROR: cannot write %s\n", path); return; }
    f << "{\n"
      << "  \"description\": \"Alpha Method D (v2.8.X) fitted parameters\",\n"
      << "  \"model\": \"alpha = r_eff^3 * g_block * g_period * g_f(n_f) * g_bind\",\n"
      << "  \"source\": \"tools/fit_alpha_model.cpp (iterative reweighting + Huber)\",\n"
      << "  \"k_period\": [\n";
    for (int i = 0; i < 7; ++i) {
        char buf[64]; std::snprintf(buf, sizeof(buf), "    %.6f", p.k_period[i]);
        f << buf; if (i < 6) f << ","; f << "  // period " << (i+1) << "\n";
    }
    f << "  ],\n";
    char buf[128];
    f << "  \"c_block\": [\n";
    const char* bn[] = {"s","p","d","f"};
    for (int i = 0; i < 4; ++i) {
        std::snprintf(buf, sizeof(buf), "    %.6f", p.c_block[i]);
        f << buf; if (i < 3) f << ","; f << "  // " << bn[i] << "-block\n";
    }
    f << "  ],\n";
    std::snprintf(buf, sizeof(buf), "  \"c_rel\":     %.9f,\n", p.c_rel);     f << buf;
    std::snprintf(buf, sizeof(buf), "  \"beta_f\":    %.9f,\n", p.beta_f);    f << buf;
    std::snprintf(buf, sizeof(buf), "  \"a_f1\":      %.6f,\n", p.a_f1);      f << buf;
    std::snprintf(buf, sizeof(buf), "  \"a_f2\":      %.6f,\n", p.a_f2);      f << buf;
    std::snprintf(buf, sizeof(buf), "  \"a_f3\":      %.6f,\n", p.a_f3);      f << buf;
    std::snprintf(buf, sizeof(buf), "  \"sigma_f1\":  %.6f,\n", p.sigma_f1);  f << buf;
    std::snprintf(buf, sizeof(buf), "  \"sigma_f2\":  %.6f,\n", p.sigma_f2);  f << buf;
    std::snprintf(buf, sizeof(buf), "  \"b_bind\":    %.6f,\n", p.b_bind);    f << buf;
    std::snprintf(buf, sizeof(buf), "  \"q_drude\":   %.6f,\n", p.q_drude);   f << buf;
    std::snprintf(buf, sizeof(buf), "  \"blob_f_lin\": %.6f,\n", p.blob_f_lin); f << buf;
    std::snprintf(buf, sizeof(buf), "  \"blob_f_half\": %.6f,\n", p.blob_f_half); f << buf;
    std::snprintf(buf, sizeof(buf), "  \"blob_f_full\": %.6f\n", p.blob_f_full); f << buf;
    f << "}\n";
    std::printf("\nWrote: %s\n", path);
}

// ============================================================================
// Warm-start loader
// Reads config/alpha_model_params.json (produced by write_json) and populates
// params.  Allows continual training: run fit_alpha_model repeatedly and each
// run refines from where the previous one left off.
// Returns true on success, false if file not found or unparseable.
// ============================================================================

static bool try_load_warmstart(const char* path, AlphaModelParams& params) {
    std::ifstream f(path);
    if (!f) return false;

    auto strip = [](std::string& s) {
        // strip C++ comments and trailing commas
        auto c = s.find("//");
        if (c != std::string::npos) s = s.substr(0, c);
        while (!s.empty() && (s.back() == ',' || s.back() == ' ' || s.back() == '\t'))
            s.pop_back();
    };

    int k_idx = 0, b_idx = 0;
    bool in_kp = false, in_cb = false;
    int loaded = 0;
    std::string line;

    while (std::getline(f, line)) {
        strip(line);

        // Section headers
        if (line.find("\"k_period\"") != std::string::npos &&
            line.find('[') != std::string::npos) { in_kp = true; in_cb = false; k_idx = 0; continue; }
        if (line.find("\"c_block\"") != std::string::npos &&
            line.find('[') != std::string::npos) { in_cb = true; in_kp = false; b_idx = 0; continue; }
        if (line.find(']') != std::string::npos) { in_kp = false; in_cb = false; continue; }

        // Try to extract the last numeric token
        std::istringstream ss(line);
        std::string tok, last_num;
        while (ss >> tok) {
            if (tok.empty()) continue;
            char fc = tok.front();
            if (std::isdigit(fc) || fc == '-' || fc == '.') {
                while (!tok.empty() && (tok.back() == ',' || tok.back() == '"'))
                    tok.pop_back();
                last_num = tok;
            }
        }
        if (last_num.empty()) continue;

        double val = 0.0;
        try { val = std::stod(last_num); } catch (...) { continue; }

        if (in_kp && k_idx < 7) { params.k_period[k_idx++] = val; ++loaded; continue; }
        if (in_cb && b_idx < 4) { params.c_block[b_idx++]  = val; ++loaded; continue; }

        if (line.find("\"c_rel\"")    != std::string::npos) { params.c_rel    = val; ++loaded; }
        if (line.find("\"beta_f\"")   != std::string::npos) { params.beta_f   = val; ++loaded; }
        if (line.find("\"a_f1\"")     != std::string::npos) { params.a_f1     = val; ++loaded; }
        if (line.find("\"a_f2\"")     != std::string::npos) { params.a_f2     = val; ++loaded; }
        if (line.find("\"a_f3\"")     != std::string::npos) { params.a_f3     = val; ++loaded; }
        if (line.find("\"sigma_f1\"") != std::string::npos) { params.sigma_f1 = val; ++loaded; }
        if (line.find("\"sigma_f2\"") != std::string::npos) { params.sigma_f2 = val; ++loaded; }
        if (line.find("\"b_bind\"")   != std::string::npos) { params.b_bind   = val; ++loaded; }
        if (line.find("\"q_drude\"")  != std::string::npos) { params.q_drude  = val; ++loaded; }
        if (line.find("\"blob_f_lin\"")  != std::string::npos) { params.blob_f_lin  = val; ++loaded; }
        if (line.find("\"blob_f_half\"") != std::string::npos) { params.blob_f_half = val; ++loaded; }
        if (line.find("\"blob_f_full\"") != std::string::npos) { params.blob_f_full = val; ++loaded; }
    }

    if (loaded >= 15) {
        std::printf("[Warm-start] Loaded %d params from %s\n", loaded, path);
        return true;
    }
    std::printf("[Warm-start] %s: only %d params read, using defaults.\n", path, loaded);
    return false;
}

// ============================================================================
// Stochastic perturbation — jitter warm-start params so each run explores
// a different trajectory through the coupled 19-D parameter space.
// ============================================================================

static void perturb_params(AlphaModelParams& p, std::mt19937& rng, double scale) {
    std::normal_distribution<double> norm(0.0, scale);
    auto jitter = [&](double& val, double lo, double hi) {
        val *= (1.0 + norm(rng));
        val = std::max(lo, std::min(hi, val));
    };
    for (int i = 0; i < 7; ++i) jitter(p.k_period[i], 0.1, 60.0);
    for (int i = 0; i < 4; ++i) jitter(p.c_block[i],  0.05, 10.0);
    jitter(p.c_rel,    -1e-4, 1e-4);
    jitter(p.beta_f,    0.0,   0.5);
    jitter(p.a_f1,     -2.0,  2.0);
    jitter(p.a_f2,     -2.0,  4.0);
    jitter(p.a_f3,     -4.0,  2.0);
    jitter(p.sigma_f1,  0.5, 10.0);
    jitter(p.sigma_f2,  0.5, 10.0);
    jitter(p.b_bind,    0.0,  1.0);
    jitter(p.q_drude,   0.1,  5.0);
    jitter(p.blob_f_lin,  -2.0,  2.0);
    jitter(p.blob_f_half, -10.0, 10.0);
    jitter(p.blob_f_full, -10.0, 10.0);
}

// ============================================================================
// Stochastic data subsampling — each run trains on a random subset so
// different runs explore different loss landscapes.  Final evaluation is
// always on the full dataset.
// ============================================================================

static std::vector<RefEntry> subsample(const std::vector<RefEntry>& data,
                                       std::mt19937& rng, double keep_frac) {
    std::uniform_real_distribution<double> u01(0.0, 1.0);
    std::vector<RefEntry> result;
    result.reserve(data.size());
    for (const auto& e : data) {
        if (u01(rng) < keep_frac) result.push_back(e);
    }
    // Safety: keep at least 80% to maintain coverage
    if (result.size() < static_cast<size_t>(data.size() * 0.8))
        return data;
    return result;
}

// ============================================================================
// main
// ============================================================================

int main() {
    std::setvbuf(stdout, nullptr, _IOLBF, 0);  // line-buffered for TUI monitor
    std::printf("=== Alpha Method D Fitter (v2.8.X) ===\n\n");

    // ── Stochastic seed (from FIT_SEED env or random_device) ──
    uint32_t seed = 0;
    const char* seed_env = std::getenv("FIT_SEED");
    if (seed_env && seed_env[0]) {
        seed = static_cast<uint32_t>(std::stoul(seed_env));
    } else {
        seed = std::random_device{}();
    }
    std::mt19937 rng(seed);
    std::printf("Seed: %u\n\n", seed);

    auto full_data = load_csv("data/polarizability_ref.csv");
    std::printf("Loaded %zu reference entries.\n", full_data.size());

    check_coverage(full_data);

    WeightConfig cfg;

    // ── Warm-start with perturbation ──
    AlphaModelParams start_params;
    const char* json_path = "config/alpha_model_params.json";
    bool warm = try_load_warmstart(json_path, start_params);
    double incumbent_rms = 1e9;
    if (warm) {
        auto fp = build_group_pairs(full_data);
        auto m0 = compute_metrics(full_data, start_params, fp);
        incumbent_rms = m0.rms_global;
        std::printf("[Warm-start] Incumbent RMS: %.2f%%\n", incumbent_rms);
        perturb_params(start_params, rng, 0.03);
        std::printf("[Warm-start] Parameters perturbed (scale=3%%)\n");
    } else {
        std::printf("[Cold-start] Using defaults.\n");
    }

    // ── Stochastic data subsampling ──
    auto train_data = subsample(full_data, rng, 0.88);
    std::printf("Training on %zu/%zu entries (stochastic subsample)\n\n",
                train_data.size(), full_data.size());

    auto params = fit(train_data, cfg, start_params);

    std::printf("\n\nFitted parameters (Alpha Method D):\n");
    for (int i = 0; i < 7; ++i)
        std::printf("  k_period[%d] = %.6f\n", i+1, params.k_period[i]);
    std::printf("  c_block[s]  = %.6f\n", params.c_block[0]);
    std::printf("  c_block[p]  = %.6f\n", params.c_block[1]);
    std::printf("  c_block[d]  = %.6f\n", params.c_block[2]);
    std::printf("  c_block[f]  = %.6f\n", params.c_block[3]);
    std::printf("  c_rel       = %.9f\n", params.c_rel);
    std::printf("  beta_f      = %.9f\n", params.beta_f);
    std::printf("  a_f1        = %.6f\n", params.a_f1);
    std::printf("  a_f2        = %.6f\n", params.a_f2);
    std::printf("  a_f3        = %.6f\n", params.a_f3);
    std::printf("  sigma_f1    = %.6f\n", params.sigma_f1);
    std::printf("  sigma_f2    = %.6f\n", params.sigma_f2);
    std::printf("  b_bind      = %.6f\n", params.b_bind);
    std::printf("  q_drude     = %.6f\n", params.q_drude);
    std::printf("  blob_f_lin  = %.6f\n", params.blob_f_lin);
    std::printf("  blob_f_half = %.6f\n", params.blob_f_half);
    std::printf("  blob_f_full = %.6f\n", params.blob_f_full);

    // ── Report on full dataset (not the subsample) ──
    auto full_pairs = build_group_pairs(full_data);
    report(full_data, params, full_pairs);

    // ── Conditional write: only update JSON if this run improved ──
    auto final_m = compute_metrics(full_data, params, full_pairs);
    if (final_m.rms_global < incumbent_rms) {
        write_json(json_path, params);
        std::printf("\n[Checkpoint] Improved: %.2f%% < %.2f%% (incumbent)\n",
                    final_m.rms_global, incumbent_rms);
    } else {
        std::printf("\n[No improvement] %.2f%% >= %.2f%% (incumbent); JSON unchanged.\n",
                    final_m.rms_global, incumbent_rms);
    }

    return 0;
}
