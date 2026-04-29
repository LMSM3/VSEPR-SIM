#pragma once
/**
 * qm_descriptors.hpp — QM Descriptor Preparation Layer (Level 0)
 *
 * Provides per-bead quantum-mechanical descriptor estimates computed
 * analytically from the coarse-grained state and species tables.
 *
 * This is Level-0 fidelity: no external QM engine is called.
 * All quantities are derived from:
 *   - Bead charge, mass, and electronegativity (from species table)
 *   - Local CG environment state (rho, C, P2, eta)
 *   - Pairwise neighbour geometry
 *
 * Interface contract:
 *   Future Level-1 (semi-empirical) and Level-2 (DFT) backends must
 *   produce the same QMDescriptor struct. This file defines the contract.
 *
 * Anti-black-box: every formula is explicit and traceable. No hidden
 * parameters. All Level-0 approximations are documented inline.
 *
 * Reference: coarse_grain/qm/README.md
 *            copilot-instructions.md §5 (anti-black-box)
 */

#include "coarse_grain/core/bead.hpp"
#include "coarse_grain/core/environment_state.hpp"
#include "atomistic/core/state.hpp"
#include <cmath>
#include <vector>

namespace coarse_grain {
namespace qm {

// ============================================================================
// QM Descriptor Struct — the Level-0/1/2 interface contract
// ============================================================================

/**
 * QMDescriptor — per-bead quantum-mechanical evidence variables.
 *
 * Level-0 sources (all analytic/empirical):
 *
 *   phi_elec:     Electrostatic potential at bead centre (kcal/mol/e)
 *                 φᵢ = Σⱼ≠ᵢ  k_e · q_j · g_elec / r_ij
 *                 where g_elec = 1 + gamma_elec · eta_bar (from env coupling).
 *
 *   chi_mean:     Mean electronegativity of parent atoms (Pauling scale).
 *                 χ̄ᵢ = (1/N_atoms) Σ χ_atom
 *                 Fallback: structural_role → tabulated χ̄ estimate.
 *
 *   alpha_proxy:  Polarisability proxy (Å³, approximate).
 *                 αᵢ ≈ (σ_bead / 2)³ · f_role
 *                 where f_role ∈ {0.1 Inert, 1.0 IonicDominant,
 *                                 1.5 DirectionalCovalent,
 *                                 2.0 Metallic, 1.2 Mixed}
 *
 *   homo_proxy:   HOMO energy proxy (eV, negative).
 *                 Estimated as −(ionisation_potential_mean_atom)
 *                 Fallback: −chi_mean × 0.54 eV (Mulliken approx).
 *
 *   lumo_proxy:   LUMO energy proxy (eV).
 *                 Estimated as homo_proxy + chemical_hardness_proxy.
 *                 Fallback: chi_mean × 0.54 − 2.0·alpha_proxy^(1/3).
 *
 *   q_eff:        Effective charge (e) after level-0 charge equilibration.
 *                 q_eff = charge + delta_q_env
 *                 where delta_q_env = −gamma_elec · eta · (charge / max(|charge|, 1))
 *
 *   omega_overlap: Orbital overlap proxy (dimensionless [0,1]).
 *                 Ω = (1/N_nbr) Σⱼ exp(−r_ij / r_overlap) · (χᵢ·χⱼ)/(χᵢ+χⱼ)
 *                 r_overlap = (sigma_i + sigma_j) / 2
 *
 * Fidelity level of this record:
 *   0 = analytic/empirical (this file)
 *   1 = semi-empirical tight-binding (future)
 *   2 = DFT single-point (future)
 */
struct QMDescriptor {
    double phi_elec{};         // Electrostatic potential (kcal/mol/e)
    double chi_mean{};         // Mean electronegativity (Pauling)
    double alpha_proxy{};      // Polarisability proxy (Å³)
    double homo_proxy{};       // HOMO energy proxy (eV)
    double lumo_proxy{};       // LUMO energy proxy (eV)
    double q_eff{};            // Effective charge after equilibration (e)
    double omega_overlap{};    // Orbital overlap proxy [0, 1]
    double chemical_gap{};     // LUMO − HOMO proxy (eV, chemical hardness × 2)
    int    fidelity_level{0};  // 0=analytic, 1=semi-empirical, 2=DFT
    bool   valid{false};       // True when computation succeeded
};

// ============================================================================
// Neighbour info (lightweight, for QM computation only)
// ============================================================================

struct QMNeighbour {
    double r_ij{};          // Distance (Å)
    double q_j{};           // Charge of neighbour (e)
    double q_j_eff{};       // Effective charge of neighbour
    double chi_j{};         // Electronegativity of neighbour
    double sigma_j{};       // LJ sigma of neighbour (Å)
    double eta_j{};         // Slow state of neighbour
};

// ============================================================================
// Structural role → electronegativity fallback
// ============================================================================

/**
 * Tabulated mean electronegativity by StructuralRole (Pauling scale).
 * Used when parent atom data is unavailable.
 *
 *   Inert:              0.5  (noble-gas-like, very low EN)
 *   IonicDominant:      2.5  (Na/Cl-like average)
 *   DirectionalCovalent:2.55 (organic average, C-like)
 *   Metallic:           1.9  (transition metal average)
 *   Mixed:              2.2  (interpolated)
 */
inline double role_electronegativity(StructuralRole role)
{
    switch (role) {
        case StructuralRole::Inert:               return 0.5;
        case StructuralRole::IonicDominant:       return 2.5;
        case StructuralRole::DirectionalCovalent: return 2.55;
        case StructuralRole::Metallic:            return 1.9;
        case StructuralRole::Mixed:
        default:                                   return 2.2;
    }
}

/**
 * Polarisability role factor.
 * Scales the geometric estimate αᵢ ≈ (σ/2)³ · f_role.
 */
inline double role_polarisability_factor(StructuralRole role)
{
    switch (role) {
        case StructuralRole::Inert:               return 0.1;
        case StructuralRole::IonicDominant:       return 1.0;
        case StructuralRole::DirectionalCovalent: return 1.5;
        case StructuralRole::Metallic:            return 2.0;
        case StructuralRole::Mixed:
        default:                                   return 1.2;
    }
}

// ============================================================================
// Level-0 QM Descriptor Computation
// ============================================================================

/**
 * Compute a Level-0 QMDescriptor for bead i.
 *
 * @param bead_i      The bead being described
 * @param env_i       Environment state of bead i
 * @param neighbours  Neighbour list for bead i
 * @param gamma_elec  Electrostatic modulation parameter (from EnvironmentParams)
 * @param sigma_i     LJ sigma of bead i (Å) — used for polarisability
 * @return Populated QMDescriptor (fidelity_level = 0)
 */
inline QMDescriptor compute_qm_descriptor_l0(
    const Bead&             bead_i,
    const EnvironmentState& env_i,
    const std::vector<QMNeighbour>& neighbours,
    double gamma_elec,
    double sigma_i)
{
    QMDescriptor d;
    d.fidelity_level = 0;

    // ---------------------------------------------------------------
    // 1. Electronegativity
    // ---------------------------------------------------------------
    d.chi_mean = role_electronegativity(bead_i.structural_role);

    // ---------------------------------------------------------------
    // 2. Electrostatic potential φᵢ
    //    φᵢ = Σⱼ k_e·q_j·g_elec(ηᵢ, ηⱼ) / r_ij
    //    k_e in kcal·Å/(mol·e²) = 332.06
    //    g_elec = 1 + gamma_elec · 0.5·(eta_i + eta_j)
    // ---------------------------------------------------------------
    static constexpr double k_e = 332.06;  // kcal·Å / (mol·e²)
    double phi = 0.0;
    for (const auto& nb : neighbours) {
        if (nb.r_ij < 1e-6) continue;
        double eta_bar = 0.5 * (env_i.eta + nb.eta_j);
        double g_elec  = 1.0 + gamma_elec * eta_bar;
        if (g_elec <= 0.0) g_elec = 1e-10;
        phi += k_e * nb.q_j * g_elec / nb.r_ij;
    }
    d.phi_elec = phi;

    // ---------------------------------------------------------------
    // 3. Polarisability proxy αᵢ ≈ (σ/2)³ · f_role
    // ---------------------------------------------------------------
    double r_eff = sigma_i * 0.5;
    d.alpha_proxy = r_eff * r_eff * r_eff
                  * role_polarisability_factor(bead_i.structural_role);

    // ---------------------------------------------------------------
    // 4. HOMO proxy (eV)
    //    Mulliken approx: IE ≈ chi_mean × 0.54 eV (ionisation energy)
    //    HOMO ≈ −IE
    // ---------------------------------------------------------------
    static constexpr double mulliken_scale = 0.54;  // eV / Pauling
    d.homo_proxy = -(d.chi_mean * mulliken_scale);

    // ---------------------------------------------------------------
    // 5. Chemical hardness proxy η_chem ≈ alpha_proxy^(−1/3) × 0.5
    //    Hard species: small α, large gap
    //    Soft species: large α, small gap
    // ---------------------------------------------------------------
    double hardness_proxy = 0.0;
    if (d.alpha_proxy > 1e-6) {
        hardness_proxy = 0.5 / std::cbrt(d.alpha_proxy);
    }
    d.lumo_proxy   = d.homo_proxy + 2.0 * hardness_proxy;
    d.chemical_gap = d.lumo_proxy - d.homo_proxy;  // = 2·hardness_proxy (eV)

    // ---------------------------------------------------------------
    // 6. Effective charge q_eff (Level-0 charge equilibration)
    //    Delta from environment: hard species in high-density environments
    //    donate partial charge to neighbours.
    //    delta_q = −gamma_elec · eta_i · sign(charge)
    //    (gamma_elec < 0 → delta_q > 0 in dense environments for cations)
    // ---------------------------------------------------------------
    double sign_q = (bead_i.charge > 0.0) ? 1.0
                  : (bead_i.charge < 0.0) ? -1.0 : 0.0;
    double delta_q = -gamma_elec * env_i.eta * sign_q;
    d.q_eff = bead_i.charge + delta_q;

    // ---------------------------------------------------------------
    // 7. Orbital overlap proxy Ω
    //    Ω = (1/N) Σⱼ exp(−r_ij / r_overlap) · (χᵢ·χⱼ)/(χᵢ+χⱼ)
    //    r_overlap = (sigma_i + sigma_j) / 2
    // ---------------------------------------------------------------
    double omega = 0.0;
    int    n_nbr = 0;
    for (const auto& nb : neighbours) {
        if (nb.r_ij < 1e-6) continue;
        double r_overlap = (sigma_i + nb.sigma_j) * 0.5;
        if (r_overlap < 1e-6) continue;
        double chi_sum = d.chi_mean + nb.chi_j;
        if (chi_sum < 1e-6) continue;
        double chi_harm = d.chi_mean * nb.chi_j / chi_sum;  // harmonic mean
        omega += std::exp(-nb.r_ij / r_overlap) * chi_harm;
        ++n_nbr;
    }
    d.omega_overlap = (n_nbr > 0) ? std::clamp(omega / n_nbr, 0.0, 1.0) : 0.0;

    d.valid = true;
    return d;
}

// ============================================================================
// QM Interface Contract (for future Level-1/2 backends)
// ============================================================================

/**
 * IQMBackend — abstract interface that Level-1 (semi-empirical) and
 * Level-2 (DFT) backends must implement.
 *
 * Level-0 analytic computation does NOT use this interface.
 * It is provided here so future backends can be drop-in replacements.
 */
class IQMBackend {
public:
    virtual ~IQMBackend() = default;

    /**
     * Compute QM descriptors for an entire bead system.
     *
     * @param beads       Full bead system
     * @param env_states  Per-bead environment state
     * @param env_params  Environment parameters
     * @return Per-bead QMDescriptor vector (same length as beads)
     */
    virtual std::vector<QMDescriptor> compute(
        const std::vector<Bead>&              beads,
        const std::vector<EnvironmentState>&  env_states,
        const EnvironmentParams&              env_params) const = 0;

    virtual int fidelity_level() const = 0;
    virtual const char* backend_name() const = 0;
};

} // namespace qm
} // namespace coarse_grain
