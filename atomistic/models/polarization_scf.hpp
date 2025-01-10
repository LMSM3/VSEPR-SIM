#pragma once
/**
 * polarization_scf.hpp  --  SCF induced-dipole polarization model.
 *
 * Physics (Applequist 1972, Thole 1981):
 *
 *   mu_i = alpha_i * E_i^loc
 *   E_i^loc = E_i^perm + SUM_{j!=i, r_ij > r_excl} T_ij(damp) * mu_j
 *
 * Short-range exclusion (r_excl):
 *   Pairs with r_ij < r_excl are excluded from BOTH the permanent-charge
 *   driving field E_perm and the dipole-dipole interaction tensor.
 *   This is the standard 1-2/1-3 exclusion used by AMOEBA and Drude force
 *   fields: the Thole model is designed for intermolecular use only.
 *   At O-H = 0.96 A, alpha_O * T_OH (even Thole-damped) exceeds the
 *   spectral-radius limit for Jacobi iteration, causing divergence.
 *   r_excl = 1.6 A covers all typical covalent bonds without bond topology.
 *
 * Thole exponential damping (applied to intermolecular pairs):
 *   T_ij^damp = f(u) * T_ij^bare
 *   f(u) = 1 - (1 + a*u/2) * exp(-a*u),  a = 2.6
 *   u    = r_ij / (alpha_i * alpha_j)^(1/6)
 *
 * Convergence criterion: ||Delta_mu||_inf < tolerance
 *
 * Units:
 *   Fields:       e/Ang^2 (Gaussian convention, NO k_e in field computation)
 *   Polarizability: Ang^3
 *   Dipoles:      e*Ang
 *   Energy:       kcal/mol  (k_e = 332.0636 applied in energy formula only)
 *
 * References:
 *   [1] Applequist et al., JACS 94, 2952 (1972)
 *   [2] Thole, Chem. Phys. 59, 341 (1981)
 *   [3] Miller, JACS 112, 8533 (1990)
 *   [4] Ponder & Case, Adv. Prot. Chem. 66, 27 (2003) -- AMOEBA exclusions
 */

#include "../core/state.hpp"
#include "alpha_model.hpp"
#include <cmath>
#include <cstdint>
#include <vector>

namespace atomistic {
namespace polarization {

// ============================================================================
// Atomic polarizabilities — generative model
// ============================================================================

/// Predict scalar isotropic polarizability alpha (Angstrom^3) for atomic number Z.
/// Delegates to the fitted generative model in alpha_model.hpp.
/// This replaces the former hardcoded switch table (Miller 1990).
inline double alpha_for_Z(uint32_t Z) noexcept {
    return alpha_predict(Z);
}

// ============================================================================
// Parameters
// ============================================================================

struct SCFParams {
    double tolerance = 1e-6;  // ||Delta_mu||_inf convergence  (e*Ang)
    int    max_iter  = 200;   // hard iteration ceiling
    double omega     = 0.5;   // linear-mixing factor in (0, 1]
    double thole_a   = 2.6;   // Thole exponential screening strength
    double r_cutoff  = 12.0;  // dipole-dipole interaction cutoff  (Ang)
    double r_excl    = 1.6;   // short-range exclusion radius (Ang)
                              // Pairs with r < r_excl are excluded from both
                              // E_perm induction and the T_ij tensor.
                              // Covers all covalent bonds; equivalent to
                              // 1-2/1-3 exclusions without bond topology.
    double r_min     = 0.1;   // absolute singularity guard  (Ang)

    static constexpr double k_e = 332.0636; // kcal*Ang/(mol*e^2)
};

// ============================================================================
// Result
// ============================================================================

struct SCFResult {
    int    iterations;   // SCF iterations taken
    double residual;     // final ||Delta_mu||_inf  (e*Ang)
    bool   converged;    // residual < tolerance
    double U_pol;        // polarization energy  (kcal/mol)
};

// ============================================================================
// API
// ============================================================================

/// Fill s.polarizabilities from s.type via alpha_for_Z.
/// Also zeros s.induced_dipoles and s.permanent_dipoles.
void init_polarizabilities(State& s);

/// Solve for self-consistent induced dipoles.
/// Reads  : s.X, s.Q, s.polarizabilities, s.permanent_dipoles, s.box
/// Writes : s.induced_dipoles, s.E.Upol
SCFResult solve(State& s, const SCFParams& p = {});

} // namespace polarization
} // namespace atomistic
