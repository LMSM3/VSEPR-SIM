#pragma once
/**
 * level3_handoff.hpp — Level 3 → Macro-DM Handoff Record
 *
 * The Level3HandoffRecord is the formal payload passed from the
 * Level 3 Bead layer to the macro-dynamics (macro-DM) model.
 *
 * It consolidates:
 *   - Effective continuum properties from the L3 domain
 *   - QM-derived transport and reactivity evidence
 *   - MacroPrecursorState (rigidity*, ductility*, transport*...)
 *   - Seed hash for full traceability back to the originating simulation
 *
 * Design contract:
 *   A macro-DM engine (continuum, FEA, or field-theoretic) must be able
 *   to consume a vector<Level3HandoffRecord> as its input without needing
 *   to know anything about the atomistic or CG layers above.
 *
 * Anti-black-box: every field has a documented source formula and units.
 * No derived quantity is computed silently. All precursor channels carry
 * their validity flags.
 *
 * Reference: coarse_grain/level3/README.md
 *            coarse_grain/analysis/macro_precursor.hpp
 */

#include "coarse_grain/level3/level3_bead.hpp"
#include "coarse_grain/analysis/macro_precursor.hpp"
#include <string>
#include <vector>

namespace coarse_grain {
namespace level3 {

// ============================================================================
// Level3HandoffRecord — macro-DM input payload
// ============================================================================

/**
 * Level3HandoffRecord — complete macro-DM input for one L3 domain.
 *
 * Effective continuum properties (all per-domain):
 *
 *   rho_eff:           Effective mass density (amu/Å³)
 *                      = total_mass / (4/3 · π · radius³)
 *
 *   charge_density:    Effective charge density (e/Å³)
 *                      = total_charge / domain_volume
 *
 *   phi_eff:           Domain-average electrostatic potential (kcal/mol/e)
 *                      = qm.phi_elec of the L3 aggregate QMDescriptor
 *
 *   polarisability:    Domain-aggregate polarisability (Å³)
 *                      = Σᵢ αᵢ for member beads
 *
 *   chi_eff:           Effective electronegativity (Pauling)
 *                      = charge-weighted mean of qm.chi_mean over members
 *
 *   order_param:       Domain structural order parameter [0, 1]
 *                      = mean_eta (convergence measure)
 *
 *   rigidity_proxy:    MacroPrecursorState::rigidity_like.value
 *   ductility_proxy:   MacroPrecursorState::ductility_like.value
 *   transport_proxy:   MacroPrecursorState::thermal_transport_like.value
 *   reactivity_proxy:  MacroPrecursorState::surface_reactivity_like.value
 */
struct Level3HandoffRecord {
    // --- Traceability ---
    std::string seed_hash;          // From originating simulation
    uint32_t    domain_id{};
    uint32_t    n_members{};
    bool        valid{};

    // --- Geometry ---
    atomistic::Vec3 position_com{};
    double          radius{};           // Å
    double          volume{};           // Å³

    // --- Effective continuum properties ---
    double rho_eff{};               // amu/Å³
    double charge_density{};        // e/Å³
    double phi_eff{};               // kcal/mol/e
    double polarisability{};        // Å³
    double chi_eff{};               // Pauling

    // --- Structural order ---
    double mean_eta{};              // [0, 1]
    double mean_rho{};
    double mean_C{};
    double mean_P2{};
    double var_eta{};               // Structural disorder signal

    // --- QM bundle ---
    qm::QMDescriptor qm{};

    // --- Macro precursor channels (from macro_precursor.hpp) ---
    MacroPrecursorState macro_state{};

    // --- Provenance ---
    std::vector<uint32_t> member_indices;
};

} // namespace level3
} // namespace coarse_grain
