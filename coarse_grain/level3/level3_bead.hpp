#pragma once
/**
 * level3_bead.hpp — Level 3 Coarse-Grained Bead
 *
 * A Level 3 Bead is a spatial domain aggregate of converged Level 2 beads.
 * It is the coarsest resolution tier in the VSEPR-SIM multiscale hierarchy
 * and the direct entry point for macro-dynamics (macro-DM) modelling.
 *
 * Scale hierarchy:
 *
 *   Atomistic (Å)          full atom positions, bonds
 *        ↓ map_reaction_to_beads()
 *   Level 1 Bead           per-atom-group (~1–5 atoms, CG mapped)
 *        ↓ SeedBeadStepper (6+9 FIRE)
 *   Level 2 Bead           converged, environment-responsive (η, ρ, C, P₂)
 *        ↓ aggregate_to_l3()
 *   Level 3 Bead           domain aggregate, macro-DM ready    ← this file
 *        ↓ MacroPrecursorState
 *   Macro-DM               continuum / FEA / transport model input
 *
 * Domain formation:
 *   L3 domains are formed by greedy spatial clustering of L2 beads.
 *   Each L3 bead aggregates all L2 beads within r_domain of a seed bead.
 *   Minimum cluster size: L3_MIN_MEMBERS (default 4).
 *
 * Anti-black-box:
 *   All aggregate fields carry explicit provenance (member bead indices).
 *   All formulas are documented. No hidden weights.
 *
 * Reference: coarse_grain/level3/README.md
 *            coarse_grain/analysis/macro_precursor.hpp
 */

#include "coarse_grain/core/bead.hpp"
#include "coarse_grain/core/environment_state.hpp"
#include "coarse_grain/qm/qm_descriptors.hpp"
#include "atomistic/core/state.hpp"
#include <cmath>
#include <cstdint>
#include <vector>

namespace coarse_grain {
namespace level3 {

// ============================================================================
// Constants
// ============================================================================

/** Default domain radius for L3 clustering (Å). */
constexpr double L3_DEFAULT_DOMAIN_RADIUS = 15.0;

/** Minimum number of L2 beads per L3 domain. */
constexpr int L3_MIN_MEMBERS = 4;

// ============================================================================
// Level3Bead
// ============================================================================

/**
 * Level3Bead — one domain in the L3 representation.
 *
 * Aggregates structural, thermodynamic, and QM information from a
 * cluster of converged Level 2 beads. Carries the full payload needed
 * for macro-DM and FEA handoff.
 *
 * Aggregate formulas (all explicit):
 *
 *   position_com:   Σᵢ mᵢ·rᵢ / Σᵢ mᵢ         (mass-weighted COM)
 *   total_mass:     Σᵢ mᵢ                       (amu)
 *   total_charge:   Σᵢ qᵢ                       (e)
 *   mean_eta:       (1/N) Σᵢ ηᵢ                 (mean slow state)
 *   mean_rho:       (1/N) Σᵢ ρᵢ                 (mean local density)
 *   mean_C:         (1/N) Σᵢ Cᵢ                 (mean coordination)
 *   mean_P2:        (1/N) Σᵢ P₂,ᵢ               (mean orientational order)
 *   var_eta:        sample variance of {ηᵢ}      (structural disorder signal)
 *   qm:             aggregate QMDescriptor       (see below)
 *   valid:          true iff N_members ≥ L3_MIN_MEMBERS and all L2 converged
 */
struct Level3Bead {
    // --- Identity ---
    uint32_t domain_id{};               // Index of this L3 domain

    // --- Geometry ---
    atomistic::Vec3 position_com{};     // Mass-weighted COM of member beads (Å)
    double          radius{};           // Effective domain radius (Å)

    // --- Aggregate mass / charge ---
    double total_mass{};                // Σ mᵢ (amu)
    double total_charge{};              // Σ qᵢ (e)

    // --- Aggregate environment state ---
    double mean_eta{};                  // (1/N) Σ ηᵢ
    double mean_rho{};                  // (1/N) Σ ρᵢ
    double mean_C{};                    // (1/N) Σ Cᵢ
    double mean_P2{};                   // (1/N) Σ P₂,ᵢ
    double var_eta{};                   // Sample variance of {ηᵢ}
    double var_rho{};                   // Sample variance of {ρᵢ}

    // --- QM descriptor bundle (Level-0 analytic) ---
    qm::QMDescriptor qm{};

    // --- Provenance ---
    std::vector<uint32_t> member_indices;   // L2 bead indices in this domain
    uint32_t n_members{};                   // = member_indices.size()

    // --- Quality flags ---
    bool all_l2_converged{};            // All member L2 beads reached steady state
    bool valid{};                       // N_members ≥ L3_MIN_MEMBERS + all converged
};

} // namespace level3
} // namespace coarse_grain
