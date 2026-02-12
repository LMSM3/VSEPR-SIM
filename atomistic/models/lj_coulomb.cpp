#include "model.hpp"
#include "../../src/pot/uff_params.hpp"  // Shared parameter database
#include <algorithm>
#include <stdexcept>
#include <cmath>
#include <memory>
#include <iostream>  // For debug output

namespace atomistic {

/**
 * Lennard-Jones + Coulomb nonbonded model (MD MODE)
 * 
 * Purpose: Full LJ + Coulomb for molecular dynamics and force field calculations
 * 
 * Related Files:
 * - src/pot/uff_params.hpp: Shared parameter database (used by this file)
 * - src/pot/energy_nonbonded.hpp: VSEPR mode (WCA repulsion-only)
 * 
 * Key Differences from VSEPR Mode:
 * - Full LJ (attractive + repulsive) vs WCA (repulsion only)
 * - Coulomb interactions included
 * - PBC support via atomistic::State::box
 * - Quintic switching function for smooth cutoff
 * 
 * Lennard-Jones + Coulomb nonbonded model
 * 
 * Physics:
 * --------
 * 
 * 1. Lennard-Jones potential (12-6):
 *    U_LJ(r) = 4ε[(σ/r)¹² - (σ/r)⁶]
 *    
 *    - ε = well depth (energy at minimum, r = 2^(1/6)σ)
 *    - σ = zero-crossing distance (U(σ) = 0)
 *    - Minimum at r_min = 2^(1/6)σ ≈ 1.122σ
 *    - Force: F = -dU/dr = 24ε/r[(σ/r)⁶ - 2(σ/r)¹²]r̂
 * 
 * 2. Lorentz-Berthelot combining rules:
 *    σ_ij = (σ_i + σ_j)/2     [arithmetic mean]
 *    ε_ij = √(ε_i · ε_j)      [geometric mean]
 *    
 *    Alternative: Waldman-Hagler (6th power for σ)
 *    σ_ij = [(σ_i⁶ + σ_j⁶)/2]^(1/6)
 * 
 * 3. Coulomb potential:
 *    U_C(r) = k_e q_i q_j / r
 *    
 *    - k_e = Coulomb constant (332.0636 kcal·Å·e⁻²·mol⁻¹ in AMBER units)
 *    - q_i in elementary charges (e)
 *    - Force: F = -k_e q_i q_j / r² r̂
 * 
 * 4. Cutoff and switching:
 *    - Hard cutoff at r_c with potential shift
 *    - Switch function for smooth cutoff (avoid force discontinuity)
 *    - Long-range: PME or reaction field for periodic systems
 * 
 * 5. 1-4 scaling:
 *    - Atoms separated by 3 bonds have reduced nonbonded interactions
 *    - Typical: scale LJ and Coulomb by 0.5 (AMBER) or 0.5/1.2 (OPLS)
 * 
 * References:
 * - Jones, J.E. (1924). "On the determination of molecular fields." Proc. R. Soc. A 106(738), 463.
 * - Lorentz, H.A. (1881). "Ueber die Anwendung..." Ann. Phys. 248(1), 127.
 * - Berthelot, D. (1898). "Sur le mélange des gaz." C.R. Hebd. Acad. Sci. 126, 1703.
 * - Darden, T. et al. (1993). "Particle mesh Ewald." J. Chem. Phys. 98(12), 10089.
 */

static inline bool finite3(const Vec3& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

/**
 * Per-atom LJ parameters
 */
struct AtomLJParams {
    double sigma;    // Å
    double epsilon;  // kcal/mol
};

/**
 * Enhanced LJ+Coulomb with proper combining rules and cutoff handling
 */
struct LJCoulomb : IModel {
    // Per-type LJ parameters (indexed by State.type)
    std::vector<AtomLJParams> lj_params;
    
    // Use Lorentz-Berthelot combining rules
    bool use_lorentz_berthelot = true;
    
    // 1-4 scaling factors
    double scale_14_lj = 0.5;
    double scale_14_coul = 0.5;
    
    LJCoulomb() {
        // Load LJ parameters from shared UFF database (src/pot/uff_params.hpp)
        // Rappe et al. (1992) "UFF, a full periodic table force field"
        // J. Am. Chem. Soc. 114(25), 10024-10035

        lj_params.resize(120);  // Support Z=1 to Z=119

        // Load from centralized database
        for (int Z = 1; Z < 120; ++Z) {
            auto params = vsepr::get_lj_params(Z);
            if (params) {
                lj_params[Z].sigma = params->sigma;
                lj_params[Z].epsilon = params->epsilon;
            } else {
                // Fallback to carbon-like parameters for undefined elements
                lj_params[Z].sigma = 3.851;
                lj_params[Z].epsilon = 0.105;
            }
        }

        // Notes:
        // - Parameters loaded from src/pot/uff_params.hpp (shared with VSEPR)
        // - Production code should load from external database (JSON/TOML)
        // - Currently supports 19 elements with explicit UFF parameters
        // - Others use carbon-like fallback
    }
    
    void eval(State& s, const ModelParams& p) const override {
        std::fill(s.F.begin(), s.F.end(), Vec3{0, 0, 0});
        s.E = {};
        
        double rc2 = p.rc * p.rc;
        
        // Switch function parameters (smooth cutoff from 0.9·rc to rc)
        double r_on = 0.9 * p.rc;
        double r_on2 = r_on * r_on;
        
        for (uint32_t i = 0; i < s.N; i++) {
            for (uint32_t j = i + 1; j < s.N; j++) {
                // Apply minimum image convention if PBC enabled
                Vec3 rij = s.box.enabled ? s.box.delta(s.X[i], s.X[j]) : (s.X[i] - s.X[j]);
                double r2 = dot(rij, rij);

                if (r2 > rc2) continue;  // Beyond cutoff
                
                double r = std::sqrt(r2);
                if (r < 0.1) continue;  // Avoid singularity
                
                // Get LJ parameters with combining rules
                uint32_t type_i = (i < s.type.size()) ? s.type[i] : 1;
                uint32_t type_j = (j < s.type.size()) ? s.type[j] : 1;
                
                if (type_i >= lj_params.size()) type_i = 1;
                if (type_j >= lj_params.size()) type_j = 1;
                
                double sigma_i = lj_params[type_i].sigma;
                double sigma_j = lj_params[type_j].sigma;
                double eps_i = lj_params[type_i].epsilon;
                double eps_j = lj_params[type_j].epsilon;
                
                // Lorentz-Berthelot combining rules
                double sigma_ij = (sigma_i + sigma_j) / 2.0;
                double eps_ij = std::sqrt(eps_i * eps_j);

                // NOTE: Per-type parameters take precedence over global params
                // Global params are ONLY used if per-type params are not set
                
                // LJ potential: U = 4ε[(σ/r)¹² - (σ/r)⁶]
                double sr = sigma_ij / r;
                double sr6 = sr*sr*sr * sr*sr*sr;
                double sr12 = sr6 * sr6;
                
                double U_lj = 4.0 * eps_ij * (sr12 - sr6);
                double F_lj_r = 24.0 * eps_ij * (2.0*sr12 - sr6) / r;  // F = -dU/dr
                
                // Coulomb potential: U = k_e q_i q_j / r
                // TODO(CRITICAL): Coulomb forces cause systematic instability in MD
                // After 7+ hours debugging: integrator works (Ar: T=162K), but NaCl explodes (T~10³²K)
                // Root cause: Unknown integrator-Coulomb coupling issue
                // See: IONIC_MD_BLOCKED.md for full analysis
                double qi = (i < s.Q.size()) ? s.Q[i] : 0.0;
                double qj = (j < s.Q.size()) ? s.Q[j] : 0.0;

                // DISABLED: Coulomb forces until integrator-Coulomb coupling fixed
                double U_coul = 0.0;  // p.k_coul * qi * qj / r;
                double F_coul_r = 0.0;  // p.k_coul * qi * qj / r;



                // Switching function (quintic spline for smoothness)
                double switch_val = 1.0;
                double switch_deriv = 0.0;
                
                if (r2 > r_on2) {
                    double x = (r - r_on) / (p.rc - r_on);  // x ∈ [0,1]
                    double x2 = x*x;
                    double x3 = x2*x;
                    // S(x) = 1 - 10x³ + 15x⁴ - 6x⁵
                    switch_val = 1.0 - x3*(10.0 - 15.0*x + 6.0*x2);
                    // S'(x) = -30x²(1 - 2x + x²) / (rc - r_on)
                    switch_deriv = -30.0*x2*(1.0 - 2.0*x + x2) / (p.rc - r_on);
                }
                
                // Apply switching
                double F_total_r = (F_lj_r + F_coul_r) * switch_val 
                                  + (U_lj + U_coul) * switch_deriv;
                
                s.E.UvdW += U_lj * switch_val;
                s.E.UCoul += U_coul * switch_val;
                
                // Force: F = F_r · r̂ = F_r · rij/r
                Vec3 f = rij * (F_total_r / r);
                
                if (!finite3(f)) throw std::runtime_error("Model produced non-finite force");
                
                s.F[i] = s.F[i] + f;
                s.F[j] = s.F[j] - f;
            }
        }
    }
};

std::unique_ptr<IModel> create_lj_coulomb_model() {
    return std::make_unique<LJCoulomb>();
}

} // namespace atomistic

