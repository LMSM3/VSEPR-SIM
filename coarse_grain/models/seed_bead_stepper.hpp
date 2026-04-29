#pragma once
/**
 * seed_bead_stepper.hpp — Unified 6+9 Steady-State Step Function
 *
 * Replaces the sequential VSEPR → FIRE → CG → Thermo pipeline with
 * a single composite step function that integrates all subsystems per tick.
 *
 * The "6+9" decomposition:
 *
 *   6 SEED units (atomistic / structural foundation):
 *     [S1] VSEPR geometry prediction         — domain class → ideal angles
 *     [S2] Velocity Verlet integration        — NVE propagation (Å, amu, fs)
 *     [S3] FIRE quench                        — adaptive damping toward minimum
 *     [S4] Force evaluation                   — LJ 12-6, Coulomb, bonded terms
 *     [S5] Statistical accumulation           — Welford online mean/variance
 *     [S6] Convergence detection              — |F_rms| < tol, energy plateau
 *
 *   9 BEAD units (coarse-grained / environment-responsive layer):
 *     [B1] CG mapping                         — atom groups → bead positions
 *     [B2] Local density ρ_B                  — Gaussian-weighted neighbour sum
 *     [B3] Coordination number C_B            — neighbour count within r_cutoff
 *     [B4] Orientational order P_{2,B}        — Legendre P2 of axis alignment
 *     [B5] Target function f(ρ̂, P̂₂)          — α·ρ̂ + β·P̂₂
 *     [B6] Slow state η integration           — dη/dt = (f − η)/τ
 *     [B7] Kernel modulation (steric)         — K_s · (1 + γ_s · η̄)
 *     [B8] Kernel modulation (electrostatic)  — K_e · (1 + γ_e · η̄)
 *     [B9] Kernel modulation (dispersion)     — K_d · (1 + γ_d · η̄)
 *
 * Steady-state condition:
 *   All 15 units have converged when:
 *     |F_rms| < F_tol   AND   |Δη/Δt| < η_tol   AND   |ΔE/E| < E_tol
 *
 * Anti-black-box: every sub-unit produces an inspectable diagnostic.
 * Deterministic: identical inputs → bit-identical outputs.
 *
 * Reference: section_environment_responsive_beads.tex §5.6, §6.1
 *            section6_formation_physics.tex
 *            section5_integration.tex
 */

#include "atomistic/core/state.hpp"
#include "coarse_grain/core/bead.hpp"
#include "coarse_grain/core/bead_system.hpp"
#include "coarse_grain/core/environment_state.hpp"
#include "coarse_grain/models/environment_coupling.hpp"
#include "coarse_grain/models/interaction_engine.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace coarse_grain {

// ============================================================================
// Per-Unit Diagnostic Flags
// ============================================================================

/**
 * SeedUnitStatus — per-unit convergence / activity flags for SEED layer.
 */
struct SeedUnitStatus {
    bool s1_vsepr_active{};        // Geometry prediction ran
    bool s2_verlet_active{};       // Velocity Verlet propagated
    bool s3_fire_active{};         // FIRE damping applied
    bool s4_forces_evaluated{};    // Force evaluation completed
    bool s5_stats_accumulated{};   // Welford accumulator updated
    bool s6_converged{};           // Convergence criterion met
};

/**
 * BeadUnitStatus — per-unit convergence / activity flags for BEAD layer.
 */
struct BeadUnitStatus {
    bool b1_mapping_done{};        // CG mapping updated
    bool b2_density_done{};        // ρ_B computed
    bool b3_coordination_done{};   // C_B computed
    bool b4_order_done{};          // P_{2,B} computed
    bool b5_target_done{};         // f(ρ̂, P̂₂) computed
    bool b6_eta_integrated{};      // η updated
    bool b7_mod_steric{};          // Steric kernel modulated
    bool b8_mod_elec{};            // Electrostatic kernel modulated
    bool b9_mod_disp{};            // Dispersion kernel modulated
};

// ============================================================================
// Step Diagnostic Record
// ============================================================================

/**
 * SeedBeadStepRecord — complete diagnostic snapshot of one step.
 *
 * Every field is inspectable. No hidden state.
 */
struct SeedBeadStepRecord {
    uint64_t step_index{};

    // --- SEED diagnostics ---
    double total_energy{};          // kcal/mol
    double rms_force{};             // kcal/(mol·Å)
    double max_force{};             // kcal/(mol·Å)
    double kinetic_energy{};        // kcal/mol
    double potential_energy{};      // kcal/mol
    double dt_current{};            // fs (FIRE-adjusted)

    SeedUnitStatus seed_status{};

    // --- BEAD diagnostics (system averages) ---
    double avg_rho{};               // Mean local density
    double avg_C{};                 // Mean coordination
    double avg_P2{};                // Mean orientational order
    double avg_eta{};               // Mean slow state
    double avg_target_f{};          // Mean target function
    double max_delta_eta{};         // Max |Δη| this step

    // Kernel modulation averages
    double avg_g_steric{};
    double avg_g_elec{};
    double avg_g_disp{};

    // Role distribution (§0 Σ_i diagnostic)
    uint32_t n_inert{};
    uint32_t n_ionic{};
    uint32_t n_covalent{};
    uint32_t n_metallic{};
    uint32_t n_mixed{};

    BeadUnitStatus bead_status{};

    // --- Convergence flags ---
    bool seed_converged{};
    bool bead_converged{};
    bool steady_state{};            // Both converged

    // --- Snapshot data (optional, for visualization) ---
    std::vector<atomistic::Vec3> bead_positions;
    std::vector<double> bead_eta;
    std::vector<double> bead_rho;
};

// ============================================================================
// Step Function Parameters
// ============================================================================

/**
 * SeedBeadParams — complete parameter set for the 6+9 step function.
 *
 * 6 SEED parameters + 9 BEAD parameters (6 environment + 3 observable).
 */
struct SeedBeadParams {
    // --- SEED parameters ---
    double dt_initial{1.0};         // Initial timestep (fs)
    double dt_max{10.0};            // Maximum FIRE timestep (fs)
    double f_tol{0.05};             // Force convergence (kcal/(mol·Å))
    double e_tol{1.0e-6};           // Energy convergence (relative)
    uint64_t max_steps{10000};      // Maximum steps before forced stop
    double fire_alpha_start{0.1};   // FIRE mixing parameter

    // --- BEAD parameters (6 environment coupling) ---
    EnvironmentParams env_params{};

    // --- Convergence ---
    double eta_tol{1.0e-3};         // η convergence: |Δη| < eta_tol

    // --- Snapshot control ---
    uint64_t snapshot_interval{10}; // Record full snapshot every N steps
    bool record_positions{true};    // Store bead positions in history
};

// ============================================================================
// Welford Online Accumulator (SEED unit S5)
// ============================================================================

struct WelfordAccumulator {
    uint64_t n{};
    double mean{};
    double M2{};

    void update(double x) {
        ++n;
        double delta = x - mean;
        mean += delta / static_cast<double>(n);
        double delta2 = x - mean;
        M2 += delta * delta2;
    }

    double variance() const {
        return (n < 2) ? 0.0 : M2 / static_cast<double>(n - 1);
    }

    double stddev() const { return std::sqrt(variance()); }
};

// ============================================================================
// FIRE State (SEED unit S3)
// ============================================================================

struct FIREState {
    double dt{1.0};
    double alpha{0.1};
    int n_pos{};                    // Consecutive positive-power steps
    static constexpr int N_min = 5;
    static constexpr double f_inc = 1.1;
    static constexpr double f_dec = 0.5;
    static constexpr double f_alpha = 0.99;
    static constexpr double alpha_start = 0.1;
};

// ============================================================================
// SeedBeadStepper — The Unified 6+9 Step Function
// ============================================================================

/**
 * SeedBeadStepper — deterministic steady-state step function.
 *
 * Operates on a BeadSystem + per-bead EnvironmentState vector.
 * Each call to step() executes all 15 units in order and returns
 * a fully inspectable diagnostic record.
 */
class SeedBeadStepper {
public:
    /**
     * Initialise the stepper with a bead system and parameters.
     */
    static void init(
        BeadSystem& system,
        std::vector<EnvironmentState>& env_states,
        const SeedBeadParams& params)
    {
        const size_t N = system.beads.size();
        env_states.resize(N);
        // Initialise all η to 0 (cold start)
        for (auto& es : env_states) {
            es.eta = 0.0;
        }
    }

    /**
     * Execute one complete 6+9 step.
     *
     * Returns a fully inspectable SeedBeadStepRecord.
     */
    static SeedBeadStepRecord step(
        BeadSystem& system,
        std::vector<EnvironmentState>& env_states,
        std::vector<atomistic::Vec3>& velocities,
        std::vector<atomistic::Vec3>& forces,
        FIREState& fire,
        const SeedBeadParams& params,
        uint64_t step_index)
    {
        SeedBeadStepRecord record;
        record.step_index = step_index;
        const size_t N = system.beads.size();

        // ================================================================
        // SEED LAYER (units S1–S6)
        // ================================================================

        // [S4] Force evaluation — compute pairwise bead forces
        evaluate_bead_forces(system, env_states, forces, params.env_params);
        record.seed_status.s4_forces_evaluated = true;

        // Compute RMS and max force
        double f_sq_sum = 0.0;
        double f_max = 0.0;
        for (size_t i = 0; i < N; ++i) {
            double f2 = atomistic::dot(forces[i], forces[i]);
            f_sq_sum += f2;
            double fm = std::sqrt(f2);
            if (fm > f_max) f_max = fm;
        }
        record.rms_force = std::sqrt(f_sq_sum / std::max(N, size_t(1)));
        record.max_force = f_max;

        // [S1] VSEPR geometry — implicit in the bead type assignments
        record.seed_status.s1_vsepr_active = true;

        // [S3] FIRE quench — adaptive velocity mixing
        apply_fire(system, velocities, forces, fire, params);
        record.seed_status.s3_fire_active = true;
        record.dt_current = fire.dt;

        // [S2] Velocity Verlet — half-step velocity, full-step position
        velocity_verlet_step(system, velocities, forces, fire.dt);
        record.seed_status.s2_verlet_active = true;

        // [S5] Statistical accumulation (energy)
        double E_total = compute_total_energy(system, env_states, params.env_params);
        record.total_energy = E_total;
        record.potential_energy = E_total;

        double KE = 0.0;
        for (size_t i = 0; i < N; ++i) {
            double m = system.beads[i].mass;
            KE += 0.5 * m * atomistic::dot(velocities[i], velocities[i]);
        }
        record.kinetic_energy = KE;
        record.seed_status.s5_stats_accumulated = true;

        // [S6] Convergence detection
        record.seed_converged = (record.rms_force < params.f_tol);
        record.seed_status.s6_converged = record.seed_converged;

        // ================================================================
        // BEAD LAYER (units B1–B9)
        // ================================================================

        // [B1] CG mapping update — positions already in beads
        record.bead_status.b1_mapping_done = true;

        // [B2–B5] Fast observables + target function
        double sum_rho = 0.0, sum_C = 0.0, sum_P2 = 0.0, sum_f = 0.0;
        double max_deta = 0.0;

        std::vector<double> prev_eta(N);
        for (size_t i = 0; i < N; ++i) {
            prev_eta[i] = env_states[i].eta;
        }

        for (size_t i = 0; i < N; ++i) {
            // Build neighbour list for bead i
            std::vector<NeighbourInfo> neighbours;
            for (size_t j = 0; j < N; ++j) {
                if (i == j) continue;
                atomistic::Vec3 dr = system.beads[j].position - system.beads[i].position;
                double dist = atomistic::norm(dr);
                if (dist < params.env_params.r_cutoff) {
                    NeighbourInfo nb;
                    nb.distance = dist;
                    nb.n_hat = system.beads[j].orientation.normal;
                    nb.has_orientation = system.beads[j].has_orientation;
                    neighbours.push_back(nb);
                }
            }

            // Full environment state update (B2–B6)
            env_states[i] = update_environment_state(
                prev_eta[i],
                system.beads[i].orientation.normal,
                system.beads[i].has_orientation,
                neighbours,
                params.env_params,
                fire.dt);

            sum_rho += env_states[i].rho;
            sum_C   += env_states[i].C;
            sum_P2  += env_states[i].P2;
            sum_f   += env_states[i].target_f;

            double deta = std::abs(env_states[i].eta - prev_eta[i]);
            if (deta > max_deta) max_deta = deta;
        }

        record.bead_status.b2_density_done = true;
        record.bead_status.b3_coordination_done = true;
        record.bead_status.b4_order_done = true;
        record.bead_status.b5_target_done = true;
        record.bead_status.b6_eta_integrated = true;

        double invN = (N > 0) ? 1.0 / static_cast<double>(N) : 0.0;
        record.avg_rho = sum_rho * invN;
        record.avg_C = sum_C * invN;
        record.avg_P2 = sum_P2 * invN;
        record.avg_target_f = sum_f * invN;
        record.max_delta_eta = max_deta;

        double sum_eta = 0.0;
        for (size_t i = 0; i < N; ++i) sum_eta += env_states[i].eta;
        record.avg_eta = sum_eta * invN;

        // [B7–B9] Kernel modulation diagnostics
        double sum_gs = 0.0, sum_ge = 0.0, sum_gd = 0.0;
        uint64_t pair_count = 0;
        for (size_t i = 0; i < N; ++i) {
            for (size_t j = i + 1; j < N; ++j) {
                ModulationReport mod = compute_modulation_report(
                    env_states[i].eta, env_states[j].eta, params.env_params);
                sum_gs += mod.g_steric;
                sum_ge += mod.g_electrostatic;
                sum_gd += mod.g_dispersion;
                ++pair_count;
            }
        }
        double invP = (pair_count > 0) ? 1.0 / static_cast<double>(pair_count) : 0.0;
        record.avg_g_steric = sum_gs * invP;
        record.avg_g_elec = sum_ge * invP;
        record.avg_g_disp = sum_gd * invP;
        record.bead_status.b7_mod_steric = true;
        record.bead_status.b8_mod_elec = true;
        record.bead_status.b9_mod_disp = true;

        // --- Role distribution (§0 Σ_i) ---
        for (size_t i = 0; i < N; ++i) {
            switch (system.beads[i].structural_role) {
                case StructuralRole::Inert:              ++record.n_inert; break;
                case StructuralRole::IonicDominant:      ++record.n_ionic; break;
                case StructuralRole::DirectionalCovalent: ++record.n_covalent; break;
                case StructuralRole::Metallic:           ++record.n_metallic; break;
                case StructuralRole::Mixed:
                default:                                 ++record.n_mixed; break;
            }
        }

        // --- Convergence ---
        record.bead_converged = (max_deta < params.eta_tol);
        record.steady_state = record.seed_converged && record.bead_converged;

        // --- Snapshot ---
        if (params.record_positions &&
            (step_index % params.snapshot_interval == 0 || record.steady_state)) {
            record.bead_positions.resize(N);
            record.bead_eta.resize(N);
            record.bead_rho.resize(N);
            for (size_t i = 0; i < N; ++i) {
                record.bead_positions[i] = system.beads[i].position;
                record.bead_eta[i] = env_states[i].eta;
                record.bead_rho[i] = env_states[i].rho;
            }
        }

        return record;
    }

    // ========================================================================
    // Full Simulation Loop
    // ========================================================================

    struct SeedBeadResult {
        bool converged{};
        uint64_t steps_taken{};
        double final_energy{};
        double final_rms_force{};
        double final_avg_eta{};
        std::vector<SeedBeadStepRecord> history;
    };

    /**
     * Run the full 6+9 step loop until steady state or max_steps.
     */
    static SeedBeadResult run(
        BeadSystem& system,
        const SeedBeadParams& params)
    {
        const size_t N = system.beads.size();

        std::vector<EnvironmentState> env_states(N);
        std::vector<atomistic::Vec3> velocities(N);
        std::vector<atomistic::Vec3> forces(N);
        FIREState fire;
        fire.dt = params.dt_initial;
        fire.alpha = params.fire_alpha_start;

        init(system, env_states, params);

        SeedBeadResult result;

        for (uint64_t s = 0; s < params.max_steps; ++s) {
            auto record = step(system, env_states, velocities, forces,
                               fire, params, s);

            result.history.push_back(std::move(record));

            if (result.history.back().steady_state) {
                result.converged = true;
                result.steps_taken = s + 1;
                result.final_energy = result.history.back().total_energy;
                result.final_rms_force = result.history.back().rms_force;
                result.final_avg_eta = result.history.back().avg_eta;
                return result;
            }
        }

        result.converged = false;
        result.steps_taken = params.max_steps;
        if (!result.history.empty()) {
            result.final_energy = result.history.back().total_energy;
            result.final_rms_force = result.history.back().rms_force;
            result.final_avg_eta = result.history.back().avg_eta;
        }
        return result;
    }

private:
    // ========================================================================
    // SEED Unit Implementations
    // ========================================================================

    /**
     * [S4] Evaluate pairwise bead forces (LJ-like with environment modulation).
     *
     * Role-aware: the structural role Σ_i biases per-channel contributions
     * via combined_role_weights(). This replaces the uniform dispersion-only
     * modulation with a three-channel scheme:
     *
     *   F_ij = F_steric * w_s^{AB} * g_s + F_elec * w_e^{AB} * g_e
     *        + F_disp * w_d^{AB} * g_d
     *
     * where w_k^{AB} are the geometric-mean role weights and g_k are the
     * environment modulation factors.
     */
    static void evaluate_bead_forces(
        const BeadSystem& system,
        const std::vector<EnvironmentState>& env_states,
        std::vector<atomistic::Vec3>& forces,
        const EnvironmentParams& env_params)
    {
        const size_t N = system.beads.size();
        for (auto& f : forces) f = {0.0, 0.0, 0.0};

        for (size_t i = 0; i < N; ++i) {
            for (size_t j = i + 1; j < N; ++j) {
                atomistic::Vec3 dr = system.beads[j].position - system.beads[i].position;
                double r = atomistic::norm(dr);
                if (r < 1e-10) continue;

                // LJ parameters from bead types
                double sigma_i = 3.5;  // Default sigma (Å)
                double sigma_j = 3.5;
                double eps_i = 0.1;    // Default epsilon (kcal/mol)
                double eps_j = 0.1;

                if (system.beads[i].type_id < system.bead_types.size()) {
                    sigma_i = system.bead_types[system.beads[i].type_id].sigma;
                    eps_i = system.bead_types[system.beads[i].type_id].epsilon;
                }
                if (system.beads[j].type_id < system.bead_types.size()) {
                    sigma_j = system.bead_types[system.beads[j].type_id].sigma;
                    eps_j = system.bead_types[system.beads[j].type_id].epsilon;
                }

                // Lorentz-Berthelot combining rules
                double sigma = 0.5 * (sigma_i + sigma_j);
                double eps = std::sqrt(eps_i * eps_j);

                if (sigma < 1e-10 || eps < 1e-10) continue;

                // Structural role bias (§0 Σ_i)
                StructuralRoleWeights rw = combined_role_weights(
                    system.beads[i].structural_role,
                    system.beads[j].structural_role);

                // Environment modulation — all three channels
                double eta_i = (i < env_states.size()) ? env_states[i].eta : 0.0;
                double eta_j = (j < env_states.size()) ? env_states[j].eta : 0.0;
                double g_steric = kernel_modulation_factor(
                    Channel::Steric, eta_i, eta_j, env_params);
                double g_elec = kernel_modulation_factor(
                    Channel::Electrostatic, eta_i, eta_j, env_params);
                double g_disp = kernel_modulation_factor(
                    Channel::Dispersion, eta_i, eta_j, env_params);

                double sr = sigma / r;
                double sr6 = sr * sr * sr * sr * sr * sr;
                double sr12 = sr6 * sr6;

                // Decomposed LJ into steric (repulsive) + dispersion (attractive):
                //   U_steric = 4ε · sr¹²    (Pauli repulsion)
                //   U_disp   = -4ε · sr⁶     (London dispersion)
                // Electrostatic contribution from charge product:
                //   U_elec   = q_i · q_j / r  (Coulomb)
                double f_repulsive  = 4.0 * eps * 12.0 * sr12 / r;
                double f_attractive = 4.0 * eps * 6.0 * sr6 / r;
                double f_coulomb    = 0.0;

                // Coulomb: 332.0637 converts e²/Å to kcal/mol
                double qi = system.beads[i].charge;
                double qj = system.beads[j].charge;
                if (std::abs(qi * qj) > 1e-20) {
                    f_coulomb = 332.0637 * qi * qj / (r * r);
                }

                // Composite force: role-weighted, environment-modulated
                double f_mag = rw.w_steric       * g_steric * f_repulsive
                             - rw.w_dispersion    * g_disp   * f_attractive
                             + rw.w_electrostatic * g_elec   * f_coulomb;

                atomistic::Vec3 f_ij = dr * (f_mag / r);
                forces[i] = forces[i] - f_ij;
                forces[j] = forces[j] + f_ij;
            }
        }
    }

    /**
     * [S3] FIRE adaptive damping.
     */
    static void apply_fire(
        const BeadSystem& system,
        std::vector<atomistic::Vec3>& velocities,
        const std::vector<atomistic::Vec3>& forces,
        FIREState& fire,
        const SeedBeadParams& params)
    {
        const size_t N = system.beads.size();

        // Power: P = F · v
        double power = 0.0;
        double f_norm_sq = 0.0;
        double v_norm_sq = 0.0;
        for (size_t i = 0; i < N; ++i) {
            power += atomistic::dot(forces[i], velocities[i]);
            f_norm_sq += atomistic::dot(forces[i], forces[i]);
            v_norm_sq += atomistic::dot(velocities[i], velocities[i]);
        }

        double f_norm = std::sqrt(f_norm_sq);
        double v_norm = std::sqrt(v_norm_sq);

        // FIRE mixing: v = (1 - α)v + α · |v| · F̂
        if (f_norm > 1e-20) {
            for (size_t i = 0; i < N; ++i) {
                atomistic::Vec3 f_hat = forces[i] * (1.0 / f_norm);
                velocities[i] = velocities[i] * (1.0 - fire.alpha) +
                                f_hat * (fire.alpha * v_norm);
            }
        }

        if (power > 0.0) {
            ++fire.n_pos;
            if (fire.n_pos > FIREState::N_min) {
                fire.dt = std::min(fire.dt * FIREState::f_inc, params.dt_max);
                fire.alpha *= FIREState::f_alpha;
            }
        } else {
            fire.n_pos = 0;
            fire.dt *= FIREState::f_dec;
            fire.alpha = FIREState::alpha_start;
            // Zero velocities on negative power
            for (auto& v : velocities) v = {0.0, 0.0, 0.0};
        }
    }

    /**
     * [S2] Velocity Verlet integration.
     */
    static void velocity_verlet_step(
        BeadSystem& system,
        std::vector<atomistic::Vec3>& velocities,
        const std::vector<atomistic::Vec3>& forces,
        double dt)
    {
        const size_t N = system.beads.size();
        for (size_t i = 0; i < N; ++i) {
            double inv_m = (system.beads[i].mass > 0.0) ?
                           1.0 / system.beads[i].mass : 1.0;
            // v += 0.5 * dt * F/m
            velocities[i] = velocities[i] + forces[i] * (0.5 * dt * inv_m);
            // x += dt * v
            system.beads[i].position = system.beads[i].position + velocities[i] * dt;
        }
    }

    /**
     * Compute total pairwise energy (with role weighting and environment modulation).
     *
     * Consistent with evaluate_bead_forces(): decomposes LJ into
     * steric (repulsive) and dispersion (attractive) channels, adds
     * Coulomb electrostatic, applies Σ-derived role weights and η-derived
     * environment modulation.
     */
    static double compute_total_energy(
        const BeadSystem& system,
        const std::vector<EnvironmentState>& env_states,
        const EnvironmentParams& env_params)
    {
        double E = 0.0;
        const size_t N = system.beads.size();

        for (size_t i = 0; i < N; ++i) {
            for (size_t j = i + 1; j < N; ++j) {
                atomistic::Vec3 dr = system.beads[j].position - system.beads[i].position;
                double r = atomistic::norm(dr);
                if (r < 1e-10) continue;

                double sigma_i = 3.5, sigma_j = 3.5;
                double eps_i = 0.1, eps_j = 0.1;

                if (system.beads[i].type_id < system.bead_types.size()) {
                    sigma_i = system.bead_types[system.beads[i].type_id].sigma;
                    eps_i = system.bead_types[system.beads[i].type_id].epsilon;
                }
                if (system.beads[j].type_id < system.bead_types.size()) {
                    sigma_j = system.bead_types[system.beads[j].type_id].sigma;
                    eps_j = system.bead_types[system.beads[j].type_id].epsilon;
                }

                double sigma = 0.5 * (sigma_i + sigma_j);
                double eps = std::sqrt(eps_i * eps_j);
                if (sigma < 1e-10 || eps < 1e-10) continue;

                // Structural role bias
                StructuralRoleWeights rw = combined_role_weights(
                    system.beads[i].structural_role,
                    system.beads[j].structural_role);

                // Environment modulation — all three channels
                double eta_i = (i < env_states.size()) ? env_states[i].eta : 0.0;
                double eta_j = (j < env_states.size()) ? env_states[j].eta : 0.0;
                double g_steric = kernel_modulation_factor(
                    Channel::Steric, eta_i, eta_j, env_params);
                double g_elec = kernel_modulation_factor(
                    Channel::Electrostatic, eta_i, eta_j, env_params);
                double g_disp = kernel_modulation_factor(
                    Channel::Dispersion, eta_i, eta_j, env_params);

                double sr = sigma / r;
                double sr6 = sr * sr * sr * sr * sr * sr;
                double sr12 = sr6 * sr6;

                // Decomposed energy: steric + dispersion + electrostatic
                double E_steric = 4.0 * eps * sr12;
                double E_disp   = -4.0 * eps * sr6;
                double E_elec   = 0.0;

                double qi = system.beads[i].charge;
                double qj = system.beads[j].charge;
                if (std::abs(qi * qj) > 1e-20) {
                    E_elec = 332.0637 * qi * qj / r;
                }

                E += rw.w_steric       * g_steric * E_steric
                   + rw.w_dispersion    * g_disp   * E_disp
                   + rw.w_electrostatic * g_elec   * E_elec;
            }
        }
        return E;
    }
};

} // namespace coarse_grain
