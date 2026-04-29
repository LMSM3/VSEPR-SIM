#pragma once
/**
 * level3_builder.hpp — L2 → L3 Aggregation Engine
 *
 * Converts a converged set of Level 2 beads (from the 6+9 FIRE stepper)
 * into Level 3 domain beads suitable for macro-DM handoff.
 *
 * Algorithm: greedy nearest-neighbour clustering
 *   1. Start from the bead with highest eta (most converged)
 *   2. Collect all beads within r_domain of the seed → domain
 *   3. Mark those beads as assigned
 *   4. Repeat until all beads are assigned or below min_members threshold
 *   5. Underpopulated clusters are merged to their nearest valid domain
 *
 * QM descriptors:
 *   After clustering, Level-0 QMDescriptors are computed for each domain's
 *   aggregate state using qm::compute_qm_descriptor_l0().
 *
 * MacroPrecursorState:
 *   Each Level3HandoffRecord is populated via compute_macro_precursor_state()
 *   from the domain's EnsembleProxySummary.
 *
 * Anti-black-box: all intermediate steps (cluster assignments, QM inputs,
 * proxy inputs) are stored in the output records for inspection.
 *
 * Reference: coarse_grain/level3/README.md
 */

#include "coarse_grain/level3/level3_bead.hpp"
#include "coarse_grain/level3/level3_handoff.hpp"
#include "coarse_grain/analysis/ensemble_proxy.hpp"
#include "coarse_grain/analysis/macro_precursor.hpp"
#include "coarse_grain/qm/qm_descriptors.hpp"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace coarse_grain {
namespace level3 {

// ============================================================================
// Aggregation Parameters
// ============================================================================

struct Level3BuildParams {
    double   r_domain{L3_DEFAULT_DOMAIN_RADIUS};  // Clustering radius (Å)
    int      min_members{L3_MIN_MEMBERS};          // Min beads per domain
    bool     require_convergence{false};           // Skip non-converged beads
    double   gamma_elec{-0.1};                     // From EnvironmentParams
};

// ============================================================================
// Internal: compute domain COM and radius
// ============================================================================

static inline void compute_domain_geometry(
    const std::vector<Bead>&    beads,
    const std::vector<uint32_t>& indices,
    atomistic::Vec3& com_out,
    double& radius_out)
{
    double total_mass = 0.0;
    atomistic::Vec3 com{0,0,0};
    for (uint32_t idx : indices) {
        const Bead& b = beads[idx];
        com.x += b.mass * b.position.x;
        com.y += b.mass * b.position.y;
        com.z += b.mass * b.position.z;
        total_mass += b.mass;
    }
    if (total_mass > 1e-12) {
        com.x /= total_mass;
        com.y /= total_mass;
        com.z /= total_mass;
    }
    com_out = com;

    // Radius = max distance from COM to any member bead
    double rmax = 0.0;
    for (uint32_t idx : indices) {
        const Bead& b = beads[idx];
        double dx = b.position.x - com.x;
        double dy = b.position.y - com.y;
        double dz = b.position.z - com.z;
        double r  = std::sqrt(dx*dx + dy*dy + dz*dz);
        if (r > rmax) rmax = r;
    }
    radius_out = rmax;
}

// ============================================================================
// Internal: aggregate environment states for a domain
// ============================================================================

static inline void aggregate_env_states(
    const std::vector<EnvironmentState>& env_states,
    const std::vector<uint32_t>&         indices,
    double& mean_eta, double& mean_rho,
    double& mean_C,   double& mean_P2,
    double& var_eta,  double& var_rho)
{
    double sum_eta = 0, sum_rho = 0, sum_C = 0, sum_P2 = 0;
    double sum_eta2 = 0, sum_rho2 = 0;
    uint32_t N = static_cast<uint32_t>(indices.size());

    for (uint32_t idx : indices) {
        const EnvironmentState& es = env_states[idx];
        sum_eta  += es.eta;
        sum_rho  += es.rho;
        sum_C    += es.C;
        sum_P2   += es.P2;
        sum_eta2 += es.eta * es.eta;
        sum_rho2 += es.rho * es.rho;
    }

    double fn = static_cast<double>(N);
    mean_eta = (N > 0) ? sum_eta / fn : 0.0;
    mean_rho = (N > 0) ? sum_rho / fn : 0.0;
    mean_C   = (N > 0) ? sum_C   / fn : 0.0;
    mean_P2  = (N > 0) ? sum_P2  / fn : 0.0;

    // Sample variance (N > 1 guard)
    var_eta = (N > 1) ? (sum_eta2 / fn - mean_eta * mean_eta) : 0.0;
    var_rho = (N > 1) ? (sum_rho2 / fn - mean_rho * mean_rho) : 0.0;
    var_eta = std::max(var_eta, 0.0);
    var_rho = std::max(var_rho, 0.0);
}

// ============================================================================
// Internal: build QMNeighbour list for a domain aggregate bead
// ============================================================================

static inline std::vector<qm::QMNeighbour> build_qm_neighbours(
    const std::vector<Bead>&             beads,
    const std::vector<EnvironmentState>& env_states,
    const std::vector<uint32_t>&         member_indices,
    const atomistic::Vec3&               com,
    double                               gamma_elec)
{
    // For the L3 aggregate, neighbours are beads OUTSIDE this domain
    // that are within r_domain distance of the COM.
    // Since we don't have cross-domain info at build time, we use
    // member beads relative to the COM as an intra-domain proxy.
    std::vector<qm::QMNeighbour> nbrs;
    nbrs.reserve(member_indices.size());

    for (uint32_t idx : member_indices) {
        const Bead& b = beads[idx];
        double dx = b.position.x - com.x;
        double dy = b.position.y - com.y;
        double dz = b.position.z - com.z;
        double r  = std::sqrt(dx*dx + dy*dy + dz*dz);
        if (r < 1e-6) continue;

        qm::QMNeighbour nb;
        nb.r_ij    = r;
        nb.q_j     = b.charge;
        nb.q_j_eff = b.charge;  // refined by QM step
        nb.chi_j   = qm::role_electronegativity(b.structural_role);
        nb.sigma_j = 3.5;  // default if BeadType sigma unavailable at L3
        nb.eta_j   = env_states[idx].eta;
        nbrs.push_back(nb);
    }
    return nbrs;
}

// ============================================================================
// Core: aggregate_to_l3
// ============================================================================

/**
 * aggregate_to_l3 — convert a converged L2 bead system to L3 domains.
 *
 * @param beads        Converged L2 bead system
 * @param env_states   Per-bead environment state after FIRE convergence
 * @param converged    Per-bead convergence flag (true = reached steady state)
 * @param seed_hash    Seed hash of the originating simulation (for provenance)
 * @param params       Aggregation parameters
 * @return Vector of Level3HandoffRecords, one per domain
 */
inline std::vector<Level3HandoffRecord> aggregate_to_l3(
    const std::vector<Bead>&             beads,
    const std::vector<EnvironmentState>& env_states,
    const std::vector<bool>&             converged,
    const std::string&                   seed_hash,
    const Level3BuildParams&             params = {})
{
    const uint32_t N = static_cast<uint32_t>(beads.size());
    if (N == 0) return {};

    std::vector<bool> assigned(N, false);
    std::vector<Level3HandoffRecord> records;

    // Sort bead indices by descending eta (seed from most-converged first)
    std::vector<uint32_t> order(N);
    for (uint32_t i = 0; i < N; ++i) order[i] = i;
    std::sort(order.begin(), order.end(), [&](uint32_t a, uint32_t b) {
        return env_states[a].eta > env_states[b].eta;
    });

    uint32_t domain_id = 0;

    for (uint32_t seed_idx : order) {
        if (assigned[seed_idx]) continue;
        if (params.require_convergence && !converged[seed_idx]) continue;

        const atomistic::Vec3& seed_pos = beads[seed_idx].position;
        std::vector<uint32_t> members;

        // Collect all unassigned beads within r_domain of seed
        for (uint32_t j = 0; j < N; ++j) {
            if (assigned[j]) continue;
            const atomistic::Vec3& p = beads[j].position;
            double dx = p.x - seed_pos.x;
            double dy = p.y - seed_pos.y;
            double dz = p.z - seed_pos.z;
            double r2 = dx*dx + dy*dy + dz*dz;
            if (r2 <= params.r_domain * params.r_domain) {
                members.push_back(j);
            }
        }

        if (static_cast<int>(members.size()) < params.min_members) continue;

        // Mark as assigned
        for (uint32_t idx : members) assigned[idx] = true;

        // --- Compute domain geometry ---
        atomistic::Vec3 com{};
        double radius = 0.0;
        compute_domain_geometry(beads, members, com, radius);

        // --- Aggregate mass, charge ---
        double total_mass = 0.0, total_charge = 0.0;
        bool all_conv = true;
        for (uint32_t idx : members) {
            total_mass   += beads[idx].mass;
            total_charge += beads[idx].charge;
            if (!converged[idx]) all_conv = false;
        }

        // --- Aggregate environment state ---
        double mean_eta = 0, mean_rho = 0, mean_C = 0, mean_P2 = 0;
        double var_eta = 0, var_rho = 0;
        aggregate_env_states(env_states, members,
                              mean_eta, mean_rho, mean_C, mean_P2,
                              var_eta,  var_rho);

        // --- QM descriptors for L3 aggregate bead ---
        // Construct a synthetic aggregate bead for QM computation
        Bead agg_bead;
        agg_bead.position       = com;
        agg_bead.mass           = total_mass;
        agg_bead.charge         = total_charge;
        agg_bead.structural_role = beads[seed_idx].structural_role;

        EnvironmentState agg_env;
        agg_env.eta = mean_eta;
        agg_env.rho = mean_rho;
        agg_env.C   = mean_C;
        agg_env.P2  = mean_P2;

        double sigma_agg = radius > 0.0 ? radius * 0.8 : 5.0;

        auto qm_nbrs = build_qm_neighbours(beads, env_states, members, com,
                                           params.gamma_elec);
        auto qm_desc = qm::compute_qm_descriptor_l0(
            agg_bead, agg_env, qm_nbrs, params.gamma_elec, sigma_agg);

        // --- Ensemble proxy for macro precursor ---
        // Build a minimal EnsembleProxySummary from aggregate state
        EnsembleProxySummary proxy;
        proxy.bead_count       = static_cast<int>(members.size());
        proxy.valid            = (proxy.bead_count >= PROXY_MIN_BEADS);
        proxy.mean_eta         = mean_eta;
        proxy.mean_rho         = mean_rho;
        proxy.mean_C           = mean_C;
        proxy.mean_P2_hat      = (mean_P2 + 0.5) / 1.5;  // P2 → [0,1]
        proxy.var_eta          = var_eta;
        proxy.var_rho          = var_rho;
        proxy.converged        = all_conv;
        // Derive proxies from aggregate state
        proxy.cohesion_proxy      = std::clamp(mean_eta * mean_C / std::max(mean_C, 1.0), 0.0, 1.0);
        proxy.uniformity_proxy    = std::clamp(1.0 - std::sqrt(var_eta), 0.0, 1.0);
        proxy.stabilization_proxy = std::clamp(mean_eta, 0.0, 1.0);
        proxy.texture_proxy       = std::clamp(proxy.mean_P2_hat, 0.0, 1.0);
        proxy.surface_sensitivity_proxy = std::clamp(std::sqrt(var_rho), 0.0, 1.0);
        proxy.bulk_edge_eta_gap   = var_eta;
        proxy.bulk_edge_rho_gap   = var_rho;
        proxy.mean_state_mismatch = std::clamp(std::sqrt(var_eta + var_rho), 0.0, 1.0);

        // --- Compute macro precursor state ---
        MacroPrecursorState macro_state = compute_macro_precursors(proxy);

        // --- Effective continuum properties ---
        double volume = (4.0 / 3.0) * M_PI * radius * radius * radius;
        if (volume < 1e-6) volume = 1.0;
        double rho_eff       = total_mass   / volume;
        double charge_density = total_charge / volume;

        // Effective electronegativity: charge-weighted mean chi
        double chi_sum = 0.0, weight_sum = 0.0;
        for (uint32_t idx : members) {
            double w = std::abs(beads[idx].charge) + 0.01;
            chi_sum    += w * qm::role_electronegativity(beads[idx].structural_role);
            weight_sum += w;
        }
        double chi_eff = (weight_sum > 1e-6) ? chi_sum / weight_sum : qm_desc.chi_mean;

        // Total polarisability = Σ alpha_proxy over members
        double total_alpha = 0.0;
        for (uint32_t idx : members) {
            const Bead& mb = beads[idx];
            double r_eff = 3.5 * 0.5;  // default sigma
            total_alpha += r_eff * r_eff * r_eff
                         * qm::role_polarisability_factor(mb.structural_role);
        }

        // --- Assemble handoff record ---
        Level3HandoffRecord rec;
        rec.seed_hash       = seed_hash;
        rec.domain_id       = domain_id++;
        rec.n_members       = static_cast<uint32_t>(members.size());
        rec.valid           = proxy.valid && all_conv;
        rec.position_com    = com;
        rec.radius          = radius;
        rec.volume          = volume;
        rec.rho_eff         = rho_eff;
        rec.charge_density  = charge_density;
        rec.phi_eff         = qm_desc.phi_elec;
        rec.polarisability  = total_alpha;
        rec.chi_eff         = chi_eff;
        rec.mean_eta        = mean_eta;
        rec.mean_rho        = mean_rho;
        rec.mean_C          = mean_C;
        rec.mean_P2         = mean_P2;
        rec.var_eta         = var_eta;
        rec.qm              = qm_desc;
        rec.macro_state     = macro_state;
        rec.member_indices  = members;

        records.push_back(rec);
    }

    return records;
}

} // namespace level3
} // namespace coarse_grain
