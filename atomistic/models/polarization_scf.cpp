/**
 * polarization_scf.cpp  --  SCF induced-dipole polarization solver.
 *
 * Key design decisions:
 *
 * 1. Short-range exclusion (r < r_excl) in BOTH loops:
 *    The Thole model is designed for intermolecular polarization.
 *    Intramolecular pairs (e.g. O-H at 0.96 A) have alpha*T_damp > 0.7
 *    even after Thole screening, pushing the spectral radius of the
 *    iteration matrix above 1 and causing divergence.  Excluding pairs
 *    with r < r_excl = 1.6 A from both E_perm and the T_ij tensor is
 *    physically correct: fixed partial charges already encode the mean-field
 *    intramolecular electron distribution; re-polarizing intramolecularly
 *    would double-count it.  (AMOEBA uses identical 1-2/1-3 exclusions.)
 *
 * 2. Thole exponential damping for intermolecular pairs:
 *    f(u) = 1 - (1 + a*u/2)*exp(-a*u),  a = 2.6
 *    prevents polarization catastrophe at intermediate distances.
 *
 * 3. Linear mixing (omega = 0.5 default):
 *    Guarantees convergence when spectral radius of the damped A matrix
 *    is bounded (which exclusions ensure).
 *
 * Unit convention (Gaussian/Tinker style for fields, AMBER for energies):
 *    - Fields: E in e/Ang^2 (Gaussian units, NO k_e factor)
 *    - Polarizability: alpha in Ang^3
 *    - Induced dipole: mu = alpha * E, in e*Ang
 *    - Energy: U_pol = -0.5 * k_e * SUM mu_i . E_perm_i, in kcal/mol
 *    k_e appears ONLY in the energy formula, not in the field computation.
 *    This ensures E_perm (from charges) and E_ind (from dipoles) are in the
 *    same units when added together in the SCF local-field equation.
 */

#include "polarization_scf.hpp"
#include <algorithm>
#include <cassert>

namespace atomistic {
namespace polarization {

// ----------------------------------------------------------------------------
// Internal helpers
// ----------------------------------------------------------------------------

static inline double thole_f(double u, double a) noexcept {
    const double au = a * u;
    return 1.0 - (1.0 + 0.5 * au) * std::exp(-au);
}

static inline Vec3 mic_disp(const Vec3& ri, const Vec3& rj, const BoxPBC& box) noexcept {
    return box.enabled ? box.delta(ri, rj) : (rj - ri);
}

/// Add contribution of site j's induced dipole to the field at site i.
///   E_ind += T_ij^damp * mu_j
static void add_dipole_field(Vec3& E_ind,
                             const Vec3& rij,
                             const Vec3& mu_j,
                             double alpha_i,
                             double alpha_j,
                             double thole_a) noexcept
{
    const double r2 = dot(rij, rij);
    const double r  = std::sqrt(r2);
    if (r < 1e-10) return;

    const double denom = std::pow(alpha_i * alpha_j, 1.0 / 6.0);
    const double u     = (denom > 1e-15) ? r / denom : 1e30;
    const double f     = thole_f(u, thole_a);

    const double r3   = r2 * r;
    const double r5   = r3 * r2;
    const double mu_r = dot(mu_j, rij);

    E_ind.x += f * (mu_j.x / r3 - 3.0 * mu_r * rij.x / r5);
    E_ind.y += f * (mu_j.y / r3 - 3.0 * mu_r * rij.y / r5);
    E_ind.z += f * (mu_j.z / r3 - 3.0 * mu_r * rij.z / r5);
}

// ----------------------------------------------------------------------------
// Public API
// ----------------------------------------------------------------------------

void init_polarizabilities(State& s) {
    s.polarizabilities.assign(s.N, 0.0);
    s.induced_dipoles.assign(s.N, Vec3{0, 0, 0});
    s.permanent_dipoles.assign(s.N, Vec3{0, 0, 0});
    for (uint32_t i = 0; i < s.N; ++i) {
        const uint32_t Z = (i < s.type.size()) ? s.type[i] : 6u;
        s.polarizabilities[i] = alpha_for_Z(Z);
    }
}

SCFResult solve(State& s, const SCFParams& p) {
    const uint32_t N = s.N;
    assert(s.polarizabilities.size() == N
           && "call init_polarizabilities() before solve()");

    if (s.induced_dipoles.size() != N)
        s.induced_dipoles.assign(N, Vec3{0, 0, 0});

    const bool has_perm_mu = (s.permanent_dipoles.size() == N);
    const bool has_charges  = (s.Q.size() == N);

    // -------------------------------------------------------------------------
    // Step 1: Permanent electric field E_perm[i].
    //
    // Pairs with r < r_excl are SKIPPED (1-2/1-3 exclusion).
    // This prevents intramolecular charges from driving unphysical
    // induced dipoles on covalently bonded neighbours.
    // -------------------------------------------------------------------------
    std::vector<Vec3> E_perm(N, Vec3{0, 0, 0});

    for (uint32_t i = 0; i < N; ++i) {
        for (uint32_t j = 0; j < N; ++j) {
            if (i == j) continue;

            const Vec3   rij = mic_disp(s.X[i], s.X[j], s.box);
            const double r2  = dot(rij, rij);
            const double r   = std::sqrt(r2);

            if (r < p.r_min)  continue;  // singularity guard
            if (r < p.r_excl) continue;  // 1-2/1-3 exclusion

            // Charge contribution: q_j * rij / r^3  (Gaussian units, no k_e)
            if (has_charges && s.Q[j] != 0.0) {
                const double fac = s.Q[j] / (r2 * r);
                E_perm[i].x += fac * rij.x;
                E_perm[i].y += fac * rij.y;
                E_perm[i].z += fac * rij.z;
            }

            // Permanent dipole contribution (bare T tensor, no extra damping)
            if (has_perm_mu) {
                const Vec3& mu0 = s.permanent_dipoles[j];
                if (dot(mu0, mu0) > 0.0) {
                    const double r3   = r2 * r;
                    const double r5   = r3 * r2;
                    const double mu_r = dot(mu0, rij);
                    E_perm[i].x += mu0.x / r3 - 3.0 * mu_r * rij.x / r5;
                    E_perm[i].y += mu0.y / r3 - 3.0 * mu_r * rij.y / r5;
                    E_perm[i].z += mu0.z / r3 - 3.0 * mu_r * rij.z / r5;
                }
            }
        }
    }

    // -------------------------------------------------------------------------
    // Step 2: SCF iteration with linear mixing.
    // -------------------------------------------------------------------------
    double residual = 1.0;
    int    iter     = 0;
    std::vector<Vec3> mu_new(N);

    for (; iter < p.max_iter; ++iter) {
        residual = 0.0;

        for (uint32_t i = 0; i < N; ++i) {
            const double alpha_i = s.polarizabilities[i];
            if (alpha_i == 0.0) { mu_new[i] = Vec3{0, 0, 0}; continue; }

            Vec3 E_ind{0, 0, 0};
            for (uint32_t j = 0; j < N; ++j) {
                if (i == j) continue;

                const Vec3   rij = mic_disp(s.X[i], s.X[j], s.box);
                const double r2  = dot(rij, rij);
                if (r2 > p.r_cutoff * p.r_cutoff) continue;
                const double r = std::sqrt(r2);
                if (r < p.r_min)  continue;
                if (r < p.r_excl) continue;  // 1-2/1-3 exclusion

                add_dipole_field(E_ind, rij,
                                 s.induced_dipoles[j],
                                 alpha_i, s.polarizabilities[j],
                                 p.thole_a);
            }

            // mu_target = alpha_i * (E_perm + E_ind)
            const Vec3 mu_target{
                alpha_i * (E_perm[i].x + E_ind.x),
                alpha_i * (E_perm[i].y + E_ind.y),
                alpha_i * (E_perm[i].z + E_ind.z)
            };

            // Linear mixing
            mu_new[i] = Vec3{
                (1.0 - p.omega) * s.induced_dipoles[i].x + p.omega * mu_target.x,
                (1.0 - p.omega) * s.induced_dipoles[i].y + p.omega * mu_target.y,
                (1.0 - p.omega) * s.induced_dipoles[i].z + p.omega * mu_target.z
            };

            // L-inf residual
            const Vec3 delta{
                mu_new[i].x - s.induced_dipoles[i].x,
                mu_new[i].y - s.induced_dipoles[i].y,
                mu_new[i].z - s.induced_dipoles[i].z
            };
            residual = std::max({residual,
                                 std::abs(delta.x),
                                 std::abs(delta.y),
                                 std::abs(delta.z)});
        }

        s.induced_dipoles = mu_new;
        if (residual < p.tolerance) { ++iter; break; }
    }

    // -------------------------------------------------------------------------
    // Step 3: Polarization energy  U_pol = -0.5 * k_e * SUM_i  mu_i . E_perm[i]
    //
    // mu_i is in e*Ang, E_perm is in e/Ang^2 (Gaussian convention),
    // so mu.E is in e^2/Ang.  Multiply by k_e to get kcal/mol.
    // -------------------------------------------------------------------------
    double U_pol = 0.0;
    for (uint32_t i = 0; i < N; ++i)
        U_pol -= 0.5 * p.k_e * dot(s.induced_dipoles[i], E_perm[i]);
    s.E.Upol = U_pol;

    return SCFResult{iter, residual, (residual < p.tolerance), U_pol};
}

} // namespace polarization
} // namespace atomistic
