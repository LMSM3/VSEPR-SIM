// mcf_cai_force.cpp  -  MCF-CAI Kernel Fork  -  Force evaluator implementation
// Phase 2b | v1.0 | VSPER-SIM v5.0.0-main | Status: BLUE

#include "../../include/vsim/kernel_mcf/mcf_cai_force.hpp"

#include <cmath>
#include <limits>

namespace vsim::kernel_mcf {

// ============================================================================
// eval_lj_pair
// Computes LJ 12-6 energy for a pair at distance r2 (Å²).
// Accumulates forces on oi and oj.
// Returns energy contribution [eV].
// ============================================================================
double McfCaiForceEvaluator::eval_lj_pair(McfCaiObject& oi, McfCaiObject& oj,
                                           const Vec3d& dr, double r2,
                                           const McfCaiForceParams& p)
{
    const LJSpeciesParams pi = lj_params_for_Z(oi.Z());
    const LJSpeciesParams pj = lj_params_for_Z(oj.Z());
    const LJSpeciesParams pij = (p.mixing == "geometric") ? mix_geo(pi, pj) : mix_lb(pi, pj);

    const double sig  = pij.sigma_A;
    const double eps  = pij.eps_eV;
    if (eps < 1e-30 || sig < 1e-10) return 0.0;

    const double sig2  = sig * sig;
    const double ir2   = sig2 / r2;         // (σ/r)²
    const double ir6   = ir2 * ir2 * ir2;   // (σ/r)⁶
    const double ir12  = ir6 * ir6;         // (σ/r)¹²

    // E_LJ = 4ε[(σ/r)¹² - (σ/r)⁶]
    const double e_lj  = 4.0 * eps * (ir12 - ir6);

    // dE/dr = 4ε/r² * [-12(σ/r)¹² + 6(σ/r)⁶]
    // F_i = -dE/dr_ij * r_hat = (dE/dr)/r * dr_vec   (sign: dr = ri - rj)
    const double fmag  = 4.0 * eps * (-12.0*ir12 + 6.0*ir6) / r2;
    for (int d = 0; d < 3; ++d) {
        const double f = fmag * dr[d];
        oi.force[d] += f;
        oj.force[d] -= f;
    }
    return e_lj;
}

// ============================================================================
// eval_coul_pair
// Computes Coulomb 1/r energy for a pair at distance r.
// Accumulates forces on oi and oj.
// Returns energy contribution [eV].
// ============================================================================
double McfCaiForceEvaluator::eval_coul_pair(McfCaiObject& oi, McfCaiObject& oj,
                                             const Vec3d& dr, double r)
{
    const double qi = oi.charge();
    const double qj = oj.charge();
    if (std::abs(qi) < 1e-12 || std::abs(qj) < 1e-12) return 0.0;

    // E = k_e * qi * qj / r
    const double e_coul = kCoulomb_eVA * qi * qj / r;
    // F_i = -k_e * qi * qj / r³ * dr
    const double fmag   = -kCoulomb_eVA * qi * qj / (r * r * r);
    for (int d = 0; d < 3; ++d) {
        const double f = fmag * dr[d];
        oi.force[d] += f;
        oj.force[d] -= f;
    }
    return e_coul;
}

// ============================================================================
// operator()  -  main force evaluation entry point
// ============================================================================
double McfCaiForceEvaluator::operator()(McfCaiWorld& world) {
    const std::size_t N   = world.size();
    const double cut2     = params_.cutoff_A * params_.cutoff_A;
    const bool   use_pbc  = world.box().any_pbc();

    // Zero all forces
    for (auto& obj : world)
        obj.force = kZeroVec3d;

    double e_lj   = 0.0;
    double e_coul = 0.0;

    // O(N²) pair loop  (neighbour list: future optimisation)
    for (std::size_t i = 0; i < N; ++i) {
        for (std::size_t j = i + 1; j < N; ++j) {
            auto& oi = world.at(i);
            auto& oj = world.at(j);

            Vec3d dr;
            if (use_pbc) {
                dr = world.min_image(oi.pos(), oj.pos());
            } else {
                for (int d = 0; d < 3; ++d)
                    dr[d] = oi.pos()[d] - oj.pos()[d];
            }

            const double r2 = dr[0]*dr[0] + dr[1]*dr[1] + dr[2]*dr[2];
            if (r2 > cut2 || r2 < 1e-10) continue;
            const double r  = std::sqrt(r2);

            if (params_.use_lj)
                e_lj   += eval_lj_pair(oi, oj, dr, r2, params_);
            if (params_.use_coulomb)
                e_coul += eval_coul_pair(oi, oj, dr, r);
        }
    }

    last_lj_   = e_lj;
    last_coul_ = e_coul;
    last_pe_   = e_lj + e_coul;

    // Store PE in world stats for integrator access
    // (world.stats() is const-exposed; use recompute_stats pathway)
    // The integrator calls world.recompute_stats() after this returns.
    return last_pe_;
}

} // namespace vsim::kernel_mcf
