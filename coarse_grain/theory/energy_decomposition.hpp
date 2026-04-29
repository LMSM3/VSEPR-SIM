#pragma once
/**
 * energy_decomposition.hpp — Full Energy Decomposition (Theory §3.1)
 *
 * Implements the formal energy decomposition for the coarse-grained system:
 *
 *   U_tot = U_bond + U_angle + U_tors + U_vdW + U_Coul
 *         + U_pol  + U_decay + U_resp  + U_ext
 *
 * At the CG (Level 2) resolution, direct bond/angle/torsion terms are
 * absorbed into the SH-expanded orientation channels.  The energy terms
 * that survive in the CG representation are:
 *
 *   U_vdW    → LJ 12-6 isotropic + dispersion modulation [B9]
 *   U_Coul   → electrostatic channel + modulation [B8]
 *   U_steric → steric channel + modulation [B7]
 *   U_orient → orientational coupling via SH dot products
 *   U_pol    → polarisation energy from QM L0 α-proxy
 *   U_decay  → η-relaxation dissipation (slow state energy)
 *   U_resp   → environment-responsive coupling energy
 *   U_ext    → external field coupling (placeholder)
 *
 * Pairwise mixed interaction structure (§3.3):
 *
 *   U_ij^{xy} = U_{ij,steric}^{xy} + U_{ij,elec}^{xy}
 *             + U_{ij,disp}^{xy}    + U_{ij,orient}^{xy}
 *             + U_{ij,decay-coupled}^{xy}
 *
 *   where x,y ∈ {a, b} denotes bead species (or type tags).
 *
 * Anti-black-box: every energy term is individually computed, stored,
 * and inspectable.  The decomposition is the canonical diagnostic record
 * for every pairwise interaction in the system.
 *
 * Reference: §3.1 Full Energy Decomposition
 *            §3.3 Pairwise Mixed Interaction Structure
 */

#include "coarse_grain/core/environment_state.hpp"
#include "coarse_grain/core/bead.hpp"
#include "coarse_grain/models/environment_coupling.hpp"
#include "atomistic/core/state.hpp"
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace coarse_grain {
namespace theory {

// ============================================================================
// §3.1  Energy Channel Tags
// ============================================================================

/** Canonical energy channels in U_tot decomposition. */
enum class EnergyChannel : uint8_t {
    VdW        = 0,   ///< Van der Waals (LJ 12-6 + dispersion modulation)
    Coulomb    = 1,   ///< Electrostatic (Coulomb + B8 modulation)
    Steric     = 2,   ///< Steric repulsion (B7 modulation)
    Orient     = 3,   ///< Orientational coupling (SH overlap, P₂-dependent)
    Pol        = 4,   ///< Polarisation (α-proxy × local E-field)
    Decay      = 5,   ///< η-relaxation dissipation
    Resp       = 6,   ///< Environment-responsive coupling energy
    External   = 7,   ///< External field (future placeholder)
    N_CHANNELS = 8
};

inline const char* channel_name(EnergyChannel ch) {
    switch (ch) {
        case EnergyChannel::VdW:      return "U_vdW";
        case EnergyChannel::Coulomb:  return "U_Coul";
        case EnergyChannel::Steric:   return "U_steric";
        case EnergyChannel::Orient:   return "U_orient";
        case EnergyChannel::Pol:      return "U_pol";
        case EnergyChannel::Decay:    return "U_decay";
        case EnergyChannel::Resp:     return "U_resp";
        case EnergyChannel::External: return "U_ext";
        default:                      return "U_???";
    }
}

// ============================================================================
// §3.1  Per-Channel Energy Record
// ============================================================================

/**
 * ChannelEnergy — a single resolved energy contribution.
 *
 * Carries the value in kcal/mol and the channel tag for provenance.
 */
struct ChannelEnergy {
    EnergyChannel channel{EnergyChannel::VdW};
    double        value{};    ///< Energy in kcal/mol
    bool          active{};   ///< True if this channel contributed this step
};

// ============================================================================
// §3.1  System Energy Decomposition
// ============================================================================

/**
 * EnergyDecomposition — full decomposition of U_tot into §3.1 channels.
 *
 * U_tot = Σ_k  channels[k].value
 *
 * Each channel is individually inspectable.
 */
struct EnergyDecomposition {
    ChannelEnergy channels[static_cast<int>(EnergyChannel::N_CHANNELS)]{};

    double total() const {
        double s = 0.0;
        for (int k = 0; k < static_cast<int>(EnergyChannel::N_CHANNELS); ++k)
            if (channels[k].active) s += channels[k].value;
        return s;
    }

    ChannelEnergy& operator[](EnergyChannel ch) {
        return channels[static_cast<int>(ch)];
    }
    const ChannelEnergy& operator[](EnergyChannel ch) const {
        return channels[static_cast<int>(ch)];
    }

    /** Set a channel value and mark it active. */
    void set(EnergyChannel ch, double val) {
        auto& c = channels[static_cast<int>(ch)];
        c.channel = ch;
        c.value   = val;
        c.active  = true;
    }
};

// ============================================================================
// §3.3  Pairwise Mixed Interaction Record
// ============================================================================

/**
 * PairInteraction — decomposed pairwise energy U_ij^{xy}
 *
 *   U_ij^{xy} = U_{ij,steric} + U_{ij,elec} + U_{ij,disp}
 *             + U_{ij,orient}  + U_{ij,decay-coupled}
 *
 * x,y ∈ {a, b} denote species tags for mixed pairs.
 */
struct PairInteraction {
    uint32_t i{};           ///< Bead index i
    uint32_t j{};           ///< Bead index j
    char     type_i{'a'};   ///< Species tag of bead i
    char     type_j{'a'};   ///< Species tag of bead j

    double r_ij{};          ///< Separation (Å)

    // §3.3 pairwise decomposition
    double U_steric{};       ///< K_s(ℓ,r) · (1 + γ_s · η̄) · SH coupling
    double U_elec{};         ///< K_e(ℓ,r) · (1 + γ_e · η̄) · SH coupling
    double U_disp{};         ///< K_d(ℓ,r) · (1 + γ_d · η̄) · SH coupling
    double U_orient{};       ///< SH overlap integral contribution
    double U_decay_coupled{}; ///< Dissipative coupling: γ_d · Δη · |F_ij|

    double total() const {
        return U_steric + U_elec + U_disp + U_orient + U_decay_coupled;
    }

    // Modulation factors (for diagnostics)
    double g_steric{1.0};
    double g_elec{1.0};
    double g_disp{1.0};
    double eta_bar{};
};

// ============================================================================
// §3.3  Pairwise decomposition from environment state
// ============================================================================

/**
 * Compute pairwise modulation factors from environment state of two beads.
 *
 * g_k(η_A, η_B) = 1 + γ_k · η̄,    η̄ = 0.5·(η_A + η_B)
 *
 * Fills the modulation fields in a PairInteraction record.
 */
inline void fill_pair_modulation(
    PairInteraction& pair,
    double eta_A,
    double eta_B,
    const EnvironmentParams& params)
{
    pair.eta_bar  = 0.5 * (eta_A + eta_B);
    pair.g_steric = 1.0 + params.gamma_steric * pair.eta_bar;
    pair.g_elec   = 1.0 + params.gamma_elec   * pair.eta_bar;
    pair.g_disp   = 1.0 + params.gamma_disp   * pair.eta_bar;

    // Invariant: kernels must not change sign
    if (pair.g_steric <= 0.0) pair.g_steric = 1e-10;
    if (pair.g_elec   <= 0.0) pair.g_elec   = 1e-10;
    if (pair.g_disp   <= 0.0) pair.g_disp   = 1e-10;
}

// ============================================================================
// Decay-coupled dissipation (§3.3 last term)
// ============================================================================

/**
 * Compute the decay-coupled energy for a bead pair.
 *
 * U_{ij,decay-coupled} = −γ_d · |Δη_ij| · f_ij(r)
 *
 * where:
 *   Δη_ij = η_i − η_j                  (environment state mismatch)
 *   f_ij(r) = switching_function(r)      (smooth cutoff)
 *   γ_d = gamma_disp from env params     (reuse dispersion coupling weight)
 *
 * Physical interpretation:
 *   Pairs with mismatched η (one ordered, one disordered) experience
 *   a dissipative coupling that drives them toward compatible states.
 *   This is the CG equivalent of interfacial energy at domain boundaries.
 */
inline double decay_coupled_energy(
    double eta_i, double eta_j,
    double r, double r_cutoff, double delta_sw,
    double gamma_disp)
{
    double delta_eta = std::abs(eta_i - eta_j);
    double sw = switching_function(r, r_cutoff, delta_sw);
    return -gamma_disp * delta_eta * sw;
}

// ============================================================================
// Polarisation energy (§3.1 U_pol term)
// ============================================================================

/**
 * Polarisation energy proxy for a bead in a local field.
 *
 * U_pol = −0.5 · α · |E_local|²
 *
 * where α = alpha_proxy (Å³) from QM L0 descriptors,
 *       E_local = Σ_j q_j / r_ij² (simplified local field proxy).
 *
 * Units: kcal/mol (via Coulomb constant).
 */
inline double polarisation_energy(double alpha_proxy_ang3,
                                   double E_local_sq_kcal)
{
    return -0.5 * alpha_proxy_ang3 * E_local_sq_kcal;
}

// ============================================================================
// η-relaxation dissipation (§3.1 U_decay term)
// ============================================================================

/**
 * Dissipation energy stored in the slow state η.
 *
 * U_decay = −k_B T_eff · Σ_i  ln(1 + |f_i − η_i|/η_ref)
 *
 * Simplified to a per-bead penalty proportional to departure from target:
 *
 *   U_decay_i = −τ_inv · (f_i − η_i)²
 *
 * where τ_inv = 1/τ is the inverse relaxation timescale,
 *       f_i = target function, η_i = current slow state.
 *
 * Physical interpretation: energy trapped in the relaxation dynamics
 * that has not yet dissipated to steady state.
 */
inline double decay_energy_per_bead(double f_target, double eta,
                                     double tau_inv)
{
    double delta = f_target - eta;
    return -tau_inv * delta * delta;
}

// ============================================================================
// Environment-responsive coupling energy (§3.1 U_resp term)
// ============================================================================

/**
 * Environment-responsive coupling energy for the system.
 *
 * U_resp = Σ_{ij} Σ_k (g_k − 1) · U_{ij,k}^{base}
 *
 * This is the *excess* energy due to environment modulation — the
 * difference between modulated and unmodulated interaction.
 *
 * Per-pair, per-channel:
 *   ΔU_{ij,k}^{resp} = (g_k(η̄) − 1) · U_{ij,k}^{bare}
 *                     = γ_k · η̄ · U_{ij,k}^{bare}
 */
inline double responsive_excess(double g_k, double U_base) {
    return (g_k - 1.0) * U_base;
}

// ============================================================================
// Convenience: build full decomposition from stepper state
// ============================================================================

/**
 * Build a system-level EnergyDecomposition from accumulated pair data.
 *
 * @param pairs     All pairwise interaction records this step
 * @param env_data  Per-bead environment states (for decay energy)
 * @param tau       Relaxation timescale (fs)
 * @return EnergyDecomposition with all 8 channels populated
 */
inline EnergyDecomposition build_decomposition(
    const std::vector<PairInteraction>& pairs,
    const std::vector<EnvironmentState>& env_data,
    double tau)
{
    EnergyDecomposition ed;

    double sum_vdw    = 0.0;
    double sum_coul   = 0.0;
    double sum_steric = 0.0;
    double sum_orient = 0.0;
    double sum_decay_c = 0.0;
    double sum_resp   = 0.0;

    for (const auto& p : pairs) {
        // Base channel energies
        sum_steric += p.U_steric;
        sum_coul   += p.U_elec;
        sum_vdw    += p.U_disp;
        sum_orient += p.U_orient;
        sum_decay_c += p.U_decay_coupled;

        // Responsive excess (per-channel)
        // The modulated energies already contain (1 + γ·η̄)·U_base,
        // so the responsive excess is γ·η̄·U_base = U_mod − U_base
        // We approximate: U_resp contribution per pair per channel
        if (p.g_steric != 0.0)
            sum_resp += responsive_excess(p.g_steric, p.U_steric / p.g_steric);
        if (p.g_elec != 0.0)
            sum_resp += responsive_excess(p.g_elec, p.U_elec / p.g_elec);
        if (p.g_disp != 0.0)
            sum_resp += responsive_excess(p.g_disp, p.U_disp / p.g_disp);
    }

    // Per-bead η-relaxation dissipation
    double sum_decay = 0.0;
    double tau_inv = (tau > 0.0) ? 1.0 / tau : 0.0;
    for (const auto& e : env_data) {
        sum_decay += decay_energy_per_bead(e.target_f, e.eta, tau_inv);
    }

    ed.set(EnergyChannel::VdW,      sum_vdw);
    ed.set(EnergyChannel::Coulomb,   sum_coul);
    ed.set(EnergyChannel::Steric,    sum_steric);
    ed.set(EnergyChannel::Orient,    sum_orient);
    ed.set(EnergyChannel::Decay,     sum_decay + sum_decay_c);
    ed.set(EnergyChannel::Resp,      sum_resp);
    // Pol and External are computed externally when QM descriptors are available
    ed.channels[static_cast<int>(EnergyChannel::Pol)].channel = EnergyChannel::Pol;
    ed.channels[static_cast<int>(EnergyChannel::External)].channel = EnergyChannel::External;

    return ed;
}

} // namespace theory
} // namespace coarse_grain
