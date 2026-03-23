#pragma once
/**
 * anisotropic_potential.hpp — Orientation-Dependent Bead-Bead Interaction
 *
 * Defines the interaction potential between two anisotropic beads:
 *
 *   U ≈ ∫ S_B(θ, φ) · S_C(θ', φ') · K(r) dΩ
 *
 * In practice, the surface coupling integral is approximated using
 * the SH coefficients of each bead and a radial kernel K(r).
 *
 * The isotropic fallback is a standard LJ 12-6 potential.
 * The anisotropic correction modulates the LJ well depth based on
 * the mutual orientation of the two beads.
 *
 * Anti-black-box: the isotropic and anisotropic contributions are
 * computed and reported separately, so their relative magnitudes
 * are always inspectable.
 *
 * Reference: Section 6 of section_anisotropic_beads.tex
 */

#include "coarse_grain/core/surface_descriptor.hpp"
#include "coarse_grain/core/spherical_harmonics.hpp"
#include "atomistic/core/state.hpp"
#include <array>
#include <cmath>

namespace coarse_grain {

/**
 * AnisotropicPotentialResult — decomposed energy from bead-bead interaction.
 */
struct AnisotropicPotentialResult {
    double E_isotropic{};       // Standard LJ contribution (kcal/mol)
    double E_anisotropic{};     // Orientation-dependent correction (kcal/mol)
    double E_total{};           // E_isotropic + E_anisotropic
    double coupling{};          // Raw surface coupling integral value
};

/**
 * Compute the surface coupling coefficient between two beads.
 *
 * Uses the SH orthogonality: ∫ Y_ℓm · Y_ℓ'm' dΩ = δ_{ℓℓ'} δ_{mm'}
 * So the coupling integral reduces to:
 *
 *   C = Σ_ℓ Σ_m c_ℓm^B · c_ℓm^C
 *
 * This is simply the dot product of the SH coefficient vectors.
 * A high value means the two beads have similar anisotropic profiles
 * and are oriented such that their surface descriptors overlap.
 *
 * Note: This simplified version assumes the beads share the same
 * angular frame (no relative rotation). For a full treatment,
 * Wigner D-matrix rotation of coefficients would be needed.
 */
inline double surface_coupling(const SurfaceDescriptor& A,
                                const SurfaceDescriptor& B)
{
    double dot = 0.0;
    for (int i = 0; i < SH_NUM_COEFFS; ++i)
        dot += A.coeffs[i] * B.coeffs[i];
    return dot;
}

/**
 * Compute anisotropic bead-bead interaction energy.
 *
 * The model:
 *   U(r, Ω_B, Ω_C) = U_LJ(r; σ, ε) + λ · C(Ω_B, Ω_C) · K(r)
 *
 * where:
 *   U_LJ = standard 12-6 LJ
 *   C    = surface coupling coefficient (SH dot product)
 *   K(r) = radial modulation: K(r) = (σ/r)^6  (same range as LJ attraction)
 *   λ    = anisotropic coupling strength (dimensionless)
 *
 * @param r_vec  Separation vector from bead B to bead C (Å)
 * @param sigma  LJ sigma parameter (Å)
 * @param epsilon LJ epsilon parameter (kcal/mol)
 * @param desc_B  Surface descriptor of bead B
 * @param desc_C  Surface descriptor of bead C
 * @param lambda  Coupling strength (default 0.1)
 */
inline AnisotropicPotentialResult anisotropic_potential(
    const atomistic::Vec3& r_vec,
    double sigma,
    double epsilon,
    const SurfaceDescriptor& desc_B,
    const SurfaceDescriptor& desc_C,
    double lambda = 0.1)
{
    AnisotropicPotentialResult result;

    double r2 = r_vec.x * r_vec.x + r_vec.y * r_vec.y + r_vec.z * r_vec.z;
    if (r2 < 1e-10) r2 = 1e-10;

    double r = std::sqrt(r2);
    double sr = sigma / r;
    double sr6 = sr * sr * sr * sr * sr * sr;
    double sr12 = sr6 * sr6;

    // Isotropic LJ 12-6
    result.E_isotropic = 4.0 * epsilon * (sr12 - sr6);

    // Surface coupling
    result.coupling = surface_coupling(desc_B, desc_C);

    // Radial kernel (same range as LJ attraction)
    double K_r = sr6;

    // Anisotropic correction
    result.E_anisotropic = lambda * result.coupling * K_r * epsilon;

    result.E_total = result.E_isotropic + result.E_anisotropic;
    return result;
}

/**
 * Pure isotropic LJ 12-6 energy (fallback for beads without surface data).
 */
inline double isotropic_lj(double r, double sigma, double epsilon) {
    if (r < 1e-10) r = 1e-10;
    double sr = sigma / r;
    double sr6 = sr * sr * sr * sr * sr * sr;
    return 4.0 * epsilon * (sr6 * sr6 - sr6);
}

} // namespace coarse_grain
