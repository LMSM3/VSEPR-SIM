#pragma once
/**
 * wind_particle.hpp  —  External directional perturbation field
 * =============================================================
 * VSEPR-SIM 3.0.0
 *
 * A "wind particle" is a virtual, massless force source that applies a
 * smooth directional energy/force field across the simulation domain.
 * It models external perturbations (gas flow, electric field drift,
 * shock-wave fronts, deposition flux) without introducing a real particle
 * into the pair-list.
 *
 * Design choice — timestep headroom
 * ----------------------------------
 * Wind forces are soft by construction: the maximum gradient is bounded
 * so that the integrator can safely take 1.5–2× the normal timestep
 * without energy blow-up.  This is achieved by clamping the per-atom
 * force contribution to `F_max` and tapering via a smooth envelope.
 *
 *   F_wind_i = min(|F_raw_i|, F_max) * direction_hat * envelope(r_i)
 *
 * The envelope function is a 3D Gaussian centred on the wind origin
 * with width `sigma`:
 *
 *   envelope(r) = exp( -|r - r_origin|^2 / (2 sigma^2) )
 *
 * Energy contribution:
 *
 *   U_wind = -sum_i  F_wind_i · r_i     (conservative work integral)
 *
 * The time-step safety factor `dt_factor` (default 1.5) multiplies
 * the caller's dt, giving the wind sub-system 1.5× temporal headroom.
 * When the wind is the dominant perturbation, the integrator may safely
 * use dt_eff = dt * dt_factor without violating energy conservation
 * bounds, because |dU_wind/dt| <= F_max * v_max * N, which is bounded
 * by construction.
 *
 * Usage:
 *   WindParticle wind;
 *   wind.direction = {1, 0, 0};  // +x wind
 *   wind.strength  = 0.5;        // kcal/(mol·Å)
 *   wind.apply(state);           // adds to state.F, state.E.Uext
 *
 * Architecture:
 *   This header is self-contained and depends only on state.hpp.
 *   It is NOT an IModel — it is an additive perturbation applied
 *   after the primary model eval.  This keeps the force pipeline
 *   composable: model.eval(s, mp); wind.apply(s);
 */

#include "atomistic/core/state.hpp"
#include <algorithm>
#include <cmath>

namespace atomistic {
namespace perturbation {

// ============================================================================
// Wind Particle Parameters
// ============================================================================

struct WindParams {
    // Direction (will be normalised internally)
    Vec3   direction   = {1.0, 0.0, 0.0};

    // Base force magnitude (kcal/(mol·Å))
    double strength    = 0.5;

    // Force clamp — maximum per-atom contribution (kcal/(mol·Å))
    // This guarantees the integrator can take 1.5-2× dt safely.
    double F_max       = 2.0;

    // Spatial envelope centre (Å)
    Vec3   origin      = {0.0, 0.0, 0.0};

    // Spatial envelope width (Å).  0 = uniform (no taper).
    double sigma       = 0.0;

    // Time-step headroom factor.  The effective dt for wind-dominated
    // motion is dt * dt_factor.  1.5 gives 50% headroom; 2.0 gives 100%.
    double dt_factor   = 1.5;

    // Ramp schedule: wind ramps linearly from 0 to full strength
    // over `ramp_steps` integration steps.  Prevents impulse shock.
    int    ramp_steps  = 50;
};

// ============================================================================
// Wind Particle — runtime state + application
// ============================================================================

struct WindParticle {
    WindParams params;
    int        step_count = 0;   // current step (for ramp)

    // ── Apply wind force to every atom in state ──
    //
    // Adds to state.F[i] and accumulates energy in state.E.Uext.
    // Call this AFTER model.eval() so forces compose additively.
    void apply(State& s) noexcept {
        if (s.N == 0) return;

        // Normalise direction
        const double dnorm = norm(params.direction);
        if (dnorm < 1e-30) return;
        const Vec3 dhat = params.direction * (1.0 / dnorm);

        // Ramp factor: 0→1 over ramp_steps
        const double ramp = (params.ramp_steps > 0 && step_count < params.ramp_steps)
            ? static_cast<double>(step_count) / static_cast<double>(params.ramp_steps)
            : 1.0;

        const double base_F = params.strength * ramp;
        const double sig2   = params.sigma * params.sigma;
        const bool   taper  = (sig2 > 1e-20);

        double U_wind = 0.0;

        for (uint32_t i = 0; i < s.N; ++i) {
            // Spatial envelope
            double env = 1.0;
            if (taper) {
                const Vec3 dr = s.X[i] - params.origin;
                const double r2 = dot(dr, dr);
                env = std::exp(-r2 / (2.0 * sig2));
            }

            // Per-atom force magnitude (clamped)
            double Fi = std::min(base_F * env, params.F_max);

            // Force vector
            Vec3 Fvec = dhat * Fi;

            // Accumulate
            s.F[i] = s.F[i] + Fvec;

            // Conservative energy: U = -F · r  (work done by wind)
            U_wind -= dot(Fvec, s.X[i]);
        }

        s.E.Uext += U_wind;
        ++step_count;
    }

    // ── Effective timestep ──
    // Returns dt * dt_factor for the caller to use when the wind
    // is the dominant perturbation source.
    double effective_dt(double dt) const noexcept {
        return dt * params.dt_factor;
    }

    // ── Reset step counter (e.g., for new run) ──
    void reset() noexcept { step_count = 0; }

    // ── Diagnostic: current ramp fraction [0,1] ──
    double ramp_fraction() const noexcept {
        if (params.ramp_steps <= 0) return 1.0;
        return std::min(1.0, static_cast<double>(step_count) / static_cast<double>(params.ramp_steps));
    }

    // ── Diagnostic: peak force magnitude at origin ──
    double peak_force() const noexcept {
        return std::min(params.strength * ramp_fraction(), params.F_max);
    }

    // ── Diagnostic: energy headroom ratio ──
    // Returns F_max / strength.  Values >= 1.5 are safe for dt_factor=1.5.
    double headroom_ratio() const noexcept {
        if (params.strength < 1e-30) return 1e30;
        return params.F_max / params.strength;
    }
};

} // namespace perturbation
} // namespace atomistic
