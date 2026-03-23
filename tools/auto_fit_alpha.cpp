/**
 * auto_fit_alpha.cpp
 * ==================
 * Fully automated, self-improving polarizability model trainer.
 *
 * Runs indefinitely in a loop of "generations".  Each generation:
 *   1. Perturbs the current-best parameters (simulated annealing).
 *   2. Runs coordinate descent with iterative reweighting (the proven
 *      inner engine from fit_alpha_model.cpp).
 *   3. Evaluates unweighted global RMS.
 *   4. If the generation improved the global best, checkpoints it.
 *   5. Writes three outputs every time a new best is found:
 *        - config/alpha_model_params.json         (JSON checkpoint)
 *        - config/alpha_best.hpp                  (ready-to-paste C++ struct)
 *        - config/training_log.csv                (full generation log)
 *
 * The program NEVER stops on its own — kill it when satisfied.
 * Progress is printed to stdout; all state is flushed to disk so you
 * can kill/restart without losing the best checkpoint.
 *
 * Strategies (cycle every 6 generations):
 *   Gen N%6 == 0: "Explore"   — large perturbation, wide scan ranges
 *   Gen N%6 == 1: "Refine"    — tight perturbation around best
 *   Gen N%6 == 2: "Block"     — freeze period/block, re-fit gates
 *   Gen N%6 == 3: "Shell"     — freeze base model, re-fit shell+rel
 *   Gen N%6 == 4: "Weighted"  — crank up chemistry weights, refit all
 *   Gen N%6 == 5: "Fresh"     — start from scratch (random init)
 *
 * Build:
 *   cmake --build . --target auto_fit_alpha
 * Run:
 *   ./auto_fit_alpha          (runs forever, Ctrl-C to stop)
 *   ./auto_fit_alpha 100      (run exactly 100 generations then stop)
 *
 * Reads:  data/polarizability_ref.csv
 * Writes: config/alpha_model_params.json
 *         config/alpha_best.hpp
 *         config/training_log.csv
 */

#include "atomistic/models/alpha_model.hpp"
#include "atomistic/models/atomic_descriptors.hpp"
#include "atomistic/models/nuclear_stability.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <limits>
#include <random>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using namespace atomistic::polarization;

// ============================================================================
// Reference data
// ============================================================================

struct RefEntry {
    uint32_t Z;
    std::string symbol;
    double alpha_ref;
    double weight;
    double weight_base;
};

static std::vector<RefEntry> load_csv(const char* path) {
    std::ifstream f(path);
    if (!f) {
        const char* fallbacks[] = {"data/polarizability_ref.csv",
                                   "../data/polarizability_ref.csv"};
        for (auto* fb : fallbacks) { f.open(fb); if (f) break; }
    }
    if (!f) { std::fprintf(stderr, "FATAL: cannot open CSV\n"); std::exit(1); }
    std::vector<RefEntry> entries;
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#' || line[0] == 'Z') continue;
        std::istringstream ss(line);
        std::string tok;
        RefEntry e{};
        std::getline(ss, tok, ','); e.Z = static_cast<uint32_t>(std::stoi(tok));
        std::getline(ss, e.symbol, ',');
        std::getline(ss, tok, ','); e.alpha_ref = std::stod(tok);
        std::getline(ss, tok, ','); e.weight_base = std::stod(tok);
        e.weight = e.weight_base;
        if (e.Z > 0 && e.alpha_ref > 0.0) entries.push_back(e);
    }
    return entries;
}

// ============================================================================
// Weight configuration
// ============================================================================

struct WeightConfig {
    double w_noble_gas      = 4.0;
    double w_halogen        = 2.5;
    double w_alkali         = 2.5;
    double w_early_actinide = 2.0;
    double w_hg             = 2.5;
    double w_group12        = 1.5;
    double heavy_z_lambda   = 1.0;
    double heavy_z_pow      = 2.0;
    double eta_smooth       = 0.15;
    double delta_huber      = 0.30;
    double alpha_reweight   = 0.25;
    double tol_target       = 0.15;
    double w_min            = 0.5;
    double w_max            = 10.0;
};

static void compute_initial_weights(std::vector<RefEntry>& data,
                                    const WeightConfig& cfg) {
    for (auto& e : data) {
        double w = e.weight_base;
        uint32_t g = desc::group(e.Z);
        if (g == 18)                w *= cfg.w_noble_gas;
        if (g == 17)                w *= cfg.w_halogen;
        if (g == 1)                 w *= cfg.w_alkali;
        if (e.Z >= 90 && e.Z <= 95) w *= cfg.w_early_actinide;
        if (e.Z == 80)              w *= cfg.w_hg;
        if (g == 12 && e.Z != 80)  w *= cfg.w_group12;
        double hz = cfg.heavy_z_lambda * std::pow(e.Z / 100.0, cfg.heavy_z_pow);
        w *= (1.0 + hz);
        e.weight = std::min(w, cfg.w_max);
    }
}

// ============================================================================
// Losses
// ============================================================================

static inline double huber(double e, double delta) noexcept {
    double ae = std::abs(e);
    return (ae <= delta) ? 0.5 * e * e : delta * (ae - 0.5 * delta);
}

using GroupPairs = std::vector<std::pair<int,int>>;

static GroupPairs build_group_pairs(const std::vector<RefEntry>& data) {
    GroupPairs pairs;
    for (uint32_t g = 1; g <= 18; ++g) {
        std::vector<std::pair<uint32_t,int>> members;
        for (int i = 0; i < (int)data.size(); ++i)
            if (desc::group(data[i].Z) == g)
                members.push_back({desc::period(data[i].Z), i});
        if (members.size() < 2) continue;
        std::sort(members.begin(), members.end());
        for (size_t k = 0; k + 1 < members.size(); ++k)
            pairs.push_back({members[k].second, members[k+1].second});
    }
    return pairs;
}

static double objective(const std::vector<RefEntry>& data,
                        const AlphaModelParams& params,
                        const GroupPairs& pairs,
                        const WeightConfig& cfg) {
    double loss = 0.0;
    for (const auto& e : data) {
        double pred = alpha_predict(e.Z, params);
        double le = std::log(pred) - std::log(e.alpha_ref);
        loss += e.weight * huber(le, cfg.delta_huber);
    }
    for (const auto& [a, b] : pairs) {
        double pa = std::log(alpha_predict(data[a].Z, params));
        double pb = std::log(alpha_predict(data[b].Z, params));
        double ra = std::log(data[a].alpha_ref);
        double rb = std::log(data[b].alpha_ref);
        loss += cfg.eta_smooth * huber((pb - pa) - (rb - ra), cfg.delta_huber);
    }
    return loss;
}

// ============================================================================
// Metrics
// ============================================================================

struct Metrics {
    double rms_global, max_pct;
    double rms_noble, rms_halogen, rms_alkali;
    double rms_block[4];
    int    mono_breaks;
    int    n_above_20;
};

static Metrics compute_metrics(const std::vector<RefEntry>& data,
                               const AlphaModelParams& params,
                               const GroupPairs& pairs) {
    Metrics m{};
    int N = (int)data.size();
    double sum_sq = 0;
    double ng_sq = 0, ng_n = 0;
    double hal_sq = 0, hal_n = 0;
    double alk_sq = 0, alk_n = 0;
    double blk_sq[4] = {}, blk_n[4] = {};
    for (const auto& e : data) {
        double pred = alpha_predict(e.Z, params);
        double pct = 100.0 * (pred - e.alpha_ref) / e.alpha_ref;
        double p2 = pct * pct;
        uint32_t g = desc::group(e.Z);
        uint32_t b = desc::block_index(e.Z);
        sum_sq += p2;
        blk_sq[b] += p2; blk_n[b] += 1;
        if (g == 18) { ng_sq += p2; ++ng_n; }
        if (g == 17) { hal_sq += p2; ++hal_n; }
        if (g == 1)  { alk_sq += p2; ++alk_n; }
        m.max_pct = std::max(m.max_pct, std::abs(pct));
        if (std::abs(pct) > 20.0) ++m.n_above_20;
    }
    m.rms_global  = std::sqrt(sum_sq / N);
    m.rms_noble   = ng_n  > 0 ? std::sqrt(ng_sq  / ng_n)  : 0;
    m.rms_halogen = hal_n > 0 ? std::sqrt(hal_sq / hal_n) : 0;
    m.rms_alkali  = alk_n > 0 ? std::sqrt(alk_sq / alk_n) : 0;
    for (int b = 0; b < 4; ++b)
        m.rms_block[b] = blk_n[b] > 0 ? std::sqrt(blk_sq[b] / blk_n[b]) : 0;
    for (const auto& [ai, bi] : pairs) {
        double pa = alpha_predict(data[ai].Z, params);
        double pb = alpha_predict(data[bi].Z, params);
        if ((pb > pa) != (data[bi].alpha_ref > data[ai].alpha_ref))
            ++m.mono_breaks;
    }
    return m;
}

// ============================================================================
// Coordinate descent engine (re-used from fit_alpha_model)
// ============================================================================

static double cd_sweep(std::vector<RefEntry>& data,
                       AlphaModelParams& best,
                       double best_loss,
                       const GroupPairs& pairs,
                       const WeightConfig& cfg,
                       int grid_pts = 80) {
    auto obj = [&](const AlphaModelParams&) {
        return objective(data, best, pairs, cfg);
    };

    auto scan = [&](double& param, double lo, double hi) {
        double p_best = param;
        for (int pass = 0; pass < 4; ++pass) {
            double range = hi - lo;
            double step = range / static_cast<double>(grid_pts);
            for (double v = lo; v <= hi; v += step) {
                param = v;
                double l = obj(best);
                if (l < best_loss) { best_loss = l; p_best = v; }
            }
            // Narrow window around best for next pass
            double margin = range * 0.04;  // 4% of current range
            lo = std::max(p_best - margin, 1e-6);
            hi = p_best + margin;
        }
        param = p_best;
        return best_loss;
    };

    // Base model
    for (int p = 0; p < 7; ++p)
        best_loss = scan(best.k_period[p], 0.1, 60.0);
    for (int b = 0; b < 4; ++b)
        best_loss = scan(best.c_block[b], 0.05, 10.0);

    // Binding stiffness + relativistic + f-shielding — interleaved re-tuning (4 rounds)
    for (int r = 0; r < 4; ++r) {
        best_loss = scan(best.b_bind,  0.0,   1.0);
        best_loss = scan(best.c_rel,  -1e-4,  1e-4);
        best_loss = scan(best.beta_f,  0.0,   0.5);
        for (int p = 0; p < 7; ++p)
            best_loss = scan(best.k_period[p], 0.1, 60.0);
        for (int b = 0; b < 4; ++b)
            best_loss = scan(best.c_block[b], 0.05, 10.0);
    }

    // F-block correction (4 rounds)
    for (int r = 0; r < 4; ++r) {
        best_loss = scan(best.a_f1,       -2.0,  2.0);
        best_loss = scan(best.a_f2,       -2.0,  4.0);
        best_loss = scan(best.a_f3,       -4.0,  2.0);
        best_loss = scan(best.blob_f_lin, -2.0,  2.0);
        best_loss = scan(best.beta_f,      0.0,  0.5);
        best_loss = scan(best.c_block[3],  0.05, 10.0);
        best_loss = scan(best.k_period[5], 0.1,  60.0);
        best_loss = scan(best.k_period[6], 0.1,  60.0);
    }
    return best_loss;
}

// ============================================================================
// Auto-weight update
// ============================================================================

static void update_weights(std::vector<RefEntry>& data,
                           const AlphaModelParams& params,
                           const WeightConfig& cfg) {
    for (auto& e : data) {
        double le = std::abs(std::log(alpha_predict(e.Z, params))
                           - std::log(e.alpha_ref));
        double excess = std::max(0.0, le / cfg.tol_target - 1.0);
        double w = e.weight * (1.0 + cfg.alpha_reweight * excess);
        e.weight = std::max(cfg.w_min, std::min(cfg.w_max, w));
    }
}

// ============================================================================
// One generation: N outer iterations of reweight + CD
// ============================================================================

static AlphaModelParams run_generation(
        std::vector<RefEntry>& data,
        AlphaModelParams init,
        const GroupPairs& pairs,
        WeightConfig cfg,
        int outer_iters,
        int grid_pts) {
    compute_initial_weights(data, cfg);
    AlphaModelParams best = init;
    double best_loss = objective(data, best, pairs, cfg);

    AlphaModelParams best_rms_params = best;
    double best_rms = 1e9;

    for (int outer = 0; outer < outer_iters; ++outer) {
        double prev = best_loss;
        for (int inner = 0; inner < 40; ++inner) {
            double nl = cd_sweep(data, best, best_loss, pairs, cfg, grid_pts);
            if (std::abs(nl - best_loss) < 1e-12) break;
            best_loss = nl;
        }
        auto m = compute_metrics(data, best, pairs);
        if (m.rms_global < best_rms) {
            best_rms = m.rms_global;
            best_rms_params = best;
        }
        update_weights(data, best, cfg);
        if (std::abs(prev - best_loss) < 1e-10 && outer >= 3) break;
    }
    return best_rms_params;
}

// ============================================================================
// Random parameter perturbation
// ============================================================================

static AlphaModelParams perturb(const AlphaModelParams& base,
                                std::mt19937& rng, double scale) {
    std::normal_distribution<double> gauss(0.0, 1.0);
    auto jitter = [&](double v, double lo, double hi) {
        double delta = (hi - lo) * scale * gauss(rng);
        return std::max(lo, std::min(hi, v + delta));
    };
    AlphaModelParams p = base;
    for (int i = 0; i < 7; ++i)
        p.k_period[i] = jitter(p.k_period[i], 0.1, 60.0);
    p.b_bind       = jitter(p.b_bind,    0.0,  1.0);
    p.c_rel        = jitter(p.c_rel,    -1e-4, 1e-4);
    p.beta_f       = jitter(p.beta_f,    0.0,  0.5);
    for (int i = 0; i < 4; ++i)
        p.c_block[i] = jitter(p.c_block[i], 0.05, 10.0);
    p.a_f1         = jitter(p.a_f1,     -2.0,  2.0);
    p.a_f2         = jitter(p.a_f2,     -2.0,  4.0);
    p.a_f3         = jitter(p.a_f3,     -4.0,  2.0);
    p.blob_f_lin   = jitter(p.blob_f_lin,-2.0,  2.0);
    return p;
}

static AlphaModelParams random_init(std::mt19937& rng) {
    std::uniform_real_distribution<double> u(0.0, 1.0);
    AlphaModelParams p;
    for (int i = 0; i < 7; ++i)
        p.k_period[i] = 1.0 + u(rng) * 30.0;
    p.b_bind       = u(rng) * 0.8;
    p.c_rel        = -1e-4 + u(rng) * 2e-4;
    p.beta_f       = u(rng) * 0.4;
    for (int i = 0; i < 4; ++i)
        p.c_block[i] = 0.3 + u(rng) * 3.0;
    p.a_f1         = -1.0 + u(rng) * 2.0;
    p.a_f2         = -1.0 + u(rng) * 4.0;
    p.a_f3         = -2.0 + u(rng) * 4.0;
    p.blob_f_lin   = -1.0 + u(rng) * 2.0;
    return p;
}

// ============================================================================
// Output writers
// ============================================================================

static void write_json(const char* path, const AlphaModelParams& p,
                       double rms, int gen) {
    std::ofstream f(path);
    if (!f) return;
    f << "{\n"
      << "  \"description\": \"Auto-fitted polarizability model (gen " << gen << ")\",\n"
      << "  \"rms_pct\": " << rms << ",\n"
      << "  \"model\": \"Alpha Method D: k_period*c_block*r_eff^3 * g_bind * g_f + blob\",\n"
      << "  \"k_period\": [";
    for (int i = 0; i < 7; ++i) {
        char b[64]; std::snprintf(b, sizeof(b), "%.8f", p.k_period[i]);
        f << b; if (i < 6) f << ", ";
    }
    f << "],\n";
    char b[128];
    std::snprintf(b, sizeof(b), "  \"b_bind\": %.8f,\n",  p.b_bind);  f << b;
    std::snprintf(b, sizeof(b), "  \"c_rel\": %.9f,\n",   p.c_rel);   f << b;
    std::snprintf(b, sizeof(b), "  \"beta_f\": %.8f,\n",  p.beta_f);  f << b;
    f << "  \"c_block\": [";
    for (int i = 0; i < 4; ++i) {
        std::snprintf(b, sizeof(b), "%.8f", p.c_block[i]);
        f << b; if (i < 3) f << ", ";
    }
    f << "],\n";
    std::snprintf(b, sizeof(b), "  \"a_f1\": %.8f,\n",      p.a_f1);      f << b;
    std::snprintf(b, sizeof(b), "  \"a_f2\": %.8f,\n",      p.a_f2);      f << b;
    std::snprintf(b, sizeof(b), "  \"a_f3\": %.8f,\n",      p.a_f3);      f << b;
    std::snprintf(b, sizeof(b), "  \"blob_f_lin\": %.8f\n", p.blob_f_lin); f << b;
    f << "}\n";
}

static void write_hpp(const char* path, const AlphaModelParams& p,
                      double rms, int gen) {
    std::ofstream f(path);
    if (!f) return;
    f << "// Auto-generated by auto_fit_alpha (gen " << gen
      << ", RMS=" << rms << "%)\n"
      << "// Paste into atomistic/models/alpha_model.hpp AlphaModelParams defaults\n\n";
    char b[128];
    f << "    double k_period[7] = {\n";
    for (int i = 0; i < 7; ++i) {
        std::snprintf(b, sizeof(b), "        %.8f", p.k_period[i]);
        f << b; if (i < 6) f << ","; f << "   // period " << (i+1) << "\n";
    }
    f << "    };\n\n";
    std::snprintf(b, sizeof(b), "    double b_bind     = %.8f;\n", p.b_bind);     f << b;
    std::snprintf(b, sizeof(b), "    double c_rel      = %.9f;\n", p.c_rel);      f << b;
    std::snprintf(b, sizeof(b), "    double beta_f     = %.8f;\n", p.beta_f);     f << b;
    f << "\n    double c_block[4] = {\n";
    const char* bn[] = {"s","p","d","f"};
    for (int i = 0; i < 4; ++i) {
        std::snprintf(b, sizeof(b), "        %.8f", p.c_block[i]);
        f << b; if (i < 3) f << ","; f << "   // " << bn[i] << "-block\n";
    }
    f << "    };\n\n";
    std::snprintf(b, sizeof(b), "    double a_f1       = %.8f;\n", p.a_f1);      f << b;
    std::snprintf(b, sizeof(b), "    double a_f2       = %.8f;\n", p.a_f2);      f << b;
    std::snprintf(b, sizeof(b), "    double a_f3       = %.8f;\n", p.a_f3);      f << b;
    std::snprintf(b, sizeof(b), "    double blob_f_lin = %.8f;\n", p.blob_f_lin); f << b;
}

static void append_log(const char* path, int gen, const char* strategy,
                       double rms, double max_pct, double rms_noble,
                       double rms_hal, int mono, int n20, double elapsed_s,
                       bool is_best) {
    bool exists = false;
    { std::ifstream test(path); exists = test.good(); }
    std::ofstream f(path, std::ios::app);
    if (!exists) {
        f << "gen,strategy,rms,max_pct,noble_rms,halogen_rms,"
             "mono_breaks,n_above_20,elapsed_s,is_best\n";
    }
    f << gen << "," << strategy << ","
      << rms << "," << max_pct << ","
      << rms_noble << "," << rms_hal << ","
      << mono << "," << n20 << ","
      << elapsed_s << "," << (is_best ? 1 : 0) << "\n";
}

// ============================================================================
// Main — infinite training loop
// ============================================================================

int main(int argc, char** argv) {
    int max_gen = 0;  // 0 = infinite
    if (argc > 1) max_gen = std::atoi(argv[1]);

    // Ensure output directory exists
    std::system("mkdir -p config 2>/dev/null || mkdir config 2>NUL");

    std::printf("╔══════════════════════════════════════════════════════════════╗\n");
    std::printf("║  AUTO-FIT ALPHA — Self-Improving Polarizability Trainer     ║\n");
    std::printf("║  Generations: %s                                       ║\n",
                max_gen > 0 ? std::to_string(max_gen).c_str() : "∞ (Ctrl-C to stop)");
    std::printf("╚══════════════════════════════════════════════════════════════╝\n\n");

    auto data_master = load_csv("data/polarizability_ref.csv");
    std::printf("Loaded %zu reference entries.\n", data_master.size());

    auto pairs = build_group_pairs(data_master);
    std::printf("Smoothness pairs: %zu\n\n", pairs.size());

    // Seed with current system time
    uint64_t seed = static_cast<uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    std::mt19937 rng(seed);

    // Global best — start from current baked defaults
    AlphaModelParams global_best;
    {
        auto data_tmp = data_master;
        WeightConfig cfg_tmp;
        compute_initial_weights(data_tmp, cfg_tmp);
        auto m = compute_metrics(data_tmp, global_best, pairs);
        std::printf("Starting RMS (baked defaults): %.2f%%\n\n", m.rms_global);
    }
    double global_best_rms = 1e9;
    {
        auto data_tmp = data_master;
        WeightConfig cfg_tmp;
        compute_initial_weights(data_tmp, cfg_tmp);
        global_best_rms = compute_metrics(data_tmp, global_best, pairs).rms_global;
    }

    const char* strategies[] = {
        "Explore", "Refine", "Block", "Shell", "Weighted", "Fresh"
    };

    for (int gen = 1; max_gen == 0 || gen <= max_gen; ++gen) {
        auto t0 = std::chrono::steady_clock::now();
        int strat_idx = (gen - 1) % 6;
        const char* strat = strategies[strat_idx];

        // Prepare this generation's starting params and config
        AlphaModelParams init;
        WeightConfig cfg;
        int outer_iters = 15;
        int grid_pts = 80;

        switch (strat_idx) {
        case 0: // Explore — large perturbation
            init = perturb(global_best, rng, 0.15);
            outer_iters = 20;
            break;
        case 1: // Refine — tight perturbation
            init = perturb(global_best, rng, 0.02);
            grid_pts = 120;
            outer_iters = 25;
            break;
        case 2: // Block — perturb only block/period params
            init = global_best;
            for (int i = 0; i < 7; ++i)
                init.k_period[i] *= (0.9 + 0.2 * std::uniform_real_distribution<>()(rng));
            for (int i = 0; i < 4; ++i)
                init.c_block[i] *= (0.9 + 0.2 * std::uniform_real_distribution<>()(rng));
            outer_iters = 15;
            break;
        case 3: // Shell — perturb only relativistic/f-shielding params
            init = global_best;
            init.beta_f *= (0.5 + 1.0 * std::uniform_real_distribution<>()(rng));
            init.c_rel  *= (0.5 + 1.0 * std::uniform_real_distribution<>()(rng));
            init.a_f1   *= (0.7 + 0.6 * std::uniform_real_distribution<>()(rng));
            init.a_f2   *= (0.7 + 0.6 * std::uniform_real_distribution<>()(rng));
            init.beta_f  = std::min(init.beta_f, 0.5);
            init.c_rel   = std::max(-1e-4, std::min(init.c_rel, 1e-4));
            outer_iters  = 20;
            break;
        case 4: // Weighted — crank up noble gas + Hg weights
            init = perturb(global_best, rng, 0.05);
            cfg.w_noble_gas = 6.0;
            cfg.w_hg = 4.0;
            cfg.w_halogen = 3.5;
            outer_iters = 20;
            break;
        case 5: // Fresh — completely random start
            init = random_init(rng);
            outer_iters = 30;
            break;
        }

        // Run the generation
        auto data = data_master;  // fresh copy each generation
        auto result = run_generation(data, init, pairs, cfg,
                                     outer_iters, grid_pts);

        // Evaluate
        data = data_master;
        WeightConfig cfg_eval;
        compute_initial_weights(data, cfg_eval);
        auto m = compute_metrics(data, result, pairs);

        auto t1 = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(t1 - t0).count();

        bool is_best = (m.rms_global < global_best_rms);
        if (is_best) {
            global_best_rms = m.rms_global;
            global_best = result;
            write_json("config/alpha_model_params.json", result, m.rms_global, gen);
            write_hpp("config/alpha_best.hpp", result, m.rms_global, gen);
        }

        append_log("config/training_log.csv", gen, strat,
                   m.rms_global, m.max_pct, m.rms_noble, m.rms_halogen,
                   m.mono_breaks, m.n_above_20, elapsed, is_best);

        // Print status line
        std::printf("Gen %4d [%-8s] RMS=%5.2f%%  noble=%5.1f%%  "
                    "halogen=%5.1f%%  max=%5.1f%%  mono=%2d  >20%%=%2d  "
                    "%.1fs%s\n",
                    gen, strat, m.rms_global, m.rms_noble,
                    m.rms_halogen, m.max_pct, m.mono_breaks,
                    m.n_above_20, elapsed,
                    is_best ? "  ★ NEW BEST" : "");
        std::fflush(stdout);

        // Every 10 generations, print the best summary
        if (gen % 10 == 0) {
            std::printf("\n  ──── Best after %d generations: RMS=%.2f%% ────\n\n",
                        gen, global_best_rms);
        }
    }

    // Final summary
    std::printf("\n════════════════════════════════════════════════════\n");
    std::printf("  Training complete.  Best RMS = %.4f%%\n", global_best_rms);
    std::printf("  Params written to config/alpha_best.hpp\n");
    std::printf("════════════════════════════════════════════════════\n");

    // Print the final best params
    auto& p = global_best;
    std::printf("\nFinal best parameters:\n");
    for (int i = 0; i < 7; ++i)
        std::printf("  k_period[%d] = %.8f\n", i+1, p.k_period[i]);
    std::printf("  b_bind       = %.8f\n", p.b_bind);
    std::printf("  c_rel        = %.9f\n", p.c_rel);
    std::printf("  beta_f       = %.8f\n", p.beta_f);
    for (int i = 0; i < 4; ++i) {
        const char* bn[] = {"s","p","d","f"};
        std::printf("  c_block[%s]  = %.8f\n", bn[i], p.c_block[i]);
    }
    std::printf("  a_f1         = %.8f\n", p.a_f1);
    std::printf("  a_f2         = %.8f\n", p.a_f2);
    std::printf("  a_f3         = %.8f\n", p.a_f3);
    std::printf("  blob_f_lin   = %.8f\n", p.blob_f_lin);

    return 0;
}
