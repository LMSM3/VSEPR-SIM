#pragma once
/**
 * env_particle.hpp  —  Environmental particle extension
 * =====================================================
 * VSEPR-SIM 3.0.1
 *
 * Extends the wind-particle perturbation system into a dual-kind
 * environmental carrier model:
 *
 *   P_k = (id, kind, r_k, v_k, E_k, I_k, omega_k, chi_k, tau_k)
 *
 * Two particle kinds:
 *   SUN  — radiative energy packets (directional, intensity-weighted)
 *   WIND — advective momentum + transport disturbance packets
 *
 * Plant-specific subsystems:
 *   - Sun deposition:   dE_sun  = A_proj * I * T * S_photo * Phi_shade
 *   - Wind force:       dF_wind = C_d * A_eff * |v_rel|^2 * n_hat * Psi_flex
 *   - Root chaos:       Gamma_i = clamp(a0 + a1*M + a2*nablaW + a3*rho + a4*chi, 0.98, 1.8)
 *   - Leaf generation:  H(beta1*E + beta2*S + beta3*T - beta4*D - Theta)
 *   - Piecewise polynomial root response
 *
 * This header is self-contained and depends only on state.hpp (Vec3).
 * It composes with the existing WindParticle as a higher-level layer.
 *
 * Anti-black-box: every coefficient, every sub-term is explicit and
 * inspectable.  No hidden state.
 */

#include "atomistic/core/state.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>
#include <string>

namespace atomistic {
namespace environment {

// ============================================================================
// Particle Kind
// ============================================================================

enum class EnvParticleKind : uint8_t {
    Sun  = 0,
    Wind = 1
};

inline const char* kind_name(EnvParticleKind k) {
    switch (k) {
        case EnvParticleKind::Sun:  return "SUN";
        case EnvParticleKind::Wind: return "WIND";
    }
    return "UNKNOWN";
}

// ============================================================================
// Environmental Particle  —  P_k = (id, kind, r, v, E, I, omega, chi, tau)
// ============================================================================

struct EnvParticle {
    uint32_t        id       = 0;
    EnvParticleKind kind     = EnvParticleKind::Sun;
    Vec3            position = {0, 0, 0};   // r_k
    Vec3            velocity = {0, 0, 0};   // v_k (direction / transport)
    double          energy   = 0.0;         // E_k  energy content
    double          intensity= 1.0;         // I_k  local intensity weight
    double          omega    = 0.0;         // oscillation / temporal modulation
    double          chaos    = 0.0;         // chi_k stochastic control
    double          lifetime = 1.0;         // tau_k persistence (seconds)
};

// ============================================================================
// Bead Properties  —  local state for interaction kernels
// ============================================================================

struct BeadProps {
    Vec3   position      = {0, 0, 0};
    Vec3   velocity      = {0, 0, 0};
    double projected_area= 1.0;        // A_proj (sun) or A_eff (wind)
    double transmissivity= 0.8;        // T_i   tissue absorptivity
    double photo_response= 1.0;        // S_photo  biological photo coeff
    double drag_coeff    = 0.47;        // C_d,i   drag-like coefficient
    double flexibility   = 1.0;        // Psi_flex structural flex response
};

// ============================================================================
// Shade Factor  —  Phi_shade(i, k) = 1 - occlusion_fraction
// ============================================================================

/// Simple geometric shade factor: fraction of particle i visible to
/// source k, considering occluding beads.  Returns [0, 1].
/// For now, uses a placeholder model based on vertical alignment.
inline double shade_factor(const BeadProps& bead,
                           const EnvParticle& p,
                           double occlusion = 0.0) {
    (void)bead; (void)p;
    return std::clamp(1.0 - occlusion, 0.0, 1.0);
}

// ============================================================================
// Plant Environment Response  —  composite output per bead
// ============================================================================

struct PlantEnvResponse {
    double dE_sun      = 0.0;   // energy deposited by sun
    Vec3   dF_wind     = {0,0,0}; // force from wind
    double drying_rate = 0.0;   // evaporation / drying contribution
    double photo_bias  = 0.0;   // growth activation from light
    double stress_bias = 0.0;   // mechanical stress accumulation
};

inline PlantEnvResponse operator+(const PlantEnvResponse& a,
                                  const PlantEnvResponse& b) {
    return {
        a.dE_sun + b.dE_sun,
        a.dF_wind + b.dF_wind,
        a.drying_rate + b.drying_rate,
        a.photo_bias + b.photo_bias,
        a.stress_bias + b.stress_bias
    };
}

// ============================================================================
// Sun Deposition Kernel
// ============================================================================

/// dE_sun = A_proj * I_k * T_i * S_photo * Phi_shade
inline double sun_deposition(const BeadProps& bead,
                             const EnvParticle& p,
                             double occlusion = 0.0) {
    double A_proj  = bead.projected_area;
    double I_k     = p.intensity;
    double T_i     = bead.transmissivity;
    double S_photo = bead.photo_response;
    double Phi     = shade_factor(bead, p, occlusion);
    return A_proj * I_k * T_i * S_photo * Phi;
}

// ============================================================================
// Wind Force Kernel
// ============================================================================

/// dF_wind = C_d * A_eff * |v_rel|^2 * n_hat * Psi_flex
inline Vec3 wind_force(const BeadProps& bead,
                       const EnvParticle& p) {
    Vec3 v_rel = p.velocity - bead.velocity;
    double v2  = dot(v_rel, v_rel);
    double v_mag = std::sqrt(v2);
    if (v_mag < 1e-30) return {0, 0, 0};

    Vec3 n_hat = v_rel * (1.0 / v_mag);
    double F_mag = bead.drag_coeff * bead.projected_area * v2 * bead.flexibility;
    return n_hat * F_mag;
}

/// Drying rate from wind: proportional to relative velocity magnitude
inline double drying_kernel(const BeadProps& bead,
                            const EnvParticle& p,
                            double drying_coeff = 0.05) {
    Vec3 v_rel = p.velocity - bead.velocity;
    double v_mag = std::sqrt(dot(v_rel, v_rel));
    return drying_coeff * v_mag * bead.projected_area;
}

// ============================================================================
// Unified Interaction
// ============================================================================

/// Compute full plant-environment response for one bead–particle pair.
inline PlantEnvResponse interact_env_particle(
    const BeadProps& bead,
    const EnvParticle& p,
    double occlusion = 0.0)
{
    PlantEnvResponse out;

    if (p.kind == EnvParticleKind::Sun) {
        out.dE_sun     = sun_deposition(bead, p, occlusion);
        out.photo_bias = out.dE_sun * 0.15;
    }

    if (p.kind == EnvParticleKind::Wind) {
        out.dF_wind     = wind_force(bead, p);
        out.drying_rate = drying_kernel(bead, p);
        out.stress_bias = norm(out.dF_wind) * 0.08;
    }

    return out;
}

// ============================================================================
// Root Chaos Factor  —  Gamma_i in [0.98, 1.8]
// ============================================================================

struct RootLocalState {
    double moisture        = 0.5;   // M_i: moisture availability [0, 1]
    double water_gradient  = 0.0;   // nabla W_i
    double soil_density    = 0.5;   // rho_soil [0, 1]
    double chaos_perturb   = 0.0;   // chi_i stochastic branching
    double damage          = 0.0;   // structural damage [0, 1]
    double compaction      = 0.0;   // soil compaction [0, 1]
    double nutrient_grad   = 0.0;   // nutrient gradient
    double sun_coupling    = 0.5;   // indirect radiation coupling
};

struct RootChaosCoeffs {
    double alpha0 = 1.0;    // base offset
    double alpha1 = 0.3;    // moisture weight
    double alpha2 = 0.15;   // water gradient weight
    double alpha3 = -0.2;   // soil density penalty (denser = slower)
    double alpha4 = 0.1;    // stochastic amplitude
    double lo     = 0.98;   // clamp lower
    double hi     = 1.80;   // clamp upper
};

/// Gamma_i = clamp(a0 + a1*M + a2*nablaW + a3*rho + a4*chi, 0.98, 1.8)
inline double root_chaos_factor(const RootLocalState& s,
                                const RootChaosCoeffs& c = {}) {
    double raw = c.alpha0
               + c.alpha1 * s.moisture
               + c.alpha2 * s.water_gradient
               + c.alpha3 * s.soil_density
               + c.alpha4 * s.chaos_perturb;
    return std::clamp(raw, c.lo, c.hi);
}

// ============================================================================
// Piecewise Polynomial Root Response  —  R_root(x)
// ============================================================================

struct PolySegment {
    double x_lo = 0.0;         // domain lower bound
    double x_hi = 1.0;         // domain upper bound
    double a = 0, b = 0, c = 0, d = 0;  // ax^3 + bx^2 + cx + d

    double eval(double x) const {
        return a * x * x * x + b * x * x + c * x + d;
    }
};

struct PiecewiseRootPoly {
    PolySegment dry;       // x < x_dry
    PolySegment optimal;   // x_dry <= x < x_opt
    PolySegment saturated; // x >= x_opt

    double x_dry = 0.2;
    double x_opt = 0.7;

    double eval(double x) const {
        if (x < x_dry)      return dry.eval(x);
        if (x < x_opt)      return optimal.eval(x);
        return saturated.eval(x);
    }
};

/// Default polynomial: quadratic rise in dry, cubic peak in optimal,
/// quadratic decline in saturated.
inline PiecewiseRootPoly default_root_poly() {
    PiecewiseRootPoly p;
    p.x_dry = 0.2;
    p.x_opt = 0.7;
    // Dry zone: 2x^2 + 0.1x + 0.05  (slow, accelerating growth)
    p.dry = {0.0, 0.2, 0.0, 2.0, 0.1, 0.05};
    // Optimal zone: -0.5x^3 + 1.2x^2 - 0.3x + 0.8  (peak growth)
    p.optimal = {0.2, 0.7, -0.5, 1.2, -0.3, 0.8};
    // Saturated: -1.5x^2 + 2.0x + 0.3  (waterlogging decline)
    p.saturated = {0.7, 1.0, 0.0, -1.5, 2.0, 0.3};
    return p;
}

struct RootGrowthLimits {
    double damage_limit   = 0.5;
    double damage_mult    = 0.55;
    double compact_limit  = 0.6;
    double compact_mult   = 0.72;
    double nutrient_trig  = 0.3;
    double nutrient_mult  = 1.18;
    double low_sun_thresh = 0.2;
    double low_sun_mult   = 0.91;
    double final_lo       = 0.05;
    double final_hi       = 2.25;
};

/// Full root growth modifier: polynomial * logic gates * chaos factor
inline double root_growth_modifier(const RootLocalState& s,
                                   const PiecewiseRootPoly& poly,
                                   const RootChaosCoeffs& cc = {},
                                   const RootGrowthLimits& lim = {}) {
    double p = poly.eval(s.moisture);

    if (s.damage      > lim.damage_limit)   p *= lim.damage_mult;
    if (s.compaction   > lim.compact_limit)  p *= lim.compact_mult;
    if (s.nutrient_grad > lim.nutrient_trig) p *= lim.nutrient_mult;
    if (s.sun_coupling < lim.low_sun_thresh) p *= lim.low_sun_mult;

    double gamma = root_chaos_factor(s, cc);
    return std::clamp(p * gamma, lim.final_lo, lim.final_hi);
}

// ============================================================================
// Leaf Generation Gate  —  Heaviside step function
// ============================================================================

struct LeafGateCoeffs {
    double beta1 = 1.0;    // energy weight
    double beta2 = 0.8;    // light weight
    double beta3 = 0.6;    // hydration weight
    double beta4 = 1.5;    // damage penalty weight
    double theta = 2.0;    // activation threshold
};

struct LeafLocalState {
    double energy_local = 0.0;   // E_local
    double sunlight     = 0.0;   // S_light
    double hydration    = 0.0;   // T_hydr
    double damage_sum   = 0.0;   // Sigma_damage
};

/// Leaf generation gate: H(beta1*E + beta2*S + beta3*T - beta4*D - Theta)
/// Returns true if the threshold is exceeded (leaf primordium appears).
inline bool leaf_generation_gate(const LeafLocalState& s,
                                 const LeafGateCoeffs& c = {}) {
    double signal = c.beta1 * s.energy_local
                  + c.beta2 * s.sunlight
                  + c.beta3 * s.hydration
                  - c.beta4 * s.damage_sum
                  - c.theta;
    return signal >= 0.0;
}

/// Leaf gate signal value (continuous, for diagnostics).
inline double leaf_gate_signal(const LeafLocalState& s,
                               const LeafGateCoeffs& c = {}) {
    return c.beta1 * s.energy_local
         + c.beta2 * s.sunlight
         + c.beta3 * s.hydration
         - c.beta4 * s.damage_sum
         - c.theta;
}

/// Leaf expansion rate: H_leaf * G_leaf(E, L, W, Sigma)
/// Once initiated, growth is continuous and proportional to resource supply.
inline double leaf_expansion_rate(const LeafLocalState& s,
                                  const LeafGateCoeffs& c = {},
                                  double base_rate = 0.1) {
    if (!leaf_generation_gate(s, c)) return 0.0;
    // Post-initiation continuous growth
    double resource = std::max(0.0, s.energy_local + s.sunlight + s.hydration
                                    - s.damage_sum);
    return base_rate * resource;
}

// ============================================================================
// Environmental Energy Decomposition  —  U_env = U_wind + U_sun + ...
// ============================================================================

struct EnvEnergyTerms {
    double U_wind  = 0.0;
    double U_sun   = 0.0;
    double U_dry   = 0.0;
    double U_photo = 0.0;
    double U_stress= 0.0;

    double total() const { return U_wind + U_sun + U_dry + U_photo + U_stress; }
};

/// Accumulate env responses into energy decomposition
inline EnvEnergyTerms accumulate_env(
    const std::vector<PlantEnvResponse>& responses,
    const std::vector<BeadProps>& beads)
{
    EnvEnergyTerms E;
    for (size_t i = 0; i < responses.size() && i < beads.size(); ++i) {
        const auto& r = responses[i];
        E.U_sun   += r.dE_sun;
        E.U_wind  += -dot(r.dF_wind, beads[i].position);
        E.U_dry   += r.drying_rate;
        E.U_photo += r.photo_bias;
        E.U_stress+= r.stress_bias;
    }
    return E;
}

// ============================================================================
// Particle Lifetime  —  advect + decay
// ============================================================================

/// Advance particle position by dt, decay lifetime.  Returns false if dead.
inline bool advance_particle(EnvParticle& p, double dt) {
    p.position = p.position + p.velocity * dt;
    p.lifetime -= dt;

    // Oscillation modulation
    if (std::abs(p.omega) > 1e-30) {
        p.intensity *= (1.0 + 0.1 * std::sin(p.omega * dt));
        p.intensity  = std::max(0.0, p.intensity);
    }

    return p.lifetime > 0.0;
}

} // namespace environment
} // namespace atomistic
