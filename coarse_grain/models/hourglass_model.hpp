#pragma once
/**
 * hourglass_model.hpp — Hourglass Convergence Model
 *
 * The Hourglass Model formalises the structural convergence funnel that
 * governs how a population of candidate bead configurations collapses
 * to a small set of stable attractors and then expands again into an
 * interpretable ensemble.
 *
 * Geometry analogy:
 *
 *   ████████████████   Wide mouth  — input layer: N_cand candidate states
 *        ██████        Neck        — bottleneck: constraint filter (Λ + ΔE)
 *   ████████████████   Wide base   — output layer: ranked stable configs
 *
 * Three layers:
 *
 *   [H1] Mouth  — candidate generation
 *        Produces N_cand bead configurations by perturbing the current
 *        system within ±δ_max around each bead's position.  Perturbations
 *        are drawn from a uniform ball (not Gaussian) so that the mouth
 *        samples a well-defined volume in configuration space.
 *
 *   [H2] Neck   — constraint bottleneck
 *        Each candidate passes through three gates in sequence:
 *          Gate 1  Stability filter  — Λ_i < Λ_min rejected immediately
 *          Gate 2  Energy filter     — ΔE > E_tol above basin minimum
 *          Gate 3  Role filter       — Σ_pair weight sum < w_min
 *        Only candidates passing all three gates survive the neck.
 *
 *   [H3] Base   — ranked output ensemble
 *        Surviving candidates are scored by:
 *          score = w_E · (E_ref − E) / |E_ref|
 *                + w_Λ · Λ_mean
 *                + w_Σ · W_role_sum
 *        Top-k candidates (k ≤ N_out) are returned in descending score.
 *
 * Anti-black-box: every layer produces a fully inspectable record.
 * Deterministic: identical system + params → identical ranked output.
 *
 * Reference: docs/section_32bit_hourglass_lookglass.tex §2
 */

#include "coarse_grain/core/bead.hpp"
#include "coarse_grain/core/bead_system.hpp"
#include "coarse_grain/models/seed_bead_stepper.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace coarse_grain {

// ============================================================================
// Hourglass Parameters
// ============================================================================

/**
 * HourglassParams — all tunable constants for one hourglass pass.
 */
struct HourglassParams {
    // ── Mouth (H1) ────────────────────────────────────────────────────────────
    uint32_t N_cand{64};        // Candidate configurations to generate
    double   delta_max{0.5};    // Maximum perturbation radius (Å)
    uint64_t rng_seed{42};      // RNG seed for reproducibility

    // ── Neck (H2) ─────────────────────────────────────────────────────────────
    StabilityClass Lambda_min{StabilityClass::Metastable}; // Gate 1: min stability
    double   E_tol{5.0};        // Gate 2: ΔE ceiling above basin (kcal/mol)
    double   w_role_min{0.5};   // Gate 3: minimum summed role-weight score

    // ── Base (H3) ─────────────────────────────────────────────────────────────
    uint32_t N_out{8};          // Maximum survivors to return
    double   w_E{1.0};          // Score weight for energy term
    double   w_Lambda{0.5};     // Score weight for stability term
    double   w_Sigma{0.3};      // Score weight for role-weight term
};

// ============================================================================
// Per-candidate diagnostics
// ============================================================================

/**
 * CandidateRecord — full diagnostic for one candidate configuration.
 */
struct CandidateRecord {
    uint32_t candidate_id{};

    // Geometry
    std::vector<atomistic::Vec3> positions;     // Bead centres (Å)

    // Energy
    double energy{};                            // Total energy (kcal/mol)
    double delta_E{};                           // E − E_basin (kcal/mol)

    // Identity statistics
    double lambda_mean{};                       // Mean Λ across beads
    double role_weight_sum{};                   // Sum of dominant channel weights

    // Gate results
    bool   passed_gate1{false};                 // Stability filter
    bool   passed_gate2{false};                 // Energy filter
    bool   passed_gate3{false};                 // Role filter
    bool   survived{false};                     // All three gates passed

    // Ranking
    double score{};                             // H3 composite score
    uint32_t rank{};                            // 1 = best
};

// ============================================================================
// Hourglass layer diagnostics
// ============================================================================

struct HourglassMouthRecord {
    uint32_t N_generated{};
    double   delta_used{};
    uint64_t rng_seed_used{};
};

struct HourglassNeckRecord {
    uint32_t N_in{};
    uint32_t N_rejected_gate1{};
    uint32_t N_rejected_gate2{};
    uint32_t N_rejected_gate3{};
    uint32_t N_survived{};
    double   E_basin{};        // Lowest energy seen (kcal/mol)
};

struct HourglassBaseRecord {
    uint32_t N_ranked{};
    double   best_score{};
    double   worst_score{};
};

/**
 * HourglassResult — complete output of one hourglass pass.
 */
struct HourglassResult {
    HourglassMouthRecord mouth{};
    HourglassNeckRecord  neck{};
    HourglassBaseRecord  base{};

    std::vector<CandidateRecord> candidates;    // All N_cand records (for audit)
    std::vector<uint32_t>        ranked_ids;    // Indices into candidates[], best first

    bool any_survived{false};
};

// ============================================================================
// Minimal LCG for reproducible perturbations (no <random> dependency)
// ============================================================================

namespace detail {

struct LCG {
    uint64_t state;
    explicit LCG(uint64_t seed) : state(seed ^ 0x853c49e6748fea9bull) {}

    uint64_t next() {
        state = state * 6364136223846793005ull + 1442695040888963407ull;
        return state;
    }

    // Uniform double in [−1, +1]
    double uniform11() {
        return static_cast<double>(static_cast<int64_t>(next() >> 11)) /
               static_cast<double>(1ull << 52);
    }
};

// Compute total energy and per-bead statistics from a bead system.
// Uses the same LJ + role-weight decomposition as seed_bead_stepper.
inline double quick_energy(const BeadSystem& sys) {
    double E = 0.0;
    const auto& beads = sys.beads;
    const size_t N = beads.size();

    for (size_t i = 0; i < N; ++i) {
        for (size_t j = i + 1; j < N; ++j) {
            const auto& bi = beads[i];
            const auto& bj = beads[j];
            auto rw = combined_role_weights(bi.structural_role, bj.structural_role);

            atomistic::Vec3 dr{
                bi.position.x - bj.position.x,
                bi.position.y - bj.position.y,
                bi.position.z - bj.position.z
            };
            double r2 = dr.x*dr.x + dr.y*dr.y + dr.z*dr.z;
            if (r2 < 1e-12) continue;

            // LJ parameters from BeadType sigma/epsilon
            double sigma   = 3.5;
            double epsilon = 0.238;
            double sr2     = (sigma * sigma) / r2;
            double sr6     = sr2 * sr2 * sr2;
            double sr12    = sr6 * sr6;

            double E_rep  =  rw.w_steric       * epsilon * sr12;
            double E_attr = -rw.w_dispersion    * epsilon * sr6;
            double E_coul =  rw.w_electrostatic * 332.0637
                              * bi.charge * bj.charge / std::sqrt(r2);
            E += E_rep + E_attr + E_coul;
        }
    }
    return E;
}

inline double lambda_mean(const BeadSystem& sys) {
    double sum = 0.0;
    for (const auto& b : sys.beads)
        sum += static_cast<double>(b.stability_class);
    return sys.beads.empty() ? 0.0 : sum / static_cast<double>(sys.beads.size());
}

inline double role_weight_sum(const BeadSystem& sys) {
    double sum = 0.0;
    for (const auto& b : sys.beads) {
        auto rw = role_weights(b.structural_role);
        double dom = rw.w_steric;
        if (rw.w_electrostatic > dom) dom = rw.w_electrostatic;
        if (rw.w_dispersion    > dom) dom = rw.w_dispersion;
        sum += dom;
    }
    return sum;
}

} // namespace detail

// ============================================================================
// Hourglass Pass
// ============================================================================

/**
 * run_hourglass — execute one full hourglass pass on a BeadSystem.
 *
 * The input system is NOT modified.  All candidate configurations are
 * generated as independent copies, evaluated, filtered, and ranked.
 *
 * @param sys     The reference bead system (defines the basin centre).
 * @param params  Hourglass tuning parameters.
 * @return        Fully populated HourglassResult.
 */
inline HourglassResult run_hourglass(
    const BeadSystem&     sys,
    const HourglassParams params = HourglassParams{})
{
    HourglassResult result;
    detail::LCG rng{params.rng_seed};

    // ── H1: Mouth — generate candidates ──────────────────────────────────────
    result.mouth.N_generated  = params.N_cand;
    result.mouth.delta_used   = params.delta_max;
    result.mouth.rng_seed_used = params.rng_seed;

    result.candidates.reserve(params.N_cand);

    double E_basin = 1e30;

    for (uint32_t c = 0; c < params.N_cand; ++c) {
        CandidateRecord rec;
        rec.candidate_id = c;

        // Deep-copy bead positions and apply random perturbation
        BeadSystem cand = sys;
        rec.positions.reserve(cand.beads.size());

        for (auto& bead : cand.beads) {
            double dx = rng.uniform11() * params.delta_max;
            double dy = rng.uniform11() * params.delta_max;
            double dz = rng.uniform11() * params.delta_max;
            bead.position.x += dx;
            bead.position.y += dy;
            bead.position.z += dz;
            rec.positions.push_back(bead.position);
        }

        rec.energy = detail::quick_energy(cand);
        if (rec.energy < E_basin) E_basin = rec.energy;

        rec.lambda_mean      = detail::lambda_mean(cand);
        rec.role_weight_sum  = detail::role_weight_sum(cand);

        result.candidates.push_back(std::move(rec));
    }

    // ── H2: Neck — apply three gates ─────────────────────────────────────────
    result.neck.N_in     = params.N_cand;
    result.neck.E_basin  = E_basin;

    for (auto& rec : result.candidates) {
        rec.delta_E = rec.energy - E_basin;

        // Gate 1: stability
        double lambda_floor = static_cast<double>(params.Lambda_min);
        rec.passed_gate1 = (rec.lambda_mean >= lambda_floor);
        if (!rec.passed_gate1) {
            result.neck.N_rejected_gate1++;
            continue;
        }

        // Gate 2: energy
        rec.passed_gate2 = (rec.delta_E <= params.E_tol);
        if (!rec.passed_gate2) {
            result.neck.N_rejected_gate2++;
            continue;
        }

        // Gate 3: role weight
        double w_per_bead = sys.beads.empty() ? 0.0
            : rec.role_weight_sum / static_cast<double>(sys.beads.size());
        rec.passed_gate3 = (w_per_bead >= params.w_role_min);
        if (!rec.passed_gate3) {
            result.neck.N_rejected_gate3++;
            continue;
        }

        rec.survived = true;
    }

    result.neck.N_survived = params.N_cand
        - result.neck.N_rejected_gate1
        - result.neck.N_rejected_gate2
        - result.neck.N_rejected_gate3;

    // ── H3: Base — score and rank survivors ───────────────────────────────────
    double E_ref = E_basin;

    for (auto& rec : result.candidates) {
        if (!rec.survived) continue;
        double e_term  = params.w_E      * (E_ref - rec.energy) / (std::abs(E_ref) + 1e-30);
        double lam_term = params.w_Lambda * (rec.lambda_mean / 3.0);
        double sig_term = params.w_Sigma  * (sys.beads.empty() ? 0.0
            : rec.role_weight_sum / static_cast<double>(sys.beads.size()));
        rec.score = e_term + lam_term + sig_term;
        result.ranked_ids.push_back(rec.candidate_id);
    }

    // Sort by score descending
    std::sort(result.ranked_ids.begin(), result.ranked_ids.end(),
        [&](uint32_t a, uint32_t b) {
            return result.candidates[a].score > result.candidates[b].score;
        });

    // Truncate to N_out
    if (result.ranked_ids.size() > params.N_out)
        result.ranked_ids.resize(params.N_out);

    // Assign ranks
    for (uint32_t r = 0; r < static_cast<uint32_t>(result.ranked_ids.size()); ++r)
        result.candidates[result.ranked_ids[r]].rank = r + 1;

    result.any_survived = !result.ranked_ids.empty();
    result.base.N_ranked = static_cast<uint32_t>(result.ranked_ids.size());
    if (!result.ranked_ids.empty()) {
        result.base.best_score  = result.candidates[result.ranked_ids.front()].score;
        result.base.worst_score = result.candidates[result.ranked_ids.back()].score;
    }

    return result;
}

// ============================================================================
// Convenience: best candidate accessor
// ============================================================================

/**
 * hourglass_best — return a pointer to the top-ranked candidate, or
 * nullptr if no candidates survived the neck.
 */
inline const CandidateRecord* hourglass_best(const HourglassResult& r) {
    if (r.ranked_ids.empty()) return nullptr;
    return &r.candidates[r.ranked_ids[0]];
}

} // namespace coarse_grain
