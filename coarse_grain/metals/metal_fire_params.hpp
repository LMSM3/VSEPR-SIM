#pragma once
/**
 * metal_fire_params.hpp — Metal-Class FIRE 6+9 Parameter Sets
 *
 * Provides tuned SeedBeadParams for each metal crystal class.
 * Parameters reflect EAM-informed τ and γ channel settings:
 *
 *   FCC noble metals    — soft, long-range dispersion-dominant
 *   FCC transition      — intermediate: steric + dispersion balanced
 *   BCC refractory      — stiff, short-range, high τ
 *   HCP proxy           — similar to FCC transition, slightly anisotropic bias
 *   Alloy (mixed)       — interpolated from A and B class parameters
 *
 * All values are deterministic constants — no runtime tuning or fitting.
 * They are physics-motivated approximations, not EAM fits.
 *
 * Reference:
 *   - Daw & Baskes, Phys. Rev. B 29, 6443 (1984) — EAM basis
 *   - Finnis & Sinclair, Phil. Mag. A 50, 45 (1984) — FS potential
 *   - coarse_grain/models/seed_bead_stepper.hpp — parameter struct
 */

#include "coarse_grain/metals/metal_registry.hpp"
#include "coarse_grain/models/seed_bead_stepper.hpp"

namespace coarse_grain {
namespace metals {

// ============================================================================
// Per-class canonical parameter sets
// ============================================================================

/**
 * Metal class FIRE parameters.
 * Each set expresses how the 9 BEAD channels couple for that structural class.
 */
struct MetalFireParams {
    SeedBeadParams seed;
    std::string class_label;
    std::string rationale;
};

/// FCC noble metals: Au, Ag, Pt
/// Dispersion-dominant. Long τ for slow η relaxation.
/// Low steric coupling — delocalized d-band.
inline MetalFireParams fcc_noble_params() {
    MetalFireParams p;
    p.class_label = "FCC-Noble";
    p.rationale   = "Dispersion-dominant; d-band delocalized; slow η relaxation";

    auto& s = p.seed;
    s.dt_initial             = 1.0;
    s.dt_max                 = 8.0;
    s.f_tol                  = 0.008;
    s.e_tol                  = 1.0e-7;
    s.max_steps              = 6000;
    s.snapshot_interval      = 50;
    s.record_positions       = true;
    s.fire_alpha_start        = 0.1;

    s.env_params.tau          = 120.0;  // slow environment relaxation
    s.env_params.alpha        = 0.55;   // density-weighted
    s.env_params.beta         = 0.45;   // orientation secondary
    s.env_params.gamma_steric = 0.12;   // low — FCC noble is soft
    s.env_params.gamma_elec   = -0.08;  // weak electrostatic modulation
    s.env_params.gamma_disp   = 0.65;   // dispersion dominant
    return p;
}

/// FCC transition metals: Cu, Ni, Al
/// Balanced steric and dispersion. Moderate τ.
inline MetalFireParams fcc_transition_params() {
    MetalFireParams p;
    p.class_label = "FCC-Transition";
    p.rationale   = "Balanced steric/dispersion; sp-d hybridisation; moderate τ";

    auto& s = p.seed;
    s.dt_initial             = 0.8;
    s.dt_max                 = 6.0;
    s.f_tol                  = 0.010;
    s.e_tol                  = 1.0e-7;
    s.max_steps              = 5000;
    s.snapshot_interval      = 40;
    s.record_positions       = true;
    s.fire_alpha_start        = 0.1;

    s.env_params.tau          = 90.0;
    s.env_params.alpha        = 0.60;
    s.env_params.beta         = 0.40;
    s.env_params.gamma_steric = 0.22;
    s.env_params.gamma_elec   = -0.10;
    s.env_params.gamma_disp   = 0.52;
    return p;
}

/// BCC refractory metals: W, Mo, Cr, Fe
/// High τ — slow structural reorganisation. Stiff steric channel.
/// Fe is magnetic — use magnetic_bias_factor to modulate γ_steric.
inline MetalFireParams bcc_refractory_params() {
    MetalFireParams p;
    p.class_label = "BCC-Refractory";
    p.rationale   = "Stiff; strong steric; high T_melt; long-range order; high τ";

    auto& s = p.seed;
    s.dt_initial             = 0.5;
    s.dt_max                 = 4.0;
    s.f_tol                  = 0.012;
    s.e_tol                  = 1.0e-7;
    s.max_steps              = 8000;
    s.snapshot_interval      = 80;
    s.record_positions       = true;
    s.fire_alpha_start        = 0.1;

    s.env_params.tau          = 220.0;  // very slow — BCC is stiffer
    s.env_params.alpha        = 0.70;   // density drives structure
    s.env_params.beta         = 0.30;
    s.env_params.gamma_steric = 0.40;   // strong steric — tight packing BCC
    s.env_params.gamma_elec   = -0.12;
    s.env_params.gamma_disp   = 0.45;
    return p;
}

/// HCP proxy metals: Ti, Co
/// Similar to FCC transition but with anisotropic orientation bias.
/// beta elevated to reflect c/a ratio sensitivity.
inline MetalFireParams hcp_proxy_params() {
    MetalFireParams p;
    p.class_label = "HCP-Proxy";
    p.rationale   = "HCP anisotropy bias; elevated beta for c/a orientation sensitivity";

    auto& s = p.seed;
    s.dt_initial             = 0.8;
    s.dt_max                 = 6.0;
    s.f_tol                  = 0.010;
    s.e_tol                  = 1.0e-7;
    s.max_steps              = 6000;
    s.snapshot_interval      = 50;
    s.record_positions       = true;
    s.fire_alpha_start        = 0.1;

    s.env_params.tau          = 100.0;
    s.env_params.alpha        = 0.50;
    s.env_params.beta         = 0.50;   // elevated — HCP orientation matters
    s.env_params.gamma_steric = 0.25;
    s.env_params.gamma_elec   = -0.10;
    s.env_params.gamma_disp   = 0.50;
    return p;
}

/// Select canonical params for a MetalRecord
inline MetalFireParams params_for_metal(const MetalRecord& m) {
    switch (m.structure) {
        case CrystalStructure::FCC:
            if (m.is_noble_metal) return fcc_noble_params();
            return fcc_transition_params();
        case CrystalStructure::BCC:
            return bcc_refractory_params();
        case CrystalStructure::HCP:
            return hcp_proxy_params();
    }
    return fcc_transition_params();
}

/// Alloy pair: linearly interpolate env_params from two pure-metal param sets
/// (Lorentz–Berthelot spirit — arithmetic mean for additive params)
inline MetalFireParams alloy_params(const MetalRecord& A,
                                    const MetalRecord& B,
                                    double x_B = 0.5)
{
    MetalFireParams pA = params_for_metal(A);
    MetalFireParams pB = params_for_metal(B);
    double x_A = 1.0 - x_B;

    MetalFireParams result;
    result.class_label = A.symbol + std::to_string(int((1-x_B)*100)) +
                         B.symbol + std::to_string(int(x_B*100));
    result.rationale = "Linear interpolation of " + pA.class_label +
                       " and " + pB.class_label;

    auto& sa = pA.seed; auto& sb = pB.seed; auto& sr = result.seed;

    sr.dt_initial        = x_A * sa.dt_initial  + x_B * sb.dt_initial;
    sr.dt_max            = x_A * sa.dt_max       + x_B * sb.dt_max;
    sr.f_tol             = x_A * sa.f_tol        + x_B * sb.f_tol;
    sr.e_tol             = 1.0e-7;
    sr.max_steps         = static_cast<uint64_t>(x_A * sa.max_steps + x_B * sb.max_steps);
    sr.snapshot_interval = static_cast<uint64_t>(x_A * sa.snapshot_interval + x_B * sb.snapshot_interval);
    sr.record_positions  = true;
    sr.fire_alpha_start   = 0.1;

    auto& ea = sa.env_params; auto& eb = sb.env_params; auto& er = sr.env_params;
    er.tau          = x_A * ea.tau          + x_B * eb.tau;
    er.alpha        = x_A * ea.alpha        + x_B * eb.alpha;
    er.beta         = x_A * ea.beta         + x_B * eb.beta;
    er.gamma_steric = x_A * ea.gamma_steric + x_B * eb.gamma_steric;
    er.gamma_elec   = x_A * ea.gamma_elec   + x_B * eb.gamma_elec;
    er.gamma_disp   = x_A * ea.gamma_disp   + x_B * eb.gamma_disp;
    return result;
}

} // namespace metals
} // namespace coarse_grain
