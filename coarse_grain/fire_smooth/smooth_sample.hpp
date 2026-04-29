#pragma once
/**
 * smooth_sample.hpp — FIRE Smooth-Sampling Infrastructure
 *
 * A SampleDescriptor identifies one random draw in the smooth-sampling
 * experiment. Each draw selects an arrangement and parameter perturbation
 * from a seeded RNG, then feeds into the full FIRE 6+9 relaxation.
 *
 * "Smooth" refers to continuous perturbation of the coupling parameters
 * (tau, gamma_steric, gamma_elec, gamma_disp, temperature) around the
 * canonical arrangement values. This produces a smooth response surface
 * rather than a discrete grid.
 *
 * Smooth perturbation model:
 *   tau_i        = tau_base · exp(σ_tau · Z)         Z ~ N(0,1)
 *   gamma_k_i    = gamma_k_base + σ_gamma · Z_k
 *   temperature_i = T_base · (1 + σ_T · Z_T)
 *
 * where σ values are the perturbation widths (configurable).
 *
 * Outputs per sample:
 *   - Full SeedBeadStepRecord history (energy, forces, eta, QM descriptors)
 *   - Final Level3HandoffRecord (macro-DM precursor payload)
 *   - Convergence metadata
 *
 * Anti-black-box: every perturbation parameter and RNG draw is
 * recorded in SampleDescriptor and reproducible from seed_hash.
 *
 * Reference: coarse_grain/fire_smooth/README.md
 */

#include "coarse_grain/chemistry/reaction_engine.hpp"
#include "coarse_grain/chemistry/reaction_library.hpp"
#include "coarse_grain/level3/level3_builder.hpp"
#include "coarse_grain/qm/qm_descriptors.hpp"
#include <chrono>
#include <cmath>
#include <cstdint>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace coarse_grain {
namespace fire_smooth {

using namespace coarse_grain::chemistry;

// ============================================================================
// Smooth Perturbation Widths
// ============================================================================

/**
 * SmoothPerturbParams — controls the width of Gaussian perturbations
 * applied to arrangement parameters at each random draw.
 *
 * Setting a width to 0 disables that perturbation channel.
 */
struct SmoothPerturbParams {
    double sigma_tau{0.15};          // Log-normal width for tau
    double sigma_gamma{0.05};        // Additive std for gamma channels
    double sigma_temperature{0.08};  // Relative std for temperature
    double sigma_dt{0.10};           // Relative std for dt_initial
};

// ============================================================================
// SampleDescriptor — one random draw
// ============================================================================

/**
 * SampleDescriptor — fully-specified random draw.
 *
 * Everything needed to reproduce a single smooth-sampling simulation
 * exactly from master_seed + sample_index.
 *
 * Recorded perturbations (all reproducible from seed_hash):
 *   tau_drawn, gamma_steric_drawn, gamma_elec_drawn, gamma_disp_drawn,
 *   temperature_drawn, dt_drawn
 */
struct SampleDescriptor {
    uint32_t    sample_index{};         // Global index in this run
    uint64_t    master_seed{};
    uint32_t    arrangement_id{};
    std::string arrangement_label;
    std::string base_reaction;
    std::string equation;
    std::string seed_hash;              // 16-char hex

    // Base (canonical) parameters
    double tau_base{};
    double gamma_steric_base{};
    double gamma_elec_base{};
    double gamma_disp_base{};
    double temperature_base{};
    double dt_base{};

    // Drawn (perturbed) parameters — the actual values used
    double tau_drawn{};
    double gamma_steric_drawn{};
    double gamma_elec_drawn{};
    double gamma_disp_drawn{};
    double temperature_drawn{};
    double dt_drawn{};

    // Gaussian draws stored for full reproducibility audit
    double z_tau{};
    double z_gamma_steric{};
    double z_gamma_elec{};
    double z_gamma_disp{};
    double z_temperature{};
    double z_dt{};

    // Sample size and mass
    uint32_t    N_beads{};
    double      total_mass_amu{};
};

// ============================================================================
// SmoothSampleRecord — result of one FIRE relaxation
// ============================================================================

/**
 * SmoothSampleRecord — full output of one smooth-sampling simulation.
 *
 * Contains:
 *   - The sample descriptor (input parameters + perturbations)
 *   - Per-step diagnostic trace (all SeedBeadStepRecord fields)
 *   - Final convergence state
 *   - Level 3 handoff records (macro-DM precursor payload)
 *   - QM descriptors for final state
 *   - Wall time
 */
struct StepTrace {
    uint64_t step_index{};
    double   total_energy{};
    double   kinetic_energy{};
    double   rms_force{};
    double   max_force{};
    double   dt_current{};
    double   avg_rho{};
    double   avg_C{};
    double   avg_P2{};
    double   avg_eta{};
    double   avg_target_f{};
    double   max_delta_eta{};
    double   avg_g_steric{};
    double   avg_g_elec{};
    double   avg_g_disp{};
    uint32_t n_inert{};
    uint32_t n_ionic{};
    uint32_t n_covalent{};
    uint32_t n_metallic{};
    uint32_t n_mixed{};
    bool     steady_state{};
};

struct SmoothSampleRecord {
    SampleDescriptor descriptor;

    // FIRE trace — one row per integration step
    std::vector<StepTrace> trace;

    // Convergence outcome
    bool     converged{};
    uint64_t steps_taken{};
    double   final_energy{};
    double   final_rms_force{};
    double   final_avg_eta{};

    // Level 3 aggregation — macro-DM precursor payload
    std::vector<level3::Level3HandoffRecord> l3_domains;

    // Elapsed wall time
    double elapsed_ms{};
};

// ============================================================================
// Seed hash utility
// ============================================================================

inline uint64_t make_sample_hash(uint64_t master, uint32_t arr_id, uint32_t idx)
{
    return master
         ^ (static_cast<uint64_t>(arr_id) * 2654435761ULL)
         ^ (static_cast<uint64_t>(idx)    * 40503ULL);
}

inline std::string fmt_hash(uint64_t h)
{
    std::ostringstream s;
    s << std::hex << std::uppercase
      << std::setw(16) << std::setfill('0') << h;
    return s.str();
}

// ============================================================================
// draw_sample — produce a SampleDescriptor from RNG
// ============================================================================

/**
 * draw_sample — draw one SampleDescriptor from a seeded RNG.
 *
 * Applies Gaussian perturbations to the base arrangement parameters.
 * All draws are recorded for full reproducibility audit.
 *
 * @param idx         Global sample index
 * @param master_seed Master seed
 * @param arr_id      Arrangement ID (0–49)
 * @param arr         ArrangementDescriptor (base parameters)
 * @param perturb     Perturbation widths
 * @param rng         Seeded RNG (mt19937_64)
 * @param max_steps   Max integration steps
 * @param base_dt     Base dt (fs)
 * @return SampleDescriptor with all fields populated
 */
template<typename RNG>
inline SampleDescriptor draw_sample(
    uint32_t         idx,
    uint64_t         master_seed,
    uint32_t         arr_id,
    const std::string& arr_label,
    const std::string& base_rxn,
    const std::string& equation,
    double tau_base, double g_steric_base,
    double g_elec_base, double g_disp_base,
    double T_base,   double dt_base,
    const SmoothPerturbParams& perturb,
    RNG& rng)
{
    std::normal_distribution<double> norm(0.0, 1.0);

    SampleDescriptor s;
    s.sample_index      = idx;
    s.master_seed       = master_seed;
    s.arrangement_id    = arr_id;
    s.arrangement_label = arr_label;
    s.base_reaction     = base_rxn;
    s.equation          = equation;
    s.seed_hash         = fmt_hash(make_sample_hash(master_seed, arr_id, idx));

    s.tau_base           = tau_base;
    s.gamma_steric_base  = g_steric_base;
    s.gamma_elec_base    = g_elec_base;
    s.gamma_disp_base    = g_disp_base;
    s.temperature_base   = T_base;
    s.dt_base            = dt_base;

    // Draw perturbations
    s.z_tau           = norm(rng);
    s.z_gamma_steric  = norm(rng);
    s.z_gamma_elec    = norm(rng);
    s.z_gamma_disp    = norm(rng);
    s.z_temperature   = norm(rng);
    s.z_dt            = norm(rng);

    // Apply
    s.tau_drawn          = tau_base        * std::exp(perturb.sigma_tau * s.z_tau);
    s.gamma_steric_drawn = g_steric_base   + perturb.sigma_gamma * s.z_gamma_steric;
    s.gamma_elec_drawn   = g_elec_base     + perturb.sigma_gamma * s.z_gamma_elec;
    s.gamma_disp_drawn   = g_disp_base     + perturb.sigma_gamma * s.z_gamma_disp;
    s.temperature_drawn  = T_base  * std::max(0.1, 1.0 + perturb.sigma_temperature * s.z_temperature);
    s.dt_drawn           = dt_base * std::max(0.1, 1.0 + perturb.sigma_dt         * s.z_dt);

    // Clamp to physically valid ranges
    s.tau_drawn          = std::max(5.0,  s.tau_drawn);
    s.dt_drawn           = std::clamp(s.dt_drawn, 0.1, 2.0);

    return s;
}

} // namespace fire_smooth
} // namespace coarse_grain
