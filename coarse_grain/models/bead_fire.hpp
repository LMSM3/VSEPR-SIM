#pragma once
/**
 * bead_fire.hpp — Lattice-Level FIRE Minimizer for CG Bead Systems
 *
 * FIRE (Fast Inertial Relaxation Engine) adapted to operate on
 * BeadSystem using the anisotropic surface-mapped interaction model.
 *
 * Force evaluation chain:
 *   BeadSystem → InteractionEngine::interaction_energy()
 *     → per-channel SH rotation (Wigner D-matrices)
 *     → per-(ℓ,m) radial kernel evaluation
 *     → environment coupling modulation (η-responsive)
 *     → forces by central-difference gradient
 *
 * This is NOT a second FIRE implementation. It reuses the same
 * algorithmic skeleton as atomistic::FIRE but operates on the CG
 * state (bead positions, orientations, unified descriptors) and
 * derives forces from the anisotropic interaction engine.
 *
 * Convergence criteria (same philosophy as atomistic FIRE):
 *   - RMS force < epsF
 *   - Per-bead energy change < epsU
 *   - Maximum steps exceeded
 *
 * Anti-black-box: per-step statistics (energy, force, α, dt, per-channel
 * decomposition) are all recorded and inspectable.
 *
 * Deterministic: same input → bit-identical output.
 *
 * Reference:
 *   - atomistic/integrators/fire.hpp (algorithmic skeleton)
 *   - coarse_grain/models/interaction_engine.hpp (force evaluation)
 *   - section_anisotropic_beads.tex §4-5 (model specification)
 *   - section_environment_responsive_beads.tex §6 (coupling)
 */

#include "coarse_grain/core/bead.hpp"
#include "coarse_grain/core/bead_system.hpp"
#include "coarse_grain/core/unified_descriptor.hpp"
#include "coarse_grain/core/environment_state.hpp"
#include "coarse_grain/models/interaction_engine.hpp"
#include "coarse_grain/models/environment_coupling.hpp"
#include "atomistic/core/state.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace coarse_grain {

// ============================================================================
// LL-FIRE Parameters
// ============================================================================

/**
 * BeadFIREParams — FIRE parameters for CG bead minimization.
 *
 * Same algorithmic constants as atomistic FIRE, but with additional
 * CG-specific options (environment update frequency, force finite
 * difference step size).
 */
struct BeadFIREParams {
    // --- Core FIRE parameters (Bitzek et al., 2006) ---
    double dt         = 1.0;      // Initial timestep (fs, CG timescale)
    double alpha      = 0.1;      // Velocity mixing parameter
    double finc       = 1.1;      // dt increase factor when P > 0
    double fdec       = 0.5;      // dt decrease factor when P ≤ 0
    double falpha     = 0.99;     // α decrease factor when P > 0
    int    nmin       = 5;        // Steps with P > 0 before dt increase
    double dt_max     = 10.0;     // Maximum timestep (fs)

    // --- Convergence criteria ---
    double epsF       = 1e-4;     // RMS force threshold (kcal/mol·Å)
    double epsU       = 1e-8;     // Per-bead energy delta threshold (kcal/mol)
    int    max_steps  = 2000;     // Maximum minimization steps

    // --- CG-specific options ---
    double fd_delta   = 1e-4;     // Finite-difference step for force (Å)
    int    env_update_freq = 10;  // Steps between environment state updates (0=never)
    bool   use_environment = true;// Enable η-responsive kernel modulation
};

// ============================================================================
// Per-Step Statistics
// ============================================================================

/**
 * BeadFIREStep — diagnostic record for one minimization step.
 * 
 * Includes bead positions for trajectory visualization.
 */
struct BeadFIREStep {
    int    step{};
    double U_total{};
    double U_steric{};
    double U_electrostatic{};
    double U_dispersion{};
    double Frms{};
    double Fmax{};
    double alpha{};
    double dt{};
    double dU_per_bead{};

    // Bead positions at this step (for trajectory visualization)
    std::vector<atomistic::Vec3> positions;
};

/**
 * BeadFIREResult — final outcome of a minimization run.
 */
struct BeadFIREResult {
    int    steps_taken{};
    double U_final{};
    double Frms_final{};
    double Fmax_final{};
    double alpha_final{};
    double dt_final{};
    bool   converged{false};

    // Per-channel energy decomposition at convergence
    double U_steric{};
    double U_electrostatic{};
    double U_dispersion{};

    // Optional: full step history (for plotting in UI)
    std::vector<BeadFIREStep> history;
    bool record_history{false};
};

// ============================================================================
// Force Evaluation on BeadSystem
// ============================================================================

/**
 * Evaluate total energy and per-bead forces for a BeadSystem.
 *
 * Forces are computed via central finite difference on the total
 * pairwise interaction energy from the InteractionEngine:
 *
 *   F_i^α = -(U(r_i^α + δ) - U(r_i^α - δ)) / (2δ)
 *
 * The interaction engine uses:
 *   - SH coefficient rotation (Wigner D-matrices)
 *   - Per-channel, per-ℓ radial kernels
 *   - Environment coupling modulation (if η states present)
 *
 * @param beads         Bead positions and descriptors
 * @param env_states    Per-bead environment states (may be empty)
 * @param forces        Output: per-bead force vectors (resized internally)
 * @param params        Interaction parameters
 * @param env_params    Environment coupling parameters
 * @param fd_delta      Finite difference step size (Å)
 * @param use_env       Whether to apply environment modulation
 * @return Total energy (kcal/mol) and per-channel decomposition
 */
struct BeadEnergyResult {
    double E_total{};
    double E_steric{};
    double E_electrostatic{};
    double E_dispersion{};
};

inline BeadEnergyResult evaluate_bead_energy(
    const std::vector<Bead>& beads,
    const std::vector<EnvironmentState>& env_states,
    const InteractionParams& params,
    const EnvironmentParams& env_params = {})
{
    BeadEnergyResult result;
    int N = static_cast<int>(beads.size());

    for (int i = 0; i < N; ++i) {
        for (int j = i + 1; j < N; ++j) {
            // Skip beads without unified descriptors
            if (!beads[i].has_unified_data() || !beads[j].has_unified_data())
                continue;

            atomistic::Vec3 r_vec = beads[j].position - beads[i].position;

            InteractionResult ir;
            bool have_env = (!env_states.empty() &&
                             static_cast<int>(env_states.size()) > std::max(i, j));

            if (have_env) {
                // Use the environment-modulated interaction path (LaTeX §4 + §6.1)
                ir = interaction_energy(
                    *beads[i].unified, *beads[j].unified, r_vec, params,
                    env_states[i].eta, env_states[j].eta, env_params);
            } else {
                ir = interaction_energy(
                    *beads[i].unified, *beads[j].unified, r_vec, params);
            }

            result.E_steric        += ir.steric.energy;
            result.E_electrostatic += ir.electrostatic.energy;
            result.E_dispersion    += ir.dispersion.energy;
            result.E_total         += ir.E_total;
        }
    }

    return result;
}

inline void evaluate_bead_forces(
    std::vector<Bead>& beads,
    const std::vector<EnvironmentState>& env_states,
    std::vector<atomistic::Vec3>& forces,
    const InteractionParams& params,
    const EnvironmentParams& env_params = {},
    double fd_delta = 1e-4)
{
    int N = static_cast<int>(beads.size());
    forces.assign(N, {0.0, 0.0, 0.0});

    // Central finite difference: F_i^α = -(U(+δ) - U(-δ)) / (2δ)
    for (int i = 0; i < N; ++i) {
        atomistic::Vec3 orig = beads[i].position;

        // x-component
        beads[i].position = {orig.x + fd_delta, orig.y, orig.z};
        double Ux_plus = evaluate_bead_energy(beads, env_states, params, env_params).E_total;
        beads[i].position = {orig.x - fd_delta, orig.y, orig.z};
        double Ux_minus = evaluate_bead_energy(beads, env_states, params, env_params).E_total;

        // y-component
        beads[i].position = {orig.x, orig.y + fd_delta, orig.z};
        double Uy_plus = evaluate_bead_energy(beads, env_states, params, env_params).E_total;
        beads[i].position = {orig.x, orig.y - fd_delta, orig.z};
        double Uy_minus = evaluate_bead_energy(beads, env_states, params, env_params).E_total;

        // z-component
        beads[i].position = {orig.x, orig.y, orig.z + fd_delta};
        double Uz_plus = evaluate_bead_energy(beads, env_states, params, env_params).E_total;
        beads[i].position = {orig.x, orig.y, orig.z - fd_delta};
        double Uz_minus = evaluate_bead_energy(beads, env_states, params, env_params).E_total;

        // Restore position
        beads[i].position = orig;

        // Central difference
        double inv_2d = 1.0 / (2.0 * fd_delta);
        forces[i].x = -(Ux_plus - Ux_minus) * inv_2d;
        forces[i].y = -(Uy_plus - Uy_minus) * inv_2d;
        forces[i].z = -(Uz_plus - Uz_minus) * inv_2d;
    }
}

// ============================================================================
// LL-FIRE Minimizer
// ============================================================================

/**
 * BeadFIRE — Lattice-Level FIRE minimizer for CG bead systems.
 *
 * Operates on bead positions using the anisotropic interaction engine
 * for force evaluation. Same FIRE algorithm as atomistic::FIRE with
 * CG-specific extensions.
 */
struct BeadFIRE {

    /**
     * Minimize a bead system using FIRE.
     *
     * @param beads         Bead positions and descriptors (modified in-place)
     * @param env_states    Per-bead environment states (updated if env_update_freq > 0)
     * @param int_params    Interaction engine parameters
     * @param fire_params   FIRE algorithm parameters
     * @return BeadFIREResult with convergence info and optional history
     */
    static BeadFIREResult minimize(
        std::vector<Bead>& beads,
        std::vector<EnvironmentState>& env_states,
        const InteractionParams& int_params,
        const BeadFIREParams& fp,
        const EnvironmentParams& env_params = {})
    {
        int N = static_cast<int>(beads.size());
        if (N < 2) return {};  // Nothing to minimize

        BeadFIREResult result;
        result.record_history = true;

        // Per-bead velocities (CG velocities, not atomistic)
        std::vector<atomistic::Vec3> vel(N, {0.0, 0.0, 0.0});
        std::vector<atomistic::Vec3> forces;

        double dt = fp.dt;
        double alpha = fp.alpha;
        int npos = 0;
        double Uprev = std::numeric_limits<double>::infinity();

        // Evaluate initial forces
        evaluate_bead_forces(beads, env_states, forces, int_params, env_params, fp.fd_delta);
        auto E0 = evaluate_bead_energy(beads, env_states, int_params, env_params);

        // Initialize velocities along force direction (prevent P=0 deadlock)
        {
            long double fnorm2 = 0;
            for (int i = 0; i < N; ++i)
                fnorm2 += atomistic::dot(forces[i], forces[i]);
            double fnorm = std::sqrt(static_cast<double>(fnorm2));
            if (fnorm > 0.0) {
                for (int i = 0; i < N; ++i)
                    vel[i] = forces[i] * (dt / fnorm);
            }
        }

        for (int t = 0; t < fp.max_steps; ++t) {
            // Evaluate forces + energy at current positions
            evaluate_bead_forces(beads, env_states, forces, int_params, env_params, fp.fd_delta);
            auto Ecur = evaluate_bead_energy(beads, env_states, int_params, env_params);
            double U = Ecur.E_total;

            // RMS and max force
            long double frms_acc = 0;
            double fmax = 0.0;
            for (int i = 0; i < N; ++i) {
                double f2 = atomistic::dot(forces[i], forces[i]);
                frms_acc += f2;
                double fi = std::sqrt(f2);
                if (fi > fmax) fmax = fi;
            }
            double Frms = std::sqrt(static_cast<double>(frms_acc) / N);

            // Record step
            if (result.record_history) {
                BeadFIREStep step;
                step.step = t;
                step.U_total = U;
                step.U_steric = Ecur.E_steric;
                step.U_electrostatic = Ecur.E_electrostatic;
                step.U_dispersion = Ecur.E_dispersion;
                step.Frms = Frms;
                step.Fmax = fmax;
                step.alpha = alpha;
                step.dt = dt;
                step.dU_per_bead = (t > 0) ? std::abs(U - Uprev) / N : 0.0;

                // Store bead positions for trajectory visualization
                step.positions.resize(N);
                for (int i = 0; i < N; ++i) {
                    step.positions[i] = beads[i].position;
                }

                result.history.push_back(step);
            }

            // Convergence check (skip first 2 steps for velocity build-up)
            if (t > 1) {
                double dU_per_bead = std::abs(U - Uprev) / N;
                if (Frms < fp.epsF || (fp.epsU > 0.0 && dU_per_bead < fp.epsU)) {
                    result.converged = true;
                    result.steps_taken = t;
                    result.U_final = U;
                    result.Frms_final = Frms;
                    result.Fmax_final = fmax;
                    result.alpha_final = alpha;
                    result.dt_final = dt;
                    result.U_steric = Ecur.E_steric;
                    result.U_electrostatic = Ecur.E_electrostatic;
                    result.U_dispersion = Ecur.E_dispersion;
                    return result;
                }
            }

            Uprev = U;

            // Power P = Σ v_i · f_i
            long double P = 0;
            long double vnorm2 = 0;
            long double fnorm2 = 0;
            for (int i = 0; i < N; ++i) {
                P += atomistic::dot(vel[i], forces[i]);
                vnorm2 += atomistic::dot(vel[i], vel[i]);
                fnorm2 += atomistic::dot(forces[i], forces[i]);
            }
            double vnorm = std::sqrt(static_cast<double>(vnorm2));
            double fnorm = std::sqrt(static_cast<double>(fnorm2));

            // Velocity mixing: v ← (1-α)v + α|v| f̂
            if (fnorm > 0 && vnorm > 0) {
                for (int i = 0; i < N; ++i) {
                    atomistic::Vec3 fhat = forces[i] * (1.0 / fnorm);
                    vel[i] = vel[i] * (1.0 - alpha) + fhat * (alpha * vnorm);
                }
            }

            // Adaptive timestep
            if (P > 0) {
                npos++;
                if (npos > fp.nmin) {
                    dt = std::min(dt * fp.finc, fp.dt_max);
                    alpha *= fp.falpha;
                }
            } else {
                npos = 0;
                dt *= fp.fdec;
                alpha = fp.alpha;
                // Kick velocities along force direction
                if (fnorm > 0.0) {
                    for (int i = 0; i < N; ++i)
                        vel[i] = forces[i] * (dt / fnorm);
                } else {
                    for (auto& v : vel) v = {0.0, 0.0, 0.0};
                }
            }

            // Position update: x ← x + dt·v
            for (int i = 0; i < N; ++i) {
                beads[i].position = beads[i].position + vel[i] * dt;
            }

            // Periodic environment state update (LaTeX §6 — exact η integration)
            if (fp.use_environment && fp.env_update_freq > 0 &&
                t > 0 && (t % fp.env_update_freq) == 0 &&
                !env_states.empty())
            {
                for (int i = 0; i < N; ++i) {
                    // Build neighbour list for bead i from current positions
                    std::vector<NeighbourInfo> nbs;
                    nbs.reserve(N - 1);
                    for (int j = 0; j < N; ++j) {
                        if (j == i) continue;
                        atomistic::Vec3 dr = beads[j].position - beads[i].position;
                        NeighbourInfo ni;
                        ni.distance = atomistic::norm(dr);
                        ni.n_hat = beads[j].orientation.normal;
                        ni.has_orientation = beads[j].has_orientation;
                        nbs.push_back(ni);
                    }
                    double eta_prev = env_states[i].eta;
                    env_states[i] = update_environment_state(
                        eta_prev,
                        beads[i].orientation.normal,
                        beads[i].has_orientation,
                        nbs, env_params, fp.dt);
                }
            }
        }

        // Did not converge — return final state
        auto Efinal = evaluate_bead_energy(beads, env_states, int_params, env_params);
        result.converged = false;
        result.steps_taken = fp.max_steps;
        result.U_final = Efinal.E_total;
        result.Frms_final = result.history.empty() ? 0.0 : result.history.back().Frms;
        result.Fmax_final = result.history.empty() ? 0.0 : result.history.back().Fmax;
        result.alpha_final = alpha;
        result.dt_final = dt;
        result.U_steric = Efinal.E_steric;
        result.U_electrostatic = Efinal.E_electrostatic;
        result.U_dispersion = Efinal.E_dispersion;
        return result;
    }
};

} // namespace coarse_grain
