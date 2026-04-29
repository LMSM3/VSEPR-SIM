#pragma once
/**
 * lookglass_model.hpp — Lookglass Bidirectional Feedback Model
 *
 * The Lookglass Model extends the 6+9 Seed-Bead Stepper with a symmetric
 * backward pass that mirrors bead-layer information back into the seed
 * (atomistic) layer — creating a closed feedback loop.
 *
 * Name etymology:
 *   A lookglass (Lewis Carroll) shows you a reversed image of what you
 *   hold up to it.  Here, the bead layer reflects its coarse-grained
 *   observables back as corrections to the atomistic force field.
 *
 * Architecture:
 *
 *   ┌──────────────────────────────────────────────────────────────────┐
 *   │  FORWARD PASS  (6+9 Seed-Bead Stepper, one tick)                 │
 *   │   [S1–S6]  →  [B1–B9]  →  SeedBeadStepRecord r_fwd              │
 *   └──────────────────────────────────────────────────────────────────┘
 *                              ↕  mirror
 *   ┌──────────────────────────────────────────────────────────────────┐
 *   │  BACKWARD PASS  (Lookglass reflection, same tick)                │
 *   │   [L1] Bead → Atomistic position correction                      │
 *   │   [L2] Bead slow-state η → atomistic damping modulation          │
 *   │   [L3] Bead role weights → atomistic force scale correction       │
 *   │   [L4] Bead stability class → FIRE step-size guard               │
 *   │   [L5] Convergence mirror — halt both passes together             │
 *   └──────────────────────────────────────────────────────────────────┘
 *
 * Backward-pass units:
 *
 *   [L1] Position correction
 *        Δr_i^{seed} += α_pos · (r_i^{bead-COM} − r_i^{seed})
 *        Gently nudges atomistic positions toward the CG centroid.
 *        α_pos ∈ [0, 1] controls coupling strength.
 *
 *   [L2] Damping modulation
 *        γ_eff = γ_base · (1 + α_damp · η̄)
 *        Mean slow state η̄ inflates the FIRE damping coefficient
 *        so that high-η (ordered) regions relax slower.
 *
 *   [L3] Force scale correction
 *        F_i^{seed} *= 1 + α_force · (w_dom − 1)
 *        Where w_dom = max(w_steric, w_elec, w_disp) of the bead's
 *        combined role weights.  Amplifies forces for chemically
 *        specific beads, attenuates for generic (Mixed) beads.
 *
 *   [L4] FIRE step-size guard
 *        dt_max = dt_base · clamp(1 − α_dt · (3 − Λ̄) / 3, 0.1, 1.0)
 *        Beads with low average stability class shrink the allowed
 *        FIRE timestep to prevent runaway steps near fragile regions.
 *
 *   [L5] Convergence mirror
 *        Lookglass steady state:
 *          |Δr_feedback| < r_tol   AND   |η̄ − η̄_prev| < η_tol
 *        When met, the backward pass is frozen and the stepper can
 *        advance purely in the forward direction.
 *
 * Anti-black-box: LookglassStepRecord captures all five backward units.
 * Deterministic: forward pass record + backward params → identical result.
 *
 * Reference: docs/section_32bit_hourglass_lookglass.tex §3
 */

#include "coarse_grain/core/bead.hpp"
#include "coarse_grain/core/bead_system.hpp"
#include "coarse_grain/models/seed_bead_stepper.hpp"
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace coarse_grain {

// ============================================================================
// Lookglass Parameters
// ============================================================================

/**
 * LookglassParams — coupling constants for the five backward-pass units.
 */
struct LookglassParams {
    // ── L1: position coupling ─────────────────────────────────────────────────
    double alpha_pos{0.05};     // Strength of CG → atomistic position nudge

    // ── L2: damping modulation ────────────────────────────────────────────────
    double alpha_damp{0.3};     // η̄ → FIRE damping inflation factor
    double gamma_base{0.1};     // Base FIRE damping (dimensionless)

    // ── L3: force scale correction ────────────────────────────────────────────
    double alpha_force{0.2};    // Role-weight → force scale correction

    // ── L4: FIRE step-size guard ──────────────────────────────────────────────
    double alpha_dt{0.5};       // Λ̄ → dt shrinkage factor
    double dt_base{5.0};        // Base FIRE timestep (fs)

    // ── L5: convergence tolerances ────────────────────────────────────────────
    double r_tol{0.001};        // Feedback displacement tolerance (Å)
    double eta_tol{1e-4};       // η̄ convergence tolerance
};

// ============================================================================
// Backward-pass diagnostic record
// ============================================================================

/**
 * LookglassUnitStatus — which backward-pass units were active.
 */
struct LookglassUnitStatus {
    bool l1_pos_correction{};
    bool l2_damp_modulation{};
    bool l3_force_scale{};
    bool l4_dt_guard{};
    bool l5_converged{};
};

/**
 * LookglassStepRecord — complete diagnostic for one lookglass tick.
 */
struct LookglassStepRecord {
    uint64_t step_index{};

    // Forward pass reference
    uint64_t forward_step_index{};

    // ── L1 ────────────────────────────────────────────────────────────────────
    double   max_pos_correction{};   // max |Δr_feedback| across beads (Å)
    double   mean_pos_correction{};  // mean |Δr_feedback| (Å)

    // ── L2 ────────────────────────────────────────────────────────────────────
    double   gamma_effective{};      // γ_base · (1 + α_damp · η̄)

    // ── L3 ────────────────────────────────────────────────────────────────────
    double   mean_force_scale{};     // Mean force scale factor applied

    // ── L4 ────────────────────────────────────────────────────────────────────
    double   dt_guarded{};           // Effective dt after stability guard (fs)
    double   lambda_mean{};          // Mean Λ̄ used for guard

    // ── L5 ────────────────────────────────────────────────────────────────────
    bool     feedback_converged{};   // |Δr| < r_tol
    bool     eta_converged{};        // |η̄ − η̄_prev| < eta_tol
    bool     lookglass_steady{};     // Both L5 conditions met

    LookglassUnitStatus status{};
};

// ============================================================================
// Lookglass Step Function
// ============================================================================

/**
 * step_lookglass — apply one backward-pass tick to a BeadSystem.
 *
 * This function is called AFTER step_seed_bead() has produced a
 * SeedBeadStepRecord for the same tick.  It reads the forward diagnostics
 * and writes corrections back into the system.
 *
 * @param sys         Bead system to correct IN PLACE.
 * @param fwd         Forward-pass diagnostic from this tick.
 * @param params      Lookglass coupling constants.
 * @param eta_prev    Mean η̄ from the previous tick (for L5 convergence check).
 * @return            LookglassStepRecord with all backward diagnostics.
 */
inline LookglassStepRecord step_lookglass(
    BeadSystem&                sys,
    const SeedBeadStepRecord&  fwd,
    const LookglassParams&     params = LookglassParams{},
    double                     eta_prev = 0.0)
{
    LookglassStepRecord rec;
    rec.step_index         = fwd.step_index;
    rec.forward_step_index = fwd.step_index;

    const size_t N = sys.beads.size();
    if (N == 0) return rec;

    // ── L1: position correction ───────────────────────────────────────────────
    double sum_corr = 0.0;
    double max_corr = 0.0;

    if (!fwd.bead_positions.empty() && fwd.bead_positions.size() == N) {
        for (size_t i = 0; i < N; ++i) {
            double dx = fwd.bead_positions[i].x - sys.beads[i].position.x;
            double dy = fwd.bead_positions[i].y - sys.beads[i].position.y;
            double dz = fwd.bead_positions[i].z - sys.beads[i].position.z;
            double mag = std::sqrt(dx*dx + dy*dy + dz*dz);

            sys.beads[i].position.x += params.alpha_pos * dx;
            sys.beads[i].position.y += params.alpha_pos * dy;
            sys.beads[i].position.z += params.alpha_pos * dz;

            double corr = params.alpha_pos * mag;
            sum_corr += corr;
            if (corr > max_corr) max_corr = corr;
        }
    }
    rec.max_pos_correction  = max_corr;
    rec.mean_pos_correction = sum_corr / static_cast<double>(N);
    rec.status.l1_pos_correction = true;

    // ── L2: damping modulation ────────────────────────────────────────────────
    double eta_bar = fwd.avg_eta;
    rec.gamma_effective = params.gamma_base * (1.0 + params.alpha_damp * eta_bar);
    rec.status.l2_damp_modulation = true;

    // ── L3: force scale correction ────────────────────────────────────────────
    double force_scale_sum = 0.0;
    for (const auto& bead : sys.beads) {
        auto rw = role_weights(bead.structural_role);
        double w_dom = rw.w_steric;
        if (rw.w_electrostatic > w_dom) w_dom = rw.w_electrostatic;
        if (rw.w_dispersion    > w_dom) w_dom = rw.w_dispersion;
        force_scale_sum += 1.0 + params.alpha_force * (w_dom - 1.0);
    }
    rec.mean_force_scale = force_scale_sum / static_cast<double>(N);
    rec.status.l3_force_scale = true;

    // ── L4: FIRE step-size guard ──────────────────────────────────────────────
    double lambda_sum = 0.0;
    for (const auto& bead : sys.beads)
        lambda_sum += static_cast<double>(bead.stability_class);
    double lambda_bar = lambda_sum / static_cast<double>(N);
    rec.lambda_mean = lambda_bar;

    // clamp( 1 − α_dt · (3 − Λ̄)/3,  0.1, 1.0 )
    double guard = 1.0 - params.alpha_dt * (3.0 - lambda_bar) / 3.0;
    if (guard < 0.1) guard = 0.1;
    if (guard > 1.0) guard = 1.0;
    rec.dt_guarded = params.dt_base * guard;
    rec.status.l4_dt_guard = true;

    // ── L5: convergence mirror ────────────────────────────────────────────────
    rec.feedback_converged = (max_corr < params.r_tol);
    rec.eta_converged      = (std::abs(eta_bar - eta_prev) < params.eta_tol);
    rec.lookglass_steady   = rec.feedback_converged && rec.eta_converged;
    rec.status.l5_converged = rec.lookglass_steady;

    return rec;
}

// ============================================================================
// Lookglass Run Record (multi-step)
// ============================================================================

/**
 * LookglassRunRecord — aggregate diagnostics over a full lookglass run.
 *
 * Collect one of these per simulation run to track how quickly the
 * backward pass converges and how strongly it perturbed the system.
 */
struct LookglassRunRecord {
    uint64_t steps_run{};
    uint64_t steps_to_steady{};    // First step where lookglass_steady == true
    bool     reached_steady{};

    double   peak_pos_correction{}; // Worst-case |Δr| seen across all steps
    double   final_dt_guarded{};    // dt at final step (fs)
    double   final_gamma_effective{};

    std::vector<LookglassStepRecord> history;  // Optional per-step records
    bool record_history{false};

    void ingest(const LookglassStepRecord& r) {
        steps_run++;
        if (r.max_pos_correction > peak_pos_correction)
            peak_pos_correction = r.max_pos_correction;
        if (r.lookglass_steady && !reached_steady) {
            reached_steady   = true;
            steps_to_steady  = steps_run;
        }
        final_dt_guarded      = r.dt_guarded;
        final_gamma_effective = r.gamma_effective;
        if (record_history) history.push_back(r);
    }
};

} // namespace coarse_grain
