#pragma once
/**
 * material_kernel.hpp — Material Kernel Vector M_k
 *
 * Implements the material kernel formalism:
 *
 *           ┌ φ_k ┐
 *           │ ψ_k │
 *   M_k =   │ χ_k │
 *           │ ω_k │
 *           └ E_k ┘
 *
 * where each component encodes a distinct physical channel:
 *
 *   φ_k  — electrostatic potential proxy (kcal/mol/e)
 *          Source: QMDescriptor::phi_elec or Σ_j q_j·g_e / r_ij
 *
 *   ψ_k  — orientational order / structural coherence [0, 1]
 *          Source: EnvironmentState::P2 (Legendre P₂)
 *          or mean η (slow state convergence signal)
 *
 *   χ_k  — electronegativity / chemical identity (Pauling)
 *          Source: QMDescriptor::chi_mean or MetalRecord::electronegativity_pauling
 *
 *   ω_k  — orbital overlap / bonding character [0, 1]
 *          Source: QMDescriptor::omega_overlap
 *
 *   E_k  — local energy density (kcal/mol per bead)
 *          Source: EnergyDecomposition, summed over pair contributions to bead k
 *
 * The material kernel vector is the 5-channel representation of each
 * bead (or domain) that flows through the analysis pipeline:
 *
 *   Atomistic → CG L2 bead → M_k kernel → L3 domain aggregate → Macro-DM
 *
 * Properties of M_k:
 *   - Deterministic: computed entirely from CG state + registry data
 *   - Low-dimensional: 5 real-valued components
 *   - Basis for comparison: ||M_k - M_ref|| measures material distance
 *   - Basis for classification: M_k clusters → material families
 *   - Invariant to rotation: all components are scalars
 *
 * Anti-black-box: every component formula is explicit and traceable.
 *
 * Reference: Material kernel specification (M_k formalism)
 *            §3.1 Energy Decomposition, §3.3 Pairwise Interaction
 */

#include "coarse_grain/core/environment_state.hpp"
#include "coarse_grain/qm/qm_descriptors.hpp"
#include "atomistic/core/state.hpp"
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace coarse_grain {
namespace theory {

// ============================================================================
// Material Kernel Vector  M_k = [φ, ψ, χ, ω, E]^T
// ============================================================================

/**
 * MaterialKernel — the 5-component per-bead (or per-domain) material
 * state vector.
 *
 * All components are real-valued scalars.  Units are documented per field.
 */
struct MaterialKernel {
    double phi{};       ///< φ_k: electrostatic potential proxy (kcal/mol/e)
    double psi{};       ///< ψ_k: structural coherence / order [0, 1]
    double chi{};       ///< χ_k: electronegativity (Pauling scale)
    double omega{};     ///< ω_k: orbital overlap proxy [0, 1]
    double E{};         ///< E_k: local energy density (kcal/mol)

    /** L2 norm of the kernel vector. */
    double norm() const {
        return std::sqrt(phi * phi + psi * psi + chi * chi
                       + omega * omega + E * E);
    }

    /** Dot product M_k · M_j. */
    double dot(const MaterialKernel& other) const {
        return phi * other.phi + psi * other.psi + chi * other.chi
             + omega * other.omega + E * other.E;
    }

    /** Euclidean distance ||M_k − M_j||₂. */
    double distance(const MaterialKernel& other) const {
        double dphi = phi - other.phi;
        double dpsi = psi - other.psi;
        double dchi = chi - other.chi;
        double domega = omega - other.omega;
        double dE   = E - other.E;
        return std::sqrt(dphi * dphi + dpsi * dpsi + dchi * dchi
                       + domega * domega + dE * dE);
    }

    /** Cosine similarity cos(θ) = (M_k · M_j) / (|M_k| · |M_j|). */
    double cosine_similarity(const MaterialKernel& other) const {
        double n1 = norm();
        double n2 = other.norm();
        if (n1 < 1e-15 || n2 < 1e-15) return 0.0;
        return dot(other) / (n1 * n2);
    }

    /** Component-wise addition. */
    MaterialKernel operator+(const MaterialKernel& o) const {
        return { phi + o.phi, psi + o.psi, chi + o.chi,
                 omega + o.omega, E + o.E };
    }

    /** Scalar multiplication. */
    MaterialKernel operator*(double s) const {
        return { phi * s, psi * s, chi * s, omega * s, E * s };
    }

    uint32_t bead_index{};   ///< Source bead (or domain) index
    bool valid{false};       ///< True when all components are populated
};

// ============================================================================
// Construction from CG state
// ============================================================================

/**
 * Build M_k for a single bead from its environment state and QM descriptor.
 *
 * @param idx       Bead index k
 * @param env       EnvironmentState for bead k
 * @param qm        QMDescriptor for bead k (Level-0 analytic)
 * @param E_local   Local energy density (kcal/mol), from energy decomposition
 * @return MaterialKernel M_k = [φ, ψ, χ, ω, E]^T
 */
inline MaterialKernel build_kernel(
    uint32_t idx,
    const EnvironmentState& env,
    const qm::QMDescriptor& qm,
    double E_local)
{
    MaterialKernel mk;
    mk.bead_index = idx;
    mk.phi   = qm.phi_elec;        // Electrostatic potential
    mk.psi   = env.eta;             // Structural coherence (slow state)
    mk.chi   = qm.chi_mean;        // Electronegativity
    mk.omega = qm.omega_overlap;   // Orbital overlap
    mk.E     = E_local;            // Local energy density
    mk.valid = qm.valid;
    return mk;
}

/**
 * Build M_k with metal registry fallback (no QM descriptor required).
 *
 * Uses metal registry data when QM descriptors are not yet available.
 *
 * @param idx       Bead index
 * @param env       EnvironmentState for bead
 * @param chi_pauling  Electronegativity from registry
 * @param phi_est   Estimated electrostatic potential
 * @param E_local   Local energy density
 * @return MaterialKernel M_k
 */
inline MaterialKernel build_kernel_from_metal(
    uint32_t idx,
    const EnvironmentState& env,
    double chi_pauling,
    double phi_est,
    double E_local)
{
    MaterialKernel mk;
    mk.bead_index = idx;
    mk.phi   = phi_est;
    mk.psi   = env.eta;
    mk.chi   = chi_pauling;
    mk.omega = 0.0;        // Not available without QM; placeholder
    mk.E     = E_local;
    mk.valid = true;
    return mk;
}

// ============================================================================
// Domain-level aggregate kernel
// ============================================================================

/**
 * Aggregate a set of per-bead kernels into a domain-level M_domain.
 *
 * M_domain = (1/N) Σ_k M_k
 *
 * This is the kernel vector for a Level 3 domain.
 */
inline MaterialKernel aggregate_kernels(
    const std::vector<MaterialKernel>& kernels)
{
    MaterialKernel agg;
    if (kernels.empty()) return agg;

    double n = 0.0;
    for (const auto& mk : kernels) {
        if (!mk.valid) continue;
        agg.phi   += mk.phi;
        agg.psi   += mk.psi;
        agg.chi   += mk.chi;
        agg.omega += mk.omega;
        agg.E     += mk.E;
        n += 1.0;
    }

    if (n > 0.0) {
        agg.phi   /= n;
        agg.psi   /= n;
        agg.chi   /= n;
        agg.omega /= n;
        agg.E     /= n;
        agg.valid = true;
    }

    return agg;
}

// ============================================================================
// Material Distance / Comparison
// ============================================================================

/**
 * MaterialDistance — comparison between two material kernels.
 */
struct MaterialDistance {
    double euclidean{};         ///< ||M_a − M_b||₂
    double cosine_sim{};        ///< cos(θ) ∈ [−1, 1]
    double phi_diff{};          ///< |φ_a − φ_b|
    double psi_diff{};          ///< |ψ_a − ψ_b|
    double chi_diff{};          ///< |χ_a − χ_b|
    double omega_diff{};        ///< |ω_a − ω_b|
    double E_diff{};            ///< |E_a − E_b|
};

/**
 * Compute material distance between two kernels.
 */
inline MaterialDistance compare_kernels(
    const MaterialKernel& a,
    const MaterialKernel& b)
{
    MaterialDistance d;
    d.euclidean = a.distance(b);
    d.cosine_sim = a.cosine_similarity(b);
    d.phi_diff   = std::abs(a.phi   - b.phi);
    d.psi_diff   = std::abs(a.psi   - b.psi);
    d.chi_diff   = std::abs(a.chi   - b.chi);
    d.omega_diff = std::abs(a.omega - b.omega);
    d.E_diff     = std::abs(a.E     - b.E);
    return d;
}

// ============================================================================
// Kernel Spectrum (system-wide)
// ============================================================================

/**
 * KernelSpectrum — statistics over all M_k in the system.
 */
struct KernelSpectrum {
    MaterialKernel mean{};        ///< <M> = (1/N) Σ M_k
    MaterialKernel variance{};    ///< Component-wise variance
    double mean_norm{};           ///< <||M||>
    double norm_variance{};       ///< Var(||M||)
    uint32_t N{};                 ///< Number of valid kernels
};

/**
 * Compute the kernel spectrum for a system of beads.
 */
inline KernelSpectrum compute_kernel_spectrum(
    const std::vector<MaterialKernel>& kernels)
{
    KernelSpectrum sp;
    if (kernels.empty()) return sp;

    // Pass 1: mean
    double n = 0.0;
    for (const auto& mk : kernels) {
        if (!mk.valid) continue;
        sp.mean.phi   += mk.phi;
        sp.mean.psi   += mk.psi;
        sp.mean.chi   += mk.chi;
        sp.mean.omega += mk.omega;
        sp.mean.E     += mk.E;
        sp.mean_norm  += mk.norm();
        n += 1.0;
    }
    if (n > 0.0) {
        sp.mean.phi   /= n;
        sp.mean.psi   /= n;
        sp.mean.chi   /= n;
        sp.mean.omega /= n;
        sp.mean.E     /= n;
        sp.mean_norm  /= n;
    }
    sp.N = static_cast<uint32_t>(n);

    // Pass 2: variance (Welford-like)
    for (const auto& mk : kernels) {
        if (!mk.valid) continue;
        double dphi = mk.phi - sp.mean.phi;
        double dpsi = mk.psi - sp.mean.psi;
        double dchi = mk.chi - sp.mean.chi;
        double domega = mk.omega - sp.mean.omega;
        double dE   = mk.E - sp.mean.E;
        sp.variance.phi   += dphi * dphi;
        sp.variance.psi   += dpsi * dpsi;
        sp.variance.chi   += dchi * dchi;
        sp.variance.omega += domega * domega;
        sp.variance.E     += dE * dE;
        double dnorm = mk.norm() - sp.mean_norm;
        sp.norm_variance  += dnorm * dnorm;
    }
    if (n > 1.0) {
        sp.variance.phi   /= (n - 1.0);
        sp.variance.psi   /= (n - 1.0);
        sp.variance.chi   /= (n - 1.0);
        sp.variance.omega /= (n - 1.0);
        sp.variance.E     /= (n - 1.0);
        sp.norm_variance  /= (n - 1.0);
    }
    sp.mean.valid = (n > 0.0);
    sp.variance.valid = (n > 1.0);

    return sp;
}

} // namespace theory
} // namespace coarse_grain
