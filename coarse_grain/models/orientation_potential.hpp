#pragma once
/**
 * orientation_potential.hpp — Orientation-Coupled Interaction Model
 *
 * Implements the reduced anisotropic interaction potential:
 *
 *   U_eff(r, θ_A, θ_B, φ) = U_0(r)
 *                           + λ₁ · f₁(r) · cos φ
 *                           + λ₂ · f₂(r) · cos²θ_A
 *                           + λ₃ · f₃(r) · cos²θ_B
 *
 * where U_0(r) is a baseline LJ 12-6 potential and f_i(r) are radial
 * modulation functions (using the attractive (σ/r)^6 range).
 *
 * Also provides:
 *   - Torque computation (τ = -∂U/∂Ω) for rotational dynamics
 *   - Decomposed energy output for anti-black-box inspection
 *   - Model hierarchy selection (isotropic / axisymmetric / descriptor)
 *
 * Anti-black-box: every contribution is separately computed and stored.
 * Deterministic: same inputs always yield same outputs.
 *
 * Reference: Section "Orientation-Coupled Interaction Framework" of
 *            section_anisotropic_beads.tex, Equations (12)-(15)
 */

#include "coarse_grain/core/orientation.hpp"
#include "atomistic/core/state.hpp"
#include <cmath>

namespace coarse_grain {

// ============================================================================
// Model Hierarchy
// ============================================================================

/**
 * InteractionLevel — selects the fidelity tier of the interaction model.
 *
 * Supports adaptive resolution: use cheaper models for exploration,
 * more detailed models when directional effects dominate.
 */
enum class InteractionLevel {
    ISOTROPIC,               // U(r) — standard LJ, no angular dependence
    AXISYMMETRIC,            // U(r, θ_A, θ_B, φ) — reduced orientation model
    DESCRIPTOR_RESOLVED      // U(r, Ω_A, Ω_B, S_A, S_B) — full SH coupling
};

// ============================================================================
// Configuration
// ============================================================================

/**
 * OrientationPotentialParams — parameters for the reduced interaction model.
 *
 * All three λ coefficients and the baseline LJ parameters are stored
 * explicitly for inspection.
 */
struct OrientationPotentialParams {
    double sigma{3.4};        // LJ sigma (Å)
    double epsilon{0.24};     // LJ epsilon (kcal/mol)
    double lambda1{0.0};      // Mutual alignment coupling (cos φ term)
    double lambda2{0.0};      // Bead A axis-alignment coupling (cos²θ_A term)
    double lambda3{0.0};      // Bead B axis-alignment coupling (cos²θ_B term)
};

// ============================================================================
// Result
// ============================================================================

/**
 * OrientationPotentialResult — fully decomposed energy and torques.
 */
struct OrientationPotentialResult {
    // Energy decomposition
    double E_isotropic{};     // U_0(r) — baseline LJ (kcal/mol)
    double E_alignment{};     // λ₁·f₁(r)·cos φ — mutual normal alignment
    double E_axis_A{};        // λ₂·f₂(r)·cos²θ_A — A alignment with axis
    double E_axis_B{};        // λ₃·f₃(r)·cos²θ_B — B alignment with axis
    double E_total{};         // Sum of all contributions

    // Orientation invariants (inspectable)
    OrientationInvariants invariants{};

    // Torque on each bead (world-frame vector)
    atomistic::Vec3 torque_A{};   // τ_A = -∂U/∂Ω_A
    atomistic::Vec3 torque_B{};   // τ_B = -∂U/∂Ω_B

    // Translational force on bead B (world-frame, F_A = -F_B)
    atomistic::Vec3 force_B{};
};

// ============================================================================
// Core Evaluation
// ============================================================================

/**
 * Evaluate the orientation-coupled interaction potential.
 *
 * @param pos_A    Position of bead A (Å)
 * @param pos_B    Position of bead B (Å)
 * @param ori_A    Orientation of bead A
 * @param ori_B    Orientation of bead B
 * @param params   Interaction parameters
 * @return OrientationPotentialResult with decomposed energy, forces, torques
 */
inline OrientationPotentialResult evaluate_orientation_potential(
    const atomistic::Vec3& pos_A,
    const atomistic::Vec3& pos_B,
    const BeadOrientation& ori_A,
    const BeadOrientation& ori_B,
    const OrientationPotentialParams& params)
{
    OrientationPotentialResult result;

    // Compute invariants
    result.invariants = compute_invariants(pos_A, pos_B, ori_A, ori_B);
    const auto& inv = result.invariants;

    double r = inv.r;
    if (r < 1e-10) r = 1e-10;

    double sr = params.sigma / r;
    double sr2 = sr * sr;
    double sr6 = sr2 * sr2 * sr2;
    double sr12 = sr6 * sr6;

    // ---- Energy decomposition ----

    // U_0(r): LJ 12-6
    result.E_isotropic = 4.0 * params.epsilon * (sr12 - sr6);

    // Radial modulation f(r) = ε · (σ/r)^6 (same range as LJ attraction)
    double f_r = params.epsilon * sr6;

    // λ₁ · f₁(r) · cos φ
    result.E_alignment = params.lambda1 * f_r * inv.cos_phi;

    // λ₂ · f₂(r) · cos²θ_A
    result.E_axis_A = params.lambda2 * f_r * inv.cos_theta_A * inv.cos_theta_A;

    // λ₃ · f₃(r) · cos²θ_B
    result.E_axis_B = params.lambda3 * f_r * inv.cos_theta_B * inv.cos_theta_B;

    result.E_total = result.E_isotropic + result.E_alignment
                   + result.E_axis_A + result.E_axis_B;

    // ---- Force on B (radial component only, angular forces omitted for now) ----
    // dU_0/dr = 4ε(-12·sr^12/r + 6·sr^6/r)
    double inv_r = 1.0 / r;
    double dU0_dr = 4.0 * params.epsilon * (-12.0 * sr12 + 6.0 * sr6) * inv_r;

    // df/dr = ε · (-6) · (σ/r)^6 / r
    double df_dr = -6.0 * f_r * inv_r;

    double dU_dr = dU0_dr
                 + params.lambda1 * df_dr * inv.cos_phi
                 + params.lambda2 * df_dr * inv.cos_theta_A * inv.cos_theta_A
                 + params.lambda3 * df_dr * inv.cos_theta_B * inv.cos_theta_B;

    // Force direction: along r̂ from A to B
    double rx = pos_B.x - pos_A.x;
    double ry = pos_B.y - pos_A.y;
    double rz = pos_B.z - pos_A.z;
    double rhat_x = rx * inv_r;
    double rhat_y = ry * inv_r;
    double rhat_z = rz * inv_r;

    // F_B = -dU/dr · r̂
    result.force_B = {-dU_dr * rhat_x, -dU_dr * rhat_y, -dU_dr * rhat_z};

    // ---- Torques ----
    // τ_A from cos²θ_A term:
    //   ∂(cos θ_A)/∂Ω_A → torque direction ∝ n̂_A × r̂
    //   ∂(cos φ)/∂Ω_A → torque direction ∝ n̂_A × n̂_B
    {
        // n̂_A × r̂
        double tA_r_x = ori_A.normal.y * rhat_z - ori_A.normal.z * rhat_y;
        double tA_r_y = ori_A.normal.z * rhat_x - ori_A.normal.x * rhat_z;
        double tA_r_z = ori_A.normal.x * rhat_y - ori_A.normal.y * rhat_x;

        // n̂_A × n̂_B
        double tA_n_x = ori_A.normal.y * ori_B.normal.z - ori_A.normal.z * ori_B.normal.y;
        double tA_n_y = ori_A.normal.z * ori_B.normal.x - ori_A.normal.x * ori_B.normal.z;
        double tA_n_z = ori_A.normal.x * ori_B.normal.y - ori_A.normal.y * ori_B.normal.x;

        // τ_A = -λ₂·f(r)·2·cos θ_A · (n̂_A × r̂) - λ₁·f(r) · (n̂_A × n̂_B)
        double coeff_axis = -params.lambda2 * f_r * 2.0 * inv.cos_theta_A;
        double coeff_align = -params.lambda1 * f_r;

        result.torque_A = {
            coeff_axis * tA_r_x + coeff_align * tA_n_x,
            coeff_axis * tA_r_y + coeff_align * tA_n_y,
            coeff_axis * tA_r_z + coeff_align * tA_n_z
        };
    }

    {
        // n̂_B × r̂
        double tB_r_x = ori_B.normal.y * rhat_z - ori_B.normal.z * rhat_y;
        double tB_r_y = ori_B.normal.z * rhat_x - ori_B.normal.x * rhat_z;
        double tB_r_z = ori_B.normal.x * rhat_y - ori_B.normal.y * rhat_x;

        // n̂_B × n̂_A
        double tB_n_x = ori_B.normal.y * ori_A.normal.z - ori_B.normal.z * ori_A.normal.y;
        double tB_n_y = ori_B.normal.z * ori_A.normal.x - ori_B.normal.x * ori_A.normal.z;
        double tB_n_z = ori_B.normal.x * ori_A.normal.y - ori_B.normal.y * ori_A.normal.x;

        // τ_B = -λ₃·f(r)·2·cos θ_B · (n̂_B × r̂) - λ₁·f(r) · (n̂_B × n̂_A)
        double coeff_axis = -params.lambda3 * f_r * 2.0 * inv.cos_theta_B;
        double coeff_align = -params.lambda1 * f_r;

        result.torque_B = {
            coeff_axis * tB_r_x + coeff_align * tB_n_x,
            coeff_axis * tB_r_y + coeff_align * tB_n_y,
            coeff_axis * tB_r_z + coeff_align * tB_n_z
        };
    }

    return result;
}

/**
 * Evaluate the isotropic-only potential (hierarchy level 0).
 * This is the fallback for beads without orientation data.
 */
inline OrientationPotentialResult evaluate_isotropic_only(
    const atomistic::Vec3& pos_A,
    const atomistic::Vec3& pos_B,
    double sigma, double epsilon)
{
    OrientationPotentialResult result;

    double rx = pos_B.x - pos_A.x;
    double ry = pos_B.y - pos_A.y;
    double rz = pos_B.z - pos_A.z;
    double r2 = rx * rx + ry * ry + rz * rz;
    if (r2 < 1e-20) r2 = 1e-20;
    double r = std::sqrt(r2);

    result.invariants.r = r;

    double sr = sigma / r;
    double sr6 = sr * sr * sr * sr * sr * sr;
    double sr12 = sr6 * sr6;

    result.E_isotropic = 4.0 * epsilon * (sr12 - sr6);
    result.E_total = result.E_isotropic;

    double inv_r = 1.0 / r;
    double dU_dr = 4.0 * epsilon * (-12.0 * sr12 + 6.0 * sr6) * inv_r;

    result.force_B = {-dU_dr * rx * inv_r,
                      -dU_dr * ry * inv_r,
                      -dU_dr * rz * inv_r};

    return result;
}

} // namespace coarse_grain
