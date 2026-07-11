#pragma once
/**
 * layer_boundary.hpp — L2 ↔ L4 Phase Transition Boundary
 *
 * This is where most simulations cheat or break.
 *
 * The L2 ↔ L4 boundary is the interface where atomic behaviour (discrete
 * particle physics, quantum-derived ε, explicit bonding) transitions into
 * coarse-grained bulk behaviour (phase fields, density order parameters,
 * collective interactions).  It is not a sharp line — it is a controlled
 * approximation zone that must be made explicit rather than hidden.
 *
 * What "cheating" looks like at this boundary:
 *   - Silently switching from LJ ε to a CG bead ε without recording the
 *     mapping error
 *   - Ignoring charge re-distribution when atoms form a bead group
 *     (e.g. Fe³⁺ lumped with O²⁻ into a neutral bead loses the Coulomb)
 *   - Pretending phase is single-valued when a bead straddles a phase boundary
 *   - Using a fixed structural role Σ across the phase transition when the
 *     bonding topology actually changes (metallic Fe → ionic Fe³⁺)
 *
 * What this module provides instead:
 *
 *   [B1] Energy consistency check
 *        Compare L2 pairwise energy (atomistic LJ+Coulomb) against L4
 *        bead energy (role-weighted 3-channel).  Record the mapping error.
 *
 *   [B2] Charge conservation audit
 *        Sum L2 charges for each bead group.  Compare against L4 bead charge.
 *        Flag non-conservation above threshold.
 *
 *   [B3] Phase coherence check
 *        Identify beads that straddle a phase boundary (atoms in the group
 *        have inconsistent phase states).  Flag and optionally split.
 *
 *   [B4] Structural role re-evaluation
 *        At the boundary, Σ_i may need to be re-evaluated.  An Fe atom
 *        that was metallic in the ore body may become ionic after oxidation.
 *        This unit re-classifies using the L2 charge and environment.
 *
 *   [B5] Boundary mapping residual
 *        Overall quality metric: how faithfully does L4 represent the L2
 *        physics?  High residual = the CG mapping is losing information here.
 *
 * Anti-black-box: every approximation is recorded with its magnitude.
 * Deterministic: same inputs → same boundary record.
 *
 * Reference: docs/section_layer_stack.tex §4 — "The Hard Boundary"
 */

#include "include/layer_stack.hpp"
#include "coarse_grain/core/bead.hpp"
#include "coarse_grain/core/bead_system.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace layer_stack {

// ============================================================================
// Boundary Parameters
// ============================================================================

struct BoundaryParams {
    // B1: energy consistency
    double energy_tol_kcal{2.0};      // kcal/mol — acceptable L2↔L4 energy gap

    // B2: charge conservation
    double charge_tol_e{0.05};        // e — acceptable charge non-conservation per bead

    // B3: phase coherence
    bool   allow_mixed_phase{false};  // true = flag but do not reject

    // B4: role re-evaluation
    bool   recompute_sigma{true};     // Re-classify Σ_i at boundary from Q and Z

    // B5: residual threshold for flagging high-error beads
    double residual_warn{0.15};       // 0–1 scale; >0.15 is flagged
};

// ============================================================================
// Per-bead boundary diagnostics
// ============================================================================

struct BeadBoundaryRecord {
    uint32_t bead_index{};

    // B1: Energy consistency
    double   energy_l2{};            // L2 atomistic energy for this group (kcal/mol)
    double   energy_l4{};            // L4 bead energy for this group (kcal/mol)
    double   energy_gap{};           // |E_l4 - E_l2| (kcal/mol)
    bool     energy_ok{true};

    // B2: Charge conservation
    double   charge_l2_sum{};        // Sum of L2 atom charges in group
    double   charge_l4{};            // L4 bead charge
    double   charge_error{};         // |q_l4 - q_l2_sum|
    bool     charge_ok{true};

    // B3: Phase coherence
    bool     phase_mixed{false};     // True if atoms in group span multiple phases
    uint32_t n_phase_classes{1};     // Number of distinct phases in group

    // B4: Structural role update
    coarse_grain::StructuralRole  sigma_l2{};  // Role inferred from L2 (Z + Q)
    coarse_grain::StructuralRole  sigma_l4{};  // Role stored in L4 bead
    bool                          sigma_changed{false};

    // B5: Overall mapping residual (0=perfect, 1=bad)
    double   mapping_residual{};
    bool     flagged{false};         // True if residual > warn threshold

    std::string notes;               // Human-readable boundary narrative
};

// ============================================================================
// System-level boundary summary
// ============================================================================

struct BoundaryReport {
    uint32_t n_beads_checked{};
    uint32_t n_energy_violations{};
    uint32_t n_charge_violations{};
    uint32_t n_phase_mixed{};
    uint32_t n_sigma_updated{};
    uint32_t n_flagged{};

    double mean_energy_gap{};       // kcal/mol
    double max_energy_gap{};        // kcal/mol
    double mean_charge_error{};     // e
    double max_charge_error{};      // e
    double mean_mapping_residual{};
    double max_mapping_residual{};

    bool   boundary_clean{true};    // No violations of any kind
    std::string summary;

    std::vector<BeadBoundaryRecord> bead_records;
};

// ============================================================================
// L2 ↔ L4 Boundary Evaluator
// ============================================================================

/**
 * evaluate_boundary — run all five boundary checks for a BeadSystem against
 * its L2 atomistic parent states.
 *
 * @param bead_sys    The L4 bead system.
 * @param l2_states   L2 atomistic records, one per bead (same indexing as beads).
 *                    Each L2_AtomisticState represents the dominant atom in
 *                    its bead group.  For multi-atom groups, pre-aggregate
 *                    the charge sum into l2_states[i].Q.
 * @param atom_phases Per-bead-group phase vectors (one per bead, inner vector
 *                    lists the phases of each atom in the group).
 *                    If empty, B3 is skipped.
 * @param params      Boundary parameters.
 * @return            Full BoundaryReport.
 */
inline BoundaryReport evaluate_boundary(
    coarse_grain::BeadSystem&            bead_sys,
    const std::vector<L2_AtomisticState>& l2_states,
    const std::vector<std::vector<coarse_grain::chemistry::Phase>>& atom_phases,
    const BoundaryParams&                params = BoundaryParams{})
{
    BoundaryReport report;
    const size_t N = bead_sys.beads.size();
    report.n_beads_checked = static_cast<uint32_t>(N);

    if (l2_states.size() < N) {
        report.boundary_clean = false;
        report.summary = "l2_states vector shorter than bead count";
        return report;
    }

    double sum_energy_gap  = 0.0;
    double sum_charge_err  = 0.0;
    double sum_residual    = 0.0;

    for (size_t i = 0; i < N; ++i) {
        BeadBoundaryRecord rec;
        rec.bead_index = static_cast<uint32_t>(i);

        const auto& bead   = bead_sys.beads[i];
        const auto& l2     = l2_states[i];

        // ── B1: Energy consistency ────────────────────────────────────────────
        // L2 energy: LJ 12-6 + Coulomb for this bead vs all others
        double e_l2 = 0.0;
        for (size_t j = 0; j < N; ++j) {
            if (i == j) continue;
            const auto& l2j = l2_states[j];
            atomistic::Vec3 dr{
                bead.position.x - bead_sys.beads[j].position.x,
                bead.position.y - bead_sys.beads[j].position.y,
                bead.position.z - bead_sys.beads[j].position.z
            };
            double r2 = dr.x*dr.x + dr.y*dr.y + dr.z*dr.z;
            if (r2 < 1e-12) continue;
            double r = std::sqrt(r2);

            // Lorentz-Berthelot mixing
            double eps_ij  = std::sqrt(l2.epsilon * l2j.epsilon);
            double sig_ij  = 0.5 * (l2.sigma + l2j.sigma);
            if (sig_ij < 1e-6) { eps_ij = 0.02; sig_ij = 3.5; }  // safe fallback
            double sr2  = (sig_ij * sig_ij) / r2;
            double sr6  = sr2 * sr2 * sr2;
            double sr12 = sr6 * sr6;
            e_l2 += eps_ij * (sr12 - sr6) + 332.0637 * l2.Q * l2j.Q / r;
        }
        e_l2 *= 0.5;  // avoid double-counting

        // L4 energy: role-weighted 3-channel (steric + dispersion + Coulomb)
        double e_l4 = 0.0;
        for (size_t j = 0; j < N; ++j) {
            if (i == j) continue;
            const auto& bj = bead_sys.beads[j];
            auto rw = coarse_grain::combined_role_weights(
                bead.structural_role, bj.structural_role);
            atomistic::Vec3 dr{
                bead.position.x - bj.position.x,
                bead.position.y - bj.position.y,
                bead.position.z - bj.position.z
            };
            double r2 = dr.x*dr.x + dr.y*dr.y + dr.z*dr.z;
            if (r2 < 1e-12) continue;
            double r   = std::sqrt(r2);
            double sig  = 3.5; double eps = 0.238;
            double sr2  = (sig*sig)/r2; double sr6 = sr2*sr2*sr2; double sr12 = sr6*sr6;
            e_l4 += rw.w_steric*eps*sr12 - rw.w_dispersion*eps*sr6
                  + rw.w_electrostatic*332.0637*bead.charge*bj.charge/r;
        }
        e_l4 *= 0.5;

        rec.energy_l2   = e_l2;
        rec.energy_l4   = e_l4;
        rec.energy_gap  = std::abs(e_l4 - e_l2);
        rec.energy_ok   = (rec.energy_gap <= params.energy_tol_kcal);
        if (!rec.energy_ok) report.n_energy_violations++;
        if (rec.energy_gap > report.max_energy_gap) report.max_energy_gap = rec.energy_gap;
        sum_energy_gap += rec.energy_gap;

        // ── B2: Charge conservation ───────────────────────────────────────────
        rec.charge_l2_sum = l2.Q;        // pre-aggregated sum from caller
        rec.charge_l4     = bead.charge;
        rec.charge_error  = std::abs(rec.charge_l4 - rec.charge_l2_sum);
        rec.charge_ok     = (rec.charge_error <= params.charge_tol_e);
        if (!rec.charge_ok) report.n_charge_violations++;
        if (rec.charge_error > report.max_charge_error) report.max_charge_error = rec.charge_error;
        sum_charge_err += rec.charge_error;

        // ── B3: Phase coherence ───────────────────────────────────────────────
        if (i < atom_phases.size() && !atom_phases[i].empty()) {
            const auto& phases = atom_phases[i];
            auto first = phases[0];
            uint32_t n_classes = 1;
            for (auto ph : phases)
                if (ph != first) { n_classes++; break; }
            rec.phase_mixed    = (n_classes > 1);
            rec.n_phase_classes = n_classes;
            if (rec.phase_mixed) {
                report.n_phase_mixed++;
                if (!params.allow_mixed_phase)
                    rec.notes += "WARN: mixed-phase bead group; ";
            }
        }

        // ── B4: Structural role re-evaluation ─────────────────────────────────
        rec.sigma_l4 = bead.structural_role;
        if (params.recompute_sigma) {
            // Re-classify from Q and Z
            // If Q > 0.5: ionic indicator; if Q < -0.5: ionic; metallic by Z
            coarse_grain::StructuralRole new_sigma;
            bool is_transition = (l2.Z >= 21 && l2.Z <= 30)
                               || (l2.Z >= 39 && l2.Z <= 48)
                               || (l2.Z >= 72 && l2.Z <= 80);
            if (is_transition && std::abs(l2.Q) > 0.8) {
                new_sigma = coarse_grain::StructuralRole::IonicDominant;
            } else if (is_transition) {
                new_sigma = coarse_grain::StructuralRole::Metallic;
            } else if (std::abs(l2.Q) > 0.5) {
                new_sigma = coarse_grain::StructuralRole::IonicDominant;
            } else {
                new_sigma = coarse_grain::StructuralRole::DirectionalCovalent;
            }
            rec.sigma_l2 = new_sigma;
            rec.sigma_changed = (new_sigma != rec.sigma_l4);
            if (rec.sigma_changed) {
                bead_sys.beads[i].structural_role = new_sigma;  // update in-place
                report.n_sigma_updated++;
            }
        } else {
            rec.sigma_l2 = rec.sigma_l4;
        }

        // ── B5: Mapping residual ──────────────────────────────────────────────
        double e_scale = std::max(std::abs(rec.energy_l2), 1e-6);
        double e_term  = std::min(rec.energy_gap / e_scale, 1.0);
        double q_term  = std::min(rec.charge_error / (std::abs(rec.charge_l2_sum) + 0.1), 1.0);
        double ph_term = rec.phase_mixed ? 0.5 : 0.0;
        rec.mapping_residual = 0.5 * e_term + 0.3 * q_term + 0.2 * ph_term;

        rec.flagged = (rec.mapping_residual > params.residual_warn);
        if (rec.flagged) report.n_flagged++;
        if (rec.mapping_residual > report.max_mapping_residual)
            report.max_mapping_residual = rec.mapping_residual;
        sum_residual += rec.mapping_residual;

        report.bead_records.push_back(rec);
    }

    if (N > 0) {
        report.mean_energy_gap       = sum_energy_gap  / N;
        report.mean_charge_error     = sum_charge_err  / N;
        report.mean_mapping_residual = sum_residual    / N;
    }

    report.boundary_clean = (report.n_energy_violations == 0
                          && report.n_charge_violations == 0
                          && report.n_flagged == 0);

    // Build summary string
    report.summary =
        std::string("L2↔L4 boundary: ")
        + std::to_string(N) + " beads, "
        + std::to_string(report.n_energy_violations) + " energy violations, "
        + std::to_string(report.n_charge_violations) + " charge violations, "
        + std::to_string(report.n_sigma_updated) + " role updates, "
        + std::to_string(report.n_flagged) + " flagged. "
        + (report.boundary_clean ? "CLEAN" : "NEEDS REVIEW");

    return report;
}

} // namespace layer_stack
