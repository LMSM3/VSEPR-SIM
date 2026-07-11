#pragma once
/**
 * fire_smooth_runner.hpp — Core Runner: 


 → Relax → L3 Handoff
 *
 * Executes one smooth-sampling simulation:
 *   1. Build BeadSystem from drawn arrangement parameters
 *   2. Run 6+9 FIRE stepper, capturing full StepTrace
 *   3. Aggregate converged beads to Level 3 domains
 *   4. Return SmoothSampleRecord (all outputs attached to seed hash)
 *
 * Reference: coarse_grain/fire_smooth/README.md
 */

#include "coarse_grain/fire_smooth/smooth_sample.hpp"
#include <chrono>

namespace coarse_grain {
namespace fire_smooth {

// ============================================================================
// Runner
// ============================================================================

/**
 * @param desc       SampleDescriptor with sample parameters and perturbations.
 * @param rxn        ChemicalReaction base state with temperature pre-set.
 * @param max_steps  Maximum allowed integration steps.
 * @param l3_params  Level 3 aggregation parameters.
 * @return           SmoothSampleRecord with full trace and Level 3 handoff.
 */
inline SmoothSampleRecord run_smooth_sample(
    const SampleDescriptor&           desc,
    ChemicalReaction                  rxn,
    uint64_t                          max_steps,
    const level3::Level3BuildParams&  l3_params = {})
{
    SmoothSampleRecord result;
    result.descriptor = desc;

    // Set temperature from drawn value
    rxn.thermal.temperature_K = desc.temperature_drawn;

    // Map to bead system
    BeadSystem system = ReactionEngine::map_reaction_to_beads(rxn);

    // Record sample size and mass
    result.descriptor.N_beads = static_cast<uint32_t>(system.beads.size());
    result.descriptor.total_mass_amu = 0.0;
    for (const auto& b : system.beads)
        result.descriptor.total_mass_amu += b.mass;

    // Build params from drawn values
    SeedBeadParams params = ReactionEngine::build_reaction_params(rxn);
    params.dt_initial              = desc.dt_drawn;
    params.max_steps               = max_steps;
    params.env_params.tau          = desc.tau_drawn;
    params.env_params.gamma_steric = desc.gamma_steric_drawn;
    params.env_params.gamma_elec   = desc.gamma_elec_drawn;
    params.env_params.gamma_disp   = desc.gamma_disp_drawn;
    params.snapshot_interval       = 0;
    params.record_positions        = false;

    // Initialise stepper
    const size_t N = system.beads.size();
    std::vector<EnvironmentState>   env_states(N);
    std::vector<atomistic::Vec3>    velocities(N);
    std::vector<atomistic::Vec3>    forces(N);
    FIREState fire;
    fire.dt    = params.dt_initial;
    fire.alpha = params.fire_alpha_start;

    SeedBeadStepper::init(system, env_states, params);

    result.trace.reserve(std::min(max_steps, uint64_t(1000)));

    auto t0 = std::chrono::high_resolution_clock::now();

    std::vector<bool> bead_converged(N, false);

    for (uint64_t s = 0; s < params.max_steps; ++s) {
        auto sr = SeedBeadStepper::step(
            system, env_states, velocities, forces, fire, params, s);

        // Capture step trace
        StepTrace row;
        row.step_index    = sr.step_index;
        row.total_energy  = sr.total_energy;
        row.kinetic_energy = sr.kinetic_energy;
        row.rms_force     = sr.rms_force;
        row.max_force     = sr.max_force;
        row.dt_current    = sr.dt_current;
        row.avg_rho       = sr.avg_rho;
        row.avg_C         = sr.avg_C;
        row.avg_P2        = sr.avg_P2;
        row.avg_eta       = sr.avg_eta;
        row.avg_target_f  = sr.avg_target_f;
        row.max_delta_eta = sr.max_delta_eta;
        row.avg_g_steric  = sr.avg_g_steric;
        row.avg_g_elec    = sr.avg_g_elec;
        row.avg_g_disp    = sr.avg_g_disp;
        row.n_inert       = sr.n_inert;
        row.n_ionic       = sr.n_ionic;
        row.n_covalent    = sr.n_covalent;
        row.n_metallic    = sr.n_metallic;
        row.n_mixed       = sr.n_mixed;
        row.steady_state  = sr.steady_state;
        result.trace.push_back(row);

        if (sr.steady_state) {
            result.converged      = true;
            result.steps_taken    = s + 1;
            result.final_energy   = sr.total_energy;
            result.final_rms_force = sr.rms_force;
            result.final_avg_eta  = sr.avg_eta;
            // All beads converged at system convergence
            std::fill(bead_converged.begin(), bead_converged.end(), true);
            break;
        }

        result.steps_taken     = s + 1;
        result.final_energy    = sr.total_energy;
        result.final_rms_force = sr.rms_force;
        result.final_avg_eta   = sr.avg_eta;
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    result.elapsed_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    // --- Level 3 aggregation ---
    level3::Level3BuildParams l3p = l3_params;
    l3p.gamma_elec = desc.gamma_elec_drawn;

    result.l3_domains = level3::aggregate_to_l3(
        system.beads, env_states, bead_converged,
        desc.seed_hash, l3p);

    return result;
}

} // namespace fire_smooth
} // namespace coarse_grain
